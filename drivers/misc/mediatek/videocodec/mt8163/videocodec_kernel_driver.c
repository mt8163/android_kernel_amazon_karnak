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

#include <asm/page.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
/* #include <mach/x_define_irq.h> */
#include <dma.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
/* #include <linux/earlysuspend.h> */
#include "sync_write.h"
/* #include "mach/mt_reg_base.h" */
#ifdef CONFIG_OF
#include <linux/clk.h>
#else
#include "mach/mt_clkmgr.h"
#endif
#include "mtk_smi.h"
#ifdef CONFIG_MTK_HIBERNATION
#include <mtk_hibernate_dpm.h>
#endif

#include "videocodec_kernel_driver.h"
#include <asm/cacheflush.h>
#include <linux/io.h>
#include <asm/sizes.h>
#include "val_types_private.h"
#include "hal_types_private.h"
#include "val_api_private.h"
#include "drv_api.h"

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pm_runtime.h>
#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/uaccess.h>
#include <linux/compat.h>
#endif

#define VDO_HW_WRITE(ptr, data) mt_reg_sync_writel(data, ptr)
#define VDO_HW_READ(ptr)           readl((void __iomem *)ptr)

#define VCODEC_DEVNAME "Vcodec"
#define MT8163_VCODEC_DEV_MAJOR_NUMBER 160 /* 189 */

static dev_t vcodec_devno = MKDEV(MT8163_VCODEC_DEV_MAJOR_NUMBER, 0);
static struct cdev *vcodec_cdev;
static struct class *vcodec_class;
static struct device *vcodec_device;

static DEFINE_MUTEX(IsOpenedLock);
static DEFINE_MUTEX(PWRLock);
static DEFINE_MUTEX(VdecHWLock);
static DEFINE_MUTEX(VencHWLock);
static DEFINE_MUTEX(EncEMILock);
static DEFINE_MUTEX(L2CLock);
static DEFINE_MUTEX(DecEMILock);
static DEFINE_MUTEX(DriverOpenCountLock);
static DEFINE_MUTEX(DecHWLockEventTimeoutLock);
static DEFINE_MUTEX(EncHWLockEventTimeoutLock);

static DEFINE_MUTEX(VdecPWRLock);
static DEFINE_MUTEX(VencPWRLock);

static DEFINE_SPINLOCK(DecIsrLock);
static DEFINE_SPINLOCK(EncIsrLock);
static DEFINE_SPINLOCK(LockDecHWCountLock);
static DEFINE_SPINLOCK(LockEncHWCountLock);
static DEFINE_SPINLOCK(DecISRCountLock);
static DEFINE_SPINLOCK(EncISRCountLock);

static struct VAL_EVENT_T DecHWLockEvent;
	/* mutex : HWLockEventTimeoutLock */
static struct VAL_EVENT_T EncHWLockEvent;
	/* mutex : HWLockEventTimeoutLock */
static struct VAL_EVENT_T DecIsrEvent;
	/* mutex : HWLockEventTimeoutLock */
static struct VAL_EVENT_T EncIsrEvent;
	/* mutex : HWLockEventTimeoutLock */
static signed int MT8163Driver_Open_Count;
	/* mutex : DriverOpenCountLock */
static unsigned int gu4PWRCounter;
	/* mutex : PWRLock */
static unsigned int gu4EncEMICounter;
	/* mutex : EncEMILock */
static unsigned int gu4DecEMICounter;
	/* mutex : DecEMILock */
static unsigned int gu4L2CCounter;
	/* mutex : L2CLock */
static char bIsOpened = VAL_FALSE;
	/* mutex : IsOpenedLock */
static unsigned int gu4HwVencIrqStatus;
	/* hardware VENC IRQ status (VP8/H264) */

static unsigned int gu4VdecPWRCounter;
	/* mutex : VdecPWRLock */
static unsigned int gu4VencPWRCounter;
	/* mutex : VencPWRLock */

static unsigned int gLockTimeOutCount;

static unsigned int gu4VdecLockThreadId;
/*#define MT8163_VCODEC_DEBUG*/
#ifdef MT8163_VCODEC_DEBUG
#undef VCODEC_DEBUG
#define VCODEC_DEBUG printk
#undef MODULE_MFV_LOGD
#define MODULE_MFV_LOGD  printk
#else
#define VCODEC_DEBUG(...)
#undef MODULE_MFV_LOGD
#define MODULE_MFV_LOGD(...)
#endif

/* VENC physical base address */
#undef VENC_BASE

#define VENC_BASE 0x17002000
#define VENC_LT_BASE 0x19002000
#define VENC_REGION 0x1000


/* VDEC virtual base address */
#define VDEC_BASE_PHY 0x16000000
#define VDEC_REGION 0x29000

#define HW_BASE 0x7FFF000
#define HW_REGION 0x2000

#define INFO_BASE 0x10000000
#define INFO_REGION 0x1000

#define VENC_IRQ_STATUS_SPS 0x1
#define VENC_IRQ_STATUS_PPS 0x2
#define VENC_IRQ_STATUS_FRM 0x4
#define VENC_IRQ_STATUS_DRAM 0x8
#define VENC_IRQ_STATUS_PAUSE 0x10
#define VENC_IRQ_STATUS_SWITCH 0x20
#define VENC_IRQ_STATUS_VPS 0x80
#define VENC_IRQ_STATUS_DRAM_VP8 0x20
#define VENC_SW_PAUSE 0x0AC
#define VENC_SW_HRST_N 0x0A8

/* #define VENC_PWR_FPGA */
/* Cheng-Jung 20120621 VENC power physical base address (FPGA only, should use
 * API) [
 */
#ifdef VENC_PWR_FPGA
#define CLK_CFG_0_addr 0x10000140
#define CLK_CFG_4_addr 0x10000150
#define VENC_PWR_addr 0x10006230
#define VENCSYS_CG_SET_addr 0x15000004

#define PWR_ONS_1_D 3
#define PWR_CKD_1_D 4
#define PWR_ONN_1_D 2
#define PWR_ISO_1_D 1
#define PWR_RST_0_D 0

#define PWR_ON_SEQ_0                                                           \
	((0x1 << PWR_ONS_1_D) | (0x1 << PWR_CKD_1_D) | (0x1 << PWR_ONN_1_D) |  \
	 (0x1 << PWR_ISO_1_D) | (0x0 << PWR_RST_0_D))
#define PWR_ON_SEQ_1                                                           \
	((0x1 << PWR_ONS_1_D) | (0x0 << PWR_CKD_1_D) | (0x1 << PWR_ONN_1_D) |  \
	 (0x1 << PWR_ISO_1_D) | (0x0 << PWR_RST_0_D))
#define PWR_ON_SEQ_2                                                           \
	((0x1 << PWR_ONS_1_D) | (0x0 << PWR_CKD_1_D) | (0x1 << PWR_ONN_1_D) |  \
	 (0x0 << PWR_ISO_1_D) | (0x0 << PWR_RST_0_D))
#define PWR_ON_SEQ_3                                                           \
	((0x1 << PWR_ONS_1_D) | (0x0 << PWR_CKD_1_D) | (0x1 << PWR_ONN_1_D) |  \
	 (0x0 << PWR_ISO_1_D) | (0x1 << PWR_RST_0_D))
/* ] */
#endif

unsigned long KVA_VENC_IRQ_ACK_ADDR, KVA_VENC_IRQ_STATUS_ADDR, KVA_VENC_BASE;
unsigned long KVA_VENC_LT_IRQ_ACK_ADDR, KVA_VENC_LT_IRQ_STATUS_ADDR,
	KVA_VENC_LT_BASE;
unsigned long KVA_VDEC_MISC_BASE, KVA_VDEC_VLD_BASE, KVA_VDEC_BASE,
	KVA_VDEC_GCON_BASE;
unsigned int VENC_IRQ_ID, VENC_LT_IRQ_ID, VDEC_IRQ_ID;
unsigned long KVA_VENC_SW_PAUSE, KVA_VENC_SW_HRST_N;
unsigned long KVA_VENC_LT_SW_PAUSE, KVA_VENC_LT_SW_HRST_N;

#ifdef VENC_PWR_FPGA
/* Cheng-Jung 20120621 VENC power physical base address (FPGA only, should use
 * API) [
 */
unsigned long KVA_VENC_CLK_CFG_0_ADDR, KVA_VENC_CLK_CFG_4_ADDR,
	KVA_VENC_PWR_ADDR,
	KVA_VENCSYS_CG_SET_ADDR;
/* ] */
#endif

/* extern unsigned long pmem_user_v2p_video(unsigned long va); */

#if defined(MT8163_VENC_USE_L2C)
/*extern int config_L2(int option); */
#endif

#ifdef CONFIG_OF
/*static struct clk *clk_venc_lt_clk;*/
/* static struct clk *clk_venc_pwr, *clk_venc_pwr2; */

struct platform_device *pvenc_dev;
struct platform_device *pvenclt_dev;

#endif
static int venc_enableIRQ(struct VAL_HW_LOCK_T *prHWLock);
static int venc_disableIRQ(struct VAL_HW_LOCK_T *prHWLock);
void vdec_power_on(void)
{
	mutex_lock(&VdecPWRLock);
	gu4VdecPWRCounter++;
	mutex_unlock(&VdecPWRLock);

/* Central power on */

#ifdef CONFIG_OF
	pr_debug("%s D+\n", __func__);
	mtk_smi_larb_clock_on(1, true);
	pr_debug("%s D -\n", __func__);
#else
	enable_clock(MT_CG_DISP0_SMI_COMMON, "VDEC");
	enable_clock(MT_CG_VDEC0_VDEC, "VDEC");
	enable_clock(MT_CG_VDEC1_LARB, "VDEC");
#endif
}

void vdec_power_off(void)
{
	mutex_lock(&VdecPWRLock);
	if (gu4VdecPWRCounter != 0) {
		gu4VdecPWRCounter--;
/* Central power off */
#ifdef CONFIG_OF
	pr_debug("%s D+\n", __func__);
	mtk_smi_larb_clock_off(1, true);
	pr_debug("%s D -\n", __func__);
#else
		disable_clock(MT_CG_VDEC0_VDEC, "VDEC");
		disable_clock(MT_CG_VDEC1_LARB, "VDEC");
		disable_clock(MT_CG_DISP0_SMI_COMMON, "VDEC");
#ifdef VDEC_USE_L2C
/* disable_clock(MT_CG_INFRA_L2C_SRAM, "VDEC"); */
#endif
#endif
	}
	mutex_unlock(&VdecPWRLock);
}

void venc_power_on(void)
{
	mutex_lock(&VencPWRLock);
	gu4VencPWRCounter++;
	mutex_unlock(&VencPWRLock);

#ifdef CONFIG_OF
	pr_debug("%s D+\n", __func__);
	mtk_smi_larb_clock_on(3, true);
	pr_debug("%s D -\n", __func__);
#else
	enable_clock(MT_CG_DISP0_SMI_COMMON, "VENC");
	enable_clock(MT_CG_VENC_VENC, "VENC");
	enable_clock(MT_CG_VENC_LARB, "VENC");
#ifdef MT8163_VENC_USE_L2C
	enable_clock(MT_CG_INFRA_L2C_SRAM, "VENC");
#endif
#endif
}

void venc_power_off(void)
{
	mutex_lock(&VencPWRLock);
	if (gu4VencPWRCounter == 0) {
		pr_debug("%s none +\n", __func__);
	} else {
		gu4VencPWRCounter--;
		pr_debug("%s D+\n", __func__);
#ifdef CONFIG_OF
		mtk_smi_larb_clock_off(3, true);
#else
		disable_clock(MT_CG_VENC_VENC, "VENC");
		disable_clock(MT_CG_VENC_LARB, "VENC");
		disable_clock(MT_CG_DISP0_SMI_COMMON, "VENC");
#ifdef MT8163_VENC_USE_L2C
		disable_clock(MT_CG_INFRA_L2C_SRAM, "VENC");
#endif
#endif
		pr_debug("%s D-\n", __func__);
	}
	mutex_unlock(&VencPWRLock);
}

void dec_isr(void)
{
	enum VAL_RESULT_T eValRet;
	unsigned long ulFlags, ulFlagsISR, ulFlagsLockHW;

	unsigned int u4TempDecISRCount = 0;
	unsigned int u4TempLockDecHWCount = 0;
	unsigned int u4CgStatus = 0;
	unsigned int u4DecDoneStatus = 0;

	u4CgStatus = VDO_HW_READ(KVA_VDEC_GCON_BASE);
	if ((u4CgStatus & 0x10) != 0) {
		pr_err("[MFV][ERROR] DEC ISR, VDEC active is not 0x0 (0x%08x)",
			u4CgStatus);
		return;
	}

	u4DecDoneStatus = VDO_HW_READ(KVA_VDEC_BASE + 0xA4);
	if ((u4DecDoneStatus & (0x1 << 16)) != 0x10000) {
		pr_err("[MFV][ERROR] DEC ISR, Decode done status is not 0x1 (0x%08x)",
			u4DecDoneStatus);
		return;
	}

	spin_lock_irqsave(&DecISRCountLock, ulFlagsISR);
	gu4DecISRCount++;
	u4TempDecISRCount = gu4DecISRCount;
	spin_unlock_irqrestore(&DecISRCountLock, ulFlagsISR);

	spin_lock_irqsave(&LockDecHWCountLock, ulFlagsLockHW);
	u4TempLockDecHWCount = gu4LockDecHWCount;
	spin_unlock_irqrestore(&LockDecHWCountLock, ulFlagsLockHW);

	/* Clear interrupt */
	VDO_HW_WRITE(KVA_VDEC_MISC_BASE + 41 * 4,
		     VDO_HW_READ(KVA_VDEC_MISC_BASE + 41 * 4) | 0x11);
	VDO_HW_WRITE(KVA_VDEC_MISC_BASE + 41 * 4,
		     VDO_HW_READ(KVA_VDEC_MISC_BASE + 41 * 4) & ~0x10);

	spin_lock_irqsave(&DecIsrLock, ulFlags);
	eValRet = eVideoSetEvent(&DecIsrEvent, sizeof(struct VAL_EVENT_T));
	if (eValRet != VAL_RESULT_NO_ERROR)
		pr_err("[MFV][ERROR] ISR set DecIsrEvent error\n");

	spin_unlock_irqrestore(&DecIsrLock, ulFlags);
}

