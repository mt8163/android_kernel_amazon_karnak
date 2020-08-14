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

#include <linux/clk-provider.h>
#include <linux/io.h>

#include "clkdbg.h"

#define ALL_CLK_ON		0
#define DUMP_INIT_STATE		0

/*
 * clkdbg dump_regs
 */

enum {
	topckgen,
	infrasys,
	scpsys,
	apmixed,
	audiosys,
	mfgsys,
	mmsys,
	imgsys,
	vdecsys,
	vencsys,
};

#define REGBASE_V(_phys, _id_name) { .phys = _phys, .name = #_id_name }

/*
 * checkpatch.pl ERROR:COMPLEX_MACRO
 *
 * #define REGBASE(_phys, _id_name) [_id_name] = REGBASE_V(_phys, _id_name)
 */

static struct regbase rb[] = {
	[topckgen] = REGBASE_V(0x10000000, topckgen),
	[infrasys] = REGBASE_V(0x10001000, infrasys),
	[scpsys]   = REGBASE_V(0x10006000, scpsys),
	[apmixed]  = REGBASE_V(0x1000c000, apmixed),
	[audiosys] = REGBASE_V(0x11220000, audiosys),
	[mfgsys]   = REGBASE_V(0x13000000, mfgsys),
	[mmsys]    = REGBASE_V(0x14000000, mmsys),
	[imgsys]   = REGBASE_V(0x15000000, imgsys),
	[vdecsys]  = REGBASE_V(0x16000000, vdecsys),
	[vencsys]  = REGBASE_V(0x17000000, vencsys),
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	REGNAME(topckgen, 0x040, CLK_CFG_0),
	REGNAME(topckgen, 0x050, CLK_CFG_1),
	REGNAME(topckgen, 0x060, CLK_CFG_2),
	REGNAME(topckgen, 0x070, CLK_CFG_3),
	REGNAME(topckgen, 0x080, CLK_CFG_4),
	REGNAME(topckgen, 0x090, CLK_CFG_5),
	REGNAME(topckgen, 0x0a0, CLK_CFG_6),
	REGNAME(topckgen, 0x0b0, CLK_CFG_7),
	REGNAME(topckgen, 0x0c0, CLK_CFG_8),
	REGNAME(topckgen, 0x104, CLK_MISC_CFG_0),
	REGNAME(topckgen, 0x10c, CLK_DBG_CFG),
	REGNAME(topckgen, 0x220, CLK26CALI_0),
	REGNAME(topckgen, 0x224, CLK26CALI_1),
	REGNAME(infrasys, 0x010, INFRA_TOPCKGEN_DCMCTL),
	REGNAME(infrasys, 0x080, INFRA0_PDN_SET),
	REGNAME(infrasys, 0x084, INFRA0_PDN_CLR),
	REGNAME(infrasys, 0x088, INFRA1_PDN_SET),
	REGNAME(infrasys, 0x08c, INFRA1_PDN_CLR),
	REGNAME(infrasys, 0x090, INFRA0_PDN_STA),
	REGNAME(infrasys, 0x094, INFRA1_PDN_STA),
	REGNAME(infrasys, 0x220, TOPAXI_PROT_EN),
	REGNAME(infrasys, 0x228, TOPAXI_PROT_STA1),
	REGNAME(scpsys, 0x210, SPM_VDE_PWR_CON),
	REGNAME(scpsys, 0x214, SPM_MFG_PWR_CON),
	REGNAME(scpsys, 0x230, SPM_VEN_PWR_CON),
	REGNAME(scpsys, 0x238, SPM_ISP_PWR_CON),
	REGNAME(scpsys, 0x23c, SPM_DIS_PWR_CON),
	REGNAME(scpsys, 0x280, SPM_CONN_PWR_CON),
	REGNAME(scpsys, 0x298, SPM_VEN2_PWR_CON),
	REGNAME(scpsys, 0x29c, SPM_AUDIO_PWR_CON),
	REGNAME(scpsys, 0x2c0, SPM_MFG_2D_PWR_CON),
	REGNAME(scpsys, 0x2c4, SPM_MFG_ASYNC_PWR_CON),
	REGNAME(scpsys, 0x2cc, SPM_USB_PWR_CON),
	REGNAME(scpsys, 0x60c, SPM_PWR_STATUS),
	REGNAME(scpsys, 0x610, SPM_PWR_STATUS_2ND),
	REGNAME(apmixed, 0x00c, AP_PLL_CON3),
	REGNAME(apmixed, 0x010, AP_PLL_CON4),
	REGNAME(apmixed, 0x210, ARMPLL_CON0),
	REGNAME(apmixed, 0x214, ARMPLL_CON1),
	REGNAME(apmixed, 0x21c, ARMPLL_PWR_CON0),
	REGNAME(apmixed, 0x220, MAINPLL_CON0),
	REGNAME(apmixed, 0x224, MAINPLL_CON1),
	REGNAME(apmixed, 0x22c, MAINPLL_PWR_CON0),
	REGNAME(apmixed, 0x230, UNIVPLL_CON0),
	REGNAME(apmixed, 0x234, UNIVPLL_CON1),
	REGNAME(apmixed, 0x23c, UNIVPLL_PWR_CON0),
	REGNAME(apmixed, 0x240, MMPLL_CON0),
	REGNAME(apmixed, 0x244, MMPLL_CON1),
	REGNAME(apmixed, 0x24c, MMPLL_PWR_CON0),
	REGNAME(apmixed, 0x250, MSDCPLL_CON0),
	REGNAME(apmixed, 0x254, MSDCPLL_CON1),
	REGNAME(apmixed, 0x25c, MSDCPLL_PWR_CON0),
	REGNAME(apmixed, 0x260, VENCPLL_CON0),
	REGNAME(apmixed, 0x264, VENCPLL_CON1),
	REGNAME(apmixed, 0x26c, VENCPLL_PWR_CON0),
	REGNAME(apmixed, 0x270, TVDPLL_CON0),
	REGNAME(apmixed, 0x274, TVDPLL_CON1),
	REGNAME(apmixed, 0x27c, TVDPLL_PWR_CON0),
	REGNAME(apmixed, 0x280, MPLL_CON0),
	REGNAME(apmixed, 0x284, MPLL_CON1),
	REGNAME(apmixed, 0x28c, MPLL_PWR_CON0),
	REGNAME(apmixed, 0x2a0, AUD1PLL_CON0),
	REGNAME(apmixed, 0x2a4, AUD1PLL_CON1),
	REGNAME(apmixed, 0x2ac, AUD1PLL_PWR_CON0),
	REGNAME(apmixed, 0x2b4, AUD2PLL_CON0),
	REGNAME(apmixed, 0x2b8, AUD2PLL_CON1),
	REGNAME(apmixed, 0x2b4, AUD2PLL_PWR_CON0),
	REGNAME(apmixed, 0x2c0, LVDSPLL_CON0),
	REGNAME(apmixed, 0x2c4, LVDSPLL_CON1),
	REGNAME(apmixed, 0x2cc, LVDSPLL_PWR_CON0),
	REGNAME(apmixed, 0xf00, PLL_HP_CON0),
	REGNAME(audiosys, 0x000, AUDIO_TOP_CON0),
	REGNAME(mfgsys, 0x000, MFG_CG_CON),
	REGNAME(mfgsys, 0x004, MFG_CG_SET),
	REGNAME(mfgsys, 0x008, MFG_CG_CLR),
	REGNAME(mmsys, 0x100, MMSYS_CG_CON0),
	REGNAME(mmsys, 0x104, MMSYS_CG_SET0),
	REGNAME(mmsys, 0x108, MMSYS_CG_CLR0),
	REGNAME(mmsys, 0x110, MMSYS_CG_CON1),
	REGNAME(mmsys, 0x114, MMSYS_CG_SET1),
	REGNAME(mmsys, 0x118, MMSYS_CG_CLR1),
	REGNAME(imgsys, 0x000, IMG_CG_CON),
	REGNAME(imgsys, 0x004, IMG_CG_SET),
	REGNAME(imgsys, 0x008, IMG_CG_CLR),
	REGNAME(vdecsys, 0x000, VDEC_CKEN_SET),
	REGNAME(vdecsys, 0x004, VDEC_CKEN_CLR),
	REGNAME(vdecsys, 0x008, LARB_CKEN_SET),
	REGNAME(vdecsys, 0x00c, LARB_CKEN_CLR),
	REGNAME(vencsys, 0x000, VENC_CG_CON),
	REGNAME(vencsys, 0x004, VENC_CG_SET),
	REGNAME(vencsys, 0x008, VENC_CG_CLR),
	{}
};

