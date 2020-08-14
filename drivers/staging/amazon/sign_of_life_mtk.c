/*
 * sign_of_life_mtk.c
 *
 * MTK platform implementation
 *
 * Copyright (C) Amazon Technologies Inc. All rights reserved.
 * Yang Liu (yangliu@lab126.com)
 * TODO: Add additional contributor's names.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/sign_of_life.h>
#include "../../misc/mediatek/include/mt-plat/mtk_rtc.h"
#include "../../misc/mediatek/rtc/include/mtk_rtc_hal.h"
#include "../../misc/mediatek/include/mt-plat/mtk_rtc_hal_common.h"
#include "../../misc/mediatek/rtc/mt6323/mtk_rtc_hw.h"


/* RTC Spare Register Definition */

/*
 * RTC_NEW_SPARE1: RTC_AL_DOM bit0~4
 * 	   bit 8 ~ 15 : reserved bits for boot reasons
 */
#define RTC_NEW_SPARE1_WARM_BOOT_KERNEL_PANIC    (1U << 8)
#define RTC_NEW_SPARE1_WARM_BOOT_KERNEL_WDOG     (1U << 9)
#define RTC_NEW_SPARE1_WARM_BOOT_HW_WDOG         (1U << 10)
#define RTC_NEW_SPARE1_WARM_BOOT_SW              (1U << 11)
#define RTC_NEW_SPARE1_COLD_BOOT_USB             (1U << 12)
#define RTC_NEW_SPARE1_COLD_BOOT_POWER_KEY       (1U << 13)
#define RTC_NEW_SPARE1_COLD_BOOT_POWER_SUPPLY    (1U << 14)

/*
 * RTC_NEW_SPARE2: RTC_AL_DOW bit0~2
 * 	   bit 8 ~ 15 : reserved bits
 */
#define RTC_NEW_SPARE2_SHUTDOWN_LONG_PWR_KEY_PRESS     (1U << 8)
#define RTC_NEW_SPARE2_SHUTDOWN_SW                     (1U << 9)
#define RTC_NEW_SPARE2_SHUTDOWN_PWR_KEY                (1U << 10)
#define RTC_NEW_SPARE2_SHUTDOWN_SUDDEN_PWR_LOSS        (1U << 11)
#define RTC_NEW_SPARE2_SHUTDOWN_UKNOWN                 (1U << 12)

/*
 * RTC_NEW_SPARE3: RTC_AL_MTH bit0~3
 * 	   bit 8 ~ 15 : reserved bits
 */
#define RTC_NEW_SPARE3_THERMAL_SHUTDOWN_BATTERY               (1U << 8)
#define RTC_NEW_SPARE3_THERMAL_SHUTDOWN_PMIC                  (1U << 9)
#define RTC_NEW_SPARE3_THERMAL_SHUTDOWN_SOC                   (1U << 10)
#define RTC_NEW_SPARE3_THERMAL_SHUTDOWN_MODEM                 (1U << 11)
#define RTC_NEW_SPARE3_THERMAL_SHUTDOWN_WIFI                  (1U << 12)
#define RTC_NEW_SPARE3_THERMAL_SHUTDOWN_PCB                   (1U << 13)
#define RTC_NEW_SPARE3_THERMAL_SHUTDOWN_VS                    (1U << 14)
#define RTC_NEW_SPARE3_THERMAL_SHUTDOWN_BTS                   (1U << 15)

/*
 * RTC_SPAR1:
 *     bit 0 - 5 : SEC in power-on time
 *     bit 6 	 : 32K less bit. True:with 32K, False:Without 32K
 *     bit 9 - 15: reserved bits
 *     bit 13      : QUIESCENT mode
 */
