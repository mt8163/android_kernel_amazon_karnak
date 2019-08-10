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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <linux/completion.h>
#include <linux/usb.h>
#include <linux/version.h>
#include <linux/firmware.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include "btmtk_usb.h"
#include <linux/skbuff.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/kfifo.h>  /*Used for kfifo*/


/* Used for wake on Bluetooth */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>


/* Local Configuration ========================================================= */
#define VERSION "2.0.12.8"
#define LOAD_PROFILE 1
#define BT_REDUCE_EP2_POLLING_INTERVAL_BY_INTR_TRANSFER 0
#define BT_SEND_HCI_CMD_BEFORE_SUSPEND 1
#define FW_PATCH "mt7662_patch_e3_hdr.bin"

/* stpbt device node */
#define BT_NODE "stpbt"
#define BT_DRIVER_NAME "BT_chrdev"
#define BUFFER_SIZE  (1024*4)	/* Size of RX Queue */
#define SUPPORT_FW_DUMP 1
#define IOC_MAGIC        0xb0
#define IOCTL_FW_ASSERT  _IO(IOC_MAGIC, 1)


/* ============================================================================= */

int LOAD_CODE_METHOD = BIN_FILE_METHOD;
static unsigned char *mt7662_rom_patch;

static struct usb_driver btmtk_usb_driver;

/*
#if LOAD_PROFILE
void btmtk_usb_load_profile(struct hci_dev *hdev);
#endif
*/

static char driver_version[64] = { 0 };
static char rom_patch_version[64] = { 0 };

static unsigned char probe_counter;

/* stpbt character device for meta */
static int BT_major;
struct class *pBTClass = NULL;
struct device *pBTDev = NULL;
static struct cdev BT_cdev;
static unsigned char i_buf[BUFFER_SIZE];	/* input buffer of read() */

static struct semaphore wr_mtx, rd_mtx;
static wait_queue_head_t inq;	/* read queues */
static int BT_devs = 1;		/* device count */
static volatile int metaMode;
static volatile int metaCount;
static volatile int leftHciEventSize;
static int leftACLSize;
static DECLARE_WAIT_QUEUE_HEAD(BT_wq);
static wait_queue_head_t inq;

static int hciEventMaxSize;
struct usb_device *meta_udev = NULL;
static ring_buffer_struct metabuffer;
struct hci_dev *meta_hdev = NULL;

#define SUSPEND_TIMEOUT (130*HZ)
#define WAKEUP_TIMEOUT (170*HZ)
struct btmtk_usb_data *g_data = NULL;

static int isbtready;
static int isUsbDisconnet;
static int isFwAssert;


static int BT_init(void);
static void BT_exit(void);

INT32 lock_unsleepable_lock(P_OSAL_UNSLEEPABLE_LOCK pUSL)
{
	spin_lock_irqsave(&(pUSL->lock), pUSL->flag);
	return 0;
}

INT32 unlock_unsleepable_lock(P_OSAL_UNSLEEPABLE_LOCK pUSL)
{
	spin_unlock_irqrestore(&(pUSL->lock), pUSL->flag);
	return 0;
}

VOID *osal_memcpy(VOID *dst, const VOID *src, UINT32 len)
{
	return memcpy(dst, src, len);
}

static int btmtk_usb_reset(struct usb_device *udev)
{
	int ret;

	pr_info("%s\n", __func__);

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x01, DEVICE_VENDOR_REQUEST_OUT,
			      0x01, 0x00, NULL, 0x00, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		pr_err("%s error(%d)\n", __func__, ret);
		return ret;
	}

	if (ret > 0)
		ret = 0;

	return ret;
}

static int btmtk_usb_io_read32(struct btmtk_usb_data *data, u32 reg, u32 *val)
{
	u8 request = data->r_request;
	struct usb_device *udev = data->udev;
	int ret;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0), request, DEVICE_VENDOR_REQUEST_IN,
			      0x0, reg, data->io_buf, 4, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		*val = 0xffffffff;
		pr_err("%s error(%d), reg=%x, value=%x\n", __func__, ret, reg, *val);
		return ret;
	}

	memmove(val, data->io_buf, 4);

	*val = le32_to_cpu(*val);

	if (ret > 0)
		ret = 0;

	return ret;
}

static int btmtk_usb_io_write32(struct btmtk_usb_data *data, u32 reg, u32 val)
{
	u16 value, index;
	u8 request = data->w_request;
	struct usb_device *udev = data->udev;
	int ret;

	index = (u16) reg;
	value = val & 0x0000ffff;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), request, DEVICE_VENDOR_REQUEST_OUT,
			      value, index, NULL, 0, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		pr_err("%s error(%d), reg=%x, value=%x\n", __func__, ret, reg, val);
		return ret;
	}

	index = (u16) (reg + 2);
	value = (val & 0xffff0000) >> 16;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), request, DEVICE_VENDOR_REQUEST_OUT,
			      value, index, NULL, 0, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		pr_err("%s error(%d), reg=%x, value=%x\n", __func__, ret, reg, val);
		return ret;
	}

	if (ret > 0)
		ret = 0;

	return ret;
}


#if SUPPORT_FW_DUMP
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/kthread.h>

#define FW_DUMP_FILE_NAME "/data/bt_fw_dump"
#define FW_DUMP_BUF_SIZE (1024*512)
static struct file *fw_dump_file;
static char fw_dump_file_name[64] = {0};
int fw_dump_total_read_size = 0;
int fw_dump_total_write_size = 0;
int fw_dump_buffer_used_size = 0;
int fw_dump_buffer_full = 0;
int fw_dump_task_should_stop = 0;
u8 *fw_dump_ptr = NULL;
u8 *fw_dump_read_ptr = NULL;
u8 *fw_dump_write_ptr = NULL;
struct timeval fw_dump_last_write_time;

int fw_dump_end_checking_task_should_stop = 0;
struct urb *fw_dump_bulk_urb = NULL;

static int btmtk_usb_fw_dump_end_checking_thread(void *param)
{
	struct timeval t;

	while (1) {
		mdelay(1000);

		if (kthread_should_stop() || fw_dump_end_checking_task_should_stop)
			return -ERESTARTSYS;

		pr_warn("%s:FW dump on going ... %d:%d\n", "btmtk_usb",
			fw_dump_total_read_size, fw_dump_total_write_size);

		do_gettimeofday(&t);
		if ((t.tv_sec-fw_dump_last_write_time.tv_sec) > 1) {
			pr_warn("%s : fw dump total read size = %d\n", "btmtk_usb", fw_dump_total_read_size);
			pr_warn("%s : fw dump total write size = %d\n", "btmtk_usb", fw_dump_total_write_size);
			if (fw_dump_file) {
				vfs_fsync(fw_dump_file, 0);
				pr_warn("%s : close file %s\n", __func__, fw_dump_file_name);
				filp_close(fw_dump_file, NULL);
				fw_dump_file = NULL;
				isFwAssert = 0;
			}
			fw_dump_end_checking_task_should_stop = 1;
		}
	}
	return 0;
}

static int btmtk_usb_fw_dump_thread(void *param)
{
	struct btmtk_usb_data *data = param;
	mm_segment_t old_fs;
	int len;

	pr_warn("%s\n", __func__);

	while (down_interruptible(&data->fw_dump_semaphore) == 0) {
		if (kthread_should_stop() || fw_dump_task_should_stop) {
			if (fw_dump_file) {
				isFwAssert = 0;
				vfs_fsync(fw_dump_file, 0);
				pr_warn("%s : close file  %s\n", __func__, fw_dump_file_name);
				filp_close(fw_dump_file, NULL);
				fw_dump_file = NULL;
				pr_warn("%s : fw dump total read size = %d\n", "btmtk_usb", fw_dump_total_read_size);
				pr_warn("%s : fw dump total write size = %d\n", "btmtk_usb", fw_dump_total_write_size);
			}
			return -ERESTARTSYS;
		}

		len = fw_dump_write_ptr - fw_dump_read_ptr;

		if (len > 0 && fw_dump_read_ptr != NULL) {
			old_fs = get_fs();
			set_fs(KERNEL_DS);

			if (fw_dump_file == NULL) {
				memset(fw_dump_file_name, 0, sizeof(fw_dump_file_name));
				snprintf(fw_dump_file_name, sizeof(fw_dump_file_name),
					FW_DUMP_FILE_NAME"_%d", probe_counter);
				pr_warn("%s : open file %s\n", __func__, fw_dump_file_name);
				fw_dump_file = filp_open(fw_dump_file_name, O_RDWR | O_CREAT, 0644);
				if (IS_ERR(fw_dump_file)) {
					pr_warn("%s : open file error:%s.\n", __func__, fw_dump_file_name);
					set_fs(old_fs);
					return 0;
				}
			}

			if (fw_dump_file != NULL) {
				fw_dump_file->f_op->write(fw_dump_file, fw_dump_read_ptr, len, &fw_dump_file->f_pos);
				fw_dump_read_ptr += len;
				fw_dump_total_write_size += len;
				do_gettimeofday(&fw_dump_last_write_time);
			}
			set_fs(old_fs);
		}

		if (fw_dump_buffer_full && fw_dump_write_ptr == fw_dump_read_ptr) {
			int err;

			fw_dump_buffer_full = 0;
			fw_dump_buffer_used_size = 0;
			fw_dump_read_ptr = fw_dump_ptr;
			fw_dump_write_ptr = fw_dump_ptr;
			pr_warn("btmtk:buffer is full\n");

			err = usb_submit_urb(fw_dump_bulk_urb, GFP_ATOMIC);
			if (err < 0) {
				/* -EPERM: urb is being killed;
				* -ENODEV: device got disconnected */
				if (err != -EPERM && err != -ENODEV)
					pr_warn("%s:urb %p failed,resubmit bulk_in_urb(%d)",
						__func__, fw_dump_bulk_urb, -err);
				usb_unanchor_urb(fw_dump_bulk_urb);
			}
		}

		if (data->fw_dump_end_check_tsk == NULL) {
			fw_dump_end_checking_task_should_stop = 0;
			data->fw_dump_end_check_tsk = kthread_create(btmtk_usb_fw_dump_end_checking_thread,
				(void *)data, "btmtk_usb_fw_dump_end_checking_thread");
			if (IS_ERR(data->fw_dump_end_check_tsk)) {
				pr_warn("%s : create fw dump end check thread failed!\n", __func__);
				data->fw_dump_end_check_tsk = NULL;
			} else
				wake_up_process(data->fw_dump_end_check_tsk);
		}
	}

	pr_warn("%s end : down != 0\n", __func__);

	return 0;
}
#endif

static int btmtk_usb_send_hci_suspend_cmd(struct usb_device *udev)
{
	int ret = 0;
	char buf[8] = { 0 };

	buf[0] = 0x6F;
	buf[1] = 0xFC;
	buf[2] = 0x05;
	buf[3] = 0x01;
	buf[4] = 0x03;
	buf[5] = 0x01;
	buf[6] = 0x00;
	buf[7] = 0x08;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0, DEVICE_CLASS_REQUEST_OUT,
			      0x00, 0x00, buf, 0x08, 100);

	if (ret < 0) {
		pr_err("%s error1(%d)\n", __func__, ret);
		return ret;
	}
	pr_warn("new btmtk_usb_send_hci_suspend_cmd : OK\n");

	/* delay to receive event */
	mdelay(20);

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0), 0x01,
		DEVICE_VENDOR_REQUEST_IN, 0x30, 0x00, buf, 8,
		CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0)
		pr_err("%s Err2(%d)\n", __func__, ret);


	pr_warn("ret:0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);

		if (buf[0] == 0xE4 &&
			buf[1] == 0x06 &&
			buf[2] == 0x02 &&
			buf[3] == 0x03 &&
			buf[4] == 0x02 && buf[5] == 0x00 && buf[6] == 0x00 && buf[7] == 0x08) {
			pr_warn("Check wmt suspend event result : OK\n");
		} else {
			pr_warn("Check wmt suspend event result : NG\n");
		}

	return 0;
}

static int btmtk_usb_send_hci_wake_up_cmd(struct usb_device *udev)
{
	int ret;
	char buf[8] = { 0 };

	pr_warn("btmtk_usb_send_hci_wake_up_cmd\n");

	buf[0] = 0x6F;
	buf[1] = 0xFC;
	buf[2] = 0x05;
	buf[3] = 0x01;
	buf[4] = 0x03;
	buf[5] = 0x01;
	buf[6] = 0x00;
	buf[7] = 0x09;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0, DEVICE_CLASS_REQUEST_OUT,
			      0x00, 0x00, buf, 0x08, 100);

	if (ret < 0) {
		pr_err("%s error1(%d)\n", __func__, ret);
		return ret;
	}
	pr_warn("btmtk_usb_send_hci_wake_up_cmd : OK\n");


	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0), 0x01,
		DEVICE_VENDOR_REQUEST_IN, 0x30, 0x00, buf, 8,
		CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0)
		pr_err("%s Err2(%d)\n", __func__, ret);


	pr_warn("ret:0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);

		if (buf[0] == 0xE4 &&
			buf[1] == 0x06 &&
			buf[2] == 0x02 &&
			buf[3] == 0x03 &&
			buf[4] == 0x02 && buf[5] == 0x00 && buf[6] == 0x00 && buf[7] == 0x09) {
			pr_warn("Check wmt wake up event result : OK\n");
		} else {
			pr_warn("Check wmt wake up event result : NG\n");
		}

	return 0;
}

static int btmtk_usb_send_assert_cmd(struct usb_device *udev)
{
	int ret = 0;
	char buf[8] = { 0 };

	pr_warn("%s\n", __func__);
	buf[0] = 0x6F;
	buf[1] = 0xFC;
	buf[2] = 0x05;
	buf[3] = 0x01;
	buf[4] = 0x02;
	buf[5] = 0x01;
	buf[6] = 0x00;
	buf[7] = 0x08;
	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0, DEVICE_CLASS_REQUEST_OUT,
			0x00, 0x00, buf, 0x08, 100);
	if (ret < 0) {
		pr_err("%s error1(%d)\n", __func__, ret);
		return ret;
	}
	pr_warn("btmtk_usb_send_assert_cmd : OK\n");
	return 0;
}

