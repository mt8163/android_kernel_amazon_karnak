/*****************************************************************************
 *
 * Filename:
 * ---------
 *    switch_charging.c
 *
 * Project:
 * --------
 *   ALPS_Software
 *
 * Description:
 * ------------
 *   This file implements the interface between BMT and ADC scheduler.
 *
 * Author:
 * -------
 *  Oscar Liu
 *
 *============================================================================
 * Revision:   1.0
 * Modtime:   11 Aug 2005 10:28:16
 * Log:   //mtkvs01/vmdata/Maui_sw/archives/mcu/hal/peripheral/inc/bmt_chr_setting.h-arc
 *
 * 03 05 2015 wy.chuang
 * [ALPS01921641] [L1_merge] for PMIC and charging
 * .
 *             HISTORY
 * Below this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <mt-plat/battery_meter.h>
#include <mt-plat/battery_common.h>
#include <mt-plat/charging.h>
#include <mt-plat/mt_boot.h>
#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#endif

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#if !defined(TA_AC_CHARGING_CURRENT)
#include <mach/mt_pe.h>
#endif
#endif

#ifdef CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT
#include <mach/diso.h>
#endif

#if defined(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT)
#include <mt-plat/internal_charging.h>
#endif
/* ============================================================ // */
/* define */
/* ============================================================ // */
/* cut off to full */
#define POST_CHARGING_TIME (30*60)	/* 30mins */
#define FULL_CHECK_TIMES 6

/* ============================================================ // */
/* global variable */
/* ============================================================ // */
static unsigned int g_bcct_flag;
static unsigned int g_bcct_value;
static unsigned int g_full_check_count;
static CHR_CURRENT_ENUM g_temp_CC_value = CHARGE_CURRENT_0_00_MA;
static CHR_CURRENT_ENUM g_temp_input_CC_value = CHARGE_CURRENT_0_00_MA;
static unsigned int g_usb_state = USB_UNCONFIGURED;
static bool usb_unlimited;

 /* ///////////////////////////////////////////////////////////////////////////////////////// */
 /* // PUMP EXPRESS */
 /* ///////////////////////////////////////////////////////////////////////////////////////// */
#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
struct wake_lock TA_charger_suspend_lock;
bool ta_check_chr_type = true;
bool ta_cable_out_occur = false;
bool is_ta_connect = false;
bool ta_vchr_tuning = true;
int ta_v_chr_org = 0;
#endif

 /* ///////////////////////////////////////////////////////////////////////////////////////// */
 /* // JEITA */
 /* ///////////////////////////////////////////////////////////////////////////////////////// */
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
static int g_temp_status = TEMP_POS_10_TO_POS_45;
static bool temp_error_recovery_chr_flag = true;
#endif

/* ============================================================ // */
/* function prototype */
/* ============================================================ // */

/* ============================================================ // */
/* extern variable */
/* ============================================================ // */
/*extern int g_platform_boot_mode; moved to battery_common.h*/

/* ============================================================ // */
/* extern function */
/* ============================================================ // */

/* ============================================================ // */
void BATTERY_SetUSBState(int usb_state_value)
{
#if defined(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT)
	if (batt_internal_charger_detect())
		return BATTERY_SetUSBState_internal(usb_state_value);
#endif

#if defined(CONFIG_POWER_EXT)
	pr_debug("[BATTERY_SetUSBState] in FPGA/EVB, no service\r\n");
#else
	if ((usb_state_value < USB_SUSPEND) || ((usb_state_value > USB_CONFIGURED))) {
		pr_debug("[BATTERY] BAT_SetUSBState Fail! Restore to default value\r\n");
		usb_state_value = USB_UNCONFIGURED;
	} else {
		pr_debug("[BATTERY] BAT_SetUSBState Success! Set %d\r\n", usb_state_value);
		g_usb_state = usb_state_value;
	}
#endif
}


unsigned int get_charging_setting_current(void)
{
#if defined(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT)
		if (batt_internal_charger_detect())
			return get_charging_setting_current_internal();
#endif

	return g_temp_CC_value;
}

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
static DEFINE_MUTEX(ta_mutex);

static void set_ta_charging_current(void)
{
	int real_v_chrA = 0;

	real_v_chrA = battery_meter_get_charger_voltage();
	pr_debug("set_ta_charging_current, chrA=%d, chrB=%d\n",
		    ta_v_chr_org, real_v_chrA);

	if ((real_v_chrA - ta_v_chr_org) > 3000) {
		g_temp_input_CC_value = TA_AC_9V_INPUT_CURRENT;	/* TA = 9V */
		g_temp_CC_value = TA_AC_CHARGING_CURRENT;
	} else if ((real_v_chrA - ta_v_chr_org) > 1000) {
		g_temp_input_CC_value = TA_AC_7V_INPUT_CURRENT;	/* TA = 7V */
		g_temp_CC_value = TA_AC_CHARGING_CURRENT;
	}
}

