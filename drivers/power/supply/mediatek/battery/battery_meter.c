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

#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/iio/consumer.h>

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/slab.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <linux/uaccess.h>
#include <linux/netdevice.h>


#include <mt-plat/mtk_boot.h>
#include <mt-plat/mtk_rtc.h>


#include <mt-plat/mtk_boot_reason.h>

#include <mt-plat/battery_meter.h>
#include <mt-plat/battery_meter_table.h>
#include <mt-plat/battery_common.h>
#include <mt-plat/battery_meter_hal.h>
#include <mach/mtk_battery_meter.h>
#include <mt-plat/upmu_common.h>
#include "battery_metrics.h"

#if defined(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT)
#include <mt-plat/internal_charging.h>
#endif
/* ============================================================ // */
/* define */
/* ============================================================ // */
#define PROFILE_SIZE 5
#define DEFAULT_SLP_CURRENT 4

static DEFINE_MUTEX(FGADC_mutex);
static DEFINE_MUTEX(qmax_mutex);

#if defined(CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT)
#define MAX_BATTERY_CYCLE 500
enum aging_state {
	AGING_RESET = 0,
	AGING_STATE_1
};
#endif

/* ============================================================ // */
/* global variable */
/* ============================================================ // */
static BATTERY_METER_CONTROL battery_meter_ctrl;

bool gFG_Is_Charging;
static signed int g_auxadc_solution;
static unsigned int g_spm_timer = 600;
#ifdef MTK_ENABLE_AGING_ALGORITHM
static unsigned int suspend_time;
#endif
static signed int g_booting_vbat;

#if defined(CUST_CAPACITY_OCV2CV_TRANSFORM)
static signed int g_currentfactor = 100;
static bool g_USE_UI_SOC = true;
#if defined(CUST_SYSTEM_OFF_VOLTAGE)
#define SYSTEM_OFF_VOLTAGE CUST_SYSTEM_OFF_VOLTAGE
#endif
#endif

bool bat_spm_timeout;
unsigned int _g_bat_sleep_total_time;

/* //////////////////////////////////////////////////////////
 *     PMIC AUXADC Related Variable
 * //////////////////////////////////////////////////////////
 */
int g_R_BAT_SENSE;
int g_R_I_SENSE;
int g_R_CHARGER_1;
int g_R_CHARGER_2;

int fg_qmax_update_for_aging_flag = 1;

/* HW FG */
signed int gFG_DOD0;
signed int gFG_DOD1;
signed int gFG_columb;
signed int gFG_voltage;
signed int gFG_current;
signed int gFG_capacity;
signed int gFG_capacity_by_c;
signed int gFG_capacity_by_c_init;
signed int gFG_capacity_by_v;
signed int gFG_capacity_by_v_init;
signed int gFG_temp = 100;
signed int gFG_resistance_bat;
signed int gFG_compensate_value;
signed int gFG_ori_voltage;
signed int gFG_BATT_CAPACITY;
signed int gFG_voltage_init;
signed int gFG_current_auto_detect_R_fg_total;
signed int gFG_current_auto_detect_R_fg_count;
signed int gFG_current_auto_detect_R_fg_result;
signed int gFG_15_vlot = 3700;
signed int gFG_BATT_CAPACITY_init_high_current = 1200;
signed int gFG_BATT_CAPACITY_aging = 1200;

/* voltage mode */
signed int gfg_percent_check_point = 50;
signed int volt_mode_update_timer;
signed int volt_mode_update_time_out = 6;	/* 1mins */

/* EM */
signed int g_fg_dbg_bat_volt;
signed int g_fg_dbg_bat_current;
signed int g_fg_dbg_bat_zcv;
signed int g_fg_dbg_bat_temp;
signed int g_fg_dbg_bat_r;
signed int g_fg_dbg_bat_car;
signed int g_fg_dbg_bat_qmax;
signed int g_fg_dbg_d0;
signed int g_fg_dbg_d1;
signed int g_fg_dbg_percentage;
signed int g_fg_dbg_percentage_fg;
signed int g_fg_dbg_percentage_voltmode;

signed int FGvbatVoltageBuffer[FG_VBAT_AVERAGE_SIZE];
signed int FGbatteryIndex;
signed int FGbatteryVoltageSum;
signed int gFG_voltage_AVG;
signed int gFG_vbat_offset;
#ifdef Q_MAX_BY_CURRENT
signed int FGCurrentBuffer[FG_CURRENT_AVERAGE_SIZE];
signed int FGCurrentIndex;
signed int FGCurrentSum;
signed int gFG_current_AVG;
#endif
signed int g_tracking_point;	/* CUST_TRACKING_POINT; */
signed int g_rtc_fg_soc;
signed int g_I_SENSE_offset;

/* SW FG */
signed int oam_v_ocv_init;
signed int oam_v_ocv_1;
signed int oam_v_ocv_2;
signed int oam_r_1;
signed int oam_r_2;
signed int oam_d0;
signed int oam_i_ori;
signed int oam_i_1;
signed int oam_i_2;
signed int oam_car_1;
signed int oam_car_2;
signed int oam_d_1 = 1;
signed int oam_d_2 = 1;
signed int oam_d_3 = 1;
signed int oam_d_3_pre;
signed int oam_d_4;
signed int oam_d_4_pre;
signed int oam_d_5;
signed int oam_init_i;
signed int oam_run_i;
signed int d5_count;
signed int d5_count_time = 60;
signed int d5_count_time_rate = 1;
signed int g_d_hw_ocv;
signed int g_vol_bat_hw_ocv;
signed int g_hw_ocv_before_sleep;
struct timespec g_rtc_time_before_sleep, xts_before_sleep;
signed int g_sw_vbat_temp;
struct timespec last_oam_run_time;

/* Workaround for high WIFI standby power consumption
 * Only care about WIFI on with network
 */
static int g_wifi_on;
static int g_slp_current;
static int g_slp_time_min = 20;
static bool g_skip_reset_sleep_time;


/* aging mechanism */
#ifdef MTK_ENABLE_AGING_ALGORITHM

#ifdef SOC_BY_HW_FG
static signed int aging_ocv_1;
static signed int aging_ocv_2;
static signed int aging_car_1;
static signed int aging_car_2;
static signed int aging_dod_1;
static signed int aging_dod_2;
#ifdef MD_SLEEP_CURRENT_CHECK
static signed int columb_before_sleep = 0x123456;
#endif
#endif
/* static time_t aging_resume_time_1; */
/* static time_t aging_resume_time_2; */

#ifndef SELF_DISCHARGE_CHECK_THRESHOLD
#define SELF_DISCHARGE_CHECK_THRESHOLD 10
#endif

#ifndef OCV_RECOVER_TIME
#define OCV_RECOVER_TIME 2100
#endif

#ifndef DOD1_ABOVE_THRESHOLD
#define DOD1_ABOVE_THRESHOLD 30
#endif

#ifndef DOD2_BELOW_THRESHOLD
#define DOD2_BELOW_THRESHOLD 70
#endif

#ifndef MIN_DOD_DIFF_THRESHOLD
#define MIN_DOD_DIFF_THRESHOLD 60
#endif

#ifndef MIN_AGING_FACTOR
#define MIN_AGING_FACTOR 90
#endif

#endif

/* battery info */
#if defined(CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT)

signed int gFG_battery_cycle;
signed int gFG_aging_factor = 100;
signed int gFG_columb_sum;
signed int gFG_pre_columb_count;

signed int gFG_max_voltage;
signed int gFG_min_voltage = 10000;
signed int gFG_max_current;
signed int gFG_min_current;
signed int gFG_max_temperature = -20;
signed int gFG_min_temperature = 100;

#endif				/* battery info */

/* extern char *saved_command_line; */
/* Temperature window size */
#define TEMP_AVERAGE_SIZE	30
#define CONSTRUCT_BUFFER_SIZE	3

bool gFG_Is_offset_init;
unsigned int g_fg_battery_id;
static const char *bat_vendor;

#ifdef CONFIG_MTK_MULTI_BAT_PROFILE_SUPPORT
static struct iio_channel *g_bat_id_iio_channel;
/*extern int IMM_GetOneChannelValue_Cali(int Channel, int *voltage);*/
/* 0~1.2V for battery 0, 1.2V~ for battery 1*/
static int g_battery_id_voltage[] = {
	1200,
	ID_VOLT_END,
	ID_VOLT_END,
	ID_VOLT_END,
	ID_VOLT_END,
	ID_VOLT_END
};

#ifdef MTK_GET_BATTERY_ID_BY_AUXADC
static unsigned int total_battery_number = 2;
static unsigned int id_volt_max = 1400;
static unsigned int id_volt_min = 800;

static int get_pin_id_volt(int index, int *volt)
{
	struct iio_channel *channel;
	int *val = volt;
	int ret;

	if (!g_bat_id_iio_channel){
		pr_err("g_bat_id_iio_channel is null\n");
		*volt = 0;
		return -EINVAL;
	}

	channel = &g_bat_id_iio_channel[index];
	if (!channel)
		return -EINVAL;

	ret = iio_read_channel_processed(channel, val);
	if (ret < 0) {
		pr_err("IIO channel read failed %d\n", ret);
		return ret;
	}
	*val = *val * 1500 / 4096;
	return 0;
}

void fgauge_get_profile_id(void)
{
	int id_volt;
	int id;
	int ret;

	ret = get_pin_id_volt(batt_meter_cust_data.battery_id_channel_number,
		&id_volt);
	if (ret != 0 || id_volt == 0)
		pr_notice("[fgauge_get_profile_id]id_volt read fail\n");
	else
		pr_notice("[fgauge_get_profile_id]id_volt = %d\n", id_volt);

	if (total_battery_number >
		(sizeof(g_battery_id_voltage) / sizeof(signed int))) {
		pr_notice("[%s]total number(%d) incorrect!\n",
			__func__, total_battery_number);
		return;
	}

	for (id = 0; id < total_battery_number; id++) {
		if ((g_battery_id_voltage[id] == ID_VOLT_END) ||
		(id_volt < g_battery_id_voltage[id])) {
			g_fg_battery_id = id;
			break;
		}
	}

	pr_notice("[fgauge_get_profile_id]Battery id (%d)\n", g_fg_battery_id);
}
bool get_battery_id_status(void)
{
	int ret, id_volt;
	bool id_connect = true;

	ret = get_pin_id_volt(batt_meter_cust_data.battery_id_channel_number,
		&id_volt);
	if ((id_volt > id_volt_max) || (id_volt < id_volt_min)) {
		pr_notice("[%s] battery ID disconnect(%d) id_volt_max:%d id_volt_min:%d \n",
				__func__, id_volt,id_volt_max,id_volt_min);
		id_connect = false;
	}

	return id_connect;
}
#else
void fgauge_get_profile_id(void)
{
	g_fg_battery_id = 0;
}
#endif
#endif

/* ============================================================ // */
/* function prototype */
/* ============================================================ // */
struct battery_meter_custom_data batt_meter_cust_data;
#ifdef CONFIG_OF
static void __batt_meter_parse_node(const struct device_node *np,
	const char *node_srting, int *cust_val)
{
	u32 val;

	if (of_property_read_u32(np, node_srting, &val) == 0) {
		(*cust_val) = (int)val;
		pr_debug("Get %s: %d\n", node_srting, (*cust_val));
	} else {
		pr_notice("Get %s failed\n", node_srting);
	}
}

static void __batt_meter_parse_table(const struct device_node *np,
	const char *node_srting, struct battery_profile_struct *profile_p)
{
	int addr, val, idx, saddles;

	/* the number of battery table is the same as the number of r table*/
	saddles = fgauge_get_saddles();
	idx = 0;
	pr_notice("batt_meter_parse_table: %s, %d\n", node_srting, saddles);

	while (!of_property_read_u32_index(np, node_srting, idx, &addr)) {
		idx++;
		if (!of_property_read_u32_index(np, node_srting, idx, &val)) {
			pr_debug("batt_temperature_table: addr: %d, val: %d\n",
				    addr, val);
		}

		profile_p->percentage = addr;
		profile_p->voltage = val;

		/* dump parsing data */
		#if 0
		msleep(20);
		pr_notice("__batt_meter_parse_table>> %s[%d]: <%d, %d>\n",
			node_srting, (idx/2), profile_p->percentage,
			profile_p->voltage);
		#endif

		profile_p++;
		if ((idx++) >= (saddles * 2))
			break;
	}

	/* use last data to fill with the rest array
	 * if raw data is less than temp array
	 */
	/* error handle */
	if (idx == 0) {
		pr_notice("[%s]:No table, %s\n", __func__, node_srting);
		return;
	}

	profile_p--;

	while (idx < (saddles * 2)) {
		profile_p++;
		if (profile_p != NULL) {
			profile_p->percentage = addr;
			profile_p->voltage = val;
		}
		idx = idx + 2;

		/* dump parsing data */
		#if 0
		msleep(20);
		pr_debug("__batt_meter_parse_table>> %s[%d]: <%d, %d>\n",
			node_srting, (idx/2) - 1, profile_p->percentage,
			profile_p->voltage);
		#endif
	}
}

#if defined(CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT)
static void reset_aging_data(void)
{
	memcpy(battery_profile_t0_0cycle,
		battery_profile_t0, sizeof(battery_profile_t0));
	memcpy(battery_profile_t0_500cycle,
		battery_profile_t0, sizeof(battery_profile_t0));
	memcpy(battery_profile_t1_0cycle,
		battery_profile_t1, sizeof(battery_profile_t1));
	memcpy(battery_profile_t1_500cycle,
		battery_profile_t1, sizeof(battery_profile_t1));
	memcpy(battery_profile_t1_5_0cycle,
		battery_profile_t1_5, sizeof(battery_profile_t1_5));
	memcpy(battery_profile_t1_5_500cycle,
		battery_profile_t1_5, sizeof(battery_profile_t1_5));
	memcpy(battery_profile_t2_0cycle,
		battery_profile_t2, sizeof(battery_profile_t2));
	memcpy(battery_profile_t2_500cycle,
		battery_profile_t2, sizeof(battery_profile_t2));
	memcpy(battery_profile_t3_0cycle,
		battery_profile_t3, sizeof(battery_profile_t3));
	memcpy(battery_profile_t3_500cycle,
		battery_profile_t3, sizeof(battery_profile_t3));

	memcpy(r_profile_t0_0cycle, r_profile_t0, sizeof(r_profile_t0));
	memcpy(r_profile_t0_500cycle, r_profile_t0, sizeof(r_profile_t0));
	memcpy(r_profile_t1_0cycle, r_profile_t1, sizeof(r_profile_t1));
	memcpy(r_profile_t1_500cycle, r_profile_t1, sizeof(r_profile_t1));
	memcpy(r_profile_t1_5_0cycle, r_profile_t1_5, sizeof(r_profile_t1_5));
	memcpy(r_profile_t1_5_500cycle,
		r_profile_t1_5, sizeof(r_profile_t1_5));
	memcpy(r_profile_t2_0cycle, r_profile_t2, sizeof(r_profile_t2));
	memcpy(r_profile_t2_500cycle, r_profile_t2, sizeof(r_profile_t2));
	memcpy(r_profile_t3_0cycle, r_profile_t3, sizeof(r_profile_t3));
	memcpy(r_profile_t3_500cycle, r_profile_t3, sizeof(r_profile_t3));

	q_max_t0_aging.Q_0_CYCLE = batt_meter_cust_data.q_max_neg_10;
	q_max_t0_aging.Q_500_CYCLE = batt_meter_cust_data.q_max_neg_10;
	q_max_t1_aging.Q_0_CYCLE = batt_meter_cust_data.q_max_pos_0;
	q_max_t1_aging.Q_500_CYCLE = batt_meter_cust_data.q_max_pos_0;
	q_max_t1_5_aging.Q_0_CYCLE = batt_meter_cust_data.q_max_pos_10;
	q_max_t1_5_aging.Q_500_CYCLE = batt_meter_cust_data.q_max_pos_10;
	q_max_t2_aging.Q_0_CYCLE = batt_meter_cust_data.q_max_pos_25;
	q_max_t2_aging.Q_500_CYCLE = batt_meter_cust_data.q_max_pos_25;
	q_max_t3_aging.Q_0_CYCLE = batt_meter_cust_data.q_max_pos_50;
	q_max_t3_aging.Q_500_CYCLE = batt_meter_cust_data.q_max_pos_50;

	q_max_t0_h_current_aging.Q_0_CYCLE =
		batt_meter_cust_data.q_max_neg_10_h_current;
	q_max_t0_h_current_aging.Q_500_CYCLE =
		batt_meter_cust_data.q_max_neg_10_h_current;
	q_max_t1_h_current_aging.Q_0_CYCLE =
		batt_meter_cust_data.q_max_pos_0_h_current;
	q_max_t1_h_current_aging.Q_500_CYCLE =
		batt_meter_cust_data.q_max_pos_0_h_current;
	q_max_t1_5_h_current_aging.Q_0_CYCLE =
		batt_meter_cust_data.q_max_pos_10_h_current;
	q_max_t1_5_h_current_aging.Q_500_CYCLE =
		batt_meter_cust_data.q_max_pos_10_h_current;
	q_max_t2_h_current_aging.Q_0_CYCLE =
		batt_meter_cust_data.q_max_pos_25_h_current;
	q_max_t2_h_current_aging.Q_500_CYCLE =
		batt_meter_cust_data.q_max_pos_25_h_current;
	q_max_t3_h_current_aging.Q_0_CYCLE =
		batt_meter_cust_data.q_max_pos_50_h_current;
	q_max_t3_h_current_aging.Q_500_CYCLE =
		batt_meter_cust_data.q_max_pos_50_h_current;
}

