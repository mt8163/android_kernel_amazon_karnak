/*
 * virtual_sensor_cpufreq_cooling.c - virtual sensor cpu
 * cooler
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

#include <linux/cpu_cooling.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/thermal.h>
#include <linux/cpumask.h>
#include "thermal_core.h"

/**
 * struct cpufreq_cooling_device - data for cooling device with cpufreq
 * @id: unique integer value corresponding to each cpufreq_cooling_device
 *registered.
 * @cool_dev: thermal_cooling_device pointer to keep track of the
 *registered cooling device.
 * @cpufreq_state: integer value representing the current state of cpufreq
 *coolingdevices.
 * @cpufreq_val: integer value representing the absolute value of the clipped
 *frequency.
 * @allowed_cpus: all the cpus involved for this cpufreq_cooling_device.
 *
 * This structure is required for keeping information of each
 * cpufreq_cooling_device registered. In order to prevent corruption of this a
 * mutex lock cooling_cpufreq_lock is used.
 */

struct cpufreq_cooling_device {
	int id;
	struct thermal_cooling_device *cool_dev;
	unsigned int cpufreq_state;
	unsigned int cpufreq_val;
	struct cpumask allowed_cpus;
	bool notify_device;
	struct notifier_block nb;
};

static ssize_t levels_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct thermal_cooling_device *cdev =
			container_of(dev,
				     struct thermal_cooling_device,
				     device);
	struct cpufreq_cooling_device *cpufreq_device;
	struct cpumask *mask;
	struct thermal_instance *instance;
	int offset = 0;
	unsigned int ufreq, lfreq;
	unsigned int cpu;

	if (!cdev)
		return -EINVAL;
	cpufreq_device = (struct cpufreq_cooling_device *)cdev->devdata;
	if (!cpufreq_device)
		return -EINVAL;
	mask = &cpufreq_device->allowed_cpus;
	cpu = cpumask_any(mask);

	mutex_lock(&cdev->lock);
	list_for_each_entry(instance, &cdev->thermal_instances, cdev_node) {
		lfreq = cpufreq_cooling_get_frequency(cpu, instance->lower);
		ufreq = cpufreq_cooling_get_frequency(cpu, instance->upper);
		offset += sprintf(buf + offset, "state=%d upper=%d lower=%d\n",
				  instance->trip, ufreq, lfreq);
	}
	mutex_unlock(&cdev->lock);
	return offset;
}

static ssize_t levels_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct thermal_cooling_device *cdev =
			container_of(dev,
				     struct thermal_cooling_device,
				     device);
	struct cpufreq_cooling_device *cpufreq_device;
	struct cpumask *mask;
	struct thermal_instance *instance;
	struct thermal_instance *prev_instance = NULL;
	unsigned int freq, trip, prev_trip;
	unsigned long level;
	unsigned int cpu;

	if (!cdev)
		return -EINVAL;
	cpufreq_device = (struct cpufreq_cooling_device *)cdev->devdata;
	if (!cpufreq_device)
		return -EINVAL;
	mask = &cpufreq_device->allowed_cpus;
	cpu = cpumask_any(mask);

	if (sscanf(buf, "%d %d\n", &trip, &freq) != 2)
		return -EINVAL;
	if (trip >= THERMAL_MAX_TRIPS)
		return -EINVAL;
	prev_trip = trip ? trip-1 : trip;
	mutex_lock(&cdev->lock);
	list_for_each_entry(instance, &cdev->thermal_instances, cdev_node) {
		if (instance->trip == prev_trip)
			prev_instance = instance;
		if (instance->trip != trip)
			continue;
		level = cpufreq_cooling_get_level(cpu, freq);
		if (level == THERMAL_CSTATE_INVALID)
			return -EINVAL;
		instance->upper = level;
		if (trip == 0)
			instance->lower = 0;
		else {
			instance->lower = instance->upper ? prev_instance->upper
							    : instance->upper;
			if (instance->lower > instance->upper)
				instance->lower = instance->upper;
		}
	}
	mutex_unlock(&cdev->lock);
	return count;
}

