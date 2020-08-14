/*
 * Copyright (C) 2015-2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/version.h>
#include <linux/iio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/uidgid.h>
#include <linux/uaccess.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mt_thermal.h"
#include "mtk_ts_cpu.h"

#ifdef CONFIG_THERMAL_SHUTDOWN_LAST_KMESG
#include <linux/thermal_framework.h>
#endif

#ifdef CONFIG_AMAZON_SIGN_OF_LIFE
#include <linux/sign_of_life.h>
#endif

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#define TSBTS_METRICS_STR_LEN 128
#endif

static bool gadc_ready = false;

int __attribute__ ((weak))
IMM_IsAdcInitReady(void)
{
	pr_err("E_WF: %s doesn't exist\n", __func__);
	return 0;
}
int __attribute__ ((weak))
IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata)
{
	pr_err("E_WF: %s doesn't exist\n", __func__);
	return -1;
}

struct BTS_TEMPERATURE {
	signed int BTS_Temp;
	signed int TemperatureR;
};
/* BTS_TEMPERATURE;*/

#ifdef CONFIG_THERMAL_abc123
#define NTC_TBL_SIZE_ERROR -200
static int ntc_tbl_size = 0;
static struct BTS_TEMPERATURE *BTS_Temperature_Table;
extern void mtkts_bts_prepare_table(int table_num);
#endif
struct mtkts_bts_channel_param {
	int g_RAP_pull_up_R;
	int g_TAP_over_critical_low;
	int g_RAP_pull_up_voltage;
	int g_RAP_ntc_table;
	int g_RAP_ADC_channel;
	int g_AP_TemperatureR;
	char *channelName;
};

#if defined(CONFIG_THERMAL_abc123)
#include "inc/mtk_ts_board_abc123.h"
#elif defined(CONFIG_THERMAL_abc123)
#include "inc/mtk_ts_board_abc123.h"
#elif defined(CONFIG_THERMAL_abh123)
#include "inc/mtk_ts_board_abh123.h"
#elif defined(CONFIG_THERMAL_rbc123)
#include "inc/mtk_ts_board_rbc123.h"
#elif defined(CONFIG_THERMAL_abc123)
#include "inc/mtk_ts_board_abh123.h"
#elif defined(CONFIG_THERMAL_abg123)
#include "inc/mtk_ts_board_abg123.h"
#elif defined(CONFIG_THERMAL_KARNAK)
#include "inc/mtk_ts_board_karnak.h"
#elif defined(CONFIG_THERMAL_abc123)
#include "inc/mtk_ts_board_abc123.h"
#elif defined(CONFIG_THERMAL_abc123)
#include "inc/mtk_ts_board_abc123.h"
#else
#include "inc/mtk_ts_board_abf123.h"
#endif

struct gadc_thermal_info {
	struct device *dev;
	struct iio_channel *channel;
};
static struct gadc_thermal_info *gti_ntc;
static int gadc_thermal_get_temp(void *data, int index, int *temp);

struct proc_dir_entry *mtk_thermal_get_proc_drv_therm_dir_entry(void);

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static unsigned int interval;	/* seconds, 0 : no auto polling */
static int trip_temp[10] = {
	120000, 110000, 100000, 90000, 80000, 70000, 65000, 60000, 55000, 50000
};

static struct thermal_zone_device *thz_dev[AUX_CHANNEL_NUM];
static int mtkts_bts_debug_log;
static int kernelmode;
static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int num_trip;
static char g_bind0[20] = { 0 };
static char g_bind1[20] = { 0 };
static char g_bind2[20] = { 0 };
static char g_bind3[20] = { 0 };
static char g_bind4[20] = { 0 };
static char g_bind5[20] = { 0 };
static char g_bind6[20] = { 0 };
static char g_bind7[20] = { 0 };
static char g_bind8[20] = { 0 };
static char g_bind9[20] = { 0 };

/**
 * If curr_temp >= polling_trip_temp1, use interval
 * else if cur_temp >= polling_trip_temp2 && curr_temp < polling_trip_temp1,
 *      use interval*polling_factor1
 * else, use interval*polling_factor2
 */
#if 0
static int polling_trip_temp1 = 40000;
static int polling_trip_temp2 = 20000;
static int polling_factor1 = 5000;
static int polling_factor2 = 10000;
#endif

int bts_cur_temp;

#define MTKTS_BTS_TEMP_CRIT 60000	/* 60.000 degree Celsius */

