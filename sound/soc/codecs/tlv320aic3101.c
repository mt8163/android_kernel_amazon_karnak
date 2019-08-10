/*
 * linux/sound/soc/codecs/tlv320aic3101.c
 *
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * Based on sound/soc/codecs/wm8753.c by Liam Girdwood
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED AS IS AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#ifdef CONFIG_ACPI
#include <linux/acpi.h>
#else
#include <linux/of_gpio.h>
#endif

#include "tlv320aic3101.h"

/* TODO: DEE-32682 Update ADCs driver to read registers rather than using a
 *  cached array of written values.
 */

/* Microphone analog gain in half dB */
#define MIC_PGA_GAIN_L      40	/* 20dB Gain */
#define MIC_PGA_GAIN_R      40	/* 20dB Gain */
#define MIC_PGA_GAIN_IDC    40	/* 20db Gain */

/* Differential input with 0db gain */
#define DIFF_MIC_INPUT_0db  0x3F

/* Default POV for ADC Input select register */
#define DEFAULT_PGA_INPUT_SEL 0x3F

/* 10msec delay required to hard reset ADC per manual */
#define RESET_LINE_DELAY 10

/* Uncomment to disable DRC
#define SET_DRC_OFF
*/

/* Saved settings for ADC PGA. These are set by kcontrols */
static u8 pga1_input_sel_l[NUM_ADC3101];
static u8 pga1_input_sel_r[NUM_ADC3101];
static u8 pga2_input_sel_l[NUM_ADC3101];
static u8 pga2_input_sel_r[NUM_ADC3101];
static u8 mic_pga_l[NUM_ADC3101];
static u8 mic_pga_r[NUM_ADC3101];

/*
 *****************************************************************************
 * Function Prototype
 *****************************************************************************
 */
static int __IDC_info(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_info *uinfo);
static int __IDC_get(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_value *ucontrol);
static int __IDC_put(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_value *ucontrol);

/*
 *****************************************************************************
 * Structure Declaration
 *****************************************************************************
 */

/* Creates an array of the Single Ended Widgets for ADC3101_B*/
static const DECLARE_TLV_DB_SCALE(adc_3101_vol_tlv, -1200, -50, 0);
static const DECLARE_TLV_DB_SCALE(micpga_gain_tlv, 0, 50, 0);

/*
 * Structure Initialization
 */
static const struct snd_kcontrol_new aic31xx_snd_controls[] = {

	SOC_DOUBLE_R_SX_TLV("ADC_A Digital Volume Control",
			    ADC_LADC_VOL(0), ADC_RADC_VOL(0), 0, 0x28,
			    0x68, adc_3101_vol_tlv),
	SOC_SINGLE("ADC_A Left Fine Volume", ADC_ADC_FGA(0), 4, 4, 1),
	SOC_SINGLE("ADC_A Right Fine Volume", ADC_ADC_FGA(0), 0, 4, 1),
	SOC_DOUBLE_R_TLV("ADC_A MICPGA Volume Ctrl",
			 ADC_LEFT_APGA_CTRL(0), ADC_RIGHT_APGA_CTRL(0), 0, 0x50,
			 0, micpga_gain_tlv),
	SOC_SINGLE("ADC_A IN1_L Input Gain", ADC_LEFT_PGA_SEL_1(0), 0, 1, 0),
	SOC_SINGLE("ADC_A IN2_L Input Gain", ADC_LEFT_PGA_SEL_1(0), 2, 1, 0),
	SOC_SINGLE("ADC_A IN3_L Input Gain", ADC_LEFT_PGA_SEL_1(0), 4, 1, 0),
	SOC_SINGLE("ADC_A DIF1_L Input Gain", ADC_LEFT_PGA_SEL_1(0), 6, 1, 0),
	SOC_SINGLE("ADC_A DIF2_L Input Gain", ADC_LEFT_PGA_SEL_2(0), 4, 1, 0),
	SOC_SINGLE("ADC_A DIF3_L Input Gain", ADC_LEFT_PGA_SEL_2(0), 2, 1, 0),
	SOC_SINGLE("ADC_A IN1_R Input Gain", ADC_RIGHT_PGA_SEL_1(0), 0, 1, 0),
	SOC_SINGLE("ADC_A IN2_R Input Gain", ADC_RIGHT_PGA_SEL_1(0), 2, 1, 0),
	SOC_SINGLE("ADC_A IN3_R Input Gain", ADC_RIGHT_PGA_SEL_1(0), 4, 1, 0),
	SOC_SINGLE("ADC_A DIF1_R Input Gain", ADC_RIGHT_PGA_SEL_1(0), 6, 1, 0),
	SOC_SINGLE("ADC_A DIF2_R Input Gain", ADC_RIGHT_PGA_SEL_2(0), 4, 1, 0),
	SOC_SINGLE("ADC_A DIF3_R Input Gain", ADC_RIGHT_PGA_SEL_2(0), 2, 1, 0),
	SOC_SINGLE("ADC_A Left Mute", ADC_ADC_FGA(0),
			 7, 1, 0),
	SOC_SINGLE("ADC_A Right Mute", ADC_ADC_FGA(0),
			 3, 1, 0),

	SOC_DOUBLE_R_SX_TLV("ADC_B Digital Volume Control",
			    ADC_LADC_VOL(1), ADC_RADC_VOL(1), 0, 0x28,
			    0x68, adc_3101_vol_tlv),
	SOC_SINGLE("ADC_B Left Fine Volume", ADC_ADC_FGA(1), 4, 4, 1),
	SOC_SINGLE("ADC_B Right Fine Volume", ADC_ADC_FGA(1), 0, 4, 1),
	SOC_DOUBLE_R_TLV("ADC_B MICPGA Volume Ctrl",
			 ADC_LEFT_APGA_CTRL(1), ADC_RIGHT_APGA_CTRL(1), 0, 0x50,
			 0, micpga_gain_tlv),
	SOC_SINGLE("ADC_B IN1_L Input Gain", ADC_LEFT_PGA_SEL_1(1), 0, 1, 0),
	SOC_SINGLE("ADC_B IN2_L Input Gain", ADC_LEFT_PGA_SEL_1(1), 2, 1, 0),
	SOC_SINGLE("ADC_B IN3_L Input Gain", ADC_LEFT_PGA_SEL_1(1), 4, 1, 0),
	SOC_SINGLE("ADC_B DIF1_L Input Gain", ADC_LEFT_PGA_SEL_1(1), 6, 1, 0),
	SOC_SINGLE("ADC_B DIF2_L Input Gain", ADC_LEFT_PGA_SEL_2(1), 4, 1, 0),
	SOC_SINGLE("ADC_B DIF3_L Input Gain", ADC_LEFT_PGA_SEL_2(1), 2, 1, 0),
	SOC_SINGLE("ADC_B IN1_R Input Gain", ADC_RIGHT_PGA_SEL_1(1), 0, 1, 0),
	SOC_SINGLE("ADC_B IN2_R Input Gain", ADC_RIGHT_PGA_SEL_1(1), 2, 1, 0),
	SOC_SINGLE("ADC_B IN3_R Input Gain", ADC_RIGHT_PGA_SEL_1(1), 4, 1, 0),
	SOC_SINGLE("ADC_B DIF1_R Input Gain", ADC_RIGHT_PGA_SEL_1(1), 6, 1, 0),
	SOC_SINGLE("ADC_B DIF2_R Input Gain", ADC_RIGHT_PGA_SEL_2(1), 4, 1, 0),
	SOC_SINGLE("ADC_B DIF3_R Input Gain", ADC_RIGHT_PGA_SEL_2(1), 2, 1, 0),
	SOC_SINGLE("ADC_B Left Mute", ADC_ADC_FGA(1),
			 7, 1, 0),
	SOC_SINGLE("ADC_B Right Mute", ADC_ADC_FGA(1),
			 3, 1, 0),

#ifdef CONFIG_SND_SOC_8_MICS
	SOC_DOUBLE_R_SX_TLV("ADC_C Digital Volume Control",
			    ADC_LADC_VOL(2), ADC_RADC_VOL(2), 0, 0x28,
			    0x68, adc_3101_vol_tlv),
	SOC_SINGLE("ADC_C Left Fine Volume", ADC_ADC_FGA(2), 4, 4, 1),
	SOC_SINGLE("ADC_C Right Fine Volume", ADC_ADC_FGA(2), 0, 4, 1),
	SOC_DOUBLE_R_TLV("ADC_C MICPGA Volume Ctrl",
			 ADC_LEFT_APGA_CTRL(2), ADC_RIGHT_APGA_CTRL(2), 0, 0x50,
			 0, micpga_gain_tlv),
	SOC_SINGLE("ADC_C IN1_L Input Gain", ADC_LEFT_PGA_SEL_1(2), 0, 1, 0),
	SOC_SINGLE("ADC_C IN2_L Input Gain", ADC_LEFT_PGA_SEL_1(2), 2, 1, 0),
	SOC_SINGLE("ADC_C IN3_L Input Gain", ADC_LEFT_PGA_SEL_1(2), 4, 1, 0),
	SOC_SINGLE("ADC_C DIF1_L Input Gain", ADC_LEFT_PGA_SEL_1(2), 6, 1, 0),
	SOC_SINGLE("ADC_C DIF2_L Input Gain", ADC_LEFT_PGA_SEL_2(2), 4, 1, 0),
	SOC_SINGLE("ADC_C DIF3_L Input Gain", ADC_LEFT_PGA_SEL_2(2), 2, 1, 0),
	SOC_SINGLE("ADC_C IN1_R Input Gain", ADC_RIGHT_PGA_SEL_1(2), 0, 1, 0),
	SOC_SINGLE("ADC_C IN2_R Input Gain", ADC_RIGHT_PGA_SEL_1(2), 2, 1, 0),
	SOC_SINGLE("ADC_C IN3_R Input Gain", ADC_RIGHT_PGA_SEL_1(2), 4, 1, 0),
	SOC_SINGLE("ADC_C DIF1_R Input Gain", ADC_RIGHT_PGA_SEL_1(2), 6, 1, 0),
	SOC_SINGLE("ADC_C DIF2_R Input Gain", ADC_RIGHT_PGA_SEL_2(2), 4, 1, 0),
	SOC_SINGLE("ADC_C DIF3_R Input Gain", ADC_RIGHT_PGA_SEL_2(2), 2, 1, 0),
	SOC_SINGLE("ADC_C Left Mute", ADC_ADC_FGA(2),
			 7, 1, 0),
	SOC_SINGLE("ADC_C Right Mute", ADC_ADC_FGA(2),
			 3, 1, 0),

	SOC_DOUBLE_R_SX_TLV("ADC_D Digital Volume Control",
			    ADC_LADC_VOL(3), ADC_RADC_VOL(3), 0, 0x28,
			    0x68, adc_3101_vol_tlv),
	SOC_SINGLE("ADC_D Left Fine Volume", ADC_ADC_FGA(3), 4, 4, 1),
	SOC_SINGLE("ADC_D Right Fine Volume", ADC_ADC_FGA(3), 0, 4, 1),
	SOC_DOUBLE_R_TLV("ADC_D MICPGA Volume Ctrl",
			 ADC_LEFT_APGA_CTRL(3), ADC_RIGHT_APGA_CTRL(3), 0, 0x50,
			 0, micpga_gain_tlv),
	SOC_SINGLE("ADC_D IN1_L Input Gain", ADC_LEFT_PGA_SEL_1(3), 0, 1, 0),
	SOC_SINGLE("ADC_D IN2_L Input Gain", ADC_LEFT_PGA_SEL_1(3), 2, 1, 0),
	SOC_SINGLE("ADC_D IN3_L Input Gain", ADC_LEFT_PGA_SEL_1(3), 4, 1, 0),
	SOC_SINGLE("ADC_D DIF1_L Input Gain", ADC_LEFT_PGA_SEL_1(3), 6, 1, 0),
	SOC_SINGLE("ADC_D DIF2_L Input Gain", ADC_LEFT_PGA_SEL_2(3), 4, 1, 0),
	SOC_SINGLE("ADC_D DIF3_L Input Gain", ADC_LEFT_PGA_SEL_2(3), 2, 1, 0),
	SOC_SINGLE("ADC_D IN1_R Input Gain", ADC_RIGHT_PGA_SEL_1(3), 0, 1, 0),
	SOC_SINGLE("ADC_D IN2_R Input Gain", ADC_RIGHT_PGA_SEL_1(3), 2, 1, 0),
	SOC_SINGLE("ADC_D IN3_R Input Gain", ADC_RIGHT_PGA_SEL_1(3), 4, 1, 0),
	SOC_SINGLE("ADC_D DIF1_R Input Gain", ADC_RIGHT_PGA_SEL_1(3), 6, 1, 0),
	SOC_SINGLE("ADC_D DIF2_R Input Gain", ADC_RIGHT_PGA_SEL_2(3), 4, 1, 0),
	SOC_SINGLE("ADC_D DIF3_R Input Gain", ADC_RIGHT_PGA_SEL_2(3), 2, 1, 0),
	SOC_SINGLE("ADC_D Left Mute", ADC_ADC_FGA(3),
			 7, 1, 0),
	SOC_SINGLE("ADC_D Right Mute", ADC_ADC_FGA(3),
			 3, 1, 0),

#endif

	/* Control to enable Internal Data Collection from user space */
	{
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= "IDC Enable",
		.info	= __IDC_info,
		.get	= __IDC_get,
		.put	= __IDC_put,
		.access	= SNDRV_CTL_ELEM_ACCESS_READWRITE,
	},

};

/* ADC A Left input selection, Single Ended inputs and Differential inputs */
static const struct snd_kcontrol_new adc_a_left_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("ADC_A IN1_L switch", ADC_LEFT_PGA_SEL_1(0), 1, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_A IN2_L switch", ADC_LEFT_PGA_SEL_1(0), 3, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_A IN3_L switch", ADC_LEFT_PGA_SEL_1(0), 5, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_A DIF1_L switch", ADC_LEFT_PGA_SEL_1(0), 7, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_A DIF2_L switch", ADC_LEFT_PGA_SEL_2(0), 5, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_A DIF3_L switch", ADC_LEFT_PGA_SEL_2(0), 3, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_A IN1_R switch", ADC_LEFT_PGA_SEL_2(0), 1, 0x1, 1),
};

