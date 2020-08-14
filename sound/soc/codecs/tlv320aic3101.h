/*
 * linux/sound/soc/codecs/tlv320aic3101.h
 *
 *
 * Copyright (C) 2015 Amazon, Inc
 *
 *
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *
 */

#ifndef _Tlv320aic3101_H
#define _Tlv320aic3101_H

#define AUDIO_NAME "aic31xx_adc31xx"
#define AIC31XX_ADC31xx_VERSION "1.2"
#define AIC31XX_VERSION "1.2"

enum {
	AIC3101_PLL_ADC_FS,
	AIC3101_PLL_BCLK,
	AIC3101_PLL_CLKOUT,
};

enum {
	AIC3101_PLL_ADC_FS_CLKIN_MCLK,
	AIC3101_PLL_ADC_FS_CLKIN_BCLK,
	AIC3101_PLL_ADC_FS_CLKIN_PLL_MCLK,
	AIC3101_PLL_ADC_FS_CLKIN_PLL_BCLK,
};

enum aic3x_micbias_voltage {
	AIC3X_MICBIAS_OFF = 0,
	AIC3X_MICBIAS_2_0V = 1,
	AIC3X_MICBIAS_2_5V = 2,
	AIC3X_MICBIAS_AVDDV = 3,
};

/* Enable slave / master mode for codec */
#define AIC31XX_MCBSP_SLAVE

/* 8 bit mask value */
#define ADC3xxx_8BITS_MASK           0xFF

/*#define CONFIG_MINI_DSP*/
#undef CONFIG_MINI_DSP

/* Enable headset detection */
/*#define HEADSET_DETECTION*/
#undef HEADSET_DETECTION

/* Macro enables or disables  AIC3xxx TiLoad Support */
/*#define AIC31XX_TiLoad*/
#undef AIC3xxx_TiLoad

/* Enable register caching on write */
#define EN_REG_CACHE

/* AIC31xx supported sample rate are 8k to 192k */
#define AIC31XX_RATES   SNDRV_PCM_RATE_8000_192000

/* AIC31xx supports the word formats 16bits, 20bits, 24bits and 32 bits */
#define AIC31XX_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	 SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

#define AIC31XX_FREQ_4800000   4800000
#define AIC31XX_FREQ_9600000   9600000
#define AIC31XX_FREQ_12000000 12000000
#define AIC31XX_FREQ_24000000 24000000
#define AIC31XX_FREQ_19200000 19200000
#define AIC31XX_FREQ_26000000 26000000
#define AIC31XX_FREQ_38400000 38400000
#define AIC31XX_FREQ_24576000 24576000

/* Audio data word length = 16-bits (default setting) */
#define AIC31XX_WORD_LEN_16BITS		0x00
#define AIC31XX_WORD_LEN_20BITS		0x01
#define AIC31XX_WORD_LEN_24BITS		0x02
#define AIC31XX_WORD_LEN_32BITS		0x03



/* sink: name of target widget */
#define AIC31XX_WIDGET_NAME             0
/* control: mixer control name */
#define AIC31XX_CONTROL_NAME        1
/* source: name of source name */
#define AIC31XX_SOURCE_NAME     2

/* D15..D8 aic31xx register offset */
#define AIC31XX_REG_OFFSET_INDEX		0
/* D7...D0 register data */
#define AIC31XX_REG_DATA_INDEX		1


/* 8 bit mask value */
#define AIC31XX_8BITS_MASK		0xFF

/* shift value for CLK_REG_3 register */
#define CLK_REG_3_SHIFT			6
/* shift value for DAC_OSR_MSB register */
#define DAC_OSR_MSB_SHIFT		4

/* Masks used for updating register bits */
#define AIC31XX_IFACE1_DATALEN_MASK	0x30
#define AIC31XX_IFACE1_DATALEN_SHIFT	(4)
#define AIC31XX_IFACE1_DATATYPE_MASK	0xC0
#define AIC31XX_IFACE1_DATATYPE_SHIFT	(6)
/* Serial data bus uses I2S mode (Default mode) */
#define AIC31XX_I2S_MODE		0x00
#define AIC31XX_DSP_MODE		0x01
#define AIC31XX_RIGHT_JUSTIFIED_MODE	0x02
#define AIC31XX_LEFT_JUSTIFIED_MODE	0x03

#define AIC31XX_IFACE1_MASTER_MASK	0x0C
#define AIC31XX_IFACE1_MASTER_SHIFT	(2)
#define AIC31XX_BCLK_MASTER	0x08
#define AIC31XX_WCLK_MASTER	0x04

