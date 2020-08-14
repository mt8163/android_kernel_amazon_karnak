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

#include <linux/dma-mapping.h>
#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"

static DEFINE_SPINLOCK(auddrv_DL2_I2S0_lock);
static struct AFE_MEM_CONTROL_T *pDL2I2s0MemControl;
static struct snd_dma_buffer *dl2_i2s0_dma_buf;

static struct device *mDev;

/*
 *    function implementation
 */

static int mtk_dl2_i2s0_probe(struct platform_device *pdev);
static int mtk_pcm_dl2_i2s0_close(struct snd_pcm_substream *substream);
static int mtk_asoc_pcm_dl2_i2s0_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_dl2_i2s0_probe(struct snd_soc_platform *platform);

static struct snd_pcm_hardware mtk_dl2_i2s0_hardware = {

	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_HIGH_USE_RATE_MIN,
	.rate_max = SOC_HIGH_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = Dl2_MAX_BUFFER_SIZE,
	.period_bytes_max = Dl2_MAX_BUFFER_SIZE,
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static int mtk_pcm_dl2_i2s0_stop(struct snd_pcm_substream *substream)
{
	struct AFE_BLOCK_T *Afe_Block = &(pDL2I2s0MemControl->rBlock);

	pr_debug("%s\n", __func__);
	SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, false);

	/* here start digital part */
	SetConnection(Soc_Aud_InterCon_DisConnect,
		      Soc_Aud_InterConnectionInput_I07,
		      Soc_Aud_InterConnectionOutput_O00);
	SetConnection(Soc_Aud_InterCon_DisConnect,
		      Soc_Aud_InterConnectionInput_I08,
		      Soc_Aud_InterConnectionOutput_O01);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL2, false);

	/* stop 2nd I2S */
	Disable_2nd_I2S_4pin();

	EnableAfe(false);

	/* clean audio hardware buffer */
	if (Afe_Block->pucVirtBufAddr ==
	    (kal_uint8 *) Get_Afe_SramBase_Pointer())
		memset_io(Afe_Block->pucVirtBufAddr, 0,
			  Afe_Block->u4BufferSize);
	else
		memset(Afe_Block->pucVirtBufAddr, 0, Afe_Block->u4BufferSize);

	RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DL2, substream);

	return 0;
}

static snd_pcm_uframes_t mtk_pcm_dl2_i2s0_pointer(struct snd_pcm_substream
						  *substream)
{
	kal_int32 HW_memory_index = 0;
	kal_int32 HW_Cur_ReadIdx = 0;
	kal_uint32 Frameidx = 0;
	kal_int32 Afe_consumed_bytes = 0;
	struct AFE_BLOCK_T *Afe_Block = &pDL2I2s0MemControl->rBlock;
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	PRINTK_AUD_DL2(" %s Afe_Block->u4DMAReadIdx = 0x%x\n", __func__,
		       Afe_Block->u4DMAReadIdx);

	Auddrv_Dl2_Spinlock_lock();

	/* get total bytes to copy */
	/* Frameidx =
		audio_bytes_to_frame(substream , Afe_Block->u4DMAReadIdx); */
	/* return Frameidx; */

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL2) == true) {
		HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DL2_CUR);
		if (HW_Cur_ReadIdx == 0) {
			PRINTK_AUDDRV("[Auddrv] HW_Cur_ReadIdx == 0\n");
			HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
		}

		HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);

		if (HW_memory_index >= Afe_Block->u4DMAReadIdx)
			Afe_consumed_bytes =
			    HW_memory_index - Afe_Block->u4DMAReadIdx;
		else {
			Afe_consumed_bytes =
			    Afe_Block->u4BufferSize + HW_memory_index -
			    Afe_Block->u4DMAReadIdx;
		}

		Afe_consumed_bytes = Align64ByteSize(Afe_consumed_bytes);

		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;

		pr_debug
		    ("[Auddrv] HW_Cur_ReadIdx = 0x%x, H",
		     HW_Cur_ReadIdx);
		pr_debug("W_memory_index = 0x%x, Afe_consumed_bytes = 0x%x\n",
			HW_memory_index, Afe_consumed_bytes);
		Auddrv_Dl2_Spinlock_unlock();
		return bytes_to_frames(substream->runtime,
				       Afe_Block->u4DMAReadIdx);
	}

	Frameidx = bytes_to_frames(substream->runtime, Afe_Block->u4DMAReadIdx);
	Auddrv_Dl2_Spinlock_unlock();
	return Frameidx;

}

