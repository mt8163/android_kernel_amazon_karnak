/*
 * Copyright (C) 2013 Lab126, Inc.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
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
#include <linux/atomic.h>
#include <linux/list_sort.h>

#include <linux/platform_data/mtk_thermal.h>
#include <linux/thermal_framework.h>

#ifdef CONFIG_AMAZON_SIGN_OF_LIFE
#include <linux/sign_of_life.h>
#endif

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#define VIRTUAL_SENSOR_METRICS_STR_LEN 128
/*
 * Define the metrics log printing interval.
 * If virtual sensor throttles, the interval
 * is 0x3F*3 seconds(3 means polling interval of virtual_sensor).
 * If doesn't throttle, the interval is 0x3FF*3 seconds.
 */
#define VIRTUAL_SENSOR_THROTTLE_TIME_MASK 0x3F
#define VIRTUAL_SENSOR_UNTHROTTLE_TIME_MASK 0x3FF
static unsigned long virtual_sensor_temp = 25000;
#endif

#include "thermal_core.h"

#define DRIVER_NAME "virtual_sensor-thermal"
#define THERMAL_NAME "virtual_sensor"
#define BUF_SIZE 128
#define DMF 1000
#define MASK (0x0FFF)
#define VIRTUAL_SENSOR_NUM_MAX 2

static DEFINE_MUTEX(therm_lock);
static unsigned int virtual_sensor_nums = 0;

#ifdef CONFIG_AMAZON_METRICS_LOG
unsigned long get_virtualsensor_temp(void)
{
	return virtual_sensor_temp/1000;
}
EXPORT_SYMBOL(get_virtualsensor_temp);
#endif

static int level_cmp(void *priv,struct list_head *a, struct list_head *b)
{
	struct cooler_sort_list *cooler_a = container_of(a, struct cooler_sort_list, list);
	struct cooler_sort_list *cooler_b = container_of(b, struct cooler_sort_list, list);
	int level;

	level = cooler_a->pdata->level - cooler_b->pdata->level;

	return level;
}

int thermal_level_compare(struct mtk_cooler_platform_data *cooler_data, struct cooler_sort_list *head, bool positive_seq)
{
	struct cooler_sort_list *level_list;

	list_sort(NULL, &head->list, level_cmp);

	if(positive_seq)
		level_list = list_entry(head->list.next, struct cooler_sort_list , list);
	else
		level_list = list_entry(head->list.prev, struct cooler_sort_list , list);

	return level_list->pdata->level;
}

#define PREFIX "thermalsensor:def"

static int match(struct thermal_zone_device *tz,
	  struct thermal_cooling_device *cdev)
{
	int i;
	struct virtual_sensor_thermal_zone *tzone = tz->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;
	const struct thermal_zone_params *tzp = tz->tzp;

	if (strncmp(tz->type, "virtual_sensor", strlen("virtual_sensor")))
		return -EINVAL;

	if (!tzp)
		return -EINVAL;

	for (i = 0; i < tzp->num_tbps; i++) {
		if (tzp->tbp[i].cdev) {
			if (!strcmp(tzp->tbp[i].cdev->type, cdev->type))
				return -EEXIST;
		}
	}

	for (i = 0; i < pdata->num_cdevs; i++) {
		pr_debug("pdata->cdevs[%d] %s cdev->type %s\n", i, pdata->cdevs[i], cdev->type);
		if (!strncmp(pdata->cdevs[i], cdev->type, strlen(cdev->type)))
			return 0;
	}

	return -EINVAL;
}

static struct mtk_thermal_platform_data virtual_sensor_thermal_data[VIRTUAL_SENSOR_NUM_MAX] = {
	{
		.ts_list =  LIST_HEAD_INIT(virtual_sensor_thermal_data[0].ts_list),
	},
	{
		.ts_list =  LIST_HEAD_INIT(virtual_sensor_thermal_data[1].ts_list),
	},
};

