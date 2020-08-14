/*
 * Copyright (C) 2018 Lab126, Inc.  All rights reserved.
 * Author: Akwasi Boateng <boatenga@lab126.com>
 * Contributors:
 * Tarun Karra <tkkarra@amazon.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/thermal_framework.h>
#include <linux/cpufreq.h>
#include <thermal_core.h>
#include <amazon_thermal_life_cycle_reasons.h>

#define DMF 1000
#define NUM_OF_MOVING_AVG_PROP 4

static LIST_HEAD(global_virtual_sensor_dev_list);
static DEFINE_MUTEX(virtual_sensor_dev_lock);

static LIST_HEAD(global_virtual_thermal_zone_list);
static DEFINE_MUTEX(virtual_thermal_zone_lock);

/**
 * struct virtual_thermal_time_val  - Structure for storing ma and time
 * @time: time in jiffies
 * @value: value at a particular time
 */
struct virtual_thermal_time_val {
	unsigned long time;
	int value;
};

/**
 * struct virtual_thermal_ma  - Structure for each virtual moving average.
 * @ma_rate: moving average calculation rate in jiffies
 * @ma_alpha_p: alpha_p used in calculating alpha = (alpha_p/alpha_q)
 * @ma_alpha_p: alpha_q used in calculating alpha = (alpha_p/alpha_q)
 * @ma_roc_rate: moving average rate of change rate in jiffies
 * @base_ma: base moving average for the rate of change calculation
 * @last_ma: last moving average for the rate of change calculation
 * @ma_roc: moving average rate of change per minute
 * @is_initialized: boolean to check if first temp data is updated
 */
struct virtual_thermal_ma {
	unsigned long ma_rate;
	unsigned int ma_alpha_p;
	unsigned int ma_alpha_q;
	unsigned long ma_roc_rate;
	struct virtual_thermal_time_val base_ma;
	struct virtual_thermal_time_val last_ma;
	int ma_roc;
	bool is_initialized;
};

/**
 * struct virtual_thermal_dev  - Structure for each virtual thermal device.
 * @tdev: Thermal device node
 * @tdp:  Thermal dev parameters (offset, alpha, weight)
 * @off_temp: Thermal dev off_temp
 * @node: The list node of the virtual_sensor_dev_list
 */
struct virtual_thermal_dev {
	struct thermal_dev *tdev;
	struct thermal_dev_params tdp;
	long off_temp;
	struct platform_device *phys_sensor;
	struct list_head node;
};

/**
 * struct virtual_thermal_dev  - Structure for each virtual thermal zone.
 * @virtual_sensor_tzd: Virtual sensor thermal zone device.
 * @virtual_sensor_dev_list:  Virtual senosr device list header.
 * @node: The list node of virtual thermal zones.
 * @vs_ma: Virtual sensor moving average.
 * @lock: To protect virtual_sensor_dev_list.
 * @last_trend: last thermal trend of the tz trips.
 */
struct virtual_thermal_zone {
	struct thermal_zone_device *virtual_sensor_tzd;
	struct list_head            virtual_sensor_dev_list;
	struct list_head node;
	struct virtual_thermal_ma *vs_ma;
	struct mutex lock;
	enum thermal_trend last_trend[THERMAL_MAX_TRIPS];
};

static void send_trip_uevent(struct thermal_zone_device *tz, int trip,
				int thermal_state)
{
	char data[2][28];
	char *envp[] = { data[0], data[1], NULL};

	snprintf(data[0], sizeof(data[0]), "TRIP=%d", trip);
	snprintf(data[1], sizeof(data[1]), "THERMAL_STATE=%d", thermal_state);
	kobject_uevent_env(&tz->device.kobj, KOBJ_CHANGE, envp);
}

