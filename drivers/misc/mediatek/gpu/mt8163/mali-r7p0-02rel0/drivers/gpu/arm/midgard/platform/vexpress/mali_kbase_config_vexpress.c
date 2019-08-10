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
#include "mali_kbase_cpu_vexpress.h"
#include "mali_kbase_config_platform.h"

#define HARD_RESET_AT_POWER_OFF 0

/* Enable this after gpufreq is ready */
#undef MTK_MALI_ENABLE_DVFS


#include "mt_gpufreq.h"

#ifndef CONFIG_OF
static struct kbase_io_resources io_resources = {
	.job_irq_number = 68,
	.mmu_irq_number = 69,
	.gpu_irq_number = 70,
	.io_memory_region = {
	.start = 0xFC010000,
	.end = 0xFC010000 + (4096 * 4) - 1
	}
};
#endif /* CONFIG_OF */


#ifdef CONFIG_PROC_FS
static void mtk_mali_create_proc(struct kbase_device *kbdev);
#endif

/* clk */
static struct clk *clk_mm;
static struct clk *clk_pll;
static struct clk *clk_topck;

/*  */
#ifdef MTK_MALI_ENABLE_DVFS
/**
 * DVFS implementation
 */
enum mtk_dvfs_status {
	MTK_DVFS_STATUS_NORMAL = 0,
	MTK_DVFS_STATUS_BOOST,
	MTK_DVFS_STATUS_DISABLE
};

static const char * const status_str[] = {
	"Normal",
	"Boosting",
	"Disabled"
};

#define STATUS_STR(s) (status_str[(s)])


enum mtk_dvfs_action {
	MTK_DVFS_NOP = KBASE_PM_DVFS_NOP,		    /**< No change in clock frequency is requested */
	MTK_DVFS_CLOCK_UP = KBASE_PM_DVFS_CLOCK_UP,		    /**< The clock frequency should be increased if possible */
	MTK_DVFS_CLOCK_DOWN = KBASE_PM_DVFS_CLOCK_DOWN,	   /**< The clock frequency should be decreased if possible */
	MTK_DVFS_BOOST,
	MTK_DVFS_POWER_ON,
	MTK_DVFS_POWER_OFF,
	MTK_DVFS_ENABLE,
	MTK_DVFS_DISABLE
};
static const char * const action_str[] = {
	"Nop",
	"Up",
	"Down",
	"Boost",
	"PowerOn",
	"PowerOff",
	"Enable",
	"Disable"
};
#define ACTION_STR(a) (action_str[(a)])

struct mtk_mali_dvfs {
	/* Pending mali action from metrics 
	 * Mali will set action in atomic context, mali_action and the spin lock is for
	 * sending them to wq.
	 */
	struct work_struct work;
	enum kbase_pm_dvfs_action mali_action; 	
	spinlock_t lock; /* mali action lock */

	int min_freq_idx;
	int max_freq_idx;
	int restore_idx;  /* index before powered off */

	int boosted;
	enum mtk_dvfs_status status;
	int power_on;

	struct mutex mlock; /* dvfs lock*/

	enum mtk_dvfs_action  last_action;	/*debug */
};
static struct mtk_mali_dvfs mtk_dvfs;

#define TOP_IDX(dvfs) ((dvfs)->min_freq_idx)
#define BOTTOM_IDX(dvfs) ((dvfs)->max_freq_idx)
#define LOWER_IDX(dvfs, idx) (((idx) >= (dvfs)->max_freq_idx)?(idx):((idx) + 1))
#define UPPER_IDX(dvfs, idx) (((idx) <= (dvfs)->min_freq_idx)?(idx):((idx) - 1))

#define IS_UPPER(idx1, idx2) ((idx1) < (idx2))


