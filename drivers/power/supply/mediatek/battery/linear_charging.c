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

#include <linux/kernel.h>
#include <linux/types.h>

#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <mach/mt_charging.h>
#include <mt-plat/battery_common.h>
#include <mt-plat/battery_meter.h>
#include <mt-plat/charging.h>
#include <mt-plat/mt_boot.h>

/* ============================================================ // */
/* define */
/* ============================================================ // */
/* cut off to full */
#define POST_CHARGING_TIME (30 * 60)  /* 30mins */
#define CV_CHECK_DELAT_FOR_BANDGAP 80 /* 80mV */

/* ============================================================ // */
/* global variable */
/* ============================================================ // */
unsigned int g_bcct_flag;
int g_temp_CC_value = CHARGE_CURRENT_0_00_MA;
unsigned int g_usb_state = USB_UNCONFIGURED;
unsigned int charging_full_current; /* = CHARGING_FULL_CURRENT; */ /* mA */
unsigned int v_cc2topoff_threshold;   /* = V_CC2TOPOFF_THRES; */
unsigned int ulc_cv_charging_current; /* = AC_CHARGER_CURRENT; */
bool ulc_cv_charging_current_flag;
static bool usb_unlimited;
#if defined(CONFIG_MTK_HAFG_20)
unsigned int g_cv_voltage = BATTERY_VOLT_04_200000_V;
#endif

/* /////////////
 */
/* // JEITA */
/* /////////////
 */
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
int g_jeita_recharging_voltage = JEITA_RECHARGE_VOLTAGE;
int g_temp_status = TEMP_POS_10_TO_POS_45;
bool temp_error_recovery_chr_flag = true;
#endif

/* ============================================================ // */
static void __init_charging_varaibles(void)
{
	static int init_flag;

	if (init_flag == 0) {
		init_flag = 1;
		charging_full_current = batt_cust_data.charging_full_current;
		v_cc2topoff_threshold = batt_cust_data.v_cc2topoff_thres;
		ulc_cv_charging_current = batt_cust_data.ac_charger_current;
	}
}

void BATTERY_SetUSBState(int usb_state_value)
{
#if defined(CONFIG_POWER_EXT)
	pr_debug("[BATTERY_SetUSBState] in FPGA/EVB, no service\r\n");
#else
	if ((usb_state_value < USB_SUSPEND) ||
	    ((usb_state_value > USB_CONFIGURED))) {
		pr_debug(
			"[BATTERY] BAT_SetUSBState Fail! Restore to default value\r\n");
		usb_state_value = USB_UNCONFIGURED;
	} else {
		pr_debug("[BATTERY] BAT_SetUSBState Success! Set %d\r\n",
			 usb_state_value);
		g_usb_state = usb_state_value;
	}
#endif
}

unsigned int get_charging_setting_current(void)
{
	return g_temp_CC_value;
}

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)

static unsigned int select_jeita_cv(void)
{
	unsigned int cv_voltage;

	if (g_temp_status == TEMP_ABOVE_POS_60) {
		cv_voltage = JEITA_TEMP_ABOVE_POS_60_CV_VOLTAGE;
	} else if (g_temp_status == TEMP_POS_45_TO_POS_60) {
		cv_voltage = JEITA_TEMP_POS_45_TO_POS_60_CV_VOLTAGE;
	} else if (g_temp_status == TEMP_POS_10_TO_POS_45) {
		if (batt_cust_data.high_battery_voltage_support)
			cv_voltage = BATTERY_VOLT_04_350000_V;
		else
			cv_voltage = JEITA_TEMP_POS_10_TO_POS_45_CV_VOLTAGE;

	} else if (g_temp_status == TEMP_POS_0_TO_POS_10) {
		cv_voltage = JEITA_TEMP_POS_0_TO_POS_10_CV_VOLTAGE;
	} else if (g_temp_status == TEMP_NEG_10_TO_POS_0) {
		cv_voltage = JEITA_TEMP_NEG_10_TO_POS_0_CV_VOLTAGE;
	} else if (g_temp_status == TEMP_BELOW_NEG_10) {
		cv_voltage = JEITA_TEMP_BELOW_NEG_10_CV_VOLTAGE;
	} else {
		cv_voltage = BATTERY_VOLT_04_200000_V;
	}

	return cv_voltage;
}