void enc_isr(void)
{
	enum VAL_RESULT_T eValRet;
	unsigned long ulFlagsISR, ulFlagsLockHW;

	unsigned int u4TempEncISRCount = 0;
	unsigned int u4TempLockEncHWCount = 0;
	/* ---------------------- */
	spin_lock_irqsave(&EncISRCountLock, ulFlagsISR);
	gu4EncISRCount++;
	u4TempEncISRCount = gu4EncISRCount;
	spin_unlock_irqrestore(&EncISRCountLock, ulFlagsISR);

	spin_lock_irqsave(&LockEncHWCountLock, ulFlagsLockHW);
	u4TempLockEncHWCount = gu4LockEncHWCount;
	spin_unlock_irqrestore(&LockEncHWCountLock, ulFlagsLockHW);

	if (grVEncHWLock.pvHandle == 0) {
		pr_err("[ERROR] NO one Lock Enc HW, please check!!\n");

		/* Clear all status */
		/* VDO_HW_WRITE(KVA_VENC_MP4_IRQ_ACK_ADDR, 1); */
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
			VENC_IRQ_STATUS_PAUSE);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
			VENC_IRQ_STATUS_SWITCH);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
			VENC_IRQ_STATUS_DRAM);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
			VENC_IRQ_STATUS_SPS);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
			VENC_IRQ_STATUS_PPS);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
			VENC_IRQ_STATUS_FRM);
		VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
			VENC_IRQ_STATUS_PAUSE);

/*VP8 IRQ reset */
		return;
	}

	if (grVEncHWLock.eDriverType ==
	    VAL_DRIVER_TYPE_H264_ENC) { /*added by bin.liu  hardwire */
		gu4HwVencIrqStatus = VDO_HW_READ(KVA_VENC_IRQ_STATUS_ADDR);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PAUSE)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_PAUSE);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SWITCH)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_SWITCH);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_DRAM)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_DRAM);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SPS)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_SPS);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PPS)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_PPS);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_FRM)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_FRM);
	} else if (grVEncHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_ENC) {
		gu4HwVencIrqStatus = VDO_HW_READ(KVA_VENC_LT_IRQ_STATUS_ADDR);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PAUSE)
			VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_PAUSE);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SWITCH)
			VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_SWITCH);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_DRAM_VP8)
			VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_DRAM_VP8);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_DRAM)
			VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_DRAM);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SPS)
			VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_SPS);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PPS)
			VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_PPS);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_FRM)
			VDO_HW_WRITE(KVA_VENC_LT_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_FRM);
	}
#ifdef CONFIG_MTK_VIDEO_HEVC_SUPPORT
	else if (grVEncHWLock.eDriverType ==
		 VAL_DRIVER_TYPE_HEVC_ENC) { /* hardwire */
		/* pr_err("[enc_isr] VAL_DRIVER_TYPE_HEVC_ENC %d!!\n",
		 * gu4HwVencIrqStatus);
		 */

		gu4HwVencIrqStatus = VDO_HW_READ(KVA_VENC_IRQ_STATUS_ADDR);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PAUSE)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_PAUSE);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SWITCH)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_SWITCH);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_DRAM)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_DRAM);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_SPS)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_SPS);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_PPS)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_PPS);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_FRM)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_FRM);
		if (gu4HwVencIrqStatus & VENC_IRQ_STATUS_VPS)
			VDO_HW_WRITE(KVA_VENC_IRQ_ACK_ADDR,
				     VENC_IRQ_STATUS_VPS);
	}
#endif
	else
		pr_err("%s Invalid lock holder driver type = %d\n",
			__func__, grVEncHWLock.eDriverType);

	eValRet = eVideoSetEvent(&EncIsrEvent, sizeof(struct VAL_EVENT_T));
	if (eValRet != VAL_RESULT_NO_ERROR)
		pr_err("[VCODEC][ERROR] ISR set EncIsrEvent error\n");

	pr_debug("[MFV] %s ISR set EncIsrEvent done\n", __func__);
}

static irqreturn_t video_intr_dlr(int irq, void *dev_id)
{
	dec_isr();
	return IRQ_HANDLED;
}

static irqreturn_t video_intr_dlr2(int irq, void *dev_id)
{
	enc_isr();
	return IRQ_HANDLED;
}

enum VAL_RESULT_T vcodec_lockhw_vdec(struct VAL_HW_LOCK_T *pHWLock,
	char *pbLockedHW)
{
	unsigned int FirstUseDecHW = 0;
	unsigned int u4TimeInterval;
	unsigned long ulFlagLockDecHW;
	unsigned long ulLockHandle;
	enum VAL_RESULT_T eValRet;
	struct VAL_TIME_T rCurTime;

	if (pHWLock == NULL || pbLockedHW == NULL) {
		pr_err("lock_hw_for_dec invalid parameter!\n");
		return VAL_RESULT_INVALID_PARAMETER;
	}

	ulLockHandle = (unsigned long)(pHWLock->pvHandle);
	eValRet = VAL_RESULT_NO_ERROR;
	while (*pbLockedHW == VAL_FALSE) {
		mutex_lock(&DecHWLockEventTimeoutLock);
		if (DecHWLockEvent.u4TimeoutMs == 1) {
			pr_debug("[NOT ERROR][VCODEC_LOCKHW] First Use Dec HW!!\n");
			FirstUseDecHW = 1;
		} else {
			FirstUseDecHW = 0;
		}
		mutex_unlock(&DecHWLockEventTimeoutLock);

		if (FirstUseDecHW == 1) {
			eValRet = eVideoWaitEvent(&DecHWLockEvent,
						sizeof(struct VAL_EVENT_T));
		}
		mutex_lock(&DecHWLockEventTimeoutLock);
		if (DecHWLockEvent.u4TimeoutMs != 1000) {
			DecHWLockEvent.u4TimeoutMs = 1000;
			FirstUseDecHW = 1;
		} else {
			FirstUseDecHW = 0;
		}
		mutex_unlock(&DecHWLockEventTimeoutLock);

		mutex_lock(&VdecHWLock);
		/* one process try to lock twice */
		if (grVDecHWLock.pvHandle ==
			(void *)pmem_user_v2p_video(ulLockHandle)) {
			pr_err("[WARNING] one decoder instance try to lock twice may cause lock HW timeout!!instance = 0x%lx, CurrentTID = %d\n",
				 (unsigned long)grVDecHWLock.pvHandle,
				 current->pid);
		}
		mutex_unlock(&VdecHWLock);

		if (FirstUseDecHW == 0) {
			pr_debug("Not first time use HW, timeout = %d\n",
				 DecHWLockEvent.u4TimeoutMs);
			eValRet = eVideoWaitEvent(&DecHWLockEvent,
						sizeof(struct VAL_EVENT_T));
		}

		if (eValRet == VAL_RESULT_INVALID_ISR) {
			pr_err("[ERROR]DecHWLock TimeOut, CurrentTID = %d\n",
				current->pid);
			if (FirstUseDecHW != 1) {
				mutex_lock(&VdecHWLock);
				if (grVDecHWLock.pvHandle == 0)
					pr_err("[WARNING] maybe mediaserver restart before please check!!\n");
				else
					pr_err("[WARNING] someone use HW, and check timeout value!!\n");

				mutex_unlock(&VdecHWLock);
			}
		} else if (eValRet == VAL_RESULT_RESTARTSYS) {
			pr_err("[WARNING] VAL_RESULT_RESTARTSYS return when HWLock!!\n");
			return -ERESTARTSYS;
		}

		mutex_lock(&VdecHWLock);
		if (grVDecHWLock.pvHandle == 0) {
			/* No one holds dec hw lock now */
			gu4VdecLockThreadId = current->pid;
			grVDecHWLock.pvHandle =
				(void *)pmem_user_v2p_video(ulLockHandle);
			grVDecHWLock.eDriverType = pHWLock->eDriverType;
			eVideoGetTimeOfDay
				(&grVDecHWLock.rLockedTime,
				sizeof(struct VAL_TIME_T));

			pr_debug("No process use dec HW, so current process can use HW\n");
			pr_debug("LockInstance = 0x%lx CurrentTID = %d, rLockedTime(s, us) = %d, %d\n",
			(unsigned long)grVDecHWLock.pvHandle,
			current->pid,
			grVDecHWLock.rLockedTime.u4Sec,
			grVDecHWLock.rLockedTime.u4uSec);

			*pbLockedHW = VAL_TRUE;
			if (eValRet == VAL_RESULT_INVALID_ISR
				&& FirstUseDecHW != 1) {
				pr_err("[WARNING] reset power/irq when HWLock!!\n");
				vdec_power_off();
				disable_irq(VDEC_IRQ_ID);
			}
			vdec_power_on();
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	/* Morris Yang moved to TEE */
			if (pHWLock->bSecureInst == VAL_FALSE) {
				if (request_irq(VDEC_IRQ_ID,
					 (irq_handler_t) video_intr_dlr,
					 IRQF_TRIGGER_LOW, VCODEC_DEVNAME,
					 NULL) < 0)
					pr_err("[ERROR] error to request dec irq\n");
				else
					pr_debug("success to request dec irq\n");
			}
#else
			enable_irq(VDEC_IRQ_ID);
#endif
#ifdef ENABLE_MMDVFS_VDEC
			VdecDvfsMonitorStart();
#endif
		} else {
	/* Another one holding dec hw now */

			pr_debug("[NOT ERROR][VCODEC_LOCKHW] E\n");
			eVideoGetTimeOfDay
				(&rCurTime, sizeof(struct VAL_TIME_T));
			u4TimeInterval = (((((rCurTime.u4Sec -
				grVDecHWLock.rLockedTime.u4Sec) *
				1000000) + rCurTime.u4uSec)
				-grVDecHWLock.rLockedTime.u4uSec) / 1000);

			pr_debug("someone use dec HW, and check timeout value\n");
			pr_debug("Instance = 0x%lx CurrentTID = %d, TimeInterval(ms) = %d, TimeOutValue(ms)) = %d\n",
			(unsigned long)grVDecHWLock.pvHandle,
			current->pid, u4TimeInterval, pHWLock->u4TimeoutMs);

			pr_debug("[VCODEC_LOCKHW] Lock Instance = 0x%lx, Lock TID = %d, CurrentTID = %d, rLockedTime(%d s, %d us), rCurTime(%d s, %d us)\n",
			(unsigned long)grVDecHWLock.pvHandle,
			gu4VdecLockThreadId, current->pid,
			grVDecHWLock.rLockedTime.u4Sec,
			grVDecHWLock.rLockedTime.u4uSec,
			rCurTime.u4Sec, rCurTime.u4uSec);
		}
		mutex_unlock(&VdecHWLock);
		spin_lock_irqsave(&LockDecHWCountLock, ulFlagLockDecHW);
		gu4LockDecHWCount++;
		spin_unlock_irqrestore(&LockDecHWCountLock, ulFlagLockDecHW);
	}
	return eValRet;
}

