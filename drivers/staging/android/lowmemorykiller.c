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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/swap.h>
#include <linux/rcupdate.h>
#include <linux/notifier.h>
/* fosmod_fireos_crash_reporting begin */
#include<linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
/* fosmod_fireos_crash_reporting end */

#ifdef CONFIG_HIGHMEM
#include <linux/highmem.h>
#endif

#ifdef CONFIG_ION_MTK
#include <linux/ion_drv.h>
#endif

#ifdef CONFIG_MTK_GPU_SUPPORT
#include <mt-plat/mtk_gpu_utility.h>
#endif

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
#define CONVERT_ADJ(x) ((x * OOM_SCORE_ADJ_MAX) / -OOM_DISABLE)
#define REVERT_ADJ(x)  (x * (-OOM_DISABLE + 1) / OOM_SCORE_ADJ_MAX)
#else
#define CONVERT_ADJ(x) (x)
#define REVERT_ADJ(x) (x)
#endif /* CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES */

static short lowmem_debug_adj = CONVERT_ADJ(0);
#ifdef CONFIG_MT_ENG_BUILD
static short lowmem_kernel_warn_adj = CONVERT_ADJ(0);
#define output_expect(x) likely(x)
static uint32_t enable_candidate_log = 1;
#else
#define output_expect(x) unlikely(x)
static uint32_t enable_candidate_log;
#endif
static DEFINE_SPINLOCK(lowmem_shrink_lock);
static uint32_t lowmem_debug_level = 1;
static short lowmem_adj[9] = {
	0,
	1,
	6,
	12,
};
static int lowmem_adj_size = 9;
int lowmem_minfree[9] = {
	3 * 512,	/* 6MB */
	2 * 1024,	/* 8MB */
	4 * 1024,	/* 16MB */
	16 * 1024,	/* 64MB */
};
static int lowmem_minfree_size = 9;

#ifdef CONFIG_HIGHMEM
static int total_low_ratio = 1;
#endif

static struct task_struct *lowmem_deathpending;
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

static int
task_notify_func(struct notifier_block *self, unsigned long val, void *data);

static struct notifier_block task_nb = {
	.notifier_call	= task_notify_func,
};

static int
task_notify_func(struct notifier_block *self, unsigned long val, void *data)
{
	struct task_struct *task = data;

	if (task == lowmem_deathpending)
		lowmem_deathpending = NULL;

	return NOTIFY_DONE;
}

static unsigned long lowmem_count(struct shrinker *s,
				  struct shrink_control *sc)
{
	return global_page_state(NR_ACTIVE_ANON) +
		global_page_state(NR_ACTIVE_FILE) +
		global_page_state(NR_INACTIVE_ANON) +
		global_page_state(NR_INACTIVE_FILE);
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
	int other_file = global_page_state(NR_FILE_PAGES) -
						global_page_state(NR_SHMEM) -
						total_swapcache_pages();

	int print_extra_info = 0;
	static unsigned long lowmem_print_extra_info_timeout;

#ifdef CONFIG_MTK_GMO_RAM_OPTIMIZE
	int other_anon = global_page_state(NR_INACTIVE_ANON) - global_page_state(NR_ACTIVE_ANON);
#endif
#ifdef CONFIG_MT_ENG_BUILD
	/* dump memory info when framework low memory*/
	int pid_dump = -1; /* process which need to be dump */
	/* int pid_sec_mem = -1; */
	int max_mem = 0;
	static int pid_flm_warn = -1;
	static unsigned long flm_warn_timeout;
#endif /* CONFIG_MT_ENG_BUILD*/
	/*
	* If we already have a death outstanding, then
	* bail out right away; indicating to vmscan
	* that we have nothing further to offer on
	* this pass.
	*
	*/
	if (lowmem_deathpending &&
	    time_before_eq(jiffies, lowmem_deathpending_timeout))
		return SHRINK_STOP;

#if 0
	/*mask for build error, need unmask later*/
	/* We are in MTKPASR stage! */
	if (unlikely(current->flags & PF_MTKPASR))
		return SHRINK_STOP;
#endif

