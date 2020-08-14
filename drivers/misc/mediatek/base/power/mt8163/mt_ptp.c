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

/**
 * @file    mt_ptp.c
 * @brief   Driver for PTP
 *
 */

#define __MT_PTP_C__

/*=============================================================
 * Include files
 *=============================================================
 */

#define pr_fmt(fmt)	"[PTP] "fmt

/* system includes */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <mt-plat/aee.h>
#include <mtk_ram_console.h>
#include <mach/mt_thermal.h>
#include <mach/mt_freqhopping.h>
#include "mt_ptp.h"
#include "mt_cpufreq.h"

#ifdef CONFIG_MTK_INHOUSE_GPU
#include "mt_gpufreq.h"
#endif

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

/* Global variable for slow idle */
unsigned int ptp_data[3] = {0, 0, 0};

struct ptp_det;
struct ptp_ctrl;

static void ptp_set_ptp_volt(struct ptp_det *det);
static void ptp_restore_ptp_volt(struct ptp_det *det);
/* static void ptp_init01_prepare(struct ptp_det *det); */
/* static void ptp_init01_finish(struct ptp_det *det); */

/*=============================================================
 * Macro definition
 *=============================================================
 */

/*
 * CONFIG (SW related)
 */
#define CONFIG_PTP_SHOWLOG	(1)
#define PTP_GET_REAL_VAL	(1) /* get val from efuse */
#define SET_PMIC_VOLT		(1) /* apply PMIC voltage */

/* DUMP_DATA_TO_DE is deprecated. Please use ptp_log_en=1,
 * if you want to dump each bank's register at ptp_isr_handler
 */
#define DUMP_DATA_TO_DE		(1)
#define LOG_INTERVAL		(2LL * NSEC_PER_SEC)
#define NR_FREQ			(8)

/*
 * 100 us, This is the PTP Detector sampling time as represented in
 * cycles of bclk_ck during INIT. 52 MHz
 */
#define DETWINDOW_VAL		0x514

/*
 * mili Volt to config value. voltage = 600mV + val * 6.25mV
 * val = (voltage - 600) / 6.25
 * @mV:	mili volt
 */
#if 1
/* from Brian Yang,1mV=>10uV */
/* ((((volt) - 700 * 100 + 625 - 1) / 625) */
#define PTP_VOLT_TO_PMIC_VAL(volt)  (((volt) - 70000 + 625 - 1) / 625)

/* pmic value from ptpod already add 16 steps(16*6.25=100mV)
 * for real voltage transfer (((pmic) * 625) / 100 + 700)
 */
#define PTP_PMIC_VAL_TO_VOLT(pmic)  (((pmic) * 625) + 70000)
#else
/* TODO: FIXME, refer to VOLT_TO_PMIC_VAL() */
#define MV_TO_VAL(MV)		((((MV) - 600) * 100 + 625 - 1) / 625)
/* TODO: FIXME, refer to PMIC_VAL_TO_VOLT() */
#define VAL_TO_MV(VAL)		(((VAL) * 625) / 100 + 600)
#endif
/* offset 0x10(16 steps) for CPU/GPU DVFS */
#define PTPOD_PMIC_OFFSET (0x0)


#define VMAX_VAL		PTP_VOLT_TO_PMIC_VAL(125000)
#define VMIN_VAL		PTP_VOLT_TO_PMIC_VAL(105000)
#define VMIN_VAL_GPU		PTP_VOLT_TO_PMIC_VAL(115000)


#define DTHI_VAL		0x01		/* positive */
#define DTLO_VAL		0xfe		/* negative (2's compliment) */
#define DETMAX_VAL		0xffff
#define AGECONFIG_VAL		0x555555
#define AGEM_VAL		0x0
#define DVTFIXED_VAL		0x6
#define VCO_VAL			0x10
#define DCCONFIG_VAL		0x555555

/*
 * bit operation
 */
#undef  BIT
#define BIT(bit)	(1U << (bit))

#define MSB(range)	(1 ? range)
#define LSB(range)	(0 ? range)
/**
 * Genearte a mask wher MSB to LSB are all 0b1
 * @r:	Range in the form of MSB:LSB
 */
#define BITMASK(r)	\
	(((u32) -1 >> (31 - MSB(r))) & ~((1U << LSB(r)) - 1))

/**
 * Set value at MSB:LSB. For example, BITS(7:3, 0x5A)
 * will return a value where bit 3 to bit 7 is 0x5A
 * @r:	Range in the form of MSB:LSB
 */
/* BITS(MSB:LSB, value) => Set value at MSB:LSB  */
#define BITS(r, val)	((val << LSB(r)) & BITMASK(r))

/*
 * LOG
 */
#define FUNC_LV_MODULE          BIT(0)  /* module, platform driver interface */
#define FUNC_LV_CPUFREQ         BIT(1)  /* cpufreq driver interface          */
#define FUNC_LV_API             BIT(2)  /* mt_cpufreq driver global function */
#define FUNC_LV_LOCAL           BIT(3)  /* mt_cpufreq driver lcaol function  */
#define FUNC_LV_HELP            BIT(4)  /* mt_cpufreq driver help function   */

/* (FUNC_LV_MODULE | FUNC_LV_CPUFREQ | FUNC_LV_API
 * | FUNC_LV_LOCAL | FUNC_LV_HELP);
 */
static unsigned int func_lv_mask;

#if defined(CONFIG_PTP_SHOWLOG)
#define FUNC_ENTER(lv)	do { \
	if ((lv) & func_lv_mask) \
		pr_debug(">> %s()\n", __func__); \
} while (0)

#define FUNC_EXIT(lv)	do { \
	if ((lv) & func_lv_mask) \
		pr_debug("<< %s():%d\n", __func__, __LINE__); \
} while (0)
#else
#define FUNC_ENTER(lv)
#define FUNC_EXIT(lv)
#endif /* CONFIG_CPU_DVFS_SHOWLOG */

/*
 * REG ACCESS
 */

#define ptp_read(addr)	__raw_readl(addr)
#define ptp_read_field(addr, range)	\
	((ptp_read(addr) & BITMASK(range)) >> LSB(range))

#define ptp_write(addr, val)	mt_reg_sync_writel(val, addr)

/**
 * Write a field of a register.
 * @addr:	Address of the register
 * @range:	The field bit range in the form of MSB:LSB
 * @val:	The value to be written to the field
 */
#define ptp_write_field(addr, range, val)	\
	ptp_write(addr, (ptp_read(addr) & ~BITMASK(range)) | BITS(range, val))

#ifdef CONFIG_MTK_RAM_CONSOLE
enum ptp_state {
	PTP_CPU_BIG_IS_SET_VOLT = 0,    /* B */
	PTP_GPU_IS_SET_VOLT = 1,        /* G */
	PTP_CPU_LITTLE_IS_SET_VOLT = 2, /* L */
};
#endif

/**
 * Helper macros
 */

/* PTP detector is disabled by who */
enum {
	BY_PROCFS	= BIT(0),
	BY_INIT_ERROR	= BIT(1),
	BY_MON_ERROR	= BIT(2),
};

#ifdef CONFIG_OF

void __iomem *ptpod_base;
u32 ptpod_irq_number;
int ptpod_phy_base;
#endif


/**
 * iterate over list of detectors
 * @det:	the detector * to use as a loop cursor.
 */
#define for_each_det(det) \
	for (det = ptp_detectors; \
		det < (ptp_detectors + ARRAY_SIZE(ptp_detectors)); det++)

/**
 * iterate over list of detectors and its controller
 * @det:	the detector * to use as a loop cursor.
 * @ctrl:	the ptp_ctrl * to use as ctrl pointer of current det.
 */
#define for_each_det_ctrl(det, ctrl)				\
	for (det = ptp_detectors,				\
	     ctrl = id_to_ptp_ctrl(det->ctrl_id);		\
	     det < (ptp_detectors + ARRAY_SIZE(ptp_detectors)); \
	     det++,						\
	     ctrl = id_to_ptp_ctrl(det->ctrl_id))

/**
 * iterate over list of controllers
 * @pos:	the ptp_ctrl * to use as a loop cursor.
 */
#define for_each_ctrl(ctrl) \
	for (ctrl = ptp_ctrls; \
		ctrl < (ptp_ctrls + ARRAY_SIZE(ptp_ctrls)); ctrl++)

/**
 * Given a ptp_det * in ptp_detectors. Return the id.
 * @det:	pointer to a ptp_det in ptp_detectors
 */
#define det_to_id(det)	((det) - &ptp_detectors[0])

/**
 * Given a ptp_ctrl * in ptp_ctrls. Return the id.
 * @det:	pointer to a ptp_ctrl in ptp_ctrls
 */
#define ctrl_to_id(ctrl)	((ctrl) - &ptp_ctrls[0])

/**
 * Check if a detector has a feature
 * @det:	pointer to a ptp_det to be check
 * @feature:	enum ptp_features to be checked
 */
#define HAS_FEATURE(det, feature)	((det)->features & feature)

#define PERCENT(nmrtr, dnmntr)	\
	(unsigned char)(((nmrtr) * 100 + (dnmntr) - 1) / (dnmntr))

static int ptp_log_en;
/*=============================================================
 * Local type definition
 *=============================================================
 */

/*
 * CONFIG (CHIP related)
 * PTPCORESEL.APBSEL
 */
unsigned int reg_dump_addr_off[] = {
	0x0000,
	0x0004,
	0x0008,
	0x000C,
	0x0010,
	0x0014,
	0x0018,
	0x001c,
	0x0024,
	0x0028,
	0x002c,
	0x0030,
	0x0034,
	0x0038,
	0x003c,
	0x0040,
	0x0044,
	0x0048,
	0x004c,
	0x0050,
	0x0054,
	0x0058,
	0x005c,
	0x0060,
	0x0064,
	0x0068,
	0x006c,
	0x0070,
	0x0074,
	0x0078,
	0x007c,
	0x0080,
	0x0084,
	0x0088,
	0x008c,
	0x0090,
	0x0094,
	0x0098,
	0x00a0,
	0x00a4,
	0x00a8,
	0x00B0,
	0x00B4,
	0x00B8,
	0x00BC,
	0x00C0,
	0x00C4,
	0x00C8,
	0x00CC,
	0x00F0,
	0x00F4,
	0x00F8,
	0x00FC,
	0x0200,
	0x0204,
	0x0208,
	0x020C,
	0x0210,
	0x0214,
	0x0218,
	0x021C,
	0x0220,
	0x0224,
	0x0228,
	0x022C,
	0x0230,
	0x0234,
	0x0238,
	0x023C,
	0x0240,
	0x0244,
	0x0248,
	0x024C,
	0x0250,
	0x0254,
	0x0258,
	0x025C,
	0x0260,
	0x0264,
	0x0268,
	0x026C,
	0x0270,
	0x0274,
	0x0278,
	0x027C,
	0x0280,
	0x0284,
	0x0400,
	0x0404,
	0x0408,
	0x040C,
	0x0410,
	0x0414,
	0x0418,
	0x041C,
	0x0420,
	0x0424,
	0x0428,
	0x042C,
	0x0430,
};

enum ptp_phase {
	PTP_PHASE_INIT01,
	PTP_PHASE_INIT02,
	PTP_PHASE_MON,

	NR_PTP_PHASE,
};

enum {
	PTP_VOLT_NONE    = 0,
	PTP_VOLT_UPDATE  = BIT(0),
	PTP_VOLT_RESTORE = BIT(1),
};

struct ptp_ctrl {
	const char *name;
	enum ptp_det_id det_id;
	/* struct completion init_done; */
	/* atomic_t in_init; */
	/* for voltage setting thread */
	wait_queue_head_t wq;
	int volt_update;
	struct task_struct *thread;
};

struct ptp_det_ops {
	/* interface to PTP-OD */
	void (*enable)(struct ptp_det *det, int reason);
	void (*disable)(struct ptp_det *det, int reason);
	void (*disable_locked)(struct ptp_det *det, int reason);
	void (*switch_bank)(struct ptp_det *det);

	int (*init01)(struct ptp_det *det);
	int (*init02)(struct ptp_det *det);
	int (*mon_mode)(struct ptp_det *det);

	int (*get_status)(struct ptp_det *det);
	void (*dump_status)(struct ptp_det *det);

	void (*set_phase)(struct ptp_det *det, enum ptp_phase phase);

	/* interface to thermal */
	int (*get_temp)(struct ptp_det *det);

	/* interface to DVFS */
	int (*get_volt)(struct ptp_det *det);
	int (*set_volt)(struct ptp_det *det);
	void (*restore_default_volt)(struct ptp_det *det);
	void (*get_freq_table)(struct ptp_det *det);
};

