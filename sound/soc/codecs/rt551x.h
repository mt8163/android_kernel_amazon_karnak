/*
 * rt551x.h  --  ALC5514/ALC5518 ALSA SoC audio driver
 *
 * Copyright 2015 Realtek Microelectronics
 * Author: Oder Chiou <oder_chiou@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>

#ifndef __RT551X_H__
#define __RT551X_H__

#define RT5514_DEVICE_ID			0x10ec5514
#define RT5518_DEVICE_ID			0x10ec5518

#define RT551X_BUFFER_VOICE_BASE	0x18000200
#define RT551X_BUFFER_VOICE_LIMIT	0x18000204
#define RT551X_BUFFER_VOICE_RP		0x18000208
#define RT551X_BUFFER_VOICE_WP		0x1800020c
#define RT551X_DRIVER_CTRL0			0x18001028
#define RT551X_REC_RP				0x18001034
#define RT551X_SW_RING_SIZE			0x18001038
#define RT551X_SW_RING_RP			0x1800103c
#define RT551X_SW_RING_WP			0x18001040

#define RT5514_DSP_CLK_1			0x18001100
#define RT5514_DSP_CLK_2			0x18001104
#define RT5514_DSP_CLK_3			0x18001108
#define RT5514_DSP_CLK_4			0x1800110c
#define RT5514_DSP_CLK_5			0x18001110
#define RT5514_DSP_CLK_6			0x18001114
#define RT5514_DSP_CLK_7			0x18001118

#define RT5518_DSP_CLK_1			0x18001100
#define RT5518_DSP_CLK_2			0x18001108
#define RT5518_DSP_CLK_3			0x1800110c
#define RT5518_DSP_CLK_4			0x18001104

#define RT551X_DSP_WDG_1			0x18001300
#define RT551X_DSP_WDG_2			0x18001304
#define RT551X_DSP_WDG_3			0x18001308
#define RT551X_DSP_WDG_4			0x1800130c
#define RT551X_DSP_WDG_5			0x18001310

#define RT551X_VENDOR_ID1			0x18002ff0
#define RT551X_VENDOR_ID2			0x18002ff4
#define RT551X_I2C_BYPASS			0xfafafafa
#define RT551X_DBG_BUF_ADDR			0x4ffaff00
#define RT551X_DBG_BUF_SIZE			0x100
#define RT551X_DBG_BUF_CNT			0x1F

#define RT551X_VENDOR_ID1_MASK		0xFF
#define RT551X_VENDOR_ID1_5518		0x00
#define RT551X_VENDOR_ID1_5518B		0x01

/********************/
/* RT5514 registers */
/********************/
#define RT5514_RESET				0x18002000
#define RT5514_PWR_ANA1				0x18002004
#define RT5514_PWR_ANA2				0x18002008
#define RT5514_I2S_CTRL1			0x18002010
#define RT5514_I2S_CTRL2			0x18002014
#define RT5514_VAD_CTRL6			0x18002030
#define RT5514_EXT_VAD_CTRL			0x1800206c
#define RT5514_DIG_IO_CTRL			0x18002070
#define RT5514_PAD_CTRL1			0x18002080
#define RT5514_DMIC_DATA_CTRL			0x180020a0
#define RT5514_DIG_SOURCE_CTRL			0x180020a4
#define RT5514_SRC_CTRL				0x180020ac
#define RT5514_DOWNFILTER2_CTRL1		0x180020d0
#define RT5514_PLL_SOURCE_CTRL			0x18002100
#define RT5514_CLK_CTRL1			0x18002104
#define RT5514_CLK_CTRL2			0x18002108
#define RT5514_PLL3_CALIB_CTRL1			0x18002110
#define RT5514_PLL3_CALIB_CTRL5			0x18002124
#define RT5514_DELAY_BUF_CTRL1			0x18002140
#define RT5514_DELAY_BUF_CTRL2			0x18002144
#define RT5514_DELAY_BUF_CTRL3			0x18002148
#define RT5514_DOWNFILTER0_CTRL1		0x18002190
#define RT5514_DOWNFILTER0_CTRL2		0x18002194
#define RT5514_DOWNFILTER0_CTRL3		0x18002198
#define RT5514_DOWNFILTER1_CTRL1		0x180021a0
#define RT5514_DOWNFILTER1_CTRL2		0x180021a4
#define RT5514_DOWNFILTER1_CTRL3		0x180021a8
#define RT5514_ANA_CTRL_LDO10			0x18002200
#define RT5514_ANA_CTRL_LDO18_16		0x18002204
#define RT5514_ANA_CTRL_ADC12			0x18002210
#define RT5514_ANA_CTRL_ADC21			0x18002214
#define RT5514_ANA_CTRL_ADC22			0x18002218
#define RT5514_ANA_CTRL_ADC23			0x1800221c
#define RT5514_ANA_CTRL_MICBST			0x18002220
#define RT5514_ANA_CTRL_ADCFED			0x18002224
#define RT5514_ANA_CTRL_INBUF			0x18002228
#define RT5514_ANA_CTRL_VREF			0x1800222c
#define RT5514_ANA_CTRL_PLL3			0x18002240
#define RT5514_ANA_CTRL_PLL1_1			0x18002260
#define RT5514_ANA_CTRL_PLL1_2			0x18002264
#define RT5514_DMIC_LP_CTRL			0x18002e00
#define RT5514_MISC_CTRL_DSP			0x18002e04
#define RT5514_DSP_CTRL1			0x18002f00
#define RT5514_DSP_CTRL3			0x18002f08
#define RT5514_DSP_CTRL4			0x18002f10