static void SetDL2Buffer(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct AFE_BLOCK_T *pblock = &pDL2I2s0MemControl->rBlock;

	pr_debug("%s\n", __func__);

	pblock->pucPhysBufAddr = runtime->dma_addr;
	pblock->pucVirtBufAddr = runtime->dma_area;
	pblock->u4BufferSize = runtime->dma_bytes;
	pblock->u4SampleNumMask = 0x001f;	/* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;

	pr_debug
	    ("u4BufferSize = %d pucVirtBufAddr = %p pucPhysBufAddr = 0x%x\n",
	     pblock->u4BufferSize, pblock->pucVirtBufAddr,
	     pblock->pucPhysBufAddr);

	/* set dram address top hardware */
	Afe_Set_Reg(AFE_DL2_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	Afe_Set_Reg(AFE_DL2_END,
		    pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1),
		    0xffffffff);
}

static int mtk_pcm_dl2_i2s0_hw_params(struct snd_pcm_substream *substream,
				      struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	int ret = 0;

	PRINTK_AUDDRV("%s\n", __func__);

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;

	if (dl2_i2s0_dma_buf->area) {
		pr_debug("%s dl2_i2s0_dma_buf->area\n", __func__);
		runtime->dma_bytes = params_buffer_bytes(hw_params);
		runtime->dma_area = dl2_i2s0_dma_buf->area;
		runtime->dma_addr = dl2_i2s0_dma_buf->addr;
	} else {
		pr_debug("%s snd_pcm_lib_malloc_pages\n", __func__);
		ret =
		    snd_pcm_lib_malloc_pages(substream,
					     params_buffer_bytes(hw_params));
		if (ret < 0)
			return ret;
	}
	pr_debug("%s dma_bytes = %zu, dma_area = %p, dma_addr = 0x%x\n",
		 __func__, runtime->dma_bytes, runtime->dma_area,
		 (uint32) runtime->dma_addr);

	pr_debug("runtime->hw.buffer_bytes_max = 0x%zu\n",
		 runtime->hw.buffer_bytes_max);
	SetDL2Buffer(substream, hw_params);

	pr_debug("dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
		 substream->runtime->dma_bytes, substream->runtime->dma_area,
		 (long)substream->runtime->dma_addr);

	return ret;
}

static int mtk_pcm_dl2_i2s0_hw_free(struct snd_pcm_substream *substream)
{
	if (dl2_i2s0_dma_buf->area)
		return 0;
	else
		return snd_pcm_lib_free_pages(substream);
}

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {

	.count = ARRAY_SIZE(soc_high_supported_sample_rates),
	.list = soc_high_supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_dl2_i2s0_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_debug("SNDRV_PCM_STREAM_PLAYBACK OK");
	else
		return -1;

	runtime->hw = mtk_dl2_i2s0_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_dl2_i2s0_hardware,
	       sizeof(struct snd_pcm_hardware));
	pDL2I2s0MemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL2);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	if (ret < 0) {
		pr_warn("snd_pcm_hw_constraint_list failed\n");
		return ret;
	}

	ret =
	    snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0) {
		pr_warn("snd_pcm_hw_constraint_integer failed\n");
		return ret;
	}

	/* print for hw pcm information */
	pr_debug
	    ("%s, runtime->rate = %d, channels = %d, ",
	     __func__, runtime->rate, runtime->channels);

	AudDrv_ANA_Clk_On();
	AudDrv_Clk_On();
	AudDrv_Emi_Clk_On();

	pr_debug("%s return\n", __func__);
	return 0;
}