enum VAL_RESULT_T vcodec_lockhw_venc(struct VAL_HW_LOCK_T *pHWLock,
	char *pbLockedHW)
{
	unsigned int FirstUseDecHW = 0;
	unsigned int u4TimeInterval;
	unsigned long ulFlagLockEncHW;
	unsigned long ulLockHandle;
	enum VAL_RESULT_T eValRet;
	struct VAL_TIME_T rCurTime;

	if (pHWLock == NULL || pbLockedHW == NULL) {
		pr_err("lock_hw_for_enc invalid parameter!\n");
		return VAL_RESULT_INVALID_PARAMETER;
	}

	ulLockHandle = (unsigned long)(pHWLock->pvHandle);
	eValRet = VAL_RESULT_NO_ERROR;

	while (*pbLockedHW == VAL_FALSE) {
		/* Early break for JPEG VENC */
		if (pHWLock->u4TimeoutMs == 0 &&
		grVEncHWLock.pvHandle != 0)
			break;

		/* Wait to acquire Enc HW lock */
		mutex_lock(&EncHWLockEventTimeoutLock);
		if (EncHWLockEvent.u4TimeoutMs == 1) {
			pr_debug("[NOT ERROR][VCODEC_LOCKHW] ENC First Use HW %d!!\n",
			pHWLock->eDriverType);
			FirstUseDecHW = 1;
		} else {
			FirstUseDecHW = 0;
		}
		mutex_unlock(&EncHWLockEventTimeoutLock);
		if (FirstUseDecHW == 1) {
			eValRet = eVideoWaitEvent(&EncHWLockEvent,
				sizeof(struct VAL_EVENT_T));
		}

		mutex_lock(&EncHWLockEventTimeoutLock);
		if (EncHWLockEvent.u4TimeoutMs == 1) {
			EncHWLockEvent.u4TimeoutMs = 1000;
			FirstUseDecHW = 1;
		} else {
			FirstUseDecHW = 0;
			if (pHWLock->u4TimeoutMs == 0)
				EncHWLockEvent.u4TimeoutMs = 0;
			/* No wait */
			else
				EncHWLockEvent.u4TimeoutMs = 1000;
			/* Wait indefinitely */
		}
		mutex_unlock(&EncHWLockEventTimeoutLock);

		mutex_lock(&VencHWLock);
		/* one process try to lock twice */
		if (grVEncHWLock.pvHandle ==
			(void *)pmem_user_v2p_video(ulLockHandle)) {
			pr_err("[VCODEC_LOCKHW] [WARNING] one ENC instance try to lock twice, may cause lock HW timeout!! instance = 0x%lx, CurrentTID = %d, type:%d\n",
			(unsigned long)grVEncHWLock.pvHandle,
			current->pid, pHWLock->eDriverType);
		}
		mutex_unlock(&VencHWLock);

		if (FirstUseDecHW == 0) {
			eValRet = eVideoWaitEvent(&EncHWLockEvent,
				sizeof(struct VAL_EVENT_T));
		}

		if ((eValRet == VAL_RESULT_INVALID_ISR) &&
		(FirstUseDecHW != 1)) {
			pr_err("[VCODEC_LOCKHW] [ERROR] EncHWLockEvent TimeOut, CurrentTID = %d\n",
				current->pid);
			mutex_lock(&VencHWLock);
			if (grVEncHWLock.pvHandle == 0) {
				pr_err("[VCODEC_LOCKHW] [WARNING] ENC maybe mediaserver restart before, please check!!\n");
			} else {
				pr_err("[VCODEC_LOCKHW] [WARNING] ENC someone use HW, and check timeout value!! %d\n",
					gLockTimeOutCount);
				++gLockTimeOutCount;
				if (gLockTimeOutCount > 30) {
					pr_err("[ERROR] VCODEC_LOCKHW - ID %d  fail, someone locked HW time out more than 30 times 0x%lx, %lx, 0x%lx, type:%d\n",
					 current->pid,
					 (unsigned long)grVEncHWLock.pvHandle,
					 pmem_user_v2p_video(ulLockHandle),
					 ulLockHandle,
					 pHWLock->eDriverType);
					gLockTimeOutCount = 0;
					mutex_unlock(&VencHWLock);
					return -EFAULT;
				}

				if (pHWLock->u4TimeoutMs == 0) {
					pr_err("[VCODEC_LOCKHW] ENC - ID %d  fail, someone locked HW already 0x%lx, %lx, 0x%lx, type:%d\n",
					current->pid,
					(unsigned long)grVEncHWLock.pvHandle,
					pmem_user_v2p_video(ulLockHandle),
					ulLockHandle,
					pHWLock->eDriverType);
					gLockTimeOutCount = 0;
					mutex_unlock(&VencHWLock);
					return -EFAULT;
				}
			}
			mutex_unlock(&VencHWLock);
		} else if (eValRet == VAL_RESULT_RESTARTSYS) {
		return -ERESTARTSYS;
		}

		mutex_lock(&VencHWLock);
		if ((grVEncHWLock.pvHandle == 0)  &&
		/* No process use HW, so current process can use HW */
		(pHWLock->eDriverType == VAL_DRIVER_TYPE_H264_ENC
		|| pHWLock->eDriverType == VAL_DRIVER_TYPE_VP8_ENC
		|| pHWLock->eDriverType == VAL_DRIVER_TYPE_HEVC_ENC
		|| pHWLock->eDriverType == VAL_DRIVER_TYPE_JPEG_ENC)) {
			grVEncHWLock.pvHandle =
			(void *)pmem_user_v2p_video(ulLockHandle);
			pr_debug("[VCODEC_LOCKHW] ENC No process use HW,so current process can use HW, handle = 0x%lx\n",
				(unsigned long)grVEncHWLock.pvHandle);
			grVEncHWLock.eDriverType =
			pHWLock->eDriverType;
			eVideoGetTimeOfDay
			(&grVEncHWLock.rLockedTime,
			sizeof(struct VAL_TIME_T));

			pr_debug("[VCODEC_LOCKHW] ENC No process use HW, so current process can use HW\n");
			pr_debug("[VCODEC_LOCKHW] ENC LockInstance = 0x%lx CurrentTID = %d, rLockedTime(s, us) = %d, %d\n",
				(unsigned long)grVEncHWLock.pvHandle,
				current->pid,
				grVEncHWLock.rLockedTime.u4Sec,
				grVEncHWLock.rLockedTime.u4uSec);

			*pbLockedHW = VAL_TRUE;
			venc_enableIRQ(pHWLock);
		} else {
			/* someone use HW, and check timeout value */
			if (pHWLock->u4TimeoutMs == 0) {
				*pbLockedHW = VAL_FALSE;
				mutex_unlock(&VencHWLock);
				break;
			}

			eVideoGetTimeOfDay(&rCurTime,
				sizeof(struct VAL_TIME_T));
			u4TimeInterval = (((((rCurTime.u4Sec -
				grVEncHWLock.rLockedTime.u4Sec) *
				1000000) + rCurTime.u4uSec)
				-grVEncHWLock.rLockedTime.u4uSec) / 1000);

			pr_debug("[VCODEC_LOCKHW] ENC someone use HW, and check timeout value\n");
			pr_debug("[VCODEC_LOCKHW] ENC LockInstance = 0x%lx,CurrentInstance = 0x%lx, CurrentTID = %d, TimeInterval(ms) = %d, TimeOutValue(ms)) = %d\n",
				(unsigned long)grVEncHWLock.pvHandle,
				pmem_user_v2p_video(ulLockHandle),
				current->pid, u4TimeInterval,
				pHWLock->u4TimeoutMs);

			pr_debug("[VCODEC_LOCKHW] ENC LockInstance = 0x%lx, CurrentInstance = 0x%lx, CurrentTID = %d, rLockedTime(s, us) = %d, %d, rCurTime(s, us) = %d, %d\n",
				(unsigned long)grVEncHWLock.pvHandle,
				pmem_user_v2p_video(ulLockHandle),
				current->pid,
				grVEncHWLock.rLockedTime.u4Sec,
				grVEncHWLock.rLockedTime.u4uSec,
				rCurTime.u4Sec, rCurTime.u4uSec);

			++gLockTimeOutCount;
			if (gLockTimeOutCount > 30) {
				pr_err("[VCODEC_LOCKHW] ENC - ID %d  fail, someone locked HW over 30 times without timeout 0x%lx, %lx, 0x%lx, type:%d\n",
				current->pid,
				(unsigned long)grVEncHWLock.pvHandle,
				pmem_user_v2p_video(ulLockHandle),
				ulLockHandle,
				pHWLock->eDriverType);
				gLockTimeOutCount = 0;
				mutex_unlock(&VencHWLock);
				return -EFAULT;
			}
			/* 2013/04/10. */
			/*Cheng-Jung Never steal hardware lock */
		}

		if (*pbLockedHW == VAL_TRUE) {
			pr_debug("[VCODEC_LOCKHW] ENC Lock ok grVEncHWLock.pvHandle = 0x%lx, va:%lx, type:%d",
			(unsigned long)grVEncHWLock.pvHandle,
			ulLockHandle,
			pHWLock->eDriverType);
			gLockTimeOutCount = 0;
		}
		mutex_unlock(&VencHWLock);
	}

	if (*pbLockedHW == VAL_FALSE) {
		pr_err("[VCODEC_LOCKHW] ENC - ID %d  fail, someone locked HW already, 0x%lx, %lx, 0x%lx, type:%d\n",
			current->pid,
			(unsigned long)grVEncHWLock.pvHandle,
			pmem_user_v2p_video(ulLockHandle),
			ulLockHandle,
			pHWLock->eDriverType);
		gLockTimeOutCount = 0;
		return -EFAULT;
	}

	spin_lock_irqsave(&LockEncHWCountLock, ulFlagLockEncHW);
	gu4LockEncHWCount++;
	spin_unlock_irqrestore(&LockEncHWCountLock, ulFlagLockEncHW);

	pr_debug("[VCODEC_LOCKHW] ENC get locked - ObjId =%d\n",
		current->pid);

	pr_debug("[VCODEC_LOCKHW] ENC - tid = %d\n", current->pid);
	return eValRet;
}

static long vcodec_lockhw(unsigned long arg)
{
	unsigned char *user_data_addr;
	char bLockedHW = VAL_FALSE;
	long ret;
	enum VAL_RESULT_T eValRet;
	struct VAL_HW_LOCK_T rHWLock;

	user_data_addr = (unsigned char *)arg;
	ret = copy_from_user(&rHWLock, user_data_addr,
		sizeof(struct VAL_HW_LOCK_T));
	if (ret) {
		pr_debug("[VCODEC] LOCKHW copy_from_user failed: %lu\n",
				ret);
		return -EFAULT;
	}

	pr_debug("[VCODEC] LOCKHW eDriverType = %d\n",
			rHWLock.eDriverType);
	eValRet = VAL_RESULT_INVALID_ISR;
	if (rHWLock.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_ADV_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VP9_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_DEC) {
		eValRet = vcodec_lockhw_vdec(&rHWLock, &bLockedHW);
		if (eValRet != VAL_RESULT_NO_ERROR)
			return eValRet;
	} else if (rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_ENC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC) {
		ret = vcodec_lockhw_venc(&rHWLock, &bLockedHW);
		if (ret != VAL_RESULT_NO_ERROR) {
			/* If there is error, return immediately */
			return ret;
		}
	} else {
		pr_err("[VCODEC_LOCKHW] [WARNING] Unknown instance\n");
		return -EFAULT;
	}

	return 0;
}

static long vcodec_unlockhw(unsigned long arg)
{
	unsigned long handle = 0;
	long ret;
	unsigned char *user_data_addr;
	struct VAL_HW_LOCK_T rHWLock;
	enum VAL_RESULT_T eValRet;

	pr_debug("[8163] VCODEC_UNLOCKHW + tid = %d\n",
		current->pid);
	user_data_addr = (unsigned char *) arg;
	ret = copy_from_user(&rHWLock, user_data_addr,
		sizeof(struct VAL_HW_LOCK_T));
	if (ret) {
		pr_err("UNLOCKHW failed: %lu\n",
			 ret);
		return -EFAULT;
	}

	pr_debug("UNLOCKHW eDriverType = %d\n",
		rHWLock.eDriverType);
	eValRet = VAL_RESULT_INVALID_ISR;
	handle = (unsigned long)rHWLock.pvHandle;
	if (rHWLock.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VC1_ADV_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VP9_DEC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_DEC) {
		mutex_lock(&VdecHWLock);
		if (grVDecHWLock.pvHandle ==
			(void *) pmem_user_v2p_video(handle)) {
			grVDecHWLock.pvHandle = 0;
			grVDecHWLock.eDriverType =
				VAL_DRIVER_TYPE_NONE;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT	/* Morris Yang moved to TEE */
			if (rHWLock.bSecureInst == VAL_FALSE) {
				/* disable_irq(VDEC_IRQ_ID); */

				free_irq(VDEC_IRQ_ID, NULL);
			}
#else
			disable_irq(VDEC_IRQ_ID);
#endif
/* TODO: check if turning power off is ok */
			vdec_power_off();
#ifdef ENABLE_MMDVFS_VDEC
			VdecDvfsAdjustment();
#endif
		} else {	/* Not current owner */

			pr_debug("Not owner trying to unlock dec hardware 0x%lx\n",
			pmem_user_v2p_video
			(handle));
			mutex_unlock(&VdecHWLock);
			return -EFAULT;
		}
		mutex_unlock(&VdecHWLock);
		eValRet = eVideoSetEvent(&DecHWLockEvent,
			sizeof(struct VAL_EVENT_T));
	} else if (rHWLock.eDriverType ==
		VAL_DRIVER_TYPE_H264_ENC ||
		rHWLock.eDriverType == VAL_DRIVER_TYPE_VP8_ENC ||
#ifdef CONFIG_MTK_VIDEO_HEVC_SUPPORT
		rHWLock.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC ||
#endif
		rHWLock.eDriverType == VAL_DRIVER_TYPE_JPEG_ENC) {
		mutex_lock(&VencHWLock);
		if (grVEncHWLock.pvHandle ==
			(void *) pmem_user_v2p_video(handle)) {
			grVEncHWLock.pvHandle = 0;
			grVEncHWLock.eDriverType =
				VAL_DRIVER_TYPE_NONE;
			venc_disableIRQ(&rHWLock);
		} else {
			/* Not current owner */
			/* [TODO] error handling */
			pr_err("Not owner trying to unlock enc hardware 0x%lx\n",
				(unsigned long) grVEncHWLock.pvHandle);
			pr_err("[ERROR] pa:%lx, va:%lx type:%d\n",
				pmem_user_v2p_video(handle),
				handle, rHWLock.eDriverType);
			mutex_unlock(&VencHWLock);
			return -EFAULT;
		}
		mutex_unlock(&VencHWLock);
		eValRet = eVideoSetEvent(&EncHWLockEvent,
			sizeof(struct VAL_EVENT_T));
	} else {
		pr_err("[WARNING] Unknown instance\n");
		return -EFAULT;
	}
	pr_debug("[8163] VCODEC_UNLOCKHW - tid = %d\n",
	current->pid);
	return 0;
}

