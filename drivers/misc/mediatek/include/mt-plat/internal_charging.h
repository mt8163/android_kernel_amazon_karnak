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

#ifndef INTERNAL_CHARGING_H
#define INTERNAL_CHARGING_H

/* ============================================================ */
/* define */
/* ============================================================ */


/* ============================================================ */
/* ENUM */
/* ============================================================ */


/* ============================================================ */
/* structure */
/* ============================================================ */


/* ============================================================ */
/* typedef */
/* ============================================================ */


/* ============================================================ */
/* External Variables */
/* ============================================================ */


/* ============================================================ */
/* External function */
/* ============================================================ */
extern int batt_internal_charger_detect(void);
extern void mt_battery_charging_algorithm_internal(void);
extern signed int chr_control_interface_internal(
	CHARGING_CTRL_CMD cmd, void *data);
extern void BATTERY_SetUSBState_internal(int usb_state);
extern unsigned int get_charging_setting_current_internal(void);
extern PMU_STATUS do_jeita_state_machine_internal(void);
extern bool get_usb_current_unlimited_internal(void);
extern void set_usb_current_unlimited_internal(bool enable);
extern void select_charging_curret_bcct_internal(void);
extern unsigned int set_bat_charging_current_limit_internal(int current_limit);
extern int get_bat_charging_current_limit_internal(void);
extern void mt_battery_charging_algorithm_internal(void);
#if defined(CONFIG_MTK_BQ24296_SUPPORT) || defined(CONFIG_MTK_BQ24297_SUPPORT)
extern unsigned int bq24297_get_pn(void);
#endif
#endif				/* #ifndef _CHARGING_H */
