/*
 * rt551x.c  --  ALC5514/ALC5518 ALSA SoC audio codec driver
 *
 * Copyright 2015 Realtek Semiconductor Corp.
 * Author: Oder Chiou <oder_chiou@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include  <linux/metricslog.h>
#include <linux/of_gpio.h>

#include "rt551x.h"
#include "rt551x-spi.h"

#define RT551X_RW_BY_SPI
#define RT551X_USE_AMIC


#define VERSION "0.0.9"
#define VERSION_LEN 16
static char fw_ver[VERSION_LEN];
static int rt551x_dsp_set_idle_mode(struct snd_soc_codec *codec, int IdleMode);
static int rt551x_set_dsp_mode(struct snd_soc_codec *codec, int DSPMode);
static struct rt551x_priv *rt551x_pointer;

#define PRIO_TWO_BIGS_TWO_LITTLES_MAX_FREQ 11
extern int set_dynamic_boost(int duration, int prio_mode);

static DBGBUF_MEM dbgBufLast;

#ifdef CONFIG_OF
static const struct of_device_id rt551x_match_table[] = {
	{.compatible = "realtek,rt551x",},
	{ },
};
#endif

/* ALC5514 functions */
static const struct reg_default rt5514_reg[] = {
	{RT551X_BUFFER_VOICE_WP,	0x00000000},
	{RT551X_DSP_WDG_1,		0x00000000},
	{RT551X_DSP_WDG_2,		0x00000000},
	{RT551X_DSP_WDG_3,		0x00000000},
	{RT5514_DSP_CLK_2,		0x00000007},
	{RT5514_DSP_CLK_6,		0x00000000},
	{RT5514_DSP_CLK_7,		0x00000000},
	{RT5514_RESET,			0x00000000},
	{RT5514_PWR_ANA1,		0x00808880},
	{RT5514_PWR_ANA2,		0x00220000},
	{RT5514_VAD_CTRL6,		0xc00007d2},
	{RT5514_EXT_VAD_CTRL,		0x80000080},
	{RT5514_DIG_IO_CTRL,		0x00f00288},
	{RT5514_PAD_CTRL1,		0x00804000},
	{RT5514_DMIC_DATA_CTRL,		0x00000005},
	{RT5514_DIG_SOURCE_CTRL,	0x00000002},
	{RT5514_SRC_CTRL,		0x00000eee},
	{RT5514_DOWNFILTER2_CTRL1,	0x0000882f},
	{RT5514_PLL_SOURCE_CTRL,	0x00000004},
	{RT5514_CLK_CTRL1,		0x38022841},
	{RT5514_CLK_CTRL2,		0x00000000},
	{RT5514_PLL3_CALIB_CTRL1,	0x00400200},
	{RT5514_PLL3_CALIB_CTRL5,	0x40220012},
	{RT5514_DELAY_BUF_CTRL1,	0x7fff006a},
	{RT5514_DELAY_BUF_CTRL3,	0x00000000},
	{RT5514_DOWNFILTER0_CTRL1,	0x00020c2f},
	{RT5514_DOWNFILTER0_CTRL2,	0x00020c2f},
	{RT5514_DOWNFILTER0_CTRL3,	0x00000362},
	{RT5514_DOWNFILTER1_CTRL1,	0x00020c2f},
	{RT5514_DOWNFILTER1_CTRL2,	0x00020c2f},
	{RT5514_DOWNFILTER1_CTRL3,	0x00000362},
	{RT5514_ANA_CTRL_LDO10,		0x00038604},
	{RT5514_ANA_CTRL_LDO18_16,	0x02000345},
	{RT5514_ANA_CTRL_ADC12,		0x0000a2a8},
	{RT5514_ANA_CTRL_ADC21,		0x00001180},
	{RT5514_ANA_CTRL_ADC22,		0x0000aaa8},
	{RT5514_ANA_CTRL_ADC23,		0x00151427},
	{RT5514_ANA_CTRL_MICBST,	0x00002000},
	{RT5514_ANA_CTRL_ADCFED,	0x00000b88},
	{RT5514_ANA_CTRL_INBUF,		0x0000015F},
	{RT5514_ANA_CTRL_VREF,		0x00008d50},
	{RT5514_ANA_CTRL_PLL3,		0x0000000e},
	{RT5514_ANA_CTRL_PLL1_1,	0x00000000},
	{RT5514_ANA_CTRL_PLL1_2,	0x00030220},
	{RT5514_DMIC_LP_CTRL,		0x00000000},
	{RT5514_MISC_CTRL_DSP,		0x00000000},
	{RT5514_DSP_CTRL1,		0x00055149},
	{RT5514_DSP_CTRL3,		0x00000006},
	{RT5514_DSP_CTRL4,		0x00000001},
	{RT551X_VENDOR_ID1,		0x00000001},
	{RT551X_VENDOR_ID2,		RT5514_DEVICE_ID},
};

static void rt5514_enable_dsp_clock(void *param)
{
	struct rt551x_priv *rt551x = (struct rt551x_priv *)param;
	regmap_write(rt551x->regmap, 0x18002000, 0x000010ec);  //reset
	regmap_write(rt551x->regmap, 0x18002200, 0x00028604);  //LDO_I_limit
	regmap_write(rt551x->regmap, 0x18002f00, 0x0005514b);  //(for reset DSP) mini-core reset
	regmap_write(rt551x->regmap, 0x18002f00, 0x00055149);  //(for reset DSP) mini-core reset
	regmap_write(rt551x->regmap, 0x180020a0, 0x00000000);  //DMIC_OUT1/2=HiZ
#ifdef RT551X_USE_AMIC
	regmap_write(rt551x->regmap, 0x18002070, 0x00000150);  //PIN config
#else
	regmap_write(rt551x->regmap, 0x18002070, 0x00000040);  //PIN config
#endif
	/* 33.8MHz start */
	regmap_write(rt551x->regmap, 0x18002240, 0x00000018);  //pll_qn=19+2 (1.3*21=27.3)
	/* 33.8MHz end */
	regmap_write(rt551x->regmap, 0x18002100, 0x0000000b);  //pll3 source=RCOSC, fsi=rt_clk
	regmap_write(rt551x->regmap, 0x18002004, 0x00808b81);  //PU RCOSC, pll3
	regmap_write(rt551x->regmap, 0x18002008, 0x00220000);  //PD ADC1/2
	regmap_write(rt551x->regmap, 0x18002f08, 0x00000005);  //DSP clk source=pll3, ENABLE DSP clk
	regmap_write(rt551x->regmap, 0x18002104, 0x10023541);  //256fs=/4,DMIC_CLK_OUT=/16(disable ad2)
	regmap_write(rt551x->regmap, 0x18002108, 0x00000000);  //clk_sys source=mux_out
	regmap_write(rt551x->regmap, 0x18001100, 0x0000021f);  //DSP clk= 32M*(31+1)/32 =32M
	regmap_write(rt551x->regmap, 0x18002148, 0x80000000);  //DB, pointer
	regmap_write(rt551x->regmap, 0x18002140, 0x3fff00fa);  //DB, pop=4x
	regmap_write(rt551x->regmap, 0x18002148, 0x00000000);  //DB, pointer
	regmap_write(rt551x->regmap, 0x18002124, 0x00220012);  //DFLL reset
	/* 33.8MHz start */
	regmap_write(rt551x->regmap, 0x18002110, 0x000103e8);  //dfll_m=750,dfll_n=1
	/* 33.8MHz end */
	regmap_write(rt551x->regmap, 0x18002124, 0x80220012);  //DFLL,reset DFLL
	regmap_write(rt551x->regmap, 0x18002124, 0xc0220012);  //DFLL
//	regmap_write(rt551x->regmap, 0x18002010, 0x10000772);  //(I2S) i2s format, TDM 4ch
//	regmap_write(rt551x->regmap, 0x180020ac, 0x44000eee);  //(I2S) source sel; tdm_0=ad0, tdm_1=ad1
#ifdef RT551X_USE_AMIC
	regmap_write(rt551x->regmap, 0x18002190, 0x0002082f);  //(ad0)source of AMIC
	regmap_write(rt551x->regmap, 0x18002194, 0x0002082f);  //(ad0)source of AMIC
	regmap_write(rt551x->regmap, 0x18002198, 0x10003162);  //(ad0)ad0 compensation gain = 3dB
	regmap_write(rt551x->regmap, 0x180020d0, 0x00008937);  //(ad2) gain=12+3dB for WOV
	regmap_write(rt551x->regmap, 0x18002220, 0x00002000);  //(ADC1)micbst gain
	regmap_write(rt551x->regmap, 0x18002224, 0x00000800);  //(ADC1)adcfed unmute and 0dB
#else
	regmap_write(rt551x->regmap, 0x18002190, 0x0002042f);  //(ad0) source of DMIC
	regmap_write(rt551x->regmap, 0x18002194, 0x0002042f);  //(ad0) source of DMIC
	regmap_write(rt551x->regmap, 0x18002198, 0x10000362);  //(ad0) DMIC-IN1 L/R select
	regmap_write(rt551x->regmap, 0x180021a0, 0x0002042f);  //(ad1) source of DMIC
	regmap_write(rt551x->regmap, 0x180021a4, 0x0002042f);  //(ad1) source of DMIC
	regmap_write(rt551x->regmap, 0x180021a8, 0x10000362);  //(ad1) DMIC-IN2 L/R select
	regmap_write(rt551x->regmap, 0x180020d0, 0x00008a2f);  //(ad2) gain=24dB for WOV
#endif
	regmap_write(rt551x->regmap, 0x18001114, 0x00000001);  //dsp clk auto switch /* Fix SPI recording */
	regmap_write(rt551x->regmap, 0x18001118, 0x00000000);  //disable clk_bus_autogat_func
	regmap_write(rt551x->regmap, 0x18001104, 0x00000003);  //UART clk=off
	regmap_write(rt551x->regmap, 0x1800201c, 0x69f32067);  //(pitch VAD)fix1
	regmap_write(rt551x->regmap, 0x18002020, 0x50d500a5);  //(pitch VAD)fix2
	regmap_write(rt551x->regmap, 0x18002024, 0x000a0206);  //(pitch VAD)fix3
	regmap_write(rt551x->regmap, 0x18002028, 0x01800114);  //(pitch VAD)fix4
	regmap_write(rt551x->regmap, 0x18002038, 0x00100010);  //(hello VAD)fix1
	regmap_write(rt551x->regmap, 0x1800204c, 0x000503c8);  //(ok VAD)fix1
	regmap_write(rt551x->regmap, 0x18002050, 0x001a0308);  //(ok VAD)fix2
	regmap_write(rt551x->regmap, 0x18002054, 0x50020502);  //(ok VAD)fix3
	regmap_write(rt551x->regmap, 0x18002058, 0x50000d18);  //(ok VAD)fix4
	regmap_write(rt551x->regmap, 0x1800205c, 0x640c0b14);  //(ok VAD)fix5
	regmap_write(rt551x->regmap, 0x18002060, 0x00100001);  //(ok VAD)fix6
	regmap_write(rt551x->regmap, 0x18002fa4, 0x00000000);  //(FW) SENSORY_SVSCORE(for UDT+SID)
	regmap_write(rt551x->regmap, 0x18002fa8, 0x00000000);  //(FW) SENSORY_THRS(for UDT+SID)
	regmap_write(rt551x->regmap, 0x18002fbc, 0x00000000);  //(FW) DLY_BUF_LTC_OFFSET (for ok/hello VAD)
}

