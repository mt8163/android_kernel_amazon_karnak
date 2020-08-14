 /*
 * ALSA PCM platform driver to capture audio through SPI Interface
 *
 * Author: Amazon Lab126 2016
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/spi/spi.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/ktime.h>
#include <linux/spinlock.h>
#include <linux/thread_info.h>
#include <linux/cpumask.h>
#include <linux/regulator/consumer.h>
#include <linux/of_device.h>

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#define OVERRUN_METRICS_STR_LEN 128
/* 16ms */
#define METRIC_DELAY_JIFFIES msecs_to_jiffies(16)
#endif

#include "dough.h"
#include "amzn-mt-spi-pcm.h"
#include "../AudDrv_Gpio.h"
#include "../mt_soc_pcm_common.h"
#include "../mt_soc_digital_type.h"

/* Debugging Purpose:: Use this mode to enable FPGA Test Pattern.
 * Keep it off for audio
 * #define FPGA_TEST_PATTERN_ENABLE
*/
/* Debugging Purpose:: Enable Dummy codec to test with ADC scripts
 * #define SPI_DUMMY_CODEC
*/
/* Debugging Purpose:: Enable for printing FPGA payload
 * #define PRINT_FPGA_PAYLOAD
 *
 * Debugging Purpose:: Enable buffer pointer prints
 * #define SPI_DATA_DEBUG
*/

/* To allocate audio transfer buffer locally
#define SPI_USES_LOCAL_DMA
*/

/* To utilize HDMI Buffer for audio transfer
 * This should also be enabled in mt-soc-machine.c */
#define SPI_USES_HDMI_BUFFER

#define SPI_SETUP_BUF_SIZE 32
#define FPGA_VCC_DELAY_MS 10
/* Module data structure */
struct amzn_spi_priv {
	struct workqueue_struct *spi_wq;
	struct work_struct spi_work;
#ifdef CONFIG_AMAZON_METRICS_LOG
	struct workqueue_struct *metrics_wq;
	struct delayed_work metrics_kernel_work;
	struct delayed_work metrics_fpga_work;
	size_t kernel_overruns;
	size_t fpga_overruns;
#endif
	struct snd_pcm_substream *substream;
	uint8_t *dma_vaddr;
	dma_addr_t dma_paddr;
	spinlock_t thread_spinlock;
	spinlock_t write_spinlock;
	size_t cur_write_offset;
	size_t elapsed;
	bool run_thread;
	bool keep_copying;
	uint32_t min_spi_wait_usec;
	uint32_t max_spi_wait_usec;
#if defined SPI_USES_LOCAL_DMA || defined SPI_USES_HDMI_BUFFER
	struct snd_dma_buffer *capture_dma_buf;
#endif
};

static void spi_data_read(struct work_struct *work);

/* Disable timestamp transfer by default */
static int transfer_timestamps_enab = SPI_HEADER_DISABLE;

/* TODO(DEE-30199): Remove global decalartaion */
static struct amzn_spi_priv spi_data;

static const char * const spi_functions[] = { "Off", "On"};

static const struct soc_enum spi_functions_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spi_functions), spi_functions),
};

static int transfer_timestamps_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s: = %d\n", __func__, transfer_timestamps_enab);
	ucontrol->value.integer.value[0] = transfer_timestamps_enab;
	return 0;
}

static int transfer_timestamps_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s: transfer_timestamps_enab=%d\n", __func__,
			transfer_timestamps_enab);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(spi_functions)) {
		pr_err("%s: Invalid input=%d\n", __func__,
			ucontrol->value.enumerated.item[0]);
		return -EINVAL;
	}

	/* Don't set this if capture is active */
	transfer_timestamps_enab = ucontrol->value.integer.value[0];

	return 0;
}

static const struct snd_kcontrol_new amzn_mt_spi_controls[] = {

	SOC_ENUM_EXT("SpiTimeStamps", spi_functions_Enum[0],
			transfer_timestamps_get, transfer_timestamps_set),
};

static struct snd_pcm_hardware amzn_mt_spi_pcm_hardware = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED),
	.formats = SNDRV_PCM_FMTBIT_S24_3LE,
	.rates = SNDRV_PCM_RATE_16000,
	.rate_min = SAMPLING_RATE,
	.rate_max = SAMPLING_RATE,
	.channels_min = SPI_N_CHANNELS,
	.channels_max = SPI_N_CHANNELS,
	.buffer_bytes_max = SPI_BUFFER_BYTES_MAX,
	.period_bytes_max = SPI_PERIOD_BYTES_MAX,
	.period_bytes_min = SPI_PERIOD_BYTES_MIN,
	.periods_min = SPI_N_PERIODS_MIN,
	.periods_max = SPI_N_PERIODS_MAX,
	.fifo_size = 0,
};

static int amzn_mt_spi_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	pr_info("%s = %x\n", __func__, fmt);
	return 0;
}

static const struct snd_soc_dai_ops amzn_mt_spi_dai_ops = {
	.startup = NULL,
	.trigger = NULL,
	.set_fmt = amzn_mt_spi_dai_fmt,
	.hw_params = NULL,
};