static void mtk_process_dvfs(struct mtk_mali_dvfs *dvfs, enum mtk_dvfs_action  action)
{
	unsigned int request_freq_idx;
	unsigned int cur_freq_idx;
	enum mtk_dvfs_status last_status;

	mutex_lock(&dvfs->mlock);

	request_freq_idx = cur_freq_idx = mt_gpufreq_get_cur_freq_index();

	last_status = dvfs->status;

	switch (last_status) {
	case MTK_DVFS_STATUS_DISABLE:
		if (action == MTK_DVFS_ENABLE)
			dvfs->status = MTK_DVFS_STATUS_NORMAL;
		/*ignore any actions */
		request_freq_idx = TOP_IDX(dvfs);
		break;
	case MTK_DVFS_STATUS_BOOST:
		if (action == MTK_DVFS_DISABLE) {
			dvfs->status = MTK_DVFS_STATUS_DISABLE;
			dvfs->boosted = 0;
			request_freq_idx = TOP_IDX(dvfs);

		} else if (action == MTK_DVFS_POWER_OFF) {
			/* status not changed.. */
			/* dvfs->status = MTK_DVFS_STATUS_POWER_OFF; */
			request_freq_idx = BOTTOM_IDX(dvfs);

		} else if (action == MTK_DVFS_POWER_ON) {
			request_freq_idx = TOP_IDX(dvfs);

		} else {
			/* should be up/down also nop. */
			dvfs->boosted++;
			if (dvfs->boosted >= 4)
				dvfs->status = MTK_DVFS_STATUS_NORMAL;
			request_freq_idx = TOP_IDX(dvfs);
		}

		break;
	case MTK_DVFS_STATUS_NORMAL:
		if (action == MTK_DVFS_CLOCK_UP) {
			request_freq_idx = UPPER_IDX(dvfs, cur_freq_idx);

		} else if (action == MTK_DVFS_CLOCK_DOWN) {
			request_freq_idx = LOWER_IDX(dvfs, cur_freq_idx);

		} else if (action == MTK_DVFS_POWER_OFF) {
			dvfs->restore_idx = cur_freq_idx;
			request_freq_idx = BOTTOM_IDX(dvfs);

		} else if (action == MTK_DVFS_POWER_ON) {
			request_freq_idx = dvfs->restore_idx;

		} else if (action == MTK_DVFS_BOOST) {
			dvfs->status = MTK_DVFS_STATUS_BOOST;
			dvfs->boosted = 0;
			request_freq_idx = TOP_IDX(dvfs);

		} else if (action == MTK_DVFS_DISABLE) {
			dvfs->status = MTK_DVFS_STATUS_DISABLE;
			request_freq_idx = TOP_IDX(dvfs);

		}
		break;
	default:
		break;
	}

	if (action != MTK_DVFS_NOP) {
		/* log status change */
		pr_debug("GPU action: %s, status: %s -> %s, idx: %d -> %d\n",
			ACTION_STR(action),
			STATUS_STR(last_status),
			STATUS_STR(dvfs->status), cur_freq_idx, request_freq_idx);
	}


	if (cur_freq_idx != request_freq_idx) {
		pr_debug("Try set gpu freq %d , cur idx is %d ", request_freq_idx, cur_freq_idx);
		/* may fail for thermal protecting */
		mt_gpufreq_target(request_freq_idx);
		cur_freq_idx = mt_gpufreq_get_cur_freq_index();
		pr_debug("After set gpu freq, cur idx is %d ", cur_freq_idx);
	}
	/* common part : */
	if (action == MTK_DVFS_POWER_ON)
		dvfs->power_on = 1;
	else if (action == MTK_DVFS_POWER_OFF)
		dvfs->power_on = 0;

	dvfs->last_action = action;

	mutex_unlock(&dvfs->mlock);
}


static void mtk_process_mali_action(struct work_struct *work)
{
	struct mtk_mali_dvfs *dvfs = container_of(work, struct mtk_mali_dvfs, work);

	unsigned long flags;
	enum mtk_dvfs_action action;

	spin_lock_irqsave(&dvfs->lock, flags);
	action = (enum mtk_dvfs_action) dvfs->mali_action;
	spin_unlock_irqrestore(&dvfs->lock, flags);

	mtk_process_dvfs(dvfs, action);
}

static void input_boost_notify(unsigned int idx)
{
	/*Idx is assumed to be TOP */
	mtk_process_dvfs(&mtk_dvfs, MTK_DVFS_BOOST);
}

static void power_limit_notify(unsigned int idx)
{
	unsigned int cur_idx;

	mutex_lock(&mtk_dvfs.mlock);
	cur_idx = mt_gpufreq_get_cur_freq_index();
	if (IS_UPPER(cur_idx, idx))
		mt_gpufreq_target(idx);

	mutex_unlock(&mtk_dvfs.mlock);
}