static void mtk_ta_reset_vchr(void)
{
	CHR_CURRENT_ENUM chr_current = CHARGE_CURRENT_70_00_MA;

	battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT, &chr_current);
	msleep(250);		/* reset Vchr to 5V */

	pr_debug("mtk_ta_reset_vchr(): reset Vchr to 5V\n");
}

static void mtk_ta_increase(void)
{
	bool ta_current_pattern = true;	/* TRUE = increase */

	if (ta_cable_out_occur == false) {
		battery_charging_control(CHARGING_CMD_SET_TA_CURRENT_PATTERN, &ta_current_pattern);
	} else {
		ta_check_chr_type = true;
		pr_debug("mtk_ta_increase() Cable out\n");
	}
}

static bool mtk_ta_retry_increase(void)
{
	int real_v_chrA;
	int real_v_chrB;
	bool retransmit = true;
	unsigned int retransmit_count = 0;

	do {
		real_v_chrA = battery_meter_get_charger_voltage();
		mtk_ta_increase();	/* increase TA voltage to 7V */
		real_v_chrB = battery_meter_get_charger_voltage();

		if (real_v_chrB - real_v_chrA >= 1000) /* 1.0V */
			retransmit = false;
		else {
			retransmit_count++;
			pr_debug("mtk_ta_detector(): retransmit_count =%d, chrA=%d, chrB=%d\n",
				    retransmit_count, real_v_chrA, real_v_chrB);
		}

		if ((retransmit_count == 3) || (BMT_status.charger_exist == false))
			retransmit = false;

	} while ((retransmit == true) && (ta_cable_out_occur == false));

	pr_debug("mtk_ta_retry_increase() real_v_chrA=%d, real_v_chrB=%d, retry=%d\n",
		    real_v_chrA, real_v_chrB, retransmit_count);

	if (retransmit_count == 3)
		return false;
	else
		return true;
}

static void mtk_ta_detector(void)
{
	int real_v_chrB = 0;

	pr_debug("mtk_ta_detector() start\n");

	ta_v_chr_org = battery_meter_get_charger_voltage();
	mtk_ta_retry_increase();
	real_v_chrB = battery_meter_get_charger_voltage();

	if (real_v_chrB - ta_v_chr_org >= 1000)
		is_ta_connect = true;
	else
		is_ta_connect = false;

	pr_debug("mtk_ta_detector() end, is_ta_connect=%d\n", is_ta_connect);
}

static void mtk_ta_init(void)
{
	is_ta_connect = false;
	ta_cable_out_occur = false;

#ifdef TA_9V_SUPPORT
	ta_vchr_tuning = false;
#endif

	battery_charging_control(CHARGING_CMD_INIT, NULL);
}

static void battery_pump_express_charger_check(void)
{
	if (true == ta_check_chr_type &&
	    STANDARD_CHARGER == BMT_status.charger_type &&
	    BMT_status.SOC >= TA_START_BATTERY_SOC && BMT_status.SOC < TA_STOP_BATTERY_SOC) {

		mutex_lock(&ta_mutex);
		wake_lock(&TA_charger_suspend_lock);

		mtk_ta_reset_vchr();

		mtk_ta_init();
		mtk_ta_detector();

		/* need to re-check if the charger plug out during ta detector */
		if (true == ta_cable_out_occur)
			ta_check_chr_type = true;
		else
			ta_check_chr_type = false;

		wake_unlock(&TA_charger_suspend_lock);
		mutex_unlock(&ta_mutex);
	} else {
		pr_debug(
		    "Stop battery_pump_express_charger_check, SOC=%d, ta_check_chr_type = %d, charger_type = %d\n",
		    BMT_status.SOC, ta_check_chr_type, BMT_status.charger_type);
	}
}

