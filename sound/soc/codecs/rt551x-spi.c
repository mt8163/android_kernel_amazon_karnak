/*
 * rt551x-spi.c  --  ALC5514/ALC5518 SPI driver
 *
 * Copyright 2015 Realtek Semiconductor Corp.
 * Author: Oder Chiou <oder_chiou@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_qos.h>
#include <linux/sysfs.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#ifdef CONFIG_AMAZON_METRICS_LOG
#include  <linux/metricslog.h> //-- add include file for metrics logging through logcat_vital-->KDM
#endif

#include "rt551x-spi.h"
#include "rt551x.h"
#define SHIFT_SPI_READ_ALIGN 3
#define SPI_READ_ALIGN_BYTE (1 << SHIFT_SPI_READ_ALIGN)
#define SPI_READ_ALIGN_MASK  (~(SPI_READ_ALIGN_BYTE - 1))

static struct spi_device *rt551x_spi;
static atomic_t need_reset_dsp_read_pointer;

struct rt551x_dsp {
	struct device *dev;
	struct delayed_work copy_work;
	struct mutex dma_lock;
	struct snd_pcm_substream *substream;
	unsigned int buf_base, buf_limit, buf_rp, buf_wp;
	size_t dma_offset;
	unsigned int pre_buf_wp;
	int had_suspend;
	int sending_crash_event;
	int pcm_is_readable;
	bool     fgMetricLogPrint;
	ktime_t  StreamOpenTime;
	int dspStreamDuration;
	/* Used to signal completion for spi resume */
	wait_queue_head_t wait;
};

static struct rt551x_dsp *rt551x_dsp_pointer;

void set_pcm_is_readable(int readable) {
	if (!rt551x_dsp_pointer)
		return;
	mutex_lock(&rt551x_dsp_pointer->dma_lock);
	rt551x_dsp_pointer->pcm_is_readable = readable;
	mutex_unlock(&rt551x_dsp_pointer->dma_lock);
}


void reset_pcm_read_pointer(void) {
	atomic_set(&need_reset_dsp_read_pointer, 1);
}

void wait_for_rt551x_spi_resumed(void) {
	if (!rt551x_dsp_pointer)
		return;
	wait_event_timeout(rt551x_dsp_pointer->wait,
				!rt551x_dsp_pointer->had_suspend,
				msecs_to_jiffies(200)); // wait for 200ms
}

static const struct snd_pcm_hardware rt551x_spi_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.period_bytes_min	= 1024,     //32ms data * 2bytes * 1 channel * 16KHz
	.period_bytes_max	= 0x20000 / 8,
	.periods_min		= 2,
	.periods_max		= 8,
	.channels_min		= 1,
	.channels_max		= 1,
	.buffer_bytes_max	= 0x20000,
};

#define MIN_DSP_BUF_SIZE (1024 * 8)
// 16kHz * 1ch * 16bits
#define ONE_MS_DATA_LENGTH (16 * 1 * 2)
// 480ms pre-roll, 330ms delay + 190ms Alexa
#define MIN_SUSPEND_AUDIO_DURATION (480 + 330 + 190)