#define AIC31XX_IFACE1_3STATES_MASK      0x01
#define AIC31XX_IFACE1_3STATES_SHIFT     (0)
#define AIC31XX_3STATES		0x01

#define AIC31XX_DATA_OFFSET_MASK	0xFE
#define AIC31XX_BCLKINV_MASK		0x08
#define AIC31XX_BCLKEN_MASK		0x04
#define AIC31XX_BDIVCLK_MASK		0x03
#define AIC31XX_BCLK_WCLK_ACTIVE	0x04

#define AIC31XX_DAC2BCLK		0x00
#define AIC31XX_DACMOD2BCLK		0x01
#define AIC31XX_ADC2BCLK		0x02
#define AIC31XX_ADCMOD2BCLK		0x03

#define AIC31XX_BCLK_N_POWER_MASK       0x80
#define AIC31XX_CLKOUT_N_MASK		0x7f
#define AIC31XX_ADC_PRB_MASK		0x7f

/* number of codec specific register for configuration */
#define NO_FEATURE_REGS             2

#ifdef CONFIG_SND_SOC_8_MICS
#define NUM_ADC3101             4
#elif defined CONFIG_SND_SOC_4_MICS
#define NUM_ADC3101             2
#else
#define NUM_ADC3101             1
#endif

#define ADC3101_CACHEREGNUM     (128 + 128)
#define AIC31XX_CACHEREGNUM     (ADC3101_CACHEREGNUM * NUM_ADC3101)

/*
 * Creates the register using 8 bits for reg, 3 bits for device
 * and 7 bits for page. The extra 2 high order bits for page are
 * necessary for large pages, which may be sent in with the 32
 * bit reg value to the following functions where dev, page, and
 * reg no are properly masked out:
 * - aic31xx_write
 * - aic31xx_read_reg_cache
 * For ALSA calls (where the register is limited to 16bits), the
 * 5 bits for page is sufficient, and no high order bits will be
 * truncated.
 */
#define MAKE_REG(device, page, reg) \
	((u32) (((page) & 0x7f) << 11 | ((device) & 0x7) << 8 | ((reg) & 0x7f)))

/****************************************************************************/
/*          ADC31xx_a Page 0 Registers                              */
/****************************************************************************/
/*
typedef union reg {
	struct adc31xx_reg {
		u8 reserved;
		u8 device;
		u8 page;
		u8 offset;
	} adc31xx_register;
	unsigned int adc31xx_register_int;
} adc31xx_reg_union;
*/
/* Page select register */
#define ADC_PAGE_SELECT(cnum)         MAKE_REG((cnum), 0, 0)
/* Software reset register */
#define ADC_RESET(cnum)               MAKE_REG((cnum), 0, 1)

/* 2-3 Reserved */

/* PLL programming register B */
#define ADC_CLKGEN_MUX(cnum)          MAKE_REG((cnum), 0, 4)
#define AIC31XX_PLL_CLKIN_MASK				0xC  /* (0b00001100) */
#define AIC31XX_PLL_CLKIN_SHIFT				2
#define AIC31XX_PLL_CLKIN_MCLK				0x0
#define AIC31XX_PLL_CLKIN_BCLK				0x1
#define AIC31XX_CODEC_CLKIN_SHIFT			0
#define AIC31XX_CODEC_CLKIN_MCLK			0x0
#define AIC31XX_CODEC_CLKIN_BCLK			0x1
#define AIC31XX_CODEC_CLKIN_PLL				0x3
/* PLL P and R-Val */
#define ADC_PLL_PROG_PR(cnum)         MAKE_REG((cnum), 0, 5)
/* PLL J-Val */
#define ADC_PLL_PROG_J(cnum)          MAKE_REG((cnum), 0, 6)
/* PLL D-Val MSB */
#define ADC_PLL_PROG_D_MSB(cnum)      MAKE_REG((cnum), 0, 7)
/* PLL D-Val LSB */
#define ADC_PLL_PROG_D_LSB(cnum)      MAKE_REG((cnum), 0, 8)

/* 9-17 Reserved */

