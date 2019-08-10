/*
 *
 * (C) COPYRIGHT 2011-2015 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */


#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/init.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>

#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <mali_kbase_config.h>
#include "mali_kbase_config_platform.h"
#include <platform/mtk_platform_common.h>

#include "mt_smi.h"
#include "ged_dvfs.h"
#include "mt_gpufreq.h"


/* MFG Implementation Begin
 * GPU controller.
 *
 * If GPU power domain needs to be enabled after disp, we will break flow
 * of mfg_enable_gpu to be 2 functions like mfg_prepare/mfg_enable.
 *
 * Currently no lock for mfg, Mali driver will take. And MFG power domain (MTCMOS) is
 * assumed that enabled before calling MFG functions.
 */

static struct clk *mfg_clk_topck;
static struct clk *mfg_clk_pll;

static void __iomem *mfg_start;
static struct platform_device *mfg_dev;

#define MFG_CG_CON     0x0
#define MFG_CG_SET     0x4
#define MFG_CG_CLR     0x8
#define MFG_DEBUG_SEL  0x180
#define MFG_DEBUG_STAT 0x184

#define MFG_READ32(r)     __raw_readl(mfg_start + (r))
#define MFG_WRITE32(v, r) __raw_writel((v), mfg_start + (r))

#define CHECK_MFG_STAT(off, exp_val) \
do { \
	u32 con = MFG_READ32(off); \
	if (exp_val != con) { \
		dev_info(&mfg_dev->dev, "MFG %s #%d read con: %#x, expect %#x\n", __func__, __LINE__, con, exp_val); \
		mfg_dump_regs(__func__); \
	} \
} while (0)

static void mfg_dump_regs(const char *prefix)
{
	u32 val;
	unsigned long off;

	dev_info(&mfg_dev->dev, "MFG BEGIN %s\n", prefix);

	for (off = 0; off < 0x1a0; off += 4) {
		val = MFG_READ32(off);
		if (val != 0)
			dev_info(&mfg_dev->dev, "  [0x%lx] : 0x%x\n", off, val);
	}

	dev_info(&mfg_dev->dev, "MFG END\n");
}

/* This function assumes power domains and smi are enabled. */
static void mtk_mfg_wait_gpu_idle(void)
{
	int polling_count = 100000;

	/* Polling the MFG_DEBUG_REG for checking GPU IDLE before MTCMOS power off (0.1ms) */
	MFG_WRITE32(0x3, MFG_DEBUG_SEL);

	do {
		/* 0x13000184[2] */
		/* 1'b1: bus idle */
		/* 1'b0: bus busy */
		if (MFG_READ32(MFG_DEBUG_STAT) & (1 << 2))
			return;
	} while (polling_count--);

	if (polling_count <= 0)
		pr_err("[MALI]: MFG(GPU) subsys is still BUSY! polling_count=%d\n", polling_count);
}

static void mtk_mfg_enable_gpu(void)
{
	if (mfg_start != NULL) {
		u32 con = MFG_READ32(MFG_CG_CON);

		if (con != 0x0)
			MFG_WRITE32(0x1, MFG_CG_CLR);

		CHECK_MFG_STAT(MFG_CG_CON, 0x0);
	}
}

static void mtk_mfg_disable_gpu(void)
{
	if (mfg_start != NULL) {
		u32 con = MFG_READ32(MFG_CG_CON);

		if (con == 0x0)
			MFG_WRITE32(0x1, MFG_CG_SET);
		else
			dev_info(&mfg_dev->dev, "Disable already\n");

		CHECK_MFG_STAT(MFG_CG_CON, 0x1);
	}
}

static int mfg_device_probe(struct platform_device *pdev)
{
	struct clk *parent;

	pr_info("MFG device start probe\n");

	/* Make sure disp pm is ready to operate. */
	if (!mtk_smi_larb_get_base(0)) {
		pr_info("disp is not ready, defer MFG\n");
		return -EPROBE_DEFER;
	}

	mfg_start = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR_OR_NULL(mfg_start)) {
		mfg_start = NULL;
		goto error_out;
	}
	pr_debug("MFG start is mapped %p\n", mfg_start);

	mfg_clk_pll = devm_clk_get(&pdev->dev, "pll");
	if (IS_ERR(mfg_clk_pll)) {
		pr_err("MFG cannot get pll clock!\n");
		mfg_clk_pll = NULL;
		goto error_out;
	}

	mfg_clk_topck = devm_clk_get(&pdev->dev, "topck");
	if (IS_ERR(mfg_clk_topck)) {
		pr_err("MFG cannot get topck clock!\n");
		mfg_clk_topck = NULL;
		goto error_out;
	}

	clk_prepare_enable(mfg_clk_topck);

	clk_set_parent(mfg_clk_topck, mfg_clk_pll);
	parent = clk_get_parent(mfg_clk_topck);
	if (!IS_ERR_OR_NULL(parent)) {
		pr_info("MFG is now selected to %s, MFG parent rate %lu\n", parent->name, clk_get_rate(parent));
	} else {
		pr_err("Failed to select mfg!\n");
		BUG();
	}

	clk_disable_unprepare(mfg_clk_topck);

	mfg_dev = pdev;

	pr_info("MFG device probed\n");
	return 0;

error_out:
	if (mfg_clk_topck)
		devm_clk_put(&pdev->dev, mfg_clk_topck);
	if (mfg_clk_pll)
		devm_clk_put(&pdev->dev, mfg_clk_pll);
	if (mfg_start)
		iounmap(mfg_start);

	return -1;
}

