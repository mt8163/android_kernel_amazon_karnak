/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Gpio.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"

#include <sound/mt_soc_audio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/completion.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/semaphore.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/switch.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/div64.h>
#include <mach/upmu_hw.h>
#include <mt-plat/mt_gpio.h>

#include <stdarg.h>
#include <linux/module.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>
#include <linux/debugfs.h>
#include "mt_soc_codec_63xx.h"
#include "mt_amzn_mclk.h"
#include "../../codecs/tlv320aic3101.h"

static int mt_soc_lowjitter_control;
static struct dentry *mt_sco_audio_debugfs;


#define DEBUG_FS_NAME "mtksocaudio"
#define DEBUG_ANA_FS_NAME "mtksocanaaudio"

static int mt_soc_channel_type;
static unsigned int mt_soc_mclk_freq = AIC31XX_FREQ_9600000;

struct of_device_id mt_machine_of_match[] = {
	{ .compatible = "mediatek,mt8163-audiosys", },
	{},
};

/* IRQ value for gpio pin*/
static int amp_fault_irq;
/* GPIO pin connected to AMP Fault */
static int amp_fault_gpiopin = -1;
/* Interrupt type*/
static unsigned int amp_fault_type;
static int amp_fault_debounce;
/* AMP Fault enabled */
static int amp_fault_enabled;
/* AMP Fault handler */
static struct delayed_work amp_fault_work;
/* AMP switch */
static struct switch_dev amp_fault_switch;

/***********************************************************************
 * amp_switch_work
 * Flip Amplifier SD line to clear Fault
 * *********************************************************************/

static void amp_switch_work(struct work_struct *work)
{
	if (amp_fault_enabled) {
		pr_info("%s: amp_fault. Reseting Amp!\n", __func__);
		/* Disable AMP first */
		AudDrv_GPIO_EXTAMP_Select(0);
		/* Wait for 1.5 sec */
		msleep(1500);
		/* Enable Amp to clear signal */
		AudDrv_GPIO_EXTAMP_Select(1);
	}
	switch_set_state((struct switch_dev *)&amp_fault_switch,
		gpio_get_value(amp_fault_gpiopin));
	enable_irq(amp_fault_gpiopin);

	return;
}

/***********************************************************************
 * mt_soc_ampfault_irq_callback
 * Handle interrupt callback for Amplifier Fault
 * *********************************************************************/
static irqreturn_t mt_soc_ampfault_irq_callback(int irq, void *data)
{
	pr_info("%s\n", __func__);

	if (amp_fault_gpiopin < 0 || !amp_fault_enabled) {
		pr_err("%s: amp_fault pin=%d enabled=%d. Cannot proceed!\n",
			__func__, amp_fault_gpiopin, amp_fault_enabled);
		return IRQ_HANDLED;
	}

	disable_irq_nosync(amp_fault_gpiopin);

	schedule_delayed_work(&amp_fault_work, msecs_to_jiffies(10));

	return IRQ_HANDLED;
}

static int mt_soc_get_dts_data(void)
{
	struct device_node *node = NULL;
	u32 ints1[2] = { 0, 0 };
	int ret = 0;

	node = of_find_matching_node(node, mt_machine_of_match);
	if (node) {
		if (of_property_read_u32(node, "channel-type",
							&mt_soc_channel_type)) {
			pr_warn("%s: No channel type defined\n", __func__);
			mt_soc_channel_type = 0;
		}
		if (of_property_read_u32(node, "mclk-frequency",
			&mt_soc_mclk_freq)) {
			pr_warn("%s: No MCLK frequency in dts. Using default\n",
				__func__);
		}

		/* Setup Amp fault if gpio is provided */
		amp_fault_gpiopin = of_get_named_gpio(node, "amp-fault-gpio",
					0);
		if (amp_fault_gpiopin < 0) {
			pr_info("%s: amp_fault pin info not found!\n",
				__func__);
		} else {
			of_property_read_u32(node, "amp-fault-debounce",
				&amp_fault_debounce);

			of_property_read_u32_array(node, "interrupts", ints1,
				ARRAY_SIZE(ints1));
			amp_fault_type = ints1[1];

			ret = gpio_request(amp_fault_gpiopin, "amp-fault-gpio");
			if (ret) {
				pr_err("%s: amp_fault pin=%d. gpio_request failed. Error=%d\n",
					__func__, amp_fault_gpiopin, ret);
				return ret;
			}

			gpio_direction_input(amp_fault_gpiopin);
			gpio_set_debounce(amp_fault_gpiopin, amp_fault_debounce);

			amp_fault_irq = gpio_to_irq(amp_fault_gpiopin);

			ret = request_irq(amp_fault_irq,
				mt_soc_ampfault_irq_callback,
				IRQF_TRIGGER_FALLING, "ampfault-eint", NULL);
			if (ret) {
				pr_err("%s: amp_fault request_irq failed. Error=%d\n",
					__func__, ret);
				return ret;
			}

			pr_info("%s: amp_fault pin=%d irq=%d, deb=%d\n",
				__func__, amp_fault_gpiopin, amp_fault_irq,
				amp_fault_debounce);

			INIT_DELAYED_WORK(&amp_fault_work, amp_switch_work);
		}

		pr_info("%s: MCLK frequency = %u\n", __func__, mt_soc_mclk_freq);

	} else {
		pr_warn("%s: No matching dts node\n", __func__);
		return -1;
	}
	return 0;
}

#define LINEIN_ADC_DIS 0
static int linein_adc_enab = LINEIN_ADC_DIS;

/* To utilize HDMI Buffer for audio transfer
 * This should also be enabled in amzn_mt_spi_pcm.c
#define SPI_USES_HDMI_BUFFER
*/

static int mtmachine_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtmachine_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static struct snd_soc_ops mt_machine_audio_ops = {

	.startup = mtmachine_startup,
	.prepare = mtmachine_prepare,
};

static const char *const mt8163_channel_cap[] = {
		"Stereo", "MonoLeft", "MonoRight"
};

static int mt8163_channel_cap_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
		return 0;
}

static int mt8163_channel_cap_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
		ucontrol->value.integer.value[0] = mt_soc_channel_type;
		return 0;
}

