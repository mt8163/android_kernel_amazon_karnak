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

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/fs.h>           /* needed by file_operations */
#include <linux/init.h>         /* needed by module macros */
#include <linux/interrupt.h>
#include <linux/io.h>             /* needed by ioremap */
#include <linux/module.h>       /* needed by all modules */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/poll.h>         /* needed by poll */
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <mach/md32_ipi.h>
#include <mach/md32_helper.h>
#include <mt_spm_sleep.h>
#include <mt-plat/sync_write.h>

#include "md32_irq.h"

#define TIMEOUT 5000

#define MD32_DEVICE_NAME		"md32"
#define MD32_DATA_IMAGE_PATH	"md32_d.bin"
#define MD32_PROGRAM_IMAGE_PATH	"md32_p.bin"

#define MD32_SEMAPHORE			(MD32_BASE + 0x90)
#define MD32_CLK_CTRL_BASE		(md32reg.clkctrl)

#define MD32_LOG_DUMP_SIZE		512
#define MD32_LOG_BUF_MAX_LEN	4096

enum md32_image {
	MD32_DATA_IMAGE,
	MD32_PROGRAM_IMAGE,
};

/* This structre need to sync with MD32-side */
struct flag_log_info {
	unsigned int flag_md32_addr;
	unsigned int flag_apmcu_addr;
};

/* This structre need to sync with MD32-side */
struct md32_log_info {
	unsigned int md32_log_buf_addr;
	unsigned int md32_log_start_idx_addr;
	unsigned int md32_log_end_idx_addr;
	unsigned int md32_log_lock_addr;
	unsigned int md32_log_buf_len_addr;
	unsigned int enable_md32_mobile_log_addr;
};

struct md32_regs	md32reg;
static struct clk	*md32_clksys;

unsigned int md32_init_done;

unsigned char *md32_send_buff;
unsigned char *md32_recv_buff;

unsigned int md32_log_buf_addr;
unsigned int md32_log_start_idx_addr;
unsigned int md32_log_end_idx_addr;
unsigned int md32_log_lock_addr;
unsigned int md32_log_buf_len_addr;
unsigned int enable_md32_mobile_log_addr;
unsigned int md32_mobile_log_ready;

unsigned int flag_md32_addr;
unsigned int flag_apmcu_addr;
unsigned int apmcu_clk_init_count;
unsigned int md32_mobile_log_ipi_check;
unsigned char *g_md32_log_buf;
unsigned char *g_md32_dump_log_buf;
unsigned int last_log_buf_max_len;
unsigned int md32_log_read_count;
unsigned int buff_full_count;
unsigned int last_buff_full_count;
static wait_queue_head_t logwait;

static DEFINE_SPINLOCK(dma_lock);

void memcpy_to_md32(void __iomem *trg, const void *src, int size)
{
	int i;
	u32 __iomem *t = trg;
	const u32 *s = src;

	for (i = 0; i < ((size + 3) >> 2); i++)
		*t++ = *s++;
}

void memcpy_from_md32(void *trg, const void __iomem *src, int size)
{
	int i;
	u32 *t = trg;
	const u32 __iomem *s = src;

	for (i = 0; i < ((size + 3) >> 2); i++)
		*t++ = *s++;
}

void write_md32_cfgreg(u32 val, u32 offset)
{
	writel(val, MD32_BASE + offset);
}

u32 read_md32_cfgreg(u32 offset)
{
	return readl(MD32_BASE + offset);
}

int get_md32_semaphore(int flag)
{
	int read_back;
	int count = 0;
	int ret = -1;

	if (flag >= NR_FLAG)
		return ret;

	flag = (flag * 2) + 1;

	read_back = (readl(MD32_SEMAPHORE) >> flag) & 0x1;
	if (read_back == 0) {
		writel((1 << flag), MD32_SEMAPHORE);

		while (count != TIMEOUT) {
			/* repeat test if we get semaphore */
			read_back = (readl(MD32_SEMAPHORE) >> flag) & 0x1;
			if (read_back == 1) {
				ret = 1;
				break;
			}
			writel((1 << flag), MD32_SEMAPHORE);
			count++;
		}

		if (ret < 0)
			pr_err("[MD32] get semaphore %d TIMEOUT!\n", flag);
	} else {
		pr_err("[MD32] try to double get semaphore %d\n", flag);
	}

	return ret;
}