static void __batt_meter_parse_multi_aging_data(
	const struct device_node *np, int bat_id)
{
	char dtsbuf[100];

	sprintf(dtsbuf, "battery_profile_t0_500cycle_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		&battery_profile_t0_500cycle[0]);

	sprintf(dtsbuf, "battery_profile_t1_500cycle_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		&battery_profile_t1_500cycle[0]);

	sprintf(dtsbuf, "battery_profile_t1_5_500cycle_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		&battery_profile_t1_5_500cycle[0]);

	sprintf(dtsbuf, "battery_profile_t2_500cycle_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		&battery_profile_t2_500cycle[0]);

	sprintf(dtsbuf, "battery_profile_t3_500cycle_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		&battery_profile_t3_500cycle[0]);

	sprintf(dtsbuf, "r_profile_t0_500cycle_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		(BATTERY_PROFILE_STRUCT *)&r_profile_t0_500cycle[0]);

	sprintf(dtsbuf, "r_profile_t1_500cycle_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		(BATTERY_PROFILE_STRUCT *)&r_profile_t1_500cycle[0]);

	sprintf(dtsbuf, "r_profile_t1_5_500cycle_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		(BATTERY_PROFILE_STRUCT *)&r_profile_t1_5_500cycle[0]);

	sprintf(dtsbuf, "r_profile_t2_500cycle_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		(BATTERY_PROFILE_STRUCT *)&r_profile_t2_500cycle[0]);

	sprintf(dtsbuf, "r_profile_t3_500cycle_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		(BATTERY_PROFILE_STRUCT *)&r_profile_t3_500cycle[0]);

	sprintf(dtsbuf, "q_max_neg_10_500cycle_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&q_max_t0_aging.Q_500_CYCLE);

	sprintf(dtsbuf, "q_max_pos_0_500cycle_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&q_max_t1_aging.Q_500_CYCLE);

	sprintf(dtsbuf, "q_max_pos_10_500cycle_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&q_max_t1_5_aging.Q_500_CYCLE);

	sprintf(dtsbuf, "q_max_pos_25_500cycle_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&q_max_t2_aging.Q_500_CYCLE);

	sprintf(dtsbuf, "q_max_pos_50_500cycle_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&q_max_t3_aging.Q_500_CYCLE);

	sprintf(dtsbuf, "q_max_neg_10_h_current_500cycle_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&q_max_t0_h_current_aging.Q_500_CYCLE);

	sprintf(dtsbuf, "q_max_pos_0_h_current_500cycle_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&q_max_t1_h_current_aging.Q_500_CYCLE);

	sprintf(dtsbuf, "q_max_pos_10_h_current_500cycle_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&q_max_t1_5_h_current_aging.Q_500_CYCLE);

	sprintf(dtsbuf, "q_max_pos_25_h_current_500cycle_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&q_max_t2_h_current_aging.Q_500_CYCLE);

	sprintf(dtsbuf, "q_max_pos_50_h_current_500cycle_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&q_max_t3_h_current_aging.Q_500_CYCLE);
}

static void __batt_meter_parse_aging_data(const struct device_node *np)
{
	reset_aging_data();
	if (g_fg_battery_id == 0) {
	__batt_meter_parse_table(np, "battery_profile_t0_500cycle",
		&battery_profile_t0_500cycle[0]);
	__batt_meter_parse_table(np, "battery_profile_t1_500cycle",
		&battery_profile_t1_500cycle[0]);
	__batt_meter_parse_table(np, "battery_profile_t1_5_500cycle",
		&battery_profile_t1_5_500cycle[0]);
	__batt_meter_parse_table(np, "battery_profile_t2_500cycle",
		&battery_profile_t2_500cycle[0]);
	__batt_meter_parse_table(np, "battery_profile_t3_500cycle",
		&battery_profile_t3_500cycle[0]);

	__batt_meter_parse_table(np, "r_profile_t0_500cycle",
		(BATTERY_PROFILE_STRUCT *)&r_profile_t0_500cycle[0]);
	__batt_meter_parse_table(np, "r_profile_t1_500cycle",
		(BATTERY_PROFILE_STRUCT *)&r_profile_t1_500cycle[0]);
	__batt_meter_parse_table(np, "r_profile_t1_5_500cycle",
		(BATTERY_PROFILE_STRUCT *)&r_profile_t1_5_500cycle[0]);
	__batt_meter_parse_table(np, "r_profile_t2_500cycle",
		(BATTERY_PROFILE_STRUCT *)&r_profile_t2_500cycle[0]);
	__batt_meter_parse_table(np, "r_profile_t3_500cycle",
		(BATTERY_PROFILE_STRUCT *)&r_profile_t3_500cycle[0]);

	__batt_meter_parse_node(np, "q_max_neg_10_500cycle",
		&q_max_t0_aging.Q_500_CYCLE);
	__batt_meter_parse_node(np, "q_max_pos_0_500cycle",
		&q_max_t1_aging.Q_500_CYCLE);
	__batt_meter_parse_node(np, "q_max_pos_10_500cycle",
		&q_max_t1_5_aging.Q_500_CYCLE);
	__batt_meter_parse_node(np, "q_max_pos_25_500cycle",
		&q_max_t2_aging.Q_500_CYCLE);
	__batt_meter_parse_node(np, "q_max_pos_50_500cycle",
		&q_max_t3_aging.Q_500_CYCLE);

	__batt_meter_parse_node(np, "q_max_neg_10_h_current_500cycle",
		&q_max_t0_h_current_aging.Q_500_CYCLE);
	__batt_meter_parse_node(np, "q_max_pos_0_h_current_500cycle",
		&q_max_t1_h_current_aging.Q_500_CYCLE);
	__batt_meter_parse_node(np, "q_max_pos_10_h_current_500cycle",
		&q_max_t1_5_h_current_aging.Q_500_CYCLE);
	__batt_meter_parse_node(np, "q_max_pos_25_h_current_500cycle",
		&q_max_t2_h_current_aging.Q_500_CYCLE);
	__batt_meter_parse_node(np, "q_max_pos_50_h_current_500cycle",
		&q_max_t3_h_current_aging.Q_500_CYCLE);
	} else {
		__batt_meter_parse_multi_aging_data(np, g_fg_battery_id);
	}
}
#endif

static void __batt_meter_parse_multi_battery_data(
	struct device_node *np, int bat_id)
{
	int num;
	char dtsbuf[100];

	sprintf(dtsbuf, "battery_vendor_%d", bat_id);
	of_property_read_string(np, dtsbuf, &bat_vendor);

	sprintf(dtsbuf, "battery_profile_t0_num_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf, &num);

	sprintf(dtsbuf, "battery_profile_t0_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		fgauge_get_profile(batt_meter_cust_data.temperature_t0));

	sprintf(dtsbuf, "battery_profile_t1_num_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf, &num);

	sprintf(dtsbuf, "battery_profile_t1_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		fgauge_get_profile(batt_meter_cust_data.temperature_t1));

	sprintf(dtsbuf, "battery_profile_t1_5_num_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf, &num);

	sprintf(dtsbuf, "battery_profile_t1_5_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		fgauge_get_profile(batt_meter_cust_data.temperature_t1_5));

	sprintf(dtsbuf, "battery_profile_t2_num_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf, &num);

	sprintf(dtsbuf, "battery_profile_t2_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		fgauge_get_profile(batt_meter_cust_data.temperature_t2));

	sprintf(dtsbuf, "battery_profile_t3_num_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf, &num);

	sprintf(dtsbuf, "battery_profile_t3_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		fgauge_get_profile(batt_meter_cust_data.temperature_t3));

	sprintf(dtsbuf, "r_profile_t0_num_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,	&num);

	sprintf(dtsbuf, "r_profile_t0_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		(BATTERY_PROFILE_STRUCT *)fgauge_get_profile_r_table(
		batt_meter_cust_data.temperature_t0));

	sprintf(dtsbuf, "r_profile_t1_num_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,	&num);

	sprintf(dtsbuf, "r_profile_t1_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
		(BATTERY_PROFILE_STRUCT *)fgauge_get_profile_r_table(
		batt_meter_cust_data.temperature_t1));

	sprintf(dtsbuf, "r_profile_t1_5_num_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf, &num);

	sprintf(dtsbuf, "r_profile_t1_5_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
			(BATTERY_PROFILE_STRUCT *)fgauge_get_profile_r_table(
			batt_meter_cust_data.temperature_t1_5));

	sprintf(dtsbuf, "r_profile_t2_num_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,	&num);

	sprintf(dtsbuf, "r_profile_t2_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
			(BATTERY_PROFILE_STRUCT *)fgauge_get_profile_r_table(
			batt_meter_cust_data.temperature_t2));

	sprintf(dtsbuf, "r_profile_t3_num_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,	&num);

	sprintf(dtsbuf, "r_profile_t3_%d", bat_id);
	__batt_meter_parse_table(np, dtsbuf,
			(BATTERY_PROFILE_STRUCT *)fgauge_get_profile_r_table(
			batt_meter_cust_data.temperature_t3));

	sprintf(dtsbuf, "q_max_pos_50_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&batt_meter_cust_data.q_max_pos_50);

	sprintf(dtsbuf, "q_max_pos_25_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&batt_meter_cust_data.q_max_pos_25);

	sprintf(dtsbuf, "q_max_pos_10_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&batt_meter_cust_data.q_max_pos_10);

	sprintf(dtsbuf, "q_max_pos_0_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&batt_meter_cust_data.q_max_pos_0);

	sprintf(dtsbuf, "q_max_neg_10_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&batt_meter_cust_data.q_max_neg_10);

	sprintf(dtsbuf, "q_max_pos_50_h_current_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&batt_meter_cust_data.q_max_pos_50_h_current);

	sprintf(dtsbuf, "q_max_pos_25_h_current_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&batt_meter_cust_data.q_max_pos_25_h_current);

	sprintf(dtsbuf, "q_max_pos_10_h_current_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&batt_meter_cust_data.q_max_pos_10_h_current);

	sprintf(dtsbuf, "q_max_pos_0_h_current_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&batt_meter_cust_data.q_max_pos_0_h_current);

	sprintf(dtsbuf, "q_max_neg_10_h_current_%d", bat_id);
	__batt_meter_parse_node(np, dtsbuf,
		&batt_meter_cust_data.q_max_neg_10_h_current);
}

int __batt_meter_init_cust_data_from_dt(void)
{
	struct device_node *np;
	int num;
	unsigned int idx, addr, val;

	/* check customer setting */
	np = of_find_compatible_node(NULL, NULL, "mediatek,bat_meter");
	if (!np) {
		pr_notice("Failed to find device-tree node: bat_meter\n");
		return -ENODEV;
	}

	__batt_meter_parse_node(np, "vbat_channel_number",
		&batt_meter_cust_data.vbat_channel_number);

	__batt_meter_parse_node(np, "isense_channel_number",
		&batt_meter_cust_data.isense_channel_number);

	__batt_meter_parse_node(np, "vbattemp_channel_number",
		&batt_meter_cust_data.vbattemp_channel_number);

	__batt_meter_parse_node(np, "vcharger_channel_number",
		&batt_meter_cust_data.vcharger_channel_number);

	__batt_meter_parse_node(np, "swchr_power_path",
		&batt_meter_cust_data.swchr_power_path);

	__batt_meter_parse_node(np, "rbat_pull_up_r",
		&batt_meter_cust_data.rbat_pull_up_r);

	__batt_meter_parse_node(np, "rbat_pull_down_r",
		&batt_meter_cust_data.rbat_pull_down_r);

	__batt_meter_parse_node(np, "rbat_pull_up_volt",
		&batt_meter_cust_data.rbat_pull_up_volt);

	__batt_meter_parse_node(np, "batt_temperature_table_num", &num);

	idx = 0;
	num = sizeof(Batt_Temperature_Table)/sizeof(BATT_TEMPERATURE);
	while (!of_property_read_u32_index(np,
			"batt_temperature_table", idx, &addr)) {
		idx++;
		if (!of_property_read_u32_index(np,
			"batt_temperature_table", idx, &val)) {
			pr_notice("batt_temperature_table: addr: %d, val: %d\n",
				    addr, val);
		}
		Batt_Temperature_Table[idx / 2].BatteryTemp = addr;
		Batt_Temperature_Table[idx / 2].TemperatureR = val;

		idx++;
		if (idx >= num * 2)
			break;
	}

	if (idx != 0) {
		while (idx < (num * 2)) {
			Batt_Temperature_Table[idx / 2].BatteryTemp = addr;
			Batt_Temperature_Table[idx / 2].TemperatureR = val;

			/* dump parsing data */
			#if 0
			msleep(20);
			pr_notice("batt_temperature_table[%d]: <%d, %d>\n",
				(idx/2)-1,
				Batt_Temperature_Table[idx/2].BatteryTemp,
				Batt_Temperature_Table[idx/2].TemperatureR);
			#endif

			idx = idx + 2;
		}
	} else
		pr_notice("no Batt_Temperature_Table in dts\n");

	__batt_meter_parse_node(np, "temperature_t0",
		&batt_meter_cust_data.temperature_t0);

	__batt_meter_parse_node(np, "temperature_t1",
		&batt_meter_cust_data.temperature_t1);

	__batt_meter_parse_node(np, "temperature_t1_5",
		&batt_meter_cust_data.temperature_t1_5);

	__batt_meter_parse_node(np, "temperature_t2",
		&batt_meter_cust_data.temperature_t2);

	__batt_meter_parse_node(np, "temperature_t3",
		&batt_meter_cust_data.temperature_t3);

	__batt_meter_parse_node(np, "temperature_t",
		&batt_meter_cust_data.temperature_t);

#ifdef CONFIG_MTK_MULTI_BAT_PROFILE_SUPPORT
	batt_meter_cust_data.battery_id_channel_number = BATTERY_ID_CHANNEL_NUM;
	__batt_meter_parse_node(np, "battery_id_channel_number",
		&batt_meter_cust_data.battery_id_channel_number);

	__batt_meter_parse_node(np, "id_volt_max", &id_volt_max);

	__batt_meter_parse_node(np, "id_volt_min", &id_volt_min);

	__batt_meter_parse_node(np, "total_battery_number",
		&total_battery_number);

	/* Parse Battery ID threshold */
	idx = 0;
	num = sizeof(g_battery_id_voltage)/sizeof(int);
	while (!of_property_read_u32_index(np,
			"bat_id_volt_thres", idx, &val)) {
		if ((val == ID_VOLT_END) || (idx >= num - 1))
			break;
		g_battery_id_voltage[idx++] = val;
	}

	fgauge_get_profile_id();
#endif

	if (g_fg_battery_id == 0) {
	of_property_read_string(np, "battery_vendor", &bat_vendor);

	__batt_meter_parse_node(np, "battery_profile_t0_num", &num);

	__batt_meter_parse_table(np, "battery_profile_t0",
		fgauge_get_profile(batt_meter_cust_data.temperature_t0));

	__batt_meter_parse_node(np, "battery_profile_t1_num", &num);

	__batt_meter_parse_table(np, "battery_profile_t1",
		fgauge_get_profile(batt_meter_cust_data.temperature_t1));

	__batt_meter_parse_node(np, "battery_profile_t1_5_num", &num);

	__batt_meter_parse_table(np, "battery_profile_t1_5",
		fgauge_get_profile(batt_meter_cust_data.temperature_t1_5));

	__batt_meter_parse_node(np, "battery_profile_t2_num", &num);

	__batt_meter_parse_table(np, "battery_profile_t2",
		fgauge_get_profile(batt_meter_cust_data.temperature_t2));

	__batt_meter_parse_node(np, "battery_profile_t3_num", &num);

	__batt_meter_parse_table(np, "battery_profile_t3",
		fgauge_get_profile(batt_meter_cust_data.temperature_t3));

	__batt_meter_parse_node(np, "r_profile_t0_num",	&num);

	__batt_meter_parse_table(np, "r_profile_t0",
		(BATTERY_PROFILE_STRUCT *)fgauge_get_profile_r_table(
		batt_meter_cust_data.temperature_t0));

	__batt_meter_parse_node(np, "r_profile_t1_num",	&num);

	__batt_meter_parse_table(np, "r_profile_t1",
		(BATTERY_PROFILE_STRUCT *)fgauge_get_profile_r_table(
		batt_meter_cust_data.temperature_t1));

	__batt_meter_parse_node(np, "r_profile_t1_5_num", &num);

	__batt_meter_parse_table(np, "r_profile_t1_5",
		(BATTERY_PROFILE_STRUCT *)fgauge_get_profile_r_table(
		batt_meter_cust_data.temperature_t1_5));

	__batt_meter_parse_node(np, "r_profile_t2_num",	&num);

	__batt_meter_parse_table(np, "r_profile_t2",
		(BATTERY_PROFILE_STRUCT *)fgauge_get_profile_r_table(
		batt_meter_cust_data.temperature_t2));

	__batt_meter_parse_node(np, "r_profile_t3_num",	&num);

	__batt_meter_parse_table(np, "r_profile_t3",
		(BATTERY_PROFILE_STRUCT *)fgauge_get_profile_r_table(
		batt_meter_cust_data.temperature_t3));

	__batt_meter_parse_node(np, "q_max_pos_50",
		&batt_meter_cust_data.q_max_pos_50);

	__batt_meter_parse_node(np, "q_max_pos_25",
		&batt_meter_cust_data.q_max_pos_25);

	__batt_meter_parse_node(np, "q_max_pos_10",
		&batt_meter_cust_data.q_max_pos_10);

	__batt_meter_parse_node(np, "q_max_pos_0",
		&batt_meter_cust_data.q_max_pos_0);

	__batt_meter_parse_node(np, "q_max_neg_10",
		&batt_meter_cust_data.q_max_neg_10);

	__batt_meter_parse_node(np, "q_max_pos_50_h_current",
		&batt_meter_cust_data.q_max_pos_50_h_current);

	__batt_meter_parse_node(np, "q_max_pos_25_h_current",
		&batt_meter_cust_data.q_max_pos_25_h_current);

	__batt_meter_parse_node(np, "q_max_pos_10_h_current",
		&batt_meter_cust_data.q_max_pos_10_h_current);

	__batt_meter_parse_node(np, "q_max_pos_0_h_current",
		&batt_meter_cust_data.q_max_pos_0_h_current);

	__batt_meter_parse_node(np, "q_max_neg_10_h_current",
		&batt_meter_cust_data.q_max_neg_10_h_current);
	} else
		__batt_meter_parse_multi_battery_data(np, g_fg_battery_id);

	__batt_meter_parse_node(np, "r_bat_sense",
		&batt_meter_cust_data.r_bat_sense);

	__batt_meter_parse_node(np, "r_i_sense",
		&batt_meter_cust_data.r_i_sense);

	__batt_meter_parse_node(np, "r_charger_1",
		&batt_meter_cust_data.r_charger_1);

	__batt_meter_parse_node(np, "r_charger_2",
		&batt_meter_cust_data.r_charger_2);

	__batt_meter_parse_node(np, "fg_meter_resistance",
		&batt_meter_cust_data.fg_meter_resistance);

	__batt_meter_parse_node(np, "oam_d5",
		&batt_meter_cust_data.oam_d5);

	__batt_meter_parse_node(np, "change_tracking_point",
		&batt_meter_cust_data.change_tracking_point);

	__batt_meter_parse_node(np, "cust_tracking_point",
		&batt_meter_cust_data.cust_tracking_point);

	__batt_meter_parse_node(np, "cust_r_sense",
		&batt_meter_cust_data.cust_r_sense);

	__batt_meter_parse_node(np, "cust_hw_cc",
		&batt_meter_cust_data.cust_hw_cc);

	__batt_meter_parse_node(np, "aging_tuning_value",
		&batt_meter_cust_data.aging_tuning_value);

	__batt_meter_parse_node(np, "cust_r_fg_offset",
		&batt_meter_cust_data.cust_r_fg_offset);

	__batt_meter_parse_node(np, "ocv_board_compesate",
		&batt_meter_cust_data.ocv_board_compesate);

	__batt_meter_parse_node(np, "r_fg_board_base",
		&batt_meter_cust_data.r_fg_board_base);

	__batt_meter_parse_node(np, "r_fg_board_slope",
		&batt_meter_cust_data.r_fg_board_slope);

	__batt_meter_parse_node(np, "car_tune_value",
		&batt_meter_cust_data.car_tune_value);

	__batt_meter_parse_node(np, "current_detect_r_fg",
		&batt_meter_cust_data.current_detect_r_fg);

	__batt_meter_parse_node(np, "minerroroffset",
		&batt_meter_cust_data.minerroroffset);

	__batt_meter_parse_node(np, "fg_vbat_average_size",
		&batt_meter_cust_data.fg_vbat_average_size);

	__batt_meter_parse_node(np, "r_fg_value",
		&batt_meter_cust_data.r_fg_value);

	__batt_meter_parse_node(np, "cust_poweron_delta_capacity_tolrance",
		&batt_meter_cust_data.cust_poweron_delta_capacity_tolrance);

	__batt_meter_parse_node(np, "cust_poweron_low_capacity_tolrance",
		&batt_meter_cust_data.cust_poweron_low_capacity_tolrance);

	__batt_meter_parse_node(np, "cust_poweron_max_vbat_tolrance",
		&batt_meter_cust_data.cust_poweron_max_vbat_tolrance);

	__batt_meter_parse_node(np, "cust_poweron_delta_vbat_tolrance",
		&batt_meter_cust_data.cust_poweron_delta_vbat_tolrance);

	__batt_meter_parse_node(np,
	"cust_poweron_delta_hw_sw_ocv_capacity_tolrance",
	&batt_meter_cust_data.cust_poweron_delta_hw_sw_ocv_capacity_tolrance);

	__batt_meter_parse_node(np, "fixed_tbat_25",
		&batt_meter_cust_data.fixed_tbat_25);

	__batt_meter_parse_node(np, "vbat_normal_wakeup",
		&batt_meter_cust_data.vbat_normal_wakeup);

	__batt_meter_parse_node(np, "vbat_low_power_wakeup",
		&batt_meter_cust_data.vbat_low_power_wakeup);

	__batt_meter_parse_node(np, "normal_wakeup_period",
		&batt_meter_cust_data.normal_wakeup_period);

	__batt_meter_parse_node(np, "low_power_wakeup_period",
		&batt_meter_cust_data.low_power_wakeup_period);

	__batt_meter_parse_node(np, "close_poweroff_wakeup_period",
		&batt_meter_cust_data.close_poweroff_wakeup_period);

	__batt_meter_parse_node(np, "step_of_qmax",
		&batt_meter_cust_data.step_of_qmax);

	__batt_meter_parse_node(np, "std_loading_current",
		&batt_meter_cust_data.std_loading_current);

#if defined(CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT)
	__batt_meter_parse_aging_data(np);
#endif

#if defined(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT)
	if (batt_internal_charger_detect()) {
		__batt_meter_parse_node(np, "vbat_internal_charging",
			&batt_meter_cust_data.vbat_channel_number);

		__batt_meter_parse_node(np, "isense_internal_charging",
			&batt_meter_cust_data.isense_channel_number);

		batt_meter_cust_data.swchr_power_path = 0;
	}
#endif

	batt_meter_cust_data.slp_current = DEFAULT_SLP_CURRENT;
	__batt_meter_parse_node(np, "slp_current",
			&batt_meter_cust_data.slp_current);
	__batt_meter_parse_node(np, "slp_current_wifi",
			&batt_meter_cust_data.slp_current_wifi);

	of_node_put(np);

	return 0;
}
#endif

int batt_meter_init_cust_data(void)
{
	static int init_done;

	if (init_done == 1)
		return 0;
	init_done = 1;

#if defined(CONFIG_OF)
	pr_notice("battery meter custom init by DTS\n");
	__batt_meter_init_cust_data_from_dt();
#endif

	_g_bat_sleep_total_time = batt_meter_cust_data.normal_wakeup_period;

	return 0;
}





/* ============================================================ // */
int get_r_fg_value(void)
{
	return (batt_meter_cust_data.r_fg_value +
			batt_meter_cust_data.cust_r_fg_offset);
}

int BattThermistorConverTemp(int Res)
{
	int i, saddles;
	int RES1 = 0, RES2 = 0;
	int TBatt_Value = -200, TMP1 = 0, TMP2 = 0;

	saddles = sizeof(Batt_Temperature_Table) / sizeof(BATT_TEMPERATURE);

	if (Res >= Batt_Temperature_Table[0].TemperatureR)
		TBatt_Value = Batt_Temperature_Table[0].BatteryTemp;
	else if (Res <= Batt_Temperature_Table[saddles-1].TemperatureR)
		TBatt_Value = Batt_Temperature_Table[saddles-1].BatteryTemp;
	else {
		RES1 = Batt_Temperature_Table[0].TemperatureR;
		TMP1 = Batt_Temperature_Table[0].BatteryTemp;
		for (i = 0; i < saddles; i++) {
			if (Res >= Batt_Temperature_Table[i].TemperatureR) {
				RES2 = Batt_Temperature_Table[i].TemperatureR;
				TMP2 = Batt_Temperature_Table[i].BatteryTemp;
				break;
			}

			RES1 = Batt_Temperature_Table[i].TemperatureR;
			TMP1 = Batt_Temperature_Table[i].BatteryTemp;
		}
		TBatt_Value = (((Res - RES2) * TMP1) + ((RES1 - Res) * TMP2)) /
			      (RES1 - RES2);
	}

	return TBatt_Value;
}

signed int fgauge_get_Q_max(signed short temperature)
{
	signed int ret_Q_max;
	signed int low_temperature, high_temperature;
	signed int low_Q_max, high_Q_max;

	if (temperature <= batt_meter_cust_data.temperature_t1) {
		low_temperature = (-10);
		low_Q_max = batt_meter_cust_data.q_max_neg_10;
		high_temperature = batt_meter_cust_data.temperature_t1;
		high_Q_max = batt_meter_cust_data.q_max_pos_0;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= batt_meter_cust_data.temperature_t1_5) {
		low_temperature = batt_meter_cust_data.temperature_t1;
		low_Q_max = batt_meter_cust_data.q_max_pos_0;
		high_temperature = batt_meter_cust_data.temperature_t1_5;
		high_Q_max = batt_meter_cust_data.q_max_pos_10;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= batt_meter_cust_data.temperature_t2) {
		low_temperature = batt_meter_cust_data.temperature_t1_5;
		low_Q_max = batt_meter_cust_data.q_max_pos_10;
		high_temperature = batt_meter_cust_data.temperature_t2;
		high_Q_max = batt_meter_cust_data.q_max_pos_25;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else {
		low_temperature = batt_meter_cust_data.temperature_t2;
		low_Q_max = batt_meter_cust_data.q_max_pos_25;
		high_temperature = batt_meter_cust_data.temperature_t3;
		high_Q_max = batt_meter_cust_data.q_max_pos_50;

		if (temperature > high_temperature)
			temperature = high_temperature;

	}

	ret_Q_max = low_Q_max + (((temperature - low_temperature)
		* (high_Q_max - low_Q_max)) /
		(high_temperature - low_temperature));

	pr_debug("[fgauge_get_Q_max] Q_max = %d\r\n", ret_Q_max);

	return ret_Q_max;
}


signed int fgauge_get_Q_max_high_current(signed short temperature)
{
	signed int ret_Q_max;
	signed int low_temperature, high_temperature;
	signed int low_Q_max, high_Q_max;

	if (temperature <= batt_meter_cust_data.temperature_t1) {
		low_temperature = (-10);
		low_Q_max = batt_meter_cust_data.q_max_neg_10_h_current;
		high_temperature = batt_meter_cust_data.temperature_t1;
		high_Q_max = batt_meter_cust_data.q_max_pos_0_h_current;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= batt_meter_cust_data.temperature_t1_5) {
		low_temperature = batt_meter_cust_data.temperature_t1;
		low_Q_max = batt_meter_cust_data.q_max_pos_0_h_current;
		high_temperature = batt_meter_cust_data.temperature_t1_5;
		high_Q_max = batt_meter_cust_data.q_max_pos_10_h_current;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= batt_meter_cust_data.temperature_t2) {
		low_temperature = batt_meter_cust_data.temperature_t1_5;
		low_Q_max = batt_meter_cust_data.q_max_pos_10_h_current;
		high_temperature = batt_meter_cust_data.temperature_t2;
		high_Q_max = batt_meter_cust_data.q_max_pos_25_h_current;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else {
		low_temperature = batt_meter_cust_data.temperature_t2;
		low_Q_max = batt_meter_cust_data.q_max_pos_25_h_current;
		high_temperature = batt_meter_cust_data.temperature_t3;
		high_Q_max = batt_meter_cust_data.q_max_pos_50_h_current;

		if (temperature > high_temperature)
			temperature = high_temperature;

	}

	ret_Q_max = low_Q_max + (((temperature - low_temperature)
		* (high_Q_max - low_Q_max)) /
		(high_temperature - low_temperature));

	pr_debug("[fgauge_get_Q_max_high_current] Q_max = %d\r\n", ret_Q_max);

	return ret_Q_max;
}

int BattVoltToTemp(int dwVolt)
{
	unsigned long long TRes_temp;
	unsigned long long TRes;
	int sBaTTMP = -100;

	TRes_temp = (batt_meter_cust_data.rbat_pull_up_r * (long long) dwVolt);
	do_div(TRes_temp, (batt_meter_cust_data.rbat_pull_up_volt - dwVolt));

	if (batt_meter_cust_data.rbat_pull_down_r > 1000) {
		TRes = (TRes_temp * batt_meter_cust_data.rbat_pull_down_r);
		do_div(TRes,
			abs(batt_meter_cust_data.rbat_pull_down_r - TRes_temp));
	} else
		TRes = TRes_temp;

	/* convert register to temperature */
	sBaTTMP = BattThermistorConverTemp((int)TRes);

	return sBaTTMP;
}

int force_get_tbat(bool update)
{
#if defined(CONFIG_POWER_EXT) || defined(FIXED_TBAT_25)
	pr_debug("[force_get_tbat] fixed TBAT=25 t\n");
	return 25;
#else
	int bat_temperature_volt = 0;
	int bat_temperature_val = 0;
	static int pre_bat_temperature_val = -1;
	int fg_r_value = 0;
	s32 fg_current_temp = 0;
	bool fg_current_state = false;
	int bat_temperature_volt_temp = 0;
	int ret = 0;

	if (batt_meter_cust_data.fixed_tbat_25) {
		pr_notice("[force_get_tbat] fixed TBAT=25 t\n");
		return 25;
	}

	if ((update == false) && (pre_bat_temperature_val == -1))
		return pre_bat_temperature_val;

	/* Get V_BAT_Temperature */
	bat_temperature_volt = 2;
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP,
		&bat_temperature_volt);

	if (bat_temperature_volt != 0) {
#if defined(SOC_BY_HW_FG)
		fg_r_value = get_r_fg_value();

		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT,
					 &fg_current_temp);
		ret = battery_meter_ctrl(
		BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN,
					   &fg_current_state);
		fg_current_temp = fg_current_temp / 10;

		if (fg_current_state == true) {
			bat_temperature_volt_temp = bat_temperature_volt;
			bat_temperature_volt =
			bat_temperature_volt -
			((fg_current_temp * fg_r_value) / 1000);
		} else {
			bat_temperature_volt_temp = bat_temperature_volt;
			bat_temperature_volt =
			bat_temperature_volt +
			((fg_current_temp * fg_r_value) / 1000);
		}
#endif

		bat_temperature_val = BattVoltToTemp(bat_temperature_volt);
	}

	if ((update == true) &&
		(abs(pre_bat_temperature_val - bat_temperature_val) > 10)) {
		pr_notice("[%s] %d,%d,%d,%d,%d,%d,%d\n", __func__,
			bat_temperature_volt_temp, bat_temperature_volt,
			fg_current_state, fg_current_temp, fg_r_value,
			bat_temperature_val, pre_bat_temperature_val);

		/* get ntc voltage again */
		bat_temperature_volt = 2;
		ret = battery_meter_ctrl(
			BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP,
			&bat_temperature_volt);

		/* dump auxadc value */
		pr_notice("[%s] Batntc(%d),Batsns(%d), Isense(%d)\n",
		__func__, bat_temperature_volt,
		battery_meter_get_battery_voltage(true),
		battery_meter_get_VSense());
	}

	pr_debug("[force_get_tbat] %d,%d,%d,%d,%d,%d\n",
		bat_temperature_volt_temp, bat_temperature_volt,
		fg_current_state, fg_current_temp, fg_r_value,
		bat_temperature_val);

	pre_bat_temperature_val = bat_temperature_val;

	return bat_temperature_val;
#endif
}
EXPORT_SYMBOL(force_get_tbat);

int fgauge_get_saddles(void)
{
	return sizeof(battery_profile_t2) / sizeof(BATTERY_PROFILE_STRUCT);
}

int fgauge_get_saddles_r_table(void)
{
	return sizeof(r_profile_t2) / sizeof(R_PROFILE_STRUCT);
}

struct battery_profile_struct *fgauge_get_profile(unsigned int temperature)
{
	if (temperature == batt_meter_cust_data.temperature_t0)
		return &battery_profile_t0[0];

	if (temperature == batt_meter_cust_data.temperature_t1)
		return &battery_profile_t1[0];

	if (temperature == batt_meter_cust_data.temperature_t1_5)
		return &battery_profile_t1_5[0];

	if (temperature == batt_meter_cust_data.temperature_t2)
		return &battery_profile_t2[0];

	if (temperature == batt_meter_cust_data.temperature_t3)
		return &battery_profile_t3[0];

	if (temperature == batt_meter_cust_data.temperature_t)
		return &battery_profile_temperature[0];


	return NULL;
}

struct r_profile_struct *fgauge_get_profile_r_table(unsigned int temperature)
{
	if (temperature == batt_meter_cust_data.temperature_t0)
		return &r_profile_t0[0];

	if (temperature == batt_meter_cust_data.temperature_t1)
		return &r_profile_t1[0];

	if (temperature == batt_meter_cust_data.temperature_t1_5)
		return &r_profile_t1_5[0];

	if (temperature == batt_meter_cust_data.temperature_t2)
		return &r_profile_t2[0];

	if (temperature == batt_meter_cust_data.temperature_t3)
		return &r_profile_t3[0];

	if (temperature == batt_meter_cust_data.temperature_t)
		return &r_profile_temperature[0];

	return NULL;
}

signed int fgauge_read_capacity_by_v(signed int voltage)
{
	int i, saddles;
	struct battery_profile_struct *profile_p;
	s32 ret_percent = 0;

	profile_p = fgauge_get_profile(batt_meter_cust_data.temperature_t);
	if (profile_p == NULL) {
		pr_err("[FGADC] fgauge get ZCV profile : fail !\r\n");
		return 100;
	}

	saddles = fgauge_get_saddles();

	if (voltage > (profile_p + 0)->voltage)
		return 100;	/* battery capacity, not dod */

	if (voltage < (profile_p + saddles - 1)->voltage)
		return 0; /* battery capacity, not dod */

	for (i = 0; i < saddles - 1; i++) {
		if ((voltage <= (profile_p + i)->voltage) &&
		    (voltage >= (profile_p + i + 1)->voltage)) {
			ret_percent = (profile_p + i)->percentage +
				      (((((profile_p + i)->voltage) - voltage) *
					(((profile_p + i + 1)->percentage) -
					 ((profile_p + i)->percentage))) /
				       (((profile_p + i)->voltage) -
					((profile_p + i + 1)->voltage)));

			break;
		}
	}
	ret_percent = 100 - ret_percent;

	return ret_percent;
}

signed int fgauge_read_v_by_capacity(int bat_capacity)
{
	int i, saddles;
	struct battery_profile_struct *profile_p;
	s32 ret_volt = 0;

	profile_p = fgauge_get_profile(batt_meter_cust_data.temperature_t);
	if (profile_p == NULL) {
		pr_debug("[fgauge_read_v_by_capacity] fgauge get ZCV profile : fail !\r\n");
		return 3700;
	}

	saddles = fgauge_get_saddles();

	if (bat_capacity < (profile_p + 0)->percentage)
		return 3700;

	if (bat_capacity > (profile_p + saddles - 1)->percentage)
		return 3700;


	for (i = 0; i < saddles - 1; i++) {
		if ((bat_capacity >= (profile_p + i)->percentage) &&
		    (bat_capacity <= (profile_p + i + 1)->percentage)) {
			ret_volt = (profile_p + i)->voltage -
				   (((bat_capacity -
				      ((profile_p + i)->percentage)) *
				     (((profile_p + i)->voltage) -
				      ((profile_p + i + 1)->voltage))) /
				    (((profile_p + i + 1)->percentage) -
				     ((profile_p + i)->percentage)));

			break;
		}
	}

	return ret_volt;
}

signed int fgauge_read_d_by_v(signed int volt_bat)
{
	int i, saddles;
	struct battery_profile_struct *profile_p;
	s32 ret_d = 0;

	profile_p = fgauge_get_profile(batt_meter_cust_data.temperature_t);
	if (profile_p == NULL) {
		pr_err("[FGADC] fgauge get ZCV profile : fail !\r\n");
		return 100;
	}

	saddles = fgauge_get_saddles();

	if (volt_bat > (profile_p + 0)->voltage)
		return 0;

	if (volt_bat < (profile_p + saddles - 1)->voltage)
		return 100;

	for (i = 0; i < saddles - 1; i++) {
		if ((volt_bat <= (profile_p + i)->voltage) &&
		    (volt_bat >= (profile_p + i + 1)->voltage)) {
			ret_d = (profile_p + i)->percentage +
				(((((profile_p + i)->voltage) - volt_bat) *
				  (((profile_p + i + 1)->percentage) -
				   ((profile_p + i)->percentage))) /
				 (((profile_p + i)->voltage) -
				  ((profile_p + i + 1)->voltage)));

			break;
		}
	}

	return ret_d;
}

signed int fgauge_read_v_by_d(int d_val)
{
	int i, saddles;
	struct battery_profile_struct *profile_p;
	s32 ret_volt = 0;

	profile_p = fgauge_get_profile(batt_meter_cust_data.temperature_t);
	if (profile_p == NULL) {
		pr_debug("[fgauge_read_v_by_capacity] fgauge get ZCV profile : fail !\r\n");
		return 3700;
	}

	saddles = fgauge_get_saddles();

	if (d_val < (profile_p + 0)->percentage)
		return 3700;

	if (d_val > (profile_p + saddles - 1)->percentage)
		return 3700;

	for (i = 0; i < saddles - 1; i++) {
		if ((d_val >= (profile_p + i)->percentage) &&
		    (d_val <= (profile_p + i + 1)->percentage)) {
			ret_volt = (profile_p + i)->voltage -
				   (((d_val - ((profile_p + i)->percentage)) *
				     (((profile_p + i)->voltage) -
				      ((profile_p + i + 1)->voltage))) /
				    (((profile_p + i + 1)->percentage) -
				     ((profile_p + i)->percentage)));

			break;
		}
	}

	return ret_volt;
}

signed int fgauge_read_r_bat_by_v(signed int voltage)
{
	int i, saddles;
	struct r_profile_struct *profile_p;
	s32 ret_r = 0;

	profile_p = fgauge_get_profile_r_table(
		batt_meter_cust_data.temperature_t);
	if (profile_p == NULL) {
		pr_debug("[FGADC] fgauge get R-Table profile : fail !\r\n");
		return 0;
	}

	saddles = fgauge_get_saddles_r_table();

	if (voltage > (profile_p + 0)->voltage)
		return (profile_p + 0)->resistance;

	if (voltage < (profile_p + saddles - 1)->voltage)
		return (profile_p + saddles - 1)->resistance;

	for (i = 0; i < saddles - 1; i++) {
		if ((voltage <= (profile_p + i)->voltage) &&
		    (voltage >= (profile_p + i + 1)->voltage)) {
			ret_r = (profile_p + i)->resistance +
				(((((profile_p + i)->voltage) - voltage) *
				  (((profile_p + i + 1)->resistance) -
				   ((profile_p + i)->resistance))) /
				 (((profile_p + i)->voltage) -
				  ((profile_p + i + 1)->voltage)));
			break;
		}
	}

	return ret_r;
}

void fgauge_construct_battery_profile_init(void)
{
	struct battery_profile_struct *temp_profile_p;
	struct battery_profile_struct *profile_p[PROFILE_SIZE];
	int i, j, saddles, profile_index;
	signed int low_p, high_p, now_p, low_vol, high_vol;

	profile_p[0] = fgauge_get_profile(batt_meter_cust_data.temperature_t0);
	profile_p[1] = fgauge_get_profile(batt_meter_cust_data.temperature_t1);
	profile_p[2] = fgauge_get_profile(
		batt_meter_cust_data.temperature_t1_5);
	profile_p[3] = fgauge_get_profile(batt_meter_cust_data.temperature_t2);
	profile_p[4] = fgauge_get_profile(batt_meter_cust_data.temperature_t3);
	saddles = fgauge_get_saddles();
	temp_profile_p = kmalloc(51 * sizeof(*temp_profile_p), GFP_KERNEL);
	if (!temp_profile_p)
		return;
	memset(temp_profile_p, 0, 51 * sizeof(*temp_profile_p));
	for (i = 0; i < PROFILE_SIZE; i++) {
		profile_index = 0;
		for (j = 0; j * 2 <= 100; j++) {
			while (profile_index < saddles && profile_index >= 0) {
				if (((profile_p[i] + profile_index)
					->percentage) < j * 2) {
					profile_index++;
					continue;
				} else if (((profile_p[i] + profile_index)
					->percentage) == j * 2) {
					(temp_profile_p + j)->voltage =
					    (profile_p[i] + profile_index)
						->voltage;
					(temp_profile_p + j)->percentage =
					    (profile_p[i] + profile_index)
						->percentage;
					break;
				}
				low_p =
				(profile_p[i] + profile_index - 1)->percentage;
				high_p =
				(profile_p[i] + profile_index)->percentage;
				now_p = j * 2;
				low_vol =
				(profile_p[i] + profile_index)->voltage;
				high_vol =
				(profile_p[i] + profile_index - 1)->voltage;
				(temp_profile_p + j)->voltage =
					(low_vol * 1000 + ((high_vol - low_vol)
					* 1000 * (now_p - low_p)
					/ (high_p - low_p))) / 1000;

				(temp_profile_p + j)->percentage = j * 2;
				break;
			}
			pr_debug("new battery_profile[%d,%d] <%d,%d>\n", i, j,
				(temp_profile_p + j)->percentage,
				(temp_profile_p + j)->voltage);
		}

		for (j = 0; j * 2 <= 100; j++) {
			(profile_p[i] + j)->voltage =
				(temp_profile_p + j)->voltage;
			(profile_p[i] + j)->percentage =
				(temp_profile_p + j)->percentage;
		}
	}
	kfree(temp_profile_p);
}

void fgauge_construct_battery_profile(signed int temperature,
	struct battery_profile_struct *temp_profile_p)
{
	struct battery_profile_struct *low_profile_p, *high_profile_p;
	signed int low_temperature, high_temperature;
	int i, saddles;
	signed int temp_v_1, temp_v_2;

	if (temperature <= batt_meter_cust_data.temperature_t1) {
		low_profile_p = fgauge_get_profile(
			batt_meter_cust_data.temperature_t0);
		high_profile_p = fgauge_get_profile(
			batt_meter_cust_data.temperature_t1);
		low_temperature = (-10);
		high_temperature = batt_meter_cust_data.temperature_t1;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= batt_meter_cust_data.temperature_t1_5) {
		low_profile_p = fgauge_get_profile(
			batt_meter_cust_data.temperature_t1);
		high_profile_p = fgauge_get_profile(
			batt_meter_cust_data.temperature_t1_5);
		low_temperature = batt_meter_cust_data.temperature_t1;
		high_temperature = batt_meter_cust_data.temperature_t1_5;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= batt_meter_cust_data.temperature_t2) {
		low_profile_p = fgauge_get_profile(
			batt_meter_cust_data.temperature_t1_5);
		high_profile_p = fgauge_get_profile(
			batt_meter_cust_data.temperature_t2);
		low_temperature = batt_meter_cust_data.temperature_t1_5;
		high_temperature = batt_meter_cust_data.temperature_t2;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else {
		low_profile_p = fgauge_get_profile(
			batt_meter_cust_data.temperature_t2);
		high_profile_p = fgauge_get_profile(
			batt_meter_cust_data.temperature_t3);
		low_temperature = batt_meter_cust_data.temperature_t2;
		high_temperature = batt_meter_cust_data.temperature_t3;

		if (temperature > high_temperature)
			temperature = high_temperature;
	}

	saddles = fgauge_get_saddles();

	for (i = 0; i < saddles; i++) {
		if (((high_profile_p + i)->voltage) >
		    ((low_profile_p + i)->voltage)) {
			temp_v_1 = (high_profile_p + i)->voltage;
			temp_v_2 = (low_profile_p + i)->voltage;

			(temp_profile_p + i)->voltage =
			temp_v_2 +
			(((temperature - low_temperature) *
			(temp_v_1 - temp_v_2)) /
			(high_temperature - low_temperature));
		} else {
			temp_v_1 = (low_profile_p + i)->voltage;
			temp_v_2 = (high_profile_p + i)->voltage;

			(temp_profile_p + i)->voltage =
			temp_v_2 +
			(((high_temperature - temperature) *
			(temp_v_1 - temp_v_2)) /
			(high_temperature - low_temperature));
		}

		(temp_profile_p + i)->percentage =
			(high_profile_p + i)->percentage;
	}
}

void fgauge_construct_r_table_profile(
	signed int temperature, struct r_profile_struct *temp_profile_p)
{
	struct r_profile_struct *low_profile_p, *high_profile_p;
	signed int low_temperature, high_temperature;
	int i, saddles;
	signed int temp_v_1, temp_v_2;
	signed int temp_r_1, temp_r_2;

	if (temperature <= batt_meter_cust_data.temperature_t1) {
		low_profile_p = fgauge_get_profile_r_table(
			batt_meter_cust_data.temperature_t0);
		high_profile_p = fgauge_get_profile_r_table(
			batt_meter_cust_data.temperature_t1);
		low_temperature = (-10);
		high_temperature = batt_meter_cust_data.temperature_t1;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= batt_meter_cust_data.temperature_t1_5) {
		low_profile_p = fgauge_get_profile_r_table(
			batt_meter_cust_data.temperature_t1);
		high_profile_p = fgauge_get_profile_r_table(
			batt_meter_cust_data.temperature_t1_5);
		low_temperature = batt_meter_cust_data.temperature_t1;
		high_temperature = batt_meter_cust_data.temperature_t1_5;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= batt_meter_cust_data.temperature_t2) {
		low_profile_p = fgauge_get_profile_r_table(
			batt_meter_cust_data.temperature_t1_5);
		high_profile_p = fgauge_get_profile_r_table(
			batt_meter_cust_data.temperature_t2);
		low_temperature = batt_meter_cust_data.temperature_t1_5;
		high_temperature = batt_meter_cust_data.temperature_t2;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else {
		low_profile_p = fgauge_get_profile_r_table(
			batt_meter_cust_data.temperature_t2);
		high_profile_p = fgauge_get_profile_r_table(
			batt_meter_cust_data.temperature_t3);
		low_temperature = batt_meter_cust_data.temperature_t2;
		high_temperature = batt_meter_cust_data.temperature_t3;

		if (temperature > high_temperature)
			temperature = high_temperature;

	}

	saddles = fgauge_get_saddles_r_table();

	/* Interpolation for V_BAT */
	for (i = 0; i < saddles; i++) {
		if (((high_profile_p + i)->voltage) >
		    ((low_profile_p + i)->voltage)) {
			temp_v_1 = (high_profile_p + i)->voltage;
			temp_v_2 = (low_profile_p + i)->voltage;

			(temp_profile_p + i)->voltage =
				temp_v_2 +
				(((temperature - low_temperature) *
				  (temp_v_1 - temp_v_2)) /
				 (high_temperature - low_temperature));
		} else {
			temp_v_1 = (low_profile_p + i)->voltage;
			temp_v_2 = (high_profile_p + i)->voltage;

			(temp_profile_p + i)->voltage =
				temp_v_2 +
				(((high_temperature - temperature) *
				  (temp_v_1 - temp_v_2)) /
				 (high_temperature - low_temperature));
		}
	}

	/* Interpolation for R_BAT */
	for (i = 0; i < saddles; i++) {
		if (((high_profile_p + i)->resistance) >
		    ((low_profile_p + i)->resistance)) {
			temp_r_1 = (high_profile_p + i)->resistance;
			temp_r_2 = (low_profile_p + i)->resistance;

			(temp_profile_p + i)->resistance =
				temp_r_2 +
				(((temperature - low_temperature) *
				  (temp_r_1 - temp_r_2)) /
				 (high_temperature - low_temperature));
		} else {
			temp_r_1 = (low_profile_p + i)->resistance;
			temp_r_2 = (high_profile_p + i)->resistance;

			(temp_profile_p + i)->resistance =
				temp_r_2 +
				(((high_temperature - temperature) *
				  (temp_r_1 - temp_r_2)) /
				 (high_temperature - low_temperature));
		}
	}

	/* Dumpt new r-table profile */
#if defined(BATTERY_DEBUG)
	for (i = 0; i < saddles; i++) {
		pr_debug("<Rbat,VBAT> at %d = <%d,%d>\r\n",
			 temperature, (temp_profile_p + i)->resistance,
			 (temp_profile_p + i)->voltage);
	}
#endif
}

#if defined(CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT)
#if defined(BATTERY_AGING_DEBUG)
static void dump_battery_cycle_table(void)
{
	int i, saddles;

	saddles = fgauge_get_saddles();
	pr_debug("battery_profile_t0_500cycle\n");
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n", battery_profile_t0_500cycle[i].percentage,
		battery_profile_t0_500cycle[i].voltage);
	pr_debug("battery_profile_t1_500cycle\n");
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n", battery_profile_t1_500cycle[i].percentage,
		battery_profile_t1_500cycle[i].voltage);
	pr_debug("battery_profile_t1_5_500cycle\n");
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n",
		battery_profile_t1_5_500cycle[i].percentage,
		battery_profile_t1_5_500cycle[i].voltage);
	pr_debug("battery_profile_t2_500cycle\n");
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n", battery_profile_t2_500cycle[i].percentage,
		battery_profile_t2_500cycle[i].voltage);
	pr_debug("battery_profile_t3_500cycle\n");
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n", battery_profile_t3_500cycle[i].percentage,
		battery_profile_t3_500cycle[i].voltage);

	pr_debug("r_profile_t0_500cycle\n");
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n", r_profile_t0_500cycle[i].resistance,
		r_profile_t0_500cycle[i].voltage);
	pr_debug("r_profile_t1_500cycle\n");
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n", r_profile_t1_500cycle[i].resistance,
		r_profile_t1_500cycle[i].voltage);
	pr_debug("r_profile_t1_5_500cycle\n");
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n", r_profile_t1_5_500cycle[i].resistance,
		r_profile_t1_5_500cycle[i].voltage);
	pr_debug("r_profile_t2_500cycle\n");
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n", r_profile_t2_500cycle[i].resistance,
		r_profile_t2_500cycle[i].voltage);
	pr_debug("r_profile_t3_500cycle\n");
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n", r_profile_t3_500cycle[i].resistance,
		r_profile_t3_500cycle[i].voltage);

	pr_debug("q_max_t0_aging(%d,%d)\n",
		q_max_t0_aging.Q_500_CYCLE,
		q_max_t0_aging.Q_0_CYCLE);
	pr_debug("q_max_t1_aging(%d,%d)\n",
		q_max_t1_aging.Q_500_CYCLE,
		q_max_t1_aging.Q_0_CYCLE);
	pr_debug("q_max_t1_5_aging(%d,%d)\n",
		q_max_t1_5_aging.Q_500_CYCLE,
		q_max_t1_5_aging.Q_0_CYCLE);
	pr_debug("q_max_t2_aging(%d,%d)\n",
		q_max_t2_aging.Q_500_CYCLE,
		q_max_t2_aging.Q_0_CYCLE);
	pr_debug("q_max_t3_aging(%d,%d)\n",
		q_max_t3_aging.Q_500_CYCLE,
		q_max_t3_aging.Q_0_CYCLE);

	pr_debug("q_max_t0_h_current_aging(%d,%d)\n",
		q_max_t0_h_current_aging.Q_500_CYCLE,
		q_max_t0_h_current_aging.Q_0_CYCLE);
	pr_debug("q_max_t1_h_current_aging(%d,%d)\n",
		q_max_t1_h_current_aging.Q_500_CYCLE,
		q_max_t1_h_current_aging.Q_0_CYCLE);
	pr_debug("q_max_t1_5_h_current_aging(%d,%d)\n",
		q_max_t1_5_h_current_aging.Q_500_CYCLE,
		q_max_t1_5_h_current_aging.Q_0_CYCLE);
	pr_debug("q_max_t2_h_current_aging(%d,%d)\n",
		q_max_t2_h_current_aging.Q_500_CYCLE,
		q_max_t2_h_current_aging.Q_0_CYCLE);
	pr_debug("q_max_t3_h_current_aging(%d,%d)\n",
		q_max_t3_h_current_aging.Q_500_CYCLE,
		q_max_t3_h_current_aging.Q_0_CYCLE);
}
#endif

static void dump_current_battery_profile(void)
{
	int i, saddles;

	saddles = fgauge_get_saddles();
	pr_debug("battery_profile_t0, cycle(%d)\n", gFG_battery_cycle);
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n", battery_profile_t0[i].percentage,
		battery_profile_t0[i].voltage);
	pr_debug("battery_profile_t1, cycle(%d)\n", gFG_battery_cycle);
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n", battery_profile_t1[i].percentage,
		battery_profile_t1[i].voltage);
	pr_debug("battery_profile_t1_5, cycle(%d)\n", gFG_battery_cycle);
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n", battery_profile_t1_5[i].percentage,
		battery_profile_t1_5[i].voltage);
	pr_debug("battery_profile_t2, cycle(%d)\n", gFG_battery_cycle);
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n", battery_profile_t2[i].percentage,
		battery_profile_t2[i].voltage);
	pr_debug("battery_profile_t3, cycle(%d)\n", gFG_battery_cycle);
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n", battery_profile_t3[i].percentage,
		battery_profile_t3[i].voltage);

	pr_debug("r_profile_t0, cycle(%d)\n", gFG_battery_cycle);
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n",
		r_profile_t0[i].resistance, r_profile_t0[i].voltage);
	pr_debug("r_profile_t1, cycle(%d)\n", gFG_battery_cycle);
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n",
		r_profile_t1[i].resistance, r_profile_t1[i].voltage);
	pr_debug("r_profile_t1_5, cycle(%d)\n", gFG_battery_cycle);
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n",
		r_profile_t1_5[i].resistance, r_profile_t1_5[i].voltage);
	pr_debug("r_profile_t2, cycle(%d)\n", gFG_battery_cycle);
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n",
		r_profile_t2[i].resistance, r_profile_t2[i].voltage);
	pr_debug("r_profile_t3, cycle(%d)\n", gFG_battery_cycle);
	for (i = 0; i < saddles; i++)
		pr_debug("(%d,%d)\n",
		r_profile_t3[i].resistance, r_profile_t3[i].voltage);

	pr_debug("q_max_neg_10(%d,%d)\n", batt_meter_cust_data.q_max_neg_10,
		batt_meter_cust_data.q_max_neg_10_h_current);
	pr_debug("q_max_pos_0(%d,%d)\n", batt_meter_cust_data.q_max_pos_0,
		batt_meter_cust_data.q_max_pos_0_h_current);
	pr_debug("q_max_pos_10(%d,%d)\n", batt_meter_cust_data.q_max_pos_10,
		batt_meter_cust_data.q_max_pos_10_h_current);
	pr_debug("q_max_pos_25(%d,%d)\n", batt_meter_cust_data.q_max_pos_25,
		batt_meter_cust_data.q_max_pos_25_h_current);
	pr_debug("q_max_pos_50(%d,%d)\n", batt_meter_cust_data.q_max_pos_50,
		batt_meter_cust_data.q_max_pos_50_h_current);
}

static int fgauge_aging_value_interpolation(int high_p, int low_p)
{
	int temp_val;

	temp_val = high_p -
		((gFG_battery_cycle * (high_p - low_p)) / MAX_BATTERY_CYCLE);

	return temp_val;
}

static void fgauge_aging_table_interpolation(void *high_p,
	void *temp_p, void *low_p)
{
	struct r_profile_struct *high_profile_p, *low_profile_p;
	struct r_profile_struct *temp_profile_p;
	int i, saddles;
	signed int temp_v_1, temp_v_2;
	signed int temp_r_1, temp_r_2;

	high_profile_p = (struct r_profile_struct *) high_p;
	temp_profile_p = (struct r_profile_struct *) temp_p;
	low_profile_p = (struct r_profile_struct *) low_p;

	saddles = fgauge_get_saddles_r_table();

	/* Interpolation for V_BAT */
	for (i = 0; i < saddles; i++) {
		if (((high_profile_p + i)->voltage) >
			((low_profile_p + i)->voltage)) {
			temp_v_1 = (high_profile_p + i)->voltage;
			temp_v_2 = (low_profile_p + i)->voltage;

			(temp_profile_p + i)->voltage = temp_v_2 +
				(((gFG_battery_cycle) * (temp_v_1 - temp_v_2)
				 ) / MAX_BATTERY_CYCLE);
		} else {
			temp_v_1 = (low_profile_p + i)->voltage;
			temp_v_2 = (high_profile_p + i)->voltage;

			(temp_profile_p + i)->voltage = temp_v_2 +
			(((MAX_BATTERY_CYCLE - gFG_battery_cycle) *
			(temp_v_1 - temp_v_2)) / MAX_BATTERY_CYCLE);
		}
	}

	/* Interpolation for R_BAT */
	for (i = 0; i < saddles; i++) {
		if (((high_profile_p + i)->resistance) >
			((low_profile_p + i)->resistance)) {
			temp_r_1 = (high_profile_p + i)->resistance;
			temp_r_2 = (low_profile_p + i)->resistance;

			(temp_profile_p + i)->resistance = temp_r_2 +
				(((gFG_battery_cycle) * (temp_r_1 - temp_r_2)
				 ) / (MAX_BATTERY_CYCLE)
				);
		} else {
			temp_r_1 = (low_profile_p + i)->resistance;
			temp_r_2 = (high_profile_p + i)->resistance;

			(temp_profile_p + i)->resistance = temp_r_2 +
				(((MAX_BATTERY_CYCLE - gFG_battery_cycle)
				* (temp_r_1 - temp_r_2)) / (MAX_BATTERY_CYCLE)
				);
		}
	}
}

static void fgauge_construct_aging_para(void)
{
	fgauge_aging_table_interpolation(&battery_profile_t0_500cycle[0],
		&battery_profile_t0[0], &battery_profile_t0_0cycle[0]);

	fgauge_aging_table_interpolation(&battery_profile_t1_500cycle[0],
		&battery_profile_t1[0], &battery_profile_t1_0cycle[0]);

	fgauge_aging_table_interpolation(&battery_profile_t1_5_500cycle[0],
		&battery_profile_t1_5[0], &battery_profile_t1_5_0cycle[0]);

	fgauge_aging_table_interpolation(&battery_profile_t2_500cycle[0],
		&battery_profile_t2[0], &battery_profile_t2_0cycle[0]);

	fgauge_aging_table_interpolation(&battery_profile_t3_500cycle[0],
		&battery_profile_t3[0], &battery_profile_t3_0cycle[0]);

	fgauge_aging_table_interpolation(&r_profile_t0_500cycle[0],
		&r_profile_t0[0], &r_profile_t0_0cycle[0]);

	fgauge_aging_table_interpolation(&r_profile_t1_500cycle[0],
		&r_profile_t1[0], &r_profile_t1_0cycle[0]);

	fgauge_aging_table_interpolation(&r_profile_t1_5_500cycle[0],
		&r_profile_t1_5[0], &r_profile_t1_5_0cycle[0]);

	fgauge_aging_table_interpolation(&r_profile_t2_500cycle[0],
		&r_profile_t2[0], &r_profile_t2_0cycle[0]);

	fgauge_aging_table_interpolation(&r_profile_t3_500cycle[0],
		&r_profile_t3[0], &r_profile_t3_0cycle[0]);

	batt_meter_cust_data.q_max_neg_10 =
		fgauge_aging_value_interpolation(
		q_max_t0_aging.Q_0_CYCLE, q_max_t0_aging.Q_500_CYCLE);
	batt_meter_cust_data.q_max_pos_0 =
		fgauge_aging_value_interpolation(
		q_max_t1_aging.Q_0_CYCLE, q_max_t1_aging.Q_500_CYCLE);
	batt_meter_cust_data.q_max_pos_10 =
		fgauge_aging_value_interpolation(
		q_max_t1_5_aging.Q_0_CYCLE, q_max_t1_5_aging.Q_500_CYCLE);
	batt_meter_cust_data.q_max_pos_25 =
		fgauge_aging_value_interpolation(
		q_max_t2_aging.Q_0_CYCLE, q_max_t2_aging.Q_500_CYCLE);
	batt_meter_cust_data.q_max_pos_50 =
		fgauge_aging_value_interpolation(
		q_max_t3_aging.Q_0_CYCLE, q_max_t3_aging.Q_500_CYCLE);

	batt_meter_cust_data.q_max_neg_10_h_current =
		fgauge_aging_value_interpolation(
		q_max_t0_h_current_aging.Q_0_CYCLE,
		q_max_t0_h_current_aging.Q_500_CYCLE);
	batt_meter_cust_data.q_max_pos_0_h_current =
		fgauge_aging_value_interpolation(
		q_max_t1_h_current_aging.Q_0_CYCLE,
		q_max_t1_h_current_aging.Q_500_CYCLE);
	batt_meter_cust_data.q_max_pos_10_h_current =
		fgauge_aging_value_interpolation(
		q_max_t1_5_h_current_aging.Q_0_CYCLE,
		q_max_t1_5_h_current_aging.Q_500_CYCLE);
	batt_meter_cust_data.q_max_pos_25_h_current =
		fgauge_aging_value_interpolation(
		q_max_t2_h_current_aging.Q_0_CYCLE,
		q_max_t2_h_current_aging.Q_500_CYCLE);
	batt_meter_cust_data.q_max_pos_50_h_current =
		fgauge_aging_value_interpolation(
		q_max_t3_h_current_aging.Q_0_CYCLE,
		q_max_t3_h_current_aging.Q_500_CYCLE);
}

static void update_aging_profile(void)
{
	mutex_lock(&qmax_mutex);
	fgauge_construct_aging_para();
	mutex_unlock(&qmax_mutex);
}

static signed int update_battery_cycle(signed int aging_factor)
{
	int old_battery_cycle;

	if (gFG_aging_factor == aging_factor) {
		pr_debug("[%s]the same aging factor(%d)\n",
			__func__, aging_factor);
	} else if (aging_factor <= 100 && aging_factor > 0) {
		gFG_aging_factor = aging_factor;
		old_battery_cycle = gFG_battery_cycle;
		gFG_battery_cycle = aging_factor * MAX_BATTERY_CYCLE / 100;
		update_aging_profile();
		pr_debug("[%s](%d), new(%d), old(%d)\n", __func__,
			gFG_aging_factor, gFG_battery_cycle, old_battery_cycle);
	}

	return gFG_battery_cycle;
}

static signed int update_battery_aging_factor(signed int cycle)
{
	int old_aging_factor;

	if (gFG_battery_cycle == cycle) {
		pr_debug("[%s]the same battery cycle(%d)\n",
			__func__, gFG_battery_cycle);
	} else if (cycle <= MAX_BATTERY_CYCLE && cycle > 0) {
		gFG_battery_cycle = cycle;
		old_aging_factor = gFG_aging_factor;
		gFG_aging_factor = 100 * cycle / MAX_BATTERY_CYCLE;
		update_aging_profile();
		pr_debug("[%s](%d), new(%d), old(%d)\n", __func__,
			gFG_battery_cycle, gFG_aging_factor, old_aging_factor);
	}

	return gFG_aging_factor;
}

static void fgauge_aging_algorithm(void)
{
	static int aging_condition = AGING_RESET;
	int current_soc;
	bool hw_charging_done;

	if (batt_meter_cust_data.oam_d5)
		current_soc = 100 - oam_d_5;
	else
		current_soc = 100 - oam_d_2;

	hw_charging_done = bat_is_charging_full();

	if ((aging_condition == AGING_RESET) &&
		(current_soc <= 30))
		aging_condition = AGING_STATE_1;
	else if (hw_charging_done &&
		(aging_condition == AGING_STATE_1)) {
		aging_condition = AGING_RESET;
		if (gFG_battery_cycle < MAX_BATTERY_CYCLE) {
			gFG_battery_cycle += 1;
			gFG_aging_factor = (100 * gFG_battery_cycle)
				/ MAX_BATTERY_CYCLE;
			update_aging_profile();
		}
	}
}
#endif

void update_qmax_by_temp(void)
{
	mutex_lock(&qmax_mutex);
	gFG_BATT_CAPACITY = fgauge_get_Q_max(gFG_temp);
	gFG_BATT_CAPACITY_aging = gFG_BATT_CAPACITY;
	mutex_unlock(&qmax_mutex);

	pr_debug(
		"[%s] gFG_BATT_CAPACITY=%d, gFG_BATT_CAPACITY_aging=%d\r\n",
		__func__, gFG_BATT_CAPACITY, gFG_BATT_CAPACITY_aging);
}

void fgauge_construct_table_by_temp(void)
{
#if defined(CONFIG_POWER_EXT)
#else
	unsigned int i;
	static signed int init_temp = true;
	static signed int curr_temp, last_temp, avg_temp;
	static signed int battTempBuffer[CONSTRUCT_BUFFER_SIZE];
	static signed int temperature_sum;

	static unsigned char tempIndex;

	curr_temp = battery_meter_get_battery_temperature();

	gFG_temp = curr_temp;

	/* Temperature window init */
	if (init_temp == true) {
		for (i = 0; i < CONSTRUCT_BUFFER_SIZE; i++)
			battTempBuffer[i] = curr_temp;

		last_temp = curr_temp;
		temperature_sum = curr_temp * CONSTRUCT_BUFFER_SIZE;
		init_temp = false;
	}
	/* Temperature sliding window */
	temperature_sum -= battTempBuffer[tempIndex];
	temperature_sum += curr_temp;
	battTempBuffer[tempIndex] = curr_temp;
	avg_temp = (temperature_sum) / CONSTRUCT_BUFFER_SIZE;

	if (avg_temp != last_temp) {
		pr_debug(
			"[fgauge_construct_table_by_temp] reconstruct table by temperature change from (%d) to (%d)\r\n",
			last_temp, avg_temp);
		fgauge_construct_r_table_profile(curr_temp,
			fgauge_get_profile_r_table
			(batt_meter_cust_data.temperature_t));
		fgauge_construct_battery_profile(curr_temp,
			fgauge_get_profile
			(batt_meter_cust_data.temperature_t));
		last_temp = avg_temp;
		update_qmax_by_temp();
	}

	tempIndex = (tempIndex + 1) % CONSTRUCT_BUFFER_SIZE;

#endif
}

#if defined(CUST_CAPACITY_OCV2CV_TRANSFORM)
/*
 *	ZCV table is created by 600mA loading.
 *	Here we calculate average current and get a factor based on 600mA.
 */
void fgauge_get_current_factor(void)
{
#if defined(CONFIG_POWER_EXT)
#else
	unsigned int i;
	static signed int init_current = true;
	static signed int inst_current, avg_current;
	static signed int battCurrentBuffer[TEMP_AVERAGE_SIZE];
	static signed int current_sum;
	static unsigned char tempcurrentIndex;

	if (true == gFG_Is_Charging) {
		init_current = true;
		g_currentfactor = 100;
		pr_debug("[fgauge_get_current_factor] Charging!!\r\n");
		return;
	}

	inst_current = gFG_current;

	if (init_current == true) {
		for (i = 0; i < TEMP_AVERAGE_SIZE; i++)
			battCurrentBuffer[i] =
				batt_meter_cust_data.std_loading_current;

		current_sum = batt_meter_cust_data.std_loading_current
			* TEMP_AVERAGE_SIZE;
		init_current = false;
	}

	/* current sliding window */
	current_sum -= battCurrentBuffer[tempcurrentIndex];
	current_sum += inst_current;
	battCurrentBuffer[tempcurrentIndex] = inst_current;
	avg_current = (current_sum) / TEMP_AVERAGE_SIZE;

	if (batt_meter_cust_data.std_loading_current > 1000)
		g_currentfactor = avg_current * 100
			/ batt_meter_cust_data.std_loading_current;
	else
		g_currentfactor = avg_current * 100
			/ 6000;	/* calculate factor by 600ma */

	pr_debug("[fgauge_get_current_factor] %d,%d,%d,%d\r\n",
		inst_current, avg_current, g_currentfactor, gFG_Is_Charging);

	tempcurrentIndex = (tempcurrentIndex + 1) % TEMP_AVERAGE_SIZE;
#endif
}

/*
 *	ZCV table has battery OCV-to-resistance information.
 *	Based on a given discharging current value,
 *  we can get a new estimated Qmax.
 *	Qmax is defined as OCV -I*R < power off voltage.
 *	Default power off voltage is 3400mV.
 */

signed int fgauge_get_Q_max_high_current_by_current(
	signed int i_current, signed short val_temp)
{
	signed int ret_Q_max;
	signed int iIndex, saddles;
	signed int OCV_temp, Rbat_temp, V_drop;
	struct r_profile_struct *p_profile_r;
	struct battery_profile_struct *p_profile_battery;

	/* for Qmax initialization */
	ret_Q_max = fgauge_get_Q_max_high_current(val_temp);

	/* get Rbat and OCV table of the current temperature */
	p_profile_r = fgauge_get_profile_r_table(
		batt_meter_cust_data.temperature_t);
	p_profile_battery = fgauge_get_profile(
		batt_meter_cust_data.temperature_t);
	if (p_profile_r == NULL || p_profile_battery == NULL) {
		pr_notice("get R-Table profile/OCV table profile : fail !\r\n");
		return ret_Q_max;
	}

	if (0 == p_profile_r->resistance || 0 == p_profile_battery->voltage) {
		pr_notice("get R-Table profile/OCV table profile : not ready !\r\n");
		return ret_Q_max;
	}

	saddles = fgauge_get_saddles();

	/* get Qmax in current temperature (>3.4) */
	for (iIndex = 0; iIndex < saddles - 1; iIndex++) {
		OCV_temp = (p_profile_battery + iIndex)->voltage;
		Rbat_temp = (p_profile_r + iIndex)->resistance;
		V_drop = (i_current * Rbat_temp) / 10000;

		if ((OCV_temp - V_drop) < batt_cust_data.system_off_voltage) {
			if (iIndex <= 1)
				ret_Q_max = batt_meter_cust_data.step_of_qmax;
			else
				ret_Q_max = (iIndex - 1) *
					batt_meter_cust_data.step_of_qmax;

			break;
		}
	}

	pr_debug("[fgauge_get_Q_max_by_current] %d,%d,%d,%d,%d\r\n",
		 i_current, iIndex, OCV_temp, Rbat_temp, ret_Q_max);

	return ret_Q_max;
}
#endif

void fg_qmax_update_for_aging(void)
{
#if defined(CONFIG_POWER_EXT)
#else
	bool hw_charging_done = bat_is_charging_full();

	if (hw_charging_done == true) {
		/* charging full, g_HW_Charging_Done == 1 */
		if (gFG_DOD0 > 85) {
			if (gFG_columb < 0)
				gFG_columb =
					gFG_columb -
					gFG_columb * 2; /* absolute value */

			gFG_BATT_CAPACITY_aging =
				(((gFG_columb * 1000) + (5 * gFG_DOD0)) /
				gFG_DOD0) / 10;

			/* tuning */
			gFG_BATT_CAPACITY_aging =
				(gFG_BATT_CAPACITY_aging * 100) /
			    batt_meter_cust_data.aging_tuning_value;

			if (gFG_BATT_CAPACITY_aging == 0) {
				gFG_BATT_CAPACITY_aging = fgauge_get_Q_max(
				battery_meter_get_battery_temperature());
				pr_notice(
					"[fg_qmax_update_for_aging] error, restore gFG_BATT_CAPACITY_aging (%d)\n",
					gFG_BATT_CAPACITY_aging);
			}

			pr_debug(
				"[fg_qmax_update_for_aging] need update : gFG_columb=%d, gFG_DOD0=%d, new_qmax=%d\r\n",
				gFG_columb, gFG_DOD0, gFG_BATT_CAPACITY_aging);
		} else {
			pr_debug(
				"[fg_qmax_update_for_aging] no update : gFG_columb=%d, gFG_DOD0=%d, new_qmax=%d\r\n",
				gFG_columb, gFG_DOD0, gFG_BATT_CAPACITY_aging);
		}
	} else
		pr_debug("[fg_qmax_update_for_aging] hw_charging_done=%d\r\n",
			hw_charging_done);
#endif
}

void dod_init(void)
{
#if defined(CONFIG_POWER_EXT)
	g_rtc_fg_soc = gFG_capacity_by_v;
#else
	g_rtc_fg_soc = get_rtc_spare_fg_value();
#endif

	pr_info("[%s] boot_reason=%d gFG_capacity_by_v=%d g_rtc_fg_soc=%d\n",
		__func__, get_boot_reason(), gFG_capacity_by_v, g_rtc_fg_soc);

#if defined(CONFIG_POWER_ON_OFF_LOOP_TEST)
/* decrease rtc soc by 1 if swocv is less by threshold 10% */
	if (g_rtc_fg_soc > 1 && g_rtc_fg_soc >= gFG_capacity_by_v_init + 10) {
		g_rtc_fg_soc -= 1;
		set_rtc_spare_fg_value(g_rtc_fg_soc);
	}

	/* increase rtc soc by 1 if swocv is more by threshold 10% */
	if (bat_is_charger_exist() == true && g_rtc_fg_soc > 1 &&
	    gFG_capacity_by_v_init - g_rtc_fg_soc > 10) {
		g_rtc_fg_soc += 1;
		set_rtc_spare_fg_value(g_rtc_fg_soc);
	}
#endif

#if defined(SOC_BY_SW_FG)
	if (((g_rtc_fg_soc != 0) &&
		(((abs(g_rtc_fg_soc - gFG_capacity_by_v)) <
		batt_meter_cust_data.cust_poweron_delta_capacity_tolrance)
		|| (abs(g_rtc_fg_soc - g_booting_vbat) <
		batt_meter_cust_data.cust_poweron_delta_capacity_tolrance))
		&& ((gFG_capacity_by_v >
		batt_meter_cust_data.cust_poweron_low_capacity_tolrance
		|| bat_is_charger_exist() == true)))
	    || ((g_rtc_fg_soc != 0)	&&
		(get_boot_reason() == BR_WDT_BY_PASS_PWK
		|| get_boot_reason() == BR_WDT
		|| get_boot_reason() == BR_TOOL_BY_PASS_PWK
		|| get_boot_reason() == BR_2SEC_REBOOT
		|| get_boot_mode() == RECOVERY_BOOT))) {
		pr_info(
			"[%s] %d: overwrite soc_v[%d] by soc_rtc[%d]\n",
			__func__, __LINE__,
			gFG_capacity_by_v, g_rtc_fg_soc);
		gFG_capacity_by_v = g_rtc_fg_soc;
	}
#endif

	pr_notice("[%s] g_rtc_fg_soc=%d, gFG_capacity_by_v=%d\n",
		 __func__, g_rtc_fg_soc, gFG_capacity_by_v);

	if (gFG_capacity_by_v == 0 && bat_is_charger_exist() == true) {
		gFG_capacity_by_v = 1;
		pr_notice("[FGADC] gFG_capacity_by_v=%d\n", gFG_capacity_by_v);
	}

	gFG_capacity = gFG_capacity_by_v;
	gFG_capacity_by_c_init = gFG_capacity;
	gFG_capacity_by_c = gFG_capacity;

	gFG_DOD0 = 100 - gFG_capacity;
	gFG_DOD1 = gFG_DOD0;

	gfg_percent_check_point = gFG_capacity;

	if (batt_meter_cust_data.change_tracking_point) {
		gFG_15_vlot = fgauge_read_v_by_capacity(
			(100 - g_tracking_point));
		pr_notice("[FGADC] gFG_15_vlot = %dmV\n", gFG_15_vlot);
	} else {
		/* gFG_15_vlot = fgauge_read_v_by_capacity(86); //14% */
		gFG_15_vlot = fgauge_read_v_by_capacity(
			(100 - g_tracking_point));
		pr_notice("[FGADC] gFG_15_vlot = %dmV\n", gFG_15_vlot);
		if ((gFG_15_vlot > 3800) || (gFG_15_vlot < 3600)) {
			pr_notice("[FGADC] gFG_15_vlot(%d) over range, reset to 3700\n",
				gFG_15_vlot);
			gFG_15_vlot = 3700;
		}
	}
}

/* ============================================================ // SW FG */
signed int mtk_imp_tracking(signed int ori_voltage,
	signed int ori_current, signed int recursion_time)
{
	signed int ret_compensate_value;
	signed int temp_voltage_1 = ori_voltage;
	signed int temp_voltage_2 = temp_voltage_1;
	int i;

	for (i = 0; i < recursion_time; i++) {
		gFG_resistance_bat = fgauge_read_r_bat_by_v(temp_voltage_2);
		ret_compensate_value = ((ori_current) *
		(gFG_resistance_bat + batt_meter_cust_data.r_fg_value))
		/ 1000;
		ret_compensate_value = (ret_compensate_value + (10 / 2)) / 10;
		temp_voltage_2 = temp_voltage_1 + ret_compensate_value;

		pr_debug(
			"[mtk_imp_tracking] temp_voltage_2=%d,temp_voltage_1=%d,ret_compensate_value=%d,gFG_resistance_bat=%d\n",
			temp_voltage_2, temp_voltage_1,
			ret_compensate_value, gFG_resistance_bat);
	}

	gFG_resistance_bat = fgauge_read_r_bat_by_v(temp_voltage_2);
	ret_compensate_value = ((ori_current) *
		(gFG_resistance_bat + batt_meter_cust_data.r_fg_value +
		batt_meter_cust_data.fg_meter_resistance)) / 1000;
	ret_compensate_value = (ret_compensate_value + (10 / 2)) / 10;
	gFG_compensate_value = ret_compensate_value;

	pr_debug(
		 "[mtk_imp_tracking] temp_voltage_2=%d,temp_voltage_1=%d,ret_compensate_value=%d,gFG_resistance_bat=%d\n",
		 temp_voltage_2, temp_voltage_1,
		 ret_compensate_value, gFG_resistance_bat);

	return ret_compensate_value;
}

void oam_init(void)
{
	int ret;
	signed int vbat_capacity;
	unsigned int charging_enable;

	/*stop charging for vbat measurement */
	battery_charging_control(
		CHARGING_CMD_ENABLE, &charging_enable);

	msleep(50);

	g_booting_vbat = 5;	/* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &gFG_voltage);
	ret = battery_meter_ctrl(
		BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE, &g_booting_vbat);


	gFG_capacity_by_v = fgauge_read_capacity_by_v(gFG_voltage);
	vbat_capacity = fgauge_read_capacity_by_v(g_booting_vbat);

	if (bat_is_charger_exist() == true) {
		pr_notice("[oam_init_inf] gFG_capacity_by_v=%d, vbat_capacity=%d,\n",
			 gFG_capacity_by_v, vbat_capacity);

	/* to avoid plug in cable without battery,
	 * then plug in battery to make hw soc = 100%
	 * if the difference bwtween ZCV and vbat is
	 * too large, using vbat instead ZCV
	 */
	if (((gFG_capacity_by_v == 100)
		&& (vbat_capacity <
		batt_meter_cust_data.cust_poweron_max_vbat_tolrance))
		|| (abs(gFG_capacity_by_v - vbat_capacity) >
		batt_meter_cust_data.cust_poweron_delta_vbat_tolrance)) {
		pr_info(
			"[oam_init] fg_vbat=(%d), vbat=(%d), set fg_vat as vat\n",
			gFG_voltage, g_booting_vbat);

		gFG_voltage = g_booting_vbat;
		gFG_capacity_by_v = vbat_capacity;
	}
	}

	gFG_capacity_by_v_init = gFG_capacity_by_v;

	dod_init();

	gFG_BATT_CAPACITY_aging =
		fgauge_get_Q_max(force_get_tbat(false));

	/* oam_v_ocv_1 = gFG_voltage; */
	/* oam_v_ocv_2 = gFG_voltage; */

	oam_v_ocv_init = fgauge_read_v_by_d(gFG_DOD0);
	oam_v_ocv_2 = oam_v_ocv_1 = oam_v_ocv_init;
	g_vol_bat_hw_ocv = gFG_voltage;

	oam_r_1 = fgauge_read_r_bat_by_v(gFG_voltage);
	oam_r_2 = oam_r_1;

	oam_d0 = gFG_DOD0;
	oam_d_5 = oam_d0;
	oam_i_ori = gFG_current;
	g_d_hw_ocv = oam_d0;

	if (oam_init_i == 0) {
		pr_debug(
			"[oam_init] oam_v_ocv_1,oam_v_ocv_2,oam_r_1,oam_r_2,oam_d0,oam_i_ori\n");
		oam_init_i = 1;
	}

	pr_debug("[oam_init] %d,%d,%d,%d,%d,%d\n",
		oam_v_ocv_1, oam_v_ocv_2, oam_r_1, oam_r_2, oam_d0, oam_i_ori);

	pr_debug("[oam_init_inf] hw_OCV, hw_D0, RTC, D0, oam_OCV_init, tbat\n");
	pr_debug(
		"[oam_run_inf] oam_OCV1, oam_OCV2, vbat, I1, I2, R1, R2, Car1, Car2,qmax, tbat\n");
	pr_debug("[oam_result_inf] D1, D2, D3, D4, D5, UI_SOC\n");


	pr_notice("[oam_init_inf] %d, %d, %d, %d, %d, %d\n",
		gFG_voltage,
		(100 - fgauge_read_capacity_by_v(gFG_voltage)),
		g_rtc_fg_soc, gFG_DOD0, oam_v_ocv_init, force_get_tbat(false));
}


void oam_run(void)
{
	int vol_bat;
	/* int vol_bat_hw_ocv=0; */
	/* int d_hw_ocv=0; */
	int charging_current = 0;
	int ret;
	/* unsigned int now_time; */
	struct timespec now_time;
	signed int delta_time;

	/* now_time = rtc_read_hw_time(); */
	getrawmonotonic(&now_time);

	/* delta_time = now_time - last_oam_run_time; */
	delta_time = now_time.tv_sec - last_oam_run_time.tv_sec;

	pr_debug("[oam_run_time] delta time=%d\n", delta_time);

	last_oam_run_time = now_time;

	/* Reconstruct table if temp changed; */
	fgauge_construct_table_by_temp();

	vol_bat = 15;		/* set avg times */
	ret = battery_meter_ctrl(
		BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE, &vol_bat);

	/* 0.1mA */
	oam_i_1 = (((oam_v_ocv_1 - vol_bat) * 1000) * 10) / oam_r_1;
	/* 0.1mA */
	oam_i_2 = (((oam_v_ocv_2 - vol_bat) * 1000) * 10) / oam_r_2;

	if (oam_i_2 < 0) {
		charging_current = get_charging_setting_current() / 10;
		if (abs(oam_i_2) > charging_current) {
			charging_current = oam_i_2;
			oam_i_2 = 0 - get_charging_setting_current() / 10;
			pr_notice("[%s] use max charging current: (%d, %d)\n",
				__func__, oam_i_2, charging_current);
		}
	}

	/* 0.1mAh */
	oam_car_1 = (oam_i_1 * delta_time / 3600) + oam_car_1;
	/* 0.1mAh */
	oam_car_2 = (oam_i_2 * delta_time / 3600) + oam_car_2;

	oam_d_1 = oam_d0 + (oam_car_1 * 100 / 10) / gFG_BATT_CAPACITY_aging;
	if (oam_d_1 < 0)
		oam_d_1 = 0;
	if (oam_d_1 > 100)
		oam_d_1 = 100;

	oam_d_2 = oam_d0 + (oam_car_2 * 100 / 10) / gFG_BATT_CAPACITY_aging;
	if (oam_d_2 < 0)
		oam_d_2 = 0;
	if (oam_d_2 > 100)
		oam_d_2 = 100;

	oam_v_ocv_1 = vol_bat + mtk_imp_tracking(vol_bat, oam_i_2, 5);

	oam_d_3 = fgauge_read_d_by_v(oam_v_ocv_1);
	if (oam_d_3 < 0)
		oam_d_3 = 0;
	if (oam_d_3 > 100)
		oam_d_3 = 100;

	oam_r_1 = fgauge_read_r_bat_by_v(oam_v_ocv_1);

	oam_v_ocv_2 = fgauge_read_v_by_d(oam_d_2);
	oam_r_2 = fgauge_read_r_bat_by_v(oam_v_ocv_2);

#if 0
	oam_d_4 = (oam_d_2 + oam_d_3) / 2;
#else
	oam_d_4 = oam_d_3;
#endif

	gFG_columb = oam_car_2 / 10;	/* mAh */

	if ((oam_i_1 < 0) || (oam_i_2 < 0))
		gFG_Is_Charging = true;
	else
		gFG_Is_Charging = false;

#if 0
	if (gFG_Is_Charging == false) {
		d5_count_time = 60;
	} else {
		charging_current = get_charging_setting_current();
		charging_current = charging_current / 100;
		d5_count_time_rate =
			(((gFG_BATT_CAPACITY_aging * 60 * 60 /
			100 / (charging_current - 50)) * 10) +
		     5) / 10;

		if (d5_count_time_rate < 1)
			d5_count_time_rate = 1;

		d5_count_time = d5_count_time_rate;
	}
#else
	d5_count_time = 60;
#endif
	d5_count = d5_count + delta_time;
	if (d5_count >= d5_count_time) {
		if (gFG_Is_Charging == false) {
			if (oam_d_3 > oam_d_5)
				oam_d_5 = oam_d_5 + 1;
			else
				if (oam_d_4 > oam_d_5)
					oam_d_5 = oam_d_5 + 1;
		} else {
			if (oam_d_5 > oam_d_3)
				oam_d_5 = oam_d_5 - 1;
			else
				if (oam_d_4 < oam_d_5)
					oam_d_5 = oam_d_5 - 1;
		}
		d5_count = 0;
		oam_d_3_pre = oam_d_3;
		oam_d_4_pre = oam_d_4;
	}

	pr_debug("[oam_run] %d,%d,%d,%d,%d,%d,%d,%d\n",
		 d5_count, d5_count_time, oam_d_3_pre,
		 oam_d_3, oam_d_4_pre, oam_d_4, oam_d_5,
		 charging_current);

	if (oam_run_i == 0) {
		pr_debug(
			 "[oam_run] oam_i_1,oam_i_2,oam_car_1,oam_car_2,oam_d_1,oam_d_2,oam_v_ocv_1,oam_d_3,oam_r_1,oam_v_ocv_2,oam_r_2,vol_bat,g_vol_bat_hw_ocv,g_d_hw_ocv\n");
		oam_run_i = 1;
	}

	pr_notice("[oam_run] %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		oam_i_1, oam_i_2, oam_car_1, oam_car_2, oam_d_1, oam_d_2,
		oam_v_ocv_1, oam_d_3, oam_r_1, oam_v_ocv_2, oam_r_2, vol_bat,
		g_vol_bat_hw_ocv, g_d_hw_ocv);

	pr_debug("[oam_total] %d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		gFG_capacity_by_c, gFG_capacity_by_v, gfg_percent_check_point,
		oam_d_1, oam_d_2, oam_d_3, oam_d_4, oam_d_5,
		gFG_capacity_by_c_init, g_d_hw_ocv);

	pr_debug("[oam_total_s] %d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		gFG_capacity_by_c,	/* 1 */
		gFG_capacity_by_v,	/* 2 */
		gfg_percent_check_point,	/* 3 */
		(100 - oam_d_1),	/* 4 */
		(100 - oam_d_2),	/* 5 */
		(100 - oam_d_3),	/* 6 */
		(100 - oam_d_4),	/* 9 */
		(100 - oam_d_5),	/* 10 */
		gFG_capacity_by_c_init,	/* 7 */
		(100 - g_d_hw_ocv)	/* 8 */
	);

	pr_debug("[oam_total_s_err] %d,%d,%d,%d,%d,%d,%d\n",
		(gFG_capacity_by_c - gFG_capacity_by_v),
		(gFG_capacity_by_c - gfg_percent_check_point),
		(gFG_capacity_by_c - (100 - oam_d_1)),
		(gFG_capacity_by_c - (100 - oam_d_2)),
		(gFG_capacity_by_c - (100 - oam_d_3)),
		(gFG_capacity_by_c - (100 - oam_d_4)),
		(gFG_capacity_by_c - (100 - oam_d_5)));

	pr_debug("[oam_init_inf] %d, %d, %d, %d, %d, %d\n",
		gFG_voltage,
		(100 - fgauge_read_capacity_by_v(gFG_voltage)),
		g_rtc_fg_soc, gFG_DOD0,
		oam_v_ocv_init, force_get_tbat(false));

	pr_debug("[oam_run_inf] %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
		oam_v_ocv_1, oam_v_ocv_2, vol_bat, oam_i_1, oam_i_2,
		oam_r_1, oam_r_2, oam_car_1, oam_car_2,
		gFG_BATT_CAPACITY_aging, force_get_tbat(false), oam_d0);

	pr_debug("[oam_result_inf] %d, %d, %d, %d, %d, %d\n", oam_d_1,
		oam_d_2, oam_d_3, oam_d_4, oam_d_5, BMT_status.UI_SOC);

	/* set gFG_current always positive */
	if (oam_i_2 > 0)
		gFG_current = oam_i_2;
	else
		gFG_current = -oam_i_2;

	#if defined(CUST_CAPACITY_OCV2CV_TRANSFORM)
	fgauge_get_current_factor();
	#endif

	#if defined(CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT)
	fgauge_aging_algorithm();
	#endif

	/* update debug tool for sw fg */
	g_fg_dbg_bat_volt = vol_bat;

	/* debug sysfs show negative for
	 * discharging, positive for charging
	 */
	g_fg_dbg_bat_current = -oam_i_2;

	g_fg_dbg_bat_zcv = oam_v_ocv_2;
	g_fg_dbg_bat_temp = gFG_temp;
	g_fg_dbg_bat_r = oam_r_2;
	g_fg_dbg_bat_car = gFG_columb;
	g_fg_dbg_bat_qmax = gFG_BATT_CAPACITY_aging;
	g_fg_dbg_d0 = oam_d0;
	g_fg_dbg_d1 = oam_d_2;
	g_fg_dbg_percentage = bat_get_ui_percentage();
	g_fg_dbg_percentage_fg = 100 - oam_d_2;
	g_fg_dbg_percentage_voltmode = 100 - oam_d_5;
}

void table_init(void)
{
	struct battery_profile_struct *profile_p;
	struct r_profile_struct *profile_p_r_table;

	int temperature = force_get_tbat(false);

	/* Re-constructure r-table profile according to current temperature */
	profile_p_r_table = fgauge_get_profile_r_table(
		batt_meter_cust_data.temperature_t);
	if (profile_p_r_table == NULL) {
		pr_err(
			 "[FGADC] fgauge_get_profile_r_table : create table fail !\r\n");
		return;
	}
	fgauge_construct_r_table_profile(
		temperature, profile_p_r_table);

	/* Re-constructure battery profile according to current temperature */
	profile_p = fgauge_get_profile(batt_meter_cust_data.temperature_t);
	if (profile_p == NULL) {
		pr_err("[FGADC] fgauge_get_profile : create table fail !\r\n");
		return;
	}

	fgauge_construct_battery_profile(temperature, profile_p);
}

signed int auxadc_algo_run(void)
{
	signed int val;

	gFG_voltage = battery_meter_get_battery_voltage(false);
	val = fgauge_read_capacity_by_v(gFG_voltage);

	pr_notice("[auxadc_algo_run] %d,%d\n", gFG_voltage, val);

	return val;
}

signed int get_dynamic_period(int first_use,
	int first_wakeup_time, int battery_capacity_level)
{
#if defined(CONFIG_POWER_EXT)

	return first_wakeup_time;

#elif defined(SOC_BY_AUXADC) ||  defined(SOC_BY_SW_FG)
	signed int vbat_val;

#ifdef CONFIG_MTK_POWER_EXT_DETECT
	if (true == bat_is_ext_power())
		return batt_meter_cust_data.normal_wakeup_period;
#endif

	vbat_val = g_sw_vbat_temp;

	/* change wake up period when system suspend. */
	if (vbat_val > batt_meter_cust_data.vbat_normal_wakeup) {
		/* 3.6v, 90 min */
		g_spm_timer = batt_meter_cust_data.normal_wakeup_period;
	} else if (vbat_val > batt_meter_cust_data.vbat_low_power_wakeup) {
		/* 3.5v, 5 min */
		g_spm_timer = batt_meter_cust_data.low_power_wakeup_period;
	} else {
		/* 0.5 min */
		g_spm_timer = batt_meter_cust_data.close_poweroff_wakeup_period;
	}

	pr_debug("vbat_val=%d, g_spm_timer=%d\n", vbat_val, g_spm_timer);
#endif
	return g_spm_timer;
}

/* ============================================================ // */
signed int battery_meter_get_battery_voltage(bool update)
{
	int ret;
	int val = 5;
	static int pre_val = -1;

	if (update == true || pre_val == -1) {
		val = 5;	/* set avg times */
		ret = battery_meter_ctrl(
			BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE, &val);
		pre_val = val;
	} else {
		val = pre_val;
	}
	g_sw_vbat_temp = val;

#if defined(CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT)
	if (g_sw_vbat_temp > gFG_max_voltage)
		gFG_max_voltage = g_sw_vbat_temp;


	if (g_sw_vbat_temp < gFG_min_voltage)
		gFG_min_voltage = g_sw_vbat_temp;

#endif

	return val;
}

signed int battery_meter_get_charging_current_imm(void)
{
#ifdef AUXADC_SUPPORT_IMM_CURRENT_MODE
	return PMIC_IMM_GetCurrent();
#else
	int ret;
	signed int ADC_I_SENSE = 1;	/* 1 measure time */
	signed int ADC_BAT_SENSE = 1;	/* 1 measure time */
	int ICharging;

	ret = battery_meter_ctrl(
		BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE, &ADC_BAT_SENSE);
	ret = battery_meter_ctrl(
		BATTERY_METER_CMD_GET_ADC_V_I_SENSE, &ADC_I_SENSE);

	ICharging =
		(ADC_I_SENSE - ADC_BAT_SENSE + g_I_SENSE_offset)
		* 1000 / batt_meter_cust_data.cust_r_sense;
	return ICharging;
#endif
}

signed int battery_meter_get_charging_current(void)
{
#ifdef DISABLE_CHARGING_CURRENT_MEASURE
	return 0;
#else
	signed int ADC_BAT_SENSE_tmp[20] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	signed int ADC_BAT_SENSE_sum = 0;
	signed int ADC_BAT_SENSE;
	signed int ADC_I_SENSE_tmp[20] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	signed int ADC_I_SENSE_sum = 0;
	signed int ADC_I_SENSE;
	int repeat = 20;
	int i;
	int j;
	signed int temp;
	int ICharging;
	int ret;
	int val = 1;

	if (batt_meter_cust_data.swchr_power_path)
		return 0;

	for (i = 0; i < repeat; i++) {
		val = 1; /* set avg times */
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE,
				&val);
		ADC_BAT_SENSE_tmp[i] = val;

		val = 1; /* set avg times */
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_I_SENSE,
					 &val);
		ADC_I_SENSE_tmp[i] = val;

		ADC_BAT_SENSE_sum += ADC_BAT_SENSE_tmp[i];
		ADC_I_SENSE_sum += ADC_I_SENSE_tmp[i];
	}

	/* sorting    BAT_SENSE */
	for (i = 0; i < repeat; i++) {
		for (j = i; j < repeat; j++) {
			if (ADC_BAT_SENSE_tmp[j] < ADC_BAT_SENSE_tmp[i]) {
				temp = ADC_BAT_SENSE_tmp[j];
				ADC_BAT_SENSE_tmp[j] = ADC_BAT_SENSE_tmp[i];
				ADC_BAT_SENSE_tmp[i] = temp;
			}
		}
	}

	pr_debug("[g_Get_I_Charging:BAT_SENSE]\r\n");
	for (i = 0; i < repeat; i++)
		pr_debug("%d,", ADC_BAT_SENSE_tmp[i]);

	pr_debug("\r\n");

	/* sorting    I_SENSE */
	for (i = 0; i < repeat; i++) {
		for (j = i; j < repeat; j++) {
			if (ADC_I_SENSE_tmp[j] < ADC_I_SENSE_tmp[i]) {
				temp = ADC_I_SENSE_tmp[j];
				ADC_I_SENSE_tmp[j] = ADC_I_SENSE_tmp[i];
				ADC_I_SENSE_tmp[i] = temp;
			}
		}
	}

	pr_debug("[g_Get_I_Charging:I_SENSE]\r\n");
	for (i = 0; i < repeat; i++)
		pr_debug("%d,", ADC_I_SENSE_tmp[i]);

	pr_debug("\r\n");

	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[0];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[1];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[18];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[19];
	ADC_BAT_SENSE = ADC_BAT_SENSE_sum / (repeat - 4);

	pr_debug("[g_Get_I_Charging] ADC_BAT_SENSE=%d\r\n", ADC_BAT_SENSE);

	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[0];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[1];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[18];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[19];
	ADC_I_SENSE = ADC_I_SENSE_sum / (repeat - 4);

	pr_debug("[g_Get_I_Charging] ADC_I_SENSE(Before)=%d\r\n", ADC_I_SENSE);


	pr_debug("[g_Get_I_Charging] ADC_I_SENSE(After)=%d\r\n", ADC_I_SENSE);

	if (ADC_I_SENSE > ADC_BAT_SENSE) {
		ICharging =
			(ADC_I_SENSE - ADC_BAT_SENSE + g_I_SENSE_offset)
			* 1000 / batt_meter_cust_data.cust_r_sense;
	} else
		ICharging = 0;

	return ICharging;
#endif
}

signed int battery_meter_get_battery_current(void)
{
	int ret;
	signed int val;

	if (g_auxadc_solution == 1)
		val = oam_i_2;
	else
		ret = battery_meter_ctrl(
		BATTERY_METER_CMD_GET_HW_FG_CURRENT, &val);

	return val;
}

bool battery_meter_get_battery_current_sign(void)
{
	int ret;
	bool val;

	if (g_auxadc_solution == 1)
		val = 0;
	else
		ret = battery_meter_ctrl(
		BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN, &val);

	return val;
}

signed int battery_meter_get_car(void)
{
	int ret;
	signed int val;

	if (g_auxadc_solution == 1)
		val = oam_car_2;
	else
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &val);

	return val;
}

signed int battery_meter_get_charge_full(void)
{
	return 1000 * g_fg_dbg_bat_qmax;
}

signed int battery_meter_get_charge_counter(void)
{
	return 1000 * g_fg_dbg_bat_qmax * g_fg_dbg_percentage / 100;
}

signed int battery_meter_get_battery_temperature(void)
{
#if defined(CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT)
	signed int batt_temp = force_get_tbat(true);

	if (batt_temp > gFG_max_temperature)
		gFG_max_temperature = batt_temp;
	if (batt_temp < gFG_min_temperature)
		gFG_min_temperature = batt_temp;

	return batt_temp;
#else
	return force_get_tbat(true);
#endif
}

signed int battery_meter_get_charger_voltage(void)
{
	int ret;
	int val;

	val = 5;
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_CHARGER, &val);
	return val;
}

signed int battery_meter_get_vbus_now(void)
{
	int ret;
	int val;

	val = 1;
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_CHARGER, &val);

	return val;
}

#if defined(CUST_CAPACITY_OCV2CV_TRANSFORM)
void battery_meter_set_reset_soc(bool bUSE_UI_SOC)
{
	g_USE_UI_SOC = bUSE_UI_SOC;
}

signed int battery_meter_get_battery_soc(void)
{
#if defined(SOC_BY_HW_FG)
	return gFG_capacity_by_c;
#endif

#if defined(SOC_BY_SW_FG)
	if (batt_meter_cust_data.oam_d5)
		return 100 - oam_d_5;
	else
		return 100 - oam_d_2;
#endif

	return 50;
}

/* Here we compensate D1 by a factor from Qmax with loading. */
signed int battery_meter_trans_battery_percentage(signed int d_val)
{
	signed int d_val_before;
	signed int temp_val;
	signed int C_0mA;
	signed int C_600mA;
	signed int C_current;
	signed int i_avg_current;

	d_val_before = d_val;
	temp_val = battery_meter_get_battery_temperature();
	C_0mA = fgauge_get_Q_max(temp_val);

	/* discharging and current > 600ma */
	i_avg_current = (g_currentfactor *
		batt_meter_cust_data.std_loading_current) / 100;
	if ((false == gFG_Is_Charging) && (g_currentfactor > 100)) {
		C_600mA = fgauge_get_Q_max_high_current(temp_val);
		C_current = fgauge_get_Q_max_high_current_by_current(
			i_avg_current, temp_val);
		if (C_current < C_600mA)
			C_600mA = C_current;
	} else
		C_600mA = fgauge_get_Q_max_high_current(temp_val);

	if (C_0mA > C_600mA) {
		if (true == gFG_Is_Charging)
			d_val = d_val -
				(((C_0mA - C_600mA) * (100 - d_val)) / C_600mA);
		else
			d_val = d_val +
				(((C_0mA - C_600mA) * (d_val)) / C_600mA);
	}

	if (d_val > 100)
		d_val = 100;
	else if (d_val < 0)
		d_val = 0;

	pr_notice("[battery_meter_trans_battery_percentage] %d,%d,%d,%d,%d,%d,%d\r\n",
		temp_val, C_0mA, C_600mA, d_val_before,
		d_val, g_currentfactor, gFG_Is_Charging);

	return d_val;
}
#endif

#if defined(FG_BAT_INT)
signed int battery_meter_set_columb_interrupt(unsigned int val)
{
	pr_debug("battery_meter_set_columb_interrupt=%d\n", val);
	battery_meter_ctrl(BATTERY_METER_CMD_SET_COLUMB_INTERRUPT, &val);
	return 0;
}
#endif				/* #if defined(FG_BAT_INT) */

signed int battery_meter_get_battery_percentage(void)
{
#if !defined(CONFIG_POWER_EXT)
	if (bat_is_charger_exist() == false)
		fg_qmax_update_for_aging_flag = 1;

#if defined(SOC_BY_AUXADC)
	return auxadc_algo_run();
#endif

#if defined(SOC_BY_SW_FG)
	oam_run();
#if !defined(CUST_CAPACITY_OCV2CV_TRANSFORM)
	if (batt_meter_cust_data.oam_d5)
		return 100 - oam_d_5;
	else
		return 100 - oam_d_2;
#else
	if (batt_meter_cust_data.oam_d5)
		return 100 - battery_meter_trans_battery_percentage(oam_d_5);
	else
		return 100 - battery_meter_trans_battery_percentage(oam_d_2);
#endif
#endif
#endif
	/* define CONFIG_POWER_EXT */
	return 50;
}


signed int battery_meter_initial(void)
{
#if !defined(CONFIG_POWER_EXT)
	static bool meter_initilized;

	mutex_lock(&FGADC_mutex);
	if (meter_initilized == false) {

#if defined(SOC_BY_AUXADC)
		g_auxadc_solution = 1;
		table_init();
		pr_notice("[battery_meter_initial] SOC_BY_AUXADC done\n");
#endif

#if defined(SOC_BY_SW_FG)
		g_auxadc_solution = 1;
		table_init();
		oam_init();
		pr_notice("[battery_meter_initial] SOC_BY_SW_FG done\n");
#endif
		meter_initilized = true;
	}
	mutex_unlock(&FGADC_mutex);
#endif

	return 0;
}

void reset_parameter_car(void)
{
#if defined(SOC_BY_SW_FG)
	oam_car_1 = 0;
	oam_car_2 = 0;
	gFG_columb = 0;
#endif
}

void reset_parameter_dod_change(void)
{
#if defined(SOC_BY_SW_FG)
	pr_debug("[FGADC] Update oam_d0(%d) by %d \r\n", oam_d0, oam_d_5);
	oam_d0 = oam_d_5;
	gFG_DOD0 = oam_d0;
	oam_d_1 = oam_d_5;
	oam_d_2 = oam_d_5;
	oam_d_3 = oam_d_5;
	oam_d_4 = oam_d_5;
#endif
}

void reset_parameter_dod_full(unsigned int ui_percentage)
{
#if defined(SOC_BY_SW_FG)
	pr_debug("[battery_meter_reset]1 oam_d0=%d,oam_d_5=%d,ui=%d\n", oam_d0,
		 oam_d_5, ui_percentage);
	oam_d0 = 100 - ui_percentage;
	gFG_DOD0 = oam_d0;
	gFG_DOD1 = oam_d0;
	oam_d_1 = oam_d0;
	oam_d_2 = oam_d0;
	oam_d_3 = oam_d0;
	oam_d_4 = oam_d0;
	oam_d_5 = oam_d0;
	pr_debug("[battery_meter_reset]2 oam_d0=%d,oam_d_5=%d,ui=%d\n", oam_d0,
		 oam_d_5, ui_percentage);
#endif
}

signed int battery_meter_reset(void)
{
#if !defined(CONFIG_POWER_EXT)
	unsigned int ui_percentage = bat_get_ui_percentage();

#if defined(CUST_CAPACITY_OCV2CV_TRANSFORM)
	if (false == g_USE_UI_SOC) {
		ui_percentage = battery_meter_get_battery_soc();
		g_USE_UI_SOC = true;
		pr_debug("[CUST_CAPACITY_OCV2CV_TRANSFORM]Use Battery SOC: %d\n",
			 ui_percentage);
	}
#endif
	reset_parameter_car();
	reset_parameter_dod_full(ui_percentage);
#endif
	return 0;
}

signed int battery_meter_get_data(struct battery_meter_data *data)
{
	if (!data)
		return -1;

	data->aging_factor = gFG_aging_factor;
	data->battery_cycle = gFG_battery_cycle;
	data->batt_capacity = gFG_BATT_CAPACITY;
	data->batt_capacity_aging = gFG_BATT_CAPACITY_aging;
	data->columb_sum = gFG_columb_sum;

	return 0;
}

signed int battery_meter_sync(signed int bat_i_sense_offset)
{
#if !defined(CONFIG_POWER_EXT)
	g_I_SENSE_offset = bat_i_sense_offset;
#endif
	return 0;
}

signed int battery_meter_get_battery_zcv(void)
{
#if defined(CONFIG_POWER_EXT)
	return 3987;
#else
	return gFG_voltage;
#endif
}

signed int battery_meter_get_battery_nPercent_zcv(void)
{
#if defined(CONFIG_POWER_EXT)
	return 3700;
#else
	return gFG_15_vlot;
#endif
}

signed int battery_meter_get_battery_nPercent_UI_SOC(void)
{
#if defined(CONFIG_POWER_EXT)
	return 15;
#else
	return g_tracking_point;
#endif
}

signed int battery_meter_get_tempR(signed int dwVolt)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	int TRes;

	TRes =
	    (batt_meter_cust_data.rbat_pull_up_r * dwVolt) /
	    (batt_meter_cust_data.rbat_pull_up_volt - dwVolt);

	return TRes;
#endif
}

signed int battery_meter_get_tempV(void)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	int ret;
	int val;

	val = 1;
	ret = battery_meter_ctrl(
		BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP, &val);
	return val;
#endif
}

signed int battery_meter_get_VSense(void)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	int ret;
	int val;

	val = 1;
	ret = battery_meter_ctrl(
		BATTERY_METER_CMD_GET_ADC_V_I_SENSE, &val);
	return val;
#endif
}

#if defined(CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT)
/* ============================================================ // */
static bool custom_battery_cycle;
static ssize_t show_FG_Battery_Cycle(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	signed int val;

	if (custom_battery_cycle)
		val = 0 - gFG_battery_cycle;
	else
		val = gFG_battery_cycle;

	pr_debug("[FG] gFG_battery_cycle  : (%d, %d)\n",
		gFG_battery_cycle, val);
	return sprintf(buf, "%d\n", val);
}

static ssize_t store_FG_Battery_Cycle(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	signed int cycle, ret;

	ret = kstrtoint(buf, 0, &cycle);
	if (ret) {
		pr_debug("[FG] format error!\n");
		return size;
	}

	pr_debug("[FG] input cycle:%d\n", cycle);
	if (abs(cycle) <= MAX_BATTERY_CYCLE) {
		if (cycle < 0) {
			custom_battery_cycle = true;
			cycle = 0 - cycle;
		} else
			custom_battery_cycle = false;

		update_battery_aging_factor(cycle);
		dump_current_battery_profile();
	}

	return size;
}

static DEVICE_ATTR(FG_Battery_Cycle, 0664,
	show_FG_Battery_Cycle, store_FG_Battery_Cycle);

static ssize_t show_FG_Max_Battery_Voltage(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] gFG_max_voltage  : %d\n", gFG_max_voltage);
	return sprintf(buf, "%d\n", gFG_max_voltage);
}

static ssize_t store_FG_Max_Battery_Voltage(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	signed int voltage, ret;

	ret = kstrtoint(buf, 0, &voltage);
	if (ret) {
		pr_debug("[FG] format error!\n");
		return size;
	}

	if (voltage > gFG_max_voltage) {
		pr_notice("[FG] update battery max voltage: %d\n",
			voltage);
		gFG_max_voltage = voltage;
	}
	return size;
}

static DEVICE_ATTR(FG_Max_Battery_Voltage, 0664,
	show_FG_Max_Battery_Voltage, store_FG_Max_Battery_Voltage);

static ssize_t show_FG_Min_Battery_Voltage(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] gFG_min_voltage  : %d\n", gFG_min_voltage);
	return sprintf(buf, "%d\n", gFG_min_voltage);
}

static ssize_t store_FG_Min_Battery_Voltage(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	signed int voltage, ret;

	ret = kstrtoint(buf, 0, &voltage);
	if (ret) {
		pr_debug("[FG] format error!\n");
		return size;
	}

	if (voltage < gFG_min_voltage) {
		pr_notice("[FG] update battery min voltage: %d\n",
			voltage);
		gFG_min_voltage = voltage;
	}
	return size;
}

static DEVICE_ATTR(FG_Min_Battery_Voltage, 0664,
	show_FG_Min_Battery_Voltage, store_FG_Min_Battery_Voltage);

static ssize_t show_FG_Max_Battery_Current(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] gFG_max_current  : %d\n", gFG_max_current);
	return sprintf(buf, "%d\n", gFG_max_current);
}

static ssize_t store_FG_Max_Battery_Current(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	signed int bat_current, ret;

	ret = kstrtoint(buf, 0, &bat_current);
	if (ret) {
		pr_debug("[FG] format error!\n");
		return size;
	}

	if (bat_current > gFG_max_current) {
		pr_notice("[FG] update battery max current: %d\n",
			bat_current);
		gFG_max_current = bat_current;
	}
	return size;
}

static DEVICE_ATTR(FG_Max_Battery_Current, 0664,
	show_FG_Max_Battery_Current, store_FG_Max_Battery_Current);

static ssize_t show_FG_Min_Battery_Current(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] gFG_min_current  : %d\n", gFG_min_current);
	return sprintf(buf, "%d\n", gFG_min_current);
}

static ssize_t store_FG_Min_Battery_Current(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	signed int bat_current, ret;

	ret = kstrtoint(buf, 0, &bat_current);
	if (ret) {
		pr_debug("[FG] format error!\n");
		return size;
	}

	if (bat_current < gFG_min_current) {
		pr_notice("[FG] update battery min current: %d\n",
			bat_current);
		gFG_min_current = bat_current;
	}
	return size;
}

static DEVICE_ATTR(FG_Min_Battery_Current, 0664,
	show_FG_Min_Battery_Current, store_FG_Min_Battery_Current);

static ssize_t show_FG_Max_Battery_Temperature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] gFG_max_temperature  : %d\n", gFG_max_temperature);
	return sprintf(buf, "%d\n", gFG_max_temperature);
}

static ssize_t store_FG_Max_Battery_Temperature(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	signed int temp, ret;

	ret = kstrtoint(buf, 0, &temp);
	if (ret) {
		pr_debug("[FG] format error!\n");
		return size;
	}

	if (temp > gFG_max_temperature) {
		pr_notice("[FG] update battery max temp: %d\n",
			temp);
		gFG_max_temperature = temp;
	}
	return size;
}

static DEVICE_ATTR(FG_Max_Battery_Temperature, 0664,
	show_FG_Max_Battery_Temperature, store_FG_Max_Battery_Temperature);

static ssize_t show_FG_Min_Battery_Temperature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] gFG_min_temperature  : %d\n",
		gFG_min_temperature);
	return sprintf(buf, "%d\n", gFG_min_temperature);
}

static ssize_t store_FG_Min_Battery_Temperature(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	signed int temp, ret;

	ret = kstrtoint(buf, 0, &temp);
	if (ret) {
		pr_debug("[FG] format error!\n");
		return size;
	}

	if (temp < gFG_min_temperature) {
		pr_notice("[FG] update battery min temp: %d\n",
			temp);
		gFG_min_temperature = temp;
	}
	return size;
}

static DEVICE_ATTR(FG_Min_Battery_Temperature, 0664,
	show_FG_Min_Battery_Temperature, store_FG_Min_Battery_Temperature);

static ssize_t show_FG_Aging_Factor(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] gFG_aging_factor  : %d\n",
		gFG_aging_factor);
	return sprintf(buf, "%d\n", gFG_aging_factor);
}

static ssize_t store_FG_Aging_Factor(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	signed int factor, ret;

	ret = kstrtoint(buf, 0, &factor);
	if (ret) {
		pr_debug("[FG] format error!\n");
		return size;
	}

	pr_debug("[FG] input factor:%d\n", factor);
	if (factor <= 100 && factor >= 0)
		update_battery_cycle(factor);

	return size;
}

static DEVICE_ATTR(FG_Aging_Factor, 0664,
	show_FG_Aging_Factor, store_FG_Aging_Factor);
#endif

/* ============================================================ // */
static ssize_t show_FG_Current(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	signed int fg_current_inout_battery;

	fg_current_inout_battery = g_fg_dbg_bat_current;
	pr_notice("[FG] gFG_current_inout_battery : %d\n",
		 fg_current_inout_battery);
	return sprintf(buf, "%d\n", fg_current_inout_battery);
}

static ssize_t store_FG_Current(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_Current, 0664, show_FG_Current, store_FG_Current);

static ssize_t show_FG_g_slp_current(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_slp_current);
}
static DEVICE_ATTR(FG_g_slp_current, 0444, show_FG_g_slp_current, NULL);


static ssize_t show_FG_g_fg_dbg_bat_volt(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] g_fg_dbg_bat_volt : %d\n",
		g_fg_dbg_bat_volt);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_volt);
}

static ssize_t store_FG_g_fg_dbg_bat_volt(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_volt, 0664,
	show_FG_g_fg_dbg_bat_volt, store_FG_g_fg_dbg_bat_volt);
/* ============================================================ // */
static ssize_t show_FG_g_fg_dbg_bat_instant_volt(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	signed int voltage;

	voltage = battery_meter_get_battery_voltage(true); /* in unit of mV */
	pr_debug("[FG] instant voltage : %d\n", voltage);
	return sprintf(buf, "%d\n", voltage);
}

static ssize_t store_FG_g_fg_dbg_bat_instant_volt(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_instant_volt, 0664,
	show_FG_g_fg_dbg_bat_instant_volt, store_FG_g_fg_dbg_bat_instant_volt);
/* ============================================================ // */
static ssize_t show_FG_g_fg_dbg_bat_current(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] g_fg_dbg_bat_current : %d\n",
		g_fg_dbg_bat_current);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_current);
}