/* ADC NADC */
#define ADC_ADC_NADC(cnum)            MAKE_REG((cnum), 0, 18)
/* ADC MADC */
#define ADC_ADC_MADC(cnum)            MAKE_REG((cnum), 0, 19)
/* ADC AOSR */
#define ADC_ADC_AOSR(cnum)            MAKE_REG((cnum), 0, 20)
/* ADC IADC */
#define ADC_ADC_IADC(cnum)            MAKE_REG((cnum), 0, 21)
/* ADC miniDSP engine decimation */
#define ADC_MINIDSP_DECIMATION(cnum)   MAKE_REG((cnum), 0, 22)

/* 23-24 Reserved */

/* CLKOUT MUX */
#define ADC_CLKOUT_MUX(cnum)      MAKE_REG((cnum), 0, 25)
/* CLOCKOUT M divider value */
#define ADC_CLKOUT_M_DIV(cnum)      MAKE_REG((cnum), 0, 26)
/*Audio Interface Setting Register 1*/
#define ADC_INTERFACE_CTRL_1(cnum)     MAKE_REG((cnum), 0, 27)
/* Data Slot Offset (Ch_Offset_1) */
#define ADC_CH_OFFSET_1(cnum)      MAKE_REG((cnum), 0, 28)
/* ADC interface control 2 */
#define ADC_INTERFACE_CTRL_2(cnum)     MAKE_REG((cnum), 0, 29)
/* BCLK N Divider */
#define ADC_BCLK_N_DIV(cnum)      MAKE_REG((cnum), 0, 30)
/* Secondary audio interface control 1 */
#define ADC_INTERFACE_CTRL_3(cnum)     MAKE_REG((cnum), 0, 31)
/* Secondary audio interface control 2 */
#define ADC_INTERFACE_CTRL_4(cnum)     MAKE_REG((cnum), 0, 32)
/* Secondary audio interface control 3 */
#define ADC_INTERFACE_CTRL_5(cnum)     MAKE_REG((cnum), 0, 33)
/* I2S sync */
#define ADC_I2S_SYNC(cnum)      MAKE_REG((cnum), 0, 34)

/* 35 Reserved */

/* ADC flag register */
#define ADC_ADC_FLAG(cnum)      MAKE_REG((cnum), 0, 36)
/* Data slot offset 2 (Ch_Offset_2) */
#define ADC_CH_OFFSET_2(cnum)      MAKE_REG((cnum), 0, 37)
/* I2S TDM control register */
#define ADC_I2S_TDM_CTRL(cnum)      MAKE_REG((cnum), 0, 38)

/* 39-41 Reserved */

/* Interrupt flags (overflow) */
#define ADC_INTR_FLAG_1(cnum)      MAKE_REG((cnum), 0, 42)
/* Interrupt flags (overflow) */
#define ADC_INTR_FLAG_2(cnum)      MAKE_REG((cnum), 0, 43)

/* 44 Reserved */

/* Interrupt flags ADC */
#define ADC_INTR_FLAG_ADC1(cnum)      MAKE_REG((cnum), 0, 45)

/* 46 Reserved */

/* Interrupt flags ADC */
#define ADC_INTR_FLAG_ADC2(cnum)      MAKE_REG((cnum), 0, 47)
/* INT1 interrupt control */
#define ADC_INT1_CTRL(cnum)      MAKE_REG((cnum), 0, 48)
/* INT2 interrupt control */
#define ADC_INT2_CTRL(cnum)      MAKE_REG((cnum), 0, 49)

/* 50 Reserved */

/* DMCLK/GPIO2 control */
#define ADC_GPIO2_CTRL(cnum)      MAKE_REG((cnum), 0, 51)
/* DMDIN/GPIO1 control */
#define ADC_GPIO1_CTRL(cnum)      MAKE_REG((cnum), 0, 52)
/* DOUT Control */
#define ADC_DOUT_CTRL(cnum)      MAKE_REG((cnum), 0, 53)

/* 54-56 Reserved */

/* ADC sync control 1 */
#define ADC_SYNC_CTRL_1(cnum)      MAKE_REG((cnum), 0, 57)
/* ADC sync control 2 */
#define ADC_SYNC_CTRL_2(cnum)      MAKE_REG((cnum), 0, 58)
/* ADC CIC filter gain control */
#define ADC_CIC_GAIN_CTRL(cnum)      MAKE_REG((cnum), 0, 59)

/* 60 Reserved */

/* ADC processing block selection  */
#define ADC_PRB_SELECT(cnum)      MAKE_REG((cnum), 0, 61)
/* Programmable instruction mode control bits */
#define ADC_INST_MODE_CTRL(cnum)      MAKE_REG((cnum), 0, 62)

/* 63-79 Reserved */