#define RTC_SPAR0_SPECIAL_MODE_LOW_BATTERY                    (1U << 9)
#define RTC_SPAR0_SPECIAL_MODE_WARM_BOOT_USB_CONNECTED        (1U << 10)
#define RTC_SPAR0_SPECIAL_MODE_OTA                            (1U << 11)
#define RTC_SPAR0_SPECIAL_MODE_FACTORY_RESET                  (1U << 12)
#define RTC_SPAR0_SPECIAL_MODE_MASK                           0xfe00
#define PMIC_RTC_SPAR1_QUIESCENT                              (1U << 13)
#define PMIC_RTC_SPAR1_QUIESCENT_MASK                         (1U << 13)

/*drivers/watchdog/mediatek/wdt/common/mtk_wdt.c*/
extern void mtk_wdt_mode_config(bool dual_mode_en, bool irq, bool ext_en, bool ext_pol, bool wdt_en);


static void mtk_clear_boot_reason(void)
{
	u16 rtc_breason;

	rtc_acquire_lock();
	rtc_breason = rtc_read(RTC_AL_DOM);

	pr_notice("%s()\n", __func__);

	rtc_breason &= ~(RTC_NEW_SPARE1_WARM_BOOT_KERNEL_PANIC |
			RTC_NEW_SPARE1_WARM_BOOT_KERNEL_WDOG   |
			RTC_NEW_SPARE1_WARM_BOOT_HW_WDOG       |
			RTC_NEW_SPARE1_WARM_BOOT_SW            |
			RTC_NEW_SPARE1_COLD_BOOT_USB           |
			RTC_NEW_SPARE1_COLD_BOOT_POWER_KEY     |
			RTC_NEW_SPARE1_COLD_BOOT_POWER_SUPPLY);

	rtc_write(RTC_AL_DOM, rtc_breason);
	rtc_write_trigger();
	rtc_release_lock();
}

static int (mtk_read_boot_reason)(life_cycle_reason_t *boot_reason)
{
	u16 rtc_breason;

	rtc_acquire_lock();
	rtc_breason = rtc_read(RTC_AL_DOM);
	rtc_release_lock();

	printk(KERN_ERR"%s: boot_reason is 0x%x\n", __func__, (u32)(rtc_breason));

	if (rtc_breason & RTC_NEW_SPARE1_WARM_BOOT_KERNEL_PANIC)
		*boot_reason = WARMBOOT_BY_KERNEL_PANIC;
	else if (rtc_breason & RTC_NEW_SPARE1_WARM_BOOT_KERNEL_WDOG)
		*boot_reason = WARMBOOT_BY_KERNEL_WATCHDOG;
	else if (rtc_breason & RTC_NEW_SPARE1_WARM_BOOT_HW_WDOG)
		*boot_reason = WARMBOOT_BY_HW_WATCHDOG;
	else if (rtc_breason & RTC_NEW_SPARE1_WARM_BOOT_SW)
		*boot_reason = WARMBOOT_BY_SW;
	else if (rtc_breason & RTC_NEW_SPARE1_COLD_BOOT_USB)
		*boot_reason = COLDBOOT_BY_USB;
	else if (rtc_breason & RTC_NEW_SPARE1_COLD_BOOT_POWER_KEY)
		*boot_reason = COLDBOOT_BY_POWER_KEY;
	else if (rtc_breason & RTC_NEW_SPARE1_COLD_BOOT_POWER_SUPPLY)
		*boot_reason = COLDBOOT_BY_POWER_SUPPLY;
	else {
		printk(KERN_ERR"Failed to read boot rtc boot reason\n");
		return -1;
	}

	mtk_clear_boot_reason();

	return 0;
}

