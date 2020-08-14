/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <mach/md32_ipi.h>
#include <mach/md32_helper.h>
#include <mt-plat/aee.h>
#include <mt-plat/sync_write.h>

#include "md32_irq.h"

#define MD32_AP_DEBUG  1
#if (MD32_AP_DEBUG == 1)
#define dbg_msg pr_debug
#else
#define dbg_msg(...)
#endif

#define MD32_AED_PHY_SIZE (MD32_PTCM_SIZE + MD32_DTCM_SIZE + MD32_CFGREG_SIZE)
#define MD32_AED_STR_LEN  (512)

struct reg_md32_to_host_ipc {
	unsigned int ipc_md2host   :1;
	unsigned int               :7;
	unsigned int md32_ipc_int  :1;
	unsigned int wdt_int       :1;
	unsigned int pmem_disp_int :1;
	unsigned int dmem_disp_int :1;
	unsigned int               :20;
};

/* This structre need to sync with MD32-side */
struct md32_irq_ast_info {
	unsigned int md32_irq_ast_time;
	unsigned int md32_irq_ast_pc_s;
	unsigned int md32_irq_ast_pc_e;
	unsigned int md32_irq_ast_lr_s;
	unsigned int md32_irq_ast_lr_e;
	unsigned int md32_irq_ast_irqd;
};

#ifdef MD32_WDT_ASSERT_REGIST
static struct md32_wdt_func MD32_WDT_FUN;
static struct md32_assert_func MD32_ASSERT_FUN;
static struct work_struct work_md32_wdt;
static struct work_struct work_md32_assert;
static struct workqueue_struct *wq_md32_wdt;
static struct workqueue_struct *wq_md32_assert;
#endif

struct md32_aed_cfg {
	int *log;
	int log_size;
	int *phy;
	int phy_size;
	char *detail;
};

struct md32_reboot_work {
	struct work_struct work;
	struct md32_aed_cfg aed;
};

struct md32_status_reg {
	unsigned int status;
	unsigned int pc;
	unsigned int r14;
	unsigned int r15;
	unsigned int m2h_irq;
};

static struct md32_status_reg md32_aee_status;

void md32_dump_regs(void)
{
	unsigned long *reg;

	pr_err("md32_dump_regs\n");
	pr_err("md32 status = 0x%08x\n", readl(MD32_BASE));
	pr_err("md32 pc=0x%08x, r14=0x%08x, r15=0x%08x\n",
		 readl(MD32_DEBUG_PC_REG), readl(MD32_DEBUG_R14_REG),
		 readl(MD32_DEBUG_R15_REG));
	pr_err("md32 to host inerrupt = 0x%08x\n", readl(MD32_TO_HOST_REG));
	pr_err("host to md32 inerrupt = 0x%08x\n", readl(HOST_TO_MD32_REG));

	pr_err("wdt en=%d, count=0x%08x\n",
		 (readl(MD32_WDT_REG) & 0x10000000) ? 1 : 0,
		 (readl(MD32_WDT_REG) & 0xFFFFF));

	/*dump all md32 regs*/
	for (reg = MD32_BASE;
	     (unsigned long)reg < (unsigned long)(MD32_BASE + 0xA8); reg++) {
		if (!((uintptr_t)(void *)reg & 0xF)) {
			pr_err("\n");
			pr_err("[0x%p]   ", reg);
		}

		pr_err("0x%016lx  ", *reg);
	}
	pr_err("\n");
	pr_err("md32_dump_regs end\n");
}

void md32_aee_stop(void)
{
	pr_debug("%s start\n", __func__);

	md32_aee_status.status = readl(MD32_BASE);
	md32_aee_status.pc = readl(MD32_DEBUG_PC_REG);
	md32_aee_status.r14 = readl(MD32_DEBUG_R14_REG);
	md32_aee_status.r15 = readl(MD32_DEBUG_R15_REG);

	mt_reg_sync_writel(0x0, MD32_BASE);

	pr_debug("%s end\n", __func__);
}