/* Digital microphone polarity control */
#define ADC_MIC_POLARITY_CTRL(cnum)    MAKE_REG((cnum), 0, 80)
/* ADC Digital */
#define ADC_ADC_DIGITAL(cnum)          MAKE_REG((cnum), 0, 81)
/* ADC Fine Gain Adjust */
#define ADC_ADC_FGA(cnum)              MAKE_REG((cnum), 0, 82)
/* Left ADC Channel Volume Control */
#define ADC_LADC_VOL(cnum)             MAKE_REG((cnum), 0, 83)
/* Right ADC Channel Volume Control */
#define ADC_RADC_VOL(cnum)             MAKE_REG((cnum), 0, 84)
/* ADC phase compensation */
#define ADC_ADC_PHASE_COMP(cnum)       MAKE_REG((cnum), 0, 85)
/* Left Channel AGC Control Register 1 */
#define ADC_LEFT_CHN_AGC_1(cnum)       MAKE_REG((cnum), 0, 86)
/* Left Channel AGC Control Register 2 */
#define ADC_LEFT_CHN_AGC_2(cnum)       MAKE_REG((cnum), 0, 87)
/* Left Channel AGC Control Register 3 */
#define ADC_LEFT_CHN_AGC_3(cnum)       MAKE_REG((cnum), 0, 88)
/* Left Channel AGC Control Register 4 */
#define ADC_LEFT_CHN_AGC_4(cnum)       MAKE_REG((cnum), 0, 89)
/* Left Channel AGC Control Register 5 */
#define ADC_LEFT_CHN_AGC_5(cnum)       MAKE_REG((cnum), 0, 90)
/* Left Channel AGC Control Register 6 */
#define ADC_LEFT_CHN_AGC_6(cnum)       MAKE_REG((cnum), 0, 91)
/* Left Channel AGC Control Register 7 */
#define ADC_LEFT_CHN_AGC_7(cnum)       MAKE_REG((cnum), 0, 92)
/* Left AGC gain */
#define ADC_LEFT_AGC_GAIN(cnum)        MAKE_REG((cnum), 0, 93)
/* Right Channel AGC Control Register 1 */
#define ADC_RIGHT_CHN_AGC_1(cnum)      MAKE_REG((cnum), 0, 94)
/* Right Channel AGC Control Register 2 */
#define ADC_RIGHT_CHN_AGC_2(cnum)      MAKE_REG((cnum), 0, 95)
/* Right Channel AGC Control Register 3 */
#define ADC_RIGHT_CHN_AGC_3(cnum)      MAKE_REG((cnum), 0, 96)
/* Right Channel AGC Control Register 4 */
#define ADC_RIGHT_CHN_AGC_4(cnum)      MAKE_REG((cnum), 0, 97)
/* Right Channel AGC Control Register 5 */
#define ADC_RIGHT_CHN_AGC_5(cnum)      MAKE_REG((cnum), 0, 98)
/* Right Channel AGC Control Register 6 */
#define ADC_RIGHT_CHN_AGC_6(cnum)      MAKE_REG((cnum), 0, 99)
/* Right Channel AGC Control Register 7 */
#define ADC_RIGHT_CHN_AGC_7(cnum)      MAKE_REG((cnum), 0, 100)
/* Right AGC gain */
#define ADC_RIGHT_AGC_GAIN(cnum)       MAKE_REG((cnum), 0, 101)

/* 102-127 Reserved */

/****************************************************************************/
/*                           Page 1 Registers                               */
/****************************************************************************/
#define ADC_PAGE_1                    128

/* 1-25 Reserved */

/* Dither control */
#define ADC_DITHER_CTRL(cnum)          MAKE_REG((cnum), 1, 26)

/* 27-50 Reserved */

/* MICBIAS Configuration Register */
#define ADC_MICBIAS_CTRL(cnum)         MAKE_REG((cnum), 1, 51)
/* Left ADC input selection for Left PGA */
#define ADC_LEFT_PGA_SEL_1(cnum)       MAKE_REG((cnum), 1, 52)

/* 53 Reserved */

/* Right ADC input selection for Left PGA */
#define ADC_LEFT_PGA_SEL_2(cnum)       MAKE_REG((cnum), 1, 54)
/* Right ADC input selection for right PGA */
#define ADC_RIGHT_PGA_SEL_1(cnum)      MAKE_REG((cnum), 1, 55)

/* 56 Reserved */

