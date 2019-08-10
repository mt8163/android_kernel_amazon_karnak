/*
 *  trip_step_wise.c - A simple thermal throttling governor
 *
 *  Copyright (C) 2015-2017 Amazon.com, Inc. or its affiliates.
 *  All Rights Reserved
 *  Author: Akwasi Boateng <boatenga@amazon.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 */

#include <linux/thermal.h>
#include <linux/module.h>
#include "thermal_core.h"

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#define METRICS_STR_LEN 128
#endif

#define PREFIX "thermalthrottle:def"

/**
 * trip_step_wise_throttle
 * @tz - thermal_zone_device
 * @trip - the trip point
 *
 */
static int trip_step_wise_throttle(struct thermal_zone_device *tz, int trip)
{
	long trip_temp;
	struct thermal_instance *tz_instance, *cd_instance;
	struct thermal_cooling_device *cdev;
	unsigned long target = 0;
	unsigned long cur_state, max_state;

	mutex_lock(&tz->lock);
	list_for_each_entry(tz_instance, &tz->thermal_instances, tz_node) {
		if (tz_instance->trip != trip)
			continue;

		cdev = tz_instance->cdev;
		mutex_lock(&cdev->lock);

		list_for_each_entry(cd_instance, &cdev->thermal_instances,
			cdev_node) {
			cd_instance->target = THERMAL_NO_TARGET;
		}

		if (trip == THERMAL_TRIPS_NONE)
			trip_temp = tz->forced_passive;
		else
			tz->ops->get_trip_temp(tz, trip, &trip_temp);

		cdev->ops->get_cur_state(cdev, &cur_state);

		if (tz->temperature >= trip_temp) {
			if (tz_instance->upper > cur_state)
				target = tz_instance->upper;
			else
				target = cur_state;
		} else {
			if (cur_state > tz_instance->lower)
				target = tz_instance->lower;
			else
				target = cur_state;
		}

		cdev->ops->get_max_state(cdev, &max_state);
		target = (target > max_state) ? max_state : target;

		tz_instance->target = target;
		cdev->updated = false;
		mutex_unlock(&cdev->lock);

		if (cur_state != target) {
#ifdef CONFIG_AMAZON_METRICS_LOG
			char buf[METRICS_STR_LEN];
			snprintf(buf, METRICS_STR_LEN,
				"%s:cooler_%s_throttling,cur_temp=%d;"
				"trip_temp=%ld;target=%lu;CT;1:NR",
				PREFIX, cdev->type, tz->temperature, trip_temp, target);
			log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", buf);
#endif

			thermal_cdev_update(cdev);
		}
	}
	mutex_unlock(&tz->lock);
	return 0;
}

static struct thermal_governor thermal_gov_trip_step_wise = {
	.name = "trip_step_wise",
	.throttle = trip_step_wise_throttle,
};

static int __init thermal_gov_trip_step_wise_init(void)
{
	return thermal_register_governor(&thermal_gov_trip_step_wise);
}

static void __exit thermal_gov_trip_step_wise_exit(void)
{
	thermal_unregister_governor(&thermal_gov_trip_step_wise);
}

fs_initcall(thermal_gov_trip_step_wise_init);
module_exit(thermal_gov_trip_step_wise_exit);

MODULE_AUTHOR("Akwasi Boateng");
MODULE_DESCRIPTION("A simple trip level throttling thermal governor");
MODULE_LICENSE("GPL");
