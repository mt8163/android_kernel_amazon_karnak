/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <mt-plat/battery_common.h>

/************* ATTENTATION ***************/
/* IF ANY NEW CHARGER IC SUPPORT IN THIS FILE, */
/* REMEMBER TO NOTIFY USB OWNER TO MODIFY OTG RELATED FILES!! */

void tbl_charger_otg_vbus(int mode)
{
	unsigned int otg_enable = mode & 0xFF;
	bool chr_status;

	battery_charging_control(
		CHARGING_CMD_GET_CHARGER_DET_STATUS, &chr_status);
	pr_debug("[%s] mode = %d, vchr(%d)\n",
		__func__, mode, chr_status);

	if (chr_status && otg_enable)
		return;

	battery_charging_control(CHARGING_CMD_BOOST_ENABLE, &otg_enable);
}