static int virtual_sensor_thermal_get_trend(struct thermal_zone_device *thermal,
					int trip, enum thermal_trend *trend)
{
	int trip_temp;
	int trip_hyst;
	struct virtual_thermal_zone *vtz = NULL;
	struct virtual_thermal_zone *list_vtz;

	if (trip < 0 || trip >= THERMAL_MAX_TRIPS)
		return -EINVAL;

	mutex_lock(&virtual_thermal_zone_lock);
	list_for_each_entry(list_vtz, &global_virtual_thermal_zone_list, node) {
		if (list_vtz->virtual_sensor_tzd == thermal) {
			vtz = list_vtz;
			break;
		}
	}
	mutex_unlock(&virtual_thermal_zone_lock);

	if (!vtz)
		return -EINVAL;

	if (!thermal->ops || !thermal->ops->get_trip_temp ||
		thermal->ops->get_trip_temp(thermal, trip, &trip_temp))
		return -EINVAL;

	if (!thermal->ops->get_trip_hyst ||
		thermal->ops->get_trip_hyst(thermal, trip, &trip_hyst))
		trip_hyst = 0;

	/* Check for false uevents when policy is loading up */
	if (trip_temp == 0)
		return 0;

	if (thermal->temperature >= trip_temp) {
		*trend = THERMAL_TREND_RAISE_FULL;
		if (vtz->last_trend[trip] != *trend) {
			send_trip_uevent(thermal, trip, trip+1);
			vtz->last_trend[trip] = *trend;
		}
	} else if (thermal->temperature < (trip_temp - trip_hyst)) {
		*trend = THERMAL_TREND_DROPPING;
		if (vtz->last_trend[trip] != *trend) {
			send_trip_uevent(thermal, trip, trip);
			/*
			 * The ucase thermal hal event will be sent with proper
			 * states only if a trip event is sent for earlier trip.
			 */
			if (trip > 0)
				send_trip_uevent(thermal, trip-1, trip);
			vtz->last_trend[trip] = *trend;
		}
	} else {
		*trend = THERMAL_TREND_STABLE;
		/*
		 * Don't update the last_trend when trend is stable.
		 * Otherwise, trip uevent will get triggered when the trend
		 * moves from STABLE to DROPPING even if the trip temp hasn't
		 * been breached before.
		 */
	}

	return 0;
}

static int virtual_sensor_thermal_notify(struct thermal_zone_device *thermal,
				int trip, enum thermal_trip_type trip_type)
{
	if (trip_type == THERMAL_TRIP_CRITICAL)
		amazon_thermal_set_shutdown_reason(thermal);

	return 0;
}

static int virtual_sensor_update_ma_and_return_roc(
				struct virtual_thermal_ma *vs_ma, int temp)
{
	unsigned long current_time = jiffies;

	if (!vs_ma->is_initialized) {
		vs_ma->base_ma.value = temp;
		vs_ma->base_ma.time = current_time;
		vs_ma->last_ma.value = vs_ma->base_ma.value;
		vs_ma->last_ma.time = vs_ma->base_ma.time;
		vs_ma->is_initialized = true;
	} else if (time_after_eq(current_time,
				(vs_ma->last_ma.time + vs_ma->ma_rate))) {
		/*
		 * MA(t) = alpha*temp(t) + (1-alpha)*MA(t-1)
		 * alpha = alpha_p/alpha_q
		 */
		vs_ma->last_ma.value = ((vs_ma->ma_alpha_p * temp) +
				((vs_ma->ma_alpha_q - vs_ma->ma_alpha_p) *
				vs_ma->last_ma.value)) / vs_ma->ma_alpha_q;
		vs_ma->last_ma.time = current_time;

		if (time_after_eq(current_time,
				(vs_ma->base_ma.time + vs_ma->ma_roc_rate))) {
			/*
			 * Calculate ROC only after ROC interval is passed
			 */
			vs_ma->ma_roc = ((vs_ma->last_ma.value -
				vs_ma->base_ma.value) * 60) /
				(int) (jiffies_to_msecs(vs_ma->ma_roc_rate) /
					1000);
			vs_ma->base_ma.value = vs_ma->last_ma.value;
			vs_ma->base_ma.time = vs_ma->last_ma.time;
		}
	}

	return vs_ma->ma_roc;
}