static long vcodec_waitisr(unsigned long arg)
{
	unsigned char *user_data_addr;
	char bLockedHW = VAL_FALSE;
	unsigned long ulFlags;
	unsigned long handle;
	long ret;
	struct VAL_ISR_T val_isr;
	enum VAL_RESULT_T eValRet;

	pr_debug("[8163] VCODEC_WAITISR + tid = %d\n",
		current->pid);
	user_data_addr = (unsigned char *) arg;
	ret = copy_from_user(&val_isr,
		user_data_addr, sizeof(struct VAL_ISR_T));
	if (ret) {
		pr_err("[ERROR] VCODEC_WAITISR, failed: %lu\n",
		ret);
		return -EFAULT;
	}

	handle = (unsigned long)val_isr.pvHandle;

	if (val_isr.eDriverType == VAL_DRIVER_TYPE_MP4_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_HEVC_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_H264_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_MP1_MP2_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_VC1_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_VC1_ADV_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_VP8_DEC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_VP9_DEC) {
		mutex_lock(&VdecHWLock);
		if (grVDecHWLock.pvHandle ==
		    (void *) pmem_user_v2p_video(handle)) {
			bLockedHW = VAL_TRUE;
		} else {
		}
		mutex_unlock(&VdecHWLock);

		if (bLockedHW == VAL_FALSE) {
			pr_err("[ERROR] DO NOT have HWLock, so return fail\n");
			return -EFAULT;
		}

		spin_lock_irqsave(&DecIsrLock, ulFlags);
		DecIsrEvent.u4TimeoutMs = val_isr.u4TimeoutMs;
		spin_unlock_irqrestore(&DecIsrLock, ulFlags);

		eValRet = eVideoWaitEvent(&DecIsrEvent,
			sizeof(struct VAL_EVENT_T));
		if (eValRet == VAL_RESULT_INVALID_ISR) {
			return -2;
		} else if (eValRet == VAL_RESULT_RESTARTSYS) {
			pr_err("[WARNING] VAL_RESULT_RESTARTSYS !!\n");
			return -ERESTARTSYS;
		}
	} else if (val_isr.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_VP8_ENC ||
		val_isr.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC) {
		mutex_lock(&VencHWLock);
		if (grVEncHWLock.pvHandle ==
		    (void *) pmem_user_v2p_video(handle)) {
			bLockedHW = VAL_TRUE;
		} else {
		}
		mutex_unlock(&VencHWLock);

		if (bLockedHW == VAL_FALSE) {
			pr_err("DO NOT have enc HWLock, pa:%lx, va:%lx\n",
				pmem_user_v2p_video(handle),
				handle);
			return -EFAULT;
		}

		spin_lock_irqsave(&EncIsrLock, ulFlags);
		EncIsrEvent.u4TimeoutMs = val_isr.u4TimeoutMs;
		spin_unlock_irqrestore(&EncIsrLock, ulFlags);

		eValRet = eVideoWaitEvent(&EncIsrEvent,
			sizeof(struct VAL_EVENT_T));
		if (eValRet == VAL_RESULT_INVALID_ISR) {
			return -2;
		} else if (eValRet == VAL_RESULT_RESTARTSYS) {
			pr_err("return when WAITISR!!\n");
			return -ERESTARTSYS;
		}

		if (val_isr.u4IrqStatusNum > 0) {
			val_isr.u4IrqStatus[0] =
				gu4HwVencIrqStatus;
			ret = copy_to_user(user_data_addr,
				&val_isr,
				sizeof(struct VAL_ISR_T));
			if (ret) {
				pr_err("WAITISR, copy_to_user failed: %lu\n",
				     ret);
				return -EFAULT;
			}
		}
	} else {
		pr_err("[WARNING] VCODEC_WAITISR Unknown instance\n");
		return -EFAULT;
	}
	pr_debug("[8163] VCODEC_WAITISR - tid = %d\n",
		current->pid);
	return 0;
}

static long vcodec_unlocked_ioctl
	(struct file *file, unsigned int cmd, unsigned long arg)
{
	signed long ret;
	unsigned char *user_data_addr;
	struct VAL_VCODEC_CORE_LOADING_T rTempCoreLoading;
	struct VAL_VCODEC_CPU_OPP_LIMIT_T rCpuOppLimit;
	signed int temp_nr_cpu_ids;
	struct VAL_POWER_T rPowerParam;
	struct VAL_MEMORY_T rTempMem;

	switch (cmd) {
	case VCODEC_SET_THREAD_ID:
		pr_debug("[8163] SET_THREAD_ID [EMPTY] + tid = %d\n",
			current->pid);

		pr_debug("[8163] SET_THREAD_ID [EMPTY] - tid = %d\n",
			current->pid);
		break;

	case VCODEC_ALLOC_NON_CACHE_BUFFER: {
		pr_err("[8163][M4U]! VCODEC_ALLOC_NON_CACHE_BUFFER + tid = %d\n",
			 current->pid);

		user_data_addr = (unsigned char *) arg;
		ret = copy_from_user(&rTempMem, user_data_addr,
			sizeof(struct VAL_MEMORY_T));
		if (ret) {
			pr_err("[ERROR] VCODEC_ALLOC_NON_CACHE_BUFFER, copy_from_user failed: %lu\n",
				ret);
			return -EFAULT;
		}

		rTempMem.u4ReservedSize /*kernel va */ =
			(unsigned long)dma_alloc_coherent(
				0, rTempMem.u4MemSize,
				(dma_addr_t *)&rTempMem.pvMemPa, GFP_KERNEL);
		if ((rTempMem.u4ReservedSize == 0) || (rTempMem.pvMemPa == 0)) {
			pr_err("[ERROR] dma_alloc_coherent fail in VCODEC_ALLOC_NON_CACHE_BUFFER\n");
			return -EFAULT;
		}

		pr_debug("kernel va = 0x%lx, kernel pa = 0x%lx, memory size = %lu\n",
			(unsigned long)rTempMem.u4ReservedSize,
			(unsigned long)rTempMem.pvMemPa,
			(unsigned long)rTempMem.u4MemSize);

		ret = copy_to_user(user_data_addr, &rTempMem,
			sizeof(struct VAL_MEMORY_T));
		if (ret) {
			pr_err("[ERROR] VCODEC_ALLOC_NON_CACHE_BUFFER, copy_to_user failed: %lu\n",
				ret);
			return -EFAULT;
		}

		pr_err("[8163][M4U]! VCODEC_ALLOC_NON_CACHE_BUFFER - tid = %d\n",
			current->pid);
	} break;

	case VCODEC_FREE_NON_CACHE_BUFFER: {
		pr_err("[8163][M4U]! VCODEC_FREE_NON_CACHE_BUFFER + tid = %d\n",
			current->pid);

		user_data_addr = (unsigned char *) arg;
		ret = copy_from_user(&rTempMem, user_data_addr,
			sizeof(struct VAL_MEMORY_T));
		if (ret) {
			pr_err("[ERROR] VCODEC_FREE_NON_CACHE_BUFFER, copy_from_user failed: %lu\n",
				ret);
			return -EFAULT;
		}

		dma_free_coherent(0, rTempMem.u4MemSize,
				  (void *)rTempMem.u4ReservedSize,
				  (dma_addr_t)rTempMem.pvMemPa);

		rTempMem.u4ReservedSize = 0;
		rTempMem.pvMemPa = NULL;

		ret = copy_to_user(user_data_addr, &rTempMem,
			sizeof(struct VAL_MEMORY_T));
		if (ret) {
			pr_err("[ERROR] VCODEC_FREE_NON_CACHE_BUFFER, copy_to_user failed: %lu\n",
				ret);
			return -EFAULT;
		}

		pr_err("[8163][M4U]! VCODEC_FREE_NON_CACHE_BUFFER - tid = %d\n",
			current->pid);
	} break;

	case VCODEC_INC_DEC_EMI_USER:
		pr_debug("[8163] VCODEC_INC_DEC_EMI_USER + tid = %d\n",
				current->pid);

		mutex_lock(&DecEMILock);
		gu4DecEMICounter++;
		pr_debug("DEC_EMI_USER = %d\n", gu4DecEMICounter);
		user_data_addr = (unsigned char *) arg;
		ret = copy_to_user(user_data_addr, &gu4DecEMICounter,
				   sizeof(unsigned int));
		if (ret) {
			pr_err("[ERROR] VCODEC_INC_DEC_EMI_USER, copy_to_user failed: %lu\n",
				ret);
			mutex_unlock(&DecEMILock);
			return -EFAULT;
		}
		mutex_unlock(&DecEMILock);

		pr_debug("[8163] VCODEC_INC_DEC_EMI_USER - tid = %d\n",
				current->pid);
		break;

	case VCODEC_DEC_DEC_EMI_USER:
		pr_debug("[8163] VCODEC_DEC_DEC_EMI_USER + tid = %d\n",
				current->pid);

		mutex_lock(&DecEMILock);
		gu4DecEMICounter--;
		pr_err("DEC_EMI_USER = %d\n", gu4DecEMICounter);
		user_data_addr = (unsigned char *) arg;
		ret = copy_to_user(user_data_addr, &gu4DecEMICounter,
				   sizeof(unsigned int));
		if (ret) {
			pr_err("[ERROR] DEC_DEC_EMI_USER, copy_to_user failed: %lu\n",
				ret);
			mutex_unlock(&DecEMILock);
			return -EFAULT;
		}
		mutex_unlock(&DecEMILock);

		pr_debug("[8163] VCODEC_DEC_DEC_EMI_USER - tid = %d\n",
				current->pid);
		break;

	case VCODEC_INC_ENC_EMI_USER:
		pr_debug("[8163] VCODEC_INC_ENC_EMI_USER + tid = %d\n",
				current->pid);

		mutex_lock(&EncEMILock);
		gu4EncEMICounter++;
		pr_debug("ENC_EMI_USER = %d\n", gu4EncEMICounter);
		user_data_addr = (unsigned char *) arg;
		ret = copy_to_user(user_data_addr, &gu4EncEMICounter,
				   sizeof(unsigned int));
		if (ret) {
			pr_err("[ERROR] VCODEC_INC_ENC_EMI_USER, copy_to_user failed: %lu\n",
				ret);
			mutex_unlock(&EncEMILock);
			return -EFAULT;
		}
		mutex_unlock(&EncEMILock);

		pr_debug("[8163] VCODEC_INC_ENC_EMI_USER - tid = %d\n",
				current->pid);
		break;

	case VCODEC_DEC_ENC_EMI_USER: {
		pr_debug("[8163] VCODEC_DEC_ENC_EMI_USER + tid = %d\n",
				current->pid);

		mutex_lock(&EncEMILock);
		gu4EncEMICounter--;
		pr_err("ENC_EMI_USER = %d\n", gu4EncEMICounter);
		user_data_addr = (unsigned char *) arg;
		ret = copy_to_user(user_data_addr, &gu4EncEMICounter,
				   sizeof(unsigned int));
		if (ret) {
			pr_err("[ERROR] DEC_ENC_EMI_USER, copy_to_user failed: %lu\n",
				ret);
			mutex_unlock(&EncEMILock);
			return -EFAULT;
		}
		mutex_unlock(&EncEMILock);

		pr_debug("[8163] VCODEC_DEC_ENC_EMI_USER - tid = %d\n",
				current->pid);
	} break;

	case VCODEC_LOCKHW: {
		ret = vcodec_lockhw(arg);
		if (ret) {
			pr_debug("[ERROR] VCODEC_LOCKHW failed! %lu\n",
				ret);
			return ret;
		}
	} break;

	case VCODEC_UNLOCKHW: {
		ret = vcodec_unlockhw(arg);
		if (ret) {
			pr_err("[ERROR] VCODEC_UNLOCKHW failed! %lu\n",
			ret);
			return ret;
		}
	} break;

	case VCODEC_INC_PWR_USER: {
		pr_debug("[8163] VCODEC_INC_PWR_USER + tid = %d\n",
				current->pid);
		user_data_addr = (unsigned char *) arg;
		ret = copy_from_user(&rPowerParam, user_data_addr,
			sizeof(struct VAL_POWER_T));
		if (ret) {
			pr_err("[ERROR] INC_PWR_USER, failed: %lu\n",
				ret);
			return -EFAULT;
		}
		pr_debug("INC_PWR_USER eDriverType = %d\n",
				rPowerParam.eDriverType);
		mutex_lock(&L2CLock);

#ifdef MT8163_VENC_USE_L2C
		if (rPowerParam.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
		    val_isr.eDriverType == VAL_DRIVER_TYPE_VP8_ENC ||
		    val_isr.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC) {
			gu4L2CCounter++;
			pr_debug("INC_PWR_USER L2C counter = %d\n",
					gu4L2CCounter);

			if (gu4L2CCounter == 1) {
				if (config_L2(0)) {
					pr_err("[ERROR] Switch size to 512K failed\n");
					mutex_unlock(&L2CLock);
					return -EFAULT;
				}
			}
		}
#endif
		mutex_unlock(&L2CLock);
		pr_debug("[8163] VCODEC_INC_PWR_USER - tid = %d\n",
				current->pid);
	} break;

	case VCODEC_DEC_PWR_USER: {
		pr_debug("[8163] VCODEC_DEC_PWR_USER + tid = %d\n",
				current->pid);
		user_data_addr = (unsigned char *) arg;
		ret = copy_from_user(&rPowerParam, user_data_addr,
			sizeof(struct VAL_POWER_T));
		if (ret) {
			pr_err("[ERROR] VCODEC_DEC_PWR_USER,failed: %lu\n",
				ret);
			return -EFAULT;
		}
		pr_debug("DEC_PWR_USER eDriverType = %d\n",
				rPowerParam.eDriverType);

		mutex_lock(&L2CLock);

#ifdef MT8163_VENC_USE_L2C
		if (rPowerParam.eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
		    val_isr.eDriverType == VAL_DRIVER_TYPE_VP8_ENC ||
		    val_isr.eDriverType == VAL_DRIVER_TYPE_HEVC_ENC) {
			gu4L2CCounter--;
			pr_debug("DEC_PWR_USER L2C counter  = %d\n",
					gu4L2CCounter);

			if (gu4L2CCounter == 0) {
				if (config_L2(1)) {
					pr_err("[ERROR] Switch size to 0K failed\n");
					mutex_unlock(&L2CLock);
					return -EFAULT;
				}
			}
		}
#endif
		mutex_unlock(&L2CLock);
		pr_debug("[8163] VCODEC_DEC_PWR_USER - tid = %d\n",
				current->pid);
	} break;

	case VCODEC_WAITISR: {
		ret = vcodec_waitisr(arg);
		if (ret) {
			pr_err("[ERROR] VCODEC_WAITISR failed! %lu\n", ret);
			return ret;
		}
	} break;

	case VCODEC_INITHWLOCK: {
		pr_err("[8163] VCODEC_INITHWLOCK [EMPTY] + - tid = %d\n",
			current->pid);

		pr_err("[8163] VCODEC_INITHWLOCK [EMPTY] - - tid = %d\n",
			current->pid);
	} break;

	case VCODEC_DEINITHWLOCK: {
		pr_err("[8163] VCODEC_DEINITHWLOCK [EMPTY] + - tid = %d\n",
			current->pid);

		pr_err("[8163] VCODEC_DEINITHWLOCK [EMPTY] - - tid = %d\n",
			current->pid);
	} break;

	case VCODEC_GET_CPU_LOADING_INFO: {
		unsigned char *user_data_addr;
		struct VAL_VCODEC_CPU_LOADING_INFO_T _temp = {0};

		pr_debug("GET_CPU_LOADING_INFO +\n");
		user_data_addr = (unsigned char *) arg;
/* TODO: */
#if 0 /* Morris Yang 20120112 mark temporarily */
			_temp._cpu_idle_time = mt_get_cpu_idle(0);
			_temp._thread_cpu_time = mt_get_thread_cputime(0);
			spin_lock_irqsave(&OalHWContextLock, ulFlags);
			_temp._inst_count = getCurInstanceCount();
			spin_unlock_irqrestore(&OalHWContextLock, ulFlags);
			_temp._sched_clock = mt_sched_clock();
#endif
		ret = copy_to_user(user_data_addr, &_temp,
			sizeof(struct VAL_VCODEC_CPU_LOADING_INFO_T));
		if (ret) {
			pr_err("GET_CPU_LOADING_INFO, failed: %lu\n",
				ret);
			return -EFAULT;
		}

		pr_debug("[8163] VCODEC_GET_CPU_LOADING_INFO -\n");
	} break;

	case VCODEC_GET_CORE_LOADING: {
		pr_debug("[8163] VCODEC_GET_CORE_LOADING + - tid = %d\n",
				current->pid);

		user_data_addr = (unsigned char *) arg;
		ret = copy_from_user(&rTempCoreLoading, user_data_addr,
			sizeof(struct VAL_VCODEC_CORE_LOADING_T));
		if (ret) {
			pr_err("[ERROR] VCODEC_GET_CORE_LOADING, copy_from_user failed: %lu\n",
				ret);
			return -EFAULT;
		}
		if (rTempCoreLoading.CPUid < 0) {
			pr_err("[ERROR] rTempCoreLoading.CPUid < 0\n");
			return -EFAULT;
		}

		if (rTempCoreLoading.CPUid > num_possible_cpus()) {
			pr_err("[ERROR] rTempCoreLoading.CPUid(%d) > num_possible_cpus(%d)\n",
				rTempCoreLoading.CPUid, num_possible_cpus());
			return -EFAULT;
		}
		/* tempory remark, must enable after function check-in */
		/* rTempCoreLoading.Loading =
		 * get_cpu_load(rTempCoreLoading.CPUid);
		 */
		ret = copy_to_user(user_data_addr, &rTempCoreLoading,
			sizeof(struct VAL_VCODEC_CORE_LOADING_T));
		if (ret) {
			pr_err("[ERROR] VCODEC_GET_CORE_LOADING, copy_to_user failed: %lu\n",
				ret);
			return -EFAULT;
		}

		pr_debug("[8163 VCODEC_GET_CORE_LOADING - - tid = %d\n",
				current->pid);
	} break;

	case VCODEC_GET_CORE_NUMBER: {
		pr_debug("[8163] VCODEC_GET_CORE_NUMBER + - tid = %d\n",
				current->pid);

		user_data_addr = (unsigned char *) arg;
		temp_nr_cpu_ids = nr_cpu_ids;
		ret = copy_to_user(user_data_addr, &temp_nr_cpu_ids,
				   sizeof(int));
		if (ret) {
			pr_err("[ERROR] VCODEC_GET_CORE_NUMBER, copy_to_user failed: %lu\n",
				ret);
			return -EFAULT;
		}
		pr_debug("[8163] VCODEC_GET_CORE_NUMBER - - tid = %d\n",
				current->pid);
	} break;
	case VCODEC_SET_CPU_OPP_LIMIT: {
		pr_debug("SET_CPU_OPP_LIMIT [EMPTY] + - tid = %d\n",
			current->pid);
		user_data_addr = (unsigned char *) arg;
		ret = copy_from_user(&rCpuOppLimit, user_data_addr,
			sizeof(struct VAL_VCODEC_CPU_OPP_LIMIT_T));
		if (ret) {
			pr_err("[ERROR] VCODEC_SET_CPU_OPP_LIMIT, copy_from_user failed: %lu\n",
				ret);
			return -EFAULT;
		}
		pr_debug("+SET_CPU_OPP_LIMIT (%d, %d, %d), tid = %d\n",
			rCpuOppLimit.limited_freq, rCpuOppLimit.limited_cpu,
			rCpuOppLimit.enable, current->pid);
		/* TODO: Check if cpu_opp_limit is available */
		/* ret = cpu_opp_limit(EVENT_VIDEO, rCpuOppLimit.limited_freq,
		 * rCpuOppLimit.limited_cpu, rCpuOppLimit.enable); // 0: PASS,
		 * other: FAIL
		 */
		if (ret) {
			pr_err("[ERROR] cpu_opp_limit failed: %lu\n",
					ret);
			return -EFAULT;
		}
		pr_err("-SET_CPU_OPP_LIMIT tid = %d, ret = %lu\n",
			current->pid, ret);
		pr_debug("[8163] VCODEC_SET_CPU_OPP_LIMIT [EMPTY] - - tid = %d\n",
			current->pid);
	} break;
	case VCODEC_MB: {
		mb();	/*memory barrier*/
	} break;
	default:
		pr_err("========[ERROR] vcodec_ioctl default case======== %u\n",
			cmd);
		break;
	}
	return 0xFF;
}