static struct snd_soc_dai_driver rt551x_spi_dai = {
	.name = "rt551x-dsp-cpu-dai",
	.id = 0,
	.capture = {
		.stream_name = "DSP_Capture",
		.channels_min = 1,
		.channels_max = 1,
		.rates = SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
};

static void send_dsp_reset_event(struct rt551x_dsp *rt551x_dsp) {
	static char * reset_event[] = { "ACTION=DSP_RESET", NULL };
	if (!rt551x_dsp->sending_crash_event) {
		pr_err("%s -- send dsp reset uevent!\n", __func__);
		kobject_uevent_env(&rt551x_dsp->dev->kobj, KOBJ_CHANGE, reset_event);
		rt551x_reset_duetoSPI();
#ifdef CONFIG_AMAZON_METRICS_LOG
		log_counter_to_vitals(ANDROID_LOG_INFO, "Kernel", "Kernel",
			"RT551X_DSP_metrics_count","DSP_Reset", 1, "count", NULL, VITALS_NORMAL);

		log_to_metrics(ANDROID_LOG_INFO, "voice_dsp", "voice_dsp:def:DSP_Reset=1;CT;1:NR");
#endif
	}
	rt551x_dsp->sending_crash_event = 1;
}

void rt551x_SetRP_onIdle(void)
{
	unsigned int tmp_wp = 0;
	unsigned int tmp_rp = 0;

	if (!rt551x_dsp_pointer)
		return;

	mutex_lock(&rt551x_dsp_pointer->dma_lock);

	if (!rt551x_dsp_pointer->buf_base) {
		pr_info("rt551x_SetRP_onIdle DSP Buffer Range is not initialized.\n");
		mutex_unlock(&rt551x_dsp_pointer->dma_lock);
		return;
	}

	rt551x_spi_read_addr(RT551X_BUFFER_VOICE_WP, &tmp_wp);
	rt551x_spi_read_addr(RT551X_REC_RP, &tmp_rp);
	pr_info("rt551x_SetRP_onIdle RT551X_REC_RP = 0x%x, RT551X_BUFFER_VOICE_WP = 0x%x\n", tmp_rp, tmp_wp);
	if (tmp_wp >= rt551x_dsp_pointer->buf_base && tmp_wp <= rt551x_dsp_pointer->buf_limit)
	{
		rt551x_spi_write_addr(RT551X_REC_RP, tmp_wp);
	}
	else
	{
		send_dsp_reset_event(rt551x_dsp_pointer);
	}
	mutex_unlock(&rt551x_dsp_pointer->dma_lock);
}

static bool is_range_valid(unsigned int rpt, unsigned int wpt,
		unsigned int base_pt, unsigned int limit_pt) {
	int check1, check2, check3, check4;

	check1 = ((RT551X_RW_ADDR_START <= rpt) && (rpt < RT551X_RW_ADDR_END))? 1 : 0;
	check2 = ((RT551X_RW_ADDR_START <= wpt) && (wpt < RT551X_RW_ADDR_END))? 1 : 0;
	check3 = ((RT551X_RW_ADDR_START <= base_pt) && (base_pt < RT551X_RW_ADDR_END))? 1 : 0;
	check4 = ((RT551X_RW_ADDR_START <= limit_pt) && (limit_pt < RT551X_RW_ADDR_END))? 1 : 0;

	if (check1 && check2 && check3 && check4) {
		if(base_pt + MIN_DSP_BUF_SIZE <= limit_pt && rpt >= base_pt && rpt <= limit_pt &&
			wpt >= base_pt && wpt <= limit_pt)
			return true;
	}
	pr_err("voice_dsp pointer error, rpt = 0x%x, wpt = 0x%x, base_pt = 0x%x, limit_pt = 0x%x",
		rpt, wpt, base_pt, limit_pt);
	return false;
}

static size_t rt551x_get_buf_size(struct rt551x_dsp *rt551x_dsp) {
	size_t buf_size;

	rt551x_spi_read_addr(RT551X_BUFFER_VOICE_WP, &rt551x_dsp->buf_wp);
	if (rt551x_dsp->buf_wp < rt551x_dsp->buf_base || rt551x_dsp->buf_wp > rt551x_dsp->buf_limit ||
		rt551x_dsp->buf_rp < rt551x_dsp->buf_base || rt551x_dsp->buf_rp > rt551x_dsp->buf_limit) {
		send_dsp_reset_event(rt551x_dsp);
		dev_err(rt551x_dsp->dev, "voice_dsp write pointer is invalid in function rt551x_get_buf_size\n");
		return 0;
	}
	rt551x_dsp->sending_crash_event = 0;

	if (rt551x_dsp->buf_wp >= rt551x_dsp->buf_rp)
		buf_size = rt551x_dsp->buf_wp - rt551x_dsp->buf_rp;
	else
		buf_size = (rt551x_dsp->buf_wp - rt551x_dsp->buf_base) +
			(rt551x_dsp->buf_limit - rt551x_dsp->buf_rp);

	return buf_size;
}

static void rt551x_spi_copy_work(struct work_struct *work)
{
	struct rt551x_dsp *rt551x_dsp =
		container_of(work, struct rt551x_dsp, copy_work.work);
	struct snd_pcm_runtime *runtime;
	size_t period_bytes, truncated_bytes = 0;
	int had_reset_read_pointer = 0;
	size_t bufsize_avaldata = 0;
#ifdef CONFIG_AMAZON_METRICS_LOG
	ktime_t Current;
#endif
	mutex_lock(&rt551x_dsp->dma_lock);

	if (!rt551x_dsp->substream) {
		dev_err(rt551x_dsp->dev, "voice_dsp No pcm substream\n");
		goto done;
	}

	if (rt551x_dsp->had_suspend) {
		goto done;
	}

	// maybe dsp is in idle mode or disable mode.
	if (!rt551x_dsp->pcm_is_readable) {
		snd_pcm_period_elapsed(rt551x_dsp->substream);
		schedule_delayed_work(&rt551x_dsp->copy_work, 0);
		goto done;
	}

	had_reset_read_pointer = atomic_xchg(&need_reset_dsp_read_pointer, 0);
	if(had_reset_read_pointer) {
	        dev_info(rt551x_dsp->dev, "voice_dsp reset read pointer for wakeword");
		rt551x_spi_read_addr(RT551X_REC_RP, &rt551x_dsp->buf_rp);
		rt551x_dsp->buf_rp = ( (rt551x_dsp->buf_rp + SPI_READ_ALIGN_BYTE - 1) & SPI_READ_ALIGN_MASK);
	}

	runtime = rt551x_dsp->substream->runtime;
	period_bytes = snd_pcm_lib_period_bytes(rt551x_dsp->substream);
	bufsize_avaldata = rt551x_get_buf_size(rt551x_dsp);
	if (bufsize_avaldata < period_bytes)
	{
		// cannot get any new data
		if(rt551x_dsp->pre_buf_wp == rt551x_dsp->buf_wp)
		{
			snd_pcm_period_elapsed(rt551x_dsp->substream);
			dev_info(rt551x_dsp->dev, "voice_dsp No new data from voice dsp codec");
		}
		rt551x_dsp->pre_buf_wp = rt551x_dsp->buf_wp;
		schedule_delayed_work(&rt551x_dsp->copy_work, msecs_to_jiffies(20));
		goto done;
	}

	rt551x_dsp->pre_buf_wp = rt551x_dsp->buf_wp;

	/* check the dma_area & offset before burst_read. */
	/* Actually this checking will never be TRUE because the read will be executed period by period, */
	/* the offset is always multiple of period. */
	if (rt551x_dsp->dma_offset + period_bytes > runtime->dma_bytes)
		rt551x_dsp->dma_offset = 0;

	if (rt551x_dsp->buf_rp + period_bytes <= rt551x_dsp->buf_limit) {
		rt551x_spi_burst_read(rt551x_dsp->buf_rp,
			runtime->dma_area + rt551x_dsp->dma_offset,
			period_bytes);

		if (rt551x_dsp->buf_rp + period_bytes == rt551x_dsp->buf_limit)
			rt551x_dsp->buf_rp = rt551x_dsp->buf_base;
		else
			rt551x_dsp->buf_rp += period_bytes;
	} else {
		truncated_bytes = rt551x_dsp->buf_limit - rt551x_dsp->buf_rp;

		rt551x_spi_burst_read(rt551x_dsp->buf_rp,
			runtime->dma_area + rt551x_dsp->dma_offset,
			truncated_bytes);

		rt551x_spi_burst_read(rt551x_dsp->buf_base,
			runtime->dma_area + rt551x_dsp->dma_offset +
			truncated_bytes, period_bytes - truncated_bytes);

		rt551x_dsp->buf_rp = rt551x_dsp->buf_base + period_bytes -
			truncated_bytes;
	}

	rt551x_dsp->dma_offset += period_bytes;
	if (rt551x_dsp->dma_offset >= runtime->dma_bytes)
		rt551x_dsp->dma_offset = 0;

	snd_pcm_period_elapsed(rt551x_dsp->substream);
	rt551x_dsp->dspStreamDuration += period_bytes / ONE_MS_DATA_LENGTH;
	if (bufsize_avaldata >= period_bytes * 2)
	{
		schedule_delayed_work(&rt551x_dsp->copy_work, 0);
	}
	else
	{
		schedule_delayed_work(&rt551x_dsp->copy_work, msecs_to_jiffies(20));
		if (rt551x_dsp->fgMetricLogPrint == false)
		{
#ifdef CONFIG_AMAZON_METRICS_LOG
			rt551x_dsp->fgMetricLogPrint = true;
			Current = ktime_get();
			if (rt551x_dsp->dspStreamDuration >= MIN_SUSPEND_AUDIO_DURATION) {
				char buf[128];
				snprintf(buf, sizeof(buf),
					"voice_dsp:def:DSP_catchup_ms=%lld;TI;1:NR",
					ktime_to_ms(Current) - ktime_to_ms(rt551x_dsp->StreamOpenTime));
				log_to_metrics(ANDROID_LOG_INFO, "voice_dsp", buf);

				log_timer_to_vitals(ANDROID_LOG_INFO, "Kernel", "Kernel",
					"RT551X_DSP_metrics_time","DSP_DATA_CATCH-UP_FINISH",
					ktime_to_ms(Current)  - ktime_to_ms(rt551x_dsp->StreamOpenTime),
					"ms", VITALS_NORMAL);

			}
			else
				log_timer_to_vitals(ANDROID_LOG_INFO, "Kernel", "Kernel",
					"RT551X_DSP_metrics_time","DSP_DATA_PROCESS_FINISH",
					ktime_to_ms(Current)  - ktime_to_ms(rt551x_dsp->StreamOpenTime),
					"ms", VITALS_NORMAL);
#endif
		}
	}
done:
	mutex_unlock(&rt551x_dsp->dma_lock);
}

/* PCM for streaming audio from the DSP buffer */
static int rt551x_spi_pcm_open(struct snd_pcm_substream *substream)
{
	snd_soc_set_runtime_hwparams(substream, &rt551x_spi_pcm_hardware);
	if (rt551x_dsp_pointer)
	{
		rt551x_dsp_pointer->fgMetricLogPrint = false;
		rt551x_dsp_pointer->StreamOpenTime = ktime_get();
		rt551x_dsp_pointer->dspStreamDuration = 0;
#ifdef CONFIG_AMAZON_METRICS_LOG
		log_counter_to_vitals(ANDROID_LOG_INFO, "Kernel", "Kernel",
			"RT551X_DSP_metrics_count","DSP_DATA_PROCESS_BEGIN", 1, "count", NULL, VITALS_NORMAL);
#endif
	}
	return 0;
}

static int rt551x_spi_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *hw_params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct rt551x_dsp *rt551x_dsp =
			snd_soc_platform_get_drvdata(rtd->platform);
	int ret;

