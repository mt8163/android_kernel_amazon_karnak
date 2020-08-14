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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/sched_clock.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>

#include "mt_freqhopping.h"
#include "sync_write.h"

#include <mt_freqhopping_drv.h>
#include <linux/seq_file.h>

#include <linux/of_address.h>
static void __iomem *g_fhctl_base;
static void __iomem *g_apmixed_base;
static void __iomem *g_ddrphy_base;

#define REG_ADDR(x)                 (g_fhctl_base   + x)
#define REG_APMIX_ADDR(x)           (g_apmixed_base + x)
#define REG_DDRPHY_ADDR(x)          (g_ddrphy_base  + x)

#define REG_FHCTL_HP_EN     REG_ADDR(0x0000)
#define REG_FHCTL_CLK_CON   REG_ADDR(0x0004)
#define REG_FHCTL_RST_CON   REG_ADDR(0x0008)
#define REG_FHCTL_SLOPE0    REG_ADDR(0x000C)
#define REG_FHCTL_SLOPE1    REG_ADDR(0x0010)
#define REG_FHCTL_DSSC_CFG  REG_ADDR(0x0014)

#define REG_FHCTL_DSSC0_CON REG_ADDR(0x0018)
#define REG_FHCTL_DSSC1_CON REG_ADDR(0x001C)
#define REG_FHCTL_DSSC2_CON REG_ADDR(0x0020)
#define REG_FHCTL_DSSC3_CON REG_ADDR(0x0024)
#define REG_FHCTL_DSSC4_CON REG_ADDR(0x0028)
#define REG_FHCTL_DSSC5_CON REG_ADDR(0x002C)
#define REG_FHCTL_DSSC6_CON REG_ADDR(0x0030)
#define REG_FHCTL_DSSC7_CON REG_ADDR(0x0034)

#define REG_FHCTL0_CFG      REG_ADDR(0x0038)
#define REG_FHCTL0_UPDNLMT  REG_ADDR(0x003C)
#define REG_FHCTL0_DDS      REG_ADDR(0x0040)
#define REG_FHCTL0_DVFS     REG_ADDR(0x0044)
#define REG_FHCTL0_MON      REG_ADDR(0x0048)

#define REG_FHCTL1_CFG      REG_ADDR(0x004C)
#define REG_FHCTL1_UPDNLMT  REG_ADDR(0x0050)
#define REG_FHCTL1_DDS      REG_ADDR(0x0054)
#define REG_FHCTL1_DVFS     REG_ADDR(0x0058)
#define REG_FHCTL1_MON      REG_ADDR(0x005C)

#define REG_FHCTL2_CFG      REG_ADDR(0x0060)
#define REG_FHCTL2_UPDNLMT  REG_ADDR(0x0064)
#define REG_FHCTL2_DDS      REG_ADDR(0x0068)
#define REG_FHCTL2_DVFS     REG_ADDR(0x006C)
#define REG_FHCTL2_MON      REG_ADDR(0x0070)

#define REG_FHCTL3_CFG      REG_ADDR(0x0074)
#define REG_FHCTL3_UPDNLMT  REG_ADDR(0x0078)
#define REG_FHCTL3_DDS      REG_ADDR(0x007C)
#define REG_FHCTL3_DVFS     REG_ADDR(0x0080)
#define REG_FHCTL3_MON      REG_ADDR(0x0084)

#define REG_FHCTL4_CFG      REG_ADDR(0x0088)
#define REG_FHCTL4_UPDNLMT  REG_ADDR(0x008C)
#define REG_FHCTL4_DDS      REG_ADDR(0x0090)
#define REG_FHCTL4_DVFS     REG_ADDR(0x0094)
#define REG_FHCTL4_MON      REG_ADDR(0x0098)

#define REG_FHCTL5_CFG      REG_ADDR(0x009C)
#define REG_FHCTL5_UPDNLMT  REG_ADDR(0x00A0)
#define REG_FHCTL5_DDS      REG_ADDR(0x00A4)
#define REG_FHCTL5_DVFS     REG_ADDR(0x00A8)
#define REG_FHCTL5_MON      REG_ADDR(0x00AC)

#define REG_FHCTL6_CFG      REG_ADDR(0x00B0)
#define REG_FHCTL6_UPDNLMT  REG_ADDR(0x00B4)
#define REG_FHCTL6_DDS      REG_ADDR(0x00B8)
#define REG_FHCTL6_DVFS     REG_ADDR(0x00BC)
#define REG_FHCTL6_MON      REG_ADDR(0x00C0)

#define REG_FHCTL7_CFG      REG_ADDR(0x00C4)
#define REG_FHCTL7_UPDNLMT  REG_ADDR(0x00C8)
#define REG_FHCTL7_DDS      REG_ADDR(0x00CC)
#define REG_FHCTL7_DVFS     REG_ADDR(0x00D0)
#define REG_FHCTL7_MON      REG_ADDR(0x00D4)

#define REG_FHCTL8_CFG      REG_ADDR(0x00D8)
#define REG_FHCTL8_UPDNLMT  REG_ADDR(0x00DC)
#define REG_FHCTL8_DDS      REG_ADDR(0x00E0)
#define REG_FHCTL8_DVFS     REG_ADDR(0x00E4)
#define REG_FHCTL8_MON      REG_ADDR(0x00E8)

/* **************************************************** */
/* APMIXED CON0/CON1 Register */
/* **************************************************** */

/*CON0, PLL enable status */
#define REG_ARMCA7PLL_CON0  REG_APMIX_ADDR(0x0210)
#define REG_MPLL_CON0       REG_APMIX_ADDR(0x0280)
#define REG_MAINPLL_CON0    REG_APMIX_ADDR(0x0220)
#define REG_MEMPLL_CON0     REG_DDRPHY_ADDR(0x0604)
#define REG_MSDCPLL_CON0    REG_APMIX_ADDR(0x0250)
#define REG_MMPLL_CON0      REG_APMIX_ADDR(0x0240)
#define REG_VENCPLL_CON0    REG_APMIX_ADDR(0x0260)
#define REG_TVDPLL_CON0     REG_APMIX_ADDR(0x0270)
#define REG_VCODECPLL_CON0  REG_APMIX_ADDR(0x0290)