static void rt5514_enable_dsp(void *param)
{
	struct rt551x_priv *rt551x = (struct rt551x_priv *)param;
	// the gain setting is 15dB(HW analog) + 15dB(HW digital) + 6dB(SW)
	regmap_write(rt551x->regmap, 0x18001028, 0x0001000a);  //(FW) DRIVER_CTRL0: (1)VAD timeout[7:0], /0.1s; (2) buffer mode[15:14]:0=key phrase+voice command, 1: voice command, 2: key phrase
#ifdef RT551X_USE_AMIC
	regmap_write(rt551x->regmap, 0x18002004, 0x00800bbf);  //PU PLL3 and RCOSC, LDO21
	regmap_write(rt551x->regmap, 0x18002008, 0x00223700);  //PU ADC2
	regmap_write(rt551x->regmap, 0x1800222c, 0x00008c50);  //(ADC2) BG VREF=self-bias
#else
	regmap_write(rt551x->regmap, 0x18002004, 0x00808b81);  //PU RCOSC, pll3
	regmap_write(rt551x->regmap, 0x18002008, 0x00220000);  //PD ADC1/2
#endif
	regmap_write(rt551x->regmap, 0x18002f00, 0x00055149);  //dsp stop
#ifdef RT551X_USE_AMIC
	/* 33.8MHz start */
	regmap_write(rt551x->regmap, 0x18002104, 0x44025751);  //CLK, 256fs=/6,
	/* 33.8MHz end */
	regmap_write(rt551x->regmap, 0x180020a4, 0x00804002);  //(PATH) ADC2->ad2 ,ad2(db_PCM)->IB2
	regmap_write(rt551x->regmap, 0x18002228, 0x0000015F);  //(PATH)AMIC-R->ADC2 (0x143 for AMIC-L, 0x153 for AMIC-R)
	regmap_write(rt551x->regmap, 0x18002210, 0x80000000);  //(ADC2) reset ADC2
	regmap_write(rt551x->regmap, 0x18002210, 0x00000000);  //(ADC2) reset ADC2
#else
	regmap_write(rt551x->regmap, 0x18002104, 0x14023541);  //CLK, 256fs=/4,DMIC_CLK_OUT=/16(enable ad2)
	regmap_write(rt551x->regmap, 0x180020a4, 0x00808002);  //(PATH) DMIC_IN1(ri)->ad2, ad2(db_PCM)->IB2
#endif
	regmap_write(rt551x->regmap, 0x18002f08, 0x00000005);  //DSP clk source=pll3, ENABLE DSP clk
	regmap_write(rt551x->regmap, 0x18002030, 0x800007d3);  //(opt)VAD, clr and enable pitch/hello VAD (0x800007d2 for ok VAD, 0x800007d3 for pitch/hello VAD)
	regmap_write(rt551x->regmap, 0x18002064, 0x80000002);  //(opt)VAD, clr and enable ok VAD (0x80000003 for ok VAD, 0x80000002 for pitch/hello VAD)
	regmap_write(rt551x->regmap, 0x1800206c, 0x80000080);  //(opt)VAD type sel(..90h:hello, ..80h:pitch, ..A0h:ok)
	regmap_write(rt551x->regmap, 0x18002f00, 0x00055148);  //dsp run
	regmap_write(rt551x->regmap, 0x18002e04, 0x00000000);  //clear IRQ
	regmap_write(rt551x->regmap, 0x18002030, 0x000007d3);  //(opt)VAD, release and enable pitch/hello VAD (0x7d2 for ok VAD, 0x7d3 for pitch/hello VAD)
	regmap_write(rt551x->regmap, 0x18002064, 0x00000002);  //(opt)VAD, release and enable ok VAD (0x3 for ok VAD, 0x2 for pitch/hello VAD)
}

static void rt5514_reset(void *param)
{
	struct rt551x_priv *rt551x = (struct rt551x_priv *)param;
	regmap_write(rt551x->regmap, 0x18002000, 0x000010ec);  //regtop reset
	regmap_write(rt551x->regmap, 0xfafafafa, 0x00000001);  //i2c bypass=1
	regmap_write(rt551x->regmap, 0x18002004, 0x00808F81);
	regmap_write(rt551x->regmap, 0x18002008, 0x00270000);
	regmap_write(rt551x->regmap, 0x18002f08, 0x00000006);  //dsp clk source=i2cosc
	regmap_write(rt551x->regmap, 0x18002f10, 0x00000000);  //dsp TOP reset
	regmap_write(rt551x->regmap, 0x18002f10, 0x00000001);  //dsp TOP reset
	regmap_write(rt551x->regmap, 0xfafafafa, 0x00000000);  //i2c bypass=0
	regmap_write(rt551x->regmap, 0x18002000, 0x000010ec);  //regtop reset
	regmap_write(rt551x->regmap, 0x18001104, 0x00000007);
	regmap_write(rt551x->regmap, 0x18001108, 0x00000000);
	regmap_write(rt551x->regmap, 0x1800110c, 0x00000000);
	regmap_write(rt551x->regmap, 0x180014c0, 0x00000001);
	regmap_write(rt551x->regmap, 0x180014c0, 0x00000000);
	regmap_write(rt551x->regmap, 0x180014c0, 0x00000001);
}

static bool rt5514_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RT551X_VENDOR_ID1:
	case RT551X_VENDOR_ID2:
	case RT551X_BUFFER_VOICE_WP:
	case RT551X_DSP_WDG_1:
	case RT551X_DSP_WDG_2:
	case RT551X_DSP_WDG_3:
	case RT5514_DSP_CLK_2:
	case RT5514_DSP_CLK_6:
	case RT5514_DSP_CLK_7:
	case RT5514_PWR_ANA2:
		return true;

	default:
		return false;
	}
}

static bool rt5514_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RT551X_BUFFER_VOICE_BASE:
	case RT551X_BUFFER_VOICE_LIMIT:
	case RT551X_BUFFER_VOICE_RP:
	case RT551X_BUFFER_VOICE_WP:
	case RT551X_REC_RP:
	case RT551X_SW_RING_SIZE:
	case RT551X_SW_RING_RP:
	case RT5514_DSP_CLK_2:
	case RT5514_DSP_CLK_6:
	case RT5514_DSP_CLK_7:
	case RT551X_DRIVER_CTRL0:
	case RT551X_DSP_WDG_1:
	case RT551X_DSP_WDG_2:
	case RT551X_DSP_WDG_3:
	case RT5514_RESET:
	case RT5514_PWR_ANA1:
	case RT5514_PWR_ANA2:
	case RT5514_VAD_CTRL6:
	case RT5514_EXT_VAD_CTRL:
	case RT5514_DIG_IO_CTRL:
	case RT5514_PAD_CTRL1:
	case RT5514_DMIC_DATA_CTRL:
	case RT5514_DIG_SOURCE_CTRL:
	case RT5514_SRC_CTRL:
	case RT5514_DOWNFILTER2_CTRL1:
	case RT5514_PLL_SOURCE_CTRL:
	case RT5514_CLK_CTRL1:
	case RT5514_CLK_CTRL2:
	case RT5514_PLL3_CALIB_CTRL1:
	case RT5514_PLL3_CALIB_CTRL5:
	case RT5514_DELAY_BUF_CTRL1:
	case RT5514_DELAY_BUF_CTRL3:
	case RT5514_DOWNFILTER0_CTRL1:
	case RT5514_DOWNFILTER0_CTRL2:
	case RT5514_DOWNFILTER0_CTRL3:
	case RT5514_DOWNFILTER1_CTRL1:
	case RT5514_DOWNFILTER1_CTRL2:
	case RT5514_DOWNFILTER1_CTRL3:
	case RT5514_ANA_CTRL_LDO10:
	case RT5514_ANA_CTRL_LDO18_16:
	case RT5514_ANA_CTRL_ADC12:
	case RT5514_ANA_CTRL_ADC21:
	case RT5514_ANA_CTRL_ADC22:
	case RT5514_ANA_CTRL_ADC23:
	case RT5514_ANA_CTRL_MICBST:
	case RT5514_ANA_CTRL_ADCFED:
	case RT5514_ANA_CTRL_INBUF:
	case RT5514_ANA_CTRL_VREF:
	case RT5514_ANA_CTRL_PLL3:
	case RT5514_ANA_CTRL_PLL1_1:
	case RT5514_ANA_CTRL_PLL1_2:
	case RT5514_DMIC_LP_CTRL:
	case RT5514_MISC_CTRL_DSP:
	case RT5514_DSP_CTRL1:
	case RT5514_DSP_CTRL3:
	case RT5514_DSP_CTRL4:
	case RT551X_VENDOR_ID1:
	case RT551X_VENDOR_ID2:
		return true;

	default:
		return false;
	}
}

