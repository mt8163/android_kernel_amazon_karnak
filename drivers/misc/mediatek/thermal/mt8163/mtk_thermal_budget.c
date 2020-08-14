#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/wait.h>
#include <linux/device.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "mt_cpufreq.h"

#include <linux/platform_data/mtk_thermal.h>

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>

#ifndef THERMO_METRICS_STR_LEN
#define THERMO_METRICS_STR_LEN 128
#endif
#endif

#define MAXIMUM_CPU_POWER 4600
#define MAX_NUM_MTK_COOLER_BUDGET 2

struct cooler_sort_list thermal_budget_list_head;

struct thermal_dev_t {
	int major, minor;
	int flag;
	dev_t devno;
	struct class *cls;
	struct mutex mutex;
	struct cdev cdev;
	wait_queue_head_t queue;
	struct mtk_cooler_platform_data *pdata;
};

static struct thermal_dev_t *tdev;
static int _thermal_budget;
static int _thermal_budget_cooler;
static int _thermal_budget_cpu;

static DEFINE_MUTEX(notify_mutex);

#define USE_KERNEL_API_DIRECTLY 1

static void _thermal_budget_notify(int budget1, int budget2)
{
	int thermal_budget = min(budget1, budget2);

	if (thermal_budget == _thermal_budget)
		return;

	_thermal_budget = thermal_budget;

#if USE_KERNEL_API_DIRECTLY
		mt_cpufreq_thermal_protect(
(_thermal_budget < MAXIMUM_CPU_POWER) ? _thermal_budget : 0);
#else
	if (tdev) {
		tdev->flag = 1;
		wake_up_interruptible(&tdev->queue);
	}
#endif
}

void thermal_budget_notify(int budget)
{
	mutex_lock(&notify_mutex);

	_thermal_budget_cpu = budget;
	if (budget == 0)
		_thermal_budget_cpu = MAXIMUM_CPU_POWER;
	_thermal_budget_notify(_thermal_budget_cpu, _thermal_budget_cooler);

	mutex_unlock(&notify_mutex);
}

int thermal_budget_get(void)
{
	return _thermal_budget;
}

static int thermal_budget_open(struct inode *inod, struct file *filp)
{
	return 0;
}

static ssize_t thermal_budget_write(struct file *filp,
				    const char __user *buf,
				    size_t len,
				    loff_t *off)
{
	unsigned int budget = 0;
	int ret;

	ret = kstrtouint_from_user(buf, len, 10, &budget);
	if (budget)
		thermal_budget_notify(budget);
	else
		thermal_budget_notify(MAXIMUM_CPU_POWER);
	return len;
}

static ssize_t thermal_budget_read(struct file *filp,
				   char __user *buf,
				   size_t len,
				   loff_t *off)
{
	char budget[30];
	int s;

	wait_event_interruptible(tdev->queue, (tdev->flag == 1));
	tdev->flag = 0;
	s = snprintf(budget, 30, "%d\n", _thermal_budget);
	if (s > len)
		s = len;
	if (copy_to_user(buf, budget, s))
		return -EFAULT;
	return s;
}

static unsigned int thermal_budget_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(filp, &tdev->queue, wait);
	if (tdev->flag == 1)
		mask = POLLIN | POLLRDNORM;
	return mask;
}

static int thermal_budget_release(struct inode *inod, struct file *filp)
{
	return 0;
}

static ssize_t thermal_budget_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	return sprintf(buf, "%d\n", _thermal_budget);
}

static int thermal_budget_get_max_state(struct thermal_cooling_device *cdev,
			   unsigned long *state)
{
	struct mtk_cooler_platform_data *pdata = cdev->devdata;
	*state = pdata->max_state;
	return 0;
}

static int thermal_budget_get_cur_state(struct thermal_cooling_device *cdev,
			   unsigned long *state)
{
	struct mtk_cooler_platform_data *pdata = cdev->devdata;
	*state = pdata->state;
	return 0;
}