static int mfg_device_remove(struct platform_device *pdev)
{
	if (mfg_clk_pll)
		devm_clk_put(&pdev->dev, mfg_clk_pll);
	if (mfg_clk_topck)
		devm_clk_put(&pdev->dev, mfg_clk_topck);
	if (mfg_start)
		iounmap(mfg_start);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mfg_dt_ids[] = {
	{.compatible = "mediatek,mt8163-mfg"},
	{.compatible = "mediatek,midgard-mfg"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mfg_dt_ids);
#endif

static struct platform_driver mtk_mfg_driver = {
	.probe = mfg_device_probe,
	.remove = mfg_device_remove,
	.driver = {
		   .name = "mfg",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(mfg_dt_ids),
		   },
};

static int __init mfg_driver_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_mfg_driver);
	return ret;
}
module_init(mfg_driver_init);
/* MFG Implementation End */


/* GPU Callabck Implementation Begin */
#define HARD_RESET_AT_POWER_OFF 0
static unsigned int g_power_off_gpu_freq_idx;

/* In suspend mode, pm_runtime_get_sync() will return -13, which indicate no premission.
   When return fail in mtk_smi_larb_clock_on(), not call mtk_smi_larb_clock_off() next time.
   Solve the issue in ALPS02813524. */
static int iSmiFail;

static int pm_callback_power_on(struct kbase_device *kbdev)
{
	int iRet;
	unsigned int current_gpu_freq_idx;

	/*
	* Hold wakelock when mfg power-on, prevent suspend when gpu active.
	* When enter system suspend flow, forbbiden power domain control.
	*/
	pm_stay_awake(kbdev->dev);

	iSmiFail = mtk_smi_larb_clock_on(0, true);
	if (iSmiFail < 0)
		pr_err("[MALI]: Failed to enable domain smi - %d!\n", iSmiFail);

	iRet = pm_runtime_get_sync(kbdev->dev);
	if (iRet < 0)
		pr_err("[MALI]: Failed to enable domain mfg - %d!\n", iRet);

	mtk_mfg_enable_gpu();

	mt_gpufreq_voltage_enable_set(1);
	mtk_set_vgpu_power_on_flag(MTK_VGPU_POWER_ON);
#ifdef ENABLE_COMMON_DVFS
	ged_dvfs_gpu_clock_switch_notify(1);
#endif

	/* restore gpu freq which is recorded in power off stage. */
	mt_gpufreq_target(g_power_off_gpu_freq_idx);
	current_gpu_freq_idx = mt_gpufreq_get_cur_freq_index();
	if (current_gpu_freq_idx > g_power_off_gpu_freq_idx)
		pr_debug("MALI: GPU freq. can't switch to idx=%d\n", g_power_off_gpu_freq_idx);

	/* Nothing is needed on VExpress, but we may have destroyed GPU state (if the below HARD_RESET code is active) */
	return 1;
}

static void pm_callback_power_off(struct kbase_device *kbdev)
{
	unsigned int uiCurrentFreqCount;

	mtk_mfg_wait_gpu_idle();

#if HARD_RESET_AT_POWER_OFF
	/* Cause a GPU hard reset to test whether we have actually idled the GPU
	 * and that we properly reconfigure the GPU on power up.
	 * Usually this would be dangerous, but if the GPU is working correctly it should
	 * be completely safe as the GPU should not be active at this point.
	 * However this is disabled normally because it will most likely interfere with
	 * bus logging etc.
	 */
	kbase_os_reg_write(kbdev, GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_HARD_RESET);
	mtk_mfg_wait_gpu_idle();
#endif

	/* set gpu to lowest freq when power off. */
    g_power_off_gpu_freq_idx = mt_gpufreq_get_cur_freq_index();
    uiCurrentFreqCount = mt_gpufreq_get_dvfs_table_num();
    mt_gpufreq_target(uiCurrentFreqCount-1);

	mt_gpufreq_voltage_enable_set(0);
#ifdef ENABLE_COMMON_DVFS
	ged_dvfs_gpu_clock_switch_notify(0);
#endif
	mtk_set_vgpu_power_on_flag(MTK_VGPU_POWER_OFF);

	mtk_mfg_disable_gpu();

	pm_runtime_put_sync_suspend(kbdev->dev);

	if (iSmiFail < 0)
		iSmiFail = 0;
	else
		mtk_smi_larb_clock_off(0, true);

	/* Release wakelock when mfg power-off */
	pm_relax(kbdev->dev);
}


struct kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
	.power_suspend_callback  = NULL,
	.power_resume_callback = NULL,
	.power_runtime_init_callback = NULL,
	.power_runtime_term_callback = NULL,
	.power_runtime_on_callback = NULL,
	.power_runtime_off_callback = NULL,
};

static struct kbase_platform_config mt8163_platform_config = {
#ifndef CONFIG_OF
	.io_resources = &io_resources
#endif
};

struct kbase_platform_config *kbase_get_platform_config(void)
{
	return &mt8163_platform_config;
}

int kbase_platform_early_init(void)
{
	if (!mfg_start) {
		pr_info("mfg is not ready, defer GPU\n");
		return -EPROBE_DEFER;
	}

	return 0;
}

int mtk_platform_init(struct platform_device *pdev, struct kbase_device *kbdev)
{
	pm_runtime_enable(kbdev->dev);

	/* Use mfg power domain as a system suspend indicator. */
	device_init_wakeup(kbdev->dev, true);

	return 0;
}

int mtk_platform_deinit(struct platform_device *pdev, struct kbase_device *kbdev)
{
	device_init_wakeup(kbdev->dev, false);
	pm_runtime_disable(kbdev->dev);

	return 0;
}

/* GPU Callabck Implementation End */