/* ALC5518 functions */
static const struct reg_default rt5518_reg[] = {
	{RT551X_BUFFER_VOICE_BASE,		0x00000000},
	{RT551X_BUFFER_VOICE_LIMIT,		0x00000000},
	{RT551X_BUFFER_VOICE_RP,		0x00000000},
	{RT551X_BUFFER_VOICE_WP,		0x00000000},
	{RT5518_DSP_CLK_1,		0x0000031f},
	{RT5518_DSP_CLK_4,		0x00000003},
	{RT5518_DSP_CLK_2,		0x00000000},
	{RT5518_DSP_CLK_3,		0x00000000},
	{RT551X_DSP_WDG_1,		0x00000000},
	{RT551X_DSP_WDG_2,		0x00000000},
	{RT551X_DSP_WDG_3,		0x00000000},
	{RT551X_DSP_WDG_4,		0x00000000},
	{RT551X_DSP_WDG_5,		0x00000000},
	{RT5518_RESET,			0x0000001f},
	{RT5518_PWR_ANA1,		0x00000880},
	{RT5518_PWR_ANA2,		0x00220000},
	{RT5518_DIG_OUT_GATE,		0x00000002},
	{RT5518_I2S_CTRL1,		0x00030320},
	{RT5518_I2S_CTRL2,		0x00000000},
	{RT5518_I2S_CTRL3,		0x00000010},
	{RT5518_I2S_CTRL4,		0x00000001},
	{RT5518_SCAN_CTRL1,		0x00000000},
	{RT5518_EXT_VAD_CTRL,		0x0c000000},
	{RT5518_DIG_IO_CTRL,		0x210000ff},
	{RT5518_GPIO_CTRL1,		0x00000000},
	{RT5518_GPIO_CTRL2,		0x00000000},
	{RT5518_GPIO_CTRL3,		0x00000000},
	{RT5518_PAD_CTRL1,		0x15800000},
	{RT5518_PAD_CTRL2,		0x00005000},
	{RT5518_TEST_CTRL1,		0x00000000},
	{RT5518_TEST_CTRL2,		0x00000000},
	{RT5518_TEST_CTRL3,		0x00000000},
	{RT5518_IRQ_CTRL1,		0x00000001},
	{RT5518_DMIC_DATA_CTRL,		0x00000003},
	{RT5518_AUDIO_PATH_CTRL,	0x00000000},
	{RT5518_DOWNFILTER2_CTRL1,	0x0000882f},
	{RT5518_PLL_SOURCE_CTRL,	0x00000004},
	{RT5518_CLK_CTRL1,		0x30003830},
	{RT5518_CLK_CTRL2,		0x00000000},
	{RT5518_DFLL_CTRL1,		0x00400200},
	{RT5518_DFLL_CTRL2,		0x00010001},
	{RT5518_DFLL_CTRL3,		0x00000400},
	{RT5518_DFLL_CTRL3_2,		0x00000000},
	{RT5518_DFLL_CTRL4,		0x00000000},
	{RT5518_DFLL_CTRL5,		0x40220012},
	{RT5518_DFLL_STA1,		0x00000000},
	{RT5518_DFLL_STA2,		0x00000000},
	{RT5518_ASRC_DWNFILT0_CTRL1,	0x00000001},
	{RT5518_ASRC_DWNFILT0_CTRL2,	0x10000000},
	{RT5518_DOWNFILTER0_CTRL1,	0x00020c2f},
	{RT5518_DOWNFILTER0_CTRL2,	0x00020c2f},
	{RT5518_DOWNFILTER0_CTRL3,	0x00000362},
	{RT5518_ANA_CTRL_LDO10,		0x00038600},
	{RT5518_ANA_CTRL_LDO18_16,	0x02000345},
	{RT5518_ANA_PLL3_LDO_21,	0x0000a821},
	{RT5518_ANA_CTRL_ADC21,		0x00001180},
	{RT5518_ANA_CTRL_ADC22,		0x00000000},
	{RT5518_ANA_CTRL_ADC23,		0x00171023},
	{RT5518_ANA_CTRL_INBUF,		0x00000105},
	{RT5518_ANA_CTRL_VREF,		0x00008c50},
	{RT5518_ANA_CTRL_MBIAS,		0x00000020},
	{RT5518_ANA_CTRL_PLL3,		0x0000000e},
	{RT5518_ANA_CTRL_PLL21,		0x00000000},
	{RT5518_ANA_CTRL_PLL22,		0x00030220},
	{RT5518_ANA_CTRL_PLL23,		0x00007334},
	{RT5518_AGC_FUNC_CTRL1,		0x21408205},
	{RT5518_AGC_FUNC_CTRL2,		0x42005f5f},
	{RT5518_AGC_FUNC_CTRL3,		0x001ff542},
	{RT5518_AGC_FUNC_CTRL4,		0x74180b03},
	{RT5518_AGC_FUNC_CTRL5,		0x50100810},
	{RT5518_AGC_FUNC_CTRL6,		0x07800000},
	{RT5518_AGC_FUNC_CTRL7,		0x96000055},
	{RT5518_AGC_FUNC_STAT,		0x00000000},
	{RT5518_SCAN_STAT,		0x0000000c},
	{RT5518_I2CBYPA_STAT,		0x00000000},
	{RT5518_I2CBURSTL_ADDR,		0x00000000},
	{RT5518_I2CBURSTL_MEM_ADDR,	0x00000000},
	{RT5518_I2CBURST_MEM_ADDR,	0x00000000},
	{RT5518_IRQ_CTRL2,		0x00000000},
	{RT5518_GPIO_INTR_CTRL,		0xc0c0c0c0},
	{RT5518_GPIO_INTR_STAT,		0x00000000},
	{RT5518_DSP_CTRL1,		0x00055189},
	{RT5518_DSP_CTRL2,		0x00000000},
	{RT5518_DSP_CTRL3,		0x00000006},
	{RT5518_DSP_CTRL4,		0x00000001},
	{RT5518_DSP_CTRL5,		0x00000011},
	{RT551X_VENDOR_ID1,		0x00000000},
	{RT551X_VENDOR_ID2,		0x10ec5518},
	{RT551X_I2C_BYPASS,		0x00000000},
};

static void rt5518_enable_dsp_clock(void *param)
{
	struct rt551x_priv *rt551x = (struct rt551x_priv *)param;
	int val;
	regmap_write(rt551x->regmap, 0x18002000,0x000010ec); //<SW reset> regtop reset
	regmap_write(rt551x->regmap, 0x18002000,0x55185518); //<SW reset> minicore reset
	regmap_write(rt551x->regmap, 0x18002000,0x23792379); //<SW reset> minitop reset
	regmap_write(rt551x->regmap, 0x18002200,0x00028600); //LDO_I_limit
	regmap_write(rt551x->regmap, 0x180020A0,0x00000000); //DMIC_OUT1/2=HiZ
	regmap_write(rt551x->regmap, 0x18002070,0x20000000); //PIN config
	regmap_write(rt551x->regmap, 0x18002240,0x00000017); //pll3(QN)=RCOSC*(23+2)=32.5M
	regmap_write(rt551x->regmap, 0x18002100,0x0000000B); //pll3 source=RCOSC, fsi=rt_clk
	regmap_write(rt551x->regmap, 0x18002004,0x00000B81); //PU RCOSC, pll3
	regmap_write(rt551x->regmap, 0x18002008,0x00220000); //PD ADC2
	regmap_write(rt551x->regmap, 0x18002F08,0x00000003); //DSP clk source=pll_out, enable dsp clk
	regmap_write(rt551x->regmap, 0x18002104,0x10005850); //ana2=/16, 256fs=/8, (disable ad2)
	regmap_write(rt551x->regmap, 0x18002108,0x00000000); //clk_sys source=mux_out
	regmap_write(rt551x->regmap, 0x18001100,0x00000213); //DSP clk= 32M*(19+1)/32 =20M
	regmap_write(rt551x->regmap, 0x18002124,0x00220012); //DFLL reset
	regmap_write(rt551x->regmap, 0x18002110,0x000103E8); //DFLL, set m/n (m=1000, n=1), (mux_out=32.768k*1000/1=32.768M)
	regmap_write(rt551x->regmap, 0x18002124,0x80220012); //DFLL,reset DFLL
	regmap_write(rt551x->regmap, 0x18002124,0xC0220012); //DFLL enable
	regmap_write(rt551x->regmap, 0x18002190,0x0002082f); //(ad0)source of AMIC_IN
	regmap_write(rt551x->regmap, 0x18002194,0x0002082f); //(ad0)source of AMIC_IN
	regmap_write(rt551x->regmap, 0x18002198,0x10000162); //(ad0)ad0 compensation gain = 0dB
	regmap_write(rt551x->regmap, 0x180020D0,0x0000892f); //(ad2) gain=12dB
	regmap_read(rt551x->regmap, 0x18002ff0, &val);       //ID1, get chip version, 0 is 5518, 1 is 5518B
	if ((val & RT551X_VENDOR_ID1_MASK) == RT551X_VENDOR_ID1_5518)
		regmap_write(rt551x->regmap, 0x18001114,0x00000000); //dsp clk auto switch disable for 5518 chip
	else // RT551X_VENDOR_ID1_5518B or later chip version
		regmap_write(rt551x->regmap, 0x18001114,0x00000001); //dsp clk auto switch enable for 5518B and later chips
	regmap_write(rt551x->regmap, 0x18001118,0x00000001); //reduce DSP power
	regmap_write(rt551x->regmap, 0x18002228,0x0000014D); //fix INBUF bias (INBUF=15dB)
	regmap_write(rt551x->regmap, 0x1800221c,0x00171023); //fix ADC2 bias
	regmap_write(rt551x->regmap, 0x18002218,0x0000aaa8); //fix ADC2 bias
	regmap_write(rt551x->regmap, 0x18002204,0x02000345); //fix LDO16 bias
}

static void rt5518_enable_dsp(void *param)
{
	struct rt551x_priv *rt551x = (struct rt551x_priv *)param;
	regmap_write(rt551x->regmap, 0x18002004,0x00000BBF); //PU PLL3 and RCOSC, LDO21
	regmap_write(rt551x->regmap, 0x18002008,0x00223700); //PU ADC2
	regmap_write(rt551x->regmap, 0x18002F00,0x00055189); //dsp stop
	regmap_write(rt551x->regmap, 0x18002124,0x00220012); //DFLL reset
	regmap_write(rt551x->regmap, 0x18002110,0x000103E8); //DFLL, set m/n (m=1000, n=1), (mux_out=32.768k*1000/1=32.768M)
	regmap_write(rt551x->regmap, 0x18002124,0x80220012); //DFLL,reset DFLL
	regmap_write(rt551x->regmap, 0x18002124,0xC0220012); //DFLL enable
	regmap_write(rt551x->regmap, 0x18002104,0x44005850); //ana2=/16, 256fs=/8, (enable ADC2/ad2)
	regmap_write(rt551x->regmap, 0x180020A4,0x20000000); //(PATH)ADC2->ad2, ad2(db_PCM)->IB2
	regmap_write(rt551x->regmap, 0x18002214,0x80001180); //(ADC2) reset ADC2
	regmap_write(rt551x->regmap, 0x18002214,0x00001180); //(ADC2) reset ADC2
	regmap_write(rt551x->regmap, 0x18002F08,0x00000005); //DSP clk source=mux_out, ENABLE DSP clk
	regmap_write(rt551x->regmap, 0x18002F00,0x00055188); //dsp run
	regmap_write(rt551x->regmap, 0x18002E04,0x00000000); //clear IRQ
}

static void rt5518_reset(void *param)
{
	struct rt551x_priv *rt551x = (struct rt551x_priv *)param;
	int val,val2;
	regmap_write(rt551x->regmap, 0x18002000, 0x000010ec);  //<SW reset> regtop reset
	regmap_write(rt551x->regmap, 0x18002000, 0x55185518);  //<SW reset> minicore reset
	regmap_write(rt551x->regmap, 0x18002000, 0x23792379);  //<SW reset> minitop reset
	/* when the SW reset does not work, we need HW reset */
	regmap_read(rt551x->regmap, 0x18002004, &val);
	regmap_read(rt551x->regmap, 0x180020d0, &val2);
	if (( val != 0x00000880) || ( val2 != 0x0000882f)){
		gpio_set_value(rt551x->hw_reset_pin, 0);
		gpio_set_value(rt551x->hw_reset_pin, 1);
		printk("SW reset failed, use HW reset instead.\n");
	}
}

static bool rt5518_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RT551X_BUFFER_VOICE_WP:
	case RT5514_DSP_CLK_1:
	case RT5514_DSP_CLK_2:
	case RT5514_DSP_CLK_3:
	case RT5514_DSP_CLK_4:
	case RT551X_DSP_WDG_1:
	case RT551X_DSP_WDG_2:
	case RT551X_DSP_WDG_3:
	case RT5518_CLK_CTRL1:
	case RT5518_DSP_CTRL1:
	case RT5514_PWR_ANA2:
	case RT551X_VENDOR_ID1:
	case RT551X_VENDOR_ID2:
		return true;
	default:
		return false;
	}
}

