/* drivers/misc/lowmemorykiller.c
 *
 * The lowmemorykiller driver lets user-space specify a set of memory thresholds
 * where processes with a range of oom_score_adj values will get killed. Specify
 * the minimum oom_score_adj values in
 * /sys/module/lowmemorykiller/parameters/adj and the number of free pages in
 * /sys/module/lowmemorykiller/parameters/minfree. Both files take a comma
 * separated list of numbers in ascending order.
 *
 * For example, write "0,8" to /sys/module/lowmemorykiller/parameters/adj and
 * "1024,4096" to /sys/module/lowmemorykiller/parameters/minfree to kill
 * processes with a oom_score_adj value of 8 or higher when the free memory
 * drops below 4096 pages and kill processes with a oom_score_adj value of 0 or
 * higher when the free memory drops below 1024 pages.
 *
 * The driver considers memory used for caches to be free, but if a large
 * percentage of the cached memory is locked this can be very inaccurate
 * and processes may not get killed until the normal oom killer is triggered.
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/swap.h>
#include <linux/rcupdate.h>
#include <linux/profile.h>
#include <linux/notifier.h>
#include <linux/freezer.h>
#include <linux/ratelimit.h>

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(CONFIG_MTK_ENG_BUILD)
#include <mt-plat/aee.h>
#include <disp_assert_layer.h>
#endif

/* fosmod_fireos_crash_reporting begin */
#include <linux/module.h>
#include<linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#ifdef CONFIG_MTK_GPU_SUPPORT
#include <mt-plat/mtk_gpu_utility.h>
#endif
/* fosmod_fireos_crash_reporting end */

#include "internal.h"

#define CREATE_TRACE_POINTS
#include "trace/lowmemorykiller.h"

static DEFINE_SPINLOCK(lowmem_shrink_lock);
static short lowmem_warn_adj, lowmem_no_warn_adj = 200;
static u32 lowmem_debug_level = 1;
static short lowmem_adj[6] = {
	0,
	1,
	6,
	12,
};

static int lowmem_adj_size = 4;
static int lowmem_minfree[6] = {
	3 * 512,	/* 6MB */
	2 * 1024,	/* 8MB */
	4 * 1024,	/* 16MB */
	16 * 1024,	/* 64MB */
};

static int lowmem_minfree_size = 4;

static unsigned long lowmem_deathpending_timeout;

/* fosmod_fireos_crash_reporting begin */
/* Declarations */
size_t ion_mm_heap_total_memory(void);
void ion_mm_heap_memory_detail(void);
void ion_mm_heap_memory_detail_lmk(void);
#ifdef CONFIG_DEBUG_FS
void kbasep_gpu_memory_seq_show_lmk(void);
#endif

/* Constants */
static int BUFFER_SIZE = 16*1024;
static int ELEMENT_SIZE = 256;

/* Variables */
static char *lmk_log_buffer;
static char *buffer_end;
static char *head;
static char *kill_msg_index;
static char *previous_crash;
static int buffer_remaining;
static int foreground_kill;

void lmk_add_to_buffer(const char *fmt, ...)
{
	if (lmk_log_buffer) {
		if (head >= buffer_end) {
			/* Don't add more logs buffer is full */
			return;
		}
		if (buffer_remaining > 0) {
			va_list args;
			int added_size = 0;

			va_start(args, fmt);
			/* If the end of the buffer is reached and the added
			 * value is truncated then vsnprintf will return the
			 * original length of the value instead of the
			 * truncated length - this is intended by design. */
			added_size = vsnprintf(head, buffer_remaining, fmt, args);
			va_end(args);
			if (added_size > 0) {
				/* Add 1 for null terminator */
				added_size = added_size + 1;
				buffer_remaining = buffer_remaining - added_size;
				head = head + added_size;
			}
		}
	}
}

#define lowmem_print(level, x...)			\
	do {						\
		if (lowmem_debug_level >= (level))	\
			pr_warn(x);			\
		if (foreground_kill)			\
			lmk_add_to_buffer(x);		\
	} while (0)
/* In lowmem_print macro only added the lines 'if (foreground_kill)' and 'lmk_add_to_buffer(x)' */
/* fosmod_fireos_crash_reporting end */

static unsigned long lowmem_count(struct shrinker *s,
				  struct shrink_control *sc)
{
#ifdef CONFIG_FREEZER
	/* Don't bother LMK when system is freezing */
	if (pm_freezing)
		return 0;
#endif
	return global_node_page_state(NR_ACTIVE_ANON) +
		global_node_page_state(NR_ACTIVE_FILE) +
		global_node_page_state(NR_INACTIVE_ANON) +
		global_node_page_state(NR_INACTIVE_FILE);
}