static int btmtk_usb_send_radio_on_cmd(struct usb_device *udev)
{
	int retry_counter = 0;
	{
		int ret = 0;
		char buf[5] = { 0 };

		pr_warn("%s\n", __func__);
		buf[0] = 0xC9;
		buf[1] = 0xFC;
		buf[2] = 0x02;
		buf[3] = 0x01;
		buf[4] = 0x01;
		ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0, DEVICE_CLASS_REQUEST_OUT,
				      0x00, 0x00, buf, 0x05, 100);

		if (ret < 0) {
			pr_err("%s error1(%d)\n", __func__, ret);
			return ret;
		}
	}

	/* Get response*/
	{
		while (1) {
			int ret = 0;
			char buf[64] = { 0 };
			int actual_length;
			int correct_response = 0;

			ret = usb_interrupt_msg(udev, usb_rcvintpipe(udev, 1),
						buf, 64, &actual_length, 2000);

			if (ret < 0) {
				pr_err("%s error2(%d)\n", __func__, ret);
				return ret;
			}

			if (buf[0] == 0xE6 &&
			    buf[1] == 0x02 &&
			    buf[2] == 0x08 && buf[3] == 0x01) {
				retry_counter++;
				correct_response = 1;
			} else {
				int i;

				pr_warn("btmtk:unknown radio on event :\n");

				for (i = 0; i < actual_length && i < 64; i++)
					pr_warn("%02X ", buf[i]);

				pr_warn("\n");
				mdelay(10);
				retry_counter++;
			}

			if (correct_response)
				break;

			if (retry_counter > 10) {
				pr_warn("%s retry timeout!\n", __func__);
				return ret;
			}
		}
	}

	pr_warn("%s:OK\n", __func__);
	return 0;
}


static int btmtk_usb_send_radio_off_cmd(struct usb_device *udev)
{
	int retry_counter = 0;
	{
		int ret = 0;
		char buf[5] = { 0 };

		pr_warn("%s\n", __func__);
		buf[0] = 0xC9;
		buf[1] = 0xFC;
		buf[2] = 0x02;
		buf[3] = 0x01;
		buf[4] = 0x00;
		ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0, DEVICE_CLASS_REQUEST_OUT,
				      0x00, 0x00, buf, 0x05, 100);

		if (ret < 0) {
			pr_err("%s error1(%d)\n", __func__, ret);
			return ret;
		}
	}

	/* Get response*/
	{
		while (1) {
			int ret = 0;
			char buf[64] = { 0 };
			int actual_length;
			int correct_response = 0;

			ret = usb_interrupt_msg(udev, usb_rcvintpipe(udev, 1),
						buf, 64, &actual_length, 2000);

			if (ret < 0) {
				pr_err("%s error2(%d)\n", __func__, ret);
				return ret;
			}

			if (buf[0] == 0xE6 &&
				buf[1] == 0x02 &&
				buf[2] == 0x08 && buf[3] == 0x00) {
				retry_counter++;
				correct_response = 1;
			} else {
				int i;

				pr_warn("btmtk:unknown radio off event :\n");

				for (i = 0; i < actual_length && i < 64; i++)
					pr_warn("%02X ", buf[i]);

				retry_counter++;
			}
			pr_warn("\n");

			if (correct_response)
				break;

			if (retry_counter > 10) {
				pr_warn("%s retry timeout!\n", __func__);
				return ret;
			}
		}
	}

	pr_warn("%s:OK\n", __func__);
	return 0;
}


static int btmtk_usb_send_hci_reset_cmd(struct usb_device *udev)
{
	int retry_counter = 0;
	/* Send HCI Reset */
	{
		int ret = 0;
		char buf[4] = { 0 };

		buf[0] = 0x03;
		buf[1] = 0x0C;
		ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0, DEVICE_CLASS_REQUEST_OUT,
				      0x00, 0x00, buf, 0x03, 100);

		if (ret < 0) {
			pr_err("%s error1(%d)\n", __func__, ret);
			return ret;
		}
	}

	/* Get response of HCI reset */
	{
		while (1) {
			int ret = 0;
			char buf[64] = { 0 };
			int actual_length;
			int correct_response = 0;

			ret = usb_interrupt_msg(udev, usb_rcvintpipe(udev, 1),
						buf, 64, &actual_length, 2000);

			if (ret < 0) {
				pr_err("%s error2(%d)\n", __func__, ret);
				return ret;
			}

			if (actual_length == 6 &&
			    buf[0] == 0x0e &&
			    buf[1] == 0x04 &&
			    buf[2] == 0x01 && buf[3] == 0x03 && buf[4] == 0x0c && buf[5] == 0x00) {
				correct_response = 1;
			} else {
				int i;

				pr_warn("%s drop unknown event :\n", __func__);
				for (i = 0; i < actual_length && i < 64; i++)
					pr_warn("%02X ", buf[i]);

				pr_warn("\n");
				mdelay(10);
				retry_counter++;
			}

			if (correct_response)
				break;

			if (retry_counter > 10) {
				pr_warn("%s retry timeout!\n", __func__);
				return ret;
			}
		}
	}

	pr_warn("btmtk_usb_send_hci_reset_cmd : OK\n");
	return 0;
}


static int btmtk_usb_send_hci_set_ce_cmd(struct usb_device *udev)
{
	char result_buf[64] = { 0 };

	pr_warn("send hci reset cmd");

	/* Send HCI Reset, read 0x41070c */
	{
		int ret = 0;
		char buf[8] = { 0xd1, 0xFC, 0x04, 0x0c, 0x07, 0x41, 0x00 };

		ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0, DEVICE_CLASS_REQUEST_OUT,
				      0x00, 0x00, buf, 0x07, 100);

		if (ret < 0) {
			pr_err("%s error1(%d)\n", __func__, ret);
			return ret;
		}
	}

	/* Get response of HCI reset */
	{
		int ret = 0;
		int actual_length;

		ret = usb_interrupt_msg(udev, usb_rcvintpipe(udev, 1),
					result_buf, 64, &actual_length, 2000);

		if (ret < 0) {
			pr_err("%s error2(%d)\n", __func__, ret);
			return ret;
		}

		if (result_buf[6] & 0x01)
			pr_warn("warning, 0x41070c[0] is 1!\n");
	}

	/* Send HCI Reset, write 0x41070c[0] to 1 */
	{
		int ret = 0;
		char buf[12] = { 0xd0, 0xfc, 0x08, 0x0c, 0x07, 0x41, 0x00 };

		buf[7] = result_buf[6] | 0x01;
		buf[8] = result_buf[7];
		buf[9] = result_buf[8];
		buf[10] = result_buf[9];
		ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0, DEVICE_CLASS_REQUEST_OUT,
				      0x00, 0x00, buf, 0x0b, 100);

		if (ret < 0) {
			pr_err("%s error1(%d)\n", __func__, ret);
			return ret;
		}
	}

	pr_warn("btmtk_usb_send_hci_set_ce_cmd : OK\n");
	return 0;
}


static int btmtk_usb_send_check_rom_patch_result_cmd(struct usb_device *udev)
{
	/* Send HCI Reset */
	{
		int ret = 0;
		unsigned char buf[8] = { 0 };

		buf[0] = 0xD1;
		buf[1] = 0xFC;
		buf[2] = 0x04;
		buf[3] = 0x00;
		buf[4] = 0xE2;
		buf[5] = 0x40;
		buf[6] = 0x00;

		ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0, DEVICE_CLASS_REQUEST_OUT,
				      0x00, 0x00, buf, 0x07, 100);

		if (ret < 0) {
			pr_err("%s error1(%d)\n", __func__, ret);
			return ret;
		}
	}

	/* Get response of HCI reset */
	{
		int ret = 0;
		unsigned char buf[64] = { 0 };
		int actual_length;

		ret = usb_interrupt_msg(udev, usb_rcvintpipe(udev, 1),
					buf, 64, &actual_length, 2000);

		if (ret < 0) {
			pr_err("%s error2(%d)\n", __func__, ret);
			return ret;
		}
		pr_warn("Check rom patch result : ");

		if (buf[6] == 0 && buf[7] == 0 && buf[8] == 0 && buf[9] == 0)
			pr_warn("NG\n");
		else
			pr_warn("OK\n");
	}

	return 0;
}