static int (mtk_write_boot_reason)(life_cycle_reason_t boot_reason)
{
	u16 rtc_breason;

	rtc_acquire_lock();
	rtc_breason = rtc_read(RTC_AL_DOM);

	printk(KERN_ERR"%s:current 0x%x boot_reason 0x%x\n", __func__, rtc_breason, boot_reason);
	if (boot_reason == WARMBOOT_BY_KERNEL_PANIC)
		rtc_breason = rtc_breason | RTC_NEW_SPARE1_WARM_BOOT_KERNEL_PANIC;
	else if (boot_reason == WARMBOOT_BY_KERNEL_WATCHDOG)
		rtc_breason = rtc_breason | RTC_NEW_SPARE1_WARM_BOOT_KERNEL_WDOG;
	else if (boot_reason == WARMBOOT_BY_HW_WATCHDOG)
		rtc_breason = rtc_breason | RTC_NEW_SPARE1_WARM_BOOT_HW_WDOG;
	else if (boot_reason == WARMBOOT_BY_SW)
		rtc_breason = rtc_breason | RTC_NEW_SPARE1_WARM_BOOT_SW;
	else if (boot_reason == COLDBOOT_BY_USB)
		rtc_breason = rtc_breason | RTC_NEW_SPARE1_COLD_BOOT_USB;
	else if (boot_reason == COLDBOOT_BY_POWER_KEY)
		rtc_breason = rtc_breason | RTC_NEW_SPARE1_COLD_BOOT_POWER_KEY;
	else if (boot_reason == COLDBOOT_BY_POWER_SUPPLY)
		rtc_breason = rtc_breason | RTC_NEW_SPARE1_COLD_BOOT_POWER_SUPPLY;

	rtc_write(RTC_AL_DOM, rtc_breason);
	rtc_write_trigger();
	rtc_release_lock();

	return 0;
}

static int (mtk_read_shutdown_reason)(life_cycle_reason_t *shutdown_reason)
{
	u16 rtc_shutdown_reason;

	rtc_acquire_lock();
	rtc_shutdown_reason = rtc_read(RTC_AL_DOW);
	rtc_release_lock();

	printk(KERN_ERR"%s: shutdown reason is 0x%x\n", __func__, rtc_shutdown_reason);
	if (rtc_shutdown_reason & RTC_NEW_SPARE2_SHUTDOWN_LONG_PWR_KEY_PRESS)
		*shutdown_reason = SHUTDOWN_BY_LONG_PWR_KEY_PRESS;
	else if (rtc_shutdown_reason & RTC_NEW_SPARE2_SHUTDOWN_SW)
		*shutdown_reason = SHUTDOWN_BY_SW;
	else if (rtc_shutdown_reason & RTC_NEW_SPARE2_SHUTDOWN_PWR_KEY)
		*shutdown_reason = SHUTDOWN_BY_PWR_KEY;
	else if (rtc_shutdown_reason & RTC_NEW_SPARE2_SHUTDOWN_SUDDEN_PWR_LOSS)
		*shutdown_reason = SHUTDOWN_BY_SUDDEN_POWER_LOSS;
	else if (rtc_shutdown_reason & RTC_NEW_SPARE2_SHUTDOWN_UKNOWN)
		*shutdown_reason = SHUTDOWN_BY_UNKNOWN_REASONS;
	else {
		printk(KERN_ERR"Failed to read shutdown rtc boot reason\n");
		return -1;
	}

	return 0;
}

static int (mtk_write_shutdown_reason)(life_cycle_reason_t shutdown_reason)
{
	u16 rtc_shutdown_reason;

	rtc_acquire_lock();
	rtc_shutdown_reason = rtc_read(RTC_AL_DOW);

	printk(KERN_ERR"%s:shutdown_reason 0x%x\n", __func__, rtc_shutdown_reason);

	if (shutdown_reason == SHUTDOWN_BY_LONG_PWR_KEY_PRESS)
		rtc_shutdown_reason = rtc_shutdown_reason | RTC_NEW_SPARE2_SHUTDOWN_LONG_PWR_KEY_PRESS;
	else if (shutdown_reason == SHUTDOWN_BY_SW)
		rtc_shutdown_reason = rtc_shutdown_reason | RTC_NEW_SPARE2_SHUTDOWN_SW;
	else if (shutdown_reason == SHUTDOWN_BY_PWR_KEY)
		rtc_shutdown_reason = rtc_shutdown_reason | RTC_NEW_SPARE2_SHUTDOWN_PWR_KEY;
	else if (shutdown_reason == SHUTDOWN_BY_SUDDEN_POWER_LOSS)
		rtc_shutdown_reason = rtc_shutdown_reason | RTC_NEW_SPARE2_SHUTDOWN_SUDDEN_PWR_LOSS;
	else if (shutdown_reason == SHUTDOWN_BY_UNKNOWN_REASONS)
		rtc_shutdown_reason = rtc_shutdown_reason | RTC_NEW_SPARE2_SHUTDOWN_UKNOWN;

	rtc_write(RTC_AL_DOW, rtc_shutdown_reason);
	rtc_write_trigger();
	rtc_release_lock();
	return 0;
}