static int tlv3101_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *codec_dai;

	pr_info("aic:%s\n", __func__);
	if (substream == NULL) {
		pr_err("invalid stream parameter\n");
		return -EINVAL;
	}

	rtd = substream->private_data;
	if (rtd == NULL) {
		pr_err("invalid runtime parameter\n");
		return -EINVAL;
	}

	codec_dai = rtd->codec_dai;
	if (codec_dai == NULL) {
		pr_err("invalid dai parameter\n");
		return -EINVAL;
	}

	if (mt_soc_mclk_freq == AIC31XX_FREQ_24576000) {
		snd_soc_dai_set_pll(codec_dai, AIC3101_PLL_BCLK,
			AIC3101_PLL_ADC_FS_CLKIN_MCLK,
			mt_soc_mclk_freq, params_rate(params));
	} else {
		snd_soc_dai_set_pll(codec_dai, AIC3101_PLL_BCLK,
			AIC3101_PLL_ADC_FS_CLKIN_PLL_MCLK,
			mt_soc_mclk_freq, params_rate(params));
	}

	snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBM_CFM |
			SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_NB_NF);

	/* If line in is enabled at adc send new mask. New value only enables
	 * left channel from the 8th adc to propagate. We also have to change
	 * the slot width value. The 8th channel by default is cut off
	 * by the fpga. To counter this we set the offset value to 0.
	 */
	if (linein_adc_enab)
		snd_soc_dai_set_tdm_slot(codec_dai, 0x00, 0x40,
					params_channels(params), 0);
	else
		snd_soc_dai_set_tdm_slot(codec_dai, 0x00, 0x7f,
				params_channels(params),
				snd_pcm_format_width(params_format(params)));

	return 0;
}

static struct snd_soc_ops tlv3101_machine_ops = {
	.startup = mtmachine_startup,
	.prepare = mtmachine_prepare,
	.hw_params = tlv3101_hw_params,
};

#if 0				/* not used */
static int mtmachine_compr_startup(struct snd_compr_stream *stream)
{
	return 0;
}

static struct snd_soc_compr_ops mt_machine_audio_compr_ops = {

	.startup = mtmachine_compr_startup,
};
#endif

