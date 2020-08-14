/*
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.
 * All Rights Reserved
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
 */

#include <linux/thermal.h>
#include <linux/sign_of_life.h>

#define TZ_MTK_TS_CPU "mtktscpu"
#define TZ_SKIN_VIRTUAL "skin_virtual"
#define TZ_CASE_VIRTUAL "case_virtual"

int amazon_thermal_set_shutdown_reason(struct thermal_zone_device *tz)
{
	if (!tz || !tz->type) {
		pr_err("%s: Thermal zone is NULL\n", __func__);
		return -EINVAL;
	}

	if (!strcmp(tz->type, TZ_MTK_TS_CPU))
		life_cycle_set_thermal_shutdown_reason(THERMAL_SHUTDOWN_REASON_SOC);
	else if (!strcmp(tz->type, TZ_SKIN_VIRTUAL) || !strcmp(tz->type, TZ_CASE_VIRTUAL))
		life_cycle_set_thermal_shutdown_reason(THERMAL_SHUTDOWN_REASON_VS);
	else
		pr_err("%s: Unsupported thermal zone", __func__);
		return -EINVAL;

	return 0;
}