static ssize_t store_FG_g_fg_dbg_bat_current(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_current, 0664,
	show_FG_g_fg_dbg_bat_current, store_FG_g_fg_dbg_bat_current);

static ssize_t show_FG_g_fg_dbg_bat_zcv(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] g_fg_dbg_bat_zcv : %d\n", g_fg_dbg_bat_zcv);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_zcv);
}

static ssize_t store_FG_g_fg_dbg_bat_zcv(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_zcv, 0664,
	show_FG_g_fg_dbg_bat_zcv, store_FG_g_fg_dbg_bat_zcv);

static ssize_t show_FG_g_fg_dbg_bat_temp(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] g_fg_dbg_bat_temp : %d\n",
		g_fg_dbg_bat_temp);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_temp);
}

static ssize_t store_FG_g_fg_dbg_bat_temp(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_temp, 0664,
	show_FG_g_fg_dbg_bat_temp, store_FG_g_fg_dbg_bat_temp);

static ssize_t show_FG_g_fg_dbg_bat_r(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] g_fg_dbg_bat_r : %d\n", g_fg_dbg_bat_r);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_r);
}

static ssize_t store_FG_g_fg_dbg_bat_r(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_r, 0664,
	show_FG_g_fg_dbg_bat_r, store_FG_g_fg_dbg_bat_r);

