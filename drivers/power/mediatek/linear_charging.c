/*****************************************************************************
 *
 * Filename:
 * ---------
 *    linear_charging.c
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
 * 04 24 2015 wy.chuang
 * [ALPS02047356] [Blocking][Rainier][L-MR1][Pump Express] Fast charging doesn't work on user load.
 *
 *	.
 *
 * 04 23 2015 wy.chuang
 * [ALPS02047356] [Blocking][Rainier][L-MR1][Pump Express] Fast charging doesn't work on user load.
 *
 *	.
 *
 * 03 28 2015 wy.chuang
 * [ALPS01990538] [MP Feature Patch Back]MT6312 driver & MT6328 init setting & charging setting
 *
 *	.
 *
 * 03 12 2015 wy.chuang
 * [ALPS01921641] [L1_merge] for PMIC and charging
 * .
 *
 * 03 10 2015 wy.chuang
 * [ALPS01974124] Device keep popup Power off charging mode notify
 * while using USB cable and connect with Specify USB port
 * .
 *
 * 03 05 2015 wy.chuang
 * [ALPS01921641] [L1_merge] for PMIC and charging
 * .
 *
 * 01 20 2015 wy.chuang
 * [ALPS01814017] MT6328 check in
 * .
 *
 * 01 18 2015 wy.chuang
 * [ALPS01814017] MT6328 check in
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

#include <mt-plat/battery_meter.h>
#include <mt-plat/battery_common.h>
#include <mt-plat/charging.h>
#include <mt-plat/mt_boot.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>



 /* ============================================================ // */
 /* define */
 /* ============================================================ // */
 /* cut off to full */
#define POST_CHARGING_TIME		(30 * 60)	/* 30mins */
#define CV_CHECK_DELAT_FOR_BANDGAP	80	/* 80mV */
#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)
#define BJT_LIMIT			1200000	/* 1.2W */
#ifndef TA_START_VCHR_TUNUNG_VOLTAG
#define TA_START_VCHR_TUNUNG_VOLTAGE	3700	/* for isink blink issue */
#define TA_CHARGING_CURRENT		CHARGE_CURRENT_1500_00_MA
#endif				/* TA_START_VCHR_TUNUNG_VOLTAG */
#endif				/* MTK_PUMP_EXPRESS_SUPPORT */


 /* ============================================================ // */
 /* global variable */
 /* ============================================================ // */
static unsigned int g_bcct_flag;
static CHR_CURRENT_ENUM g_temp_CC_value = CHARGE_CURRENT_0_00_MA;
static unsigned int g_usb_state = USB_UNCONFIGURED;
/* CHARGING_FULL_CURRENT, unit: mA */
static unsigned int charging_full_current;
static unsigned int v_cc2topoff_threshold;	/* = V_CC2TOPOFF_THRES; */
/* AC_CHARGER_CURRENT */
static CHR_CURRENT_ENUM ulc_cv_charging_current;
static bool ulc_cv_charging_current_flag;
static bool usb_unlimited;

  /* ///////////////////////////////////////////////////////////////////////////////////////// */
  /* // JEITA */
  /* ///////////////////////////////////////////////////////////////////////////////////////// */
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
static int g_jeita_recharging_voltage = 4100;
static int g_temp_status = TEMP_POS_10_TO_POS_45;
static bool temp_error_recovery_chr_flag = true;
#endif

  /* ///////////////////////////////////////////////////////////////////////////////////////// */
  /* // PUMP EXPRESS */
  /* ///////////////////////////////////////////////////////////////////////////////////////// */
#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)
struct wake_lock TA_charger_suspend_lock;
CHR_CURRENT_ENUM ta_charging_current = TA_CHARGING_CURRENT;
int ta_current_level = 5000;
int ta_pre_vbat = 0;
bool ta_check_chr_type = true;
bool ta_check_ta_control = false;
bool ta_vchr_tuning = false;
bool first_vchr_det = true;
bool ta_cable_out_occur = false;
bool is_ta_connect = false;
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



void BATTERY_SetUSBState_internal(int usb_state_value)
{
#if defined(CONFIG_POWER_EXT)
	pr_notice("[BATTERY_SetUSBState] in FPGA/EVB, no service\r\n");
#else
	if ((usb_state_value < USB_SUSPEND) || ((usb_state_value > USB_CONFIGURED))) {
		pr_notice(
			    "[BATTERY] BAT_SetUSBState Fail! Restore to default value\r\n");
		usb_state_value = USB_UNCONFIGURED;
	} else {
		pr_notice("[BATTERY] BAT_SetUSBState Success! Set %d\r\n", usb_state_value);
		g_usb_state = usb_state_value;
	}
#endif
}

unsigned int get_charging_setting_current_internal(void)
{
	return g_temp_CC_value;
}


#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)

static DEFINE_MUTEX(ta_mutex);


static void mtk_ta_decrease(void)
{
	bool ta_current_pattern = false;	/* FALSE = decrease */

	/* if(BMT_status.charger_exist == true) */
	if (ta_cable_out_occur == false) {
		battery_charging_control(CHARGING_CMD_SET_TA_CURRENT_PATTERN, &ta_current_pattern);
		ta_current_level -= 200;
	} else {
		ta_check_chr_type = true;
		/* is_ta_connect = false; */
		pr_notice("mtk_ta_decrease() Cable out\n");
	}
}