int release_md32_semaphore(int flag)
{
	int read_back;
	int ret = -1;

	if (flag >= NR_FLAG)
		return ret;

	flag = (flag * 2) + 1;

	read_back = (readl(MD32_SEMAPHORE) >> flag) & 0x1;
	if (read_back == 1) {
		/* Write 1 clear */
		writel((1 << flag), MD32_SEMAPHORE);
		read_back = (readl(MD32_SEMAPHORE) >> flag) & 0x1;
		if (read_back == 0)
			ret = 1;
		else
			pr_err("[MD32] release semaphore %d failed!\n", flag);
	} else {
		pr_err("[MD32] try to double release semaphore %d\n", flag);
	}

	return ret;
}

static ssize_t md32_get_log_buf(unsigned char *md32_log_buf, size_t b_len)
{
	ssize_t i = 0;
	unsigned int log_start_idx;
	unsigned int log_end_idx;
	unsigned int log_buf_max_len;
	unsigned char *__log_buf = (unsigned char *)(MD32_DTCM +
							md32_log_buf_addr);

	log_start_idx = readl((void __iomem *)(MD32_DTCM +
				md32_log_start_idx_addr));
	log_end_idx = readl((void __iomem *)(MD32_DTCM +
				md32_log_end_idx_addr));
	log_buf_max_len = readl((void __iomem *)(MD32_DTCM +
				md32_log_buf_len_addr));

	if (!md32_log_buf) {
		pr_err("[MD32] input null buffer\n");
		goto out;
	}

	if (b_len > log_buf_max_len) {
		pr_warn("[MD32] b_len %zu > log_buf_max_len %d\n", b_len,
			log_buf_max_len);
		b_len = log_buf_max_len;
	}

#define LOG_BUF_MASK (log_buf_max_len-1)
#define LOG_BUF(idx) (__log_buf[(idx) & LOG_BUF_MASK])

	/* Read MD32 log */
	/* Lock the log buffer */
	mt_reg_sync_writel(0x1, (MD32_DTCM + md32_log_lock_addr));

	i = 0;
	while ((log_start_idx != log_end_idx) && i < b_len) {
		md32_log_buf[i] = LOG_BUF(log_start_idx);
		log_start_idx++;
		i++;
	}
	mt_reg_sync_writel(log_start_idx, (MD32_DTCM +
				md32_log_start_idx_addr));

	/* Unlock the log buffer */
	mt_reg_sync_writel(0x0, (MD32_DTCM + md32_log_lock_addr));

out:

	return i;
}

static int md32_log_open(struct inode *inode, struct file *file)
{
	pr_debug("[MD32] %s\n", __func__);
	return nonseekable_open(inode, file);
}

static ssize_t md32_log_read(struct file *file, char __user *data, size_t len,
			     loff_t *ppos)
{
	int ret_len;
	unsigned long copy_size;
	unsigned int log_buf_max_len;

#ifndef LOG_TO_AP_UART
	log_buf_max_len = readl((void __iomem *)(MD32_DTCM +
				md32_log_buf_len_addr));

	if (log_buf_max_len != last_log_buf_max_len)
		last_log_buf_max_len = log_buf_max_len;

	ret_len = md32_get_log_buf(g_md32_log_buf, len);
	if (ret_len) {
		g_md32_log_buf[ret_len] = '\0';
	} else {
		/*
		strcpy(g_md32_log_buf, " ");
		ret_len = strlen(g_md32_log_buf);
		*/
		g_md32_log_buf[0] = '\0';
	}

	copy_size = copy_to_user((unsigned char *)data,
				 (unsigned char *)g_md32_log_buf, ret_len);
#else
	ret_len = 0;
#endif

	return ret_len;
}