static ssize_t show_FG_g_fg_dbg_bat_car(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] g_fg_dbg_bat_car : %d\n", g_fg_dbg_bat_car);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_car);
}

static ssize_t store_FG_g_fg_dbg_bat_car(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_car, 0664,
	show_FG_g_fg_dbg_bat_car, store_FG_g_fg_dbg_bat_car);

static ssize_t show_FG_g_fg_dbg_bat_qmax(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] g_fg_dbg_bat_qmax : %d\n",
		g_fg_dbg_bat_qmax);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_qmax);
}

static ssize_t store_FG_g_fg_dbg_bat_qmax(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_qmax, 0664,
	show_FG_g_fg_dbg_bat_qmax, store_FG_g_fg_dbg_bat_qmax);

static ssize_t show_FG_g_fg_dbg_d0(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] g_fg_dbg_d0 : %d\n", g_fg_dbg_d0);
	return sprintf(buf, "%d\n", g_fg_dbg_d0);
}

static ssize_t store_FG_g_fg_dbg_d0(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_d0, 0664,
	show_FG_g_fg_dbg_d0, store_FG_g_fg_dbg_d0);

static ssize_t show_FG_g_fg_dbg_d1(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] g_fg_dbg_d1 : %d\n", g_fg_dbg_d1);
	return sprintf(buf, "%d\n", g_fg_dbg_d1);
}