static struct snd_soc_dai_driver amzn_mt_spi_cpu_dai[] = {
	{
		.name = AMZN_MT_SPI_PCM,
		.capture = {
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S24_3LE,
			.channels_min = SPI_N_CHANNELS,
			.channels_max = SPI_N_CHANNELS,
		},
		.ops = NULL,
	},
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {

	.count = ARRAY_SIZE(soc_normal_supported_sample_rates),
	.list = soc_normal_supported_sample_rates,
};

#ifdef CONFIG_AMAZON_METRICS_LOG
static void kernel_overrun_metrics(struct work_struct *work)
{
	char buf[OVERRUN_METRICS_STR_LEN];

	/* No need of logging 0 overruns */
	if (spi_data.kernel_overruns) {
		snprintf(buf, OVERRUN_METRICS_STR_LEN,
			"amzn-mt-spi-pcm:KernelOverrun:num=%zu;CT;1:NR",
			 spi_data.kernel_overruns);

		log_to_metrics(ANDROID_LOG_WARN, "AudioRecordOverrun", buf);
		spi_data.kernel_overruns = 0;
	}
}

static void fpga_overrun_metrics(struct work_struct *work)
{
	char buf[OVERRUN_METRICS_STR_LEN];

	/* No need of logging 0 overruns */
	if (spi_data.fpga_overruns) {
		snprintf(buf, OVERRUN_METRICS_STR_LEN,
			"amzn-mt-spi-pcm:FPGAOverrun:num=%zu;CT;1:NR",
			 spi_data.fpga_overruns);

		log_to_metrics(ANDROID_LOG_WARN, "AudioRecordOverrun", buf);
		spi_data.fpga_overruns = 0;
	}
}
#endif

/*
 * ASoC Platform driver
 */
static int amzn_mt_spi_pcm_hw_params(struct snd_pcm_substream *ss,
				struct snd_pcm_hw_params *hw_params)
{

	int ret = 0, rate;
	struct snd_pcm_runtime *runtime = ss->runtime;

	pr_info("%s: Allocating buffer_size = %d\n", __func__,
		params_buffer_bytes(hw_params));

	if (spi_data.dma_vaddr == NULL) {
#if defined SPI_USES_HDMI_BUFFER || defined SPI_USES_LOCAL_DMA
		runtime->dma_bytes = params_buffer_bytes(hw_params);
		runtime->dma_area = spi_data.capture_dma_buf->area;
		runtime->dma_addr = spi_data.capture_dma_buf->addr;
#else
		ret = snd_pcm_lib_malloc_pages(ss,
			params_buffer_bytes(hw_params));
		if (ret < 0) {
			pr_err("%s: snd_pcm_lib_malloc_pages failed: ret=%d\n",
				__func__, ret);
			return ret;
		}
#endif
		spi_data.dma_vaddr = runtime->dma_area;
		spi_data.dma_paddr = runtime->dma_addr;
	}

	rate = params_rate(hw_params);
	switch (rate) {
	case 48000:
			spi_data.min_spi_wait_usec = SPI_READ_WAIT_MIN_48K_USEC;
			spi_data.max_spi_wait_usec = SPI_READ_WAIT_MAX_48K_USEC;
			break;

	case 96000:
			spi_data.min_spi_wait_usec = SPI_READ_WAIT_MIN_96K_USEC;
			spi_data.max_spi_wait_usec = SPI_READ_WAIT_MAX_96K_USEC;
			break;

	default:
			spi_data.min_spi_wait_usec = SPI_READ_WAIT_MIN_USEC;
			spi_data.max_spi_wait_usec = SPI_READ_WAIT_MAX_USEC;
	}

	pr_info("%s: alloc dma_area=%p, dma_addr=%p, size=%zu, ret %d\n",
			 __func__, runtime->dma_area, (void *)runtime->dma_addr,
			runtime->dma_bytes, ret);

	pr_info("%s: frame_bits=%u period_size=%lu periods=%u buffer_size=%lu\n",
			__func__, runtime->frame_bits, runtime->period_size,
			runtime->periods, runtime->buffer_size);

	return ret;
}

static int amzn_mt_spi_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pr_info("%s\n", __func__);

#if !defined SPI_USES_HDMI_BUFFER && !defined SPI_USES_LOCAL_DMA
	/* Preallocate pages */
	ret = snd_pcm_lib_preallocate_pages_for_all(rtd->pcm,
			SNDRV_DMA_TYPE_DEV, rtd->card->dev,
			SPI_DMA_BYTES_MAX, SPI_DMA_BYTES_MAX);
	if (ret < 0)
		pr_err("%s: Preallocated pages failed\n", __func__);
#endif

	return ret;
}

static int amzn_mt_spi_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret;

	pr_info("%s\n", __func__);

	if (substream->stream != SNDRV_PCM_STREAM_CAPTURE) {
		pr_err("%s:Platform audio driver only for capture\n", __func__);
		return -EINVAL;
	}

	runtime->hw = amzn_mt_spi_pcm_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&amzn_mt_spi_pcm_hardware,
		sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime,
					SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		pr_warn("%s: snd_pcm_hw_constraint_integer failed = %d\n",
			__func__, ret);

	spi_data.spi_wq = alloc_workqueue("amznspi",
				WQ_HIGHPRI | WQ_MEM_RECLAIM,
				MAX_SCHEDULED_WORK_Q);
	if (spi_data.spi_wq == NULL) {
		pr_err("%s:Couldn't create_singlethread_workqueue\n", __func__);
		return -ENOMEM;
	}