/*CON1, DDS value */
#define REG_ARMCA7PLL_CON1  REG_APMIX_ADDR(0x0214)
#define REG_MPLL_CON1       REG_APMIX_ADDR(0x0284)
#define REG_MAINPLL_CON1    REG_APMIX_ADDR(0x0224)
#define REG_MEMPLL_CON1     REG_DDRPHY_ADDR(0x0600)
#define REG_MSDCPLL_CON1    REG_APMIX_ADDR(0x0254)
#define REG_MMPLL_CON1      REG_APMIX_ADDR(0x0244)
#define REG_VENCPLL_CON1    REG_APMIX_ADDR(0x0264)
#define REG_TVDPLL_CON1     REG_APMIX_ADDR(0x0274)
#define REG_VCODECPLL_CON1  REG_APMIX_ADDR(0x0294)

/* masks */
#define MASK_FRDDSX_DYS         (0xFU<<20)
#define MASK_FRDDSX_DTS         (0xFU<<16)
#define FH_FHCTLX_SRHMODE       (0x1U<<5)
#define FH_SFSTRX_BP            (0x1U<<4)
#define FH_SFSTRX_EN            (0x1U<<2)
#define FH_FRDDSX_EN            (0x1U<<1)
#define FH_FHCTLX_EN            (0x1U<<0)
#define FH_FRDDSX_DNLMT         (0xFFU<<16)
#define FH_FRDDSX_UPLMT         (0xFFU)
#define FH_FHCTLX_PLL_TGL_ORG   (0x1U<<31)
#define FH_FHCTLX_PLL_ORG       (0xFFFFFU)
#define FH_FHCTLX_PAUSE         (0x1U<<31)
#define FH_FHCTLX_PRD           (0x1U<<30)
#define FH_SFSTRX_PRD           (0x1U<<29)
#define FH_FRDDSX_PRD           (0x1U<<28)
#define FH_FHCTLX_STATE         (0xFU<<24)
#define FH_FHCTLX_PLL_CHG       (0x1U<<21)
#define FH_FHCTLX_PLL_DDS       (0xFFFFFU)


#define USER_DEFINE_SETTING_ID	(1)

#define MASK21b (0x1FFFFF)
#define BIT32   (1U<<31)

#define GPU_DVFS_LOG	0
#define FH_BUG_ON(x) \
do {    \
	if ((x)) \
		pr_err("BUGON %s:%d %s:%d\n", __func__   \
		, __LINE__, current->comm, current->pid); \
} while (0)

static DEFINE_SPINLOCK(g_fh_lock);

#define PERCENT_TO_DDSLMT(dDS, pERCENT_M10) (((dDS * pERCENT_M10) >> 5) / 100)

static unsigned int	g_initialize;

/* default VCO freq. */
#define ARMCA7PLL_DEF_FREQ      1599000
#define MEMPLL_DEF_FREQ          160000
#define MAINPLL_DEF_FREQ        1092000
#define MPLL_DEF_FREQ            208000
#define MSDCPLL_DEF_FREQ        1600000
#define MMPLL_DEF_FREQ          1092000
#define VENCPLL_DEF_FREQ        1518002
#define TVDPLL_DEF_FREQ         1782000
#define VCODECPLL_DEF_FREQ      1976000

/* keep track the status of each PLL */
static struct fh_pll_t g_fh_pll[FH_PLL_NUM] = {
	{FH_FH_DISABLE,	    FH_PLL_ENABLE, 0, ARMCA7PLL_DEF_FREQ,    0},
	{FH_FH_DISABLE,	    FH_PLL_ENABLE, 0, MPLL_DEF_FREQ,         0},
	{FH_FH_DISABLE,	    FH_PLL_ENABLE, 0, MAINPLL_DEF_FREQ,      0},
	{FH_FH_DISABLE,     FH_PLL_ENABLE, 0, MEMPLL_DEF_FREQ,       0},
	{FH_FH_DISABLE,	    FH_PLL_ENABLE, 0, MSDCPLL_DEF_FREQ,      0},
	{FH_FH_DISABLE,	    FH_PLL_ENABLE, 0, MMPLL_DEF_FREQ,        0},
	{FH_FH_DISABLE,	    FH_PLL_ENABLE, 0, VENCPLL_DEF_FREQ,      0},
	{FH_FH_DISABLE,	    FH_PLL_ENABLE, 0, TVDPLL_DEF_FREQ,       0},
	{FH_FH_DISABLE,	    FH_PLL_ENABLE, 0, VCODECPLL_DEF_FREQ,    0}
};

static const struct freqhopping_ssc ssc_armca7pll_setting[] = {
	{0, 0, 0, 0, 0, 0},/* Means disable */
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},/* Means User-Define */
	{ARMCA7PLL_DEF_FREQ, 0, 9, 0, 8, 0xF6000}, /* 0 ~ -8% */
	{0, 0, 0, 0, 0, 0} /* EOF */
};

static const struct freqhopping_ssc ssc_mpll_setting[] = {
	{0, 0, 0, 0, 0, 0},
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	/* {MPLL_DEF_FREQ, 0, 9, 0, 8, 0x1C000}, */ /* 0 ~ -8% */
	{MPLL_DEF_FREQ, 0, 9, 0, 5, 0x1C000},/* 0 ~ -5% */
	{0, 0, 0, 0, 0, 0}
};


static const struct freqhopping_ssc ssc_mainpll_setting[] = {
	{0, 0, 0, 0, 0, 0},
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{MAINPLL_DEF_FREQ, 0, 9, 0, 8, 0xA8000},   /* 0 ~ -8% */
	{0, 0, 0, 0, 0, 0}
};

