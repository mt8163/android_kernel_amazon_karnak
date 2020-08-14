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

#ifndef BATTERY_COMMON_H
#define BATTERY_COMMON_H

#include <linux/ioctl.h>
#include <mt-plat/charging.h>
#include <linux/time.h>

/*****************************************************************************
 *  BATTERY VOLTAGE
 ****************************************************************************/
#define PRE_CHARGE_VOLTAGE                  (3200)
#define SYSTEM_OFF_VOLTAGE                  (3400)
#define CONSTANT_CURRENT_CHARGE_VOLTAGE     (4100)
#define CONSTANT_VOLTAGE_CHARGE_VOLTAGE     (4200)
#define CV_DROPDOWN_VOLTAGE                 (4000)
#define CHARGER_THRESH_HOLD                 (4300)
#define BATTERY_UVLO_VOLTAGE                (2700)
#ifndef SHUTDOWN_SYSTEM_VOLTAGE
#define SHUTDOWN_SYSTEM_VOLTAGE		(3400)
#endif

/* Precise Tunning */
#define BATTERY_AVERAGE_DATA_NUMBER	3
#define BATTERY_AVERAGE_SIZE	30

/*****************************************************************************
 *  BATTERY TIMER
 ****************************************************************************/
#define MAX_POSTFULL_SAFETY_TIME		(1*30*60)/* 30mins */
#define MAX_PreCC_CHARGING_TIME		(1*30*60)/* 0.5hr */

/* #define MAX_CV_CHARGING_TIME                  1*30*60         // 0.5hr */
#define MAX_CV_CHARGING_TIME			(3*60*60)/* 3hr */


#define MUTEX_TIMEOUT                       (5000)
#define BAT_TASK_PERIOD                     (10)/* 10sec */
#define g_free_bat_temp					(100)0	/* 1 s */

/*****************************************************************************
 *  BATTERY Protection
 ****************************************************************************/
#define Battery_Percent_100    (100)
#define charger_OVER_VOL	    (1)
#define BATTERY_UNDER_VOL		(2)
#define BATTERY_OVER_TEMP		(3)
#define ADC_SAMPLE_TIMES        (5)

/*****************************************************************************
 *  Pulse Charging State
 ****************************************************************************/
#define  CHR_PRE                        (0x1000)
#define  CHR_CC                         (0x1001)
#define  CHR_TOP_OFF                    (0x1002)
#define  CHR_POST_FULL                  (0x1003)
#define  CHR_BATFULL                    (0x1004)
#define  CHR_ERROR                      (0x1005)
#define  CHR_HOLD						(0x1006)

/*****************************************************************************
 *  CallState
 ****************************************************************************/
#define CALL_IDLE (0)
#define CALL_ACTIVE (1)

/*****************************************************************************
 *  Enum
 ****************************************************************************/
enum PMU_STATUS {
	PMU_STATUS_OK = 0,
	PMU_STATUS_FAIL = 1,
};

enum USB_STATE_ENUM {
	USB_SUSPEND = 0,
	USB_UNCONFIGURED,
	USB_CONFIGURED
};

enum BATTERY_AVG_ENUM {
	BATTERY_AVG_CURRENT = 0,
	BATTERY_AVG_VOLT = 1,
	BATTERY_AVG_TEMP = 2,
	BATTERY_AVG_MAX
};

enum BATTERY_TIME_ENUM {
	BATTERY_THREAD_TIME = 0,
	CAR_TIME,
	SUSPEND_TIME,
	DURATION_NUM
};

/*****************************************************************************
 *  JEITA battery temperature standard
 *  charging info ,like temperatue, charging current, re-charging voltage,
 *  CV threshold would be reconfigurated.
 *  Temperature hysteresis default 6C.
 *  Reference table:
 *  degree   AC Current    USB current CV threshold  Recharge  hysteresis cond.
 *  > 60     no charging current,          X               X     <54
 *  45~60    600mA         450mA           4.1V            4V    <39 >60
 *  10~45    600mA         450mA           4.2V            4.1V  <10 >45
 *  0~10     600mA         450mA           4.1V            4V    <0  >16
 *  -10~0    200mA         200mA           4V              3.9V  <-10 >6
 *  <-10     no charging current,          X               X     >-10
 ****************************************************************************/
enum temp_state_enum {
	TEMP_BELOW_NEG_10 = 0,
	TEMP_NEG_10_TO_POS_0,
	TEMP_POS_0_TO_POS_10,
	TEMP_POS_10_TO_POS_45,
	TEMP_POS_45_TO_POS_60,
	TEMP_ABOVE_POS_60
};