static ssize_t store_FG_g_fg_dbg_d1(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_d1, 0664,
	show_FG_g_fg_dbg_d1, store_FG_g_fg_dbg_d1);

static ssize_t show_FG_g_fg_dbg_percentage(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] g_fg_dbg_percentage : %d\n",
		g_fg_dbg_percentage);
	return sprintf(buf, "%d\n", g_fg_dbg_percentage);
}

static ssize_t store_FG_g_fg_dbg_percentage(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_percentage, 0664,
	show_FG_g_fg_dbg_percentage, store_FG_g_fg_dbg_percentage);

static ssize_t show_FG_g_fg_dbg_percentage_fg(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] g_fg_dbg_percentage_fg : %d\n",
		g_fg_dbg_percentage_fg);
	return sprintf(buf, "%d\n", g_fg_dbg_percentage_fg);
}

static ssize_t store_FG_g_fg_dbg_percentage_fg(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_percentage_fg, 0664,
	show_FG_g_fg_dbg_percentage_fg, store_FG_g_fg_dbg_percentage_fg);

static ssize_t
show_FG_g_fg_dbg_percentage_voltmode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FG] g_fg_dbg_percentage_voltmode : %d\n",
		g_fg_dbg_percentage_voltmode);
	return sprintf(buf, "%d\n", g_fg_dbg_percentage_voltmode);
}

