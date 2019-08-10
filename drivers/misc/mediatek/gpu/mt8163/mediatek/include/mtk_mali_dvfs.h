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


#ifndef _MTK_DVFS_H_
#define _MTK_DVFS_H_


void mtk_mali_dvfs_init(void);
void mtk_mali_dvfs_deinit(void);
void mtk_mali_dvfs_notify_utilization(int utilization) ;
void mtk_mali_dvfs_set_threshold(int down, int up);
void mtk_mali_dvfs_get_threshold(int *down, int *up);
void mtk_mali_dvfs_power_on(void);
void mtk_mali_dvfs_power_off(void);
void mtk_mali_dvfs_enable(void);
void mtk_mali_dvfs_disable(void);

#endif