static int btmtk_usb_switch_iobase(struct btmtk_usb_data *data, int base)
{
	int ret = 0;

	switch (base) {
	case SYSCTL:
		data->w_request = 0x42;
		data->r_request = 0x47;
		break;
	case WLAN:
		data->w_request = 0x02;
		data->r_request = 0x07;
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static void btmtk_usb_cap_init(struct btmtk_usb_data *data)
{
	btmtk_usb_io_read32(data, 0x00, &data->chip_id);

	pr_warn("chip id = %x\n", data->chip_id);

	if (is_mt7630(data) || is_mt7650(data)) {
		data->need_load_fw = 1;
		data->need_load_rom_patch = 0;
		data->fw_header_image = NULL;
		data->fw_bin_file_name = "mtk/mt7650.bin";
		data->fw_len = 0;
	} else if (is_mt7632(data) || is_mt7662(data)) {
		data->need_load_fw = 0;
		data->need_load_rom_patch = 1;
		if (LOAD_CODE_METHOD == HEADER_METHOD) {
			data->rom_patch_header_image = mt7662_rom_patch;
			data->rom_patch_len = sizeof(mt7662_rom_patch);
			data->rom_patch_offset = 0x90000;
		} else {
			data->rom_patch_bin_file_name = kmalloc(32, GFP_ATOMIC);
			if (!data->rom_patch_bin_file_name) {
				pr_err("btmtk_usb_cap_init(): Can't allocate memory (32)\n");
				return;
			}
			memset(data->rom_patch_bin_file_name, 0, 32);

			if ((data->chip_id & 0xf) < 0x2) {
				pr_warn("chip_id < 0x2\n");
				memcpy(data->rom_patch_bin_file_name, FW_PATCH, 23);
				/* memcpy(data->rom_patch_bin_file_name, "mt7662_patch_e1_hdr.bin", 23); */
			} else {
				pr_warn("chip_id >= 0x2\n");
				pr_warn("patch bin file name:%s\n", FW_PATCH);
				memcpy(data->rom_patch_bin_file_name, FW_PATCH, 23);
			}

			data->rom_patch_offset = 0x90000;
			data->rom_patch_len = 0;
		}
	} else
		pr_err("unknown chip(%x)\n", data->chip_id);
}

u16 checksume16(u8 *pData, int len)
{
	int sum = 0;

	while (len > 1) {
		sum += *((u16 *) pData);

		pData = pData + 2;

		if (sum & 0x80000000)
			sum = (sum & 0xFFFF) + (sum >> 16);

		len -= 2;
	}

	if (len)
		sum += *((u8 *) pData);

	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ~sum;
}

static int btmtk_usb_chk_crc(struct btmtk_usb_data *data, u32 checksum_len)
{
	int ret = 0;
	struct usb_device *udev = data->udev;

	BT_DBG("%s", __func__);

	memmove(data->io_buf, &data->rom_patch_offset, 4);
	memmove(&data->io_buf[4], &checksum_len, 4);

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x1, DEVICE_VENDOR_REQUEST_OUT,
			      0x20, 0x00, data->io_buf, 8, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0)
		pr_err("%s error(%d)\n", __func__, ret);

	return ret;
}

static u16 btmtk_usb_get_crc(struct btmtk_usb_data *data)
{
	int ret = 0;
	struct usb_device *udev = data->udev;
	u16 crc, count = 0;

	BT_DBG("%s", __func__);

	while (1) {
		ret =
		    usb_control_msg(udev, usb_rcvctrlpipe(udev, 0), 0x01, DEVICE_VENDOR_REQUEST_IN,
				    0x21, 0x00, data->io_buf, 2, CONTROL_TIMEOUT_JIFFIES);

		if (ret < 0) {
			crc = 0xFFFF;
			pr_err("%s error(%d)\n", __func__, ret);
		}

		memmove(&crc, data->io_buf, 2);

		crc = le16_to_cpu(crc);

		if (crc != 0xFFFF)
			break;

		mdelay(100);

		if (count++ > 100) {
			pr_err("Query CRC over %d times\n", count);
			break;
		}
	}

	return crc;
}

static int btmtk_usb_reset_wmt(struct btmtk_usb_data *data)
{
	int ret = 0;

	/* reset command */
	u8 cmd[9] = { 0x6F, 0xFC, 0x05, 0x01, 0x07, 0x01, 0x00, 0x04 };

	memmove(data->io_buf, cmd, 8);


	ret = usb_control_msg(data->udev, usb_sndctrlpipe(data->udev, 0), 0x01,
			      DEVICE_CLASS_REQUEST_OUT, 0x30, 0x00, data->io_buf, 8,
			      CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		pr_err("%s:Err1(%d)\n", __func__, ret);
		return ret;
	}

	mdelay(20);

	ret = usb_control_msg(data->udev, usb_rcvctrlpipe(data->udev, 0), 0x01,
			      DEVICE_VENDOR_REQUEST_IN, 0x30, 0x00, data->io_buf, 7,
			      CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0)
		pr_err("%s Err2(%d)\n", __func__, ret);

	if (data->io_buf[0] == 0xe4 &&
	    data->io_buf[1] == 0x05 &&
	    data->io_buf[2] == 0x02 &&
	    data->io_buf[3] == 0x07 &&
	    data->io_buf[4] == 0x01 && data->io_buf[5] == 0x00 && data->io_buf[6] == 0x00) {
		pr_warn("Check reset wmt result : OK\n");
	} else
		pr_warn("Check reset wmt result : NG\n");

	return ret;
}

static u16 btmtk_usb_get_rom_patch_result(struct btmtk_usb_data *data)
{
	int ret = 0;

	ret = usb_control_msg(data->udev, usb_rcvctrlpipe(data->udev, 0), 0x01,
			      DEVICE_VENDOR_REQUEST_IN, 0x30, 0x00, data->io_buf, 7,
			      CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0)
		pr_warn("%s error(%d)\n", __func__, ret);

	if (data->io_buf[0] == 0xe4 &&
	    data->io_buf[1] == 0x05 &&
	    data->io_buf[2] == 0x02 &&
	    data->io_buf[3] == 0x01 &&
	    data->io_buf[4] == 0x01 && data->io_buf[5] == 0x00 && data->io_buf[6] == 0x00) {
		pr_warn("Get rom patch result : OK\n");
	} else {
		pr_warn("Get rom patch result : NG\n");
	}

	return ret;
}

static void load_code_from_bin(unsigned char **image, char *bin_name, struct device *dev,
			       u32 *code_len)
{
	const struct firmware *fw_entry;

	if (request_firmware(&fw_entry, bin_name, dev) != 0) {
		*image = NULL;
		return;
	}

	*image = kmalloc(fw_entry->size, GFP_ATOMIC);
	memcpy(*image, fw_entry->data, fw_entry->size);
	*code_len = fw_entry->size;

	release_firmware(fw_entry);
}

static void load_rom_patch_complete(struct urb *urb)
{

	struct completion *sent_to_mcu_done = (struct completion *)urb->context;

	complete(sent_to_mcu_done);
}

static int btmtk_usb_load_rom_patch(struct btmtk_usb_data *data)
{
	u32 loop = 0;
	u32 value;
	s32 sent_len;
	int ret = 0, total_checksum = 0;
	struct urb *urb;
	u32 patch_len = 0;
	u32 cur_len = 0;
	dma_addr_t data_dma;
	struct completion sent_to_mcu_done;
	int first_block = 1;
	unsigned char phase;
	void *buf;
	char *pos;
	char *tmp_str;
	unsigned int pipe = usb_sndbulkpipe(data->udev,
					    data->bulk_tx_ep->bEndpointAddress);

	pr_warn("btmtk_usb_load_rom_patch begin\n");
 load_patch_protect:
	btmtk_usb_switch_iobase(data, WLAN);
	btmtk_usb_io_read32(data, SEMAPHORE_03, &value);
	loop++;

	if ((value & 0x01) == 0x00) {
		if (loop < 1000) {
			mdelay(1);
			goto load_patch_protect;
		} else {
			pr_err("btmtk_usb_load_rom_patch ERR! Can't get semaphore! Continue\n");
		}
	}

	btmtk_usb_switch_iobase(data, SYSCTL);

	btmtk_usb_io_write32(data, 0x1c, 0x30);

	btmtk_usb_switch_iobase(data, WLAN);

	/* check ROM patch if upgrade */
	if ((MT_REV_GTE(data, mt7662, REV_MT76x2E3)) || (MT_REV_GTE(data, mt7632, REV_MT76x2E3))) {
		btmtk_usb_io_read32(data, CLOCK_CTL, &value);
		if ((value & 0x01) == 0x01) {
			pr_warn("btmtk_usb_load_rom_patch : no need to load rom patch\n");
			btmtk_usb_send_hci_reset_cmd(data->udev);
			goto error0;
		}
	} else {
		btmtk_usb_io_read32(data, COM_REG0, &value);
		if ((value & 0x02) == 0x02) {
			pr_warn("btmtk_usb_load_rom_patch : no need to load rom patch\n");
			btmtk_usb_send_hci_reset_cmd(data->udev);
			goto error0;
		}
	}

	urb = usb_alloc_urb(0, GFP_ATOMIC);

	if (!urb) {
		ret = -ENOMEM;
		goto error0;
	}

	buf = usb_alloc_coherent(data->udev, UPLOAD_PATCH_UNIT, GFP_ATOMIC, &data_dma);

	if (!buf) {
		ret = -ENOMEM;
		goto error1;
	}

	pos = buf;

	if (LOAD_CODE_METHOD == BIN_FILE_METHOD) {
		load_code_from_bin(&data->rom_patch, data->rom_patch_bin_file_name,
				   &data->udev->dev, &data->rom_patch_len);
		pr_warn("LOAD_CODE_METHOD : BIN_FILE_METHOD\n");
	} else {
		data->rom_patch = data->rom_patch_header_image;
		pr_warn("LOAD_CODE_METHOD : HEADER_METHOD\n");
	}

	if (!data->rom_patch) {
		if (LOAD_CODE_METHOD == BIN_FILE_METHOD) {
			pr_err
			    ("%s:please assign a rom patch(/etc/firmware/%s)or(/lib/firmware/%s)\n",
			     __func__, data->rom_patch_bin_file_name,
			     data->rom_patch_bin_file_name);
		} else {
			pr_err("%s:please assign a rom patch\n", __func__);
		}

		ret = -1;
		goto error2;
	}

	tmp_str = data->rom_patch;
	pr_warn("FW Version = %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
	       tmp_str[0], tmp_str[1], tmp_str[2], tmp_str[3],
	       tmp_str[4], tmp_str[5], tmp_str[6], tmp_str[7],
	       tmp_str[8], tmp_str[9], tmp_str[10], tmp_str[11],
	       tmp_str[12], tmp_str[13], tmp_str[14], tmp_str[15]);

	pr_warn("build Time = %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
	       tmp_str[0], tmp_str[1], tmp_str[2], tmp_str[3],
	       tmp_str[4], tmp_str[5], tmp_str[6], tmp_str[7],
	       tmp_str[8], tmp_str[9], tmp_str[10], tmp_str[11],
	       tmp_str[12], tmp_str[13], tmp_str[14], tmp_str[15]);

	memset(rom_patch_version, 0, sizeof(rom_patch_version));
	memcpy(rom_patch_version, tmp_str, 15);

	tmp_str = data->rom_patch + 16;
	pr_warn("platform = %c%c%c%c\n", tmp_str[0], tmp_str[1], tmp_str[2], tmp_str[3]);

	pr_warn("\nloading rom patch...\n");

	init_completion(&sent_to_mcu_done);

	cur_len = 0x00;
	patch_len = data->rom_patch_len - PATCH_INFO_SIZE;

	/* loading rom patch */
	while (1) {
		s32 sent_len_max = UPLOAD_PATCH_UNIT - PATCH_HEADER_SIZE;

		sent_len =
		    (patch_len - cur_len) >= sent_len_max ? sent_len_max : (patch_len - cur_len);

		pr_warn("patch_len = %d\n", patch_len);
		pr_warn("cur_len = %d\n", cur_len);
		pr_warn("sent_len = %d\n", sent_len);

		if (sent_len > 0) {
			if (first_block == 1) {
				if (sent_len < sent_len_max)
					phase = PATCH_PHASE3;
				else
					phase = PATCH_PHASE1;
				first_block = 0;
			} else if (sent_len == sent_len_max) {
				phase = PATCH_PHASE2;
			} else {
				phase = PATCH_PHASE3;
			}

			/* prepare HCI header */
			pos[0] = 0x6F;
			pos[1] = 0xFC;
			pos[2] = (sent_len + 5) & 0xFF;
			pos[3] = ((sent_len + 5) >> 8) & 0xFF;

			/* prepare WMT header */
			pos[4] = 0x01;
			pos[5] = 0x01;
			pos[6] = (sent_len + 1) & 0xFF;
			pos[7] = ((sent_len + 1) >> 8) & 0xFF;

			pos[8] = phase;

			memcpy(&pos[9], data->rom_patch + PATCH_INFO_SIZE + cur_len, sent_len);

			pr_warn("sent_len + PATCH_HEADER_SIZE = %d, phase = %d\n",
			       sent_len + PATCH_HEADER_SIZE, phase);

			usb_fill_bulk_urb(urb,
					  data->udev,
					  pipe,
					  buf,
					  sent_len + PATCH_HEADER_SIZE,
					  load_rom_patch_complete, &sent_to_mcu_done);

			urb->transfer_dma = data_dma;
			urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

			ret = usb_submit_urb(urb, GFP_ATOMIC);

			if (ret)
				goto error2;

			if (!wait_for_completion_timeout(&sent_to_mcu_done, msecs_to_jiffies(1000))) {
				usb_kill_urb(urb);
				pr_warn("upload rom_patch timeout\n");
				goto error2;
			}

			mdelay(1);

			cur_len += sent_len;

		} else {
			break;
		}
	}

	mdelay(20);
	ret = btmtk_usb_get_rom_patch_result(data);
	mdelay(20);

	/* Send Checksum request */
	total_checksum = checksume16(data->rom_patch + PATCH_INFO_SIZE, patch_len);
	btmtk_usb_chk_crc(data, patch_len);

	mdelay(20);

	if (total_checksum != btmtk_usb_get_crc(data)) {
		pr_err("checksum fail!, local(0x%x) <> fw(0x%x)\n", total_checksum,
		       btmtk_usb_get_crc(data));
		ret = -1;
		goto error2;
	}

	mdelay(20);
	/* send check rom patch result request */
	btmtk_usb_send_check_rom_patch_result_cmd(data->udev);
	mdelay(20);
	/* CHIP_RESET */
	ret = btmtk_usb_reset_wmt(data);
	mdelay(20);
	/* BT_RESET */
	btmtk_usb_send_hci_reset_cmd(data->udev);

	/* for WoBLE/WoW low power */
	btmtk_usb_send_hci_set_ce_cmd(data->udev);
 error2:
	usb_free_coherent(data->udev, UPLOAD_PATCH_UNIT, buf, data_dma);
 error1:
	usb_free_urb(urb);
 error0:
	btmtk_usb_io_write32(data, SEMAPHORE_03, 0x1);
	pr_warn("btmtk_usb_load_rom_patch end\n");
	return ret;
}


static int load_fw_iv(struct btmtk_usb_data *data)
{
	int ret;
	struct usb_device *udev = data->udev;
	char *buf = kmalloc(64, GFP_ATOMIC);

	memmove(buf, data->fw_image + 32, 64);

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x01,
			      DEVICE_VENDOR_REQUEST_OUT, 0x12, 0x0, buf, 64,
			      CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		pr_warn("%s error(%d) step4\n", __func__, ret);
		kfree(buf);
		return ret;
	}

	if (ret > 0)
		ret = 0;

	kfree(buf);

	return ret;
}

static void load_fw_complete(struct urb *urb)
{

	struct completion *sent_to_mcu_done = (struct completion *)urb->context;

	complete(sent_to_mcu_done);
}

static int btmtk_usb_load_fw(struct btmtk_usb_data *data)
{
	struct usb_device *udev = data->udev;
	struct urb *urb;
	void *buf;
	u32 cur_len = 0;
	u32 packet_header = 0;
	u32 value;
	u32 ilm_len = 0, dlm_len = 0;
	u16 fw_ver, build_ver;
	u32 loop = 0;
	dma_addr_t data_dma;
	int ret = 0, sent_len;
	struct completion sent_to_mcu_done;
	unsigned int pipe = usb_sndbulkpipe(data->udev,
					    data->bulk_tx_ep->bEndpointAddress);

	pr_warn("bulk_tx_ep = %x\n", data->bulk_tx_ep->bEndpointAddress);

 loadfw_protect:
	btmtk_usb_switch_iobase(data, WLAN);
	btmtk_usb_io_read32(data, SEMAPHORE_00, &value);
	loop++;

	if (((value & 0x1) == 0) && (loop < 10000))
		goto loadfw_protect;

	/* check MCU if ready */
	btmtk_usb_io_read32(data, COM_REG0, &value);

	if ((value & 0x01) == 0x01)
		goto error0;

	/* Enable MPDMA TX and EP2 load FW mode */
	btmtk_usb_io_write32(data, 0x238, 0x1c000000);

	btmtk_usb_reset(udev);
	mdelay(100);

	if (LOAD_CODE_METHOD == BIN_FILE_METHOD) {
		load_code_from_bin(&data->fw_image, data->fw_bin_file_name, &data->udev->dev,
				   &data->fw_len);
		pr_warn("LOAD_CODE_METHOD : BIN_FILE_METHOD\n");
	} else {
		data->fw_image = data->fw_header_image;
		pr_warn("LOAD_CODE_METHOD : HEADER_METHOD\n");
	}

	if (!data->fw_image) {
		if (LOAD_CODE_METHOD == BIN_FILE_METHOD) {
			pr_warn("%s:please assign a fw(/etc/firmware/%s)or(/lib/firmware/%s)\n",
			       __func__, data->fw_bin_file_name, data->fw_bin_file_name);
		} else {
			pr_warn("%s:please assign a fw\n", __func__);
		}

		ret = -1;
		goto error0;
	}

	ilm_len = (*(data->fw_image + 3) << 24) | (*(data->fw_image + 2) << 16) |
	    (*(data->fw_image + 1) << 8) | (*data->fw_image);

	dlm_len = (*(data->fw_image + 7) << 24) | (*(data->fw_image + 6) << 16) |
	    (*(data->fw_image + 5) << 8) | (*(data->fw_image + 4));

	fw_ver = (*(data->fw_image + 11) << 8) | (*(data->fw_image + 10));

	build_ver = (*(data->fw_image + 9) << 8) | (*(data->fw_image + 8));

	pr_warn("fw version:%d.%d.%02d ", (fw_ver & 0xf000) >> 8,
	       (fw_ver & 0x0f00) >> 8, (fw_ver & 0x00ff));

	pr_warn("build:%x\n", build_ver);

	pr_warn("build Time =");

	for (loop = 0; loop < 16; loop++)
		pr_warn("%c", *(data->fw_image + 16 + loop));

	pr_warn("\n");

	pr_warn("ILM length = %d(bytes)\n", ilm_len);
	pr_warn("DLM length = %d(bytes)\n", dlm_len);

	btmtk_usb_switch_iobase(data, SYSCTL);

	/* U2M_PDMA rx_ring_base_ptr */
	btmtk_usb_io_write32(data, 0x790, 0x400230);

	/* U2M_PDMA rx_ring_max_cnt */
	btmtk_usb_io_write32(data, 0x794, 0x1);

	/* U2M_PDMA cpu_idx */
	btmtk_usb_io_write32(data, 0x798, 0x1);

	/* U2M_PDMA enable */
	btmtk_usb_io_write32(data, 0x704, 0x44);

	urb = usb_alloc_urb(0, GFP_ATOMIC);

	if (!urb) {
		ret = -ENOMEM;
		goto error1;
	}

	buf = usb_alloc_coherent(udev, 14592, GFP_ATOMIC, &data_dma);

	if (!buf) {
		ret = -ENOMEM;
		goto error2;
	}

	pr_warn("loading fw");

	init_completion(&sent_to_mcu_done);

	btmtk_usb_switch_iobase(data, SYSCTL);

	cur_len = 0x40;

	/* Loading ILM */
	while (1) {
		sent_len = (ilm_len - cur_len) >= 14336 ? 14336 : (ilm_len - cur_len);

		if (sent_len > 0) {
			packet_header &= ~(0xffffffff);
			packet_header |= (sent_len << 16);
			packet_header = cpu_to_le32(packet_header);

			memmove(buf, &packet_header, 4);
			memmove(buf + 4, data->fw_image + 32 + cur_len, sent_len);

			/* U2M_PDMA descriptor */
			btmtk_usb_io_write32(data, 0x230, cur_len);

			while ((sent_len % 4) != 0)
				sent_len++;

			/* U2M_PDMA length */
			btmtk_usb_io_write32(data, 0x234, sent_len << 16);

			usb_fill_bulk_urb(urb,
					  udev,
					  pipe,
					  buf, sent_len + 4, load_fw_complete, &sent_to_mcu_done);

			urb->transfer_dma = data_dma;
			urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

			ret = usb_submit_urb(urb, GFP_ATOMIC);

			if (ret)
				goto error3;

			if (!wait_for_completion_timeout(&sent_to_mcu_done, msecs_to_jiffies(1000))) {
				usb_kill_urb(urb);
				pr_warn("upload ilm fw timeout\n");
				goto error3;
			}

			BT_DBG(".");

			mdelay(200);

			cur_len += sent_len;
		} else {
			break;
		}
	}

	init_completion(&sent_to_mcu_done);
	cur_len = 0x00;

	/* Loading DLM */
	while (1) {
		sent_len = (dlm_len - cur_len) >= 14336 ? 14336 : (dlm_len - cur_len);

		if (sent_len > 0) {
			packet_header &= ~(0xffffffff);
			packet_header |= (sent_len << 16);
			packet_header = cpu_to_le32(packet_header);

			memmove(buf, &packet_header, 4);
			memmove(buf + 4, data->fw_image + 32 + ilm_len + cur_len, sent_len);

			/* U2M_PDMA descriptor */
			btmtk_usb_io_write32(data, 0x230, 0x80000 + cur_len);

			while ((sent_len % 4) != 0) {
				BT_DBG("sent_len is not divided by 4\n");
				sent_len++;
			}

			/* U2M_PDMA length */
			btmtk_usb_io_write32(data, 0x234, sent_len << 16);

			usb_fill_bulk_urb(urb,
					  udev,
					  pipe,
					  buf, sent_len + 4, load_fw_complete, &sent_to_mcu_done);

			urb->transfer_dma = data_dma;
			urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

			ret = usb_submit_urb(urb, GFP_ATOMIC);

			if (ret)
				goto error3;

			if (!wait_for_completion_timeout(&sent_to_mcu_done, msecs_to_jiffies(1000))) {
				usb_kill_urb(urb);
				pr_warn("upload dlm fw timeout\n");
				goto error3;
			}

			BT_DBG(".");

			mdelay(500);

			cur_len += sent_len;

		} else {
			break;
		}
	}

	/* upload 64bytes interrupt vector */
	ret = load_fw_iv(data);
	mdelay(100);

	btmtk_usb_switch_iobase(data, WLAN);

	/* check MCU if ready */
	loop = 0;

	do {
		btmtk_usb_io_read32(data, COM_REG0, &value);

		if (value == 0x01)
			break;

		mdelay(10);
		loop++;
	} while (loop <= 100);

	if (loop > 1000) {
		pr_err("wait for 100 times\n");
		ret = -ENODEV;
	}

 error3:
	usb_free_coherent(udev, 14592, buf, data_dma);
 error2:
	usb_free_urb(urb);
 error1:
	/* Disbale load fw mode */
	btmtk_usb_io_read32(data, 0x238, &value);
	value = value & ~(0x10000000);
	btmtk_usb_io_write32(data, 0x238, value);
 error0:
	btmtk_usb_io_write32(data, SEMAPHORE_00, 0x1);
	return ret;
}