/* Right ADC input selection for right PGA */
#define ADC_RIGHT_PGA_SEL_2(cnum)      MAKE_REG((cnum), 1, 57)

/* 58 Reserved */

/* Left analog PGA settings */
#define ADC_LEFT_APGA_CTRL(cnum)       MAKE_REG((cnum), 1, 59)
/* Right analog PGA settings */
#define ADC_RIGHT_APGA_CTRL(cnum)      MAKE_REG((cnum), 1, 60)
/* ADC Low current Modes */
#define ADC_LOW_CURRENT_MODES(cnum)    MAKE_REG((cnum), 1, 61)
/* ADC analog PGA flags */
#define ADC_ANALOG_PGA_FLAGS(cnum)     MAKE_REG((cnum), 1, 62)

/* 63-127 Reserved */

/****************************************************************************/
/*              Macros and definitions                              */
/****************************************************************************/

/* ADC31xx register space */
#define ADC31xx_CACHEREGNUM     192
#define ADC31xx_PAGE_SIZE       128

#define ADC31xx_RATES   SNDRV_PCM_RATE_8000_96000
#define ADC31xx_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

/* bits defined for easy usage */
#define D7                    (0x01 << 7)
#define D6                    (0x01 << 6)
#define D5                    (0x01 << 5)
#define D4                    (0x01 << 4)
#define D3                    (0x01 << 3)
#define D2                    (0x01 << 2)
#define D1                    (0x01 << 1)
#define D0                    (0x01 << 0)

/****************************************************************************/
/*              ADC31xx Register bits                               */
/****************************************************************************/
/* PLL Enable bits */
#define ADC_ENABLE_PLL              D7
#define ADC_ENABLE_NADC             D7
#define ADC_ENABLE_MADC             D7
#define ADC_ENABLE_BCLK             D7

/* Power bits */
#define ADC_LADC_PWR_ON             D7
#define ADC_RADC_PWR_ON             D6

#define ADC_SOFT_RESET              D0
#define ADC_BCLK_MASTER             D3
#define ADC_WCLK_MASTER             D2

/* Interface register masks */
#define ADC_FMT_MASK                (D7|D6|D3|D2)
#define ADC_WLENGTH_MASK            (D5|D4)

/* PLL P/R bit offsets */
#define ADC_PLLP_SHIFT          4
#define ADC_PLLR_SHIFT          0
#define ADC_PLL_PR_MASK         0x7F
#define ADC_PLLJ_MASK           0x3F
#define ADC_PLLD_MSB_MASK       0x3F
#define ADC_PLLD_LSB_MASK       0xFF
#define ADC_NADC_MASK           0x7F
#define ADC_MADC_MASK           0x7F
#define ADC_AOSR_MASK           0xFF
#define ADC_IADC_MASK           0xFF
#define ADC_BDIV_MASK           0x7F

/* PLL_CLKIN bits */
#define ADC_PLL_CLKIN_SHIFT         2
#define ADC_PLL_CLKIN_MCLK          0x0
#define ADC_PLL_CLKIN_BCLK          0x1
#define ADC_PLL_CLKIN_ZERO          0x3

/* CODEC_CLKIN bits */
#define ADC_CODEC_CLKIN_SHIFT       0
#define ADC_CODEC_CLKIN_MCLK        0x0
#define ADC_CODEC_CLKIN_BCLK        0x1
#define ADC_CODEC_CLKIN_PLL_CLK     0x3
#define ADC_PLL_POWER_UP            0x80

#define ADC_USE_PLL     ((ADC_PLL_CLKIN_MCLK << ADC_PLL_CLKIN_SHIFT) | \
	(ADC_CODEC_CLKIN_PLL_CLK << ADC_CODEC_CLKIN_SHIFT))

/*  Analog PGA control bits */
#define ADC_LPGA_MUTE               D7
#define ADC_RPGA_MUTE               D7

#define ADC_LPGA_GAIN_MASK          0x7F
#define ADC_RPGA_GAIN_MASK          0x7F

/* ADC current modes */
#define ADC_LOW_CURR_MODE       D0

/* Left ADC Input selection bits */
#define ADC_LCH_SEL1_SHIFT          0
#define ADC_LCH_SEL2_SHIFT          2
#define ADC_LCH_SEL3_SHIFT          4
#define ADC_LCH_SEL4_SHIFT          6

#define ADC_LCH_SEL1X_SHIFT         0
#define ADC_LCH_SEL2X_SHIFT         2
#define ADC_LCH_SEL3X_SHIFT         4
#define ADC_LCH_COMMON_MODE         D6
#define ADC_BYPASS_LPGA             D7

