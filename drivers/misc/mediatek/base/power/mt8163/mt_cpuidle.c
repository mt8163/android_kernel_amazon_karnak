/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Cheng-En Chung <cheng-en.chung@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <asm/irqflags.h>
#include <asm/neon.h>
#include <asm/suspend.h>
#ifdef CONFIG_ARM64
#include <asm/cpuidle.h>
/* TODO: remove later, only for test */
#include <asm/cpu_ops.h>
#endif
#include <linux/of_address.h>
#include <linux/of.h>
/* TODO: remove later, only for test */
#include <linux/of_device.h>
#include <linux/cpuidle.h>
#include <linux/psci.h>

/* #include <mt-plat/mt_dbg.h> */
#include <mt-plat/mtk_io.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_chip.h>

#include "mt_cpuidle.h"
#include "mt_spm.h"

#ifdef CONFIG_MTK_RAM_CONSOLE
/* #include <mach/mt_secure_api.h> */
#endif


#define TAG "[Power-Dormant] "

#define dormant_err(fmt, args...)	pr_info(TAG fmt, ##args)
#define dormant_warn(fmt, args...)	pr_info(TAG fmt, ##args)
#define dormant_dbg(fmt, args...)	pr_debug(TAG fmt, ##args)


#ifdef CONFIG_MTK_RAM_CONSOLE
#if 0
unsigned long *sleep_aee_rec_cpu_dormant_pa;
unsigned long *sleep_aee_rec_cpu_dormant_va;
#endif
#endif

#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M)
static unsigned long biu_base;
#endif
static unsigned long gic_id_base;

/* static unsigned int kp_irq_bit; */
/* static unsigned int conn_wdt_irq_bit; */
/* static unsigned int lowbattery_irq_bit; */

#define MAX_CORES 4
#define MAX_CLUSTER 2

#if defined(CONFIG_ARCH_MT6735)
#define BIU_NODE		"mediatek,mt6735-mcu_biu"
#elif defined(CONFIG_ARCH_MT6735M)
#define BIU_NODE		"mediatek,mt6735m-mcu_biu"
#endif
#define GIC_NODE		"arm,gic-400"
#define KP_NODE			"mediatek,mt8163-keypad"
/* #define CONSYS_NODE		"mediatek,CONSYS" */
/* #define AUXADC_NODE		"mediatek,mt6735-auxadc" */

#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M)
#define DMT_BIU_BASE		(biu_base)
#endif
#define DMT_GIC_DIST_BASE	(gic_id_base)
/* #define DMT_KP_IRQ_BIT		(kp_irq_bit) */
/* #define DMT_CONN_WDT_IRQ_BIT      (conn_wdt_irq_bit) */
/* #define DMT_LOWBATTERY_IRQ_BIT	(lowbattery_irq_bit) */


#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M)
#define BIU_CONTROL		(DMT_BIU_BASE)
#define CMD_QUEUE_EN		(1 << 0)
#define DCM_EN			(1 << 1)
#define TLB_ULTRA_EN		(1 << 8)
#endif

#define GIC_PRIVATE_SIGNALS			(32)

#define reg_read(addr)		__raw_readl(IOMEM(addr))
#define reg_write(addr, val)	mt_reg_sync_writel(val, addr)

struct core_context {
	unsigned int banked_regs[32];
	u64 timestamp[5];
	unsigned long timer_data[8];
};

struct cluster_context {
	struct core_context core[MAX_CORES] ____cacheline_aligned;
	unsigned long dbg_data[40];
};

struct system_context {
	struct cluster_context cluster[MAX_CLUSTER];
};

struct system_context dormant_data[1];
static int mt_dormant_initialized;

#define SPM_CORE_ID() core_idx()
#define SPM_IS_CPU_IRQ_OCCUR(core_id) \
({(!!(spm_read(SPM_SLEEP_WAKEUP_MISC) &\
		((0x101<<(core_id)))));\
	})