static int virtual_sensor_get_temp(void *data, int *t)
{
	struct virtual_thermal_dev *vtdev;
	long temp = 0;
	long tempv = 0;
	int alpha, offset, weight;
	struct virtual_thermal_zone *vtz = data;
	struct thermal_dev *tdev;

	mutex_lock(&vtz->lock);

	list_for_each_entry(vtdev, &vtz->virtual_sensor_dev_list, node) {
		tdev = vtdev->tdev;
		alpha = vtdev->tdp.alpha;
		offset = vtdev->tdp.offset;
		weight = vtdev->tdp.weight;

		if ((tdev != NULL) && (weight != 0)) {
			/*
			 * thermistor temp readings are used as is
			 * mtktscpu(thermal) needs a multiplier to
			 * display correct value
			 */
			temp = (tdev->dev_ops->get_temp(tdev));
			if (strcmp(tdev->name, "thermal") == 0)
				temp *= 1000;

			pr_debug("%s %s t=%ld a=%d o=%d w=%d\n", __func__,
				tdev->name, temp, alpha, offset, weight);

			if (!vtdev->off_temp) {
				vtdev->off_temp = temp - offset;
			} else {
				vtdev->off_temp = alpha * (temp - offset) +
					(DMF - alpha) * vtdev->off_temp;
				vtdev->off_temp /= DMF;
			}
			tempv += (weight * vtdev->off_temp)/DMF;

			pr_debug("%s tempv=%ld\n", __func__, tempv);
		}
	}

	if (vtz->vs_ma && tempv)
		tempv = virtual_sensor_update_ma_and_return_roc(vtz->vs_ma,
									tempv);
	mutex_unlock(&vtz->lock);

	*t = tempv;
	return 0;
}

static int virtual_sensor_set_emul_temp(void *data, int temp)
{
	struct virtual_thermal_zone *vtz = data;
	struct thermal_zone_device *tz = vtz->virtual_sensor_tzd;

	mutex_lock(&tz->lock);
	tz->emul_temperature = temp;
	mutex_unlock(&tz->lock);

	return 0;
}

static struct thermal_zone_of_device_ops virtual_sensor_ops = {
	.get_temp = virtual_sensor_get_temp,
	.set_emul_temp = virtual_sensor_set_emul_temp,
};

static ssize_t _show_params(struct device *dev,
		       struct device_attribute *devattr, char *buf)
{
	struct virtual_thermal_zone *vtz = dev_get_drvdata(dev);
	struct virtual_thermal_dev *vtdev;
	int n = 0;

	mutex_lock(&vtz->lock);

	list_for_each_entry(vtdev, &vtz->virtual_sensor_dev_list, node) {
		if (vtdev->tdev) {
			n += sprintf(buf + n,
				"%s : offset: %d alpha: %d weight: %d\n",
				vtdev->tdev->name, vtdev->tdp.offset,
				vtdev->tdp.alpha, vtdev->tdp.weight);
		} else {
			n += sprintf(buf + n,
				"(UNINITIALIZED) : offset: %d alpha: %d weight: %d\n",
				vtdev->tdp.offset, vtdev->tdp.alpha,
				vtdev->tdp.weight);
		}
	}

	mutex_unlock(&vtz->lock);

	return n;
}

static ssize_t _store_params(struct device *dev,
				      struct device_attribute *devattr,
				      const char *buf,
				      size_t count)
{
	char param[20], sensor[20];
	int value;
	struct virtual_thermal_zone *vtz = dev_get_drvdata(dev);
	struct virtual_thermal_dev *vtdev;

	if (!vtz)
		return -EINVAL;

	mutex_lock(&vtz->lock);

	if (sscanf(buf, "%20s %20s %d", sensor, param, &value) == 3) {
		list_for_each_entry(vtdev, &vtz->virtual_sensor_dev_list, node) {
			if (vtdev->tdev && !strcmp(sensor, vtdev->tdev->name)) {
				if (!strcmp(param, "offset"))
					vtdev->tdp.offset = value;
				else if (!strcmp(param, "alpha"))
					vtdev->tdp.alpha = value;
				else if (!strcmp(param, "weight"))
					vtdev->tdp.weight = value;

				mutex_unlock(&vtz->lock);
				return count;
			}
		}
	}

	mutex_unlock(&vtz->lock);

	return -EINVAL;
}

static DEVICE_ATTR(params, 0644, _show_params, _store_params);

static ssize_t _show_ma(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct virtual_thermal_zone *vtz = dev_get_drvdata(dev);
	int n = 0;

	mutex_lock(&vtz->lock);

	if (vtz->vs_ma) {
		n += sprintf(buf,
			"ma_rate: %ums ma_alpha_p: %u ma_alpha_q: %u ma_roc_rate %ums\n",
			jiffies_to_msecs(vtz->vs_ma->ma_rate),
			vtz->vs_ma->ma_alpha_p, vtz->vs_ma->ma_alpha_q,
			jiffies_to_msecs(vtz->vs_ma->ma_roc_rate));
	} else {
		n += sprintf(buf, "NA\n");
	}

	mutex_unlock(&vtz->lock);

	return n;
}

