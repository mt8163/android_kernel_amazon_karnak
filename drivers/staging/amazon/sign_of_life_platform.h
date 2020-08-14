/*
* sign_of_life_platform.h
*
* platfrom specific lrc data header file
*
* Copyright (C) 2015-2017 Amazon Technologies Inc. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#ifndef __SIGN_OF_LIFE_PLATFORM_H
#define __SIGN_OF_LIFE_PLATFORM_H

/* platform specific lcr_data */
static struct sign_of_life_reason_data lcr_data[] = {
	/* The default value in case we fail to find a good reason */
	{LIFE_CYCLE_NOT_AVAILABLE, "Life Cycle Reason Not Available", "LCR_abnormal"},
	/* Normal software shutdown */
	{SHUTDOWN_BY_SW, "Software Shutdown", "LCR_normal"},
	/* Device shutdown due to long pressing of power key */
	{SHUTDOWN_BY_LONG_PWR_KEY_PRESS, "Long Pressed Power Key Shutdown", "LCR_abnormal"},
	/* Device shutdown due to overheated PMIC */
	{THERMAL_SHUTDOWN_REASON_PMIC, "PMIC Overheated Thermal Shutdown", "LCR_abnormal"},
	/* Device shutdown due to overheated battery */
	{THERMAL_SHUTDOWN_REASON_BATTERY, "Battery Overheated Thermal Shutdown", "LCR_abnormal"},
	/* Device shutdown due to overheated Soc chipset */
	{THERMAL_SHUTDOWN_REASON_SOC, "SOC Overheated Thermal Shutdown", "LCR_abnormal"},
	/* Device shutdown due to overheated PCB main board */
	{THERMAL_SHUTDOWN_REASON_PCB, "PCB Overheated Thermal Shutdown", "LCR_abnormal"},
	/* Device shutdown due to overheated WIFI chipset */
	{THERMAL_SHUTDOWN_REASON_WIFI, "WIFI Overheated Thermal Shutdown", "LCR_abnormal"},
	/* Device shutdown due to overheated BTS */
	{THERMAL_SHUTDOWN_REASON_BTS, "BTS Overheated Thermal Shutdown", "LCR_abnormal"},
	/* Device shutdown due to overheated Modem chipset */
	{THERMAL_SHUTDOWN_REASON_MODEM, "Modem Overheated Thermal Shutdown", "LCR_abnormal"},
        /* Device shutdown due to overheated virtual sensor */
        {THERMAL_SHUTDOWN_REASON_VS, "VS Overheated Thermal Shutdown", "LCR_abnormal"},
	/* Device shutdown due to extream low battery level */
	{LIFE_CYCLE_SMODE_LOW_BATTERY, "Low Battery Shutdown", "LCR_normal"},
	/* Device shutdown due to sudden power lost */
	{SHUTDOWN_BY_SUDDEN_POWER_LOSS, "Sudden Power Loss Shutdown", "LCR_abnormal"},
	/* Device shutdown due to unknown reason */
	{SHUTDOWN_BY_UNKNOWN_REASONS, "Unknown Shutdown", "LCR_abnormal"},
	/* Device powerup due to power key pressed */
	{COLDBOOT_BY_POWER_KEY, "Cold Boot By Power Key", "LCR_normal"},
	/* Device powerup due to USB cable plugged */
	{COLDBOOT_BY_USB, "Cold Boot By USB", "LCR_normal"},
	/* Device powerup due to power supply plugged */
	{COLDBOOT_BY_POWER_SUPPLY, "Cold Boot By Power Supply", "LCR_normal"},
	/* Device reboot due software reboot */
	{WARMBOOT_BY_SW, "Warm Boot By Software", "LCR_normal"},
	/* Device reboot due kernel panic */
	{WARMBOOT_BY_KERNEL_PANIC, "Warm Boot By Kernel Panic", "LCR_abnormal"},
	/* Device reboot due software watchdog timeout */
	{WARMBOOT_BY_KERNEL_WATCHDOG, "Warm Boot By Kernel Watchdog", "LCR_abnormal"},
	/* Device reboot due kernel HW watchdog timeout */
	{WARMBOOT_BY_HW_WATCHDOG, "Warm Boot By HW Watchdog", "LCR_abnormal"},
	/* Device powerup into charger mode */
	{LIFE_CYCLE_SMODE_WARM_BOOT_USB_CONNECTED, "Power Off Charging Mode", "LCR_normal"},
	/* Device reboot into factory reset mode */
	{LIFE_CYCLE_SMODE_FACTORY_RESET, "Factory Reset Reboot", "LCR_normal"},
	/* Device reboot into recovery OTA mode */
	{LIFE_CYCLE_SMODE_OTA, "OTA Reboot", "LCR_normal"},
};
#endif

