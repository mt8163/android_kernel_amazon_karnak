/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __BTMTK_USB_H__
#define __BTMTK_USB_H_

/* Memory map for MTK BT */

/* SYS Control */
#define SYSCTL	0x400000

/* WLAN */
#define WLAN		0x410000

/* MCUCTL */
#define CLOCK_CTL		0x0708
#define INT_LEVEL		0x0718
#define COM_REG0		0x0730
#define SEMAPHORE_00	0x07B0
#define SEMAPHORE_01	0x07B4
#define SEMAPHORE_02	0x07B8
#define SEMAPHORE_03	0x07BC

/* Chip definition */

#define CONTROL_TIMEOUT_JIFFIES ((300 * HZ) / 100)
#define DEVICE_VENDOR_REQUEST_OUT	0x40
#define DEVICE_VENDOR_REQUEST_IN	0xc0
#define DEVICE_CLASS_REQUEST_OUT	0x20
#define DEVICE_CLASS_REQUEST_IN	    0xa0

#define BTUSB_MAX_ISOC_FRAMES	10
#define BTUSB_INTR_RUNNING	0
#define BTUSB_BULK_RUNNING	1
#define BTUSB_ISOC_RUNNING	2
#define BTUSB_SUSPENDING	3
#define BTUSB_DID_ISO_RESUME	4

/* ROM Patch */
#define PATCH_HCI_HEADER_SIZE 4
#define PATCH_WMT_HEADER_SIZE 5
#define PATCH_HEADER_SIZE (PATCH_HCI_HEADER_SIZE + PATCH_WMT_HEADER_SIZE)
#define UPLOAD_PATCH_UNIT 2048
#define PATCH_INFO_SIZE 30
#define PATCH_PHASE1 1
#define PATCH_PHASE2 2
#define PATCH_PHASE3 3

#define META_BUFFER_SIZE (1024*50)
typedef unsigned char UINT8;

typedef signed int INT32;
typedef unsigned int UINT32;

typedef unsigned long ULONG;
typedef void VOID;


/* clayton: use this */
typedef struct _OSAL_UNSLEEPABLE_LOCK_ {
	spinlock_t lock;
	ULONG flag;
} OSAL_UNSLEEPABLE_LOCK, *P_OSAL_UNSLEEPABLE_LOCK;

typedef struct {
	OSAL_UNSLEEPABLE_LOCK spin_lock;
	UINT8 buffer[META_BUFFER_SIZE];	/* MTKSTP_BUFFER_SIZE:1024 */
	UINT32 read_p;		/* indicate the current read index */
	UINT32 write_p;		/* indicate the current write index */
} ring_buffer_struct;


struct btmtk_usb_data {
	struct hci_dev *hdev;
	struct usb_device *udev;	/* store the usb device informaiton */
	struct usb_interface *intf;	/* current interface */
	struct usb_interface *isoc;	/* isochronous interface */

	spinlock_t lock;

	unsigned long flags;
	struct work_struct work;
	struct work_struct waker;

	struct task_struct *fw_dump_tsk;
	struct task_struct *fw_dump_end_check_tsk;
	struct semaphore fw_dump_semaphore;

	struct usb_anchor tx_anchor;
	struct usb_anchor intr_anchor;
	struct usb_anchor bulk_anchor;
	struct usb_anchor isoc_anchor;
	struct usb_anchor deferred;
	int tx_in_flight;

	int meta_tx;

	spinlock_t txlock;

	struct usb_endpoint_descriptor *intr_ep;
	struct usb_endpoint_descriptor *bulk_tx_ep;
	struct usb_endpoint_descriptor *bulk_rx_ep;
	struct usb_endpoint_descriptor *isoc_tx_ep;
	struct usb_endpoint_descriptor *isoc_rx_ep;

	__u8 cmdreq_type;

	unsigned int sco_num;
	int isoc_altsetting;
	int suspend_count;

	/* request for different io operation */
	u8 w_request;
	u8 r_request;

	/* io buffer for usb control transfer */
	unsigned char *io_buf;

	struct semaphore fw_upload_sem;

	unsigned char *fw_image;
	unsigned char *fw_header_image;
	unsigned char *fw_bin_file_name;

	unsigned char *rom_patch;
	unsigned char *rom_patch_header_image;
	unsigned char *rom_patch_bin_file_name;
	u32 chip_id;
	u8 need_load_fw;
	u8 need_load_rom_patch;
	u32 rom_patch_offset;
	u32 rom_patch_len;
	u32 fw_len;

	/* wake on bluetooth */
	unsigned int wobt_irq;
	int wobt_irqlevel;
	atomic_t irq_enable_count;
};

static inline int is_mt7630(struct btmtk_usb_data *data)
{
	return ((data->chip_id & 0xffff0000) == 0x76300000);
}

static inline int is_mt7650(struct btmtk_usb_data *data)
{
	return ((data->chip_id & 0xffff0000) == 0x76500000);
}

static inline int is_mt7632(struct btmtk_usb_data *data)
{
	return ((data->chip_id & 0xffff0000) == 0x76320000);
}

static inline int is_mt7662(struct btmtk_usb_data *data)
{
	return ((data->chip_id & 0xffff0000) == 0x76620000);
}

#define REV_MT76x2E3        0x0022

#define MT_REV_LT(_data, _chip, _rev)	\
	(is_##_chip(_data) && (((_data)->chip_id & 0x0000ffff) < (_rev)))

#define MT_REV_GTE(_data, _chip, _rev)	\
	(is_##_chip(_data) && (((_data)->chip_id & 0x0000ffff) >= (_rev)))

/*
 *  Load code method
 */
enum LOAD_CODE_METHOD {
	BIN_FILE_METHOD,
	HEADER_METHOD,
};

#endif