static int inc_tx(struct btmtk_usb_data *data)
{
	unsigned long flags;
	int rv;

	pr_warn("btmtk:inc_tx\n");

	spin_lock_irqsave(&data->txlock, flags);
	rv = test_bit(BTUSB_SUSPENDING, &data->flags);
	if (!rv) {
		data->tx_in_flight++;
		pr_warn("btmtk:tx_in_flight++\n");
	}

	spin_unlock_irqrestore(&data->txlock, flags);

	return rv;
}

static void btmtk_usb_intr_complete(struct urb *urb)
{
	struct hci_dev *hdev = urb->context;
	unsigned char *event_buf;
	UINT32 roomLeft, last_len, length;

	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
	int err;

	if (urb->status != 0)
		pr_warn("%s: %s urb %p urb->status %d count %d\n", __func__, hdev->name,
	       urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags) && (metaMode == 0)) {
		pr_warn("stop interrupt urb\n");
		return;
	}

	if (urb->status == 0 && metaMode == 0) {
		hdev->stat.byte_rx += urb->actual_length;
	} else if (urb->status == 0 && metaMode == 1 && urb->actual_length != 0) {
		event_buf = urb->transfer_buffer;

		/* In this condition, need to add header*/
		if (leftHciEventSize == 0)
			length = urb->actual_length + 1;
		else
			length = urb->actual_length;

		lock_unsleepable_lock(&(metabuffer.spin_lock));
		/* clayton: roomleft means the usable space */

		if (metabuffer.read_p <= metabuffer.write_p)
			roomLeft = META_BUFFER_SIZE - metabuffer.write_p + metabuffer.read_p - 1;
		else
			roomLeft = metabuffer.read_p - metabuffer.write_p - 1;

		/* clayton: no enough space to store the received data */
		if (roomLeft < length)
			pr_err("Queue is full !!");

		if (length + metabuffer.write_p < META_BUFFER_SIZE) {
			/* only one interrupt event, not be splited */
			if (leftHciEventSize == 0) {
				/* copy event header: 0x04 */
				metabuffer.buffer[metabuffer.write_p] = 0x04;
				metabuffer.write_p += 1;
			}
			/* copy event data */
			osal_memcpy(metabuffer.buffer + metabuffer.write_p, event_buf,
				    urb->actual_length);
			metabuffer.write_p += urb->actual_length;
		} else {
			pr_debug("btmtk:back to meta buffer head\n");
			last_len = META_BUFFER_SIZE - metabuffer.write_p;

			/* only one interrupt event, not be splited */
			if (leftHciEventSize == 0) {

				if (last_len != 0) {
					/* copy evnet header: 0x04 */
					metabuffer.buffer[metabuffer.write_p] = 0x04;
					metabuffer.write_p += 1;
					last_len--;
				} else {
					metabuffer.buffer[0] = 0x04;
					metabuffer.write_p = 1;
				}
			}
			/* copy event data */
			osal_memcpy(metabuffer.buffer + metabuffer.write_p, event_buf, last_len);
			osal_memcpy(metabuffer.buffer, event_buf + last_len,
				    urb->actual_length - last_len);
			metabuffer.write_p = urb->actual_length - last_len;
		}

		unlock_unsleepable_lock(&(metabuffer.spin_lock));

		/* means there is some data in next event */
		if (leftHciEventSize == 0 && event_buf[1] >= 15) {
			leftHciEventSize = event_buf[1] - 14;	/* the left data size in next interrupt event */
			pr_debug("there is left event, size:%d\n", leftHciEventSize);
		} else if (leftHciEventSize != 0) {
			leftHciEventSize -= urb->actual_length;

			if (leftHciEventSize == 0) {
				wake_up(&BT_wq);
				wake_up_interruptible(&inq);
			}

		} else if (leftHciEventSize == 0 && event_buf[1] < 15) {
			wake_up(&BT_wq);
			wake_up_interruptible(&inq);
		}
	} else {
		pr_warn("meta mode:%d, urb->status:%d, urb->length:%d", metaMode, urb->status, urb->actual_length);
	}

	if (!test_bit(BTUSB_INTR_RUNNING, &data->flags) && metaMode == 0) {
		pr_warn("btmtk:Not in meta mode\n");
		return;
	}

	usb_mark_last_busy(data->udev);
	usb_anchor_urb(urb, &data->intr_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);

	if (err < 0) {
		/* -EPERM: urb is being killed;
		 * -ENODEV: device got disconnected */
		if (err != -EPERM && err != -ENODEV)
			pr_warn("%s urb %p failed to resubmit intr_in_urb(%d)",
			       hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}
}

static int btmtk_usb_submit_intr_urb(struct hci_dev *hdev, gfp_t mem_flags)
{
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size;
	struct btmtk_usb_data *data;

	if (hdev == NULL)
		return -ENODEV;

	data = hci_get_drvdata(hdev);

	pr_warn("%s", __func__);

	if (!data->intr_ep)
		return -ENODEV;

	urb = usb_alloc_urb(0, mem_flags);
	if (!urb)
		return -ENOMEM;

	size = le16_to_cpu(data->intr_ep->wMaxPacketSize);

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}
	/* clayton: generate a interrupt pipe, direction: usb device to cpu */
	pipe = usb_rcvintpipe(data->udev, data->intr_ep->bEndpointAddress);

	usb_fill_int_urb(urb, data->udev, pipe, buf, size,
			 btmtk_usb_intr_complete, hdev, data->intr_ep->bInterval);

	urb->transfer_flags |= URB_FREE_BUFFER;

	usb_anchor_urb(urb, &data->intr_anchor);

	err = usb_submit_urb(urb, mem_flags);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			pr_err("%s urb %p submission failed (%d)", hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);
	pr_info("%s:end\n", __func__);

	return err;


}

static void btmtk_usb_bulk_in_complete(struct urb *urb)
{
	struct hci_dev *hdev = urb->context;
	struct btmtk_usb_data *data;
	/* actual_length: the ACL data size (doesn't contain header) */
	UINT32 roomLeft, last_len, length, index, actual_length;
	unsigned char *event_buf;
	int err;
	u8 *buf;
	u16 len;

	data = hci_get_drvdata(hdev);
	roomLeft = 0;
	last_len = 0;
	length = 0;
	index = 0;
	actual_length = 0;

	if (urb->status != 0)
		pr_warn("%s:%s urb %p urb->status %d count %d\n", __func__, hdev->name,
	       urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags) && (metaMode == 0)) {
		pr_warn("stop bulk urb\n");
		return;
	}

#if SUPPORT_FW_DUMP
	{
		buf = urb->transfer_buffer;
		len = 0;
		if (urb->actual_length > 4) {
			len = buf[2] + ((buf[3]<<8)&0xff00);
			if (buf[0] == 0x6f && buf[1] == 0xfc && len+4 == urb->actual_length) {
				isFwAssert = 1;
				if (fw_dump_buffer_full)
					pr_warn("btmtk FW DUMP : data comes when buffer full!! (Should Never Hzappen!!)\n");

				fw_dump_total_read_size += len;
				if (fw_dump_write_ptr + len < fw_dump_ptr+FW_DUMP_BUF_SIZE) {
					fw_dump_buffer_used_size += len;
					if (fw_dump_buffer_used_size + 512 > FW_DUMP_BUF_SIZE)
						fw_dump_buffer_full = 1;

					memcpy(fw_dump_write_ptr, &buf[4], len);
					fw_dump_write_ptr += len;
					up(&data->fw_dump_semaphore);
				} else
					pr_warn("btmtk FW DUMP:buffer size too small!(%d:%d)\n",
						FW_DUMP_BUF_SIZE, fw_dump_total_read_size);
			}
		}
	}
#endif

	if (urb->status == 0 && metaMode == 0) {
		hdev->stat.byte_rx += urb->actual_length;
/*
		if (hci_recv_fragment(hdev, HCI_ACLDATA_PKT,
				      urb->transfer_buffer, urb->actual_length) < 0) {
			pr_err("%s corrupted ACL packet", hdev->name);
			hdev->stat.err_rx++;
		}
*/
	} else if (urb->status == 0 && metaMode == 1) {
		event_buf = urb->transfer_buffer;
		len = buf[2] + ((buf[3] << 8) & 0xff00);

		if (urb->actual_length > 4 && event_buf[0] == 0x6f && event_buf[1] == 0xfc
			&& len + 4 == urb->actual_length)
			pr_warn("Coredump message\n");
		else {
			length = urb->actual_length + 1;

			actual_length = 1*(event_buf[2]&0x0f) + 16*((event_buf[2]&0xf0)>>4)
				+ 256*((event_buf[3]&0x0f)) + 4096*((event_buf[3]&0xf0)>>4);

			lock_unsleepable_lock(&(metabuffer.spin_lock));

			/* clayton: roomleft means the usable space */
			if (metabuffer.read_p <= metabuffer.write_p)
				roomLeft = META_BUFFER_SIZE - metabuffer.write_p + metabuffer.read_p - 1;
			else
				roomLeft = metabuffer.read_p - metabuffer.write_p - 1;

			/* clayton: no enough space to store the received data */
			if (roomLeft < length)
				pr_warn("Queue is full!!\n");

			if (length + metabuffer.write_p < META_BUFFER_SIZE) {

				if (leftACLSize == 0) {
					/* copy ACL data header: 0x02 */
					metabuffer.buffer[metabuffer.write_p] = 0x02;
					metabuffer.write_p += 1;
				}

				/* copy event data */
				osal_memcpy(metabuffer.buffer + metabuffer.write_p, event_buf,
						urb->actual_length);
				metabuffer.write_p += urb->actual_length;
			} else {
				last_len = META_BUFFER_SIZE - metabuffer.write_p;
				if (leftACLSize == 0) {
					if (last_len != 0) {
						/* copy ACL data header: 0x02 */
						metabuffer.buffer[metabuffer.write_p] = 0x02;
						metabuffer.write_p += 1;
						last_len--;
					} else {
						metabuffer.buffer[0] = 0x02;
						metabuffer.write_p = 1;
					}
				}

				/* copy event data */
				osal_memcpy(metabuffer.buffer + metabuffer.write_p, event_buf, last_len);
				osal_memcpy(metabuffer.buffer, event_buf + last_len,
						urb->actual_length - last_len);
				metabuffer.write_p = urb->actual_length - last_len;
			}

			unlock_unsleepable_lock(&(metabuffer.spin_lock));

			/* the maximize bulk in ACL data packet size is 512 (4byte header + 508 byte data)*/
			/* maximum received data size of one packet is 1025 (4byte header + 1021 byte data) */
			if (leftACLSize == 0 && actual_length > 1017) {
				/* the data in next interrupt event */
				leftACLSize = actual_length + 4 - urb->actual_length;
				pr_debug("there is left ACL event, size:%d\n", leftHciEventSize);
			} else if (leftACLSize > 0) {
				leftACLSize -= urb->actual_length;

				if (leftACLSize == 0) {
					pr_debug("no left size for ACL big data\n");
					wake_up(&BT_wq);
					wake_up_interruptible(&inq);
				}
			} else if (leftACLSize == 0 && actual_length <= 1017) {
				wake_up(&BT_wq);
				wake_up_interruptible(&inq);
			} else {
				pr_warn("ACL data count fail, leftACLSize:%d", leftACLSize);
			}
		}

	} else {
		pr_warn("meta mode:%d, urb->status:%d\n", metaMode, urb->status);
	}

	if (!test_bit(BTUSB_BULK_RUNNING, &data->flags) && metaMode == 0)
		return;

	usb_anchor_urb(urb, &data->bulk_anchor);
	usb_mark_last_busy(data->udev);