static void battery_pump_express_algorithm_start(void)
{
	signed int charger_vol;
	unsigned int charging_enable = false;

	mutex_lock(&ta_mutex);
	wake_lock(&TA_charger_suspend_lock);

	if (true == is_ta_connect) {
		/* check cable impedance */
		charger_vol = battery_meter_get_charger_voltage();
		if (false == ta_vchr_tuning) {
			mtk_ta_retry_increase();	/* increase TA voltage to 9V */
			charger_vol = battery_meter_get_charger_voltage();
			ta_vchr_tuning = true;
		} else if (BMT_status.SOC > TA_STOP_BATTERY_SOC) {
			/* disable charging, avoid Iterm issue */
			battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
			mtk_ta_reset_vchr();	/* decrease TA voltage to 5V */
			charger_vol = battery_meter_get_charger_voltage();
			if (abs(charger_vol - ta_v_chr_org) <= 1000)	/* 1.0V */
				is_ta_connect = false;

			pr_debug(
				"Stop battery_pump_express_algorithm, SOC=%d is_ta_connect =%d, TA_STOP_BATTERY_SOC: %d\n",
				BMT_status.SOC, is_ta_connect, TA_STOP_BATTERY_SOC);
		}
		pr_debug("[BATTERY] check cable impedance, VA(%d) VB(%d) delta(%d).\n",
			    ta_v_chr_org, charger_vol, charger_vol - ta_v_chr_org);

		pr_debug("mtk_ta_algorithm() end\n");
	} else {
		pr_debug("It's not a TA charger, bypass TA algorithm\n");
	}

	wake_unlock(&TA_charger_suspend_lock);
	mutex_unlock(&ta_mutex);
}
#endif

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)

static BATTERY_VOLTAGE_ENUM select_jeita_cv(void)
{
	BATTERY_VOLTAGE_ENUM cv_voltage;

	if (g_temp_status == TEMP_ABOVE_POS_60)
		cv_voltage = batt_cust_data.jeita_temp_above_pos_60_cv_voltage;
	else if (g_temp_status == TEMP_POS_45_TO_POS_60)
		cv_voltage = batt_cust_data.jeita_temp_pos_45_to_pos_60_cv_voltage;
	else if (g_temp_status == TEMP_POS_10_TO_POS_45)
		cv_voltage = batt_cust_data.jeita_temp_pos_10_to_pos_45_cv_voltage;
	else if (g_temp_status == TEMP_POS_0_TO_POS_10)
		cv_voltage = batt_cust_data.jeita_temp_pos_0_to_pos_10_cv_voltage;
	else if (g_temp_status == TEMP_NEG_10_TO_POS_0)
		cv_voltage = batt_cust_data.jeita_temp_neg_10_to_pos_0_cv_voltage;
	else if (g_temp_status == TEMP_BELOW_NEG_10)
		cv_voltage = batt_cust_data.jeita_temp_below_neg_10_cv_voltage;
	else {
		if (batt_cust_data.high_battery_voltage_support)
			cv_voltage = BATTERY_VOLT_04_350000_V;
		else
			cv_voltage = BATTERY_VOLT_04_200000_V;
	}

	if (g_custom_charging_cv != -1)
		cv_voltage = g_custom_charging_cv;

	if (g_custom_charging_mode) /* For demo unit */
		cv_voltage = BATTERY_VOLT_04_100000_V;

	pr_info("[%s] CV(%d) custom CV(%d)\r\n",
		__func__, cv_voltage, g_custom_charging_cv);

	return cv_voltage;
}