/* ADC A Right input selection, Single Ended inputs and Differential inputs */
static const struct snd_kcontrol_new adc_a_right_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("ADC_A IN1_R switch", ADC_RIGHT_PGA_SEL_1(0), 1, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_A IN2_R switch", ADC_RIGHT_PGA_SEL_1(0), 3, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_A IN3_R switch", ADC_RIGHT_PGA_SEL_1(0), 5, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_A DIF1_R switch", ADC_RIGHT_PGA_SEL_1(0), 7, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_A DIF2_R switch", ADC_RIGHT_PGA_SEL_2(0), 5, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_A DIF3_R switch", ADC_RIGHT_PGA_SEL_2(0), 3, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_A IN1_L switch", ADC_RIGHT_PGA_SEL_2(0), 1, 0x1, 1),
};

/* Left Digital Mic input for left ADC_A */
static const struct snd_kcontrol_new adc_a_left_input_dmic_controls[] = {
	SOC_DAPM_SINGLE("ADC_A Left ADC switch", ADC_ADC_DIGITAL(0), 3, 0x1, 0),
};

/* Right Digital Mic input for Right ADC_A */
static const struct snd_kcontrol_new adc_a_right_input_dmic_controls[] = {
	SOC_DAPM_SINGLE("ADC_A Right ADC switch", ADC_ADC_DIGITAL(0), 2, 0x1, 0),
};

/* ADC B Left input selection, Single Ended inputs and Differential inputs */
static const struct snd_kcontrol_new adc_b_left_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("ADC_B IN1_L switch", ADC_LEFT_PGA_SEL_1(1), 1, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_B IN2_L switch", ADC_LEFT_PGA_SEL_1(1), 3, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_B IN3_L switch", ADC_LEFT_PGA_SEL_1(1), 5, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_B DIF1_L switch", ADC_LEFT_PGA_SEL_1(1), 7, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_B DIF2_L switch", ADC_LEFT_PGA_SEL_2(1), 5, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_B DIF3_L switch", ADC_LEFT_PGA_SEL_2(1), 3, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_B IN1_R switch", ADC_LEFT_PGA_SEL_2(1), 1, 0x1, 1),
};

/* ADC B Right input selection, Single Ended inputs and Differential inputs */
static const struct snd_kcontrol_new adc_b_right_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("ADC_B IN1_R switch", ADC_RIGHT_PGA_SEL_1(1), 1, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_B IN2_R switch", ADC_RIGHT_PGA_SEL_1(1), 3, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_B IN3_R switch", ADC_RIGHT_PGA_SEL_1(1), 5, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_B DIF1_R switch", ADC_RIGHT_PGA_SEL_1(1), 7, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_B DIF2_R switch", ADC_RIGHT_PGA_SEL_2(1), 5, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_B DIF3_R switch", ADC_RIGHT_PGA_SEL_2(1), 3, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_B IN1_L switch", ADC_RIGHT_PGA_SEL_2(1), 1, 0x1, 1),
};

/* Left Digital Mic input for left ADC B */
static const struct snd_kcontrol_new adc_b_left_input_dmic_controls[] = {
	SOC_DAPM_SINGLE("ADC_B Left ADC switch", ADC_ADC_DIGITAL(1), 3, 0x1, 0),
};

/* Right Digital Mic input for Right ADC B */
static const struct snd_kcontrol_new adc_b_right_input_dmic_controls[] = {
	SOC_DAPM_SINGLE("ADC_B Right ADC switch", ADC_ADC_DIGITAL(1), 2, 0x1, 0),
};

#ifdef CONFIG_SND_SOC_8_MICS
/* ADC C Left input selection, Single Ended inputs and Differential inputs */
static const struct snd_kcontrol_new adc_c_left_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("ADC_C IN1_L switch", ADC_LEFT_PGA_SEL_1(2), 1, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_C IN2_L switch", ADC_LEFT_PGA_SEL_1(2), 3, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_C IN3_L switch", ADC_LEFT_PGA_SEL_1(2), 5, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_C DIF1_L switch", ADC_LEFT_PGA_SEL_1(2), 7, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_C DIF2_L switch", ADC_LEFT_PGA_SEL_2(2), 5, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_C DIF3_L switch", ADC_LEFT_PGA_SEL_2(2), 3, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_C IN1_R switch", ADC_LEFT_PGA_SEL_2(2), 1, 0x1, 1),
};

/* ADC C Right input selection, Single Ended inputs and Differential inputs */
static const struct snd_kcontrol_new adc_c_right_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("ADC_C IN1_R switch", ADC_RIGHT_PGA_SEL_1(2), 1, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_C IN2_R switch", ADC_RIGHT_PGA_SEL_1(2), 3, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_C IN3_R switch", ADC_RIGHT_PGA_SEL_1(2), 5, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_C DIF1_R switch", ADC_RIGHT_PGA_SEL_1(2), 7, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_C DIF2_R switch", ADC_RIGHT_PGA_SEL_2(2), 5, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_C DIF3_R switch", ADC_RIGHT_PGA_SEL_2(2), 3, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_C IN1_L switch", ADC_RIGHT_PGA_SEL_2(2), 1, 0x1, 1),
};

/* ADC D Left input selection, Single Ended inputs and Differential inputs */
static const struct snd_kcontrol_new adc_d_left_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("ADC_D IN1_L switch", ADC_LEFT_PGA_SEL_1(3), 1, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_D IN2_L switch", ADC_LEFT_PGA_SEL_1(3), 3, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_D IN3_L switch", ADC_LEFT_PGA_SEL_1(3), 5, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_D DIF1_L switch", ADC_LEFT_PGA_SEL_1(3), 7, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_D DIF2_L switch", ADC_LEFT_PGA_SEL_2(3), 5, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_D DIF3_L switch", ADC_LEFT_PGA_SEL_2(3), 3, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_D IN1_R switch", ADC_LEFT_PGA_SEL_2(3), 1, 0x1, 1),
};

/* ADC D Right input selection, Single Ended inputs and Differential inputs */
static const struct snd_kcontrol_new adc_d_right_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("ADC_D IN1_R switch", ADC_RIGHT_PGA_SEL_1(3), 1, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_D IN2_R switch", ADC_RIGHT_PGA_SEL_1(3), 3, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_D IN3_R switch", ADC_RIGHT_PGA_SEL_1(3), 5, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_D DIF1_R switch", ADC_RIGHT_PGA_SEL_1(3), 7, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_D DIF2_R switch", ADC_RIGHT_PGA_SEL_2(3), 5, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_D DIF3_R switch", ADC_RIGHT_PGA_SEL_2(3), 3, 0x1, 1),
	SOC_DAPM_SINGLE("ADC_D IN1_L switch", ADC_RIGHT_PGA_SEL_2(3), 1, 0x1, 1),
};
#endif


/* the sturcture contains the different values for mclk */
static const struct adc31xx_rate_divs aic31xx_divs_pll[] = {
	/*
	 * mclk,     rate, pll_p, pll_r, pll_j, pll_d, nadc, madc, aosr, bdiv_n,
	 * codec_speficic_initializations
	 */

	/* 8k rate */
	{ 19200000,  8000,     5,     1,    24,     0,   15,    6,  128,      3 },
	/* 16k rate */
	{ 19200000, 16000,     1,     1,     5,  1200,    8,    6,  128,      3 },
	/* 44.1k rate */
	{ 19200000, 44100,     1,     1,     4,  7040,    4,    4,  128,      2 },
	/* 48k rate */
	{ 19200000, 48000,     1,     1,     5,  1200,    8,    2,  128,      1 },
	/* CODEC_IN: PLL, PLL_IN: BCLK */
	{ 9600000,  16000,     5,     1,    48,     0,   15,    3,  128,      1 },
	{ 24576000, 16000,     1,     1,     1,     0,    2,    6,  128,      2 },
	{ 4800000,  48000,     1,     1,    20,  4800,    4,    4,  128,      4 },
};

static const struct adc31xx_rate_divs aic31xx_divs_bclk[] = {
	/*
	 * mclk,     rate, pll_p, pll_r, pll_j, pll_d, nadc, madc, aosr, bdiv_n,
	 * codec_speficic_initializations
	 */
	{ 4800000,  48000,     1,     1,    41,     0,    1,    1,  100,      4 },
	{ 3072000,  16000,     1,     1,    10,  2400,    1,    3,   64,      1 },
	{ 6144000,  16000,     5,     1,    48,     0,    1,    3,  128,      1 },
	{12288000,  48000,     1,     1,     1,     0,    1,    2,  128,      1 },
};

/*
 *****************************************************************************
 * Initializations
 *****************************************************************************
 */

/*
 * AIC31xx register cache
 * We are caching the registers here. Only caching Page0 and Page1. Will not
 * cache biquad and miniDSP registers.
 * There is no point in caching the reset register.
 * NOTE: In AIC31xx, there are 127 registers supported in both page0 and page1
 *       The following table contains the page0 and page1 registers values.
 */