static int mtmachine_startupmedia2(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtmachine_preparemedia2(struct snd_pcm_substream *substream)
{
	return 0;
}

static struct snd_soc_ops mtmachine_audio_ops2 = {

	.startup = mtmachine_startupmedia2,
	.prepare = mtmachine_preparemedia2,
};

static int mt_soc_audio_init(struct snd_soc_pcm_runtime *rtd)
{
	pr_info("%s\n", __func__);
	return 0;
}

static int mt_soc_audio_init_tlv(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pr_info("%s\n", __func__);
#ifndef CONFIG_SND_I2S_MCLK
	if (rtd == NULL || rtd->codec_dai == NULL) {
		pr_err("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	/* Initialize ISP clocks */
	ret = isp_clk_init();
	if (ret) {
		pr_err("%s: isp_clock_initialize Failed. Error = %d\n",
		__func__, ret);
		return ret;
	}
	/* Enable ISP clocks */
	ret = isp_clk_enable(1);
	if (ret) {
		pr_err("%s: isp_clock_enable Failed. Error = %d\n",
		__func__, ret);
		return ret;
	}
	/* Turn on MCLK clocks */
	ret = mclk_enable_reg(1);
	if (ret) {
		pr_err("%s: mclk_enable_reg Failed. Error = %d\n",
		__func__, ret);
		return ret;
	}
	/* Initialize CCF clocks */
	ret = ccf_clk_init();
	if (ret) {
		pr_err("%s: ccf_clk_initialize Failed. Error = %d\n",
		__func__, ret);
		return ret;
	}
	/* Enable CCF clocks */
	ret = ccf_clk_enable(1);
	if (ret) {
		pr_err("%s: ccf_clk_enable Failed. Error = %d\n",
		__func__, ret);
		return ret;
	}
	/* Turn on MCLK clocks again */
	ret = mclk_enable_reg(1);
	if (ret) {
		pr_err("%s: mclk_enable_reg Failed. Error = %d\n",
		__func__, ret);
		return ret;
	}
	/* Setup clock divider */
	ret = mclk_divider_reg(AIC31XX_FREQ_9600000);
	if (ret) {
		pr_err("%s: mclk_cnt_reg Failed. Error = %d\n",
		__func__, ret);
		return ret;
	}

#ifdef DEBUG_ISP_MMCLK
	register_dump();
#endif
#else
	AudDrv_ANA_Clk_On();
	AudDrv_Clk_On();
	EnableApll(48000, true);
	EnableApllTuner(48000, true);
	SetCLkMclk(Soc_Aud_I2S2, 48000);
	EnableI2SDivPower(AUDIO_APLL12_DIV2, true);

	Afe_Set_Reg(AFE_I2S_CON2, 1<<12, 1<<12);
	Afe_Set_Reg(AFE_I2S_CON2, 1, 1);        /* enable I2S2 port */
	EnableAfe(true);
#endif
	return ret;
}

static int tlv320aic3204_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *codec_dai;

	if (substream == NULL) {
		pr_err("%s: invalid stream parameter\n", __func__);
		return -EINVAL;
	}

	rtd = substream->private_data;
	if (rtd == NULL) {
		pr_err("%s: invalid runtime parameter\n", __func__);
		return -EINVAL;
	}

	codec_dai = rtd->codec_dai;
	if (codec_dai == NULL) {
		pr_err("%s: invalid dai parameter\n", __func__);
		return -EINVAL;
	}

	/* set codec DAI configuration */
	if (snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS))
		pr_err("Failed to set fmt for %s\n", codec_dai->name);

	if (snd_soc_dai_set_sysclk(codec_dai, 0, mt_soc_mclk_freq,
		SND_SOC_CLOCK_OUT))
		pr_err("%s: Failed to set sysclk for %s\n", __func__,
		rtd->codec_dai->name);

	return 0;
}

static struct snd_soc_ops tlv320aic3204_machine_ops = {
	.startup = mtmachine_startup,
	.prepare = mtmachine_prepare,
	.hw_params = tlv320aic3204_hw_params,
};

static int mt_soc_audio_init2(struct snd_soc_pcm_runtime *rtd)
{
	pr_info("mt_soc_audio_init2\n");
	return 0;
}

static int mt_soc_ana_debug_open(struct inode *inode, struct file *file)
{
	pr_info("mt_soc_ana_debug_open\n");
	return 0;
}

static ssize_t mt_soc_ana_debug_read(struct file *file, char __user *buf,
				     size_t count, loff_t *pos)
{
	const int size = 4096;
	char buffer[size];
	int n = 0;

	pr_info("mt_soc_ana_debug_read count = %zu\n", count);
	AudDrv_ANA_Clk_On();
	AudDrv_Clk_On();
	audckbufEnable(true);

	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON0  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON0));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON1  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON1));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON2  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON2));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON3  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON3));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON4  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON4));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON5  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON5));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON6  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON6));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON7  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON7));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON8  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON8));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON9  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON9));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON10  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON10));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_CON11  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON11));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_STA0  = 0x%x\n", Ana_Get_Reg(ABB_AFE_STA0));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_STA1  = 0x%x\n", Ana_Get_Reg(ABB_AFE_STA1));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_STA2  = 0x%x\n", Ana_Get_Reg(ABB_AFE_STA2));
	n += scnprintf(buffer + n, size - n, "AFE_UP8X_FIFO_CFG0  = 0x%x\n",
		       Ana_Get_Reg(AFE_UP8X_FIFO_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_UP8X_FIFO_LOG_MON0  = 0x%x\n",
		       Ana_Get_Reg(AFE_UP8X_FIFO_LOG_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_UP8X_FIFO_LOG_MON1  = 0x%x\n",
		       Ana_Get_Reg(AFE_UP8X_FIFO_LOG_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG0  = 0x%x\n",
		       Ana_Get_Reg(AFE_PMIC_NEWIF_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG1  = 0x%x\n",
		       Ana_Get_Reg(AFE_PMIC_NEWIF_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG2  = 0x%x\n",
		       Ana_Get_Reg(AFE_PMIC_NEWIF_CFG2));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG3  = 0x%x\n",
		       Ana_Get_Reg(AFE_PMIC_NEWIF_CFG3));
	n += scnprintf(buffer + n, size - n, "ABB_AFE_TOP_CON0  = 0x%x\n",
		       Ana_Get_Reg(ABB_AFE_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "ABB_MON_DEBUG0  = 0x%x\n",
		       Ana_Get_Reg(ABB_MON_DEBUG0));

	n += scnprintf(buffer + n, size - n, "TOP_CKPDN0  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN0));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN1  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN1));
	n += scnprintf(buffer + n, size - n, "TOP_CKPDN2  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN2));
	n += scnprintf(buffer + n, size - n, "TOP_CKCON1  = 0x%x\n", Ana_Get_Reg(TOP_CKCON1));
	n += scnprintf(buffer + n, size - n, "SPK_CON0  = 0x%x\n", Ana_Get_Reg(SPK_CON0));
	n += scnprintf(buffer + n, size - n, "SPK_CON1  = 0x%x\n", Ana_Get_Reg(SPK_CON1));
	n += scnprintf(buffer + n, size - n, "SPK_CON2  = 0x%x\n", Ana_Get_Reg(SPK_CON2));
	n += scnprintf(buffer + n, size - n, "SPK_CON6  = 0x%x\n", Ana_Get_Reg(SPK_CON6));
	n += scnprintf(buffer + n, size - n, "SPK_CON7  = 0x%x\n", Ana_Get_Reg(SPK_CON7));
	n += scnprintf(buffer + n, size - n, "SPK_CON8  = 0x%x\n", Ana_Get_Reg(SPK_CON8));
	n += scnprintf(buffer + n, size - n, "SPK_CON9  = 0x%x\n", Ana_Get_Reg(SPK_CON9));
	n += scnprintf(buffer + n, size - n, "SPK_CON10  = 0x%x\n", Ana_Get_Reg(SPK_CON10));
	n += scnprintf(buffer + n, size - n, "SPK_CON11  = 0x%x\n", Ana_Get_Reg(SPK_CON11));
	n += scnprintf(buffer + n, size - n, "SPK_CON12  = 0x%x\n", Ana_Get_Reg(SPK_CON12));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON0  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON1  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON1));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON2  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON2));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON3  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON3));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON4  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON4));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON5  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON5));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON6  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON6));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON7  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON7));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON8  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON8));
	n += scnprintf(buffer + n, size - n, "AUDTOP_CON9  = 0x%x\n", Ana_Get_Reg(AUDTOP_CON9));
	pr_info("mt_soc_ana_debug_read len = %d\n", n);

	audckbufEnable(false);
	AudDrv_Clk_Off();
	AudDrv_ANA_Clk_Off();

	return simple_read_from_buffer(buf, count, pos, buffer, n);
}


static int mt_soc_debug_open(struct inode *inode, struct file *file)
{
	pr_info("mt_soc_debug_open\n");
	return 0;
}