int md32_aee_dump(char *buf)
{
	unsigned long *reg;
	char *ptr = buf;

	ssize_t i, len;
	unsigned int log_start_idx;
	unsigned int log_end_idx;
	unsigned int log_buf_max_len;
	unsigned char *__log_buf = (unsigned char *)(MD32_DTCM +
				    md32_log_buf_addr);

	if (!buf)
		return 0;

	pr_debug("md32_aee_dump\n");
	ptr += sprintf(ptr, "md32 status = 0x%08x\n", md32_aee_status.status);
	ptr += sprintf(ptr, "md32 pc=0x%08x, r14=0x%08x, r15=0x%08x\n",
		       md32_aee_status.pc, md32_aee_status.r14,
		       md32_aee_status.r15);
	ptr += sprintf(ptr, "md32 to host irq = 0x%08x\n",
		       md32_aee_status.m2h_irq);
	ptr += sprintf(ptr, "host to md32 irq = 0x%08x\n",
		       readl(HOST_TO_MD32_REG));

	ptr += sprintf(ptr, "wdt en=%d, count=0x%08x\n",
		       (readl(MD32_WDT_REG) & 0x10000000) ? 1 : 0,
		       (readl(MD32_WDT_REG) & 0xFFFFF));

	/*dump all md32 regs*/
	for (reg = MD32_BASE;
	     (unsigned long)reg < (unsigned long)(MD32_BASE + 0xA8); reg++) {
		if (!((uintptr_t)(void *)reg & 0xF)) {
			ptr += sprintf(ptr, "\n");
			ptr += sprintf(ptr, "[0x%p]   ", reg);
		}

		ptr += sprintf(ptr, "0x%016lx  ", *reg);
	}
	ptr += sprintf(ptr, "\n");

#define LOG_BUF_MASK (log_buf_max_len-1)
#define LOG_BUF(idx) (__log_buf[(idx) & LOG_BUF_MASK])

	log_start_idx = readl((void __iomem *)(MD32_DTCM +
			      md32_log_start_idx_addr));
	log_end_idx = readl((void __iomem *)(MD32_DTCM +
			    md32_log_end_idx_addr));
	log_buf_max_len = readl((void __iomem *)(MD32_DTCM +
				md32_log_buf_len_addr));

	ptr += sprintf(ptr, "log_buf_addr = 0x%08x\n",
			(unsigned int)md32_log_buf_addr);
	ptr += sprintf(ptr, "log_start_idx = %u\n", log_start_idx);
	ptr += sprintf(ptr, "log_end_idx = %u\n", log_end_idx);
	ptr += sprintf(ptr, "log_buf_max_len = %u\n", log_buf_max_len);

	ptr += sprintf(ptr, "<<md32 log buf start>>\n");
	len = (log_buf_max_len > 0x1000) ? 0x1000 : log_buf_max_len;
	i = 0;
	while ((log_start_idx != log_end_idx) && i < len) {
		ptr += sprintf(ptr, "%c", LOG_BUF(log_start_idx));
		log_start_idx++;
		i++;
	}
	ptr += sprintf(ptr, "<<md32 log buf end>>\n");

	pr_debug("md32_aee_dump end\n");

	return ptr - buf;
}

