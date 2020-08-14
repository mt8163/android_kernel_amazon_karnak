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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "mtcmos.h"

static struct regmap *regmap_spm;

static DEFINE_SPINLOCK(spm_cpu_lock);

int __init mt_spm_mtcmos_init(void)
{
	static int init;
	struct device_node *spm_node;

	if (init)
		return 0;

	spm_node = of_find_compatible_node(NULL, NULL,
				"mediatek,mt8163-scpsys");
	if (!spm_node) {
		pr_err("Cannot found:%s!\n", spm_node->full_name);
		return -1;
	}

	regmap_spm = syscon_node_to_regmap(spm_node);
	if (IS_ERR(regmap_spm)) {
		pr_err("Cannot find regmap %s: %ld.\n",
			    spm_node->full_name,
			PTR_ERR(regmap_spm));
		return PTR_ERR(regmap_spm);
	}

	init = 1;

	return 0;
}

static unsigned int mtcmos_read(struct regmap *regmap, unsigned int off)
{
	unsigned int val = 0;

	regmap_read(regmap, off, &val);
	return val;
}
static void mtcmos_write(struct regmap *regmap, unsigned int off,
				unsigned int val)
{
	regmap_write(regmap, off, val);
}

#define spm_read(reg)			mtcmos_read(regmap_spm, reg)
#define spm_write(reg, val)		mtcmos_write(regmap_spm, reg, val)

void spm_mtcmos_cpu_lock(unsigned long *flags)
{
	spin_lock_irqsave(&spm_cpu_lock, *flags);
}

void spm_mtcmos_cpu_unlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&spm_cpu_lock, *flags);
}

typedef int (*spm_cpu_mtcmos_ctrl_func) (int state, int chkWfiBeforePdn);
static spm_cpu_mtcmos_ctrl_func spm_cpu_mtcmos_ctrl_funcs[] = {
	spm_mtcmos_ctrl_cpu0,
	spm_mtcmos_ctrl_cpu1,
	spm_mtcmos_ctrl_cpu2,
	spm_mtcmos_ctrl_cpu3,
};

__init int spm_mtcmos_cpu_init(void)
{
	mt_spm_mtcmos_init();

	pr_info("CPU num: %d\n", num_possible_cpus());

	return 0;
}


int spm_mtcmos_ctrl_cpu(unsigned int cpu, int state, int chkWfiBeforePdn)
{
	return (*spm_cpu_mtcmos_ctrl_funcs[cpu]) (state, chkWfiBeforePdn);
}

