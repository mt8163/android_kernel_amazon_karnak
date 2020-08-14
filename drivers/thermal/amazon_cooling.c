/*
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/thermal.h>
#include <linux/of_platform.h>
#include <linux/reboot.h>
#include <linux/suspend.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include "thermal_core.h"
#include "amazon_thermal_life_cycle_reasons.h"
//#include "amazon_thermal_pm_helper.h"

#define SUSPEND_COOLER "suspend_cooler"
#define SUSPEND_COOLER_SUSPEND_STATE 2
#define SUSPEND_COOLER_SLEEP_TIME_S 30
#define SUSPEND_COOLER_SLEEP_TIMEOUT_MS (SUSPEND_COOLER_SLEEP_TIME_S*1000*2)
#define SUSPEND_COOLER_AWAKE_TIME_MS 3000
#define SUSPEND_COOLER_SUSPEND_MAX_RETRY 3

static DECLARE_WAIT_QUEUE_HEAD(suspend_wq);
static DEFINE_MUTEX(amazon_cooler_mutex);
static bool suspend_wakeup_ack;

/*
 * struct cooler_platform_data
 * state : current state of the cooler
 * max_state : max state of the cooler
 * cdev : cooling device pointer
 */
struct cooler_platform_data {
	unsigned int state;
	unsigned int max_state;
	struct thermal_cooling_device *cdev;
};

int amazon_enable_throttle(struct thermal_cooling_device *cdev, int state)
{
	char data[4][32];
	char *envp[] = { data[0], data[1], data[2], data[3], NULL };

	snprintf(data[0], sizeof(data[0]), "THERMAL_STATE=%d", state);
	snprintf(data[1], sizeof(data[1]), "ID=%d", cdev->id);
	snprintf(data[2], sizeof(data[2]), "CDTYPE=%s", cdev->type);
	snprintf(data[3], sizeof(data[3]), "CDLEV=%d", state);
	kobject_uevent_env(&cdev->device.kobj, KOBJ_CHANGE, envp);

	return 0;
}

static struct thermal_zone_device *amazon_cooling_find_tz_in_state(
	struct thermal_cooling_device *cdev, unsigned long state)
{
	struct thermal_instance *instance;

	list_for_each_entry(instance, &cdev->thermal_instances, cdev_node) {
		/* return the first thermal instance in target state */
		if (instance->target == state)
			return instance->tz;
	}
	pr_warn("%s: No thermal zone found for cdev %s target state %lu",
		__func__, cdev->type, state);
	return NULL;
}

static int amazon_cooling_get_max_state(struct thermal_cooling_device *cdev,
	unsigned long *state)
{
	struct cooler_platform_data *pdata = cdev->devdata;
	*state = pdata->max_state;
	return 0;
}

static int amazon_cooling_get_cur_state(struct thermal_cooling_device *cdev,
	unsigned long *state)
{
	struct cooler_platform_data *pdata = cdev->devdata;
	*state = pdata->state;
	return 0;
}

static int amazon_suspend_cooler_kthread(void *data)
{
	struct thermal_cooling_device *suspend_cdev = data;
	unsigned long cur_state;
	int ret;
	unsigned int suspend_retry = 0;

	/*
	 * The set_suspend_wakeup_timer_s() is a helper function that calls
	 * mt8167 specific functions to enable/set suspend ctrl wakeup timer.
	 * We can expand this to non mt8167 platforms by replacing
	 * set_suspend_wakeup_timer_s() call to set ALARM_REALTIME
	 * alarm timer to wakeup the system from suspend.
	 *
	 * Please not that if alarm timer is used the timer should be
	 * set before every call to pm_suspend().
	 */

/*
	ret = set_suspend_wakeup_timer_s(SUSPEND_COOLER_SLEEP_TIME_S);
	if (ret) {
		pr_err("Suspend cooler wakeup time set failed with err(%d) - Rebooting", ret);
		orderly_reboot();
		return 0;
	}
*/
	do {
		amazon_cooling_get_cur_state(suspend_cdev, &cur_state);
		if (cur_state) {
			pr_crit("Suspend cooler state(%lu) - Suspending", cur_state);
			suspend_wakeup_ack = false;
			ret = pm_suspend(PM_SUSPEND_MEM);
			if (ret) {
				pr_err("Suspend failed with err(%d)", ret);
				suspend_retry++;
			} else {
				suspend_retry = 0;
				ret = wait_event_timeout(suspend_wq,
					suspend_wakeup_ack,
					msecs_to_jiffies(
					SUSPEND_COOLER_SLEEP_TIMEOUT_MS));
				if (!ret) {
					pr_crit("Did not recevie wakeup callback - Rebooting");
					orderly_reboot();
					return 0;
				}
			}
			/*
			 * Wait here for temp readings from drivers to
			 * stabilize before checking suspend cooler state.
			 */
			msleep(SUSPEND_COOLER_AWAKE_TIME_MS);
		} else {
			pr_crit("Suspend cooler state(%lu) - Rebooting", cur_state);
			orderly_reboot();
			return 0;
		}
	} while (suspend_retry < SUSPEND_COOLER_SUSPEND_MAX_RETRY);

	pr_crit("Unable to suspend the system - Rebooting");
	orderly_reboot();
	return 0;
}