static ssize_t mt_soc_debug_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	const int size = 4096;
	char buffer[size];
	int n = 0;

	AudDrv_ANA_Clk_On();
	AudDrv_Clk_On();

	pr_info("mt_soc_debug_read\n");
	n = scnprintf(buffer + n, size - n, "AUDIO_TOP_CON0  = 0x%x\n",
		      Afe_Get_Reg(AUDIO_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON1  = 0x%x\n",
		       Afe_Get_Reg(AUDIO_TOP_CON1));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON2  = 0x%x\n",
		       Afe_Get_Reg(AUDIO_TOP_CON2));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON3  = 0x%x\n",
		       Afe_Get_Reg(AUDIO_TOP_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON0  = 0x%x\n", Afe_Get_Reg(AFE_DAC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON1  = 0x%x\n", Afe_Get_Reg(AFE_DAC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON  = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON));
	n += scnprintf(buffer + n, size - n, "AFE_DAIBT_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_DAIBT_CON0));

	n += scnprintf(buffer + n, size - n, "AFE_CONN0  = 0x%x\n", Afe_Get_Reg(AFE_CONN0));
	n += scnprintf(buffer + n, size - n, "AFE_CONN1  = 0x%x\n", Afe_Get_Reg(AFE_CONN1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN2  = 0x%x\n", Afe_Get_Reg(AFE_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_CONN3  = 0x%x\n", Afe_Get_Reg(AFE_CONN3));
	n += scnprintf(buffer + n, size - n, "AFE_CONN4  = 0x%x\n", Afe_Get_Reg(AFE_CONN4));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON1  = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON2  = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_CON  = 0x%x\n", Afe_Get_Reg(AFE_MRGIF_CON));

	n += scnprintf(buffer + n, size - n, "AFE_DL1_BASE  = 0x%x\n", Afe_Get_Reg(AFE_DL1_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_CUR  = 0x%x\n", Afe_Get_Reg(AFE_DL1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_END  = 0x%x\n", Afe_Get_Reg(AFE_DL1_END));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON3  = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON3));

	n += scnprintf(buffer + n, size - n, "AFE_DL2_BASE  = 0x%x\n", Afe_Get_Reg(AFE_DL2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_CUR  = 0x%x\n", Afe_Get_Reg(AFE_DL2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_END  = 0x%x\n", Afe_Get_Reg(AFE_DL2_END));
	n += scnprintf(buffer + n, size - n, "AFE_CONN5  = 0x%x\n", Afe_Get_Reg(AFE_CONN5));
	n += scnprintf(buffer + n, size - n, "AFE_CONN_24BIT  = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN_24BIT));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_BASE  = 0x%x\n", Afe_Get_Reg(AFE_AWB_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_END  = 0x%x\n", Afe_Get_Reg(AFE_AWB_END));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_CUR  = 0x%x\n", Afe_Get_Reg(AFE_AWB_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_BASE  = 0x%x\n", Afe_Get_Reg(AFE_VUL_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_END  = 0x%x\n", Afe_Get_Reg(AFE_VUL_END));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_CUR  = 0x%x\n", Afe_Get_Reg(AFE_VUL_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_CONN6  = 0x%x\n", Afe_Get_Reg(AFE_CONN6));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_BASE  = 0x%x\n", Afe_Get_Reg(AFE_DAI_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_END  = 0x%x\n", Afe_Get_Reg(AFE_DAI_END));
	n += scnprintf(buffer + n, size - n, "AFE_DAI_CUR  = 0x%x\n", Afe_Get_Reg(AFE_DAI_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MSB  = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MSB));

	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON2  = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON4  = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON4));

	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC2_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC2_CON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_UL_SRC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_CON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_UL_SRC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_TOP_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_DL_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_UL_DL_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_SRC_DEBUG));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG_MON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_SRC_DEBUG_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG_MON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_SRC_DEBUG_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_NEWIF_CFG0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_NEWIF_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_NEWIF_CFG1  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_NEWIF_CFG1));

	n += scnprintf(buffer + n, size - n, "AFE_SIDETONE_DEBUG  = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_DEBUG));
	n += scnprintf(buffer + n, size - n, "AFE_SIDETONE_MON  = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_MON));
	n += scnprintf(buffer + n, size - n, "AFE_SIDETONE_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_SIDETONE_COEFF  = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_COEFF));
	n += scnprintf(buffer + n, size - n, "AFE_SIDETONE_CON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_SIDETONE_GAIN  = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_GAIN));
	n += scnprintf(buffer + n, size - n, "AFE_SGEN_CON0  = 0x%x\n", Afe_Get_Reg(AFE_SGEN_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_TOP_CON0  = 0x%x\n", Afe_Get_Reg(AFE_TOP_CON0));

	n += scnprintf(buffer + n, size - n, "AFE_ADDA_PREDIS_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_PREDIS_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_PREDIS_CON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_PREDIS_CON1));

	n += scnprintf(buffer + n, size - n, "AFE_MRG_MON0  = 0x%x\n", Afe_Get_Reg(AFE_MRGIF_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_MRG_MON1  = 0x%x\n", Afe_Get_Reg(AFE_MRGIF_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_MRG_MON2  = 0x%x\n", Afe_Get_Reg(AFE_MRGIF_MON2));

	n += scnprintf(buffer + n, size - n, "AFE_MOD_DAI_BASE  = 0x%x\n",
		       Afe_Get_Reg(AFE_MOD_DAI_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_MOD_DAI_END  = 0x%x\n",
		       Afe_Get_Reg(AFE_MOD_DAI_END));
	n += scnprintf(buffer + n, size - n, "AFE_MOD_DAI_CUR  = 0x%x\n",
		       Afe_Get_Reg(AFE_MOD_DAI_CUR));

	n += scnprintf(buffer + n, size - n, "AFE_DL1_D2_BASE  = 0x%x\n",
		       Afe_Get_Reg(AFE_DL1_D2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_D2_END  = 0x%x\n",
		       Afe_Get_Reg(AFE_DL1_D2_END));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_D2_CUR  = 0x%x\n",
		       Afe_Get_Reg(AFE_DL1_D2_CUR));

	n += scnprintf(buffer + n, size - n, "AFE_VUL_D2_BASE  = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_D2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_D2_END  = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_D2_END));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_D2_CUR  = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_D2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CON  = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CON  = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_STATUS  = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_STATUS));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_CLR  = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CLR));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT1  = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CNT1));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT2  = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CNT2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_EN  = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_EN));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MON2  = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_CNT_MON  = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ1_MCU_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ2_CNT_MON  = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ2_MCU_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_EN_CNT_MON  = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ1_MCU_EN_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MAXLEN  = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MAXLEN));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_PBUF_SIZE  = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_PBUF_SIZE));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT7  = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CNT7));

	n += scnprintf(buffer + n, size - n, "AFE_APLL1_TUNER_CFG  = 0x%x\n",
		       Afe_Get_Reg(AFE_APLL1_TUNER_CFG));
	n += scnprintf(buffer + n, size - n, "AFE_APLL2_TUNER_CFG  = 0x%x\n",
		       Afe_Get_Reg(AFE_APLL2_TUNER_CFG));

	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN1_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN1_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON2  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN1_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON3  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN1_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CONN  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN1_CONN));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CUR  = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON2  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON3  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CONN  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CONN));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CUR  = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CONN2  = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CONN2));

	n += scnprintf(buffer + n, size - n, "FPGA_CFG2  = 0x%x\n", Afe_Get_Reg(FPGA_CFG2));
	n += scnprintf(buffer + n, size - n, "FPGA_CFG3  = 0x%x\n", Afe_Get_Reg(FPGA_CFG3));
	n += scnprintf(buffer + n, size - n, "FPGA_CFG0  = 0x%x\n", Afe_Get_Reg(FPGA_CFG0));
	n += scnprintf(buffer + n, size - n, "FPGA_CFG1  = 0x%x\n", Afe_Get_Reg(FPGA_CFG1));
	n += scnprintf(buffer + n, size - n, "FPGA_STC  = 0x%x\n", Afe_Get_Reg(FPGA_STC));

	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON0  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON1  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON2  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON3  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON4  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON4));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON5  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON5));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON6  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON6));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON7  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON7));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON8  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON8));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON9  = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON9));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON10  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CON10));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON11  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CON11));
	n += scnprintf(buffer + n, size - n, "PCM_INTF_CON1  = 0x%x\n",
				Afe_Get_Reg(PCM_INTF_CON1));
	n += scnprintf(buffer + n, size - n, "PCM_INTF_CON2  = 0x%x\n", Afe_Get_Reg(PCM_INTF_CON2));
	n += scnprintf(buffer + n, size - n, "PCM2_INTF_CON  = 0x%x\n", Afe_Get_Reg(PCM2_INTF_CON));

	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON13  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CON13));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON14  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CON14));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON15  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CON15));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON16  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CON16));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON17  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CON17));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON18  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CON18));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON19  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CON19));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON20  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CON20));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CON21  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CON21));

	n += scnprintf(buffer + n, size - n, "AFE_ASRC4_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC4_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC4_CON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC4_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC4_CON2  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC4_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC4_CON3  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC4_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC4_CON4  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC4_CON4));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC4_CON5  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC4_CON5));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC4_CON6  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC4_CON6));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC4_CON7  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC4_CON7));

	n += scnprintf(buffer + n, size - n, "AFE_ASRC2_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC2_CON5  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC2_CON5));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC2_CON6  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC2_CON6));

	n += scnprintf(buffer + n, size - n, "AFE_ASRC3_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC3_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC3_CON5  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC3_CON5));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC3_CON6  = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC3_CON6));

	/* add */
	n += scnprintf(buffer + n, size - n, "AFE_ADDA4_TOP_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA4_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA4_UL_SRC_CON0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA4_UL_SRC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA4_UL_SRC_CON1  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA4_UL_SRC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA4_NEWIF_CFG0  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA4_NEWIF_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA4_NEWIF_CFG1  = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA4_NEWIF_CFG1));

	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_0  = 0x%x\n",
		       GetClkCfg(AUDIO_CLK_AUDDIV_0));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUDDIV_1  = 0x%x\n",
		       GetClkCfg(AUDIO_CLK_AUDDIV_1));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_CFG_6      = 0x%x\n",
		       GetClkCfg(AUDIO_CLK_CFG_6));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_CFG_6_SET  = 0x%x\n",
		       GetClkCfg(AUDIO_CLK_CFG_6_SET));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_CFG_6_CLR  = 0x%x\n",
		       GetClkCfg(AUDIO_CLK_CFG_6_CLR));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUD_DIV0   = 0x%x\n",
		       GetClkCfg(AUDIO_CLK_AUD_DIV0));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUD_DIV1   = 0x%x\n",
		       GetClkCfg(AUDIO_CLK_AUD_DIV1));
	n += scnprintf(buffer + n, size - n, "AUDIO_CLK_AUD_DIV2   = 0x%x\n",
		       GetClkCfg(AUDIO_CLK_AUD_DIV2));
	n += scnprintf(buffer + n, size - n, "AP_PLL_CON5   = 0x%x\n", GetpllCfg(AP_PLL_CON5));

	n += scnprintf(buffer + n, size - n, "AFE_HDMI_BASE  = 0x%x\n", Afe_Get_Reg(AFE_HDMI_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_CUR  = 0x%x\n", Afe_Get_Reg(AFE_HDMI_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_END  = 0x%x\n", Afe_Get_Reg(AFE_HDMI_END));

	n += scnprintf(buffer + n, size - n, "AFE_HDMI_CONN0  = 0x%x\n",
		       Afe_Get_Reg(AFE_HDMI_CONN0));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_CONN1  = 0x%x\n",
		       Afe_Get_Reg(AFE_HDMI_CONN1));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_OUT_CON0	= 0x%x\n",
		       Afe_Get_Reg(AFE_HDMI_OUT_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_TDM_CON1	= 0x%x\n",
		       Afe_Get_Reg(AFE_TDM_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_TDM_CON2	= 0x%x\n",
		       Afe_Get_Reg(AFE_TDM_CON2));

	pr_info("mt_soc_debug_read len = %d\n", n);
	AudDrv_Clk_Off();
	AudDrv_ANA_Clk_Off();

	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static const char ParSetkeyAfe[] = "Setafereg";
static const char ParSetkeyAna[] = "Setanareg";
static const char ParSetkeyCfg[] = "Setcfgreg";
static const char PareGetkeyAfe[] = "Getafereg";
static const char PareGetkeyAna[] = "Getanareg";
/* static const char ParGetkeyCfg[] = "Getcfgreg"; */
/* static const char ParSetAddr[] = "regaddr"; */
/* static const char ParSetValue[] = "regvalue"; */

static ssize_t mt_soc_debug_write(struct file *f, const char __user *buf,
				  size_t count, loff_t *offset)
{
	int ret = 0;
	char InputString[256];
	char *token1 = NULL;
	char *token2 = NULL;
	char *token3 = NULL;
	char *token4 = NULL;
	char *token5 = NULL;
	char *temp = NULL;

	unsigned long int regaddr = 0;
	unsigned long int regvalue = 0;
	char delim[] = " ,";

	memset((void *)InputString, 0, 256);
	if (copy_from_user((InputString), buf, count)) {
		pr_info("copy_from_user mt_soc_debug_write count = %zu temp = %s\n",
			count, InputString);
	}
	temp = kstrdup(InputString, GFP_KERNEL);
	pr_info("copy_from_user mt_soc_debug_write count = %zu temp = %s pointer = %p\n",
		count, InputString, InputString);
	token1 = strsep(&temp, delim);
	pr_debug("token1\n");
	pr_debug("token1 = %s\n", token1);
	token2 = strsep(&temp, delim);
	pr_debug("token2 = %s\n", token2);
	token3 = strsep(&temp, delim);
	pr_debug("token3 = %s\n", token3);
	token4 = strsep(&temp, delim);
	pr_debug("token4 = %s\n", token4);
	token5 = strsep(&temp, delim);
	pr_debug("token5 = %s\n", token5);

	if (strcmp(token1, ParSetkeyAfe) == 0) {
		pr_debug("strcmp (token1,ParSetkeyAfe)\n");
		ret = kstrtol(token3, 16, &regaddr);
		ret = kstrtol(token5, 16, &regvalue);
		pr_debug("%s regaddr = 0x%lu regvalue = 0x%lu\n", ParSetkeyAfe, regaddr, regvalue);
		Afe_Set_Reg(regaddr, regvalue, 0xffffffff);
		regvalue = Afe_Get_Reg(regaddr);
		pr_debug("%s regaddr = 0x%lu regvalue = 0x%lu\n", ParSetkeyAfe, regaddr, regvalue);
	}
	if (strcmp(token1, ParSetkeyAna) == 0) {
		pr_debug("strcmp (token1,ParSetkeyAna)\n");
		ret = kstrtol(token3, 16, &regaddr);
		ret = kstrtol(token5, 16, &regvalue);
		pr_debug("%s regaddr = 0x%lu regvalue = 0x%lu\n", ParSetkeyAna, regaddr, regvalue);
		/* clk_buf_ctrl(CLK_BUF_AUDIO, true); */
		AudDrv_ANA_Clk_On();
		AudDrv_Clk_On();
		audckbufEnable(true);
		Ana_Set_Reg(regaddr, regvalue, 0xffffffff);
		regvalue = Ana_Get_Reg(regaddr);
		pr_debug("%s regaddr = 0x%lu regvalue = 0x%lu\n", ParSetkeyAna, regaddr, regvalue);
	}
	if (strcmp(token1, ParSetkeyCfg) == 0) {
		pr_debug("strcmp (token1,ParSetkeyCfg)\n");
		ret = kstrtol(token3, 16, &regaddr);
		ret = kstrtol(token5, 16, &regvalue);
		pr_debug("%s regaddr = 0x%lu regvalue = 0x%lu\n", ParSetkeyCfg, regaddr, regvalue);
		SetClkCfg(regaddr, regvalue, 0xffffffff);
		regvalue = GetClkCfg(regaddr);
		pr_debug("%s regaddr = 0x%lu regvalue = 0x%lu\n", ParSetkeyCfg, regaddr, regvalue);
	}
	if (strcmp(token1, PareGetkeyAfe) == 0) {
		pr_debug("strcmp (token1,PareGetkeyAfe)\n");
		ret = kstrtol(token3, 16, &regaddr);
		regvalue = Afe_Get_Reg(regaddr);
		pr_debug("%s regaddr = 0x%lu regvalue = 0x%lu\n", PareGetkeyAfe, regaddr, regvalue);
	}
	if (strcmp(token1, PareGetkeyAna) == 0) {
		pr_debug("strcmp (token1,PareGetkeyAna)\n");
		ret = kstrtol(token3, 16, &regaddr);
		regvalue = Ana_Get_Reg(regaddr);
		pr_debug("%s regaddr = 0x%lu regvalue = 0x%lu\n", PareGetkeyAna, regaddr, regvalue);
	}
	return count;
}

static const struct file_operations mtaudio_debug_ops = {

	.open = mt_soc_debug_open,
	.read = mt_soc_debug_read,
	.write = mt_soc_debug_write,
};


static const struct file_operations mtaudio_ana_debug_ops = {

	.open = mt_soc_ana_debug_open,
	.read = mt_soc_ana_debug_read,
};


/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt_soc_dai_common[] = {

	/* FrontEnd DAI Links */
	{
	 .name = "MultiMedia1",
	 .stream_name = MT_SOC_DL1_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_DL1DAI_NAME,
	 .platform_name = MT_SOC_DL1_PCM,
	 .codec_dai_name = MT_SOC_CODEC_TXDAI_NAME,
	 .codec_name = MT_SOC_CODEC_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "MultiMedia2",
	 .stream_name = MT_SOC_UL1_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_UL1DAI_NAME,
	 .platform_name = MT_SOC_UL1_PCM,
	 .codec_dai_name = MT_SOC_CODEC_RXDAI_NAME,
	 .codec_name = MT_SOC_CODEC_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "Voice_MD1",
	 .stream_name = MT_SOC_VOICE_MD1_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_VOICE_MD1_NAME,
	 .platform_name = MT_SOC_VOICE_MD1,
	 .codec_dai_name = MT_SOC_CODEC_VOICE_MD1DAI_NAME,
	 .codec_name = MT_SOC_CODEC_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
#ifndef SPI_USES_HDMI_BUFFER
	{
	 .name = "HDMI_OUT",
	 .stream_name = MT_SOC_HDMI_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_HDMI_NAME,
	 .platform_name = MT_SOC_HDMI_PCM,
	 .codec_dai_name = MT_SOC_CODEC_HDMI_DUMMY_DAI_NAME,
	 .codec_name = MT_SOC_CODEC_DUMMY_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
#endif
	{
	 .name = "ULDLOOPBACK",
	 .stream_name = MT_SOC_ULDLLOOPBACK_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_ULDLLOOPBACK_NAME,
	 .platform_name = MT_SOC_ULDLLOOPBACK_PCM,
	 .codec_dai_name = MT_SOC_CODEC_ULDLLOOPBACK_NAME,
	 .codec_name = MT_SOC_CODEC_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "I2S0OUTPUT",
	 .stream_name = MT_SOC_I2S0_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_I2S0_NAME,
	 .platform_name = MT_SOC_I2S0_PCM,
	 .codec_dai_name = MT_SOC_CODEC_I2S0_DUMMY_DAI_NAME,
	 .codec_name = MT_SOC_CODEC_DUMMY_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "MRGRX",
	 .stream_name = MT_SOC_MRGRX_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_MRGRX_NAME,
	 .platform_name = MT_SOC_MRGRX_PCM,
	 .codec_dai_name = MT_SOC_CODEC_MRGRX_DAI_NAME,
	 .codec_name = MT_SOC_CODEC_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "MRGRXCAPTURE",
	 .stream_name = MT_SOC_MRGRX_CAPTURE_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_MRGRX_NAME,
	 .platform_name = MT_SOC_MRGRX_AWB_PCM,
	 .codec_dai_name = MT_SOC_CODEC_MRGRX_DUMMY_DAI_NAME,
	 .codec_name = MT_SOC_CODEC_DUMMY_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "I2S0DL1OUTPUT",
	 .stream_name = MT_SOC_I2SDL1_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_I2S0DL1_NAME,
	 .platform_name = MT_SOC_I2S0DL1_PCM,
	 .codec_dai_name = MT_SOC_CODEC_I2S0TXDAI_NAME,
	 .codec_name = MT_SOC_CODEC_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "DL1AWBCAPTURE",
	 .stream_name = MT_SOC_DL1_AWB_RECORD_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_DL1AWB_NAME,
	 .platform_name = MT_SOC_DL1_AWB_PCM,
	 .codec_dai_name = MT_SOC_CODEC_DL1AWBDAI_NAME,
	 .codec_name = MT_SOC_CODEC_DUMMY_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "Voice_MD1_BT",
	 .stream_name = MT_SOC_VOICE_MD1_BT_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_VOICE_MD1_BT_NAME,
	 .platform_name = MT_SOC_VOICE_MD1_BT,
	 .codec_dai_name = MT_SOC_CODEC_VOICE_MD1_BTDAI_NAME,
	 .codec_name = MT_SOC_CODEC_DUMMY_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "VOIP_CALL_BT_PLAYBACK",
	 .stream_name = MT_SOC_VOIP_BT_OUT_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_VOIP_CALL_BT_OUT_NAME,
	 .platform_name = MT_SOC_VOIP_BT_OUT,
	 .codec_dai_name = MT_SOC_CODEC_VOIPCALLBTOUTDAI_NAME,
	 .codec_name = MT_SOC_CODEC_DUMMY_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "VOIP_CALL_BT_CAPTURE",
	 .stream_name = MT_SOC_VOIP_BT_IN_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_VOIP_CALL_BT_IN_NAME,
	 .platform_name = MT_SOC_VOIP_BT_IN,
	 .codec_dai_name = MT_SOC_CODEC_VOIPCALLBTINDAI_NAME,
	 .codec_name = MT_SOC_CODEC_DUMMY_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "TDM_Debug_CAPTURE",
	 .stream_name = MT_SOC_TDM_CAPTURE_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_TDMRX_NAME,
	 .platform_name = MT_SOC_TDMRX_PCM,
	 .codec_dai_name = MT_SOC_CODEC_TDMRX_DAI_NAME,
	 .codec_name = MT_SOC_CODEC_DUMMY_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "FM_MRG_TX",
	 .stream_name = MT_SOC_FM_MRGTX_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_FM_MRGTX_NAME,
	 .platform_name = MT_SOC_FM_MRGTX_PCM,
	 .codec_dai_name = MT_SOC_CODEC_FMMRGTXDAI_DUMMY_DAI_NAME,
	 .codec_name = MT_SOC_CODEC_DUMMY_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "MultiMedia3",
	 .stream_name = MT_SOC_UL1DATA2_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_UL2DAI_NAME,
	 .platform_name = MT_SOC_UL2_PCM,
	 .codec_dai_name = MT_SOC_CODEC_RXDAI2_NAME,
	 .codec_name = MT_SOC_CODEC_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "I2S0_AWB_CAPTURE",
	 .stream_name = MT_SOC_I2S0AWB_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_I2S0AWBDAI_NAME,
	 .platform_name = MT_SOC_I2S0_AWB_PCM,
	 .codec_dai_name = MT_SOC_CODEC_I2S0AWB_NAME,
	 .codec_name = MT_SOC_CODEC_DUMMY_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },

	{
	 .name = "Voice_MD2",
	 .stream_name = MT_SOC_VOICE_MD2_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_VOICE_MD2_NAME,
	 .platform_name = MT_SOC_VOICE_MD2,
	 .codec_dai_name = MT_SOC_CODEC_VOICE_MD2DAI_NAME,
	 .codec_name = MT_SOC_CODEC_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "PLATOFRM_CONTROL",
	 .stream_name = MT_SOC_ROUTING_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_ROUTING_DAI_NAME,
	 .platform_name = MT_SOC_ROUTING_PCM,
	 .codec_dai_name = MT_SOC_CODEC_DUMMY_DAI_NAME,
	 .codec_name = MT_SOC_CODEC_DUMMY_NAME,
	 .init = mt_soc_audio_init2,
	 .ops = &mtmachine_audio_ops2,
	 },
	{
	 .name = "Voice_MD2_BT",
	 .stream_name = MT_SOC_VOICE_MD2_BT_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_VOICE_MD2_BT_NAME,
	 .platform_name = MT_SOC_VOICE_MD2_BT,
	 .codec_dai_name = MT_SOC_CODEC_VOICE_MD2_BTDAI_NAME,
	 .codec_name = MT_SOC_CODEC_DUMMY_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "HP_IMPEDANCE",
	 .stream_name = MT_SOC_HP_IMPEDANCE_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_HP_IMPEDANCE_NAME,
	 .platform_name = MT_SOC_HP_IMPEDANCE_PCM,
	 .codec_dai_name = MT_SOC_CODEC_HP_IMPEDANCE_NAME,
	 .codec_name = MT_SOC_CODEC_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "FM_I2S_RX_Playback",
	 .stream_name = MT_SOC_FM_I2S_PLAYBACK_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_FM_I2S_NAME,
	 .platform_name = MT_SOC_FM_I2S_PCM,
	 .codec_dai_name = MT_SOC_CODEC_FM_I2S_DAI_NAME,
	 .codec_name = MT_SOC_CODEC_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "FM_I2S_RX_Capture",
	 .stream_name = MT_SOC_FM_I2S_CAPTURE_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_FM_I2S_NAME,
	 .platform_name = MT_SOC_FM_I2S_AWB_PCM,
	 .codec_dai_name = MT_SOC_CODEC_FM_I2S_DUMMY_DAI_NAME,
	 .codec_name = MT_SOC_CODEC_DUMMY_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 },
	{
	 .name = "TI_DAC_Playback",
	 .stream_name = MT_SOC_TI_PLAY_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_DL1DAI_NAME,
	 .platform_name = MT_SOC_I2S0DL1_PCM,
	 .codec_dai_name = "tlv320aic32x4-hifi",
	 .codec_name = "tlv320aic32x4.2-0018",
	 .init = mt_soc_audio_init_tlv,
	 .ops = &tlv320aic3204_machine_ops,
	 .ignore_pmdown_time = 1,
	 },
	{
	 .name = "AMZN_SPI_Capture",
	 .stream_name = MT_SOC_TI_AIC3101_CAPTURE_STREAM_NAME,
	 .cpu_dai_name = AMZN_MT_SPI_PCM,
	 .platform_name = AMZN_MT_SPI_PCM,
	 .codec_dai_name = "tlv320aic3101-codec",
	 .codec_name = "tlv320aic3101.0-0018",
	 .ops = &tlv3101_machine_ops,
	 .ignore_pmdown_time = 1,
	},
	{
	 .name = "I2S1_Playback",
	 .stream_name = MT_SOC_I2S1_PLAYBACK_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_DL1DAI_NAME,
	 .platform_name = MT_SOC_I2S0DL1_PCM,
	 .codec_dai_name = MT_SOC_CODEC_DUMMY_DAI_NAME,
	 .codec_name = MT_SOC_CODEC_DUMMY_NAME,
	 .init = mt_soc_audio_init,
	 .ops = &mt_machine_audio_ops,
	 .ignore_pmdown_time = 1,
	},
};

static const char * const I2S_low_jittermode[] = { "Off", "On" };

static const struct soc_enum mt_soc_machine_enum[] = {

	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(I2S_low_jittermode), I2S_low_jittermode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt8163_channel_cap), mt8163_channel_cap),
};


static int mt8163_get_lowjitter(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s: mt_soc_lowjitter_control = %d\n", __func__, mt_soc_lowjitter_control);
	ucontrol->value.integer.value[0] = mt_soc_lowjitter_control;
	return 0;
}

static int mt8163_set_lowjitter(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s()\n", __func__);
	mt_soc_lowjitter_control = ucontrol->value.integer.value[0];
	return 0;
}

static const char * const control_functions[] = { "Off", "On"};

static const struct soc_enum control_function_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(control_functions),
			control_functions),
};

