/*
 *  virtual_sensor_gov.c - A simple thermal throttling governor
 *
 *  Copyright (C) 2015 Amazon.com, Inc. or its affiliates. All Rights Reserved
 *  Author: Akwasi Boateng <boatenga@amazon.com>
 *
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 *
 */

#include <linux/thermal.h>
#include <linux/module.h>
#include "thermal_core.h"
#include <linux/platform_data/mtk_thermal.h>

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#define VIRTUAL_SENSOR_GOV_METRICS_STR_LEN 128
#endif

#define PREFIX "thermalthrottle:def"

/**
 * virtual_sensor_throttle
 * @tz - thermal_zone_device
 * @trip - the trip point
 *
 */
static int virtual_sensor_throttle(struct thermal_zone_device *tz, int trip)
{
	int trip_temp, temp;
	struct thermal_instance *tz_instance, *cd_instance;
	struct thermal_cooling_device *cdev;
	unsigned long target = 0;
	char data[3][25];
	char *envp[] = { data[0], data[1], data[2], NULL };
	unsigned long max_state;
	unsigned long cur_state;
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[VIRTUAL_SENSOR_GOV_METRICS_STR_LEN];
#endif
	struct virtual_sensor_thermal_zone *tzone = tz->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (trip)
		return 0;

	mutex_lock(&tz->lock);
	list_for_each_entry(tz_instance, &tz->thermal_instances, tz_node) {
		if (tz_instance->trip != trip)
			continue;

		cdev = tz_instance->cdev;
		cdev->ops->get_max_state(cdev, &max_state);

		mutex_lock(&cdev->lock);
		list_for_each_entry(cd_instance, &cdev->thermal_instances, cdev_node) {
			if (cd_instance->trip >= tz->trips)
				continue;

			if (trip == THERMAL_TRIPS_NONE)
				trip_temp = tz->forced_passive;
			else
				tz->ops->get_trip_temp(tz, cd_instance->trip, &trip_temp);

			if (tz->temperature >= trip_temp)
				target = cd_instance->trip + 1;

			cd_instance->target = THERMAL_NO_TARGET;
		}

		target = (target > max_state) ? max_state : target;

		cdev->ops->get_cur_state(cdev, &cur_state);
		if (cur_state < target) {
			tz_instance->target = target;
			cdev->updated = false;
		} else if (cur_state > target) {
			if (trip == THERMAL_TRIPS_NONE)
				trip_temp = tz->forced_passive;
			else
				tz->ops->get_trip_temp(tz, cur_state - 1, &trip_temp);

			if (tz->temperature <= (trip_temp - pdata->trips[cur_state - 1].hyst)) {
				tz_instance->target = target;
				cdev->updated = false;
			} else {
				target = cur_state;
			}
		}
		mutex_unlock(&cdev->lock);

		if (cur_state != target) {
			thermal_cdev_update(cdev);

			if (target)
				tz->ops->get_trip_temp(tz, target - 1, &temp);
			else
				tz->ops->get_trip_temp(tz, target, &temp);

			if (target) {
				pr_warning("VS cooler %s throttling, cur_temp=%d, trip_temp=%d, target=%lu\n",
							cdev->type, tz->temperature, temp, target);
#ifdef CONFIG_AMAZON_METRICS_LOG
				snprintf(buf, VIRTUAL_SENSOR_GOV_METRICS_STR_LEN,
					"%s:vs_cooler_%s_throttling=1;CT;1,cur_temp=%d;CT;1,"
					"trip_temp=%d;CT;1,target=%lu;CT;1:NR",
					PREFIX, cdev->type, tz->temperature, temp, target);
				log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", buf);
#endif
			} else {
				pr_warning("VS cooler %s unthrottling, cur_temp=%d, trip_temp=%d, target=%lu\n",
							cdev->type, tz->temperature, temp, target);
#ifdef CONFIG_AMAZON_METRICS_LOG
				snprintf(buf, VIRTUAL_SENSOR_GOV_METRICS_STR_LEN,
					"%s:vs_cooler_%s_unthrottling=1;CT;1,cur_temp=%d;CT;1,"
					"trip_temp=%d;CT;1,target=%lu;CT;1:NR",
					PREFIX, cdev->type, tz->temperature, temp, target);
				log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", buf);
#endif
			}

			snprintf(data[0], sizeof(data[0]), "TRIP=%d", trip);
			snprintf(data[1], sizeof(data[1]), "THERMAL_STATE=%ld", target);
			/*
			 * PhoneWindowManagerMetricsHelper.java may need to filter out
			 * all but one cooling device type.
			 */
			snprintf(data[2], sizeof(data[2]), "CDEV_TYPE=%s", cdev->type);
			kobject_uevent_env(&tz->device.kobj, KOBJ_CHANGE, envp);
		}
	}
	mutex_unlock(&tz->lock);
	return 0;
}

static struct thermal_governor thermal_gov_virtual_sensor = {
	.name = "virtual_sensor",
	.throttle = virtual_sensor_throttle,
};

static int __init thermal_gov_virtual_sensor_init(void)
{
	return thermal_register_governor(&thermal_gov_virtual_sensor);
}

static void __exit thermal_gov_virtual_sensor_exit(void)
{
	thermal_unregister_governor(&thermal_gov_virtual_sensor);
}

fs_initcall(thermal_gov_virtual_sensor_init);
module_exit(thermal_gov_virtual_sensor_exit);

MODULE_AUTHOR("Akwasi Boateng");
MODULE_DESCRIPTION("A simple level throttling thermal governor");
MODULE_LICENSE("GPL");
