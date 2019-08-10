/**
 * @file mt_cpufreq.h
 * @brief CPU DVFS driver interface
 */

#ifndef __MT_CPUFREQ_H__
#define __MT_CPUFREQ_H__

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================*/
/* Include files */
/*=============================================================*/

/* system includes */

/* project includes */

/* local includes */

/* forward references */


/*=============================================================*/
/* Macro definition */
/*=============================================================*/


/*=============================================================*/
/* Type definition */
/*=============================================================*/

enum mt_cpu_dvfs_id {
	MT_CPU_DVFS_LITTLE,
	NR_MT_CPU_DVFS,
};

enum top_ckmuxsel {
	TOP_CKMUXSEL_CLKSQ   = 0, /* i.e. reg setting */
	TOP_CKMUXSEL_ARMPLL  = 1,
	TOP_CKMUXSEL_MAINPLL = 2,
	TOP_CKMUXSEL_UNIVPLL = 3,

	NR_TOP_CKMUXSEL,
};

/*
 * PMIC_WRAP
 */

/* Phase */
enum pmic_wrap_phase_id {
	PMIC_WRAP_PHASE_NORMAL,
	PMIC_WRAP_PHASE_SUSPEND,
	PMIC_WRAP_PHASE_DEEPIDLE,
	PMIC_WRAP_PHASE_SODI,

	NR_PMIC_WRAP_PHASE,
};

/* IDX mapping */
enum {
	IDX_NM_VSRAM,		/* 0 */ /* PMIC_WRAP_PHASE_NORMAL */
	IDX_NM_VPROC,		/* 1 */
	IDX_NM_VGPU,		/* 2 */
	IDX_NM_VCORE,		/* 3 */
	IDX_NM_VLTE,		/* 4 */

	NR_IDX_NM,
};
enum {
	IDX_SP_VPROC_PWR_ON,        /* 0 */ /* PMIC_WRAP_PHASE_SUSPEND */
	IDX_SP_VPROC_SHUTDOWN,      /* 1 */
	IDX_SP_VSRAM_PWR_ON,        /* 2 */
	IDX_SP_VSRAM_SHUTDOWN,      /* 3 */
	IDX_SP_VCORE_NORMAL,        /* 4 */
	IDX_SP_VCORE_SLEEP,         /* 5 */
	IDX_SP_VPROC_NORMAL,        /* 6 */
	IDX_SP_VPROC_SLEEP,         /* 7 */

	NR_IDX_SP,
};
enum {
	IDX_DI_VPROC_NORMAL,	/* 0 */ /* PMIC_WRAP_PHASE_DEEPIDLE */
	IDX_DI_VPROC_SLEEP,	/* 1 */
	IDX_DI_VSRAM_NORMAL,	/* 2 */
	IDX_DI_VSRAM_SLEEP,	/* 3 */
	IDX_DI_VCORE_NORMAL,	/* 4 */
	IDX_DI_VCORE_SLEEP,		/* 5 */
	IDX_DI_VSRAM_FAST_TRSN_DIS, /* 6 */
	IDX_DI_VSRAM_FAST_TRSN_EN,  /* 7 */

	NR_IDX_DI,
};



typedef void (*cpuVoltsampler_func)(enum mt_cpu_dvfs_id , unsigned int mv);
/*=============================================================*/
/* Global variable definition */
/*=============================================================*/


/*=============================================================*/
/* Global function definition */
/*=============================================================*/

/* PMIC WRAP */
extern void mt_cpufreq_set_pmic_phase(enum pmic_wrap_phase_id phase);
extern void mt_cpufreq_set_pmic_cmd(enum pmic_wrap_phase_id phase, int idx, unsigned int cmd_wdata);
extern void mt_cpufreq_apply_pmic_cmd(int idx);

/* PTP-OD */
extern unsigned int mt_cpufreq_get_freq_by_idx(enum mt_cpu_dvfs_id id, int idx);
extern int mt_cpufreq_update_volt(enum mt_cpu_dvfs_id id, unsigned int *volt_tbl, int nr_volt_tbl);
extern void mt_cpufreq_restore_default_volt(enum mt_cpu_dvfs_id id);
extern unsigned int mt_cpufreq_get_cur_volt(enum mt_cpu_dvfs_id id);
extern void mt_cpufreq_enable_by_ptpod(enum mt_cpu_dvfs_id id);
extern unsigned int mt_cpufreq_disable_by_ptpod(enum mt_cpu_dvfs_id id);