static const u8 aic31xx_reg[AIC31XX_CACHEREGNUM] = {
	0x00, 0x00, 0x00, 0x00,	/* 0 *//*ADC_A registers start here */
	0x00, 0x11, 0x04, 0x00,	/* 4 */
	0x00, 0x00, 0x00, 0x00,	/* 8 */
	0x00, 0x00, 0x00, 0x00,	/* 12 */
	0x00, 0x00, 0x01, 0x01,	/* 16 */
	0x80, 0x80, 0x04, 0x00,	/* 20 */
	0x00, 0x00, 0x01, 0x00,	/* 24 */
	0x00, 0x02, 0x01, 0x00,	/* 28 */
	0x00, 0x10, 0x00, 0x00,	/* 32 */
	0x00, 0x00, 0x02, 0x00,	/* 36 */
	0x00, 0x00, 0x00, 0x00,	/* 40 */
	0x00, 0x00, 0x00, 0x00,	/* 44 */
	0x00, 0x00, 0x00, 0x00,	/* 48 */
	0x00, 0x12, 0x00, 0x00,	/* 52 */
	0x00, 0x00, 0x00, 0x44,	/* 56 */
	0x00, 0x01, 0x00, 0x00,	/* 60 */
	0x00, 0x00, 0x00, 0x00,	/* 64 */
	0x00, 0x00, 0x00, 0x00,	/* 68 */
	0x00, 0x00, 0x00, 0x00,	/* 72 */
	0x00, 0x00, 0x00, 0x00,	/* 76 */
	0x00, 0x00, 0x88, 0x00,	/* 80 */
	0x00, 0x00, 0x00, 0x00,	/* 84 */
	0x7F, 0x00, 0x00, 0x00,	/* 88 */
	0x00, 0x00, 0x00, 0x00,	/* 92 */
	0x7F, 0x00, 0x00, 0x00,	/* 96 */
	0x00, 0x00, 0x00, 0x00,	/* 100 */
	0x00, 0x00, 0x00, 0x00,	/* 104 */
	0x00, 0x00, 0x00, 0x00,	/* 108 */
	0x00, 0x00, 0x00, 0x00,	/* 112 */
	0x00, 0x00, 0x00, 0x00,	/* 116 */
	0x00, 0x00, 0x00, 0x00,	/* 120 */
	0x00, 0x00, 0x00, 0x00,	/* 124 - ADC_A PAGE0 Registers(127) ends here */
	0x00, 0x00, 0x00, 0x00,	/* 128, PAGE1-0 */
	0x00, 0x00, 0x00, 0x00,	/* 132, PAGE1-4 */
	0x00, 0x00, 0x00, 0x00,	/* 136, PAGE1-8 */
	0x00, 0x00, 0x00, 0x00,	/* 140, PAGE1-12 */
	0x00, 0x00, 0x00, 0x00,	/* 144, PAGE1-16 */
	0x00, 0x00, 0x00, 0x00,	/* 148, PAGE1-20 */
	0x00, 0x00, 0x00, 0x00,	/* 152, PAGE1-24 */
	0x00, 0x00, 0x00, 0x00,	/* 156, PAGE1-28 */
	0x00, 0x00, 0x00, 0x00,	/* 160, PAGE1-32 */
	0x00, 0x00, 0x00, 0x00,	/* 164, PAGE1-36 */
	0x00, 0x00, 0x00, 0x00,	/* 168, PAGE1-40 */
	0x00, 0x00, 0x00, 0x00,	/* 172, PAGE1-44 */
	0x00, 0x00, 0x00, 0x00,	/* 176, PAGE1-48 */
	0xFF, 0x00, 0x3F, 0xFF,	/* 180, PAGE1-52 */
	0x00, 0x3F, 0x00, 0x80,	/* 184, PAGE1-56 */
	0x80, 0x00, 0x00, 0x00,	/* 188, PAGE1-60 */
	0x00, 0x00, 0x00, 0x00,	/* 192, PAGE1-64 */
	0x00, 0x00, 0x00, 0x00,	/* 196, PAGE1-68 */
	0x00, 0x00, 0x00, 0x00,	/* 200, PAGE1-72 */
	0x00, 0x00, 0x00, 0x00,	/* 204, PAGE1-76 */
	0x00, 0x00, 0x00, 0x00,	/* 208, PAGE1-80 */
	0x00, 0x00, 0x00, 0x00,	/* 212, PAGE1-84 */
	0x00, 0x00, 0x00, 0x00,	/* 216, PAGE1-88 */
	0x00, 0x00, 0x00, 0x00,	/* 220, PAGE1-92 */
	0x00, 0x00, 0x00, 0x00,	/* 224, PAGE1-96 */
	0x00, 0x00, 0x00, 0x00,	/* 228, PAGE1-100 */
	0x00, 0x00, 0x00, 0x00,	/* 232, PAGE1-104 */
	0x00, 0x00, 0x00, 0x00,	/* 236, PAGE1-108 */
	0x00, 0x00, 0x00, 0x00,	/* 240, PAGE1-112 */
	0x00, 0x00, 0x00, 0x00,	/* 244, PAGE1-116 */
	0x00, 0x00, 0x00, 0x00,	/* 248, PAGE1-120 */
	0x00, 0x00, 0x00, 0x00,	/* 252, PAGE1-124 */
	0x00, 0x00, 0x00, 0x00,	/* 0 *//* ADC_B regsiters start here */
	0x00, 0x11, 0x04, 0x00,	/* 4 */
	0x00, 0x00, 0x00, 0x00,	/* 8 */
	0x00, 0x00, 0x00, 0x00,	/* 12 */
	0x00, 0x00, 0x01, 0x01,	/* 16 */
	0x80, 0x80, 0x04, 0x00,	/* 20 */
	0x00, 0x00, 0x01, 0x00,	/* 24 */
	0x00, 0x02, 0x01, 0x00,	/* 28 */
	0x00, 0x10, 0x00, 0x00,	/* 32 */
	0x00, 0x00, 0x02, 0x00,	/* 36 */
	0x00, 0x00, 0x00, 0x00,	/* 40 */
	0x00, 0x00, 0x00, 0x00,	/* 44 */
	0x00, 0x00, 0x00, 0x00,	/* 48 */
	0x00, 0x12, 0x00, 0x00,	/* 52 */
	0x00, 0x00, 0x00, 0x44,	/* 56 */
	0x00, 0x01, 0x00, 0x00,	/* 60 */
	0x00, 0x00, 0x00, 0x00,	/* 64 */
	0x00, 0x00, 0x00, 0x00,	/* 68 */
	0x00, 0x00, 0x00, 0x00,	/* 72 */
	0x00, 0x00, 0x00, 0x00,	/* 76 */
	0x00, 0x00, 0x88, 0x00,	/* 80 */
	0x00, 0x00, 0x00, 0x00,	/* 84 */
	0x7F, 0x00, 0x00, 0x00,	/* 88 */
	0x00, 0x00, 0x00, 0x00,	/* 92 */
	0x7F, 0x00, 0x00, 0x00,	/* 96 */
	0x00, 0x00, 0x00, 0x00,	/* 100 */
	0x00, 0x00, 0x00, 0x00,	/* 104 */
	0x00, 0x00, 0x00, 0x00,	/* 108 */
	0x00, 0x00, 0x00, 0x00,	/* 112 */
	0x00, 0x00, 0x00, 0x00,	/* 116 */
	0x00, 0x00, 0x00, 0x00,	/* 120 */
	0x00, 0x00, 0x00, 0x00,	/* 124 - ADC_B PAGE0 Registers(127) ends here */
	0x00, 0x00, 0x00, 0x00,	/* 128, PAGE1-0 */
	0x00, 0x00, 0x00, 0x00,	/* 132, PAGE1-4 */
	0x00, 0x00, 0x00, 0x00,	/* 136, PAGE1-8 */
	0x00, 0x00, 0x00, 0x00,	/* 140, PAGE1-12 */
	0x00, 0x00, 0x00, 0x00,	/* 144, PAGE1-16 */
	0x00, 0x00, 0x00, 0x00,	/* 148, PAGE1-20 */
	0x00, 0x00, 0x00, 0x00,	/* 152, PAGE1-24 */
	0x00, 0x00, 0x00, 0x00,	/* 156, PAGE1-28 */
	0x00, 0x00, 0x00, 0x00,	/* 160, PAGE1-32 */
	0x00, 0x00, 0x00, 0x00,	/* 164, PAGE1-36 */
	0x00, 0x00, 0x00, 0x00,	/* 168, PAGE1-40 */
	0x00, 0x00, 0x00, 0x00,	/* 172, PAGE1-44 */
	0x00, 0x00, 0x00, 0x00,	/* 176, PAGE1-48 */
	0xFF, 0x00, 0x3F, 0xFF,	/* 180, PAGE1-52 */
	0x00, 0x3F, 0x00, 0x80,	/* 184, PAGE1-56 */
	0x80, 0x00, 0x00, 0x00,	/* 188, PAGE1-60 */
	0x00, 0x00, 0x00, 0x00,	/* 192, PAGE1-64 */
	0x00, 0x00, 0x00, 0x00,	/* 196, PAGE1-68 */
	0x00, 0x00, 0x00, 0x00,	/* 200, PAGE1-72 */
	0x00, 0x00, 0x00, 0x00,	/* 204, PAGE1-76 */
	0x00, 0x00, 0x00, 0x00,	/* 208, PAGE1-80 */
	0x00, 0x00, 0x00, 0x00,	/* 212, PAGE1-84 */
	0x00, 0x00, 0x00, 0x00,	/* 216, PAGE1-88 */
	0x00, 0x00, 0x00, 0x00,	/* 220, PAGE1-92 */
	0x00, 0x00, 0x00, 0x00,	/* 224, PAGE1-96 */
	0x00, 0x00, 0x00, 0x00,	/* 228, PAGE1-100 */
	0x00, 0x00, 0x00, 0x00,	/* 232, PAGE1-104 */
	0x00, 0x00, 0x00, 0x00,	/* 236, PAGE1-108 */
	0x00, 0x00, 0x00, 0x00,	/* 240, PAGE1-112 */
	0x00, 0x00, 0x00, 0x00,	/* 244, PAGE1-116 */
	0x00, 0x00, 0x00, 0x00,	/* 248, PAGE1-120 */
	0x00, 0x00, 0x00, 0x00,	/* 252, PAGE1-124 */
#ifdef CONFIG_SND_SOC_8_MICS
	0x00, 0x00, 0x00, 0x00,	/* 0 *//*ADC_C registers start here */
	0x00, 0x11, 0x04, 0x00,	/* 4 */
	0x00, 0x00, 0x00, 0x00,	/* 8 */
	0x00, 0x00, 0x00, 0x00,	/* 12 */
	0x00, 0x00, 0x01, 0x01,	/* 16 */
	0x80, 0x80, 0x04, 0x00,	/* 20 */
	0x00, 0x00, 0x01, 0x00,	/* 24 */
	0x00, 0x02, 0x01, 0x00,	/* 28 */
	0x00, 0x10, 0x00, 0x00,	/* 32 */
	0x00, 0x00, 0x02, 0x00,	/* 36 */
	0x00, 0x00, 0x00, 0x00,	/* 40 */
	0x00, 0x00, 0x00, 0x00,	/* 44 */
	0x00, 0x00, 0x00, 0x00,	/* 48 */
	0x00, 0x12, 0x00, 0x00,	/* 52 */
	0x00, 0x00, 0x00, 0x44,	/* 56 */
	0x00, 0x01, 0x00, 0x00,	/* 60 */
	0x00, 0x00, 0x00, 0x00,	/* 64 */
	0x00, 0x00, 0x00, 0x00,	/* 68 */
	0x00, 0x00, 0x00, 0x00,	/* 72 */
	0x00, 0x00, 0x00, 0x00,	/* 76 */
	0x00, 0x00, 0x88, 0x00,	/* 80 */
	0x00, 0x00, 0x00, 0x00,	/* 84 */
	0x7F, 0x00, 0x00, 0x00,	/* 88 */
	0x00, 0x00, 0x00, 0x00,	/* 92 */
	0x7F, 0x00, 0x00, 0x00,	/* 96 */
	0x00, 0x00, 0x00, 0x00,	/* 100 */
	0x00, 0x00, 0x00, 0x00,	/* 104 */
	0x00, 0x00, 0x00, 0x00,	/* 108 */
	0x00, 0x00, 0x00, 0x00,	/* 112 */
	0x00, 0x00, 0x00, 0x00,	/* 116 */
	0x00, 0x00, 0x00, 0x00,	/* 120 */
	0x00, 0x00, 0x00, 0x00,	/* 124 - ADC_C PAGE0 Registers(127) ends here */
	0x00, 0x00, 0x00, 0x00,	/* 128, PAGE1-0 */
	0x00, 0x00, 0x00, 0x00,	/* 132, PAGE1-4 */
	0x00, 0x00, 0x00, 0x00,	/* 136, PAGE1-8 */
	0x00, 0x00, 0x00, 0x00,	/* 140, PAGE1-12 */
	0x00, 0x00, 0x00, 0x00,	/* 144, PAGE1-16 */
	0x00, 0x00, 0x00, 0x00,	/* 148, PAGE1-20 */
	0x00, 0x00, 0x00, 0x00,	/* 152, PAGE1-24 */
	0x00, 0x00, 0x00, 0x00,	/* 156, PAGE1-28 */
	0x00, 0x00, 0x00, 0x00,	/* 160, PAGE1-32 */
	0x00, 0x00, 0x00, 0x00,	/* 164, PAGE1-36 */
	0x00, 0x00, 0x00, 0x00,	/* 168, PAGE1-40 */
	0x00, 0x00, 0x00, 0x00,	/* 172, PAGE1-44 */
	0x00, 0x00, 0x00, 0x00,	/* 176, PAGE1-48 */
	0xFF, 0x00, 0x3F, 0xFF,	/* 180, PAGE1-52 */
	0x00, 0x3F, 0x00, 0x80,	/* 184, PAGE1-56 */
	0x80, 0x00, 0x00, 0x00,	/* 188, PAGE1-60 */
	0x00, 0x00, 0x00, 0x00,	/* 192, PAGE1-64 */
	0x00, 0x00, 0x00, 0x00,	/* 196, PAGE1-68 */
	0x00, 0x00, 0x00, 0x00,	/* 200, PAGE1-72 */
	0x00, 0x00, 0x00, 0x00,	/* 204, PAGE1-76 */
	0x00, 0x00, 0x00, 0x00,	/* 208, PAGE1-80 */
	0x00, 0x00, 0x00, 0x00,	/* 212, PAGE1-84 */
	0x00, 0x00, 0x00, 0x00,	/* 216, PAGE1-88 */
	0x00, 0x00, 0x00, 0x00,	/* 220, PAGE1-92 */
	0x00, 0x00, 0x00, 0x00,	/* 224, PAGE1-96 */
	0x00, 0x00, 0x00, 0x00,	/* 228, PAGE1-100 */
	0x00, 0x00, 0x00, 0x00,	/* 232, PAGE1-104 */
	0x00, 0x00, 0x00, 0x00,	/* 236, PAGE1-108 */
	0x00, 0x00, 0x00, 0x00,	/* 240, PAGE1-112 */
	0x00, 0x00, 0x00, 0x00,	/* 244, PAGE1-116 */
	0x00, 0x00, 0x00, 0x00,	/* 248, PAGE1-120 */
	0x00, 0x00, 0x00, 0x00,	/* 252, PAGE1-124 */
	0x00, 0x00, 0x00, 0x00,	/* 0 *//* ADC_D regsiters start here */
	0x00, 0x11, 0x04, 0x00,	/* 4 */
	0x00, 0x00, 0x00, 0x00,	/* 8 */
	0x00, 0x00, 0x00, 0x00,	/* 12 */
	0x00, 0x00, 0x01, 0x01,	/* 16 */
	0x80, 0x80, 0x04, 0x00,	/* 20 */
	0x00, 0x00, 0x01, 0x00,	/* 24 */
	0x00, 0x02, 0x01, 0x00,	/* 28 */
	0x00, 0x10, 0x00, 0x00,	/* 32 */
	0x00, 0x00, 0x02, 0x00,	/* 36 */
	0x00, 0x00, 0x00, 0x00,	/* 40 */
	0x00, 0x00, 0x00, 0x00,	/* 44 */
	0x00, 0x00, 0x00, 0x00,	/* 48 */
	0x00, 0x12, 0x00, 0x00,	/* 52 */
	0x00, 0x00, 0x00, 0x44,	/* 56 */
	0x00, 0x01, 0x00, 0x00,	/* 60 */
	0x00, 0x00, 0x00, 0x00,	/* 64 */
	0x00, 0x00, 0x00, 0x00,	/* 68 */
	0x00, 0x00, 0x00, 0x00,	/* 72 */
	0x00, 0x00, 0x00, 0x00,	/* 76 */
	0x00, 0x00, 0x88, 0x00,	/* 80 */
	0x00, 0x00, 0x00, 0x00,	/* 84 */
	0x7F, 0x00, 0x00, 0x00,	/* 88 */
	0x00, 0x00, 0x00, 0x00,	/* 92 */
	0x7F, 0x00, 0x00, 0x00,	/* 96 */
	0x00, 0x00, 0x00, 0x00,	/* 100 */
	0x00, 0x00, 0x00, 0x00,	/* 104 */
	0x00, 0x00, 0x00, 0x00,	/* 108 */
	0x00, 0x00, 0x00, 0x00,	/* 112 */
	0x00, 0x00, 0x00, 0x00,	/* 116 */
	0x00, 0x00, 0x00, 0x00,	/* 120 */
	0x00, 0x00, 0x00, 0x00,	/* 124 - ADC_D PAGE0 Registers(127) ends here */
	0x00, 0x00, 0x00, 0x00,	/* 128, PAGE1-0 */
	0x00, 0x00, 0x00, 0x00,	/* 132, PAGE1-4 */
	0x00, 0x00, 0x00, 0x00,	/* 136, PAGE1-8 */
	0x00, 0x00, 0x00, 0x00,	/* 140, PAGE1-12 */
	0x00, 0x00, 0x00, 0x00,	/* 144, PAGE1-16 */
	0x00, 0x00, 0x00, 0x00,	/* 148, PAGE1-20 */
	0x00, 0x00, 0x00, 0x00,	/* 152, PAGE1-24 */
	0x00, 0x00, 0x00, 0x00,	/* 156, PAGE1-28 */
	0x00, 0x00, 0x00, 0x00,	/* 160, PAGE1-32 */
	0x00, 0x00, 0x00, 0x00,	/* 164, PAGE1-36 */
	0x00, 0x00, 0x00, 0x00,	/* 168, PAGE1-40 */
	0x00, 0x00, 0x00, 0x00,	/* 172, PAGE1-44 */
	0x00, 0x00, 0x00, 0x00,	/* 176, PAGE1-48 */
	0xFF, 0x00, 0x3F, 0xFF,	/* 180, PAGE1-52 */
	0x00, 0x3F, 0x00, 0x80,	/* 184, PAGE1-56 */
	0x80, 0x00, 0x00, 0x00,	/* 188, PAGE1-60 */
	0x00, 0x00, 0x00, 0x00,	/* 192, PAGE1-64 */
	0x00, 0x00, 0x00, 0x00,	/* 196, PAGE1-68 */
	0x00, 0x00, 0x00, 0x00,	/* 200, PAGE1-72 */
	0x00, 0x00, 0x00, 0x00,	/* 204, PAGE1-76 */
	0x00, 0x00, 0x00, 0x00,	/* 208, PAGE1-80 */
	0x00, 0x00, 0x00, 0x00,	/* 212, PAGE1-84 */
	0x00, 0x00, 0x00, 0x00,	/* 216, PAGE1-88 */
	0x00, 0x00, 0x00, 0x00,	/* 220, PAGE1-92 */
	0x00, 0x00, 0x00, 0x00,	/* 224, PAGE1-96 */
	0x00, 0x00, 0x00, 0x00,	/* 228, PAGE1-100 */
	0x00, 0x00, 0x00, 0x00,	/* 232, PAGE1-104 */
	0x00, 0x00, 0x00, 0x00,	/* 236, PAGE1-108 */
	0x00, 0x00, 0x00, 0x00,	/* 240, PAGE1-112 */
	0x00, 0x00, 0x00, 0x00,	/* 244, PAGE1-116 */
	0x00, 0x00, 0x00, 0x00,	/* 248, PAGE1-120 */
	0x00, 0x00, 0x00, 0x00,	/* 252, PAGE1-124 */
#endif
};