	mutex_lock(&rt551x_dsp->dma_lock);
	ret = snd_pcm_lib_alloc_vmalloc_buffer(substream,
			params_buffer_bytes(hw_params));
	rt551x_dsp->substream = substream;
	rt551x_dsp->buf_base = 0;
	rt551x_dsp->sending_crash_event = 0;
	mutex_unlock(&rt551x_dsp->dma_lock);

	return ret;
}

static int rt551x_spi_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct rt551x_dsp *rt551x_dsp =
			snd_soc_platform_get_drvdata(rtd->platform);
#ifdef CONFIG_AMAZON_METRICS_LOG
		ktime_t Current;
#endif
	mutex_lock(&rt551x_dsp->dma_lock);
	rt551x_dsp->substream = NULL;
	rt551x_dsp->buf_base = 0;
	mutex_unlock(&rt551x_dsp->dma_lock);

	cancel_delayed_work_sync(&rt551x_dsp->copy_work);
	if (rt551x_dsp->fgMetricLogPrint == false)
	{
		rt551x_dsp->fgMetricLogPrint = true;
#ifdef CONFIG_AMAZON_METRICS_LOG
		Current = ktime_get();
		log_timer_to_vitals(ANDROID_LOG_INFO, "Kernel", "Kernel",
			"RT551X_DSP_metrics_time","DSP_DATA_CANCEL",
			ktime_to_ms(Current)  - ktime_to_ms(rt551x_dsp->StreamOpenTime),
			"ms", VITALS_NORMAL);
#endif
	}

	return snd_pcm_lib_free_vmalloc_buffer(substream);
}