PMU_STATUS do_jeita_state_machine(void)
{
	BATTERY_VOLTAGE_ENUM cv_voltage;
	PMU_STATUS jeita_status = PMU_STATUS_OK;

#if defined(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT)
	if (batt_internal_charger_detect())
		return do_jeita_state_machine_internal();
#endif

	/* JEITA battery temp Standard */

	if (BMT_status.temperature >= TEMP_POS_60_THRESHOLD) {
		pr_debug("[BATTERY] Battery Over high Temperature(%d) !!\n\r",
			    TEMP_POS_60_THRESHOLD);

		g_temp_status = TEMP_ABOVE_POS_60;

		return PMU_STATUS_FAIL;
	} else if (BMT_status.temperature > TEMP_POS_45_THRESHOLD) {	/* control 45c to normal behavior */
		if ((g_temp_status == TEMP_ABOVE_POS_60)
		    && (BMT_status.temperature >= TEMP_POS_60_THRES_MINUS_X_DEGREE)) {
			pr_debug(
			    "[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
			    TEMP_POS_60_THRES_MINUS_X_DEGREE, TEMP_POS_60_THRESHOLD);

			jeita_status = PMU_STATUS_FAIL;
		} else {
			pr_debug("[BATTERY] Battery Temperature between %d and %d !!\n\r",
				    TEMP_POS_45_THRESHOLD, TEMP_POS_60_THRESHOLD);

			g_temp_status = TEMP_POS_45_TO_POS_60;
		}
	} else if (BMT_status.temperature >= TEMP_POS_10_THRESHOLD) {
		if (((g_temp_status == TEMP_POS_45_TO_POS_60)
		     && (BMT_status.temperature >= TEMP_POS_45_THRES_MINUS_X_DEGREE))
		    || ((g_temp_status == TEMP_POS_0_TO_POS_10)
			&& (BMT_status.temperature <= TEMP_POS_10_THRES_PLUS_X_DEGREE))) {
			pr_debug(
				"[BATTERY] Battery Temperature not recovery to normal temperature charging mode yet!!\n\r");
		} else {
			pr_debug("[BATTERY] Battery Normal Temperature between %d and %d !!\n\r",
				    TEMP_POS_10_THRESHOLD, TEMP_POS_45_THRESHOLD);
			g_temp_status = TEMP_POS_10_TO_POS_45;
		}
	} else if (BMT_status.temperature >= TEMP_POS_0_THRESHOLD) {
		if ((g_temp_status == TEMP_NEG_10_TO_POS_0 || g_temp_status == TEMP_BELOW_NEG_10)
		    && (BMT_status.temperature <= TEMP_POS_0_THRES_PLUS_X_DEGREE)) {
			if (g_temp_status == TEMP_NEG_10_TO_POS_0) {
				pr_debug("[BATTERY] Battery Temperature between %d and %d !!\n\r",
					    TEMP_POS_0_THRES_PLUS_X_DEGREE, TEMP_POS_10_THRESHOLD);
			}
			if (g_temp_status == TEMP_BELOW_NEG_10) {
				pr_debug(
				    "[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
				    TEMP_POS_0_THRESHOLD, TEMP_POS_0_THRES_PLUS_X_DEGREE);
				return PMU_STATUS_FAIL;
			}
		} else {
			pr_debug("[BATTERY] Battery Temperature between %d and %d !!\n\r",
				    TEMP_POS_0_THRESHOLD, TEMP_POS_10_THRESHOLD);

			g_temp_status = TEMP_POS_0_TO_POS_10;
		}
	} else if (BMT_status.temperature >= TEMP_NEG_10_THRESHOLD) {
		pr_debug("[BATTERY] Battery Temperature between %d and %d !!\n\r",
			    TEMP_NEG_10_THRESHOLD, TEMP_POS_0_THRESHOLD);

		g_temp_status = TEMP_NEG_10_TO_POS_0;
		jeita_status = PMU_STATUS_FAIL;
	} else {
		pr_debug("[BATTERY] Battery below low Temperature(%d) !!\n\r",
			    TEMP_NEG_10_THRESHOLD);
		g_temp_status = TEMP_BELOW_NEG_10;

		jeita_status = PMU_STATUS_FAIL;
	}

	/* set CV after temperature changed */
	cv_voltage = select_jeita_cv();
	battery_charging_control(CHARGING_CMD_SET_CV_VOLTAGE, &cv_voltage);

	return jeita_status;
}


static void set_jeita_charging_current(void)
{
	static bool above_4000 = false;
#ifdef CONFIG_USB_IF
	if (BMT_status.charger_type == STANDARD_HOST)
		return;
#endif

	if (g_temp_status == TEMP_NEG_10_TO_POS_0
		|| g_temp_status == TEMP_BELOW_NEG_10
		|| g_temp_status == TEMP_ABOVE_POS_60)
		g_temp_CC_value = CHARGE_CURRENT_0_00_MA;
	else if (g_temp_status == TEMP_POS_0_TO_POS_10) {
		if (BMT_status.bat_vol > 4000) {
			g_temp_CC_value = CHARGE_CURRENT_500_00_MA;
			above_4000 = true;
		} else if(BMT_status.bat_vol > 3900 && above_4000 == true) {
			g_temp_CC_value = CHARGE_CURRENT_500_00_MA;
		} else {
			g_temp_CC_value = (CHARGE_CURRENT_800_00_MA > g_temp_CC_value) ? g_temp_CC_value : CHARGE_CURRENT_800_00_MA;
			above_4000 = false;
		}
	}

	pr_debug("[%s] status: %d input current: %d, charging current : %d\r\n",
		__func__, g_temp_status, g_temp_input_CC_value, g_temp_CC_value);
}

#endif

bool get_usb_current_unlimited(void)
{
#if defined(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT)
	if (batt_internal_charger_detect())
		return get_usb_current_unlimited_internal();
#endif

	if (BMT_status.charger_type == STANDARD_HOST || BMT_status.charger_type == CHARGING_HOST)
		return usb_unlimited;
	else
		return false;
}

void set_usb_current_unlimited(bool enable)
{
#if defined(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT)
	if (batt_internal_charger_detect())
		return set_usb_current_unlimited_internal(enable);
#endif

	usb_unlimited = enable;
}