#define TEMP_POS_60_THRESHOLD  60
#define TEMP_POS_60_THRES_MINUS_X_DEGREE 60

#define TEMP_POS_45_THRESHOLD  45
#define TEMP_POS_45_THRES_MINUS_X_DEGREE 45

#define TEMP_POS_10_THRESHOLD  14
#define TEMP_POS_10_THRES_PLUS_X_DEGREE 14

#define TEMP_POS_0_THRESHOLD  0
#define TEMP_POS_0_THRES_PLUS_X_DEGREE 0

#define TEMP_NEG_10_THRESHOLD  -10
#define TEMP_NEG_10_THRES_PLUS_X_DEGREE  -10

/*****************************************************************************
 *  Normal battery temperature state
 ****************************************************************************/
enum batt_temp_state_enum {
	TEMP_POS_LOW = 0,
	TEMP_POS_NORMAL,
	TEMP_POS_HIGH
};

/*****************************************************************************
 *  structure
 ****************************************************************************/
#define PMU_ChargerStruct struct pmu_chargerstruct
struct pmu_chargerstruct {
	bool bat_exist;
	bool bat_full;
	signed int bat_charging_state;
	unsigned int bat_vol;
	bool bat_in_recharging_state;
	unsigned int Vsense;
	bool charger_exist;
	unsigned int charger_vol;
	signed int charger_protect_status;
	signed int ICharging;
	signed int IBattery;
	signed int temperature;
	signed int temperatureR;
	signed int temperatureV;
	unsigned int total_charging_time;
	unsigned int PRE_charging_time;
	unsigned int CC_charging_time;
	unsigned int TOPOFF_charging_time;
	unsigned int POSTFULL_charging_time;
	unsigned int charger_type;
	signed int SOC;
	signed int UI_SOC;
	signed int pre_UI_SOC;
	unsigned int nPercent_ZCV;
	unsigned int nPrecent_UI_SOC_check_point;
	unsigned int ZCV;
	bool aicl_done;
	bool ap15_charger_detected;
	int aicl_result;
	bool force_trigger_charging;
};

struct battery_custom_data {
	/* mt_charging.h */
	/* stop charging while in talking mode */
	int stop_charging_in_takling;
	int talking_recharge_voltage;
	int talking_sync_time;

	/* Battery Temperature Protection */
	int mtk_temperature_recharge_support;
	int max_charge_temperature;
	int max_charge_temperature_minus_x_degree;
	int min_charge_temperature;
	int min_charge_temperature_plus_x_degree;
	int err_charge_temperature;

	int max_charging_time;

	/* Linear Charging Threshold */
	int v_pre2cc_thres;
	int v_cc2topoff_thres;
	int recharging_voltage;
	int charging_full_current;

	/* Charging Current Setting */
	int config_usb_if;
	int usb_charger_current_suspend;
	int usb_charger_current_unconfigured;
	int usb_charger_current_configured;
	int usb_charger_current;
	int ac_charger_input_current;
	int ac_charger_current;
	int non_std_ac_charger_input_current;
	int non_std_ac_charger_current;
	int charging_host_charger_current;
	int apple_0_5a_charger_current;
	int apple_1_0a_charger_current;
	int apple_2_1a_charger_current;
	int charge_current_termination;


	/* charger error check */
	int bat_low_temp_protect_enable;
	int v_charger_enable;
	int v_charger_max;
	int v_charger_min;

	/* Tracking TIME */
	int onehundred_percent_tracking_time;
	int npercent_tracking_time;
	int sync_to_real_tracking_time;
	int v_0percent_tracking;
	int system_off_voltage;


	/* High battery support */
	int high_battery_voltage_support;

	/* JEITA parameter */
	int mtk_jeita_standard_support;
	int cust_soc_jeita_sync_time;
	int jeita_recharge_voltage;
	int jeita_temp_above_pos_60_cv_voltage;
	int jeita_temp_pos_45_to_pos_60_cv_voltage;
	int jeita_temp_pos_10_to_pos_45_cv_voltage;
	int jeita_temp_pos_0_to_pos_10_cv_voltage;
	int jeita_temp_neg_10_to_pos_0_cv_voltage;
	int jeita_temp_below_neg_10_cv_voltage;