static int thermal_budget_set_cur_state(struct thermal_cooling_device *cdev,
			   unsigned long state)
{
	int level;
	struct mtk_cooler_platform_data *pdata = cdev->devdata;

#if 0
#ifdef CONFIG_AMAZON_METRICS_LOG
	char mBuf[THERMO_METRICS_STR_LEN];
	char vBuf[THERMO_METRICS_STR_LEN];
	long timer_value = 0;
#endif
#endif

	if (pdata->state == state)
		return 0;

	mutex_lock(&notify_mutex);

	pdata->state = (state > pdata->max_state) ? pdata->max_state : state;
	if (!pdata->state)
		level = MAXIMUM_CPU_POWER;
	else
		level = pdata->levels[pdata->state - 1];

	pdata->level = level;
	level = thermal_level_compare(pdata, &thermal_budget_list_head, true);
	_thermal_budget_cooler = level;
	_thermal_budget_notify(_thermal_budget_cpu, level);

#if 0
#ifdef CONFIG_AMAZON_METRICS_LOG
	if (cdev->prev_state != state) {
		ktime_t delta = ktime_sub(ktime_get(), cdev->last_time);

		snprintf(mBuf, THERMO_METRICS_STR_LEN,
			 "syseng_board_status:tempstat:soc_ts%d_time=%d:CT;1:NR",
			 (int) cdev->prev_state,
			 (int) ktime_to_ms(delta));
		log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", mBuf);

		timer_value = ((long)ktime_to_ms(ktime_get()))/MSEC_PER_SEC;
		snprintf(vBuf,
			 THERMO_METRICS_STR_LEN,
			 "zone%d",
			 (int) state);
		log_timer_to_vitals(ANDROID_LOG_INFO,
				    "thermal engine",
				    "thermal",
				    "time_in_thermal_bucket",
				    vBuf,
				    timer_value,
				    "s",
				    VITALS_TIME_BUCKET);
		cdev->last_time = ktime_get();
		cdev->prev_state = state;
	}
#endif
#endif
	mutex_unlock(&notify_mutex);

	return 0;
}

static ssize_t levels_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct thermal_cooling_device *cdev =
container_of(dev, struct thermal_cooling_device, device);
	struct mtk_cooler_platform_data *pdata = cdev->devdata;
	int i;
	int offset = 0;

	if (!pdata)
		return -EINVAL;
	for (i = 0; i < THERMAL_MAX_TRIPS; i++)
		offset += sprintf(
buf + offset, "%d %d\n", i+1, pdata->levels[i]);
	return offset;
}
static ssize_t levels_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int level, state;
	struct thermal_cooling_device *cdev =
container_of(dev, struct thermal_cooling_device, device);
	struct mtk_cooler_platform_data *pdata = cdev->devdata;

	if (!pdata)
		return -EINVAL;
	if (sscanf(buf, "%d %d\n", &state, &level) != 2)
		return -EINVAL;
	if (state >= THERMAL_MAX_TRIPS)
		return -EINVAL;
	pdata->levels[state] = level;
	return count;
}
static struct thermal_cooling_device_ops cooling_ops = {
	.get_max_state = thermal_budget_get_max_state,
	.get_cur_state = thermal_budget_get_cur_state,
	.set_cur_state = thermal_budget_set_cur_state,
};

static const struct file_operations thermal_budget_fops = {
	.owner = THIS_MODULE,
	.open = thermal_budget_open,
	.read = thermal_budget_read,
	.write = thermal_budget_write,
	.poll = thermal_budget_poll,
	.release = thermal_budget_release,
};

static DEVICE_ATTR(thermal_budget, S_IRUGO, thermal_budget_show, NULL);
static DEVICE_ATTR(levels, S_IRUGO | S_IWUSR, levels_show, levels_store);

static int thermal_budget_probe(struct platform_device *pdev)
{
	int result;

	tdev = kzalloc(sizeof(struct thermal_dev_t), GFP_KERNEL);
	result = alloc_chrdev_region(&tdev->devno, 0, 1, "thermal_budget");
	if (result) {
		pr_err("%s: Can't get major %d\n", __func__, tdev->major);
		return result;
	}

	cdev_init(&tdev->cdev, &thermal_budget_fops);
	tdev->cdev.owner = THIS_MODULE;

	result = cdev_add(&tdev->cdev, tdev->devno, 1);
	if (result) {
		pr_err("%s: Failed to add thermal_budget device\n", __func__);
		unregister_chrdev_region(tdev->devno, 1);
		return result;
	}

	tdev->cls = class_create(THIS_MODULE, "thermal_notify");
	if (IS_ERR(tdev->cls)) {
		pr_alert("thermal_budget: failed in creating class\n");
		cdev_del(&tdev->cdev);
		unregister_chrdev_region(tdev->devno, 1);
		return -EINVAL;
	}

	tdev->major = MAJOR(tdev->devno);
	tdev->minor = MINOR(tdev->devno);
	device_create(tdev->cls, NULL, MKDEV(tdev->major, 0),
NULL, "thermal_budget");

	mutex_init(&tdev->mutex);
	init_waitqueue_head(&tdev->queue);
	result = device_create_file(&pdev->dev, &dev_attr_thermal_budget);
	if (result)
		pr_err("%s Failed to create thermal_budget attr\n", __func__);

	_thermal_budget = _thermal_budget_cpu =
	_thermal_budget_cooler = MAXIMUM_CPU_POWER;
	return result;
}