/* Check memory status by zone, pgdat */
static int lowmem_check_status_by_zone(enum zone_type high_zoneidx,
				       int *other_free, int *other_file)
{
	struct pglist_data *pgdat;
	struct zone *z;
	enum zone_type zoneidx;
	unsigned long accumulated_pages = 0;
	u64 scale = (u64)totalram_pages;
	int new_other_free = 0, new_other_file = 0;
	int memory_pressure = 0;
	int unreclaimable = 0;

	if (high_zoneidx < MAX_NR_ZONES - 1) {
		/* Go through all memory nodes */
		for_each_online_pgdat(pgdat) {
			for (zoneidx = 0; zoneidx <= high_zoneidx; zoneidx++) {
				z = pgdat->node_zones + zoneidx;
				accumulated_pages += z->managed_pages;
				new_other_free +=
					zone_page_state(z, NR_FREE_PAGES);
				new_other_free -= high_wmark_pages(z);
				new_other_file +=
				zone_page_state(z, NR_ZONE_ACTIVE_FILE) +
				zone_page_state(z, NR_ZONE_INACTIVE_FILE);

				/* Compute memory pressure level */
				memory_pressure +=
				zone_page_state(z, NR_ZONE_ACTIVE_FILE) +
				zone_page_state(z, NR_ZONE_INACTIVE_FILE) +
#ifdef CONFIG_SWAP
				zone_page_state(z, NR_ZONE_ACTIVE_ANON) +
				zone_page_state(z, NR_ZONE_INACTIVE_ANON) +
#endif
				new_other_free;
			}

			/*
			 * Consider pgdat as unreclaimable when hitting one of
			 * following two cases,
			 * 1. Memory node is unreclaimable in vmscan.c
			 * 2. Memory node is reclaimable, but nearly no user
			 *    pages(under high wmark)
			 */
			if (!pgdat_reclaimable(pgdat) ||
			    (pgdat_reclaimable(pgdat) && memory_pressure < 0))
				unreclaimable++;
		}

		/*
		 * Update if we go through ONLY lower zone(s) ACTUALLY
		 * and scale in totalram_pages
		 */
		if (totalram_pages > accumulated_pages) {
			do_div(scale, accumulated_pages);
			if ((u64)totalram_pages >
			    (u64)accumulated_pages * scale)
				scale += 1;
			new_other_free *= scale;
			new_other_file *= scale;
		}

		/*
		 * Update if not kswapd or
		 * "being kswapd and high memory pressure"
		 */
		if (!current_is_kswapd() ||
		    (current_is_kswapd() && memory_pressure < 0)) {
			*other_free = new_other_free;
			*other_file = new_other_file;
		}
	}

	return unreclaimable;
}

/* Aggressive Memory Reclaim(AMR) */
static short lowmem_amr_check(int *to_be_aggressive, int other_file)
{
#ifdef CONFIG_SWAP
#ifdef CONFIG_64BIT
#define ENABLE_AMR_RAMSIZE	(0x60000)	/* > 1.5GB */
#else
#define ENABLE_AMR_RAMSIZE	(0x40000)	/* > 1GB */
#endif

	unsigned long swap_pages = 0;
	short amr_adj = OOM_SCORE_ADJ_MAX + 1;
#ifndef CONFIG_MTK_GMO_RAM_OPTIMIZE
	int i;
#endif
	swap_pages = atomic_long_read(&nr_swap_pages);
	/* More than 1/2 swap usage */
	if (swap_pages * 2 < total_swap_pages)
		(*to_be_aggressive)++;
	/* More than 3/4 swap usage */
	if (swap_pages * 4 < total_swap_pages)
		(*to_be_aggressive)++;

#ifndef CONFIG_MTK_GMO_RAM_OPTIMIZE
	/* Try to enable AMR when we have enough memory */
	if (totalram_pages < ENABLE_AMR_RAMSIZE) {
		*to_be_aggressive = 0;
	} else {
		i = lowmem_adj_size - 1;
		/*
		 * Comparing other_file with lowmem_minfree to make
		 * amr less aggressive.
		 * ex.
		 * For lowmem_adj[] = {0, 100, 200, 300, 900, 906},
		 * if swap usage > 50%,
		 * try to kill 906       when other_file >= lowmem_minfree[5]
		 * try to kill 300 ~ 906 when other_file  < lowmem_minfree[5]
		 */
		if (*to_be_aggressive > 0) {
			if (other_file < lowmem_minfree[i])
				i -= *to_be_aggressive;
			if (likely(i >= 0))
				amr_adj = lowmem_adj[i];
		}
	}
#endif

	return amr_adj;
#undef ENABLE_AMR_RAMSIZE

#else	/* !CONFIG_SWAP */
	*to_be_aggressive = 0;
	return OOM_SCORE_ADJ_MAX + 1;
#endif
}