/* 1: turn on arbitration reasonable temo; 0: turn off */
#define AUTO_ARBITRATION_REASONABLE_TEMP (0)


#if AUTO_ARBITRATION_REASONABLE_TEMP
#define XTAL_BTS_TEMP_DIFF 10000	/* 10 degree */

#endif

#define mtkts_bts_dprintk(fmt, args...)   \
do {                                    \
	if (mtkts_bts_debug_log) {                \
		pr_debug("Power/BTS_Thermal" fmt, ##args); \
	}                                   \
} while (0)


/* #define INPUT_PARAM_FROM_USER_AP */

/*
 * kernel fopen/fclose
 */
/*
 *static mm_segment_t oldfs;
 *
 *static void my_close(int fd)
 *{
 *	set_fs(oldfs);
 *	sys_close(fd);
 *}
 *
 *static int my_open(char *fname, int flag)
 *{
 *	oldfs = get_fs();
 *    set_fs(KERNEL_DS);
 *    return sys_open(fname, flag, 0);
 *}
 */

/* convert register to temperature  */
static signed short mtkts_bts_thermistor_conver_temp(signed int Res)
{
	int i = 0;
	int asize = 0;
	signed int RES1 = 0, RES2 = 0;
	signed int TAP_Value = -200, TMP1 = 0, TMP2 = 0;

#ifdef CONFIG_THERMAL_abc123
	if (ntc_tbl_size <= 0) {
		pr_err("ntc_tbl_size value %d error\n",ntc_tbl_size);
		return NTC_TBL_SIZE_ERROR;
	} else {
		asize = (ntc_tbl_size/sizeof(struct BTS_TEMPERATURE));
	}
#else
	asize = (sizeof(BTS_Temperature_Table) /
		sizeof(struct BTS_TEMPERATURE));
#endif
	/* xlog_printk(ANDROID_LOG_INFO, "Power/BTS_Thermal",
	 * "mtkts_bts_thermistor_conver_temp() :
	 * asize = %d, Res = %d\n",asize,Res);
	 */
	if (Res >= BTS_Temperature_Table[0].TemperatureR) {
		TAP_Value = -40;	/* min */
	} else if (Res <= BTS_Temperature_Table[asize - 1].TemperatureR) {
		TAP_Value = 125;	/* max */
	} else {
		RES1 = BTS_Temperature_Table[0].TemperatureR;
		TMP1 = BTS_Temperature_Table[0].BTS_Temp;
		/* xlog_printk(ANDROID_LOG_INFO, "Power/BTS_Thermal",
		 *  "%d : RES1 = %d,TMP1 = %d\n",__LINE__,RES1,TMP1);
		 */

		for (i = 0; i < asize; i++) {
			if (Res >= BTS_Temperature_Table[i].TemperatureR) {
				RES2 = BTS_Temperature_Table[i].TemperatureR;
				TMP2 = BTS_Temperature_Table[i].BTS_Temp;
			/* xlog_printk(ANDROID_LOG_INFO, "Power/BTS_Thermal",
			 *		"%d :i=%d, RES2 = %d,TMP2 = %d\n",
			 *	__LINE__,i,RES2,TMP2);
			 */
				break;
			}
			RES1 = BTS_Temperature_Table[i].TemperatureR;
			TMP1 = BTS_Temperature_Table[i].BTS_Temp;
			/* xlog_printk(ANDROID_LOG_INFO, "Power/BTS_Thermal",
			 *		"%d :i=%d, RES1 = %d,TMP1 = %d\n",
			 *		__LINE__,i,RES1,TMP1);
			 */
		}

		TAP_Value = (((Res - RES2) * TMP1) + ((RES1 - Res) * TMP2)) /
								(RES1 - RES2);
	}

	return TAP_Value;
}

/* convert ADC_AP_temp_volt to register */
/*Volt to Temp formula same with 6589*/
static signed short mtk_ts_bts_volt_to_temp(int index, unsigned int dwVolt)
{
	signed int TRes;
	signed int dwVCriAP = 0;
	signed int BTS_TMP = -100;

	/* SW workaround----------------------------------------------------- */
	/* dwVCriAP = (TAP_OVER_CRITICAL_LOW * 1800) /
	 * (TAP_OVER_CRITICAL_LOW + 39000);
	 */
	/* dwVCriAP = (TAP_OVER_CRITICAL_LOW * RAP_PULL_UP_VOLT) /
	 * (TAP_OVER_CRITICAL_LOW + RAP_PULL_UP_R);
	 */
	dwVCriAP =
	    (bts_channel_param[index].g_TAP_over_critical_low *
		bts_channel_param[index].g_RAP_pull_up_voltage) /
			(bts_channel_param[index].g_TAP_over_critical_low +
		bts_channel_param[index].g_RAP_pull_up_R);

	if (dwVolt > dwVCriAP) {
		TRes = bts_channel_param[index].g_TAP_over_critical_low;
	} else {
		/* TRes = (39000*dwVolt) / (1800-dwVolt); */
		/* TRes = (RAP_PULL_UP_R*dwVolt) / (RAP_PULL_UP_VOLT-dwVolt); */
		TRes = (bts_channel_param[index].g_RAP_pull_up_R * dwVolt) /
		(bts_channel_param[index].g_RAP_pull_up_voltage - dwVolt);
	}
	/* ------------------------------------------------------------------ */
	mtkts_bts_dprintk("index=%d, dwVCriAP=%d, TRes=%d\n",
		index, dwVCriAP, TRes);
	bts_channel_param[index].g_AP_TemperatureR = TRes;
#ifdef CONFIG_THERMAL_abc123
	mtkts_bts_prepare_table(bts_channel_param[index].g_RAP_ntc_table);
#endif

	/* convert register to temperature */
	BTS_TMP = mtkts_bts_thermistor_conver_temp(TRes);

	return BTS_TMP;
}

static DEFINE_MUTEX(BTS_lock);
int ts_AP_at_boot_time;
int mtkts_bts_get_hw_temp(int index)
{
	int t_ret = 0;
/* static int AP[60]={0}; */
/* int i=0; */

	mutex_lock(&BTS_lock);

	/* get HW AP temp (TSAP) */
	/* cat /sys/class/power_supply/AP/AP_temp */
	/*use upstream adc api*/
	gadc_thermal_get_temp(gti_ntc, index, &t_ret);
	t_ret = t_ret * 1000;

	mutex_unlock(&BTS_lock);

	bts_cur_temp = t_ret;

	if (t_ret > 60000)	/* abnormal high temp */
		pr_debug("[Power/BTS_Thermal] T_AP=%d\n", t_ret);

	mtkts_bts_dprintk("[mtkts_bts_get_hw_temp] T_AP, %d\n", t_ret);
	return t_ret;
}

static int mtkts_bts_get_temp(struct thermal_zone_device *thermal,
		int *t)
{
	int i = thermal->type[strlen(thermal->type) - 1] - '0';

	mtkts_bts_dprintk("[mtkts_bts_get_temp]index = %d\n", i);

	if (i < 0 || i > AUX_CHANNEL_NUM) {
		pr_err("%s bad channel index %d, name=%s\n",
			__func__, i, thermal->type);
		return -EINVAL;
	}
	*t = mtkts_bts_get_hw_temp(i);

	/* if ((int) *t > 52000) */
	/* xlog_printk(ANDROID_LOG_INFO, "Power/BTS_Thermal",
	 *	"T=%d\n", (int) *t);
	 */
#if 0
	if ((int)*t >= polling_trip_temp1)
		thermal->polling_delay = interval * 1000;
	else if ((int)*t < polling_trip_temp2)
		thermal->polling_delay = interval * polling_factor2;
	else
		thermal->polling_delay = interval * polling_factor1;
#endif
	return 0;
}

static int mtkts_bts_bind(struct thermal_zone_device *thermal,
		struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else {
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtkts_bts_dprintk(
		"[mtkts_bts_bind] error binding cooling dev\n");
		return -EINVAL;
	}
	mtkts_bts_dprintk("[mtkts_bts_bind] binding OK, %d\n", table_val);

	return 0;
}

static int mtkts_bts_unbind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtkts_bts_dprintk(
			"[mtkts_bts_unbind]error unbinding cooling dev\n");
		return -EINVAL;
	}
	mtkts_bts_dprintk("[mtkts_bts_unbind] unbinding OK\n");

	return 0;
}