static ssize_t _store_ma(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	unsigned int ma_rate, ma_alpha_p, ma_alpha_q, ma_roc_rate;
	struct virtual_thermal_zone *vtz = dev_get_drvdata(dev);
	size_t ret;

	if (!vtz)
		return -EINVAL;

	mutex_lock(&vtz->lock);

	if ((sscanf(buf, "%u %u %u %u", &ma_rate, &ma_alpha_p,
		&ma_alpha_q, &ma_roc_rate) == NUM_OF_MOVING_AVG_PROP)
			&& ma_rate && ma_alpha_p && ma_alpha_q && ma_roc_rate) {
		if (vtz->vs_ma)
			memset(vtz->vs_ma, 0, sizeof(*vtz->vs_ma));
		else
			vtz->vs_ma = devm_kzalloc(dev,
				sizeof(*vtz->vs_ma), GFP_KERNEL);
		if (vtz->vs_ma) {
			vtz->vs_ma->ma_rate = msecs_to_jiffies(ma_rate);
			vtz->vs_ma->ma_alpha_p = ma_alpha_p;
			vtz->vs_ma->ma_alpha_q = ma_alpha_q;
			vtz->vs_ma->ma_roc_rate =
					msecs_to_jiffies(ma_roc_rate);
			ret = count;
		} else {
			ret = -ENOMEM;
		}
	} else {
		ret = -EINVAL;
	}

	mutex_unlock(&vtz->lock);

	return ret;
}

static DEVICE_ATTR(ma, 0644, _show_ma, _store_ma);

