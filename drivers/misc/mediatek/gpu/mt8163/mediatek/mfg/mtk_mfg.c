/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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
#include "mt_gpufreq.h"
#include "mtk_mfg.h"

#define DEBUG

/* clk */
static struct clk *clk_pll;
static struct clk *clk_topck;

/* MFG begin
 * GPU controller.
 *
 * If GPU power domain needs to be enabled after disp, we will break flow
 * of mfg_enable_gpu to be 2 functions like mfg_prepare/mfg_enable.
 *
 * Currently no lock for mfg, Mali driver will take. And MFG power domain (MTCMOS) is
 * assumed that enabled before calling MFG functions.
 */

#define MFG_CG_CON 0x0
#define MFG_CG_SET 0x4
#define MFG_CG_CLR 0x8
#define MFG_DEBUG_SEL 0x180
#define MFG_DEBUG_STAT 0x184

#define MFG_READ32(r) __raw_readl(mfg_start + (r))
#define MFG_WRITE32(v, r) __raw_writel((v), mfg_start + (r))

struct platform_device *mfg_dev = NULL;
static void __iomem *mfg_start;


#ifdef CONFIG_OF
static const struct of_device_id mfg_dt_ids[] = {
	{.compatible = "mediatek,mt8163-mfg"},
	{.compatible = "mediatek,midgard-mfg"},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, mfg_dt_ids);
#endif


static int mfg_device_probe(struct platform_device *pdev)
{
	struct clk *parent;
	unsigned long freq;
	
	pr_info("MFG device start probe\n");

	mfg_start = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR_OR_NULL(mfg_start)) {
		mfg_start = NULL;
		goto error_out;
	}
	pr_debug("MFG start is mapped %p\n", mfg_start);
	
	clk_pll = devm_clk_get(&pdev->dev, "pll");
	if (IS_ERR(clk_pll)) {
		pr_err("MFG cannot get pll clock\n");
		clk_pll = NULL;
		goto error_out;
	}

	clk_topck = devm_clk_get(&pdev->dev, "topck");
	if (IS_ERR(clk_topck)) {
		pr_err("MFG cannot get topck clock\n");
		clk_topck = NULL;
		goto error_out;
	}

	clk_prepare_enable(clk_topck);

	clk_set_parent(clk_topck, clk_pll);
	parent = clk_get_parent(clk_topck);
	if (!IS_ERR_OR_NULL(parent)) {
		freq = clk_get_rate(parent);
		pr_info("MFG is now selected to %s, parent rate %lu\n", parent->name, freq);
	} else {
		pr_err("Failed to select mfg\n");
		BUG();
	}

	clk_disable_unprepare(clk_topck);

	mfg_dev = pdev;
	pm_runtime_enable(&pdev->dev);

	pr_info("MFG device probed\n");
	return 0;

error_out:
	if (clk_topck)
		devm_clk_put(&pdev->dev, clk_topck);
	if (clk_pll)
		devm_clk_put(&pdev->dev, clk_pll);
	if (mfg_start)
		iounmap(mfg_start);

	return -1;
}

static int mfg_device_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

	if (clk_topck)
		devm_clk_put(&pdev->dev, clk_topck);
	if (clk_pll)
		devm_clk_put(&pdev->dev, clk_pll);
	if (mfg_start)
		iounmap(mfg_start);

	return 0;
}

static struct platform_driver mtk_mfg_driver = {
	.probe = mfg_device_probe,
	.remove = mfg_device_remove,
	.driver = {
		   .name = "mfg",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(mfg_dt_ids),
		   },
};


/* This function assumes power domains and smi are enabled. */
void mtk_mfg_wait_gpu_idle(void)
{
	int polling_count = 100000;

	/* Polling the MFG_DEBUG_REG for checking GPU IDLE before MTCMOS power off (0.1ms) */
	MFG_WRITE32(0x3, MFG_DEBUG_SEL);
	do {
		/* 0x13000184[2]  */
		/* 1'b1: bus idle */
		/* 1'b0: bus busy */
		if (MFG_READ32(MFG_DEBUG_STAT) & (1 << 2))
			return;
	} while (polling_count--);

	if (polling_count <= 0) {
		pr_err("[MALI]!!!!MFG(GPU) subsys is still BUSY!!!!!, polling_count=%d\n",
		       polling_count);
	}
}

void mfg_dump_regs(const char * prefix)
{
	u32 val;
	unsigned long off;

	dev_info(&mfg_dev->dev, "MFG BEGIN %s\n", prefix);

	for (off = 0; off < 0x1a0; off += 4) {
		val = MFG_READ32(off);
		if (val != 0) {
			dev_info(&mfg_dev->dev, "  [0x%lx] : 0x%x\n", off, val);
		}
	}

	dev_info(&mfg_dev->dev, "MFG END\n");
}

#ifdef DEBUG
#define CHECK_MFG_STAT(off, exp_val) \
do {\
	u32 con;	\
	con = MFG_READ32(off);	\
	if (exp_val != con) { \
		dev_info(&mfg_dev->dev, "MFG %s #%d read con: %#x, expect %#x\n", __func__, __LINE__, con, exp_val); \
		mfg_dump_regs(__FUNCTION__); \
	}	\
} while (0)
#else
#define CHECK_MFG_STAT(off, exp_val) do{} while(0)
#endif


void mtk_mfg_enable_gpu() {
	if (mfg_start != NULL) {
		u32 con = MFG_READ32(MFG_CG_CON);
		if (con != 0x0)
			MFG_WRITE32(0x1, MFG_CG_CLR);

		CHECK_MFG_STAT(MFG_CG_CON, 0x0);
	}
}

void mtk_mfg_disable_gpu() {
	if (mfg_start != NULL) {	
		u32 con = MFG_READ32(MFG_CG_CON);
		if (con == 0x0)
			MFG_WRITE32(0x1, MFG_CG_SET);
		else 
			dev_info(&mfg_dev->dev, "Disable already\n");

		CHECK_MFG_STAT(MFG_CG_CON, 0x1);
	}
}

bool mtk_mfg_is_ready(void) {
	return (mfg_start != NULL);
}

unsigned long mtk_mfg_get_frequence(void) {
	return mt_gpufreq_get_cur_freq();
}

static int __init mfg_driver_init(void)
{
	return platform_driver_register(&mtk_mfg_driver);
}

module_init(mfg_driver_init);