/*
 * aic31xx and adc31xx biquad settings
 * This structure should be loaded with biquad coefficients
 * for ADC's and AIC's as generated by the TI biquad coefficient tool
 * and as put into proper form with the biquad script
 */
static const struct aic31xx_configs biquad_settings[] = {
	/*
	 ************************
	 *** Capture Filters ****
	 ************************
	 * 20 ADC HPF Bessel
	 * Cleaner HPF without bump in 16kHz mode
	*/
	/*	Coefficient write to 3101-A ADC Left
		Page change to 4
	 */
	{0x200e, 0x7f},		/* reg[4][14] = 127 */
	{0x200f, 0x22},		/* reg[4][15] = 34 */
	{0x2010, 0x80},		/* reg[4][16] = 128 */
	{0x2011, 0xde},		/* reg[4][17] = 222 */
	{0x2012, 0x7f},		/* reg[4][18] = 127 */
	{0x2013, 0x22},		/* reg[4][19] = 34 */
	{0x2014, 0x7f},		/* reg[4][20] = 127 */
	{0x2015, 0x21},		/* reg[4][21] = 33 */
	{0x2016, 0x81},		/* reg[4][22] = 129 */
	{0x2017, 0xba},		/* reg[4][23] = 186 */
	/*	Coefficient write to 3101-A ADC Rght
		Page change to 4
	 */
	{0x204e, 0x7f},		/* reg[4][78] = 127 */
	{0x204f, 0x22},		/* reg[4][79] = 34 */
	{0x2050, 0x80},		/* reg[4][80] = 128 */
	{0x2051, 0xde},		/* reg[4][81] = 222 */
	{0x2052, 0x7f},		/* reg[4][82] = 127 */
	{0x2053, 0x22},		/* reg[4][83] = 34 */
	{0x2054, 0x7f},		/* reg[4][84] = 127 */
	{0x2055, 0x21},		/* reg[4][85] = 33 */
	{0x2056, 0x81},		/* reg[4][86] = 129 */
	{0x2057, 0xba},		/* reg[4][87] = 186 */
	/*	Coefficient write to 3101-B ADC Left
		Page change to 4
	 */
	{0x210e, 0x7f},		/* reg[4][14] = 127 */
	{0x210f, 0x22},		/* reg[4][15] = 34 */
	{0x2110, 0x80},		/* reg[4][16] = 128 */
	{0x2111, 0xde},		/* reg[4][17] = 222 */
	{0x2112, 0x7f},		/* reg[4][18] = 127 */
	{0x2113, 0x22},		/* reg[4][19] = 34 */
	{0x2114, 0x7f},		/* reg[4][20] = 127 */
	{0x2115, 0x21},		/* reg[4][21] = 33 */
	{0x2116, 0x81},		/* reg[4][22] = 129 */
	{0x2117, 0xba},		/* reg[4][23] = 186 */
	/*	Coefficient write to 3101-B ADC Rght
		Page change to 4
	 */
	{0x214e, 0x7f},		/* reg[4][78] = 127 */
	{0x214f, 0x22},		/* reg[4][79] = 34 */
	{0x2150, 0x80},		/* reg[4][80] = 128 */
	{0x2151, 0xde},		/* reg[4][81] = 222 */
	{0x2152, 0x7f},		/* reg[4][82] = 127 */
	{0x2153, 0x22},		/* reg[4][83] = 34 */
	{0x2154, 0x7f},		/* reg[4][84] = 127 */
	{0x2155, 0x21},		/* reg[4][85] = 33 */
	{0x2156, 0x81},		/* reg[4][86] = 129 */
	{0x2157, 0xba},		/* reg[4][87] = 186 */
#ifdef CONFIG_SND_SOC_8_MICS
	/*	Coefficient write to 3101-C ADC Left
		Page change to 4
	 */
	{0x220e, 0x7f},		/* reg[4][14] = 127 */
	{0x220f, 0x22},		/* reg[4][15] = 34 */
	{0x2210, 0x80},		/* reg[4][16] = 128 */
	{0x2211, 0xde},		/* reg[4][17] = 222 */
	{0x2212, 0x7f},		/* reg[4][18] = 127 */
	{0x2213, 0x22},		/* reg[4][19] = 34 */
	{0x2214, 0x7f},		/* reg[4][20] = 127 */
	{0x2215, 0x21},		/* reg[4][21] = 33 */
	{0x2216, 0x81},		/* reg[4][22] = 129 */
	{0x2217, 0xba},		/* reg[4][23] = 186 */
	/*	Coefficient write to 3101-C ADC Rght
		Page change to 4
	 */
	{0x224e, 0x7f},		/* reg[4][78] = 127 */
	{0x224f, 0x22},		/* reg[4][79] = 34 */
	{0x2250, 0x80},		/* reg[4][80] = 128 */
	{0x2251, 0xde},		/* reg[4][81] = 222 */
	{0x2252, 0x7f},		/* reg[4][82] = 127 */
	{0x2253, 0x22},		/* reg[4][83] = 34 */
	{0x2254, 0x7f},		/* reg[4][84] = 127 */
	{0x2255, 0x21},		/* reg[4][85] = 33 */
	{0x2256, 0x81},		/* reg[4][86] = 129 */
	{0x2257, 0xba},		/* reg[4][87] = 186 */
	/*	Coefficient write to 3101-D ADC Left
		Page change to 4
	 */
	{0x230e, 0x7f},		/* reg[4][14] = 127 */
	{0x230f, 0x22},		/* reg[4][15] = 34 */
	{0x2310, 0x80},		/* reg[4][16] = 128 */
	{0x2311, 0xde},		/* reg[4][17] = 222 */
	{0x2312, 0x7f},		/* reg[4][18] = 127 */
	{0x2313, 0x22},		/* reg[4][19] = 34 */
	{0x2314, 0x7f},		/* reg[4][20] = 127 */
	{0x2315, 0x21},		/* reg[4][21] = 33 */
	{0x2316, 0x81},		/* reg[4][22] = 129 */
	{0x2317, 0xba},		/* reg[4][23] = 186 */
	/*	Coefficient write to 3101-D ADC Rght
		Page change to 4
	 */
	{0x234e, 0x7f},		/* reg[4][78] = 127 */
	{0x234f, 0x22},		/* reg[4][79] = 34 */
	{0x2350, 0x80},		/* reg[4][80] = 128 */
	{0x2351, 0xde},		/* reg[4][81] = 222 */
	{0x2352, 0x7f},		/* reg[4][82] = 127 */
	{0x2353, 0x22},		/* reg[4][83] = 34 */
	{0x2354, 0x7f},		/* reg[4][84] = 127 */
	{0x2355, 0x21},		/* reg[4][85] = 33 */
	{0x2356, 0x81},		/* reg[4][86] = 129 */
	{0x2357, 0xba},		/* reg[4][87] = 186 */
#endif
};

/* adc31xx_a Widget structure */
static const struct snd_soc_dapm_widget adc31xx_dapm_widgets_a[] = {

	/* Left Input Selection */
	SND_SOC_DAPM_MIXER("ADC_A Left Ip Select", SND_SOC_NOPM, 0, 0,
			   &adc_a_left_input_mixer_controls[0],
			   ARRAY_SIZE(adc_a_left_input_mixer_controls)),
	/* Right Input Select */
	SND_SOC_DAPM_MIXER("ADC_A Right Ip Select", SND_SOC_NOPM, 0, 0,
			   &adc_a_right_input_mixer_controls[0],
			   ARRAY_SIZE(adc_a_right_input_mixer_controls)),
	/* PGA selection */
	SND_SOC_DAPM_PGA("ADC_A Left PGA", ADC_LEFT_APGA_CTRL(0), 7, 1, NULL,
			 0),
	SND_SOC_DAPM_PGA("ADC_A Right PGA", ADC_RIGHT_APGA_CTRL(0), 7, 1, NULL,
			 0),

	/*Digital Microphone Input Control for Left/Right ADC */
	SND_SOC_DAPM_MIXER("ADC_A Left DMic Ip", SND_SOC_NOPM, 0, 0,
			   &adc_a_left_input_dmic_controls[0],
			   ARRAY_SIZE(adc_a_left_input_dmic_controls)),
	SND_SOC_DAPM_MIXER("ADC_A Right DMic Ip", SND_SOC_NOPM, 0, 0,
			   &adc_a_right_input_dmic_controls[0],
			   ARRAY_SIZE(adc_a_right_input_dmic_controls)),

	/* Left/Right ADC */
	SND_SOC_DAPM_ADC("ADC_A Left ADC", "Capture",
			 ADC_ADC_DIGITAL(0), 7, 0),
	SND_SOC_DAPM_ADC("ADC_A Right ADC", "Capture",
			 ADC_ADC_DIGITAL(0), 6, 0),

	/* Inputs */
	SND_SOC_DAPM_INPUT("ADC_A_IN1_L"),
	SND_SOC_DAPM_INPUT("ADC_A_IN1_R"),
	SND_SOC_DAPM_INPUT("ADC_A_IN2_L"),
	SND_SOC_DAPM_INPUT("ADC_A_IN2_R"),
	SND_SOC_DAPM_INPUT("ADC_A_IN3_L"),
	SND_SOC_DAPM_INPUT("ADC_A_IN3_R"),
	SND_SOC_DAPM_INPUT("ADC_A_DIF1_L"),
	SND_SOC_DAPM_INPUT("ADC_A_DIF2_L"),
	SND_SOC_DAPM_INPUT("ADC_A_DIF3_L"),
	SND_SOC_DAPM_INPUT("ADC_A_DIF1_R"),
	SND_SOC_DAPM_INPUT("ADC_A_DIF2_R"),
	SND_SOC_DAPM_INPUT("ADC_A_DIF3_R"),
	SND_SOC_DAPM_INPUT("ADC_A_DMic_L"),
	SND_SOC_DAPM_INPUT("ADC_A_DMic_R"),
};

/* adc31xx_b Widget structure */
static const struct snd_soc_dapm_widget adc31xx_dapm_widgets_b[] = {

	/* Left Input Selection */
	SND_SOC_DAPM_MIXER("ADC_B Left Ip Select", SND_SOC_NOPM, 0, 0,
			   &adc_b_left_input_mixer_controls[0],
			   ARRAY_SIZE(adc_b_left_input_mixer_controls)),
	/* Right Input Selection */
	SND_SOC_DAPM_MIXER("ADC_B Right Ip Select", SND_SOC_NOPM, 0, 0,
			   &adc_b_right_input_mixer_controls[0],
			   ARRAY_SIZE(adc_b_right_input_mixer_controls)),
	/*PGA selection */
	SND_SOC_DAPM_PGA("ADC_B Left PGA", ADC_LEFT_APGA_CTRL(1), 7, 1, NULL,
			 0),
	SND_SOC_DAPM_PGA("ADC_B Right PGA", ADC_RIGHT_APGA_CTRL(1), 7, 1, NULL,
			 0),

	/*Digital Microphone Input Control for Left/Right ADC */
	SND_SOC_DAPM_MIXER("ADC_B Left DMic Ip", SND_SOC_NOPM, 0, 0,
			   &adc_b_left_input_dmic_controls[0],
			   ARRAY_SIZE(adc_b_left_input_dmic_controls)),
	SND_SOC_DAPM_MIXER("ADC_B Right DMic Ip", SND_SOC_NOPM, 0, 0,
			   &adc_b_right_input_dmic_controls[0],
			   ARRAY_SIZE(adc_b_right_input_dmic_controls)),

	SND_SOC_DAPM_ADC("ADC_B Left ADC", "Capture",
			 ADC_ADC_DIGITAL(1),
			 7, 0),
	SND_SOC_DAPM_ADC("ADC_B Right ADC", "Capture",
			 ADC_ADC_DIGITAL(1), 6, 0),

	/* Inputs */
	SND_SOC_DAPM_INPUT("ADC_B_IN1_L"),
	SND_SOC_DAPM_INPUT("ADC_B_IN1_R"),
	SND_SOC_DAPM_INPUT("ADC_B_IN2_L"),
	SND_SOC_DAPM_INPUT("ADC_B_IN2_R"),
	SND_SOC_DAPM_INPUT("ADC_B_IN3_L"),
	SND_SOC_DAPM_INPUT("ADC_B_IN3_R"),
	SND_SOC_DAPM_INPUT("ADC_B_DIF1_L"),
	SND_SOC_DAPM_INPUT("ADC_B_DIF2_L"),
	SND_SOC_DAPM_INPUT("ADC_B_DIF3_L"),
	SND_SOC_DAPM_INPUT("ADC_B_DIF1_R"),
	SND_SOC_DAPM_INPUT("ADC_B_DIF2_R"),
	SND_SOC_DAPM_INPUT("ADC_B_DIF3_R"),
	SND_SOC_DAPM_INPUT("ADC_B_DMic_L"),
	SND_SOC_DAPM_INPUT("ADC_B_DMic_R"),
};