static int rt551x_spi_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct rt551x_dsp *rt551x_dsp =
			snd_soc_platform_get_drvdata(rtd->platform);
	unsigned int tmp_wp = 0;

	if(rt551x_dsp->buf_base == 0) {
		mutex_lock(&rt551x_dsp->dma_lock);
		if (!rt551x_dsp->pcm_is_readable) {
			dev_err(rt551x_dsp->dev, "voice_dsp is not readable in rt551x_spi_prepare function\n");
			mutex_unlock(&rt551x_dsp->dma_lock);
			return -1;
		}
		rt551x_dsp->dma_offset = 0;
		rt551x_spi_read_addr(RT551X_BUFFER_VOICE_BASE, &rt551x_dsp->buf_base);
		rt551x_spi_read_addr(RT551X_BUFFER_VOICE_LIMIT, &rt551x_dsp->buf_limit);
		rt551x_spi_read_addr(RT551X_REC_RP, &rt551x_dsp->buf_rp);
		rt551x_spi_read_addr(RT551X_BUFFER_VOICE_WP, &tmp_wp);

		rt551x_dsp->buf_rp = ( (rt551x_dsp->buf_rp + SPI_READ_ALIGN_BYTE - 1) & SPI_READ_ALIGN_MASK );
		rt551x_dsp->buf_base = ( (rt551x_dsp->buf_base + SPI_READ_ALIGN_BYTE - 1) & SPI_READ_ALIGN_MASK );
		rt551x_dsp->buf_limit = ( (rt551x_dsp->buf_limit + SPI_READ_ALIGN_BYTE - 1) & SPI_READ_ALIGN_MASK );

		if (!is_range_valid(rt551x_dsp->buf_rp, tmp_wp,
			rt551x_dsp->buf_base, rt551x_dsp->buf_limit)) {
			send_dsp_reset_event(rt551x_dsp);
			dev_err(rt551x_dsp->dev, "voice_dsp pointer is invalid in rt551x_spi_prepare function\n");
			mutex_unlock(&rt551x_dsp->dma_lock);
			return -1;
		}

		rt551x_dsp->pre_buf_wp = 0;
		atomic_set(&need_reset_dsp_read_pointer, 0);
		mutex_unlock(&rt551x_dsp->dma_lock);
	}
	dev_info(rt551x_dsp->dev, "voice_dsp rt551x_spi_prepare rt551x_dsp->buf_rp = 0x%x, wp = 0x%x, "
		"rt551x_dsp->buf_base = 0x%x, rt551x_dsp->buf_limit = 0x%x",
		rt551x_dsp->buf_rp, tmp_wp, rt551x_dsp->buf_base,rt551x_dsp->buf_limit);

	return 0;
}

