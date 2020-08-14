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

#ifndef _BATTERY_METER_H
#define _BATTERY_METER_H

/* ============================================================ */
/* define */
/* ============================================================ */
#define FG_CURRENT_AVERAGE_SIZE 30
#define FG_VBAT_AVERAGE_SIZE	18

#ifdef CONFIG_MTK_MULTI_BAT_PROFILE_SUPPORT
#define MTK_GET_BATTERY_ID_BY_AUXADC
#define BATTERY_ID_CHANNEL_NUM 0
#define ID_VOLT_END (-1)
#endif

#define CUST_CAPACITY_OCV2CV_TRANSFORM
/* ============================================================ */
/* ENUM */
/* ============================================================ */

/* ============================================================ */
/* structure */
/* ============================================================ */

#define FGD_NL_MSG_T_HDR_LEN 12
#define FGD_NL_MSG_MAX_LEN 9200

struct fgd_nl_msg_t {
	unsigned int fgd_cmd;
	unsigned int fgd_data_len;
	unsigned int fgd_ret_data_len;
	char fgd_data[FGD_NL_MSG_MAX_LEN];
};

enum {
	FG_MAIN = 1,
	FG_SUSPEND = 2,
	FG_RESUME = 4,
	FG_CHARGER = 8
};

enum {
	HW_FG,
	SW_FG,
	AUXADC
};

/* ============================================================ */
/* typedef */
/* ============================================================ */
#define BATT_TEMPERATURE struct batt_temperature
struct batt_temperature {
	signed int BatteryTemp;
	signed int TemperatureR;
};

struct battery_meter_custom_data {
	int vbat_channel_number;
	int isense_channel_number;
	int vbattemp_channel_number;
	int vcharger_channel_number;
	int swchr_power_path;

	/* mt_battery_meter.h */
	/* ADC resister */
	int r_bat_sense;
	int r_i_sense;
	int r_charger_1;
	int r_charger_2;

	int temperature_t0;
	int temperature_t1;
	int temperature_t1_5;
	int temperature_t2;
	int temperature_t3;
	int temperature_t;

	int fg_meter_resistance;

	/* Qmax for battery  */
	int q_max_pos_50;
	int q_max_pos_25;
	int q_max_pos_10;
	int q_max_pos_0;
	int q_max_neg_10;
	int q_max_pos_50_h_current;
	int q_max_pos_25_h_current;
	int q_max_pos_10_h_current;
	int q_max_pos_0_h_current;
	int q_max_neg_10_h_current;

	int oam_d5;		/* 1 : D5,   0: D2 */

	int change_tracking_point;
	int cust_tracking_point;
	int cust_r_sense;
	int cust_hw_cc;
	int aging_tuning_value;
	int cust_r_fg_offset;
	int ocv_board_compesate;
	int r_fg_board_base;
	int r_fg_board_slope;
	int car_tune_value;

	/* HW Fuel gague  */
	int current_detect_r_fg;
	int minerroroffset;
	int fg_vbat_average_size;
	int r_fg_value;
	int cust_poweron_delta_capacity_tolrance;
	int cust_poweron_low_capacity_tolrance;
	int cust_poweron_max_vbat_tolrance;
	int cust_poweron_delta_vbat_tolrance;
	int cust_poweron_delta_hw_sw_ocv_capacity_tolrance;

	int fixed_tbat_25;

	/* Dynamic change wake up period of battery thread when suspend */
	int vbat_normal_wakeup;
	int vbat_low_power_wakeup;
	int normal_wakeup_period;
	int low_power_wakeup_period;
	int close_poweroff_wakeup_period;

	/* mt_battery_meter.h */
	int bat_ntc;
	int rbat_pull_up_r;
	int rbat_pull_down_r;
	int rbat_pull_up_volt;

	int std_loading_current;
	int step_of_qmax;
	int battery_id_channel_number;
	int slp_current;
	int slp_current_wifi;
};