#ifdef CONFIG_AMAZON_METRICS_LOG
	spi_data.metrics_wq = alloc_workqueue("audio_overrun_metrics",
					WQ_MEM_RECLAIM, 0);
	if (spi_data.metrics_wq == NULL) {
		pr_err("%s:Couldn't alloc_workqueue\n", __func__);
		destroy_workqueue(spi_data.spi_wq);
		spi_data.spi_wq = NULL;
		return -ENOMEM;
	}
#endif

	spi_data.cur_write_offset = 0;
	spi_data.elapsed = 0;

	spin_lock_init(&(spi_data.thread_spinlock));
	spin_lock_init(&(spi_data.write_spinlock));
	spi_data.substream = substream;
	INIT_WORK(&(spi_data.spi_work), spi_data_read);
#ifdef CONFIG_AMAZON_METRICS_LOG
	INIT_DELAYED_WORK(&(spi_data.metrics_kernel_work),
		kernel_overrun_metrics);
	INIT_DELAYED_WORK(&(spi_data.metrics_fpga_work),
		fpga_overrun_metrics);
#endif
	return 0;
}

static int amzn_mt_spi_pcm_close(struct snd_pcm_substream *substream)
{
	pr_info("%s: Destroy\n", __func__);
	if (spi_data.spi_wq) {
		destroy_workqueue(spi_data.spi_wq);
		spi_data.spi_wq = NULL;
	}

#ifdef CONFIG_AMAZON_METRICS_LOG
	if (spi_data.metrics_wq) {
		destroy_workqueue(spi_data.metrics_wq);
		spi_data.metrics_wq = NULL;
	}
#endif

	pr_info("%s: End\n", __func__);

	return 0;
}

static int amzn_mt_spi_pcm_hw_free(struct snd_pcm_substream *ss)
{
	pr_info("%s: Free buffer = %lu, dma_bytes %zu dma_addr %p\n",
		__func__, ss->runtime->buffer_size, ss->runtime->dma_bytes,
		(void *)ss->runtime->dma_addr);

	flush_workqueue(spi_data.spi_wq);

#ifdef CONFIG_AMAZON_METRICS_LOG
	flush_workqueue(spi_data.metrics_wq);
#endif

	if (spi_data.dma_vaddr) {
#if !defined SPI_USES_HDMI_BUFFER && !defined SPI_USES_LOCAL_DMA
		snd_pcm_lib_free_pages(ss);
#endif
		spi_data.dma_vaddr = NULL;
	}

	return 0;
}

static int amzn_mt_spi_pcm_prepare(struct snd_pcm_substream *substream)
{
	pr_info("%s\n", __func__);
	return 0;
}

#ifdef PRINT_FPGA_PAYLOAD
static void print_raw24(uint8_t *buf, size_t len)
{
	size_t i, c, j;

	if (0 != len % DOUGH_AUDIO_FRAME_BYTES) {
		pr_err("%s: Buffer must be a multiple of frame size(%u)\n",
			__func__, DOUGH_AUDIO_FRAME_BYTES);
		return;
	}

	for (i = 0, c = 0; i < len; i += DOUGH_AUDIO_SAMPLE_WIDTH, c++) {
		if (c % DOUGH_AUDIO_NUM_CHANNELS == 0)
			pr_info("\n");

		pr_info("%04zu: 0x", c);
		for (j = DOUGH_AUDIO_SAMPLE_WIDTH-1; j >= 0; j--)
			pr_info("%02x", buf[i+j]);

		pr_info("    ");
	}
	pr_info("\n");
}
#endif

static int spi_txrx(struct spi_device *spi, void *txb, void *rxb, int len,
			int dma_enable, dma_addr_t paddr)
{
	struct spi_message msg = {};
	struct spi_transfer xfer = {};

	spi_message_init(&msg);
	xfer.tx_buf = txb;
	xfer.rx_buf = rxb;
	xfer.len = len;
	xfer.bits_per_word = 8;
	xfer.speed_hz = SPI_SPEED_HZ;

	if (dma_enable) {
		msg.is_dma_mapped = 1;
		xfer.rx_dma = paddr;
	}

	spi_message_add_tail(&xfer, &msg);
	return spi_sync_locked(spi, &msg);
}

static bool verify_fpga_frm_ver(uint8_t ver)
{
	if (ver < DOUGH_FPGA_REV_MIN || ver > DOUGH_FPGA_REV_MAX) {
		pr_err("%s: Unrecognized FPGA rev: %u\n", __func__, ver);
		return false;
	}

	pr_debug("%s: Good FPGA rev: %u\n", __func__, ver);
	return true;
}

static unsigned long ktime_diff(ktime_t *lhs, ktime_t *rhs)
{
	ktime_t diff_ktime;
	long diff_usec;

	if (ktime_compare(*lhs, *rhs) < 0) {
		diff_ktime = ktime_sub(*rhs, *lhs);
		diff_usec = ktime_to_us(diff_ktime);
		pr_info("%s: Timestamp Error lhs=%lld rhs=%lld diff=%ld\n",
			__func__, lhs->tv64, rhs->tv64, diff_usec);
		/* Return enough delay so thread wont sleep */
		return spi_data.min_spi_wait_usec;
	}

	diff_ktime = ktime_sub(*lhs, *rhs);
	return ktime_to_us(diff_ktime);
}

static void set_run_thread(bool run)
{
	unsigned long flags;

	spin_lock_irqsave(&(spi_data.thread_spinlock), flags);
	spi_data.run_thread = run;
	spin_unlock_irqrestore(&(spi_data.thread_spinlock), flags);
}