static const struct regname *get_all_regnames(void)
{
	return rn;
}

static void __init init_regbase(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rb); i++)
		rb[i].virt = ioremap(rb[i].phys, PAGE_SIZE);
}

/*
 * clkdbg fmeter
 */

#include <linux/delay.h>

#ifndef GENMASK
#define GENMASK(h, l)	(((1U << ((h) - (l) + 1)) - 1) << (l))
#endif

#define ALT_BITS(o, h, l, v) \
	(((o) & ~GENMASK(h, l)) | (((v) << (l)) & GENMASK(h, l)))

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */
#define clk_writel_mask(addr, h, l, v)	\
	clk_writel(addr, (clk_readl(addr) & ~GENMASK(h, l)) | ((v) << (l)))

#define ABS_DIFF(a, b)	((a) > (b) ? (a) - (b) : (b) - (a))

#define CLK_MISC_CFG_0	(rb[topckgen].virt + 0x104)
#define CLK_DBG_CFG	(rb[topckgen].virt + 0x10c)
#define CLK26CALI_0	(rb[topckgen].virt + 0x220)
#define CLK26CALI_1	(rb[topckgen].virt + 0x224)
#define PLL_HP_CON0	(rb[apmixed].virt + 0xf00)

enum FMETER_TYPE {
	FT_NULL,
	ABIST,
	CKGEN
};