static int virtual_sensor_thermal_probe(struct platform_device *pdev)
{
	struct virtual_thermal_zone *vtz;
	struct device_node *np;
	int i, num_str, size, offset_sign, error;
	struct thermal_dev *tdev;
	struct virtual_thermal_dev *vtdev;
	struct platform_device *phys_sensor;
	struct of_phandle_args args;
	unsigned int ma_prop[NUM_OF_MOVING_AVG_PROP];

	if (!cpufreq_frequency_get_table(0)) {
		dev_info(&pdev->dev, "Frequency table not initialized. Deferring probe...\n");
		return -EPROBE_DEFER;
	}

	np = pdev->dev.of_node;
	if (!np) {
		dev_err(&pdev->dev, "%s: Error no of_node\n", __func__);
		return -EINVAL;
	}

	vtz = devm_kzalloc(&pdev->dev, sizeof(*vtz), GFP_KERNEL);
	if (!vtz)
		return -ENOMEM;

	if (of_property_read_u32(np, "sensor-list-count", &size))
		return -EINVAL;

	num_str = of_count_phandle_with_args(np, "sensor-list", NULL);
	if (!num_str) {
		dev_info(&pdev->dev, "%s: No memory\n", __func__);
		return -EINVAL;
	}

	num_str = (num_str / size);

	mutex_lock(&virtual_thermal_zone_lock);
	INIT_LIST_HEAD(&vtz->virtual_sensor_dev_list);

	for (i = 0; i < num_str ; i++) {

		if (of_parse_phandle_with_fixed_args(np, "sensor-list",
					size-1, i, &args)) {
			dev_err(&pdev->dev, "Missing the phandle args name\n");
			mutex_unlock(&virtual_thermal_zone_lock);
			return -ENODEV;
		}

		phys_sensor = of_find_device_by_node(args.np);
		if (!phys_sensor) {
			of_node_put(args.np);
			mutex_unlock(&virtual_thermal_zone_lock);
			return -EINVAL;
		}

		mutex_lock(&virtual_sensor_dev_lock);

		vtdev = devm_kzalloc(&pdev->dev, sizeof(*vtdev), GFP_KERNEL);
		if (!vtdev) {
			of_node_put(args.np);
			mutex_unlock(&virtual_sensor_dev_lock);
			mutex_unlock(&virtual_thermal_zone_lock);
			return -ENOMEM;
		}

		vtdev->phys_sensor = phys_sensor;

		list_for_each_entry(tdev, &global_virtual_sensor_dev_list, node) {
			if (tdev->dev == &phys_sensor->dev) {
				vtdev->tdev = tdev;
				break;
			}
		}

		if (args.args[0] > 0)
			offset_sign = 1;
		else
			offset_sign = -1;
		vtdev->tdp.offset = (args.args[1] * offset_sign);
		vtdev->tdp.alpha = args.args[2];
		vtdev->tdp.weight = args.args[3];

		list_add_tail(&vtdev->node, &vtz->virtual_sensor_dev_list);

		mutex_unlock(&virtual_sensor_dev_lock);

		of_node_put(args.np);
	}

	if (of_property_read_bool(np, "moving-average-prop")) {
		if ((of_property_count_u32_elems(np, "moving-average-prop") ==
						NUM_OF_MOVING_AVG_PROP) &&
			!of_property_read_u32_array(np, "moving-average-prop",
					ma_prop, NUM_OF_MOVING_AVG_PROP) &&
			ma_prop[0] && ma_prop[1] && ma_prop[2] && ma_prop[3]) {
			if (vtz->vs_ma)
				memset(vtz->vs_ma, 0, sizeof(*vtz->vs_ma));
			else
				vtz->vs_ma = devm_kzalloc(&pdev->dev,
					sizeof(*vtz->vs_ma), GFP_KERNEL);
			if (vtz->vs_ma) {
				vtz->vs_ma->ma_rate =
						msecs_to_jiffies(ma_prop[0]);
				vtz->vs_ma->ma_alpha_p = ma_prop[1];
				vtz->vs_ma->ma_alpha_q = ma_prop[2];
				vtz->vs_ma->ma_roc_rate =
						msecs_to_jiffies(ma_prop[3]);
			} else {
				return -ENOMEM;
			}
		} else {
			dev_err(&pdev->dev,
				"%s Invalid moving-average-properties\n",
				__func__);
		}
	}

	mutex_init(&vtz->lock);

	list_add_tail(&vtz->node, &global_virtual_thermal_zone_list);
	mutex_unlock(&virtual_thermal_zone_lock);

	vtz->virtual_sensor_tzd = thermal_zone_of_sensor_register(&pdev->dev,
					  0,  vtz, /* private data */
					  &virtual_sensor_ops);

	if (IS_ERR(vtz->virtual_sensor_tzd)) {
		dev_err(&pdev->dev, "%s Failed to register sensor\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(vtz->last_trend); i++)
		vtz->last_trend[i] = THERMAL_TREND_DROPPING;

	/* Overriding get_trend with virtual sensor get_trend */
	vtz->virtual_sensor_tzd->ops->get_trend =
					virtual_sensor_thermal_get_trend;
	vtz->virtual_sensor_tzd->ops->notify =
					virtual_sensor_thermal_notify;
	thermal_zone_device_update(vtz->virtual_sensor_tzd, THERMAL_EVENT_UNSPECIFIED);

	dev_set_drvdata(&pdev->dev, vtz);

	error = device_create_file(&pdev->dev, &dev_attr_params);
	if (error)
		pr_warn("%s Failed to create params attr - error (%d)\n",
			__func__, error);

	error = device_create_file(&pdev->dev, &dev_attr_ma);
	if (error)
		pr_warn("%s Failed to create ma attr - error (%d)\n",
			__func__, error);

	return 0;
}
static int virtual_sensor_thermal_remove(struct platform_device *pdev)
{
	struct virtual_thermal_zone *vtz = dev_get_drvdata(&pdev->dev);

	if (vtz) {
		mutex_destroy(&vtz->lock);
		thermal_zone_of_sensor_unregister(&pdev->dev, vtz->virtual_sensor_tzd);
		mutex_lock(&virtual_thermal_zone_lock);
		__list_del_entry(&vtz->node);
		mutex_unlock(&virtual_thermal_zone_lock);
	}

	return 0;
}

void thermal_parse_node_int(const struct device_node *np,
			const char *node_name, int *cust_val)
{
	u32 val = 0;

	if (of_property_read_u32(np, node_name, &val) == 0) {
		(*cust_val) = (int)val;
		pr_debug("Get %s %d\n", node_name, *cust_val);
	} else
		pr_notice("Get %s failed\n", node_name);
}

EXPORT_SYMBOL(thermal_parse_node_int);

struct thermal_dev_params *thermal_sensor_dt_to_params(struct device *dev,
		struct thermal_dev_params *params, struct thermal_dev_node_names *name_params)
{
	struct device_node *np = dev->of_node;
	int offset_invert = 0;
	int weight_invert = 0;

	if (!params || !name_params) {
		dev_err(dev, "the params or name_params is NULL\n");
		return NULL;
	}

	thermal_parse_node_int(np, name_params->offset_name, &params->offset);
	thermal_parse_node_int(np, name_params->alpha_name, &params->alpha);
	thermal_parse_node_int(np, name_params->weight_name, &params->weight);
	thermal_parse_node_int(np, name_params->select_device_name, &params->select_device);

	if (*name_params->offset_invert_name)
		thermal_parse_node_int(np, name_params->offset_invert_name, &offset_invert);

	if (offset_invert)
		params->offset = 0 - params->offset;

	if (*name_params->weight_invert_name)
		thermal_parse_node_int(np, name_params->weight_invert_name, &weight_invert);

	if (weight_invert)
		params->weight = 0 - params->weight;

	if (*name_params->aux_channel_num_name)
		thermal_parse_node_int(np, name_params->aux_channel_num_name, &params->aux_channel_num);

	return params;
}

EXPORT_SYMBOL(thermal_sensor_dt_to_params);

int thermal_dev_register(struct thermal_dev *tdev)
{
	struct virtual_thermal_dev *vtdev;
	struct virtual_thermal_zone *vtz;

	if (unlikely(IS_ERR_OR_NULL(tdev))) {
		pr_err("%s: NULL sensor thermal device\n", __func__);
		return -ENODEV;
	}
	if (!tdev->dev_ops->get_temp) {
		pr_err("%s: Error getting get_temp()\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&virtual_thermal_zone_lock);
	mutex_lock(&virtual_sensor_dev_lock);
	/* This could be a late init. Add to existing virt thermal zones. */
	list_for_each_entry(vtz, &global_virtual_thermal_zone_list, node) {
		list_for_each_entry(vtdev,
					&vtz->virtual_sensor_dev_list, node) {
			if ((vtdev->tdev == NULL) &&
			    (tdev->dev == &vtdev->phys_sensor->dev)) {
				vtdev->tdev = tdev;
				break;
			}
		}
	}
	list_add_tail(&tdev->node, &global_virtual_sensor_dev_list);
	mutex_unlock(&virtual_sensor_dev_lock);
	mutex_unlock(&virtual_thermal_zone_lock);
	return 0;
}
EXPORT_SYMBOL(thermal_dev_register);

int thermal_dev_deregister(struct thermal_dev *tdev)
{
	struct virtual_thermal_dev *vtdev;
	struct virtual_thermal_zone *vtz;

	if (unlikely(IS_ERR_OR_NULL(tdev))) {
		pr_err("%s: NULL sensor thermal device\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&virtual_thermal_zone_lock);
	mutex_lock(&virtual_sensor_dev_lock);
	list_for_each_entry(vtz, &global_virtual_thermal_zone_list, node) {
		list_for_each_entry(vtdev,
					&vtz->virtual_sensor_dev_list, node) {
			if (tdev->dev == &vtdev->phys_sensor->dev) {
				vtdev->tdev = NULL;
				break;
			}
		}
	}
	__list_del_entry(&tdev->node);
	mutex_unlock(&virtual_sensor_dev_lock);
	mutex_unlock(&virtual_thermal_zone_lock);
	return 0;
}
EXPORT_SYMBOL(thermal_dev_deregister);

static struct of_device_id virtual_sensor_driver_of_match[] = {
	{ .compatible = "amazon,virtual_sensor" },
	{},
};

static struct platform_driver virtual_sensor_driver = {
	.probe = virtual_sensor_thermal_probe,
	.remove = virtual_sensor_thermal_remove,
	.driver     = {
		.name  = "amazon_virtual_sensor",
		.owner = THIS_MODULE,
		.of_match_table = virtual_sensor_driver_of_match,
	},
};

static int __init virtual_sensor_thermal_init(void)
{
	return platform_driver_register(&virtual_sensor_driver);
}
static void __exit virtual_sensor_thermal_exit(void)
{
	platform_driver_unregister(&virtual_sensor_driver);
}

late_initcall(virtual_sensor_thermal_init);
module_exit(virtual_sensor_thermal_exit);

MODULE_DESCRIPTION("VIRTUAL_SENSOR pcb virtual sensor thermal zone driver");
MODULE_AUTHOR("Akwasi Boateng <boatenga@amazon.com>");
MODULE_LICENSE("GPL");