void md32_prepare_aed(char *aed_str, struct md32_aed_cfg *aed)
{
	char *detail;
	u8 *log, *phy, *ptr;
	u32 log_size, phy_size;

	pr_debug("md32_prepare_aed\n");

	detail = kmalloc(MD32_AED_STR_LEN, GFP_KERNEL);
	if (detail)
		ptr = detail;
	else
		return;
	detail[MD32_AED_STR_LEN - 1] = '\0';
	ptr += snprintf(detail, MD32_AED_STR_LEN, "%s", aed_str);
	ptr += sprintf(ptr, " md32 pc=0x%08x, r14=0x%08x, r15=0x%08x\n",
		       md32_aee_status.pc, md32_aee_status.r14,
		       md32_aee_status.r15);

	log_size = 0x4000; /* 16KB */
	log = kmalloc(log_size, GFP_KERNEL);
	if (!log) {
		pr_err("ap allocate log buffer fail, size=0x%x\n", log_size);
		log_size = 0;
	} else {
		memset(log, 0, log_size);

		ptr = log;

		ptr += md32_aee_dump(ptr);

		/* print log in kernel */
		pr_debug("%s", log);

		ptr += sprintf(ptr, "dump memory info\n");
		ptr += sprintf(ptr, "md32 cfgreg: 0x%08x\n", 0);
		ptr += sprintf(ptr, "md32 ptcm  : 0x%08x\n", MD32_CFGREG_SIZE);
		ptr += sprintf(ptr, "md32 dtcm  : 0x%08x\n", MD32_CFGREG_SIZE +
				MD32_PTCM_SIZE);
		*ptr = '\0';
		log_size = ptr - log;
	}

	phy_size = MD32_AED_PHY_SIZE;
	phy = kmalloc(phy_size, GFP_KERNEL);
	if (!phy) {
		pr_debug("ap allocate phy buffer fail, size=0x%x\n", phy_size);
		phy_size = 0;
	} else {
		ptr = phy;

		memcpy_from_md32((void *)ptr,
				 (void *)MD32_BASE, MD32_CFGREG_SIZE);
		ptr += MD32_CFGREG_SIZE;

		memcpy_from_md32((void *)ptr,
				 (void *)MD32_PTCM, MD32_PTCM_SIZE);
		ptr += MD32_PTCM_SIZE;

		memcpy_from_md32((void *)ptr,
				 (void *)MD32_DTCM, MD32_DTCM_SIZE);
		ptr += MD32_DTCM_SIZE;
	}

	aed->log = (int *)log;
	aed->log_size = log_size;
	aed->phy = (int *)phy;
	aed->phy_size = phy_size;
	aed->detail = detail;

	pr_debug("md32_prepare_aed end\n");
}

void md32_dmem_abort_handler(void)
{
	pr_err("[MD32 EXCEP] DMEM Abort\n");
	md32_dump_regs();
}

void md32_pmem_abort_handler(void)
{
	pr_err("[MD32 EXCEP] PMEM Abort\n");
	md32_dump_regs();
}

void md32_wdt_handler(void)
{
	pr_err("[MD32 EXCEP] WDT\n");
	md32_dump_regs();
}

#ifdef MD32_WDT_ASSERT_REGIST
void md32_wdt_reset_func(void)
{
	int index;
#define TEST_PHY_SIZE 0x10000
	pr_debug("reset_md32_func\n");

	/* 1. AEE function */
	aee_kernel_exception("MD32", "MD32 WDT Time out ");
	/* 2. Reset md32 */
	pr_debug("WDT Reset md32 ok!\n");
	/* 3. Call driver's callback */
	for (index = 0; index < MD32_MAX_USER; index++) {
		if (MD32_WDT_FUN.in_use[index] &&
		    MD32_WDT_FUN.reset_func[index] != NULL) {
			void *pri_data = MD32_WDT_FUN.private_data[index];

			MD32_WDT_FUN.reset_func[index](pri_data);
		}
	}
}

void md32_assert_reset_func(void)
{
	int index;

	pr_debug("reset_md32_func\n");
	/* 1. AEE function */
	aee_kernel_exception("MD32", "MD32 ASSERT ");
	/* 2. Reset md32 */
	pr_debug("ASSERT Reset md32 ok!\n");
	/* 3. Call driver's callback */
	for (index = 0; index < MD32_MAX_USER; index++) {
		if (MD32_ASSERT_FUN.in_use[index] &&
		    MD32_ASSERT_FUN.reset_func[index] != NULL) {
			void *pri_data = MD32_ASSERT_FUN.private_data[index];

			MD32_ASSERT_FUN.reset_func[index](pri_data);
		}
	}
}

int alloc_md32_wdt_func(void)
{
	int index;

	for (index = 0; index < MD32_MAX_USER; index++) {
		if (!MD32_WDT_FUN.in_use[index]) {
			MD32_WDT_FUN.in_use[index] = 1;
			return index;
		}
	}
	return -1;
}

int alloc_md32_assert_func(void)
{
	int index;

	for (index = 0; index < MD32_MAX_USER; index++) {
		if (!MD32_ASSERT_FUN.in_use[index]) {
			MD32_ASSERT_FUN.in_use[index] = 1;
			return index;
		}
	}
	return -1;
}