/* Thermal */
extern void mt_cpufreq_thermal_protect(unsigned int limited_power);

/* SDIO */
extern void mt_vcore_dvfs_disable_by_sdio(unsigned int type, bool disabled);
extern void mt_vcore_dvfs_volt_set_by_sdio(unsigned int volt);
extern unsigned int mt_vcore_dvfs_volt_get_by_sdio(void);

extern unsigned int mt_get_cur_volt_vcore_ao(void);
/* extern unsigned int mt_get_cur_volt_vcore_pdn(void); */

/* Generic */
extern int mt_cpufreq_state_set(int enabled);
extern int mt_cpufreq_clock_switch(enum mt_cpu_dvfs_id id, enum top_ckmuxsel sel);
extern enum top_ckmuxsel mt_cpufreq_get_clock_switch(enum mt_cpu_dvfs_id id);
extern void mt_cpufreq_setvolt_registerCB(cpuVoltsampler_func pCB);
extern bool mt_cpufreq_earlysuspend_status_get(void);
void interactive_boost_cpu(int boost);

extern void mt_cpufreq_set_ramp_down_count_const(enum mt_cpu_dvfs_id id, int count);

extern int is_ext_buck_sw_ready(void);
extern int is_ext_buck_exist(void);
/* mt8163-TBD external buck */
extern unsigned long sym827_buck_get_voltage(void);
extern int sym827_buck_set_voltage(unsigned long voltage);

/* move from mt_cpufreq.c to mt_cpufreq.h MB */
extern unsigned int get_pmic_mt6325_cid(void);
extern u32 get_devinfo_with_index(u32 index);
/* extern int mtktscpu_get_Tj_temp(void); */ /* TODO: ask Jerry to provide the head file */
extern void (*cpufreq_freq_check)(enum mt_cpu_dvfs_id id);
/* Freq Meter API */
#ifdef __KERNEL__
extern unsigned int mt_get_cpu_freq(void);
#endif

extern void aee_rr_rec_cpu_dvfs_vproc_big(u8 val);
extern void aee_rr_rec_cpu_dvfs_vproc_little(u8 val);
extern void aee_rr_rec_cpu_dvfs_oppidx(u8 val);
extern u8 aee_rr_curr_cpu_dvfs_oppidx(void);
extern void aee_rr_rec_cpu_dvfs_status(u8 val);
extern u8 aee_rr_curr_cpu_dvfs_status(void);

extern void do_DRAM_DFS(int high_freq);
extern unsigned int mt_get_emi_freq(void);
extern unsigned int is_md32_enable(void);

/* PMIC WRAP ADDR */ /* TODO: include other head file */
#ifdef CONFIG_OF
extern void __iomem *pwrap_base;
#define PWRAP_BASE_ADDR     ((unsigned long)pwrap_base)
#else
#include "mach/mt_reg_base.h"
#define PWRAP_BASE_ADDR     PWRAP_BASE
#endif

/* move from mt_cpufreq.c to mt_cpufreq.h ME */

#ifndef __KERNEL__
extern int mt_cpufreq_pdrv_probe(void);
extern int mt_cpufreq_set_opp_volt(enum mt_cpu_dvfs_id id, int idx);
extern int mt_cpufreq_set_freq(enum mt_cpu_dvfs_id id, int idx);
extern unsigned int dvfs_get_cpu_freq(enum mt_cpu_dvfs_id id);
extern void dvfs_set_cpu_freq_FH(enum mt_cpu_dvfs_id id, int freq);
extern unsigned int cpu_frequency_output_slt(enum mt_cpu_dvfs_id id);
extern void dvfs_set_cpu_volt(enum mt_cpu_dvfs_id id, int volt);
extern void dvfs_set_gpu_volt(int pmic_val);
extern void dvfs_set_vcore_ao_volt(int pmic_val);
/* extern void dvfs_set_vcore_pdn_volt(int pmic_val); */
extern void dvfs_disable_by_ptpod(void);
extern void dvfs_enable_by_ptpod(void);
#endif /* ! __KERNEL__ */

#ifdef __cplusplus
}
#endif

#endif /* __MT_CPUFREQ_H__ */