static unsigned int md32_poll(struct file *file, poll_table *wait)
{
	unsigned int log_start_idx;
	unsigned int log_end_idx;
	unsigned int ret = 0;  /* POLLOUT | POLLWRNORM; */

	/* pr_debug("[MD32] Enter md32_poll\n"); */

	if (!(file->f_mode & FMODE_READ)) {
		/* pr_debug("[MD32] Exit1 md32_poll, ret = %d\n", ret); */
		return ret;
	}

	poll_wait(file, &logwait, wait);

	if (!md32_mobile_log_ipi_check && md32_mobile_log_ready) {
		log_start_idx = readl((void __iomem *)(MD32_DTCM +
					md32_log_start_idx_addr));
		log_end_idx = readl((void __iomem *)(MD32_DTCM +
					md32_log_end_idx_addr));
		if (log_start_idx != log_end_idx)
			ret |= (POLLIN | POLLRDNORM);
	} else if (last_buff_full_count != buff_full_count) {
		last_buff_full_count++;
		log_start_idx = readl((void __iomem *)(MD32_DTCM +
					md32_log_start_idx_addr));
		log_end_idx = readl((void __iomem *)(MD32_DTCM +
					md32_log_end_idx_addr));
		if (log_start_idx != log_end_idx)
			ret |= (POLLIN | POLLRDNORM);
	}

	/* pr_debug("[MD32] Exit2 md32_poll, ret = %d\n", ret); */

	return ret;
}

static const struct file_operations md32_file_ops = {
	.owner = THIS_MODULE,
	.read = md32_log_read,
	.open = md32_log_open,
	.poll = md32_poll,
};

static struct miscdevice md32_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MD32_DEVICE_NAME,
	.fops = &md32_file_ops
};

void logger_ipi_handler(int id, void *data, unsigned int len)
{
	struct md32_log_info *log_info = (struct md32_log_info *)data;

	md32_log_buf_addr = log_info->md32_log_buf_addr;
	md32_log_start_idx_addr = log_info->md32_log_start_idx_addr;
	md32_log_end_idx_addr = log_info->md32_log_end_idx_addr;
	md32_log_lock_addr = log_info->md32_log_lock_addr;
	md32_log_buf_len_addr = log_info->md32_log_buf_len_addr;
	enable_md32_mobile_log_addr = log_info->enable_md32_mobile_log_addr;

	md32_mobile_log_ready = 1;

	pr_debug("[MD32] md32_log_buf_addr = 0x%x\n", md32_log_buf_addr);
	pr_debug("[MD32] md32_log_start_idx_addr = 0x%x\n",
		 md32_log_start_idx_addr);
	pr_debug("[MD32] md32_log_end_idx_addr = 0x%x\n",
		 md32_log_end_idx_addr);
	pr_debug("[MD32] md32_log_lock_addr = 0x%x\n", md32_log_lock_addr);
	pr_debug("[MD32] md32_log_buf_len_addr = 0x%x\n",
		 md32_log_buf_len_addr);
	pr_debug("[MD32] enable_md32_mobile_log_addr = 0x%x\n",
		 enable_md32_mobile_log_addr);
}

void buf_full_ipi_handler(int id, void *data, unsigned int len)
{
#ifndef LOG_TO_AP_UART
	buff_full_count += 8;
	wake_up(&logwait);
#else
	int ret_len;
	unsigned int log_buf_max_len;
	int print_len, idx = 0;

	log_buf_max_len = readl((void __iomem *)(MD32_DTCM +
				md32_log_buf_len_addr));

	if (log_buf_max_len != last_log_buf_max_len)
		last_log_buf_max_len = log_buf_max_len;

	ret_len = md32_get_log_buf(g_md32_log_buf, log_buf_max_len);
	while (ret_len > 0) {
		print_len = min(ret_len, MD32_LOG_DUMP_SIZE);
		memcpy(g_md32_dump_log_buf, g_md32_log_buf+idx, print_len);
		g_md32_dump_log_buf[print_len] = '\0';
		pr_debug("[MD32 LOG] %s", g_md32_dump_log_buf);
		idx += print_len;
		ret_len -= print_len;
	}
#endif
}