static int rt551x_spi_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct rt551x_dsp *rt551x_dsp =
			snd_soc_platform_get_drvdata(rtd->platform);

	if (cmd == SNDRV_PCM_TRIGGER_START) {
		if (rt551x_dsp->buf_base && rt551x_dsp->buf_limit)
			schedule_delayed_work(&rt551x_dsp->copy_work, 0);
	}

	return 0;
}

static snd_pcm_uframes_t rt551x_spi_pcm_pointer(
		struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct rt551x_dsp *rt551x_dsp =
		snd_soc_platform_get_drvdata(rtd->platform);

	return bytes_to_frames(runtime, rt551x_dsp->dma_offset);
}

static struct snd_pcm_ops rt551x_spi_pcm_ops = {
	.open		= rt551x_spi_pcm_open,
	.hw_params	= rt551x_spi_hw_params,
	.hw_free	= rt551x_spi_hw_free,
	.trigger	= rt551x_spi_trigger,
	.prepare	= rt551x_spi_prepare,
	.pointer	= rt551x_spi_pcm_pointer,
	.mmap		= snd_pcm_lib_mmap_vmalloc,
	.page		= snd_pcm_lib_get_vmalloc_page,
};

static int rt551x_spi_pcm_probe(struct snd_soc_platform *platform)
{
	struct rt551x_dsp *rt551x_dsp;

	rt551x_dsp = devm_kzalloc(platform->dev, sizeof(*rt551x_dsp),
			GFP_KERNEL);
	if (rt551x_dsp == NULL)
		return -ENOMEM;

	rt551x_dsp->dev = &rt551x_spi->dev;
	mutex_init(&rt551x_dsp->dma_lock);
	INIT_DELAYED_WORK(&rt551x_dsp->copy_work, rt551x_spi_copy_work);
	init_waitqueue_head(&rt551x_dsp->wait);
	snd_soc_platform_set_drvdata(platform, rt551x_dsp);
	rt551x_dsp_pointer = rt551x_dsp;
	rt551x_spi->dev.platform_data = rt551x_dsp;

	return 0;
}
#ifdef CONFIG_PM_SLEEP
static int rt551x_spi_suspend(struct device *dev)
{
	struct rt551x_dsp *rt551x_dsp = dev_get_platdata(dev);
	dev_info(rt551x_dsp->dev, "voice_dsp rt551x_spi_suspend called");
	mutex_lock(&rt551x_dsp->dma_lock);
	rt551x_dsp->had_suspend = 1;
	mutex_unlock(&rt551x_dsp->dma_lock);
	cancel_delayed_work_sync(&rt551x_dsp->copy_work);
	return 0;
}

