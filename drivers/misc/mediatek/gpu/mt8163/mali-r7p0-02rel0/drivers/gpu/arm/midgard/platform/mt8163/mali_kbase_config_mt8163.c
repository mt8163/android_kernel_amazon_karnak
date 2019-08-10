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
#include "mali_kbase_config_platform.h"

#include "mtk_mfg.h"
#include "mtk_mali_dvfs.h"

#define HARD_RESET_AT_POWER_OFF 0

/* Enable this after gpufreq is ready */
#define MTK_MALI_ENABLE_DVFS

extern struct platform_device *mfg_dev;


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

#ifdef MTK_MALI_ENABLE_DVFS
	mtk_mali_dvfs_notify_utilization(utilisation);
#endif

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


#ifdef CONFIG_OF
static const struct of_device_id mfg_dt_ids[] = {
	{.compatible = "mediatek,mt8163-mfg"},
	{.compatible = "mediatek,midgard-mfg"},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, mfg_dt_ids);
#endif


static int mtk_platform_init(struct kbase_device *kbdev)
{
	pm_runtime_enable(kbdev->dev);

#ifdef MTK_MALI_ENABLE_DVFS
	mtk_mali_dvfs_init();
#endif

	/* MTK MET */
	mtk_get_gpu_loading_fp = mtk_met_get_mali_loading;

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
	pm_runtime_get_sync(&mfg_dev->dev);
	pm_runtime_get_sync(kbdev->dev);

	mtk_mfg_enable_gpu();

#ifdef MTK_MALI_ENABLE_DVFS
	mtk_mali_dvfs_power_on();
#endif

	return 1;
}

static void pm_callback_power_off(struct kbase_device *kbdev)
{
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

#ifdef MTK_MALI_ENABLE_DVFS
	mtk_mali_dvfs_power_off();
#endif

	mtk_mfg_disable_gpu();

	pm_runtime_put_sync_suspend(kbdev->dev);
	pm_runtime_put_sync(&mfg_dev->dev);
}


struct kbase_pm_callback_conf pm_callbacks = {
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
	.power_suspend_callback = NULL,
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
	if (!mtk_mfg_is_ready()) {
		pr_info("mfg is not ready, defer GPU\n");
		return -EPROBE_DEFER;
	}
	return 0;
}
/* Platform end */

/* PROC */
#ifdef CONFIG_PROC_FS
static int utilization_seq_show(struct seq_file *m, void *v)
{
	unsigned long freq = 0;	
	freq = mtk_mfg_get_frequence();
	if (snap_loading > 0)
		seq_printf(m, "gpu/cljs0/cljs1=%u/%u/%u [Freq : %lu]\n",
			   snap_loading, snap_cl0, snap_cl1, freq);
	else
		seq_printf(m, "gpu=%u [Freq : %lu]\n", 0, freq);

	return 0;
}

static int utilization_seq_open(struct inode *in, struct file *file)
{
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
	seq_puts(m, "MTK Mali dvfs is built\n");
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
		mtk_mali_dvfs_enable();
	else if (!strncmp(desc, "0", 1))
		mtk_mali_dvfs_disable();

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

	mtk_mali_dvfs_get_threshold(&min_loading, &max_loading);
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
		mtk_mali_dvfs_set_threshold(min_loading, max_loading);

	return count;
}

static const struct file_operations dvfs_threshhold_proc_fops = {
	.open = dvfs_threshhold_seq_open,
	.read = seq_read,
	.write = dvfs_threshhold_seq_write,
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
		proc_create_data("dvfs_threshold", 0, mali_pentry, &dvfs_threshhold_proc_fops, kbdev);
#endif
	}
}

#endif /*CONFIG_PROC_FS */