/* RT5514_PWR_ANA1 (0x2004) */
#define RT5514_POW_LDO18_IN			(0x1 << 5)
#define RT5514_POW_LDO18_IN_BIT			5
#define RT5514_POW_LDO18_ADC			(0x1 << 4)
#define RT5514_POW_LDO18_ADC_BIT		4
#define RT5514_POW_LDO21			(0x1 << 3)
#define RT5514_POW_LDO21_BIT			3
#define RT5514_POW_BG_LDO18_IN			(0x1 << 2)
#define RT5514_POW_BG_LDO18_IN_BIT		2
#define RT5514_POW_BG_LDO21			(0x1 << 1)
#define RT5514_POW_BG_LDO21_BIT			1

/* RT5514_PWR_ANA2 (0x2008) */
#define RT5514_POW_PLL1				(0x1 << 18)
#define RT5514_POW_PLL1_BIT			18
#define RT5514_POW_PLL1_LDO			(0x1 << 16)
#define RT5514_POW_PLL1_LDO_BIT			16
#define RT5514_POW_BG_MBIAS			(0x1 << 15)
#define RT5514_POW_BG_MBIAS_BIT			15
#define RT5514_POW_MBIAS			(0x1 << 14)
#define RT5514_POW_MBIAS_BIT			14
#define RT5514_POW_VREF2			(0x1 << 13)
#define RT5514_POW_VREF2_BIT			13
#define RT5514_POW_VREF1			(0x1 << 12)
#define RT5514_POW_VREF1_BIT			12
#define RT5514_POWR_LDO16			(0x1 << 11)
#define RT5514_POWR_LDO16_BIT			11
#define RT5514_POWL_LDO16			(0x1 << 10)
#define RT5514_POWL_LDO16_BIT			10
#define RT5514_POW_ADC2				(0x1 << 9)
#define RT5514_POW_ADC2_BIT			9
#define RT5514_POW_INPUT_BUF			(0x1 << 8)
#define RT5514_POW_INPUT_BUF_BIT		8
#define RT5514_POW_ADC1_R			(0x1 << 7)
#define RT5514_POW_ADC1_R_BIT			7
#define RT5514_POW_ADC1_L			(0x1 << 6)
#define RT5514_POW_ADC1_L_BIT			6
#define RT5514_POW2_BSTR			(0x1 << 5)
#define RT5514_POW2_BSTR_BIT			5
#define RT5514_POW2_BSTL			(0x1 << 4)
#define RT5514_POW2_BSTL_BIT			4
#define RT5514_POW_BSTR				(0x1 << 3)
#define RT5514_POW_BSTR_BIT			3
#define RT5514_POW_BSTL				(0x1 << 2)
#define RT5514_POW_BSTL_BIT			2
#define RT5514_POW_ADCFEDR			(0x1 << 1)
#define RT5514_POW_ADCFEDR_BIT			1
#define RT5514_POW_ADCFEDL			(0x1 << 0)
#define RT5514_POW_ADCFEDL_BIT			0