static void mtk_mali_dvfs_init(void)
{
	mtk_dvfs.restore_idx = mt_gpufreq_get_cur_freq_index();
	mtk_dvfs.max_freq_idx = mt_gpufreq_get_dvfs_table_num() - 1;
	mtk_dvfs.min_freq_idx = 0;
	mtk_dvfs.status = MTK_DVFS_STATUS_NORMAL;
	mtk_dvfs.power_on = 0;
	mtk_dvfs.last_action = MTK_DVFS_NOP;
	mutex_init(&mtk_dvfs.mlock);

	mtk_dvfs.mali_action = KBASE_PM_DVFS_NOP;
	spin_lock_init(&mtk_dvfs.lock);

	INIT_WORK(&mtk_dvfs.work, mtk_process_mali_action);

	mt_gpufreq_input_boost_notify_registerCB(input_boost_notify);
	mt_gpufreq_power_limit_notify_registerCB(power_limit_notify);

}

static void mtk_mali_dvfs_deinit(void)
{
	mt_gpufreq_power_limit_notify_registerCB(NULL);
	mt_gpufreq_input_boost_notify_registerCB(NULL);
}


void kbase_platform_dvfs_action(struct kbase_device *kbdev, enum kbase_pm_dvfs_action action)
{
	unsigned long flags;
	struct mtk_mali_dvfs *dvfs = &mtk_dvfs;

	spin_lock_irqsave(&dvfs->lock, flags);
	dvfs->mali_action = action;
	schedule_work(&dvfs->work);
	spin_unlock_irqrestore(&dvfs->lock, flags);
}
#else

#if 0
void kbase_platform_dvfs_action(struct kbase_device *kbdev, enum kbase_pm_dvfs_action action)
{
	/* Nothing to do */
}
#endif
	
#endif

/* Simply recording for MET tool */
static u32 snap_loading;
static u32 snap_cl0;
static u32 snap_cl1;

int kbase_platform_dvfs_event(struct kbase_device *kbdev,
			      u32 utilisation, u32 util_gl_share, u32 util_cl_share[2])
{

	snap_loading = utilisation;
	snap_cl0 = util_cl_share[0];
	snap_cl1 = util_cl_share[1];

	return 0;
}

static u32 mtk_met_get_mali_loading(void)
{
	u32 loading = snap_loading;
	return loading;
}
KBASE_EXPORT_SYMBOL(mtk_met_get_mali_loading);
extern unsigned int (*mtk_get_gpu_loading_fp)(void);
/* MET part end */

/* MFG begin
 * GPU controller.
 *
 * If GPU power domain needs to be enabled after disp, we will break flow
 * of mfg_enable_gpu to be 2 functions like mfg_prepare/mfg_enable.
 *
 * Currently no lock for mfg, Mali driver will take .
 */

#define MFG_CG_CON 0x0
#define MFG_CG_SET 0x4
#define MFG_CG_CLR 0x8
#define MFG_DEBUG_SEL 0x180
#define MFG_DEBUG_STAT 0x184

#define MFG_READ32(r) __raw_readl((void __iomem *)((unsigned long)mfg_start + (r)))
#define MFG_WRITE32(v, r) __raw_writel((v), (void __iomem *)((unsigned long)mfg_start + (r)))

static struct platform_device *mfg_dev;
static int mfg_enable_count;
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

	pr_info("MFG device probed\n");
	
	mfg_start = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR_OR_NULL(mfg_start)) {
		mfg_start = NULL;
		goto error_out;
	}
	pr_debug("MFG start is mapped %p\n", mfg_start);

	clk_mm = devm_clk_get(&pdev->dev, "mm");
	if (IS_ERR(clk_mm)) {
		pr_err("MFG cannot get mm clock\n");
		clk_mm = NULL;
		goto error_out;
	}

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
		pr_info("MFG is now selected to %s\n", parent->name);
		freq = clk_get_rate(parent);
		pr_info("MFG parent rate %lu\n", freq);
		/* Should set by goufreq? */
		/* clk_set_rate(parent, 520000000); */
	} else {
		pr_err("Failed to select mfg\n");
		BUG();
	}

	pm_runtime_enable(&pdev->dev);

	mfg_dev = pdev;

	return 0;