static bool rt5518_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RT551X_BUFFER_VOICE_BASE:
	case RT551X_BUFFER_VOICE_LIMIT:
	case RT551X_BUFFER_VOICE_RP:
	case RT551X_BUFFER_VOICE_WP:
	case RT551X_REC_RP:
	case RT551X_SW_RING_SIZE:
	case RT551X_SW_RING_RP:
	case RT551X_SW_RING_WP:
	case RT5518_DSP_CLK_1:
	case RT5518_DSP_CLK_2:
	case RT5518_DSP_CLK_3:
	case RT5518_DSP_CLK_4:
	case RT551X_DRIVER_CTRL0:
	case RT551X_DSP_WDG_1:
	case RT551X_DSP_WDG_2:
	case RT551X_DSP_WDG_3:
	case RT551X_DSP_WDG_4:
	case RT551X_DSP_WDG_5:
	case RT5518_RESET:
	case RT5518_PWR_ANA1:
	case RT5518_PWR_ANA2:
	case RT5518_DIG_OUT_GATE:
	case RT5518_SCAN_CTRL1:
	case RT5518_EXT_VAD_CTRL:
	case RT5518_DIG_IO_CTRL:
	case RT5518_GPIO_CTRL1:
	case RT5518_GPIO_CTRL2:
	case RT5518_GPIO_CTRL3:
	case RT5518_PAD_CTRL1:
	case RT5518_PAD_CTRL2:
	case RT5518_TEST_CTRL1:
	case RT5518_TEST_CTRL2:
	case RT5518_TEST_CTRL3:
	case RT5518_IRQ_CTRL1:
	case RT5518_DMIC_DATA_CTRL:
	case RT5518_AUDIO_PATH_CTRL:
	case RT5518_DOWNFILTER2_CTRL1:
	case RT5518_PLL_SOURCE_CTRL:
	case RT5518_CLK_CTRL1:
	case RT5518_CLK_CTRL2:
	case RT5518_DFLL_CTRL1:
	case RT5518_DFLL_CTRL2:
	case RT5518_DFLL_CTRL3:
	case RT5518_DFLL_CTRL3_2:
	case RT5518_DFLL_CTRL4:
	case RT5518_DFLL_CTRL5:
	case RT5518_DFLL_STA1:
	case RT5518_DFLL_STA2:
	case RT5518_ASRC_DWNFILT0_CTRL1:
	case RT5518_ASRC_DWNFILT0_CTRL2:
	case RT5518_DOWNFILTER0_CTRL1:
	case RT5518_DOWNFILTER0_CTRL2:
	case RT5518_DOWNFILTER0_CTRL3:
	case RT5518_ANA_CTRL_LDO10:
	case RT5518_ANA_CTRL_LDO18_16:
	case RT5518_ANA_PLL3_LDO_21:
	case RT5518_ANA_CTRL_ADC21:
	case RT5518_ANA_CTRL_ADC22:
	case RT5518_ANA_CTRL_ADC23:
	case RT5518_ANA_CTRL_INBUF:
	case RT5518_ANA_CTRL_VREF:
	case RT5518_ANA_CTRL_MBIAS:
	case RT5518_ANA_CTRL_PLL3:
	case RT5518_ANA_CTRL_PLL21:
	case RT5518_ANA_CTRL_PLL22:
	case RT5518_ANA_CTRL_PLL23:
	case RT5518_AGC_FUNC_CTRL1:
	case RT5518_AGC_FUNC_CTRL2:
	case RT5518_AGC_FUNC_CTRL3:
	case RT5518_AGC_FUNC_CTRL4:
	case RT5518_AGC_FUNC_CTRL5:
	case RT5518_AGC_FUNC_CTRL6:
	case RT5518_AGC_FUNC_CTRL7:
	case RT5518_AGC_FUNC_STAT:
	case RT5518_SCAN_STAT:
	case RT5518_I2CBYPA_STAT:
	case RT5518_I2CBURSTL_ADDR:
	case RT5518_I2CBURSTL_MEM_ADDR:
	case RT5518_I2CBURST_MEM_ADDR:
	case RT5518_IRQ_CTRL2:
	case RT5518_GPIO_INTR_CTRL:
	case RT5518_GPIO_INTR_STAT:
	case RT5518_DSP_CTRL1:
	case RT5518_DSP_CTRL2:
	case RT5518_DSP_CTRL3:
	case RT5518_DSP_CTRL4:
	case RT5518_DSP_CTRL5:
	case RT551X_VENDOR_ID1:
	case RT551X_VENDOR_ID2:
	case RT551X_I2C_BYPASS:
		return true;
	default:
		return false;
	}
}

static int rt5518_hw_reset_gpio_init(struct rt551x_priv *rt551x)
{
	int ret = 0;
	struct device_node *node = NULL;

	node = of_find_matching_node(node, rt551x_match_table);
	if (node) {
		rt551x->hw_reset_pin = of_get_named_gpio(node, "rt5518-reset", 0);
		if (rt551x->hw_reset_pin < 0)
			printk("[rt551x] not find rt5518-reset\n");

	}
	ret = gpio_request(rt551x->hw_reset_pin, "rt5518_hw_reset");
	if(ret){
		printk("gpio %d request failed!,ret:%d\n", rt551x->hw_reset_pin, ret);
		goto err_gpio;
	}
	ret = gpio_direction_output(rt551x->hw_reset_pin, 1);
	if(ret){
		printk("gpio %d unable to set as output!\n", rt551x->hw_reset_pin);
		goto err_free_gpio;
	}

	printk("GPIO pin requested ok, rt5518_hw_reset = %s\n", gpio_get_value(rt551x->hw_reset_pin)? "H":"L");
	return 0;
err_free_gpio:
	if (gpio_is_valid(rt551x->hw_reset_pin))
		gpio_free(rt551x->hw_reset_pin);
err_gpio:
	return ret;
}

static const char *rt551x_dsp_mode[] = {
	"Stop", "Mode1", "Mode2", "Mode3", "Mode4", "Mode5", "Mode6", "Mode7", "Mode8"
};
/* Note that, the binary naming rule of DSP FW may be different to the CSM layer */
static const char *rt551x_dsp_binary_name[] = {
	"",
	"SMicBin_rt5518_mode11.dat", "SMicBin_rt5518_mode12.dat", "SMicBin_rt5518_mode13.dat", "SMicBin_rt5518_mode14.dat",
	"SMicBin_rt5518_mode21.dat", "SMicBin_rt5518_mode22.dat", "SMicBin_rt5518_mode23.dat", "SMicBin_rt5518_mode24.dat"
};

static const SOC_ENUM_SINGLE_DECL(rt551x_dsp_mod_enum, 0, 0,
	rt551x_dsp_mode);

static int rt551x_dsp_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt551x->dsp_enabled;

	return 0;
}

static unsigned int rt551x_4byte_le_to_uint(const u8 *data)
{
	return data[0] | data[1] << 8 | data[2] << 16 | data[3] << 24;
}

void rt551x_parse_header(struct snd_soc_codec *codec, const u8 *buf)
{
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);
//	const struct firmware *fw = NULL;
#ifdef CONFIG_SND_SOC_RT551X_TEST_ONLY
	u8 *cmp_buf = NULL;
#endif
	SMicFWHeader sMicFWHeader;
//	SMicFWSubHeader sMicFWSubHeader;

	int i, offset = 0;
//	char file_path[32];

	sMicFWHeader.Sync = rt551x_4byte_le_to_uint(buf);
	dev_dbg(codec->dev, "sMicFWHeader.Sync = %08x\n", sMicFWHeader.Sync);

	offset += 4;
	sMicFWHeader.Version =  rt551x_4byte_le_to_uint(buf + offset);
	dev_dbg(codec->dev, "sMicFWHeader.Version = %08x\n",
		sMicFWHeader.Version);
	sprintf(fw_ver, "%d", sMicFWHeader.Version);

	offset += 4;
	sMicFWHeader.NumBin = rt551x_4byte_le_to_uint(buf + offset);
	dev_dbg(codec->dev, "sMicFWHeader.NumBin = %08x\n",
		sMicFWHeader.NumBin);

	sMicFWHeader.BinArray =
		kzalloc(sizeof(SMicBinInfo) * sMicFWHeader.NumBin, GFP_KERNEL);

	offset += 4 * 9;

	for (i = 0 ; i < sMicFWHeader.NumBin; i++) {
		offset += 4;
		sMicFWHeader.BinArray[i].Offset =
			rt551x_4byte_le_to_uint(buf + offset);
		dev_dbg(codec->dev, "sMicFWHeader.BinArray[%d].Offset = %08x\n",
			i, sMicFWHeader.BinArray[i].Offset);

		offset += 4;
		sMicFWHeader.BinArray[i].Size =
			rt551x_4byte_le_to_uint(buf + offset);
		dev_dbg(codec->dev, "sMicFWHeader.BinArray[%d].Size = %08x\n",
			i, sMicFWHeader.BinArray[i].Size);

		offset += 4;
		sMicFWHeader.BinArray[i].Addr =
			rt551x_4byte_le_to_uint(buf + offset);
		dev_dbg(codec->dev, "sMicFWHeader.BinArray[%d].Addr = %08x\n",
			i, sMicFWHeader.BinArray[i].Addr);

		rt551x_spi_burst_write(sMicFWHeader.BinArray[i].Addr,
			buf + sMicFWHeader.BinArray[i].Offset,
			((sMicFWHeader.BinArray[i].Size/8)+1)*8);

#ifdef CONFIG_SND_SOC_RT551X_TEST_ONLY
		if (rt551x->dsp_fw_check) {
			cmp_buf = kmalloc(((sMicFWHeader.BinArray[i].Size/8)+1)*8, GFP_KERNEL);
			if (cmp_buf == NULL) {
				dev_err(codec->dev, "Failed to allocate kernel memory\n");
				goto exit_BinArray;
			}
			rt551x_spi_burst_read(sMicFWHeader.BinArray[i].Addr,
				cmp_buf, ((sMicFWHeader.BinArray[i].Size/8)+1)*8);
			if (memcmp(buf + sMicFWHeader.BinArray[i].Offset, cmp_buf,
				sMicFWHeader.BinArray[i].Size)) {
				rt551x->dsp_fw_check = 3;
				dev_dbg(codec->dev, "sMicFWHeader.BinArray[%d].Addr = %08x firmware check failed\n",
					i, sMicFWHeader.BinArray[i].Addr);
				kfree(cmp_buf);
				goto exit_BinArray;
			} else {
				rt551x->dsp_fw_check = 2;
				dev_dbg(codec->dev, "sMicFWHeader.BinArray[%d].Addr = %08x firmware check successful\n",
					i, sMicFWHeader.BinArray[i].Addr);
				kfree(cmp_buf);
			}
		}
#endif
	}

exit_BinArray:
	if (sMicFWHeader.BinArray)
		kfree(sMicFWHeader.BinArray);
}

static int rt551x_dsp_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);
	int PrevIdleMode;
	mutex_lock(&rt551x->dspcontrol_lock);
	PrevIdleMode = rt551x->dsp_idle;
	pr_info("%s record idle mode [%d] \n", __func__, PrevIdleMode);
	rt551x_set_dsp_mode(codec, ucontrol->value.integer.value[0]);
	rt551x_dsp_set_idle_mode(rt551x->codec, PrevIdleMode);
	mutex_unlock(&rt551x->dspcontrol_lock);
	return 0;
}