static void __lowmem_trigger_warning(struct task_struct *selected)
{
#if defined(CONFIG_MTK_AEE_FEATURE) && defined(CONFIG_MTK_ENG_BUILD)
#define MSG_SIZE_TO_AEE 70
	char msg_to_aee[MSG_SIZE_TO_AEE];

	lowmem_print(1, "low memory trigger kernel warning\n");
	snprintf(msg_to_aee, MSG_SIZE_TO_AEE,
		 "please contact AP/AF memory module owner[pid:%d]\n",
		 selected->pid);

	aee_kernel_warning_api("LMK", 0, DB_OPT_DEFAULT |
			       DB_OPT_DUMPSYS_ACTIVITY |
			       DB_OPT_LOW_MEMORY_KILLER |
			       DB_OPT_PID_MEMORY_INFO | /* smaps and hprof*/
			       DB_OPT_PROCESS_COREDUMP |
			       DB_OPT_DUMPSYS_SURFACEFLINGER |
			       DB_OPT_DUMPSYS_GFXINFO |
			       DB_OPT_DUMPSYS_PROCSTATS,
			       "Framework low memory\nCRDISPATCH_KEY:FLM_APAF",
			       msg_to_aee);
#undef MSG_SIZE_TO_AEE
#else
	pr_info("(%s) no warning triggered for selected(%s)(%d)\n",
		__func__, selected->comm, selected->pid);
#endif
}

/* try to trigger warning to get more information */
static void lowmem_trigger_warning(struct task_struct *selected,
				   short selected_oom_score_adj)
{
	static DEFINE_RATELIMIT_STATE(ratelimit, 60 * HZ, 1);

	if (selected_oom_score_adj > lowmem_warn_adj)
		return;

	if (!__ratelimit(&ratelimit))
		return;

	__lowmem_trigger_warning(selected);
}

/* try to dump more memory status */
static void dump_memory_status(short selected_oom_score_adj)
{
	static DEFINE_RATELIMIT_STATE(ratelimit, 5 * HZ, 1);
	static DEFINE_RATELIMIT_STATE(ratelimit_urgent, 2 * HZ, 1);

	if (selected_oom_score_adj > lowmem_warn_adj &&
	    !__ratelimit(&ratelimit))
		return;

	if (!__ratelimit(&ratelimit_urgent))
		return;

	show_task_mem();
	show_free_areas(0);
	oom_dump_extra_info();
}