int spm_mtcmos_ctrl_cpu0(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) &
					CA7_CPU0_STANDBYWFI) == 0)
				;

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU0_L1_PDN,
			spm_read(SPM_CA7_CPU0_L1_PDN) | L1_PDN);

		while ((spm_read(SPM_CA7_CPU0_L1_PDN) & L1_PDN_ACK)
				!= L1_PDN_ACK)
			;

		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_RST_B);
		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_CLK_DIS);

		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU0) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU0) != 0))
			;

		spm_mtcmos_cpu_unlock(&flags);
	} else {		/* STA_POWER_ON */

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU0) !=
					CA7_CPU0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU0) !=
					CA7_CPU0))
			;

		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA7_CPU0_L1_PDN,
			spm_read(SPM_CA7_CPU0_L1_PDN) & ~L1_PDN);

		while ((spm_read(SPM_CA7_CPU0_L1_PDN) & L1_PDN_ACK) != 0)
			;

		udelay(1);
		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) | SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu1(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET,
		(SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) &
					CA7_CPU1_STANDBYWFI) == 0)
				;

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU1_L1_PDN,
			spm_read(SPM_CA7_CPU1_L1_PDN) | L1_PDN);

		while ((spm_read(SPM_CA7_CPU1_L1_PDN) & L1_PDN_ACK) !=
				L1_PDN_ACK)
			;

		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_RST_B);
		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_CLK_DIS);

		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU1) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU1) != 0))
			;

		spm_mtcmos_cpu_unlock(&flags);
	} else {

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU1) != CA7_CPU1)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU1) !=
					CA7_CPU1))
			;

		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA7_CPU1_L1_PDN,
			spm_read(SPM_CA7_CPU1_L1_PDN) & ~L1_PDN);

		while ((spm_read(SPM_CA7_CPU1_L1_PDN) & L1_PDN_ACK) != 0)
			;

		udelay(1);
		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) | SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu2(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET,
		(SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) &
					CA7_CPU2_STANDBYWFI) == 0)
				;

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU2_L1_PDN,
			spm_read(SPM_CA7_CPU2_L1_PDN) | L1_PDN);

		while ((spm_read(SPM_CA7_CPU2_L1_PDN) & L1_PDN_ACK) !=
				L1_PDN_ACK)
			;

		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_RST_B);
		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_CLK_DIS);

		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU2) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU2) != 0))
			;

		spm_mtcmos_cpu_unlock(&flags);
	} else {		/* STA_POWER_ON */

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU2) != CA7_CPU2)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU2) !=
					CA7_CPU2))
			;

		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA7_CPU2_L1_PDN,
			spm_read(SPM_CA7_CPU2_L1_PDN) & ~L1_PDN);

		while ((spm_read(SPM_CA7_CPU2_L1_PDN) & L1_PDN_ACK) != 0)
			;

		udelay(1);
		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) | SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu3(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET,
		(SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) &
					CA7_CPU3_STANDBYWFI) == 0)
				;

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU3_L1_PDN,
			spm_read(SPM_CA7_CPU3_L1_PDN) | L1_PDN);

		while ((spm_read(SPM_CA7_CPU3_L1_PDN) & L1_PDN_ACK) !=
				L1_PDN_ACK)
			;

		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_RST_B);
		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_CLK_DIS);

		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU3) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU3) != 0))
			;

		spm_mtcmos_cpu_unlock(&flags);
	} else {

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU3) != CA7_CPU3)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU3) !=
				   CA7_CPU3))
			;

		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA7_CPU3_L1_PDN,
			spm_read(SPM_CA7_CPU3_L1_PDN) & ~L1_PDN);

		while ((spm_read(SPM_CA7_CPU3_L1_PDN) & L1_PDN_ACK) != 0)
			;

		udelay(1);
		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) | SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpusys0(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET,
		(SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {

		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) &
					CA7_CPUTOP_STANDBYWFI) == 0)
				;

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPUTOP_L2_PDN,
			spm_read(SPM_CA7_CPUTOP_L2_PDN) | L2_SRAM_PDN);

		while ((spm_read(SPM_CA7_CPUTOP_L2_PDN) & L2_SRAM_PDN_ACK) !=
					L2_SRAM_PDN_ACK)
			;
		ndelay(1500);

		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_RST_B);
		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_CLK_DIS);

		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPUTOP) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPUTOP) != 0))
			;

		spm_mtcmos_cpu_unlock(&flags);
	} else {

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPUTOP) != CA7_CPUTOP)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPUTOP) !=
					CA7_CPUTOP))
			;

		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA7_CPUTOP_L2_PDN,
			spm_read(SPM_CA7_CPUTOP_L2_PDN) & ~L2_SRAM_PDN);

		while ((spm_read(SPM_CA7_CPUTOP_L2_PDN) & L2_SRAM_PDN_ACK) != 0)
			;

		ndelay(900);
		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) | SRAM_ISOINT_B);
		ndelay(100);
		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

bool spm_cpusys0_can_power_down(void)
{
	return !(spm_read(SPM_PWR_STATUS) &
		 (CA7_CPU1 | CA7_CPU2 | CA7_CPU3))
	    && !(spm_read(SPM_PWR_STATUS_2ND) &
		 (CA7_CPU1 | CA7_CPU2 | CA7_CPU3));
}

bool spm_cpusys1_can_power_down(void)
{
	return !(spm_read(SPM_PWR_STATUS) &
		 (CA7_CPU0 | CA7_CPU1 | CA7_CPU2 | CA7_CPU3
		  | CA7_CPUTOP | CA15_CPU1 | CA15_CPU2 |
		  CA15_CPU3))
	    && !(spm_read(SPM_PWR_STATUS_2ND) &
		 (CA7_CPU0 | CA7_CPU1 | CA7_CPU2 | CA7_CPU3
		  | CA7_CPUTOP | CA15_CPU1 | CA15_CPU2 |
		  CA15_CPU3));
}