static bool get_run_thread(void)
{
	bool value;
	unsigned long flags;

	spin_lock_irqsave(&(spi_data.thread_spinlock), flags);
	value = spi_data.run_thread;
	spin_unlock_irqrestore(&(spi_data.thread_spinlock), flags);
	return value;
}

static void spi_data_read(struct work_struct *work)
{
	ktime_t cur_ktime, prev_ktime, sleep_ktime;
	unsigned long min_sleep_usec = 0, max_sleep_usec = 0;
	unsigned long time_diff_usec = 0, overrun_duration, slept_duration,
			spi_duration, irq_flags;
	unsigned long wakeup_maxlat = 0, wakeup_minlat = ULONG_MAX;
	/* Get SPI device from substream */
	struct amzn_spi_priv *spi_priv_data = container_of(work,
					struct amzn_spi_priv, spi_work);
	struct snd_pcm_substream *ss = spi_priv_data->substream;
	struct snd_soc_pcm_runtime *soc_runtime = ss->private_data;
	struct spi_device *spi = to_spi_device(soc_runtime->platform->dev);
	struct dough_frame *tx_df = 0, *rx_df = 0;
	void *dst_ptr, *src_ptr;
	size_t n_bytes, bytes, copied, elapsed_threshold;
	struct task_struct *kworker_task;
	struct thread_info *kworker_info;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 2 };
	int ret = 0, iter_count = 0;
	uint32_t prev_fpga_ts = 0;

	pr_info("%s\n", __func__);
	tx_df = kzalloc(sizeof(struct dough_frame), GFP_KERNEL | GFP_DMA);
#ifdef SPI_USES_LOCAL_DMA
	rx_df = (struct dough_frame *)spi_data.dma_vaddr;
#else
	rx_df = kzalloc(sizeof(struct dough_frame), GFP_KERNEL | GFP_DMA);
	if (!tx_df || !rx_df) {
		pr_err("%s: Failed to allocate spi buffer\n", __func__);
		return;
	}