static int virtual_sensor_thermal_get_temp(struct thermal_zone_device *thermal,
				   int *t)
{
	struct thermal_dev *tdev;
	struct virtual_sensor_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;
	long temp = 0;
	long tempv = 0;
	int alpha, offset, weight;
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[VIRTUAL_SENSOR_METRICS_STR_LEN];
#endif

	if (!tzone || !pdata)
		return -EINVAL;

#ifdef CONFIG_AMAZON_METRICS_LOG
	atomic_inc(&tzone->query_count);
#endif

	list_for_each_entry(tdev, &pdata->ts_list, node) {
		temp = tdev->dev_ops->get_temp(tdev);
#ifdef CONFIG_AMAZON_METRICS_LOG
		/* Log in metrics around every 1 hour normally
			and 3 mins wheny throttling */
		if (!(atomic_read(&tzone->query_count) & tzone->mask)) {
			snprintf(buf, VIRTUAL_SENSOR_METRICS_STR_LEN,
				"%s:%s_%s_temp=%ld;CT;1:NR",
				PREFIX, thermal->type, tdev->name, temp);
			log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", buf);
		}
#endif

		alpha = tdev->tdp->alpha;
		offset = tdev->tdp->offset;
		weight = tdev->tdp->weight;
		if (!tdev->off_temp)
			tdev->off_temp = temp - offset;
		else {
			tdev->off_temp = alpha * (temp - offset) +
				(DMF - alpha) * tdev->off_temp;
			tdev->off_temp /= DMF;
		}

		tempv += (weight * tdev->off_temp)/DMF;
	}

#ifdef CONFIG_AMAZON_METRICS_LOG
	/* Log in metrics around every 1 hour normally
		and 3 mins wheny throttling */
	if (!(atomic_read(&tzone->query_count) & tzone->mask)) {
		snprintf(buf, VIRTUAL_SENSOR_METRICS_STR_LEN,
			"%s:%s_temp=%ld;CT;1:NR",
			PREFIX, thermal->type, tempv);
		log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", buf);
	}

	if (tempv > pdata->trips[0].temp)
		tzone->mask = VIRTUAL_SENSOR_THROTTLE_TIME_MASK;
	else
		tzone->mask = VIRTUAL_SENSOR_UNTHROTTLE_TIME_MASK;
#endif

	*t = (unsigned long) tempv;
#ifdef CONFIG_AMAZON_METRICS_LOG
	virtual_sensor_temp = (unsigned long)tempv;
#endif

	return 0;
}
static int virtual_sensor_thermal_get_mode(struct thermal_zone_device *thermal,
				   enum thermal_device_mode *mode)
{
	struct virtual_sensor_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;
	if (!pdata)
		return -EINVAL;

	mutex_lock(&therm_lock);
	*mode = pdata->mode;
	mutex_unlock(&therm_lock);
	return 0;
}
static int virtual_sensor_thermal_set_mode(struct thermal_zone_device *thermal,
				   enum thermal_device_mode mode)
{
	struct virtual_sensor_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	mutex_lock(&therm_lock);
	pdata->mode = mode;
	if (mode == THERMAL_DEVICE_DISABLED) {
		tzone->tz->polling_delay = 0;
		thermal_zone_device_update(tzone->tz, THERMAL_EVENT_UNSPECIFIED);
		mutex_unlock(&therm_lock);
		return 0;
	}
	schedule_work(&tzone->therm_work);
	mutex_unlock(&therm_lock);
	return 0;
}
static int virtual_sensor_thermal_get_trip_type(struct thermal_zone_device *thermal,
					int trip,
					enum thermal_trip_type *type)
{
	struct virtual_sensor_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;
	*type = pdata->trips[trip].type;
	return 0;
}
static int virtual_sensor_thermal_get_trip_temp(struct thermal_zone_device *thermal,
					int trip,
					int *temp)
{
	struct virtual_sensor_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;
	*temp = pdata->trips[trip].temp;
	return 0;
}
static int virtual_sensor_thermal_set_trip_temp(struct thermal_zone_device *thermal,
					int trip,
					 int temp)
{
	struct virtual_sensor_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;
	pdata->trips[trip].temp = temp;
	return 0;
}
static int virtual_sensor_thermal_get_crit_temp(struct thermal_zone_device *thermal,
					int *temp)
{
	int i;
	struct virtual_sensor_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	for (i = 0; i < THERMAL_MAX_TRIPS; i++) {
		if (pdata->trips[i].type == THERMAL_TRIP_CRITICAL) {
			*temp = pdata->trips[i].temp;
			return 0;
		}
	}
	return -EINVAL;
}
static int virtual_sensor_thermal_get_trip_hyst(struct thermal_zone_device *thermal,
					int trip,
					int *hyst)
{
	struct virtual_sensor_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;
	*hyst = pdata->trips[trip].hyst;
	return 0;
}
static int virtual_sensor_thermal_set_trip_hyst(struct thermal_zone_device *thermal,
					int trip,
					int hyst)
{
	struct virtual_sensor_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;
	pdata->trips[trip].hyst = hyst;
	return 0;
}