unsigned int do_jeita_state_machine(void)
{
	int previous_g_temp_status;
	unsigned int cv_voltage;

	previous_g_temp_status = g_temp_status;
	/* JEITA battery temp Standard */
	if (BMT_status.temperature >= TEMP_POS_60_THRESHOLD) {
		pr_debug("[BATTERY] Battery Over high Temperature(%d) !!\n\r",
			 TEMP_POS_60_THRESHOLD);
		g_temp_status = TEMP_ABOVE_POS_60;
		return PMU_STATUS_FAIL;
	} else if (BMT_status.temperature > TEMP_POS_45_THRESHOLD) {
		if ((g_temp_status == TEMP_ABOVE_POS_60) &&
		    (BMT_status.temperature >=
		     TEMP_POS_60_THRES_MINUS_X_DEGREE)) {
			pr_debug(
				"[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
				TEMP_POS_60_THRES_MINUS_X_DEGREE,
				TEMP_POS_60_THRESHOLD);
			return PMU_STATUS_FAIL;
		} /*else {*/
		pr_debug(
			"[BATTERY] Battery Temperature between %d and %d !!\n\r",
			TEMP_POS_45_THRESHOLD, TEMP_POS_60_THRESHOLD);
		g_temp_status = TEMP_POS_45_TO_POS_60;
		g_jeita_recharging_voltage =
			JEITA_TEMP_POS_45_TO_POS_60_RECHARGE_VOLTAGE;
		v_cc2topoff_threshold =
			JEITA_TEMP_POS_45_TO_POS_60_CC2TOPOFF_THRESHOLD;
		charging_full_current = batt_cust_data.charging_full_current;
		/*}*/
	} else if (BMT_status.temperature >= TEMP_POS_10_THRESHOLD) {
		if (((g_temp_status == TEMP_POS_45_TO_POS_60) &&
		     (BMT_status.temperature >=
		      TEMP_POS_45_THRES_MINUS_X_DEGREE)) ||
		    ((g_temp_status == TEMP_POS_0_TO_POS_10) &&
		     (BMT_status.temperature <=
		      TEMP_POS_10_THRES_PLUS_X_DEGREE))) {
			pr_debug(
				"[BATTERY] Battery Temperature not recovery to normal temperature charging mode yet!!\n\r");
		} else {
			pr_debug(
				"[BATTERY] Battery Normal Temperature between %d and %d !!\n\r",
				TEMP_POS_10_THRESHOLD, TEMP_POS_45_THRESHOLD);

			g_temp_status = TEMP_POS_10_TO_POS_45;
		if (batt_cust_data.high_battery_voltage_support)
			g_jeita_recharging_voltage = 4200;
		else
			g_jeita_recharging_voltage =
			JEITA_TEMP_POS_10_TO_POS_45_RECHARGE_VOLTAGE;

			v_cc2topoff_threshold =
				JEITA_TEMP_POS_10_TO_POS_45_CC2TOPOFF_THRESHOLD;
			charging_full_current =
				batt_cust_data.charging_full_current;
		}
	} else if (BMT_status.temperature >= TEMP_POS_0_THRESHOLD) {
		if ((g_temp_status == TEMP_NEG_10_TO_POS_0 ||
		     g_temp_status == TEMP_BELOW_NEG_10) &&
		    (BMT_status.temperature <=
		     TEMP_POS_0_THRES_PLUS_X_DEGREE)) {
			if (g_temp_status == TEMP_NEG_10_TO_POS_0) {
				pr_debug(
					"[BATTERY] Battery Temperature between %d and %d !!\n\r",
					TEMP_POS_0_THRES_PLUS_X_DEGREE,
					TEMP_POS_10_THRESHOLD);
			}
			if (g_temp_status == TEMP_BELOW_NEG_10) {
				pr_debug(
					"[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
					TEMP_POS_0_THRESHOLD,
					TEMP_POS_0_THRES_PLUS_X_DEGREE);
				return PMU_STATUS_FAIL;
			}
		} else {
			pr_debug(
				"[BATTERY] Battery Temperature between %d and %d !!\n\r",
				TEMP_POS_0_THRESHOLD, TEMP_POS_10_THRESHOLD);
			g_temp_status = TEMP_POS_0_TO_POS_10;
			g_jeita_recharging_voltage =
				JEITA_TEMP_POS_0_TO_POS_10_RECHARGE_VOLTAGE;
			v_cc2topoff_threshold =
				JEITA_TEMP_POS_0_TO_POS_10_CC2TOPOFF_THRESHOLD;
			charging_full_current =
				batt_cust_data.charging_full_current;
		}
	} else if (BMT_status.temperature >= TEMP_NEG_10_THRESHOLD) {
		if ((g_temp_status == TEMP_BELOW_NEG_10) &&
		    (BMT_status.temperature <=
		     TEMP_NEG_10_THRES_PLUS_X_DEGREE)) {
			pr_debug(
				"[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
				TEMP_NEG_10_THRESHOLD,
				TEMP_NEG_10_THRES_PLUS_X_DEGREE);
			return PMU_STATUS_FAIL;
		} /*else {*/
		pr_debug(
			"[BATTERY] Battery Temperature between %d and %d !!\n\r",
			TEMP_NEG_10_THRESHOLD, TEMP_POS_0_THRESHOLD);
		g_temp_status = TEMP_NEG_10_TO_POS_0;
		g_jeita_recharging_voltage =
			JEITA_TEMP_NEG_10_TO_POS_0_RECHARGE_VOLTAGE;
		v_cc2topoff_threshold =
			JEITA_TEMP_NEG_10_TO_POS_0_CC2TOPOFF_THRESHOLD;
		charging_full_current = JEITA_NEG_10_TO_POS_0_FULL_CURRENT;
		/*}*/
	} else {
		pr_debug("[BATTERY] Battery below low Temperature(%d) !!\n\r",
			 TEMP_NEG_10_THRESHOLD);
		g_temp_status = TEMP_BELOW_NEG_10;
		return PMU_STATUS_FAIL;
	}

	/* set CV after temperature changed */
	if (g_temp_status != previous_g_temp_status) {
		cv_voltage = select_jeita_cv();
		battery_charging_control(CHARGING_CMD_SET_CV_VOLTAGE,
					 &cv_voltage);

#if defined(CONFIG_MTK_HAFG_20)
		g_cv_voltage = cv_voltage;
#endif
	}

	return PMU_STATUS_OK;
}