#ifdef CONFIG_MTK_RAM_CONSOLE
#define DORMANT_LOG(cid, pattern)
#else
#define DORMANT_LOG(cid, pattern)
#endif

#define core_idx()							\
	({								\
		((read_cluster_id() >> 6) | read_cpu_id());		\
	})

inline void read_id(int *cpu_id, int *cluster_id)
{
	*cpu_id = read_cpu_id();
	*cluster_id = read_cluster_id();
}

#define system_cluster(system, clusterid)\
	(&((struct system_context *)system)->cluster[clusterid])
#define cluster_core(cluster, cpuid)\
	(&((struct cluster_context *)cluster)->core[cpuid])

void *_get_data(int core_or_cluster)
{
	int cpuid, clusterid;
	struct cluster_context *cluster;
	struct core_context *core;

	read_id(&cpuid, &clusterid);

	cluster = system_cluster(dormant_data, clusterid);
	if (core_or_cluster == 1)
		return (void *)cluster;

	core = cluster_core(cluster, cpuid);

	return (void *)core;
}

#define GET_CORE_DATA()		((struct core_context *)_get_data(0))
#define GET_CLUSTER_DATA()	((struct cluster_context *)_get_data(1))

void stop_generic_timer(void)
{
	write_cntpctl(read_cntpctl() & ~1);
}

void start_generic_timer(void)
{
	write_cntpctl(read_cntpctl() | 1);
}

struct set_and_clear_regs {
	unsigned int set[32], clear[32];
};

struct interrupt_distributor {
	unsigned int control;			/* 0x000 */
	const unsigned int controller_type;
	const unsigned int implementer;
	const char padding1[116];
	unsigned int security[32];		/* 0x080 */
	struct set_and_clear_regs enable;	/* 0x100 */
	struct set_and_clear_regs pending;	/* 0x200 */
	struct set_and_clear_regs active;	/* 0x300 */
	unsigned int priority[256];		/* 0x400 */
	unsigned int target[256];		/* 0x800 */
	unsigned int configuration[64];	/* 0xC00 */
	const char padding3[256];	/* 0xD00 */
	unsigned int non_security_access_control[64]; /* 0xE00 */
	unsigned int software_interrupt;	/* 0xF00 */
	unsigned int sgi_clr_pending[4];	/* 0xF10 */
	unsigned int sgi_set_pending[4];	/* 0xF20 */
	const char padding4[176];

	unsigned const int peripheral_id[4];	/* 0xFE0 */
	unsigned const int primecell_id[4];	/* 0xFF0 */
};

#if 0
static void restore_gic_spm_irq(unsigned long gic_distributor_address)
{
	struct interrupt_distributor *id =
		(struct interrupt_distributor *) gic_distributor_address;
	unsigned int backup;
	int i, j;

	backup = id->control;
	id->control = 0;

	/* Set the pending bit for spm wakeup source that is edge triggerd */
	if (reg_read(SPM_SLEEP_ISR_RAW_STA) & WAKE_SRC_KP) {
		i = DMT_KP_IRQ_BIT / GIC_PRIVATE_SIGNALS;
		j = DMT_KP_IRQ_BIT % GIC_PRIVATE_SIGNALS;
		id->pending.set[i] |= (1 << j);
	}
#if 0
	if (reg_read(SPM_SLEEP_ISR_RAW_STA) & WAKE_SRC_CONN_WDT) {
		i = DMT_CONN_WDT_IRQ_BIT / GIC_PRIVATE_SIGNALS;
		j = DMT_CONN_WDT_IRQ_BIT % GIC_PRIVATE_SIGNALS;
		id->pending.set[i] |= (1 << j);
	}
#endif
#if 0
	if (reg_read(SPM_SLEEP_ISR_RAW_STA) & WAKE_SRC_LOW_BAT) {
		i = DMT_LOWBATTERY_IRQ_BIT / GIC_PRIVATE_SIGNALS;
		j = DMT_LOWBATTERY_IRQ_BIT % GIC_PRIVATE_SIGNALS;
		id->pending.set[i] |= (1 << j);
	}
#endif
	id->control = backup;
}
#endif

