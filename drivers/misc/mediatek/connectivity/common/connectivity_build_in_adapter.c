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

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG "[CONNADP]"
#include "connectivity_build_in_adapter.h"

#include <kernel/sched/sched.h>

/*device tree mode*/
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/irqreturn.h>
#include <linux/of_address.h>
#endif

#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of_reserved_mem.h>

#include <linux/interrupt.h>

#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
#include <mtk_6306_gpio.h>
#endif

#ifdef CONNADP_HAS_CLOCK_BUF_CTRL
#include <mtk_clkbuf_ctl.h>
#endif

/* PMIC */
#include <upmu_common.h>

/* MMC */
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <sdio_ops.h>

#include "mtk_spm_resource_req.h"
#include <linux/mm.h>

#ifdef CONFIG_ARCH_MT6570
#define CPU_BOOST y
#endif
#ifdef CONFIG_ARCH_MT6755
#define CPU_BOOST y
#endif
#ifdef CONFIG_MACH_MT6757
#define CPU_BOOST y
#endif
#ifdef CONFIG_MACH_MT6763
#define CPU_BOOST y
#endif

#ifdef CPU_BOOST
#include "mtk_ppm_api.h"
#endif

#include <linux/fdtable.h>
#include <linux/fs.h>

phys_addr_t gConEmiPhyBase;
EXPORT_SYMBOL(gConEmiPhyBase);
unsigned long long gConEmiSize;
EXPORT_SYMBOL(gConEmiSize);

phys_addr_t gWifiRsvMemPhyBase;
EXPORT_SYMBOL(gWifiRsvMemPhyBase);
unsigned long long gWifiRsvMemSize;
EXPORT_SYMBOL(gWifiRsvMemSize);

/*Reserved memory by device tree!*/

int reserve_memory_consys_fn(struct reserved_mem *rmem)
{
	pr_info(DFT_TAG "[W]%s: name: %s,base: 0x%llx,size: 0x%llx\n",
		__func__, rmem->name, (unsigned long long)rmem->base,
		(unsigned long long)rmem->size);
	gConEmiPhyBase = rmem->base;
	gConEmiSize = rmem->size;
	return 0;
}

RESERVEDMEM_OF_DECLARE(reserve_memory_test, "mediatek,consys-reserve-memory",
			reserve_memory_consys_fn);

int reserve_memory_wifi_fn(struct reserved_mem *rmem)
{
	pr_info(DFT_TAG "[W]%s: name: %s,base: 0x%llx,size: 0x%llx\n",
		__func__, rmem->name, (unsigned long long)rmem->base,
		(unsigned long long)rmem->size);
	gWifiRsvMemPhyBase = rmem->base;
	gWifiRsvMemSize = rmem->size;
	return 0;
}
RESERVEDMEM_OF_DECLARE(reserve_memory_wifi, "mediatek,wifi-reserve-memory",
		       reserve_memory_wifi_fn);

void connectivity_export_show_stack(struct task_struct *tsk, unsigned long *sp)
{
	show_stack(tsk, sp);
}
EXPORT_SYMBOL(connectivity_export_show_stack);

void connectivity_export_tracing_record_cmdline(struct task_struct *tsk)
{
//	tracing_record_cmdline(tsk);cc_temp
}
EXPORT_SYMBOL(connectivity_export_tracing_record_cmdline);

#ifdef CPU_BOOST
bool connectivity_export_spm_resource_req(unsigned int user,
					  unsigned int req_mask)
{
	return spm_resource_req(user, req_mask);
}
EXPORT_SYMBOL(connectivity_export_spm_resource_req);

void connectivity_export_mt_ppm_sysboost_freq(enum ppm_sysboost_user user,
					      unsigned int freq)
{
	mt_ppm_sysboost_freq(user, freq);
}
EXPORT_SYMBOL(connectivity_export_mt_ppm_sysboost_freq);

void connectivity_export_mt_ppm_sysboost_core(enum ppm_sysboost_user user,
					      unsigned int core_num)
{
	mt_ppm_sysboost_core(user, core_num);
}
EXPORT_SYMBOL(connectivity_export_mt_ppm_sysboost_core);

void connectivity_export_mt_ppm_sysboost_set_core_limit(
				enum ppm_sysboost_user user,
				unsigned int cluster,
				int min_core, int max_core)
{
	mt_ppm_sysboost_set_core_limit(user, cluster, min_core, max_core);
}
EXPORT_SYMBOL(connectivity_export_mt_ppm_sysboost_set_core_limit);

void connectivity_export_mt_ppm_sysboost_set_freq_limit(
				enum ppm_sysboost_user user,
				unsigned int cluster,
				int min_freq, int max_freq)
{
	mt_ppm_sysboost_set_freq_limit(user, cluster, min_freq, max_freq);
}
EXPORT_SYMBOL(connectivity_export_mt_ppm_sysboost_set_freq_limit);
#endif