void select_charging_curret_bcct(void)
{
#if defined(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT)
	if (batt_internal_charger_detect())
		return select_charging_curret_bcct_internal();
#endif

	switch (BMT_status.charger_type) {
	case STANDARD_HOST:
		g_temp_input_CC_value = batt_cust_data.usb_charger_current;
		break;

	case CHARGING_HOST:
		g_temp_input_CC_value =
			batt_cust_data.charging_host_charger_current;
		break;

	case NONSTANDARD_CHARGER:
		if (batt_cust_data.non_std_ac_charger_input_current != 0)
			g_temp_input_CC_value =
				batt_cust_data.non_std_ac_charger_input_current;
		else
			g_temp_input_CC_value =
				batt_cust_data.non_std_ac_charger_current;
		break;

	case STANDARD_CHARGER:
		if (batt_cust_data.ac_charger_input_current != 0)
			g_temp_input_CC_value =
				batt_cust_data.ac_charger_input_current;
		else
			g_temp_input_CC_value =
				batt_cust_data.ac_charger_current;
		break;

	default:
		pr_debug("[%s]Unknown charger type(%d)\n",
			__func__, BMT_status.charger_type);
		g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
		break;
	}

	g_temp_CC_value = g_bcct_value*100;

	pr_debug("[%s] input current(%d), charging current(%d), BCCT:%d\r\n",
		__func__, g_temp_input_CC_value, g_temp_CC_value, g_bcct_value);
}