static int rt551x_spi_resume(struct device *dev)
{
	struct rt551x_dsp *rt551x_dsp = dev_get_platdata(dev);
	dev_info(rt551x_dsp->dev, "voice_dsp rt551x_spi_resume called");
	mutex_lock(&rt551x_dsp->dma_lock);
	rt551x_dsp->had_suspend = 0;
	mutex_unlock(&rt551x_dsp->dma_lock);
	wake_up(&rt551x_dsp->wait);
	return 0;
}
#endif
static struct snd_soc_platform_driver rt551x_spi_platform = {
	.probe = rt551x_spi_pcm_probe,
	.ops = &rt551x_spi_pcm_ops,
};

static const struct snd_soc_component_driver rt551x_spi_dai_component = {
	.name		= "rt551x-spi-dai",
};

int rt551x_spi_read_addr(unsigned int addr, unsigned int *val)
{
	struct spi_device *spi = rt551x_spi;
	struct spi_message message;
	struct spi_transfer x[3];
	u8 spi_cmd = RT551X_SPI_CMD_32_READ;
	int status;
	u8 write_buf[5];
	u8 read_buf[4];

	write_buf[0] = spi_cmd;
	write_buf[1] = (addr & 0xff000000) >> 24;
	write_buf[2] = (addr & 0x00ff0000) >> 16;
	write_buf[3] = (addr & 0x0000ff00) >> 8;
	write_buf[4] = (addr & 0x000000ff) >> 0;

	spi_message_init(&message);
	memset(x, 0, sizeof(x));

	x[0].len = 5;
	x[0].tx_buf = write_buf;
	spi_message_add_tail(&x[0], &message);

	x[1].len = 4;
	x[1].tx_buf = write_buf;
	spi_message_add_tail(&x[1], &message);

	x[2].len = 4;
	x[2].rx_buf = read_buf;
	spi_message_add_tail(&x[2], &message);

	status = spi_sync(spi, &message);

	*val = read_buf[3] | read_buf[2] << 8 | read_buf[1] << 16 |
		read_buf[0] << 24;

	return status;
}