static int mtkts_bts_get_mode(struct thermal_zone_device *thermal,
		enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtkts_bts_set_mode(struct thermal_zone_device *thermal,
		enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtkts_bts_get_trip_type(struct thermal_zone_device *thermal,
		int trip, enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtkts_bts_get_trip_temp(struct thermal_zone_device *thermal,
		int trip, int *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtkts_bts_set_trip_temp(struct thermal_zone_device *thermal,
		int trip, int temp)
{
	trip_temp[trip] = temp;
	return 0;
}

static int mtkts_bts_get_crit_temp(struct thermal_zone_device *thermal,
		int *temperature)
{
	*temperature = MTKTS_BTS_TEMP_CRIT;
	return 0;
}

#define PREFIX "thermaltsbts:def"
static int mtkts_bts_thermal_notify(struct thermal_zone_device *thermal,
		int trip, enum thermal_trip_type type)
{
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[TSBTS_METRICS_STR_LEN];
#endif

#ifdef CONFIG_AMAZON_SIGN_OF_LIFE
	if (type == THERMAL_TRIP_CRITICAL) {
		pr_err("[%s] Thermal shutdown bts, temp=%d, trip=%d\n",
				__func__, thermal->temperature, trip);
		life_cycle_set_thermal_shutdown_reason(
		THERMAL_SHUTDOWN_REASON_BTS);
	}
#endif

#ifdef CONFIG_THERMAL_SHUTDOWN_LAST_KMESG
	if (type == THERMAL_TRIP_CRITICAL) {
		pr_err("%s: thermal_shutdown notify\n", __func__);
		last_kmsg_thermal_shutdown();
		pr_err("%s: thermal_shutdown notify end\n", __func__);
	}
#endif

#ifdef CONFIG_AMAZON_METRICS_LOG
	if (type == THERMAL_TRIP_CRITICAL &&
		snprintf(buf, TSBTS_METRICS_STR_LEN,
			"%s:tsbtsmonitor;CT;1,temp=%d;trip=%d;CT;1:NR",
			PREFIX, thermal->temperature, trip) > 0)
		log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", buf);

#endif
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtkts_BTS_dev_ops = {
	.bind = mtkts_bts_bind,
	.unbind = mtkts_bts_unbind,
	.get_temp = mtkts_bts_get_temp,
	.get_mode = mtkts_bts_get_mode,
	.set_mode = mtkts_bts_set_mode,
	.get_trip_type = mtkts_bts_get_trip_type,
	.get_trip_temp = mtkts_bts_get_trip_temp,
	.set_trip_temp = mtkts_bts_set_trip_temp,
	.get_crit_temp = mtkts_bts_get_crit_temp,
	.notify = mtkts_bts_thermal_notify,
};


/* int  mtkts_AP_register_cooler(void) */
/* { */
	/* cooling devices */
	/* cl_dev_sysrst = mtk_thermal_cooling_device_register( */
	/*	"mtktsAPtery-sysrst", NULL, */
	/* &mtkts_AP_cooling_sysrst_ops); */
	/* return 0; */
/* } */

int mtkts_bts_register_thermal(void)
{
	int i;

	mtkts_bts_dprintk("[mtkts_bts_register_thermal]\n");

	/* trips : trip 0~1 */
	for (i = 0; i < AUX_CHANNEL_NUM; i++) {
		thz_dev[i] = mtk_thermal_zone_device_register(
		bts_channel_param[i].channelName, num_trip, NULL,
		&mtkts_BTS_dev_ops, 0, 0, 0, interval * 1000);
	}

	return 0;
}

/* void mtkts_AP_unregister_cooler(void) */
/* { */
	/* if (cl_dev_sysrst) { */
	/* mtk_thermal_cooling_device_unregister(cl_dev_sysrst); */
	/* cl_dev_sysrst = NULL; */
	/* } */
/* } */
void mtkts_bts_unregister_thermal(void)
{
	int i;

	mtkts_bts_dprintk("[mtkts_bts_unregister_thermal]\n");

	for (i = 0; i < AUX_CHANNEL_NUM; i++) {
		if (thz_dev[i]) {
			mtk_thermal_zone_device_unregister(thz_dev[i]);
			thz_dev[i] = NULL;
		}
	}
}


static int mtkts_bts_read(struct seq_file *m, void *v)
{

	seq_printf(m, "[mtkts_bts_read] trip_0_temp=%d,trip_1_temp=%d,",
trip_temp[0], trip_temp[1]);
	seq_printf(m, "trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d,\n",
trip_temp[2], trip_temp[3], trip_temp[4]);
	seq_printf(m, "trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,",
trip_temp[5], trip_temp[6], trip_temp[7]);
	seq_printf(m, "trip_8_temp=%d,trip_9_temp=%d,\n",
trip_temp[8], trip_temp[9]);
	seq_printf(m, "g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,",
g_THERMAL_TRIP[0], g_THERMAL_TRIP[1], g_THERMAL_TRIP[2]);
	seq_printf(m, "g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,\n",
g_THERMAL_TRIP[3], g_THERMAL_TRIP[4]);
	seq_printf(m, "g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,",
g_THERMAL_TRIP[5], g_THERMAL_TRIP[6], g_THERMAL_TRIP[7]);
	seq_printf(m, "g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);
	seq_printf(m, "cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\n",
g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);
	seq_printf(m, "cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s,time_ms=%d\n",
g_bind5, g_bind6, g_bind7, g_bind8, g_bind9, interval * 1000);

	return 0;
}

static ssize_t mtkts_bts_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *data)
{
#if AUTO_ARBITRATION_REASONABLE_TEMP
	int Ap_temp = 0, XTAL_temp = 0, CPU_Tj = 0;
	int AP_XTAL_diff = 0;
#endif
	int len = 0, time_msec = 0;
	int trip[10] = { 0 };
	int t_type[10] = { 0 };
	int i;
	char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
	char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
	char desc[512];



	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (sscanf(desc,
			"%d %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d %d %s %d",
			&num_trip, &trip[0], &t_type[0], bind0, &trip[1],
			&t_type[1], bind1, &trip[2], &t_type[2], bind2,
			&trip[3], &t_type[3], bind3, &trip[4], &t_type[4],
			bind4, &trip[5], &t_type[5], bind5, &trip[6],
			&t_type[6], bind6, &trip[7], &t_type[7], bind7,
			&trip[8], &t_type[8], bind8, &trip[9], &t_type[9],
			bind9, &time_msec) == 32) {
		mtkts_bts_dprintk(
"[mtkts_bts_write]mtkts_bts_unregister_thermal\n");
		mtkts_bts_unregister_thermal();
		if (num_trip < 0 || num_trip > 10)
			return -EINVAL;

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0] =
		g_bind4[0] = g_bind5[0] = g_bind6[0] = g_bind7[0] =
		g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = bind0[i];
			g_bind1[i] = bind1[i];
			g_bind2[i] = bind2[i];
			g_bind3[i] = bind3[i];
			g_bind4[i] = bind4[i];
			g_bind5[i] = bind5[i];
			g_bind6[i] = bind6[i];
			g_bind7[i] = bind7[i];
			g_bind8[i] = bind8[i];
			g_bind9[i] = bind9[i];
		}

		mtkts_bts_dprintk(
		"[mtkts_bts_write] g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,",
		g_THERMAL_TRIP[0], g_THERMAL_TRIP[1]);
		mtkts_bts_dprintk(
		"g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d, g_THERMAL_TRIP_4=%d,",
		g_THERMAL_TRIP[2], g_THERMAL_TRIP[3], g_THERMAL_TRIP[4]);
		mtkts_bts_dprintk(
		"g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,",
		g_THERMAL_TRIP[5], g_THERMAL_TRIP[6], g_THERMAL_TRIP[7]);
		mtkts_bts_dprintk("g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
		g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);
		mtkts_bts_dprintk(
		"[mtkts_bts_write] cooldev0=%s,cooldev1=%s,",
		g_bind0, g_bind1);
		mtkts_bts_dprintk(
		"cooldev2=%s,cooldev3=%s,cooldev4=%s,cooldev5=%s,",
		g_bind2, g_bind3, g_bind4, g_bind5);
		mtkts_bts_dprintk(
		"cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
		g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = trip[i];

		interval = time_msec / 1000;

		mtkts_bts_dprintk(
		"[mtkts_bts_write] trip_0_temp=%d,trip_1_temp=%d,",
		trip_temp[0], trip_temp[1]);
		mtkts_bts_dprintk(
		"trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d,",
		trip_temp[2], trip_temp[3], trip_temp[4]);
		mtkts_bts_dprintk(
		"trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,",
		trip_temp[5], trip_temp[6], trip_temp[7]);
		mtkts_bts_dprintk(
		"trip_8_temp=%d,trip_9_temp=%d,time_ms=%d\n",
		trip_temp[8], trip_temp[9], time_msec);

		mtkts_bts_dprintk(
"[mtkts_bts_write]mtkts_bts_register_thermal\n");

#if AUTO_ARBITRATION_REASONABLE_TEMP
		/*Thermal will issue "set parameter policy"
		 *than issue "register policy"
		 */
		Ap_temp = mtkts_bts_get_hw_temp();
		XTAL_temp = mtktsxtal_get_xtal_temp();
		pr_debug("[ts_AP]Ap_temp=%d,XTAL_temp=%d,CPU_Tj=%d\n",
		Ap_temp, XTAL_temp, CPU_Tj);


		if (XTAL_temp > Ap_temp)
			AP_XTAL_diff = XTAL_temp - Ap_temp;
		else
			AP_XTAL_diff = Ap_temp - XTAL_temp;

		/* check temp from Tj and Txal */
		if ((Ap_temp < CPU_Tj) &&
			(AP_XTAL_diff <= XTAL_BTS_TEMP_DIFF)) {
			/* pr_debug("AP_XTAL_diff <= 10 degree\n"); */
			mtkts_bts_register_thermal();
		}
#else
		mtkts_bts_register_thermal();
#endif
		/* AP_write_flag=1; */
		return count;
	}
	mtkts_bts_dprintk("[mtkts_bts_write] bad argument\n");

	return -EINVAL;
}


void mtkts_bts_copy_table(struct BTS_TEMPERATURE *des,
	struct BTS_TEMPERATURE *src)
{
	int i = 0;
	int j = 0;

	j = (sizeof(BTS_Temperature_Table) / sizeof(struct BTS_TEMPERATURE));
	for (i = 0; i < j; i++)
		des[i] = src[i];
}

#ifdef CONFIG_THERMAL_abc123
void mtkts_bts_prepare_table(int table_num)
{

	switch (table_num) {
	case 1:
		BTS_Temperature_Table = BTS_Temperature_Table1;
		ntc_tbl_size = sizeof(BTS_Temperature_Table1);
		break;
	case 2:
		BTS_Temperature_Table = BTS_Temperature_Table2;
		ntc_tbl_size = sizeof(BTS_Temperature_Table2);
		break;
	case 3:
		BTS_Temperature_Table = BTS_Temperature_Table3;
		ntc_tbl_size = sizeof(BTS_Temperature_Table3);
		break;
	case 4:
		BTS_Temperature_Table = BTS_Temperature_Table4;
		ntc_tbl_size = sizeof(BTS_Temperature_Table4);
		break;
	case 5:
		BTS_Temperature_Table = BTS_Temperature_Table5;
		ntc_tbl_size = sizeof(BTS_Temperature_Table5);
		break;
	case 6:		/* NCP03WF104F05R */
		BTS_Temperature_Table = BTS_Temperature_Table6;
		ntc_tbl_size = sizeof(BTS_Temperature_Table6);
		break;
	case 7:		/* NCU18XH103F60RB */
		BTS_Temperature_Table = BTS_Temperature_Table7;
		ntc_tbl_size = sizeof(BTS_Temperature_Table7);
		break;
	default:	/* AP_NTC_10 */
		BTS_Temperature_Table = BTS_Temperature_Table4;
		ntc_tbl_size = sizeof(BTS_Temperature_Table4);
		break;
	}
}
#else
void mtkts_bts_prepare_table(int table_num)
{
	switch (table_num) {
	case 1:		/* AP_NTC_BL197 */
		mtkts_bts_copy_table(BTS_Temperature_Table,
		BTS_Temperature_Table1);
		WARN_ON_ONCE(
		sizeof(BTS_Temperature_Table) !=
		sizeof(BTS_Temperature_Table1));
		break;
	case 2:		/* AP_NTC_TSM_1 */
		mtkts_bts_copy_table(BTS_Temperature_Table,
		BTS_Temperature_Table2);
		WARN_ON_ONCE(
		sizeof(BTS_Temperature_Table) !=
		sizeof(BTS_Temperature_Table2));
		break;
	case 3:		/* AP_NTC_10_SEN_1 */
		mtkts_bts_copy_table(BTS_Temperature_Table,
		BTS_Temperature_Table3);
		WARN_ON_ONCE(
		sizeof(BTS_Temperature_Table) !=
		sizeof(BTS_Temperature_Table3));
		break;
	case 4:		/* AP_NTC_10 */
		mtkts_bts_copy_table(BTS_Temperature_Table,
		BTS_Temperature_Table4);
		WARN_ON_ONCE(
		sizeof(BTS_Temperature_Table) !=
		sizeof(BTS_Temperature_Table4));
		break;
	case 5:		/* AP_NTC_47 */
		mtkts_bts_copy_table(BTS_Temperature_Table,
		BTS_Temperature_Table5);
		WARN_ON_ONCE(
		sizeof(BTS_Temperature_Table) !=
		sizeof(BTS_Temperature_Table5));
		break;
	case 6:		/* NTCG104EF104F */
		mtkts_bts_copy_table(BTS_Temperature_Table,
		BTS_Temperature_Table6);
		WARN_ON_ONCE(
		sizeof(BTS_Temperature_Table) !=
		sizeof(BTS_Temperature_Table6));
		break;
	case 7: /*NCP15XH103F03RC */
		mtkts_bts_copy_table(BTS_Temperature_Table,
		BTS_Temperature_Table7);
		WARN_ON_ONCE(
		sizeof(BTS_Temperature_Table) !=
		sizeof(BTS_Temperature_Table7));
		break;
	default:		/* AP_NTC_10 */
		mtkts_bts_copy_table(BTS_Temperature_Table,
		BTS_Temperature_Table4);
		WARN_ON_ONCE(
		sizeof(BTS_Temperature_Table) !=
		sizeof(BTS_Temperature_Table4));
		break;
	}
}
#endif

static int mtkts_bts_param_read(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < AUX_CHANNEL_NUM; i++) {
		seq_printf(m, "%d\t", bts_channel_param[i].g_RAP_pull_up_R);
		seq_printf(m, "%d\t",
		bts_channel_param[i].g_RAP_pull_up_voltage);
		seq_printf(m, "%d\t",
		bts_channel_param[i].g_TAP_over_critical_low);
		seq_printf(m, "%d\t", bts_channel_param[i].g_RAP_ntc_table);
		seq_printf(m, "%d\n", bts_channel_param[i].g_RAP_ADC_channel);
	}

	return 0;
}


static ssize_t mtkts_bts_param_write(struct file *file,
		const char __user *buffer,
		size_t count, loff_t *data)
{
	int len = 0;
	char desc[512];

	char pull_R[10], pull_V[10];
	char overcrilow[16];
	char NTC_TABLE[10];
	unsigned int valR, valV, over_cri_low, ntc_table;
	/* external pin: 0/1/12/13/14/15, can't use pin:2/3/4/5/6/7/8/9/10/11,
	 * choose "adc_channel=11" to check if there is any param input
	 */
	unsigned int adc_channel;
	int i;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	mtkts_bts_dprintk("[mtkts_bts_write]\n");

	if (sscanf(desc, "%s %d %s %d %s %d %s %d %d",
		pull_R, &valR, pull_V, &valV,
		overcrilow, &over_cri_low, NTC_TABLE,
		&ntc_table, &adc_channel) >= 8) {
		pr_debug("[mtkts_bts_write] bad argument\n");
		return -EINVAL;
	}

	for (i = 0; i < AUX_CHANNEL_NUM; i++) {
		if (bts_channel_param[i].g_RAP_ADC_channel == adc_channel)
			break;
	}
	if (i == AUX_CHANNEL_NUM) {
		pr_err("%s bad channel argument %d\n", __func__, adc_channel);
		return -EINVAL;
	}

	if (!strcmp(pull_R, "PUP_R")) {
		bts_channel_param[i].g_RAP_pull_up_R = valR;
	} else {
		pr_err("[mtkts_bts_write] bad PUP_R argument\n");
		return -EINVAL;
	}

	if (!strcmp(pull_V, "PUP_VOLT")) {
		bts_channel_param[i].g_RAP_pull_up_voltage = valV;
	} else {
		pr_err("[mtkts_bts_write] bad PUP_VOLT argument\n");
		return -EINVAL;
	}

	if (!strcmp(overcrilow, "OVER_CRITICAL_L")) {
		bts_channel_param[i].g_TAP_over_critical_low = over_cri_low;
	} else {
		pr_err("[mtkts_bts_write] bad OVERCRIT_L argument\n");
		return -EINVAL;
	}

	if (!strcmp(NTC_TABLE, "NTC_TABLE")) {
		bts_channel_param[i].g_RAP_ntc_table = ntc_table;
	} else {
		pr_err("[mtkts_bts_write] bad NTC_TABLE argument\n");
		return -EINVAL;
	}

	mtkts_bts_prepare_table(bts_channel_param[i].g_RAP_ntc_table);

	return count;
}

static int mtkts_bts_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtkts_bts_read, NULL);
}

static const struct file_operations mtkts_AP_fops = {
	.owner = THIS_MODULE,
	.open = mtkts_bts_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtkts_bts_write,
	.release = single_release,
};


static int mtkts_bts_param_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtkts_bts_param_read, NULL);
}