static ssize_t
store_FG_g_fg_dbg_percentage_voltmode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_percentage_voltmode, 0664,
	show_FG_g_fg_dbg_percentage_voltmode,
	store_FG_g_fg_dbg_percentage_voltmode);

static ssize_t show_battery_vendor_name(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_notice("Battery Vendor: %s\n", bat_vendor);
	return sprintf(buf, "%s\n", bat_vendor);
}

static DEVICE_ATTR(Battery_Vendor_Name, 0444,
	show_battery_vendor_name, NULL);
/* ============================================================ // */
static int battery_meter_probe(struct platform_device *dev)
{
	int ret_device_file;
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	char *temp_strptr;
#endif

	pr_notice("[battery_meter_probe] probe\n");

	batt_meter_init_cust_data();

	/* select battery meter control method */
	battery_meter_ctrl = bm_ctrl_cmd;
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	if (get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT
	    || get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT) {
		temp_strptr =
		    kzalloc(strlen(saved_command_line) +
			strlen(" androidboot.mode=charger") + 1, GFP_KERNEL);
		strncpy(temp_strptr, saved_command_line,
			strlen(saved_command_line));
		strncat(temp_strptr, " androidboot.mode=charger",
			strlen(" androidboot.mode=charger"));
		saved_command_line = temp_strptr;
	}
#endif

	/* last_oam_run_time = rtc_read_hw_time(); */
	get_monotonic_boottime(&last_oam_run_time);
	/* Create File For FG UI DEBUG */
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Current);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_volt);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_instant_volt);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_current);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_zcv);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_temp);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_r);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_car);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_qmax);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_d0);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_d1);
	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_FG_g_fg_dbg_percentage);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_g_fg_dbg_percentage_fg);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_g_fg_dbg_percentage_voltmode);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_Battery_Vendor_Name);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_g_slp_current);