#if SUPPORT_FW_DUMP
	if (fw_dump_buffer_full) {
		fw_dump_bulk_urb = urb;
		err = 0;
	} else {
		err = usb_submit_urb(urb, GFP_ATOMIC);
	}
#else
	err = usb_submit_urb(urb, GFP_ATOMIC);
#endif
	if (err != 0) {
		/* -EPERM: urb is being killed;
		 * -ENODEV: device got disconnected */
		if (err != -EPERM && err != -ENODEV)
			pr_warn("%s urb %p failed to resubmit bulk_in_urb(%d)",
			       hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}
}

static int btmtk_usb_submit_bulk_in_urb(struct hci_dev *hdev, gfp_t mem_flags)
{
	struct btmtk_usb_data *data;
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size = HCI_MAX_FRAME_SIZE;

	if (hdev == NULL)
		return -ENODEV;

	data = hci_get_drvdata(hdev);

	pr_warn("%s:%s\n", __func__, hdev->name);

	if (!data->bulk_rx_ep)
		return -ENODEV;

	urb = usb_alloc_urb(0, mem_flags);
	if (!urb)
		return -ENOMEM;

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}
#if BT_REDUCE_EP2_POLLING_INTERVAL_BY_INTR_TRANSFER
	pipe = usb_rcvintpipe(data->udev, data->bulk_rx_ep->bEndpointAddress);
	usb_fill_int_urb(urb, data->udev, pipe, buf, size, btmtk_usb_bulk_in_complete, hdev, 4);
#else
	pipe = usb_rcvbulkpipe(data->udev, data->bulk_rx_ep->bEndpointAddress);
	usb_fill_bulk_urb(urb, data->udev, pipe, buf, size, btmtk_usb_bulk_in_complete, hdev);
#endif

	urb->transfer_flags |= URB_FREE_BUFFER;

	usb_mark_last_busy(data->udev);
	usb_anchor_urb(urb, &data->bulk_anchor);

	err = usb_submit_urb(urb, mem_flags);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			pr_err("%s urb %p submission failed (%d)", hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	return err;
}

static void btmtk_usb_isoc_in_complete(struct urb *urb)
{
	struct hci_dev *hdev = urb->context;
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
	int i, err;

	pr_debug("%s: %s urb %p status %d count %d", __func__, hdev->name,
	       urb, urb->status, urb->actual_length);

	if (urb->status == 0) {
		for (i = 0; i < urb->number_of_packets; i++) {
			unsigned int length = urb->iso_frame_desc[i].actual_length;
/*			unsigned int offset = urb->iso_frame_desc[i].offset; */
			if (urb->iso_frame_desc[i].status)
				continue;

			hdev->stat.byte_rx += length;
/*
			if (hci_recv_fragment(hdev, HCI_SCODATA_PKT,
					      urb->transfer_buffer + offset, length) < 0) {
				pr_err("%s corrupted SCO packet", hdev->name);
				hdev->stat.err_rx++;
			}
*/
		}
	}

	if (!test_bit(BTUSB_ISOC_RUNNING, &data->flags))
		return;

	usb_anchor_urb(urb, &data->isoc_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		/* -EPERM: urb is being killed;
		 * -ENODEV: device got disconnected */
		if (err != -EPERM && err != -ENODEV)
			pr_warn("%s urb %p failed to resubmit iso_in_urb(%d)",
			       hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}
}

static inline void __fill_isoc_descriptor(struct urb *urb, int len, int mtu)
{
	int i, offset = 0;

	BT_DBG("len %d mtu %d", len, mtu);

	for (i = 0; i < BTUSB_MAX_ISOC_FRAMES && len >= mtu; i++, offset += mtu, len -= mtu) {
		urb->iso_frame_desc[i].offset = offset;
		urb->iso_frame_desc[i].length = mtu;
	}

	if (len && i < BTUSB_MAX_ISOC_FRAMES) {
		urb->iso_frame_desc[i].offset = offset;
		urb->iso_frame_desc[i].length = len;
		i++;
	}

	urb->number_of_packets = i;
}

static int btmtk_usb_submit_isoc_in_urb(struct hci_dev *hdev, gfp_t mem_flags)
{
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size;
	struct btmtk_usb_data *data;

	if (hdev == NULL)
		return -ENODEV;

	data = hci_get_drvdata(hdev);

	if (!data->isoc_rx_ep)
		return -ENODEV;

	urb = usb_alloc_urb(BTUSB_MAX_ISOC_FRAMES, mem_flags);
	if (!urb)
		return -ENOMEM;

	size = le16_to_cpu(data->isoc_rx_ep->wMaxPacketSize) * BTUSB_MAX_ISOC_FRAMES;

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}

	pipe = usb_rcvisocpipe(data->udev, data->isoc_rx_ep->bEndpointAddress);

	usb_fill_int_urb(urb, data->udev, pipe, buf, size, btmtk_usb_isoc_in_complete,
			 hdev, data->isoc_rx_ep->bInterval);

	urb->transfer_flags = URB_FREE_BUFFER | URB_ISO_ASAP;

	__fill_isoc_descriptor(urb, size, le16_to_cpu(data->isoc_rx_ep->wMaxPacketSize));

	usb_anchor_urb(urb, &data->isoc_anchor);

	err = usb_submit_urb(urb, mem_flags);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			pr_err("%s urb %p submission failed (%d)", hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	return err;
}

#if LOAD_PROFILE
static void btmtk_usb_ctrl_complete(struct urb *urb)
{
	BT_DBG("btmtk_usb_ctrl_complete\n");
	kfree(urb->setup_packet);
	kfree(urb->transfer_buffer);
}

static int btmtk_usb_submit_ctrl_urb(struct hci_dev *hdev, char *buf, int length)
{
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
	struct usb_ctrlrequest *setup_packet;
	struct urb *urb;
	unsigned int pipe;
	char *send_buf;
	int err;

	BT_DBG("btmtk_usb_submit_ctrl_urb, length=%d\n", length);

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		pr_err("btmtk_usb_submit_ctrl_urb error1\n");
		return -ENOMEM;
	}

	send_buf = kmalloc(length, GFP_ATOMIC);
	if (!send_buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}
	memcpy(send_buf, buf, length);

	setup_packet = kmalloc(sizeof(*setup_packet), GFP_ATOMIC);
	if (!setup_packet) {
		usb_free_urb(urb);
		kfree(send_buf);
		return -ENOMEM;
	}

	setup_packet->bRequestType = data->cmdreq_type;
	setup_packet->bRequest = 0;
	setup_packet->wIndex = 0;
	setup_packet->wValue = 0;
	setup_packet->wLength = __cpu_to_le16(length);

	pipe = usb_sndctrlpipe(data->udev, 0x00);

	usb_fill_control_urb(urb, data->udev, pipe, (void *)setup_packet,
			     send_buf, length, btmtk_usb_ctrl_complete, hdev);

	usb_anchor_urb(urb, &data->tx_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			pr_err("%s urb %p submission failed (%d)", hdev->name, urb, -err);
		kfree(urb->setup_packet);
		usb_unanchor_urb(urb);
	} else {
		usb_mark_last_busy(data->udev);
	}

	usb_free_urb(urb);
	return err;
}

int _ascii_to_int(char buf)
{
	switch (buf) {
	case 'a':
	case 'A':
		return 10;
	case 'b':
	case 'B':
		return 11;
	case 'c':
	case 'C':
		return 12;
	case 'd':
	case 'D':
		return 13;
	case 'e':
	case 'E':
		return 14;
	case 'f':
	case 'F':
		return 15;
	default:
		return buf - '0';
	}
}

void btmtk_usb_load_profile(struct hci_dev *hdev)
{
	mm_segment_t old_fs;
	struct file *file = NULL;
	unsigned char *buf;
	unsigned char target_buf[256 + 4] = { 0 };
	int i = 0;
	int j = 4;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	file = filp_open("/etc/Wireless/RT2870STA/BT_CONFIG.dat", O_RDONLY, 0);
	if (IS_ERR(file)) {
		set_fs(old_fs);
		return;
	}

	buf = kmalloc(1280, GFP_ATOMIC);
	if (!buf) {
		pr_warn
		    ("malloc error when parsing /etc/Wireless/RT2870STA/BT_CONFIG.dat, exiting...\n");
		filp_close(file, NULL);
		set_fs(old_fs);
		return;
	}

	pr_warn("/etc/Wireless/RT2870STA/BT_CONFIG.dat exits, parse it.\n");
	memset(buf, 0, 1280);
	file->f_op->read(file, buf, 1280, &file->f_pos);

	for (i = 0; i < 1280; i++) {
		if (buf[i] == '\r')
			continue;
		if (buf[i] == '\n')
			continue;
		if (buf[i] == 0)
			break;
		if (buf[i] == '0' && buf[i + 1] == 'x') {
			i += 1;
			continue;
		}

		{
			if (buf[i + 1] == '\r' || buf[i + 1] == '\n' || buf[i + 1] == 0) {
				target_buf[j] = _ascii_to_int(buf[i]);
				j++;
			} else {
				target_buf[j] =
				    _ascii_to_int(buf[i]) << 4 | _ascii_to_int(buf[i + 1]);
				j++;
				i++;
			}
		}
	}
	kfree(buf);
	filp_close(file, NULL);
	set_fs(old_fs);

	/* Send to dongle */
	{
		target_buf[0] = 0xc3;
		target_buf[1] = 0xfc;
		target_buf[2] = j - 4 + 1;
		target_buf[3] = 0x01;

		pr_warn("Profile Configuration : (%d)\n", j);
		for (i = 0; i < j; i++)
			pr_warn("    0x%02X\n", target_buf[i]);

		if (hdev != NULL)
			btmtk_usb_submit_ctrl_urb(hdev, target_buf, j);
	}
}
#endif

static int btmtk_usb_open(struct hci_dev *hdev)
{
	struct btmtk_usb_data *data;
	int err;

	if (hdev == NULL)
		return -1;

	data = hci_get_drvdata(hdev);

	pr_warn("%s\n", __func__);

	/* assign to meta hdev */
	meta_hdev = hdev;

	g_data = data;

	/* clayton: add count to this interface to avoid autosuspend */
	err = usb_autopm_get_interface(data->intf);
	if (err < 0)
		return err;

	data->intf->needs_remote_wakeup = 1;

	/* clayton: set bit[HCI_RUNNING] to 1 */
	if (test_and_set_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	/* clayton: set bit[BTUSB_INTR_RUNNING] to 1 */
	if (test_and_set_bit(BTUSB_INTR_RUNNING, &data->flags))
		goto done;

	/* clayton: polling to receive interrupt from usb device */
	err = btmtk_usb_submit_intr_urb(hdev, GFP_KERNEL);
	if (err < 0)
		goto failed;

	/* clayton: polling to receive ACL data from usb device */
	err = btmtk_usb_submit_bulk_in_urb(hdev, GFP_KERNEL);
	if (err < 0) {
		usb_kill_anchored_urbs(&data->intr_anchor);
		goto failed;
	}
	/* clayton: set bit[BTUSB_BULK_RUNNING] to 1 */
	set_bit(BTUSB_BULK_RUNNING, &data->flags);

	/* BlueAngel has own initialization procedure. Don't do initialization */
	set_bit(HCI_RAW, &hdev->flags);
#if LOAD_PROFILE
	btmtk_usb_load_profile(hdev);
#endif

#if SUPPORT_FW_DUMP
	{
		sema_init(&data->fw_dump_semaphore, 0);
		data->fw_dump_tsk =
		    kthread_create(btmtk_usb_fw_dump_thread, (void *)data,
				   "btmtk_usb_fw_dump_thread");
		if (IS_ERR(data->fw_dump_tsk)) {
			pr_err("%s : create fw dump thread failed!\n", __func__);
			err = PTR_ERR(data->fw_dump_tsk);
			data->fw_dump_tsk = NULL;
			goto failed;
		}
		fw_dump_task_should_stop = 0;
		fw_dump_ptr = kmalloc(FW_DUMP_BUF_SIZE, GFP_ATOMIC);
		if (fw_dump_ptr == NULL) {
			pr_err("%s : kmalloc(%d) failed!\n", __func__, FW_DUMP_BUF_SIZE);
			goto failed;
		}
		fw_dump_file = NULL;
		fw_dump_read_ptr = fw_dump_ptr;
		fw_dump_write_ptr = fw_dump_ptr;
		fw_dump_total_read_size = 0;
		fw_dump_total_write_size = 0;
		fw_dump_buffer_used_size = 0;
		fw_dump_buffer_full = 0;
		fw_dump_bulk_urb = NULL;
		data->fw_dump_end_check_tsk = NULL;
		wake_up_process(data->fw_dump_tsk);
	}
#endif

 done:
	usb_autopm_put_interface(data->intf);
	pr_warn("%s:successfully\n", __func__);
	return 0;

 failed:
	clear_bit(BTUSB_INTR_RUNNING, &data->flags);
	clear_bit(HCI_RUNNING, &hdev->flags);
	usb_autopm_put_interface(data->intf);
	return err;
}

static void btmtk_usb_stop_traffic(struct btmtk_usb_data *data)
{
	pr_warn("%s:start\n", __func__);

	usb_kill_anchored_urbs(&data->intr_anchor);
	usb_kill_anchored_urbs(&data->bulk_anchor);
	/* SCO */
	/* usb_kill_anchored_urbs(&data->isoc_anchor); */
}

static int btmtk_usb_close(struct hci_dev *hdev)
{
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
	int err;

	err = 0;

	pr_warn("%s\n", __func__);
	/* clayton: mark for SCO over HCI */
	/* if (!test_and_clear_bit(HCI_RUNNING, &hdev->flags))
		return 0;

	cancel_work_sync(&data->work);
	cancel_work_sync(&data->waker); */
	/* SCO */
	/* clear_bit(BTUSB_ISOC_RUNNING, &data->flags); */
	clear_bit(BTUSB_BULK_RUNNING, &data->flags);
	clear_bit(BTUSB_INTR_RUNNING, &data->flags);

	btmtk_usb_stop_traffic(data);

	/*
	err = usb_autopm_get_interface(data->intf);
	*/
	if (err < 0)
		goto failed;
    /*
	data->intf->needs_remote_wakeup = 0;
	usb_autopm_put_interface(data->intf);*/

	isbtready = 1;
	pr_warn("%s:successfully\n", __func__);
	return 0;

 failed:
	/* usb_scuttle_anchored_urbs(&data->deferred); */
	isbtready = 0;
	return 0;
}

static int btmtk_usb_flush(struct hci_dev *hdev)
{
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);

	BT_DBG("%s", __func__);

	usb_kill_anchored_urbs(&data->tx_anchor);

	return 0;
}

