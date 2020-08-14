
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/reboot.h>
#include <linux/mdump.h>
#include <linux/atomic.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/syscore_ops.h>
#include <linux/sysfs.h>
#include <linux/mm.h>
#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
#include "mediatek/include/mt-plat/mtk_boot_common.h"
#endif

#ifdef CONFIG_MDUMP_COMPRESS
#include <linux/vmalloc.h>
#endif

struct mdbinattr {
	struct bin_attribute binattr;
	int readonly;
	char *address;
};
struct mdtxtattr {
	struct attribute attr;
	void *udata;
	ssize_t (*show)(void*, char *buffer);
	ssize_t (*store)(void*, const char *buffer, size_t size);
};

#ifdef CONFIG_MDUMP_COMPRESS
struct mdcompbinattr {
	struct bin_attribute binattr;
	void *vaddr;
	void *paddr;
};
#endif

static struct mdump_buffer *s_mdump_buffer;
static struct kobject s_mdump_kobj;

static void mdump_dma_cache_flush_range(void *start, size_t size)
{
#ifdef CONFIG_ARM64
	__dma_flush_area((void *)start, size);
#else
	dmac_flush_range((void *)start, (void *)(start + size));
#endif
}

static ssize_t mdump_binary_read(struct file *filp,
		struct kobject *kobj,
		struct bin_attribute *attr,
		char *buf, loff_t off, size_t count)
{
	struct mdbinattr *mdattr = (struct mdbinattr *) attr;

	if (off > mdattr->binattr.size)
		return -ERANGE;
	if (off + count > mdattr->binattr.size)
		count = mdattr->binattr.size - off;
	if (count != 0)
		memcpy(buf, mdattr->address + off, count);
	return count;
}

static ssize_t mdump_binary_write(struct file *filp,
		struct kobject *kobj,
		struct bin_attribute *attr,
		char *data, loff_t off, size_t count)
{
	struct mdbinattr *mdattr = (struct mdbinattr *) attr;

	if (mdattr->readonly == 0) {
		if (off > mdattr->binattr.size)
			return -ERANGE;
		if (off + count > mdattr->binattr.size)
			count = mdattr->binattr.size - off;
		if (count != 0)
			memcpy(mdattr->address + off, data, count);
	}
	return count;
}

#ifdef CONFIG_MDUMP_COMPRESS
static void *remap_lowmem(phys_addr_t start, phys_addr_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	prot = pgprot_writecombine(PAGE_KERNEL);

	pages = kmalloc(sizeof(struct page *) * page_count, GFP_KERNEL);
	if (!pages) {
		pr_err("%s: Failed to allocate array for %u pages\n", __func__, page_count);
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);
	if (!vaddr) {
		pr_err("%s: Failed to map %u pages\n", __func__, page_count);
		return NULL;
	}

	return vaddr + offset_in_page(start);
}

/* for safe, we only map 32MB per time */
#define PHYMEM_MAP_CHUNK    0x2000000
static ssize_t mdump_compdump_binary_read(struct file *filp,
		struct kobject *kobj,
		struct bin_attribute *attr,
		char *buf, loff_t off, size_t count)
{
	unsigned int expbase;
	struct mdcompbinattr *mdcattr = (struct mdcompbinattr *) attr;

	if (off > mdcattr->binattr.size)
		return -ERANGE;

	if (off + count > mdcattr->binattr.size)
		count = mdcattr->binattr.size - off;
	expbase = (off & (~(PHYMEM_MAP_CHUNK-1))) + COMPRESS_START_ADDRESS;
	if (expbase > (unsigned int)(unsigned long)(mdcattr->paddr)) {
		vunmap((void *)mdcattr->vaddr);
		mdcattr->vaddr = remap_lowmem((phys_addr_t)expbase, (phys_addr_t)PHYMEM_MAP_CHUNK);
		if (mdcattr->vaddr == NULL) {
			pr_err("compress dump function can not map physicall addr: 0x%x\n", expbase);
			return 0;
		}
		mdcattr->paddr = (void *)(unsigned long)expbase;
	}
	/* make sure not to read beyond the mapped memory region */
	if ((off - (expbase - COMPRESS_START_ADDRESS) + count) > PHYMEM_MAP_CHUNK)
		count = PHYMEM_MAP_CHUNK - (off - (expbase - COMPRESS_START_ADDRESS));

	if (count != 0) {
		memcpy(buf, mdcattr->vaddr + off-(expbase-COMPRESS_START_ADDRESS), count);
		memset(mdcattr->vaddr + off-(expbase-COMPRESS_START_ADDRESS), 0, count);
		mdump_dma_cache_flush_range(mdcattr->vaddr + off-(expbase-COMPRESS_START_ADDRESS), count);
	}

	if ((off % PHYMEM_MAP_CHUNK) == 0)
		pr_err("total %u MBytes had been sent!\n", (unsigned int)(off/PHYMEM_MAP_CHUNK)*32);

	return count;
}