static const struct freqhopping_ssc ssc_mempll_setting[] = {
	{0, 0, 0, 0, 0, 0},
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	/* {MEMPLL_DEF_FREQ, 0, 9, 0, 8, 0x1C000}, */ /* 0 ~ -8% */
	{MEMPLL_DEF_FREQ, 0, 9, 0, 2, 0x1C000},/* 0 ~ -2% */
	{0, 0, 0, 0, 0, 0}
};


static const struct freqhopping_ssc ssc_msdcpll_setting[] = {
	{0, 0, 0, 0, 0, 0},
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{MSDCPLL_DEF_FREQ, 0, 9, 0, 8, 0xF6276}, /* 0 ~ -8% */
	{0, 0, 0, 0, 0, 0}
};

static const struct freqhopping_ssc ssc_mmpll_setting[] = {
	{0, 0, 0, 0, 0, 0},
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{MMPLL_DEF_FREQ, 0, 9, 0, 8, 0xD8000},
	{0, 0, 0, 0, 0, 0}
};

static const struct freqhopping_ssc ssc_vencpll_setting[] = {
	{0, 0, 0, 0, 0, 0},
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{VENCPLL_DEF_FREQ, 0, 9, 0, 8, 0xE989E}, /* 0~-8% */
	{0, 0, 0, 0, 0, 0}
};

static const struct freqhopping_ssc ssc_tvdpll_setting[] = {
	{0, 0, 0, 0, 0, 0},
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{TVDPLL_DEF_FREQ, 0, 9, 0, 8, 0x112276}, /* 0~-8% */
	{0, 0, 0, 0, 0, 0}
};

static const struct freqhopping_ssc ssc_vcodecpll_setting[] = {
	{0, 0, 0, 0, 0, 0},
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{VCODECPLL_DEF_FREQ, 0, 9, 0, 8, 0x130000}, /* 0~-8% */
	{0, 0, 0, 0, 0, 0}
};

static const unsigned int g_default_freq[] = {
	ARMCA7PLL_DEF_FREQ, MPLL_DEF_FREQ, MAINPLL_DEF_FREQ,
	MEMPLL_DEF_FREQ, MSDCPLL_DEF_FREQ, MMPLL_DEF_FREQ,
	VENCPLL_DEF_FREQ, TVDPLL_DEF_FREQ, VCODECPLL_DEF_FREQ
};

static const struct freqhopping_ssc *g_ssc_setting[] = {
	ssc_armca7pll_setting,
	ssc_mpll_setting,
	ssc_mainpll_setting,
	ssc_mempll_setting,
	ssc_msdcpll_setting,
	ssc_mmpll_setting,
	ssc_vencpll_setting,
	ssc_tvdpll_setting,
	ssc_vcodecpll_setting
};

static const unsigned int g_ssc_setting_size[] = {
	ARRAY_SIZE(ssc_armca7pll_setting),
	ARRAY_SIZE(ssc_mpll_setting),
	ARRAY_SIZE(ssc_mainpll_setting),
	ARRAY_SIZE(ssc_mempll_setting),
	ARRAY_SIZE(ssc_msdcpll_setting),
	ARRAY_SIZE(ssc_mmpll_setting),
	ARRAY_SIZE(ssc_vencpll_setting),
	ARRAY_SIZE(ssc_tvdpll_setting),
	ARRAY_SIZE(ssc_vcodecpll_setting)
};


static void __iomem *g_reg_dds[FH_PLL_NUM];
static void __iomem *g_reg_cfg[FH_PLL_NUM];
static void __iomem *g_reg_updnlmt[FH_PLL_NUM];
static void __iomem *g_reg_mon[FH_PLL_NUM];
static void __iomem *g_reg_dvfs[FH_PLL_NUM];
static void __iomem *g_reg_pll_con1[FH_PLL_NUM];

#define VALIDATE_PLLID(id)\
	WARN_ON(id >= FH_PLL_NUM)

#define fh_read8(reg)		readb((reg))
#define fh_read16(reg)		readw((reg))
#define fh_read32(reg)		readl((reg))
#define fh_write8(reg, val)	mt_reg_sync_writeb((val), (reg))
#define fh_write16(reg, val)	mt_reg_sync_writew((val), (reg))
#define fh_write32(reg, val)	mt_reg_sync_writel((val), (reg))

static unsigned int uffs(unsigned int mask)
{
	unsigned int shift = 0u;

	if (mask == 0u)
		return 0;

	if ((mask & 0xffffu) == 0u) {
		mask >>= 16u;
		shift += 16u;
	}

	if ((mask & 0xffu) == 0u) {
		mask >>= 8u;
		shift += 8u;
	}

	if ((mask & 0xfu) == 0u) {
		mask >>= 4u;
		shift += 4u;
	}

	if ((mask & 0x3U) == 0U) {
		mask >>= 2u;
		shift += 2u;
	}

	if ((mask & 0x1u) == 0u) {
		mask >>= 1u;
		shift += 1u;
	}

	return shift;
}

static void fh_set_field(void __iomem *fh_reg
	, unsigned int mask, unsigned int mask_val)
{
	unsigned int tv = fh_read32(fh_reg);

	tv &= ~mask;
	tv |= mask_val << uffs(mask);
	fh_write32(fh_reg, tv);
}