static void btmtk_usb_tx_complete_meta(struct urb *urb)
{
	unsigned char *buf = urb->context;

	if (metaMode == 0) {
		pr_err("tx complete error\n");
		goto done;
	}

done:
	kfree(buf);
	kfree(urb->setup_packet);
}

static void btmtk_usb_tx_complete(struct urb *urb)
{
	struct sk_buff *skb = urb->context;
	struct hci_dev *hdev = (struct hci_dev *)skb->dev;
	struct btmtk_usb_data *data;

	data = hci_get_drvdata(hdev);

	BT_DBG("%s: %s urb %p status %d count %d\n", __func__, hdev->name,
	       urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (!urb->status)
		hdev->stat.byte_tx += urb->transfer_buffer_length;
	else
		hdev->stat.err_tx++;

 done:
	spin_lock(&data->txlock);
	data->tx_in_flight--;
	spin_unlock(&data->txlock);

	kfree(urb->setup_packet);

	kfree_skb(skb);
}

static void btmtk_usb_isoc_tx_complete_meta(struct urb *urb)
{
	unsigned char *buf = urb->context;

	if ((metaMode == 0))
		goto done;
done:
	kfree(buf);
	kfree(urb->setup_packet);
}

static void btmtk_usb_isoc_tx_complete(struct urb *urb)
{
	struct sk_buff *skb = urb->context;
	struct hci_dev *hdev = (struct hci_dev *)skb->dev;

	BT_DBG("%s: %s urb %p status %d count %d", __func__, hdev->name,
	       urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (!urb->status)
		hdev->stat.byte_tx += urb->transfer_buffer_length;
	else
		hdev->stat.err_tx++;

 done:
	kfree(urb->setup_packet);

	kfree_skb(skb);
}

static int btmtk_usb_send_frame(struct hci_dev *hdev1, struct sk_buff *skb)
{
	struct hci_dev *hdev = (struct hci_dev *)skb->dev;
	unsigned char *dPoint;
	struct btmtk_usb_data *data;
	struct usb_ctrlrequest *dr;
	struct urb *urb;
	unsigned int pipe;
	int err, i, length;

	data = hci_get_drvdata(hdev);

	pr_warn("%s\n", __func__);

	if (skb == NULL) {
		pr_err("btmtk_usb_send_frame:skb is null, return");
		return -ENOMEM;
	}

	if (skb != NULL) {
		pr_err("btmtk_usb_send_frame:skb isn't null, return");
		return -ENODEV;
	}

	switch (bt_cb(skb)->pkt_type) {
	case HCI_COMMAND_PKT:
		return 0;
		urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (!urb)
			return -ENOMEM;

		dr = kmalloc(sizeof(*dr), GFP_ATOMIC);
		if (!dr) {
			usb_free_urb(urb);
			return -ENOMEM;
		}
		pr_warn("HCI command:\n");


		dr->bRequestType = data->cmdreq_type;
		dr->bRequest = 0;
		dr->wIndex = 0;
		dr->wValue = 0;
		dr->wLength = __cpu_to_le16(skb->len);
		pr_warn("bRequestType = %d\n", dr->bRequestType);
		pr_warn("wLength = %d\n", dr->wLength);


		pipe = usb_sndctrlpipe(data->udev, 0x00);

		if (test_bit(HCI_RUNNING, &hdev->flags)) {
			u16 op_code;

			memcpy(&op_code, skb->data, 2);
			pr_warn("ogf = %x\n", (op_code & 0xfc00) >> 10);
			pr_warn("ocf = %x\n", op_code & 0x03ff);
		}

		length = skb->len;
		dPoint = skb->data;
		pr_warn("skb length = %d\n", length);


		for (i = 0; i < length; i++)
			pr_warn("0x%02x", dPoint[i]);
		pr_warn("\n");

		usb_fill_control_urb(urb, data->udev, pipe, (void *)dr,
				     skb->data, skb->len, btmtk_usb_tx_complete, skb);

		hdev->stat.cmd_tx++;
		break;

	case HCI_ACLDATA_PKT:
		if (!data->bulk_tx_ep)
			return -ENODEV;

		urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (!urb)
			return -ENOMEM;

		pipe = usb_sndbulkpipe(data->udev, data->bulk_tx_ep->bEndpointAddress);

		usb_fill_bulk_urb(urb, data->udev, pipe,
				  skb->data, skb->len, btmtk_usb_tx_complete, skb);

		hdev->stat.acl_tx++;
		pr_warn("HCI_ACLDATA_PKT:\n");
		break;

	case HCI_SCODATA_PKT:
		if (!data->isoc_tx_ep || hdev->conn_hash.sco_num < 1)
			return -ENODEV;

		urb = usb_alloc_urb(BTUSB_MAX_ISOC_FRAMES, GFP_ATOMIC);
		if (!urb)
			return -ENOMEM;

		pipe = usb_sndisocpipe(data->udev, data->isoc_tx_ep->bEndpointAddress);

		usb_fill_int_urb(urb, data->udev, pipe,
				 skb->data, skb->len, btmtk_usb_isoc_tx_complete,
				 skb, data->isoc_tx_ep->bInterval);

		urb->transfer_flags = URB_ISO_ASAP;

		__fill_isoc_descriptor(urb, skb->len,
				       le16_to_cpu(data->isoc_tx_ep->wMaxPacketSize));

		hdev->stat.sco_tx++;
		goto skip_waking;

	default:
		return -EILSEQ;
	}

	err = inc_tx(data);

	if (err) {
		usb_anchor_urb(urb, &data->deferred);
		schedule_work(&data->waker);
		err = 0;
		goto done;
	}

 skip_waking:
	usb_anchor_urb(urb, &data->tx_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		pr_err("%s urb %p submission failed (%d)", hdev->name, urb, -err);
		kfree(urb->setup_packet);
		usb_unanchor_urb(urb);
	} else {
		usb_mark_last_busy(data->udev);
	}

 done:
	usb_free_urb(urb);
	return err;
}

static void btmtk_usb_notify(struct hci_dev *hdev, unsigned int evt)
{
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);

	pr_warn("%s evt %d", hdev->name, evt);

	if (hdev->conn_hash.sco_num != data->sco_num) {
		data->sco_num = hdev->conn_hash.sco_num;
		schedule_work(&data->work);
	}
}

static inline int __set_isoc_interface(struct hci_dev *hdev, int altsetting)
{
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
	struct usb_interface *intf = data->isoc;
	struct usb_endpoint_descriptor *ep_desc;
	int i, err;

	if (!data->isoc)
		return -ENODEV;

	err = usb_set_interface(data->udev, 1, altsetting);
	if (err < 0) {
		pr_err("%s setting interface failed (%d)", hdev->name, -err);
		return err;
	}

	data->isoc_altsetting = altsetting;

	data->isoc_tx_ep = NULL;
	data->isoc_rx_ep = NULL;

	for (i = 0; i < intf->cur_altsetting->desc.bNumEndpoints; i++) {
		ep_desc = &intf->cur_altsetting->endpoint[i].desc;

		if (!data->isoc_tx_ep && usb_endpoint_is_isoc_out(ep_desc)) {
			data->isoc_tx_ep = ep_desc;
			continue;
		}

		if (!data->isoc_rx_ep && usb_endpoint_is_isoc_in(ep_desc)) {
			data->isoc_rx_ep = ep_desc;
			continue;
		}
	}

	if (!data->isoc_tx_ep || !data->isoc_rx_ep) {
		pr_err("%s invalid SCO descriptors", hdev->name);
		return -ENODEV;
	}

	return 0;
}

static void btmtk_usb_work(struct work_struct *work)
{
	struct btmtk_usb_data *data = container_of(work, struct btmtk_usb_data, work);
	struct hci_dev *hdev = data->hdev;
	int new_alts;
	int err;

	pr_warn("%s\n", __func__);

	pr_warn("\t%s() sco_num=%d\n", __func__, hdev->conn_hash.sco_num);
	if (hdev->conn_hash.sco_num > 0) {
		if (!test_bit(BTUSB_DID_ISO_RESUME, &data->flags)) {
			err = usb_autopm_get_interface(data->isoc ? data->isoc : data->intf);
			if (err < 0) {
				pr_warn("%s: get isoc interface fail\n", __func__);
				clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
				usb_kill_anchored_urbs(&data->isoc_anchor);
				return;
			}

			set_bit(BTUSB_DID_ISO_RESUME, &data->flags);
		}

		if (hdev->voice_setting & 0x0020) {
			static const int alts[3] = { 2, 4, 5 };

			new_alts = alts[hdev->conn_hash.sco_num - 1];
			pr_warn("\t%s() 0x0020 new_alts=%d\n", __func__, new_alts);
		} else if (hdev->voice_setting & 0x2000) {
			new_alts = 4;
			pr_warn("\t%s() 0x2000 new_alts=%d\n", __func__, new_alts);
		} else {
			new_alts = hdev->conn_hash.sco_num;
			pr_warn("\t%s() ? new_alts=%d\n", __func__, new_alts);
		}

		if (data->isoc_altsetting != new_alts) {
			clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
			usb_kill_anchored_urbs(&data->isoc_anchor);

			if (__set_isoc_interface(hdev, new_alts) < 0)
				return;
		}

		if (!test_and_set_bit(BTUSB_ISOC_RUNNING, &data->flags)) {
			if (btmtk_usb_submit_isoc_in_urb(hdev, GFP_KERNEL) < 0) {
				clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
				pr_warn("%s: submit isoc urb fail\n", __func__);
			} else
				btmtk_usb_submit_isoc_in_urb(hdev, GFP_KERNEL);
		}
	} else {
		clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
		usb_kill_anchored_urbs(&data->isoc_anchor);

		__set_isoc_interface(hdev, 0);

		if (test_and_clear_bit(BTUSB_DID_ISO_RESUME, &data->flags))
			usb_autopm_put_interface(data->isoc ? data->isoc : data->intf);
	}
}

static void btmtk_usb_waker(struct work_struct *work)
{
	struct btmtk_usb_data *data = container_of(work, struct btmtk_usb_data, waker);
	int err;

	err = usb_autopm_get_interface(data->intf);

	if (err < 0)
		return;

	usb_autopm_put_interface(data->intf);
}


static irqreturn_t mt76xx_wobt_isr(int irq, void *dev)
{
	struct btmtk_usb_data *data = (struct btmtk_usb_data *)dev;

	pr_warn("%s()\n", __func__);
	disable_irq_nosync(data->wobt_irq);
	atomic_dec(&(data->irq_enable_count));
	pr_warn("%s():disable BT IRQ\n", __func__);
	return IRQ_HANDLED;
}


static int mt76xxRegisterBTIrq(struct btmtk_usb_data *data)
{
	struct device_node *eint_node = NULL;

	eint_node = of_find_compatible_node(NULL, NULL, "mediatek,mt76xx_bt_ctrl");
	pr_warn("btmtk:%s()\n", __func__);
	if (eint_node) {
		pr_warn("Get mt76xx_bt_ctrl compatible node\n");
		data->wobt_irq = irq_of_parse_and_map(eint_node, 0);
		pr_warn("btmtk:wobt_irq number:%d", data->wobt_irq);
		if (data->wobt_irq) {
			int interrupts[2];

			of_property_read_u32_array(eint_node, "interrupts",
						   interrupts, ARRAY_SIZE(interrupts));
			data->wobt_irqlevel = interrupts[1];
			if (request_irq(data->wobt_irq, mt76xx_wobt_isr,
					data->wobt_irqlevel, "mt76xx_bt_ctrl-eint", data))
				pr_warn("MT76xx WOBTIRQ LINE NOT AVAILABLE!!\n");
			else {
				pr_warn("%s():disable BT IRQ\n", __func__);
				disable_irq_nosync(data->wobt_irq);
			}

		} else
			pr_warn("can't find mt76xx_bt_ctrl irq\n");

	} else {
		data->wobt_irq = 0;
		pr_warn("can't find mt76xx_bt_ctrl compatible node\n");
	}
	pr_warn("btmtk:%s(): end\n", __func__);
	return 0;
}


static int mt76xxUnRegisterBTIrq(struct btmtk_usb_data *data)
{
	pr_warn("%s()\n", __func__);
	if (data->wobt_irq)
		free_irq(data->wobt_irq, data);
	return 0;
}

void suspend_workqueue(struct work_struct *work)
{
	pr_warn("%s\n", __func__);
	btmtk_usb_send_assert_cmd(g_data->udev);
}

static int btmtk_usb_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct btmtk_usb_data *data;
	struct usb_endpoint_descriptor *ep_desc;
	int i, err;
	struct hci_dev *hdev;

	pr_warn("===========================================\n");
	pr_warn("Mediatek Bluetooth USB driver ver %s\n", VERSION);
	pr_warn("===========================================\n");
	memset(driver_version, 0, sizeof(driver_version));
	memcpy(driver_version, VERSION, sizeof(VERSION));
	probe_counter++;
	isbtready = 0;
	isFwAssert = 0;
	pr_warn("probe_counter = %d\n", probe_counter);

	pr_warn("btmtk_usb_probe begin\n");

	/* interface numbers are hardcoded in the spec */
	if (intf->cur_altsetting->desc.bInterfaceNumber != 0) {
		pr_err("[ERR] interface number != 0 (%d)\n",
		       intf->cur_altsetting->desc.bInterfaceNumber);
		pr_err("btmtk_usb_probe end Error 1\n");
		return -ENODEV;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);

	if (!data)
		return -ENOMEM;

	mt76xxRegisterBTIrq(data);
	/* clayton: set the endpoint type of the interface to btmtk_usb_data */
	for (i = 0; i < intf->cur_altsetting->desc.bNumEndpoints; i++) {
		ep_desc = &intf->cur_altsetting->endpoint[i].desc;	/* clayton: Endpoint descriptor */

		if (!data->intr_ep && usb_endpoint_is_int_in(ep_desc)) {
			data->intr_ep = ep_desc;
			continue;
		}

		if (!data->bulk_tx_ep && usb_endpoint_is_bulk_out(ep_desc)) {
			data->bulk_tx_ep = ep_desc;
			continue;
		}

		if (!data->bulk_rx_ep && usb_endpoint_is_bulk_in(ep_desc)) {
			data->bulk_rx_ep = ep_desc;
			continue;
		}
	}

	if (!data->intr_ep || !data->bulk_tx_ep || !data->bulk_rx_ep) {
		kfree(data);
		pr_err("btmtk_usb_probe end Error 3\n");
		return -ENODEV;
	}

	data->cmdreq_type = USB_TYPE_CLASS;

	data->udev = interface_to_usbdev(intf);
	meta_udev = data->udev;

	data->intf = intf;

	spin_lock_init(&data->lock);
	INIT_WORK(&data->work, btmtk_usb_work);
	INIT_WORK(&data->waker, btmtk_usb_waker);
	spin_lock_init(&data->txlock);

	data->meta_tx = 0;

	/* clayton: init all usb anchor */
	init_usb_anchor(&data->tx_anchor);
	init_usb_anchor(&data->intr_anchor);
	init_usb_anchor(&data->bulk_anchor);
	init_usb_anchor(&data->isoc_anchor);
	init_usb_anchor(&data->deferred);

	data->io_buf = kmalloc(256, GFP_ATOMIC);

	btmtk_usb_switch_iobase(data, WLAN);

	/* clayton: according to the chip id, load f/w or rom patch */
	btmtk_usb_cap_init(data);

	if (data->need_load_rom_patch) {
		err = btmtk_usb_load_rom_patch(data);

		if (err < 0) {
			kfree(data->io_buf);
			kfree(data);
			pr_err("btmtk_usb_probe end Error 4\n");
			return err;
		}
	}

	if (data->need_load_fw) {
		err = btmtk_usb_load_fw(data);

		if (err < 0) {
			kfree(data->io_buf);
			kfree(data);
			pr_err("btmtk_usb_probe end Error 5\n");
			return err;
		}
	}
	/* allocate a hci device */
	hdev = hci_alloc_dev();
	if (!hdev) {
		kfree(data);
		pr_err("btmtk_usb_probe end Error 6\n");
		return -ENOMEM;
	}

	hdev->bus = HCI_USB;
	hci_set_drvdata(hdev, data);
	data->hdev = hdev;

	/* clayton: set the interface device to the hdev's parent */
	SET_HCIDEV_DEV(hdev, &intf->dev);

	hdev->open = btmtk_usb_open;	/* used when device is power on/receiving HCIDEVUP command */
	hdev->close = btmtk_usb_close;	/* remove interrupt-in/bulk-in urb.used */
	hdev->flush = btmtk_usb_flush;	/* Not necessary.remove tx urb. Used in device reset/power on fail/power off */
	hdev->send = btmtk_usb_send_frame;	/* used when transfer data to device */
	hdev->notify = btmtk_usb_notify;	/* adjust sco related setting */

	/* Interface numbers are hardcoded in the specification */
	data->isoc = usb_ifnum_to_if(data->udev, 1);

	/* initialize the IRQ enable counter */
	atomic_set(&(data->irq_enable_count), 0);

	kfree(data->rom_patch_bin_file_name);

	/* bind isoc interface to usb driver */
	if (data->isoc) {
		err = usb_driver_claim_interface(&btmtk_usb_driver, data->isoc, data);
		if (err < 0) {
			hci_free_dev(hdev);
			kfree(data->io_buf);
			kfree(data);
			pr_err("btmtk_usb_probe end Error 7\n");
			return err;
		}
	}

	err = hci_register_dev(hdev);
	if (err < 0) {
		hci_free_dev(hdev);
		kfree(data->io_buf);
		kfree(data);
		pr_err("btmtk_usb_probe end Error 8\n");
		return err;
	}

	usb_set_intfdata(intf, data);
	isUsbDisconnet = 0;

	pr_warn("btmtk_usb_probe end\n");
	return 0;
}