#ifdef CONFIG_SND_SOC_8_MICS
/* adc31xx_c Widget structure */
static const struct snd_soc_dapm_widget adc31xx_dapm_widgets_c[] = {

	/* Left Input Selection */
	SND_SOC_DAPM_MIXER("ADC_C Left Ip Select", SND_SOC_NOPM, 0, 0,
			   &adc_c_left_input_mixer_controls[0],
			   ARRAY_SIZE(adc_c_left_input_mixer_controls)),
	/* Right Input Selection */
	SND_SOC_DAPM_MIXER("ADC_C Right Ip Select", SND_SOC_NOPM, 0, 0,
			   &adc_c_right_input_mixer_controls[0],
			   ARRAY_SIZE(adc_c_right_input_mixer_controls)),
	/*PGA selection */
	SND_SOC_DAPM_PGA("ADC_C Left PGA", ADC_LEFT_APGA_CTRL(2), 7, 1, NULL,
			 0),
	SND_SOC_DAPM_PGA("ADC_C Right PGA", ADC_RIGHT_APGA_CTRL(2), 7, 1, NULL,
			 0),

	SND_SOC_DAPM_ADC("ADC_C Left ADC", "Capture",
			 ADC_ADC_DIGITAL(2),
			 7, 0),
	SND_SOC_DAPM_ADC("ADC_C Right ADC", "Capture",
			 ADC_ADC_DIGITAL(2), 6, 0),

	/* Inputs */
	SND_SOC_DAPM_INPUT("ADC_C_IN1_L"),
	SND_SOC_DAPM_INPUT("ADC_C_IN1_R"),
	SND_SOC_DAPM_INPUT("ADC_C_IN2_L"),
	SND_SOC_DAPM_INPUT("ADC_C_IN2_R"),
	SND_SOC_DAPM_INPUT("ADC_C_IN3_L"),
	SND_SOC_DAPM_INPUT("ADC_C_IN3_R"),
	SND_SOC_DAPM_INPUT("ADC_C_DIF1_L"),
	SND_SOC_DAPM_INPUT("ADC_C_DIF2_L"),
	SND_SOC_DAPM_INPUT("ADC_C_DIF3_L"),
	SND_SOC_DAPM_INPUT("ADC_C_DIF1_R"),
	SND_SOC_DAPM_INPUT("ADC_C_DIF2_R"),
	SND_SOC_DAPM_INPUT("ADC_C_DIF3_R"),
	SND_SOC_DAPM_INPUT("ADC_C_DMic_L"),
	SND_SOC_DAPM_INPUT("ADC_C_DMic_R"),
};

/* adc31xx_d Widget structure */
static const struct snd_soc_dapm_widget adc31xx_dapm_widgets_d[] = {

	/* Left Input Selection */
	SND_SOC_DAPM_MIXER("ADC_D Left Ip Select", SND_SOC_NOPM, 0, 0,
			   &adc_d_left_input_mixer_controls[0],
			   ARRAY_SIZE(adc_d_left_input_mixer_controls)),
	/* Right Input Selection */
	SND_SOC_DAPM_MIXER("ADC_D Right Ip Select", SND_SOC_NOPM, 0, 0,
			   &adc_d_right_input_mixer_controls[0],
			   ARRAY_SIZE(adc_d_right_input_mixer_controls)),
	/*PGA selection */
	SND_SOC_DAPM_PGA("ADC_D Left PGA", ADC_LEFT_APGA_CTRL(3), 7, 1, NULL,
			 0),
	SND_SOC_DAPM_PGA("ADC_D Right PGA", ADC_RIGHT_APGA_CTRL(3), 7, 1, NULL,
			 0),

	SND_SOC_DAPM_ADC("ADC_D Left ADC", "Capture",
			 ADC_ADC_DIGITAL(3),
			 7, 0),
	SND_SOC_DAPM_ADC("ADC_D Right ADC", "Capture",
			 ADC_ADC_DIGITAL(3), 6, 0),

	/* Inputs */
	SND_SOC_DAPM_INPUT("ADC_D_IN1_L"),
	SND_SOC_DAPM_INPUT("ADC_D_IN1_R"),
	SND_SOC_DAPM_INPUT("ADC_D_IN2_L"),
	SND_SOC_DAPM_INPUT("ADC_D_IN2_R"),
	SND_SOC_DAPM_INPUT("ADC_D_IN3_L"),
	SND_SOC_DAPM_INPUT("ADC_D_IN3_R"),
	SND_SOC_DAPM_INPUT("ADC_D_DIF1_L"),
	SND_SOC_DAPM_INPUT("ADC_D_DIF2_L"),
	SND_SOC_DAPM_INPUT("ADC_D_DIF3_L"),
	SND_SOC_DAPM_INPUT("ADC_D_DIF1_R"),
	SND_SOC_DAPM_INPUT("ADC_D_DIF2_R"),
	SND_SOC_DAPM_INPUT("ADC_D_DIF3_R"),
	SND_SOC_DAPM_INPUT("ADC_D_DMic_L"),
	SND_SOC_DAPM_INPUT("ADC_D_DMic_R"),
};
#endif

/* DAPM Routing related array declaration for ADC_A */
static const struct snd_soc_dapm_route intercon_a[] = {

	/* Left input selection from switchs */
	{"ADC_A Left Ip Select", "ADC_A IN1_L switch", "ADC_A_IN1_L"},
	{"ADC_A Left Ip Select", "ADC_A IN2_L switch", "ADC_A_IN2_L"},
	{"ADC_A Left Ip Select", "ADC_A IN3_L switch", "ADC_A_IN3_L"},
	{"ADC_A Left Ip Select", "ADC_A DIF1_L switch", "ADC_A_DIF1_L"},
	{"ADC_A Left Ip Select", "ADC_A DIF2_L switch", "ADC_A_DIF2_L"},
	{"ADC_A Left Ip Select", "ADC_A DIF3_L switch", "ADC_A_DIF3_L"},
	{"ADC_A Left Ip Select", "ADC_A IN1_R switch", "ADC_A_IN1_R"},

	/* Left input selection to left PGA */
	{"ADC_A Left PGA", NULL, "ADC_A Left Ip Select"},

	/* Left PGA to left ADC */
	{"ADC_A Left ADC", NULL, "ADC_A Left PGA"},

	/* Right input selection from switchs */
	{"ADC_A Right Ip Select", "ADC_A IN1_R switch", "ADC_A_IN1_R"},
	{"ADC_A Right Ip Select", "ADC_A IN2_R switch", "ADC_A_IN2_R"},
	{"ADC_A Right Ip Select", "ADC_A IN3_R switch", "ADC_A_IN3_R"},
	{"ADC_A Right Ip Select", "ADC_A DIF1_R switch", "ADC_A_DIF1_R"},
	{"ADC_A Right Ip Select", "ADC_A DIF2_R switch", "ADC_A_DIF2_R"},
	{"ADC_A Right Ip Select", "ADC_A DIF3_R switch", "ADC_A_DIF3_R"},
	{"ADC_A Right Ip Select", "ADC_A IN1_L switch", "ADC_A_IN1_L"},

	/* Right input selection to right PGA */
	{"ADC_A Right PGA", NULL, "ADC_A Right Ip Select"},

	/* Right PGA to right ADC */
	{"ADC_A Right ADC", NULL, "ADC_A Right PGA"},

	/* Left DMic Input selection from switch */
	{"ADC_A Left DMic Ip", "ADC_A Left ADC switch", "ADC_A_DMic_L"},

	/* Left DMic to left ADC */
	{"ADC_A Left ADC", NULL, "ADC_A Left DMic Ip"},

	/* Right DMic Input selection from switch */
	{"ADC_A Right DMic Ip", "ADC_A Right ADC switch", "ADC_A_DMic_R"},

	/* Right DMic to right ADC */
	{"ADC_A Right ADC", NULL, "ADC_A Right DMic Ip"},
};

/* DAPM Routing related array declaration for ADC_B */
static const struct snd_soc_dapm_route intercon_b[] = {

	/* Left input selection from switchs */
	{"ADC_B Left Ip Select", "ADC_B IN1_L switch", "ADC_B_IN1_L"},
	{"ADC_B Left Ip Select", "ADC_B IN2_L switch", "ADC_B_IN2_L"},
	{"ADC_B Left Ip Select", "ADC_B IN3_L switch", "ADC_B_IN3_L"},
	{"ADC_B Left Ip Select", "ADC_B DIF1_L switch", "ADC_B_DIF1_L"},
	{"ADC_B Left Ip Select", "ADC_B DIF2_L switch", "ADC_B_DIF2_L"},
	{"ADC_B Left Ip Select", "ADC_B DIF3_L switch", "ADC_B_DIF3_L"},
	{"ADC_B Left Ip Select", "ADC_B IN1_R switch", "ADC_B_IN1_R"},

	/* Left input selection to left PGA */
	{"ADC_B Left PGA", NULL, "ADC_B Left Ip Select"},

	/* Left PGA to left ADC */
	{"ADC_B Left ADC", NULL, "ADC_B Left PGA"},

	/* Right input selection from switchs */
	{"ADC_B Right Ip Select", "ADC_B IN1_R switch", "ADC_B_IN1_R"},
	{"ADC_B Right Ip Select", "ADC_B IN2_R switch", "ADC_B_IN2_R"},
	{"ADC_B Right Ip Select", "ADC_B IN3_R switch", "ADC_B_IN3_R"},
	{"ADC_B Right Ip Select", "ADC_B DIF1_R switch", "ADC_B_DIF1_R"},
	{"ADC_B Right Ip Select", "ADC_B DIF2_R switch", "ADC_B_DIF2_R"},
	{"ADC_B Right Ip Select", "ADC_B DIF3_R switch", "ADC_B_DIF3_R"},
	{"ADC_B Right Ip Select", "ADC_B IN1_L switch", "ADC_B_IN1_L"},

	/* Right input selection to right PGA */
	{"ADC_B Right PGA", NULL, "ADC_B Right Ip Select"},

	/* Right PGA to right ADC */
	{"ADC_B Right ADC", NULL, "ADC_B Right PGA"},

	/* Left DMic Input selection from switch */
	{"ADC_B Left DMic Ip", "ADC_B Left ADC switch", "ADC_B_DMic_L"},

	/* Left DMic to left ADC */
	{"ADC_B Left ADC", NULL, "ADC_B Left DMic Ip"},

	/* Right DMic Input selection from switch */
	{"ADC_B Right DMic Ip", "ADC_B Right ADC switch", "ADC_B_DMic_R"},

	/* Right DMic to right ADC */
	{"ADC_B Right ADC", NULL, "ADC_B Right DMic Ip"},
};

#ifdef CONFIG_SND_SOC_8_MICS
/* DAPM Routing related array declaration for ADC_C */
static const struct snd_soc_dapm_route intercon_c[] = {
	/* Left input selection from switchs */
	{"ADC_C Left Ip Select", "ADC_C IN1_L switch", "ADC_C_IN1_L"},
	{"ADC_C Left Ip Select", "ADC_C IN2_L switch", "ADC_C_IN2_L"},
	{"ADC_C Left Ip Select", "ADC_C IN3_L switch", "ADC_C_IN3_L"},
	{"ADC_C Left Ip Select", "ADC_C IN1_R switch", "ADC_C_IN1_R"},
	{"ADC_C Left Ip Select", "ADC_C DIF1_L switch", "ADC_C_DIF1_L"},
	{"ADC_C Left Ip Select", "ADC_C DIF2_L switch", "ADC_C_DIF2_L"},
	{"ADC_C Left Ip Select", "ADC_C DIF3_L switch", "ADC_C_DIF3_L"},

	/* Left input selection to left PGA */
	{"ADC_C Left PGA", NULL, "ADC_C Left Ip Select"},

	/* Left PGA to left ADC */
	{"ADC_C Left ADC", NULL, "ADC_C Left PGA"},

	/* Right input selection from switchs */
	{"ADC_C Right Ip Select", "ADC_C IN1_R switch", "ADC_C_IN1_R"},
	{"ADC_C Right Ip Select", "ADC_C IN2_R switch", "ADC_C_IN2_R"},
	{"ADC_C Right Ip Select", "ADC_C IN3_R switch", "ADC_C_IN3_R"},
	{"ADC_C Right Ip Select", "ADC_C IN1_L switch", "ADC_C_IN1_L"},
	{"ADC_C Right Ip Select", "ADC_C DIF1_R switch", "ADC_C_DIF1_R"},
	{"ADC_C Right Ip Select", "ADC_C DIF2_R switch", "ADC_C_DIF2_R"},
	{"ADC_C Right Ip Select", "ADC_C DIF3_R switch", "ADC_C_DIF3_R"},

	/* Right input selection to right PGA */
	{"ADC_C Right PGA", NULL, "ADC_C Right Ip Select"},

	/* Right PGA to right ADC */
	{"ADC_C Right ADC", NULL, "ADC_C Right PGA"},
};

/* DAPM Routing related array declaration for ADC_D */
static const struct snd_soc_dapm_route intercon_d[] = {
	/* Left input selection from switchs */
	{"ADC_D Left Ip Select", "ADC_D IN1_L switch", "ADC_D_IN1_L"},
	{"ADC_D Left Ip Select", "ADC_D IN2_L switch", "ADC_D_IN2_L"},
	{"ADC_D Left Ip Select", "ADC_D IN3_L switch", "ADC_D_IN3_L"},
	{"ADC_D Left Ip Select", "ADC_D IN1_R switch", "ADC_D_IN1_R"},
	{"ADC_D Left Ip Select", "ADC_D DIF1_L switch", "ADC_D_DIF1_L"},
	{"ADC_D Left Ip Select", "ADC_D DIF2_L switch", "ADC_D_DIF2_L"},
	{"ADC_D Left Ip Select", "ADC_D DIF3_L switch", "ADC_D_DIF3_L"},

	/* Left input selection to left PGA */
	{"ADC_D Left PGA", NULL, "ADC_D Left Ip Select"},

	/* Left PGA to left ADC */
	{"ADC_D Left ADC", NULL, "ADC_D Left PGA"},

	/* Right input selection from switchs */
	{"ADC_D Right Ip Select", "ADC_D IN1_R switch", "ADC_D_IN1_R"},
	{"ADC_D Right Ip Select", "ADC_D IN2_R switch", "ADC_D_IN2_R"},
	{"ADC_D Right Ip Select", "ADC_D IN3_R switch", "ADC_D_IN3_R"},
	{"ADC_D Right Ip Select", "ADC_D IN1_L switch", "ADC_D_IN1_L"},
	{"ADC_D Right Ip Select", "ADC_D DIF1_R switch", "ADC_D_DIF1_R"},
	{"ADC_D Right Ip Select", "ADC_D DIF2_R switch", "ADC_D_DIF2_R"},
	{"ADC_D Right Ip Select", "ADC_D DIF3_R switch", "ADC_D_DIF3_R"},

	/* Right input selection to right PGA */
	{"ADC_D Right PGA", NULL, "ADC_D Right Ip Select"},

	/* Right PGA to right ADC */
	{"ADC_D Right ADC", NULL, "ADC_D Right PGA"},
};

#endif

/**********************************************************************************
 * Internal Data Collection
 * Fix the values for DRC - PGA - Crossover
 *********************************************************************************/