/* RT5514_I2S_CTRL1 (0x2010) */
#define RT5514_TDM_MODE				(0x1 << 28)
#define RT5514_TDM_MODE_SFT			28
#define RT5514_I2S_LR_MASK			(0x1 << 26)
#define RT5514_I2S_LR_SFT			26
#define RT5514_I2S_LR_NOR			(0x0 << 26)
#define RT5514_I2S_LR_INV			(0x1 << 26)
#define RT5514_I2S_BP_MASK			(0x1 << 25)
#define RT5514_I2S_BP_SFT			25
#define RT5514_I2S_BP_NOR			(0x0 << 25)
#define RT5514_I2S_BP_INV			(0x1 << 25)
#define RT5514_I2S_DF_MASK			(0x7 << 16)
#define RT5514_I2S_DF_SFT			16
#define RT5514_I2S_DF_I2S			(0x0 << 16)
#define RT5514_I2S_DF_LEFT			(0x1 << 16)
#define RT5514_I2S_DF_PCM_A			(0x2 << 16)
#define RT5514_I2S_DF_PCM_B			(0x3 << 16)
#define RT5514_TDMSLOT_SEL_RX_MASK		(0x3 << 10)
#define RT5514_TDMSLOT_SEL_RX_SFT		10
#define RT5514_TDMSLOT_SEL_RX_4CH		(0x1 << 10)
#define RT5514_CH_LEN_RX_MASK			(0x3 << 8)
#define RT5514_CH_LEN_RX_SFT			8
#define RT5514_CH_LEN_RX_16			(0x0 << 8)
#define RT5514_CH_LEN_RX_20			(0x1 << 8)
#define RT5514_CH_LEN_RX_24			(0x2 << 8)
#define RT5514_CH_LEN_RX_32			(0x3 << 8)
#define RT5514_TDMSLOT_SEL_TX_MASK		(0x3 << 6)
#define RT5514_TDMSLOT_SEL_TX_SFT		6
#define RT5514_TDMSLOT_SEL_TX_4CH		(0x1 << 6)
#define RT5514_CH_LEN_TX_MASK			(0x3 << 4)
#define RT5514_CH_LEN_TX_SFT			4
#define RT5514_CH_LEN_TX_16			(0x0 << 4)
#define RT5514_CH_LEN_TX_20			(0x1 << 4)
#define RT5514_CH_LEN_TX_24			(0x2 << 4)
#define RT5514_CH_LEN_TX_32			(0x3 << 4)
#define RT5514_I2S_DL_MASK			(0x3 << 0)
#define RT5514_I2S_DL_SFT			0
#define RT5514_I2S_DL_16			(0x0 << 0)
#define RT5514_I2S_DL_20			(0x1 << 0)
#define RT5514_I2S_DL_24			(0x2 << 0)
#define RT5514_I2S_DL_8				(0x3 << 0)

/* RT5514_DIG_SOURCE_CTRL (0x20a4) */
#define RT5514_AD1_DMIC_INPUT_SEL		(0x1 << 1)
#define RT5514_AD1_DMIC_INPUT_SEL_SFT		1
#define RT5514_AD0_DMIC_INPUT_SEL		(0x1 << 0)
#define RT5514_AD0_DMIC_INPUT_SEL_SFT		0

/* RT5514_PLL_SOURCE_CTRL (0x2100) */
#define RT5514_PLL_1_SEL_MASK			(0x7 << 12)
#define RT5514_PLL_1_SEL_SFT			12
#define RT5514_PLL_1_SEL_SCLK			(0x3 << 12)
#define RT5514_PLL_1_SEL_MCLK			(0x4 << 12)

/* RT5514_CLK_CTRL1 (0x2104) */
#define RT5514_CLK_AD_ANA1_EN			(0x1 << 31)
#define RT5514_CLK_AD_ANA1_EN_BIT		31
#define RT5514_CLK_AD1_EN			(0x1 << 24)
#define RT5514_CLK_AD1_EN_BIT			24
#define RT5514_CLK_AD0_EN			(0x1 << 23)
#define RT5514_CLK_AD0_EN_BIT			23
#define RT5514_CLK_DMIC_OUT_SEL_MASK		(0x7 << 8)
#define RT5514_CLK_DMIC_OUT_SEL_SFT		8
#define RT5514_CLK_AD_ANA1_SEL_MASK		(0xf << 0)
#define RT5514_CLK_AD_ANA1_SEL_SFT		0

/* RT5514_CLK_CTRL2 (0x2108) */
#define RT5514_CLK_SYS_DIV_OUT_MASK		(0x7 << 8)
#define RT5514_CLK_SYS_DIV_OUT_SFT		8
#define RT5514_SEL_ADC_OSR_MASK			(0x7 << 4)
#define RT5514_SEL_ADC_OSR_SFT			4
#define RT5514_CLK_SYS_PRE_SEL_MASK		(0x3 << 0)
#define RT5514_CLK_SYS_PRE_SEL_SFT		0
#define RT5514_CLK_SYS_PRE_SEL_MCLK		(0x2 << 0)
#define RT5514_CLK_SYS_PRE_SEL_PLL		(0x3 << 0)

/*  RT5514_DOWNFILTER_CTRL (0x2190 0x2194 0x21a0 0x21a4) */
#define RT5514_AD_DMIC_MIX			(0x1 << 11)
#define RT5514_AD_DMIC_MIX_BIT			11
#define RT5514_AD_AD_MIX			(0x1 << 10)
#define RT5514_AD_AD_MIX_BIT			10
#define RT5514_AD_AD_MUTE			(0x1 << 7)
#define RT5514_AD_AD_MUTE_BIT			7
#define RT5514_AD_GAIN_MASK			(0x7f << 0)
#define RT5514_AD_GAIN_SFT			0

/*  RT5514_ANA_CTRL_MICBST (0x2220) */
#define RT5514_SEL_BSTL_MASK			(0xf << 4)
#define RT5514_SEL_BSTL_SFT			4
#define RT5514_SEL_BSTR_MASK			(0xf << 0)
#define RT5514_SEL_BSTR_SFT			0