static void mtk_ta_increase(void)
{
	bool ta_current_pattern = true;	/* TRUE = increase */

	/* if(BMT_status.charger_exist == true) */
	if (ta_cable_out_occur == false) {
		battery_charging_control(CHARGING_CMD_SET_TA_CURRENT_PATTERN, &ta_current_pattern);
		ta_current_level += 200;
	} else {
		ta_check_chr_type = true;
		/* is_ta_connect = false; */
		pr_notice("mtk_ta_increase() Cable out\n");
	}
}

static void mtk_ta_reset_vchr(void)
{
	CHR_CURRENT_ENUM chr_current = CHARGE_CURRENT_70_00_MA;

	battery_charging_control(CHARGING_CMD_SET_CURRENT, &chr_current);
	msleep(250);		/* reset Vchr to 5V */

	ta_current_level = 5000;

	pr_notice("mtk_ta_reset_vchr(): reset Vchr to 5V\n");
}

static void mtk_ta_init(void)
{
	ta_current_level = 5000;
	is_ta_connect = false;
	ta_pre_vbat = 0;
	ta_vchr_tuning = false;
	ta_check_ta_control = false;
	ta_cable_out_occur = false;

	battery_charging_control(CHARGING_CMD_RESET_WATCH_DOG_TIMER, NULL);
	battery_charging_control(CHARGING_CMD_INIT, NULL);

}

int ta_get_charger_voltage(void)
{
	int voltage = 0;
	int i;

	for (i = 0; i < 10; i++)
		voltage = voltage + battery_meter_get_charger_voltage();


	return voltage / 10;
}


static void mtk_ta_detector(void)
{
	int real_v_chrA;
	int real_v_chrB;
	bool retransmit = true;
	unsigned int retransmit_count = 0;
	U32 charging_enable = true;

	pr_notice("mtk_ta_detector() start\n");

	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	do {
		real_v_chrA = ta_get_charger_voltage();
		mtk_ta_decrease();
		mtk_ta_decrease();
		real_v_chrB = ta_get_charger_voltage();

		if (real_v_chrA - real_v_chrB >= 300) {	/* 0.3V */
			retransmit = false;
			is_ta_connect = true;
		} else {
			retransmit_count++;

			pr_notice("mtk_ta_detector(): retransmit_count =%d, chrA=%d, chrB=%d\n",
				    retransmit_count, real_v_chrA, real_v_chrB);

			mtk_ta_reset_vchr();
		}

		if ((retransmit_count == 3) || (BMT_status.charger_exist == false)) {
			retransmit = false;
			is_ta_connect = false;
		}

	} while ((retransmit == true) && (ta_cable_out_occur == false));


	pr_notice("mtk_ta_detector() ta_current_level=%d, real_v_chrA=%d, real_v_chrB=%d, is_ta_connect=%d\n",
		    ta_current_level, real_v_chrA, real_v_chrB, is_ta_connect);

	pr_notice("mtk_ta_detector() end, retry_count=%d, ta_cable_out_occur=%d\n",
		    retransmit_count, ta_cable_out_occur);
}

static void mtk_tuning_voltage(int curr_level, int target_level)
{
	int is_increase = 0;
	int exec_level = 0;
	CHR_CURRENT_ENUM chr_current = CHARGE_CURRENT_70_00_MA;

	/* if(BMT_status.charger_exist == true) */
	if (ta_cable_out_occur == false) {
		pr_notice("mtk_tuning_voltage() start\n");

		if (curr_level >= target_level) {
			exec_level = (curr_level - target_level) / 200;
			is_increase = 0;
		} else {
			exec_level = (target_level - curr_level) / 200;
			is_increase = 1;
		}

		if (exec_level == 0) {	/* curr_level == target_level */
			battery_charging_control(CHARGING_CMD_SET_CURRENT, &chr_current);
			msleep(50);	/* for VChr reading to check error occur or not */
		}


		pr_notice(
			    "mtk_tuning_voltage() before : ta_current_level=%d, real_v_chr=%d, is_ta_connect=%d, is_increase=%d, exec_level=%d\n",
			    ta_current_level, battery_meter_get_charger_voltage(), is_ta_connect,
			    is_increase, exec_level);

		while ((exec_level > 0) && (ta_cable_out_occur == false)) {
			if (is_increase == 1)
				mtk_ta_increase();
			else
				mtk_ta_decrease();
			pr_notice(
				    "mtk_tuning_voltage() after ta_current_level=%d, real_v_chr=%d, is_ta_connect=%d, is_increase=%d, exec_level=%d\n",
				    ta_current_level, battery_meter_get_charger_voltage(),
				    is_ta_connect, is_increase, exec_level);

			exec_level--;
		}

		pr_notice("mtk_tuning_voltage() end\n");
	} else {
		ta_check_chr_type = true;
		/* is_ta_connect = false; */
		pr_notice("mtk_tuning_voltage(), Cable Out\n");
	}
}