#define FMCLK(_t, _i, _n) { .type = _t, .id = _i, .name = _n }

static const struct fmeter_clk fclks[] = {
	FMCLK(ABIST,  2, "AD_ARMCA7PLL_754M_CORE_CK"),
	FMCLK(ABIST,  3, "AD_OSC_CK"),
	FMCLK(ABIST,  4, "AD_MAIN_H546M_CK"),
	FMCLK(ABIST,  5, "AD_MAIN_H364M_CK"),
	FMCLK(ABIST,  6, "AD_MAIN_H218P4M_CK"),
	FMCLK(ABIST,  7, "AD_MAIN_H156M_CK"),
	FMCLK(ABIST,  8, "AD_UNIV_178P3M_CK"),
	FMCLK(ABIST,  9, "AD_UNIV_48M_CK"),
	FMCLK(ABIST, 10, "AD_UNIV_624M_CK"),
	FMCLK(ABIST, 11, "AD_UNIV_416M_CK"),
	FMCLK(ABIST, 12, "AD_UNIV_249P6M_CK"),
	FMCLK(ABIST, 13, "AD_APLL1_180P6336M_CK"),
	FMCLK(ABIST, 14, "AD_APLL2_196P608M_CK"),
	FMCLK(ABIST, 15, "AD_LTEPLL_FS26M_CK"),
	FMCLK(ABIST, 16, "rtc32k_ck_i"),
	FMCLK(ABIST, 17, "AD_MMPLL_700M_CK"),
	FMCLK(ABIST, 18, "AD_VENCPLL_410M_CK"),
	FMCLK(ABIST, 19, "AD_TVDPLL_594M_CK"),
	FMCLK(ABIST, 20, "AD_MPLL_208M_CK"),
	FMCLK(ABIST, 21, "AD_MSDCPLL_806M_CK"),
	FMCLK(ABIST, 22, "AD_USB_48M_CK"),
	FMCLK(ABIST, 24, "MIPI_DSI_TST_CK"),
	FMCLK(ABIST, 25, "AD_PLLGP_TST_CK"),
	FMCLK(ABIST, 26, "AD_LVDSTX_MONCLK"),
	FMCLK(ABIST, 27, "AD_DSI0_LNTC_DSICLK"),
	FMCLK(ABIST, 28, "AD_DSI0_MPPLL_TST_CK"),
	FMCLK(ABIST, 29, "AD_CSI0_DELAY_TSTCLK"),
	FMCLK(ABIST, 30, "AD_CSI1_DELAY_TSTCLK"),
	FMCLK(ABIST, 31, "AD_HDMITX_MON_CK"),
	FMCLK(ABIST, 32, "AD_ARMPLL_1508M_CK"),
	FMCLK(ABIST, 33, "AD_MAINPLL_1092M_CK"),
	FMCLK(ABIST, 34, "AD_MAINPLL_PRO_CK"),
	FMCLK(ABIST, 35, "AD_UNIVPLL_1248M_CK"),
	FMCLK(ABIST, 36, "AD_LVDSPLL_150M_CK"),
	FMCLK(ABIST, 37, "AD_LVDSPLL_ETH_CK"),
	FMCLK(ABIST, 38, "AD_SSUSB_48M_CK"),
	FMCLK(ABIST, 39, "AD_MIPI_26M_CK"),
	FMCLK(ABIST, 40, "AD_MEM_26M_CK"),
	FMCLK(ABIST, 41, "AD_MEMPLL_MONCLK"),
	FMCLK(ABIST, 42, "AD_MEMPLL2_MONCLK"),
	FMCLK(ABIST, 43, "AD_MEMPLL3_MONCLK"),
	FMCLK(ABIST, 44, "AD_MEMPLL4_MONCLK"),
	FMCLK(ABIST, 45, "AD_MEMPLL_REFCLK_BUF"),
	FMCLK(ABIST, 46, "AD_MEMPLL_FBCLK_BUF"),
	FMCLK(ABIST, 47, "AD_MEMPLL2_REFCLK_BUF"),
	FMCLK(ABIST, 48, "AD_MEMPLL2_FBCLK_BUF"),
	FMCLK(ABIST, 49, "AD_MEMPLL3_REFCLK_BUF"),
	FMCLK(ABIST, 50, "AD_MEMPLL3_FBCLK_BUF"),
	FMCLK(ABIST, 51, "AD_MEMPLL4_REFCLK_BUF"),
	FMCLK(ABIST, 52, "AD_MEMPLL4_FBCLK_BUF"),
	FMCLK(ABIST, 53, "AD_USB20_MONCLK"),
	FMCLK(ABIST, 54, "AD_USB20_MONCLK_1P"),
	FMCLK(ABIST, 55, "AD_MONREF_CK"),
	FMCLK(ABIST, 56, "AD_MONFBK_CK"),
	FMCLK(CKGEN,  1, "hd_faxi_ck"),
	FMCLK(CKGEN,  2, "hf_fddrphycfg_ck"),
	FMCLK(CKGEN,  3, "f_fpwm_ck"),
	FMCLK(CKGEN,  4, "hf_fvdec_ck"),
	FMCLK(CKGEN,  5, "hf_fmm_ck"),
	FMCLK(CKGEN,  6, "hf_fcamtg_ck"),
	FMCLK(CKGEN,  7, "f_fuart_ck"),
	FMCLK(CKGEN,  8, "hf_fspi_ck"),
	FMCLK(CKGEN,  9, "hf_fmsdc_30_0_ck"),
	FMCLK(CKGEN, 10, "hf_fmsdc_30_1_ck"),
	FMCLK(CKGEN, 11, "hf_fmsdc_30_2_ck"),
	FMCLK(CKGEN, 12, "hf_fmsdc_50_3_ck"),
	FMCLK(CKGEN, 13, "hf_fmsdc_50_3_hclk_ck"),
	FMCLK(CKGEN, 14, "hf_faudio_ck"),
	FMCLK(CKGEN, 15, "hf_faud_intbus_ck"),
	FMCLK(CKGEN, 16, "hf_fpmicspi_ck"),
	FMCLK(CKGEN, 17, "hf_fscp_ck"),
	FMCLK(CKGEN, 18, "hf_fatb_ck"),
	FMCLK(CKGEN, 19, "hf_fsnfi_ck"),
	FMCLK(CKGEN, 20, "hf_fdpi0_ck"),
	FMCLK(CKGEN, 21, "hf_faud_1_ck_pre"),
	FMCLK(CKGEN, 22, "hf_faud_2_ck_pre"),
	FMCLK(CKGEN, 23, "hf_fscam_ck"),
	FMCLK(CKGEN, 24, "hf_fmfg_ck"),
	FMCLK(CKGEN, 25, "eco_ao12_mem_clkmux_ck"),
	FMCLK(CKGEN, 26, "eco_a012_mem_dcm_ck"),
	FMCLK(CKGEN, 27, "hf_fdpi1_ck"),
	FMCLK(CKGEN, 28, "hf_fufoenc_ck"),
	FMCLK(CKGEN, 29, "hf_fufodec_ck"),
	FMCLK(CKGEN, 30, "hf_feth_ck"),
	FMCLK(CKGEN, 31, "hf_fonfi_ck"),
	{}
};

