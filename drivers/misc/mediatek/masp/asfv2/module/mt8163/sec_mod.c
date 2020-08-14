/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/******************************************************************************
 *  INCLUDE LINUX HEADER
 ******************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#ifdef CONFIG_OF
#include <linux/of_fdt.h>
#endif
#include "../sec_clk.h"
/******************************************************************************
 *  INCLUDE LIBRARY
 ******************************************************************************/

#define SEC_DEV_NAME                "HACC"
#define MOD                         "hacc"

#define TRACE_FUNC()                MSG_FUNC(SEC_DEV_NAME)


/*************************************************************************
 *  GLOBAL VARIABLE
 **************************************************************************/

#ifdef CONFIG_ARM64
unsigned long long hacc_base;
#else
unsigned int hacc_base;
#endif

static const struct of_device_id hacc_of_ids[] = {
	{.compatible = "mediatek,hacc",},
	{}
};

/**************************************************************************
 *  SEC DRIVER INIT
 **************************************************************************/
static int sec_init(struct platform_device *dev)
{
	int ret = 0;

	pr_debug("[%s] sec_init (%d)\n", SEC_DEV_NAME, ret);

	#ifdef CONFIG_ARM64
	hacc_base = (unsigned long long)of_iomap(dev->dev.of_node, 0);
	#else
	hacc_base = (unsigned int)of_iomap(dev->dev.of_node, 0);
	#endif
	if (!hacc_base) {
		pr_err("[%s] HACC register remapping failed\n", SEC_DEV_NAME);
		return -ENXIO;
	}

	ret = sec_clk_enable(dev);
	if (ret) {
		pr_err("[%s] Cannot get hacc clock\n", SEC_DEV_NAME);
		return ret;
	}

	return ret;
}

/**************************************************************************
 *  hacc PLATFORM DRIVER WRAPPER, FOR BUILD-IN SEQUENCE
 **************************************************************************/
int hacc_probe(struct platform_device *dev)
{
	int ret = 0;

	ret = sec_init(dev);
	return ret;
}

int hacc_remove(struct platform_device *dev)
{
	return 0;
}

static struct platform_driver hacc_driver = {
	.driver = {
			.name = "hacc",
			.owner = THIS_MODULE,
			.of_match_table = hacc_of_ids,
			},
	.probe = hacc_probe,
	.remove = hacc_remove,
};

static int __init hacc_init(void)
{
	int ret;

	ret = platform_driver_register(&hacc_driver);
	if (ret) {
		pr_err("[%s] Reg platform driver failed (%d)\n",
			SEC_DEV_NAME, ret);
		return ret;
	}

	return ret;
}

static void __exit hacc_exit(void)
{
	platform_driver_unregister(&hacc_driver);
}
module_init(hacc_init);
module_exit(hacc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("Mediatek Security Module");