static void btmtk_usb_disconnect(struct usb_interface *intf)
{
	struct btmtk_usb_data *data = usb_get_intfdata(intf);
	struct hci_dev *hdev;

	pr_warn("%s\n", __func__);

	if (!data)
		return;

	hdev = data->hdev;

	usb_set_intfdata(data->intf, NULL);

	if (data->isoc)
		usb_set_intfdata(data->isoc, NULL);

	hci_unregister_dev(hdev);

	if (intf == data->isoc)
		usb_driver_release_interface(&btmtk_usb_driver, data->intf);
	else if (data->isoc)
		usb_driver_release_interface(&btmtk_usb_driver, data->isoc);

	hci_free_dev(hdev);
	kfree(data->io_buf);

	isbtready = 0;
	metaCount = 0;
	metabuffer.read_p = 0;
	metabuffer.write_p = 0;

	/* reset the IRQ enable counter to zero */
	atomic_set(&(data->irq_enable_count), 0);

	if (LOAD_CODE_METHOD == BIN_FILE_METHOD) {
		if (data->need_load_rom_patch)
			kfree(data->rom_patch);

		if (data->need_load_fw)
			kfree(data->fw_image);
	}

	pr_warn("unregister bt irq\n");
	mt76xxUnRegisterBTIrq(data);

	isUsbDisconnet = 1;
	pr_warn("btmtk: stop all URB\n");
	clear_bit(BTUSB_BULK_RUNNING, &data->flags);
	clear_bit(BTUSB_INTR_RUNNING, &data->flags);
	clear_bit(BTUSB_ISOC_RUNNING, &data->flags);

	btmtk_usb_stop_traffic(data);
	cancel_work_sync(&data->work);
	cancel_work_sync(&data->waker);

	kfree(data);
}

#ifdef CONFIG_PM
static int btmtk_usb_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct btmtk_usb_data *data = usb_get_intfdata(intf);

	if ((data->suspend_count++) || (metaMode == 0))
		return 0;

	/*The value of meta mode  should be 1*/
	pr_warn("%s: meta value:%d\n", __func__, metaMode);

	if (data->wobt_irq != 0 && atomic_read(&(data->irq_enable_count)) == 0) {
		pr_warn("%s:enable BT IRQ:%d\n", __func__, data->wobt_irq);
		irq_set_irq_wake(data->wobt_irq, 1);
		enable_irq(data->wobt_irq);
		atomic_inc(&(data->irq_enable_count));
	} else
		pr_warn("%s:irq_enable count:%d\n", __func__, atomic_read(&(data->irq_enable_count)));


#if BT_SEND_HCI_CMD_BEFORE_SUSPEND
	btmtk_usb_send_hci_suspend_cmd(data->udev);
#endif

	spin_lock_irq(&data->txlock);
	if (!(PMSG_IS_AUTO(message) && data->tx_in_flight)) {
		set_bit(BTUSB_SUSPENDING, &data->flags);
		spin_unlock_irq(&data->txlock);
		pr_warn("Enter suspend\n");
		pr_warn("tx in flight:%d\n", data->tx_in_flight);
	} else {
		spin_unlock_irq(&data->txlock);
		data->suspend_count--;
		pr_warn("Current is busy\n");
		return -EBUSY;
	}

	cancel_work_sync(&data->work);

	btmtk_usb_stop_traffic(data);
	usb_kill_anchored_urbs(&data->tx_anchor);

	return 0;
}

static void play_deferred(struct btmtk_usb_data *data)
{
	struct urb *urb;
	int err;

	while ((urb = usb_get_from_anchor(&data->deferred))) {
		err = usb_submit_urb(urb, GFP_ATOMIC);
		if (err < 0)
			break;

		data->tx_in_flight++;
	}

	usb_scuttle_anchored_urbs(&data->deferred);
}

static int meta_usb_submit_intr_urb(struct hci_dev *hdev)
{
	struct btmtk_usb_data *data;
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size;

	if (hdev == NULL)
		return -ENODEV;

	data = hci_get_drvdata(hdev);

	if (!data->intr_ep)
		return -ENODEV;

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb)
		return -ENOMEM;

	size = le16_to_cpu(data->intr_ep->wMaxPacketSize);
	hciEventMaxSize = size;
	pr_warn("%s\n, maximum packet size:%d", __func__, hciEventMaxSize);

	buf = kmalloc(size, GFP_KERNEL);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}
	/* clayton: generate a interrupt pipe, direction: usb device to cpu */
	pipe = usb_rcvintpipe(data->udev, data->intr_ep->bEndpointAddress);

	usb_fill_int_urb(urb, meta_udev, pipe, buf, size,
			 btmtk_usb_intr_complete, hdev, data->intr_ep->bInterval);

	urb->transfer_flags |= URB_FREE_BUFFER;

	usb_anchor_urb(urb, &data->intr_anchor);

	err = usb_submit_urb(urb, GFP_KERNEL);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			pr_err("%s urb %p submission failed (%d)", hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	return err;
}

static int meta_usb_submit_bulk_in_urb(struct hci_dev *hdev)
{
	struct btmtk_usb_data *data = hci_get_drvdata(hdev);
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size = HCI_MAX_FRAME_SIZE;

	pr_warn("%s:%s\n", __func__, hdev->name);

	if (!data->bulk_rx_ep)
		return -ENODEV;

	urb = usb_alloc_urb(0, GFP_NOIO);
	if (!urb)
		return -ENOMEM;

	buf = kmalloc(size, GFP_NOIO);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}
#if BT_REDUCE_EP2_POLLING_INTERVAL_BY_INTR_TRANSFER
	pipe = usb_rcvintpipe(data->udev, data->bulk_rx_ep->bEndpointAddress);
	usb_fill_int_urb(urb, data->udev, pipe, buf, size, btmtk_usb_bulk_in_complete, hdev, 4);
#else
	pipe = usb_rcvbulkpipe(data->udev, data->bulk_rx_ep->bEndpointAddress);
	usb_fill_bulk_urb(urb, data->udev, pipe, buf, size, btmtk_usb_bulk_in_complete, hdev);
#endif

	urb->transfer_flags |= URB_FREE_BUFFER;

	usb_mark_last_busy(data->udev);
	usb_anchor_urb(urb, &data->bulk_anchor);

	err = usb_submit_urb(urb, GFP_NOIO);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			pr_err("%s urb %p submission failed (%d)", hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	return err;
}


static int btmtk_usb_resume(struct usb_interface *intf)
{
	struct btmtk_usb_data *data = usb_get_intfdata(intf);
	struct hci_dev *hdev = data->hdev;
	int err = 0;

	if (--data->suspend_count)
		return 0;

	if (!test_bit(HCI_RUNNING, &hdev->flags) && (metaMode == 0))
		goto done;

	/*The value of meta mode  should be 0*/
	pr_warn("%s: meta value:%d\n", __func__, metaMode);

	/* send wake up command to notify firmware*/
	btmtk_usb_send_hci_wake_up_cmd(data->udev);

	spin_lock_irq(&data->txlock);
	play_deferred(data);
	clear_bit(BTUSB_SUSPENDING, &data->flags);
	spin_unlock_irq(&data->txlock);
	schedule_work(&data->work);

	if (data->wobt_irq != 0 && atomic_read(&(data->irq_enable_count)) == 1) {
		pr_warn("%s:disable BT IRQ:%d\n", __func__, data->wobt_irq);
		atomic_dec(&(data->irq_enable_count));
		disable_irq_nosync(data->wobt_irq);
	} else
		pr_warn("%s:irq_enable count:%d\n", __func__, atomic_read(&(data->irq_enable_count)));


	if (test_bit(BTUSB_INTR_RUNNING, &data->flags) || metaMode != 0) {
		pr_warn("%s: BT USB INTR RUNNING\n", __func__);
		err = meta_usb_submit_intr_urb(hdev);
		if (err < 0) {
			pr_warn("%s: error number:%d\n", __func__, err);
			clear_bit(BTUSB_INTR_RUNNING, &data->flags);
			goto failed;
		}
	}

	if (test_bit(BTUSB_BULK_RUNNING, &data->flags) || metaMode != 0) {
		pr_warn("%s: BT USB BULK RUNNING\n", __func__);
		err = meta_usb_submit_bulk_in_urb(hdev);
		if (err < 0) {
			pr_warn("%s: error number:%d\n", __func__, err);
			clear_bit(BTUSB_BULK_RUNNING, &data->flags);
			goto failed;
		}
	}

	if (test_bit(BTUSB_ISOC_RUNNING, &data->flags) || metaMode != 0) {
		if (btmtk_usb_submit_isoc_in_urb(hdev, GFP_NOIO) < 0)
			clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
		else
			btmtk_usb_submit_isoc_in_urb(hdev, GFP_NOIO);
	}

	return 0;

 failed:
	usb_scuttle_anchored_urbs(&data->deferred);
 done:
	spin_lock_irq(&data->txlock);
	clear_bit(BTUSB_SUSPENDING, &data->flags);
	spin_unlock_irq(&data->txlock);

	return err;
}
#endif


static int btmtk_usb_meta_send_data(unsigned char *buffer, const unsigned int length)
{
	int ret = 0;

	if (buffer[0] != 0x01) {
		pr_warn("the data from meta isn't HCI command, value:%d", buffer[0]);
	} else {
		ret =
		    usb_control_msg(meta_udev, usb_sndctrlpipe(meta_udev, 0), 0x0,
				    DEVICE_CLASS_REQUEST_OUT, 0x00, 0x00, buffer+1, length-1,
				    CONTROL_TIMEOUT_JIFFIES);
		kfree(buffer);
	}

	if (ret < 0) {
		pr_warn("%s error1(%d)\n", __func__, ret);
		return ret;
	}

	pr_info("send HCI command length:%d\n", ret);

	return length;
}