void apdma_spin_lock_irqsave(unsigned long *flags)
{
	spin_lock_irqsave(&dma_lock, *flags);
}

void apdma_spin_unlock_irqrestore(unsigned long *flags)
{
	spin_unlock_irqrestore(&dma_lock, *flags);
}

void apdma_ipi_handler(int id, void *data, unsigned int len)
{
	struct flag_log_info *flag_info = (struct flag_log_info *)data;

	spin_lock(&dma_lock);
	flag_md32_addr = flag_info->flag_md32_addr;
	flag_apmcu_addr = flag_info->flag_apmcu_addr;
	mt_reg_sync_writel(apmcu_clk_init_count, (MD32_DTCM + flag_apmcu_addr));
	spin_unlock(&dma_lock);

	pr_debug("[MD32] flag_md32_addr = 0x%x\n", flag_md32_addr);
	pr_debug("[MD32] flag_apmcu_addr = 0x%x\n", flag_apmcu_addr);
	pr_debug("[MD32] INFRA APDMA clock count = %d\n", apmcu_clk_init_count);
}

unsigned int is_md32_enable(void)
{
	if (md32_init_done && readl(MD32_BASE))
		return 1;
	else
		return 0;
}

static int load_md32_fw(struct device *dev, enum md32_image type, int *size)
{
	int err;
	const struct firmware *fw;

	if (type == MD32_DATA_IMAGE) {
		const char fwname[] = MD32_DATA_IMAGE_PATH;

		err = request_firmware(&fw, fwname, dev);
		if (err != 0) {
			pr_err("fw %s not available, err:%d\n", fwname, err);
			return -err;
		}
		*size = fw->size;

		if (fw->size < MD32_DTCM_SIZE) {
			memcpy_to_md32((void *)MD32_DTCM, fw->data, fw->size);
			pr_info("fw %s load success!\n", fwname);
		} else {
			pr_err("fw %s size too large\n", fwname);
			release_firmware(fw);
			return -EFBIG;
		}

		release_firmware(fw);
	} else if (type == MD32_PROGRAM_IMAGE) {
		const char fwname[] = MD32_PROGRAM_IMAGE_PATH;

		err = request_firmware(&fw, fwname, dev);
		if (err != 0) {
			pr_err("fw %s not available, err:%d\n", fwname, err);
			return -err;
		}
		*size = fw->size;

		if (fw->size < MD32_PTCM_SIZE) {
			memcpy_to_md32((void *)MD32_PTCM, fw->data, fw->size);
			pr_info("fw %s load success!\n", fwname);
		} else{
			pr_err("fw %s size too large\n", fwname);
			release_firmware(fw);
			return -EFBIG;
		}

		release_firmware(fw);
	} else
		pr_warn("type %d unknown!\n", type);

	return 0;
}

/* put md32 in reset state */
void md32_reset_hold(void)
{
	unsigned int sw_rstn;

	sw_rstn = readl(MD32_BASE);
	if (sw_rstn == 0x0)
		pr_debug("[MD32] has already been reset!\n");
	else
		mt_reg_sync_writel(0x0, MD32_BASE);
}

/* release md32 reset */
void md32_boot_up(void)
{
	mt_reg_sync_writel(0x1, MD32_BASE);
}

static inline ssize_t md32_log_len_show(struct device *kobj,
					struct device_attribute *attr,
					char *buf)
{
	int log_legnth = 0;

	if (md32_mobile_log_ready)
		log_legnth = readl(MD32_DTCM + md32_log_buf_len_addr);

	return sprintf(buf, "%08x\n", log_legnth);
}

static ssize_t md32_log_len_store(struct device *kobj,
				  struct device_attribute *attr,
				  const char *buf, size_t n)
{
	/*do nothing*/
	return n;
}

