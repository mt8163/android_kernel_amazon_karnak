/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef _MT_GPUFREQ_H
#define _MT_GPUFREQ_H

#include <linux/module.h>

/* #define MT_GPUFREQ_LOW_BATT_VOLT_PROTECT */
/* #define MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT */
/* #define MT_GPUFREQ_OC_PROTECT */
/* #define MT_GPUFREQ_INPUT_BOOST */

/* Power table struct for thermal */
struct mt_gpufreq_power_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_volt;
	unsigned int gpufreq_power;
};

struct mtk_gpu_power_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_power;
};

extern u32 get_devinfo_with_index(u32 index);

/* extern function */
extern int mtk_gpufreq_register(struct mtk_gpu_power_info *freqs, int num);
extern unsigned int mt_gpufreq_get_cur_freq_index(void);
extern unsigned int mt_gpufreq_get_cur_freq(void);
extern unsigned int mt_gpufreq_get_cur_volt(void);
extern unsigned int mt_gpufreq_get_dvfs_table_num(void);
extern int mt_gpufreq_target(unsigned int idx);
extern int mt_gpufreq_voltage_enable_set(unsigned int enable);
extern unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx);
extern void mt_gpufreq_thermal_protect(unsigned int limited_power);
extern unsigned int mt_gpufreq_get_thermal_limit_index(void);
extern unsigned int mt_gpufreq_get_thermal_limit_freq(void);
extern unsigned int mt_gpufreq_get_leakage_mw(void);

/* power limit notification */
typedef void (*gpufreq_power_limit_notify) (unsigned int);
extern void mt_gpufreq_power_limit_notify_registerCB
	(gpufreq_power_limit_notify pCB);

/* input boost notification */
typedef void (*gpufreq_input_boost_notify) (unsigned int);
extern void mt_gpufreq_input_boost_notify_registerCB
	(gpufreq_input_boost_notify pCB);

/* profiling purpose */
typedef void (*sampler_func) (unsigned int);
extern void mt_gpufreq_setfreq_registerCB(sampler_func pCB);
extern void mt_gpufreq_setvolt_registerCB(sampler_func pCB);

#endif
