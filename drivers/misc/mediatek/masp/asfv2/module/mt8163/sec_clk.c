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
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/clk.h>
#include <linux/of_platform.h>
#include "../sec_clk.h"

#define SEC_DEV_NAME                "sec"

static struct clk *sec_clk;

int sec_clk_enable(struct platform_device *dev)
{
	sec_clk = devm_clk_get(&dev->dev, "main");
	if (IS_ERR(sec_clk))
		return -1;

	clk_prepare_enable(sec_clk);
	pr_debug("[%s] get hacc clock\n", SEC_DEV_NAME);

	return 0;
}