#if IS_ENABLED(CONFIG_COMPAT)

enum STRUCT_TYPE {
	VAL_HW_LOCK_TYPE = 0,
	VAL_POWER_TYPE,
	VAL_ISR_TYPE,
	VAL_MEMORY_TYPE
};

enum COPY_DIRECTION {
	COPY_FROM_USER = 0,
	COPY_TO_USER,
};

struct COMPAT_VAL_HW_LOCK {
	compat_uptr_t pvHandle;
		/* /< [IN]	The video codec driver handle */
	compat_uint_t u4HandleSize;
		/* /< [IN]	The size of video codec driver handle */
	compat_uptr_t pvLock;
		/* /< [IN/OUT]	The Lock discriptor */
	compat_uint_t u4TimeoutMs;
		/* /< [IN]	The timeout ms */
	compat_uptr_t pvReserved;
		/* /< [IN/OUT] The reserved parameter */
	compat_uint_t u4ReservedSize;
		/* /< [IN]	The size of reserved parameter structure */
	compat_uint_t eDriverType;
		/* /< [IN]	The driver type */
	char bSecureInst;
		/* /< [IN]     True if this is a secure instance */
		/* MTK_SEC_VIDEO_PATH_SUPPORT */
};

struct COMPAT_VAL_POWER {
	compat_uptr_t pvHandle;
		/* /< [IN]	The video codec driver handle */
	compat_uint_t u4HandleSize;
		/* /< [IN]	The size of video codec driver handle */
	compat_uint_t eDriverType;
		/* /< [IN]	The driver type */
	char fgEnable;
		/* /< [IN]	Enable or not. */
	compat_uptr_t pvReserved;
		/* /< [IN/OUT] The reserved parameter */
	compat_uint_t u4ReservedSize;
		/* /< [IN]	The size of reserved parameter structure */
	/* unsigned int        u4L2CUser; */
		/*< [OUT]    The number of power user right now */
};

struct COMPAT_VAL_ISR {
	compat_uptr_t pvHandle;
		/* /< [IN]	The video codec driver handle */
	compat_uint_t u4HandleSize;
		/* /< [IN]	The size of video codec driver handle */
	compat_uint_t eDriverType;
		/* /< [IN]	The driver type */
	compat_uptr_t pvIsrFunction;
		/* /< [IN]	The isr function */
	compat_uptr_t pvReserved;
		/* /< [IN/OUT]	The reserved parameter */
	compat_uint_t u4ReservedSize;
		/* /< [IN]	The size of reserved parameter structure */
	compat_uint_t u4TimeoutMs;
		/* /< [IN]	The timeout in ms */
	compat_uint_t u4IrqStatusNum;
		/* /< [IN]	The num of return registers when HW done */
	compat_uint_t u4IrqStatus[IRQ_STATUS_MAX_NUM];
		/* /< [IN/OUT]	The value of return registers when HW done */
};

struct COMPAT_VAL_MEMORY {
	compat_uint_t eMemType;
		/* /< [IN]	The allocation memory type */
	compat_ulong_t u4MemSize;
		/* /< [IN]	The size of memory allocation */
	compat_uptr_t pvMemVa;
		/* /< [IN/OUT]	The memory virtual address */
	compat_uptr_t pvMemPa;
		/* /< [IN/OUT]	The memory physical address */
	compat_uint_t eAlignment;
		/* /< [IN]     The memory byte alignment setting */
	compat_uptr_t pvAlignMemVa;
		/* /< [IN/OUT]	The align memory virtual address */
	compat_uptr_t pvAlignMemPa;
		/* /< [IN/OUT]	The align memory physical address */
	compat_uint_t eMemCodec;
		/* /< [IN]	The memory codec for VENC or VDEC */
	compat_uint_t i4IonShareFd;
	compat_uptr_t pIonBufhandle;
	compat_uptr_t pvReserved;
		/* /< [IN/OUT]	The reserved parameter */
	compat_ulong_t u4ReservedSize;
	/* /< [IN]	The size of reserved parameter structure */
};