enum FG_DAEMON_CTRL_CMD_FROM_USER {
	FG_DAEMON_CMD_GET_INIT_FLAG,
	FG_DAEMON_CMD_GET_SOC,
	FG_DAEMON_CMD_GET_DOD0,
	FG_DAEMON_CMD_GET_DOD1,
	FG_DAEMON_CMD_GET_HW_OCV,
	FG_DAEMON_CMD_GET_HW_FG_INIT_CURRENT,
	FG_DAEMON_CMD_GET_HW_FG_CURRENT,
	FG_DAEMON_CMD_GET_HW_FG_INIT_CURRENT_SIGN,
	FG_DAEMON_CMD_GET_HW_FG_CURRENT_SIGN,
	FG_DAEMON_CMD_GET_HW_FG_CAR_ACT,
	FG_DAEMON_CMD_GET_TEMPERTURE,
	FG_DAEMON_CMD_DUMP_REGISTER,
	FG_DAEMON_CMD_CHARGING_ENABLE,
	FG_DAEMON_CMD_GET_BATTERY_INIT_VOLTAGE,
	FG_DAEMON_CMD_GET_BATTERY_VOLTAGE,
	FG_DAEMON_CMD_FGADC_RESET,
	FG_DAEMON_CMD_GET_BATTERY_PLUG_STATUS,
	FG_DAEMON_CMD_GET_RTC_SPARE_FG_VALUE,
	FG_DAEMON_CMD_IS_CHARGER_EXIST,
	FG_DAEMON_CMD_IS_BATTERY_FULL,	/* bat_is_battery_full, */
	FG_DAEMON_CMD_SET_BATTERY_FULL,	/* bat_set_battery_full, */
	FG_DAEMON_CMD_SET_RTC,	/* set RTC, */
	FG_DAEMON_CMD_SET_POWEROFF,	/* set Poweroff, */
	FG_DAEMON_CMD_IS_KPOC,	/* is KPOC, */
	FG_DAEMON_CMD_GET_BOOT_REASON,	/* g_boot_reason, */
	FG_DAEMON_CMD_GET_CHARGING_CURRENT,
	FG_DAEMON_CMD_GET_CHARGER_VOLTAGE,
	FG_DAEMON_CMD_GET_SHUTDOWN_COND,
	FG_DAEMON_CMD_GET_CUSTOM_SETTING,
	FG_DAEMON_CMD_GET_UI_SOC,
	FG_DAEMON_CMD_GET_CV_VALUE,
	FG_DAEMON_CMD_GET_DURATION_TIME,
	FG_DAEMON_CMD_GET_TRACKING_TIME,
	FG_DAEMON_CMD_GET_CURRENT_TH,
	FG_DAEMON_CMD_GET_CHECK_TIME,
	FG_DAEMON_CMD_GET_DIFFERENCE_VOLTAGE_UPDATE,
	FG_DAEMON_CMD_GET_AGING1_LOAD_SOC,
	FG_DAEMON_CMD_GET_AGING1_UPDATE_SOC,
	FG_DAEMON_CMD_GET_SHUTDOWN_SYSTEM_VOLTAGE,
	FG_DAEMON_CMD_GET_CHARGE_TRACKING_TIME,
	FG_DAEMON_CMD_GET_DISCHARGE_TRACKING_TIME,
	FG_DAEMON_CMD_GET_SHUTDOWN_GAUGE0,
	FG_DAEMON_CMD_GET_SHUTDOWN_GAUGE1_XMINS,
	FG_DAEMON_CMD_GET_SHUTDOWN_GAUGE1_MINS,
	FG_DAEMON_CMD_SET_SUSPEND_TIME,
	FG_DAEMON_CMD_SET_WAKEUP_SMOOTH_TIME,
	FG_DAEMON_CMD_SET_IS_CHARGING,
	FG_DAEMON_CMD_SET_RBAT,
	FG_DAEMON_CMD_SET_SWOCV,
	FG_DAEMON_CMD_SET_DOD0,
	FG_DAEMON_CMD_SET_DOD1,
	FG_DAEMON_CMD_SET_QMAX,
	FG_DAEMON_CMD_SET_SOC,
	FG_DAEMON_CMD_SET_UI_SOC,
	FG_DAEMON_CMD_SET_UI_SOC2,
	FG_DAEMON_CMD_SET_INIT_FLAG,
	FG_DAEMON_CMD_SET_DAEMON_PID,
	FG_DAEMON_CMD_NOTIFY_DAEMON,