static int amazon_cooler_pm_notify(struct notifier_block *nb,
				unsigned long mode, void *_unused)
{
	switch (mode) {
	case PM_POST_SUSPEND:
		suspend_wakeup_ack = true;
		wake_up(&suspend_wq);
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block amazon_cooler_pm_nb = {
	.notifier_call = amazon_cooler_pm_notify
};

static int amazon_cooling_set_cur_state(struct thermal_cooling_device *cdev,
	unsigned long state)
{
	struct cooler_platform_data *pdata = cdev->devdata;
	struct thermal_zone_device *tz;
	struct task_struct *amazon_suspend_cooler_task;
	static bool suspend_cooler_in_suspend_state;

	mutex_lock(&amazon_cooler_mutex);
	if (pdata->state != state && !suspend_cooler_in_suspend_state) {
		amazon_enable_throttle(cdev, state);
		if (!strcmp(cdev->type, SUSPEND_COOLER) &&
				state == SUSPEND_COOLER_SUSPEND_STATE) {
			suspend_cooler_in_suspend_state = true;
			tz = amazon_cooling_find_tz_in_state(cdev, state);
			if (tz)
				amazon_thermal_set_shutdown_reason(tz);
			amazon_suspend_cooler_task =
				kthread_create(amazon_suspend_cooler_kthread,
					cdev, "amazon_suspend_cooler_kthread");
			wake_up_process(amazon_suspend_cooler_task);
		}
	}
	mutex_unlock(&amazon_cooler_mutex);

	pdata->state = state;

	return 0;
}

static struct thermal_cooling_device_ops amazon_cooling_ops = {
	.get_max_state = amazon_cooling_get_max_state,
	.get_cur_state = amazon_cooling_get_cur_state,
	.set_cur_state = amazon_cooling_set_cur_state,
};

static int amazon_cooling_probe(struct platform_device *pdev)
{
	struct cooler_platform_data *pdata;
	struct device_node *np;
	int ret;

	np = pdev->dev.of_node;
	if (!np) {
		dev_err(&pdev->dev, "%s: Error no of_node\n", __func__);
		return -EINVAL;
	}

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "%s: No Memory \n", __func__);
		return -ENOMEM;
	}

	ret = of_property_read_u32(np, "max_state", &pdata->max_state);
	if (ret) {
		dev_info(&pdev->dev, "%s: max state not set %d\n", __func__, ret);
		return -EINVAL;
	}

	pdata->cdev = thermal_of_cooling_device_register(np, (char *)np->name,
					pdata, &amazon_cooling_ops);

	if (IS_ERR(pdata->cdev)) {
		dev_err(&pdev->dev,
				"Failed to register amazon cooling device\n");
		return PTR_ERR(pdata->cdev);
	}

	platform_set_drvdata(pdev, pdata->cdev);
	dev_info(&pdev->dev, "Cooling device registered: %s\n",
		pdata->cdev->type);

	return 0;
}

static int amazon_cooling_remove(struct platform_device *pdev)
{
	struct thermal_cooling_device *cdev = platform_get_drvdata(pdev);

	thermal_cooling_device_unregister(cdev);

	return 0;
}

static const struct of_device_id amazon_cooler_of_match[] = {
	{.compatible = "amazon,thermal_cooler", },
	{ },
};

static struct platform_driver amazon_cooling_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "amazon_thermal_cooler",
		.of_match_table = amazon_cooler_of_match,
	},
	.probe = amazon_cooling_probe,
	.remove = amazon_cooling_remove,
};

static int __init amazon_cooling_init(void)
{
	int ret;

	ret = register_pm_notifier(&amazon_cooler_pm_nb);
	if (ret)
		pr_err("Unable to register pm notifier (%d)\n", ret);

	ret = platform_driver_register(&amazon_cooling_driver);
	if (ret) {
		pr_err("Unable to register amazon cooling driver (%d)\n", ret);
		return ret;
	}

	return ret;
}

static void __exit amazon_cooling_exit(void)
{
	platform_driver_unregister(&amazon_cooling_driver);
}

module_init(amazon_cooling_init);
module_exit(amazon_cooling_exit);
MODULE_DESCRIPTION("Amazon cooling thermal driver");
MODULE_AUTHOR("Tarun Karra <tkkarra@amazon.com>");
MODULE_LICENSE("GPL");