static int mtk_pcm_dl2_i2s0_close(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);
	AudDrv_Emi_Clk_Off();
	AudDrv_Clk_Off();
	AudDrv_ANA_Clk_Off();
	return 0;
}

static int mtk_pcm_dl2_i2s0_prepare(struct snd_pcm_substream *substream)
{
	pr_debug("%s, substream->rate = %d, substream->channels = %d\n",
		 __func__, substream->runtime->rate,
		 substream->runtime->channels);
	return 0;
}

static int mtk_pcm_dl2_i2s0_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s\n", __func__);

	SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DL2, substream);
	if (runtime->format == SNDRV_PCM_FORMAT_S32_LE
	    || runtime->format == SNDRV_PCM_FORMAT_S32_LE) {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2,
			AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2,
			AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
	} else {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2,
			AFE_WLEN_16_BIT);
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2,
			AFE_WLEN_16_BIT);
	}

	SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
		Soc_Aud_InterConnectionOutput_O00);
	SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
		Soc_Aud_InterConnectionOutput_O01);

	/* here start digital part */
	SetConnection(Soc_Aud_InterCon_Connection,
		      Soc_Aud_InterConnectionInput_I07,
		      Soc_Aud_InterConnectionOutput_O00);
	SetConnection(Soc_Aud_InterCon_Connection,
		      Soc_Aud_InterConnectionInput_I08,
		      Soc_Aud_InterConnectionOutput_O01);

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_2) == false ||
	    GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2) == false) {
		Enable_2nd_I2S_4pin(runtime->rate);
	}

	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL2, runtime->rate);
	SetChannels(Soc_Aud_Digital_Block_MEM_DL2, runtime->channels);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL2, true);

	/* here to set interrupt */
	SetIrqMcuCounter(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE,
			 runtime->period_size);
	SetIrqMcuSampleRate(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, runtime->rate);
	SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, true);

	EnableAfe(true);

	return 0;
}