static unsigned long lowmem_scan(struct shrinker *s, struct shrink_control *sc)
{
	struct task_struct *tsk;
	struct task_struct *selected = NULL;
	unsigned long rem = 0;
	int tasksize;
	int i;
	short min_score_adj = OOM_SCORE_ADJ_MAX + 1;
	int minfree = 0;
	int selected_tasksize = 0;
	short selected_oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);
	int other_free = global_page_state(NR_FREE_PAGES) - totalreserve_pages;
	int other_file = global_node_page_state(NR_FILE_PAGES) -
				global_node_page_state(NR_SHMEM) -
				global_node_page_state(NR_UNEVICTABLE) -
				total_swapcache_pages();
	enum zone_type high_zoneidx = gfp_zone(sc->gfp_mask);
	int d_state_is_found = 0;
	short other_min_score_adj = OOM_SCORE_ADJ_MAX + 1;
	int to_be_aggressive = 0;

	if (!spin_trylock(&lowmem_shrink_lock)) {
		lowmem_print(4, "lowmem_shrink lock failed\n");
		return SHRINK_STOP;
	}

	/*
	 * Check whether it is caused by low memory in lower zone(s)!
	 * This will help solve over-reclaiming situation while total number
	 * of free pages is enough, but lower one(s) is(are) under low memory.
	 */
	if (lowmem_check_status_by_zone(high_zoneidx, &other_free, &other_file)
			> 0)
		other_min_score_adj = 0;

	other_min_score_adj =
		min(other_min_score_adj,
		    lowmem_amr_check(&to_be_aggressive, other_file));

	/* Let other_free be positive or zero */
	if (other_free < 0)
		other_free = 0;

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;
	for (i = 0; i < array_size; i++) {
		minfree = lowmem_minfree[i];
		if (other_free < minfree && other_file < minfree) {
			if (to_be_aggressive != 0 && i > 3) {
				i -= to_be_aggressive;
				if (i < 3)
					i = 3;
			}
			min_score_adj = lowmem_adj[i];
			break;
		}
	}

	/* Compute suitable min_score_adj */
	min_score_adj = min(min_score_adj, other_min_score_adj);

	lowmem_print(3, "lowmem_scan %lu, %x, ofree %d %d, ma %hd\n",
		     sc->nr_to_scan, sc->gfp_mask, other_free,
		     other_file, min_score_adj);

	if (min_score_adj == OOM_SCORE_ADJ_MAX + 1) {
		lowmem_print(5, "lowmem_scan %lu, %x, return 0\n",
			     sc->nr_to_scan, sc->gfp_mask);
		spin_unlock(&lowmem_shrink_lock);
		return 0;
	}

	selected_oom_score_adj = min_score_adj;

	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		short oom_score_adj;

		if (tsk->flags & PF_KTHREAD)
			continue;

		if (task_lmk_waiting(tsk) &&
		    time_before_eq(jiffies, lowmem_deathpending_timeout)) {
			rcu_read_unlock();
			spin_unlock(&lowmem_shrink_lock);
			return 0;
		}

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		/* Bypass D-state process */
		if (p->state & TASK_UNINTERRUPTIBLE) {
			lowmem_print(2,
				     "lowmem_scan filter D state process: %d (%s) state:0x%lx\n",
				     p->pid, p->comm, p->state);
			task_unlock(p);
			d_state_is_found = 1;
			continue;
		}

		oom_score_adj = p->signal->oom_score_adj;
		if (oom_score_adj < min_score_adj) {
			task_unlock(p);
			continue;
		}

		tasksize = get_mm_rss(p->mm) +
			get_mm_counter(p->mm, MM_SWAPENTS);
		task_unlock(p);
		if (tasksize <= 0)
			continue;
		if (selected) {
			if (oom_score_adj < selected_oom_score_adj)
				continue;
			if (oom_score_adj == selected_oom_score_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_score_adj = oom_score_adj;
		lowmem_print(2, "select '%s' (%d), adj %hd, size %d, to kill\n",
			     p->comm, p->pid, oom_score_adj, tasksize);
	}

/* fosmod_fireos_crash_reporting begin */
#ifndef CONFIG_MT_ENG_BUILD
	if (lmk_log_buffer && selected && selected_oom_score_adj == 0) {
		foreground_kill = 1;
		head = lmk_log_buffer;
		buffer_remaining = BUFFER_SIZE;
		if (kill_msg_index && previous_crash)
			strncpy(previous_crash, kill_msg_index, ELEMENT_SIZE);
		lowmem_print(1, "======low memory killer=====\n");
		lowmem_print(1, "Free memory other_free: %d, other_file:%d pages\n", other_free, other_file);
		if (gfp_zone(sc->gfp_mask) == ZONE_NORMAL)
			lowmem_print(1, "ZONE_NORMAL\n");
		else
			lowmem_print(1, "ZONE_HIGHMEM\n");

		rcu_read_lock();
		for_each_process(tsk) {
			struct task_struct *p2;
			short oom_score_adj2;

			if (tsk->flags & PF_KTHREAD)
				continue;

			p2 = find_lock_task_mm(tsk);
			if (!p2)
				continue;

			oom_score_adj2 = p2->signal->oom_score_adj;
#ifdef CONFIG_ZRAM
			lowmem_print(1, "Candidate %d (%s), score_adj %d, rss %lu, rswap %lu, to kill\n",
				p2->pid, p2->comm, oom_score_adj2, get_mm_rss(p2->mm),
				get_mm_counter(p2->mm, MM_SWAPENTS));
#else /* CONFIG_ZRAM */
			lowmem_print(1, "Candidate %d (%s), score_adj %d, rss %lu, to kill\n",
				p2->pid, p2->comm, oom_score_adj2, get_mm_rss(p2->mm));
#endif /* CONFIG_ZRAM */
			task_unlock(p2);
		}
		rcu_read_unlock();
#ifdef CONFIG_MTK_GPU_SUPPORT
#ifdef CONFIG_DEBUG_FS
		kbasep_gpu_memory_seq_show_lmk();
#endif
#endif /* CONFIG_MTK_GPU_SUPPORT */
		kill_msg_index = head;
	}
#endif /* CONFIG_MT_ENG_BUILD */
/* fosmod_fireos_crash_reporting end */

	if (selected) {
		long cache_size = other_file * (long)(PAGE_SIZE / 1024);
		long cache_limit = minfree * (long)(PAGE_SIZE / 1024);
		long free = other_free * (long)(PAGE_SIZE / 1024);

		task_lock(selected);
		send_sig(SIGKILL, selected, 0);
		if (selected->mm)
			task_set_lmk_waiting(selected);
		task_unlock(selected);
		trace_lowmemory_kill(selected, cache_size, cache_limit, free);
		lowmem_print(1, "Killing '%s' (%d) (tgid %d), adj %hd,\n"
				 "   to free %ldkB on behalf of '%s' (%d) because\n"
				 "   cache %ldkB is below limit %ldkB for oom_score_adj %hd (%hd)\n"
				 "   Free memory is %ldkB above reserved(decrease %d level)\n",
			     selected->comm, selected->pid, selected->tgid,
			     selected_oom_score_adj,
			     selected_tasksize * (long)(PAGE_SIZE / 1024),
			     current->comm, current->pid,
			     cache_size, cache_limit,
			     min_score_adj, other_min_score_adj,
			     free, to_be_aggressive);
		lowmem_deathpending_timeout = jiffies + HZ;
		lowmem_trigger_warning(selected, selected_oom_score_adj);

		rem += selected_tasksize;

/* fosmod_fireos_crash_reporting begin */
		if (foreground_kill) {
				show_free_areas(0);
			#ifdef CONFIG_MTK_ION
				/* Show ION status */
				/* fosmod_fireos_crash_reporting begin */
				lowmem_print(1, "ion_mm_heap_total_memory[%ld]\n", (unsigned long)ion_mm_heap_total_memory());
				ion_mm_heap_memory_detail();
				if (foreground_kill)
					ion_mm_heap_memory_detail_lmk();
				/* fosmod_fireos_crash_reporting end */
			#endif
			#ifdef CONFIG_MTK_GPU_SUPPORT
				if (mtk_dump_gpu_memory_usage() == false)
					lowmem_print(1, "mtk_dump_gpu_memory_usage not support\n");
			#endif
		}
/* fosmod_fireos_crash_reporting end */
	} else {
		if (d_state_is_found == 1)
			lowmem_print(2,
				     "No selected (full of D-state processes at %d)\n",
				     (int)min_score_adj);
	}

	lowmem_print(4, "lowmem_scan %lu, %x, return %lu\n",
		     sc->nr_to_scan, sc->gfp_mask, rem);
	rcu_read_unlock();
	spin_unlock(&lowmem_shrink_lock);

	/* dump more memory info outside the lock */
	if (selected && selected_oom_score_adj <= lowmem_no_warn_adj &&
	    min_score_adj <= lowmem_warn_adj)
		dump_memory_status(selected_oom_score_adj);

	foreground_kill = 0; /* fosmod_fireos_crash_reporting oneline */

	return rem;
}

