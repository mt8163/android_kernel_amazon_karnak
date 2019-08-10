/*****************************************************************************
 *
 * Filename:
 * ---------
 *    internal_charging.h
 *
 * Project:
 * --------
 *   Maui_Software
 *
 * Description:
 * ------------
 *   This Module defines internal charger function.
 *
 * Author:
 * -------
 *
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * Revision:   1.0
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
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