static int mtk_pcm_dl2_i2s0_trigger(struct snd_pcm_substream *substream,
				    int cmd)
{
	pr_debug("%s cmd = %d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_dl2_i2s0_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_dl2_i2s0_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_dl2_i2s0_copy(struct snd_pcm_substream *substream,
				 int channel, snd_pcm_uframes_t pos,
				 void __user *dst, snd_pcm_uframes_t count)
{
	struct AFE_BLOCK_T *Afe_Block = NULL;
	int copy_size = 0, Afe_WriteIdx_tmp;
	unsigned long flags;
	char *data_w_ptr = (char *)dst;
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	PRINTK_AUD_DL2("%s, pos = 0x%x, count = 0x%x\n", __func__,
		       (unsigned int)pos, (unsigned int)count);

	count = frames_to_bytes(substream->runtime, count);

	/* check which memif nned to be write */
	Afe_Block = &pDL2I2s0MemControl->rBlock;

	/* handle for buffer management */
	PRINTK_AUD_DL2("WriteIdx=0x%x, ReadIdx=0x%x, DataRemained=0x%x\n",
		       Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx,
		       Afe_Block->u4DataRemained);

	if (Afe_Block->u4BufferSize == 0) {
		pr_err("%s u4BufferSize = 0, Error!!\n", __func__);
		return 0;
	}

	spin_lock_irqsave(&auddrv_DL2_I2S0_lock, flags);
	/* free space of the buffer */
	copy_size = Afe_Block->u4BufferSize - Afe_Block->u4DataRemained;
	spin_unlock_irqrestore(&auddrv_DL2_I2S0_lock, flags);

	if (count <= copy_size) {
		if (copy_size < 0)
			copy_size = 0;
		else
			copy_size = count;
	}

	copy_size = Align64ByteSize(copy_size);
	PRINTK_AUD_DL2("copy_size = 0x%x, count = 0x%x\n",
		       (unsigned int)copy_size, (unsigned int)count);
	if (copy_size != 0) {
		spin_lock_irqsave(&auddrv_DL2_I2S0_lock, flags);
		Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
		spin_unlock_irqrestore(&auddrv_DL2_I2S0_lock, flags);

		/* copy once */
		if (Afe_WriteIdx_tmp + copy_size < Afe_Block->u4BufferSize) {
			if (!access_ok(VERIFY_READ, data_w_ptr, copy_size)) {
				pr_warn
				    ("0 ptr invalid data_w_ptr=%p, size=%d\n",
				     data_w_ptr, copy_size);
				pr_warn("u4BufferSize=%d, u4DataRemained=%d\n",
					 Afe_Block->u4BufferSize,
					 Afe_Block->u4DataRemained);
			} else {
			pr_debug
			("memcpy Afe_Block->pucVirtBufAddr+Afe_WriteIdx=%p,",
			Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp);

				pr_debug(" data_w_ptr=%p, copy_size=0x%x\n",
					data_w_ptr, copy_size);
				if (copy_from_user
				    ((Afe_Block->pucVirtBufAddr +
				      Afe_WriteIdx_tmp), data_w_ptr,
				     copy_size)) {
					pr_debug("Fail copy from user\n");
					return -1;
				}
			}

			spin_lock_irqsave(&auddrv_DL2_I2S0_lock, flags);
			Afe_Block->u4DataRemained += copy_size;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + copy_size;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_DL2_I2S0_lock, flags);
			data_w_ptr += copy_size;
			count -= copy_size;

			pr_debug
			    ("finish 1, copy_size:%x, WriteIdx:%x, ",
			     copy_size, Afe_Block->u4WriteIdx);

			pr_debug("ReadIdx=%x, DataRemained:%x, count=%x\n",
				Afe_Block->u4DMAReadIdx,
				Afe_Block->u4DataRemained, (unsigned int)count);

		} else {	/* copy twice */
			kal_uint32 size_1 = 0, size_2 = 0;

			size_1 =
			    Align64ByteSize((Afe_Block->u4BufferSize -
					     Afe_WriteIdx_tmp));
			size_2 = Align64ByteSize((copy_size - size_1));
			PRINTK_AUD_DL2("size_1 = 0x%x, size_2 = 0x%x\n", size_1,
				       size_2);
			if (!access_ok(VERIFY_READ, data_w_ptr, size_1)) {
				pr_debug
				("1 ptr invalid, data_w_ptr = %p, size_1 = %d ",
				data_w_ptr, size_1);
				pr_debug
				("u4BufferSize = %d, u4DataRemained = %d\n",
				Afe_Block->u4BufferSize,
				Afe_Block->u4DataRemained);
			} else {
			PRINTK_AUD_DL2
			("mcmcpy, Afe_Block->pucVirtBufAddr+Afe_WriteIdx=%p, ",
			Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp);

				PRINTK_AUD_DL2("data_w_ptr=%p, size_1=%x\n",
					data_w_ptr, size_1);
				if ((copy_from_user
				     ((Afe_Block->pucVirtBufAddr +
				       Afe_WriteIdx_tmp), data_w_ptr,
				      size_1))) {
					PRINTK_AUDDRV
					    ("Fail 1 copy from user\n");
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_DL2_I2S0_lock, flags);
			Afe_Block->u4DataRemained += size_1;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_1;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
			spin_unlock_irqrestore(&auddrv_DL2_I2S0_lock, flags);

			if (!access_ok
			    (VERIFY_READ, data_w_ptr + size_1, size_2)) {
				pr_debug
				    ("2 ptr invalid, data_w_ptr = %p, ",
				     data_w_ptr);
				pr_debug("size_1 = %d, size_2 = %d",
					size_1, size_2);
				pr_debug
				    ("u4BufferSize = %d, u4DataRemained = %d\n",
				     Afe_Block->u4BufferSize,
				     Afe_Block->u4DataRemained);
			} else {
			pr_debug
			("mcmcpy, Afe_Block->pucVirtBufAddr+Afe_WriteIdx=%p,",
			Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp);
				pr_debug(" data_w_ptr+size_1=%p, size_2=%x\n",
					data_w_ptr + size_1, size_2);
				if ((copy_from_user
				     ((Afe_Block->pucVirtBufAddr +
				       Afe_WriteIdx_tmp), (data_w_ptr + size_1),
				      size_2))) {
					pr_debug("AudDrv_write Fail 2 copy from user\n");
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_DL2_I2S0_lock, flags);

			Afe_Block->u4DataRemained += size_2;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_2;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_DL2_I2S0_lock, flags);
			count -= copy_size;
			data_w_ptr += copy_size;

			pr_debug
			    ("finish 2, copy size:%x, WriteIdx:%x, ",
			     copy_size, Afe_Block->u4WriteIdx);
			pr_debug("ReadIdx:%x, DataRemained:%x\n",
			Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained);
		}
	}
	PRINTK_AUD_DL2("pcm_copy return\n");
	return 0;
}

static struct snd_pcm_ops mtk_dl2_i2s0_ops = {

	.open = mtk_pcm_dl2_i2s0_open,
	.close = mtk_pcm_dl2_i2s0_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_dl2_i2s0_hw_params,
	.hw_free = mtk_pcm_dl2_i2s0_hw_free,
	.prepare = mtk_pcm_dl2_i2s0_prepare,
	.trigger = mtk_pcm_dl2_i2s0_trigger,
	.pointer = mtk_pcm_dl2_i2s0_pointer,
	.copy = mtk_pcm_dl2_i2s0_copy,
};

static struct snd_soc_platform_driver mtk_i2s0_soc_platform = {

	.ops = &mtk_dl2_i2s0_ops,
	.pcm_new = mtk_asoc_pcm_dl2_i2s0_new,
	.probe = mtk_afe_dl2_i2s0_probe,
};

static int mtk_dl2_i2s0_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_VOIP_BT_OUT);

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	mDev = &pdev->dev;

	return snd_soc_register_platform(&pdev->dev, &mtk_i2s0_soc_platform);
}