static int (mtk_read_thermal_shutdown_reason)(life_cycle_reason_t *thermal_shutdown_reason)
{
	u16 rtc_thermal_shutdown_reason;

	rtc_acquire_lock();
	rtc_thermal_shutdown_reason = rtc_read(RTC_AL_MTH);
	rtc_release_lock();

	printk(KERN_ERR"%s: thermal shutdown reason 0x%x\n", __func__, rtc_thermal_shutdown_reason);
	if (rtc_thermal_shutdown_reason & RTC_NEW_SPARE3_THERMAL_SHUTDOWN_BATTERY)
		*thermal_shutdown_reason = THERMAL_SHUTDOWN_REASON_BATTERY;
	else if (rtc_thermal_shutdown_reason & RTC_NEW_SPARE3_THERMAL_SHUTDOWN_PMIC)
		*thermal_shutdown_reason = THERMAL_SHUTDOWN_REASON_PMIC;
	else if (rtc_thermal_shutdown_reason & RTC_NEW_SPARE3_THERMAL_SHUTDOWN_SOC)
		*thermal_shutdown_reason = THERMAL_SHUTDOWN_REASON_SOC;
	else if (rtc_thermal_shutdown_reason & RTC_NEW_SPARE3_THERMAL_SHUTDOWN_MODEM)
		*thermal_shutdown_reason = THERMAL_SHUTDOWN_REASON_MODEM;
	else if (rtc_thermal_shutdown_reason & RTC_NEW_SPARE3_THERMAL_SHUTDOWN_WIFI)
		*thermal_shutdown_reason = THERMAL_SHUTDOWN_REASON_WIFI;
	else if (rtc_thermal_shutdown_reason & RTC_NEW_SPARE3_THERMAL_SHUTDOWN_PCB)
		*thermal_shutdown_reason = THERMAL_SHUTDOWN_REASON_PCB;
	else if (rtc_thermal_shutdown_reason & RTC_NEW_SPARE3_THERMAL_SHUTDOWN_BTS)
		*thermal_shutdown_reason = THERMAL_SHUTDOWN_REASON_BTS;
        else if (rtc_thermal_shutdown_reason & RTC_NEW_SPARE3_THERMAL_SHUTDOWN_VS)
                *thermal_shutdown_reason = THERMAL_SHUTDOWN_REASON_VS;
	else {
		printk(KERN_ERR"Failed to read boot rtc boot reason\n");
		return -1;
	}

	return 0;
}