static void set_jeita_charging_current(void)
{
#ifdef CONFIG_USB_IF
	if (BMT_status.charger_type == STANDARD_HOST)
		return;
#endif

	if (g_temp_status == TEMP_NEG_10_TO_POS_0) {
		g_temp_CC_value = CHARGE_CURRENT_200_00_MA; /* for low temp */
		pr_debug("[BATTERY] JEITA set charging current : %d\r\n",
			 g_temp_CC_value);
	}
}

#endif

bool get_usb_current_unlimited(void)
{
	if (BMT_status.charger_type == STANDARD_HOST ||
	    BMT_status.charger_type == CHARGING_HOST)
		return usb_unlimited;
	else
		return false;
}

void set_usb_current_unlimited(bool enable)
{
	usb_unlimited = enable;
}

void select_charging_curret_bcct(void)
{
	/* done on set_bat_charging_current_limit */
}

unsigned int set_bat_charging_current_limit(int current_limit)
{
	pr_debug("[BATTERY] set_bat_charging_current_limit (%d)\r\n",
		 current_limit);

	if (current_limit != -1) {
		g_bcct_flag = 1;

		if (current_limit < 70)
			g_temp_CC_value = CHARGE_CURRENT_0_00_MA;
		else if (current_limit < 200)
			g_temp_CC_value = CHARGE_CURRENT_70_00_MA;
		else if (current_limit < 300)
			g_temp_CC_value = CHARGE_CURRENT_200_00_MA;
		else if (current_limit < 400)
			g_temp_CC_value = CHARGE_CURRENT_300_00_MA;
		else if (current_limit < 450)
			g_temp_CC_value = CHARGE_CURRENT_400_00_MA;
		else if (current_limit < 550)
			g_temp_CC_value = CHARGE_CURRENT_450_00_MA;
		else if (current_limit < 650)
			g_temp_CC_value = CHARGE_CURRENT_550_00_MA;
		else if (current_limit < 700)
			g_temp_CC_value = CHARGE_CURRENT_650_00_MA;
		else if (current_limit < 800)
			g_temp_CC_value = CHARGE_CURRENT_700_00_MA;
		else if (current_limit < 900)
			g_temp_CC_value = CHARGE_CURRENT_800_00_MA;
		else if (current_limit < 1000)
			g_temp_CC_value = CHARGE_CURRENT_900_00_MA;
		else if (current_limit < 1100)
			g_temp_CC_value = CHARGE_CURRENT_1000_00_MA;
		else if (current_limit < 1200)
			g_temp_CC_value = CHARGE_CURRENT_1100_00_MA;
		else if (current_limit < 1300)
			g_temp_CC_value = CHARGE_CURRENT_1200_00_MA;
		else if (current_limit < 1400)
			g_temp_CC_value = CHARGE_CURRENT_1300_00_MA;
		else if (current_limit < 1500)
			g_temp_CC_value = CHARGE_CURRENT_1400_00_MA;
		else if (current_limit < 1600)
			g_temp_CC_value = CHARGE_CURRENT_1500_00_MA;
		else if (current_limit == 1600)
			g_temp_CC_value = CHARGE_CURRENT_1600_00_MA;
		else
			g_temp_CC_value = CHARGE_CURRENT_450_00_MA;
	} else {
		/* change to default current setting */
		g_bcct_flag = 0;
	}

	wake_up_bat();

	return g_bcct_flag;
}