/*  RT5514_ANA_CTRL_PLL1_1 (0x2260) */
#define RT5514_PLL_K_MAX			0x1f
#define RT5514_PLL_K_MASK			(RT5514_PLL_K_MAX << 16)
#define RT5514_PLL_K_SFT			16
#define RT5514_PLL_N_MAX			0x1ff
#define RT5514_PLL_N_MASK			(RT5514_PLL_N_MAX << 7)
#define RT5514_PLL_N_SFT			4
#define RT5514_PLL_M_MAX			0xf
#define RT5514_PLL_M_MASK			(RT5514_PLL_M_MAX << 0)
#define RT5514_PLL_M_SFT			0

/*  RT5514_ANA_CTRL_PLL1_2 (0x2264) */
#define RT5514_PLL_M_BP				(0x1 << 2)
#define RT5514_PLL_M_BP_SFT			2
#define RT5514_PLL_K_BP				(0x1 << 1)
#define RT5514_PLL_K_BP_SFT			1
#define RT5514_EN_LDO_PLL1			(0x1 << 0)
#define RT5514_EN_LDO_PLL1_BIT			0

#define RT5514_PLL_INP_MAX			40000000
#define RT5514_PLL_INP_MIN			256000

/********************/
/* RT5518 registers */
/********************/
#define RT5518_RESET				0x18002000
#define RT5518_PWR_ANA1				0x18002004
#define RT5518_PWR_ANA2				0x18002008
#define RT5518_DIG_OUT_GATE			0x1800200c
#define RT5518_I2S_CTRL1			0x18002010
#define RT5518_I2S_CTRL2			0x18002014
#define RT5518_I2S_CTRL3			0x18002018
#define RT5518_I2S_CTRL4			0x1800201c
#define RT5518_SCAN_CTRL1			0x18002050
#define RT5518_EXT_VAD_CTRL			0x1800206c
#define RT5518_DIG_IO_CTRL			0x18002070
#define RT5518_GPIO_CTRL1			0x18002074
#define RT5518_GPIO_CTRL2			0x18002078
#define RT5518_GPIO_CTRL3			0x1800207c
#define RT5518_PAD_CTRL1			0x18002080
#define RT5518_PAD_CTRL2			0x18002084
#define RT5518_TEST_CTRL1			0x18002088
#define RT5518_TEST_CTRL2			0x1800208c
#define RT5518_TEST_CTRL3			0x18002090
#define RT5518_IRQ_CTRL1			0x18002094
#define RT5518_DMIC_DATA_CTRL			0x180020a0
#define RT5518_AUDIO_PATH_CTRL			0x180020a4
#define RT5518_DOWNFILTER2_CTRL1		0x180020d0
#define RT5518_PLL_SOURCE_CTRL			0x18002100
#define RT5518_CLK_CTRL1			0x18002104
#define RT5518_CLK_CTRL2			0x18002108
#define RT5518_DFLL_CTRL1			0x18002110
#define RT5518_DFLL_CTRL2			0x18002114
#define RT5518_DFLL_CTRL3			0x18002118
#define RT5518_DFLL_CTRL3_2			0x1800211c
#define RT5518_DFLL_CTRL4			0x18002120
#define RT5518_DFLL_CTRL5			0x18002124
#define RT5518_DFLL_STA1			0x18002128
#define RT5518_DFLL_STA2			0x1800212c
#define RT5518_ASRC_DWNFILT0_CTRL1		0x18002180
#define RT5518_ASRC_DWNFILT0_CTRL2		0x18002184
#define RT5518_DOWNFILTER0_CTRL1		0x18002190
#define RT5518_DOWNFILTER0_CTRL2		0x18002194
#define RT5518_DOWNFILTER0_CTRL3		0x18002198
#define RT5518_ANA_CTRL_LDO10			0x18002200
#define RT5518_ANA_CTRL_LDO18_16		0x18002204
#define RT5518_ANA_PLL3_LDO_21			0x18002208
#define RT5518_ANA_CTRL_ADC21			0x18002214
#define RT5518_ANA_CTRL_ADC22			0x18002218
#define RT5518_ANA_CTRL_ADC23			0x1800221c
#define RT5518_ANA_CTRL_INBUF			0x18002228
#define RT5518_ANA_CTRL_VREF			0x1800222c
#define RT5518_ANA_CTRL_MBIAS			0x18002230
#define RT5518_ANA_CTRL_PLL3			0x18002240
#define RT5518_ANA_CTRL_PLL21			0x18002250
#define RT5518_ANA_CTRL_PLL22			0x18002254
#define RT5518_ANA_CTRL_PLL23			0x18002258
#define RT5518_AGC_FUNC_CTRL1			0x18002300
#define RT5518_AGC_FUNC_CTRL2			0x18002304
#define RT5518_AGC_FUNC_CTRL3			0x18002308
#define RT5518_AGC_FUNC_CTRL4			0x1800230c
#define RT5518_AGC_FUNC_CTRL5			0x18002310
#define RT5518_AGC_FUNC_CTRL6			0x18002314
#define RT5518_AGC_FUNC_CTRL7			0x18002318
#define RT5518_AGC_FUNC_STAT			0x1800231c
#define RT5518_SCAN_STAT			0x18002c00
#define RT5518_I2CBYPA_STAT			0x18002c04
#define RT5518_I2CBURSTL_ADDR			0x18002d00
#define RT5518_I2CBURSTL_MEM_ADDR		0x18002d04
#define RT5518_I2CBURST_MEM_ADDR		0x18002d08
#define RT5518_IRQ_CTRL2			0x18002e04
#define RT5518_GPIO_INTR_CTRL			0x18002e08
#define RT5518_GPIO_INTR_STAT			0x18002e0c
#define RT5518_DSP_CTRL1			0x18002f00
#define RT5518_DSP_CTRL2			0x18002f04
#define RT5518_DSP_CTRL3			0x18002f08
#define RT5518_DSP_CTRL4			0x18002f10
#define RT5518_DSP_CTRL5			0x18002f14