error_out:
	if (mfg_start)
		iounmap(mfg_start);
	if (clk_mm)
		devm_clk_put(&pdev->dev, clk_mm);
	if (clk_pll)
		devm_clk_put(&pdev->dev, clk_pll);
	if (clk_topck)
		devm_clk_put(&pdev->dev, clk_topck);

	return -1;
}

static int mfg_device_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

	if (mfg_start)
		iounmap(mfg_start);
	if (clk_mm)
		devm_clk_put(&pdev->dev, clk_mm);
	if (clk_pll)
		devm_clk_put(&pdev->dev, clk_pll);
	if (clk_topck) {
		clk_disable_unprepare(clk_topck);
		devm_clk_put(&pdev->dev, clk_topck);
	}

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

#if 0
static struct platform_device mtk_mfg_device = {
	.name = "mfg",
	.id = -1,
};
#endif

/* This function assumes power domains and smi are enabled. */
static void mfg_wait_gpu_idle(void)
{
	int polling_count = 100000;
	/* Polling the MFG_DEBUG_REG for checking GPU IDLE before MTCMOS power off (0.1ms) */
	MFG_WRITE32(0x3, MFG_DEBUG_SEL);
	do {
		/* / 0x13000184[2] */
		/* / 1'b1: bus idle */
		/* / 1'b0: bus busy */
		if (MFG_READ32(MFG_DEBUG_STAT) & (1 << 2))
			return;
	} while (polling_count--);

	if (polling_count <= 0) {
		pr_err("[MALI]!!!!MFG(GPU) subsys is still BUSY!!!!!, polling_count=%d\n",
		       polling_count);
	}
}

#define DEBUG_MFG_STAT \
do {\
	u32 con;\
	con = MFG_READ32(MFG_CG_CON);\
	pr_debug("MFG %s #%d CON: %d\n", __func__, __LINE__, con);	\
} while (0)

static void mfg_enable_gpu(void)
{
	if (mfg_start == NULL)
		return;
	mfg_enable_count++;
	if (mfg_enable_count == 1) {
		/*
		 * Enable mmsys before enable mm clock.
		 * See MFG driver's power-domain.
		 */
		pm_runtime_get_sync(&mfg_dev->dev);
		/*
		 * Enable mm before accessing MFG registers.
		 */
		clk_prepare_enable(clk_mm);
		DEBUG_MFG_STAT;
		MFG_WRITE32(0x1, MFG_CG_CLR);
		DEBUG_MFG_STAT;
	} else {
		pr_warn("MFG: Mali called enable when count %d\n", mfg_enable_count);
		DEBUG_MFG_STAT;
	}
}

static void mfg_disable_gpu(void)
{
	if (mfg_start == NULL)
		return;
	mfg_enable_count--;
	if (mfg_enable_count == 0) {
		DEBUG_MFG_STAT;
		MFG_WRITE32(0x1, MFG_CG_SET);
		DEBUG_MFG_STAT;
		clk_disable_unprepare(clk_mm);
		pm_runtime_put_sync(&mfg_dev->dev);
	} else {
		pr_warn("MFG: Mali called disable when count %d\n", mfg_enable_count);
		DEBUG_MFG_STAT;
	}
}

static int __init mfg_driver_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_mfg_driver);
	return ret;
}

/* We need make mfg probed before GPU */
subsys_initcall(mfg_driver_init);


/* MFG end */
static int mtk_platform_init(struct kbase_device *kbdev)
{
	pm_runtime_enable(kbdev->dev);
#ifdef MTK_MALI_ENABLE_DVFS
	mtk_mali_dvfs_init();
#endif
	/* MTK MET */
	mtk_get_gpu_loading_fp = mtk_met_get_mali_loading;

	/* Memory report ... */
	/* mtk_get_gpu_memory_usage_fp = kbase_report_gpu_memory_usage; */
#ifdef CONFIG_PROC_FS
	mtk_mali_create_proc(kbdev);
#endif
	return 0;
}

static void mtk_platform_term(struct kbase_device *kbdev)
{
#ifdef MTK_MALI_ENABLE_DVFS
	mtk_mali_dvfs_deinit();
#endif
	pm_runtime_disable(kbdev->dev);
}
struct kbase_platform_funcs_conf platform_func = {
	.platform_init_func = mtk_platform_init,
	.platform_term_func = mtk_platform_term
};