static int linein_adc_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s: = %d\n", __func__, linein_adc_enab);
	ucontrol->value.integer.value[0] = linein_adc_enab;
	return 0;
}

static int linein_adc_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(control_functions)) {
		pr_err("%s: Invalid input=%d\n", __func__,
		       ucontrol->value.enumerated.item[0]);
		return -EINVAL;
	}
	linein_adc_enab = ucontrol->value.integer.value[0];
	pr_info("%s: linein_adc_enab = %d\n", __func__,
			linein_adc_enab);
	return 0;
}

static int amp_fault_enable_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: amp_fault_enab= %d\n", __func__, linein_adc_enab);
	ucontrol->value.integer.value[0] = amp_fault_enabled;
	return 0;
}

static int amp_fault_enable_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(control_functions)) {
		pr_err("%s: Invalid input=%d\n", __func__,
		       ucontrol->value.enumerated.item[0]);
		return -EINVAL;
	}
	amp_fault_enabled = ucontrol->value.integer.value[0];
	pr_debug("%s: amp_fault_enab = %d\n", __func__,
			amp_fault_enabled);
	return 0;
}

static const struct snd_kcontrol_new mt_soc_controls[] = {

	SOC_ENUM_EXT("I2S low Jitter function", mt_soc_machine_enum[0],
			mt8163_get_lowjitter, mt8163_set_lowjitter),
	SOC_ENUM_EXT("Board Channel Config", mt_soc_machine_enum[1],
			mt8163_channel_cap_get, mt8163_channel_cap_set),
	SOC_ENUM_EXT("LineIn ADC", control_function_Enum[0], linein_adc_get,
			linein_adc_set),
	SOC_ENUM_EXT("Amp Fault Enable", control_function_Enum[0],
			amp_fault_enable_get, amp_fault_enable_set),
};

