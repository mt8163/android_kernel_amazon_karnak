/*
* Copyright (C) 2011-2014 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/stddef.h>
#include <linux/sysfs.h>
#include <linux/err.h>
#include <linux/reboot.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/i2c.h>

#include <linux/platform_data/mtk_thermal.h>
#include <linux/thermal_framework.h>
#include "inc/mtk_ts_pmic6323.h"

#define PMIC_SENSOR_NAME "mtktspmic_sensor"

struct thermal_dev_node_names pmic_node_names = {
	.offset_name = "thermal_pmic,offset",
	.alpha_name = "thermal_pmic,alpha",
	.weight_name = "thermal_pmic,weight",
	.select_device_name = "select_device",
};

static int mtktspmic_read_temp(struct thermal_dev *tdev)
{
	return mtktspmic_get_hw_temp();
}

static struct thermal_dev_ops mtktspmic_sensor_fops = {
	.get_temp = mtktspmic_read_temp,
};

static ssize_t mtktspmic_sensor_show_temp(struct device *dev,
				 struct device_attribute *devattr,
				 char *buf)
{
	int temp = mtktspmic_get_hw_temp();
	return sprintf(buf, "%d\n", temp);
}

static ssize_t mtktspmic_sensor_show_params(struct device *dev,
				 struct device_attribute *devattr,
				 char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct virtual_sensor_temp_sensor *virtual_sensor = platform_get_drvdata(pdev);

	if (!virtual_sensor)
		return -EINVAL;

	return sprintf(buf, "offset=%d alpha=%d weight=%d\n",
		       virtual_sensor->therm_fw->tdp->offset,
		       virtual_sensor->therm_fw->tdp->alpha,
		       virtual_sensor->therm_fw->tdp->weight);
}

static ssize_t mtktspmic_sensor_store_params(struct device *dev,
				      struct device_attribute *devattr,
				      const char *buf,
				      size_t count)
{
	char param[20];
	int value = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct virtual_sensor_temp_sensor *virtual_sensor = platform_get_drvdata(pdev);

	if (!virtual_sensor)
		return -EINVAL;

	if (sscanf(buf, "%19s %d", param, &value) == 2) {
		if (!strcmp(param, "offset"))
			virtual_sensor->therm_fw->tdp->offset = value;
		if (!strcmp(param, "alpha"))
			virtual_sensor->therm_fw->tdp->alpha = value;
		if (!strcmp(param, "weight"))
			virtual_sensor->therm_fw->tdp->weight = value;
		return count;
	}
	return -EINVAL;
}

static DEVICE_ATTR(params, 0644, mtktspmic_sensor_show_params, mtktspmic_sensor_store_params);
static DEVICE_ATTR(temp, 0444, mtktspmic_sensor_show_temp, NULL);

static int mtktspmic_probe(struct platform_device *pdev)
{
	struct virtual_sensor_temp_sensor *virtual_sensor;
	struct thermal_dev_params *pmic_params;
	int ret = 0;

	virtual_sensor = kzalloc(sizeof(struct virtual_sensor_temp_sensor), GFP_KERNEL);
	if (!virtual_sensor)
		return -ENOMEM;

	pmic_params = devm_kzalloc(&pdev->dev, sizeof(*pmic_params), GFP_KERNEL);
	if (!pmic_params)
		return -ENOMEM;

	mutex_init(&virtual_sensor->sensor_mutex);
	virtual_sensor->dev = &pdev->dev;
	platform_set_drvdata(pdev, virtual_sensor);

        virtual_sensor->last_update = jiffies - HZ;

        virtual_sensor->therm_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
        if (virtual_sensor->therm_fw) {
                virtual_sensor->therm_fw->name = PMIC_SENSOR_NAME;
                virtual_sensor->therm_fw->dev = virtual_sensor->dev;
                virtual_sensor->therm_fw->dev_ops = &mtktspmic_sensor_fops;
                virtual_sensor->therm_fw->tdp = thermal_sensor_dt_to_params(&pdev->dev,
						pmic_params, &pmic_node_names);
#ifdef CONFIG_VIRTUAL_SENSOR_THERMAL
                ret = thermal_dev_register(virtual_sensor->therm_fw);
                if (ret) {
                        dev_err(&pdev->dev, "error registering therml device\n");
			return -EINVAL;
                }
#endif
        } else {
                ret = -ENOMEM;
                goto therm_fw_alloc_err;
        }

	ret = device_create_file(&pdev->dev, &dev_attr_params);
	if (ret)
		pr_err("%s Failed to create params attr\n", __func__);
	ret = device_create_file(&pdev->dev, &dev_attr_temp);
	if (ret)
		pr_err("%s Failed to create temp attr\n", __func__);

	return 0;

therm_fw_alloc_err:
        mutex_destroy(&virtual_sensor->sensor_mutex);
        kfree(virtual_sensor);
        return ret;
}

static int mtktspmic_remove(struct platform_device *pdev)
{
	struct virtual_sensor_temp_sensor *virtual_sensor = platform_get_drvdata(pdev);

        if (virtual_sensor->therm_fw)
		kfree(virtual_sensor->therm_fw);
        if (virtual_sensor)
		kfree(virtual_sensor);

	device_remove_file(&pdev->dev, &dev_attr_params);
	device_remove_file(&pdev->dev, &dev_attr_temp);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mtkspmic_of_match_table[] = {
		{.compatible = "amazon,mtkspmic_sensor", },
		{},
};
MODULE_DEVICE_TABLE(of, mtkspmic_of_match_table);
#endif

static struct platform_driver mtktspmic_driver = {
	.probe = mtktspmic_probe,
	.remove = mtktspmic_remove,
	.driver     = {
		.name  = PMIC_SENSOR_NAME,
#ifdef CONFIG_OF
		.of_match_table = mtkspmic_of_match_table,
#endif
		.owner = THIS_MODULE,
	},
};

static int __init mtktspmic_sensor_init(void)
{
	int ret;

	ret = platform_driver_register(&mtktspmic_driver);
	if (ret) {
		pr_err("Unable to register mtktspmic driver (%d)\n", ret);
		goto err_unreg;
	}
	return 0;

err_unreg:
	return ret;
}

static void __exit mtktspmic_sensor_exit(void)
{
	platform_driver_unregister(&mtktspmic_driver);
}

module_init(mtktspmic_sensor_init);
module_exit(mtktspmic_sensor_exit);