static void pchr_turn_on_charging(void);
unsigned int set_bat_charging_current_limit(int current_limit)
{
	pr_debug("[BATTERY] set_bat_charging_current_limit (%d)\r\n", current_limit);

#if defined(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT)
	if (batt_internal_charger_detect())
		return set_bat_charging_current_limit_internal(current_limit);
#endif

	if (current_limit != -1) {
		g_bcct_flag = 1;
		g_bcct_value = current_limit;

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

	/* wake_up_bat(); */
	pchr_turn_on_charging();

	return g_bcct_flag;
}

int get_bat_charging_current_limit(void)
{
#if defined(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT)
	if (batt_internal_charger_detect())
		return get_bat_charging_current_limit_internal();
#endif

	return ((g_bcct_flag) ? g_bcct_value : CHARGE_CURRENT_MAX - 1) / 100;
}

static void select_charging_curret(void)
{
	if (g_ftm_battery_flag) {
		pr_debug("[BATTERY] FTM charging : %d\r\n", charging_level_data[0]);
		g_temp_CC_value = charging_level_data[0];

		if (g_temp_CC_value == CHARGE_CURRENT_450_00_MA) {
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
		} else {
			g_temp_input_CC_value = CHARGE_CURRENT_MAX;
			g_temp_CC_value = batt_cust_data.ac_charger_current;

			pr_debug("[BATTERY] set_ac_current \r\n");
		}
	} else if (g_custom_charging_current != -1) {
			pr_debug("[BATTERY] custom charging : %d\r\n", g_custom_charging_current);
			g_temp_CC_value = g_custom_charging_current;

			if (g_temp_CC_value <= CHARGE_CURRENT_500_00_MA)
				g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
			else if (g_custom_charging_current <= CHARGE_CURRENT_2000_00_MA)
				g_temp_input_CC_value = g_custom_charging_current;
			else
				g_temp_input_CC_value = CHARGE_CURRENT_2000_00_MA;
	} else {
		if (BMT_status.charger_type == STANDARD_HOST) {
#ifdef CONFIG_USB_IF
			{
				g_temp_input_CC_value = CHARGE_CURRENT_MAX;
				if (g_usb_state == USB_SUSPEND)
					g_temp_CC_value = USB_CHARGER_CURRENT_SUSPEND;
				else if (g_usb_state == USB_UNCONFIGURED)
					g_temp_CC_value = batt_cust_data.usb_charger_current_unconfigured;
				else if (g_usb_state == USB_CONFIGURED)
					g_temp_CC_value = batt_cust_data.usb_charger_current_configured;
				else
					g_temp_CC_value = batt_cust_data.usb_charger_current_unconfigured;

				pr_debug(
				    "[BATTERY] STANDARD_HOST CC mode charging : %d on %d state\r\n",
				    g_temp_CC_value, g_usb_state);
			}
#else
			{
				g_temp_input_CC_value = batt_cust_data.usb_charger_current;
				g_temp_CC_value = batt_cust_data.usb_charger_current;
			}
#endif
		} else if (BMT_status.charger_type == NONSTANDARD_CHARGER) {
			if (batt_cust_data.
				non_std_ac_charger_input_current != 0)
				g_temp_input_CC_value =
					batt_cust_data.
					non_std_ac_charger_input_current;
			else
				g_temp_input_CC_value =
					batt_cust_data.
					non_std_ac_charger_current;

		g_temp_CC_value = batt_cust_data.non_std_ac_charger_current;
		} else if (BMT_status.charger_type == STANDARD_CHARGER) {
			if (batt_cust_data.ac_charger_input_current != 0)
				g_temp_input_CC_value =
					batt_cust_data.ac_charger_input_current;
			else
				g_temp_input_CC_value =
					batt_cust_data.ac_charger_current;

			g_temp_CC_value = batt_cust_data.ac_charger_current;
#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
			if (is_ta_connect == true)
				set_ta_charging_current();
#endif
		} else if (BMT_status.charger_type == CHARGING_HOST) {
			g_temp_input_CC_value = batt_cust_data.charging_host_charger_current;
			g_temp_CC_value = batt_cust_data.charging_host_charger_current;
		} else if (BMT_status.charger_type == APPLE_2_1A_CHARGER) {
			g_temp_input_CC_value = batt_cust_data.apple_2_1a_charger_current;
			g_temp_CC_value = batt_cust_data.apple_2_1a_charger_current;
		} else if (BMT_status.charger_type == APPLE_1_0A_CHARGER) {
			g_temp_input_CC_value = batt_cust_data.apple_1_0a_charger_current;
			g_temp_CC_value = batt_cust_data.apple_1_0a_charger_current;
		} else if (BMT_status.charger_type == APPLE_0_5A_CHARGER) {
			g_temp_input_CC_value = batt_cust_data.apple_0_5a_charger_current;
			g_temp_CC_value = batt_cust_data.apple_0_5a_charger_current;
		} else {
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
			g_temp_CC_value = CHARGE_CURRENT_500_00_MA;
		}

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		if (DISO_data.diso_state.cur_vdc_state == DISO_ONLINE) {
			g_temp_input_CC_value = batt_cust_data.ac_charger_current;
			g_temp_CC_value = batt_cust_data.ac_charger_current;
		}
#endif

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		set_jeita_charging_current();
#endif
	}


}


static unsigned int charging_full_check(void)
{
	unsigned int status;

	battery_charging_control(CHARGING_CMD_GET_CHARGING_STATUS, &status);
	if (status == true) {
		g_full_check_count++;
		if (g_full_check_count >= FULL_CHECK_TIMES)
			return true;
		else
			return false;
	} else {
		g_full_check_count = 0;
		return status;
	}
}

#if defined(CONFIG_SW_AICL_SUPPORT)
static bool aicl_check_vbus_valid(void)
{
	int vbus_mv = battery_meter_get_vbus_now();
	bool is_valid = true;

	pr_info("%s: vbus_mv = %d\n", __func__, vbus_mv);
	if (vbus_mv < batt_cust_data.aicl_vbus_valid) {
		is_valid = false;
		pr_info("%s: VBUS drop below %d\n", __func__,
					batt_cust_data.aicl_vbus_valid);
	}

	return is_valid;
}

static bool is_aicl_allowed(void)
{
	int is_supported_by_hardware = 0;

	if (!batt_cust_data.aicl_enable)
		return false;

	battery_charging_control(CHARGING_CMD_GET_SW_AICL_SUPPORT,
			&is_supported_by_hardware);
	if (is_supported_by_hardware != 1)
		return false;

	if (BMT_status.aicl_done
		|| !BMT_status.charger_exist
		|| BMT_status.charger_type != STANDARD_CHARGER)
		return false;

	if (g_bcct_flag
		|| g_temp_status != TEMP_POS_10_TO_POS_45
		|| BMT_status.bat_charging_state == CHR_ERROR
		|| g_custom_charging_current != -1
		|| g_ftm_battery_flag) {
		pr_err("%s: ignore aicl by [%d %d 0x%x %d %d]\n", __func__,
			g_bcct_flag, g_temp_status, BMT_status.bat_charging_state,
			g_custom_charging_current, g_ftm_battery_flag);
		return false;
	}

	return true;
}

static int ap15_charger_detection(void)
{
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[128] = {0};
#endif

	int iusb = g_temp_input_CC_value;
	int current_max = 0;

	if (!is_aicl_allowed())
		goto exit;

	pr_info("%s: aicl start: %d\n", __func__, iusb);
	current_max = batt_cust_data.aicl_charging_current_max;
	battery_charging_control(CHARGING_CMD_SET_CURRENT, &current_max);

	while (iusb < batt_cust_data.aicl_input_current_max) {
		iusb += batt_cust_data.aicl_step_current;
		battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT,
				&iusb);
		pr_info("%s: aicl set %d\n", __func__, iusb);
		msleep(batt_cust_data.aicl_vbus_state_phase);
		if (!aicl_check_vbus_valid()) {
			iusb -= batt_cust_data.aicl_step_current;
			battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT,
				&iusb);
			pr_info("%s: aicl set back: %d\n", __func__, iusb);
			break;
		}
		msleep(batt_cust_data.aicl_step_interval);
	}
	BMT_status.aicl_done = true;
	BMT_status.aicl_result = iusb;
	pr_info("%s: aicl done: %d\n", __func__, iusb);

	if (iusb >= batt_cust_data.aicl_input_current_min) {
		pr_info("%s: AP15(9W) adapter detected, update setting %d %d\n",
				__func__ , current_max, iusb);
		BMT_status.ap15_charger_detected = true;
		batt_cust_data.ac_charger_input_current = iusb;
		batt_cust_data.ac_charger_current = current_max;
	} else {
		pr_info("%s: aicl: AP15(9W) adapter not detected\n", __func__);
		BMT_status.ap15_charger_detected = false;
		battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT,
					&g_temp_input_CC_value);
		battery_charging_control(CHARGING_CMD_SET_CURRENT,
					&g_temp_CC_value);
	}