/* RT5518_PWR_ANA1 (0x2004) */
#define RT5518_POW_LDO18_IN			(0x1 << 5)
#define RT5518_POW_LDO18_IN_BIT			5
#define RT5518_POW_LDO18_ADC			(0x1 << 4)
#define RT5518_POW_LDO18_ADC_BIT		4
#define RT5518_POW_LDO21			(0x1 << 3)
#define RT5518_POW_LDO21_BIT			3
#define RT5518_POW_BG_LDO18_IN			(0x1 << 2)
#define RT5518_POW_BG_LDO18_IN_BIT		2
#define RT5518_POW_BG_LDO21			(0x1 << 1)
#define RT5518_POW_BG_LDO21_BIT			1

/* RT5518_PWR_ANA2 (0x2008) */
#define RT5518_POW_PLL2				(0x1 << 18)
#define RT5518_POW_PLL2_BIT			18
#define RT5518_POW_PLL2_LDO			(0x1 << 16)
#define RT5518_POW_PLL2_LDO_BIT			16
#define RT5518_POW_BG_MBIAS			(0x1 << 15)
#define RT5518_POW_BG_MBIAS_BIT			15
#define RT5518_POW_MBIAS			(0x1 << 14)
#define RT5518_POW_MBIAS_BIT			14
#define RT5518_POW_VREF2			(0x1 << 13)
#define RT5518_POW_VREF2_BIT			13
#define RT5518_POW_VREF1			(0x1 << 12)
#define RT5518_POW_VREF1_BIT			12
#define RT5518_POWR_LDO16			(0x1 << 11)
#define RT5518_POWR_LDO16_BIT			11
#define RT5518_POWL_LDO16			(0x1 << 10)
#define RT5518_POWL_LDO16_BIT			10
#define RT5518_POW_ADC2				(0x1 << 9)
#define RT5518_POW_ADC2_BIT			9
#define RT5518_POW_INPUT_BUF			(0x1 << 8)
#define RT5518_POW_INPUT_BUF_BIT		8

/* RT5518_DIG_OUT_GATE (0x200c) */
#define RT5518_DMICDATA_GATE			(0x1 << 1)
#define RT5518_DMICDATA_GATE_BIT		1
#define RT5518_I2SOUT_GATE			(0x1 << 0)
#define RT5518_I2SOUT_GATE_BIT			0

/* RT5518_I2S_CTRL1 (0x2010) */
#define RT5518_I2S_LR_MASK			(0x1 << 26)
#define RT5518_I2S_LR_SFT			26
#define RT5518_I2S_LR_NOR			(0x0 << 26)
#define RT5518_I2S_LR_INV			(0x1 << 26)
#define RT5518_I2S_BP_MASK			(0x1 << 25)
#define RT5518_I2S_BP_SFT			25
#define RT5518_I2S_BP_NOR			(0x0 << 25)
#define RT5518_I2S_BP_INV			(0x1 << 25)
#define RT5518_I2S_CH_LEN_MASK			(0x7 << 16)
#define RT5518_I2S_CH_LEN_TX_SFT		16
#define RT5518_I2S_CH_LEN_TX_16			(0x0 << 16)
#define RT5518_I2S_CH_LEN_TX_20			(0x1 << 16)
#define RT5518_I2S_CH_LEN_TX_24			(0x2 << 16)
#define RT5518_I2S_CH_LEN_TX_32			(0x3 << 16)
#define RT5518_I2S_CH_LEN_TX_8			(0x4 << 16)
#define RT5518_I2S_CH_NUM_RX_MASK		(0x3 << 12)
#define RT5518_I2S_CH_NUM_RX_SFT		12
#define RT5518_I2S_CH_NUM_RX_2CH		(0x0 << 12)
#define RT5518_I2S_CH_NUM_RX_4CH		(0x1 << 12)
#define RT5518_I2S_CH_NUM_RX_6CH		(0x2 << 12)
#define RT5518_I2S_CH_NUM_RX_8CH		(0x3 << 12)
#define RT5518_CH_LEN_RX_MASK			(0x7 << 8)
#define RT5518_CH_LEN_RX_SFT			8
#define RT5518_CH_LEN_RX_16			(0x0 << 8)
#define RT5518_CH_LEN_RX_20			(0x1 << 8)
#define RT5518_CH_LEN_RX_24			(0x2 << 8)
#define RT5518_CH_LEN_RX_32			(0x3 << 8)
#define RT5518_CH_LEN_RX_8			(0x4 << 8)
#define RT5518_I2S_DATA_LEN_MASK		(0x7 << 4)
#define RT5518_I2S_DATA_LEN_SFT			4
#define RT5518_CH_LEN_TX_16			(0x0 << 4)
#define RT5518_CH_LEN_TX_20			(0x1 << 4)
#define RT5518_CH_LEN_TX_24			(0x2 << 4)
#define RT5518_CH_LEN_TX_32			(0x3 << 4)
#define RT5518_CH_LEN_TX_8			(0x4 << 4)
#define RT5518_I2S_FORMAT_MASK			(0x7 << 0)
#define RT5518_I2S_FORMAT_SFT			0
#define RT5518_I2S_FORMAT_I2S			(0x0 << 0)
#define RT5518_I2S_FORMAT_LJ			(0x1 << 0)
#define RT5518_I2S_FORMAT_PCMA			(0x2 << 0)
#define RT5518_I2S_FORMAT_PCMB			(0x3 << 0)
#define RT5518_I2S_FORMAT_PCMAN			(0x6 << 0)
#define RT5518_I2S_FORMAT_PCMBN			(0x7 << 0)

