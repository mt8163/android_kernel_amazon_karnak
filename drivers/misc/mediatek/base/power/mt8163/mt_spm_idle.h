#ifndef _MT_SPM_IDLE_
#define _MT_SPM_IDLE_

#include <linux/kernel.h>
#include "mt_spm.h"
#include "mt_spm_sleep.h"

#define spm_idle_ver(fmt, args...)		pr_debug("[SPM-Idle] " fmt, ##args)

/*
 * for Deep Idle
 */
void spm_dpidle_before_wfi(void);	/* can be redefined */
void spm_dpidle_after_wfi(void);	/* can be redefined */
wake_reason_t spm_go_to_dpidle(u32 spm_flags, u32 spm_data);
wake_reason_t spm_go_to_sleep_dpidle(u32 spm_flags, u32 spm_data);
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
void spm_enable_sodi(bool);
bool spm_get_sodi_en(void);
void spm_sodi_cpu_dvs_en(bool en);

extern unsigned int mt_get_clk_mem_sel(void);

#endif