#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M)
static inline void biu_reconfig(void)
{
	int val;

	val = reg_read(BIU_CONTROL);
	val |= TLB_ULTRA_EN;
	val |= DCM_EN;
	val |= CMD_QUEUE_EN;
	reg_write(BIU_CONTROL, val);
}
#endif

static void mt_cluster_restore(int flags)
{
#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M)
	biu_reconfig();
#endif
}

unsigned int __weak *mt_save_dbg_regs(unsigned int *p, unsigned int cpuid)
{
	return p;
}
void __weak mt_restore_dbg_regs(unsigned int *p, unsigned int cpuid) { }
void __weak mt_copy_dbg_regs(int to, int from) { }
void __weak mt_save_banked_registers(unsigned int *container) { }
void __weak mt_restore_banked_registers(unsigned int *container) { }

void mt_cpu_save(void)
{
	struct core_context *core;
/*	struct cluster_context *cluster; */
	unsigned int sleep_sta;
	int cpuid, clusterid;

	read_id(&cpuid, &clusterid);

	core = GET_CORE_DATA();

	mt_save_generic_timer((unsigned int *)core->timer_data, 0x0);
	stop_generic_timer();

	if (clusterid == 0)
		sleep_sta = (spm_read(SPM_SLEEP_TIMER_STA) >> 16) & 0x0f;
	else
		sleep_sta = (spm_read(SPM_SLEEP_TIMER_STA) >> 20) & 0x0f;

#if 0
	if ((sleep_sta | (1 << cpuid)) == 0x0f) {	/* last core */
		cluster = GET_CLUSTER_DATA();
		mt_save_dbg_regs((unsigned int *)cluster->dbg_data,
			cpuid + (clusterid * 4));
	}
#endif

	mt_save_banked_registers(core->banked_regs);
}

void mt_cpu_restore(void)
{
	struct core_context *core;
/*	struct cluster_context *cluster; */
	unsigned int sleep_sta;
	int cpuid, clusterid;

	read_id(&cpuid, &clusterid);

	core = GET_CORE_DATA();

	mt_restore_banked_registers(core->banked_regs);

	if (clusterid == 0)
		sleep_sta = (spm_read(SPM_SLEEP_TIMER_STA) >> 16) & 0x0f;
	else
		sleep_sta = (spm_read(SPM_SLEEP_TIMER_STA) >> 20) & 0x0f;

	sleep_sta = (sleep_sta | (1 << cpuid));

#if 0
	if (sleep_sta == 0x0f) {	/* first core */
		cluster = GET_CLUSTER_DATA();
		mt_restore_dbg_regs((unsigned int *)cluster->
			dbg_data, cpuid + (clusterid * 4));
	} else {
		int any = __builtin_ffs(~sleep_sta) - 1;

		mt_copy_dbg_regs(cpuid + (clusterid * 4),
			any + (clusterid * 4));
	}
#endif

	mt_restore_generic_timer((unsigned int *)core->timer_data, 0x0);
}

void mt_platform_save_context(int flags)
{
	mt_cpu_save();
}

void mt_platform_restore_context(int flags)
{
	mt_cluster_restore(flags);
	mt_cpu_restore();

#if 0
	if (IS_DORMANT_GIC_OFF(flags))
		restore_gic_spm_irq(DMT_GIC_DIST_BASE);
#endif
}

#ifndef CONFIG_ARM64
int mt_cpu_dormant_psci(unsigned long flags)
{
	int ret = 1;
	int cpuid, clusterid;
	unsigned int state;

	read_id(&cpuid, &clusterid);

	state = 0x201 << 16;

	if (psci_ops.cpu_suspend) {
		ret = psci_ops.cpu_suspend(state, virt_to_phys(cpu_resume));
	}

	WARN_ON(1);

	return ret;
}
#endif

