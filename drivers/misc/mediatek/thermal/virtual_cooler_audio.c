/*
 * virtual_cooler_audio.c - Cooling device for Audio.
 *
 * Copyright 2017 Amazon.com, Inc. or its Affiliates. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */


#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/kobject.h>

#include "mt-plat/mtk_thermal_monitor.h"
#include <linux/platform_data/mtk_thermal.h>

#define virtual_cooler_audio_dprintk(fmt, args...) pr_debug("thermal/cooler/audio "\
							fmt, ##args)

#define MAX_LEVEL	3
static struct mutex bl_update_lock;
static struct mtk_cooler_platform_data cool_dev = {
	.type = "thermal-audio",
	.state = 0,
	.max_state = THERMAL_MAX_TRIPS,
	.level = MAX_LEVEL,
	.levels = {
		1, 1, 2,
		2, 2, 2,
		3, 3, 3,
		3, 3, 3
	},
};

static int virtual_sensor_get_max_state(struct thermal_cooling_device *cdev,
			   unsigned long *state)
{
	*state = cool_dev.max_state;
	return 0;
}

static int virtual_sensor_get_cur_state(struct thermal_cooling_device *cdev,
			   unsigned long *state)
{
	*state = cool_dev.state;
	return 0;
}

static int virtual_sensor_set_cur_state(struct thermal_cooling_device *cdev,
			   unsigned long state)
{
	int level;
	unsigned long max_state;
	char data[2][25];
	char *envp[] = { data[0], data[1], NULL };

	if (!cool_dev.cdev)
		return 0;

	mutex_lock(&(bl_update_lock));

	if (cool_dev.state == state)
		goto out;

	max_state = cool_dev.max_state;
	cool_dev.state = (state > max_state) ? max_state : state;

	if (!cool_dev.state) {
		level = cool_dev.levels[cool_dev.state];
	 } else {
		level = cool_dev.levels[cool_dev.state - 1];
	 }

	if (level == cool_dev.level)
		goto out;

	cool_dev.level = level;

	snprintf(data[0], sizeof(data[0]), "THERMAL_AUDIO_STATE=%d", level);
	snprintf(data[1], sizeof(data[2]), "CDEV_TYPE=%s", cdev->type);
	kobject_uevent_env(&cdev->device.kobj, KOBJ_CHANGE, envp);

out:
	mutex_unlock(&(bl_update_lock));

	return 0;
}

static ssize_t levels_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	int i;
	int offset = 0;

	if (!cool_dev.cdev)
		return -EINVAL;
	for (i = 0; i < THERMAL_MAX_TRIPS; i++)
		offset += sprintf(buf + offset, "%d %d\n", i+1,
				  cool_dev.levels[i]);
	return offset;
}

static ssize_t levels_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int level, state;

	if (!cool_dev.cdev)
		return -EINVAL;
	if (sscanf(buf, "%d %d\n", &state, &level) != 2)
		return -EINVAL;
	if (state >= THERMAL_MAX_TRIPS)
		return -EINVAL;
	cool_dev.levels[state] = level;
	return count;
}


static struct thermal_cooling_device_ops cooling_ops = {
	.get_max_state = virtual_sensor_get_max_state,
	.get_cur_state = virtual_sensor_get_cur_state,
	.set_cur_state = virtual_sensor_set_cur_state,
};
static DEVICE_ATTR(levels, S_IRUGO | S_IWUSR, levels_show, levels_store);

static int virtual_cooler_audio_register_ltf(void)
{
	virtual_cooler_audio_dprintk("register ltf\n");

	cool_dev.cdev = thermal_cooling_device_register(cool_dev.type, &cool_dev,
						  &cooling_ops);
	if (!cool_dev.cdev)
		return -EINVAL;

	device_create_file(&cool_dev.cdev->device, &dev_attr_levels);
	mutex_init(&(bl_update_lock));

	return 0;
}

static void virtual_cooler_audio_unregister_ltf(void)
{
	virtual_cooler_audio_dprintk("unregister ltf\n");

	thermal_cooling_device_unregister(cool_dev.cdev);
	mutex_destroy(&(bl_update_lock));
}


static int __init virtual_cooler_audio_init(void)
{
	int err = 0;

	virtual_cooler_audio_dprintk("init\n");

	err = virtual_cooler_audio_register_ltf();
	if (err)
		goto err_unreg;

	return 0;

 err_unreg:
	virtual_cooler_audio_unregister_ltf();
	return err;
}

static void __exit virtual_cooler_audio_exit(void)
{
	virtual_cooler_audio_dprintk("exit\n");

	virtual_cooler_audio_unregister_ltf();
}
module_init(virtual_cooler_audio_init);
module_exit(virtual_cooler_audio_exit);