/*******************************************************************************
 * Clock Buffer Control
 ******************************************************************************/
#ifdef CONNADP_HAS_CLOCK_BUF_CTRL
void connectivity_export_clk_buf_ctrl(enum clk_buf_id id, bool onoff)
{
	clk_buf_ctrl(id, onoff);
}
EXPORT_SYMBOL(connectivity_export_clk_buf_ctrl);
#endif

/*******************************************************************************
 * MT6306 I2C-based GPIO Expander
 ******************************************************************************/
#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
void connectivity_export_mt6306_set_gpio_out(unsigned long pin,
					unsigned long output)
{
	mt6306_set_gpio_out(MT6306_GPIO_01, MT6306_GPIO_OUT_LOW);
}
EXPORT_SYMBOL(connectivity_export_mt6306_set_gpio_out);

void connectivity_export_mt6306_set_gpio_dir(unsigned long pin,
					unsigned long dir)
{
	mt6306_set_gpio_dir(MT6306_GPIO_01, MT6306_GPIO_DIR_OUT);
}
EXPORT_SYMBOL(connectivity_export_mt6306_set_gpio_dir);
#endif

/*******************************************************************************
 * PMIC
 ******************************************************************************/
void connectivity_export_pmic_config_interface(unsigned int RegNum,
		unsigned int val, unsigned int MASK, unsigned int SHIFT)
{
	pmic_config_interface(RegNum, val, MASK, SHIFT);
}
EXPORT_SYMBOL(connectivity_export_pmic_config_interface);

void connectivity_export_pmic_read_interface(unsigned int RegNum,
		unsigned int *val, unsigned int MASK, unsigned int SHIFT)
{
	pmic_read_interface(RegNum, val, MASK, SHIFT);
}
EXPORT_SYMBOL(connectivity_export_pmic_read_interface);

void connectivity_export_pmic_set_register_value(int flagname, unsigned int val)
{
#ifdef CONFIG_MTK_PMIC_COMMON
	pmic_set_register_value(flagname, val);
#endif
}
EXPORT_SYMBOL(connectivity_export_pmic_set_register_value);

unsigned short connectivity_export_pmic_get_register_value(int flagname)
{
#ifdef CONFIG_MTK_PMIC_COMMON
	return pmic_get_register_value(flagname);
#else
	return 0;
#endif
}
EXPORT_SYMBOL(connectivity_export_pmic_get_register_value);

void connectivity_export_upmu_set_reg_value(unsigned int reg,
		unsigned int reg_val)
{
	upmu_set_reg_value(reg, reg_val);
}
EXPORT_SYMBOL(connectivity_export_upmu_set_reg_value);

void connectivity_export_upmu_set_vcn_1v8_lp_mode_set(unsigned int val)
{
	upmu_set_vcn_1v8_lp_mode_set(val);
}
EXPORT_SYMBOL(connectivity_export_upmu_set_vcn_1v8_lp_mode_set);

void connectivity_export_upmu_set_vcn28_on_ctrl(unsigned int val)
{
	upmu_set_vcn28_on_ctrl(val);
}
EXPORT_SYMBOL(connectivity_export_upmu_set_vcn28_on_ctrl);

void connectivity_export_upmu_set_vcn33_on_ctrl_bt(unsigned int val)
{
	upmu_set_vcn33_on_ctrl_bt(val);
}
EXPORT_SYMBOL(connectivity_export_upmu_set_vcn33_on_ctrl_bt);

/*******************************************************************************
 * MMC
 ******************************************************************************/
int connectivity_export_mmc_io_rw_direct(struct mmc_card *card,
				int write, unsigned fn,
				unsigned addr, u8 in, u8 *out)
{
	return mmc_io_rw_direct(card, write, fn, addr, in, out);
}
EXPORT_SYMBOL(connectivity_export_mmc_io_rw_direct);

void connectivity_flush_dcache_area(void *addr, size_t len)
{
#ifdef CONFIG_ARM64
	__flush_dcache_area(addr, len);
#else
	v7_flush_kern_dcache_area(addr, len);
#endif
}
EXPORT_SYMBOL(connectivity_flush_dcache_area);

void connectivity_arch_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
				     struct iommu_ops *iommu, bool coherent)
{
	arch_setup_dma_ops(dev, dma_base, size, iommu, coherent);
}
EXPORT_SYMBOL(connectivity_arch_setup_dma_ops);

/******************************************************************************
 * GPIO dump information
 ******************************************************************************/
#ifndef CONFIG_MTK_GPIO
void __weak gpio_dump_regs_range(int start, int end)
{
	pr_info(DFT_TAG "[W]%s: is not define!\n", __func__);
}
#endif
#ifndef CONFIG_MTK_GPIO
void connectivity_export_dump_gpio_info(int start, int end)
{
	gpio_dump_regs_range(start, end);
}
EXPORT_SYMBOL(connectivity_export_dump_gpio_info);
#endif