int md32_wdt_register_handler_services(void MD32_WDT_FUNC_PTR(void *),
				       void WDT_RESET(void), void *private_data,
				       char *module_name)
{
	int index_cur;

	index_cur = alloc_md32_wdt_func();
	dbg_msg("wdt register index %d\n", index_cur);
	if (index_cur < 0) {
		dbg_msg("MD32_WDT_FUNC is full");
		return -1;
	}
	MD32_WDT_FUN.wdt_func[index_cur] = MD32_WDT_FUNC_PTR;
	MD32_WDT_FUN.reset_func[index_cur] = WDT_RESET;
	MD32_WDT_FUN.private_data[index_cur] = private_data;
	strcpy(MD32_WDT_FUN.MODULE_NAME[index_cur], module_name);
	return 1;
}

int md32_assert_register_handler_services(void MD32_ASSERT_FUNC_PTR(void *),
					  void ASSERT_RESET(void),
					  void *private_data,
					  char *module_name)
{
	int index_cur;

	index_cur = alloc_md32_assert_func();
	dbg_msg("assert register index %d\n", index_cur);
	if (index_cur < 0) {
		dbg_msg("MD32_ASSERT_FUNC is full");
		return -1;
	}
	MD32_ASSERT_FUN.assert_func[index_cur] = MD32_ASSERT_FUNC_PTR;
	MD32_ASSERT_FUN.reset_func[index_cur] = ASSERT_RESET;
	MD32_ASSERT_FUN.private_data[index_cur] = private_data;
	strcpy(MD32_ASSERT_FUN.MODULE_NAME[index_cur], module_name);
	return 1;
}

int free_md32_wdt_func(char *module_name)
{
	int index;

	dbg_msg("Flush works in WDT work queue\n");
	flush_workqueue(wq_md32_wdt);
	dbg_msg("Free md32_wdt structure\n");
	for (index = 0; index < MD32_MAX_USER; index++) {
		if (strcmp(module_name, MD32_WDT_FUN.MODULE_NAME[index]) == 0) {
			MD32_WDT_FUN.in_use[index] = 0;
			MD32_WDT_FUN.wdt_func[index] = NULL;
			MD32_WDT_FUN.reset_func[index] = NULL;
			MD32_WDT_FUN.private_data[index] = NULL;
			return 0;
		}
	}
	dbg_msg("Can't free %s\n", module_name);
	return -1;
}

int free_md32_assert_func(char *module_name)
{
	int index;

	dbg_msg("Flush works in ASSERT work queue\n");
	flush_workqueue(wq_md32_assert);
	dbg_msg("Free md32_assert structure\n");
	for (index = 0; index < MD32_MAX_USER; index++) {
		if (strcmp(module_name,
			   MD32_ASSERT_FUN.MODULE_NAME[index]) == 0) {
			MD32_ASSERT_FUN.in_use[index] = 0;
			MD32_ASSERT_FUN.assert_func[index] = NULL;
			MD32_ASSERT_FUN.reset_func[index] = NULL;
			MD32_ASSERT_FUN.private_data[index] = NULL;
			return 0;
		}
	}
	dbg_msg("Can't free %s\n", module_name);
	return -1;
}
#endif

irqreturn_t md32_irq_handler(int irq, void *dev_id)
{
	struct mtk_md32 *md32_dev = (struct mtk_md32 *)dev_id;
	struct reg_md32_to_host_ipc *md32_irq;
	int reboot = 0;

	md32_irq = (struct reg_md32_to_host_ipc *)MD32_TO_HOST_ADDR;

	if (md32_irq->wdt_int) {
		md32_wdt_handler();
		md32_aee_stop();
		md32_aee_status.m2h_irq = readl(MD32_TO_HOST_REG);
		md32_irq->wdt_int = 0;
		reboot = 1;
	}

	if (md32_irq->pmem_disp_int) {
		md32_pmem_abort_handler();
		md32_aee_stop();
		md32_aee_status.m2h_irq = readl(MD32_TO_HOST_REG);
		md32_irq->pmem_disp_int = 0;
		reboot = 1;
	}

	if (md32_irq->dmem_disp_int) {
		md32_dmem_abort_handler();
		md32_aee_stop();
		md32_aee_status.m2h_irq = readl(MD32_TO_HOST_REG);
		md32_irq->dmem_disp_int = 0;
		reboot = 1;
	}

	if (md32_irq->md32_ipc_int) {
		md32_ipi_handler();
		md32_irq->ipc_md2host = 0;
		md32_irq->md32_ipc_int = 0;
	}

	writel(0x0, MD32_TO_HOST_REG);

	if (reboot)
		queue_work(md32_dev->md32_workqueue, &md32_dev->md32_work);

	return IRQ_HANDLED;
}