static struct snd_soc_card snd_soc_card_mt = {

	.name = "mt-snd-card",
	.dai_link = mt_soc_dai_common,
	.num_links = ARRAY_SIZE(mt_soc_dai_common),
	.controls = mt_soc_controls,
	.num_controls = ARRAY_SIZE(mt_soc_controls),
};

static struct platform_device *mt_snd_device;

static int __init mt_soc_snd_init(void)
{
	int ret;
	struct snd_soc_card *card = &snd_soc_card_mt;

	pr_info("mt_soc_snd_init card addr = %p\n", card);

	mt_snd_device = platform_device_alloc("soc-audio", -1);
	if (!mt_snd_device) {
		pr_err("mt6589_probe  platform_device_alloc fail\n");
		return -ENOMEM;
	}
	platform_set_drvdata(mt_snd_device, &snd_soc_card_mt);
	ret = platform_device_add(mt_snd_device);

	if (ret != 0) {
		pr_err("mt_soc_snd_init goto put_device fail\n");
		goto put_device;
	}

	pr_info("mt_soc_snd_init dai_link = %p\n", snd_soc_card_mt.dai_link);

	pr_info("mt_soc_snd_init dai_link -----\n");
	/* create debug file */
	mt_sco_audio_debugfs = debugfs_create_file(DEBUG_FS_NAME,
						   S_IFREG | S_IRUGO, NULL, (void *)DEBUG_FS_NAME,
						   &mtaudio_debug_ops);


	/* create analog debug file */
	mt_sco_audio_debugfs = debugfs_create_file(DEBUG_ANA_FS_NAME,
						   S_IFREG | S_IRUGO, NULL,
						   (void *)DEBUG_ANA_FS_NAME,
						   &mtaudio_ana_debug_ops);
	ret = mt_soc_get_dts_data();
	if (ret != 0) {
		pr_err("mt_soc_snd_init failed to load dts data\n");
		goto put_device;
	}

	if (amp_fault_gpiopin >= 0) {
		amp_fault_switch.name = "amp_fault";
		amp_fault_switch.index = 0;
		amp_fault_switch.state = 1;
		ret = switch_dev_register(&amp_fault_switch);
		if (ret) {
			pr_err("%s: switch_dev_register returned:%d!\n",
				__func__, ret);
		}
		switch_set_state((struct switch_dev *)&amp_fault_switch,
			gpio_get_value(amp_fault_gpiopin));
	}

	return 0;
put_device:
	platform_device_put(mt_snd_device);
	return ret;

}

static void __exit mt_soc_snd_exit(void)
{
	if (amp_fault_gpiopin >= 0)
		switch_dev_unregister(&amp_fault_switch);

	platform_device_unregister(mt_snd_device);
}
module_init(mt_soc_snd_init);
module_exit(mt_soc_snd_exit);

/* Module information */
MODULE_AUTHOR("ChiPeng <chipeng.chang@mediatek.com>");
MODULE_DESCRIPTION("ALSA SoC driver ");

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mt-snd-card");