/* RT5518_I2S_CTRL2 (0x2014) */
#define RT5518_I2S_DOCK_EN_MASK			(0x1 << 31)
#define RT5518_I2S_DOCK_EN_SHIFT		31
#define RT5518_I2S_DOCK_EN			(0x1 << 31)
#define RT5518_I2S_DOCK_DIS			(0x0 << 31)
#define RT5518_I2S_DOCK_ADCDAT_MASK		(0x7 << 28)
#define RT5518_I2S_DOCK_ADCDAT_SHT		28
#define RT5518_I2S_DOCK_ADCDAT_SLOT0		(0x0 << 28)
#define RT5518_I2S_DOCK_ADCDAT_SLOT1		(0x1 << 28)
#define RT5518_I2S_DOCK_ADCDAT_SLOT2		(0x2 << 28)
#define RT5518_I2S_DOCK_ADCDAT_SLOT3		(0x3 << 28)
#define RT5518_I2S_DOCK_ADCDAT_SLOT4		(0x4 << 28)
#define RT5518_I2S_DOCK_ADCDAT_SLOT5		(0x5 << 28)
#define RT5518_I2S_DOCK_ADCDAT_SLOT6		(0x6 << 28)
#define RT5518_I2S_DOCK_ADCDAT_SLOT7		(0x7 << 28)

/* RT5518_I2S_CTRL3 (0x2018) */
#define RT5518_I2S_TXR_DAC1_MASK		(0x7 << 4)
#define RT5518_I2S_TXR_DAC1_SLOT0		(0x0 << 4)
#define RT5518_I2S_TXR_DAC1_SLOT1		(0x1 << 4)
#define RT5518_I2S_TXR_DAC1_SLOT2		(0x2 << 4)
#define RT5518_I2S_TXR_DAC1_SLOT3		(0x3 << 4)
#define RT5518_I2S_TXR_DAC1_SLOT4		(0x4 << 4)
#define RT5518_I2S_TXR_DAC1_SLOT5		(0x5 << 4)
#define RT5518_I2S_TXR_DAC1_SLOT6		(0x6 << 4)
#define RT5518_I2S_TXR_DAC1_SLOT7		(0x7 << 4)
#define RT5518_I2S_TXL_DAC1_MASK		(0x7 << 0)
#define RT5518_I2S_TXL_DAC1_SLOT0		(0x0 << 0)
#define RT5518_I2S_TXL_DAC1_SLOT1		(0x1 << 0)
#define RT5518_I2S_TXL_DAC1_SLOT2		(0x2 << 0)
#define RT5518_I2S_TXL_DAC1_SLOT3		(0x3 << 0)
#define RT5518_I2S_TXL_DAC1_SLOT4		(0x4 << 0)
#define RT5518_I2S_TXL_DAC1_SLOT5		(0x5 << 0)
#define RT5518_I2S_TXL_DAC1_SLOT6		(0x6 << 0)
#define RT5518_I2S_TXL_DAC1_SLOT7		(0x7 << 0)

/* RT5518_I2S_CTRL_MASTER_MODE (0x201c) */
#define RT5518_I2S_DATA_FORM_MASK		(0x7 << 12)
#define RT5518_I2S_DATA_FORM_SHIF		12
#define RT5518_I2S_DATA_FORM_I2S		(0x0 << 12)
#define RT5518_I2S_DATA_FORM_LEFTJ		(0x1 << 12)
#define RT5518_I2S_DATA_FORM_PCMA		(0x2 << 12)
#define RT5518_I2S_DATA_FORM_PCMB		(0x3 << 12)
#define RT5518_I2S_DATA_FORM_PCMAN		(0x6 << 12)
#define RT5518_I2S_DATA_FORM_PCMBN		(0x7 << 12)