DEVICE_ATTR(md32_log_len, 0644, md32_log_len_show, md32_log_len_store);

int reboot_load_md32(struct device *dev)
{
	int i;
	int ret = 0;
	int d_sz = 0, p_sz = 0;
	int d_num, p_num;
	unsigned int sw_rstn;
	unsigned int reg;

	sw_rstn = readl(MD32_BASE);

	if (sw_rstn == 0x1)
		pr_info("[MD32] MD32 is already running, reboot now...\n");

	/* reset MD32 */
	md32_reset_hold();

	mt_reg_sync_writel(0x1FF, MD32_CLK_CTRL_BASE+0x030);
	mt_reg_sync_writel(0x1, MD32_BASE+0x008);

	reg = readl(MD32_CLK_CTRL_BASE+0x02C);
	for (i = 0; i < 7; ++i) {
		reg &= ~(0x1 << i);
		mt_reg_sync_writel(reg, MD32_CLK_CTRL_BASE+0x02C);
	}

	ret = load_md32_fw(dev, MD32_DATA_IMAGE, &d_sz);
	if (ret != 0) {
	/* Change from '<' to '!=', since err ret is a positive number */
		pr_err("boot up failed --> load data image failed!\n");
		goto load_error;
	}

	ret = load_md32_fw(dev, MD32_PROGRAM_IMAGE, &p_sz);
	if (ret != 0) {
	/* Change from '<' to '!=', since err ret is a positive number */
		pr_err("boot up failed --> load program image failed!\n");
		goto load_error;
	}

	/* Turn on the power of ptcm block. 32K for one block */
	p_num = (p_sz / (32*1024)) + 1;
	reg = readl(MD32_CLK_CTRL_BASE+0x02C);
	for (i = p_num; i < 3; ++i) {
		reg |= (0x1 << i);
		mt_reg_sync_writel(reg, MD32_CLK_CTRL_BASE+0x02C);
	}

	/* Turn on the power of dtcm block.  32K for one block */
	d_num = (d_sz / (32*1024)) + 1;
	reg = readl(MD32_CLK_CTRL_BASE+0x02C);
	for (i = 6-d_num; i > 2; --i) {
		reg |= (0x1 << i);
		mt_reg_sync_writel(reg, MD32_CLK_CTRL_BASE+0x02C);
	}

	/* boot up MD32 */
	md32_boot_up();
	return ret;

load_error:
	pr_err("boot up failed!!!\n");
	return ret;
}

static inline ssize_t md32_boot_show(struct device *kobj,
				     struct device_attribute *attr, char *buf)
{
	unsigned int sw_rstn;

	sw_rstn = readl(MD32_BASE);

	if (sw_rstn == 0x0)
		return sprintf(buf, "MD32 not enabled\n");
	else
		return sprintf(buf, "MD32 is running...\n");
}

static ssize_t md32_boot_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t n)
{
	unsigned int enable = 0;
	unsigned int sw_rstn;
	int ret;

	ret = kstrtouint(buf, 10, &enable);
	if (ret)
		return ret;

	sw_rstn = readl(MD32_BASE);
	if (sw_rstn == 0x0) {
		pr_debug("[MD32] Enable MD32\n");
		if (reboot_load_md32(dev) < 0) {
			pr_err("[MD32] Enable MD32 fail\n");
			return -EINVAL;
		}
	} else {
		pr_debug("[MD32] MD32 is enabled\n");
	}

	return n;
}

DEVICE_ATTR(md32_boot, 0644, md32_boot_show, md32_boot_store);

static inline ssize_t md32_mobile_log_show(struct device *kobj,
					   struct device_attribute *attr,
					   char *buf)
{
	unsigned int enable_md32_mobile_log = 0;

	if (md32_mobile_log_ready)
		enable_md32_mobile_log = readl((void __iomem *)(MD32_DTCM +
					       enable_md32_mobile_log_addr));

	if (enable_md32_mobile_log == 0x0)
		return sprintf(buf, "MD32 mobile log is disabled\n");
	else if (enable_md32_mobile_log == 0x1)
		return sprintf(buf, "MD32 mobile log is enabled\n");
	else
		return sprintf(buf, "MD32 mobile log is in unknown state...\n");
}