static int rt551x_set_dsp_mode(struct snd_soc_codec *codec, int DSPMode)
{
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);
	const struct firmware *fw = NULL;
	char file_path[32];
	bool restart = false;

	if  (rt551x->dsp_enabled == DSPMode)
		return 0;

	if ((DSPMode < 0) || (DSPMode >=  sizeof (rt551x_dsp_mode) / sizeof(rt551x_dsp_mode[0])) )
	{
		return -EINVAL;
	}
	if (codec->dapm.bias_level == SND_SOC_BIAS_OFF)
	{
		if (rt551x->dsp_enabled && DSPMode)
			restart = true;

		rt551x->dsp_enabled = DSPMode;

		if (rt551x->dsp_enabled)
		{
			if (restart)
			{
				set_pcm_is_readable(0);
				regcache_cache_bypass(rt551x->regmap, true);
				rt551x_reset(rt551x);
				regcache_cache_bypass(rt551x->regmap, false);
			}
			regcache_cache_bypass(rt551x->regmap, true);
			rt551x_enable_dsp_clock(rt551x);
			if (rt551x->chip_id == RT5514_DEVICE_ID)
				request_firmware(&fw, "SMicBin_rt5514.dat", codec->dev);
			else if (rt551x->chip_id == RT5518_DEVICE_ID)
				request_firmware(&fw, rt551x_dsp_binary_name[rt551x->dsp_enabled], codec->dev);
			if (fw)
			{
				rt551x_parse_header(codec, fw->data);
				release_firmware(fw);
				fw = NULL;
			}

			else
			{
				printk("%s(%d)Firmware not found\n",__func__,__LINE__);
#ifdef CONFIG_SND_SOC_RT551X_TEST_ONLY
				if (rt551x->dsp_fw_check)
				{
					rt551x->dsp_fw_check = 4;
				}
#endif
				return -EINVAL;
			}

			rt551x_enable_dsp(rt551x);
			regcache_cache_bypass(rt551x->regmap, false);
			msleep (150);
			// set idle state to off
            pr_info("%s  turn off idle mode in reset 1 \n", __func__);
			rt551x_dsp_set_idle_mode(rt551x->codec, 0);
			set_pcm_is_readable(1);
		}
		else
		{
			set_pcm_is_readable(0);
			regcache_cache_bypass(rt551x->regmap, true);
			rt551x_reset(rt551x);
			regcache_cache_bypass(rt551x->regmap, false);
			regcache_mark_dirty(rt551x->regmap);
			regcache_sync(rt551x->regmap);
			// set idle state to on
			rt551x_dsp_set_idle_mode(rt551x->codec, 1);
		}
	}

	return 0;
}

#ifdef CONFIG_SND_SOC_RT551X_TEST_ONLY
static int rt551x_dsp_fw_check_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt551x->dsp_fw_check;

	return 0;
}

static int rt551x_dsp_fw_check_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);

	rt551x->dsp_fw_check = ucontrol->value.integer.value[0];

	return 0;
}

static int rt551x_sw_reset_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt551x->sw_reset;

	return 0;
}

static int rt551x_sw_reset_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);

	rt551x->sw_reset = ucontrol->value.integer.value[0];
	if (rt551x->sw_reset){
		printk("%s(%d)sw reset\n",__func__,__LINE__);
		rt551x_reset(rt551x);
	}
	printk("%s(%d)sw reset:%d\n",__func__,__LINE__,rt551x->sw_reset);
	return 0;
}

static int rt551x_dsp_stop_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt551x->dsp_stop;

	return 0;
}

static int rt551x_dsp_stop_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);
	unsigned int val = 0;

	rt551x->dsp_stop = ucontrol->value.integer.value[0];
	printk("%s(%d)dsp stall:%d\n",__func__,__LINE__,rt551x->dsp_stop);
	if (rt551x->dsp_stop){
		regmap_update_bits(rt551x->regmap, RT5514_DSP_CTRL1,
			(0x1 << 0), (0x1 << 0));
		regmap_update_bits(rt551x->regmap, RT5514_DSP_CTRL1,
			(0x1 << 0), (0x0 << 0));
		regmap_update_bits(rt551x->regmap, RT5514_DSP_CTRL1,
			(0x1 << 0), (0x1 << 0));
		regmap_read(rt551x->regmap, RT5514_DSP_CTRL1, &val);
		printk("%s(%d)stall, 0x18002f00:0x%x\n",__func__,__LINE__,val);
	} else {
		printk("%s(%d)dsp run\n",__func__,__LINE__);
		regmap_update_bits(rt551x->regmap, RT5514_DSP_CTRL1,
			(0x1 << 0), (0x0 << 0));
	}
	return 0;
}

static int rt551x_dsp_core_reset_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt551x->dsp_core_reset;
	return 0;
}

static int rt551x_dsp_core_reset_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);
	unsigned int val = 0;
	mutex_lock(&rt551x->dspcontrol_lock);
	rt551x->dsp_core_reset = ucontrol->value.integer.value[0];
	printk("%s(%d)dsp core reset:%d\n",__func__,__LINE__,rt551x->dsp_core_reset);
	if (rt551x->dsp_core_reset){
		regmap_update_bits(rt551x->regmap, RT5514_DSP_CTRL1,
			(0x1 << 1), (0x1 << 1));
		regmap_read(rt551x->regmap, RT5514_DSP_CTRL1, &val);
		printk("%s(%d)core reset, 0x18002f00:0x%x\n",__func__,__LINE__,val);
	} else {
		printk("%s(%d)dsp run\n",__func__,__LINE__);
		regmap_update_bits(rt551x->regmap, RT5514_DSP_CTRL1,
			(0x1 << 1), (0x0 << 1));
	}
	mutex_unlock(&rt551x->dspcontrol_lock);
	return 0;
}

static int rt551x_hw_reset_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);

	if (rt551x->chip_id != RT5518_DEVICE_ID) {
		printk("%s: HW RESET is supported on ALC5518 only\n",__func__);
		ucontrol->value.integer.value[0] = 0;
	}
	else
		ucontrol->value.integer.value[0] = rt551x->hw_reset;

	return 0;
}

static int rt551x_hw_reset_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);

	if (rt551x->chip_id != RT5518_DEVICE_ID)
		printk("%s: HW RESET is supported on ALC5518 only\n",__func__);
	else {
		rt551x->hw_reset = ucontrol->value.integer.value[0];
		if (rt551x->hw_reset){
			gpio_set_value(rt551x->hw_reset_pin, 0);
			gpio_set_value(rt551x->hw_reset_pin, 1);
		}
		else
			gpio_set_value(rt551x->hw_reset_pin, 1);
	}
	return 0;
}
#endif
static int rt551x_dsp_idle_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt551x->dsp_idle;

	return 0;
}

static int rt551x_dsp_set_idle_mode(struct snd_soc_codec *codec, int IdleMode)
{
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);
	if ( rt551x->dsp_idle == IdleMode)
	{
		printk("%s IdleMode is the same with previous one%d\n",__func__,IdleMode);
		return 0;
	}

	if (IdleMode)
	{
		set_pcm_is_readable(0);
		rt551x_SetRP_onIdle();
		if (rt551x->chip_id == RT5514_DEVICE_ID)
			regmap_write(rt551x->regmap, RT5514_DELAY_BUF_CTRL1, 0x3fff00ee);
		else
			regmap_update_bits(rt551x->regmap, RT5518_CLK_CTRL1,(0x1 << 26),(0x0 << 26));
		regmap_write(rt551x->regmap, RT551X_DSP_WDG_3, 0x00000001);
		regmap_write(rt551x->regmap, RT551X_DSP_WDG_1, 0x00000004);
		regmap_update_bits(rt551x->regmap, RT5514_DSP_CTRL1, (0x1 << 0),(0x1 << 0));
		/* {-3, 0, +3, +4.5, +7.5, +9.5, +12, +14, +17} dB */
		printk("%s(%d)dsp idle on\n",__func__,__LINE__);
	}
	else if (rt551x->dsp_enabled)
	{
		regmap_update_bits(rt551x->regmap, RT5514_DSP_CTRL1, (0x1 << 0),(0x0 << 0));
		regmap_write(rt551x->regmap, RT551X_DSP_WDG_1, 0x00000005);
		regmap_write(rt551x->regmap, RT551X_DSP_WDG_3, 0x00000001);
		if (rt551x->chip_id == RT5514_DEVICE_ID)
			regmap_write(rt551x->regmap, RT5514_DELAY_BUF_CTRL1, 0x3fff00fe);
		else
			regmap_update_bits(rt551x->regmap, RT5518_CLK_CTRL1,(0x1 << 26),(0x1 << 26));
		printk("%s(%d)dsp idle off\n",__func__,__LINE__);
		set_pcm_is_readable(1);
	}
	else
	{
		printk("%s IdleMode cannot be turn-off because DSP is not enabled  %d\n",__func__,rt551x->dsp_enabled);
	}
	rt551x->dsp_idle = IdleMode;
	return 0;
}

static int rt551x_dsp_idle_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);
	mutex_lock(&rt551x->dspcontrol_lock);
	rt551x_dsp_set_idle_mode(codec, ucontrol->value.integer.value[0]);
	printk("%s(%d)dsp idle:%ld\n",__func__,__LINE__,ucontrol->value.integer.value[0]);
	mutex_unlock(&rt551x->dspcontrol_lock);
	return 0;
}

static const struct snd_kcontrol_new rt551x_snd_controls[] = {
	SOC_ENUM_EXT("DSP Control", rt551x_dsp_mod_enum, rt551x_dsp_mode_get,
		rt551x_dsp_mode_put),
#ifdef CONFIG_SND_SOC_RT551X_TEST_ONLY
	SOC_SINGLE_EXT("DSP FW Check", SND_SOC_NOPM, 0, 4, 0,
		rt551x_dsp_fw_check_get, rt551x_dsp_fw_check_put),
	SOC_SINGLE_EXT("SW Reset", SND_SOC_NOPM, 0, 1, 0,
		rt551x_sw_reset_get, rt551x_sw_reset_put),
	SOC_SINGLE_EXT("HW RESET", SND_SOC_NOPM, 0, 1, 0,
		rt551x_hw_reset_get, rt551x_hw_reset_put),
	SOC_SINGLE_EXT("DSP Stop", SND_SOC_NOPM, 0, 1, 0,
		rt551x_dsp_stop_get, rt551x_dsp_stop_put),
	SOC_SINGLE_EXT("DSP Core Reset", SND_SOC_NOPM, 0, 1, 0,
		rt551x_dsp_core_reset_get, rt551x_dsp_core_reset_put),
#endif
	SOC_SINGLE_EXT("DSP Idle", SND_SOC_NOPM, 0, 1, 0,
		rt551x_dsp_idle_get, rt551x_dsp_idle_put),
};
/*
static int rt551x_is_sys_clk_from_pll(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_codec *codec = source->codec;
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);

	if (rt551x->sysclk_src == RT551X_SCLK_S_PLL1)
		return 1;
	else
		return 0;
}
*/
static int get_clk_info(int sclk, int rate)
{
	int i, pd[] = {1, 2, 3, 4, 6, 8, 12, 16};

	if (sclk <= 0 || rate <= 0)
		return -EINVAL;

	rate = rate << 8;
	for (i = 0; i < ARRAY_SIZE(pd); i++)
		if (sclk == rate * pd[i])
			return i;

	return -EINVAL;
}

static int get_adc_clk_info(int sclk, int rate)
{
	int i, pd[] = {1, 2, 3, 4, 6, 8, 12, 16, 24};

	if (sclk <= 0 || rate <= 0)
		return -EINVAL;

	rate = rate << 7;
	for (i = 0; i < ARRAY_SIZE(pd); i++)
		if (sclk == rate * pd[i])
			return i;

	return -EINVAL;
}