/* RT5518_TEST_PAD_SCAN_CTRL (0x2050) */
#define RT5518_SEL_PAD_DRV_MASK			(0x1 << 1)
#define RT5518_SEL_PAD_DRV_SHIFT		1
#define RT5518_SEL_PAD_DRV_8mA			(0x1 << 1)
#define RT5518_SEL_PAD_DRV_4mA			(0x0 << 1)
#define RT5518_PADSEL_EN_MASK			0x1
#define RT5518_PADSEL_EN_SHIFT			0
#define RT5518_PADSEL_EN_OUTPUT			0x1
#define RT5518_PADSEL_EN_INPUT			0x0

/* RT5518_EXTERNAL_VAD_CTRL (0x206c) */
#define RT5518_EN_EXT_DET_MASK			(0x1 << 20)
#define RT5518_EN_EXT_DET_EN			(0x1 << 20)
#define RT5518_EN_EXT_DET_DIS			(0x0 << 20)
#define RT5518_EN_EXT_DET_SHIFT			20

/* RT5518_AUDIO_PATH_CTRL (0x20a4) */
#define RT5518_DSP_INB1_SRC_SEL_MASK		(0x3 << 26)
#define RT5518_DSP_INB1_SRC_SEL_SHF		26
#define RT5518_DSP_INB1_SRC_TDM_IR		(0x0 << 26)
#define RT5518_DSP_INB1_SRC_PCMDATA0_R		(0x1 << 26)
#define RT5518_DSP_INB1_SRC_PCMDATA_MONO	(0x2 << 26)
#define RT5518_DSP_INB0_SRC_SEL_MASK		(0x3 << 24)
#define RT5518_DSP_INB0_SRC_SEL_SHF		24
#define RT5518_DSP_INB0_SRC_TDM_IL		(0x0 << 24)
#define RT5518_DSP_INB0_SRC_PCMDATA0_L		(0x1 << 24)
#define RT5518_DSP_INB0_SRC_PCMDATA_MONO	(0x2 << 24)


/* RT5518_PLL_SOURCE_CTRL (0x2100) */
#define RT5518_PLL_2_SEL_MASK			(0x7 << 8)
#define RT5518_PLL_2_SEL_SFT			8
#define RT5518_PLL_2_SEL_SCLK			(0x3 << 8)
#define RT5518_PLL_2_SEL_MCLK			(0x4 << 8)

/* RT5518_CLK_CTRL1 (0x2104) */
#define RT5518_CLK_AD_ANA2_EN			(0x1 << 30)
#define RT5518_CLK_AD_ANA2_EN_BIT		30
#define RT5518_CLK_AD_EN			(0x1 << 26)
#define RT5518_CLK_AD_DIS			(0x0 << 26)
#define RT5518_CLK_AD_EN_BIT			26
#define RT5518_CLK_AD0_EN			(0x1 << 24)
#define RT5518_CLK_AD0_EN_BIT			24
#define RT5518_CLK_DMIC_OUT_SEL_MASK		(0x7 << 8)
#define RT5518_CLK_DMIC_OUT_SEL_SFT		8

/* RT5518_CLK_CTRL2 (0x2108) */
#define RT5518_CLK_SYS_DIV_OUT_MASK		(0x7 << 8)
#define RT5518_CLK_SYS_DIV_OUT_SFT		8
#define RT5518_SEL_ADC_OSR_MASK			(0x7 << 4)
#define RT5518_SEL_ADC_OSR_SFT			4
#define RT5518_CLK_SYS_PRE_SEL_MASK		(0x3 << 0)
#define RT5518_CLK_SYS_PRE_SEL_SFT		0
#define RT5518_CLK_SYS_PRE_SEL_PLL_1		(0x1 << 0)
#define RT5518_CLK_SYS_PRE_SEL_MCLK		(0x2 << 0)
#define RT5518_CLK_SYS_PRE_SEL_PLL_2		(0x3 << 0)

/* RT5518_ASRC_DWNFILT0_CTRL1 (0x2180) */
#define RT5518_ASRCIN_FTK_M1_MASK		(0x7 << 8)
#define RT5518_ASRCIN_FTK_M1_BIT		8

/* RT5518_DOWNFILTER0_CTRL1 (0x2190, 0x2194)  */
#define RT5518_AD_DMIC_MIX_BIT			11
#define RT5518_AD_AD_MIX_BIT			10
#define RT5518_AD_AD_MUTE_BIT			7
#define RT5518_AD_GAIN_MASK			(0x7f << 0)
#define RT5518_AD_GAIN_SFT			0