static ssize_t mdump_compdump_binary_write(struct file *filp,
		struct kobject *kobj,
		struct bin_attribute *attr,
		char *data, loff_t off, size_t count)
{
	return 0;
}

static struct mdcompbinattr s_mdump_compdump = {
	{
		{ .name = "compmsg", .mode = S_IRUGO },
		.size = 0,
		.read = mdump_compdump_binary_read,
		.write = mdump_compdump_binary_write
	},
	.vaddr = NULL,
	.paddr = NULL
};

static void check_compress_dump(void)
{
	struct compress_file_header *compresshdr = NULL;

	/*
	   No action needed if mdump is not enabled, or reboot reason is not kernel panic or
	   watchdog timeout.
	*/
	if (!s_mdump_buffer->enable_flags ||
		(s_mdump_buffer->backup_reason != MDUMP_REBOOT_PANIC &&
			s_mdump_buffer->backup_reason != MDUMP_REBOOT_WATCHDOG &&
			s_mdump_buffer->backup_reason != MDUMP_REBOOT_HARDWARE))
		return;

	/* Map the first 32MB where the compressed content stores */
	s_mdump_compdump.vaddr = remap_lowmem((phys_addr_t)COMPRESS_START_ADDRESS, (phys_addr_t)PHYMEM_MAP_CHUNK);
	if (s_mdump_compdump.vaddr == NULL) {
		pr_err("%s: Can not map physical adress 0x%x\n", __func__, COMPRESS_START_ADDRESS);
		return;
	}

	/* check the header string */
	compresshdr = (struct compress_file_header *)s_mdump_compdump.vaddr;
	if (strncmp(compresshdr->header_signature, COMP_HEAD_SIGNATURE, COMP_SIGNATURE_SIZE)) {
		pr_info("%s: The compressed content header signature is not valid!\n", __func__);
		goto error_exit;
	}
	/* check number of compressed segments and total file size. */
	if (!compresshdr->num_of_segments || (compresshdr->total_file_size <= sizeof(struct compress_file_header))) {
		pr_info("%s: No valid ccompressed conent!\n", __func__);
		goto error_exit;
	}
	/* create bin file style sysfs node */
	s_mdump_compdump.paddr = (void *) COMPRESS_START_ADDRESS;
	s_mdump_compdump.binattr.size = compresshdr->total_file_size;
	pr_info("%s: compression dump size is: 0x%lx\n", __func__, (long)s_mdump_compdump.binattr.size);
	if (sysfs_create_bin_file(&s_mdump_kobj, &s_mdump_compdump.binattr) == 0)
		return;

	pr_err("%s: Create compression dump sysfs node failed!\n", __func__);

error_exit:
	vunmap((void *)s_mdump_compdump.vaddr);
	s_mdump_compdump.vaddr = NULL;
	s_mdump_compdump.paddr = NULL;
	s_mdump_compdump.binattr.size = 0;
}
#endif /* CONFIG_MDUMP_COMPRESS */

static ssize_t mdump_text_show(struct kobject *kobj,
		struct attribute *attr,
		char *buffer)
{
	struct mdtxtattr *mdattr = (struct mdtxtattr *) attr;
	if (mdattr->show == NULL) {
		buffer[0] = 0;
		return 0;
	}
	return mdattr->show(mdattr->udata, buffer);
}

static ssize_t mdump_text_store(struct kobject *kobj,
		struct attribute *attr,
		const char *buffer, size_t size)
{
	struct mdtxtattr *mdattr = (struct mdtxtattr *) attr;
	if (mdattr->store == NULL)
		return size;
	return mdattr->store(mdattr->udata, buffer, size);
}