#endif
	spi_priv_data->elapsed = 0;
	spi_priv_data->cur_write_offset = 0;
	spi_priv_data->keep_copying = true;
	elapsed_threshold = SPI_BYTES_PER_PERIOD;
	spi_priv_data->fpga_overruns = 0;
	spi_priv_data->kernel_overruns = 0;
	set_run_thread(true);
	kworker_info = current_thread_info();
	if (kworker_info == NULL) {
		pr_err("%s: Failed to get thread info\n", __func__);
		goto fail;
	}
	kworker_task = kworker_info->task;
	if (kworker_task == NULL) {
		pr_err("%s: Failed to get current thread\n", __func__);
		goto fail;
	}
	sched_setscheduler(kworker_task, SCHED_FIFO, &param);

	cur_ktime = ktime_get_raw();
	/* Initialize with the same value */
	prev_ktime = cur_ktime;

	while (get_run_thread()) {
#ifdef SPI_USES_LOCAL_DMA
		ret = spi_txrx(spi, (void *)tx_df, spi_data.dma_vaddr,
			sizeof(struct dough_frame), 1, spi_data.dma_paddr);
		rx_df = (struct dough_frame *)spi_data.dma_vaddr;
#else
		ret = spi_txrx(spi, (void *)tx_df, (void *) rx_df,
				sizeof(struct dough_frame), 0, 0);
#endif
		if (ret < 0) {
			pr_err("%s: Failed to rx SPI audio\n", __func__);
			goto fail;
		}
		if (!verify_fpga_frm_ver(rx_df->dsf.fpga_rev)) {
			pr_debug("%s: ts=%u trx_usec=%lu\n", __func__,
				rx_df->dsf.timestamp_48mhz, time_diff_usec);
			goto delay;
		}
#ifdef SPI_DATA_DEBUG
		pr_info("%s: fpga_rev=%d overrun=%d mode=%d dacOff=%d adcOff=%d frames=%d Wroff=%lu ts=%u, trx_usec=%lu\n",
			__func__, rx_df->dsf.fpga_rev, rx_df->dsf.overrun,
			rx_df->dsf.mode, rx_df->dsf.dac_inactive,
			rx_df->dsf.i2s_inactive, rx_df->dsf.num_audio_frames,
			spi_priv_data->cur_write_offset,
			rx_df->dsf.timestamp_48mhz, time_diff_usec);
#endif
#ifdef PRINT_FPGA_PAYLOAD
		print_raw24((uint8_t *)rx_df, sizeof(struct dough_frame));
#endif

		if (rx_df->dsf.overrun == 1) {
			/* Ignore overun and skip data for first few frames */
			if (iter_count < MAX_FLUSHED_CYCLES)
				goto delay;

			/* Save time before thread slept */
			sleep_ktime = cur_ktime;
			cur_ktime = ktime_get_raw();
			/* Time passed since previous spi transaction */
			overrun_duration = ktime_diff(&cur_ktime, &sleep_ktime);
			/* Time spent in last sleep */
			slept_duration = ktime_diff(&prev_ktime, &sleep_ktime);
			/* Time spent in last spi transaction */
			spi_duration =  ktime_diff(&cur_ktime, &prev_ktime);
			pr_err("%s: FPGA_OVERRUN mode=%d dacOff=%d adcOff=%d frames=%d Wroff=%zu new_ts=%u diff_ts=%u prev_cycle_us=%lu curr_cycle_us=%lu slept_us=%lu spi_trx_us=%lu overruns=%zu\n",
				__func__, rx_df->dsf.mode,
				rx_df->dsf.dac_inactive,
				rx_df->dsf.i2s_inactive,
				rx_df->dsf.num_audio_frames,
				spi_priv_data->cur_write_offset,
				rx_df->dsf.timestamp_48mhz,
				(rx_df->dsf.timestamp_48mhz - prev_fpga_ts),
				time_diff_usec, overrun_duration,
				slept_duration, spi_duration,
				++spi_priv_data->fpga_overruns);
#ifdef CONFIG_AMAZON_METRICS_LOG
			 /* All overruns occurring within METRIC_DELAY_JIFFIES
			  * will log once since this doesn't queue same work
			  */
			 queue_delayed_work(spi_priv_data->metrics_wq,
			    &(spi_priv_data->metrics_fpga_work),
				METRIC_DELAY_JIFFIES);
#endif
		}

		if (!get_run_thread()) {
			pr_err("%s: Exiting Thread\n", __func__);
			break;
		}

		if (rx_df->dsf.num_audio_frames > DOUGH_AUDIO_FRAME_BUF) {
			pr_err("%s: FPGA_FRAMES are not correct %d\n", __func__,
				rx_df->dsf.num_audio_frames);
			goto delay;
		}

		prev_fpga_ts = rx_df->dsf.timestamp_48mhz;
		if (transfer_timestamps_enab) {
			n_bytes = (rx_df->dsf.num_audio_frames+1) *
					SPI_BYTES_PER_FRAME;
			src_ptr = rx_df;
		} else {
			n_bytes = rx_df->dsf.num_audio_frames *
					SPI_BYTES_PER_FRAME;
			src_ptr = rx_df->daf;
		}
		copied = 0;

		while (n_bytes > 0) {
			bytes = min((ss->runtime->dma_bytes -
				spi_priv_data->cur_write_offset), n_bytes);
#ifdef SPI_DATA_DEBUG
			if (bytes % SPI_BYTES_PER_FRAME)
				pr_err("%s: bytes calculation invalid\n",
				__func__);
#endif
			dst_ptr = ss->runtime->dma_area +
				spi_priv_data->cur_write_offset;
			src_ptr += copied;

			memcpy(dst_ptr, src_ptr, bytes);

			/* Only need to protect value against read in copy */
			spin_lock_irqsave(&(spi_priv_data->write_spinlock),
				irq_flags);
			spi_priv_data->cur_write_offset =
				(spi_priv_data->cur_write_offset + bytes) %
				ss->runtime->dma_bytes;
			spin_unlock_irqrestore(&(spi_priv_data->write_spinlock),
				irq_flags);
			n_bytes -= bytes;
			copied += bytes;
		}

		spi_priv_data->elapsed += copied;
		if (spi_priv_data->elapsed >= elapsed_threshold) {
#ifdef SPI_DATA_DEBUG
			pr_info("%s: ELAPSED=%lu threshold=%lu WrOff=%lu\n",
				__func__, spi_priv_data->elapsed,
				elapsed_threshold,
				spi_priv_data->cur_write_offset);
#endif
			elapsed_threshold += SPI_BYTES_PER_PERIOD;
			if (elapsed_threshold > ss->runtime->dma_bytes) {
				elapsed_threshold = SPI_BYTES_PER_PERIOD;
				spi_priv_data->elapsed -=
						ss->runtime->dma_bytes;
#ifdef SPI_DATA_DEBUG
				pr_info("%s: THRESHOLD=%lu elapsed=%lu\n",
					__func__, elapsed_threshold,
					spi_priv_data->elapsed);
#endif
			}
			snd_pcm_period_elapsed(ss);
		}

delay:
		if (iter_count < MAX_FLUSHED_CYCLES)
			iter_count++;

		/* Sleep and measure wakeup latency */
		cur_ktime = ktime_get_raw();

		/* Calculate time spent since last iteration */
		time_diff_usec = ktime_diff(&cur_ktime, &prev_ktime);
		/* Sleep if iteration completed in less time and no overun */
		if (time_diff_usec < (spi_data.min_spi_wait_usec - MARGIN_USEC) &&
			rx_df->dsf.overrun == 0) {
			min_sleep_usec = spi_data.min_spi_wait_usec -
					time_diff_usec;
			max_sleep_usec = spi_data.max_spi_wait_usec -
					time_diff_usec;
			usleep_range(min_sleep_usec, max_sleep_usec);
			prev_ktime = ktime_get_raw();
		} else {
			/* Thread didn't sleep, just use last ktime */
			prev_ktime = cur_ktime;
		}

		if (time_diff_usec < wakeup_minlat)
			wakeup_minlat = time_diff_usec;
		if (time_diff_usec > wakeup_maxlat)
			wakeup_maxlat = time_diff_usec;
	}

fail:
#ifndef SPI_USES_LOCAL_DMA
	kfree(rx_df);
#endif
	kfree(tx_df);

	pr_info("%s: wakeup_minlat=%lu, wakeup_maxlat=%lu\n", __func__,
		wakeup_minlat, wakeup_maxlat);
}

static int amzn_mt_spi_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_info("%s: cmd=%d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
#ifdef CONFIG_MTK_SPI_AFFINITY
		queue_work_on(1, spi_data.spi_wq, &(spi_data.spi_work));
#else
		queue_work(spi_data.spi_wq, &(spi_data.spi_work));
#endif
		break;

	case SNDRV_PCM_TRIGGER_STOP:
		set_run_thread(false);
		break;

	default:
		pr_err("%s: cmd=%x not supported\n", __func__, cmd);
	}
	return 0;
}

