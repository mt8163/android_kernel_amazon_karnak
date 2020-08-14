#ifndef _ACCDEH_H_
#define _ACCDEH_H_
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/ctype.h>

#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/switch.h>

#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/input.h>
#include <linux/string.h>

/*----------------------------------------------------------------------
 * IOCTL
 *---------------------------------------------------------------------- */
#define ACCDET_DEVNAME "accdet"
#define ACCDET_IOC_MAGIC 'A'
#define ACCDET_INIT _IO(ACCDET_IOC_MAGIC, 0)
#define SET_CALL_STATE _IO(ACCDET_IOC_MAGIC, 1)
#define GET_BUTTON_STATUS _IO(ACCDET_IOC_MAGIC, 2)

extern const struct file_operations *accdet_get_fops(void); /* from accdet_drv.c */
extern struct platform_driver accdet_driver_func(void); /* from accdet_drv.c */
extern struct of_device_id accdet_of_match[];
void mt_accdet_pm_restore_noirq(void);
long mt_accdet_unlocked_ioctl(unsigned int cmd, unsigned long arg);
int mt_accdet_probe(struct platform_device *dev);

#ifdef CONFIG_ACCDET_EINT
extern struct platform_device accdet_device;
#endif
#endif