/* Right ADC Input selection bits */
#define ADC_RCH_SEL1_SHIFT          0
#define ADC_RCH_SEL2_SHIFT          2
#define ADC_RCH_SEL3_SHIFT          4
#define ADC_RCH_SEL4_SHIFT          6

#define ADC_RCH_SEL1X_SHIFT         0
#define ADC_RCH_SEL2X_SHIFT         2
#define ADC_RCH_SEL3X_SHIFT         4
#define ADC_RCH_COMMON_MODE         D6
#define ADC_BYPASS_RPGA             D7

/* MICBIAS control bits */
#define ADC_MICBIAS1_SHIFT          5
#define ADC_MICBIAS2_SHIFT          3

#define ADC_MAX_VOLUME          64
#define ADC_POS_VOL         24

#define ADC_TDM_CHANNEL_EN_SHIFT    2
#define ADC_TDM_CHANNEL_SWAP_SHIFT  4
#define ADC_TDM_EN_SHIFT            0
#define ADC_TDM_MASK                0x3

#define EARLY_3STATE_ENABLED        0x02
/*
 *****************************************************************************
 * Structures Definitions
 *****************************************************************************
 */
/*
 *----------------------------------------------------------------------------
 * @struct  aic31xx_setup_data |
 *          i2c specific data setup for AIC31xx.
 * @field   unsigned short |i2c_address |
 *          Unsigned short for i2c address.
 *----------------------------------------------------------------------------
 */
struct aic31xx_setup_data {
	unsigned short i2c_address;
};

/*
 *----------------------------------------------------------------------------
 * @struct  aic31xx_priv |
 *          AIC31xx priviate data structure to set the system clock, mode and
 *          page number.
 * @field   u32 | sysclk |
 *          system clock
 * @field   s32 | master |
 *          master/slave mode setting for AIC31xx
 * @field   u8 | page_no |
 *          page number. Here, page 0 and page 1 are used.
 *----------------------------------------------------------------------------
 */
struct aic31xx_priv {
	u32 clkid;
	u32 clksrc;
	u32 sysclk;
	struct clk *mclk;
	u8 adc_page_no[NUM_ADC3101];
	struct i2c_client *adc_control_data[NUM_ADC3101];
	struct mutex codecMutex;
	struct gpio_desc *enable_gpiod;
	u32 freq_in;
	u32 freq_out;
	u8 dsp_a_val;
	unsigned int tdm_rx_mask;
	int tdm_slot_width;
};

/*
 *----------------------------------------------------------------------------
 * @struct  aic31xx_configs |
 *          AIC31xx initialization data which has register offset and register
 *          value.
 * @field   u16 | reg_offset |
 *          AIC31xx Register offsets required for initialization..
 * @field   u8 | reg_val |
 *          value to set the AIC31xx register to initialize the AIC31xx.
 *----------------------------------------------------------------------------
 */
struct aic31xx_configs {
	u32 reg_offset;
	u8 reg_val;
};

/*
 * adc3xxx initialization data
 * This structure initialization contains the initialization required for
 * ADC3xxx.
 * These registers values (reg_val) are written into the respective ADC3xxx
 * register offset (reg_offset) to  initialize ADC3xxx.
 * These values are used in adc3xxx_init() function only.
 */
struct adc31xx_configs {
	u32 reg_offset;
	u8 reg_val;
};

/****************** RATES TABLE FOR ADC31xx ************************/
struct adc31xx_rate_divs {
	u32 mclk;
	u32 rate;
	u8 pll_p;
	u8 pll_r;
	u8 pll_j;
	u16 pll_d;
	u8 nadc;
	u8 madc;
	u8 aosr;
	u8 bdiv_n;
	u8 iadc;
};

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_dai |
 *          It is SoC Codec DAI structure which has DAI capabilities viz.,
 *          playback and capture, DAI runtime information viz. state of DAI
 *          and pop wait state, and DAI private data.
 *----------------------------------------------------------------------------
 */
extern struct snd_soc_dai tlv320aic3101_dai;

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_device |
 *          This structure is soc audio codec device sturecute which pointer
 *          to basic functions aic31xx_probe(), aic31xx_remove(),
 *          aic31xx_suspend() and aic31xx_resume()
 *
 */
extern struct snd_soc_codec_device soc_codec_dev_aic31xx;

#endif              /* _Tlv320aic3101_H */
