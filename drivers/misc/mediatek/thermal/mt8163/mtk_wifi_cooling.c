/*
 * Copyright (C) 2015-2017 Amazon.com, Inc. or its affiliates.
 * All Rights Reserved
 * Author: Akwasi Boateng <boatenga@lab126.com>
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
#include <../drivers/thermal/thermal_core.h>
#include <linux/platform_data/mtk_thermal.h>

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#define METRICS_STR_LEN 128
#endif

#define PREFIX "thermalthrottle:def"

#define MAX_LEVEL	3
#define MIN_LEVEL	0
#define MAX_STATE	8

int wifi_enable_throttle(struct thermal_cooling_device *cdev, int state)
{
	char data[4][32];
	char *envp[] = { data[0], data[1], data[2], data[3], NULL };
	int cdev_level;
	struct mtk_cooler_platform_data *pdata = cdev->devdata;
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[METRICS_STR_LEN];
#endif

	cdev_level = pdata->levels[state];

	snprintf(data[0], sizeof(data[0]), "THERMAL_STATE=%d", state);
	snprintf(data[1], sizeof(data[1]), "ID=%d", cdev->id);
	snprintf(data[2], sizeof(data[2]), "CDTYPE=%s", cdev->type);
	snprintf(data[3], sizeof(data[3]), "CDLEV=%d", cdev_level);
	kobject_uevent_env(&cdev->device.kobj, KOBJ_CHANGE, envp);

#ifdef CONFIG_AMAZON_METRICS_LOG
	snprintf(buf, METRICS_STR_LEN,
		"%s:cooler_%s_throttling,state=%d;level=%d;CT;1:NR",
		PREFIX, cdev->type, state, cdev_level);
	log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", buf);
#endif

	dev_info(&cdev->device, "wifi_enable_throttle uevent state=%d level=%d\n",
		state, cdev_level);
	return 0;
}

static int mtk_wifi_cooling_get_max_state(struct thermal_cooling_device *cdev,
	unsigned long *state)
{
	struct mtk_cooler_platform_data *pdata = cdev->devdata;
	*state = pdata->max_state;
	return 0;
}

static int mtk_wifi_cooling_get_cur_state(struct thermal_cooling_device *cdev,
	unsigned long *state)
{
	struct mtk_cooler_platform_data *pdata = cdev->devdata;
	*state = pdata->state;
	return 0;
}

static int mtk_wifi_cooling_set_cur_state(struct thermal_cooling_device *cdev,
	unsigned long state)
{
	struct mtk_cooler_platform_data *pdata = cdev->devdata;

	if (pdata->state != state)
		wifi_enable_throttle(cdev, state);
	pdata->state = state;

	return 0;
}
static ssize_t levels_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct thermal_cooling_device *cdev =
		container_of(dev, struct thermal_cooling_device, device);
	struct mtk_cooler_platform_data *pdata = cdev->devdata;
	struct thermal_instance *instance;
	int offset = 0;

	if (!pdata)
		return -EINVAL;

	list_for_each_entry(instance, &cdev->thermal_instances, cdev_node) {
		offset += sprintf(buf + offset, "%d %d\n",
			instance->trip, pdata->levels[instance->trip]);
	}
	return offset;
}

static ssize_t levels_store(struct device *dev, struct device_attribute *attr,
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
	if (state > MAX_STATE || state < 0)
		return -EINVAL;
	if (level < MIN_LEVEL || level > MAX_LEVEL)
		return -EINVAL;

	pdata->levels[state] = level;
	return count;
}

static DEVICE_ATTR(levels, S_IRUGO | S_IWUSR, levels_show, levels_store);


static struct thermal_cooling_device_ops mtk_wifi_cooling_ops = {
	.get_max_state = mtk_wifi_cooling_get_max_state,
	.get_cur_state = mtk_wifi_cooling_get_cur_state,
	.set_cur_state = mtk_wifi_cooling_set_cur_state,
};

static int mtk_wifi_cooling_probe(struct platform_device *pdev)
{
	struct mtk_cooler_platform_data *pdata = dev_get_platdata(&pdev->dev);

	if (!pdata)
		return -EINVAL;

	pdata->cdev = thermal_cooling_device_register(pdata->type, pdata,
					&mtk_wifi_cooling_ops);

	if (IS_ERR(pdata->cdev)) {
		dev_err(&pdev->dev,
				"Failed to register wifi cooling device\n");
		return PTR_ERR(pdata->cdev);
	}

	platform_set_drvdata(pdev, pdata->cdev);
	device_create_file(&pdata->cdev->device, &dev_attr_levels);
	dev_info(&pdev->dev, "Cooling device registered: %s\n",
		pdata->cdev->type);

	return 0;
}

static int mtk_wifi_cooling_remove(struct platform_device *pdev)
{
	struct thermal_cooling_device *cdev = platform_get_drvdata(pdev);

	thermal_cooling_device_unregister(cdev);

	return 0;
}

static struct platform_driver mtk_wifi_cooling_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "mtk-wifi-cooling",
	},
	.probe = mtk_wifi_cooling_probe,
	.remove = mtk_wifi_cooling_remove,
};

static struct mtk_cooler_platform_data mtk_wifi_cooling_pdata = {
	.type = "wifi",
	.state = 0,
	.max_state = MAX_STATE,
};

static struct platform_device mtk_wifi_cooling_device = {
	.name = "mtk-wifi-cooling",
	.id = -1,
	.dev = {
		.platform_data = &mtk_wifi_cooling_pdata,
	},
};

static int __init mtk_wifi_cooling_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_wifi_cooling_driver);
	if (ret) {
		pr_err("Unable to register wifi cooling driver (%d)\n", ret);
		return ret;
	}
	ret = platform_device_register(&mtk_wifi_cooling_device);
	if (ret) {
		pr_err("Unable to register wifi cooling device (%d)\n", ret);
		platform_driver_unregister(&mtk_wifi_cooling_driver);
	}

	return ret;
}

static void __exit mtk_wifi_cooling_exit(void)
{
	platform_driver_unregister(&mtk_wifi_cooling_driver);
	platform_device_unregister(&mtk_wifi_cooling_device);
}

module_init(mtk_wifi_cooling_init);
module_exit(mtk_wifi_cooling_exit);
MODULE_DESCRIPTION("WiFi cooling thermal driver");
MODULE_LICENSE("GPL");