static struct usb_device_id btmtk_usb_table[] = {
	/* Mediatek MT662 */
	{USB_DEVICE_AND_INTERFACE_INFO(0x0e8d, 0x7662, 0xe0, 0x01, 0x01)},
	{USB_DEVICE_AND_INTERFACE_INFO(0x0e8d, 0x7632, 0xe0, 0x01, 0x01)},
	{}			/* Terminating entry */
};


/* clayton: usb interface driver */
static struct usb_driver btmtk_usb_driver = {
	.name = "btmtk_usb",
	.probe = btmtk_usb_probe,
	.disconnect = btmtk_usb_disconnect,
#ifdef CONFIG_PM
	.suspend = btmtk_usb_suspend,
	.resume = btmtk_usb_resume,
#endif
	.id_table = btmtk_usb_table,
	.supports_autosuspend = 1,
	.disable_hub_initiated_lpm = 1,
};

static int btmtk_usb_send_data(struct hci_dev *hdev, unsigned char *buffer, const unsigned int length)
{
	struct urb *urb;
	unsigned int pipe;
	int err;

	struct btmtk_usb_data *data = hci_get_drvdata(hdev);

	if (!data->bulk_tx_ep) {
		pr_warn("%s: No bulk_tx_ep\n", __func__);
		return -ENODEV;
	}

	urb = usb_alloc_urb(0, GFP_ATOMIC);

	if (!urb) {
		pr_warn("%s: No memory\n", __func__);
		return -ENOMEM;
	}

	if (buffer[0] == 0x02) {
		pr_debug("%s:send ACL Data\n", __func__);

		pipe = usb_sndbulkpipe(data->udev, data->bulk_tx_ep->bEndpointAddress);

		usb_fill_bulk_urb(urb, data->udev, pipe,
			(void *)buffer+1, length-1, btmtk_usb_tx_complete_meta, buffer);
		usb_anchor_urb(urb, &data->tx_anchor);

		err = usb_submit_urb(urb, GFP_ATOMIC);
		if (err != 0)
			pr_warn("send ACL data fail, error:%d", err);

	} else if (buffer[0] == 0x03) {
		pr_debug("%s:send SCO Data\n", __func__);

		if (!data->isoc_tx_ep || hdev->conn_hash.sco_num < 1)
			return -ENODEV;

		urb = usb_alloc_urb(BTUSB_MAX_ISOC_FRAMES, GFP_ATOMIC);
		if (!urb)
			return -ENOMEM;

		pipe = usb_sndisocpipe(data->udev,
					data->isoc_tx_ep->bEndpointAddress);

		/* remove the SCO header:0x03 */
		usb_fill_int_urb(urb, data->udev, pipe,
				(void *)buffer+1, length-1, btmtk_usb_isoc_tx_complete_meta,
				buffer, data->isoc_tx_ep->bInterval);

		urb->transfer_flags  = URB_ISO_ASAP;

		__fill_isoc_descriptor(urb, length,
				le16_to_cpu(data->isoc_tx_ep->wMaxPacketSize));
		err = 0;

	} else {
		pr_warn("%s:unknown data\n", __func__);
		err = 0;
	}

	if (err < 0) {
		pr_warn("%s urb %p submission failed (%d)",
			hdev->name, urb, -err);

		kfree(urb->setup_packet);
		usb_unanchor_urb(urb);
	} else {
		usb_mark_last_busy(data->udev);
	}

	usb_free_urb(urb);
	return err;

}


long BT_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	INT32 retval = 0;

	switch (cmd) {
	case IOCTL_FW_ASSERT:
		/* BT trigger fw assert for debug */
		pr_warn("BT Set fw assert......");
		break;
	default:
		retval = -EFAULT;
		pr_warn("BT_ioctl(): unknown cmd (%d)\n", cmd);
		break;
	}

	return retval;
}

unsigned int BT_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	if (metabuffer.read_p == metabuffer.write_p) {
		poll_wait(filp, &inq,  wait);

		/* empty let select sleep */
		if (metabuffer.read_p != metabuffer.write_p)
			mask |= POLLIN | POLLRDNORM;  /* readable */

	} else
		mask |= POLLIN | POLLRDNORM;  /* readable */

	/* do we need condition? */
	mask |= POLLOUT | POLLWRNORM; /* writable */

	return mask;
}


ssize_t BT_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int retval = 0;
	unsigned char *tx_buf;

	metaCount++;
	metaMode = 1;

	/* create interrupt in endpoint */
	if (metaCount == 1) {
		if (!meta_hdev) {
			pr_warn("btmtk:meta hdev isn't ready\n");
			metaCount = 0;
			return -EFAULT;
		}
	}

	if (isUsbDisconnet == 1) {
		pr_warn("%s:btmtk: usb driver is disconnect now\n", __func__);
		return -EIO;
	}

	tx_buf = kmalloc(count, GFP_ATOMIC);
	if (!tx_buf)
		return -ENOMEM;


	/* clayton: semaphore mechanism, the waited process will sleep */
	down(&wr_mtx);

	if (count > 0) {
		if (copy_from_user(tx_buf, &buf[0], count)) {
			retval = -EFAULT;
			pr_warn("btmtk:BT_write: copy data from user fail\n");
			goto OUT;
		}

		/* command */
		if (tx_buf[0] == 0x01) {
			if (tx_buf[1] == 0x6f && tx_buf[2] == 0xfc && tx_buf[3] == 0x05 && tx_buf[4] == 0x01
				&& tx_buf[5] == 0x02 && tx_buf[6] == 0x01 && tx_buf[7] == 0x00 && tx_buf[8] == 0x08) {
				pr_warn("trigger fw assert\n");
				isFwAssert = 1;
			}

			retval = btmtk_usb_meta_send_data(&tx_buf[0], count);
		} else if (tx_buf[0] == 0x02) {
			retval = btmtk_usb_send_data(meta_hdev, &tx_buf[0], count);
		} else if (tx_buf[0] == 0x03) {
			retval = btmtk_usb_send_data(meta_hdev, &tx_buf[0], count);
		} else {
			pr_warn("btmtk:BT_write:this is unknown bt data:0x%02x\n", tx_buf[0]);
		}
	} else {
		retval = -EFAULT;
		pr_warn("btmtk:BT_write:target packet length:%zu is not allowed, retval = %d.\n", count, retval);
	}

 OUT:
	up(&wr_mtx);
	return retval;
}

ssize_t BT_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int retval = 0;
	int copyLen = 0;
	unsigned short tailLen = 0;
	UINT8 *buffer = i_buf;

	down(&rd_mtx);

	if (count > BUFFER_SIZE) {
		count = BUFFER_SIZE;
		pr_warn("read size is bigger than 1024\n");
	}

	if (isUsbDisconnet == 1) {
		pr_warn("%s:btmtk: usb driver is disconnect now\n", __func__);
		return -EIO;
	}

	lock_unsleepable_lock(&(metabuffer.spin_lock));

	/* means the buffer is empty */
	while (metabuffer.read_p == metabuffer.write_p) {

		/*  unlock the buffer to let other process write data to buffer */
		unlock_unsleepable_lock(&(metabuffer.spin_lock));

		/*If nonblocking mode, return directly O_NONBLOCK is specified during open() */
		if (filp->f_flags & O_NONBLOCK) {
			pr_info("Non-blocking BT_read()\n");
			retval = -EAGAIN;
			goto OUT;
		}
		wait_event(BT_wq, metabuffer.read_p != metabuffer.write_p);
		lock_unsleepable_lock(&(metabuffer.spin_lock));
	}

	while (metabuffer.read_p != metabuffer.write_p) {
		if (metabuffer.write_p > metabuffer.read_p) {
			copyLen = metabuffer.write_p - metabuffer.read_p;
			if (copyLen > count)
				copyLen = count;

			osal_memcpy(i_buf, metabuffer.buffer + metabuffer.read_p, copyLen);
			metabuffer.read_p += copyLen;
		} else {
			tailLen = META_BUFFER_SIZE - metabuffer.read_p;
			if (tailLen > count) {	/* exclude equal case to skip wrap check */
				copyLen = count;
				osal_memcpy(i_buf, metabuffer.buffer + metabuffer.read_p, copyLen);
				metabuffer.read_p += copyLen;
			} else {
				/* part 1: copy tailLen */
				osal_memcpy(i_buf, metabuffer.buffer + metabuffer.read_p, tailLen);

				buffer += tailLen;	/* update buffer offset */

				/* part 2: check if head length is enough */
				copyLen = count - tailLen;

				/* if write_p < copyLen: copy all data; else: copy data for copyLen */
				copyLen =
				    (metabuffer.write_p < copyLen) ? metabuffer.write_p : copyLen;

				/* clayton: if copylen not 0, copy data to buffer */
				if (copyLen)
					osal_memcpy(buffer, metabuffer.buffer + 0, copyLen);

				/* Update read_p final position */
				metabuffer.read_p = copyLen;

				/* update return length: head + tail */
				copyLen += tailLen;
			}
		}
		break;
	}

	unlock_unsleepable_lock(&(metabuffer.spin_lock));

	if (copy_to_user(buf, i_buf, copyLen)) {
		pr_err("copy to user fail\n");
		copyLen = -EFAULT;
		goto OUT;
	}
 OUT:
	up(&rd_mtx);
	return copyLen;
}

static int BT_open(struct inode *inode, struct file *file)
{
	if (meta_hdev == NULL) {
		pr_warn("%s: meta_hdev isn't ready\n", __func__);
		return -ENODEV;
	}

	if (isFwAssert == 1) {
		pr_warn("%s: firmware is core dump\n", __func__);
		return 0;
	}

	pr_warn("%s: major %d minor %d (pid %d)\n", __func__,
	       imajor(inode), iminor(inode), current->pid);
	if (current->pid == 1)
		return 0;

	if (isbtready == 0) {
		pr_warn("%s:bluetooth driver is not ready\n", __func__);
		return -ENODEV;
	}

	if (isUsbDisconnet == 1) {
		pr_warn("%s:btmtk: usb driver is disconnect now\n", __func__);
		return -EIO;
	}

	/* init meta buffer */
	spin_lock_init(&(metabuffer.spin_lock.lock));

	sema_init(&wr_mtx, 1);
	sema_init(&rd_mtx, 1);

	/* init wait queue */
	init_waitqueue_head(&(inq));

	btmtk_usb_send_radio_on_cmd(g_data->udev);

	pr_warn("enable interrupt and bulk in urb\n");
	meta_usb_submit_intr_urb(meta_hdev);
	meta_usb_submit_bulk_in_urb(meta_hdev);
	pr_warn("%s:OK\n", __func__);

	return 0;
}

static int BT_close(struct inode *inode, struct file *file)
{
	pr_warn("%s: major %d minor %d (pid %d)\n", __func__,
	       imajor(inode), iminor(inode), current->pid);

	if (isUsbDisconnet == 1) {
		pr_warn("%s:btmtk: usb driver is disconnect now\n", __func__);
		return -EIO;
	}

	if (isFwAssert == 1) {
		pr_warn("%s: firmware is core dump\n", __func__);
		return 0;
	}

	btmtk_usb_stop_traffic(g_data);
	btmtk_usb_send_radio_off_cmd(g_data->udev);
	metaMode = 0; /* means BT is close */

	lock_unsleepable_lock(&(metabuffer.spin_lock));
	metabuffer.read_p = 0;
	metabuffer.write_p = 0;
	unlock_unsleepable_lock(&(metabuffer.spin_lock));

	if (current->pid == 1)
		return 0;

	pr_warn("%s:OK\n", __func__);
	return 0;
}

static const struct file_operations BT_fops = {
	.open = BT_open,
	.release = BT_close,
	.read = BT_read,
	.write = BT_write,
	.poll = BT_poll,
	.unlocked_ioctl = BT_unlocked_ioctl,
};

static int BT_init(void)
{
	dev_t devID = MKDEV(BT_major, 0);
	int ret = 0;
	int cdevErr = 0;
	int major = 0;

	pr_warn("BT_init\n");

	ret = alloc_chrdev_region(&devID, 0, 1, BT_DRIVER_NAME);
	if (ret) {
		pr_err("fail to allocate chrdev\n");
		return ret;
	}

	BT_major = major = MAJOR(devID);
	pr_warn("major number:%d", BT_major);

	cdev_init(&BT_cdev, &BT_fops);
	BT_cdev.owner = THIS_MODULE;

	cdevErr = cdev_add(&BT_cdev, devID, BT_devs);
	if (cdevErr)
		goto error;

	pr_warn("%s driver(major %d) installed.\n", BT_DRIVER_NAME, BT_major);

	pBTClass = class_create(THIS_MODULE, BT_DRIVER_NAME);
	if (IS_ERR(pBTClass)) {
		pr_err("class create fail, error code(%ld)\n", PTR_ERR(pBTClass));
		goto err1;
	}

	pBTDev = device_create(pBTClass, NULL, devID, NULL, BT_NODE);
	if (IS_ERR(pBTDev)) {
		pr_err("device create fail, error code(%ld)\n", PTR_ERR(pBTDev));
		goto err2;
	}
	/* init wait queue */
	init_waitqueue_head(&(inq));

	return 0;

 err2:
	if (pBTClass) {
		class_destroy(pBTClass);
		pBTClass = NULL;
	}

 err1:

 error:
	if (cdevErr == 0)
		cdev_del(&BT_cdev);

	if (ret == 0)
		unregister_chrdev_region(devID, BT_devs);

	return -1;
}

static void BT_exit(void)
{
	dev_t dev = MKDEV(BT_major, 0);

	pr_warn("BT_exit\n");

	if (pBTDev) {
		device_destroy(pBTClass, dev);
		pBTDev = NULL;
	}

	if (pBTClass) {
		class_destroy(pBTClass);
		pBTClass = NULL;
	}

	cdev_del(&BT_cdev);
	unregister_chrdev_region(dev, 1);
	pr_warn("%s driver removed.\n", BT_DRIVER_NAME);
}

/* module_usb_driver(btmtk_usb_driver); */
static int __init btmtk_usb_init(void)
{
	pr_warn("btmtk usb driver ver %s", VERSION);
	BT_init();

	return usb_register(&btmtk_usb_driver);
}

static void __exit btmtk_usb_exit(void)
{
	usb_deregister(&btmtk_usb_driver);
	BT_exit();
}
module_init(btmtk_usb_init);
module_exit(btmtk_usb_exit);


MODULE_DESCRIPTION("Mediatek Bluetooth USB driver ver " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL v2");