static void set_fmeter_divider_ca7(u32 k1)
{
	u32 v = clk_readl(CLK_MISC_CFG_0);

	v = ALT_BITS(v, 23, 16, k1);
	clk_writel(CLK_MISC_CFG_0, v);
}

static void set_fmeter_divider(u32 k1)
{
	u32 v = clk_readl(CLK_MISC_CFG_0);

	v = ALT_BITS(v, 31, 24, k1);
	clk_writel(CLK_MISC_CFG_0, v);
}

static bool wait_fmeter_done(u32 tri_bit)
{
	static int max_wait_count;
	int wait_count = (max_wait_count > 0) ? (max_wait_count * 2 + 2) : 100;
	int i;

	/* wait fmeter */
	for (i = 0; i < wait_count && (clk_readl(CLK26CALI_0) & tri_bit); i++)
		udelay(20);

	if (!(clk_readl(CLK26CALI_0) & tri_bit)) {
		max_wait_count = max(max_wait_count, i);
		return true;
	}

	return false;
}

static u32 fmeter_freq(enum FMETER_TYPE type, int k1, int clk)
{
	u32 cksw_ckgen[] =	{15, 8, clk};
	u32 cksw_abist[] =	{23, 16, clk};
	u32 fqmtr_ck =		(type == CKGEN) ? 1 : 0;
	void __iomem *clk_cfg_reg =
				(type == CKGEN) ? CLK_DBG_CFG	: CLK_DBG_CFG;
	void __iomem *cnt_reg =
				(type == CKGEN) ? CLK26CALI_1	: CLK26CALI_1;
	u32 *cksw_hlv =		(type == CKGEN) ? cksw_ckgen	: cksw_abist;
	u32 tri_bit =		(type == CKGEN) ? BIT(4)	: BIT(4);

	u32 clk_misc_cfg_0;
	u32 clk_cfg_val;
	u32 freq;

	/* setup fmeter */
	clk_writel_mask(CLK_DBG_CFG, 1, 0, fqmtr_ck);	/* fqmtr_ck_sel */
	clk_setl(CLK26CALI_0, BIT(12));			/* enable fmeter_en */

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);	/* backup reg value */
	clk_cfg_val = clk_readl(clk_cfg_reg);

	set_fmeter_divider(k1);				/* set div (0 = /1) */
	set_fmeter_divider_ca7(k1);
	clk_writel_mask(clk_cfg_reg, cksw_hlv[0], cksw_hlv[1], cksw_hlv[2]);

	clk_setl(CLK26CALI_0, tri_bit);			/* start fmeter */

	freq = 0;

	if (wait_fmeter_done(tri_bit)) {
		u32 cnt = clk_readl(cnt_reg) & 0xFFFF;

		/* freq = counter * 26M / 1024 (KHz) */
		freq = (cnt * 26000) * (k1 + 1) / 1024;
	}

	/* restore register settings */
	clk_writel(clk_cfg_reg, clk_cfg_val);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);

	return freq;
}