static void select_v_chr_candidate(int curr_vbat, int ta_v_chr_candidate[])
{
	pr_notice("select_v_chr_candidate() start\n");

	if (curr_vbat > 4200)
		ta_v_chr_candidate[0] = 4600;
	else if (curr_vbat > 4000)
		ta_v_chr_candidate[0] = 4400;
	else if (curr_vbat > 3800)
		ta_v_chr_candidate[0] = 4200;
	else if (curr_vbat > 3600)
		ta_v_chr_candidate[0] = 4000;
	else
		ta_v_chr_candidate[0] = 3800;

	ta_v_chr_candidate[1] = ta_v_chr_candidate[0] + 200;
	ta_v_chr_candidate[2] = ta_v_chr_candidate[0] + 400;
	ta_v_chr_candidate[3] = ta_v_chr_candidate[0] + 600;

	pr_notice("select_v_chr_candidate() vbat=%d, candidate=%d,%d,%d\n",
		    curr_vbat, ta_v_chr_candidate[1], ta_v_chr_candidate[2], ta_v_chr_candidate[3]);

	pr_notice("select_v_chr_candidate() end\n");
}


static void mtk_ta_vchr_select(int i, int ta_v_chr_candidate[], int ta_charging_current_candidate[],
			       int *max_charging_current, int *max_charging_current_i)
{
	int current_vchr;
	bool retransmit = true;
	unsigned int retransmit_count = 0;

	current_vchr = battery_meter_get_charger_voltage();
	if (ta_current_level != 5000 && current_vchr >= 4900) {	/* pattern error before, so reset vchr to 5V */
		pr_notice("mtk_ta_vchr_select() : curr_VChr=%d, ta_current_level=%d\n", current_vchr, ta_current_level);
		mtk_ta_reset_vchr();
	}

	do {
		mtk_tuning_voltage(ta_current_level, ta_v_chr_candidate[i]);

		current_vchr = battery_meter_get_charger_voltage();
		if ((abs(current_vchr - ta_current_level) > 300) && (ta_cable_out_occur == false)) {
			/* variation > 0.3V, error occur */
			retransmit_count++;

			pr_notice("mtk_ta_vchr_select(): retransmit_count =%d, cur_chr=%d, ta_current_level=%d\n",
				    retransmit_count, current_vchr, ta_current_level);

			mtk_ta_reset_vchr();
		} else
			retransmit = false;


		if ((retransmit_count == 2) || (ta_cable_out_occur == true))
			retransmit = false;


	} while ((retransmit == true) && (ta_cable_out_occur == false));

	battery_charging_control(CHARGING_CMD_SET_CURRENT, &ta_charging_current);	/* 1.5A */

	pr_notice("mtk_ta_vchr_select() : use 1.5A for select max current\n");
	msleep(900);		/* over 800ms to avoid interference pattern */

	ta_charging_current_candidate[i] =
		battery_meter_get_charging_current_imm();

	/* we hope to choose the less VChr if the current difference between 2 step
	* is not large, so we add weighting for different VChr step */
	if (i == 1)
		ta_charging_current_candidate[i] += 100;	/* weighting, plus 120mA for Vbat+0.4V */
	else if (i == 2)
		ta_charging_current_candidate[i] += 50;	/* weighting, plug 60mA for Vbat+0.6V */

	if (ta_charging_current_candidate[i] > *max_charging_current) {
		*max_charging_current = ta_charging_current_candidate[i];
		*max_charging_current_i = i;
	}
}


static void mtk_ta_BJT_check(void)
{
	int curr_vbat = 0;
	int curr_current = 0;
	int vchr = 0;
	int watt = 0;
	int i = 0, cnt = 0;

	for (i = 0; i < 3; i++) {
		vchr = battery_meter_get_charger_voltage();
		curr_vbat = battery_meter_get_battery_voltage(true);
		curr_current = battery_meter_get_charging_current_imm();

		watt = ((vchr - curr_vbat) * curr_current);

		pr_notice("mtk_ta_BJT_check() vchr=%d, vbat=%d, current=%d, Watt=%d, ta_current_level=%d\n",
			    vchr, curr_vbat, curr_current, watt, ta_current_level);

		if (watt > BJT_LIMIT)	/* 1.2W */
			cnt++;
		else
			break;

		msleep(200);

	}

	if (cnt >= 3)
		is_ta_connect = false;


	pr_notice("mtk_ta_BJT_check() vchr=%d, vbat=%d, current=%d, Watt=%d, ta_current_level=%d cnt=%d\n",
		    vchr, curr_vbat, curr_current, watt, ta_current_level, cnt);
}

static void battery_pump_express_charger_check(void)
{
	if (ta_check_chr_type == true && BMT_status.charger_type == STANDARD_CHARGER) {
		mutex_lock(&ta_mutex);
		wake_lock(&TA_charger_suspend_lock);

		mtk_ta_reset_vchr();
		mtk_ta_init();
		mtk_ta_detector();

		first_vchr_det = true;

		if (ta_cable_out_occur == false) {
			ta_check_chr_type = false;
		} else {
			/* need to re-check if the charger plug out during ta detector */
			ta_check_chr_type = true;
		}

		wake_unlock(&TA_charger_suspend_lock);
		mutex_unlock(&ta_mutex);
	}

}

