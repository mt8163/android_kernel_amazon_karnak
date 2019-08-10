/*
 * Thermal Framework Driver
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Dan Murphy <DMurphy@ti.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#ifndef __LINUX_THERMAL_FRAMEWORK_H__
#define __LINUX_THERMAL_FRAMEWORK_H__

#define NUM_COOLERS 10
#define NODE_NAME_MAX 40
#include <linux/seq_file.h>

struct thermal_dev;
struct cooling_device;

/**
 * struct virtual_sensor_params  - Structure for each virtual sensor params.
 * @alpha:  Moving average coefficient
 * @offset: Temperature offset
 * @weight: Weight
 * @select_device: Decide to register which thermalzone device
 * @aux_channel_num: the thermistor index
 */
struct thermal_dev_params {
	int offset;
	int alpha;
	int weight;
	int select_device;
	int aux_channel_num;
};

/**
 * struct virtual_sensor_params  - Structure for each virtual sensor params.
 * @offset_name: The offset-node name in DTS
 * @offset_invert_name: The offset-invert-name in DTS
 * @alpha: The alpha-name in DTS
 * @weight: The alpha-name in DTS
 * @select_device: The weight-name in DTS
 * @aux_channel_num: The aux-channel-num-name in DTS
 */
struct thermal_dev_node_names {
	char offset_name[NODE_NAME_MAX];
	char offset_invert_name[NODE_NAME_MAX];
	char alpha_name[NODE_NAME_MAX];
	char weight_name[NODE_NAME_MAX];
	char select_device_name[NODE_NAME_MAX];
	char aux_channel_num_name[NODE_NAME_MAX];
};

/**
 * struct thermal_dev_ops  - Structure for device operation call backs
 * @get_temp: A temp sensor call back to get the current temperature.
 *		temp is reported in milli degrees.
 */
struct thermal_dev_ops {
	int (*get_temp) (struct thermal_dev *);
};

/**
 * struct thermal_dev  - Structure for each thermal device.
 * @name: The name of the device that is registering to the framework
 * @dev: Device node
 * @dev_ops: The device specific operations for the sensor, governor and cooling
 *           agents.
 * @node: The list node of the
 * @current_temp: The current temperature reported for the specific domain
 *
 */
struct thermal_dev {
	const char *name;
	struct device *dev;
	struct thermal_dev_ops *dev_ops;
	struct list_head node;
	struct thermal_dev_params *tdp;
	int current_temp;
	long off_temp;
};
/**
 * API to register a temperature sensor with a thermal zone
 */
int thermal_dev_register(struct thermal_dev *tdev);
void thermal_parse_node_int(const struct device_node *np,
			const char *node_name, int *cust_val);
struct thermal_dev_params *thermal_sensor_dt_to_params(struct device *dev,
			struct thermal_dev_params *params, struct thermal_dev_node_names *name_params);
#endif /* __LINUX_THERMAL_FRAMEWORK_H__ */