#if defined(CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT)
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_Battery_Cycle);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_Aging_Factor);
	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_FG_Max_Battery_Voltage);
	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_FG_Min_Battery_Voltage);
	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_FG_Max_Battery_Current);
	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_FG_Min_Battery_Current);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_Max_Battery_Temperature);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_Min_Battery_Temperature);
#endif

	return 0;
}

static int battery_meter_remove(struct platform_device *dev)
{
	pr_notice("[battery_meter_remove]\n");
	return 0;
}

static void battery_meter_shutdown(struct platform_device *dev)
{
	pr_notice("[battery_meter_shutdown]\n");
}

static int battery_meter_suspend(
	struct platform_device *dev, pm_message_t state)
{
	/* -- hibernation path */
	if (state.event == PM_EVENT_FREEZE) {
		pr_warn("[%s] %p:%p\n", __func__,
			battery_meter_ctrl, &bm_ctrl_cmd);
		battery_meter_ctrl = bm_ctrl_cmd;
	}
	/* -- end of hibernation path */
#if defined(CONFIG_POWER_EXT)
#elif defined(SOC_BY_SW_FG) || defined(SOC_BY_HW_FG)
	{
#ifdef MTK_POWER_EXT_DETECT
		if (true == bat_is_ext_power())
			return 0;
#endif
		get_monotonic_boottime(&xts_before_sleep);
		if (!g_skip_reset_sleep_time) {
			get_monotonic_boottime(&g_rtc_time_before_sleep);
			pr_info("%s: reset g_rtc_time_before_sleep\n",__func__);
		}

		if (_g_bat_sleep_total_time >= g_spm_timer)
			_g_bat_sleep_total_time = 0;

		battery_meter_ctrl(
			BATTERY_METER_CMD_GET_HW_OCV, &g_hw_ocv_before_sleep);
	}
#endif