enum ptp_features {
	FEA_INIT01	= BIT(PTP_PHASE_INIT01),
	FEA_INIT02	= BIT(PTP_PHASE_INIT02),
	FEA_MON		= BIT(PTP_PHASE_MON),
};

struct ptp_det {
	const char *name;
	struct ptp_det_ops *ops;
	int status;			/* TODO: enable/disable */
	int features;		/* enum ptp_features */
	enum ptp_ctrl_id ctrl_id;

	/* devinfo */
	unsigned int PTPINITEN;
	unsigned int PTPMONEN;
	unsigned int MDES;
	unsigned int BDES;
	unsigned int DCMDET;
	unsigned int DCBDET;
	unsigned int AGEDELTA;
	unsigned int MTDES;

	/* constant */
	unsigned int DETWINDOW;
	unsigned int VMAX;
	unsigned int VMIN;
	unsigned int DTHI;
	unsigned int DTLO;
	unsigned int VBOOT;
	unsigned int DETMAX;
	unsigned int AGECONFIG;
	unsigned int AGEM;
	unsigned int DVTFIXED;
	unsigned int VCO;
	unsigned int DCCONFIG;

	/* Generated by PTP init01. Used in PTP init02 */
	unsigned int DCVOFFSETIN;
	unsigned int AGEVOFFSETIN;

	/* for debug */
	unsigned int dcvalues[NR_PTP_PHASE];

	unsigned int ptp_freqpct30[NR_PTP_PHASE];
	unsigned int ptp_26c[NR_PTP_PHASE];
	unsigned int ptp_vop30[NR_PTP_PHASE];
	unsigned int ptp_ptpen[NR_PTP_PHASE];
#if DUMP_DATA_TO_DE
	unsigned int reg_dump_data[ARRAY_SIZE(reg_dump_addr_off)][NR_PTP_PHASE];
#endif
	/* slope */
	unsigned int MTS;
	unsigned int BTS;

	/* dvfs */
	unsigned int num_freq_tbl; /* can get the same time with freq_tbl[] */
	unsigned int max_freq_khz; /* max_freq used to calculate percentage */
	unsigned char freq_tbl[NR_FREQ]; /* percentage to maximum freq */

	unsigned int volt_tbl[NR_FREQ]; /* pmic value */
	unsigned int volt_tbl_init2[NR_FREQ]; /* pmic value */
	unsigned int volt_tbl_pmic[NR_FREQ]; /* pmic value */
	int volt_offset;

	int disabled; /* Disabled by error or sysfs */
};


struct ptp_devinfo {
	/* PTP_OD0 */
	unsigned int PTPINITEN		: 1;
	unsigned int PTPMONEN		: 1;
	unsigned int Bodybias		: 1;
	unsigned int PTPOD_T		: 1;
	unsigned int EPS			: 1;
	unsigned int PTP_OD0_OTHERS : 27;

	/* PTP_OD1 */
	unsigned int PTP_OD1		: 32;

	/* PTP_OD2 */
	unsigned int CPU_BDES		: 8;
	unsigned int CPU_MDES		: 8;
	unsigned int CPU_DCBDET		: 8;
	unsigned int CPU_DCMDET		: 8;

	/* PTP_OD3 */
	unsigned int GPU_MTDES		: 8;
	unsigned int GPU_AGEDELTA	: 8;
	unsigned int CPU_MTDES		: 8;
	unsigned int CPU_AGEDELTA	: 8;

	/* PTP_OD4 */
	unsigned int GPU_BDES		: 8;
	unsigned int GPU_MDES		: 8;
	unsigned int GPU_DCBDET		: 8;
	unsigned int GPU_DCMDET		: 8;

	/* PTP_OD5 */
	unsigned int CPU_SPD_BOND	: 3;
	unsigned int PTP_OD5_Rsrv	: 13;
	unsigned int CPU_Leakage	: 8;
	unsigned int GPU_Leakage	: 8;
};


/*=============================================================
 *Local variable definition
 *=============================================================
 */

/**
 * PTP controllers
 */

struct ptp_ctrl ptp_ctrls[NR_PTP_CTRL] = {
	[PTP_CTRL_MCUSYS] = {
		.name = __stringify(PTP_CTRL_MCUSYS),
		.det_id = PTP_DET_MCUSYS,
	},
#ifdef CONFIG_MTK_INHOUSE_GPU
	[PTP_CTRL_GPUSYS] = {
		.name = __stringify(PTP_CTRL_GPUSYS),
		.det_id = PTP_DET_GPUSYS,
	},
#endif
};

/*
 * PTP detectors
 */
static void base_ops_enable(struct ptp_det *det, int reason);
static void base_ops_disable(struct ptp_det *det, int reason);
static void base_ops_disable_locked(struct ptp_det *det, int reason);
static void base_ops_switch_bank(struct ptp_det *det);

static int base_ops_init01(struct ptp_det *det);
static int base_ops_init02(struct ptp_det *det);
static int base_ops_mon_mode(struct ptp_det *det);

static int base_ops_get_status(struct ptp_det *det);
static void base_ops_dump_status(struct ptp_det *det);

static void base_ops_set_phase(struct ptp_det *det, enum ptp_phase phase);
static int base_ops_get_temp(struct ptp_det *det);
static int base_ops_get_volt(struct ptp_det *det);
static int base_ops_set_volt(struct ptp_det *det);
static void base_ops_restore_default_volt(struct ptp_det *det);
static void base_ops_get_freq_table(struct ptp_det *det);

static int get_volt_cpu(struct ptp_det *det);
static int set_volt_cpu(struct ptp_det *det);
static void restore_default_volt_cpu(struct ptp_det *det);
static void get_freq_table_cpu(struct ptp_det *det);

/* static void switch_to_vcore_ao(struct ptp_det *det); */
/* static void switch_to_vcore_pdn(struct ptp_det *det); */

#ifdef CONFIG_MTK_INHOUSE_GPU
static int get_volt_gpu(struct ptp_det *det);
static int set_volt_gpu(struct ptp_det *det);
static void restore_default_volt_gpu(struct ptp_det *det);
static void get_freq_table_gpu(struct ptp_det *det);
#endif

/* static int get_volt_vcore(struct ptp_det *det); */

#define BASE_OP(fn)	.fn = base_ops_ ## fn
static struct ptp_det_ops ptp_det_base_ops = {
	BASE_OP(enable),
	BASE_OP(disable),
	BASE_OP(disable_locked),
	BASE_OP(switch_bank),

	BASE_OP(init01),
	BASE_OP(init02),
	BASE_OP(mon_mode),

	BASE_OP(get_status),
	BASE_OP(dump_status),

	BASE_OP(set_phase),

	BASE_OP(get_temp),

	BASE_OP(get_volt),
	BASE_OP(set_volt),
	BASE_OP(restore_default_volt),
	BASE_OP(get_freq_table),
};


static struct ptp_det_ops mcu_det_ops = {
	.get_volt				= get_volt_cpu,
	.set_volt				= set_volt_cpu, /* <-@@@ */
	.restore_default_volt	= restore_default_volt_cpu,
	.get_freq_table			= get_freq_table_cpu,
};

#ifdef CONFIG_MTK_INHOUSE_GPU
static struct ptp_det_ops gpu_det_ops = {
	.get_volt				= get_volt_gpu,
	.set_volt				= set_volt_gpu, /* <-@@@ */
	.restore_default_volt	= restore_default_volt_gpu,
	.get_freq_table			= get_freq_table_gpu,
};
#endif

static struct ptp_det ptp_detectors[NR_PTP_DET] = {
	[PTP_DET_MCUSYS] = {
		.name		= __stringify(PTP_DET_MCUSYS),
		.ops		= &mcu_det_ops,
		.ctrl_id	= PTP_CTRL_MCUSYS,
		.features	= FEA_INIT01 | FEA_INIT02 | FEA_MON,
		.max_freq_khz	= 1300000, /* 1300Mhz */
		.VBOOT		= PTP_VOLT_TO_PMIC_VAL(115000), /* 1.0v: 0x30 */
	},
#ifdef CONFIG_MTK_INHOUSE_GPU
	[PTP_DET_GPUSYS] = {
		.name		= __stringify(PTP_DET_GPUSYS),
		.ops		= &gpu_det_ops,
		.ctrl_id	= PTP_CTRL_GPUSYS,
		.features	= FEA_INIT01 | FEA_INIT02 | FEA_MON, /* <-@@@ */
		.max_freq_khz	= 480000, /* 480Mhz */
		.VBOOT		= PTP_VOLT_TO_PMIC_VAL(115000), /* 1.0v: 0x30 */
		.volt_offset	= 0, /* <-@@@ */
/*		.disabled	= BY_PROCFS, */
	},
#endif
};
static struct ptp_devinfo ptp_devinfo;

static unsigned int ptp_level; /* debug info */

/**
 * timer for log
 */
static struct hrtimer ptp_log_timer;

/*=============================================================
 * Local function definition
 *=============================================================
 */

static struct ptp_det *id_to_ptp_det(enum ptp_det_id id)
{
	if (likely(id < NR_PTP_DET))
		return &ptp_detectors[id];
	else
		return NULL;
}

static struct ptp_ctrl *id_to_ptp_ctrl(enum ptp_ctrl_id id)
{
	if (likely(id < NR_PTP_CTRL))
		return &ptp_ctrls[id];
	else
		return NULL;
}

static void base_ops_enable(struct ptp_det *det, int reason)
{
	/* FIXME: UNDER CONSTRUCTION */
	FUNC_ENTER(FUNC_LV_HELP);
	det->disabled &= ~reason;
	FUNC_EXIT(FUNC_LV_HELP);
}

static void base_ops_switch_bank(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	/* APBSEL = 2:0 */
	ptp_write_field(PTP_PTPCORESEL, 2:0, det->ctrl_id);
	FUNC_EXIT(FUNC_LV_HELP);
}

static void base_ops_disable_locked(struct ptp_det *det, int reason)
{
	FUNC_ENTER(FUNC_LV_HELP);

	/* disable PTP */
	ptp_write(PTP_PTPEN, 0x0);

	/* Clear PTP interrupt PTPINTSTS */
	ptp_write(PTP_PTPINTSTS, 0x00ffffff);

	switch (reason) {
	case BY_MON_ERROR:
		/* set init2 value to DVFS table (PMIC) */
		memcpy(det->volt_tbl,
		       det->volt_tbl_init2, sizeof(det->volt_tbl_init2));
		ptp_set_ptp_volt(det);
		break;

	case BY_INIT_ERROR:
	case BY_PROCFS:
	default:
		/* restore default DVFS table (PMIC) */
		ptp_restore_ptp_volt(det);
		break;
	}

	pr_err("Disable PTP-OD[%s] done.reason=%d\n", det->name, reason);
	det->disabled |= reason;

	FUNC_EXIT(FUNC_LV_HELP);
}

static void base_ops_disable(struct ptp_det *det, int reason)
{
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	mt_ptp_lock(&flags);
	det->ops->switch_bank(det);
	det->ops->disable_locked(det, reason);
	mt_ptp_unlock(&flags);

	FUNC_EXIT(FUNC_LV_HELP);
}