static ssize_t md32_mobile_log_store(struct device *kobj,
				     struct device_attribute *attr,
				     const char *buf, size_t n)
{
	unsigned int enable;
	int ret;

	ret = kstrtouint(buf, 10, &enable);

	if (ret)
		return ret;

	if (md32_mobile_log_ready)
		mt_reg_sync_writel(enable,
				   (MD32_DTCM + enable_md32_mobile_log_addr));

	return n;
}

DEVICE_ATTR(md32_mobile_log, 0644, md32_mobile_log_show, md32_mobile_log_store);

static int create_files(void)
{
	int ret = device_create_file(md32_device.this_device,
				     &dev_attr_md32_log_len);

	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(md32_device.this_device, &dev_attr_md32_boot);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(md32_device.this_device,
				 &dev_attr_md32_mobile_log);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(md32_device.this_device, &dev_attr_md32_ocd);
	if (unlikely(ret != 0))
		return ret;

	return 0;
}

int md32_dt_init(struct platform_device *pdev)
{
	int ret = 0;
	struct md32_regs *mreg = &md32reg;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "get md32 sram memory resource failed.\n");
		ret = -ENXIO;
		return ret;
	}
	mreg->sram = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mreg->sram)) {
		dev_err(&pdev->dev, "devm_ioremap_resource md32 sram failed.\n");
		ret = PTR_ERR(mreg->sram);
		return ret;
	}
	pr_debug("[MD32] md32 sram base=0x%p\n", mreg->sram);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res == NULL) {
		dev_err(&pdev->dev, "get md32 cfg memory resource failed.\n");
		ret = -ENXIO;
		return ret;
	}
	mreg->cfg = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mreg->cfg)) {
		dev_err(&pdev->dev, "devm_ioremap_resource vpu cfg failed.\n");
		ret = PTR_ERR(mreg->cfg);
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "get irq resource failed.\n");
		ret = -ENXIO;
		return ret;
	}
	mreg->irq = platform_get_irq(pdev, 0);
	pr_debug("[MD32] md32 irq=%d, cfgreg=0x%p\n", mreg->irq, mreg->cfg);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (res == NULL) {
		dev_err(&pdev->dev, "get md32 clk memory resource failed.\n");
		ret = -ENXIO;
		return ret;
	}
	mreg->clkctrl = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mreg->clkctrl)) {
		dev_err(&pdev->dev, "devm_ioremap_resource vpu clk failed.\n");
		ret = PTR_ERR(mreg->clkctrl);
		return ret;
	}
	pr_debug("[MD32] md32 clkctrl=0x%p\n", mreg->clkctrl);

	return ret;
}