static int compat_copy_struct(enum STRUCT_TYPE eType,
	enum COPY_DIRECTION eDirection,
	void __user *data32, void __user *data)
{
	compat_uint_t u;
	compat_ulong_t l;
	compat_uptr_t p;
	void *ptr;
	char c;
	int err = 0;

	switch (eType) {
	case VAL_HW_LOCK_TYPE: {
		if (eDirection == COPY_FROM_USER) {
			struct COMPAT_VAL_HW_LOCK __user *from32 =
			    (struct COMPAT_VAL_HW_LOCK *) data32;
			struct VAL_HW_LOCK_T __user *to =
				(struct VAL_HW_LOCK_T *) data;

			err = get_user(p, &(from32->pvHandle));
			err |= put_user(compat_ptr(p), &(to->pvHandle));
			err |= get_user(u, &(from32->u4HandleSize));
			err |= put_user(u, &(to->u4HandleSize));
			err |= get_user(p, &(from32->pvLock));
			err |= put_user(compat_ptr(p), &(to->pvLock));
			err |= get_user(u, &(from32->u4TimeoutMs));
			err |= put_user(u, &(to->u4TimeoutMs));
			err |= get_user(p, &(from32->pvReserved));
			err |= put_user(compat_ptr(p), &(to->pvReserved));
			err |= get_user(u, &(from32->u4ReservedSize));
			err |= put_user(u, &(to->u4ReservedSize));
			err |= get_user(u, &(from32->eDriverType));
			err |= put_user(u, &(to->eDriverType));
			err |= get_user(c, &(from32->bSecureInst));
			err |= put_user(c, &(to->bSecureInst));
		} else {
			struct COMPAT_VAL_HW_LOCK __user *to32 =
				(struct COMPAT_VAL_HW_LOCK *) data32;
			struct VAL_HW_LOCK_T __user *from =
				(struct VAL_HW_LOCK_T *) data;

			err = get_user(ptr, &(from->pvHandle));
			err |= put_user(ptr_to_compat(ptr),
				&(to32->pvHandle));
			err |= get_user(u, &(from->u4HandleSize));
			err |= put_user(u, &(to32->u4HandleSize));
			err |= get_user(ptr, &(from->pvLock));
			err |= put_user(ptr_to_compat(ptr),
				&(to32->pvLock));
			err |= get_user(u, &(from->u4TimeoutMs));
			err |= put_user(u, &(to32->u4TimeoutMs));
			err |= get_user(ptr, &(from->pvReserved));
			err |= put_user(ptr_to_compat(ptr),
					&(to32->pvReserved));
			err |= get_user(u, &(from->u4ReservedSize));
			err |= put_user(u, &(to32->u4ReservedSize));
			err |= get_user(u, &(from->eDriverType));
			err |= put_user(u, &(to32->eDriverType));
			err |= get_user(c, &(from->bSecureInst));
			err |= put_user(c, &(to32->bSecureInst));
		}
	} break;
	case VAL_POWER_TYPE: {
		if (eDirection == COPY_FROM_USER) {
			struct COMPAT_VAL_POWER __user *from32 =
				(struct COMPAT_VAL_POWER *) data32;
			struct VAL_POWER_T __user *to =
				(struct VAL_POWER_T *) data;

			err = get_user(p, &(from32->pvHandle));
			err |= put_user(compat_ptr(p), &(to->pvHandle));
			err |= get_user(u, &(from32->u4HandleSize));
			err |= put_user(u, &(to->u4HandleSize));
			err |= get_user(u, &(from32->eDriverType));
			err |= put_user(u, &(to->eDriverType));
			err |= get_user(c, &(from32->fgEnable));
			err |= put_user(c, &(to->fgEnable));
			err |= get_user(p, &(from32->pvReserved));
			err |= put_user(compat_ptr(p),
				&(to->pvReserved));
			err |= get_user(u, &(from32->u4ReservedSize));
			err |= put_user(u, &(to->u4ReservedSize));
		} else {
			struct COMPAT_VAL_POWER __user *to32 =
				(struct COMPAT_VAL_POWER *) data32;
			struct VAL_POWER_T __user *from =
				(struct VAL_POWER_T *) data;

			err = get_user(ptr, &(from->pvHandle));
			err |= put_user(ptr_to_compat(ptr),
				&(to32->pvHandle));
			err |= get_user(u, &(from->u4HandleSize));
			err |= put_user(u, &(to32->u4HandleSize));
			err |= get_user(u, &(from->eDriverType));
			err |= put_user(u, &(to32->eDriverType));
			err |= get_user(c, &(from->fgEnable));
			err |= put_user(c, &(to32->fgEnable));
			err |= get_user(ptr, &(from->pvReserved));
			err |= put_user(ptr_to_compat(ptr),
					&(to32->pvReserved));
			err |= get_user(u, &(from->u4ReservedSize));
			err |= put_user(u, &(to32->u4ReservedSize));
		}
	} break;
	case VAL_ISR_TYPE: {
		int i = 0;

		if (eDirection == COPY_FROM_USER) {
			struct COMPAT_VAL_ISR __user *from32 =
				(struct COMPAT_VAL_ISR *) data32;
			struct VAL_ISR_T __user *to =
				(struct VAL_ISR_T *) data;

			err = get_user(p, &(from32->pvHandle));
			err |= put_user(compat_ptr(p), &(to->pvHandle));
			err |= get_user(u, &(from32->u4HandleSize));
			err |= put_user(u, &(to->u4HandleSize));
			err |= get_user(u, &(from32->eDriverType));
			err |= put_user(u, &(to->eDriverType));
			err |= get_user(p, &(from32->pvIsrFunction));
			err |= put_user(compat_ptr(p),
				&(to->pvIsrFunction));
			err |= get_user(p, &(from32->pvReserved));
			err |= put_user(compat_ptr(p),
				&(to->pvReserved));
			err |= get_user(u, &(from32->u4ReservedSize));
			err |= put_user(u, &(to->u4ReservedSize));
			err |= get_user(u, &(from32->u4TimeoutMs));
			err |= put_user(u, &(to->u4TimeoutMs));
			err |= get_user(u, &(from32->u4IrqStatusNum));
			err |= put_user(u, &(to->u4IrqStatusNum));
			for (; i < IRQ_STATUS_MAX_NUM; i++) {
				err |= get_user(u,
					&(from32->u4IrqStatus[i]));
				err |= put_user(u,
					&(to->u4IrqStatus[i]));
			}
		} else {
			struct COMPAT_VAL_ISR __user *to32 =
				(struct COMPAT_VAL_ISR *) data32;
			struct VAL_ISR_T __user *from =
				(struct VAL_ISR_T *) data;

			err = get_user(ptr, &(from->pvHandle));
			err |= put_user(ptr_to_compat(ptr),
				&(to32->pvHandle));
			err |= get_user(u, &(from->u4HandleSize));
			err |= put_user(u, &(to32->u4HandleSize));
			err |= get_user(u, &(from->eDriverType));
			err |= put_user(u, &(to32->eDriverType));
			err |= get_user(ptr, &(from->pvIsrFunction));
			err |= put_user(ptr_to_compat(ptr),
					&(to32->pvIsrFunction));
			err |= get_user(ptr, &(from->pvReserved));
			err |= put_user(ptr_to_compat(ptr),
					&(to32->pvReserved));
			err |= get_user(u, &(from->u4ReservedSize));
			err |= put_user(u, &(to32->u4ReservedSize));
			err |= get_user(u, &(from->u4TimeoutMs));
			err |= put_user(u, &(to32->u4TimeoutMs));
			err |= get_user(u, &(from->u4IrqStatusNum));
			err |= put_user(u, &(to32->u4IrqStatusNum));
			for (; i < IRQ_STATUS_MAX_NUM; i++) {
				err |= get_user(u, &(from->u4IrqStatus[i]));
				err |= put_user(u, &(to32->u4IrqStatus[i]));
			}
		}
	} break;
	case VAL_MEMORY_TYPE: {
		if (eDirection == COPY_FROM_USER) {
			struct COMPAT_VAL_MEMORY __user *from32 =
				(struct COMPAT_VAL_MEMORY *) data32;
			struct VAL_MEMORY_T __user *to =
				(struct VAL_MEMORY_T *) data;

			err = get_user(u, &(from32->eMemType));
			err |= put_user(u, &(to->eMemType));
			err |= get_user(l, &(from32->u4MemSize));
			err |= put_user(l, &(to->u4MemSize));
			err |= get_user(p, &(from32->pvMemVa));
			err |= put_user(compat_ptr(p), &(to->pvMemVa));
			err |= get_user(p, &(from32->pvMemPa));
			err |= put_user(compat_ptr(p), &(to->pvMemPa));
			err |= get_user(u, &(from32->eAlignment));
			err |= put_user(u, &(to->eAlignment));
			err |= get_user(p, &(from32->pvAlignMemVa));
			err |= put_user(compat_ptr(p),
				&(to->pvAlignMemVa));
			err |= get_user(p, &(from32->pvAlignMemPa));
			err |= put_user(compat_ptr(p),
				&(to->pvAlignMemPa));
			err |= get_user(u, &(from32->eMemCodec));
			err |= put_user(u, &(to->eMemCodec));
			err |= get_user(u, &(from32->i4IonShareFd));
			err |= put_user(u, &(to->i4IonShareFd));
			err |= get_user(p, &(from32->pIonBufhandle));
			err |= put_user(compat_ptr(p),
				&(to->pIonBufhandle));
			err |= get_user(p, &(from32->pvReserved));
			err |= put_user(compat_ptr(p),
				&(to->pvReserved));
			err |= get_user(l, &(from32->u4ReservedSize));
			err |= put_user(l, &(to->u4ReservedSize));
		} else {
			struct COMPAT_VAL_MEMORY __user *to32 =
				(struct COMPAT_VAL_MEMORY *) data32;
			struct VAL_MEMORY_T __user *from =
				(struct VAL_MEMORY_T *) data;

			err = get_user(u, &(from->eMemType));
			err |= put_user(u, &(to32->eMemType));
			err |= get_user(l, &(from->u4MemSize));
			err |= put_user(l, &(to32->u4MemSize));
			err |= get_user(ptr, &(from->pvMemVa));
			err |= put_user(ptr_to_compat(ptr),
				&(to32->pvMemVa));
			err |= get_user(ptr, &(from->pvMemPa));
			err |= put_user(ptr_to_compat(ptr),
				&(to32->pvMemPa));
			err |= get_user(u, &(from->eAlignment));
			err |= put_user(u, &(to32->eAlignment));
			err |= get_user(ptr, &(from->pvAlignMemVa));
			err |= put_user(ptr_to_compat(ptr),
					&(to32->pvAlignMemVa));
			err |= get_user(ptr, &(from->pvAlignMemPa));
			err |= put_user(ptr_to_compat(ptr),
					&(to32->pvAlignMemPa));
			err |= get_user(u, &(from->eMemCodec));
			err |= put_user(u, &(to32->eMemCodec));
			err |= get_user(u, &(from->i4IonShareFd));
			err |= put_user(u, &(to32->i4IonShareFd));
			err |= get_user(ptr, &(from->pIonBufhandle));
			err |= put_user(ptr_to_compat(ptr),
					&(to32->pIonBufhandle));
			err |= get_user(ptr, &(from->pvReserved));
			err |= put_user(ptr_to_compat(ptr),
					&(to32->pvReserved));
			err |= get_user(l, &(from->u4ReservedSize));
			err |= put_user(l, &(to32->u4ReservedSize));
		}
	} break;
	default:
		break;
	}

	return err;
}


static long vcodec_unlocked_compat_ioctl
	(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	pr_debug("[VCODEC_DEBUG] %s: 0x%x\n", __func__, cmd);
	switch (cmd) {
	case VCODEC_ALLOC_NON_CACHE_BUFFER:
	case VCODEC_FREE_NON_CACHE_BUFFER: {
		struct COMPAT_VAL_MEMORY __user *data32;
		struct VAL_MEMORY_T __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(struct VAL_MEMORY_T));
		if (data == NULL)
			return -EFAULT;

		err = compat_copy_struct(VAL_MEMORY_TYPE, COPY_FROM_USER,
					 (void *)data32, (void *)data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)data);

		err = compat_copy_struct(VAL_MEMORY_TYPE, COPY_TO_USER,
					 (void *)data32, (void *)data);

		if (err)
			return err;
		return ret;
	} break;
	case VCODEC_LOCKHW:
	case VCODEC_UNLOCKHW: {
		struct COMPAT_VAL_HW_LOCK __user *data32;
		struct VAL_HW_LOCK_T __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space
			(sizeof(struct VAL_HW_LOCK_T));
		if (data == NULL)
			return -EFAULT;

		err = compat_copy_struct(VAL_HW_LOCK_TYPE, COPY_FROM_USER,
					 (void *)data32, (void *)data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)data);

		err = compat_copy_struct(VAL_HW_LOCK_TYPE, COPY_TO_USER,
					 (void *)data32, (void *)data);

		if (err)
			return err;
		return ret;
	} break;

	case VCODEC_INC_PWR_USER:
	case VCODEC_DEC_PWR_USER: {
		struct COMPAT_VAL_POWER __user *data32;
		struct VAL_POWER_T __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space
			(sizeof(struct VAL_POWER_T));
		if (data == NULL)
			return -EFAULT;

		err = compat_copy_struct(VAL_POWER_TYPE, COPY_FROM_USER,
					 (void *)data32, (void *)data);

		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)data);

		err = compat_copy_struct(VAL_POWER_TYPE, COPY_TO_USER,
					 (void *)data32, (void *)data);

		if (err)
			return err;
		return ret;
	} break;

	case VCODEC_WAITISR: {
		struct COMPAT_VAL_ISR __user *data32;
		struct VAL_ISR_T __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space
			(sizeof(struct VAL_ISR_T));
		if (data == NULL)
			return -EFAULT;

		err = compat_copy_struct(VAL_ISR_TYPE, COPY_FROM_USER,
					 (void *)data32, (void *)data);
		if (err)
			return err;

		ret = file->f_op->unlocked_ioctl(file, VCODEC_WAITISR,
						 (unsigned long)data);

		err = compat_copy_struct(VAL_ISR_TYPE, COPY_TO_USER,
					 (void *)data32, (void *)data);

		if (err)
			return err;
		return ret;
	} break;

	default: {
		return vcodec_unlocked_ioctl(file, cmd, arg);
	} break;
	}
	return 0;
}
#else
#define vcodec_unlocked_compat_ioctl NULL
#endif
static int vcodec_open(struct inode *inode, struct file *file)
{
	pr_debug("[VCODEC_DEBUG] %s\n", __func__);

	mutex_lock(&DriverOpenCountLock);
	MT8163Driver_Open_Count++;

	pr_debug("%s pid = %d, MT8163Driver_Open_Count %d\n",
		__func__, current->pid,
		MT8163Driver_Open_Count);
	mutex_unlock(&DriverOpenCountLock);

	/* TODO: Check upper limit of concurrent users? */

	return 0;
}