static u32 measure_stable_fmeter_freq(enum FMETER_TYPE type, int k1, int clk)
{
	u32 last_freq = 0;
	u32 freq;
	u32 maxfreq;

	freq = fmeter_freq(type, k1, clk);
	maxfreq = max(freq, last_freq);

	while (maxfreq > 0 && ABS_DIFF(freq, last_freq) * 100 / maxfreq > 10) {
		last_freq = freq;
		freq = fmeter_freq(type, k1, clk);
		maxfreq = max(freq, last_freq);
	}

	return freq;
}

static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return fclks;
}

static void *prepare_fmeter(void)
{
	static u32 old_pll_hp_con0;

	old_pll_hp_con0 = clk_readl(PLL_HP_CON0);
	clk_writel(PLL_HP_CON0, 0x0);		/* disable PLL hopping */
	udelay(10);

	return &old_pll_hp_con0;
}

static void unprepare_fmeter(void *data)
{
	u32 old_pll_hp_con0 = *(u32 *)data;

	/* restore old setting */
	clk_writel(PLL_HP_CON0, old_pll_hp_con0);
}

static u32 fmeter_freq_op(const struct fmeter_clk *fclk)
{
	if (fclk->type)
		return measure_stable_fmeter_freq(fclk->type, 0, fclk->id);

	return 0;
}