	if (!spin_trylock(&lowmem_shrink_lock)) {
		lowmem_print(4, "lowmem_shrink lock failed\n");
		return SHRINK_STOP;
	}

#ifdef CONFIG_ZRAM
	other_file -= total_swapcache_pages();
#endif

#ifdef CONFIG_HIGHMEM
	/*
	* Check whether it is caused by low memory in normal zone!
	* This will help solve over-reclaiming situation while
	* total free pages is enough, but normal zone is under low memory.
	*/
	if (gfp_zone(sc->gfp_mask) == ZONE_NORMAL) {
		int nid;
		struct zone *z;

		/* Restore other_free */
		other_free += totalreserve_pages;

		/* Go through all memory nodes & substract (free, file) from ZONE_HIGHMEM */
		for_each_online_node(nid) {
			z = &NODE_DATA(nid)->node_zones[ZONE_HIGHMEM];
			other_free -= zone_page_state(z, NR_FREE_PAGES);
			other_file -= zone_page_state(z, NR_FILE_PAGES);
			/* Don't substract NR_SHMEM twice! */
			other_file += zone_page_state(z, NR_SHMEM);
			/* Subtract high watermark of normal zone */
			z = &NODE_DATA(nid)->node_zones[ZONE_NORMAL];
			other_free -= high_wmark_pages(z);
		}

		/* Normalize */
		other_free *= total_low_ratio;
		other_file *= total_low_ratio;
	}
#endif
	/* Let it be positive or zero */
	if (other_free < 0) {
		/* lowmem_print(1, "Original other_free [%d] is too low!\n", other_free); */
		other_free = 0;
	}

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;
	for (i = 0; i < array_size; i++) {
		minfree = lowmem_minfree[i];
		if (other_free < minfree && other_file < minfree) {
			min_score_adj = lowmem_adj[i];
			break;
		}
	}
#ifdef CONFIG_MTK_GMO_RAM_OPTIMIZE /* Need removal */
	if (min_score_adj < 9 && other_anon > 70 * 256) {
		/* if other_anon > 70MB, don't kill adj <= 8 */
		min_score_adj = 9;
	}
#endif

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

	/* add debug log */
	if (output_expect(enable_candidate_log)) {
		if (min_score_adj <= lowmem_debug_adj) {
			if (time_after_eq(jiffies, lowmem_print_extra_info_timeout)) {
				lowmem_print_extra_info_timeout = jiffies + HZ;
				print_extra_info = 1;
			}
		}

		if (print_extra_info)
			lowmem_print(1, "Free memory other_free: %d, other_file:%d pages\n", other_free, other_file);
	}

	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		short oom_score_adj;

		if (tsk->flags & PF_KTHREAD)
			continue;

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		if (test_tsk_thread_flag(p, TIF_MEMDIE) &&
		    time_before_eq(jiffies, lowmem_deathpending_timeout)) {
#ifdef CONFIG_MT_ENG_BUILD
			static pid_t last_dying_pid;

			if (last_dying_pid != p->pid) {
				lowmem_print(1, "lowmem_shrink return directly, due to  %d (%s) is dying\n",
					p->pid, p->comm);
				last_dying_pid = p->pid;
			}
#endif
			task_unlock(p);
			rcu_read_unlock();
			spin_unlock(&lowmem_shrink_lock);
			return SHRINK_STOP;
		}
		oom_score_adj = p->signal->oom_score_adj;

#ifdef CONFIG_MT_ENG_BUILD
		tasksize = get_mm_rss(p->mm);
#ifdef CONFIG_ZRAM
		tasksize += get_mm_counter(p->mm, MM_SWAPENTS);
#endif
		/*
		* dump memory info when framework low memory:
		* record the first two pid which consumed most memory.
		*/
		if (tasksize > max_mem) {
			max_mem = tasksize;
			/* pid_sec_mem = pid_dump; */
			pid_dump = p->pid;

			if (output_expect(enable_candidate_log)) {
				if (print_extra_info) {
#ifdef CONFIG_ZRAM
					pr_info("Candidate %d (%s), adj %d, score_adj %d, rss %lu, rswap %lu, to kill\n",
								p->pid, p->comm,
								REVERT_ADJ(oom_score_adj), oom_score_adj,
								get_mm_rss(p->mm),
								get_mm_counter(p->mm, MM_SWAPENTS));
#else /* CONFIG_ZRAM */
					pr_info("Candidate %d (%s), adj %d, score_adj %d, rss %lu, to kill\n",
								p->pid, p->comm,
								REVERT_ADJ(oom_score_adj), oom_score_adj,
								get_mm_rss(p->mm));
#endif
				}
			}
		}