static int rt551x_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);
	int pre_div, pre_div_adc, bclk_ms, frame_size;
//	unsigned int val_len = 0;

	rt551x->lrck = params_rate(params);
	pre_div = get_clk_info(rt551x->sysclk, rt551x->lrck);
	if (pre_div < 0) {
		dev_err(codec->dev, "Unsupported clock setting\n");
		return -EINVAL;
	}

	pre_div_adc = get_adc_clk_info(rt551x->sysclk, rt551x->lrck);
	if (pre_div_adc < 0) {
		dev_err(codec->dev, "Unsupported adc clock setting\n");
		return -EINVAL;
	}

	frame_size = snd_soc_params_to_frame_size(params);
	if (frame_size < 0) {
		dev_err(codec->dev, "Unsupported frame size: %d\n", frame_size);
		return -EINVAL;
	}

	bclk_ms = frame_size > 32 ? 1 : 0;
	rt551x->bclk = rt551x->lrck * (32 << bclk_ms);

	dev_dbg(dai->dev, "bclk is %dHz and lrck is %dHz\n",
		rt551x->bclk, rt551x->lrck);
	dev_dbg(dai->dev, "bclk_ms is %d and pre_div is %d for iis %d\n",
				bclk_ms, pre_div, dai->id);

	regmap_update_bits(rt551x->regmap, RT5514_CLK_CTRL2,
		RT5514_CLK_SYS_DIV_OUT_MASK | RT5514_SEL_ADC_OSR_MASK,
		pre_div << RT5514_CLK_SYS_DIV_OUT_SFT |
		pre_div << RT5514_SEL_ADC_OSR_SFT);
	regmap_update_bits(rt551x->regmap, RT5514_CLK_CTRL1,
		RT5514_CLK_AD_ANA1_SEL_MASK,
		pre_div_adc << RT5514_CLK_AD_ANA1_SEL_SFT);

	return 0;
}

static int rt551x_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
//	struct snd_soc_codec *codec = dai->codec;
//	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);
	return 0;
}

static int rt551x_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_val = 0;

	if (freq == rt551x->sysclk && clk_id == rt551x->sysclk_src)
		return 0;

	switch (clk_id) {
	case RT551X_SCLK_S_MCLK:
		reg_val |= RT5514_CLK_SYS_PRE_SEL_MCLK;
		break;

	case RT551X_SCLK_S_PLL1:
		reg_val |= ((rt551x->chip_id == RT5518_DEVICE_ID) ? RT5518_CLK_SYS_PRE_SEL_PLL_1 : RT5514_CLK_SYS_PRE_SEL_PLL);
		break;

	default:
		dev_err(codec->dev, "Invalid clock id (%d)\n", clk_id);
		return -EINVAL;
	}

	regmap_update_bits(rt551x->regmap, RT5514_CLK_CTRL2,
		RT5514_CLK_SYS_PRE_SEL_MASK, reg_val);

	rt551x->sysclk = freq;
	rt551x->sysclk_src = clk_id;

	dev_dbg(dai->dev, "Sysclk is %dHz and clock id is %d\n", freq, clk_id);

	return 0;
}

/**
 * rt551x_pll_calc - Calcualte PLL M/N/K code.
 * @freq_in: external clock provided to codec.
 * @freq_out: target clock which codec works on.
 * @pll_code: Pointer to structure with M, N, K, bypass K and bypass M flag.
 *
 * Calcualte M/N/K code and bypass K/M flag to configure PLL for codec.
 *
 * Returns 0 for success or negative error code.
 */
static int rt551x_pll_calc(const unsigned int freq_in,
	const unsigned int freq_out, struct rt551x_pll_code *pll_code)
{
	int max_n = RT5514_PLL_N_MAX, max_m = RT5514_PLL_M_MAX;
	int k, n = 0, m = 0, red, n_t, m_t, pll_out, in_t;
	int out_t, red_t = abs(freq_out - freq_in);
	bool m_bypass = false, k_bypass = false;

	if (RT5514_PLL_INP_MAX < freq_in || RT5514_PLL_INP_MIN > freq_in)
		return -EINVAL;

	k = 100000000 / freq_out - 2;
	if (k > RT5514_PLL_K_MAX)
		k = RT5514_PLL_K_MAX;
	if (k < 0) {
		k = 0;
		k_bypass = true;
	}
	for (n_t = 0; n_t <= max_n; n_t++) {
		in_t = freq_in / (k_bypass ? 1 : (k + 2));
		pll_out = freq_out / (n_t + 2);
		if (in_t < 0)
			continue;
		if (in_t == pll_out) {
			m_bypass = true;
			n = n_t;
			goto code_find;
		}
		red = abs(in_t - pll_out);
		if (red < red_t) {
			m_bypass = true;
			n = n_t;
			m = m_t;
			if (red == 0)
				goto code_find;
			red_t = red;
		}
		for (m_t = 0; m_t <= max_m; m_t++) {
			out_t = in_t / (m_t + 2);
			red = abs(out_t - pll_out);
			if (red < red_t) {
				m_bypass = false;
				n = n_t;
				m = m_t;
				if (red == 0)
					goto code_find;
				red_t = red;
			}
		}
	}
	pr_debug("Only get approximation about PLL\n");

code_find:

	pll_code->m_bp = m_bypass;
	pll_code->k_bp = k_bypass;
	pll_code->m_code = m;
	pll_code->n_code = n;
	pll_code->k_code = k;

	return 0;
}

static int rt551x_set_dai_pll(struct snd_soc_dai *dai, int pll_id, int source,
			unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);
	struct rt551x_pll_code pll_code;
	int ret;

	if (!freq_in || !freq_out) {
		dev_dbg(codec->dev, "PLL disabled\n");

		rt551x->pll_in = 0;
		rt551x->pll_out = 0;
		regmap_update_bits(rt551x->regmap, RT5514_CLK_CTRL2,
			RT5514_CLK_SYS_PRE_SEL_MASK,
			RT5514_CLK_SYS_PRE_SEL_MCLK);

		return 0;
	}

	if (source == rt551x->pll_src && freq_in == rt551x->pll_in &&
	    freq_out == rt551x->pll_out)
		return 0;

	switch (source) {
	case RT551X_PLL1_S_MCLK:
		regmap_update_bits(rt551x->regmap, RT5514_PLL_SOURCE_CTRL,
			RT5514_PLL_1_SEL_MASK, RT5514_PLL_1_SEL_MCLK);
		break;

	case RT551X_PLL1_S_BCLK:
		regmap_update_bits(rt551x->regmap, RT5514_PLL_SOURCE_CTRL,
			RT5514_PLL_1_SEL_MASK, RT5514_PLL_1_SEL_SCLK);
		break;

	default:
		dev_err(codec->dev, "Unknown PLL source %d\n", source);
		return -EINVAL;
	}

	ret = rt551x_pll_calc(freq_in, freq_out, &pll_code);
	if (ret < 0) {
		dev_err(codec->dev, "Unsupport input clock %d\n", freq_in);
		return ret;
	}

	dev_dbg(codec->dev, "m_bypass=%d k_bypass=%d m=%d n=%d k=%d\n",
		pll_code.m_bp, pll_code.k_bp,
		(pll_code.m_bp ? 0 : pll_code.m_code), pll_code.n_code,
		(pll_code.k_bp ? 0 : pll_code.k_code));

	regmap_write(rt551x->regmap, RT5514_ANA_CTRL_PLL1_1,
		pll_code.k_code << RT5514_PLL_K_SFT |
		pll_code.n_code << RT5514_PLL_N_SFT |
		(pll_code.m_bp ? 0 : pll_code.m_code) << RT5514_PLL_M_SFT);
	regmap_update_bits(rt551x->regmap, RT5514_ANA_CTRL_PLL1_2,
		RT5514_PLL_M_BP, pll_code.m_bp << RT5514_PLL_M_BP_SFT |
		pll_code.k_bp << RT5514_PLL_K_BP_SFT);

	rt551x->pll_in = freq_in;
	rt551x->pll_out = freq_out;
	rt551x->pll_src = source;

	return 0;
}

static int rt551x_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
			unsigned int rx_mask, int slots, int slot_width)
{
//	struct snd_soc_codec *codec = dai->codec;
//	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);
	return 0;
}

static int rt551x_set_bias_level(struct snd_soc_codec *codec,
			enum snd_soc_bias_level level)
{
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		if (codec->dapm.bias_level == SND_SOC_BIAS_STANDBY) {
			if (rt551x->dsp_enabled) {
				rt551x->dsp_enabled = 0;
				rt551x_reset(rt551x);
				regcache_mark_dirty(rt551x->regmap);
				regcache_sync(rt551x->regmap);
			}
		}
		break;

	case SND_SOC_BIAS_STANDBY:
		break;

	case SND_SOC_BIAS_OFF:
		break;

	default:
		break;
	}
	codec->dapm.bias_level = level;

	return 0;
}

static int rt551x_probe(struct snd_soc_codec *codec)
{
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);

	pr_info("Codec driver version %s\n", VERSION);

	rt551x->codec = codec;
	rt551x_set_bias_level(codec, SND_SOC_BIAS_OFF);
	mutex_lock(&rt551x->dspcontrol_lock);
	// init idle mode to on
	rt551x->dsp_idle = 1;
	rt551x_pointer = rt551x;
	mutex_unlock(&rt551x->dspcontrol_lock);
	return 0;
}

static int rt551x_remove(struct snd_soc_codec *codec)
{
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);
	rt551x_set_bias_level(codec, SND_SOC_BIAS_OFF);
	mutex_lock(&rt551x->dspcontrol_lock);
	rt551x_set_dsp_mode(codec, 0);
	rt551x_pointer = NULL;
	mutex_unlock(&rt551x->dspcontrol_lock);
	return 0;
}

static int rt551x_suspend(struct snd_soc_codec *codec)
{
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);

	rt551x->had_suspend = true;
	//pr_info("%s -- OK!\n", __func__);
	return 0;
}

static int rt551x_resume(struct snd_soc_codec *codec)
{
	struct rt551x_priv *rt551x = snd_soc_codec_get_drvdata(codec);

	rt551x->had_suspend = false;
	pr_info("%s -- OK!\n", __func__);
	return 0;
}

#define RT551X_STEREO_RATES SNDRV_PCM_RATE_8000_192000
#define RT551X_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S8)

struct snd_soc_dai_ops rt551x_aif_dai_ops = {
	.hw_params = rt551x_hw_params,
	.set_fmt = rt551x_set_dai_fmt,
	.set_sysclk = rt551x_set_dai_sysclk,
	.set_pll = rt551x_set_dai_pll,
	.set_tdm_slot = rt551x_set_tdm_slot,
};

struct snd_soc_dai_driver rt551x_dai[] = {
	{
		.name = "rt551x-aif1",
		.id = 0,
		.capture = {
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 4,
			.rates = RT551X_STEREO_RATES,
			.formats = RT551X_FORMATS,
		},
		.ops = &rt551x_aif_dai_ops,
	}
};

static struct snd_soc_codec_driver soc_codec_dev_rt551x = {
	.probe = rt551x_probe,
	.remove = rt551x_remove,
	.suspend = rt551x_suspend,
	.resume = rt551x_resume,
	.set_bias_level = rt551x_set_bias_level,
	.idle_bias_off = true,
	.controls = rt551x_snd_controls,
	.num_controls = ARRAY_SIZE(rt551x_snd_controls),
};