static const struct file_operations mtkts_AP_param_fops = {
	.owner = THIS_MODULE,
	.open = mtkts_bts_param_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtkts_bts_param_write,
	.release = single_release,
};

static int __init mtkts_bts_init(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtkts_AP_dir = NULL;

	mtkts_bts_dprintk("[mtkts_bts_init]\n");

	/* setup default table */
	mtkts_bts_prepare_table(bts_channel_param[0].g_RAP_ntc_table);

	mtkts_AP_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtkts_AP_dir)
		mtkts_bts_dprintk("[%s]: mkdir /proc/driver/thermal failed\n",
		__func__);
	else {
		entry = proc_create("tzbts",
			0664,
			mtkts_AP_dir,
			&mtkts_AP_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry = proc_create("tzbts_param",
			0664,
			mtkts_AP_dir,
			&mtkts_AP_param_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}

	return 0;
}

static void __exit mtkts_bts_exit(void)
{
	mtkts_bts_dprintk("[mtkts_bts_exit]\n");
	mtkts_bts_unregister_thermal();
	/* mtkts_AP_unregister_cooler(); */
}

#if AUTO_ARBITRATION_REASONABLE_TEMP
late_initcall(mtkts_bts_init);
module_exit(mtkts_bts_exit);
#else
module_init(mtkts_bts_init);
module_exit(mtkts_bts_exit);
#endif

static int gadc_thermal_get_temp(void *data, int index, int *temp)
{
	struct gadc_thermal_info *gti = data;
	struct iio_channel *channel;
	int val;
	int ret;

	if (gti == NULL || !gadc_ready)
		return 0;

	if (index >= AUX_CHANNEL_NUM) {
		dev_err(gti->dev, "%s: error: index:%d > AUX_CHANNEL_NUM:%d\n",
				__func__, index, AUX_CHANNEL_NUM);
		return -EINVAL;
	}

	channel = &gti->channel[index];

	if (IS_ERR(channel)) {
		ret = PTR_ERR(channel);
		pr_info("%s: IIO channel not found: %d\n", __func__, ret);
		return ret;
	}

	ret = iio_read_channel_processed(channel, &val);
	if (ret < 0) {
		dev_err(gti->dev, "IIO channel read failed %d\n", ret);
		return ret;
	}
	val = val * 1500 / 4096;
	*temp = mtk_ts_bts_volt_to_temp(index, val);

	return 0;
}

static int gadc_thermal_probe(struct platform_device *pdev)
{
	struct gadc_thermal_info *gti;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "Only DT based supported\n");
		return -ENODEV;
	}

	gti = devm_kzalloc(&pdev->dev, sizeof(*gti), GFP_KERNEL);
	if (!gti)
		return -ENOMEM;
	gti_ntc = gti;

	gti->dev = &pdev->dev;
	platform_set_drvdata(pdev, gti);

	gti->channel = iio_channel_get_all(&pdev->dev);
	/*gti->channel = iio_channel_get(&pdev->dev, "sensor-channel");*/
	if (IS_ERR(gti->channel)) {
		ret = PTR_ERR(gti->channel);
		dev_err(&pdev->dev, "IIO channel not found: %d\n", ret);
		return ret;
	}

	gadc_ready = true;
	pr_err("%s OK\n", __func__);

	return 0;
}

static int gadc_thermal_remove(struct platform_device *pdev)
{
	struct gadc_thermal_info *gti = platform_get_drvdata(pdev);

	iio_channel_release_all(gti->channel);

	return 0;
}

static const struct of_device_id of_adc_thermal_match[] = {
	{ .compatible = "generic-adc-thermal", },
	{},
};
MODULE_DEVICE_TABLE(of, of_adc_thermal_match);

static struct platform_driver gadc_thermal_driver = {
	.driver = {
		.name = "generic-adc-thermal",
		.of_match_table = of_adc_thermal_match,
	},
	.probe = gadc_thermal_probe,
	.remove = gadc_thermal_remove,
};

module_platform_driver(gadc_thermal_driver);