		if (p->pid == pid_flm_warn &&
			time_before_eq(jiffies, flm_warn_timeout)) {
			task_unlock(p);
			continue;
		}
#else
		if (output_expect(enable_candidate_log)) {
			if (print_extra_info) {
#ifdef CONFIG_ZRAM
				pr_info("Candidate %d (%s), adj %d, score_adj %d, rss %lu, rswap %lu, to kill\n",
								p->pid, p->comm,
								REVERT_ADJ(oom_score_adj), oom_score_adj,
								get_mm_rss(p->mm),
								get_mm_counter(p->mm, MM_SWAPENTS));
#else /* CONFIG_ZRAM */
				pr_info("Candidate %d (%s), adj %d, score_adj %d, rss %lu, to kill\n",
								p->pid, p->comm,
								REVERT_ADJ(oom_score_adj), oom_score_adj,
								get_mm_rss(p->mm));
#endif
			}
		}
#endif

		if (oom_score_adj < min_score_adj) {
			task_unlock(p);
			continue;
		}

#ifndef CONFIG_MT_ENG_BUILD
		tasksize = get_mm_rss(p->mm);
#ifdef CONFIG_ZRAM
		tasksize += get_mm_counter(p->mm, MM_SWAPENTS);
#endif
#endif
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
#ifdef CONFIG_MTK_GMO_RAM_OPTIMIZE
		/*
		* if cached > 30MB, don't kill ub:secureRandom while its adj is 9
		*/
		if (!strcmp(p->comm, "ub:secureRandom") &&
			(REVERT_ADJ(oom_score_adj) == 9) && (other_file > 30*256)) {
			pr_info("select but ignore '%s' (%d), oom_score_adj %d, oom_adj %d, size %d, to kill, cache %ldkB is below limit %ldkB",
							p->comm, p->pid,
							oom_score_adj, REVERT_ADJ(oom_score_adj),
							tasksize,
							other_file * (long)(PAGE_SIZE / 1024),
							minfree * (long)(PAGE_SIZE / 1024));
		    continue;
		}
#endif
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_score_adj = oom_score_adj;
		lowmem_print(2, "select '%s' (%d), adj %d, score_adj %hd, size %d, to kill\n",
			     p->comm, p->pid, REVERT_ADJ(oom_score_adj), oom_score_adj, tasksize);
	}

/* fosmod_fireos_crash_reporting begin */
#ifndef CONFIG_MT_ENG_BUILD
	if (lmk_log_buffer && selected && selected_oom_score_adj == 0 && !print_extra_info) {
		print_extra_info = 1;
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
			lowmem_print(1, "Candidate %d (%s), adj %d, score_adj %d, rss %lu, rswap %lu, to kill\n",
				p2->pid, p2->comm, REVERT_ADJ(oom_score_adj2), oom_score_adj2, get_mm_rss(p2->mm),
				get_mm_counter(p2->mm, MM_SWAPENTS));
#else /* CONFIG_ZRAM */
			lowmem_print(1, "Candidate %d (%s), adj %d, score_adj %d, rss %lu, to kill\n",
				p2->pid, p2->comm, REVERT_ADJ(oom_score_adj2), oom_score_adj2, get_mm_rss(p2->mm));
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
		lowmem_print(1, "Killing '%s' (%d), adj %d, score_adj %hd,\n"
				"   to free %ldkB on behalf of '%s' (%d) because\n"
				"   cache %ldkB is below limit %ldkB for oom_score_adj %hd\n"
				"   Free memory is %ldkB above reserved\n",
			     selected->comm, selected->pid,
				 REVERT_ADJ(selected_oom_score_adj),
			     selected_oom_score_adj,
			     selected_tasksize * (long)(PAGE_SIZE / 1024),
			     current->comm, current->pid,
			     other_file * (long)(PAGE_SIZE / 1024),
			     minfree * (long)(PAGE_SIZE / 1024),
			     min_score_adj,
			     other_free * (long)(PAGE_SIZE / 1024));
		lowmem_deathpending = selected;
		lowmem_deathpending_timeout = jiffies + HZ;
		set_tsk_thread_flag(selected, TIF_MEMDIE);

/* fosmod_fireos_crash_reporting begin */
		if ((output_expect(enable_candidate_log) && print_extra_info)
				|| foreground_kill) {
/* fosmod_fireos_crash_reporting end */
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

		send_sig(SIGKILL, selected, 0);
		rem += selected_tasksize;
	}

	lowmem_print(4, "lowmem_scan %lu, %x, return %lu\n",
		     sc->nr_to_scan, sc->gfp_mask, rem);
	rcu_read_unlock();
	spin_unlock(&lowmem_shrink_lock);

	foreground_kill = 0; /* fosmod_fireos_crash_reporting oneline */

	return rem;
}