static int mtk_asoc_pcm_dl2_i2s0_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
	return ret;
}

static int mtk_afe_dl2_i2s0_probe(struct snd_soc_platform *platform)
{
	pr_debug("mtk_afe_dl2_i2s0_probe\n");
	AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_DL2,
				   Dl2_MAX_BUFFER_SIZE);
	dl2_i2s0_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL2);
	return 0;
}

static int mtk_dl2_i2s0_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_dl2_i2s0_of_ids[] = {

	{.compatible = "mediatek,mt8163-soc-pcm-dl1-bt",},
	{}
};
#endif

static struct platform_driver mtk_dl2_i2s0_driver = {

	.driver = {
		   .name = MT_SOC_VOIP_BT_OUT,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mt_soc_pcm_dl2_i2s0_of_ids,
#endif
		   },
	.probe = mtk_dl2_i2s0_probe,
	.remove = mtk_dl2_i2s0_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkdl2i2s0_dev;
#endif

static int __init mtk_dl2_i2s0_soc_platform_init(void)
{
	int ret;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkdl2i2s0_dev = platform_device_alloc(MT_SOC_VOIP_BT_OUT, -1);

	if (!soc_mtkdl2i2s0_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkdl2i2s0_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkdl2i2s0_dev);
		return ret;
	}
#endif

	ret = platform_driver_register(&mtk_dl2_i2s0_driver);
	return ret;

}

module_init(mtk_dl2_i2s0_soc_platform_init);

static void __exit mtk_dl2_i2s0_soc_platform_exit(void)
{
	pr_debug("%s\n", __func__);

	platform_driver_unregister(&mtk_dl2_i2s0_driver);
}

module_exit(mtk_dl2_i2s0_soc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