/*
 * clkdbg dump_state
 */

static const char * const *get_all_clk_names(void)
{
	static const char * const clks[] = {
		/* ROOT */
		"clk_null",
		"clk26m",
		"clk32k",
		"hdmitx_dig_cts",
		/* APMIXEDSYS */
		"armpll",
		"mainpll",
		"univpll",
		"mmpll",
		"msdcpll",
		"vencpll",
		"tvdpll",
		"mpll",
		"aud1pll",
		"aud2pll",
		"lvdspll",
		/* TOP_DIVS */
		"main_h546m",
		"main_h364m",
		"main_h218p4m",
		"main_h156m",
		"univ_624m",
		"univ_416m",
		"univ_249p6m",
		"univ_178p3m",
		"univ_48m",
		"apll1_ck",
		"apll2_ck",
		"clkrtc_ext",
		"clkrtc_int",
		"dmpll_ck",
		"f26m_mem_ckgen",
		"hdmi_cts_ck",
		"hdmi_cts_d2",
		"hdmi_cts_d3",
		"lvdspll_ck",
		"lvdspll_d2",
		"lvdspll_d4",
		"lvdspll_d8",
		"lvdspll_eth_ck",
		"mmpll_ck",
		"mpll_208m_ck",
		"msdcpll_ck",
		"msdcpll_d2",
		"msdcpll_d4",
		"syspll_d2",
		"syspll_d2p5",
		"syspll_d3",
		"syspll_d5",
		"syspll_d7",
		"syspll1_d2",
		"syspll1_d4",
		"syspll1_d8",
		"syspll1_d16",
		"syspll2_d2",
		"syspll2_d4",
		"syspll2_d8",
		"syspll3_d2",
		"syspll3_d4",
		"syspll4_d2",
		"syspll4_d4",
		"tvdpll_d2",
		"tvdpll_d4",
		"tvdpll_d8",
		"tvdpll_d16",
		"univpll_d2",
		"univpll_d2p5",
		"univpll_d3",
		"univpll_d5",
		"univpll_d7",
		"univpll_d26",
		"univpll1_d2",
		"univpll1_d4",
		"univpll2_d2",
		"univpll2_d4",
		"univpll2_d8",
		"univpll3_d2",
		"univpll3_d4",
		"univpll3_d8",
		"vencpll_ck",
		/* TOP */
		"axi_sel",
		"mem_sel",
		"ddrphycfg_sel",
		"mm_sel",
		"pwm_sel",
		"vdec_sel",
		"mfg_sel",
		"camtg_sel",
		"uart_sel",
		"spi_sel",
		"msdc30_0_sel",
		"msdc30_1_sel",
		"msdc30_2_sel",
		"msdc50_3_hsel",
		"msdc50_3_sel",
		"audio_sel",
		"aud_intbus_sel",
		"pmicspi_sel",
		"scp_sel",
		"atb_sel",
		"mjc_sel",
		"dpi0_sel",
		"scam_sel",
		"aud_1_sel",
		"aud_2_sel",
		"dpi1_sel",
		"ufoenc_sel",
		"ufodec_sel",
		"eth_sel",
		"onfi_sel",
		"snfi_sel",
		"hdmi_sel",
		"rtc_sel",
		/* INFRA */
		"infra_pmic_tmr",
		"infra_pmic_ap",
		"infra_pmic_md",
		"infra_pmic_conn",
		"infra_scpsys",
		"infra_sej",
		"infra_apxgpt",
		"infra_usb",
		"infra_icusb",
		"infra_gce",
		"infra_therm",
		"infra_i2c0",
		"infra_i2c1",
		"infra_i2c2",
		"infra_pwm_hclk",
		"infra_pwm1",
		"infra_pwm2",
		"infra_pwm3",
		"infra_pwm",
		"infra_uart0",
		"infra_uart1",
		"infra_uart2",
		"infra_uart3",
		"infra_usb_mcu",
		"infra_nfi_ecc66",
		"infra_nfi_66m",
		"infra_btif",
		"infra_spi",
		"infra_msdc3",
		"infra_msdc1",
		"infra_msdc2",
		"infra_msdc0",
		"infra_gcpu",
		"infra_auxadc",
		"infra_cpum",
		"infra_irrx",
		"infra_ufo",
		"infra_cec",
		"infra_cec_26m",
		"infra_nfi_bclk",
		"infra_nfi_ecc",
		"infra_ap_dma",
		"infra_xiu",
		"infra_devapc",
		"infra_xiu2ahb",
		"infra_l2c_sram",
		"infra_eth_50m",
		"infra_debugsys",
		"infra_audio",
		"infra_eth_25m",
		"infra_nfi",
		"infra_onfi",
		"infra_snfi",
		"infra_eth",
		"infra_dramc_26m",
		/* MM */
		"mm_smi_comoon",
		"mm_smi_larb0",
		"mm_cam_mdp",
		"mm_mdp_rdma",
		"mm_mdp_rsz0",
		"mm_mdp_rsz1",
		"mm_mdp_tdshp",
		"mm_mdp_wdma",
		"mm_mdp_wrot",
		"mm_fake_eng",
		"mm_disp_ovl0",
		"mm_disp_ovl1",
		"mm_disp_rdma0",
		"mm_disp_rdma1",
		"mm_disp_wdma0",
		"mm_disp_color",
		"mm_disp_ccorr",
		"mm_disp_aal",
		"mm_disp_gamma",
		"mm_disp_dither",
		"mm_disp_ufoe",
		"mm_larb4_mm",
		"mm_larb4_mjc",
		"mm_disp_wdma1",
		"mm_ufodrdma0_l0",
		"mm_ufodrdma0_l1",
		"mm_ufodrdma0_l2",
		"mm_ufodrdma0_l3",
		"mm_ufodrdma1_l0",
		"mm_ufodrdma1_l1",
		"mm_ufodrdma1_l2",
		"mm_ufodrdma1_l3",
		"mm_disp_pwm0mm",
		"mm_disp_pwm026m",
		"mm_dsi0_engine",
		"mm_dsi0_digital",
		"mm_dpi_pixel",
		"mm_dpi_engine",
		"mm_lvds_pixel",
		"mm_lvds_cts",
		"mm_dpi1_pixel",
		"mm_dpi1_engine",
		"mm_hdmi_pixel",
		"mm_hdmi_spdif",
		"mm_hdmi_audio",
		"mm_hdmi_pllck",
		"mm_disp_dsc_eng",
		"mm_disp_dsc_mem",
		/* IMG */
		"img_larb2_smi",
		"img_jpgenc",
		"img_cam_smi",
		"img_cam_cam",
		"img_sen_tg",
		"img_sen_cam",
		"img_cam_sv",
		/* VDEC */
		"vdec_cken",
		"vdec_larb_cken",
		/* VENC */
		"venc_cke0",
		"venc_cke1",
		"venc_cke2",
		"venc_cke3",
		/* MFG */
		"mfg_ff",
		"mfg_dw",
		"mfg_tx",
		"mfg_mx",
		/* AUDIO */
		"aud_afe",
		"aud_i2s",
		"aud_22m",
		"aud_24m",
		"aud_spdf2",
		"aud_apll2_tnr",
		"aud_apll_tnr",
		"aud_hdmi",
		"aud_spdf",
		"aud_adc",
		"aud_dac",
		"aud_dac_predis",
		"aud_tml",
		"aud_ahb_idl_ex",
		"aud_ahb_idl_in",
		/* end */
		NULL
	};

	return clks;
}