static const struct regmap_config precfg_regmap = {
	.name="nocache",
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 2,
	.max_register = RT551X_I2C_BYPASS,
	.cache_type = REGCACHE_NONE,
};

static const struct regmap_config rt5514_regmap = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 2,

	.max_register = RT551X_I2C_BYPASS,
	.volatile_reg = rt5514_volatile_register,
	.readable_reg = rt5514_readable_register,

	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = rt5514_reg,
	.num_reg_defaults = ARRAY_SIZE(rt5514_reg),
	.use_single_rw = true,
};

static const struct regmap_config rt5518_regmap = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 2,

	.max_register = RT551X_I2C_BYPASS,
	.volatile_reg = rt5518_volatile_register,
	.readable_reg = rt5518_readable_register,

	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = rt5518_reg,
	.num_reg_defaults = ARRAY_SIZE(rt5518_reg),
	.use_single_rw = true,
};

static const struct i2c_device_id rt551x_i2c_id[] = {
	{ "rt551x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rt551x_i2c_id);

static irqreturn_t rt551x_irq_handler(int irq, void *dev_id)
{
	struct rt551x_priv *rt551x = dev_id;

	pr_info("%s -- ALC551X DSP IRQ Issued Successfully!\n", __func__);

	set_dynamic_boost(200, PRIO_TWO_BIGS_TWO_LITTLES_MAX_FREQ);

	if(rt551x->had_suspend)
	{
		reset_pcm_read_pointer();
	}


	if (!rt551x->i2c_suppend)
		schedule_work(&rt551x->handler_work);
	else
		rt551x->dsp_had_irq = true;

	return IRQ_HANDLED;
}

static void rt551x_handler_work(struct work_struct *work)
{
	int iVdIdVal, wdg_status = 0;
	struct rt551x_priv *rt551x = container_of(work, struct rt551x_priv, handler_work);

	regcache_cache_bypass(rt551x->regmap, true);
	regmap_read(rt551x->regmap, RT551X_VENDOR_ID2, &iVdIdVal);
	if (iVdIdVal == RT5518_DEVICE_ID)
	{
		regmap_read(rt551x->regmap, RT5518_DSP_CTRL2, &wdg_status);
		pr_info("%s wdg_status:0x%x\n", __func__,wdg_status);
		wdg_status = wdg_status & 0x2;
	}
	regcache_cache_bypass(rt551x->regmap, false);
	pr_info("%s iVdIdVal:0x%x, wdg_status:0x%x\n", __func__,iVdIdVal,wdg_status);

	if ((iVdIdVal == rt551x->chip_id) && !wdg_status)
	{
		schedule_work(&rt551x->hotword_work);
		wake_lock_timeout(&rt551x->vad_wake, msecs_to_jiffies(800));
#ifdef CONFIG_AMAZON_METRICS_LOG
		log_counter_to_vitals(ANDROID_LOG_INFO, "Kernel", "Kernel","RT5514_DSP_metrics_count","DSP_IRQ", 1, "count", NULL, VITALS_NORMAL);

		log_to_metrics(ANDROID_LOG_INFO, "voice_dsp", "voice_dsp:def:DSP_IRQ=1;CT;1:NR");
#endif
	}
	else
	{
#ifdef CONFIG_AMAZON_METRICS_LOG
		log_counter_to_vitals(ANDROID_LOG_INFO, "Kernel", "Kernel","RT5514_DSP_metrics_count","DSP_Watchdog", 1, "count", NULL, VITALS_NORMAL);

		log_to_metrics(ANDROID_LOG_INFO, "voice_dsp", "voice_dsp:def:DSP_Watchdog=1;CT;1:NR");
#endif
		schedule_work(&rt551x->watchdog_work);
	}
}

void rt551x_reset_duetoSPI(void)
{
	pr_err("rt551x_reset_duetoSPI enter\n");
	if (!rt551x_pointer)
	{
		return;
	}
	schedule_work(&rt551x_pointer->watchdog_work);
	wake_lock_timeout(&rt551x_pointer->vad_wake, msecs_to_jiffies(3500));
	pr_err("%s -- exit\n", __func__);
}

static void rt551x_dbgBuf_print(DBGBUF_MEM *ptr)
{
	int i, index;

	if (ptr->idx >= RT551X_DBG_BUF_CNT)
	{
		printk("Warning! Invalid index, %d\n", ptr->idx);
		index = 0;
	}
	else
		index = ptr->idx;
	if (index > 0)
	{
		for (i = index-1; i >= 0; i--)
			printk("id:%d, ts:%d, val:0x%x\n", ptr->unit[i].id, ptr->unit[i].ts, ptr->unit[i].val);
	}
	for (i = RT551X_DBG_BUF_CNT-1; i >= index; i--)
		printk("id:%d, ts:%d, val:0x%x\n", ptr->unit[i].id, ptr->unit[i].ts, ptr->unit[i].val);
	return ;
}

static void rt551x_do_hotword_work(struct work_struct *work)
{
	struct rt551x_priv *rt551x = container_of(work, struct rt551x_priv, hotword_work);
	static char * hot_event[] = { "ACTION=HOTWORD", NULL };

	wait_for_rt551x_spi_resumed();
	pr_info("%s -- send hotword uevent!\n", __func__);

	kobject_uevent_env(&rt551x->codec->dev->kobj, KOBJ_CHANGE, hot_event);
}

static void rt551x_do_watchdog_work(struct work_struct *work)
{
	struct rt551x_priv *rt551x = container_of(work, struct rt551x_priv, watchdog_work);
//	static const char * const wdg_event[] = { "ACTION=WATCHDOG", NULL };
	int PrevDspMode;
	int PrevIdleMode;
	int ret;

	mutex_lock(&rt551x->dspcontrol_lock);
	PrevDspMode = rt551x->dsp_enabled;
	PrevIdleMode = rt551x->dsp_idle;
	pr_info("%s watchdog DSP Idle Mode Recover [%d] \n", __func__, PrevIdleMode);

	ret = rt551x_spi_burst_read(RT551X_DBG_BUF_ADDR, (u8 *)&dbgBufLast, sizeof(DBGBUF_MEM));
	if (!ret)
		pr_err("%s dbgBuf spi_burst_read failed! %d\n", __func__, ret);
	else
	{
		dbgBufLast.reserve = 1;
		rt551x_dbgBuf_print(&dbgBufLast);
	}

	rt551x_set_dsp_mode(rt551x->codec, 0);
	pr_info("%s watchdog cause DSP Reset\n", __func__);

	if (PrevDspMode)
	{
		pr_info("%s watchdog cause DSP Reload Firmware, Mode [%d] \n", __func__, PrevDspMode);
		rt551x_set_dsp_mode(rt551x->codec, PrevDspMode);
	}

	rt551x_dsp_set_idle_mode(rt551x->codec, PrevIdleMode);
	mutex_unlock(&rt551x->dspcontrol_lock);
	//pr_info("%s -- send watchdog uevent!\n", __func__);
	//kobject_uevent_env(&rt551x->codec->dev->kobj, KOBJ_CHANGE, wdg_event);
}

#ifdef CONFIG_SND_SOC_RT551X_TEST_ONLY
static ssize_t rt551x_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt551x_priv *rt551x = i2c_get_clientdata(client);
	int count = 0;
	unsigned int i, value;
	int ret;

	for (i = RT551X_BUFFER_VOICE_BASE; i <= RT551X_VENDOR_ID2; i+=4) {
		if (rt551x->fp_readable_register(NULL, i)) {
#ifdef RT551X_RW_BY_SPI
			ret = rt551x_spi_read_addr(i, &value);
#else
			ret = regmap_read(rt551x->regmap, i, &value);
#endif
			if (ret < 0)
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
					"%08x: XXXXXXXX\n", i);
			else
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "%08x: %08x\n", i,
					value);

			if (count >= PAGE_SIZE - 1) {
				/* string cut by snprintf(), add this to prevent potential buffer overflow */
				count = PAGE_SIZE - 1;
				break;
			}
		}
	}

	return count;
}

static ssize_t rt551x_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
#ifndef RT551X_RW_BY_SPI
	struct i2c_client *client = to_i2c_client(dev);
	struct rt551x_priv *rt551x = i2c_get_clientdata(client);
#endif
	unsigned int val = 0, addr = 0;
	int i;

	for (i = 0; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			addr = (addr << 4) | (*(buf + i)-'0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			addr = (addr << 4) | ((*(buf + i)-'A') + 0xa);
		else
			break;
	}

	for (i = i + 1 ; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			val = (val << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			val = (val << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;
	}

	if (i == count) {
#ifdef RT551X_RW_BY_SPI
		rt551x_spi_read_addr(addr, &val);
#else
		regmap_read(rt551x->regmap, addr, &val);
#endif
		pr_info("0x%08x = 0x%08x\n", addr, val);
	} else {
#ifdef RT551X_RW_BY_SPI
		rt551x_spi_write_addr(addr, val);
#else
		regmap_write(rt551x->regmap, addr, val);
#endif
	}
	return count;
}
static DEVICE_ATTR(rt551x_reg, 0644, rt551x_reg_show, rt551x_reg_store);

static ssize_t rt551x_i2c_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt551x_priv *rt551x = i2c_get_clientdata(client);
	unsigned int value = 0, ret;
	int i,count = 0;

	for (i = (RT551X_VENDOR_ID1);
		i <= (RT551X_VENDOR_ID2); i+=4) {

		ret = regmap_read(rt551x->regmap, i, &value);
		if (ret < 0)
			count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
				"%08x: XXXXXXXX\n", i);
		else
			count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "%08x: %08x\n", i,
				value);
		if (count >= PAGE_SIZE - 1) {
			count = PAGE_SIZE - 1;
			break;
		}
	}

	return count;
}
static DEVICE_ATTR(rt551x_id, 0444, rt551x_i2c_id_show, NULL);

static ssize_t rt551x_i2c_version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "Firmware version: %s\nDriver version: %s\n",
		fw_ver, VERSION);
}
static DEVICE_ATTR(rt551x_version, 0444, rt551x_i2c_version_show, NULL);

static ssize_t rt551x_i2c_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt551x_priv *rt551x = i2c_get_clientdata(client);
	int count = 0;
	unsigned int i, value;
	int ret;

	regcache_cache_bypass(rt551x->regmap, true);
	for (i = RT551X_BUFFER_VOICE_BASE; i <= RT551X_VENDOR_ID2; i+=4) {
		if (rt551x->fp_readable_register(NULL, i)) {

			ret = regmap_read(rt551x->regmap, i, &value);

			if (ret < 0)
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count,
					"%08x: XXXXXXXX\n", i);
			else
				count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "%08x: %08x\n", i,
					value);

			if (count >= PAGE_SIZE - 1) {
				count = PAGE_SIZE - 1;
				break;
			}
		}
	}
	regcache_cache_bypass(rt551x->regmap, false);
	return count;
}

