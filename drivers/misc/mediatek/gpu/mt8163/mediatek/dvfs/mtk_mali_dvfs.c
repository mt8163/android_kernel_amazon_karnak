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

#include "mtk_mali_dvfs.h"


#define DVFS_LOGD pr_debug
#define DVFS_LOGE pr_err

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
	MTK_DVFS_NOP = 0,		    /**< No change in clock frequency is requested */
	MTK_DVFS_CLOCK_UP,		    /**< The clock frequency should be increased if possible */
	MTK_DVFS_CLOCK_DOWN,	   /**< The clock frequency should be decreased if possible */
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
	enum mtk_dvfs_action mali_action; 
	int threshold_down;
	int threshold_up;
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

	if (action != MTK_DVFS_NOP && (cur_freq_idx != request_freq_idx)) {
		/* log status change */
		DVFS_LOGD("GPU action: %s, status: %s -> %s, idx: %d -> %d\n",
			ACTION_STR(action),
			STATUS_STR(last_status),
			STATUS_STR(dvfs->status), cur_freq_idx, request_freq_idx);
	}

	if (cur_freq_idx != request_freq_idx) {
		/* may fail for thermal protecting */
		mt_gpufreq_target(request_freq_idx);
		cur_freq_idx = mt_gpufreq_get_cur_freq_index();
		DVFS_LOGD("After set gpu freq, cur idx is %d \n", cur_freq_idx);
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

static void MTKSetBottomGPUFreq(unsigned int ui32FreqLevel)
{
	/*Idx is assumed to be TOP for GED boost*/
	if (ui32FreqLevel == mt_gpufreq_get_dvfs_table_num() - 1)
		mtk_process_dvfs(&mtk_dvfs, MTK_DVFS_BOOST);
}

extern void (*mtk_set_bottom_gpu_freq_fp)(unsigned int);
void mtk_mali_dvfs_init(void)
{
	mtk_dvfs.restore_idx = mt_gpufreq_get_cur_freq_index();
	mtk_dvfs.max_freq_idx = mt_gpufreq_get_dvfs_table_num() - 1;
	mtk_dvfs.min_freq_idx = 0;
	mtk_dvfs.status = MTK_DVFS_STATUS_NORMAL;
	mtk_dvfs.power_on = 0;
	mtk_dvfs.last_action = MTK_DVFS_NOP;
	
	mutex_init(&mtk_dvfs.mlock);

	mtk_dvfs.threshold_down = 50;
	mtk_dvfs.threshold_up = 70;
	mtk_dvfs.mali_action = MTK_DVFS_NOP;
	spin_lock_init(&mtk_dvfs.lock);

	INIT_WORK(&mtk_dvfs.work, mtk_process_mali_action);

	mt_gpufreq_input_boost_notify_registerCB(input_boost_notify);
	mt_gpufreq_power_limit_notify_registerCB(power_limit_notify);

	mtk_set_bottom_gpu_freq_fp = MTKSetBottomGPUFreq;

	DVFS_LOGD("inited mali dvfs \n");

}

void mtk_mali_dvfs_deinit(void)
{
	mt_gpufreq_power_limit_notify_registerCB(NULL);
	mt_gpufreq_input_boost_notify_registerCB(NULL);
}

/*
 * Some version of Mali driver implementation does not callback with mali orignal action.
 * So we just need utilization callback. Leave this for r5 or feature version.
 */
#if 0
void kbase_platform_dvfs_action(struct kbase_device *kbdev, enum kbase_pm_dvfs_action action)
{
	unsigned long flags;
	struct mtk_mali_dvfs *dvfs = &mtk_dvfs;

	spin_lock_irqsave(&dvfs->lock, flags);
	dvfs->mali_action = action;
	schedule_work(&dvfs->work);
	spin_unlock_irqrestore(&dvfs->lock, flags);
}
#endif
 
void mtk_mali_dvfs_notify_utilization(int utilization) {
	unsigned long flags;

	spin_lock_irqsave(&mtk_dvfs.lock, flags);
	
	if(utilization < mtk_dvfs.threshold_down) {
		mtk_dvfs.mali_action = MTK_DVFS_CLOCK_DOWN;
	} else 	if(utilization > mtk_dvfs.threshold_up) {
		mtk_dvfs.mali_action = MTK_DVFS_CLOCK_UP;
	} else {
		mtk_dvfs.mali_action = MTK_DVFS_NOP;
	}
	schedule_work(&mtk_dvfs.work);
	
	spin_unlock_irqrestore(&mtk_dvfs.lock, flags);
	
}

void mtk_mali_dvfs_set_threshold(int down, int up) {
	unsigned long flags;
	
	spin_lock_irqsave(&mtk_dvfs.lock, flags);
	mtk_dvfs.threshold_down = down;
	mtk_dvfs.threshold_up = up;
	spin_unlock_irqrestore(&mtk_dvfs.lock, flags);	
}

void mtk_mali_dvfs_get_threshold(int *down, int *up) {
	unsigned long flags;
	
	spin_lock_irqsave(&mtk_dvfs.lock, flags);
	*down = mtk_dvfs.threshold_down;
	*up = mtk_dvfs.threshold_up;
	spin_unlock_irqrestore(&mtk_dvfs.lock, flags);	
}



void mtk_mali_dvfs_power_on(void) {
	mtk_process_dvfs(&mtk_dvfs, MTK_DVFS_POWER_ON);
}

void mtk_mali_dvfs_power_off(void) {
	mtk_process_dvfs(&mtk_dvfs, MTK_DVFS_POWER_OFF);
}

void mtk_mali_dvfs_enable(void){
	mtk_process_dvfs(&mtk_dvfs, MTK_DVFS_ENABLE);
}

void mtk_mali_dvfs_disable(void){
	mtk_process_dvfs(&mtk_dvfs, MTK_DVFS_DISABLE);
}

