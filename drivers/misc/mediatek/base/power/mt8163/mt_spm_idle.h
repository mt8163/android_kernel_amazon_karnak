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

#ifndef _MT_SPM_IDLE_
#define _MT_SPM_IDLE_

#include <linux/kernel.h>
#include "mt_spm.h"
#include "mt_spm_sleep.h"

#define spm_idle_ver(fmt, args...) pr_debug("[SPM-Idle] " fmt, ##args)

/* AEE */
#ifdef CONFIG_MTK_RAM_CONSOLE
#define SPM_AEE_RR_REC 1
#else
#define SPM_AEE_RR_REC 0
#endif

/*
 * for Deep Idle
 */
void spm_dpidle_before_wfi(void);	/* can be redefined */
void spm_dpidle_after_wfi(void);	/* can be redefined */
int spm_go_to_dpidle(u32 spm_flags, u32 spm_data);
int spm_go_to_sleep_dpidle(u32 spm_flags, u32 spm_data);
int spm_set_dpidle_wakesrc(u32 wakesrc, bool enable, bool replace);
bool spm_set_dpidle_pcm_init_flag(void);


/*
 * for Screen On Deep Idle
 */
void soidle_before_wfi(int cpu);	/* can be redefined */
void soidle_after_wfi(int cpu);	/* can be redefined */
void spm_go_to_sodi(u32 spm_flags, u32 spm_data);
void spm_sodi_lcm_video_mode(bool IsLcmVideoMode);
void spm_sodi_mempll_pwr_mode(bool pwr_mode);
void spm_enable_sodi(bool en);
bool spm_get_sodi_en(void);
void spm_sodi_cpu_dvs_en(bool en);
void spm_sodi_init(void);
#if SPM_AEE_RR_REC
extern void aee_rr_rec_sodi_val(u32 val);
extern u32 aee_rr_curr_sodi_val(void);
#endif

#endif