static int venc_hw_reset(int type)
{
	unsigned int uValue;
	enum VAL_RESULT_T eValRet;

	pr_debug("Start VENC HW Reset");

	/* //clear irq event */
	/* EncIsrEvent.u4TimeoutMs = 1; */
	/* eValRet = eVideoWaitEvent(&EncIsrEvent, sizeof(VAL_EVENT_T)); */
	/* pr_err("ret %d", eValRet); */
	/* if type == 0//Soft Reset */
	/* step 1 */
	VDO_HW_WRITE(KVA_VENC_SW_PAUSE, 1);
	/* step 2 */
	/* EncIsrEvent.u4TimeoutMs = 10000; */
	EncIsrEvent.u4TimeoutMs = 2;
	eValRet = eVideoWaitEvent(&EncIsrEvent, sizeof(struct VAL_EVENT_T));
	if (eValRet == VAL_RESULT_INVALID_ISR ||
	    gu4HwVencIrqStatus != VENC_IRQ_STATUS_PAUSE) {
		uValue = VDO_HW_READ(KVA_VENC_IRQ_STATUS_ADDR);
		if (gu4HwVencIrqStatus != VENC_IRQ_STATUS_PAUSE)
			udelay(200);

		pr_debug("irq_status 0x%x", uValue);
		VDO_HW_WRITE(KVA_VENC_SW_PAUSE, 0);
		VDO_HW_WRITE(KVA_VENC_SW_HRST_N, 0);
		uValue = VDO_HW_READ(KVA_VENC_SW_HRST_N);
		pr_debug("3 HRST = %d, isr = 0x%x", uValue, gu4HwVencIrqStatus);
	} else { /* step 4 */
		VDO_HW_WRITE(KVA_VENC_SW_HRST_N, 0);
		uValue = gu4HwVencIrqStatus;
		VDO_HW_WRITE(KVA_VENC_SW_PAUSE, 0);
		pr_debug("4 HRST = %d, isr = 0x%x", uValue, gu4HwVencIrqStatus);
	}

	VDO_HW_WRITE(KVA_VENC_SW_HRST_N, 1);
	uValue = VDO_HW_READ(KVA_VENC_SW_HRST_N);
	pr_debug("HRST = %d", uValue);

	return 1;
}

static int vcodec_flush(struct file *file, fl_owner_t id)
{
	pr_debug("[VCODEC_DEBUG] %s, curr_tid =%d\n", __func__, current->pid);
	pr_debug("%s pid = %d, MT8163Driver_Open_Count %d\n",
		__func__, current->pid, MT8163Driver_Open_Count);

	return 0;
}

static int vcodec_release(struct inode *inode, struct file *file)
{
	unsigned long ulFlagsLockHW, ulFlagsISR;
	/* dump_stack(); */
	pr_debug("[VCODEC_DEBUG] %s, curr_tid =%d\n", __func__, current->pid);
	mutex_lock(&DriverOpenCountLock);
	pr_debug("%s pid = %d, MT8163Driver_Open_Count %d\n",
		__func__, current->pid,
		MT8163Driver_Open_Count);
	MT8163Driver_Open_Count--;

	if (MT8163Driver_Open_Count == 0) {
		mutex_lock(&VencHWLock);
		if (grVEncHWLock.eDriverType == VAL_DRIVER_TYPE_H264_ENC) {
			venc_hw_reset(0);
			disable_irq(VENC_IRQ_ID);
			venc_power_off();
			pr_debug("Clean venc lock\n");
		}
		mutex_unlock(&VencHWLock);

		mutex_lock(&VdecHWLock);
		gu4VdecLockThreadId = 0;
		grVDecHWLock.pvHandle = 0;
		grVDecHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
		grVDecHWLock.rLockedTime.u4Sec = 0;
		grVDecHWLock.rLockedTime.u4uSec = 0;
		mutex_unlock(&VdecHWLock);

		mutex_lock(&VencHWLock);
		grVEncHWLock.pvHandle = 0;
		grVEncHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
		grVEncHWLock.rLockedTime.u4Sec = 0;
		grVEncHWLock.rLockedTime.u4uSec = 0;
		mutex_unlock(&VencHWLock);

		mutex_lock(&DecEMILock);
		gu4DecEMICounter = 0;
		mutex_unlock(&DecEMILock);

		mutex_lock(&EncEMILock);
		gu4EncEMICounter = 0;
		mutex_unlock(&EncEMILock);

		mutex_lock(&PWRLock);
		gu4PWRCounter = 0;
		mutex_unlock(&PWRLock);

#ifdef MT8163_VENC_USE_L2C
		mutex_lock(&L2CLock);
		if (gu4L2CCounter != 0) {
			pr_err("%s pid = %d, L2 user = %d, force restore L2 settings\n",
				__func__, current->pid, gu4L2CCounter);
			if (config_L2(1))
				pr_err("restore L2 settings failed\n");
		}
		gu4L2CCounter = 0;
		mutex_unlock(&L2CLock);
#endif
		spin_lock_irqsave(&LockDecHWCountLock, ulFlagsLockHW);
		gu4LockDecHWCount = 0;
		spin_unlock_irqrestore(&LockDecHWCountLock, ulFlagsLockHW);

		spin_lock_irqsave(&LockEncHWCountLock, ulFlagsLockHW);
		gu4LockEncHWCount = 0;
		spin_unlock_irqrestore(&LockEncHWCountLock, ulFlagsLockHW);

		spin_lock_irqsave(&DecISRCountLock, ulFlagsISR);
		gu4DecISRCount = 0;
		spin_unlock_irqrestore(&DecISRCountLock, ulFlagsISR);

		spin_lock_irqsave(&EncISRCountLock, ulFlagsISR);
		gu4EncISRCount = 0;
		spin_unlock_irqrestore(&EncISRCountLock, ulFlagsISR);
	}
	mutex_unlock(&DriverOpenCountLock);

	return 0;
}

void vcodec_vma_open(struct vm_area_struct *vma)
{
	pr_debug("vcodec VMA open, virt %lx, phys %lx\n", vma->vm_start,
			vma->vm_pgoff << PAGE_SHIFT);
}

void vcodec_vma_close(struct vm_area_struct *vma)
{
	pr_debug("vcodec VMA close, virt %lx, phys %lx\n", vma->vm_start,
			vma->vm_pgoff << PAGE_SHIFT);
}

static const struct vm_operations_struct vcodec_remap_vm_ops = {
	.open = vcodec_vma_open,
	.close = vcodec_vma_close,
};

static int vcodec_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned int u4I = 0;
	unsigned long length;
	unsigned long pfn;

	length = vma->vm_end - vma->vm_start;
	pfn = vma->vm_pgoff << PAGE_SHIFT;

	if (((length > VENC_REGION) || (pfn < VENC_BASE) ||
	     (pfn > VENC_BASE + VENC_REGION)) &&
	    ((length > VENC_REGION) || (pfn < VENC_LT_BASE) ||
	     (pfn > VENC_LT_BASE + VENC_REGION)) &&
	    ((length > VDEC_REGION) || (pfn < VDEC_BASE_PHY) ||
	     (pfn > VDEC_BASE_PHY + VDEC_REGION)) &&
	    ((length > HW_REGION) || (pfn < HW_BASE) ||
	     (pfn > HW_BASE + HW_REGION)) &&
	    ((length > INFO_REGION) || (pfn < INFO_BASE) ||
	     (pfn > INFO_BASE + INFO_REGION))) {
		unsigned long ulAddr, ulSize;

		for (u4I = 0; u4I < VCODEC_MULTIPLE_INSTANCE_NUM_x_10; u4I++) {
			if ((grNCMemList[u4I].ulKVA != -1L)
			    && (grNCMemList[u4I].ulKPA != -1L)) {
				ulAddr = grNCMemList[u4I].ulKPA;
				ulSize = (grNCMemList[u4I].ulSize +
				    0x1000 - 1) & ~(0x1000 - 1);
				if ((length == ulSize) && (pfn == ulAddr)) {
					pr_debug(" cache idx %d\n", u4I);
					break;
				}
			}
		}

		if (u4I == VCODEC_MULTIPLE_INSTANCE_NUM_x_10) {
			pr_err("[ERROR] mmap region error: Length(0x%lx), pfn(0x%lx)\n",
				 (unsigned long) length, pfn);
			return -EAGAIN;
		}
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	pr_debug("[mmap] vma->start 0x%lx, vma->end 0x%lx, vma->pgoff 0x%lx\n",
		 (unsigned long) vma->vm_start, (unsigned long) vma->vm_end,
		 (unsigned long) vma->vm_pgoff);
	if (remap_pfn_range
	    (vma, vma->vm_start, vma->vm_pgoff, vma->vm_end - vma->vm_start,
	    vma->vm_page_prot)) {
		return -EAGAIN;
	}

	vma->vm_ops = &vcodec_remap_vm_ops;
	vcodec_vma_open(vma);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void vcodec_early_suspend(struct early_suspend *h)
{
	mutex_lock(&PWRLock);
	pr_debug("%s, tid = %d, PWR_USER = %d\n",
		__func__, current->pid, gu4PWRCounter);
	mutex_unlock(&PWRLock);

	pr_debug("%s - tid = %d\n", __func__, current->pid);
}

static void vcodec_late_resume(struct early_suspend *h)
{
	mutex_lock(&PWRLock);
	pr_debug("%s, tid = %d, PWR_USER = %d\n",
		__func__, current->pid, gu4PWRCounter);
	mutex_unlock(&PWRLock);
	pr_debug("%s - tid = %d\n", __func__, current->pid);
}

static struct early_suspend vcodec_early_suspend_handler = {
	.level = (EARLY_SUSPEND_LEVEL_DISABLE_FB - 1),
	.suspend = vcodec_early_suspend,
	.resume = vcodec_late_resume,
};
#endif

static const struct file_operations vcodec_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = vcodec_unlocked_ioctl,
	.open = vcodec_open,
	.flush = vcodec_flush,
	.release = vcodec_release,
	.mmap = vcodec_mmap,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = vcodec_unlocked_compat_ioctl,
#endif
};

static int vcodec_probe(struct platform_device *pdev)
{
	int ret;

	pr_debug("+%s\n", __func__);

	mutex_lock(&DecEMILock);
	gu4DecEMICounter = 0;
	mutex_unlock(&DecEMILock);

	mutex_lock(&EncEMILock);
	gu4EncEMICounter = 0;
	mutex_unlock(&EncEMILock);

	mutex_lock(&PWRLock);
	gu4PWRCounter = 0;
	mutex_unlock(&PWRLock);

	mutex_lock(&L2CLock);
	gu4L2CCounter = 0;
	mutex_unlock(&L2CLock);

	ret = register_chrdev_region(vcodec_devno, 1, VCODEC_DEVNAME);
	if (ret)
		pr_err("[ERROR] Can't Get Major number for VCodec Device\n");

	vcodec_cdev = cdev_alloc();
	vcodec_cdev->owner = THIS_MODULE;
	vcodec_cdev->ops = &vcodec_fops;

	ret = cdev_add(vcodec_cdev, vcodec_devno, 1);
	if (ret)
		pr_err("[VCODEC_DEBUG][ERROR] Can't add Vcodec Device\n");

	vcodec_class = class_create(THIS_MODULE, VCODEC_DEVNAME);
	if (IS_ERR(vcodec_class)) {
		ret = PTR_ERR(vcodec_class);
		pr_err("Unable to create class, err = %d", ret);
		return ret;
	}

	vcodec_device = device_create(vcodec_class, NULL, vcodec_devno, NULL,
				      VCODEC_DEVNAME);

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
#else
	if (request_irq(VDEC_IRQ_ID, (irq_handler_t)video_intr_dlr,
			IRQF_TRIGGER_LOW, VCODEC_DEVNAME, NULL) < 0) {
		pr_debug("[VCODEC_DEBUG][ERROR] error to request dec irq\n");
	} else {
		pr_debug("[VCODEC_DEBUG] success to request dec irq: %d\n",
			VDEC_IRQ_ID);
	}

	if (request_irq(VENC_IRQ_ID, (irq_handler_t)video_intr_dlr2,
			IRQF_TRIGGER_LOW, VCODEC_DEVNAME, NULL) < 0) {
		pr_debug("[VCODEC_DEBUG][ERROR] error to request enc irq\n");
	} else {
		pr_debug("[VCODEC_DEBUG] success to request enc irq: %d\n",
			VENC_IRQ_ID);
	}

#endif

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
#else
	/* disable_irq(MT_VDEC_IRQ_ID); */
	disable_irq(VDEC_IRQ_ID);
	/* disable_irq(MT_VENC_IRQ_ID); */
	disable_irq(VENC_IRQ_ID);
#endif
	pr_debug("[VCODEC_DEBUG] %s Done\n", __func__);

	return 0;
}

static int venc_disableIRQ(struct VAL_HW_LOCK_T *prHWLock)
{
	unsigned int  u4IrqId = VENC_IRQ_ID;

	if (prHWLock->eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
	    prHWLock->eDriverType == VAL_DRIVER_TYPE_HEVC_ENC)
		u4IrqId = VENC_IRQ_ID;
	else if (prHWLock->eDriverType == VAL_DRIVER_TYPE_VP8_ENC)
		u4IrqId = VENC_LT_IRQ_ID;

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT /* Morris Yang moved to TEE */
	if (prHWLock->bSecureInst == VAL_FALSE) {
		/* disable_irq(VENC_IRQ_ID); */

		free_irq(u4IrqId, NULL);
	}
#else
	/* disable_irq(MT_VENC_IRQ_ID); */
	disable_irq(u4IrqId);
#endif
	/* turn venc power off */
	venc_power_off();
	return 0;
}