static int pm_callback_power_on(struct kbase_device *kbdev)
{
	pm_runtime_get_sync(kbdev->dev);
	mfg_enable_gpu();

#ifdef MTK_MALI_ENABLE_DVFS
	mtk_process_dvfs(&mtk_dvfs, MTK_DVFS_POWER_ON);
#endif
	/* Nothing is needed on VExpress, but we may have destroyed GPU state (if the below HARD_RESET code is active) */
	return 1;
}


static void pm_callback_power_off(struct kbase_device *kbdev)
{
	/* / 1. Delay 0.01ms before power off */
	int i;

	for (i = 0; i < 100000; i++)
		;

	mfg_wait_gpu_idle();

#if HARD_RESET_AT_POWER_OFF
	/* Cause a GPU hard reset to test whether we have actually idled the GPU
	 * and that we properly reconfigure the GPU on power up.
	 * Usually this would be dangerous, but if the GPU is working correctly it should
	 * be completely safe as the GPU should not be active at this point.
	 * However this is disabled normally because it will most likely interfere with
	 * bus logging etc.
	 */
	/* KBASE_TRACE_ADD(kbdev, CORE_GPU_HARD_RESET, NULL, NULL, 0u, 0); */
	kbase_os_reg_write(kbdev, GPU_CONTROL_REG(GPU_COMMAND), GPU_COMMAND_HARD_RESET);
#endif

	mfg_wait_gpu_idle();

#ifdef MTK_MALI_ENABLE_DVFS
	mtk_process_dvfs(&mtk_dvfs, MTK_DVFS_POWER_OFF);
#endif

	mfg_disable_gpu();
	pm_runtime_put_sync(kbdev->dev);

#if 0				/* Nothing to do now for 8163 */
	mt_gpufreq_voltage_enable_set(0);
#endif
}


struct kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
	.power_suspend_callback  = NULL,
	.power_resume_callback = NULL
};

static struct kbase_platform_config versatile_platform_config = {
#ifndef CONFIG_OF
	.io_resources = &io_resources
#endif
};

struct kbase_platform_config *kbase_get_platform_config(void)
{
	return &versatile_platform_config;
}


int kbase_platform_early_init(void)
{
	/* Nothing needed at this stage */
	return 0;
}

/* Platform end */

/* PROC */
#ifdef CONFIG_PROC_FS
#if 1				/*TEMP disable */
static int utilization_seq_show(struct seq_file *m, void *v)
{
	unsigned long freq = 0;	

	if(clk_pll != NULL) {
		freq = clk_get_rate(clk_pll);
	}

	if (snap_loading > 0)
		seq_printf(m, "gpu/cljs0/cljs1=%u/%u/%u [Freq : %lu]\n",
			   snap_loading, snap_cl0, snap_cl1, freq);
	else
		seq_printf(m, "gpu=%u [Freq : %lu]\n", 0, freq);
	return 0;
}

static int utilization_seq_open(struct inode *in, struct file *file)
{
/* struct kbase_device *kbdev = PDE_DATA(file_inode(file)); */
	return single_open(file, utilization_seq_show, NULL);
}