/* fosmod_fireos_crash_reporting begin */
static int lowmem_proc_show(struct seq_file *m, void *v)
{
	char *ptr;

	if (!lmk_log_buffer) {
		seq_printf(m, "lmk_logs are not functioning - something went wrong during init");
		return 0;
	}
	ptr = lmk_log_buffer;
	while (ptr < head) {
		int cur_line_len = strlen(ptr);
		seq_printf(m, ptr, "\n");
		if (cur_line_len <= 0)
			break;
		/* add 1 to skip the null terminator for C Strings */
		ptr = ptr + cur_line_len + 1;
	}
	if (previous_crash && previous_crash[0] != '\0') {
		seq_printf(m, "previous crash:\n");
		seq_printf(m, previous_crash, "\n");
	}
	return 0;
}

static int lowmem_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, lowmem_proc_show, NULL);
}

static const struct file_operations lowmem_proc_fops = {
	.open       = lowmem_proc_open,
	.read       = seq_read,
	.release    = single_release
};
/* fosmod_fireos_crash_reporting end */

static struct shrinker lowmem_shrinker = {
	.scan_objects = lowmem_scan,
	.count_objects = lowmem_count,
	.seeks = DEFAULT_SEEKS * 16
};

static int __init lowmem_init(void)
{
	if (IS_ENABLED(CONFIG_ZRAM) &&
	    IS_ENABLED(CONFIG_MTK_GMO_RAM_OPTIMIZE))
		vm_swappiness = 100;

	register_shrinker(&lowmem_shrinker);

    /* fosmod_fireos_crash_reporting begin */
	proc_create("lmk_logs", 0, NULL, &lowmem_proc_fops);
	lmk_log_buffer = kzalloc(BUFFER_SIZE, GFP_KERNEL);
	if (lmk_log_buffer) {
		buffer_end = lmk_log_buffer + BUFFER_SIZE;
		head = lmk_log_buffer;
		buffer_remaining = BUFFER_SIZE;
		foreground_kill = 0;
		kill_msg_index = NULL;
		previous_crash = kzalloc(ELEMENT_SIZE, GFP_KERNEL);
		if (!previous_crash)
			printk(KERN_ALERT "unable to allocate previous_crash for /proc/lmk_logs - previous_crash will not be logged");
	} else {
		printk(KERN_ALERT "unable to allocate buffer for /proc/lmk_logs - feature will be disabled");
	}
    /* fosmod_fireos_crash_reporting end */

	return 0;
}