/* RT5518_DOWNFILTER0_CTRL3 (0x2198) */
#define RT5518_DMICR_SEL_SFT			1
#define RT5518_DMICL_SEL_SFT			0

/* RT5518_ANA_CTRL_INBUF (0x2228) */
#define RT5518_GAIN_INBUF_SFT			0

/* RT5518_ANA_CTRL_PLL22 (0x2254) */
#define RT5518_EN_LDO_PLL2_BIT			0

/*  RT5518_AGC_FUNC_CTRL1 (0x2300) */
#define RT5518_AGC_EN_CTRL_MASK			(0x1 << 31)
#define RT5518_AGC_EN_CTRL_SFT			31
#define RT5518_AGC_EN_CTRL_EN			(0x1 << 31)
#define RT5518_AGC_EN_CTRL_DIS			(0x0 << 31)
#define RT5518_AGCDRC_EN_CTRL_MASK		(0x1 << 30)
#define RT5518_AGCDRC_EN_CTRL_SFT		30
#define RT5518_AGCDRC_EN_CTRL_EN		(0x1 << 30)
#define RT5518_AGCDRC_EN_CTRL_DIS		(0x0 << 30)
#define RT5518_AGC_SPEEDUP_MASK			(0x1 << 21)
#define RT5518_AGC_SPEEDUP_SFT			21
#define RT5518_AGC_SPEEDUP_EN			(0x1 << 21)
#define RT5518_AGC_SPEEDUP_DIS			(0x0 << 21)

/*  RT5518_AGC_FUNC_CTRL2 (0x2304) */
#define RT5518_AGC_FF_RECOV_MASK		(0x1 << 31)
#define RT5518_AGC_FF_RECOV_SFT			31
#define RT5518_AGC_FF_RECOV_EN			(0x1 << 31)
#define RT5518_AGC_FF_RECOV_DIS			(0x0 << 31)


/* System Clock Source */
enum {
	RT551X_SCLK_S_MCLK,
	RT551X_SCLK_S_PLL1,
};

/* PLL1 Source */
enum {
	RT551X_PLL1_S_MCLK,
	RT551X_PLL1_S_BCLK,
};

struct rt551x_pll_code {
	bool m_bp;
	bool k_bp;
	int m_code;
	int n_code;
	int k_code;
};

typedef void (*CTRL_FUNC)(void *);
typedef bool (*RCHK_FUNC)(struct device *, unsigned int);

struct rt551x_priv {
	struct snd_soc_codec *codec;
	struct regmap *regmap;
	struct work_struct hotword_work;
	struct work_struct watchdog_work;
	struct work_struct handler_work;
	struct wake_lock vad_wake;
	struct mutex dspcontrol_lock;
	struct i2c_client *i2c;
	unsigned int dbg_base, dbg_limit, dbg_rp, dbg_wp;
	int sysclk;
	int sysclk_src;
	int lrck;
	int bclk;
	int pll_src;
	int pll_in;
	int pll_out;
	int dsp_enabled;
	int had_suspend;
	int i2c_suppend;
	int dsp_had_irq;
	int hw_reset_pin;
	int hw_irq_pin;
#ifdef CONFIG_SND_SOC_RT551X_TEST_ONLY
	int dsp_fw_check;
	int sw_reset;
	int sw_reset_spi;
	int dsp_stop;
	int dsp_core_reset;
#endif
	int dsp_idle;
	int hw_reset;
	unsigned int chip_id;
	CTRL_FUNC fp_reset;
	CTRL_FUNC fp_enable_dsp;
	CTRL_FUNC fp_enable_dsp_clock;
	RCHK_FUNC fp_readable_register;
};

typedef struct {
	unsigned int Offset;
	unsigned int Size;
	unsigned int Addr;
} SMicBinInfo;

//typedef struct {
//	unsigned int ID;
//	unsigned int Addr;
//} SMicTDInfo;

//#define	SMICFW_SYNC			0x23795888

typedef struct {
	unsigned int Sync;
	unsigned int Version;
	unsigned int NumBin;
	unsigned int dspclock;
	unsigned int reserved[8];
	SMicBinInfo *BinArray;
} SMicFWHeader;

//typedef struct {
//	unsigned int NumTD;
//	SMicTDInfo *TDArray;
//} SMicFWSubHeader;

typedef struct _dbgBuf_Unit {
	unsigned int id : 8;
	unsigned int ts : 24;
	unsigned int val;
} DBGBUF_UNIT;

typedef struct _dbgBuf_Mem {
	DBGBUF_UNIT unit[RT551X_DBG_BUF_CNT];
	unsigned int reserve;
	unsigned int idx;
} DBGBUF_MEM;

void rt551x_reset_duetoSPI(void);
#define rt551x_reset(x) ((x->fp_reset)(x))
#define rt551x_enable_dsp(x) ((x->fp_enable_dsp)(x))
#define rt551x_enable_dsp_clock(x) ((x->fp_enable_dsp_clock)(x))
#endif /* __RT551X_H__ */