static void __reg_tbl_init(void)
{
	int i = 0;

	void __iomem *reg_dds[] = {
	REG_FHCTL0_DDS, REG_FHCTL1_DDS, REG_FHCTL2_DDS,
	REG_FHCTL3_DDS, REG_FHCTL4_DDS, REG_FHCTL5_DDS,
	REG_FHCTL6_DDS, REG_FHCTL7_DDS, REG_FHCTL8_DDS};

	void __iomem *reg_cfg[] = {
	REG_FHCTL0_CFG, REG_FHCTL1_CFG, REG_FHCTL2_CFG,
	REG_FHCTL3_CFG, REG_FHCTL4_CFG, REG_FHCTL5_CFG,
	REG_FHCTL6_CFG, REG_FHCTL7_CFG, REG_FHCTL8_CFG};

	void __iomem *reg_updnlmt[] = {
	REG_FHCTL0_UPDNLMT, REG_FHCTL1_UPDNLMT, REG_FHCTL2_UPDNLMT,
	REG_FHCTL3_UPDNLMT, REG_FHCTL4_UPDNLMT, REG_FHCTL5_UPDNLMT,
	REG_FHCTL6_UPDNLMT, REG_FHCTL7_UPDNLMT, REG_FHCTL8_UPDNLMT};

	void __iomem *reg_mon[] = {
	REG_FHCTL0_MON, REG_FHCTL1_MON, REG_FHCTL2_MON,
	REG_FHCTL3_MON, REG_FHCTL4_MON, REG_FHCTL5_MON,
	REG_FHCTL6_MON, REG_FHCTL7_MON, REG_FHCTL8_MON};

	void __iomem *reg_dvfs[] = {
	REG_FHCTL0_DVFS, REG_FHCTL1_DVFS, REG_FHCTL2_DVFS,
	REG_FHCTL3_DVFS, REG_FHCTL4_DVFS, REG_FHCTL5_DVFS,
	REG_FHCTL6_DVFS, REG_FHCTL7_DVFS, REG_FHCTL8_DVFS};

	void __iomem *reg_pll_con1[] = {
	REG_ARMCA7PLL_CON1, REG_MPLL_CON1, REG_MAINPLL_CON1,
	REG_MEMPLL_CON1, REG_MSDCPLL_CON1, REG_MMPLL_CON1,
	REG_VENCPLL_CON1, REG_TVDPLL_CON1, REG_VCODECPLL_CON1};

	FH_MSG_DEBUG("EN: %s", __func__);


	for (i = 0; i < FH_PLL_NUM; ++i) {
		g_reg_dds[i]      = reg_dds[i];
		g_reg_cfg[i]      = reg_cfg[i];
		g_reg_updnlmt[i]  = reg_updnlmt[i];
		g_reg_mon[i]      = reg_mon[i];
		g_reg_dvfs[i]     = reg_dvfs[i];
		g_reg_pll_con1[i] = reg_pll_con1[i];
	}
}

/* Device Tree Initialize */
static int __reg_base_addr_init(void)
{
	struct device_node *fhctl_node;
	struct device_node *apmixed_node;
	struct device_node *ddrphy_node;

	/* Init FHCTL base address */
	fhctl_node = of_find_compatible_node(NULL, NULL
					     , "mediatek,mt8163-fhctl");
	if (!fhctl_node) {
		FH_MSG_DEBUG(" Error, Cannot find FHCTL device tree node");
		return -1;
	}

	g_fhctl_base = of_iomap(fhctl_node, 0);
	if (!g_fhctl_base) {
		FH_MSG_DEBUG("Error, FHCTL iomap failed");
		return -1;
	}

	/* Init APMIXED base address */
	apmixed_node = of_find_compatible_node(NULL, NULL
					       , "mediatek,mt8163-apmixedsys");
	if (!fhctl_node) {
		FH_MSG_DEBUG(" Error, Cannot find APMIXED device tree node");
		return -1;
	}

	g_apmixed_base = of_iomap(apmixed_node, 0);
	if (!g_apmixed_base) {
		FH_MSG_DEBUG("Error, APMIXED iomap failed");
		return -1;
	}

	/* Init DDRPHY base address */
	ddrphy_node = of_find_compatible_node(NULL, NULL
					      , "mediatek,mt8163-ddrphy");
	if (!fhctl_node) {
		FH_MSG_DEBUG(" Error, Cannot find FHCTL device tree node");
		return -1;
	}

	g_ddrphy_base = of_iomap(ddrphy_node, 0);
	if (!g_ddrphy_base) {
		FH_MSG_DEBUG("Error, FHCTL DDRPHY failed");
		return -1;
	}

	__reg_tbl_init();

	return 0;
}

static int mt_fh_hal_init(void)
{
	int i = 0;
	int	ret = 0;
	unsigned long flags = 0;


	FH_MSG_DEBUG("EN: %s", __func__);

	if (g_initialize == 1)
		return 0;

	/* Init relevant register base address by device tree */
	ret = __reg_base_addr_init();
	if (ret)
		return ret;


	for (i = 0; i < FH_PLL_NUM; ++i) {
		unsigned int mask = 1<<i;

		spin_lock_irqsave(&g_fh_lock, flags);

		/* TODO: clock should be turned on only when FH is needed */
		/* Turn on all clock */
		fh_set_field(REG_FHCTL_CLK_CON, mask, 1);

		/* Release software-reset to reset */
		fh_set_field(REG_FHCTL_RST_CON, mask, 0);
		fh_set_field(REG_FHCTL_RST_CON, mask, 1);

		g_fh_pll[i].setting_id = 0;
		/* No SSC and FH enabled */
		fh_write32(g_reg_cfg[i], 0x00000000);
		/* clear all the settings */
		fh_write32(g_reg_updnlmt[i], 0x00000000);
		/* clear all the settings */
		fh_write32(g_reg_dds[i], 0x00000000);

		spin_unlock_irqrestore(&g_fh_lock, flags);
	}

	g_initialize = 1;

	return 0;
}