int rt551x_spi_write_addr(unsigned int addr, unsigned int val)
{
	struct spi_device *spi = rt551x_spi;
	u8 spi_cmd = RT551X_SPI_CMD_32_WRITE;
	int status;
	u8 write_buf[10];

	write_buf[0] = spi_cmd;
	write_buf[1] = (addr & 0xff000000) >> 24;
	write_buf[2] = (addr & 0x00ff0000) >> 16;
	write_buf[3] = (addr & 0x0000ff00) >> 8;
	write_buf[4] = (addr & 0x000000ff) >> 0;
	write_buf[5] = (val & 0xff000000) >> 24;
	write_buf[6] = (val & 0x00ff0000) >> 16;
	write_buf[7] = (val & 0x0000ff00) >> 8;
	write_buf[8] = (val & 0x000000ff) >> 0;
	write_buf[9] = spi_cmd;

	status = spi_write(spi, write_buf, sizeof(write_buf));

	if (status)
		dev_err(&spi->dev, "%s error %d\n", __func__, status);

	return status;
}

/**
 * rt551x_spi_burst_read - Read data from SPI by rt551x address.
 * @addr: Start address.
 * @rxbuf: Data Buffer for reading.
 * @len: Data length, it must be a multiple of 8.
 *
 *
 * Returns true for success.
 */
int rt551x_spi_burst_read(unsigned int addr, u8 *rxbuf, size_t len)
{
	u8 spi_cmd = RT551X_SPI_CMD_BURST_READ;
	int status;
	u8 write_buf[8];
	unsigned int i, end, offset = 0;

	struct spi_message message;
	struct spi_transfer x[3];

	while (offset < len) {
		if (offset + RT551X_SPI_BUF_LEN <= len)
			end = RT551X_SPI_BUF_LEN;
		else
			end = len % RT551X_SPI_BUF_LEN;

		write_buf[0] = spi_cmd;
		write_buf[1] = ((addr + offset) & 0xff000000) >> 24;
		write_buf[2] = ((addr + offset) & 0x00ff0000) >> 16;
		write_buf[3] = ((addr + offset) & 0x0000ff00) >> 8;
		write_buf[4] = ((addr + offset) & 0x000000ff) >> 0;

		spi_message_init(&message);
		memset(x, 0, sizeof(x));

		x[0].len = 5;
		x[0].tx_buf = write_buf;
		spi_message_add_tail(&x[0], &message);

		x[1].len = 4;
		x[1].tx_buf = write_buf;
		spi_message_add_tail(&x[1], &message);

		x[2].len = end;
		x[2].rx_buf = rxbuf + offset;
		spi_message_add_tail(&x[2], &message);

		status = spi_sync(rt551x_spi, &message);

		if (status)
			return false;

		offset += RT551X_SPI_BUF_LEN;
	}

	for (i = 0; i < len; i += 8) {
		write_buf[0] = rxbuf[i + 0];
		write_buf[1] = rxbuf[i + 1];
		write_buf[2] = rxbuf[i + 2];
		write_buf[3] = rxbuf[i + 3];
		write_buf[4] = rxbuf[i + 4];
		write_buf[5] = rxbuf[i + 5];
		write_buf[6] = rxbuf[i + 6];
		write_buf[7] = rxbuf[i + 7];

		rxbuf[i + 0] = write_buf[7];
		rxbuf[i + 1] = write_buf[6];
		rxbuf[i + 2] = write_buf[5];
		rxbuf[i + 3] = write_buf[4];
		rxbuf[i + 4] = write_buf[3];
		rxbuf[i + 5] = write_buf[2];
		rxbuf[i + 6] = write_buf[1];
		rxbuf[i + 7] = write_buf[0];
	}

	return true;
}