static int base_ops_init01(struct ptp_det *det)
{
	/* struct ptp_ctrl *ctrl = id_to_ptp_ctrl(det->ctrl_id); */

	FUNC_ENTER(FUNC_LV_HELP);

	if (unlikely(!HAS_FEATURE(det, FEA_INIT01))) {
		pr_err("det %s has no INIT01\n", det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	if (det->disabled & BY_PROCFS) {
		pr_err("[%s] Disabled by PROCFS\n", __func__);
		FUNC_EXIT(FUNC_LV_HELP);
		return -2;
	}

	pr_debug("%s(%s) start (ptp_level = 0x%08X).\n",
		 __func__, det->name, ptp_level);
	/* atomic_inc(&ctrl->in_init); */
	/* ptp_init01_prepare(det); */
	/* det->ops->dump_status(det); */ /* <-@@@ */
	det->ops->set_phase(det, PTP_PHASE_INIT01);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static int base_ops_init02(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

	if (unlikely(!HAS_FEATURE(det, FEA_INIT02))) {
		pr_err("det %s has no INIT02\n", det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	if (det->disabled & BY_PROCFS) {
		pr_err("[%s] Disabled by PROCFS\n", __func__);
		FUNC_EXIT(FUNC_LV_HELP);
		return -2;
	}

	if (ptp_log_en) {
		pr_notice("%s(%s) start (ptp_level = 0x%08X).\n",
			  __func__, det->name, ptp_level);
		pr_notice("DCVOFFSETIN = 0x%08X\n", det->DCVOFFSETIN);
		pr_notice("AGEVOFFSETIN = 0x%08X\n", det->AGEVOFFSETIN);
	}

	/* det->ops->dump_status(det); */ /* <-@@@ */
	det->ops->set_phase(det, PTP_PHASE_INIT02);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static int base_ops_mon_mode(struct ptp_det *det)
{
	struct TS_PTPOD ts_info;
	enum thermal_bank_name ts_bank;

	FUNC_ENTER(FUNC_LV_HELP);

	if (!HAS_FEATURE(det, FEA_MON)) {
		pr_err("det %s has no MON mode\n", det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	if (det->disabled & BY_PROCFS) {
		pr_err("[%s] Disabled by PROCFS\n", __func__);
		FUNC_EXIT(FUNC_LV_HELP);
		return -2;
	}

	if (ptp_log_en)
		pr_notice("%s(%s) start (ptp_level = 0x%08X).\n",
			  __func__, det->name, ptp_level);

	if (det->ctrl_id == PTP_CTRL_MCUSYS) {
		ts_bank = THERMAL_BANK0;
#ifdef CONFIG_MTK_INHOUSE_GPU
	} else if (det->ctrl_id == PTP_CTRL_GPUSYS) {
		ts_bank = THERMAL_BANK1;
	} else {
#else
	} else {
#endif
		pr_err("undefine det->ctrl_id = %d\n", det->ctrl_id);
		WARN_ON(1);
	}

#ifdef CONFIG_THERMAL
	get_thermal_slope_intercept(&ts_info, ts_bank);
#else
	ts_info.ts_MTS = 0x100;
	ts_info.ts_BTS = 0x100;
#endif

	det->MTS = ts_info.ts_MTS;
	det->BTS = ts_info.ts_BTS;

	if (ptp_log_en)
		pr_debug("%s det->MTS = %d, det->BTS = %d\n",
			 __func__, det->MTS, det->BTS);

	if ((det->PTPINITEN == 0x0) || (det->PTPMONEN == 0x0)) {
		pr_err("PTPINITEN = 0x%08X, PTPMONEN = 0x%08X\n",
		       det->PTPINITEN, det->PTPMONEN);
		FUNC_EXIT(FUNC_LV_HELP);
		return 1;
	}

	/* det->ops->dump_status(det); */
	det->ops->set_phase(det, PTP_PHASE_MON);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static int base_ops_get_status(struct ptp_det *det)
{
	int status;
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	mt_ptp_lock(&flags);
	det->ops->switch_bank(det);
	status = (ptp_read(PTP_PTPEN) != 0) ? 1 : 0;
	mt_ptp_unlock(&flags);

	FUNC_EXIT(FUNC_LV_HELP);

	return status;
}

static void base_ops_dump_status(struct ptp_det *det)
{
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	pr_notice("[%s]\n",			det->name);

	pr_notice("PTPINITEN = 0x%08X\n",	det->PTPINITEN);
	pr_notice("PTPMONEN = 0x%08X\n",	det->PTPMONEN);
	pr_notice("MDES = 0x%08X\n",		det->MDES);
	pr_notice("BDES = 0x%08X\n",		det->BDES);
	pr_notice("DCMDET = 0x%08X\n",	det->DCMDET);

	pr_notice("DCCONFIG = 0x%08X\n",	det->DCCONFIG);
	pr_notice("DCBDET = 0x%08X\n",	det->DCBDET);

	pr_notice("AGECONFIG = 0x%08X\n",	det->AGECONFIG);
	pr_notice("AGEM = 0x%08X\n",		det->AGEM);

	pr_notice("AGEDELTA = 0x%08X\n",	det->AGEDELTA);
	pr_notice("DVTFIXED = 0x%08X\n",	det->DVTFIXED);
	pr_notice("MTDES = 0x%08X\n",	det->MTDES);
	pr_notice("VCO = 0x%08X\n",		det->VCO);

	pr_notice("DETWINDOW = 0x%08X\n",	det->DETWINDOW);
	pr_notice("VMAX = 0x%08X\n",		det->VMAX);
	pr_notice("VMIN = 0x%08X\n",		det->VMIN);
	pr_notice("DTHI = 0x%08X\n",		det->DTHI);
	pr_notice("DTLO = 0x%08X\n",		det->DTLO);
	pr_notice("VBOOT = 0x%08X\n",	det->VBOOT);
	pr_notice("DETMAX = 0x%08X\n",	det->DETMAX);

	pr_notice("DCVOFFSETIN = 0x%08X\n",	det->DCVOFFSETIN);
	pr_notice("AGEVOFFSETIN = 0x%08X\n",	det->AGEVOFFSETIN);

	pr_notice("MTS = 0x%08X\n",		det->MTS);
	pr_notice("BTS = 0x%08X\n",		det->BTS);

	pr_notice("num_freq_tbl = %d\n",	det->num_freq_tbl);

	for (i = 0; i < det->num_freq_tbl; i++)
		pr_info("freq_tbl[%d] = %d\n", i, det->freq_tbl[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		pr_info("volt_tbl[%d] = %d\n", i, det->volt_tbl[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		pr_info("volt_tbl_init2[%d] = %d\n",
			i, det->volt_tbl_init2[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		pr_info("volt_tbl_pmic[%d] = %d\n", i, det->volt_tbl_pmic[i]);

	FUNC_EXIT(FUNC_LV_HELP);
}

static void partial_reg_dump(struct ptp_det *det)
{
	pr_notice("[%s]\n", det->name);
	pr_notice("Source :\n");
	pr_notice("PTP_DESCHAR = 0x%x\n", ptp_read(PTP_DESCHAR));
	pr_notice("PTP_DCCONFIG = 0x%x\n", ptp_read(PTP_DCCONFIG));
	pr_notice("PTP_DETCHAR = 0x%x\n", ptp_read(PTP_DETCHAR));
	pr_notice("PTP_AGECONFIG = 0x%x\n",
						ptp_read(PTP_AGECONFIG));
	pr_notice("PTP_AGECHAR = 0x%x\n", ptp_read(PTP_AGECHAR));
	pr_notice("PTP_TEMPCHAR = 0x%x\n", ptp_read(PTP_TEMPCHAR));
	pr_notice("PTP_RUNCONFIG = 0x%x\n",
						ptp_read(PTP_RUNCONFIG));
	pr_notice("PTP_FREQPCT30 = 0x%x\n",
						ptp_read(PTP_FREQPCT30));
	pr_notice("PTP_FREQPCT74 = 0x%x\n",
						ptp_read(PTP_FREQPCT74));
	pr_notice("PTP_LIMITVALS = 0x%x\n",
						ptp_read(PTP_LIMITVALS));
	pr_notice("PTP_VBOOT = 0x%x\n", ptp_read(PTP_VBOOT));
	pr_notice("PTP_DETWINDOW = 0x%x\n",
						ptp_read(PTP_DETWINDOW));
	pr_notice("PTP_PTPCONFIG = 0x%x\n",
						ptp_read(PTP_PTPCONFIG));
	pr_notice("PTP_TSCALCS = 0x%x\n", ptp_read(PTP_TSCALCS));

	pr_notice("Output check :\n");
	pr_notice("PTP_PTPINTSTS = 0x%x\n",
						ptp_read(PTP_PTPINTSTS));
	pr_notice("PTP_DCVALUES = 0x%x\n", ptp_read(PTP_DCVALUES));
	pr_notice("PTP_AGECOUNT = 0x%x\n", ptp_read(PTP_AGECOUNT));
	pr_notice("PTP_INIT2VALS = 0x%x\n",
						ptp_read(PTP_INIT2VALS));
	pr_notice("PTP_VDESIGN30 = 0x%x\n",
						ptp_read(PTP_VDESIGN30));
	pr_notice("PTP_VDESIGN74 = 0x%x\n",
						ptp_read(PTP_VDESIGN74));
	pr_notice("PTP_TEMP = 0x%x\n", ptp_read(PTP_TEMP));
	pr_notice("PTP_DVT30 = 0x%x\n", ptp_read(PTP_DVT30));
	pr_notice("PTP_DVT74 = 0x%x\n", ptp_read(PTP_DVT74));
	pr_notice("PTP_VOP30 = 0x%x\n", ptp_read(PTP_VOP30));
	pr_notice("PTP_VOP74 = 0x%x\n", ptp_read(PTP_VOP74));
}

static void base_ops_set_phase(struct ptp_det *det, enum ptp_phase phase)
{
	unsigned int i, filter, val;
	/* unsigned long flags; */ /* <-XXX */

	FUNC_ENTER(FUNC_LV_HELP);

	/* mt_ptp_lock(&flags); */ /* <-XXX */

	det->ops->switch_bank(det);
	/* config PTP register */
	ptp_write(PTP_DESCHAR,
		  ((det->BDES << 8) & 0xff00) | (det->MDES & 0xff));
	ptp_write(PTP_TEMPCHAR,
		  (((det->VCO << 16) & 0xff0000) |
		   ((det->MTDES << 8) & 0xff00) | (det->DVTFIXED & 0xff)));
	ptp_write(PTP_DETCHAR,
		  ((det->DCBDET << 8) & 0xff00) | (det->DCMDET & 0xff));
	ptp_write(PTP_AGECHAR,
		  ((det->AGEDELTA << 8) & 0xff00) | (det->AGEM & 0xff));
	ptp_write(PTP_DCCONFIG, det->DCCONFIG);
	ptp_write(PTP_AGECONFIG, det->AGECONFIG);

	if (phase == PTP_PHASE_MON)
		ptp_write(PTP_TSCALCS,
			  ((det->BTS << 12) & 0xfff000) | (det->MTS & 0xfff));

	if (det->AGEM == 0x0)
		ptp_write(PTP_RUNCONFIG, 0x80000000);
	else {
		val = 0x0;

		for (i = 0; i < 24; i += 2) {
			filter = 0x3 << i;

			if (((det->AGECONFIG) & filter) == 0x0)
				val |= (0x1 << i);
			else
				val |= ((det->AGECONFIG) & filter);
		}

		ptp_write(PTP_RUNCONFIG, val);
	}

	ptp_write(PTP_FREQPCT30,
		  ((det->freq_tbl[3] << 24) & 0xff000000)	|
		  ((det->freq_tbl[2] << 16) & 0xff0000)	|
		  ((det->freq_tbl[1] << 8) & 0xff00)	|
		  (det->freq_tbl[0] & 0xff));
	ptp_write(PTP_FREQPCT74,
		  ((det->freq_tbl[7] << 24) & 0xff000000)	|
		  ((det->freq_tbl[6] << 16) & 0xff0000)	|
		  ((det->freq_tbl[5] << 8) & 0xff00)	|
		  ((det->freq_tbl[4]) & 0xff));
	ptp_write(PTP_LIMITVALS,
		  ((det->VMAX << 24) & 0xff000000)	|
		  ((det->VMIN << 16) & 0xff0000)		|
		  ((det->DTHI << 8) & 0xff00)		|
		  (det->DTLO & 0xff));
	ptp_write(PTP_VBOOT, (((det->VBOOT) & 0xff)));
	ptp_write(PTP_DETWINDOW, (((det->DETWINDOW) & 0xffff)));
	ptp_write(PTP_PTPCONFIG, (((det->DETMAX) & 0xffff)));

	/* clear all pending PTP interrupt & config PTPINTEN */
	ptp_write(PTP_PTPINTSTS, 0xffffffff);

	if (ptp_log_en) {
		pr_notice("Check registers setting before next interrupt\n");
		partial_reg_dump(det);
	}

	switch (phase) {
	case PTP_PHASE_INIT01:
		ptp_write(PTP_PTPINTEN, 0x00005f01);
		/* enable PTP INIT measurement */
		ptp_write(PTP_PTPEN, 0x00000001);
		break;

	case PTP_PHASE_INIT02:
		ptp_write(PTP_PTPINTEN, 0x00005f01);
		ptp_write(PTP_INIT2VALS,
			  ((det->AGEVOFFSETIN << 16) & 0xffff0000) |
			  (det->DCVOFFSETIN & 0xffff));
		/* enable PTP INIT measurement */
		ptp_write(PTP_PTPEN, 0x00000005);
		break;

	case PTP_PHASE_MON:
		ptp_write(PTP_PTPINTEN, 0x00FF0000);
		/* enable PTP monitor mode */
		ptp_write(PTP_PTPEN, 0x00000002);
		break;

	default:
		WARN_ON(1);
		break;
	}

	/* mt_ptp_unlock(&flags); */ /* <-XXX */

	FUNC_EXIT(FUNC_LV_HELP);
}

static int base_ops_get_temp(struct ptp_det *det)
{
	enum thermal_bank_name ts_bank;
	enum ptp_det_id det_id = det_to_id(det);
/* #if 1 TODO: FIXME Jerry */
	FUNC_ENTER(FUNC_LV_HELP);
/*
 *  THERMAL_BANK0     = 0,//CPU (TS_MCU1,TS_MCU2)		(TS3, TS4)
 *  THERMAL_BANK1     = 1,//GPU (TS_MCU3)			(TS5)
 *  THERMAL_BANK2     = 2,//SOC (TS_MCU4,TS_MCU2,TS_MCU3)	(TS1, TS4, TS5)
 */
	if (det_id == PTP_DET_MCUSYS) {
		ts_bank = THERMAL_BANK0;
#ifdef CONFIG_MTK_INHOUSE_GPU
	} else if (det_id == PTP_DET_GPUSYS) {
		ts_bank = THERMAL_BANK1;
#endif
	} else {
		ts_bank = THERMAL_BANK2;
	}

	FUNC_EXIT(FUNC_LV_HELP);
/* #endif */

#ifdef CONFIG_THERMAL
	return tscpu_get_temp_by_bank(ts_bank);
#else
	return 25000;
#endif
}

static int base_ops_get_volt(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	pr_debug("[%s] default func\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static int base_ops_set_volt(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	pr_debug("[%s] default func\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static void base_ops_restore_default_volt(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	pr_debug("[%s] default func\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);
}

static void base_ops_get_freq_table(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

	det->freq_tbl[0] = 100;
	det->num_freq_tbl = 1;

	FUNC_EXIT(FUNC_LV_HELP);
}

/* Will return 10uV */
static int get_volt_cpu(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);
	return mt_cpufreq_get_cur_volt(MT_CPU_DVFS_LITTLE); /* unit: 10uv */
}

/* volt_tbl_pmic is convert from 10uV */
static int set_volt_cpu(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);
	return mt_cpufreq_update_volt(MT_CPU_DVFS_LITTLE,
				      det->volt_tbl_pmic, det->num_freq_tbl);
}

static void restore_default_volt_cpu(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);


	mt_cpufreq_restore_default_volt(MT_CPU_DVFS_LITTLE);

	FUNC_EXIT(FUNC_LV_HELP);
}

static void get_freq_table_cpu(struct ptp_det *det)
{
	int i;
	enum mt_cpu_dvfs_id cpu;

	FUNC_ENTER(FUNC_LV_HELP);
	cpu = MT_CPU_DVFS_LITTLE;
#if 0
	if (cpu != 0)
		return; /* TODO: FIXME, just for E1 */
#endif
	for (i = 0; i < NR_FREQ; i++) {
		det->freq_tbl[i] = PERCENT(mt_cpufreq_get_freq_by_idx(cpu, i),
					   det->max_freq_khz);

		if (det->freq_tbl[i] == 0)
			break;
	}

	det->num_freq_tbl = i;

	FUNC_EXIT(FUNC_LV_HELP);
}

#ifdef CONFIG_MTK_INHOUSE_GPU
static int get_volt_gpu(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);

	/* printk("get_volt_gpu=%d\n",mt_gpufreq_get_cur_volt()); */
	return mt_gpufreq_get_cur_volt(); /* unit  mv * 100 = 10uv */
}
#endif

#ifdef CONFIG_MTK_INHOUSE_GPU
static int set_volt_gpu(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);

	return mt_gpufreq_update_volt(det->volt_tbl_pmic, det->num_freq_tbl);
}
#endif

#ifdef CONFIG_MTK_INHOUSE_GPU
static void restore_default_volt_gpu(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	mt_gpufreq_restore_default_volt();
	FUNC_EXIT(FUNC_LV_HELP);
}
#endif

#ifdef CONFIG_MTK_INHOUSE_GPU
static void get_freq_table_gpu(struct ptp_det *det)
{
	int i;

	FUNC_ENTER(FUNC_LV_HELP);

	for (i = 0; i < NR_FREQ; i++) {
		det->freq_tbl[i] = PERCENT(mt_gpufreq_get_freq_by_idx(i),
					   det->max_freq_khz);

		if (det->freq_tbl[i] == 0)
			break;
	}

	det->num_freq_tbl = i;
	FUNC_EXIT(FUNC_LV_HELP);
}
#endif

#if 0 /* could be reopend if need to be used */
static int get_volt_vcore(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);

	/* printk("get_volt_vcore=%d\n",mt_get_cur_volt_vcore_ao()); */
	return mt_get_cur_volt_vcore_ao(); /* unit = 10 uv */
}
#endif

/*=============================================================
 * Global function definition
 *=============================================================
 */
#if 0
unsigned int mt_ptp_get_level(void)
{
	unsigned int spd_bin_resv = 0, ret = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	spd_bin_resv = (get_devinfo_with_index(15) >> 28) & 0x7;

	switch (spd_bin_resv) {
	case 1:
		ret = 1; /* 2.0G */
		break;

	case 2:
		ret = 2; /* 1.3G */
		break;

	case 4:
		ret = 2; /* 1.3G */
		break;

	default:
		ret = 0; /* 1.7G */
		break;
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return ret;
}
#endif
#if 0	/* TODO: FIXME, remove it latter (unused) */
static unsigned int ptp_trasnfer_to_volt(unsigned int value)
{
	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);

	/* (700mv + n * 6.25mv) */
	return VAL_TO_MV(value);
}
#endif	/* TODO: FIXME, remove it latter (unused) */

#if 0
static DEFINE_SPINLOCK(ptp_spinlock);
void mt_ptp_lock(unsigned long *flags)
{
	/* FUNC_ENTER(FUNC_LV_HELP); */
	/* FIXME: lock with MD32 */
	/* get_md32_semaphore(SEMAPHORE_PTP); */
	spin_lock_irqsave(&ptp_spinlock, *flags);
	/* FUNC_EXIT(FUNC_LV_HELP); */
}
EXPORT_SYMBOL(mt_ptp_lock);

void mt_ptp_unlock(unsigned long *flags)
{
	/* FUNC_ENTER(FUNC_LV_HELP); */
	spin_unlock_irqrestore(&ptp_spinlock, *flags);
	/* FIXME: lock with MD32 */
	/* release_md32_semaphore(SEMAPHORE_PTP); */
	/* FUNC_EXIT(FUNC_LV_HELP); */
}
EXPORT_SYMBOL(mt_ptp_unlock);
#endif

#if 0
int mt_ptp_idle_can_enter(void)
{
	struct ptp_ctrl *ctrl;

	FUNC_ENTER(FUNC_LV_HELP);

	for_each_ctrl(ctrl) {
		if (atomic_read(&ctrl->in_init)) {
			FUNC_EXIT(FUNC_LV_HELP);
			return 0;
		}
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return 1;
}
EXPORT_SYMBOL(mt_ptp_idle_can_enter);
#endif
/*
 * timer for log
 */
static enum hrtimer_restart ptp_log_timer_func(struct hrtimer *timer)
{
	struct ptp_det *det;

	FUNC_ENTER(FUNC_LV_HELP);

	for_each_det(det) {
		pr_notice("PTP: [%s] (%d) - (%d, %d, %d, %d, %d, %d, %d, %d)",
			  det->name, det->ops->get_temp(det),
			  PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[0]),
			  PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[1]),
			  PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[2]),
			  PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[3]),
			  PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[4]),
			  PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[5]),
			  PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[6]),
			  PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[7]));

		pr_notice(" - (%d, %d, %d, %d, %d, %d, %d, %d)\n",
			  det->freq_tbl[0],
			  det->freq_tbl[1],
			  det->freq_tbl[2],
			  det->freq_tbl[3],
			  det->freq_tbl[4],
			  det->freq_tbl[5],
			  det->freq_tbl[6],
			  det->freq_tbl[7]);
	}

	hrtimer_forward_now(timer, ns_to_ktime(LOG_INTERVAL));
	FUNC_EXIT(FUNC_LV_HELP);

	return HRTIMER_RESTART;
}

/*
 * Thread for voltage setting
 */
static int ptp_volt_thread_handler(void *data)
{
	struct ptp_ctrl *ctrl = (struct ptp_ctrl *)data;
	struct ptp_det *det = id_to_ptp_det(ctrl->det_id);
#ifdef CONFIG_MTK_RAM_CONSOLE
	int ptp_status_temp = -1;
#endif

	FUNC_ENTER(FUNC_LV_HELP);

	do {
		wait_event_interruptible(ctrl->wq, ctrl->volt_update);
#ifdef CONFIG_MTK_RAM_CONSOLE
		/* update set volt status for this bank */
		switch (det->ctrl_id) {
		case PTP_CTRL_MCUSYS:
			aee_rr_rec_ptp_status(aee_rr_curr_ptp_status() |
				(1 << PTP_CPU_LITTLE_IS_SET_VOLT));
			ptp_status_temp = PTP_CPU_LITTLE_IS_SET_VOLT;
			break;
		default:
			pr_err("%s: %u, wrong ctrl_id = %d\n",
				__func__, __LINE__, det->ctrl_id);
			break;
		}
#endif
		if ((ctrl->volt_update & PTP_VOLT_UPDATE) &&
		    det->ops->set_volt)
			det->ops->set_volt(det);

		if ((ctrl->volt_update & PTP_VOLT_RESTORE) &&
		    det->ops->restore_default_volt)
			det->ops->restore_default_volt(det);

		ctrl->volt_update = PTP_VOLT_NONE;

#ifdef CONFIG_MTK_RAM_CONSOLE
		/* clear out set volt status for this bank */
		if (ptp_status_temp >= 0)
			aee_rr_rec_ptp_status(aee_rr_curr_ptp_status() &
				~(1 << ptp_status_temp));
#endif

	} while (!kthread_should_stop());

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static void inherit_base_det(struct ptp_det *det)
{
	/*
	 * Inherit ops from ptp_det_base_ops if ops in det is NULL
	 */
	FUNC_ENTER(FUNC_LV_HELP);

#define INIT_OP(ops, func)					\
		do {							\
			if (ops->func == NULL)				\
				ops->func = ptp_det_base_ops.func;	\
		} while (0)

	INIT_OP(det->ops, disable);
	INIT_OP(det->ops, disable_locked);
	INIT_OP(det->ops, switch_bank);
	INIT_OP(det->ops, init01);
	INIT_OP(det->ops, init02);
	INIT_OP(det->ops, mon_mode);
	INIT_OP(det->ops, get_status);
	INIT_OP(det->ops, dump_status);
	INIT_OP(det->ops, set_phase);
	INIT_OP(det->ops, get_temp);
	INIT_OP(det->ops, get_volt);
	INIT_OP(det->ops, set_volt);
	INIT_OP(det->ops, restore_default_volt);
	INIT_OP(det->ops, get_freq_table);

	FUNC_EXIT(FUNC_LV_HELP);
}

static void ptp_init_ctrl(struct ptp_ctrl *ctrl)
{
	FUNC_ENTER(FUNC_LV_HELP);

	/* init_completion(&ctrl->init_done); */
	/* atomic_set(&ctrl->in_init, 0); */

	init_waitqueue_head(&ctrl->wq);
	ctrl->thread = kthread_run(ptp_volt_thread_handler, ctrl, ctrl->name);
	if (IS_ERR(ctrl->thread))
		pr_err("Create %s thread failed: %ld\n",
		       ctrl->name, PTR_ERR(ctrl->thread));

	FUNC_EXIT(FUNC_LV_HELP);
}

static void ptp_init_det(struct ptp_det *det, struct ptp_devinfo *devinfo)
{
	enum ptp_det_id det_id = det_to_id(det);

	FUNC_ENTER(FUNC_LV_HELP);
	pr_debug("det name=%s, det_id=%d\n", det->name, det_id);

	inherit_base_det(det);

	/* init with devinfo */
	det->PTPINITEN	= devinfo->PTPINITEN;
	det->PTPMONEN	= devinfo->PTPMONEN;

	/* init with constant */
	det->DETWINDOW	= DETWINDOW_VAL;
	det->VMAX		= VMAX_VAL;
	det->VMIN		= VMIN_VAL;
	det->VBOOT		= PTP_VOLT_TO_PMIC_VAL(115000);

	det->DTHI		= DTHI_VAL;
	det->DTLO		= DTLO_VAL;
	det->DETMAX		= DETMAX_VAL;

	det->AGECONFIG	= AGECONFIG_VAL;
	det->AGEM		= AGEM_VAL;
	det->DVTFIXED	= DVTFIXED_VAL;
	det->VCO		= VCO_VAL;
	det->DCCONFIG	= DCCONFIG_VAL;

	if (det->ops->get_volt != NULL) {
		det->VBOOT = PTP_VOLT_TO_PMIC_VAL(det->ops->get_volt(det));
		pr_debug("@%s(), det->VBOOT = %d\n", __func__, det->VBOOT);
	}

	switch (det_id) {
	case PTP_DET_MCUSYS:
		det->MDES	= devinfo->CPU_MDES;
		det->BDES	= devinfo->CPU_BDES;
		det->DCMDET	= devinfo->CPU_DCMDET;
		det->DCBDET	= devinfo->CPU_DCBDET;
		/* Vsram and Vproc use the same buck. Besides,
		 * Vmin of Vsram is 115000 * 10uV.
		 * So, I need to override default setting
		 */
		det->VMIN	= PTP_VOLT_TO_PMIC_VAL(115000);
		det->VMAX	= PTP_VOLT_TO_PMIC_VAL(130000);
		break;

#ifdef PTP_DET_GPUSYS
	case PTP_DET_GPUSYS:
		det->MDES	= devinfo->GPU_MDES;
		det->BDES	= devinfo->GPU_BDES;
		det->DCMDET	= devinfo->GPU_DCMDET;
		det->DCBDET	= devinfo->GPU_DCBDET;
		det->VMIN	= VMIN_VAL_GPU; /* override default setting */
		det->DVTFIXED = 0x4;
		break;
#endif

	default:
		pr_err("[%s]: Unknown det_id %d\n", __func__, det_id);
		break;
	}

	switch (det->ctrl_id) {
	case PTP_CTRL_MCUSYS:
		det->AGEDELTA	= devinfo->CPU_AGEDELTA;
		det->MTDES		= devinfo->CPU_MTDES;
		break;

#ifdef CONFIG_MTK_INHOUSE_GPU
	case PTP_CTRL_GPUSYS:
		det->AGEDELTA	= devinfo->GPU_AGEDELTA;
		det->MTDES		= devinfo->GPU_MTDES;
		break;
#endif

	default:
		pr_err("[%s]: Unknown ctrl_id %d\n", __func__, det->ctrl_id);
		break;
	}

	/* get DVFS frequency table */
	det->ops->get_freq_table(det);

	FUNC_EXIT(FUNC_LV_HELP);
}


static void ptp_set_ptp_volt(struct ptp_det *det)
{
#if SET_PMIC_VOLT
	int dvfs_index, cur_temp, low_temp_offset;
	struct ptp_ctrl *ctrl = id_to_ptp_ctrl(det->ctrl_id);
#ifdef CONFIG_MTK_RAM_CONSOLE
	u64 temp_long;
	u64 temp_cur = (u64)aee_rr_curr_ptp_temp();
#endif

	cur_temp = det->ops->get_temp(det);

	if (ptp_log_en)
		pr_notice("ptp_set_ptp_volt cur_temp = %d\n", cur_temp);

	if (cur_temp <= 33000)
		low_temp_offset = 10;
	else
		low_temp_offset = 0;

	/* all scale of volt_tbl_pmic,volt_tbl,volt_offset are pmic value */
	/* scale of det->volt_offset must equal 10uV */
	for (dvfs_index = 0; dvfs_index < det->num_freq_tbl; dvfs_index++) {
		det->volt_tbl_pmic[dvfs_index] =
			clamp(det->volt_tbl[dvfs_index] + det->volt_offset +
			      low_temp_offset, det->VMIN, det->VMAX);
	}
#ifdef CONFIG_MTK_RAM_CONSOLE
	switch (det->ctrl_id) {
	case PTP_CTRL_MCUSYS:
		/* update AEE temperature */
		temp_long = (u64) (cur_temp / 1000);
		if (temp_long != 0) {
			aee_rr_rec_ptp_temp(temp_long <<
				(8 * PTP_CPU_LITTLE_IS_SET_VOLT) |
				(temp_cur & ~((u64)0xFF <<
				(8 * PTP_CPU_LITTLE_IS_SET_VOLT))));
		}

		/* update AEE voltage */
		for (dvfs_index = 0; dvfs_index < NR_FREQ; dvfs_index++) {
			aee_rr_rec_ptp_cpu_little_volt(
			((u64)(det->volt_tbl_pmic[dvfs_index]) <<
			(8 * dvfs_index)) | (aee_rr_curr_ptp_cpu_little_volt() &
			~((u64)(0xFF) << (8 * dvfs_index))));
		}

		break;
	default:
		pr_err("%s: %u, wrong ctrl_id = %d\n",
		       __func__, __LINE__, det->ctrl_id);
		break;
	}
#endif
	ctrl->volt_update |= PTP_VOLT_UPDATE;
	wake_up_interruptible(&ctrl->wq);
#endif

	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);
}

static void ptp_restore_ptp_volt(struct ptp_det *det)
{
#if SET_PMIC_VOLT
	struct ptp_ctrl *ctrl = id_to_ptp_ctrl(det->ctrl_id);

	ctrl->volt_update |= PTP_VOLT_RESTORE;
	wake_up_interruptible(&ctrl->wq);
#endif

	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);
}

static inline void handle_init01_isr(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_LOCAL);

	det->dcvalues[PTP_PHASE_INIT01]		= ptp_read(PTP_DCVALUES);
	det->ptp_freqpct30[PTP_PHASE_INIT01]	= ptp_read(PTP_FREQPCT30);
	det->ptp_26c[PTP_PHASE_INIT01]		= ptp_read(PTP_PTPINTEN + 0x10);
	det->ptp_vop30[PTP_PHASE_INIT01]	= ptp_read(PTP_VOP30);
	det->ptp_ptpen[PTP_PHASE_INIT01]	= ptp_read(PTP_PTPEN);

	if (ptp_log_en) {
		pr_debug("@ %s(%s)\n", __func__, det->name);
		pr_notice("Dump registers\n");
		partial_reg_dump(det);
	}

#if DUMP_DATA_TO_DE
	{
		int i;

		for (i = 0; i < ARRAY_SIZE(reg_dump_addr_off); i++)
			det->reg_dump_data[i][PTP_PHASE_INIT01] =
				ptp_read(PTP_BASEADDR + reg_dump_addr_off[i]);
	}
#endif

	pr_notice("%s: %s: VDN74:0x%08x, VDN30:0x%08x, DCVALUES:0x%08x\n",
		  det->name, __func__, ptp_read(PTP_VDESIGN74),
		  ptp_read(PTP_VDESIGN30), ptp_read(PTP_DCVALUES));

	/*
	 * Read & store 16 bit values DCVALUES.DCVOFFSET and
	 * AGEVALUES.AGEVOFFSET for later use in INIT2 procedure
	 */
	det->DCVOFFSETIN = ~(ptp_read(PTP_DCVALUES) & 0xffff) + 1;
	det->AGEVOFFSETIN = ptp_read(PTP_AGEVALUES) & 0xffff;

	/* always dump the Vproc DC offset here, that is the gap data we want to track & statistic */
	pr_err("[PTP] INIT2VALS %d, %d, VDESIGN0 %d\n",
		  det->DCVOFFSETIN & 0xffff, det->AGEVOFFSETIN,
		  ptp_read(PTP_VDESIGN30) & 0xff);

	/*
	 * Set PTPEN.PTPINITEN/PTPEN.PTPINIT2EN = 0x0 &
	 * Clear PTP INIT interrupt PTPINTSTS = 0x00000001
	 */
	ptp_write(PTP_PTPEN, 0x0);
	ptp_write(PTP_PTPINTSTS, 0x1);
	/* ptp_init01_finish(det); */
	det->ops->init02(det);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_init02_isr(struct ptp_det *det)
{
	unsigned int temp;
	int i;
	/* struct ptp_ctrl *ctrl = id_to_ptp_ctrl(det->ctrl_id); */

	FUNC_ENTER(FUNC_LV_LOCAL);

	if (ptp_log_en) {
		pr_notice("@ %s(%s)\n", __func__, det->name);
		pr_notice("Dump registers\n");
		partial_reg_dump(det);
	}

	det->dcvalues[PTP_PHASE_INIT02]		= ptp_read(PTP_DCVALUES);
	det->ptp_freqpct30[PTP_PHASE_INIT02]	= ptp_read(PTP_FREQPCT30);
	det->ptp_26c[PTP_PHASE_INIT02]		= ptp_read(PTP_PTPINTEN + 0x10);
	det->ptp_vop30[PTP_PHASE_INIT02]	= ptp_read(PTP_VOP30);
	det->ptp_ptpen[PTP_PHASE_INIT02]	= ptp_read(PTP_PTPEN);
#if DUMP_DATA_TO_DE
	{
		int i;

		for (i = 0; i < ARRAY_SIZE(reg_dump_addr_off); i++)
			det->reg_dump_data[i][PTP_PHASE_INIT02] =
				ptp_read(PTP_BASEADDR + reg_dump_addr_off[i]);
	}
#endif

	pr_notice("%s: %s: VOP74:0x%08x, VOP30:0x%08x, DCVALUES:0x%08x\n",
		   det->name, __func__, ptp_read(PTP_VOP74),
		   ptp_read(PTP_VOP30), ptp_read(PTP_DCVALUES));

	temp = ptp_read(PTP_VOP30);
	/* PTP_VOP30=>pmic value */
	det->volt_tbl[0] =  (temp & 0xff) + PTPOD_PMIC_OFFSET;
	det->volt_tbl[1] = ((temp >> 8)  & 0xff) + PTPOD_PMIC_OFFSET;
	det->volt_tbl[2] = ((temp >> 16) & 0xff) + PTPOD_PMIC_OFFSET;
	det->volt_tbl[3] = ((temp >> 24) & 0xff) + PTPOD_PMIC_OFFSET;

	temp = ptp_read(PTP_VOP74);
	/* PTP_VOP74=>pmic value */
	det->volt_tbl[4] =  (temp & 0xff) + PTPOD_PMIC_OFFSET;
	det->volt_tbl[5] = ((temp >> 8)  & 0xff) + PTPOD_PMIC_OFFSET;
	det->volt_tbl[6] = ((temp >> 16) & 0xff) + PTPOD_PMIC_OFFSET;
	det->volt_tbl[7] = ((temp >> 24) & 0xff) + PTPOD_PMIC_OFFSET;

	/* backup to volt_tbl_init2 */
	memcpy(det->volt_tbl_init2, det->volt_tbl, sizeof(det->volt_tbl_init2));

	if (ptp_log_en) {
		for (i = 0; i < NR_FREQ; i++)
			pr_notice("init02, [%s].volt_tbl[%d] = 0x%08X (%d)\n",
				  det->name, i, det->volt_tbl[i],
				  PTP_PMIC_VAL_TO_VOLT(det->volt_tbl[i]));

		pr_notice("ptp_level = 0x%08X\n", ptp_level);
	}

	ptp_set_ptp_volt(det);

	/*
	 * Set PTPEN.PTPINITEN/PTPEN.PTPINIT2EN = 0x0 &
	 * Clear PTP INIT interrupt PTPINTSTS = 0x00000001
	 */
	ptp_write(PTP_PTPEN, 0x0);
	ptp_write(PTP_PTPINTSTS, 0x1);

	/* atomic_dec(&ctrl->in_init); */
	/* complete(&ctrl->init_done); */
	det->ops->mon_mode(det);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_init_err_isr(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_LOCAL);

	pr_err("====================================================\n");
	pr_err("PTP init err: PTPEN(%p) = 0x%08X, PTPINTSTS(%p) = 0x%08X\n",
		     PTP_PTPEN, ptp_read(PTP_PTPEN),
		     PTP_PTPINTSTS, ptp_read(PTP_PTPINTSTS));
	pr_err("PTP_SMSTATE0 (%p) = 0x%08X\n",
		     PTP_SMSTATE0, ptp_read(PTP_SMSTATE0));
	pr_err("PTP_SMSTATE1 (%p) = 0x%08X\n",
		     PTP_SMSTATE1, ptp_read(PTP_SMSTATE1));
	pr_err("====================================================\n");

	/* TODO: FIXME */
	{
		/* struct ptp_ctrl *ctrl = id_to_ptp_ctrl(det->ctrl_id); */
		/* atomic_dec(&ctrl->in_init); */
		/* complete(&ctrl->init_done); */
	}
	/* TODO: FIXME */

	aee_kernel_warning("mt_ptp", "@%s():%d, get_volt(%s) = 0x%08X\n",
			   __func__, __LINE__, det->name, det->VBOOT);
	det->ops->disable_locked(det, BY_INIT_ERROR);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_mon_mode_isr(struct ptp_det *det)
{
	unsigned int temp;
	int i;

	FUNC_ENTER(FUNC_LV_LOCAL);


	if (ptp_log_en) {
		pr_notice("@ %s(%s)\n", __func__, det->name);
		pr_notice("Dump registers\n");
		partial_reg_dump(det);
	}

	det->dcvalues[PTP_PHASE_MON]		= ptp_read(PTP_DCVALUES);
	det->ptp_freqpct30[PTP_PHASE_MON]	= ptp_read(PTP_FREQPCT30);
	det->ptp_26c[PTP_PHASE_MON]		= ptp_read(PTP_PTPINTEN + 0x10);
	det->ptp_vop30[PTP_PHASE_MON]		= ptp_read(PTP_VOP30);
	det->ptp_ptpen[PTP_PHASE_MON]		= ptp_read(PTP_PTPEN);
#if DUMP_DATA_TO_DE
	{
		int i;

		for (i = 0; i < ARRAY_SIZE(reg_dump_addr_off); i++)
			det->reg_dump_data[i][PTP_PHASE_MON] =
				ptp_read(PTP_BASEADDR + reg_dump_addr_off[i]);
	}
#endif
	/* check if thermal sensor init completed? */
	temp = (ptp_read(PTP_TEMP) & 0xff);

	if ((temp > 0x4b) && (temp < 0xd3)) {
		pr_err("thermal sensor init not completed\n");
		pr_err("ptp_read(PTP_TEMP): 0x%08X\n", temp);
		goto out;
	}

	temp = ptp_read(PTP_VOP30);
	/* PTP_VOP30=>pmic value */
	det->volt_tbl[0] =  (temp & 0xff) + PTPOD_PMIC_OFFSET;
	det->volt_tbl[1] = ((temp >> 8)  & 0xff) + PTPOD_PMIC_OFFSET;
	det->volt_tbl[2] = ((temp >> 16) & 0xff) + PTPOD_PMIC_OFFSET;
	det->volt_tbl[3] = ((temp >> 24) & 0xff) + PTPOD_PMIC_OFFSET;

	temp = ptp_read(PTP_VOP74);
	/* PTP_VOP74=>pmic value */
	det->volt_tbl[4] = (temp & 0xff) + PTPOD_PMIC_OFFSET;
	det->volt_tbl[5] = ((temp >> 8)  & 0xff) + PTPOD_PMIC_OFFSET;
	det->volt_tbl[6] = ((temp >> 16) & 0xff) + PTPOD_PMIC_OFFSET;
	det->volt_tbl[7] = ((temp >> 24) & 0xff) + PTPOD_PMIC_OFFSET;

	if (ptp_log_en) {
		for (i = 0; i < NR_FREQ; i++)
			pr_notice("mon, [%s]: volt_tbl[%d] = 0x%08X (%d)\n",
				  det->name, i, det->volt_tbl[i],
				  PTP_PMIC_VAL_TO_VOLT(det->volt_tbl[i]));

		pr_notice("ptp_level = 0x%08X\n", ptp_level);
	}

#ifdef CONFIG_THERMAL
	if (ptp_log_en)
		pr_notice("ISR : TEMPSPARE1 = 0x%08X\n", ptp_read(TEMPSPARE1));
#endif

	ptp_set_ptp_volt(det);

out:
	/* Clear PTP INIT interrupt PTPINTSTS = 0x00ff0000 */
	ptp_write(PTP_PTPINTSTS, 0x00ff0000);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_mon_err_isr(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_LOCAL);

	/* PTP Monitor mode error handler */
	pr_err("====================================================\n");
	pr_err("PTP mon err: PTPEN(%p) = 0x%08X, PTPINTSTS(%p) = 0x%08X\n",
		     PTP_PTPEN, ptp_read(PTP_PTPEN),
		     PTP_PTPINTSTS, ptp_read(PTP_PTPINTSTS));
	pr_err("PTP_SMSTATE0 (%p) = 0x%08X\n",
		     PTP_SMSTATE0, ptp_read(PTP_SMSTATE0));
	pr_err("PTP_SMSTATE1 (%p) = 0x%08X\n",
		     PTP_SMSTATE1, ptp_read(PTP_SMSTATE1));
	pr_err("PTP_TEMP (%p) = 0x%08X\n",
		     PTP_TEMP, ptp_read(PTP_TEMP));
	pr_err("PTP_TEMPMSR0 (%p) = 0x%08X\n",
		     PTP_TEMPMSR0, ptp_read(PTP_TEMPMSR0));
	pr_err("PTP_TEMPMSR1 (%p) = 0x%08X\n",
		     PTP_TEMPMSR1, ptp_read(PTP_TEMPMSR1));
	pr_err("PTP_TEMPMSR2 (%p) = 0x%08X\n",
		     PTP_TEMPMSR2, ptp_read(PTP_TEMPMSR2));
	pr_err("PTP_TEMPMONCTL0 (%p) = 0x%08X\n",
		     PTP_TEMPMONCTL0, ptp_read(PTP_TEMPMONCTL0));
	pr_err("PTP_TEMPMSRCTL1 (%p) = 0x%08X\n",
		     PTP_TEMPMSRCTL1, ptp_read(PTP_TEMPMSRCTL1));
	pr_err("====================================================\n");

	aee_kernel_warning("mt_ptp", "@%s():%d, get_volt(%s) = 0x%08X\n",
			   __func__, __LINE__, det->name, det->VBOOT);
	det->ops->disable_locked(det, BY_MON_ERROR);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void ptp_isr_handler(struct ptp_det *det)
{
	unsigned int PTPINTSTS, PTPEN;

	FUNC_ENTER(FUNC_LV_LOCAL);

	PTPINTSTS = ptp_read(PTP_PTPINTSTS);
	PTPEN = ptp_read(PTP_PTPEN);

	if (ptp_log_en) {
		pr_notice("func = %s\n", __func__);
		pr_notice("[%s]\n", det->name);
		pr_notice("PTP_PTPEN = 0x%08X\n", ptp_read(PTP_PTPEN));
		pr_notice("PTP_PTPODINTST = 0x%X\n", ptp_read(PTP_PTPODINTST));
		pr_notice("PTP_PTPCORESEL = 0x%X\n", ptp_read(PTP_PTPCORESEL));
		pr_notice("PTP_DCVALUES = 0x%08X\n", ptp_read(PTP_DCVALUES));
		pr_notice("PTP_AGECOUNT = 0x%08X\n", ptp_read(PTP_AGECOUNT));
	}

	if (PTPINTSTS == 0x1) { /* PTP init1 or init2 */
		if ((PTPEN & 0x7) == 0x1)   /* PTP init1 */
			handle_init01_isr(det);
		else if ((PTPEN & 0x7) == 0x5)   /* PTP init2 */
			handle_init02_isr(det);
		else {
			/*
			 * error : init1 or init2,
			 * but enable setting is wrong.
			 */
			handle_init_err_isr(det);
		}
	} else if ((PTPINTSTS & 0x00ff0000) != 0x0)
		handle_mon_mode_isr(det);
	else { /* PTP error handler */
		/* init 1  || init 2 error handler */
		if (((PTPEN & 0x7) == 0x1) || ((PTPEN & 0x7) == 0x5))
			handle_init_err_isr(det);
		else /* PTP Monitor mode error handler */
			handle_mon_err_isr(det);
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static irqreturn_t ptp_isr(int irq, void *dev_id)
{
	unsigned long flags;
	struct ptp_det *det = NULL;
	int i;

	FUNC_ENTER(FUNC_LV_MODULE);

	if (ptp_log_en) {
		pr_notice("%s start.\n", __func__);
		pr_notice("PTP_PTPEN = 0x%08X\n", ptp_read(PTP_PTPEN));
		pr_notice("PTP_PTPODINTST = 0x%X\n", ptp_read(PTP_PTPODINTST));
		pr_notice("PTP_PTPCORESEL = 0x%X\n", ptp_read(PTP_PTPCORESEL));
	}

	mt_ptp_lock(&flags);
#if 0
	if (!(BIT(PTP_CTRL_VCORE) & ptp_read(PTP_PTPODINTST))) {
		switch (ptp_read_field(PERI_VCORE_PTPOD_CON0, VCORE_PTPODSEL)) {
		case SEL_VCORE_AO:
			det = &ptp_detectors[PTP_DET_VCORE_AO];
			break;

		case SEL_VCORE_PDN:
			det = &ptp_detectors[PTP_DET_VCORE_AO];
			break;
		}

		if (likely(det)) {
			det->ops->switch_bank(det);
			ptp_isr_handler(det);
		}
	}
#endif
	for (i = 0; i < NR_PTP_CTRL; i++) {
		/* PTP_BANK interrupt check */
		if ((BIT(i) & ptp_read(PTP_PTPODINTST)))
			continue;

		det = &ptp_detectors[i];

		det->ops->switch_bank(det);

		ptp_isr_handler(det);
	}

	mt_ptp_unlock(&flags);

	FUNC_EXIT(FUNC_LV_MODULE);

	if (ptp_log_en)
		pr_notice("%s done.\n", __func__);

	return IRQ_HANDLED;
}

/* static atomic_t ptp_init01_cnt; */

#if 0 /* could be reopened if need to use it */
static void ptp_init01_prepare(struct ptp_det *det)
{
	FUNC_ENTER(FUNC_LV_LOCAL);

	atomic_inc(&ptp_init01_cnt);

	if (atomic_read(&ptp_init01_cnt) == 1) {
#if 0
		enum mt_cpu_dvfs_id cpu;

		switch (det_to_id(det)) {
		case PTP_DET_LITTLE:
			cpu = MT_CPU_DVFS_LITTLE;
			break;

		case PTP_DET_BIG:
			cpu = MT_CPU_DVFS_BIG;
			break;

		default:
			return;
		}
#endif
#if 0	/* TODO: move to ptp_init01() */

		if (cpu == 0) { /* TODO: FIXME, for E1 */
			/* disable frequency hopping (main PLL) */
			mt_fh_popod_save();
			/* disable DVFS and set vproc = 1.15v (1 GHz) */
			mt_cpufreq_disable_by_ptpod(cpu);
		}

#endif	/* TODO: move to ptp_init01() */
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}
#endif
#if 0 /* could be reopend if no used */
static void ptp_init01_finish(struct ptp_det *det)
{
	atomic_dec(&ptp_init01_cnt);

	FUNC_ENTER(FUNC_LV_LOCAL);

	if (atomic_read(&ptp_init01_cnt) < 0)
		WARN();

	if (atomic_read(&ptp_init01_cnt) == 0) {
#if 0
		enum mt_cpu_dvfs_id cpu;

		switch (det_to_id(det)) {
		case PTP_DET_LITTLE:
			cpu = MT_CPU_DVFS_LITTLE;
			break;

		case PTP_DET_BIG:
			cpu = MT_CPU_DVFS_BIG;
			break;

		default:
			return;
		}
#endif
#if 0	/* TODO: move to ptp_init01() */

		if (cpu == 0) { /* TODO: FIXME, for E1 */
			/* enable DVFS */
			mt_cpufreq_enable_by_ptpod(cpu);
			/* enable frequency hopping (main PLL) */
			mt_fh_popod_restore();
		}

#endif	/* TODO: move to ptp_init01() */
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}
#endif

void ptp_init01(void)
{
	struct ptp_det *det;
	struct ptp_ctrl *ctrl;
	unsigned long flag;
	unsigned int vboot;

	FUNC_ENTER(FUNC_LV_LOCAL);

	for_each_det_ctrl(det, ctrl) {
		vboot = PTP_VOLT_TO_PMIC_VAL(det->ops->get_volt(det));

#ifdef CONFIG_MTK_RAM_CONSOLE
		/* record vboot of this bank */
		aee_rr_rec_ptp_vboot(
			((u64)(vboot) << (8 * det->ctrl_id)) |
			(aee_rr_curr_ptp_vboot() & ~((u64)(0xFF) <<
			(8 * det->ctrl_id))));
#endif
		pr_debug("%s(),get_volt(%s), vboot = %d\n", __func__,
			 det->name, vboot);
		if (vboot != det->VBOOT) {
			pr_err("%s(): get_volt = 0x%08X, VBOOT = 0x%08X\n",
			       det->name, vboot, det->VBOOT);
			aee_kernel_warning(
				"mt_ptp",
				"%s(): get_volt = 0x%08X, VBOOT = 0x%08X\n",
				det->name, vboot, det->VBOOT);
		}
		mt_ptp_lock(&flag); /* <-XXX */
		det->ops->init01(det);
		mt_ptp_unlock(&flag); /* <-XXX */

		/*
		 * VCORE_AO and VCORE_PDN use the same controller.
		 * Wait until VCORE_AO init01 and init02 done
		 */
#if 0
		if (atomic_read(&ctrl->in_init))
			wait_for_completion(&ctrl->init_done);
#endif
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

void ptp_init02(void)
{
	struct ptp_det *det;
	struct ptp_ctrl *ctrl;

	FUNC_ENTER(FUNC_LV_LOCAL);

	for_each_det_ctrl(det, ctrl) {
		if (HAS_FEATURE(det, FEA_MON)) {
			unsigned long flag;

			mt_ptp_lock(&flag);
			det->ops->init02(det);
			mt_ptp_unlock(&flag);
		}
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

#if EN_PTP_OD
#if 0
static char *readline(struct file *fp)
{
#define BUFSIZE 1024
	static char buf[BUFSIZE]; /* TODO: FIXME, dynamic alloc */
	static int buf_end;
	static int line_start;
	static int line_end;
	char *ret;

	buf_end = 0;
	line_start = 0;
	line_end = 0;

	FUNC_ENTER(FUNC_LV_HELP);
empty:

	if (line_start >= buf_end) {
		line_start = 0;
		buf_end = fp->f_op->read(fp,
					 &buf[line_end],
					 sizeof(buf) - line_end, &fp->f_pos);

		if (buf_end == 0) {
			line_end = 0;
			return NULL;
		}

		buf_end += line_end;
	}

	while (buf[line_end] != '\n') {
		line_end++;

		if (line_end >= buf_end) {
			memcpy(&buf[0], &buf[line_start], buf_end - line_start);
			line_end = buf_end - line_start;
			line_start = buf_end;
			goto empty;
		}
	}

	buf[line_end] = '\0';
	ret = &buf[line_start];
	line_start = line_end + 1;

	FUNC_EXIT(FUNC_LV_HELP);

	return ret;
}
#endif
/* leakage */
unsigned int leakage_core;
unsigned int leakage_gpu;
unsigned int leakage_sram2;
unsigned int leakage_sram1;


void get_devinfo(struct ptp_devinfo *p)
{
	int *M_HW_RES = (int *)p;

	FUNC_ENTER(FUNC_LV_HELP);

#if 1
	M_HW_RES[0] = get_devinfo_with_index(15);
	M_HW_RES[1] = get_devinfo_with_index(16);
	M_HW_RES[2] = get_devinfo_with_index(17);
	M_HW_RES[3] = get_devinfo_with_index(18);
	M_HW_RES[4] = get_devinfo_with_index(19);
	M_HW_RES[5] = get_devinfo_with_index(21);
#else
	M_HW_RES[0] = 0x00000003;
	M_HW_RES[1] = 0x00000000;
	M_HW_RES[2] = 0x12A74033;
	M_HW_RES[3] = 0x00370000;
	M_HW_RES[4] = 0x00000000;
	M_HW_RES[5] = 0x76BB34D0;
#endif

#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_ptp_e0((u32)M_HW_RES[0]);
	aee_rr_rec_ptp_e1((u32)M_HW_RES[1]);
	aee_rr_rec_ptp_e2((u32)M_HW_RES[2]);
	aee_rr_rec_ptp_e3((u32)M_HW_RES[3]);
	aee_rr_rec_ptp_e4((u32)M_HW_RES[4]);
	aee_rr_rec_ptp_e5((u32)M_HW_RES[5]);
#endif
	pr_notice("M_HW_RES[0]=0x%x\n", M_HW_RES[0]);
	pr_notice("M_HW_RES[1]=0x%x\n", M_HW_RES[1]);
	pr_notice("M_HW_RES[2]=0x%x\n", M_HW_RES[2]);
	pr_notice("M_HW_RES[3]=0x%x\n", M_HW_RES[3]);
	pr_notice("M_HW_RES[4]=0x%x\n", M_HW_RES[4]);
	pr_notice("M_HW_RES[5]=0x%x\n", M_HW_RES[5]);

	pr_debug("p->PTPINITEN=0x%x\n", p->PTPINITEN);
	pr_debug("p->PTPMONEN=0x%x\n", p->PTPMONEN);

	/* p->PTPINITEN = 0; */ /* TODO: FIXME */
	/* p->PTPMONEN  = 0; */ /* TODO: FIXME */

	FUNC_EXIT(FUNC_LV_HELP);
}

#ifdef CONFIG_MTK_RAM_CONSOLE
static void _mt_ptp_aee_init(void)
{
	aee_rr_rec_ptp_vboot(0x0);
	aee_rr_rec_ptp_cpu_little_volt(0x0);
	aee_rr_rec_ptp_temp(0x0);

	/* 0xFF default value. 1 means writing new voltage, 0 means finish */
	aee_rr_rec_ptp_status(0x0);
}
#endif

static int ptp_probe(struct platform_device *pdev)
{
	int ret;
	struct ptp_det *det;
	struct ptp_ctrl *ctrl;
	struct clk *clk_infra_therm;

	FUNC_ENTER(FUNC_LV_MODULE);

#ifdef CONFIG_MTK_RAM_CONSOLE
	_mt_ptp_aee_init();
#endif
	/* enable thermal CG */
	/* enable_clock(MT_CG_INFRA_THERM, "PTPOD"); */
	clk_infra_therm = devm_clk_get(&pdev->dev, "ptp_infra_therm");
	WARN_ON(IS_ERR(clk_infra_therm));
	clk_prepare_enable(clk_infra_therm);

	/* set PTP IRQ */
	ret = request_irq(ptpod_irq_number,
			  ptp_isr, IRQF_TRIGGER_LOW, "ptp", NULL);
	if (ret) {
		pr_err("PTP IRQ register failed (%d)\n", ret);
		WARN_ON(1);
	}

	pr_debug("Set PTP IRQ OK.\n");

	/* ptp_level = mt_ptp_get_level(); */

	/* atomic_set(&ptp_init01_cnt, 0); */
	for_each_ctrl(ctrl) {
		ptp_init_ctrl(ctrl);
	}

	/* disable frequency hopping (main PLL) */
	mt_fh_popod_save();

	/* disable DVFS and set vproc = 1.15v (1 GHz) */
	mt_cpufreq_disable_by_ptpod(MT_CPU_DVFS_LITTLE);
#ifdef CONFIG_MTK_INHOUSE_GPU
	mt_gpufreq_disable_by_ptpod();
	/* enable_clock( MT_CG_DISP0_SMI_COMMON, "GPU"); */
	/* enable_clock( MT_CG_MFG_BG3D, "GPU"); */
#endif

	/* turn off slow idle*/
	ptp_data[0] = 0xffffffff;

	for_each_det(det) {
		ptp_init_det(det, &ptp_devinfo);
	}

	ptp_init01();

	/* turn on slow idle */
	ptp_data[0] = 0;

#ifdef CONFIG_MTK_INHOUSE_GPU
	/* disable_clock( MT_CG_MFG_BG3D, "GPU"); */
	/* disable_clock( MT_CG_DISP0_SMI_COMMON, "GPU"); */
	/* enable DVFS */
	mt_gpufreq_enable_by_ptpod();
#endif
	mt_cpufreq_enable_by_ptpod(MT_CPU_DVFS_LITTLE);

	/* enable frequency hopping (main PLL) */
	mt_fh_popod_restore();

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int ptp_suspend(struct platform_device *pdev, pm_message_t state)
{
	FUNC_ENTER(FUNC_LV_MODULE);
	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int ptp_resume(struct platform_device *pdev)
{
	FUNC_ENTER(FUNC_LV_MODULE);
	ptp_init02();
	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_ptpod_of_match[] = {
	{ .compatible = "mediatek,mt8163-ptp_od", },
	{},
};
#endif
static struct platform_driver ptp_driver = {
	.remove     = NULL,
	.shutdown   = NULL,
	.probe      = ptp_probe,
	.suspend    = ptp_suspend,
	.resume     = ptp_resume,
	.driver     = {
		.name   = "mt-ptp",
		#ifdef CONFIG_OF
		.of_match_table = mt_ptpod_of_match,
		#endif
	},
};


int mt_ptp_opp_num(enum ptp_det_id id)
{
	struct ptp_det *det = id_to_ptp_det(id);

	FUNC_ENTER(FUNC_LV_API);
	FUNC_EXIT(FUNC_LV_API);

	return det->num_freq_tbl;
}
EXPORT_SYMBOL(mt_ptp_opp_num);

void mt_ptp_opp_freq(enum ptp_det_id id, unsigned int *freq)
{
	struct ptp_det *det = id_to_ptp_det(id);
	int i = 0;

	FUNC_ENTER(FUNC_LV_API);

	for (i = 0; i < det->num_freq_tbl; i++)
		freq[i] = det->freq_tbl[i];

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_ptp_opp_freq);

void mt_ptp_opp_status(enum ptp_det_id id,
		       unsigned int *temp, unsigned int *volt)
{
	struct ptp_det *det = id_to_ptp_det(id);
	int i = 0;

	FUNC_ENTER(FUNC_LV_API);

#ifdef CONFIG_THERMAL
	if (id == PTP_DET_MCUSYS)
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK0);
	else
		*temp = tscpu_get_temp_by_bank(THERMAL_BANK1);
#else
	*temp = 25000;
#endif
	for (i = 0; i < det->num_freq_tbl; i++)
		volt[i] = PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[i]);

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_ptp_opp_status);

/***************************
 * return current PTP stauts
 ***************************
 */
int mt_ptp_status(enum ptp_det_id id)
{
	struct ptp_det *det = id_to_ptp_det(id);

	FUNC_ENTER(FUNC_LV_API);

	WARN_ON(!det);
	WARN_ON(!det->ops);
	WARN_ON(!det->ops->get_status);

	FUNC_EXIT(FUNC_LV_API);

	return det->ops->get_status(det);
}

/**
 * ===============================================
 * PROCFS interface for debugging
 * ===============================================
 */

/*
 * show current PTP stauts
 */
static int ptp_debug_proc_show(struct seq_file *m, void *v)
{
	struct ptp_det *det = (struct ptp_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	/* FIXME: PTPEN sometimes is disabled temp */
	seq_printf(m, "PTPOD[%s] %s (ptp_level = 0x%08X)\n",
		   det->name,
		   det->ops->get_status(det) ?
		   "enabled" : "disable",
		   ptp_level);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * set PTP status by procfs interface
 */
static ssize_t ptp_debug_proc_write(struct file *file,
				    const char __user *buffer,
				    size_t count, loff_t *pos)
{
	int ret;
	int enabled = 0;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct ptp_det *det = (struct ptp_det *)PDE_DATA(file_inode(file));

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (!kstrtoint(buf, 10, &enabled)) {
		ret = 0;

		if (enabled == 1)
			; /* det->ops->enable(det, BY_PROCFS); */
		else
			det->ops->disable(det, BY_PROCFS);
	} else {
		ret = -EINVAL;
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

/*
 * show current PTP data
 */
static int ptp_dump_proc_show(struct seq_file *m, void *v)
{
	struct ptp_det *det;
	int *M_HW_RES = (int *)&ptp_devinfo;
	int i, j;

	FUNC_ENTER(FUNC_LV_HELP);

	for (i = 0; i < sizeof(struct ptp_devinfo)/sizeof(unsigned int); i++)
		seq_printf(m, "M_HW_RES%d\t= 0x%08X\n", i, M_HW_RES[i]);
#if 0
	seq_printf(m, "det->PTPMONEN= 0x%08X,det->PTPINITEN= 0x%08X\n",
		   det->PTPMONEN, det->PTPINITEN);
	seq_printf(m, "leakage_core\t= %d\n"
		   "leakage_gpu\t= %d\n"
		   "leakage_little\t= %d\n"
		   "leakage_big\t= %d\n",
		   leakage_core,
		   leakage_gpu,
		   leakage_sram2,
		   leakage_sram1
		   );
#endif
	for_each_det(det) {
		seq_printf(m, "PTP_DCVALUES[%s]\t= 0x%08X\n",
			   det->name, det->VBOOT);

		for (i = PTP_PHASE_INIT01; i < NR_PTP_PHASE; i++) {
			seq_printf(m, "dcvalues=0x%08X, ptp_freqpct30=0x%08X,",
				   det->dcvalues[i], det->ptp_freqpct30[i]);
			seq_printf(m, "ptp_26c=0x%08X,", det->ptp_26c[i]);
			seq_printf(m, "ptp_vop30=0x%08X, ptp_ptpen= 0x%08X\n",
				   det->ptp_vop30[i], det->ptp_ptpen[i]);

			if (det->ptp_ptpen[i] == 0x5) {
				seq_printf(m, "[%s] (%d) - (%d, %d, %d, %d, ",
				det->name, det->ops->get_temp(det),
				PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[0]),
				PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[1]),
				PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[2]),
				PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[3]));

				seq_printf(m, "%d, %d, %d, %d) - ",
				PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[4]),
				PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[5]),
				PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[6]),
				PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[7]));

				seq_printf(m, "(%d, %d, %d, %d, %d, %d, %d, ",
					   det->freq_tbl[0],
					   det->freq_tbl[1],
					   det->freq_tbl[2],
					   det->freq_tbl[3],
					   det->freq_tbl[4],
					   det->freq_tbl[5],
					   det->freq_tbl[6]);

				seq_printf(m, "%d)\n", det->freq_tbl[7]);
			}
		}
	}

#if DUMP_DATA_TO_DE
	for_each_det(det) {
		for (i = PTP_PHASE_INIT01; i < NR_PTP_PHASE; i++) {
			seq_printf(m, "Bank_number = %d\n", det->ctrl_id);

			if (i < PTP_PHASE_MON)
				seq_printf(m, "mode = init%d\n", i + 1);
			else
				seq_puts(m, "mode = mon\n");

			for (j = 0; j < ARRAY_SIZE(reg_dump_addr_off); j++)
				seq_printf(m, "0x%p = 0x%08X\n",
					   PTP_BASEADDR + reg_dump_addr_off[j],
					   det->reg_dump_data[j][i]);
		}
		seq_printf(m, "[PTP] INIT2VALS %d, %d, VDESIGN0 %d\n", (short int)(det->DCVOFFSETIN & 0xffff), det->AGEVOFFSETIN, ptp_read(PTP_VDESIGN30) & 0xff);
	}
#endif

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * show current voltage
 */
static int ptp_cur_volt_proc_show(struct seq_file *m, void *v)
{
	struct ptp_det *det = (struct ptp_det *)m->private;
	u32 rdata = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	rdata = det->ops->get_volt(det);

	if (rdata != 0)
		seq_printf(m, "%d\n", rdata);
	else
		seq_printf(m, "[%s] read current voltage fail\n", det->name);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}
#define CPU_TURBO_VOLT_DIFF_1 4375
#define CPU_TURBO_VOLT_DIFF_2 2500
#define NUM_OF_CPU_FREQ_TABLE 8
#define NUM_OF_GPU_FREQ_TABLE 6

static int ptpod_not_work(u32 cur_volt, unsigned int *volt_tbl_pmic, int num)
{
	int i;

	if (num == NUM_OF_CPU_FREQ_TABLE)
		pr_notice("CPU: In ptpod_not_work, cur_volt = %d -> %d\n",
			  cur_volt,  PTP_VOLT_TO_PMIC_VAL(cur_volt));
	else
		pr_notice("GPU: In ptpod_not_work, cur_volt = %d -> %d\n",
			  cur_volt,  PTP_VOLT_TO_PMIC_VAL(cur_volt));

	for (i = 0; i < num; i++) {
		pr_notice("volt_tbl_pmic[%d] = %d => %d\n",
			  i, volt_tbl_pmic[i],
			  PTP_PMIC_VAL_TO_VOLT(volt_tbl_pmic[i]));

		if (cur_volt == PTP_PMIC_VAL_TO_VOLT(volt_tbl_pmic[i]))
			return 0;
	}

	if ((cur_volt == (PTP_PMIC_VAL_TO_VOLT(volt_tbl_pmic[0]) +
			  CPU_TURBO_VOLT_DIFF_1)) ||
	    (cur_volt == (PTP_PMIC_VAL_TO_VOLT(volt_tbl_pmic[0]) +
			  CPU_TURBO_VOLT_DIFF_2))) {
		pr_notice("%d, %d, %d\n", cur_volt,
			  PTP_PMIC_VAL_TO_VOLT(volt_tbl_pmic[0]) +
					       CPU_TURBO_VOLT_DIFF_1,
			  PTP_PMIC_VAL_TO_VOLT(volt_tbl_pmic[0]) +
					       CPU_TURBO_VOLT_DIFF_2);
		return 0;
	}

	return 1;
}

static int ptp_stress_result_proc_show(struct seq_file *m, void *v)
{
	unsigned int result = 0;
	struct ptp_det *det;
	u32 rdata = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	for_each_det(det) {
		rdata = det->ops->get_volt(det);

		switch (det->ctrl_id) {
		case PTP_CTRL_MCUSYS:
			result |= (ptpod_not_work(rdata,
						  det->volt_tbl_pmic,
						  NUM_OF_CPU_FREQ_TABLE)
				   << PTP_CTRL_MCUSYS);

			if ((result & (1 << PTP_CTRL_MCUSYS)) != 0)
				pr_err("CPU PTP fail\n");
			break;
#ifdef CONFIG_MTK_INHOUSE_GPU
		case PTP_CTRL_GPUSYS:
			result |= (ptpod_not_work(rdata,
						  det->volt_tbl_pmic,
						  NUM_OF_GPU_FREQ_TABLE)
				   << PTP_CTRL_GPUSYS);

			if ((result & (1 << PTP_CTRL_GPUSYS)) != 0)
				pr_err("GPU PTP fail\n");
			break;
#endif
		default:
			break;
		}
	}

	seq_printf(m, "0x%X\n", result);

	return 0;
}
/*
 * show current PTP status
 */
static int ptp_status_proc_show(struct seq_file *m, void *v)
{
	struct ptp_det *det = (struct ptp_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	seq_printf(m, "[%s] (%d) - (%d, %d, %d, %d, %d, %d, %d, %d) - ",
		   det->name, det->ops->get_temp(det),
		   PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[0]),
		   PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[1]),
		   PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[2]),
		   PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[3]),
		   PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[4]),
		   PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[5]),
		   PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[6]),
		   PTP_PMIC_VAL_TO_VOLT(det->volt_tbl_pmic[7]));

	seq_printf(m, "(%d, %d, %d, %d, %d, %d, %d, %d)\n",
		   det->freq_tbl[0],
		   det->freq_tbl[1],
		   det->freq_tbl[2],
		   det->freq_tbl[3],
		   det->freq_tbl[4],
		   det->freq_tbl[5],
		   det->freq_tbl[6],
		   det->freq_tbl[7]);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * set PTP log enable by procfs interface
 */

static int ptp_log_en_proc_show(struct seq_file *m, void *v)
{
	FUNC_ENTER(FUNC_LV_HELP);
	seq_printf(m, "%d\n", ptp_log_en);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static ssize_t ptp_log_en_proc_write(struct file *file,
				     const char __user *buffer,
				     size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	ret = 0;

	if (kstrtoint(buf, 10, &ptp_log_en))
		pr_notice("kstrtoint(xx, 10, xx) cannot parse your input.");

	switch (ptp_log_en) {
	case 0:
		pr_notice("ptp log disabled, ptp_log_en = %d\n", ptp_log_en);
		hrtimer_cancel(&ptp_log_timer);
		break;

	case 1:
		pr_notice("ptp log enabled, ptp_log_en = %d\n", ptp_log_en);
		hrtimer_start(&ptp_log_timer,
			      ns_to_ktime(LOG_INTERVAL), HRTIMER_MODE_REL);
		break;

	default:
		pr_warn("bad argument!! Should be \"0\" or \"1\"\n");
		ret = -EINVAL;
		break;
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}


/*
 * show PTP offset
 */
static int ptp_offset_proc_show(struct seq_file *m, void *v)
{
	struct ptp_det *det = (struct ptp_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	seq_printf(m, "%d\n", det->volt_offset);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

/*
 * set PTP offset by procfs
 */
static ssize_t ptp_offset_proc_write(struct file *file,
				     const char __user *buffer,
				     size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	int offset = 0;
	struct ptp_det *det = (struct ptp_det *)PDE_DATA(file_inode(file));

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (!kstrtoint(buf, 10, &offset)) {
		ret = 0;
		det->volt_offset = offset;
		ptp_set_ptp_volt(det);
	} else {
		ret = -EINVAL;
		pr_warn("bad argument_1!! argument should be \"0\"\n");
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

#define PROC_FOPS_RW(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner          = THIS_MODULE,				\
		.open           = name ## _proc_open,			\
		.read           = seq_read,				\
		.llseek         = seq_lseek,				\
		.release        = single_release,			\
		.write          = name ## _proc_write,			\
	}

#define PROC_FOPS_RO(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner          = THIS_MODULE,				\
		.open           = name ## _proc_open,			\
		.read           = seq_read,				\
		.llseek         = seq_lseek,				\
		.release        = single_release,			\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(ptp_debug);
PROC_FOPS_RO(ptp_dump);
PROC_FOPS_RO(ptp_stress_result);
PROC_FOPS_RW(ptp_log_en);
PROC_FOPS_RO(ptp_status);
PROC_FOPS_RO(ptp_cur_volt);
PROC_FOPS_RW(ptp_offset);

static int create_procfs(void)
{
	struct proc_dir_entry *ptp_dir = NULL;
	struct proc_dir_entry *det_dir = NULL;
	int i;
	struct ptp_det *det;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry det_entries[] = {
		PROC_ENTRY(ptp_debug),
		PROC_ENTRY(ptp_status),
		PROC_ENTRY(ptp_cur_volt),
		PROC_ENTRY(ptp_offset),
	};

	struct pentry ptp_entries[] = {
		PROC_ENTRY(ptp_dump),
		PROC_ENTRY(ptp_log_en),
		PROC_ENTRY(ptp_stress_result),
	};

	FUNC_ENTER(FUNC_LV_HELP);

	ptp_dir = proc_mkdir("ptp", NULL);

	if (!ptp_dir) {
		pr_err("[%s]: mkdir /proc/ptp failed\n", __func__);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(ptp_entries); i++) {
		if (!proc_create(ptp_entries[i].name,
				 0664, ptp_dir, ptp_entries[i].fops)) {
			pr_err("[%s]: create /proc/ptp/%s failed\n",
			       __func__, ptp_entries[i].name);
			FUNC_EXIT(FUNC_LV_HELP);
			return -3;
		}
	}

	for_each_det(det) {
		det_dir = proc_mkdir(det->name, ptp_dir);

		if (!det_dir) {
			pr_err("[%s]: mkdir /proc/ptp/%s failed\n",
			       __func__, det->name);
			FUNC_EXIT(FUNC_LV_HELP);
			return -2;
		}

		for (i = 0; i < ARRAY_SIZE(det_entries); i++) {
			if (!proc_create_data(det_entries[i].name, 0664,
				det_dir, det_entries[i].fops, det)) {
				pr_err("[%s]: create /proc/ptp/%s/%s failed\n",
				       __func__, det->name,
				       det_entries[i].name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -3;
			}
		}
	}

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}


int get_ptpod_status(void)
{
	get_devinfo(&ptp_devinfo);

	return ptp_devinfo.PTPINITEN;
}
EXPORT_SYMBOL(get_ptpod_status);


/*
 * Module driver
 */
static int __init ptp_init(void)
{
	int err = 0;
	struct device_node *node = NULL;

	func_lv_mask = 0;

	FUNC_ENTER(FUNC_LV_MODULE);
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8163-ptp_od");
	if (node) {
		/* Setup IO addresses */
		ptpod_base = of_iomap(node, 0);
		pr_debug("ptpod_base=0x%p\n", ptpod_base);
	}

	/* get ptpod irq num */
	ptpod_irq_number = irq_of_parse_and_map(node, 0);
	pr_debug("[THERM_CTRL] ptpod_irq_number=%d\n", ptpod_irq_number);
	if (!ptpod_irq_number) {
		pr_err("get irq num failed=0x%x\n", ptpod_irq_number);
		return 0;
	}

	get_devinfo(&ptp_devinfo);

	if (ptp_devinfo.PTPINITEN == 0) {
		pr_err("PTPINITEN = 0x%08X\n", ptp_devinfo.PTPINITEN);
		FUNC_EXIT(FUNC_LV_MODULE);
		return 0;
	}

	/*
	 * init timer for log / volt
	 */
	hrtimer_init(&ptp_log_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ptp_log_timer.function = ptp_log_timer_func; /* <-XXX */

	create_procfs();

	/*
	 * reg platform device driver
	 */
	err = platform_driver_register(&ptp_driver);

	if (err) {
		pr_err("PTP driver callback register failed..\n");
		FUNC_EXIT(FUNC_LV_MODULE);
		return err;
	}

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static void __exit ptp_exit(void)
{
	FUNC_ENTER(FUNC_LV_MODULE);
	pr_err("PTP de-initialization\n");
	FUNC_EXIT(FUNC_LV_MODULE);
}

/* late_initcall(ptp_init); */
late_initcall_sync(ptp_init);
#endif

MODULE_DESCRIPTION("MediaTek PTPOD Driver v0.3");
MODULE_LICENSE("GPL");

#undef __MT_PTP_C__