static int venc_enableIRQ(struct VAL_HW_LOCK_T *prHWLock)
{
	unsigned int  u4IrqId = VENC_IRQ_ID;

	pr_debug("%s+\n", __func__);
	if (prHWLock->eDriverType == VAL_DRIVER_TYPE_H264_ENC ||
	    prHWLock->eDriverType == VAL_DRIVER_TYPE_HEVC_ENC)
		u4IrqId = VENC_IRQ_ID;
	else if (prHWLock->eDriverType == VAL_DRIVER_TYPE_VP8_ENC)
		u4IrqId = VENC_LT_IRQ_ID;

	venc_power_on();
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	pr_debug("[VCODEC_LOCKHW] ENC rHWLock.bSecureInst 0x%x\n",
			prHWLock->bSecureInst);
	if (prHWLock->bSecureInst == VAL_FALSE) {
		pr_debug("[VCODEC_LOCKHW]  ENC Request IR by type 0x%x\n",
			prHWLock->eDriverType);
		if (request_irq(u4IrqId, (irq_handler_t)video_intr_dlr2,
				IRQF_TRIGGER_LOW, VCODEC_DEVNAME, NULL) < 0) {
			pr_err("[VCODEC_LOCKHW] ENC [MFV_DEBUG][ERROR] error to request enc irq\n");
		} else {
			pr_debug("[VCODEC_LOCKHW] ENC [MFV_DEBUG] success to request enc irq\n");
		}
	}

#else
	enable_irq(u4IrqId);
#endif

	pr_debug("%s-\n", __func__);
	return 0;
}

static int vcodec_remove(struct platform_device *pDev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int vcodec_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	if (mutex_trylock(&VdecPWRLock) == 1)
	{
		if (gu4VdecPWRCounter > 0)
		{
			pr_err("vcodec_suspend count=%d\n",gu4VdecPWRCounter);
			mutex_unlock(&VdecPWRLock);
			return -EBUSY;
		} else {
			mutex_unlock(&VdecPWRLock);
			return 0;
		}
	}  else {
		return -EBUSY;
	}
}

static int vcodec_resume(struct platform_device *pdev)
{
	pr_debug("vcodec_resume\n");
	return 0;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/

static int mtkvdec_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	BUG_ON(pdev == NULL);

	return vcodec_suspend(pdev, PMSG_SUSPEND);
}

static int mtkvdec_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	BUG_ON(pdev == NULL);

	return vcodec_resume(pdev);
}

/*---------------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#define mtkvdec_pm_suspend NULL
#define mtkvdec_pm_resume NULL
/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/

const struct dev_pm_ops mtkvdec_pm_ops = {
	.suspend = mtkvdec_pm_suspend,
	.resume = mtkvdec_pm_resume,
};

#ifdef CONFIG_OF
/* VDEC main device */
static const struct of_device_id vcodec_of_ids[] = {
	{
		.compatible = "mediatek,mt8163-vdec_gcon",
	},
	{} };

static struct platform_driver VCodecDriver = {
	.probe = vcodec_probe,
	.remove = vcodec_remove,
	.driver = {
		.name = VCODEC_DEVNAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &mtkvdec_pm_ops,
#endif
		.of_match_table = vcodec_of_ids,
	} };
#endif

#ifdef CONFIG_MTK_HIBERNATION
static int vcodec_pm_restore_noirq(struct device *device)
{
	/* vdec : IRQF_TRIGGER_LOW */
	mt_irq_set_sens(VDEC_IRQ_ID, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(VDEC_IRQ_ID, MT_POLARITY_LOW);
	/* venc: IRQF_TRIGGER_LOW */
	mt_irq_set_sens(VENC_IRQ_ID, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(VENC_IRQ_ID, MT_POLARITY_LOW);

	return 0;
}
#endif

static int __init vcodec_driver_init(void)
{
	enum VAL_RESULT_T eValHWLockRet;
	unsigned long ulFlags, ulFlagsLockHW, ulFlagsISR;
	struct device_node *node = NULL;

	pr_debug("+vcodec_init !!\n");

	mutex_lock(&DriverOpenCountLock);
	MT8163Driver_Open_Count = 0;
	mutex_unlock(&DriverOpenCountLock);

/* get VENC related */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8163-venc");
	KVA_VENC_BASE = (unsigned long) of_iomap(node, 0);
	VENC_IRQ_ID = irq_of_parse_and_map(node, 0);
	KVA_VENC_IRQ_STATUS_ADDR = KVA_VENC_BASE + 0x05C;
	KVA_VENC_IRQ_ACK_ADDR = KVA_VENC_BASE + 0x060;
	KVA_VENC_SW_PAUSE = KVA_VENC_BASE + VENC_SW_PAUSE;
	KVA_VENC_SW_HRST_N = KVA_VENC_BASE + VENC_SW_HRST_N;
	{
		struct device_node *node = NULL;

		node = of_find_compatible_node(NULL, NULL,
					       "mediatek,mt8163-vdec");
		KVA_VDEC_BASE = (unsigned long) of_iomap(node, 0);
		VDEC_IRQ_ID = irq_of_parse_and_map(node, 0);
		KVA_VDEC_MISC_BASE = KVA_VDEC_BASE + 0x0000;
		KVA_VDEC_VLD_BASE = KVA_VDEC_BASE + 0x1000;
	}
	{
		struct device_node *node = NULL;

		node = of_find_compatible_node(NULL, NULL,
					       "mediatek,mt8163-vdec_gcon");
		KVA_VDEC_GCON_BASE = (unsigned long) of_iomap(node, 0);
		pr_debug("[DeviceTree] KVA_VENC_BASE(0x%lx), KVA_VDEC_BASE(0x%lx), KVA_VDEC_GCON_BASE(0x%lx)",
			KVA_VENC_BASE, KVA_VDEC_BASE, KVA_VDEC_GCON_BASE);
		pr_debug("[DeviceTree] VDEC_IRQ_ID(%d), VENC_IRQ_ID(%d)",
				VDEC_IRQ_ID, VENC_IRQ_ID);
	}
	spin_lock_irqsave(&LockDecHWCountLock, ulFlagsLockHW);
	gu4LockDecHWCount = 0;
	spin_unlock_irqrestore(&LockDecHWCountLock, ulFlagsLockHW);

	spin_lock_irqsave(&LockEncHWCountLock, ulFlagsLockHW);
	gu4LockEncHWCount = 0;
	spin_unlock_irqrestore(&LockEncHWCountLock, ulFlagsLockHW);

	spin_lock_irqsave(&DecISRCountLock, ulFlagsISR);
	gu4DecISRCount = 0;
	spin_unlock_irqrestore(&DecISRCountLock, ulFlagsISR);

	spin_lock_irqsave(&EncISRCountLock, ulFlagsISR);
	gu4EncISRCount = 0;
	spin_unlock_irqrestore(&EncISRCountLock, ulFlagsISR);

	mutex_lock(&VdecPWRLock);
	gu4VdecPWRCounter = 0;
	mutex_unlock(&VdecPWRLock);

	mutex_lock(&VencPWRLock);
	gu4VencPWRCounter = 0;
	mutex_unlock(&VencPWRLock);

	mutex_lock(&IsOpenedLock);
	if (bIsOpened == VAL_FALSE) {
		bIsOpened = VAL_TRUE;
#ifdef CONFIG_OF
		platform_driver_register(&VCodecDriver);
#else
		vcodec_probe(NULL);
#endif
	}
	mutex_unlock(&IsOpenedLock);

	mutex_lock(&VdecHWLock);
	gu4VdecLockThreadId = 0;
	grVDecHWLock.pvHandle = 0;
	grVDecHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
	grVDecHWLock.rLockedTime.u4Sec = 0;
	grVDecHWLock.rLockedTime.u4uSec = 0;
	mutex_unlock(&VdecHWLock);

	mutex_lock(&VencHWLock);
	grVEncHWLock.pvHandle = 0;
	grVEncHWLock.eDriverType = VAL_DRIVER_TYPE_NONE;
	grVEncHWLock.rLockedTime.u4Sec = 0;
	grVEncHWLock.rLockedTime.u4uSec = 0;
	mutex_unlock(&VencHWLock);

	/* MT8163_HWLockEvent part */
	mutex_lock(&DecHWLockEventTimeoutLock);
	DecHWLockEvent.pvHandle = "DECHWLOCK_EVENT";
	DecHWLockEvent.u4HandleSize = sizeof("DECHWLOCK_EVENT") + 1;
	DecHWLockEvent.u4TimeoutMs = 1;
	mutex_unlock(&DecHWLockEventTimeoutLock);
	eValHWLockRet = eVideoCreateEvent
		(&DecHWLockEvent, sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR)
		pr_err("[MFV][ERROR] create dec hwlock event error\n");

	mutex_lock(&EncHWLockEventTimeoutLock);
	EncHWLockEvent.pvHandle = "ENCHWLOCK_EVENT";
	EncHWLockEvent.u4HandleSize = sizeof("ENCHWLOCK_EVENT") + 1;
	EncHWLockEvent.u4TimeoutMs = 1;
	mutex_unlock(&EncHWLockEventTimeoutLock);
	eValHWLockRet = eVideoCreateEvent
		(&EncHWLockEvent, sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR)
		pr_err("[MFV][ERROR] create enc hwlock event error\n");
	/* MT8163_IsrEvent part */
	spin_lock_irqsave(&DecIsrLock, ulFlags);
	DecIsrEvent.pvHandle = "DECISR_EVENT";
	DecIsrEvent.u4HandleSize = sizeof("DECISR_EVENT") + 1;
	DecIsrEvent.u4TimeoutMs = 1;
	spin_unlock_irqrestore(&DecIsrLock, ulFlags);
	eValHWLockRet = eVideoCreateEvent
		(&DecIsrEvent, sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR)
		pr_err("[MFV][ERROR] create dec isr event error\n");

	spin_lock_irqsave(&EncIsrLock, ulFlags);
	EncIsrEvent.pvHandle = "ENCISR_EVENT";
	EncIsrEvent.u4HandleSize = sizeof("ENCISR_EVENT") + 1;
	EncIsrEvent.u4TimeoutMs = 1;
	spin_unlock_irqrestore(&EncIsrLock, ulFlags);
	eValHWLockRet = eVideoCreateEvent
		(&EncIsrEvent, sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR)
		pr_err("[MFV][ERROR] create enc isr event error\n");

	pr_debug("[VCODEC_DEBUG] %s Done\n", __func__);

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&vcodec_early_suspend_handler);
#endif

#ifdef CONFIG_MTK_HIBERNATION
	register_swsusp_restore_noirq_func(ID_M_VCODEC,
		vcodec_pm_restore_noirq, NULL);
#endif

	return 0;
}

static void __exit vcodec_driver_exit(void)
{
	enum VAL_RESULT_T eValHWLockRet;

	pr_debug("[VCODEC_DEBUG] %s\n", __func__);

	mutex_lock(&IsOpenedLock);
	if (bIsOpened == VAL_TRUE) {
		pr_debug("+%s remove device !!\n", __func__);
#ifdef CONFIG_OF
		platform_driver_unregister(&VCodecDriver);
#else
		bIsOpened = VAL_FALSE;
#endif
		pr_debug("+%s remove done !!\n", __func__);
	}
	mutex_unlock(&IsOpenedLock);

	cdev_del(vcodec_cdev);
	unregister_chrdev_region(vcodec_devno, 1);

/* [TODO] iounmap the following? */
#if 0
	iounmap((void *)KVA_VENC_IRQ_STATUS_ADDR);
	iounmap((void *)KVA_VENC_IRQ_ACK_ADDR);
#endif
#ifdef VENC_PWR_FPGA
	iounmap((void *)KVA_VENC_CLK_CFG_0_ADDR);
	iounmap((void *)KVA_VENC_CLK_CFG_4_ADDR);
	iounmap((void *)KVA_VENC_PWR_ADDR);
	iounmap((void *)KVA_VENCSYS_CG_SET_ADDR);
#endif

	/* [TODO] free IRQ here */
	/* free_irq(MT_VENC_IRQ_ID, NULL); */
	free_irq(VENC_IRQ_ID, NULL);
	/* free_irq(MT_VDEC_IRQ_ID, NULL); */
	free_irq(VDEC_IRQ_ID, NULL);

	/* MT6589_HWLockEvent part */
	eValHWLockRet = eVideoCloseEvent
		(&DecHWLockEvent, sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR)
		pr_err("[MFV][ERROR] close dec hwlock event error\n");

	eValHWLockRet = eVideoCloseEvent
		(&EncHWLockEvent, sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR)
		pr_err("[MFV][ERROR] close enc hwlock event error\n");

	/* MT6589_IsrEvent part */
	eValHWLockRet = eVideoCloseEvent
		(&DecIsrEvent, sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR)
		pr_err("[MFV][ERROR] close dec isr event error\n");

	eValHWLockRet = eVideoCloseEvent
		(&EncIsrEvent, sizeof(struct VAL_EVENT_T));
	if (eValHWLockRet != VAL_RESULT_NO_ERROR)
		pr_err("[MFV][ERROR] close enc isr event error\n");
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&vcodec_early_suspend_handler);
#endif

#ifdef CONFIG_MTK_HIBERNATION
	unregister_swsusp_restore_noirq_func(ID_M_VCODEC);
#endif
}
module_init(vcodec_driver_init);
module_exit(vcodec_driver_exit);
MODULE_AUTHOR("Legis, Lu <legis.lu@mediatek.com>");
MODULE_DESCRIPTION("8163 Vcodec Driver");
MODULE_LICENSE("GPL");