/**
 * rt551x_spi_burst_write - Write data to SPI by rt551x address.
 * @addr: Start address.
 * @txbuf: Data Buffer for writng.
 * @len: Data length, it must be a multiple of 8.
 *
 *
 * Returns true for success.
 */
int rt551x_spi_burst_write(u32 addr, const u8 *txbuf, size_t len)
{
	u8 spi_cmd = RT551X_SPI_CMD_BURST_WRITE;
	u8 *write_buf;
	unsigned int i, end, offset = 0;

	write_buf = kmalloc(RT551X_SPI_BUF_LEN + 6, GFP_KERNEL);

	if (write_buf == NULL)
		return -ENOMEM;

	while (offset < len) {
		if (offset + RT551X_SPI_BUF_LEN <= len)
			end = RT551X_SPI_BUF_LEN;
		else
			end = len % RT551X_SPI_BUF_LEN;

		write_buf[0] = spi_cmd;
		write_buf[1] = ((addr + offset) & 0xff000000) >> 24;
		write_buf[2] = ((addr + offset) & 0x00ff0000) >> 16;
		write_buf[3] = ((addr + offset) & 0x0000ff00) >> 8;
		write_buf[4] = ((addr + offset) & 0x000000ff) >> 0;

		for (i = 0; i < end; i += 8) {
			write_buf[i + 12] = txbuf[offset + i + 0];
			write_buf[i + 11] = txbuf[offset + i + 1];
			write_buf[i + 10] = txbuf[offset + i + 2];
			write_buf[i +  9] = txbuf[offset + i + 3];
			write_buf[i +  8] = txbuf[offset + i + 4];
			write_buf[i +  7] = txbuf[offset + i + 5];
			write_buf[i +  6] = txbuf[offset + i + 6];
			write_buf[i +  5] = txbuf[offset + i + 7];
		}

		write_buf[end + 5] = spi_cmd;

		spi_write(rt551x_spi, write_buf, end + 6);

		offset += RT551X_SPI_BUF_LEN;
	}

	kfree(write_buf);

	return 0;
}

static int rt551x_spi_probe(struct spi_device *spi)
{
	int ret;

	rt551x_spi = spi;

	ret = snd_soc_register_platform(&spi->dev, &rt551x_spi_platform);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to register platform.\n");
		goto err_plat;
	}

	ret = snd_soc_register_component(&spi->dev, &rt551x_spi_dai_component,
					 &rt551x_spi_dai, 1);
	if (ret < 0) {
		dev_err(&spi->dev, "Failed to register component.\n");
		goto err_comp;
	}

	return 0;
err_comp:
	snd_soc_unregister_platform(&spi->dev);
err_plat:

	return ret;
}

static int rt551x_spi_remove(struct spi_device *spi)
{
	snd_soc_unregister_component(&spi->dev);
	snd_soc_unregister_platform(&spi->dev);

	return 0;
}

static const struct of_device_id rt551x_of_match[] = {
	{ .compatible = "realtek,rt551x", },
	{},
};
MODULE_DEVICE_TABLE(of, rt551x_of_match);
static SIMPLE_DEV_PM_OPS(rt551x_spi_pm_ops, rt551x_spi_suspend, rt551x_spi_resume);
static struct spi_driver rt551x_spi_driver = {
	.driver = {
		.name = "rt551x",
		.of_match_table = of_match_ptr(rt551x_of_match),
		.pm = &rt551x_spi_pm_ops,
	},
	.probe = rt551x_spi_probe,
	.remove = rt551x_spi_remove,
};
module_spi_driver(rt551x_spi_driver);

MODULE_DESCRIPTION("ALC551X SPI driver");
MODULE_AUTHOR("Oder Chiou <oder_chiou@realtek.com>");
MODULE_LICENSE("GPL v2");