static void mt_fh_hal_default_conf(void)
{
	FH_MSG_DEBUG("%s", __func__);

	#if 0
	freqhopping_config(FH_ARMCA7_PLLID
	, g_default_freq[FH_ARMCA7_PLLID], false);
	freqhopping_config(FH_M_PLLID
	, g_default_freq[FH_M_PLLID], true);
	freqhopping_config(FH_MAIN_PLLID
	, g_default_freq[FH_MAIN_PLLID], true);
	freqhopping_config(FH_MEM_PLLID
	, g_default_freq[FH_MEM_PLLID], true);
	freqhopping_config(FH_MSDC_PLLID
	, g_default_freq[FH_MSDC_PLLID], true);
	freqhopping_config(FH_MM_PLLID
	, g_default_freq[FH_MM_PLLID], true);
	freqhopping_config(FH_VENC_PLLID
	, g_default_freq[FH_VENC_PLLID], true);
	freqhopping_config(FH_TVD_PLLID
	, g_default_freq[FH_TVD_PLLID], true);
	freqhopping_config(FH_VCODEC_PLLID
	, g_default_freq[FH_VCODEC_PLLID], true);
	#endif

	#ifdef ENABLE_DVT_LTE_SIDEBAND_SIGNAL_TESTCASE
	fh_set_field(REG_FHCTL1_DDS, (0x1FFFFFU<<0), 0X100000);
	fh_set_field(REG_FHCTL1_DDS, (0x1U<<32), 1);

	fh_set_field(REG_FHCTL_HP_EN, (0x1U<<31), 1);
	fh_set_field(REG_FHCTL_HP_EN, (0x1U<<1), 0x1);  /* /< MPLL */

	fh_set_field(REG_FHCTL1_CFG, (0x1U<<0), 1);     /* /< Enable */
	fh_set_field(REG_FHCTL1_CFG, (0x1U<<3), 1);     /* /< DYSSC Enable */

	fh_set_field(REG_FHCTL_DSSC_CFG, (0x1U<<3), 0);
	fh_set_field(REG_FHCTL_DSSC_CFG, (0x1U<<19), 0);

	/* fh_set_field(REG_FHCTL_DSSC3_CON, (0x1U<<1), 1); */
	#endif
}

/* freq is in KHz, return at which number of entry in mt_ssc_xxx_setting[] */
static noinline int __freq_to_index(enum FH_PLL_ID pll_id, int freq)
{
	unsigned int retVal = 0;
	/* 0 is disable, 1 is user defines, so start from 2 */
	unsigned int i = 2;
	const unsigned int size = g_ssc_setting_size[pll_id];

	while (i < size) {
		if (freq == g_ssc_setting[pll_id][i].freq) {
			retVal = i;
			break;
		}
		++i;
	}

	return retVal;
}

static void fh_switch2fhctl(enum FH_PLL_ID pll_id, int i_control)
{
	unsigned int mask = 0;

	VALIDATE_PLLID(pll_id);

	mask = 0x1U<<pll_id;

	/* FIXME: clock should be turned on/off at entry functions */
	/* Turn on clock */
	/* if (i_control == 1) */
	/* fh_set_field(REG_FHCTL_CLK_CON, mask, i_control); */

	/* Release software reset */
	/* fh_set_field(REG_FHCTL_RST_CON, mask, 0); */

	/* Switch to FHCTL_CORE controller */
	fh_set_field(REG_FHCTL_HP_EN, mask, i_control);

	/* Turn off clock */
	/* if (i_control == 0) */
	/* fh_set_field(REG_FHCTL_CLK_CON, mask, i_control); */
}

static void fh_sync_ncpo_to_fhctl_dds(enum FH_PLL_ID pll_id)
{
	unsigned int pll_con1;

	pll_con1 = fh_read32(g_reg_pll_con1[pll_id]);

	VALIDATE_PLLID(pll_id);

	if (pll_id == FH_MEM_PLLID) {
		pll_con1
		= ((fh_read32(g_reg_pll_con1[pll_id]) >> 10) & MASK21b)|BIT32;
		fh_write32(g_reg_dds[pll_id], pll_con1);
	} else {
		pll_con1 = (fh_read32(g_reg_pll_con1[pll_id])&MASK21b)|BIT32;
		fh_write32(g_reg_dds[pll_id], pll_con1);
	}
}

static void __enable_ssc(unsigned int pll_id
	, const struct freqhopping_ssc *setting)
{
	unsigned long flags = 0;
	void __iomem *reg_cfg = g_reg_cfg[pll_id];
	void __iomem *reg_updnlmt = g_reg_updnlmt[pll_id];
	void __iomem *reg_dds = g_reg_dds[pll_id];

	FH_MSG_DEBUG("%s: %x~%x df:%d dt:%d dds:%x",
		__func__, setting->lowbnd, setting->upbnd
		, setting->df, setting->dt, setting->dds);

	mb(); /* mb */

	g_fh_pll[pll_id].fh_status = FH_FH_ENABLE_SSC;

	local_irq_save(flags);

	/* Set the relative parameter registers (dt/df/upbnd/downbnd) */
	fh_set_field(reg_cfg, MASK_FRDDSX_DYS, setting->df);
	fh_set_field(reg_cfg, MASK_FRDDSX_DTS, setting->dt);

	fh_sync_ncpo_to_fhctl_dds(pll_id);

	/* TODO: Not setting upper due to they are all 0? */
	fh_write32(reg_updnlmt,
		(PERCENT_TO_DDSLMT((fh_read32(reg_dds)&MASK21b)
				   , setting->lowbnd) << 16));

	/* Switch to FHCTL */
	fh_switch2fhctl(pll_id, 1);
	mb(); /* mb */

	/* Enable SSC */
	fh_set_field(reg_cfg, FH_FRDDSX_EN, 1);
	/* Enable Hopping control */
	fh_set_field(reg_cfg, FH_FHCTLX_EN, 1);

	local_irq_restore(flags);
}