void set_bat_sw_cv_charging_current_limit(int current_limit)
{
	pr_debug("[BATTERY] set_bat_sw_cv_charging_current_limit (%d)\r\n",
		 current_limit);

	if (current_limit <= CHARGE_CURRENT_70_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_0_00_MA;
	else if (current_limit <= CHARGE_CURRENT_200_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_70_00_MA;
	else if (current_limit <= CHARGE_CURRENT_300_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_200_00_MA;
	else if (current_limit <= CHARGE_CURRENT_400_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_300_00_MA;
	else if (current_limit <= CHARGE_CURRENT_450_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_400_00_MA;
	else if (current_limit <= CHARGE_CURRENT_550_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_450_00_MA;
	else if (current_limit <= CHARGE_CURRENT_650_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_550_00_MA;
	else if (current_limit <= CHARGE_CURRENT_700_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_650_00_MA;
	else if (current_limit <= CHARGE_CURRENT_800_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_700_00_MA;
	else if (current_limit <= CHARGE_CURRENT_900_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_800_00_MA;
	else if (current_limit <= CHARGE_CURRENT_1000_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_900_00_MA;
	else if (current_limit <= CHARGE_CURRENT_1100_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_1000_00_MA;
	else if (current_limit <= CHARGE_CURRENT_1200_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_1100_00_MA;
	else if (current_limit <= CHARGE_CURRENT_1300_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_1200_00_MA;
	else if (current_limit <= CHARGE_CURRENT_1400_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_1300_00_MA;
	else if (current_limit <= CHARGE_CURRENT_1500_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_1400_00_MA;
	else if (current_limit <= CHARGE_CURRENT_1600_00_MA)
		ulc_cv_charging_current = CHARGE_CURRENT_1500_00_MA;
	else
		ulc_cv_charging_current = CHARGE_CURRENT_450_00_MA;
}

void select_charging_curret(void)
{
	if (g_ftm_battery_flag) {
		pr_debug("[BATTERY] FTM charging : %d\r\n",
			 charging_level_data[0]);
		g_temp_CC_value = charging_level_data[0];
	} else {
		if (BMT_status.charger_type == STANDARD_HOST) {
#ifdef CONFIG_USB_IF
			if (g_usb_state == USB_SUSPEND)
				g_temp_CC_value =
				batt_cust_data
				.usb_charger_current_suspend;
			else if (g_usb_state == USB_UNCONFIGURED)
				g_temp_CC_value =
				batt_cust_data
				.usb_charger_current_unconfigured;
			else if (g_usb_state == USB_CONFIGURED)
				g_temp_CC_value =
				batt_cust_data
				.usb_charger_current_configured;
			else
				g_temp_CC_value =
				batt_cust_data
				.usb_charger_current_unconfigured;

			pr_debug(
				"[BATTERY] STANDARD_HOST CC mode charging : %d on %d state\r\n",
				g_temp_CC_value, g_usb_state);
#else
			g_temp_CC_value = batt_cust_data.usb_charger_current;
#endif
		} else if (BMT_status.charger_type == NONSTANDARD_CHARGER)
			g_temp_CC_value =
				batt_cust_data.non_std_ac_charger_current;
		else if (BMT_status.charger_type == STANDARD_CHARGER)
			g_temp_CC_value = batt_cust_data.ac_charger_current;
		else if (BMT_status.charger_type == CHARGING_HOST)
			g_temp_CC_value =
				batt_cust_data.charging_host_charger_current;
		else if (BMT_status.charger_type == APPLE_2_1A_CHARGER)
			g_temp_CC_value =
				batt_cust_data.apple_2_1a_charger_current;
		else if (BMT_status.charger_type == APPLE_1_0A_CHARGER)
			g_temp_CC_value =
				batt_cust_data.apple_1_0a_charger_current;
		else if (BMT_status.charger_type == APPLE_0_5A_CHARGER)
			g_temp_CC_value =
				batt_cust_data.apple_0_5a_charger_current;
		else
			g_temp_CC_value = CHARGE_CURRENT_70_00_MA;

		pr_debug("[BATTERY] Default CC mode charging : %d\r\n",
			 g_temp_CC_value);

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		set_jeita_charging_current();
#endif
	}
}

static unsigned int charging_full_check(void)
{
	unsigned int status = false;
#if defined(POST_TIME_ENABLE)
	static unsigned int post_charging_time;

	if (post_charging_time >= POST_CHARGING_TIME) {
		status = true;
		post_charging_time = 0;

		pr_debug(
			"[BATTERY] Battery real full and disable charging on %d mA\n",
			BMT_status.ICharging);
	} else if (post_charging_time > 0) {
		post_charging_time += BAT_TASK_PERIOD;
		pr_debug(
			"[BATTERY] post_charging_time=%d,POST_CHARGING_TIME=%d\n",
			post_charging_time, POST_CHARGING_TIME);
	} else if ((BMT_status.TOPOFF_charging_time > 60) &&
		   (BMT_status.ICharging <= charging_full_current)) {
		post_charging_time = BAT_TASK_PERIOD;
		pr_debug(
			"[BATTERY] Enter Post charge, post_charging_time=%d,POST_CHARGING_TIME=%d\n",
			post_charging_time, POST_CHARGING_TIME);
	} else
		post_charging_time = 0;
#else
	static unsigned char full_check_count;

	if (BMT_status.ICharging <= charging_full_current) {
		full_check_count++;
		if (full_check_count == 6) {
			status = true;
			full_check_count = 0;
			pr_debug(
				"[BATTERY] Battery full and disable charging on %d mA\n",
				BMT_status.ICharging);
		}
	} else {
		full_check_count = 0;
	}
#endif

	return status;
}

static void charging_current_calibration(void)
{
	signed int bat_isense_offset;
#if 0
	signed int bat_vol = battery_meter_get_battery_voltage();
	signed int Vsense = battery_meter_get_VSense();

	bat_isense_offset = bat_vol - Vsense;

	pr_debug("[BATTERY] bat_vol=%d, Vsense=%d, offset=%d \r\n",
		bat_vol, Vsense, bat_isense_offset);
#else
	bat_isense_offset = 0;
#endif

	battery_meter_sync(bat_isense_offset);
}

static void pchr_sw_cv_charing_current_check(void)
{
	bool charging_enable = true;
	unsigned int csdac_full_flag = true;

	battery_charging_control(CHARGING_CMD_SET_CURRENT,
				 &ulc_cv_charging_current);
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	msleep(192);

	battery_charging_control(CHARGING_CMD_GET_CSDAC_FALL_FLAG,
				 &csdac_full_flag);

	if (csdac_full_flag == true)
		ulc_cv_charging_current =
			battery_meter_get_charging_current() * 100;

	while (csdac_full_flag == true &&
	       ulc_cv_charging_current != CHARGE_CURRENT_0_00_MA) {
		set_bat_sw_cv_charging_current_limit(ulc_cv_charging_current);
		battery_charging_control(CHARGING_CMD_SET_CURRENT,
					 &ulc_cv_charging_current);
		ulc_cv_charging_current_flag = true;

		msleep(192); /* large than 512 code x 0.25ms */

		battery_charging_control(CHARGING_CMD_GET_CSDAC_FALL_FLAG,
					 &csdac_full_flag);

		pr_debug(
			"[BATTERY] Sw CV set charging current, csdac_full_flag=%d, current=%d !\n",
			csdac_full_flag, ulc_cv_charging_current);
	}

	if (ulc_cv_charging_current == CHARGE_CURRENT_0_00_MA)
		pr_debug("[BATTERY] Sw CV set charging current Error!\n");
}

static void pchr_turn_on_charging(void)
{
#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	unsigned int cv_voltage;
#endif
	unsigned int charging_enable = true;

	pr_debug("[BATTERY] pchr_turn_on_charging()!\r\n");

	if (BMT_status.bat_charging_state == CHR_ERROR) {
		pr_debug("[BATTERY] Charger Error, turn OFF charging !\n");
		charging_enable = false;
	} else if ((g_platform_boot_mode == META_BOOT) ||
		   (g_platform_boot_mode == ADVMETA_BOOT)) {
		pr_debug(
			"[BATTERY] In meta or advanced meta mode, disable charging.\n");
		charging_enable = false;
	} else {
		/*HW initialization */
		pr_debug("charging_hw_init\n");
		battery_charging_control(CHARGING_CMD_INIT, NULL);

		/* Set Charging Current */
		if (get_usb_current_unlimited()) {
			g_temp_CC_value = batt_cust_data.ac_charger_current;
			pr_debug(
				"USB_CURRENT_UNLIMITED, use AC_CHARGER_CURRENT\n");
		} else {
			if (g_bcct_flag == 1) {
				pr_debug(
					"[BATTERY] select_charging_curret_bcct !\n");
				select_charging_curret_bcct();
			} else {
				pr_debug(
					"[BATTERY] select_charging_current !\n");
				select_charging_curret();
			}
		}

		if (g_temp_CC_value == CHARGE_CURRENT_0_00_MA) {
			charging_enable = false;
			pr_debug(
				"[BATTERY] charging current is set 0mA, turn off charging !\r\n");
		} else {
			if (ulc_cv_charging_current_flag == true)
				battery_charging_control(
					CHARGING_CMD_SET_CURRENT,
					&ulc_cv_charging_current);
			else
				battery_charging_control(
					CHARGING_CMD_SET_CURRENT,
					&g_temp_CC_value);

/* Set CV */
#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
			if (batt_cust_data.high_battery_voltage_support)
				cv_voltage = BATTERY_VOLT_04_350000_V;
			else
				cv_voltage = BATTERY_VOLT_04_200000_V;

			battery_charging_control(CHARGING_CMD_SET_CV_VOLTAGE,
						 &cv_voltage);
#if defined(CONFIG_MTK_HAFG_20)
			g_cv_voltage = cv_voltage;
#endif
#endif
		}
	}

	/* enable/disable charging */
	pr_debug("[BATTERY] pchr_turn_on_charging(), enable =%d \r\n",
		 charging_enable);
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
}

unsigned int BAT_PreChargeModeAction(void)
{
	pr_debug("[BATTERY] Pre-CC mode charge, timer=%d on %d !!\n\r",
		 BMT_status.PRE_charging_time, BMT_status.total_charging_time);

	BMT_status.PRE_charging_time += BAT_TASK_PERIOD;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.total_charging_time += BAT_TASK_PERIOD;

	select_charging_curret();
	ulc_cv_charging_current = g_temp_CC_value;
	ulc_cv_charging_current_flag = false;

	if (BMT_status.UI_SOC == 100) {
		BMT_status.bat_charging_state = CHR_BATFULL;
		BMT_status.bat_full = true;
		g_charging_full_reset_bat_meter = true;
	} else if (BMT_status.bat_vol > batt_cust_data.v_pre2cc_thres)
		BMT_status.bat_charging_state = CHR_CC;
	{
		bool charging_enable = false;

		/*Charging 9s and discharging 1s : start */
		battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
		msleep(1000);
	}

	charging_current_calibration();
	pchr_turn_on_charging();

	return PMU_STATUS_OK;
}

unsigned int BAT_ConstantCurrentModeAction(void)
{
	pr_debug("[BATTERY] CC mode charge, timer=%d on %d !!\n\r",
		 BMT_status.CC_charging_time, BMT_status.total_charging_time);

	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time += BAT_TASK_PERIOD;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.total_charging_time += BAT_TASK_PERIOD;

	ulc_cv_charging_current_flag = false;
	ulc_cv_charging_current = g_temp_CC_value;

	if (BMT_status.bat_vol > v_cc2topoff_threshold)
		BMT_status.bat_charging_state = CHR_TOP_OFF;

	{
		bool charging_enable = false;

		/* Charging 9s and discharging 1s : start */
		battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
		msleep(1000);
	}

	charging_current_calibration();

	pchr_turn_on_charging();

	return PMU_STATUS_OK;
}

unsigned int BAT_TopOffModeAction(void)
{
	unsigned int charging_enable = false;
	unsigned int cv_voltage;

	if (batt_cust_data.high_battery_voltage_support)
		cv_voltage = 4350;
	else
		cv_voltage = 4200;

	pr_debug("[BATTERY] Top Off mode charge, timer=%d on %d !!\n\r",
		 BMT_status.TOPOFF_charging_time,
		 BMT_status.total_charging_time);

	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time += BAT_TASK_PERIOD;
	BMT_status.total_charging_time += BAT_TASK_PERIOD;

	if (BMT_status.bat_vol > (cv_voltage - CV_CHECK_DELAT_FOR_BANDGAP)) {
		/* CV - 0.08V */
		pchr_sw_cv_charing_current_check();
	}
	pchr_turn_on_charging();

	if ((BMT_status.TOPOFF_charging_time >= MAX_CV_CHARGING_TIME) ||
	    (charging_full_check() == true)) {
		BMT_status.bat_charging_state = CHR_BATFULL;
		BMT_status.bat_full = true;
		g_charging_full_reset_bat_meter = true;

		/*  Disable charging */
		battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
	}

	return PMU_STATUS_OK;
}

unsigned int BAT_BatteryFullAction(void)
{
	unsigned int charging_enable = false;

	pr_debug("[BATTERY] Battery full !!\n\r");

	BMT_status.bat_full = true;
	BMT_status.total_charging_time = 0;
	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.POSTFULL_charging_time = 0;
	BMT_status.bat_in_recharging_state = false;

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	if (BMT_status.bat_vol < g_jeita_recharging_voltage)
#else
	if (BMT_status.bat_vol < batt_cust_data.recharging_voltage)
#endif
	{
		pr_debug(
			"[BATTERY] Battery Enter Re-charging!! , vbat=(%d)\n\r",
			BMT_status.bat_vol);

		BMT_status.bat_in_recharging_state = true;
		BMT_status.bat_charging_state = CHR_CC;
		ulc_cv_charging_current = g_temp_CC_value;
		ulc_cv_charging_current_flag = false;
	}

	/*  Disable charging */
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	return PMU_STATUS_OK;
}

unsigned int BAT_BatteryHoldAction(void)
{
	unsigned int charging_enable;

	pr_debug("[BATTERY] Hold mode !!\n\r");

	if (BMT_status.bat_vol < batt_cust_data.talking_recharge_voltage ||
	    g_call_state == CALL_IDLE) {
		BMT_status.bat_charging_state = CHR_CC;
		pr_debug("[BATTERY] Exit Hold mode and Enter CC mode !!\n\r");
	}

	/*  Disable charger */
	charging_enable = false;
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	return PMU_STATUS_OK;
}

unsigned int BAT_BatteryStatusFailAction(void)
{
	unsigned int charging_enable;

	pr_debug("[BATTERY] BAD Battery status... Charging Stop !!\n\r");

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	if ((g_temp_status == TEMP_ABOVE_POS_60) ||
	    (g_temp_status == TEMP_BELOW_NEG_10))
		temp_error_recovery_chr_flag = false;

	if ((temp_error_recovery_chr_flag == false) &&
	    (g_temp_status != TEMP_ABOVE_POS_60) &&
	    (g_temp_status != TEMP_BELOW_NEG_10)) {
		temp_error_recovery_chr_flag = true;
		BMT_status.bat_charging_state = CHR_PRE;
	}
#endif

	BMT_status.total_charging_time = 0;
	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.POSTFULL_charging_time = 0;

	/*  Disable charger */
	charging_enable = false;
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	return PMU_STATUS_OK;
}

void mt_battery_charging_algorithm(void)
{
	__init_charging_varaibles();
	switch (BMT_status.bat_charging_state) {
	case CHR_PRE:
		BAT_PreChargeModeAction();
		break;

	case CHR_CC:
		BAT_ConstantCurrentModeAction();
		break;

	case CHR_TOP_OFF:
		BAT_TopOffModeAction();
		break;

	case CHR_BATFULL:
		BAT_BatteryFullAction();
		break;

	case CHR_HOLD:
		BAT_BatteryHoldAction();
		break;

	case CHR_ERROR:
		BAT_BatteryStatusFailAction();
		break;
	}
}
