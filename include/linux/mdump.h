
#ifndef __LINUX_MDUMP_H__
#define __LINUX_MDUMP_H__

#define MDUMP_COLD_RESET	0x00
#define MDUMP_FORCE_RESET	0x1E
#define MDUMP_REBOOT_WATCHDOG	0x2D
#define MDUMP_REBOOT_PANIC	0x3C
#define MDUMP_REBOOT_NORMAL	0xC3
#define MDUMP_REBOOT_HARDWARE	0xE1

#ifndef CONFIG_MDUMP

static inline void mdump_mark_reboot_reason(int mode) { }

#else

#define MDUMP_BUFFER_SIG	0x4842444dU	/* MDBH */

#ifndef CONFIG_MDUMP_MESSAGE_SIZE
#define CONFIG_MDUMP_MESSAGE_SIZE	65536
#endif

#ifdef CONFIG_MDUMP_COMPRESS


#define COMP_SIGNATURE_SIZE  16
#define COMP_HEAD_SIGNATURE  "UFBLCOMPDUMPSTA"

#define COMPRESS_START_ADDRESS	0x4C000000

struct compress_file_header {
	char header_signature[COMP_SIGNATURE_SIZE];
	uint32_t num_of_segments;
	uint32_t total_file_size;
};

#endif /* CONFIG_MDUMP_COMPRESS */

struct mdump_buffer {
	unsigned int   signature;
	unsigned char  reboot_reason;
	unsigned char  backup_reason;
	unsigned short enable_flags;
	char           stage1_messages[CONFIG_MDUMP_MESSAGE_SIZE - 12];
	char           zero_pad1[4];
	char           stage2_messages[CONFIG_MDUMP_MESSAGE_SIZE - 4];
	char           zero_pad2[4];
};

extern void mdump_mark_reboot_reason(int);

#endif

#define MDUMP_HEADER_SIG 0x524448504d55444dULL	/* MDUMPHDR */

/* This block size will support all(maybe?) type block devices */
#define MDUMP_BLOCK_SIZE	4096

#define MDUMP_BANK_MAXIMUM	\
	((MDUMP_BLOCK_SIZE - 16) / sizeof(struct mdump_bank))

struct mdump_bank {
	char         name[8];
	unsigned int size; /* bank size */
	unsigned int address;
} __packed;
struct mdump_header {
	unsigned long long signature;
	unsigned char      reboot_reason;
	unsigned char      bank_count;
	unsigned short     block_size;
	unsigned int       crc32;
	struct mdump_bank  entries[MDUMP_BANK_MAXIMUM];
} __packed;

#endif /* __LINUX_MDUMP_H__ */