static void __disable_ssc(unsigned int pll_id
	, const struct freqhopping_ssc *ssc_setting)
{
	unsigned long	flags = 0;
	void __iomem *reg_cfg = g_reg_cfg[pll_id];

	FH_MSG_DEBUG("Calling %s", __func__);

	local_irq_save(flags);

	/* Set the relative registers */
	fh_set_field(reg_cfg, FH_FRDDSX_EN, 0);
	fh_set_field(reg_cfg, FH_FHCTLX_EN, 0);
	mb(); /* mb */
	fh_switch2fhctl(pll_id, 0);
	g_fh_pll[pll_id].fh_status = FH_FH_DISABLE;
	local_irq_restore(flags);
	mb(); /* mb */
}

static int __freqhopping_ctrl(struct freqhopping_ioctl *fh_ctl, bool enable)
{
	const struct freqhopping_ssc *pSSC_setting = NULL;
	unsigned int    ssc_setting_id = 0;
	int retVal = 1;
	struct fh_pll_t *pfh_pll = NULL;

	FH_MSG("%s for pll %d", __func__, fh_ctl->pll_id);

	/* Check the out of range of frequency hopping PLL ID */
	VALIDATE_PLLID(fh_ctl->pll_id);

	pfh_pll = &g_fh_pll[fh_ctl->pll_id];

	pfh_pll->curr_freq = g_default_freq[fh_ctl->pll_id];

	if ((enable == true) && (pfh_pll->fh_status == FH_FH_ENABLE_SSC)) {
		__disable_ssc(fh_ctl->pll_id, pSSC_setting);
	} else if ((enable == false)
		   && (pfh_pll->fh_status == FH_FH_DISABLE)) {
		retVal = 0;
		goto Exit;
	}

	/* enable freq. hopping @ fh_ctl->pll_id */
	if (enable == true) {
		if (pfh_pll->pll_status == FH_PLL_DISABLE) {
			pfh_pll->fh_status = FH_FH_ENABLE_SSC;
			retVal = 0;
			goto Exit;
		} else {
			if (pfh_pll->curr_freq != 0) {
				ssc_setting_id = pfh_pll->setting_id =
					__freq_to_index(fh_ctl->pll_id
							, pfh_pll->curr_freq);
			} else {
				ssc_setting_id = 0;
			}

			if (ssc_setting_id == 0) {
				FH_MSG("!! No corresponding setting found !!");

				/* just disable FH & exit */
				__disable_ssc(fh_ctl->pll_id, pSSC_setting);
				goto Exit;
			}

			pSSC_setting
			= &g_ssc_setting[fh_ctl->pll_id][ssc_setting_id];

			if (pSSC_setting == NULL) {
				FH_MSG("SSC_setting is NULL!");

				/* disable FH & exit */
				__disable_ssc(fh_ctl->pll_id, pSSC_setting);
				goto Exit;
			}

			__enable_ssc(fh_ctl->pll_id, pSSC_setting);
			retVal = 0;
		}
	} else { /* disable req. hopping @ fh_ctl->pll_id */
		__disable_ssc(fh_ctl->pll_id, pSSC_setting);
		retVal = 0;
	}

Exit:
	return retVal;
}

static void wait_dds_stable(
	unsigned int target_dds,
	void __iomem *reg_mon,
	unsigned int wait_count)
{
	unsigned int fh_dds = 0;
	unsigned int i = 0;

	fh_dds = fh_read32(reg_mon) & MASK21b;
	while ((target_dds != fh_dds) && (i < wait_count)) {
		udelay(10);
		fh_dds = (fh_read32(reg_mon)) & MASK21b;
		++i;
	}
	FH_MSG("target_dds = %d, fh_dds = %d, i = %d", target_dds, fh_dds, i);
}

static int mt_fh_hal_dvfs(enum FH_PLL_ID pll_id, unsigned int dds_value)
{
	unsigned long	flags = 0;

	FH_MSG_DEBUG("%s for pll %d:", __func__, pll_id);

	VALIDATE_PLLID(pll_id);

	local_irq_save(flags);

	/* 1. sync ncpo to DDS of FHCTL */
	fh_sync_ncpo_to_fhctl_dds(pll_id);

	/* FH_MSG("1. sync ncpo to DDS of FHCTL"); */
	FH_MSG_DEBUG("FHCTL%d_DDS: 0x%08x", pll_id,
		(fh_read32(g_reg_dds[pll_id])&MASK21b));

	/* 2. enable DVFS and Hopping control */
	{
		void __iomem *reg_cfg = g_reg_cfg[pll_id];

		/* enable dvfs mode */
		fh_set_field(reg_cfg, FH_SFSTRX_EN, 1);
		/* enable hopping control */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 1);
	}

	/* for slope setting. */
	/* TODO: Does this need to be changed? */
	fh_write32(REG_FHCTL_SLOPE0, 0x6003c97);

	/* FH_MSG("2. enable DVFS and Hopping control"); */

	/* 3. switch to hopping control */
	fh_switch2fhctl(pll_id, 1);
	mb(); /* mb */

	/* FH_MSG("3. switch to hopping control"); */

	/* 4. set DFS DDS */
	{
		void __iomem *dvfs_req = g_reg_dvfs[pll_id];

		fh_write32(dvfs_req, (dds_value)|(BIT32));  /* set dds */

		/* FH_MSG("4. set DFS DDS"); */
		FH_MSG_DEBUG("FHCTL%d_DDS: 0x%08x"
		     , pll_id, (fh_read32(dvfs_req)&MASK21b));
		FH_MSG_DEBUG("FHCTL%d_DVFS: 0x%08x"
			      , pll_id, (fh_read32(dvfs_req)&MASK21b));
	}

	/* 4.1 ensure jump to target DDS */
	wait_dds_stable(dds_value, g_reg_mon[pll_id], 100);
	/* FH_MSG("4.1 ensure jump to target DDS"); */

	/* 5. write back to ncpo */
	/* FH_MSG("5. write back to ncpo"); */
	{
		void __iomem *reg_dvfs = 0;
		void __iomem *reg_pll_con1 = 0;

		if (pll_id == FH_MEM_PLLID) {
			reg_pll_con1 = g_reg_pll_con1[pll_id];
			reg_dvfs = g_reg_dvfs[pll_id];
				FH_MSG_DEBUG("MEMPLL_CON1: 0x%08x"
					     , (fh_read32(reg_pll_con1)));

			fh_write32(reg_pll_con1,
				((fh_read32(g_reg_mon[pll_id])&MASK21b) << 10)
				|(fh_read32(reg_pll_con1)&0x80000000)|(BIT32));
				FH_MSG_DEBUG("MEMPLL_CON1: 0x%08x"
					     , (fh_read32(reg_pll_con1)));
		} else {
			reg_pll_con1 = g_reg_pll_con1[pll_id];
			reg_dvfs = g_reg_dvfs[pll_id];
				FH_MSG_DEBUG("PLL_CON1: 0x%08x"
					, (fh_read32(reg_pll_con1)&MASK21b));

			fh_write32(reg_pll_con1,
			(fh_read32(g_reg_mon[pll_id])&MASK21b)
				|(fh_read32(reg_pll_con1)&0xFFE00000)|(BIT32));
				FH_MSG_DEBUG("PLL_CON1: 0x%08x"
					, (fh_read32(reg_pll_con1)&MASK21b));
		}
	}

	/* 6. switch to register control */
	fh_switch2fhctl(pll_id, 0);
	mb(); /* mb */

	/* FH_MSG("6. switch to register control"); */

	local_irq_restore(flags);
	return 0;
}