static const struct aic31xx_configs biquad_settings_Capt_IDC[] = {
	/**
	 *   ========================
	 *   =    Capture Filters   =
	 *   ========================
	 *   20 ADC HPF Bessel
	 *   Cleaner HPF without bump in 16kHz mode
	 */
	/*    Coefficient write to 3101-A ADC Left
	      Page change to 4
	*/
	{0x200e, 0x7f},		/* reg[4][14] = 127 */
	{0x200f, 0x22},		/* reg[4][15] = 34 */
	{0x2010, 0x80},		/* reg[4][16] = 128 */
	{0x2011, 0xde},		/* reg[4][17] = 222 */
	{0x2012, 0x7f},		/* reg[4][18] = 127 */
	{0x2013, 0x22},		/* reg[4][19] = 34 */
	{0x2014, 0x7f},		/* reg[4][20] = 127 */
	{0x2015, 0x21},		/* reg[4][21] = 33 */
	{0x2016, 0x81},		/* reg[4][22] = 129 */
	{0x2017, 0xba},		/* reg[4][23] = 186 */
	/*     Coefficient write to 3101-A ADC Rght
	       Page change to 4
	*/
	{0x204e, 0x7f},		/* reg[4][78] = 127 */
	{0x204f, 0x22},		/* reg[4][79] = 34 */
	{0x2050, 0x80},		/* reg[4][80] = 128 */
	{0x2051, 0xde},		/* reg[4][81] = 222 */
	{0x2052, 0x7f},		/* reg[4][82] = 127 */
	{0x2053, 0x22},		/* reg[4][83] = 34 */
	{0x2054, 0x7f},		/* reg[4][84] = 127 */
	{0x2055, 0x21},		/* reg[4][85] = 33 */
	{0x2056, 0x81},		/* reg[4][86] = 129 */
	{0x2057, 0xba},		/* reg[4][87] = 186 */
	/*    Coefficient write to 3101-B ADC Left
	      Page change to 4
	 */
	{0x210e, 0x7f},		/* reg[4][14] = 127 */
	{0x210f, 0x22},		/* reg[4][15] = 34 */
	{0x2110, 0x80},		/* reg[4][16] = 128 */
	{0x2111, 0xde},		/* reg[4][17] = 222 */
	{0x2112, 0x7f},		/* reg[4][18] = 127 */
	{0x2113, 0x22},		/* reg[4][19] = 34 */
	{0x2114, 0x7f},		/* reg[4][20] = 127 */
	{0x2115, 0x21},		/* reg[4][21] = 33 */
	{0x2116, 0x81},		/* reg[4][22] = 129 */
	{0x2117, 0xba},		/* reg[4][23] = 186 */
	/*    Coefficient write to 3101-B ADC Rght
	      Page change to 4
	 */
	{0x214e, 0x7f},		/* reg[4][78] = 127 */
	{0x214f, 0x22},		/* reg[4][79] = 34 */
	{0x2150, 0x80},		/* reg[4][80] = 128 */
	{0x2151, 0xde},		/* reg[4][81] = 222 */
	{0x2152, 0x7f},		/* reg[4][82] = 127 */
	{0x2153, 0x22},		/* reg[4][83] = 34 */
	{0x2154, 0x7f},		/* reg[4][84] = 127 */
	{0x2155, 0x21},		/* reg[4][85] = 33 */
	{0x2156, 0x81},		/* reg[4][86] = 129 */
	{0x2157, 0xba},		/* reg[4][87] = 186 */

	/* config record path default gains */
	{ADC_LEFT_APGA_CTRL(0), MIC_PGA_GAIN_IDC},	/* PGA 20dB Gain */
	{ADC_RIGHT_APGA_CTRL(0), MIC_PGA_GAIN_IDC},	/* PGA 20dB Gain */
	{ADC_LEFT_APGA_CTRL(1), MIC_PGA_GAIN_IDC},	/* PGA 20dB Gain */
	{ADC_RIGHT_APGA_CTRL(1), MIC_PGA_GAIN_IDC},	/* PGA 20dB Gain */
};

/**----------------------------------------------------------------------------
 * Function : __IDC_info
 * Purpose  : This function is to initialize data to enable IDC mode.
 *----------------------------------------------------------------------------*/
static int __IDC_info(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

/* No need to initialise statics to 0 or NULL */
static int IDC_enabled;
/**----------------------------------------------------------------------------
 * Function : __IDC_get
 * Purpose  : This function is used to get the state of IDC mode.
 *----------------------------------------------------------------------------*/
static int __IDC_get(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = IDC_enabled;

	return 0;
}

/**----------------------------------------------------------------------------
 * Function : __IDC_put
 * Purpose  : Called to enable the IDC mode.
 *----------------------------------------------------------------------------*/
static int __IDC_put(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_value *ucontrol)
{
	int enable = ucontrol->value.integer.value[0];

	IDC_enabled = enable;

	return 0;
}

/*********************************************************************************
 * End of Internal Data Collection
 ********************************************************************************/


static void aic31xx_apply_tuning(struct snd_soc_codec *codec)
{
	const struct aic31xx_configs *config;
	size_t config_size;
	int i;

	config = IDC_enabled ?
		biquad_settings_Capt_IDC : biquad_settings;
	config_size = IDC_enabled ?
		ARRAY_SIZE(biquad_settings_Capt_IDC) : ARRAY_SIZE(biquad_settings);

	for (i = 0; i < config_size; i++) {
		u32 offset = config[i].reg_offset;
		u8 val = config[i].reg_val;
		snd_soc_write(codec, offset, val);
	}

	/* Set the signal processing block (PRB) modes */
	for (i = 0; i < NUM_ADC3101; i++)
		snd_soc_write(codec, ADC_PRB_SELECT(i), 0x2);
}

/*
 *----------------------------------------------------------------------------
 * Function : adc31xx_change_page
 * Purpose  : This function is to switch between page 0 and page 1.
 *
 *----------------------------------------------------------------------------
 */
static int adc31xx_change_page(unsigned int device, struct snd_soc_codec *codec,
			       u8 new_page)
{
	struct aic31xx_priv *adc31xx = snd_soc_codec_get_drvdata(codec);

	u8 data[2];

	data[0] = 0x0;
	data[1] = new_page;

	if (i2c_master_send(adc31xx->adc_control_data[device], data, 2) != 2) {
		dev_err(codec->dev, "adc31xx_change_page %d: I2C Wrte Error\n",
			device);
		return -1;
	}
	adc31xx->adc_page_no[device] = new_page;
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic31xx_write_reg_cache
 * Purpose  : This function is to write aic31xx register cache
 *----------------------------------------------------------------------------
 */
static inline void aic31xx_write_reg_cache(struct snd_soc_codec *codec, u8 device,
					   u8 page, u16 reg, u8 value)
{
	u8 *cache = codec->reg_cache;
	int offset;

	offset = device * ADC3101_CACHEREGNUM;

	if (page == 1)
		offset += 128;

	if (offset + reg >= AIC31XX_CACHEREGNUM)
		return;

	dev_dbg(codec->dev, "(%d) wrote %#x to %#x (%d) cache offset %d\n",
		device, value, reg, reg, offset);

	cache[offset + reg] = value;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic31xx_read_reg_cache
 * Purpose  : This function is to read the register cache
 *
 *----------------------------------------------------------------------------
 */
static unsigned int aic31xx_read_reg_cache(struct snd_soc_codec *codec,
					   unsigned int reg)
{
	u8 *cache = codec->reg_cache;
	u16 device, page, reg_no;
	int offset;

	page = (reg & 0x3f800) >> 11;
	device = (reg & 0x700) >> 8;
	reg_no = reg & 0x7f;

	if (page > 1) {
		dev_warn(codec->dev, "unable to read register: page:%u, reg:%u\n",
			 page, reg_no);
		return -EINVAL;
	}

	offset = device * ADC3101_CACHEREGNUM;

	if (page == 1)
		offset += 128;

	if (offset + reg_no >= AIC31XX_CACHEREGNUM) {
		dev_warn(codec->dev, "unable to read register: page:%u, reg:%u\n",
			 page, reg_no);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "(%d) read %#x from %#x (%d) cache offset %d\n",
		device, cache[offset + reg_no], reg_no, reg_no, offset);

	return cache[offset + reg_no];
}

/*
 *----------------------------------------------------------------------------
 * Function : aic31xx_write
 * Purpose  : This function is to write to the aic31xx register space.
 *----------------------------------------------------------------------------
 */
static int aic31xx_write(struct snd_soc_codec *codec, unsigned int reg,
			 unsigned int value)
{
	int ret = 0;
	u8 data[2];
	u16 page;
	u16 device;
	unsigned int reg_no;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
	struct i2c_client *i2cdev;

	if (!aic31xx) {
		return -ENODEV;
	}

	page = (reg & 0x3f800) >> 11;
	/* bit7 is zero while using MAKE_REG */
	/* bit7 is used by soc-cache to store page */
	if (reg & 0x80)
		page = 1;
	device = (reg & 0x700) >> 8;
	reg_no = reg & 0x7f;

	dev_dbg(codec->dev, "AIC Write dev %#x page %d reg %#x val %#x\n",
		device, page, reg_no, value);

	i2cdev = aic31xx->adc_control_data[device];
	if (!i2cdev) {
		dev_err(codec->dev,
			"Error: AIC Write dev %#x page %d reg %#x val %#x (no i2c device)\n",
			device, page, reg_no, value);
		return -ENODEV;
	}

	mutex_lock(&aic31xx->codecMutex);

	if (aic31xx->adc_page_no[device] != page)
		adc31xx_change_page(device, codec, page);

	/*
	 * data is
	 *   D15..D8 register offset
	 *   D7...D0 register data
	 */
	data[0] = reg_no;
	data[1] = value;

	if (i2c_master_send(i2cdev, data, 2) != 2) {
		dev_err(codec->dev, "Error in i2c write in i2c dev: %x \n",
			device);

		ret = -EIO;
	} else if ((page < 2) && !((page == 0) && (reg == 1))) {
		/* Shadow the first two pages only */
		aic31xx_write_reg_cache(codec, device, page, reg_no, value);
	}

	mutex_unlock(&aic31xx->codecMutex);

	return ret;
}

/*
 * aic31xx_add_widgets
 *
 * adds all the ASoC Widgets identified by aic31xx_snd_controls array. This
 * routine will be invoked * during the Audio Driver Initialization.
 */
static int aic31xx_add_widgets(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	dev_dbg(codec->dev, "###aic31xx_add_widgets\n");

	snd_soc_dapm_new_controls(dapm, adc31xx_dapm_widgets_a,
				  ARRAY_SIZE(adc31xx_dapm_widgets_a));
	snd_soc_dapm_new_controls(dapm, adc31xx_dapm_widgets_b,
				  ARRAY_SIZE(adc31xx_dapm_widgets_b));

	/* set up audio path interconnects */
	snd_soc_dapm_add_routes(dapm, intercon_a,
				ARRAY_SIZE(intercon_a));
	snd_soc_dapm_add_routes(dapm, intercon_b,
				ARRAY_SIZE(intercon_b));

#ifdef CONFIG_SND_SOC_8_MICS
	snd_soc_dapm_new_controls(dapm, adc31xx_dapm_widgets_c,
				  ARRAY_SIZE(adc31xx_dapm_widgets_c));
	snd_soc_dapm_new_controls(dapm, adc31xx_dapm_widgets_d,
				  ARRAY_SIZE(adc31xx_dapm_widgets_d));

	/* set up audio path interconnects */
	snd_soc_dapm_add_routes(dapm, intercon_c,
				ARRAY_SIZE(intercon_c));
	snd_soc_dapm_add_routes(dapm, intercon_d,
				ARRAY_SIZE(intercon_d));
#endif

	return 0;
}

#ifdef CONFIG_ACPI
static int acpi_get_mic_pos(struct device *dev, int *mic_pos)
{
	acpi_handle handle = ACPI_HANDLE(dev);
	struct acpi_device *adev;
	union acpi_object *obj, *item;
	u8 uuid[] = {
	   0x45, 0xDE, 0xC2, 0xEF, 0xDE, 0xA5, 0x4C, 0x3A,
	   0x87, 0xB0, 0x47, 0xFC, 0xB4, 0x04, 0x73, 0x3E
	};
	int i = 0;

	if (!handle || acpi_bus_get_device(handle, &adev)) {
		return -ENODEV;
	}

	obj = acpi_evaluate_dsm_typed(handle, uuid, 0, 0, NULL,
			ACPI_TYPE_PACKAGE);
	if (!obj) {
		dev_err(dev, "deivce DSM execution failed.\n");
		return -ENODEV;
	}

	for (i = 0; i < obj->package.count; i += 2) {
		item = &(obj->package.elements[i]);
		if (item->type == ACPI_TYPE_STRING &&
			!strcmp(item->string.pointer, "Mic_Position")) {
			union acpi_object *str_item;

			str_item = &(obj->package.elements[i + 1]);
			kstrtoint(str_item->string.pointer, 0, mic_pos);
			dev_err(dev, "ACPI string = %s, mic_pos = %d \n",
				item->string.pointer, *mic_pos);
			break;
		}
	}

	ACPI_FREE(obj);

	return 0;
}
#endif

static void tlv320aic3101_sw_reset(struct snd_soc_codec *codec)
{
	int i;

	/* # Software Reset */
	for (i = 0; i < NUM_ADC3101; i++)
		snd_soc_write(codec, ADC_RESET(i), 0x1);

	mdelay(20);
}

static int aic31xx_dai_set_tdm_slot(struct snd_soc_dai *dai,
				    unsigned int tx_mask,
				    unsigned int rx_mask,
				    int slots, int slot_width)

{
	struct snd_soc_codec *codec = dai->codec;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "tx_mask=%u rx_mask=%u slots=%d slot_width=%d\n",
		tx_mask, rx_mask, slots, slot_width);

	aic31xx->tdm_rx_mask = rx_mask;
	aic31xx->tdm_slot_width = slot_width;

	return 0;
}
static int aic31xx_dai_apply_tdm_slot(struct snd_soc_codec *codec)
{
	int i, mic_pos = 0, swap_channels = 0;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
#ifdef CONFIG_ACPI
	struct i2c_client *i2cdev = aic31xx->adc_control_data[0];

	int ret = acpi_get_mic_pos(&i2cdev->dev, &mic_pos);
	if (ret) {
		dev_err(codec->dev, "Can't get mic position, so use the default position.\n", ret);
		mic_pos = 0;
	}
#endif

	if (mic_pos == 0) {
		if (aic31xx->tdm_rx_mask == 0x40) {
			dev_info(codec->dev, "linein enabled slot_width=%d\n",
				aic31xx->tdm_slot_width);
			swap_channels = 0x1 << ADC_TDM_CHANNEL_SWAP_SHIFT;
		}

		for (i = 0; i < NUM_ADC3101; i++) {
			u8 enable_channels = ~(aic31xx->tdm_rx_mask >> (i * 2)) & ADC_TDM_MASK;
			snd_soc_write(codec, ADC_I2S_TDM_CTRL(i),
				(enable_channels << ADC_TDM_CHANNEL_EN_SHIFT) | EARLY_3STATE_ENABLED | swap_channels);
			snd_soc_write(codec, ADC_CH_OFFSET_1(i),
				(2 * i * aic31xx->tdm_slot_width) | aic31xx->dsp_a_val);
		}
	} else if (mic_pos == 1) {
		u8 mic_pos_offset[] = {
			0x00, 0x90, 0x30, 0x30,
			0x48, 0x00, 0x18, 0x60
		};
		for (i = 0; i < NUM_ADC3101; i++) {
			u8 enable_channels = ~(aic31xx->tdm_rx_mask >> (i * 2)) & 0x3;
			/* ADC_A: left=MIC_8, right=MIC_1
			 * Set it to be left=MIC_1, right=MIC_8, and then swap left and right channels.
			 * ADC_B: left=MIC_6, right=MIC_3
			 * Set it to be left=MIC_3, right=MIC_6, and then swap left and right channels.
			 * ADC_C: left=MIC_5, right=MIC_4
			 * Set it to be left=MIC_4, right=MIC_5, and then swap left and right channels.
			 * ADC_D: left=MIC_7, right=MIC_2
			 * Set it to be left=MIC_2, right=MIC_7, and then swap left and right channels.
			 */
			snd_soc_write(codec, ADC_CH_OFFSET_1(i), mic_pos_offset[2*i]);
			snd_soc_write(codec, ADC_CH_OFFSET_2(i), mic_pos_offset[2*i + 1]);
			snd_soc_update_bits(codec, ADC_I2S_TDM_CTRL(i),
					0x3 << ADC_TDM_CHANNEL_EN_SHIFT,
					enable_channels << ADC_TDM_CHANNEL_EN_SHIFT);
			snd_soc_update_bits(codec, ADC_I2S_TDM_CTRL(i),
					0x1 << ADC_TDM_CHANNEL_SWAP_SHIFT,
					0x1 << ADC_TDM_CHANNEL_SWAP_SHIFT);
			snd_soc_update_bits(codec, ADC_I2S_TDM_CTRL(i),
					0x1 << ADC_TDM_EN_SHIFT,
					0x1 << ADC_TDM_EN_SHIFT);
		}
	}

	return 0;
}

static void tlv320aic3101_save_settings(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < NUM_ADC3101; i++) {
		/* Save left and right ADC PGA input selection */
		pga1_input_sel_l[i] = snd_soc_read(codec, ADC_LEFT_PGA_SEL_1(i));
		pga1_input_sel_r[i] = snd_soc_read(codec, ADC_RIGHT_PGA_SEL_1(i));
		pga2_input_sel_l[i] = snd_soc_read(codec, ADC_LEFT_PGA_SEL_2(i));
		pga2_input_sel_r[i] = snd_soc_read(codec, ADC_RIGHT_PGA_SEL_2(i));

		/* Save PGA gain */
		mic_pga_l[i] = snd_soc_read(codec, ADC_LEFT_APGA_CTRL(i));
		mic_pga_r[i] = snd_soc_read(codec, ADC_RIGHT_APGA_CTRL(i));
	}
}

static void tlv320aic3101_adc_cfg(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < NUM_ADC3101; i++) {
		/* Setup left and right ADC PGA input selection */
		snd_soc_write(codec, ADC_LEFT_PGA_SEL_1(i), pga1_input_sel_l[i]);
		snd_soc_write(codec, ADC_RIGHT_PGA_SEL_1(i), pga1_input_sel_r[i]);
		snd_soc_write(codec, ADC_LEFT_PGA_SEL_2(i), pga2_input_sel_l[i]);
		snd_soc_write(codec, ADC_RIGHT_PGA_SEL_2(i), pga2_input_sel_r[i]);

		/* Setup PGA gain */
		snd_soc_write(codec, ADC_LEFT_APGA_CTRL(i), mic_pga_l[i]);
		snd_soc_write(codec, ADC_RIGHT_APGA_CTRL(i), mic_pga_r[i]);

		/* Unmute ADC channels and set fine gain */
		snd_soc_write(codec, ADC_ADC_FGA(i), 0x00);
	}
}