/*
 * clkdbg pwr_status
 */

static const char * const *get_pwr_names(void)
{
	static const char * const pwr_names[] = {
		[0]  = "MD",
		[1]  = "CONN",
		[2]  = "DDRPHY",
		[3]  = "DISP",
		[4]  = "MFG",
		[5]  = "ISP",
		[6]  = "INFRA",
		[7]  = "VDEC",
		[8]  = "CA7_CPUTOP",
		[9]  = "CA7_CPU0",
		[10] = "CA7_CPU1",
		[11] = "CA7_CPU2",
		[12] = "CA7_CPU3",
		[13] = "CA7_DBG",
		[14] = "MCUSYS",
		[15] = "CA15_CPUTOP",
		[16] = "CA15_CPU0",
		[17] = "CA15_CPU1",
		[18] = "CA15_CPU2",
		[19] = "CA15_CPU3",
		[20] = "MJC",
		[21] = "VEN",
		[22] = "MFG_2D",
		[23] = "MFG_ASYNC",
		[24] = "AUDIO",
		[25] = "VCORE_PDN",
		[26] = "ARMPLL_DIV",
		[27] = "MD2",
		[28] = "",
		[29] = "",
		[30] = "",
		[31] = "",
	};

	return pwr_names;
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt8163_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = prepare_fmeter,
	.unprepare_fmeter = unprepare_fmeter,
	.fmeter_freq = fmeter_freq_op,
	.get_all_regnames = get_all_regnames,
	.get_all_clk_names = get_all_clk_names,
	.get_pwr_names = get_pwr_names,
};

static int __init clkdbg_mt8163_init(void)
{
	if (!of_machine_is_compatible("mediatek,mt8163"))
		return -ENODEV;

	init_regbase();

	set_clkdbg_ops(&clkdbg_mt8163_ops);

#if ALL_CLK_ON
	prepare_enable_provider("topckgen");
	reg_pdrv("all");
	prepare_enable_provider("all");
#endif

#if DUMP_INIT_STATE
	print_regs();
	print_fmeter_all();
#endif /* DUMP_INIT_STATE */

	return 0;
}
device_initcall(clkdbg_mt8163_init);