/* armpll dfs mdoe */
static int mt_fh_hal_dfs_armpll(unsigned int pll, unsigned int dds)
{
	unsigned long flags = 0;
	void __iomem *reg_cfg = 0;

	if (g_initialize == 0) {
		FH_MSG("(Warning) %s FHCTL isn't ready.", __func__);
		return -1;
	}

	FH_MSG("%s for pll %d dds %d", __func__, pll, dds);

	switch (pll) {
	case FH_ARMCA7_PLLID:
		reg_cfg = g_reg_cfg[pll];
		FH_MSG("(PLL_CON1): 0x%x"
		       , (fh_read32(g_reg_pll_con1[pll])&MASK21b));
		break;
	default:
		WARN_ON(1);
		return 1;
	};

	/* TODO: provelock issue spin_lock(&g_fh_lock); */
	spin_lock_irqsave(&g_fh_lock, flags);

	fh_set_field(reg_cfg, FH_FRDDSX_EN, 0);  /* disable SSC mode */
	fh_set_field(reg_cfg, FH_SFSTRX_EN, 0);  /* disable dvfs mode */
	fh_set_field(reg_cfg, FH_FHCTLX_EN, 0);  /* disable hopping control */

	mt_fh_hal_dvfs(pll, dds);

	fh_set_field(reg_cfg, FH_FRDDSX_EN, 0);  /* disable SSC mode */
	fh_set_field(reg_cfg, FH_SFSTRX_EN, 0);  /* disable dvfs mode */
	fh_set_field(reg_cfg, FH_FHCTLX_EN, 0);  /* disable hopping control */

	spin_unlock_irqrestore(&g_fh_lock, flags);

	return 0;
}

static int mt_fh_hal_dfs_mmpll(unsigned int target_dds)  /* mmpll dfs mode */
{
	unsigned long	flags = 0;
	const unsigned int  pll_id  = FH_MM_PLLID;
	void __iomem *reg_cfg = g_reg_cfg[pll_id];

	if (g_initialize == 0) {
		FH_MSG("(Warning) %s FHCTL isn't ready. ", __func__);
		return -1;
	}

	#if GPU_DVFS_LOG
	FH_MSG("%s, current dds(MMPLL_CON1): 0x%x, target dds %d",
		__func__, (fh_read32(g_reg_pll_con1[pll_id])&MASK21b),
		target_dds);
	#endif

	spin_lock_irqsave(&g_fh_lock, flags);

	if (g_fh_pll[pll_id].fh_status == FH_FH_ENABLE_SSC) {
		unsigned int	pll_dds = 0;
		unsigned int	fh_dds  = 0;

		/* only when SSC is enable, turn off MEMPLL hopping */
		/* disable SSC mode */
		fh_set_field(reg_cfg, FH_FRDDSX_EN, 0);
		/* disable dvfs mode */
		fh_set_field(reg_cfg, FH_SFSTRX_EN, 0);
		/* disable hopping control */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 0);

		pll_dds =  (fh_read32(g_reg_dds[pll_id])) & MASK21b;
		fh_dds	=  (fh_read32(g_reg_mon[pll_id])) & MASK21b;

		FH_MSG(">p:f< %x:%x", pll_dds, fh_dds);

	wait_dds_stable(pll_dds, g_reg_mon[pll_id], 100);
	}

	#if 0
	target_dds = (target_freq / 1000) * 8192 / 13;
	#endif

	#if GPU_DVFS_LOG
	FH_MSG("target dds: 0x%x", target_dds);
	#endif
	mt_fh_hal_dvfs(pll_id, target_dds);

	if (g_fh_pll[pll_id].fh_status == FH_FH_ENABLE_SSC) {
		const struct freqhopping_ssc *p_setting
			= &ssc_mmpll_setting[2];

		/* disable SSC mode */
		fh_set_field(reg_cfg, FH_FRDDSX_EN, 0);
		/* disable dvfs mode */
		fh_set_field(reg_cfg, FH_SFSTRX_EN, 0);
		/* disable hopping control */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 0);

		fh_sync_ncpo_to_fhctl_dds(pll_id);

		FH_MSG("Enable mmpll SSC mode");
		FH_MSG("DDS: 0x%08x", (fh_read32(g_reg_dds[pll_id])&MASK21b));

		fh_set_field(reg_cfg, MASK_FRDDSX_DYS, p_setting->df);
		fh_set_field(reg_cfg, MASK_FRDDSX_DTS, p_setting->dt);

		fh_write32(g_reg_updnlmt[pll_id],
		(PERCENT_TO_DDSLMT((fh_read32(g_reg_dds[pll_id])&MASK21b)
		, p_setting->lowbnd) << 16));
		FH_MSG("UPDNLMT: 0x%08x", fh_read32(g_reg_updnlmt[pll_id]));

		fh_switch2fhctl(pll_id, 1);

		/* enable SSC mode */
		fh_set_field(reg_cfg, FH_FRDDSX_EN, 1);
		/* enable hopping control */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 1);

		FH_MSG("CFG: 0x%08x", fh_read32(reg_cfg));

	}
	spin_unlock_irqrestore(&g_fh_lock, flags);

	return 0;
}