#ifdef CONFIG_THERMAL_SHUTDOWN_LAST_KMESG
void last_kmsg_thermal_shutdown(void)
{
	int rc;
	char *argv[] = {
		"/sbin/crashreport",
		"thermal_shutdown",
		NULL
	};

	pr_err("%s: start to save last kmsg\n", __func__);
	//UMH_WAIT_PROC UMH_WAIT_EXEC
	rc = call_usermodehelper(argv[0], argv, NULL, UMH_WAIT_EXEC);
	pr_err("%s: save last kmsg finish\n", __func__);

	if (rc < 0)
		pr_err("call /sbin/crashreport failed, rc = %d\n", rc);
	else
		msleep(6000); /* 6000ms */
}
EXPORT_SYMBOL_GPL(last_kmsg_thermal_shutdown);
#endif

static int virtual_sensor_thermal_notify(struct thermal_zone_device *thermal,
				 int trip,
				 enum thermal_trip_type type)
{
	char data[20];
	char *envp[] = { data, NULL};
	snprintf(data, sizeof(data), "%s", "SHUTDOWN_WARNING");
	kobject_uevent_env(&thermal->device.kobj, KOBJ_CHANGE, envp);

#ifdef CONFIG_THERMAL_SHUTDOWN_LAST_KMESG
	if (type == THERMAL_TRIP_CRITICAL) {
		pr_err("%s: thermal_shutdown notify\n", __func__);
		last_kmsg_thermal_shutdown();
		pr_err("%s: thermal_shutdown notify end\n", __func__);
	}
#endif

#ifdef CONFIG_AMAZON_SIGN_OF_LIFE
	if (type == THERMAL_TRIP_CRITICAL)
		life_cycle_set_thermal_shutdown_reason(THERMAL_SHUTDOWN_REASON_PCB);
#endif
	return 0;
}
static struct thermal_zone_device_ops virtual_sensor_tz_dev_ops = {
	.get_temp = virtual_sensor_thermal_get_temp,
	.get_mode = virtual_sensor_thermal_get_mode,
	.set_mode = virtual_sensor_thermal_set_mode,
	.get_trip_type = virtual_sensor_thermal_get_trip_type,
	.get_trip_temp = virtual_sensor_thermal_get_trip_temp,
	.set_trip_temp = virtual_sensor_thermal_set_trip_temp,
	.get_crit_temp = virtual_sensor_thermal_get_crit_temp,
	.get_trip_hyst = virtual_sensor_thermal_get_trip_hyst,
	.set_trip_hyst = virtual_sensor_thermal_set_trip_hyst,
	.notify = virtual_sensor_thermal_notify,
};
static ssize_t params_show(struct device *dev,
		       struct device_attribute *devattr, char *buf)
{
	int o = 0;
	int a = 0;
	int w = 0;
	char pbufo[BUF_SIZE];
	char pbufa[BUF_SIZE];
	char pbufw[BUF_SIZE];
	int alpha, offset, weight;
	struct thermal_dev *tdev;
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct mtk_thermal_platform_data *pdata = NULL;
	struct virtual_sensor_thermal_zone *tzone = NULL;