static ssize_t mdump_enable_show(void *ud, char *buffer)
{
	if (!s_mdump_buffer->enable_flags) {
		strcpy(buffer, "OFF");
		return 3;
	} else {
		strcpy(buffer, "ON");
		return 2;
	}
}

static ssize_t mdump_enable_store(void *ud, const char *buffer, size_t size)
{
	u16 flags = s_mdump_buffer->enable_flags;

	if ((size >= 3) && !strncmp(buffer, "OFF", 3))
		flags = 0;
	else if ((size >= 3) && !strncmp(buffer, "off", 3))
		flags = 0;
	else if ((size >= 2) && !strncmp(buffer, "ON", 2))
		flags = 1;
	else if ((size >= 2) && !strncmp(buffer, "on", 2))
		flags = 1;
	else
		pr_err("Unknown store value: %s\n", buffer);
	if (flags != s_mdump_buffer->enable_flags) {
		s_mdump_buffer->enable_flags = flags;
		mdump_dma_cache_flush_range((void*)s_mdump_buffer, sizeof(*s_mdump_buffer));
	}
	return size;
}

static ssize_t mdump_reason_show(void *ud, char *buffer)
{
	const char *mesg = 0;

	switch (s_mdump_buffer->backup_reason) {
	case MDUMP_COLD_RESET:
		mesg = "Cold reset";
		break;
	case MDUMP_REBOOT_WATCHDOG:
		mesg = "Watchdog";
		break;
	case MDUMP_REBOOT_PANIC:
		mesg = "Kernel Panic";
		break;
	case MDUMP_REBOOT_NORMAL:
		mesg = "Warm Reboot";
		break;
	case MDUMP_REBOOT_HARDWARE:
		mesg = "Hardware reset";
		break;
	default:
		mesg = "Unknown reason";
		break;
	}
	strcpy(buffer, mesg);
	return strlen(buffer);
}

static ssize_t mdump_list_show(void *ud, char *buffer)
{
	buffer[0] = 0;
	return 0;
}

static ssize_t mdump_list_store(void *ud, const char *buffer, size_t size)
{
	return size;
}

/*---------------------------------------------------------------------------*/

void mdump_mark_reboot_reason(int reason)
{
	if ((s_mdump_buffer != NULL) 
		&& (reason < s_mdump_buffer->reboot_reason)) {
		s_mdump_buffer->reboot_reason = (u8) reason;
		mdump_dma_cache_flush_range((void*)s_mdump_buffer, sizeof(*s_mdump_buffer));
	}
}

EXPORT_SYMBOL(mdump_mark_reboot_reason);

/*---------------------------------------------------------------------------*/

static struct mdbinattr s_mdump_pblmsg = {
	{
		{ .name = "pblmsg", .mode = S_IRUGO },
		.size = 0,
		.read = mdump_binary_read,
		.write = mdump_binary_write
	},
	.readonly = 1,
	.address = NULL
};
static struct mdbinattr s_mdump_lkmsg = {
	{
		{ .name = "lkmsg", .mode = S_IRUGO },
		.size = 0,
		.read = mdump_binary_read,
		.write = mdump_binary_write
	},
	1,
	NULL
};
static struct mdtxtattr s_mdump_enable = {
	{ .name = "enable", .mode = S_IRUGO | S_IWUSR },
	NULL,
	mdump_enable_show,
	mdump_enable_store
};
static struct mdtxtattr s_mdump_reason = {
	{ .name = "reason", .mode = S_IRUGO },
	NULL,
	mdump_reason_show,
	NULL
};
static struct mdtxtattr s_mdump_list = {
	{ .name = "list", .mode = S_IRUGO | S_IWUSR },
	NULL,
	mdump_list_show,
	mdump_list_store
};
static const struct sysfs_ops s_mdump_sysfs_ops = {
	.show   = mdump_text_show,
	.store  = mdump_text_store,
};
static struct attribute *s_mdump_attributes[] = {
	&s_mdump_enable.attr,
	&s_mdump_reason.attr,
	&s_mdump_list.attr,
	NULL,
};