static int (mtk_write_thermal_shutdown_reason)(life_cycle_reason_t thermal_shutdown_reason)
{
	u16 rtc_thermal_shutdown_reason;

	rtc_acquire_lock();
	rtc_thermal_shutdown_reason = rtc_read(RTC_AL_MTH);

	printk(KERN_ERR"%s:shutdown_reason 0x%0x\n", __func__, rtc_thermal_shutdown_reason);


	if (thermal_shutdown_reason == THERMAL_SHUTDOWN_REASON_BATTERY)
		rtc_thermal_shutdown_reason = rtc_thermal_shutdown_reason | RTC_NEW_SPARE3_THERMAL_SHUTDOWN_BATTERY;
	else if (thermal_shutdown_reason == THERMAL_SHUTDOWN_REASON_PMIC)
		rtc_thermal_shutdown_reason = rtc_thermal_shutdown_reason | RTC_NEW_SPARE3_THERMAL_SHUTDOWN_PMIC;
	else if (thermal_shutdown_reason == THERMAL_SHUTDOWN_REASON_SOC)
		rtc_thermal_shutdown_reason = rtc_thermal_shutdown_reason | RTC_NEW_SPARE3_THERMAL_SHUTDOWN_SOC;
	else if (thermal_shutdown_reason == THERMAL_SHUTDOWN_REASON_MODEM)
		rtc_thermal_shutdown_reason = rtc_thermal_shutdown_reason | RTC_NEW_SPARE3_THERMAL_SHUTDOWN_MODEM;
	else if (thermal_shutdown_reason == THERMAL_SHUTDOWN_REASON_WIFI)
		rtc_thermal_shutdown_reason = rtc_thermal_shutdown_reason | RTC_NEW_SPARE3_THERMAL_SHUTDOWN_WIFI;
	else if (thermal_shutdown_reason == THERMAL_SHUTDOWN_REASON_PCB)
		rtc_thermal_shutdown_reason = rtc_thermal_shutdown_reason | RTC_NEW_SPARE3_THERMAL_SHUTDOWN_PCB;
	else if (thermal_shutdown_reason == THERMAL_SHUTDOWN_REASON_BTS)
		rtc_thermal_shutdown_reason = rtc_thermal_shutdown_reason | RTC_NEW_SPARE3_THERMAL_SHUTDOWN_BTS;
        else if (thermal_shutdown_reason == THERMAL_SHUTDOWN_REASON_VS)
                rtc_thermal_shutdown_reason = rtc_thermal_shutdown_reason | RTC_NEW_SPARE3_THERMAL_SHUTDOWN_VS;

	rtc_write(RTC_AL_MTH, rtc_thermal_shutdown_reason);
	rtc_write_trigger();
	rtc_release_lock();
	return 0;
}

static int (mtk_read_special_mode)(life_cycle_reason_t *special_mode)
{
	u16 rtc_smode;
	rtc_acquire_lock();
	rtc_smode = rtc_read(RTC_SPAR1);
	rtc_release_lock();

	printk(KERN_ERR"%s: special mode is 0x%x\n", __func__, rtc_smode);
	if (rtc_smode & RTC_SPAR0_SPECIAL_MODE_LOW_BATTERY)
		*special_mode = LIFE_CYCLE_SMODE_LOW_BATTERY;
	else if (rtc_smode & RTC_SPAR0_SPECIAL_MODE_WARM_BOOT_USB_CONNECTED)
		*special_mode = LIFE_CYCLE_SMODE_WARM_BOOT_USB_CONNECTED;
	else if (rtc_smode & RTC_SPAR0_SPECIAL_MODE_OTA)
		*special_mode = LIFE_CYCLE_SMODE_OTA;
	else if (rtc_smode & RTC_SPAR0_SPECIAL_MODE_FACTORY_RESET)
		*special_mode = LIFE_CYCLE_SMODE_FACTORY_RESET;
	else {
		printk(KERN_ERR"Failed to read boot rtc boot reason\n");
		return -1;
	}
	return 0;
}