/*
 * This function is to set the hardware parameters for aic31xx.  The
 * functions set the sample rate and audio serial data word length.
 */
static int aic31xx_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 data = 0;
	int i;

	dev_dbg(codec->dev, "%s: ##- Data length: %x\n", __func__,
		params_format(params));

	/* DOUT disable: bus keeper disabled */
	for (i = 0; i < NUM_ADC3101; i++)
		snd_soc_write(codec, ADC_DOUT_CTRL(i), 0x10);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		data |= (AIC31XX_WORD_LEN_20BITS <<
				AIC31XX_IFACE1_DATALEN_SHIFT);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
		data |= (AIC31XX_WORD_LEN_24BITS <<
				AIC31XX_IFACE1_DATALEN_SHIFT);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		data |= (AIC31XX_WORD_LEN_32BITS <<
				AIC31XX_IFACE1_DATALEN_SHIFT);
		break;
	}
	for (i = 0; i < NUM_ADC3101; i++)
		snd_soc_update_bits(codec, ADC_INTERFACE_CTRL_1(i),
				    AIC31XX_IFACE1_DATALEN_MASK, data);

	/* Apply gains and unmute */
	tlv320aic3101_adc_cfg(codec);

	/* Enable NADC of master */
	snd_soc_update_bits(codec, ADC_ADC_NADC(0), ADC_ENABLE_NADC,
		ADC_ENABLE_NADC);

	/* mandatory delay to allow ADC clocks to synchronize */
	mdelay(100);

	/* Set mic position on TDM slots */
	aic31xx_dai_apply_tdm_slot(codec);

	/* # Turn Douts back on */
	/* DOUT: Primary Output Enable, bus keeper disabled */
	for (i = 0; i < NUM_ADC3101; i++)
		snd_soc_write(codec, ADC_DOUT_CTRL(i), 0x12);

	return 0;
}

/*
 * aic31xx_dai_set_fmt
 *
 * This function is to set the DAI format.
 * Note that if the first ADC is used as master, and the others as slaves,
 * please call set_pll() before set_fmt().
 */
static int aic31xx_dai_set_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
	const struct adc31xx_rate_divs *divs = NULL;
	u8 iface_reg1 = 0;
	u8 iface_reg3 = AIC31XX_ADC2BCLK; /* Default value */
	u8 dsp_a_val = 0;
	int i;

	dev_dbg(codec->dev, "%s:+ %x\n", __func__, fmt);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface_reg1 = AIC31XX_BCLK_MASTER | AIC31XX_WCLK_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		iface_reg1 &= ~(AIC31XX_BCLK_MASTER | AIC31XX_WCLK_MASTER);
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		iface_reg1 = AIC31XX_BCLK_MASTER;
		iface_reg1 &= ~(AIC31XX_WCLK_MASTER);
		break;
	default:
		dev_alert(codec->dev, "Invalid DAI master/slave interface\n");
		return -EINVAL;
	}
	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_DSP_A:
		dsp_a_val = 0x1;
	case SND_SOC_DAIFMT_DSP_B:
		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			break;
		case SND_SOC_DAIFMT_IB_NF:
			iface_reg3 |= AIC31XX_BCLKINV_MASK;
			break;
		default:
			return -EINVAL;
		}
		iface_reg1 |= (AIC31XX_DSP_MODE <<
				AIC31XX_IFACE1_DATATYPE_SHIFT);
		iface_reg1 |= (AIC31XX_3STATES <<
				AIC31XX_IFACE1_3STATES_SHIFT);
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface_reg1 |= (AIC31XX_RIGHT_JUSTIFIED_MODE <<
				AIC31XX_IFACE1_DATATYPE_SHIFT);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface_reg1 |= (AIC31XX_LEFT_JUSTIFIED_MODE <<
				AIC31XX_IFACE1_DATATYPE_SHIFT);
		break;
	default:
		dev_alert(codec->dev, "Invalid DAI interface format\n");
		return -EINVAL;
	}

	/* Store 1 bclk offset for Ch_OffSet_1 */
	aic31xx->dsp_a_val = dsp_a_val;

	if (iface_reg1 & AIC31XX_BCLK_MASTER) {
		int bclk_out = 0;
		/* Calcualte the BCLK out of the first codec */
		for (i = 0; i < ARRAY_SIZE(aic31xx_divs_pll); i++) {
			if ((aic31xx_divs_pll[i].rate == aic31xx->freq_out)
			    && (aic31xx_divs_pll[i].mclk == aic31xx->freq_in)) {
				divs = &aic31xx_divs_pll[i];
				bclk_out = (aic31xx->freq_in * divs->pll_r *
					    divs->pll_j + aic31xx->freq_in/10000 *
					    divs->pll_r * divs->pll_d) /
					(divs->pll_p * divs->nadc * divs->bdiv_n);
				divs = NULL;
				break;
			}
		}
		dev_info(codec->dev, "%s: InFreq %d bclk_out %d\n", __func__, aic31xx->freq_in, bclk_out);

		/* Find the settings of the slave codecs*/
		for (i = 0; i < ARRAY_SIZE(aic31xx_divs_bclk); i++) {
			if ((aic31xx_divs_bclk[i].rate == aic31xx->freq_out)
				&& (aic31xx_divs_bclk[i].mclk == bclk_out)) {
					divs = &aic31xx_divs_bclk[i];
					break;
			}
		}

		if (i == ARRAY_SIZE(aic31xx_divs_bclk)) {
			dev_err(codec->dev, "No matching bclk\n");
			return -EINVAL;
		}
	}

	for (i = 0; i < NUM_ADC3101; i++) {
		/* Inverted BCLK flag */
		snd_soc_write(codec, ADC_INTERFACE_CTRL_2(i), iface_reg3);
		if (i == 0) {
			/* Set Format and Master mode */
			snd_soc_write(codec, ADC_INTERFACE_CTRL_1(0),
				iface_reg1);

			if (iface_reg1 & AIC31XX_BCLK_MASTER) {
				/* Enable BCLK and WCLK out when the codec is powered down */
				snd_soc_write(codec, ADC_INTERFACE_CTRL_2(0),
					iface_reg3|AIC31XX_BCLK_WCLK_ACTIVE);
				/* If the first ADC is BCLK master
				 * power BCLK N divider up. Already programmed in set_pll
				 */
				snd_soc_update_bits(codec, ADC_BCLK_N_DIV(0),
					AIC31XX_BCLK_N_POWER_MASK,
					AIC31XX_BCLK_N_POWER_MASK);
			}
		} else {
			/* Only 1st codec can be master */
			snd_soc_write(codec, ADC_INTERFACE_CTRL_1(i),
				iface_reg1 & (~(AIC31XX_BCLK_MASTER | AIC31XX_WCLK_MASTER)));
			if (divs) {
				/* CODEC_CLKIN = BCLK */
				snd_soc_write(codec, ADC_CLKGEN_MUX(i),
					AIC31XX_CODEC_CLKIN_BCLK);
				/*  BCLK Divider */
				snd_soc_write(codec, ADC_BCLK_N_DIV(i), divs->bdiv_n);
				/* P and R divider value */
				snd_soc_write(codec, ADC_PLL_PROG_PR(i),
						(divs->pll_p & 0x7) << 4 | divs->pll_r);
				/* AOSR */
				snd_soc_write(codec, ADC_ADC_AOSR(i), divs->aosr & 0xff);
				/* NADC divider value */
				snd_soc_write(codec, ADC_ADC_NADC(i),
						0x80 | (divs->nadc & 0x7f));
				/* MADC divider value */
				snd_soc_write(codec, ADC_ADC_MADC(i),
						0x80 | (divs->madc & 0x7f));
			}
		}

		/* Power-up Left ADC and Right ADC */
		snd_soc_write(codec, ADC_ADC_DIGITAL(i), 0XC2);
	}

	dev_dbg(codec->dev, "%s:- \n", __func__);

	return 0;
}