static snd_pcm_uframes_t
amzn_mt_spi_pcm_pointer(struct snd_pcm_substream *substream)
{
	snd_pcm_uframes_t frames;
	unsigned long flags;

	spin_lock_irqsave(&(spi_data.write_spinlock), flags);
	frames = bytes_to_frames(substream->runtime,
			spi_data.cur_write_offset);
	/* Return value should be within buffer_size-1 frames */
	if (frames == substream->runtime->dma_bytes)
		frames--;
	spin_unlock_irqrestore(&(spi_data.write_spinlock), flags);

#ifdef SPI_DATA_DEBUG
	pr_info("%s: write_offset = %lu available_frames %lu\n",
		__func__, spi_data.cur_write_offset,
		frames);
#endif
	return frames;
}

static int amzn_mt_spi_pcm_silence(struct snd_pcm_substream *substream,
				int channel, snd_pcm_uframes_t pos,
				snd_pcm_uframes_t count)
{
	pr_info("%s\n", __func__);
	return 0;
}

static int amzn_mt_spi_pcm_copy(struct snd_pcm_substream *substream,
				int channel, snd_pcm_uframes_t pos,
				void __user *dst, snd_pcm_uframes_t count)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	void *src;
	size_t src_offset, bytes_to_cpy, end;

	src_offset = frames_to_bytes(runtime, pos);
	src = runtime->dma_area + src_offset;

	bytes_to_cpy = frames_to_bytes(runtime, count);
	end = src_offset + bytes_to_cpy;

	if ((spi_data.cur_write_offset < end) &&
	(spi_data.cur_write_offset > src_offset)) {
		pr_err("%s: KERNEL_OVERRUN: pos=%lu count=%lu elasped=%zu src_offset(%zu)+bytes_to_cpy(%zu)= end(%zu) while: WrOff=%zu overruns=%zu\n",
			__func__, pos, count, spi_data.elapsed,
			src_offset, bytes_to_cpy, end,
			spi_data.cur_write_offset, ++spi_data.kernel_overruns);
#ifdef CONFIG_AMAZON_METRICS_LOG
		/* All overruns occurring within METRIC_DELAY_JIFFIES will
		* log once since queue_delayed_work() doesn't queue same work
		*/
		queue_delayed_work(spi_data.metrics_wq,
			&(spi_data.metrics_kernel_work), METRIC_DELAY_JIFFIES);
#endif
	}
#ifdef SPI_DATA_DEBUG
	else {
		pr_info("%s:COPY: Pos=%lu Count=%lu Srcoff=%lu cpy=%lu WrOff=%lu\n",
			__func__, pos, count, src_offset, bytes_to_cpy,
			spi_data.cur_write_offset);
	}
#endif
	if (copy_to_user(dst, src, bytes_to_cpy)) {
		pr_err("%s: copy_to_user failed\n", __func__);
		return -EFAULT;
	}

	return 0;
}


static int amzn_asoc_capt_probe(struct snd_soc_platform *platform)
{
	int error;

	pr_info("%s\n", __func__);

	error = snd_soc_add_platform_controls(platform, amzn_mt_spi_controls,
					ARRAY_SIZE(amzn_mt_spi_controls));
	if (error) {
		pr_err("%s: failed to add %u controls\n", __func__,
				ARRAY_SIZE(amzn_mt_spi_controls));
		return error;
	}

	of_dma_configure(platform->dev, platform->dev->of_node);


	platform->dev->coherent_dma_mask = DMA_BIT_MASK(64);
	if (!platform->dev->dma_mask)
		platform->dev->dma_mask = &platform->dev->coherent_dma_mask;

#if defined SPI_USES_HDMI_BUFFER
	AudDrv_Allocate_mem_Buffer(platform->dev,
			Soc_Aud_Digital_Block_MEM_HDMI, SPI_DMA_BYTES_MAX);
	spi_data.capture_dma_buf =
			Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_HDMI);

#elif defined SPI_USES_LOCAL_DMA
	spi_data.capture_dma_buf = kmalloc(sizeof(struct snd_dma_buffer),
					GFP_KERNEL);
	memset((void *)spi_data.capture_dma_buf, 0,
			sizeof(struct snd_dma_buffer));

	spi_data.capture_dma_buf->area = dma_alloc_coherent(platform->dev,
					SPI_DMA_BYTES_MAX,
					&(spi_data.capture_dma_buf->addr),
					GFP_KERNEL);
#endif

	return 0;
}


static struct snd_pcm_ops amzn_mt_spi_platform_ops = {
	.open = amzn_mt_spi_pcm_open,
	.close = amzn_mt_spi_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = amzn_mt_spi_pcm_hw_params,
	.hw_free = amzn_mt_spi_pcm_hw_free,
	.copy = amzn_mt_spi_pcm_copy,
	.prepare = amzn_mt_spi_pcm_prepare,
	.trigger = amzn_mt_spi_pcm_trigger,
	.pointer = amzn_mt_spi_pcm_pointer,
	.silence = amzn_mt_spi_pcm_silence,
};

static struct snd_soc_platform_driver amzn_mt_spi_pltfm_drv = {
	.ops = &amzn_mt_spi_platform_ops,
	.pcm_new = amzn_mt_spi_pcm_new,
	.probe = amzn_asoc_capt_probe,
};