static void md32_reboot_from_irq(struct work_struct *work)
{
	struct mtk_md32 *md32_dev;
	struct md32_reboot_work *rb_ws = (struct md32_reboot_work *)work;
	struct md32_aed_cfg *aed = &rb_ws->aed;
	struct reg_md32_to_host_ipc *md32_irq = (struct reg_md32_to_host_ipc *)
						&md32_aee_status.m2h_irq;

	md32_dev = container_of(work, struct mtk_md32, md32_work);

	if (md32_irq->wdt_int)
		md32_prepare_aed("md32 wdt", aed);
	else if (md32_irq->pmem_disp_int)
		md32_prepare_aed("md32 pmem_abort", aed);
	else if (md32_irq->dmem_disp_int)
		md32_prepare_aed("md32 dmem_abort", aed);
	else
		md32_prepare_aed("md32 exception", aed);

	pr_debug("%s", aed->detail);

	aed_md32_exception_api(aed->log, aed->log_size, aed->phy, aed->phy_size,
			       aed->detail, DB_OPT_DEFAULT);

	kfree(aed->detail);
	kfree(aed->phy);
	kfree(aed->log);

	pr_debug("[MD32] md32 exception dump is done\n");

	/* Reboot MD32 when receiver exception interrupt */
	pr_err("md32 exception dump is done, Reload MD32 bin from IRQ\n");
	reboot_load_md32(md32_dev->dev);

	pr_err("[MD32] Reload MD32 bin from IRQ done\n");
}

void irq_ast_ipi_handler(int id, void *data, unsigned int len)
{
	struct md32_irq_ast_info *irq_ast_info;

	irq_ast_info = (struct md32_irq_ast_info *)data;
	pr_debug("[MD32] md32 irq was disabled for %d ns\n",
		 irq_ast_info->md32_irq_ast_time*5);
	pr_debug("[MD32] pc_s=0x%x, pc_e=0x%x, lr_s=0x%x, lr_e=0x%x, irq=%d\n",
		 irq_ast_info->md32_irq_ast_pc_s,
		 irq_ast_info->md32_irq_ast_pc_e,
		 irq_ast_info->md32_irq_ast_lr_s,
		 irq_ast_info->md32_irq_ast_lr_e,
		 irq_ast_info->md32_irq_ast_irqd);
}

void md32_irq_init(struct mtk_md32 *md32)
{
	writel(0x0, MD32_TO_HOST_REG); /* clear md32 irq */

#ifdef MD32_WDT_ASSERT_REGIST
	/* Reset Structures */
	memset(&MD32_WDT_FUN, 0, sizeof(MD32_WDT_FUN));
	memset(&MD32_ASSERT_FUN, 0, sizeof(MD32_ASSERT_FUN));
#endif

	md32->md32_workqueue = alloc_ordered_workqueue("MD32_REBOOT_WQ", 0);
#ifdef MD32_WDT_ASSERT_REGIST
	wq_md32_wdt = alloc_ordered_workqueue("MD32_WDT_WQ", 0);
	wq_md32_assert = alloc_ordered_workqueue("MD32_ASSERT_WQ", 0);
#endif

	INIT_WORK(&md32->md32_work, md32_reboot_from_irq);
#ifdef MD32_WDT_ASSERT_REGIST
	INIT_WORK(&work_md32_wdt, md32_wdt_reset_func);
	INIT_WORK(&work_md32_assert, md32_assert_reset_func);
#endif
}