/*
* aic31xx_dai_set_pll
*
* This function is invoked as part of the PLL call-back
* handler from the ALSA layer.
*/
static int aic31xx_dai_set_pll(struct snd_soc_dai *dai,
		int pll_id, int source, unsigned int freq_in,
		unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
	const struct adc31xx_rate_divs *divs;
	size_t divs_len;
	u8 clk_mux, pll_enab = 0;
	int i;

	switch (source) {
	case AIC3101_PLL_ADC_FS_CLKIN_PLL_MCLK:
		clk_mux = (AIC31XX_PLL_CLKIN_MCLK << AIC31XX_PLL_CLKIN_SHIFT) |
			   (AIC31XX_CODEC_CLKIN_PLL << AIC31XX_CODEC_CLKIN_SHIFT);
		divs = aic31xx_divs_pll;
		divs_len = ARRAY_SIZE(aic31xx_divs_pll);
		pll_enab = ADC_PLL_POWER_UP;
		break;
	case AIC3101_PLL_ADC_FS_CLKIN_PLL_BCLK:
		clk_mux = (AIC31XX_PLL_CLKIN_BCLK << AIC31XX_PLL_CLKIN_SHIFT) |
			   (AIC31XX_CODEC_CLKIN_PLL << AIC31XX_CODEC_CLKIN_SHIFT);
		divs = aic31xx_divs_pll;
		divs_len = ARRAY_SIZE(aic31xx_divs_pll);
		pll_enab = ADC_PLL_POWER_UP;
		break;
	case AIC3101_PLL_ADC_FS_CLKIN_MCLK:
		clk_mux = (AIC31XX_PLL_CLKIN_MCLK << AIC31XX_PLL_CLKIN_SHIFT) |
			   (AIC31XX_CODEC_CLKIN_MCLK << AIC31XX_CODEC_CLKIN_SHIFT);
		divs = aic31xx_divs_pll;
		divs_len = ARRAY_SIZE(aic31xx_divs_pll);
		break;
	case AIC3101_PLL_ADC_FS_CLKIN_BCLK:
		clk_mux = (AIC31XX_PLL_CLKIN_BCLK << AIC31XX_PLL_CLKIN_SHIFT) |
			   (AIC31XX_CODEC_CLKIN_BCLK << AIC31XX_CODEC_CLKIN_SHIFT);
		divs = aic31xx_divs_bclk;
		divs_len = ARRAY_SIZE(aic31xx_divs_bclk);
		break;
	default:
		dev_err(codec->dev, "source %d not supported\n", source);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "In aic31xx: dai_set_pll mux=0x%x out=%d in=%d\n", clk_mux, freq_in, freq_out);

	for (i = 0; i < divs_len; i++) {
		if ((divs[i].rate == freq_out)
			&& (divs[i].mclk == freq_in)) {
				break;
		}
	}

	if (i == divs_len) {
		dev_err(codec->dev, "sampling rate not supported\n");
		return -EINVAL;
	}

	aic31xx->freq_in = freq_in;
	aic31xx->freq_out = freq_out;
	divs = &divs[i];

	/* Backup gains and mode settings before reset */
	tlv320aic3101_save_settings(codec);

	/* Reset ADCs */
	tlv320aic3101_sw_reset(codec);

	/*turn off pll and m/n dividers on adc3101*/
	for (i = 0; i < NUM_ADC3101; i++) {
		snd_soc_write(codec, ADC_PLL_PROG_PR(i), 0x00);
		snd_soc_write(codec, ADC_ADC_NADC(i), 0x00);
		snd_soc_write(codec, ADC_ADC_MADC(i), 0x00);
		snd_soc_write(codec, ADC_BCLK_N_DIV(i), 0X00);
	}

	snd_soc_write(codec, ADC_CLKGEN_MUX(0), clk_mux);

	snd_soc_write(codec, ADC_ADC_AOSR(0), divs->aosr & 0xff);

	snd_soc_write(codec, ADC_PLL_PROG_PR(0),
		pll_enab | (divs->pll_p & 0x7) << ADC_PLLP_SHIFT | divs->pll_r);
	snd_soc_write(codec, ADC_PLL_PROG_J(0), divs->pll_j & ADC_PLLJ_MASK);
	snd_soc_write(codec, ADC_PLL_PROG_D_MSB(0),
		(divs->pll_d >> 8) & ADC_PLLD_MSB_MASK);
	snd_soc_write(codec, ADC_PLL_PROG_D_LSB(0),
		divs->pll_d & ADC_PLLD_LSB_MASK);

	/* NADC divider value. Do not turn on */
	snd_soc_write(codec, ADC_ADC_NADC(0), (divs->nadc & ADC_NADC_MASK));

	/* MADC divider value */
	snd_soc_write(codec, ADC_ADC_MADC(0),
			0x80 | (divs->madc & ADC_NADC_MASK));

	mdelay(15);

	if (pll_id == AIC3101_PLL_BCLK) {
		/* Add BDIV
		 * Only the first ADC could be master, so only set the bdiv_n for the first ADC.
		 */
		snd_soc_write(codec, ADC_BCLK_N_DIV(0), divs->bdiv_n);
	} else if (pll_id == AIC3101_PLL_CLKOUT) {
		/* Add M_Val */
	}

	/* Program DSP */
	aic31xx_apply_tuning(codec);

	dev_info(codec->dev, "%s: DAI ID %d PLL_ID %d InFreq %d OutFreq %d\n",
		__func__, dai->id, pll_id, freq_in, freq_out);
	return 0;
}


/*
 * aic31xx_set_sysclk - This function is called from machine driver to switch
 *			clock source based on DAPM Event
 */
static int aic31xx_set_sysclk(struct snd_soc_codec *codec,
		int clk_id, int source, unsigned int freq, int dir)
{
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, " %s: %d \n", __func__, freq);

	switch (freq) {
	case AIC31XX_FREQ_4800000:
	case AIC31XX_FREQ_9600000:
	case AIC31XX_FREQ_12000000:
	case AIC31XX_FREQ_19200000:
	case AIC31XX_FREQ_24000000:
	case AIC31XX_FREQ_24576000:
	case AIC31XX_FREQ_26000000:
	case AIC31XX_FREQ_38400000:
		aic31xx->clkid = clk_id;
		aic31xx->clksrc = source;
		aic31xx->sysclk = freq;
		return 0;
	default:
		dev_err(codec->dev, "Invalid frequency to set DAI system clock\n");
		return -EINVAL;
	}
}

/*
 * aic31xx_set_bias_level - This function is to get triggered when dapm
 * events occurs.
 */
static int aic31xx_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	int ret = 0;
	int i;

	dev_dbg(codec->dev, "%s: %d\n", __func__, level);

	switch (level) {
	case SND_SOC_BIAS_ON:
		for (i = 0; i < NUM_ADC3101; i++)
			snd_soc_write(codec, ADC_MICBIAS_CTRL(i),
				AIC3X_MICBIAS_2_5V << 5 | AIC3X_MICBIAS_2_5V << 3);
		break;

	case SND_SOC_BIAS_OFF:
		for (i = 0; i < NUM_ADC3101; i++)
			snd_soc_write(codec, ADC_MICBIAS_CTRL(i), 0);
		break;

	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_STANDBY:
		break;
	default:
		dev_err(codec->dev, "%s: Invalid bias level=%d\n", __func__, level);
		ret =  -EINVAL;
	}

	codec->dapm.bias_level = level;

	return ret;
}


static int aic31xx_suspend(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s: Entered\n", __func__);

	aic31xx_set_bias_level(codec, SND_SOC_BIAS_OFF);
	dev_dbg(codec->dev, "%s: Exiting\n", __func__);
	return 0;
}

static int aic31xx_resume(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s: Entered\n", __func__);

	dev_dbg(codec->dev, "%s: Exiting\n", __func__);

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : aic31xx_init
 * Purpose  : This function is to initialise the AIC31xx driver
 *            register the mixer and dsp interfaces with the kernel.
 *
 *----------------------------------------------------------------------------
 */
static int tlv320aic3101_init(struct snd_soc_codec *codec)
{
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
	int ret;
	int i;

	/* Initilaize with default values */
	for (i = 0; i < NUM_ADC3101; i++) {
		pga1_input_sel_l[i] = DIFF_MIC_INPUT_0db;
		pga1_input_sel_r[i] = DIFF_MIC_INPUT_0db;
		pga2_input_sel_l[i] = DEFAULT_PGA_INPUT_SEL;
		pga2_input_sel_r[i] = DEFAULT_PGA_INPUT_SEL;
		mic_pga_l[i] = MIC_PGA_GAIN_L;
		mic_pga_r[i] = MIC_PGA_GAIN_R;
	}

	tlv320aic3101_sw_reset(codec);

	aic31xx_add_widgets(codec);

	/* Switch on master clock */
	ret = clk_prepare_enable(aic31xx->mclk);
	if (ret) {
		dev_err(codec->dev, "Failed to enable master clock\n");
		return ret;
	}

	aic31xx_set_bias_level(codec, SND_SOC_BIAS_OFF);
	codec->dapm.idle_bias_off = true;

	return ret;
}

static void aic31xx_shutdown(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
}

/*
 * Codec probe function, called upon codec registration
 *
 */

static int aic31xx_codec_probe(struct snd_soc_codec *codec)
{
	int ret = 0;
	struct aic31xx_priv *aic31xx;

	aic31xx = snd_soc_codec_get_drvdata(codec);

	codec->control_data = aic31xx->adc_control_data[0];

	tlv320aic3101_init(codec);

	return ret;
}

/*
 * Remove aic31xx soc device
 */
static int aic31xx_codec_remove(struct snd_soc_codec *codec)
{
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s\n", __func__);

	/* Switch off master clock */
	clk_disable_unprepare(aic31xx->mclk);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_driver_aic31xx = {
	.probe			= aic31xx_codec_probe,
	.remove			= aic31xx_codec_remove,
	.suspend		= aic31xx_suspend,
	.resume			= aic31xx_resume,
	.set_bias_level		= aic31xx_set_bias_level,
	.controls		= aic31xx_snd_controls,
	.num_controls		= ARRAY_SIZE(aic31xx_snd_controls),
	.read			= aic31xx_read_reg_cache,
	.write			= aic31xx_write,
	.reg_cache_size		= ARRAY_SIZE(aic31xx_reg),
	.reg_word_size		= sizeof(u8),
	.reg_cache_default	= aic31xx_reg,
	.set_sysclk		= aic31xx_set_sysclk,
};

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_dai |
 *		It is SoC Codec DAI structure which has DAI capabilities viz.,
 *		playback and capture, DAI runtime information viz. state of DAI
 *		and pop wait state, and DAI private data.
 *		The AIC31xx rates ranges from 8k to 192k
 *		The PCM bit format supported are 16, 20, 24 and 32 bits
 *----------------------------------------------------------------------------
 */

/*
 * DAI ops
 */

static struct snd_soc_dai_ops aic31xx_dai_ops = {
	.hw_params	= aic31xx_hw_params,
	.set_pll	= aic31xx_dai_set_pll,
	.set_fmt	= aic31xx_dai_set_fmt,
	.set_tdm_slot	= aic31xx_dai_set_tdm_slot,
	.shutdown	= aic31xx_shutdown,
};

static struct snd_soc_dai_driver tlv320aic3101_dai_driver[] = {
{
	.name = "tlv320aic3101-codec",
	.capture = {
		.stream_name	= "Capture",
		.channels_min	= 1,
#if defined CONFIG_SND_SOC_8_MICS
		.channels_max	= 9,
#elif defined CONFIG_SND_I2S_MCLK
		.channels_max	= 6,
#else
		.channels_max	= 4,
#endif
		.rates		= AIC31XX_RATES,
		.formats	= AIC31XX_FORMATS,
	},
	.ops = &aic31xx_dai_ops,
}
};

static int aic31xx_i2c_probe(struct i2c_client *pdev,
			     const struct i2c_device_id *id)
{
	int ret = 0;
	struct aic31xx_priv *aic31xx;
	int enable_gpio;

	static struct aic31xx_priv *aic31xx_global;
	static int i;

	dev_info(&pdev->dev, "TLV320AIC3101 Audio Codec %s (%d)\n", AIC31XX_VERSION, i);

	if (i == 0) {
		aic31xx = kzalloc(sizeof(struct aic31xx_priv), GFP_KERNEL);
		if (aic31xx == NULL) {
			return -ENOMEM;
		}
		i2c_set_clientdata(pdev, aic31xx);
		aic31xx_global = aic31xx;
		aic31xx->adc_control_data[i] = pdev;

		mutex_init(&aic31xx->codecMutex);

		ret = snd_soc_register_codec(&pdev->dev,
					     &soc_codec_driver_aic31xx,
					     tlv320aic3101_dai_driver,
					     ARRAY_SIZE
					     (tlv320aic3101_dai_driver));
		if (ret)
			dev_err(&pdev->dev,
				"codec: %s : snd_soc_register_codec failed\n",
				__func__);

		/* Setup default clk config - TODO do in machine driver */
		aic31xx->clkid = AIC3101_PLL_ADC_FS;
		aic31xx->clksrc = AIC3101_PLL_ADC_FS_CLKIN_PLL_MCLK;
		aic31xx->sysclk = AIC31XX_FREQ_9600000;

		aic31xx->mclk = devm_clk_get(&pdev->dev, "mclk");
		if (IS_ERR(aic31xx->mclk)) {
			dev_err(&pdev->dev, "%s: Failed getting mclk\n", __func__);
			return PTR_ERR(aic31xx->mclk);
		}

#ifdef CONFIG_ACPI
		if (!ACPI_HANDLE(&pdev->dev)) {
			dev_err(&pdev_dev, "Not a valid ACPI device\n");
			return -EINVAL;
		}
		aic31xx->enable_gpiod =
			devm_gpiod_get_index(&pdev->dev, "aic3101_enable",
					     0);
		if (IS_ERR_OR_NULL(aic31xx->enable_gpiod)) {
			dev_err(&pdev->dev, "Failed to get enable gpio!\n");
			return -EINVAL;
		}

#else
		enable_gpio = of_get_named_gpio(pdev->dev.of_node, "enable-gpio", 0);
		if (enable_gpio < 0) {
			dev_err(&pdev->dev, "Failed to get enable gpio from device tree!\n");
			return -EINVAL;
		}

		ret = devm_gpio_request_one(&pdev->dev, enable_gpio, 0, "aic3101_enable");
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to request enable gpio! %d\n", ret);
			return -EINVAL;
		}

		aic31xx->enable_gpiod = gpio_to_desc(enable_gpio);
#endif
		/* Reset ADC */
		ret = gpiod_direction_output(aic31xx->enable_gpiod, 0);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"could not set gpio(%d) to 0 (err=%d)\n",
				enable_gpio, ret);
			return -EINVAL;
		}
		/* Hold it down for required time */
		mdelay(RESET_LINE_DELAY);
		ret = gpiod_direction_output(aic31xx->enable_gpiod, 1);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"could not set gpio(%d) to 1 (err=%d)\n",
				enable_gpio, ret);
			return -EINVAL;
		}
	} else {
		aic31xx_global->adc_control_data[i] = pdev;
	}

	i++;

	dev_info(&pdev->dev, "%s: complete (%d)\n", __func__, i);

	return ret;
}

static int aic31xx_i2c_remove(struct i2c_client *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

#ifdef CONFIG_ACPI
static const struct acpi_device_id aic31xx_acpi_match[] = {
	{ "LVADC320", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, aic31xx_acpi_match);
#endif

static const struct i2c_device_id aic31xx_i2c_id[] = {
	{ "tlv320adc3101", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aic31xx_i2c_id);

static const struct of_device_id aic31xx_of_id[] = {
	{ .compatible = "ti,tlv320aic3101", },
	{ }
};
MODULE_DEVICE_TABLE(of, aic31xx_of_id);

static struct i2c_driver aic31xx_i2c_driver = {
	.driver = {
		.name	= "tlv320aic3101",
		.owner	= THIS_MODULE,
		.of_match_table = aic31xx_of_id,
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(aic31xx_acpi_match),
#endif
	},
	.probe		= aic31xx_i2c_probe,
	.remove		= aic31xx_i2c_remove,
	.id_table	= aic31xx_i2c_id,
};
module_i2c_driver(aic31xx_i2c_driver);

MODULE_DESCRIPTION("ASoC TLV320AIC3101 codec driver");
MODULE_LICENSE("GPL");