static const struct snd_soc_component_driver amzn_mt_spi_cpu_dai_component = {
	.name = "amzn-mt-spi-cpu-dai",
};

static int amzn_mt_spi_probe(struct spi_device *spi)
{
	int rc, rst_gpio;
	char buf[SPI_SETUP_BUF_SIZE] = {0};
	const struct firmware *fw_entry;
	struct device_node *node;
	struct dough_frame *tx_df, *rx_df;
	size_t bytes;
	void *fw_buf;
	/* TODO: Enable it for all products */
#if defined CONFIG_rbc123 || defined CONFIG_FPGA_POWER_SEQUENCE
	struct regulator *reg = NULL;
	int volt, reg_status_before = 0, reg_status_after;
#endif

	pr_info("%s\n", __func__);

#if defined CONFIG_rbc123 || defined CONFIG_FPGA_POWER_SEQUENCE
	/* VCCIO2 Enabled */
	reg = devm_regulator_get(&spi->dev, "vcamaf");
	if (IS_ERR(reg)) {
		pr_info("%s vcamaf not found\n", __func__);
		return -EINVAL;
	}
	reg_status_before = regulator_is_enabled(reg);
	rc = regulator_enable(reg);
	if (rc != 0) {
		pr_err("Fails to enable vcamaf, ret = 0x%x", rc);
		return -EINVAL;
	}
	reg_status_after = regulator_is_enabled(reg);
	volt = regulator_get_voltage(reg);
	pr_info("%s: vcamaf enable = %d:%d voltage = %d\n", __func__,
		reg_status_before, reg_status_after, volt);

	msleep(FPGA_VCC_DELAY_MS);

	/* VCCIO0 Enabled */
#ifdef CONFIG_rbc123
	reg = devm_regulator_get(&spi->dev, "vgp3");
#elif defined CONFIG_FPGA_POWER_SEQUENCE
	reg = devm_regulator_get(&spi->dev, "vcn18");
#endif
	if (IS_ERR(reg)) {
		pr_info("%s vgp3 not found\n", __func__);
		return -EINVAL;
	}

	reg_status_before = regulator_is_enabled(reg);
	rc = regulator_enable(reg);
	if (rc != 0) {
		pr_err("Fails to enable vgp3, ret = 0x%x", rc);
		return -EINVAL;
	}
	reg_status_after = regulator_is_enabled(reg);
	volt = regulator_get_voltage(reg);
	pr_info("%s: vgp3 enable = %d:%d voltage = %d\n", __func__,
		reg_status_before, reg_status_after, volt);
#endif

	/* TODO(DEE-30199): Fix with global decalration
	spi_data = devm_kzalloc(&spi->dev, sizeof(struct amzn_spi_priv),
							GFP_KERNEL);
	 if (spi_data == NULL)
		 return -ENOMEM;
	*/
	tx_df = kzalloc(sizeof(struct dough_frame), GFP_KERNEL | GFP_DMA);
	if (!tx_df) {
		pr_err("%s: failed to alloc dough frame tx_df\n", __func__);
		return -ENOMEM;
	}

	rx_df = kzalloc(sizeof(struct dough_frame), GFP_KERNEL | GFP_DMA);
	if (!rx_df) {
		pr_err("%s: failed to alloc dough frame rx_df\n", __func__);
		rc = -ENOMEM;
		goto free_tx_df;
	}

	fw_buf = kzalloc(FIRMWARE_MAX_BYTES, GFP_KERNEL | GFP_DMA);
	if (!fw_buf) {
		pr_err("%s: no mem for fw buf\n", __func__);
		rc = -ENOMEM;
		goto free_rx_df;
	}

	if (sizeof(struct dough_frame) !=
			(DOUGH_AUDIO_FRAME_BYTES * (1+DOUGH_AUDIO_FRAME_BUF))) {
		pr_err("%s: Basic understanding of dough frames incorrect\n",
			__func__);
		rc = -EINVAL;
		goto free_buffers;
	}

	if (!spi->dev.of_node) {
		pr_err("%s: Only DT devices are supported\n", __func__);
		rc = -EINVAL;
		goto free_buffers;
	}

	dev_set_name(&spi->dev, "%s", AMZN_MT_SPI_PCM);
	spi_set_drvdata(spi, &spi_data);

	pr_info("%s: dev name %s\n", __func__, dev_name(&spi->dev));

	/* Initialize I2S Pins going to FPGA */
	AudDrv_GPIO_probe(&spi->dev);

	/*
	 * Setup SPI configuration borrowed from spid
	 */
	spi->mode = SPI_MODE_3;		/* CPHA | CPOL */
	spi->bits_per_word = 8;
	spi->max_speed_hz = SPI_SPEED_HZ;
	rc = spi_setup(spi);
	if (rc < 0) {
		pr_err("%s: SPI setup failed\n", __func__);
		goto free_buffers;
	}

	/*
	 * Load RTL firmware. From comments in spid Running a small transaction
	 * seems to pull chip select low on the fpga.  This needs to happen
	 * while the reset line is toggled in order to flash a new firmware
	 * image.
	 */
	/* Put FPGA in default mode */
	buf[0] = dough_fw_off;
	rc = spi_txrx(spi, buf, NULL, SPI_SETUP_BUF_SIZE, 0, 0);
	if (rc < 0) {
		pr_err("%s: failed to initiate sync SPI transfer\n", __func__);
		goto free_buffers;
	}

	/* Reset FPGA */
	node = spi->dev.of_node;
	rst_gpio = of_get_gpio_flags(node, 0, NULL);
	rc = gpio_request_one(rst_gpio, GPIOF_INIT_LOW, AMZN_MT_SPI_PCM);
	if (rc) {
		pr_err("%s: Error request FPGA reset GPIO\n", __func__);
		goto free_buffers;
	}

	AudDrv_GPIO_I2S_Select(false);

	gpio_set_value(rst_gpio, 0);
	msleep(FPGA_DELAY_MS);
	gpio_set_value(rst_gpio, 1);
	msleep(FPGA_DELAY_MS);

	/* Now load the actual firmware */
	rc = request_firmware(&fw_entry, FPGA_FIRMWARE_NAME, &spi->dev);
	if (rc) {
		pr_err("%s: FPGA Firmware couldn't be requested\n", __func__);
		goto free_gpio;
	}

	bytes = roundup(fw_entry->size, 1024) + 1024;
	if (bytes > FIRMWARE_MAX_BYTES) {
		pr_err("%s: FPGA firmware too big\n", __func__);
		rc = -1;
		release_firmware(fw_entry);
		goto free_gpio;
	}

	memcpy(fw_buf, (void *)fw_entry->data, bytes);

	rc = spi_txrx(spi, fw_buf, NULL, bytes, 0, 0);
	if (rc < 0) {
		pr_err("%s: FPGA load failed to initiate sync SPI transfer\n",
			__func__);
		release_firmware(fw_entry);
		goto free_gpio;
	}
	release_firmware(fw_entry);

	msleep(FPGA_DELAY_MS);

	/* Verify FW is active and reporting correct version */
	rc = spi_txrx(spi, (void *)tx_df, (void *)rx_df,
		sizeof(struct dough_frame), 0, 0);
	if (rc < 0) {
		pr_err("%s: Failed to get FPGA rev version\n", __func__);
		goto free_gpio;
	}

	if (!verify_fpga_frm_ver(rx_df->dsf.fpga_rev)) {
		rc = -EINVAL;
		goto free_gpio;
	}

	pr_info("%s: FPGA Revision = %u\n", __func__, rx_df->dsf.fpga_rev);

	/* Enable MCLK */
	AudDrv_GPIO_MCLK_Select();

	/* Restore I2S */
	AudDrv_GPIO_I2S_Select(true);

	msleep(PINCTRL_DELAY_MS);

	/* Release pinctrl */
	AudDrv_GPIO_release();

	/* Put FPGA in I2S Audio to Test Pattern mode */
#ifdef FPGA_TEST_PATTERN_ENABLE
	buf[0] = dough_fw_tpg;
#else
	buf[0] = dough_fw_i2s;
#endif

	rc = spi_txrx(spi, buf, NULL, SPI_SETUP_BUF_SIZE, 0, 0);
	if (rc < 0) {
		pr_err("%s: failed to initiate async SPI transfer\n", __func__);
		goto free_gpio;
	}

	/*
	 * Register CPU DAI and component
	 * Note: cpu_dai_name to be used in the machine driver will be the
	 * device name (amzn-mt-spi-pcm), not the component name since there
	 * is only 1 DAI
	 */
	rc = snd_soc_register_component(&spi->dev,
			&amzn_mt_spi_cpu_dai_component, amzn_mt_spi_cpu_dai,
			ARRAY_SIZE(amzn_mt_spi_cpu_dai));
	if (rc < 0) {
		pr_err("%s: cpu dai component registration error\n", __func__);
		goto free_gpio;
	}

	/*
	 * Register the ASoC platform driver, this adds it to the platform list
	 * and makes it available in "platform_name" for the machine driver.
	 */
	rc = snd_soc_register_platform(&spi->dev, &amzn_mt_spi_pltfm_drv);

free_gpio:
	gpio_free(rst_gpio);

free_buffers:
	kfree(fw_buf);
free_rx_df:
	kfree(rx_df);
free_tx_df:
	kfree(tx_df);

	return rc;
}