static void battery_pump_express_algorithm_start(void)
{
	int ta_v_chr_candidate[4] = { 0, 0, 0, 0 };
	int ta_charging_current_candidate[4] = { 0, 0, 0, 0 };
	int max_charging_current = 0;
	int max_charging_current_i = 0;
	int curr_vbat = 0;
	int i = 0;
	int ta_cv_vchr;
	unsigned int cv_voltage;

	if (batt_cust_data.high_battery_voltage_support)
		cv_voltage = 4350;
	else
		cv_voltage = 4200;

	mutex_lock(&ta_mutex);
	wake_lock(&TA_charger_suspend_lock);

	if (is_ta_connect == true) {
		pr_notice("mtk_ta_algorithm() start\n");

		curr_vbat = battery_meter_get_battery_voltage(true);
		if (((curr_vbat - ta_pre_vbat) > 100) &&
			(curr_vbat < (cv_voltage - (CV_CHECK_DELAT_FOR_BANDGAP + 20))) &&
			(curr_vbat > TA_START_VCHR_TUNUNG_VOLTAGE)) {
			/*cv -0.12V && to avoid screen flash( VBAT less than 3.7V) */
			ta_pre_vbat = curr_vbat;

			select_v_chr_candidate(curr_vbat, ta_v_chr_candidate);

			if (first_vchr_det == true) {
				for (i = 3; i >= 1; i--) {	/* measure  VBAT+0.8V, VBAT+0.6V then VBAT+0.4V */
					if (ta_cable_out_occur == false)
						mtk_ta_vchr_select(i, ta_v_chr_candidate,
								   ta_charging_current_candidate,
								   &max_charging_current,
								   &max_charging_current_i);
				}

				first_vchr_det = false;
			} else {
				for (i = 1; i <= 3; i++) {	/* measure VBAT+0.4V,VBAT+0.6V then VBAT+0.8V */
					if (ta_cable_out_occur == false)
						mtk_ta_vchr_select(i, ta_v_chr_candidate,
								   ta_charging_current_candidate,
								   &max_charging_current,
								   &max_charging_current_i);
				}
			}

			pr_notice(
				    "mtk_ta_algorithm() candidate=%d,%d,%d,%d ; i=%d,%d,%d,%d ; max_charging_current_i=%d\n",
				    ta_v_chr_candidate[0], ta_v_chr_candidate[1],
				    ta_v_chr_candidate[2], ta_v_chr_candidate[3],
				    ta_charging_current_candidate[0],
				    ta_charging_current_candidate[1],
				    ta_charging_current_candidate[2],
				    ta_charging_current_candidate[3], max_charging_current_i);

			mtk_tuning_voltage(ta_current_level,
					   ta_v_chr_candidate[max_charging_current_i]);

			ta_vchr_tuning = true;
			ta_check_ta_control = true;
		} else if (curr_vbat >= (cv_voltage - (CV_CHECK_DELAT_FOR_BANDGAP + 20))) {
			if (cv_voltage == 4200)
				ta_cv_vchr = 4800;
			else	/* cv 4.35V */
				ta_cv_vchr = 5000;

			if (ta_current_level != ta_cv_vchr)
				mtk_tuning_voltage(ta_current_level, ta_cv_vchr);


			ta_vchr_tuning = true;
			ta_check_ta_control = false;

			pr_notice(
				    "mtk_ta_algorithm(),curr_vbat > cv_voltage, ta_current_level=%d, cv_voltage=%d, ta_cv_vchr=%d,",
				    ta_current_level, cv_voltage, ta_cv_vchr);
		}

		/* --for normal charging */
		if ((is_ta_connect == true) && (curr_vbat > TA_START_VCHR_TUNUNG_VOLTAGE) &&
			(ta_check_ta_control == true)) {
			/* to avoid screen flash( VBAT less than 3.7V) */
			battery_charging_control(CHARGING_CMD_SET_CURRENT, &ta_charging_current);	/* 1.5A */
			pr_notice(
				    "mtk_ta_algorithm() : detect TA, use 1.5A for normal charging, curr_vbat=%d, ta_pre_vbat=%d, ta_current_level=%d\n",
				    curr_vbat, ta_pre_vbat, ta_current_level);
			/* msleep(1500); */
		}
		/* ------------------------ */

		mtk_ta_BJT_check();
		pr_notice("mtk_ta_algorithm() end\n");
	} else {
		pr_notice("It's not a TA charger, bypass TA algorithm\n");
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
		cv_voltage =
		batt_cust_data.jeita_temp_above_pos_60_cv_voltage;
	else if (g_temp_status == TEMP_POS_45_TO_POS_60)
		cv_voltage =
		batt_cust_data.jeita_temp_pos_45_to_pos_60_cv_voltage;
	else if (g_temp_status == TEMP_POS_10_TO_POS_45)
		cv_voltage =
		batt_cust_data.jeita_temp_pos_10_to_pos_45_cv_voltage;
	else if (g_temp_status == TEMP_POS_0_TO_POS_10)
		cv_voltage =
		batt_cust_data.jeita_temp_pos_0_to_pos_10_cv_voltage;
	else if (g_temp_status == TEMP_NEG_10_TO_POS_0)
		cv_voltage =
		batt_cust_data.jeita_temp_neg_10_to_pos_0_cv_voltage;
	else if (g_temp_status == TEMP_BELOW_NEG_10)
		cv_voltage =
		batt_cust_data.jeita_temp_below_neg_10_cv_voltage;
	else {
		if (batt_cust_data.high_battery_voltage_support)
			cv_voltage = BATTERY_VOLT_04_350000_V;
		else
			cv_voltage = BATTERY_VOLT_04_200000_V;
	}

	return cv_voltage;
}

PMU_STATUS do_jeita_state_machine_internal(void)
{
	BATTERY_VOLTAGE_ENUM cv_voltage;

	/* JEITA battery temp Standard */
	if (BMT_status.temperature >= TEMP_POS_60_THRESHOLD) {
		pr_notice(
			    "[BATTERY] Battery Over high Temperature(%d) !!\n\r",
			    TEMP_POS_60_THRESHOLD);
		g_temp_status = TEMP_ABOVE_POS_60;
		return PMU_STATUS_FAIL;
	} else if (BMT_status.temperature > TEMP_POS_45_THRESHOLD) {
		if ((g_temp_status == TEMP_ABOVE_POS_60)
		    && (BMT_status.temperature >= TEMP_POS_60_THRES_MINUS_X_DEGREE)) {
			pr_notice(
				    "[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
				    TEMP_POS_60_THRES_MINUS_X_DEGREE, TEMP_POS_60_THRESHOLD);
			return PMU_STATUS_FAIL;
		} /*else {*/
			pr_notice(
				    "[BATTERY] Battery Temperature between %d and %d !!\n\r",
				    TEMP_POS_45_THRESHOLD, TEMP_POS_60_THRESHOLD);
			g_temp_status = TEMP_POS_45_TO_POS_60;
			g_jeita_recharging_voltage =
				batt_cust_data.
				jeita_temp_pos_45_to_pos_60_recharge_voltage;
			v_cc2topoff_threshold =
				batt_cust_data.
				jeita_temp_pos_45_to_pos_60_cc2topoff_threshold;
			charging_full_current = batt_cust_data.charging_full_current;
		/*}*/
	} else if (BMT_status.temperature >= TEMP_POS_10_THRESHOLD) {
		if (((g_temp_status == TEMP_POS_45_TO_POS_60)
		     && (BMT_status.temperature >= TEMP_POS_45_THRES_MINUS_X_DEGREE))
		    || ((g_temp_status == TEMP_POS_0_TO_POS_10)
			&& (BMT_status.temperature <= TEMP_POS_10_THRES_PLUS_X_DEGREE))) {
			pr_notice(
				    "[BATTERY] Battery Temperature not recovery to normal temperature charging mode yet!!\n\r");
		} else {
			pr_notice(
				    "[BATTERY] Battery Normal Temperature between %d and %d !!\n\r",
				    TEMP_POS_10_THRESHOLD, TEMP_POS_45_THRESHOLD);

			g_temp_status = TEMP_POS_10_TO_POS_45;
			g_jeita_recharging_voltage =
				batt_cust_data.
				jeita_temp_pos_10_to_pos_45_recharge_voltage;
			v_cc2topoff_threshold =
				batt_cust_data.
				jeita_temp_pos_10_to_pos_45_cc2topoff_threshold;
			charging_full_current = batt_cust_data.charging_full_current;
		}
	} else if (BMT_status.temperature >= TEMP_POS_0_THRESHOLD) {
		if ((g_temp_status == TEMP_NEG_10_TO_POS_0 || g_temp_status == TEMP_BELOW_NEG_10)
		    && (BMT_status.temperature <= TEMP_POS_0_THRES_PLUS_X_DEGREE)) {
			if (g_temp_status == TEMP_NEG_10_TO_POS_0) {
				pr_notice(
					    "[BATTERY] Battery Temperature between %d and %d !!\n\r",
					    TEMP_POS_0_THRES_PLUS_X_DEGREE, TEMP_POS_10_THRESHOLD);
			}
			if (g_temp_status == TEMP_BELOW_NEG_10) {
				pr_notice(
					    "[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
					    TEMP_POS_0_THRESHOLD, TEMP_POS_0_THRES_PLUS_X_DEGREE);
				return PMU_STATUS_FAIL;
			}
		} else {
			pr_notice(
				    "[BATTERY] Battery Temperature between %d and %d !!\n\r",
				    TEMP_POS_0_THRESHOLD, TEMP_POS_10_THRESHOLD);
			g_temp_status = TEMP_POS_0_TO_POS_10;
			g_jeita_recharging_voltage =
				batt_cust_data.
				jeita_temp_pos_0_to_pos_10_recharge_voltage;
			v_cc2topoff_threshold =
				batt_cust_data.
				jeita_temp_pos_0_to_pos_10_cc2topoff_threshold;
			charging_full_current = batt_cust_data.charging_full_current;
		}
	} else if (BMT_status.temperature >= TEMP_NEG_10_THRESHOLD) {
		if ((g_temp_status == TEMP_BELOW_NEG_10)
		    && (BMT_status.temperature <= TEMP_NEG_10_THRES_PLUS_X_DEGREE)) {
			pr_notice(
				    "[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
				    TEMP_NEG_10_THRESHOLD, TEMP_NEG_10_THRES_PLUS_X_DEGREE);
			return PMU_STATUS_FAIL;
		} /*else {*/
			pr_notice(
				    "[BATTERY] Battery Temperature between %d and %d !!\n\r",
				    TEMP_NEG_10_THRESHOLD, TEMP_POS_0_THRESHOLD);
			g_temp_status = TEMP_NEG_10_TO_POS_0;
			g_jeita_recharging_voltage =
				batt_cust_data.
				jeita_temp_neg_10_to_pos_0_recharge_voltage;
			v_cc2topoff_threshold =
				batt_cust_data.
				jeita_temp_neg_10_to_pos_0_cc2topoff_threshold;
			charging_full_current =
			batt_cust_data.jeita_neg_10_to_pos_0_full_current;
		/*}*/
	} else {
		pr_notice(
			    "[BATTERY] Battery below low Temperature(%d) !!\n\r",
			    TEMP_NEG_10_THRESHOLD);
		g_temp_status = TEMP_BELOW_NEG_10;
		return PMU_STATUS_FAIL;
	}

	/* set CV after temperature changed */
	cv_voltage = select_jeita_cv();
	battery_charging_control(CHARGING_CMD_SET_CV_VOLTAGE, &cv_voltage);

	return PMU_STATUS_OK;
}


static void set_jeita_charging_current(void)
{
	static bool above_4000;
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
		} else if (BMT_status.bat_vol > 3900 && above_4000 == true) {
			g_temp_CC_value = CHARGE_CURRENT_500_00_MA;
		} else {
			g_temp_CC_value =
			(CHARGE_CURRENT_800_00_MA > g_temp_CC_value) ?
			g_temp_CC_value : CHARGE_CURRENT_800_00_MA;
			above_4000 = false;
		}
	}

	pr_debug("[%s] status: %d, charging current : %d\r\n",
		__func__, g_temp_status, g_temp_CC_value);
}