static struct kobj_type s_mdump_type = {
	.sysfs_ops = &s_mdump_sysfs_ops,
	.default_attrs = s_mdump_attributes,
};


static int mdump_panic_notify(struct notifier_block *this, unsigned long ev, void *ptr)
{
	mdump_mark_reboot_reason(MDUMP_REBOOT_PANIC);
	return NOTIFY_DONE;
}

static struct notifier_block mdump_panic_nb = {
	.notifier_call = mdump_panic_notify,
};

static int mdump_reboot_notify(struct notifier_block *this,
				   unsigned long ev, void *unused3)
{
	mdump_mark_reboot_reason(MDUMP_REBOOT_NORMAL);
	return NOTIFY_DONE;
}

static struct notifier_block mdump_reboot_nb = {
	.notifier_call = mdump_reboot_notify,
};

static int __init mdump_init(void)
{
	pr_info("Check mdump buffer on address: 0x%x\n",
			CONFIG_MDUMP_BUFFER_ADDRESS);
	s_mdump_buffer = (struct mdump_buffer *)
			phys_to_virt(CONFIG_MDUMP_BUFFER_ADDRESS);
	if (s_mdump_buffer->signature != MDUMP_BUFFER_SIG) {
		memset(s_mdump_buffer, 0, sizeof(struct mdump_buffer));
		s_mdump_buffer->signature = MDUMP_BUFFER_SIG;
		s_mdump_buffer->backup_reason = MDUMP_COLD_RESET;
		pr_err("MDump buffer not found. initial.\n");
	} else {
		pr_info("MDump: found existing mdump_buffer\n");
		pr_info("MDump Boot Reason: 0x%x\n",
				s_mdump_buffer->backup_reason);
	}

	s_mdump_buffer->reboot_reason = MDUMP_REBOOT_HARDWARE;
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	if (get_boot_mode() != KERNEL_POWER_OFF_CHARGING_BOOT && get_boot_mode() != LOW_POWER_OFF_CHARGING_BOOT)
		s_mdump_buffer->enable_flags = 1;
	else
		s_mdump_buffer->enable_flags = 0;
#else
	s_mdump_buffer->enable_flags = 1;
#endif

	if (kobject_init_and_add(&s_mdump_kobj,
			&s_mdump_type, kernel_kobj, "mdump")) {
		kobject_put(&s_mdump_kobj);
		return -ENOMEM;
	}

	s_mdump_pblmsg.binattr.size = strlen(s_mdump_buffer->stage1_messages);
	s_mdump_pblmsg.address = s_mdump_buffer->stage1_messages;
	if (sysfs_create_bin_file(&s_mdump_kobj, &s_mdump_pblmsg.binattr) != 0)
		pr_err("Create Preloader message file failed.\n");

	s_mdump_lkmsg.binattr.size = strlen(s_mdump_buffer->stage2_messages);
	s_mdump_lkmsg.address = s_mdump_buffer->stage2_messages;
	if (sysfs_create_bin_file(&s_mdump_kobj, &s_mdump_lkmsg.binattr) != 0)
		pr_err("Create LK message file failed.\n");

#ifdef CONFIG_MDUMP_COMPRESS
	/* create sysfs node if valid compressed dump presents in RAM */
	check_compress_dump();
#endif

	atomic_notifier_chain_register(&panic_notifier_list, &mdump_panic_nb);
	register_reboot_notifier(&mdump_reboot_nb);
	return 0;
}

int mdump_reserve_memory(struct reserved_mem *rmem)
{
	pr_alert("[memblock]%s: 0x%llx - 0x%llx (0x%llx)\n", "mdump-reserve-memory",
		 (unsigned long long)rmem->base,
		 (unsigned long long)rmem->base + (unsigned long long)rmem->size,
		 (unsigned long long)rmem->size);
	return 0;
}


arch_initcall(mdump_init);

RESERVEDMEM_OF_DECLARE(reserve_memory_mdump, "amazon,mdump-reserve-memory", mdump_reserve_memory);
/*---------------------------------------------------------------------------*/
MODULE_AUTHOR("NEUSOFT");
MODULE_DESCRIPTION("NEUSOFT MDUMP MODULE");
MODULE_LICENSE("GPL");