static DEVICE_ATTR(levels, S_IRUGO | S_IWUSR, levels_show, levels_store);

static int virtual_sensor_cpufreq_cooling_probe(struct platform_device *pdev)
{
	struct thermal_cooling_device *cdev[2];
	struct cpumask mask_val[2];

	if (!cpufreq_frequency_get_table(0))
		return -EPROBE_DEFER;

	if (!cpufreq_frequency_get_table(2))
		return -EPROBE_DEFER;

	cpumask_set_cpu(0, &mask_val[0]);
	cpumask_set_cpu(1, &mask_val[0]);

	cdev[0] = cpufreq_cooling_register(&mask_val[0]);

	if (IS_ERR(cdev[0])) {
		dev_err(&pdev->dev, "Failed to register cooling device\n");
		return PTR_ERR(cdev[0]);
	}
	device_create_file(&cdev[0]->device, &dev_attr_levels);

	cpumask_set_cpu(2, &mask_val[1]);
	cpumask_set_cpu(3, &mask_val[1]);

	cdev[1] = cpufreq_cooling_register(&mask_val[1]);

	if (IS_ERR(cdev[1])) {
		dev_err(&pdev->dev, "Failed to register cooling device\n");
		return PTR_ERR(cdev[1]);
	}
	device_create_file(&cdev[1]->device, &dev_attr_levels);

	platform_set_drvdata(pdev, cdev);

	dev_info(&pdev->dev, "Cooling device registered: %s\n",	cdev[0]->type);
	dev_info(&pdev->dev, "Cooling device registered: %s\n", cdev[1]->type);

	return 0;
}

static int virtual_sensor_cpufreq_cooling_remove(struct platform_device *pdev)
{
	struct thermal_cooling_device **cdev = platform_get_drvdata(pdev);

	cpufreq_cooling_unregister(cdev[0]);
	cpufreq_cooling_unregister(cdev[1]);

	return 0;
}

static int virtual_sensor_cpufreq_cooling_suspend(struct platform_device *pdev,
						  pm_message_t state)
{
	return 0;
}

static int virtual_sensor_cpufreq_cooling_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver virtual_sensor_cpufreq_cooling_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "virtual_sensor-cpufreq-cooling",
	},
	.probe = virtual_sensor_cpufreq_cooling_probe,
	.suspend = virtual_sensor_cpufreq_cooling_suspend,
	.resume = virtual_sensor_cpufreq_cooling_resume,
	.remove = virtual_sensor_cpufreq_cooling_remove,
};

static struct platform_device virtual_sensor_cpufreq_cooling_device = {
	.name = "virtual_sensor-cpufreq-cooling",
	.id = -1,
};

static int __init virtual_sensor_cpufreq_cooling_init(void)
{
	int ret;
	ret = platform_driver_register(&virtual_sensor_cpufreq_cooling_driver);

	if (ret) {
		pr_err("Unable to register virtual_sensor_cpufreq driver (%d)\n"
		       , ret);
		return ret;
	}
	ret = platform_device_register(&virtual_sensor_cpufreq_cooling_device);
	if (ret) {
		pr_err("Unable to register virtual_sensor_cpufreq device (%d)\n"
		       , ret);
		return ret;
	}
	return 0;
}

static void __exit virtual_sensor_cpufreq_cooling_exit(void)
{
	platform_driver_unregister(&virtual_sensor_cpufreq_cooling_driver);
	platform_device_unregister(&virtual_sensor_cpufreq_cooling_device);
}

late_initcall(virtual_sensor_cpufreq_cooling_init);
module_exit(virtual_sensor_cpufreq_cooling_exit);

MODULE_AUTHOR("Akwasi Boateng <boatenga@amazon.com>");
MODULE_DESCRIPTION("VIRTUAL_SENSOR cpufreq cooling driver");
MODULE_LICENSE("GPL");