static const struct file_operations utilization_proc_fops = {
	.open = utilization_seq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dvfs_seq_show(struct seq_file *m, void *v)
{
#ifdef MTK_MALI_ENABLE_DVFS
	mutex_lock(&mtk_dvfs.mlock);
	seq_printf(m,
		   "Status : %s\nLast action : %s\nIndex : %d [%d-%d]\nPower : %d\nBoosted count : %d\necho 0 can disable\n",
		   STATUS_STR(mtk_dvfs.status), ACTION_STR(mtk_dvfs.last_action),
		   mt_gpufreq_get_cur_freq_index(), mtk_dvfs.min_freq_idx, mtk_dvfs.max_freq_idx,
		   mtk_dvfs.power_on, mtk_dvfs.boosted);

	mutex_unlock(&mtk_dvfs.mlock);
#else
	seq_puts(m, "MTK Mali dvfs is disabled in build\n");
#endif
	return 0;
}

static int dvfs_seq_open(struct inode *in, struct file *file)
{
	return single_open(file, dvfs_seq_show, NULL);
}

static ssize_t dvfs_seq_write(struct file *file, const char __user *buffer,
			      size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';
#ifdef MTK_MALI_ENABLE_DVFS
	if (!strncmp(desc, "1", 1))
		mtk_process_dvfs(&mtk_dvfs, MTK_DVFS_ENABLE);
	else if (!strncmp(desc, "0", 1))
		mtk_process_dvfs(&mtk_dvfs, MTK_DVFS_DISABLE);

	return count;
#else
	return 0;
#endif
}

static const struct file_operations dvfs_proc_fops = {
	.open = dvfs_seq_open,
	.read = seq_read,
	.write = dvfs_seq_write,
	.release = single_release,
};


static int memory_usage_seq_show(struct seq_file *m, void *v)
{
	/* just use debugfs impl to calculate */
	const struct list_head *kbdev_list;
	struct list_head *entry;
	unsigned long usage = 0;

	kbdev_list = kbase_dev_list_get();

	list_for_each(entry, kbdev_list) {
		struct kbase_device *kbdev = NULL;

		kbdev = list_entry(entry, struct kbase_device, entry);
		usage += PAGE_SIZE * atomic_read(&(kbdev->memdev.used_pages));
	}
	kbase_dev_list_put(kbdev_list);

	seq_printf(m, "%ld\n", usage);

	return 0;
}

static int memory_usage_seq_open(struct inode *in, struct file *file)
{
	return single_open(file, memory_usage_seq_show, NULL);
}


static const struct file_operations memory_usage_proc_fops = {
	.open = memory_usage_seq_open,
	.read = seq_read,
	.release = single_release,
};

#ifdef MTK_MALI_ENABLE_DVFS
static int dvfs_threshhold_seq_show(struct seq_file *m, void *v)
{
	u32 min_loading;
	u32 max_loading;

	kbase_pm_metrics_get_threshold(&min_loading, &max_loading);
	seq_printf(m, "min:%d, max:%d\n", min_loading, max_loading);
	return 0;
}

static int dvfs_threshhold_seq_open(struct inode *in, struct file *file)
{
	return single_open(file, dvfs_threshhold_seq_show, NULL);
}

static ssize_t dvfs_threshhold_seq_write(struct file *file, const char __user *buffer,
					 size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	u32 min_loading;
	u32 max_loading;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &min_loading, &max_loading) == 2)
		kbase_pm_metrics_set_threshold(min_loading, max_loading);

	return count;
}

static const struct file_operations dvfs_threshhold_proc_fops = {
	.open = dvfs_threshhold_seq_open,
	.read = seq_read,
	.write = dvfs_threshhold_seq_write,
	.release = single_release,
};

static int dvfs_timer_seq_show(struct seq_file *m, void *v)
{
	u32 time;
	struct kbase_device *kbdev = (struct kbase_device *)m->private;

	time = kbase_pm_metrics_get_timer(kbdev);
	seq_printf(m, "%d\n", time);
	return 0;
}

static int dvfs_timer_seq_open(struct inode *in, struct file *file)
{
	struct kbase_device *kbdev = PDE_DATA(file_inode(file));

	return single_open(file, dvfs_timer_seq_show, kbdev);
}

static ssize_t dvfs_timer_seq_write(struct file *file, const char __user *buffer,
				    size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	u32 time;

	struct kbase_device *kbdev = PDE_DATA(file_inode(file));

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &time))
		kbase_pm_metrics_set_timer(kbdev, time);

	return count;
}

static const struct file_operations dvfs_timer_proc_fops = {
	.open = dvfs_timer_seq_open,
	.read = seq_read,
	.write = dvfs_timer_seq_write,
	.release = single_release,
};
#endif

static void mtk_mali_create_proc(struct kbase_device *kbdev)
{
	struct proc_dir_entry *mali_pentry = proc_mkdir("mali", NULL);

	if (mali_pentry) {
		proc_create_data("utilization", 0, mali_pentry, &utilization_proc_fops, kbdev);
		proc_create_data("dvfs", 0, mali_pentry, &dvfs_proc_fops, kbdev);
		proc_create_data("memory_usage", 0, mali_pentry, &memory_usage_proc_fops, kbdev);
#ifdef MTK_MALI_ENABLE_DVFS
		proc_create_data("dvfs_threshold", 0, mali_pentry, &dvfs_threshhold_proc_fops,
				 kbdev);
		proc_create_data("dvfs_timer", 0, mali_pentry, &dvfs_timer_proc_fops, kbdev);
#endif
	}
}
#endif				/* 0 */
#endif				/*CONFIG_PROC_FS */