static int thermal_budget_remove(struct platform_device *pdev)
{
	cdev_del(&tdev->cdev);
	unregister_chrdev_region(tdev->devno, 1);
	if (tdev->cls) {
		device_destroy(tdev->cls, MKDEV(tdev->major, 0));
		class_destroy(tdev->cls);
	}
	kfree(tdev);
	return 0;
}

static struct platform_driver thermal_budget_driver = {
	.driver = {
		.name = "thermal_budget",
		.owner = THIS_MODULE,
	},
	.probe = thermal_budget_probe,
	.remove = thermal_budget_remove,
};

static struct mtk_cooler_platform_data thermal_budget_pdata[MAX_NUM_MTK_COOLER_BUDGET] = {
	{
		.type = "thermal_budget0",
		.state = 0,
		.max_state = THERMAL_MAX_TRIPS,
		.level = MAXIMUM_CPU_POWER,
		.levels = {
			MAXIMUM_CPU_POWER, MAXIMUM_CPU_POWER, MAXIMUM_CPU_POWER,
			MAXIMUM_CPU_POWER, MAXIMUM_CPU_POWER, MAXIMUM_CPU_POWER,
			MAXIMUM_CPU_POWER, MAXIMUM_CPU_POWER, MAXIMUM_CPU_POWER,
			MAXIMUM_CPU_POWER, MAXIMUM_CPU_POWER, MAXIMUM_CPU_POWER
		},
	},
	{
		.type = "thermal_budget1",
		.state = 0,
		.max_state = THERMAL_MAX_TRIPS,
		.level = MAXIMUM_CPU_POWER,
		.levels = {
			MAXIMUM_CPU_POWER, MAXIMUM_CPU_POWER, MAXIMUM_CPU_POWER,
			MAXIMUM_CPU_POWER, MAXIMUM_CPU_POWER, MAXIMUM_CPU_POWER,
			MAXIMUM_CPU_POWER, MAXIMUM_CPU_POWER, MAXIMUM_CPU_POWER,
			MAXIMUM_CPU_POWER, MAXIMUM_CPU_POWER, MAXIMUM_CPU_POWER
		},
	},

};

static int mtk_cooler_budget_register(void)
{
        int i;
	struct mtk_cooler_platform_data *pdata = NULL;
	struct cooler_sort_list *tz_list = NULL;

	INIT_LIST_HEAD(&thermal_budget_list_head.list);

	for (i=0; i<MAX_NUM_MTK_COOLER_BUDGET; i++) {
		pdata = thermal_budget_pdata+i;
 		pdata->cdev = thermal_cooling_device_register(pdata->type,
						    pdata,
						    &cooling_ops);
		if (!pdata->cdev) {
			pr_err("%s Failed to create thermal_budget cooling device\n",
				__func__);
			return -EINVAL;
		}

		device_create_file(&pdata->cdev->device, &dev_attr_levels);

		tz_list = kzalloc(sizeof(struct cooler_sort_list), GFP_KERNEL);
		tz_list->pdata = pdata;
		list_add(&tz_list->list, &thermal_budget_list_head.list);
        }

        return 0;
}

static void mtk_cooler_budget_unregister(void)
{
        int i;
	struct mtk_cooler_platform_data *pdata = NULL;
	struct cooler_sort_list *cooler_list, *tmp;

        for (i=0; i<MAX_NUM_MTK_COOLER_BUDGET; i++) {
		pdata = thermal_budget_pdata+i;
 		thermal_cooling_device_unregister(pdata->cdev);
        }


	list_for_each_entry_safe(cooler_list, tmp, &thermal_budget_list_head.list, list) {
		list_del(&cooler_list->list);
		kfree(cooler_list);
	}

	cooler_list = NULL;
}

static struct platform_device thermal_budget_device = {
	.name = "thermal_budget",
	.id = -1,
};

static int __init thermal_budget_init(void)
{
	int ret;

	ret = platform_driver_register(&thermal_budget_driver);
	if (ret) {
		pr_err("Unable to register thermal budget driver (%d)\n", ret);
		return ret;
	}
	ret = platform_device_register(&thermal_budget_device);
	if (ret) {
		pr_err("Unable to register thermal budget device (%d)\n", ret);
		return ret;
	}

	ret = mtk_cooler_budget_register();
	if(ret) {
		pr_err("Unable to register thermal budget cooler device (%d)\n", ret);
		return ret;
	}

	return 0;
}

static void __exit thermal_budget_exit(void)
{
	platform_driver_unregister(&thermal_budget_driver);
	platform_device_unregister(&thermal_budget_device);
	mtk_cooler_budget_unregister();
}

module_init(thermal_budget_init);
module_exit(thermal_budget_exit);