void connectivity_export_dump_thread_state(const char *name)
{
	static const char stat_nam[] = TASK_STATE_TO_CHAR_STR;
	struct task_struct *p;
	int cpu;
	struct rq *rq;
	struct task_struct *curr;
	struct thread_info *ti;

	if (name == NULL || strlen(name) > 255) {
		pr_info("invalid name:%p or thread name too long\n", name);
		return;
	}

	pr_info("start to show debug info of %s\n", name);

	rcu_read_lock();
	for_each_process(p) {
		unsigned long state;

		if (strncmp(p->comm, name, strlen(name)) != 0)
			continue;
		state = p->state;
		cpu = task_cpu(p);
		rq = cpu_rq(cpu);
		curr = rq->curr;
		ti = task_thread_info(curr);
		if (state)
			state = __ffs(state) + 1;
		pr_info("%d:%-15.15s %c", p->pid, p->comm,
			state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?');
		pr_info("cpu=%d on_cpu=%d ", cpu, p->on_cpu);
		show_stack(p, NULL);
		pr_info("CPU%d curr=%d:%-15.15s preempt_count=0x%x", cpu,
			curr->pid, curr->comm, ti->preempt_count);

		if (state == TASK_RUNNING && curr != p)
			show_stack(curr, NULL);

		break;
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL(connectivity_export_dump_thread_state);


int conn_export_file_opened_by_task(struct task_struct *task,
	struct file *file)
{
	struct files_struct *files;
	unsigned int fd;
	int ret = -1;

	files = get_files_struct(task);
	if (!files)
		return ret;

	for (fd = 3; fd < files_fdtable(files)->max_fds; fd++) {
		if (fcheck_files(files, fd) == file) {
			ret = 0;
			break;
		}
	}
	put_files_struct(files);
	return ret;
}
EXPORT_SYMBOL(conn_export_file_opened_by_task);

void conn_export_read_task_name(struct task_struct *task, char *pcName, unsigned char name_size)
{
	char aucBuf[128] = {0};
	char *pucBuf = &aucBuf[0];
	struct mm_struct *mm;
	unsigned long count = 127;
	unsigned long arg_start, arg_end, env_start, env_end;
	unsigned long len1, len2, len;
	unsigned long p;
	char c;
	unsigned char fgDone = 0;
#define READ_VM() \
	do {\
		while (count > 0 && len > 0 && !fgDone) {\
			unsigned int _count, l;\
			int nr_read;\
\
			_count = min(count, len);\
			nr_read = access_remote_vm(mm, p, pucBuf, _count, 0);\
			if (nr_read <= 0)\
				break;\
			l = strnlen(pucBuf, nr_read);\
			if (l < nr_read) {\
				nr_read = l;\
				fgDone = 1;\
			}\
			p	+= nr_read;\
			len -= nr_read;\
			pucBuf += nr_read;\
			count	-= nr_read;\
		}\
	} while (0)

	mm = get_task_mm(task);
	if (!mm)
		goto use_comm;
	/* Check if process spawned far enough to have cmdline. */
	if (!mm->env_end) {
		goto use_comm;
	}

	down_read(&mm->mmap_sem);
	arg_start = mm->arg_start;
	arg_end = mm->arg_end;
	env_start = mm->env_start;
	env_end = mm->env_end;
	up_read(&mm->mmap_sem);

	BUG_ON(arg_start > arg_end);
	BUG_ON(env_start > env_end);

	len1 = arg_end - arg_start;
	len2 = env_end - env_start;

	/* Empty ARGV. */
	if (len1 == 0) {
		pr_info("argv length is 0\n");
		goto use_comm;
	}
	/* Read last byte of argv */
	if (access_remote_vm(mm, arg_end - 1, &c, 1, 0) <= 0)
		goto use_comm;

	/* Read argv */
	p = arg_start;
	len = len1;
	READ_VM();
	/* Read Env args */
	if (c != '\0' && len2 > 0 && !fgDone) {
		pr_info("read env\n");
		p = env_start;
		len = len2;
		READ_VM();
	}

use_comm:
	if (mm)
		mmput(mm);

	if (pucBuf != &aucBuf[0]) {
		pucBuf = strchr(aucBuf, ' ');
		if (pucBuf)
			*pucBuf = '\0';
		pucBuf = strrchr(aucBuf, '/');
		if (!pucBuf)
			pucBuf = &aucBuf[0];
		else
			pucBuf++;
		strncpy(pcName, pucBuf, name_size);
	} else {
		pr_info("No cmdline, using comm %s, %d\n", task->comm, aucBuf[0]);
		strncpy(pcName, task->comm, name_size);
	}
}
EXPORT_SYMBOL(conn_export_read_task_name);