	/* For JEITA Linear Charging only */
	int jeita_neg_10_to_pos_0_full_current;
	int jeita_temp_pos_45_to_pos_60_recharge_voltage;
	int jeita_temp_pos_10_to_pos_45_recharge_voltage;
	int jeita_temp_pos_0_to_pos_10_recharge_voltage;
	int jeita_temp_neg_10_to_pos_0_recharge_voltage;
	int jeita_temp_pos_45_to_pos_60_cc2topoff_threshold;
	int jeita_temp_pos_10_to_pos_45_cc2topoff_threshold;
	int jeita_temp_pos_0_to_pos_10_cc2topoff_threshold;
	int jeita_temp_neg_10_to_pos_0_cc2topoff_threshold;

	/* cust_pe.h */
	int mtk_pump_express_plus_support;
	int ta_start_battery_soc;
	int ta_stop_battery_soc;
	int ta_ac_9v_input_current;
	int ta_ac_7v_input_current;
	int ta_ac_charging_current;
	int ta_9v_support;

	/* SW AICL detection */
	bool aicl_enable;
	int aicl_charging_current_max;
	int aicl_input_current_max;
	int aicl_input_current_min;
	int aicl_step_current;
	int aicl_step_interval;
	int aicl_vbus_valid;
	int aicl_vbus_state_phase;

	/* SW AICL for non-dock mode */
	int ap15_charger_input_current_max;
	int ap15_charger_input_current_min;

	/* SW AICL for dock mode */
	int ap15_dock_input_current_max;
	int ap15_dock_input_current_min;

	int toggle_charge_enable;
};

/*****************************************************************************
 *  Extern Variable
 ****************************************************************************/
extern PMU_ChargerStruct BMT_status;
extern struct battery_custom_data batt_cust_data;
extern CHARGING_CONTROL battery_charging_control;
extern bool g_ftm_battery_flag;
extern int charging_level_data[1];
extern unsigned int g_call_state;
extern bool g_charging_full_reset_bat_meter;
extern signed int g_custom_charging_current;
extern signed int g_custom_charging_cv;
extern unsigned int g_custom_charging_mode;

/*****************************************************************************
 *  Extern Function
 ****************************************************************************/
extern void charging_suspend_enable(void);
extern void charging_suspend_disable(void);
extern bool bat_is_charger_exist(void);
extern bool bat_is_charging_full(void);
extern unsigned int bat_get_ui_percentage(void);
extern unsigned int get_charging_setting_current(void);
extern unsigned int bat_is_recharging_phase(void);
extern void do_chrdet_int_task(void);
extern void set_usb_current_unlimited(bool enable);
extern bool get_usb_current_unlimited(void);
extern unsigned int mt_get_charger_type(void);

extern unsigned int mt_battery_get_duration_time(int duration_type);
extern void mt_battery_update_time(struct timespec *pre_time,
		unsigned int duration_type);
extern unsigned int mt_battery_shutdown_check(void);
extern unsigned char bat_is_kpoc(void);

#ifdef CONFIG_MTK_SMART_BATTERY
extern void wake_up_bat(void);
extern void wake_up_bat2(void);
extern void wake_up_bat3(void);

extern unsigned long BAT_Get_Battery_Voltage(int polling_mode);
extern void mt_battery_charging_algorithm(void);
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
extern unsigned int do_jeita_state_machine(void);
#endif

#else

#define wake_up_bat()			do {} while (0)
#define wake_up_bat2()			do {} while (0)
#define wake_up_bat3()			do {} while (0)

#define BAT_Get_Battery_Voltage(polling_mode)	({ 0; })

#endif

#ifdef CONFIG_MTK_POWER_EXT_DETECT
extern bool bat_is_ext_power(void);
#endif

extern int g_platform_boot_mode;
extern bool mt_usb_is_device(void);
#if defined(CONFIG_USB_MTK_HDRC) || defined(CONFIG_USB_MU3D_DRV)
extern void mt_usb_connect(void);
extern void mt_usb_disconnect(void);
#else
#define mt_usb_connect() do { } while (0)
#define mt_usb_disconnect() do { } while (0)
#endif
void check_battery_exist(void);

extern int get_bat_charging_current_limit(void);
extern unsigned int set_bat_charging_current_limit(int current_limit);
extern bool is_usb_rdy(void);
extern unsigned int upmu_get_reg_value(unsigned int reg);
extern bool pmic_chrdet_status(void);
#endif				/* #ifndef BATTERY_COMMON_H */
