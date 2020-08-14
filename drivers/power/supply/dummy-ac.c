/*
 * Dummy AC power supply driver
 *
 * Copyright (C) 2016 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>

struct dummy_ac_info {
	struct platform_device *pdev;
	struct power_supply *ac;
	struct power_supply_desc desc;
};

static enum power_supply_property dummy_ac_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TYPE,
};

static int dummy_ac_remove(struct platform_device *pdev);

static int dummy_ac_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 1;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int dummy_ac_probe(struct platform_device *pdev)
{
	struct dummy_ac_info *info;
	struct power_supply_config psy_cfg = {};

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "mem alloc failed\n");
		return -ENOMEM;
	}

	info->pdev = pdev;
	platform_set_drvdata(pdev, info);

	info->desc.name = "ac";
	info->desc.type = POWER_SUPPLY_TYPE_MAINS;
	info->desc.properties = dummy_ac_props;
	info->desc.num_properties = ARRAY_SIZE(dummy_ac_props);
	info->desc.get_property = dummy_ac_get_property;
	psy_cfg.drv_data = info;
	info->ac = power_supply_register(&pdev->dev, &info->desc, &psy_cfg);

	if (IS_ERR(info->ac)){
		dev_err(&pdev->dev, "Failed: power supply register(%ld)\n", PTR_ERR(info->ac));
		dummy_ac_remove(info->pdev);
	}else
		dev_err(&pdev->dev, "dummy ac power supply registered\n");

	return 0;
}

static int dummy_ac_remove(struct platform_device *pdev)
{
	struct dummy_ac_info *info =  dev_get_drvdata(&pdev->dev);

	power_supply_unregister(info->ac);
	return 0;
}
static struct platform_driver dummy_ac_driver = {
	.driver = {
		.name = "dummy-ac",
		.owner	= THIS_MODULE,
	},
	.probe = dummy_ac_probe,
	.remove = dummy_ac_remove,
};

static struct platform_device *dummy_pdev;

static int __init dummy_ac_init(void)
{
	int ret;

	ret =  platform_driver_register(&dummy_ac_driver);
	if (ret < 0) {
		pr_err("dummy-ac driver reg failed\n");
		return ret;
	}

	dummy_pdev = platform_device_alloc("dummy-ac", -1);
	if (!dummy_pdev) {
		pr_err("dummy-ac device alloc failed\n");
		return -ENODEV;
	}

	ret = platform_device_add(dummy_pdev);
	if (ret) {
		pr_err("dummy-ac device add failed\n");
		return ret;
	}

	return 0;
}
device_initcall(dummy_ac_init);

static void __exit dummy_ac_exit(void)
{
	platform_driver_unregister(&dummy_ac_driver);
}
module_exit(dummy_ac_exit);

MODULE_AUTHOR("Ramakrishna Pallala <ramakrishna.pallala@intel.com>");
MODULE_DESCRIPTION("Dummy AC power supply Driver");
MODULE_LICENSE("GPL");