static int md32_pm_event(struct notifier_block *nb,
			 unsigned long pm_event, void *unused)
{
	int retval;
	struct mtk_md32 *md32_dev = container_of(nb, struct mtk_md32,
						    pm_nb);

	switch (pm_event) {
	case PM_POST_HIBERNATION:
		pr_debug("[MD32] %s reboot\n", __func__);
		retval = reboot_load_md32(md32_dev->dev);
		if (retval < 0) {
			retval = -EINVAL;
			pr_err("[MD32] md32_pm_event reboot Fail\n");
		}
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block md32_pm_notifier_block = {
	.notifier_call = md32_pm_event,
	.priority = 0,
};

static int md32_probe(struct platform_device *pdev)
{
	int ret = 0, i;
	struct device *dev;
	unsigned int reg;
	struct mtk_md32 *md32_dev;

	init_waitqueue_head(&logwait);

	ret = misc_register(&md32_device);
	if (unlikely(ret != 0)) {
		pr_err("[MD32] misc register failed\n");
		ret = -1;
	}

	ret = md32_dt_init(pdev);
	if (ret) {
		pr_err("[MD32] Device Init Fail\n");
		return ret;
	}
	md32_dev = kzalloc(sizeof(*md32_dev), GFP_KERNEL);
	if (!md32_dev)
		return -ENOMEM;

	dev = &pdev->dev;
	md32_dev->dev = &pdev->dev;

	/*MD32 clock */
	md32_clksys = devm_clk_get(dev, "sys");
	if (IS_ERR(md32_clksys)) {
		pr_err("[MD32] devm_clk_get clksys fail, %d\n",
		       IS_ERR(md32_clksys));
		return -EINVAL;
	}
	/* Enable MD32 clock */
	ret = clk_prepare_enable(md32_clksys);
	if (ret) {
		pr_err("[MD32] enable md32 clksys failed %d\n", ret);
		return ret;
	}

	md32_send_buff = vmalloc((size_t)64);
	if (!md32_send_buff)
		return -ENOMEM;

	md32_recv_buff = vmalloc((size_t)64);
	if (!md32_recv_buff)
		return -ENOMEM;

	g_md32_log_buf = vmalloc(MD32_LOG_BUF_MAX_LEN+1);
	if (!g_md32_log_buf)
		return -ENOMEM;

	g_md32_dump_log_buf = vmalloc(MD32_LOG_DUMP_SIZE+1);
	if (!g_md32_dump_log_buf)
		return -ENOMEM;

	/* Enable peripherals' clock */
	mt_reg_sync_writel(0x1FF, MD32_CLK_CTRL_BASE+0x030);

	/* Configure PTCM/DTCM to 96K/128K */
	mt_reg_sync_writel(0x1, MD32_BASE+0x008);

	/* Powe on MD32 SRAM power */
	reg = readl(MD32_CLK_CTRL_BASE+0x02C);
	for (i = 0; i < 7; ++i) {
		reg &= ~(0x1 << i);
		mt_reg_sync_writel(reg, MD32_CLK_CTRL_BASE+0x02C);
	}

	md32_irq_init(md32_dev);
	md32_ipi_init();
	md32_ocd_init();

	/* register logger IPI */
	md32_ipi_registration(IPI_LOGGER, logger_ipi_handler, "logger");

	/* register log buf full IPI */
	md32_ipi_registration(IPI_BUF_FULL, buf_full_ipi_handler, "buf_full");

	/* register apdma IPI */
	md32_ipi_registration(IPI_APDMA, apdma_ipi_handler, "apdma");

	/* register IRQ Assert IPI */
	md32_ipi_registration(IPI_IRQ_AST, irq_ast_ipi_handler, "irq_ast");

	pr_debug("[MD32] helper init\n");

	ret = create_files();
	if (unlikely(ret != 0))
		pr_err("[MD32] create files failed\n");

	ret = register_pm_notifier(&md32_pm_notifier_block);
	if (ret)
		pr_err("[MD32] failed to register PM notifier %d\n", ret);

	ret = devm_request_irq(dev, md32reg.irq, md32_irq_handler, 0,
			       pdev->name, md32_dev);
	if (ret)
		dev_err(&pdev->dev, "[MD32] failed to request irq\n");

//	pr_debug("[MD32] start run MD32 firmware\n");
//	reboot_load_md32(&pdev->dev);

	return ret;
}

static int md32_remove(struct platform_device *pdev)
{
	vfree(md32_send_buff);
	vfree(md32_recv_buff);
	vfree(g_md32_log_buf);
	vfree(g_md32_dump_log_buf);
	misc_deregister(&md32_device);
	return 0;
}

static const struct of_device_id md32_match[] = {
	{ .compatible = "mediatek,mt8163-md32",},
	{},
};
MODULE_DEVICE_TABLE(of, md32_match);

static struct platform_driver md32_driver = {
	.probe	= md32_probe,
	.remove	= md32_remove,
	.driver	= {
		.name	= MD32_DEVICE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = md32_match,
	},
};

module_platform_driver(md32_driver);