	pr_debug("[battery_meter_suspend]\n");

	return 0;
}

static int battery_meter_resume(struct platform_device *dev)
{
#if defined(CONFIG_POWER_EXT)

#elif defined(SOC_BY_SW_FG) || defined(SOC_BY_HW_FG)
#if defined(SOC_BY_SW_FG)
	signed int hw_ocv_after_sleep = -1;
	signed int DOD_hwocv, sleep_interval;
	struct timespec now_time;
#endif
	struct timespec rtc_time_after_sleep;
#ifdef MTK_POWER_EXT_DETECT
	if (bat_is_ext_power() == true)
		return 0;
#endif

	get_monotonic_boottime(&rtc_time_after_sleep);
	sleep_interval =
		rtc_time_after_sleep.tv_sec - g_rtc_time_before_sleep.tv_sec;
	if (sleep_interval <= g_slp_time_min) {
		g_skip_reset_sleep_time = true;
	} else {
		g_skip_reset_sleep_time = false;
		_g_bat_sleep_total_time += sleep_interval;
		pr_debug("[%s] sleep interval=%d, sleep time=%d, g_spm_timer=%d\n",
			__func__, sleep_interval,_g_bat_sleep_total_time,
			g_spm_timer);
	}

	/* trigger gauge update if accumulated
	 * sleep time more than give period
	 */
	if (_g_bat_sleep_total_time >= g_spm_timer) {
		bat_spm_timeout = true;
		pr_warn("[%s] trigger oam_run(), sleep time=%d, g_spm_timer=%d\n",
			__func__, _g_bat_sleep_total_time, g_spm_timer);
	}

#if defined(SOC_BY_SW_FG)

	/* trigger gauge update if oam_run()
	 * not run in the last 30s kernel active time
	 */
	getrawmonotonic(&now_time);
	if (now_time.tv_sec - last_oam_run_time.tv_sec > 30) {
		bat_spm_timeout = true;
		pr_warn("[battery_meter] trigger oam_run() for 30s threshold.\n");
	}

	/* try to calibrate D0 by HWOCV
	 * if battery has no loading for
	 * more than 30mins
	 */
	if (sleep_interval > 1800 && bat_is_charger_exist() == false) {

		battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV,
			&hw_ocv_after_sleep);

		DOD_hwocv = fgauge_read_d_by_v(hw_ocv_after_sleep);

		if (hw_ocv_after_sleep < g_hw_ocv_before_sleep) {
			oam_d0 = DOD_hwocv;
			oam_v_ocv_2 = oam_v_ocv_1 = hw_ocv_after_sleep;
			oam_car_1 = 0;
			oam_car_2 = 0;

			pr_notice(
				"[self-discharge check] reset to HWOCV. dod_ocv(%d) dod_now(%d)\n",
				DOD_hwocv, oam_d_2);
		} else {
			/* 0.1mAh */
			oam_car_1 = oam_car_1 + (g_slp_current*10*sleep_interval/3600);
			/* 0.1mAh */
			oam_car_2 = oam_car_2 + (g_slp_current*10*sleep_interval/3600);
		}

		pr_notice("[self-discharge check] dod_ocv(%d) dod_now(%d)\n",
			DOD_hwocv, oam_d_2);
	} else {
		/* 0.1mAh */
		oam_car_1 = oam_car_1 + (g_slp_current*10*sleep_interval/3600);
		/* 0.1mAh */
		oam_car_2 = oam_car_2 + (g_slp_current*10*sleep_interval/3600);
	}

	pr_warn("%s: sleeptime=(%d)s, interval(%d)s, skip_reset_sleeptime(%d), be_ocv=(%d), af_ocv=(%d), D0=(%d), car1=(%d), car2=(%d)\n",
		__func__, _g_bat_sleep_total_time, sleep_interval,
		g_skip_reset_sleep_time, g_hw_ocv_before_sleep,
		hw_ocv_after_sleep, oam_d0, oam_car_1, oam_car_2);
#endif
#endif

	pr_debug("[battery_meter_resume]\n");
	return 0;
}

/* ----------------------------------------------------- */

#ifdef CONFIG_OF
static const struct of_device_id mt_bat_meter_of_match[] = {
	{.compatible = "mediatek,bat_meter",},
	{},
};

MODULE_DEVICE_TABLE(of, mt_bat_meter_of_match);
#endif
struct platform_device battery_meter_device = {
	.name = "battery_meter",
	.id = -1,
};


static struct platform_driver battery_meter_driver = {
	.probe = battery_meter_probe,
	.remove = battery_meter_remove,
	.shutdown = battery_meter_shutdown,
	.suspend = battery_meter_suspend,
	.resume = battery_meter_resume,
	.driver = {
		.name = "battery_meter",
	},
};

static int wifi_notifier_call(struct notifier_block *nb,
					 unsigned long state, void *ptr)
{
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);
	int is_carrier_ok = 0;

	if (!dev)
		return NOTIFY_DONE;

	if (strcmp(dev->name, "wlan0"))
		return NOTIFY_DONE;

	switch (state) {
	case NETDEV_CHANGE:
		is_carrier_ok = netif_carrier_ok(dev);
		if (is_carrier_ok ^ g_wifi_on) {
			g_wifi_on = is_carrier_ok;
			if (g_wifi_on)
				g_slp_current = batt_meter_cust_data.slp_current_wifi;
			else
				g_slp_current = batt_meter_cust_data.slp_current;

			bat_metrics_slp_current(g_slp_current);
			/* Get minimum time slice of sleep time for rounded error */
			g_slp_time_min = (3600 / (g_slp_current * 10)) + 5;
			pr_info("%s: is_carrier_ok=%d g_slp_current=%d g_slp_time_min=%d\n",
					__func__, is_carrier_ok,
					g_slp_current, g_slp_time_min);
		}
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}


static struct notifier_block wifi_notifier = {
	.notifier_call = wifi_notifier_call,
};


static int battery_meter_dts_probe(struct platform_device *dev)
{
	int ret;
	pr_notice("******** battery_meter_dts_probe!! ********\n");

	if (!dev->dev.of_node) {
		dev_err(&dev->dev, "Only DT based supported\n");
		return -ENODEV;
	}

#ifdef CONFIG_MTK_MULTI_BAT_PROFILE_SUPPORT
	g_bat_id_iio_channel = iio_channel_get_all(&dev->dev);
	if (IS_ERR(g_bat_id_iio_channel)) {
		ret = PTR_ERR(g_bat_id_iio_channel);
		dev_err(&dev->dev, "IIO channel not found: %d\n", ret);
		return -EPROBE_DEFER;
	}
#endif

	battery_meter_device.dev.of_node = dev->dev.of_node;
	ret = platform_device_register(&battery_meter_device);
	if (ret) {
		pr_notice("[battery_meter_dts_probe] Unable to register device (%d)\n",
			ret);
		return ret;
	}

	ret = register_netdevice_notifier(&wifi_notifier);
	if (ret) {
		pr_notice("register_netdevice_notifier fail (%d)\n", ret);
		return ret;
	}
	return 0;

}

static int battery_meter_dts_remove(struct platform_device *dev)
{
	unregister_netdevice_notifier(&wifi_notifier);
	return 0;
}


static struct platform_driver battery_meter_dts_driver = {
	.probe = battery_meter_dts_probe,
	.remove = battery_meter_dts_remove,
	.shutdown = NULL,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		.name = "battery_meter_dts",
#ifdef CONFIG_OF
		.of_match_table = mt_bat_meter_of_match,
#endif
	},
};

static int __init battery_meter_init(void)
{
	int ret;

#ifdef CONFIG_OF
	/*  */
#else
	ret = platform_device_register(&battery_meter_device);
	if (ret) {
		pr_notice("[battery_meter_driver] Unable to device register(%d)\n",
			 ret);
		return ret;
	}
#endif

	ret = platform_driver_register(&battery_meter_driver);
	if (ret) {
		pr_notice("[battery_meter_driver] Unable to register driver (%d)\n",
			 ret);
		return ret;
	}
#ifdef CONFIG_OF
	ret = platform_driver_register(&battery_meter_dts_driver);
#endif
	pr_notice("[battery_meter_driver] Initialization : DONE\n");

	return 0;

}
#ifdef BATTERY_MODULE_INIT
/* #if 0 */
/* late_initcall(battery_meter_init); */
device_initcall(battery_meter_init);
#else
static void __exit battery_meter_exit(void)
{
}
module_init(battery_meter_init);
/* module_exit(battery_meter_exit); */
#endif

MODULE_AUTHOR("James Lo");
MODULE_DESCRIPTION("Battery Meter Device Driver");
MODULE_LICENSE("GPL");