#endif

bool get_usb_current_unlimited_internal(void)
{
	if (BMT_status.charger_type == STANDARD_HOST || BMT_status.charger_type == CHARGING_HOST)
		return usb_unlimited;
	else
		return false;
}

void set_usb_current_unlimited_internal(bool enable)
{
	usb_unlimited = enable;
}

void select_charging_curret_bcct_internal(void)
{
	/* done on set_bat_charging_current_limit */
}


unsigned int set_bat_charging_current_limit_internal(int current_limit)
{
	pr_notice("[BATTERY] set_bat_charging_current_limit (%d)\r\n",
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
	pr_notice("[BATTERY] set_bat_sw_cv_charging_current_limit (%d)\r\n",
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

int get_bat_charging_current_limit_internal(void)
{
	return ((g_bcct_flag) ? g_temp_CC_value : CHARGE_CURRENT_MAX - 1) / 100;
}

static void select_charging_curret(void)
{
	if (g_ftm_battery_flag) {
		pr_notice("[BATTERY] FTM charging : %d\r\n", charging_level_data[0]);
		g_temp_CC_value = charging_level_data[0];
	} else if (g_custom_charging_current != -1) {
			pr_debug("[BATTERY] custom charging : %d\r\n", g_custom_charging_current);
			g_temp_CC_value = g_custom_charging_current;
	} else {
		if (BMT_status.charger_type == STANDARD_HOST) {
#ifdef CONFIG_USB_IF
			{
				if (g_usb_state == USB_SUSPEND) {
					g_temp_CC_value =
					    batt_cust_data.usb_charger_current_suspend;
				} else if (g_usb_state == USB_UNCONFIGURED) {
					g_temp_CC_value =
					    batt_cust_data.usb_charger_current_unconfigured;
				} else if (g_usb_state == USB_CONFIGURED) {
					g_temp_CC_value =
					    batt_cust_data.usb_charger_current_configured;
				} else {
					g_temp_CC_value =
					    batt_cust_data.usb_charger_current_unconfigured;
				}

				pr_notice(
					    "[BATTERY] STANDARD_HOST CC mode charging : %d on %d state\r\n",
					    g_temp_CC_value, g_usb_state);
			}
#else
			{
				g_temp_CC_value = batt_cust_data.usb_charger_current;
			}
#endif
		} else if (BMT_status.charger_type == NONSTANDARD_CHARGER) {
			g_temp_CC_value = batt_cust_data.non_std_ac_charger_current;
		} else if (BMT_status.charger_type == STANDARD_CHARGER) {
			g_temp_CC_value = batt_cust_data.ac_charger_current;
#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)
			if (is_ta_connect == true && ta_vchr_tuning == true)
				g_temp_CC_value = CHARGE_CURRENT_1500_00_MA;
#endif
		} else if (BMT_status.charger_type == CHARGING_HOST) {
			g_temp_CC_value = batt_cust_data.charging_host_charger_current;
		} else if (BMT_status.charger_type == APPLE_2_1A_CHARGER) {
			g_temp_CC_value = batt_cust_data.apple_2_1a_charger_current;
		} else if (BMT_status.charger_type == APPLE_1_0A_CHARGER) {
			g_temp_CC_value = batt_cust_data.apple_1_0a_charger_current;
		} else if (BMT_status.charger_type == APPLE_0_5A_CHARGER) {
			g_temp_CC_value = batt_cust_data.apple_0_5a_charger_current;
		} else {
			g_temp_CC_value = CHARGE_CURRENT_70_00_MA;
		}

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		set_jeita_charging_current();
#endif

		pr_notice(
		"[BATTERY] Default CC mode charging : %d\r\n", g_temp_CC_value);
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

		pr_notice("[BATTERY] Battery real full and disable charging on %d mA\n",
			    BMT_status.ICharging);
	} else if (post_charging_time > 0) {
		post_charging_time += BAT_TASK_PERIOD;
		pr_notice("[BATTERY] post_charging_time=%d,POST_CHARGING_TIME=%d\n",
			    post_charging_time, POST_CHARGING_TIME);
	} else if ((BMT_status.TOPOFF_charging_time > 60)
		   && (BMT_status.ICharging <= charging_full_current)) {
		post_charging_time = BAT_TASK_PERIOD;
		pr_notice(
			    "[BATTERY] Enter Post charge, post_charging_time=%d,POST_CHARGING_TIME=%d\n",
			    post_charging_time, POST_CHARGING_TIME);
	} else {
		post_charging_time = 0;
	}
#else
	static unsigned char full_check_count;

	if (BMT_status.ICharging <= charging_full_current) {
		full_check_count++;
		if (6 == full_check_count) {
			status = true;
			full_check_count = 0;
			pr_notice("[BATTERY] Battery full and disable charging on %d mA\n",
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
	bat_isense_offset = 0;
	battery_meter_sync(bat_isense_offset);
}

static void pchr_sw_cv_charing_current_check(void)
{
	bool charging_enable = true;
	unsigned int csdac_full_flag = true;

	battery_charging_control(CHARGING_CMD_SET_CURRENT, &ulc_cv_charging_current);
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	msleep(192);

	battery_charging_control(CHARGING_CMD_GET_CSDAC_FALL_FLAG, &csdac_full_flag);

	if (csdac_full_flag == true)
		ulc_cv_charging_current = battery_meter_get_charging_current() * 100;
	/* get immedeate charging current and align to enum value */


	while (csdac_full_flag == true && ulc_cv_charging_current != CHARGE_CURRENT_0_00_MA) {
		set_bat_sw_cv_charging_current_limit(ulc_cv_charging_current);
		battery_charging_control(CHARGING_CMD_SET_CURRENT, &ulc_cv_charging_current);
		ulc_cv_charging_current_flag = true;

		msleep(192);	/* large than 512 code x 0.25ms */

		battery_charging_control(CHARGING_CMD_GET_CSDAC_FALL_FLAG, &csdac_full_flag);

		pr_notice(
			    "[BATTERY] Sw CV set charging current, csdac_full_flag=%d, current=%d !\n",
			    csdac_full_flag, ulc_cv_charging_current);
	}

	if (ulc_cv_charging_current == CHARGE_CURRENT_0_00_MA)
		pr_notice("[BATTERY] Sw CV set charging current Error!\n");
}

static void pchr_turn_on_charging(void)
{
#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	BATTERY_VOLTAGE_ENUM cv_voltage;
#endif
	unsigned int charging_enable = true;

	pr_info("[BATTERY] pchr_turn_on_charging()!\r\n");

	if (BMT_status.bat_charging_state == CHR_ERROR) {
		pr_notice("[BATTERY] Charger Error, turn OFF charging !\n");

		charging_enable = false;
	} else if ((g_platform_boot_mode == META_BOOT) || (g_platform_boot_mode == ADVMETA_BOOT)) {
		pr_notice(
			    "[BATTERY] In meta or advanced meta mode, disable charging.\n");
		charging_enable = false;
	} else {
		/*HW initialization */
		pr_info("charging_hw_init\n");
		battery_charging_control(CHARGING_CMD_INIT, NULL);

#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)
		battery_pump_express_algorithm_start();
#endif

		/* Set Charging Current */
		if (get_usb_current_unlimited()) {
			g_temp_CC_value = batt_cust_data.ac_charger_current;
			pr_info("USB_CURRENT_UNLIMITED, use AC_CHARGER_CURRENT\n");
		} else {
			if (g_bcct_flag == 1) {
				pr_info("[BATTERY] select_charging_curret_bcct !\n");
				select_charging_curret_bcct_internal();
			} else {
				pr_info("[BATTERY] select_charging_current !\n");
				select_charging_curret();
			}
		}

		if (g_temp_CC_value == CHARGE_CURRENT_0_00_MA) {
			charging_enable = false;
			pr_notice(
				    "[BATTERY] charging current is set 0mA, turn off charging !\r\n");
		} else {
			#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)
			if (ta_check_ta_control == false) {
			#endif
				if (ulc_cv_charging_current_flag == true)
					battery_charging_control(CHARGING_CMD_SET_CURRENT,
								 &ulc_cv_charging_current);
				else
					battery_charging_control(CHARGING_CMD_SET_CURRENT,
								 &g_temp_CC_value);
			#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)
			}
			#endif
			/* Set CV */
#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
			if (batt_cust_data.high_battery_voltage_support)
				cv_voltage = BATTERY_VOLT_04_350000_V;
			else
				cv_voltage = BATTERY_VOLT_04_200000_V;

			battery_charging_control(CHARGING_CMD_SET_CV_VOLTAGE, &cv_voltage);
#endif
		}
	}

	/* enable/disable charging */
	pr_notice("[BATTERY] pchr_turn_on_charging(), enable =%d \r\n", charging_enable);
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
}


static PMU_STATUS BAT_PreChargeModeAction(void)
{
	pr_notice("[BATTERY] Pre-CC mode charge, timer=%d on %d !!\n\r",
		    BMT_status.PRE_charging_time, BMT_status.total_charging_time);

	BMT_status.PRE_charging_time += BAT_TASK_PERIOD;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.total_charging_time += BAT_TASK_PERIOD;

	ulc_cv_charging_current = g_temp_CC_value;
	ulc_cv_charging_current_flag = false;

	if (BMT_status.UI_SOC == 100) {
		BMT_status.bat_charging_state = CHR_BATFULL;
		BMT_status.bat_full = true;
		g_charging_full_reset_bat_meter = true;
	} else if (BMT_status.bat_vol > batt_cust_data.v_pre2cc_thres) {
		BMT_status.bat_charging_state = CHR_CC;
	}
#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)	/* defined(MTK_LINEAR_CHARGER_NO_DISCHARGE) */
	/* no disable charging */
#else
	#if 0 /* no stop charging */
	{
		bool charging_enable = false;

		/*Charging 9s and discharging 1s : start */
		battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
		msleep(1000);
	}
	#endif
#endif

	charging_current_calibration();
	pchr_turn_on_charging();

	return PMU_STATUS_OK;
}


static PMU_STATUS BAT_ConstantCurrentModeAction(void)
{
	pr_notice("[BATTERY] CC mode charge, timer=%d on %d !!\n\r",
		    BMT_status.CC_charging_time, BMT_status.total_charging_time);

	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time += BAT_TASK_PERIOD;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.total_charging_time += BAT_TASK_PERIOD;

	ulc_cv_charging_current_flag = false;
	ulc_cv_charging_current = g_temp_CC_value;

	if (BMT_status.bat_vol > v_cc2topoff_threshold)
		BMT_status.bat_charging_state = CHR_TOP_OFF;

#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)	/* defined(MTK_LINEAR_CHARGER_NO_DISCHARGE) */
	/* no disable charging#else */
#else
	#if 0 /* no stop charging */
	{
		bool charging_enable = false;

		/* Charging 9s and discharging 1s : start */
		battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
		msleep(1000);
	}
	#endif
#endif

	charging_current_calibration();

	pchr_turn_on_charging();

	return PMU_STATUS_OK;
}


static PMU_STATUS BAT_TopOffModeAction(void)
{
	unsigned int charging_enable = false;
	unsigned int cv_voltage;

	if (batt_cust_data.high_battery_voltage_support)
		cv_voltage = 4350;
	else
		cv_voltage = 4200;


	pr_notice("[BATTERY] Top Off mode charge, timer=%d on %d !!\n\r",
		    BMT_status.TOPOFF_charging_time, BMT_status.total_charging_time);

	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time += BAT_TASK_PERIOD;
	BMT_status.total_charging_time += BAT_TASK_PERIOD;


	if (BMT_status.bat_vol > (cv_voltage - CV_CHECK_DELAT_FOR_BANDGAP)) {	/* CV - 0.08V */
		pchr_sw_cv_charing_current_check();
	}
	pchr_turn_on_charging();

	if ((BMT_status.TOPOFF_charging_time >= MAX_CV_CHARGING_TIME)
	    || (charging_full_check() == true)) {
		BMT_status.bat_charging_state = CHR_BATFULL;
		BMT_status.bat_full = true;
		g_charging_full_reset_bat_meter = true;

		/*  Disable charging */
		battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
	}

	return PMU_STATUS_OK;
}


static PMU_STATUS BAT_BatteryFullAction(void)
{
	unsigned int charging_enable = false;

	pr_notice("[BATTERY] Battery full !!\n\r");

	BMT_status.bat_full = true;
	BMT_status.total_charging_time = 0;
	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.POSTFULL_charging_time = 0;
	BMT_status.bat_in_recharging_state = false;

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	if (BMT_status.bat_vol < g_jeita_recharging_voltage) {
#else
	if (BMT_status.bat_vol < batt_cust_data.recharging_voltage) {
#endif
		pr_notice("[BATTERY] Battery Enter Re-charging!! , vbat=(%d)\n\r",
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


static PMU_STATUS BAT_BatteryHoldAction(void)
{
	unsigned int charging_enable;

	pr_notice("[BATTERY] Hold mode !!\n\r");

	if (BMT_status.bat_vol < batt_cust_data.talking_recharge_voltage
	    || g_call_state == CALL_IDLE) {
		BMT_status.bat_charging_state = CHR_CC;
		pr_notice("[BATTERY] Exit Hold mode and Enter CC mode !!\n\r");
	}

	/*  Disable charger */
	charging_enable = false;
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	return PMU_STATUS_OK;
}


static PMU_STATUS BAT_BatteryStatusFailAction(void)
{
	unsigned int charging_enable;

	pr_notice("[BATTERY] BAD Battery status... Charging Stop !!\n\r");

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	if ((g_temp_status == TEMP_ABOVE_POS_60) || (g_temp_status == TEMP_BELOW_NEG_10))
		temp_error_recovery_chr_flag = false;

	if ((temp_error_recovery_chr_flag == false) && (g_temp_status != TEMP_ABOVE_POS_60)
	    && (g_temp_status != TEMP_BELOW_NEG_10)) {
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


void mt_battery_charging_algorithm_internal(void)
{
	__init_charging_varaibles();
#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)
	battery_pump_express_charger_check();
#endif
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