static struct shrinker lowmem_shrinker = {
	.scan_objects = lowmem_scan,
	.count_objects = lowmem_count,
	.seeks = DEFAULT_SEEKS * 16
};


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


static int __init lowmem_init(void)
{
#ifdef CONFIG_HIGHMEM
	unsigned long normal_pages;
#endif

#ifdef CONFIG_ZRAM
	vm_swappiness = 100;
#endif


	task_free_register(&task_nb);
	register_shrinker(&lowmem_shrinker);

#ifdef CONFIG_HIGHMEM
	normal_pages = totalram_pages - totalhigh_pages;
	total_low_ratio = (totalram_pages + normal_pages - 1) / normal_pages;
	pr_err("[LMK]total_low_ratio[%d] - totalram_pages[%lu] - totalhigh_pages[%lu]\n",
			total_low_ratio, totalram_pages, totalhigh_pages);
#endif

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
	task_free_unregister(&task_nb);
}

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
 * get_min_free_pages
 * returns the low memory killer watermark of the given pid,
 * When the system free memory is lower than the watermark, the LMK (low memory
 * killer) may try to kill processes.
 */
int get_min_free_pages(pid_t pid)
{
	struct task_struct *p;
	int target_oom_adj = 0;
	int i = 0;
	int array_size = ARRAY_SIZE(lowmem_adj);

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;

	for_each_process(p) {
		/* search pid */
		if (p->pid == pid) {
			task_lock(p);
			target_oom_adj = p->signal->oom_score_adj;
			task_unlock(p);
			/* get min_free value of the pid */
			for (i = array_size - 1; i >= 0; i--) {
				if (target_oom_adj >= lowmem_adj[i]) {
					pr_debug("pid: %d, target_oom_adj = %d, lowmem_adj[%d] = %d, lowmem_minfree[%d] = %d\n",
							pid, target_oom_adj, i, lowmem_adj[i], i,
							lowmem_minfree[i]);
					return lowmem_minfree[i];
				}
			}
			goto out;
		}
	}

out:
	lowmem_print(3, "[%s]pid: %d, adj: %d, lowmem_minfree = 0\n",
			__func__, pid, p->signal->oom_score_adj);
	return 0;
}
EXPORT_SYMBOL(get_min_free_pages);

/* Query LMK minfree settings */
/* To query default value, you can input index with value -1. */
size_t query_lmk_minfree(int index)
{
	int which;

	/* Invalid input index, return default value */
	if (index < 0)
		return lowmem_minfree[2];

	/* Find a corresponding output */
	which = 5;
	do {
		if (lowmem_adj[which] <= index)
			break;
	} while (--which >= 0);

	/* Fix underflow bug */
	which = (which < 0) ? 0 : which;

	return lowmem_minfree[which];
}
EXPORT_SYMBOL(query_lmk_minfree);

module_param_named(cost, lowmem_shrinker.seeks, int, S_IRUGO | S_IWUSR);
#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
module_param_cb(adj, &lowmem_adj_array_ops,
		.arr = &__param_arr_adj, S_IRUGO | S_IWUSR);
__MODULE_PARM_TYPE(adj, "array of short");
#else
module_param_array_named(adj, lowmem_adj, short, &lowmem_adj_size,
			 S_IRUGO | S_IWUSR);
#endif
module_param_array_named(minfree, lowmem_minfree, uint, &lowmem_minfree_size,
			 S_IRUGO | S_IWUSR);
module_param_named(debug_level, lowmem_debug_level, uint, S_IRUGO | S_IWUSR);

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
static int debug_adj_set(const char *val, const struct kernel_param *kp)
{
	const int ret = param_set_uint(val, kp);

	lowmem_debug_adj = lowmem_oom_adj_to_oom_score_adj(lowmem_debug_adj);
	return ret;
}

static struct kernel_param_ops debug_adj_ops = {
	.set = &debug_adj_set,
	.get = &param_get_uint,
};

module_param_cb(debug_adj, &debug_adj_ops, &lowmem_debug_adj, S_IRUGO | S_IWUSR);
__MODULE_PARM_TYPE(debug_adj, short);
#else
module_param_named(debug_adj, lowmem_debug_adj, short, S_IRUGO | S_IWUSR);
#endif
module_param_named(candidate_log, enable_candidate_log, uint, S_IRUGO | S_IWUSR);

late_initcall(lowmem_init);
module_exit(lowmem_exit);

MODULE_LICENSE("GPL");