static ssize_t rt551x_i2c_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt551x_priv *rt551x = i2c_get_clientdata(client);
	unsigned int val = 0, addr = 0;
	int i;

	regcache_cache_bypass(rt551x->regmap, true);
	for (i = 0; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			addr = (addr << 4) | (*(buf + i)-'0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			addr = (addr << 4) | ((*(buf + i)-'A') + 0xa);
		else
			break;
	}

	for (i = i + 1 ; i < count; i++) {
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			val = (val << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			val = (val << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;
	}

	if (i == count) {
		regmap_read(rt551x->regmap, addr, &val);
		pr_info("0x%08x = 0x%08x\n", addr, val);
	} else {
		regmap_write(rt551x->regmap, addr, val);
	}
	regcache_cache_bypass(rt551x->regmap, false);
	return count;
}
static DEVICE_ATTR(rt551x_reg_i2c, 0644, rt551x_i2c_reg_show, rt551x_i2c_reg_store);

static int rt551x_dbgBuf_dump(DBGBUF_MEM *ptr, char *buf, int count)
{
	int i, index;

	if (ptr->idx >= RT551X_DBG_BUF_CNT)
	{
		count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "Warning! Invalid index, %d\n", ptr->idx);
		index = 0;
	}
	else
		index = ptr->idx;
	if (index > 0)
	{
		for (i = index-1; i >= 0; i--)
		{
			count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "id:%d, ts:%d, val:0x%x\n", ptr->unit[i].id, ptr->unit[i].ts, ptr->unit[i].val);
			if (count >= PAGE_SIZE - 1)
				return PAGE_SIZE - 1;
		}
	}
	for (i = RT551X_DBG_BUF_CNT-1; i >= index; i--)
	{
		count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "id:%d, ts:%d, val:0x%x\n", ptr->unit[i].id, ptr->unit[i].ts, ptr->unit[i].val);
		if (count >= PAGE_SIZE - 1)
			break;
	}
	return count;
}
static ssize_t rt551x_debug_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret, count = 0;
	DBGBUF_MEM dbgBuf;

	ret = rt551x_spi_burst_read(RT551X_DBG_BUF_ADDR, (u8 *)&dbgBuf, sizeof(DBGBUF_MEM));
	if (!ret)
		count = snprintf(buf, (ssize_t)PAGE_SIZE, "Warning! Failed to read DBG_BUF!\n");
	else
		count = rt551x_dbgBuf_dump(&dbgBuf, buf, count);

	/* dump the last NG dbgBuf if there is WDG detected */
	if (dbgBufLast.reserve)
	{
		count += snprintf(buf + count, (ssize_t)PAGE_SIZE - count, "Here's the dbgBuf when the last WDG detected\n");
		count = rt551x_dbgBuf_dump(&dbgBufLast, buf, count);
	}
	return count;
}
static DEVICE_ATTR(rt551x_debug, 0444, rt551x_debug_info_show, NULL);
#endif

static int rt5518_hw_irq_init(struct rt551x_priv *rt551x)
{
	int ret = 0;
	struct device_node *node = NULL;
	struct pinctrl *rt5518_pinctrl;
	struct pinctrl_state *pins_eint_int;

	node = of_find_matching_node(node, rt551x_match_table);
	if (node) {
		rt551x->hw_irq_pin = of_get_named_gpio(node, "rt5518-irq", 0);
		if (rt551x->hw_irq_pin < 0)
			printk("[rt551x] not find rt5518-irq\n");

	}
	ret = gpio_request(rt551x->hw_irq_pin, "rt5518_hw_irq");
	if(ret){
		printk("gpio %d request failed!,ret:%d\n", rt551x->hw_irq_pin, ret);
		goto err_gpio;
	}
	ret = gpio_direction_input(rt551x->hw_irq_pin);
	if(ret < 0){
		printk("gpio %d unable to set as input!\n", rt551x->hw_irq_pin);
		goto err_free_gpio;
	}

	rt5518_pinctrl = devm_pinctrl_get(&rt551x->i2c->dev);
        if (IS_ERR(rt5518_pinctrl)) {
                ret = PTR_ERR(rt5518_pinctrl);
                dev_err(&rt551x->i2c->dev, "fwq Cannot find rt5518_pinctrl\n");
                return ret;
        }
	pins_eint_int = pinctrl_lookup_state(rt5518_pinctrl, "state_eint_as_int");
        if (IS_ERR(pins_eint_int)) {
                ret = PTR_ERR(pins_eint_int);
                dev_err(&rt551x->i2c->dev, "fwq Cannot find rt5518_pinctrl state_eint_int\n");
                return ret;
        } else {
                pr_info("have find rt5518 pinctrl state_eint_int\n");
        }

        pinctrl_select_state(rt5518_pinctrl, pins_eint_int);

	return 0;
err_free_gpio:
	if (gpio_is_valid(rt551x->hw_irq_pin))
		gpio_free(rt551x->hw_irq_pin);
err_gpio:
	return ret;
}

static int rt551x_i2c_probe(struct i2c_client *i2c,
		    const struct i2c_device_id *id)
{
	struct rt551x_priv *rt551x;
	int ret;

	rt551x = devm_kzalloc(&i2c->dev, sizeof(struct rt551x_priv),
				GFP_KERNEL);
	if (rt551x == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, rt551x);

	rt551x->regmap = devm_regmap_init_i2c(i2c, &precfg_regmap);
	if (IS_ERR(rt551x->regmap)) {
		ret = PTR_ERR(rt551x->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}
	regmap_read(rt551x->regmap, RT551X_VENDOR_ID2, &ret);
	printk("%s: Device ID:0x%x\n",__func__,ret);
	rt551x->chip_id = ret;
	switch (ret) {
		case RT5514_DEVICE_ID:
			rt551x->regmap = devm_regmap_init_i2c(i2c, &rt5514_regmap);
			rt551x->fp_reset = rt5514_reset;
			rt551x->fp_enable_dsp = rt5514_enable_dsp;
			rt551x->fp_enable_dsp_clock = rt5514_enable_dsp_clock;
			rt551x->fp_readable_register = rt5514_readable_register;
			break;
		case RT5518_DEVICE_ID:
			rt551x->regmap = devm_regmap_init_i2c(i2c, &rt5518_regmap);
			rt551x->fp_reset = rt5518_reset;
			rt551x->fp_enable_dsp = rt5518_enable_dsp;
			rt551x->fp_enable_dsp_clock = rt5518_enable_dsp_clock;
			rt551x->fp_readable_register = rt5518_readable_register;
			ret = rt5518_hw_reset_gpio_init(rt551x);
			if (ret) {
				printk("%s: rt5518_hw_reset request failed!\n",__func__);
			}
			break;
		default:
			dev_err(&i2c->dev,
				"Device with ID register %#x is not rt5514 or rt5518\n",
				ret);
			// if i2c cannot read the correct device ID, use rt5518 as default.
			rt551x->chip_id = RT5518_DEVICE_ID;
			rt551x->regmap = devm_regmap_init_i2c(i2c, &rt5518_regmap);
			rt551x->fp_reset = rt5518_reset;
			rt551x->fp_enable_dsp = rt5518_enable_dsp;
			rt551x->fp_enable_dsp_clock = rt5518_enable_dsp_clock;
			rt551x->fp_readable_register = rt5518_readable_register;
			ret = rt5518_hw_reset_gpio_init(rt551x);
			if (ret) {
				printk("%s: rt5518_hw_reset request failed!\n",__func__);
			}
			// return -ENODEV;
	}
	rt551x_reset(rt551x);
#ifdef CONFIG_SND_SOC_RT551X_TEST_ONLY
	ret = device_create_file(&i2c->dev, &dev_attr_rt551x_reg);

	if (ret < 0)
		printk("failed to add rt551x_reg sysfs files\n");
#endif

	rt551x->had_suspend = false;

	rt551x->i2c = i2c;
        ret = rt5518_hw_irq_init(rt551x);
	if (ret < 0)
		printk("rt5518_hw_irq_init failed \n");

	ret = request_irq(i2c->irq, rt551x_irq_handler,
		IRQF_TRIGGER_RISING|IRQF_ONESHOT, "rt551x", rt551x);
	if (ret < 0)
		dev_err(&i2c->dev, "rt551x_irq not available (%d)\n", ret);
	ret = irq_set_irq_wake(i2c->irq, 1);
	if (ret < 0)
		dev_err(&i2c->dev, "failed to set irq wake (%d)\n", ret);

	INIT_WORK(&rt551x->hotword_work, rt551x_do_hotword_work);
	INIT_WORK(&rt551x->watchdog_work, rt551x_do_watchdog_work);
	INIT_WORK(&rt551x->handler_work, rt551x_handler_work);
	mutex_init(&rt551x->dspcontrol_lock);
	wake_lock_init(&rt551x->vad_wake, WAKE_LOCK_SUSPEND, "rt551x_wake");

#ifdef CONFIG_SND_SOC_RT551X_TEST_ONLY
	ret = device_create_file(&i2c->dev, &dev_attr_rt551x_reg_i2c);
	if (ret < 0)
		printk("failed to add rt551x_reg_i2c sysfs files\n");

	ret = device_create_file(&i2c->dev, &dev_attr_rt551x_id);
	if (ret < 0)
		printk("failed to add rt551x_id sysfs files\n");

	ret = device_create_file(&i2c->dev, &dev_attr_rt551x_version);
	if (ret < 0)
		printk("failed to add rt551x_version sysfs files\n");
	ret = device_create_file(&i2c->dev, &dev_attr_rt551x_debug);
	if (ret < 0)
		printk("failed to add rt551x_debug sysfs files\n");
#endif
	memset((void *)&dbgBufLast, 0, sizeof(DBGBUF_MEM));
	return snd_soc_register_codec(&i2c->dev, &soc_codec_dev_rt551x,
			rt551x_dai, ARRAY_SIZE(rt551x_dai));
}

static int rt551x_i2c_supspend(struct i2c_client *client, pm_message_t mesg)
{
	struct rt551x_priv *rt551x = i2c_get_clientdata(client);
	rt551x->i2c_suppend = true;
	rt551x->dsp_had_irq = false;
	pr_info("%s -- OK!\n", __func__);
	return 0;
}

static int rt551x_i2c_resume(struct i2c_client *client)
{
	struct rt551x_priv *rt551x = i2c_get_clientdata(client);
	rt551x->i2c_suppend = false;

	if (rt551x->dsp_had_irq)
		schedule_work(&rt551x->handler_work);
	rt551x->dsp_had_irq = false;
	pr_info("%s -- OK!\n", __func__);
	return 0;
}

static int rt551x_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);

	return 0;
}

void rt551x_i2c_shutdown(struct i2c_client *client)
{
	struct rt551x_priv *rt551x = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt551x->codec;

	if (codec != NULL)
		rt551x_set_bias_level(codec, SND_SOC_BIAS_OFF);
}

struct i2c_driver rt551x_i2c_driver = {
	.driver = {
		.name = "rt551x",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = rt551x_match_table,
#endif
	},
	.probe = rt551x_i2c_probe,
	.remove   = rt551x_i2c_remove,
	.suspend = rt551x_i2c_supspend,
	.resume = rt551x_i2c_resume,
	.shutdown = rt551x_i2c_shutdown,
	.id_table = rt551x_i2c_id,
};
module_i2c_driver(rt551x_i2c_driver);


MODULE_DESCRIPTION("ASoC ALC551X driver");
MODULE_AUTHOR("Oder Chiou <oder_chiou@realtek.com>");
MODULE_LICENSE("GPL v2");