static void __exit lowmem_exit(void)
{
    /* fosmod_fireos_crash_reporting begin */
	kfree(lmk_log_buffer);
	kfree(previous_crash);
    /* fosmod_fireos_crash_reporting end */
	unregister_shrinker(&lowmem_shrinker);
}

device_initcall(lowmem_init);
module_exit(lowmem_exit);

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
static short lowmem_oom_adj_to_oom_score_adj(short oom_adj)
{
	if (oom_adj == OOM_ADJUST_MAX)
		return OOM_SCORE_ADJ_MAX;
	else
		return (oom_adj * OOM_SCORE_ADJ_MAX) / -OOM_DISABLE;
}

static void lowmem_autodetect_oom_adj_values(void)
{
	int i;
	short oom_adj;
	short oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;

	if (array_size <= 0)
		return;

	oom_adj = lowmem_adj[array_size - 1];
	if (oom_adj > OOM_ADJUST_MAX)
		return;

	oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
	if (oom_score_adj <= OOM_ADJUST_MAX)
		return;

	lowmem_print(1, "lowmem_shrink: convert oom_adj to oom_score_adj:\n");
	for (i = 0; i < array_size; i++) {
		oom_adj = lowmem_adj[i];
		oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
		lowmem_adj[i] = oom_score_adj;
		lowmem_print(1, "oom_adj %d => oom_score_adj %d\n",
			     oom_adj, oom_score_adj);
	}
}

static int lowmem_adj_array_set(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = param_array_ops.set(val, kp);

	/* HACK: Autodetect oom_adj values in lowmem_adj array */
	lowmem_autodetect_oom_adj_values();

	return ret;
}

static int lowmem_adj_array_get(char *buffer, const struct kernel_param *kp)
{
	return param_array_ops.get(buffer, kp);
}

static void lowmem_adj_array_free(void *arg)
{
	param_array_ops.free(arg);
}

static struct kernel_param_ops lowmem_adj_array_ops = {
	.set = lowmem_adj_array_set,
	.get = lowmem_adj_array_get,
	.free = lowmem_adj_array_free,
};

static const struct kparam_array __param_arr_adj = {
	.max = ARRAY_SIZE(lowmem_adj),
	.num = &lowmem_adj_size,
	.ops = &param_ops_short,
	.elemsize = sizeof(lowmem_adj[0]),
	.elem = lowmem_adj,
};
#endif

/*
 * not really modular, but the easiest way to keep compat with existing
 * bootargs behaviour is to continue using module_param here.
 */
module_param_named(cost, lowmem_shrinker.seeks, int, 0644);
#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
module_param_cb(adj, &lowmem_adj_array_ops,
		.arr = &__param_arr_adj,
		0644);
__MODULE_PARM_TYPE(adj, "array of short");
#else
module_param_array_named(adj, lowmem_adj, short, &lowmem_adj_size, 0644);
#endif
module_param_array_named(minfree, lowmem_minfree, uint, &lowmem_minfree_size,
			 0644);
module_param_named(debug_level, lowmem_debug_level, uint, 0644);
module_param_named(debug_adj, lowmem_warn_adj, short, 0644);
module_param_named(no_debug_adj, lowmem_no_warn_adj, short, 0644);