static int (mtk_write_special_mode)(life_cycle_reason_t special_mode)
{
	u16 rtc_smode;

	rtc_acquire_lock();
	rtc_smode = rtc_read(RTC_SPAR1);

	printk(KERN_ERR"%s: special_mode 0x%x\n", __func__, rtc_smode);

	if (special_mode == LIFE_CYCLE_SMODE_LOW_BATTERY)
		rtc_smode = rtc_smode | RTC_SPAR0_SPECIAL_MODE_LOW_BATTERY;
	else if (special_mode == LIFE_CYCLE_SMODE_WARM_BOOT_USB_CONNECTED)
		rtc_smode = rtc_smode | RTC_SPAR0_SPECIAL_MODE_WARM_BOOT_USB_CONNECTED;
	else if (special_mode == LIFE_CYCLE_SMODE_OTA)
		rtc_smode = rtc_smode | RTC_SPAR0_SPECIAL_MODE_OTA;
	else if (special_mode == LIFE_CYCLE_SMODE_FACTORY_RESET)
		rtc_smode = rtc_smode | RTC_SPAR0_SPECIAL_MODE_FACTORY_RESET;

	rtc_write(RTC_SPAR1, rtc_smode);
	rtc_write_trigger();
	rtc_release_lock();
	return 0;
}

int rtc_mark_quiescent(int value)
{
	u16 rtc_special_mode;

	rtc_acquire_lock();
	rtc_special_mode = rtc_read(RTC_SPAR1);
	if (value == 0)
		rtc_special_mode &= ~PMIC_RTC_SPAR1_QUIESCENT;
	else
		rtc_special_mode |= PMIC_RTC_SPAR1_QUIESCENT;
	rtc_write(RTC_SPAR1, rtc_special_mode);
	rtc_write_trigger();
	rtc_release_lock();
	return 0;
}
EXPORT_SYMBOL(rtc_mark_quiescent);

int mtk_lcr_reset(void)
{
	u16 data;

	rtc_acquire_lock();
	/* clean up the boot reason */
	data = rtc_read(RTC_AL_DOM);
	data = data & ~RTC_NEW_SPARE1;
	rtc_write(RTC_AL_DOM, data);

	/* clean up the shutdown reason */
	data = rtc_read(RTC_AL_DOW);
	data = data & ~RTC_NEW_SPARE2;
	rtc_write(RTC_AL_DOW, data);

	/* clean up the thermal shutdown reason */
	data = rtc_read(RTC_AL_MTH);
	data = data & ~RTC_NEW_SPARE3;
	rtc_write(RTC_AL_MTH, data);

	/* clean up special mode */
	data = rtc_read(RTC_SPAR1);
	data = data & ~RTC_SPAR0_SPECIAL_MODE_MASK;
	rtc_write(RTC_SPAR1, data);

	rtc_write_trigger();
	rtc_release_lock();
	return 0;
}

int life_cycle_platform_init(sign_of_life_ops *sol_ops)
{
	printk(KERN_ERR "%s: Support MTK platform\n", __func__);
	sol_ops->read_boot_reason = mtk_read_boot_reason;
	sol_ops->write_boot_reason = mtk_write_boot_reason;
	sol_ops->read_shutdown_reason = mtk_read_shutdown_reason;
	sol_ops->write_shutdown_reason = mtk_write_shutdown_reason;
	sol_ops->read_thermal_shutdown_reason = mtk_read_thermal_shutdown_reason;
	sol_ops->write_thermal_shutdown_reason = mtk_write_thermal_shutdown_reason;
	sol_ops->read_special_mode = mtk_read_special_mode;
	sol_ops->write_special_mode = mtk_write_special_mode;
	sol_ops->lcr_reset = mtk_lcr_reset;

	return 0;
}


void kree_disable_fiq(int);
void sysrq_trigger_lcr_test(int act)
{
	unsigned long flags;
	DEFINE_SPINLOCK(wdt_test_lock);

	switch (act) {
	case 3:
		spin_lock_irqsave(&wdt_test_lock, flags);
		/* mtk_wdt_mode_config(0, 0, 1, 0, 1); */
		while (1) {
			;
		};
		break;
	case 2:
		spin_lock(&wdt_test_lock);
		while (1)
			;
		break;
	default:
		*(int *)NULL = 0;
		break;
	}
}
EXPORT_SYMBOL(sysrq_trigger_lcr_test);