int mt_hal_dfs_general_pll(unsigned int pll_id, unsigned int target_dds)
{
	switch (pll_id) {
	case FH_ARMCA7_PLLID:
		mt_fh_hal_dfs_armpll(pll_id, target_dds);
		break;

	case FH_MM_PLLID:
		mt_fh_hal_dfs_mmpll(target_dds);
		break;

	default:
		WARN_ON(1);
		return 1;
	};
	return 0;
}

static void mt_fh_hal_popod_save(void)
{
	const unsigned int  pll_id  = FH_MAIN_PLLID;

	FH_MSG_DEBUG("EN: %s", __func__);

	/* disable maipll SSC mode */
	if (g_fh_pll[pll_id].fh_status == FH_FH_ENABLE_SSC) {
		unsigned int	fh_dds = 0;
		unsigned int	pll_dds = 0;
		void __iomem *reg_cfg = g_reg_cfg[pll_id];

		/* only when SSC is enable, turn off MAINPLL hopping */
		/* disable SSC mode */
		fh_set_field(reg_cfg, FH_FRDDSX_EN, 0);
		/* disable dvfs mode */
		fh_set_field(reg_cfg, FH_SFSTRX_EN, 0);
		/* disable hopping control */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 0);

		pll_dds =  (fh_read32(g_reg_dds[pll_id])) & MASK21b;
		fh_dds	=  (fh_read32(g_reg_mon[pll_id])) & MASK21b;

		FH_MSG("Org pll_dds:%x fh_dds:%x", pll_dds, fh_dds);

		wait_dds_stable(pll_dds, g_reg_mon[pll_id], 100);

		/* write back to ncpo, only for MAINPLL.
		 * Don't need to add MEMPLL handle.
		 */
		fh_write32(g_reg_pll_con1[pll_id],
			(fh_read32(g_reg_dds[pll_id])&MASK21b)
			|(fh_read32(REG_MAINPLL_CON1)&0xFFE00000)|(BIT32));
			FH_MSG("REG_MAINPLL_CON1: 0x%08x"
			       , (fh_read32(g_reg_pll_con1[pll_id])&MASK21b));

		/* switch to register control */
		fh_switch2fhctl(pll_id, 0);

		mb(); /* mb */
	}
}

static void mt_fh_hal_popod_restore(void)
{
	const unsigned int pll_id = FH_MAIN_PLLID;

	FH_MSG_DEBUG("EN: %s", __func__);

	/* enable maipll SSC mode */
	if (g_fh_pll[pll_id].fh_status == FH_FH_ENABLE_SSC) {
		const struct freqhopping_ssc *p_setting
			= &ssc_mainpll_setting[2];
		void __iomem *reg_cfg = g_reg_cfg[pll_id];

		/* disable SSC mode */
		fh_set_field(reg_cfg, FH_FRDDSX_EN, 0);
		/* disable dvfs mode */
		fh_set_field(reg_cfg, FH_SFSTRX_EN, 0);
		/* disable hopping control */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 0);

		fh_sync_ncpo_to_fhctl_dds(pll_id);

		FH_MSG("Enable mainpll SSC mode");
		FH_MSG("sync ncpo to DDS of FHCTL");
		FH_MSG("FHCTL1_DDS: 0x%08x"
		       , (fh_read32(g_reg_dds[pll_id])&MASK21b));

		fh_set_field(reg_cfg, MASK_FRDDSX_DYS, p_setting->df);
		fh_set_field(reg_cfg, MASK_FRDDSX_DTS, p_setting->dt);

		fh_write32(g_reg_updnlmt[pll_id]
			, (PERCENT_TO_DDSLMT(
			(fh_read32(g_reg_dds[pll_id])&MASK21b)
			, p_setting->lowbnd) << 16));
		FH_MSG("REG_FHCTL2_UPDNLMT: 0x%08x"
		       , fh_read32(g_reg_updnlmt[pll_id]));

		fh_switch2fhctl(pll_id, 1);

		/* enable SSC mode */
		fh_set_field(reg_cfg, FH_FRDDSX_EN, 1);
		/* enable hopping control */
		fh_set_field(reg_cfg, FH_FHCTLX_EN, 1);

		FH_MSG("REG_FHCTL2_CFG: 0x%08x", fh_read32(reg_cfg));
	}
}


static struct mt_fh_hal_driver g_fh_hal_drv = {
	.fh_pll                 = g_fh_pll,
	.pll_cnt                = FH_PLL_NUM,

	.mt_fh_hal_init         = mt_fh_hal_init,
	.mt_fh_hal_default_conf = mt_fh_hal_default_conf,
	.mt_fh_hal_ctrl         = __freqhopping_ctrl,
	.mt_dfs_general_pll     = mt_hal_dfs_general_pll,

	.mt_fh_popod_restore    = mt_fh_hal_popod_restore,
	.mt_fh_popod_save	= mt_fh_hal_popod_save,
};

struct mt_fh_hal_driver *mt_get_fh_hal_drv(void)
{
	return &g_fh_hal_drv;
}



/* TODO: module_exit(cpufreq_exit); */