static int amzn_mt_spi_remove(struct spi_device *spi)
{
	struct snd_soc_platform *platform = snd_soc_lookup_platform(&spi->dev);

	snd_soc_remove_platform(platform);

	return 0;
}

static const struct of_device_id amzn_mt_spi_dt_ids[] = {
	{ .compatible = "amzn-mtk,spi-audio-pltfm" },
	{},
};

MODULE_DEVICE_TABLE(of, amzn_mt_spi_dt_ids);

static struct spi_driver amzn_mt_spi_driver = {
	.driver = {
		.name  = "amzn_mt_audio_platform_drv",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(amzn_mt_spi_dt_ids),
		.bus = &spi_bus_type,
	},
	.probe = amzn_mt_spi_probe,
	.remove = amzn_mt_spi_remove,
};

static int __init spi_init(void)
{
	int rc;

	rc = spi_register_driver(&amzn_mt_spi_driver);
	if (rc < 0) {
		pr_err("%s: Failed to reg spi driver\n", __func__);
		return rc;
	}
	return rc;
}

module_init(spi_init);

static void __exit spi_exit(void)
{
	spi_unregister_driver(&amzn_mt_spi_driver);
}

module_exit(spi_exit);

MODULE_AUTHOR("Amazon Lab126 Inc.");
MODULE_DESCRIPTION("Amazon Lab126 SPI Audio Driver");
MODULE_LICENSE("GPL v2");
