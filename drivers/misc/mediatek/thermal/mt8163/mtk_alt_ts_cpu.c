/*
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>

#include "inc/mtk_ts_cpu.h"

#include <linux/platform_data/mtk_thermal.h>
#include <linux/thermal_framework.h>

#define DRIVER_NAME "alt_cpu-thermal"
#define THERMAL_NAME "alt_cpu"
#define MASK (0x0FFF)

static DEFINE_MUTEX(therm_lock);

static int alt_cpu_thermal_get_temp(struct thermal_zone_device *thermal,
				   unsigned long *t)
{
	*t = tscpu_get_cpu_current_temperature();
	return 0;
}

static int alt_cpu_thermal_get_mode(struct thermal_zone_device *thermal,
				   enum thermal_device_mode *mode)
{
	struct alt_cpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!pdata)
		return -EINVAL;

	mutex_lock(&therm_lock);
	*mode = pdata->mode;
	mutex_unlock(&therm_lock);
	return 0;
}

static int alt_cpu_thermal_set_mode(struct thermal_zone_device *thermal,
				   enum thermal_device_mode mode)
{
	struct alt_cpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	mutex_lock(&therm_lock);
	pdata->mode = mode;
	if (mode == THERMAL_DEVICE_DISABLED) {
		tzone->tz->polling_delay = 0;
		thermal_zone_device_update(tzone->tz);
		mutex_unlock(&therm_lock);
		return 0;
	}
	schedule_work(&tzone->therm_work);
	mutex_unlock(&therm_lock);
	return 0;
}

static int alt_cpu_thermal_get_trip_type(struct thermal_zone_device *thermal,
					int trip,
					enum thermal_trip_type *type)
{
	struct alt_cpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;

	return THERMAL_TRIP_ACTIVE;
}

static int alt_cpu_thermal_get_trip_temp(struct thermal_zone_device *thermal,
					int trip,
					unsigned long *temp)
{
	struct alt_cpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;
	*temp = pdata->trips[trip].temp;
	return 0;
}

static int alt_cpu_thermal_set_trip_temp(struct thermal_zone_device *thermal,
					int trip,
					 unsigned long temp)
{
	struct alt_cpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;
	pdata->trips[trip].temp = temp;
	return 0;
}

static int alt_cpu_thermal_get_crit_temp(struct thermal_zone_device *thermal,
					unsigned long *temp)
{
	int i;
	struct alt_cpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	for (i = 0; i < THERMAL_MAX_TRIPS; i++) {
		if (pdata->trips[i].type == THERMAL_TRIP_CRITICAL) {
			*temp = pdata->trips[i].temp;
			return 0;
		}
	}
	return -EINVAL;
}

static int alt_cpu_thermal_get_trip_hyst(struct thermal_zone_device *thermal,
					int trip,
					unsigned long *hyst)
{
	struct alt_cpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;
	*hyst = pdata->trips[trip].hyst;
	return 0;
}

static int alt_cpu_thermal_set_trip_hyst(struct thermal_zone_device *thermal,
					int trip,
					unsigned long hyst)
{
	struct alt_cpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;
	pdata->trips[trip].hyst = hyst;
	return 0;
}

static int alt_cpu_match_cdev(struct mtk_thermal_platform_data *pdata,
					struct thermal_cooling_device *cdev)
{
	int i;

	if (!strlen(cdev->type))
		return -EINVAL;

	for (i = 0; i < pdata->num_cdevs; i++)
		if (!strcmp(cdev->type, pdata->cdevs[i]))
			return 0;

	return -ENODEV;
}

static int alt_cpu_cdev_bind(struct thermal_zone_device *thermal,
			      struct thermal_cooling_device *cdev)
{
	struct alt_cpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;
	int i, ret = -EINVAL;

	if (alt_cpu_match_cdev(pdata, cdev))
		return ret;

	for (i = 0; i < pdata->num_trips; i++) {
		ret = thermal_zone_bind_cooling_device(thermal,
						       i,
						       cdev,
						       i+1,
						       i);
		dev_info(&cdev->device, "%s bind to %d: %d-%s\n", cdev->type,
			 i, ret, ret ? "fail" : "succeed");
	}

	return ret;
}

static int alt_cpu_cdev_unbind(struct thermal_zone_device *thermal,
				struct thermal_cooling_device *cdev)
{
	struct alt_cpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;
	int i, ret = -EINVAL;

	if (alt_cpu_match_cdev(pdata, cdev))
		return ret;

	for (i = 0; i < pdata->num_trips; i++) {
		ret = thermal_zone_unbind_cooling_device(thermal, i, cdev);
		dev_info(&cdev->device, "%s unbind from %d: %s\n", cdev->type,
			 i, ret ? "fail" : "succeed");
	}
	return ret;
}

static struct thermal_zone_device_ops alt_cpu_tz_dev_ops = {
	.bind = alt_cpu_cdev_bind,
	.unbind = alt_cpu_cdev_unbind,
	.get_temp = alt_cpu_thermal_get_temp,
	.get_mode = alt_cpu_thermal_get_mode,
	.set_mode = alt_cpu_thermal_set_mode,
	.get_trip_type = alt_cpu_thermal_get_trip_type,
	.get_trip_temp = alt_cpu_thermal_get_trip_temp,
	.set_trip_temp = alt_cpu_thermal_set_trip_temp,
	.get_crit_temp = alt_cpu_thermal_get_crit_temp,
	.get_trip_hyst = alt_cpu_thermal_get_trip_hyst,
	.set_trip_hyst = alt_cpu_thermal_set_trip_hyst,
};

static ssize_t trips_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct thermal_zone_device *thermal =
		container_of(dev, struct thermal_zone_device, device);

	return sprintf(buf, "%d\n", thermal->trips);
}

static ssize_t trips_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int trips = 0;
	struct thermal_zone_device *thermal =
		container_of(dev, struct thermal_zone_device, device);
	struct alt_cpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;
	if (sscanf(buf, "%d\n", &trips) != 1)
		return -EINVAL;
	if (trips < 0)
		return -EINVAL;

	pdata->num_trips = trips;
	thermal->trips = pdata->num_trips;
	return count;
}

static ssize_t polling_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct thermal_zone_device *thermal =
		container_of(dev, struct thermal_zone_device, device);
	return sprintf(buf, "%d\n", thermal->polling_delay);
}

static ssize_t polling_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int polling_delay = 0;
	struct thermal_zone_device *thermal =
		container_of(dev, struct thermal_zone_device, device);
	struct alt_cpu_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;

	if (sscanf(buf, "%d\n", &polling_delay) != 1)
		return -EINVAL;
	if (polling_delay < 0)
		return -EINVAL;

	pdata->polling_delay = polling_delay;
	thermal->polling_delay = pdata->polling_delay;
	thermal_zone_device_update(thermal);
	return count;
}

static DEVICE_ATTR(trips, S_IRUGO | S_IWUSR, trips_show, trips_store);
static DEVICE_ATTR(polling, S_IRUGO | S_IWUSR, polling_show, polling_store);

static int alt_cpu_create_sysfs(struct alt_cpu_thermal_zone *tzone)
{
	int ret = 0;

	ret = device_create_file(&tzone->tz->device, &dev_attr_polling);
	if (ret)
		pr_err("%s Failed to create polling attr\n", __func__);
	ret = device_create_file(&tzone->tz->device, &dev_attr_trips);
	if (ret)
		pr_err("%s Failed to create trips attr\n", __func__);
	return ret;
}

static void alt_cpu_thermal_work(struct work_struct *work)
{
	struct alt_cpu_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata;

	mutex_lock(&therm_lock);
	tzone = container_of(work, struct alt_cpu_thermal_zone, therm_work);
	if (!tzone)
		return;
	pdata = tzone->pdata;
	if (!pdata)
		return;
	if (pdata->mode == THERMAL_DEVICE_ENABLED)
		thermal_zone_device_update(tzone->tz);
	mutex_unlock(&therm_lock);
}

static int alt_cpu_thermal_probe(struct platform_device *pdev)
{
	int ret;

	struct alt_cpu_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata = dev_get_platdata(&pdev->dev);

	if (!pdata)
		return -EINVAL;

	tzone = devm_kzalloc(&pdev->dev, sizeof(*tzone), GFP_KERNEL);
	if (!tzone)
		return -ENOMEM;
	memset(tzone, 0, sizeof(*tzone));

	tzone->pdata = pdata;
	tzone->tz = thermal_zone_device_register(THERMAL_NAME,
						 THERMAL_MAX_TRIPS,
						 MASK,
						 tzone,
						 &alt_cpu_tz_dev_ops,
						 &pdata->tzp,
						 0,
						 pdata->polling_delay);
	if (IS_ERR(tzone->tz)) {
		pr_err("%s Failed to register thermal zone device\n", __func__);
		kfree(tzone);
		return -EINVAL;
	}

	tzone->tz->trips = pdata->num_trips;
	ret = alt_cpu_create_sysfs(tzone);
	INIT_WORK(&tzone->therm_work, alt_cpu_thermal_work);
	platform_set_drvdata(pdev, tzone);
	pdata->mode = THERMAL_DEVICE_ENABLED;
	return ret;
}

static int alt_cpu_thermal_remove(struct platform_device *pdev)
{
	struct alt_cpu_thermal_zone *tzone = platform_get_drvdata(pdev);

	if (tzone) {
		cancel_work_sync(&tzone->therm_work);
		if (tzone->tz)
			thermal_zone_device_unregister(tzone->tz);
		kfree(tzone);
	}
	return 0;
}

static struct platform_driver alt_cpu_thermal_zone_driver = {
	.probe = alt_cpu_thermal_probe,
	.remove = alt_cpu_thermal_remove,
	.suspend = NULL,
	.resume = NULL,
	.shutdown   = NULL,
	.driver     = {
		.name  = THERMAL_NAME,
		.owner = THIS_MODULE,
	},
};

static struct mtk_thermal_platform_data alt_cpu_thermal_data = {
	.num_trips = 8,
	.mode = THERMAL_DEVICE_DISABLED,
	.polling_delay = 1000,
	.num_cdevs = 1,
	.cdevs[0] = "wifi",
};

static struct platform_device alt_cpu_thermal_zone_device = {
	.name = THERMAL_NAME,
	.id = -1,
	.dev = {
		.platform_data = &alt_cpu_thermal_data,
	},
};

static int __init alt_cpu_thermal_init(void)
{
	int ret;

	ret = platform_driver_register(&alt_cpu_thermal_zone_driver);
	if (ret) {
		pr_err("Unable to register alt_cpu_thermal zone driver (%d)\n",
			ret);
		return ret;
	}

	ret = platform_device_register(&alt_cpu_thermal_zone_device);
	if (ret) {
		pr_err("Unable to register alt_cpu_thermal zone driver (%d)\n",
			ret);
		platform_driver_unregister(&alt_cpu_thermal_zone_driver);
	}

	return ret;
}
static void __exit alt_cpu_thermal_exit(void)
{
	platform_driver_unregister(&alt_cpu_thermal_zone_driver);
	platform_device_unregister(&alt_cpu_thermal_zone_device);
}

module_init(alt_cpu_thermal_init);
module_exit(alt_cpu_thermal_exit);

MODULE_DESCRIPTION("Alternate cpu thermal zone driver");
MODULE_LICENSE("GPL");