	tzone = thermal->devdata;
	pdata = tzone->pdata;

	o += sprintf(pbufo + o, "offsets ");
	a += sprintf(pbufa + a, "alphas ");
	w += sprintf(pbufw + w, "weights ");

	list_for_each_entry(tdev, &pdata->ts_list ,node) {
		alpha = tdev->tdp->alpha;
		offset = tdev->tdp->offset;
		weight = tdev->tdp->weight;

		o += sprintf(pbufo + o, "%d ", offset);
		a += sprintf(pbufa + a, "%d ", alpha);
		w += sprintf(pbufw + w, "%d ", weight);
	}
	return sprintf(buf, "%s\n%s\n%s\n", pbufo, pbufa, pbufw);
}
static ssize_t trips_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	return sprintf(buf, "%d\n", thermal->trips);
}
static ssize_t trips_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int trips = 0;
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct virtual_sensor_thermal_zone *tzone = thermal->devdata;
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
static ssize_t polling_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	return sprintf(buf, "%d\n", thermal->polling_delay);
}
static ssize_t polling_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int polling_delay = 0;
	struct thermal_zone_device *thermal = container_of(dev, struct thermal_zone_device, device);
	struct virtual_sensor_thermal_zone *tzone = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tzone->pdata;

	if (!tzone || !pdata)
		return -EINVAL;

	if (sscanf(buf, "%d\n", &polling_delay) != 1)
		return -EINVAL;
	if (polling_delay < 0)
		return -EINVAL;

	pdata->polling_delay = polling_delay;
	thermal->polling_delay = pdata->polling_delay;
	thermal_zone_device_update(thermal, THERMAL_EVENT_UNSPECIFIED);
	return count;
}

static DEVICE_ATTR(trips, S_IRUGO | S_IWUSR, trips_show, trips_store);
static DEVICE_ATTR(polling, S_IRUGO | S_IWUSR, polling_show, polling_store);
static DEVICE_ATTR(params, S_IRUGO, params_show, NULL);