#ifdef CONFIG_AMAZON_METRICS_LOG
	snprintf(buf, sizeof(buf),
			"%s:aicl:detected=%d;CT;1,aicl_result=%d;CT;1:NR",
			__func__, BMT_status.ap15_charger_detected,
			BMT_status.aicl_result / 100);
	log_to_metrics(ANDROID_LOG_INFO, "AICL", buf);
#endif

exit:
	return 0;
}
#else
static inline bool ap15_charger_detection(void) { return false; }
#endif

static void pchr_turn_on_charging(void)
{
#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	BATTERY_VOLTAGE_ENUM cv_voltage;
#endif
	unsigned int charging_enable = true;

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	if (true == BMT_status.charger_exist)
		charging_enable = true;
	else
		charging_enable = false;
#endif

	if (BMT_status.bat_charging_state == CHR_ERROR) {
		pr_debug("[BATTERY] Charger Error, turn OFF charging !\n");

		charging_enable = false;

	} else if ((g_platform_boot_mode == META_BOOT) || (g_platform_boot_mode == ADVMETA_BOOT)) {
		pr_debug("[BATTERY] In meta or advanced meta mode, disable charging.\n");
		charging_enable = false;
	} else {
		/*HW initialization */
		battery_charging_control(CHARGING_CMD_INIT, NULL);

		pr_debug("charging_hw_init\n");

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
		battery_pump_express_algorithm_start();
#endif

		/* Set Charging Current */
		if (get_usb_current_unlimited()) {
			if (batt_cust_data.ac_charger_input_current != 0)
				g_temp_input_CC_value =
					batt_cust_data.ac_charger_input_current;
			else
				g_temp_input_CC_value =
					batt_cust_data.ac_charger_current;

			g_temp_CC_value =
				batt_cust_data.ac_charger_current;
			pr_debug("USB_CURRENT_UNLIMITED, use batt_cust_data.ac_charger_current\n");
		} else if (g_bcct_flag == 1) {
			select_charging_curret_bcct();

			pr_debug("[BATTERY] select_charging_curret_bcct !\n");
		} else {
			select_charging_curret();

			pr_debug("[BATTERY] select_charging_curret !\n");
		}
		pr_debug("[BATTERY] Default CC mode charging : %d, input current = %d\r\n",
			    g_temp_CC_value, g_temp_input_CC_value);

		if (g_temp_CC_value == CHARGE_CURRENT_0_00_MA
		    || g_temp_input_CC_value == CHARGE_CURRENT_0_00_MA) {

			charging_enable = false;

			pr_debug("[BATTERY] charging current is set 0mA, turn off charging !\r\n");
		}

		battery_charging_control(CHARGING_CMD_SET_INPUT_CURRENT,
			&g_temp_input_CC_value);

		battery_charging_control(CHARGING_CMD_SET_CURRENT, &g_temp_CC_value);

		/*Set CV Voltage */
#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		if (batt_cust_data.high_battery_voltage_support)
			cv_voltage = BATTERY_VOLT_04_350000_V;
		else
			cv_voltage = BATTERY_VOLT_04_200000_V;

		battery_charging_control(CHARGING_CMD_SET_CV_VOLTAGE, &cv_voltage);
#endif
	}

	/* enable/disable charging */
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	pr_debug("[BATTERY] pchr_turn_on_charging(), enable =%d !\r\n", charging_enable);

	ap15_charger_detection();
}