	FG_DAEMON_CMD_FROM_USER_NUMBER
};

struct battery_meter_data {
	int batt_capacity_aging;
	int batt_capacity;
	int aging_factor;
	int battery_cycle;
	int columb_sum;
};


/* ============================================================ */
/* External Variables */
/* ============================================================ */

extern struct battery_meter_custom_data batt_meter_cust_data;

#ifdef MTK_ENABLE_AGING_ALGORITHM
extern unsigned int suspend_time;
#endif
extern bool bat_spm_timeout;
extern unsigned int _g_bat_sleep_total_time;

#if defined(CONFIG_AMAZON_METRICS_LOG)
extern signed int gFG_BATT_CAPACITY_aging;
extern signed int gFG_BATT_CAPACITY;
#endif

/* ============================================================ */
/* External function */
/* ============================================================ */
extern signed int battery_meter_get_data(struct battery_meter_data *aging);
extern signed int battery_meter_get_battery_voltage(bool update);
extern signed int battery_meter_get_charging_current_imm(void);
extern signed int battery_meter_get_charging_current(void);
extern signed int battery_meter_get_battery_current(void);
extern bool battery_meter_get_battery_current_sign(void);
extern signed int battery_meter_get_car(void);
extern signed int battery_meter_get_charge_full(void);
extern signed int battery_meter_get_charge_counter(void);
extern signed int battery_meter_get_battery_temperature(void);
extern signed int battery_meter_get_charger_voltage(void);
extern signed int battery_meter_get_vbus_now(void);
extern signed int battery_meter_get_battery_percentage(void);
extern signed int battery_meter_initial(void);
extern signed int battery_meter_reset(void);
extern signed int battery_meter_sync(signed int bat_i_sense_offset);

extern signed int battery_meter_get_battery_zcv(void);

/* 15% zcv,  15% can be customized */
extern signed int battery_meter_get_battery_nPercent_zcv(void);
/* tracking point */
extern signed int battery_meter_get_battery_nPercent_UI_SOC(void);

extern signed int battery_meter_get_tempR(signed int dwVolt);
extern signed int battery_meter_get_tempV(void);
extern signed int battery_meter_get_VSense(void);/* isense voltage */
extern int wakeup_fg_algo(int flow_state);

extern int batt_meter_init_cust_data(void);
#if defined(CUST_CAPACITY_OCV2CV_TRANSFORM)
extern void battery_meter_set_reset_soc(bool bUSE_UI_SOC);
extern signed int battery_meter_get_battery_soc(void);
#endif

#ifdef CONFIG_MTK_MULTI_BAT_PROFILE_SUPPORT
extern int IMM_GetOneChannelValue_Cali(int Channel, int *voltage);
#ifdef MTK_GET_BATTERY_ID_BY_AUXADC
extern unsigned int upmu_get_reg_value(unsigned int reg);
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_IsAdcInitReady(void);
extern unsigned int pmic_config_interface(unsigned int RegNum,
		unsigned int val, unsigned int MASK, unsigned int SHIFT);
extern unsigned int pmic_read_interface(unsigned int RegNum,
		unsigned int *val, unsigned int MASK, unsigned int SHIFT);
extern unsigned int get_pmic_mt6325_cid(void);
extern bool get_battery_id_status(void);
#endif
#endif

#endif /* #ifndef _BATTERY_METER_H */