int mt_cpu_dormant(unsigned long flags)
{
#ifndef CONFIG_ARM64
	int ret;
#endif

	if (!mt_dormant_initialized)
		return MT_CPU_DORMANT_BYPASS;

	/* to mark as cpu clobs vfp register. */
	kernel_neon_begin();

	mt_platform_save_context(flags);

#ifndef CONFIG_ARM64
	ret = cpu_suspend(2, mt_cpu_dormant_psci);
#else
	arm_cpuidle_suspend(2);
#endif

	mt_platform_restore_context(flags);

	local_fiq_enable();

	kernel_neon_end();

	return 0;
}

static int mt_dormant_dts_map(void)
{
	struct device_node *node;
/*	u32 kp_interrupt[3]; */
/*	u32 auxadc_interrupt[3]; */

	node = of_find_compatible_node(NULL, NULL, GIC_NODE);
	if (!node) {
		dormant_err("error: cannot find node " GIC_NODE);
		WARN_ON(1);
	}
	gic_id_base = (unsigned long)of_iomap(node, 0);
	if (!gic_id_base) {
		dormant_err("error: cannot iomap " GIC_NODE);
		WARN_ON(1);
	}
	of_node_put(node);

#if 0
	node = of_find_compatible_node(NULL, NULL, KP_NODE);
	if (!node) {
		dormant_err("error: cannot find node " KP_NODE);
		WARN_ON(1);
	}
	if (of_property_read_u32_array(node, "interrupts",
		kp_interrupt, ARRAY_SIZE(kp_interrupt))) {
		dormant_err("error: cannot property_read " KP_NODE);
		WARN_ON(1);
	}
	/* irq[0] = 0 => spi */
	kp_irq_bit = ((1 - kp_interrupt[0]) << 5) + kp_interrupt[1];
	of_node_put(node);
	dormant_dbg("kp_irq_bit = %u\n", kp_irq_bit);
#endif
#if 0
	/* conn_wdt_irq_bit */
	node = of_find_compatible_node(NULL, NULL, CONSYS_NODE);
	if (!node) {
		CPU_DORMANT_INFO("error: cannot find node " CONSYS_NODE);
		WARN_ON(1);
	}
	if (of_property_read_u32_array(node, "interrupts",
		consys_interrupt, ARRAY_SIZE(consys_interrupt))) {
		CPU_DORMANT_INFO("error: cannot property_read " CONSYS_NODE);
		WARN_ON(1);
	}
	/* irq[0] = 0 => spi */
	conn_wdt_irq_bit = ((1 - consys_interrupt[3]) << 5) +
		consys_interrupt[4];
	of_node_put(node);
	CPU_DORMANT_INFO("conn_wdt_irq_bit = %u\n", conn_wdt_irq_bit);
#endif
#if 0
	node = of_find_compatible_node(NULL, NULL, AUXADC_NODE);
	if (!node) {
		dormant_err("error: cannot find node " AUXADC_NODE);
		WARN_ON(1);
	}
	if (of_property_read_u32_array(node, "interrupts",
		auxadc_interrupt, ARRAY_SIZE(auxadc_interrupt))) {
		dormant_err("error: cannot property_read " AUXADC_NODE);
		WARN_ON(1);
	}
	/* irq[0] = 0 => spi */
	lowbattery_irq_bit = ((1 - auxadc_interrupt[0]) << 5) +
		auxadc_interrupt[1];
	of_node_put(node);
	dormant_dbg("lowbattery_irq_bit = %u\n", lowbattery_irq_bit);
#endif

	return 0;
}

static int mt_cpu_dormant_init(void)
{
	int cpuid, clusterid;

	read_id(&cpuid, &clusterid);

	if (mt_dormant_initialized == 1)
		return MT_CPU_DORMANT_BYPASS;

	mt_dormant_dts_map();

	mt_dormant_initialized = 1;

	return 0;
}

postcore_initcall(mt_cpu_dormant_init);