static int virtual_sensor_create_sysfs(struct virtual_sensor_thermal_zone *tzone)
{
	int ret = 0;
	ret = device_create_file(&tzone->tz->device, &dev_attr_params);
	if (ret)
		pr_err("%s Failed to create polling attr\n", __func__);
	ret = device_create_file(&tzone->tz->device, &dev_attr_polling);
	if (ret)
		pr_err("%s Failed to create polling attr\n", __func__);
	ret = device_create_file(&tzone->tz->device, &dev_attr_trips);
	if (ret)
		pr_err("%s Failed to create trips attr\n", __func__);
	return ret;
}
static void virtual_sensor_thermal_work(struct work_struct *work)
{
	struct virtual_sensor_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata;

	mutex_lock(&therm_lock);
	tzone = container_of(work, struct virtual_sensor_thermal_zone, therm_work);
	if (!tzone)
		return;
	pdata = tzone->pdata;
	if (!pdata)
		return;
	if (pdata->mode == THERMAL_DEVICE_ENABLED)
		thermal_zone_device_update(tzone->tz, THERMAL_EVENT_UNSPECIFIED);
	mutex_unlock(&therm_lock);
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

static void virtual_sensor_thermal_parse_node_int_array(const struct device_node *np,
			const char *node_name, unsigned long *tripsdata)
{
	u32 array[THERMAL_MAX_TRIPS] = {0};
	int i = 0;

	if (of_property_read_u32_array(np, node_name, array, ARRAY_SIZE(array)) == 0) {
		for (i = 0; i < ARRAY_SIZE(array); i++) {
			tripsdata[i] = (unsigned long)array[i];
			pr_debug("Get %s %ld\n", node_name, tripsdata[i]);
		}
	} else
		pr_notice("Get %s failed\n", node_name);
}

static void virtual_sensor_thermal_tripsdata_convert(char *type, unsigned long *tripsdata, struct trip_t *trips)
{
	int i = 0;

	if (strncmp(type, "type", 4) == 0) {
		for (i = 0; i < THERMAL_MAX_TRIPS; i++) {
			switch (tripsdata[i]) {
			case 0:
				trips[i].type = THERMAL_TRIP_ACTIVE;
				break;
			case 1:
				trips[i].type = THERMAL_TRIP_PASSIVE;
				break;
			case 2:
				trips[i].type = THERMAL_TRIP_HOT;
				break;
			case 3:
				trips[i].type = THERMAL_TRIP_CRITICAL;
				break;
			default:
				pr_notice("unknow type!\n");
			}
			pr_debug("tripsdata[%d]=%ld\n", i, tripsdata[i]);
		}
	}
	else if (strncmp(type, "temp", 4) == 0) {
		for (i = 0; i < THERMAL_MAX_TRIPS; i++) {
			trips[i].temp = tripsdata[i];
			pr_debug("tripsdata[%d]=%ld\n", i, tripsdata[i]);
		}
	} else {
		for (i = 0; i < THERMAL_MAX_TRIPS; i++) {
			trips[i].hyst = tripsdata[i];
			pr_debug("tripsdata[%d]=%ld\n", i, tripsdata[i]);
		}
	}
}

static void virtual_sensor_thermal_init_trips(const struct device_node *np, struct trip_t *trips)
{
	unsigned long tripsdata[THERMAL_MAX_TRIPS] = {0};

	virtual_sensor_thermal_parse_node_int_array(np, "temp", tripsdata);
	virtual_sensor_thermal_tripsdata_convert("temp", tripsdata, trips);
	virtual_sensor_thermal_parse_node_int_array(np, "type", tripsdata);
	virtual_sensor_thermal_tripsdata_convert("type", tripsdata, trips);
	virtual_sensor_thermal_parse_node_int_array(np, "hyst", tripsdata);
	virtual_sensor_thermal_tripsdata_convert("hyst", tripsdata, trips);
}

static void virtual_sensor_thermal_parse_node_string_index(const struct device_node *np,
			const char *node_name, int index, char *cust_string)
{
	const char *string;

	if (of_property_read_string_index(np, node_name, index, &string) == 0) {
		strncpy(cust_string, string, strlen(string));
		pr_debug("Get %s %s\n", node_name, cust_string);
	} else
		pr_notice("Get %s failed\n", node_name);
}

static int virtual_sensor_thermal_init_tbp(struct thermal_zone_params *tzp, struct platform_device *pdev)
{
	int i;
	struct thermal_bind_params *tbp;

	tbp = devm_kzalloc(&pdev->dev, sizeof(struct thermal_bind_params) * tzp->num_tbps, GFP_KERNEL);
	if (!tbp)
		return -ENOMEM;

	for (i = 0; i < tzp->num_tbps; i++) {
		tbp[i].cdev = NULL;
		tbp[i].weight = 0;
		tbp[i].trip_mask = MASK;
		tbp[i].match = match;
	}
	tzp->tbp = tbp;

	return 0;
}

static int virtual_sensor_thermal_init_cust_data_from_dt(struct platform_device *dev)
{
	int ret = 0;
	struct device_node *np = dev->dev.of_node;
	struct mtk_thermal_platform_data *p_virtual_sensor_thermal_data;
	int i = 0;

	dev->id = 0;
	virtual_sensor_nums++;
	thermal_parse_node_int(np, "dev_id", &dev->id);
	if (dev->id < VIRTUAL_SENSOR_NUM_MAX)
		p_virtual_sensor_thermal_data = &virtual_sensor_thermal_data[dev->id];
	else {
		ret = -1;
		pr_err("dev->id invalid!\n");
		return ret;
	}

	thermal_parse_node_int(np, "num_trips", &p_virtual_sensor_thermal_data->num_trips);
	thermal_parse_node_int(np, "mode", (int*)&p_virtual_sensor_thermal_data->mode);
	thermal_parse_node_int(np, "polling_delay", &p_virtual_sensor_thermal_data->polling_delay);
	virtual_sensor_thermal_parse_node_string_index(np, "governor_name", 0, p_virtual_sensor_thermal_data->tzp.governor_name);
	thermal_parse_node_int(np, "num_tbps", &p_virtual_sensor_thermal_data->tzp.num_tbps);
	virtual_sensor_thermal_init_trips(np, p_virtual_sensor_thermal_data->trips);

	thermal_parse_node_int(np, "num_cdevs", &p_virtual_sensor_thermal_data->num_cdevs);
	if (p_virtual_sensor_thermal_data->num_cdevs > THERMAL_MAX_TRIPS)
		p_virtual_sensor_thermal_data->num_cdevs = THERMAL_MAX_TRIPS;

	while (i < p_virtual_sensor_thermal_data->num_cdevs) {
		virtual_sensor_thermal_parse_node_string_index(np, "cdev_names", i, p_virtual_sensor_thermal_data->cdevs[i]);
		i++;
	}

	ret = virtual_sensor_thermal_init_tbp(&p_virtual_sensor_thermal_data->tzp, dev);

	return ret;
}

#ifdef DEBUG
static void test_data(void)
{
	int i = 0;
	int j = 0;
	int h = 0;

	for (i = 0; i < virtual_sensor_nums; i++) {
		pr_debug("num_trips %d\n", virtual_sensor_thermal_data[i].num_trips);
		pr_debug("mode %d\n", virtual_sensor_thermal_data[i].mode);
		pr_debug("polling_delay %d\n", virtual_sensor_thermal_data[i].polling_delay);
		pr_debug("governor_name %s\n", virtual_sensor_thermal_data[i].tzp.governor_name);
		pr_debug("num_tbps %d\n", virtual_sensor_thermal_data[i].tzp.num_tbps);
		while (j < THERMAL_MAX_TRIPS) {
			pr_debug("trips[%d] %d\n", j, virtual_sensor_thermal_data[i].trips[j].temp);
			pr_debug("type[%d] %d\n", j, virtual_sensor_thermal_data[i].trips[j].type);
			pr_debug("hyst[%d] %d\n", j, virtual_sensor_thermal_data[i].trips[j].hyst);
			j++;
		}
		j = 0;
		pr_debug("num_cdevs %d\n", virtual_sensor_thermal_data[i].num_cdevs);

		while (h < virtual_sensor_thermal_data[i].num_cdevs) {
			pr_debug("cdevs[%d] %s\n", h, virtual_sensor_thermal_data[i].cdevs[h]);
			h++;
		}
		h = 0;
	}
}
#endif

static int virtual_sensor_thermal_probe(struct platform_device *pdev)
{
	int ret;
	struct virtual_sensor_thermal_zone *tzone;
	struct mtk_thermal_platform_data *pdata = NULL;
	char thermal_name[THERMAL_NAME_LENGTH];

#ifdef CONFIG_OF
	pr_notice("virtual_sensor thermal custom init by DTS!\n");
	ret = virtual_sensor_thermal_init_cust_data_from_dt(pdev);
	if(ret)
	{
		pr_err("%s: init data error\n", __func__);
		return ret;
	}
#endif
	if (pdev->id < VIRTUAL_SENSOR_NUM_MAX)
		pdata = &virtual_sensor_thermal_data[pdev->id];
	else {
		pr_err("pdev->id is invalid!\n");
	}

	if (!pdata)
		return -EINVAL;
#ifdef DEBUG
	pr_notice("%s %d test data\n", __func__, __LINE__);
	test_data();
#endif
	tzone = devm_kzalloc(&pdev->dev, sizeof(*tzone), GFP_KERNEL);
	if (!tzone)
		return -ENOMEM;
	memset(tzone, 0, sizeof(*tzone));

	tzone->pdata = pdata;
	snprintf(thermal_name, sizeof(thermal_name), "%s%d", THERMAL_NAME, pdev->id+1);

	tzone->tz = thermal_zone_device_register(thermal_name,
						 THERMAL_MAX_TRIPS,
						 MASK,
						 tzone,
						 &virtual_sensor_tz_dev_ops,
						 &pdata->tzp,
						 0,
						 pdata->polling_delay);
	if (IS_ERR(tzone->tz)) {
		pr_err("%s Failed to register thermal zone device\n", __func__);
		kfree(tzone);
		return -EINVAL;
	}
	tzone->tz->trips = pdata->num_trips;

#ifdef CONFIG_AMAZON_METRICS_LOG
	tzone->mask = VIRTUAL_SENSOR_UNTHROTTLE_TIME_MASK;
#endif
	ret = virtual_sensor_create_sysfs(tzone);
	INIT_WORK(&tzone->therm_work, virtual_sensor_thermal_work);
	platform_set_drvdata(pdev, tzone);
	pdata->mode = THERMAL_DEVICE_ENABLED;
	return ret;
}
static int virtual_sensor_thermal_remove(struct platform_device *pdev)
{
	struct virtual_sensor_thermal_zone *tzone = platform_get_drvdata(pdev);
	if (tzone) {
		cancel_work_sync(&tzone->therm_work);
		if (tzone->tz)
			thermal_zone_device_unregister(tzone->tz);
		kfree(tzone);
	}
	return 0;
}

int thermal_dev_register(struct thermal_dev *tdev)
{
	struct mtk_thermal_platform_data *pdata = NULL;

	if (unlikely(IS_ERR_OR_NULL(tdev))) {
		pr_err("%s: NULL sensor thermal device\n", __func__);
		return -ENODEV;
	}
	if (!tdev->dev_ops->get_temp) {
		pr_err("%s: Error getting get_temp()\n", __func__);
		return -EINVAL;
	}
	pr_info("%s %s select_device: %d aux_channel_num %d\n",
		__func__, tdev->name, tdev->tdp->select_device, tdev->tdp->aux_channel_num);

	if (tdev->tdp->select_device < VIRTUAL_SENSOR_NUM_MAX) {
		pdata = &virtual_sensor_thermal_data[tdev->tdp->select_device];
	} else{
		pdata = &virtual_sensor_thermal_data[0];
		pr_err("%s select_device invalid! \n", tdev->name);
	}
	mutex_lock(&therm_lock);
	list_add_tail(&tdev->node, &pdata->ts_list);
	mutex_unlock(&therm_lock);
	return 0;
}
EXPORT_SYMBOL(thermal_dev_register);

#ifdef CONFIG_OF
static const struct of_device_id virtual_sensor_thermal_of_match[] = {
	{.compatible = "amazon,virtual_sensor-thermal",},
	{},
};

MODULE_DEVICE_TABLE(of, virtual_sensor_thermal_of_match);
#endif

static struct platform_driver virtual_sensor_thermal_zone_driver = {
	.probe = virtual_sensor_thermal_probe,
	.remove = virtual_sensor_thermal_remove,
	.suspend = NULL,
	.resume = NULL,
	.shutdown   = NULL,
	.driver     = {
		.name  = DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = virtual_sensor_thermal_of_match,
#endif
	},
};

static int __init virtual_sensor_thermal_init(void)
{
	int err = 0;

	err = platform_driver_register(&virtual_sensor_thermal_zone_driver);
	if (err) {
		pr_err("%s: Failed to register driver %s\n", __func__,
			virtual_sensor_thermal_zone_driver.driver.name);
		return err;
	}

	return err;
}
static void __exit virtual_sensor_thermal_exit(void)
{
	platform_driver_unregister(&virtual_sensor_thermal_zone_driver);
}

late_initcall(virtual_sensor_thermal_init);
module_exit(virtual_sensor_thermal_exit);

MODULE_DESCRIPTION("VIRTUAL_SENSOR pcb virtual sensor thermal zone driver");
MODULE_AUTHOR("Akwasi Boateng <boatenga@amazon.com>");
MODULE_LICENSE("GPL");