static PMU_STATUS BAT_PreChargeModeAction(void)
{
	pr_debug("[BATTERY] Pre-CC mode charge, timer=%d on %d !!\n\r",
		    BMT_status.PRE_charging_time, BMT_status.total_charging_time);

	BMT_status.PRE_charging_time += BAT_TASK_PERIOD;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.total_charging_time += BAT_TASK_PERIOD;

	/*  Enable charger */
	pchr_turn_on_charging();

	if (BMT_status.UI_SOC == 100) {
		BMT_status.bat_charging_state = CHR_BATFULL;
		BMT_status.bat_full = true;
		g_charging_full_reset_bat_meter = true;
	} else if (BMT_status.bat_vol > batt_cust_data.v_pre2cc_thres) {
		BMT_status.bat_charging_state = CHR_CC;
	}



	return PMU_STATUS_OK;
}


static PMU_STATUS BAT_ConstantCurrentModeAction(void)
{
	pr_debug("[BATTERY] CC mode charge, timer=%d on %d !!\n\r",
		    BMT_status.CC_charging_time, BMT_status.total_charging_time);

	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time += BAT_TASK_PERIOD;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.total_charging_time += BAT_TASK_PERIOD;

	/*  Enable charger */
	pchr_turn_on_charging();

	if (charging_full_check() == true) {
		BMT_status.bat_charging_state = CHR_BATFULL;
		BMT_status.bat_full = true;
		g_charging_full_reset_bat_meter = true;
	}

	return PMU_STATUS_OK;
}


static PMU_STATUS BAT_BatteryFullAction(void)
{
	pr_debug("[BATTERY] Battery full !!\n\r");

	BMT_status.bat_full = true;
	BMT_status.total_charging_time = 0;
	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.POSTFULL_charging_time = 0;
	BMT_status.bat_in_recharging_state = false;

	if (charging_full_check() == false) {
		pr_debug("[BATTERY] Battery Re-charging !!\n\r");

		BMT_status.bat_in_recharging_state = true;
		BMT_status.bat_charging_state = CHR_CC;
#ifndef CONFIG_MTK_HAFG_20
		battery_meter_reset();
#endif

	}


	return PMU_STATUS_OK;
}

static PMU_STATUS BAT_BatteryStatusFailAction(void)
{
	unsigned int charging_enable;

	pr_debug("[BATTERY] BAD Battery status... Charging Stop !!\n\r");

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	#if defined(CONFIG_MTK_MULTI_BAT_PROFILE_SUPPORT) && \
		defined(MTK_GET_BATTERY_ID_BY_AUXADC)
	if ((g_temp_status == TEMP_ABOVE_POS_60) ||
		(g_temp_status == TEMP_BELOW_NEG_10) ||
		(false == get_battery_id_status()))
	#else
	if ((g_temp_status == TEMP_ABOVE_POS_60) || (g_temp_status == TEMP_BELOW_NEG_10))
	#endif
		temp_error_recovery_chr_flag = false;

	#if defined(CONFIG_MTK_MULTI_BAT_PROFILE_SUPPORT) && \
			defined(MTK_GET_BATTERY_ID_BY_AUXADC)
	if ((temp_error_recovery_chr_flag == false) &&
		(g_temp_status != TEMP_ABOVE_POS_60) &&
		(g_temp_status != TEMP_BELOW_NEG_10) &&
		(true == get_battery_id_status())) {
	#else
	if ((temp_error_recovery_chr_flag == false) && (g_temp_status != TEMP_ABOVE_POS_60)
			&& (g_temp_status != TEMP_BELOW_NEG_10)) {
	#endif
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

#if defined(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT)
	if (batt_internal_charger_detect())
		return mt_battery_charging_algorithm_internal();
#endif

	battery_charging_control(CHARGING_CMD_RESET_WATCH_DOG_TIMER, NULL);

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
	battery_pump_express_charger_check();
#endif
	switch (BMT_status.bat_charging_state) {
	case CHR_PRE:
		BAT_PreChargeModeAction();
		break;

	case CHR_CC:
		BAT_ConstantCurrentModeAction();
		break;

	case CHR_BATFULL:
		BAT_BatteryFullAction();
		break;

	case CHR_ERROR:
		BAT_BatteryStatusFailAction();
		break;
	}

	battery_charging_control(CHARGING_CMD_DUMP_REGISTER, NULL);
}
