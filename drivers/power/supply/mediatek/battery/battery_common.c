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
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#ifdef CONFIG_USB_AMAZON_DOCK
#include <linux/switch.h>
#endif
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include <linux/suspend.h>
#include <linux/reboot.h>


#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/irq.h>

#include <mt-plat/mtk_boot.h>
#include <mt-plat/mtk_rtc.h>

#include <mach/mtk_charging.h>
#include <mt-plat/upmu_common.h>

#include <mt-plat/charging.h>
#include <mt-plat/battery_meter.h>
#include "battery_metrics.h"
#include <mt-plat/battery_common.h>
#include <mach/mtk_battery_meter.h>
#include <mach/mtk_charging.h>

#if defined(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT)
#include <mt-plat/internal_charging.h>
#endif

#ifdef CONFIG_AMAZON_SIGN_OF_LIFE
#include <linux/sign_of_life.h>
#endif

/* ////////////////////////// */
/* // Smart Battery Structure */
/* ////////////////////////// */
PMU_ChargerStruct BMT_status;
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
DISO_ChargerStruct DISO_data;
/* Debug Msg */
static char *DISO_state_s[8] = {
	"IDLE",
	"OTG_ONLY",
	"USB_ONLY",
	"USB_WITH_OTG",
	"DC_ONLY",
	"DC_WITH_OTG",
	"DC_WITH_USB",
	"DC_USB_OTG",
};
#endif

#ifdef CONFIG_USB_AMAZON_DOCK
enum DOCK_STATE_TYPE {
	TYPE_DOCKED = 5,
	TYPE_UNDOCKED = 6,
};
#endif

/*
 * Thermal related flags
 */
int g_battery_thermal_throttling_flag = 1;
/*  0:nothing,
 *	1:enable batTT&chrTimer,
 *	2:disable batTT&chrTimer,
 *	3:enable batTT,
 *	disable chrTimer
 */
int battery_cmd_thermal_test_mode;
int battery_cmd_thermal_test_mode_value;
int g_battery_tt_check_flag;
/* 0:default enable check batteryTT, 1:default disable check batteryTT */


/*
 *  Global Variable
 */

struct wakeup_source *battery_suspend_lock;
struct wakeup_source *battery_fg_lock;
CHARGING_CONTROL battery_charging_control;
unsigned int g_BatteryNotifyCode;
unsigned int g_BN_TestMode;
bool g_bat_init_flag;
unsigned int g_call_state = CALL_IDLE;
bool g_charging_full_reset_bat_meter;
int g_platform_boot_mode;
struct timespec g_bat_time_before_sleep;
int g_smartbook_update;
signed int g_custom_charging_current = -1;
signed int g_custom_rtc_soc = -1;
signed int g_custom_charging_cv = -1;
unsigned int g_custom_charging_mode; /* 0=ratail unit, 1=demo unit */
static unsigned long g_custom_plugin_time;
int g_custom_aicl_input_current;

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
bool g_vcdt_irq_delay_flag;
#endif

bool skip_battery_update;

unsigned int g_batt_temp_status = TEMP_POS_NORMAL;


bool battery_suspended;
struct timespec chr_plug_in_time;
#define PLUGIN_THRESHOLD (7*86400)

#if defined(CUST_SYSTEM_OFF_VOLTAGE)
#define SYSTEM_OFF_VOLTAGE CUST_SYSTEM_OFF_VOLTAGE
#endif

struct battery_custom_data batt_cust_data;

/*
 * Integrate with NVRAM
 */
#define ADC_CALI_DEVNAME "MT_pmic_adc_cali"
#define TEST_ADC_CALI_PRINT _IO('k', 0)
#define SET_ADC_CALI_Slop _IOW('k', 1, int)
#define SET_ADC_CALI_Offset _IOW('k', 2, int)
#define SET_ADC_CALI_Cal _IOW('k', 3, int)
#define ADC_CHANNEL_READ _IOW('k', 4, int)
#define BAT_STATUS_READ _IOW('k', 5, int)
#define Set_Charger_Current _IOW('k', 6, int)
/* add for meta tool----------------------------------------- */
#define Get_META_BAT_VOL _IOW('k', 10, int)
#define Get_META_BAT_SOC _IOW('k', 11, int)
/* add for meta tool----------------------------------------- */

static struct class *adc_cali_class;
static int adc_cali_major;
static dev_t adc_cali_devno;
static struct cdev *adc_cali_cdev;

int adc_cali_slop[14] = {
1000, 1000, 1000, 1000, 1000, 1000, 1000,
1000, 1000, 1000, 1000, 1000, 1000, 1000
};

int adc_cali_offset[14] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int adc_cali_cal[1] = { 0 };
int battery_in_data[1] = { 0 };
int battery_out_data[1] = { 0 };
int charging_level_data[1] = { 0 };

bool g_ADC_Cali;
bool g_ftm_battery_flag;
#if !defined(CONFIG_POWER_EXT)
static int g_wireless_state;
#endif

/*
 *  Thread related
 */
#define BAT_MS_TO_NS(x) (x * 1000 * 1000)
#define RECHARGE_THRES 3 /*the threshold to decrease ui_soc*/
static bool bat_thread_timeout;
static bool chr_wake_up_bat;	/* charger in/out to wake up battery thread */
static bool fg_wake_up_bat;
static bool bat_meter_timeout;
static DEFINE_MUTEX(bat_mutex);
static DEFINE_MUTEX(charger_type_mutex);
static DECLARE_WAIT_QUEUE_HEAD(bat_thread_wq);
static struct hrtimer charger_hv_detect_timer;
static struct task_struct *charger_hv_detect_thread;
static bool charger_hv_detect_flag;
static DECLARE_WAIT_QUEUE_HEAD(charger_hv_detect_waiter);
static struct hrtimer battery_kthread_timer;
static bool g_battery_soc_ready;
static bool fg_battery_shutdown;
static bool fg_bat_thread;
static bool fg_hv_thread;
static bool bat_7days_flag;
static bool bat_demo_flag;

/*
 * FOR ADB CMD
 */
/* Dual battery */
int g_status_smb = POWER_SUPPLY_STATUS_NOT_CHARGING;
int g_capacity_smb = 50;
int g_present_smb;
/* ADB charging CMD */
static int cmd_discharging = -1;
static int adjust_power = -1;
static int suspend_discharging = -1;
static bool is_uisoc_ever_100;

/* ///////////////////////////
 * FOR ANDROID BATTERY SERVICE
 * ///////////////////////////
 */

struct wireless_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	int WIRELESS_ONLINE;
};

struct ac_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	int AC_ONLINE;
	int ac_present;
	int ac_present_old;
	int ac_old_state;
};

struct usb_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	int USB_ONLINE;
	int usb_present;
	int usb_present_old;
	int usb_old_state;
};

struct battery_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	int BAT_STATUS;
	int BAT_HEALTH;
	int BAT_PRESENT;
	int BAT_TECHNOLOGY;
	int BAT_CAPACITY;
#ifdef CONFIG_USB_AMAZON_DOCK
	struct switch_dev dock_state;
#endif
	/* Add for Battery Service */
	int BAT_batt_vol;
	int BAT_batt_temp;
	/* Add for EM */
	int BAT_TemperatureR;
	int BAT_TempBattVoltage;
	int BAT_InstatVolt;
	int BAT_BatteryAverageCurrent;
	int BAT_BatterySenseVoltage;
	int BAT_ISenseVoltage;
	int BAT_ChargerVoltage;

	/* Dual battery */
	int status_smb;
	int capacity_smb;
	int present_smb;
	int adjust_power;
};

static enum power_supply_property wireless_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_MAX,
};

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_MAX,
};

static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	/* Add for Battery Service */
	POWER_SUPPLY_PROP_batt_vol,
	POWER_SUPPLY_PROP_batt_temp,
	POWER_SUPPLY_PROP_TEMP,
	/* Add for EM */
	POWER_SUPPLY_PROP_TemperatureR,
	POWER_SUPPLY_PROP_TempBattVoltage,
	POWER_SUPPLY_PROP_InstatVolt,
	POWER_SUPPLY_PROP_BatteryAverageCurrent,
	POWER_SUPPLY_PROP_BatterySenseVoltage,
	POWER_SUPPLY_PROP_ISenseVoltage,
	POWER_SUPPLY_PROP_ChargerVoltage,
	/* Dual battery */
	POWER_SUPPLY_PROP_status_smb,
	POWER_SUPPLY_PROP_capacity_smb,
	POWER_SUPPLY_PROP_present_smb,
	/* ADB CMD Discharging */
	POWER_SUPPLY_PROP_adjust_power,
#ifdef CONFIG_USB_AMAZON_DOCK
	POWER_SUPPLY_PROP_DOCK_PRESENT,
#endif
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

/*void check_battery_exist(void);*/
void charging_suspend_enable(void)
{
	unsigned int charging_enable = true;

	suspend_discharging = 0;
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
}

void charging_suspend_disable(void)
{
	unsigned int charging_enable = false;

	suspend_discharging = 1;
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
}

int read_tbat_value(void)
{
	return BMT_status.temperature;
}

int get_charger_detect_status(void)
{
	bool chr_status;

	battery_charging_control(
		CHARGING_CMD_GET_CHARGER_DET_STATUS, &chr_status);
	return chr_status;
}

#if defined(CONFIG_MTK_POWER_EXT_DETECT)
bool bat_is_ext_power(void)
{
	bool pwr_src = 0;

	battery_charging_control(CHARGING_CMD_GET_POWER_SOURCE, &pwr_src);
	pr_debug("[BAT_IS_EXT_POWER] is_ext_power = %d\n", pwr_src);
	return pwr_src;
}
#endif

static void get_charging_control(void);
/* /////////////////////////////////////////
 *    PMIC PCHR Related APIs
 * /////////////////////////////////////////
 */
bool __attribute__((weak)) mt_usb_is_device(void)
{
	return 1;
}

bool __attribute__((weak)) is_usb_rdy(void)
{
	pr_notice("[%s] need usb porting\r\n", __func__);
	return true;
}

bool upmu_is_chr_det(void)
{
#if !defined(CONFIG_POWER_EXT)
	unsigned int tmp32;
#endif

	if (battery_charging_control == NULL)
		get_charging_control();

#if defined(CONFIG_POWER_EXT)
	/* return true; */
	return get_charger_detect_status();
#else
	if (suspend_discharging == 1)
		return false;

	tmp32 = get_charger_detect_status();

#ifdef CONFIG_MTK_POWER_EXT_DETECT
	if (true == bat_is_ext_power())
		return tmp32;
#endif

	if (tmp32 == 0)
		return false;
	/*else {*/
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		if (mt_usb_is_device()) {
			pr_debug(
				    "[upmu_is_chr_det] Charger exist and USB is not host\n");

			return true;
		} /*else {*/
			pr_debug(
				    "[upmu_is_chr_det] Charger exist but USB is host\n");

			return false;
		/*}*/
#else
		return true;
#endif
	/*}*/
#endif
}
EXPORT_SYMBOL(upmu_is_chr_det);


void wake_up_bat(void)
{
	pr_debug("[BATTERY] wake_up_bat. \r\n");

	chr_wake_up_bat = true;
	bat_thread_timeout = true;
#ifdef MTK_ENABLE_AGING_ALGORITHM
	suspend_time = 0;
#endif
	_g_bat_sleep_total_time = 0;
	wake_up(&bat_thread_wq);
}
EXPORT_SYMBOL(wake_up_bat);


#ifdef FG_BAT_INT
void wake_up_bat2(void)
{
	pr_debug("[BATTERY] wake_up_bat2. \r\n");

	__pm_stay_awake(battery_fg_lock);
	fg_wake_up_bat = true;
	bat_thread_timeout = true;
#ifdef MTK_ENABLE_AGING_ALGORITHM
	suspend_time = 0;
#endif
	_g_bat_sleep_total_time = 0;
	wake_up(&bat_thread_wq);
}
EXPORT_SYMBOL(wake_up_bat2);
#endif				/* #ifdef FG_BAT_INT */

static int wireless_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;
	struct wireless_data *data =
		container_of(psy->desc, struct wireless_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->WIRELESS_ONLINE;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int ac_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;
	struct ac_data *data = container_of(psy->desc, struct ac_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->AC_ONLINE;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = data->ac_present;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = batt_cust_data.ac_charger_input_current * 10;
		break;

	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;
	struct usb_data *data = container_of(psy->desc, struct usb_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
#if defined(CONFIG_POWER_EXT)
		/* #if 0 */
		data->USB_ONLINE = 1;
		val->intval = data->USB_ONLINE;
#else
#if defined(CONFIG_MTK_POWER_EXT_DETECT)
		if (true == bat_is_ext_power())
			data->USB_ONLINE = 1;
#endif
		val->intval = data->USB_ONLINE;
#endif
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = data->usb_present;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = batt_cust_data.usb_charger_current * 10;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

#ifdef CONFIG_USB_AMAZON_DOCK
static int battery_property_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_DOCK_PRESENT:
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}
#endif

static int battery_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	int ret = 0;
	struct battery_data *data =
		container_of(psy->desc, struct battery_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = data->BAT_STATUS;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = data->BAT_HEALTH;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = data->BAT_PRESENT;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = data->BAT_TECHNOLOGY;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = data->BAT_CAPACITY;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = battery_meter_get_charge_full();
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = battery_meter_get_charge_counter();
		break;
	case POWER_SUPPLY_PROP_batt_vol:
		val->intval = data->BAT_batt_vol;
		break;
	case POWER_SUPPLY_PROP_batt_temp:
		val->intval = data->BAT_batt_temp;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = data->BAT_batt_temp;
		break;
	case POWER_SUPPLY_PROP_TemperatureR:
		val->intval = data->BAT_TemperatureR;
		break;
	case POWER_SUPPLY_PROP_TempBattVoltage:
		val->intval = data->BAT_TempBattVoltage;
		break;
	case POWER_SUPPLY_PROP_InstatVolt:
		val->intval = data->BAT_InstatVolt;
		break;
	case POWER_SUPPLY_PROP_BatteryAverageCurrent:
		val->intval = data->BAT_BatteryAverageCurrent;
		break;
	case POWER_SUPPLY_PROP_BatterySenseVoltage:
		val->intval = data->BAT_BatterySenseVoltage;
		break;
	case POWER_SUPPLY_PROP_ISenseVoltage:
		val->intval = data->BAT_ISenseVoltage;
		break;
	case POWER_SUPPLY_PROP_ChargerVoltage:
		val->intval = data->BAT_ChargerVoltage;
		break;
		/* Dual battery */
	case POWER_SUPPLY_PROP_status_smb:
		val->intval = data->status_smb;
		break;
	case POWER_SUPPLY_PROP_capacity_smb:
		val->intval = data->capacity_smb;
		break;
	case POWER_SUPPLY_PROP_present_smb:
		val->intval = data->present_smb;
		break;
	case POWER_SUPPLY_PROP_adjust_power:
		val->intval = data->adjust_power;
		break;
#ifdef CONFIG_USB_AMAZON_DOCK
	case POWER_SUPPLY_PROP_DOCK_PRESENT:
		val->intval = switch_get_state(&data->dock_state)
			== TYPE_UNDOCKED ? 0 : 1;
		break;
#endif
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = data->BAT_batt_vol;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

#define DOCK_STATE_TYPE 5
static int battery_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
#ifdef CONFIG_USB_AMAZON_DOCK
	int state;
	struct battery_data *data = container_of(psy->desc, struct battery_data, psd);
#endif

	switch (psp) {
#ifdef CONFIG_USB_AMAZON_DOCK
	case POWER_SUPPLY_PROP_DOCK_PRESENT:
		if (val->intval == 0) {
			state = TYPE_UNDOCKED;
			if (g_custom_aicl_input_current == 0) {
				batt_cust_data.aicl_input_current_max
				= batt_cust_data.ap15_charger_input_current_max;
				batt_cust_data.aicl_input_current_min
				= batt_cust_data.ap15_charger_input_current_min;
			}
		} else {
			state = TYPE_DOCKED;
			if (g_custom_aicl_input_current == 0) {
				batt_cust_data.aicl_input_current_max
				= batt_cust_data.ap15_dock_input_current_max;
				batt_cust_data.aicl_input_current_min
				= batt_cust_data.ap15_dock_input_current_min;
			}
		}
		switch_set_state(&data->dock_state, state);
		break;
#endif
	default:
		break;
	}

	return 0;
}

/* wireless_data initialization */
static struct wireless_data wireless_main = {
	.psd = {
		.name = "wireless",
		.type = POWER_SUPPLY_TYPE_WIRELESS,
		.properties = wireless_props,
		.num_properties = ARRAY_SIZE(wireless_props),
		.get_property = wireless_get_property,
		},
	.WIRELESS_ONLINE = 0,
};

/* ac_data initialization */
static struct ac_data ac_main = {
	.psd = {
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.properties = ac_props,
		.num_properties = ARRAY_SIZE(ac_props),
		.get_property = ac_get_property,
		},
	.AC_ONLINE = 0,
	.ac_present = -1,
	.ac_old_state = -1,
};

/* usb_data initialization */
static struct usb_data usb_main = {
	.psd = {
		.name = "usb",
		.type = POWER_SUPPLY_TYPE_USB,
		.properties = usb_props,
		.num_properties = ARRAY_SIZE(usb_props),
		.get_property = usb_get_property,
		},
	.USB_ONLINE = 0,
	.usb_present = -1,
	.usb_old_state = -1,
};

/* battery_data initialization */
static struct battery_data battery_main = {
	.psd = {
		.name = "battery",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = battery_props,
		.num_properties = ARRAY_SIZE(battery_props),
		.get_property = battery_get_property,
		.set_property = battery_set_property,
#ifdef CONFIG_USB_AMAZON_DOCK
		.property_is_writeable = battery_property_is_writeable,
#endif
		},
/* CC: modify to have a full power supply status */
#if defined(CONFIG_POWER_EXT)
	.BAT_STATUS = POWER_SUPPLY_STATUS_FULL,
	.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD,
	.BAT_PRESENT = 1,
	.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION,
	.BAT_CAPACITY = 100,
	.BAT_batt_vol = 4200,
	.BAT_batt_temp = 22,
	/* Dual battery */
	.status_smb = POWER_SUPPLY_STATUS_NOT_CHARGING,
	.capacity_smb = 50,
	.present_smb = 0,
	/* ADB CMD discharging */
	.adjust_power = -1,
#else
	.BAT_STATUS = POWER_SUPPLY_STATUS_NOT_CHARGING,
	.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD,
	.BAT_PRESENT = 1,
	.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION,
	.BAT_CAPACITY = 50,
	.BAT_batt_vol = 0,
	.BAT_batt_temp = 0,
	/* Dual battery */
	.status_smb = POWER_SUPPLY_STATUS_NOT_CHARGING,
	.capacity_smb = 50,
	.present_smb = 0,
	/* ADB CMD discharging */
	.adjust_power = -1,
#endif
};

#if !defined(CONFIG_POWER_EXT)
/* ///////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Charger_Voltage
 * ///////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Charger_Voltage(struct device *dev,
	struct device_attribute *attr,
					char *buf)
{
	pr_debug("[EM] show_ADC_Charger_Voltage : %d\n",
		BMT_status.charger_vol);
	return sprintf(buf, "%d\n", BMT_status.charger_vol);
}

static ssize_t store_ADC_Charger_Voltage(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Charger_Voltage, 0664,
	show_ADC_Charger_Voltage, store_ADC_Charger_Voltage);

/* ///////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_0_Slope
 * ///////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_0_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 0));
	pr_debug("[EM] ADC_Channel_0_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_0_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_0_Slope, 0664,
	show_ADC_Channel_0_Slope, store_ADC_Channel_0_Slope);
/* /////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_1_Slope
 * /////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_1_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 1));
	pr_debug("[EM] ADC_Channel_1_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_1_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_1_Slope, 0664,
	show_ADC_Channel_1_Slope, store_ADC_Channel_1_Slope);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_2_Slope
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_2_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 2));
	pr_debug("[EM] ADC_Channel_2_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_2_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_2_Slope, 0664,
	show_ADC_Channel_2_Slope, store_ADC_Channel_2_Slope);
/* ///////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_3_Slope
 * ///////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_3_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 3));
	pr_debug("[EM] ADC_Channel_3_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_3_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_3_Slope, 0664,
	show_ADC_Channel_3_Slope, store_ADC_Channel_3_Slope);
/* /////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_4_Slope
 * /////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_4_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 4));
	pr_debug("[EM] ADC_Channel_4_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_4_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_4_Slope, 0664,
	show_ADC_Channel_4_Slope, store_ADC_Channel_4_Slope);
/* //////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_5_Slope
 * //////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_5_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 5));
	pr_debug("[EM] ADC_Channel_5_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_5_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_5_Slope, 0664,
	show_ADC_Channel_5_Slope, store_ADC_Channel_5_Slope);
/* ////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_6_Slope
 * ////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_6_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 6));
	pr_debug("[EM] ADC_Channel_6_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_6_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_6_Slope, 0664,
	show_ADC_Channel_6_Slope, store_ADC_Channel_6_Slope);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_7_Slope
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_7_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 7));
	pr_debug("[EM] ADC_Channel_7_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_7_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_7_Slope, 0664,
	show_ADC_Channel_7_Slope, store_ADC_Channel_7_Slope);
/* /////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_8_Slope
 * /////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_8_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 8));
	pr_debug("[EM] ADC_Channel_8_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_8_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_8_Slope, 0664,
	show_ADC_Channel_8_Slope, store_ADC_Channel_8_Slope);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_9_Slope
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_9_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 9));
	pr_debug("[EM] ADC_Channel_9_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_9_Slope(struct device *dev,
	 struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_9_Slope, 0664,
	show_ADC_Channel_9_Slope, store_ADC_Channel_9_Slope);
/* /////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_10_Slope
 * /////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_10_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 10));
	pr_debug("[EM] ADC_Channel_10_Slope : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_10_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_10_Slope, 0664,
	show_ADC_Channel_10_Slope, store_ADC_Channel_10_Slope);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_11_Slope
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_11_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 11));
	pr_debug("[EM] ADC_Channel_11_Slope : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_11_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_11_Slope, 0664,
	show_ADC_Channel_11_Slope, store_ADC_Channel_11_Slope);
/* ////////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_12_Slope
 * ////////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_12_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 12));
	pr_debug("[EM] ADC_Channel_12_Slope : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_12_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_12_Slope, 0664,
	show_ADC_Channel_12_Slope, store_ADC_Channel_12_Slope);
/* ////////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_13_Slope
 * ////////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_13_Slope(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 13));
	pr_debug("[EM] ADC_Channel_13_Slope : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_13_Slope(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_13_Slope, 0664,
	show_ADC_Channel_13_Slope, store_ADC_Channel_13_Slope);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_0_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_0_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 0));
	pr_debug("[EM] ADC_Channel_0_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_0_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_0_Offset, 0664,
	show_ADC_Channel_0_Offset, store_ADC_Channel_0_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_1_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_1_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 1));
	pr_debug("[EM] ADC_Channel_1_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_1_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_1_Offset, 0664,
	show_ADC_Channel_1_Offset, store_ADC_Channel_1_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_2_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_2_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 2));
	pr_debug("[EM] ADC_Channel_2_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_2_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_2_Offset, 0664,
	show_ADC_Channel_2_Offset, store_ADC_Channel_2_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_3_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_3_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 3));
	pr_debug("[EM] ADC_Channel_3_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_3_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_3_Offset, 0664,
	show_ADC_Channel_3_Offset, store_ADC_Channel_3_Offset);
/* ///////////////////////////////////////////////////////////////
 * // Create File For EM : ADC_Channel_4_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_4_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 4));
	pr_debug("[EM] ADC_Channel_4_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_4_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_4_Offset, 0664,
	show_ADC_Channel_4_Offset, store_ADC_Channel_4_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_5_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_5_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 5));
	pr_debug("[EM] ADC_Channel_5_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_5_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_5_Offset, 0664,
	show_ADC_Channel_5_Offset, store_ADC_Channel_5_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_6_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_6_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 6));
	pr_debug("[EM] ADC_Channel_6_Offset : %d\n",
		    ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_6_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_6_Offset, 0664,
	show_ADC_Channel_6_Offset, store_ADC_Channel_6_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_7_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_7_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 7));
	pr_debug("[EM] ADC_Channel_7_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_7_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_7_Offset, 0664,
	show_ADC_Channel_7_Offset, store_ADC_Channel_7_Offset);
/* ///////////////////////////////////////////////////////////////
 *      Create File For EM : ADC_Channel_8_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_8_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 8));
	pr_debug("[EM] ADC_Channel_8_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_8_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_8_Offset, 0664,
	show_ADC_Channel_8_Offset, store_ADC_Channel_8_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_9_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_9_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 9));
	pr_debug("[EM] ADC_Channel_9_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_9_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_9_Offset, 0664,
	show_ADC_Channel_9_Offset, store_ADC_Channel_9_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_10_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_10_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 10));
	pr_debug("[EM] ADC_Channel_10_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%d\n", ret_value);
}

static ssize_t store_ADC_Channel_10_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_10_Offset, 0664,
	show_ADC_Channel_10_Offset, store_ADC_Channel_10_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_11_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_11_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 11));
	pr_debug("[EM] ADC_Channel_11_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%d\n", ret_value);
}

static ssize_t store_ADC_Channel_11_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_11_Offset, 0664,
	show_ADC_Channel_11_Offset, store_ADC_Channel_11_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_12_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_12_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 12));
	pr_debug("[EM] ADC_Channel_12_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_12_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_12_Offset, 0664,
	show_ADC_Channel_12_Offset, store_ADC_Channel_12_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_13_Offset
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_13_Offset(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 13));
	pr_debug("[EM] ADC_Channel_13_Offset : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_13_Offset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_13_Offset, 0664,
	show_ADC_Channel_13_Offset, store_ADC_Channel_13_Offset);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : ADC_Channel_Is_Calibration
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_ADC_Channel_Is_Calibration(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 2;

	ret_value = g_ADC_Cali;
	pr_debug("[EM] ADC_Channel_Is_Calibration : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_Is_Calibration(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_Is_Calibration, 0664,
	show_ADC_Channel_Is_Calibration, store_ADC_Channel_Is_Calibration);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : Power_On_Voltage
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_Power_On_Voltage(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 3400;
	pr_debug("[EM] Power_On_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_On_Voltage(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Power_On_Voltage, 0664,
	show_Power_On_Voltage, store_Power_On_Voltage);
/* ///////////////////////////////////////////////////////////////
 * // Create File For EM : Power_Off_Voltage
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_Power_Off_Voltage(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 3400;
	pr_debug("[EM] Power_Off_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_Off_Voltage(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Power_Off_Voltage, 0664,
	show_Power_Off_Voltage, store_Power_Off_Voltage);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : Charger_TopOff_Value
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_Charger_TopOff_Value(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 4110;
	pr_debug("[EM] Charger_TopOff_Value : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Charger_TopOff_Value(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Charger_TopOff_Value, 0664,
	show_Charger_TopOff_Value, store_Charger_TopOff_Value);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : FG_Battery_CurrentConsumption
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_FG_Battery_CurrentConsumption(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret_value = 8888;

	ret_value = battery_meter_get_battery_current();
	pr_debug("[EM] FG_Battery_CurrentConsumption : %d/10 mA\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_FG_Battery_CurrentConsumption(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(FG_Battery_CurrentConsumption, 0664,
	show_FG_Battery_CurrentConsumption,
	store_FG_Battery_CurrentConsumption);
/* ///////////////////////////////////////////////////////////////
 *     Create File For EM : FG_SW_CoulombCounter
 * ///////////////////////////////////////////////////////////////
 */
static ssize_t show_FG_SW_CoulombCounter(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	s32 ret_value = 7777;

	ret_value = battery_meter_get_car();
	pr_debug("[EM] FG_SW_CoulombCounter : %d\n",
		ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_FG_SW_CoulombCounter(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(FG_SW_CoulombCounter, 0664,
	show_FG_SW_CoulombCounter, store_FG_SW_CoulombCounter);

static ssize_t show_Charging_CallState(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	pr_debug("call state = %d\n", g_call_state);
	return sprintf(buf, "%u\n", g_call_state);
}

static ssize_t store_Charging_CallState(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, call_state;

	ret = kstrtoint(buf, 0, &call_state);
	if (ret) {
		pr_debug("wrong format!\n");
		return size;
	}

	g_call_state = (call_state ? true : false);
	pr_debug("call state = %d\n", g_call_state);
	return size;
}

static DEVICE_ATTR(Charging_CallState, 0664,
	show_Charging_CallState, store_Charging_CallState);

static ssize_t show_Charging_Enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int charging_enable = 1;

	if (cmd_discharging == 1)
		charging_enable = 0;
	else
		charging_enable = 1;

	pr_notice("hold charging = %d\n", !charging_enable);
	return sprintf(buf, "%u\n", charging_enable);
}

static ssize_t store_Charging_Enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int charging_enable = 1;
	int ret;
	int call_wake_up_bat;

	ret = kstrtouint(buf, 0, &charging_enable);

	if ((charging_enable == 1 && cmd_discharging != 1) ||
	   (charging_enable == 0 && cmd_discharging == 1)) {
		call_wake_up_bat = 0;
	} else
		call_wake_up_bat = 1;

	if (charging_enable == 1)
		cmd_discharging = 0;
	else if (charging_enable == 0)
		cmd_discharging = 1;

	if (call_wake_up_bat)
		wake_up_bat();
	else
		pr_notice("skip repeated wake_up_bat() when store_Charging_Enable()\n");

	pr_notice("hold charging = %d\n", cmd_discharging);
	return size;
}

static DEVICE_ATTR(Charging_Enable, 0664,
	show_Charging_Enable, store_Charging_Enable);

static ssize_t show_Custom_Charging_Current(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_notice("custom charging current = %d\n",
			    g_custom_charging_current);
	return sprintf(buf, "%d\n", g_custom_charging_current);
}

static ssize_t store_Custom_Charging_Current(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t size)
{
	int ret, cur = 0;

	ret = kstrtouint(buf, 0, &cur);
	g_custom_charging_current = cur;
	pr_notice("custom charging current = %d\n",
			    g_custom_charging_current);
	wake_up_bat();
	return size;
}

static DEVICE_ATTR(Custom_Charging_Current, 0664,
	show_Custom_Charging_Current, store_Custom_Charging_Current);

static ssize_t show_Custom_PlugIn_Time(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_notice("custom plugin_time = %lu\n", g_custom_plugin_time);
	return sprintf(buf, "0");
}

static ssize_t store_Custom_PlugIn_Time(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;

	ret = kstrtoul(buf, 0, &g_custom_plugin_time);
	pr_notice("custom plugin_time = %lu\n", g_custom_plugin_time);
	if (g_custom_plugin_time > PLUGIN_THRESHOLD)
		g_custom_plugin_time = PLUGIN_THRESHOLD;

	wake_up_bat();
	return size;
}

static DEVICE_ATTR(Custom_PlugIn_Time, 0664,
	show_Custom_PlugIn_Time, store_Custom_PlugIn_Time);

static ssize_t show_Custom_Charging_Mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_notice("Charging mode = %u\n", g_custom_charging_mode);
	return sprintf(buf, "%u\n", g_custom_charging_mode);
}

static ssize_t store_Custom_Charging_Mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;

	ret = kstrtouint(buf, 0, &g_custom_charging_mode);
	pr_notice("Charging mode= %u\n", g_custom_charging_mode);

	return size;
}

static DEVICE_ATTR(Custom_Charging_Mode, 0660,
	show_Custom_Charging_Mode, store_Custom_Charging_Mode);

static ssize_t show_Custom_RTC_SOC(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_notice("custom rtc soc = %d\n",
			    g_custom_rtc_soc);
	return sprintf(buf, "%d\n", g_custom_rtc_soc);
}

static ssize_t store_Custom_RTC_SOC(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, cur = 0;

	ret = kstrtouint(buf, 0, &cur);

	if (cur >= 0 && cur <= 100) {
		g_custom_rtc_soc = cur;
		pr_notice("custom rtc soc = %d\n", g_custom_rtc_soc);
		wake_up_bat();
	}
	return size;
}

static DEVICE_ATTR(Custom_RTC_SOC, 0664,
	show_Custom_RTC_SOC, store_Custom_RTC_SOC);

/* V_0Percent_Tracking */
static ssize_t show_V_0Percent_Tracking(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_notice("V_0Percent_Tracking = %d\n",
		batt_cust_data.v_0percent_tracking);
	return sprintf(buf, "%u\n", batt_cust_data.v_0percent_tracking);
}

static ssize_t store_V_0Percent_Tracking(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rv;

	/*rv = sscanf(buf, "%u", &V_0Percent_Tracking);*/
	rv = kstrtouint(buf, 0, &batt_cust_data.v_0percent_tracking);
	if (rv != 0)
		return -EINVAL;
	pr_notice("V_0Percent_Tracking = %d\n",
		batt_cust_data.v_0percent_tracking);
	return size;
}

static DEVICE_ATTR(V_0Percent_Tracking, 0664,
	show_V_0Percent_Tracking, store_V_0Percent_Tracking);


static ssize_t show_Charger_Type(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned int chr_ype = CHARGER_UNKNOWN;

	chr_ype = BMT_status.charger_exist ?
		BMT_status.charger_type : CHARGER_UNKNOWN;

	pr_notice("CHARGER_TYPE = %d\n", chr_ype);
	return sprintf(buf, "%u\n", chr_ype);
}

static ssize_t store_Charger_Type(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Charger_Type, 0664,
	show_Charger_Type, store_Charger_Type);

static ssize_t show_usbin_now(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int usbin_mv = 0;

	usbin_mv = battery_meter_get_charger_voltage();
	return sprintf(buf, "%d\n", usbin_mv);
}

static DEVICE_ATTR(usbin_now, 0444, show_usbin_now, NULL);


static ssize_t show_aicl_vbus_state_phase(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("aicl_vbus_state_phase = %d\n",
		batt_cust_data.aicl_vbus_state_phase);
	return sprintf(buf, "%u\n", batt_cust_data.aicl_vbus_state_phase);
}

static ssize_t store_aicl_vbus_state_phase(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0, phase_ms = 0;

	ret = kstrtoint(buf, 0, &phase_ms);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	if (phase_ms > 0 && phase_ms < 100)
		batt_cust_data.aicl_vbus_state_phase = phase_ms;
	else
		pr_info("[%s] out of range: 0x%x\n", __func__, phase_ms);

	return size;
}

static DEVICE_ATTR(aicl_vbus_state_phase, 0664,
	show_aicl_vbus_state_phase, store_aicl_vbus_state_phase);

static ssize_t show_aicl_vbus_valid(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("aicl_vbus_valid = %d\n", batt_cust_data.aicl_vbus_valid);
	return sprintf(buf, "%u\n", batt_cust_data.aicl_vbus_valid);
}

static ssize_t store_aicl_vbus_valid(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0, valid_mv = 0;

	ret = kstrtoint(buf, 0, &valid_mv);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	if (valid_mv >= 4400 && valid_mv <= 4800)
		batt_cust_data.aicl_vbus_valid = valid_mv;
	else
		pr_info("[%s] out of range: 0x%x\n", __func__, valid_mv);

	return size;
}

static DEVICE_ATTR(aicl_vbus_valid, 0664,
	show_aicl_vbus_valid, store_aicl_vbus_valid);


static ssize_t show_aicl_input_current_max(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("aicl_input_current_max = %d\n",
		batt_cust_data.aicl_input_current_max);
	return sprintf(buf, "%u\n",
		batt_cust_data.aicl_input_current_max / 100);
}

static ssize_t store_aicl_aicl_input_current_max(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0, iusb_max = 0;

	ret = kstrtoint(buf, 0, &iusb_max);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	if (iusb_max >= 1400 && iusb_max <= 2000) {
		g_custom_aicl_input_current = 1;
		batt_cust_data.aicl_input_current_max = iusb_max * 100;
	} else
		pr_info("[%s] out of range: 0x%x\n", __func__, iusb_max);

	return size;
}

static DEVICE_ATTR(aicl_input_current_max, 0664,
	show_aicl_input_current_max, store_aicl_aicl_input_current_max);

static ssize_t show_aicl_input_current_min(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_debug("ap15_charger_input_current_min = %d\n",
		batt_cust_data.aicl_input_current_min);
	return sprintf(buf, "%u\n",
		batt_cust_data.aicl_input_current_min / 100);
}

static ssize_t store_aicl_aicl_input_current_min(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0, iusb_min = 0;

	ret = kstrtoint(buf, 0, &iusb_min);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	if (iusb_min >= 1000 && iusb_min <= 1500) {
		g_custom_aicl_input_current = 1;
		batt_cust_data.aicl_input_current_min = iusb_min * 100;
	} else
		pr_info("[%s] out of range: 0x%x\n", __func__, iusb_min);

	return size;
}

static DEVICE_ATTR(aicl_input_current_min, 0664,
	show_aicl_input_current_min, store_aicl_aicl_input_current_min);

static ssize_t show_aicl_result(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("BMT_status.aicl_result = %d\n", BMT_status.aicl_result / 100);
	return sprintf(buf, "%u\n", BMT_status.aicl_result / 100);
}

static DEVICE_ATTR(aicl_result, 0444, show_aicl_result, NULL);

static ssize_t show_battery_cmd(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_notice("bat_tt_enable=%d, bat_thr_test_mode=%d, bat_thr_test_value=%d\n",
		g_battery_thermal_throttling_flag,
		battery_cmd_thermal_test_mode,
		battery_cmd_thermal_test_mode_value);

	return sprintf(buf, "%d, %d, %d\n",
		g_battery_thermal_throttling_flag,
		battery_cmd_thermal_test_mode,
		battery_cmd_thermal_test_mode_value);
}

static ssize_t store_battery_cmd(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	char *pvalue, *pvalue_buf[4];
	int i;

	if (buf != NULL && size != 0) {
		pvalue = (char *)buf;
		for (i = 0; i < 3; i++) {
			pvalue_buf[i] = strsep(&pvalue, " ");
			if (!pvalue_buf[i]) {
				pr_notice("Please input correct data\n");
				return -EINVAL;
			}
		}

		pvalue_buf[3] = strsep(&pvalue, " ");
		if (pvalue_buf[3]) {
			pr_notice("Please input correct data\n");
			return -EINVAL;
		}

		if (kstrtoint(pvalue_buf[0], 10,
			&g_battery_thermal_throttling_flag) < 0)
			pr_notice("%s: Invalid INT input data", __func__);

		if (kstrtoint(pvalue_buf[1], 10,
			&battery_cmd_thermal_test_mode) < 0)
			pr_notice("%s: Invalid INT input data", __func__);

		if (kstrtoint(pvalue_buf[2], 10,
			&battery_cmd_thermal_test_mode_value) < 0)
			pr_notice("%s: Invalid INT input data", __func__);

		pr_notice(
			"bat_tt_enable=%d, bat_thr_test_mode=%d, bat_thr_test_value=%d\n",
			g_battery_thermal_throttling_flag,
			battery_cmd_thermal_test_mode,
			battery_cmd_thermal_test_mode_value);
	}

	return size;
}

static DEVICE_ATTR(battery_cmd, 0664, show_battery_cmd, store_battery_cmd);

static void mt_battery_update_EM(struct battery_data *bat_data)
{
	bat_data->BAT_CAPACITY = BMT_status.UI_SOC;
	bat_data->BAT_TemperatureR = BMT_status.temperatureR;
	bat_data->BAT_TempBattVoltage = BMT_status.temperatureV;
	bat_data->BAT_InstatVolt = BMT_status.bat_vol;
	bat_data->BAT_BatteryAverageCurrent = BMT_status.ICharging;
	bat_data->BAT_BatterySenseVoltage = BMT_status.bat_vol;
	bat_data->BAT_ISenseVoltage = BMT_status.Vsense;
	bat_data->BAT_ChargerVoltage = BMT_status.charger_vol;
	/* Dual battery */
	bat_data->status_smb = g_status_smb;
	bat_data->capacity_smb = g_capacity_smb;
	bat_data->present_smb = g_present_smb;
	pr_debug("status_smb = %d, capacity_smb = %d, present_smb = %d\n",
		bat_data->status_smb,
		bat_data->capacity_smb,
		bat_data->present_smb);

#ifdef CONFIG_MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION
	if (bat_data->BAT_CAPACITY <= 0)
		bat_data->BAT_CAPACITY = 1;

	pr_notice(
		"BAT_CAPACITY=1, due to define CONFIG_MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION\r\n");
#endif
}


static bool mt_battery_100Percent_tracking_check(void)
{
	bool resetBatteryMeter = false;

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	unsigned int cust_sync_time =
		batt_cust_data.cust_soc_jeita_sync_time;
#else
	unsigned int cust_sync_time =
		batt_cust_data.onehundred_percent_tracking_time;
#endif
	static unsigned int timer_counter;
	static int timer_counter_init;

	/* Init timer_counter for 1st time */
	if (timer_counter_init == 0) {
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		timer_counter =	(batt_cust_data.cust_soc_jeita_sync_time /
				BAT_TASK_PERIOD);
#else
		timer_counter =
			(batt_cust_data.onehundred_percent_tracking_time /
			BAT_TASK_PERIOD);
#endif
		timer_counter_init = 1;
	}

	/* charging full first, UI tracking to 100% */
	if (BMT_status.bat_full == true) {
		if (BMT_status.UI_SOC >= 100) {
			BMT_status.UI_SOC = 100;

			if ((g_charging_full_reset_bat_meter == true)
			    && (BMT_status.bat_charging_state == CHR_BATFULL)) {
				resetBatteryMeter = true;
				g_charging_full_reset_bat_meter = false;
			} else {
				resetBatteryMeter = false;
			}
		} else {
			/* increase UI percentage every xxs */
			if (timer_counter >=
				(cust_sync_time / BAT_TASK_PERIOD)) {
				timer_counter = 1;
				BMT_status.UI_SOC++;
			} else {
				timer_counter++;

				return resetBatteryMeter;
			}

			resetBatteryMeter = true;
		}

		if (BMT_status.UI_SOC == 100)
			is_uisoc_ever_100 = true;

		if (((BMT_status.UI_SOC - BMT_status.SOC) > RECHARGE_THRES)
			&& is_uisoc_ever_100) {
			is_uisoc_ever_100 = false;
			BMT_status.bat_full = false;
		}

		pr_debug("[100percent], UI_SOC(%d), reset(%d) bat_full(%d) ever100(%d)\n",
		BMT_status.UI_SOC, resetBatteryMeter,
		BMT_status.bat_full, is_uisoc_ever_100);
	} else if (is_uisoc_ever_100) {
		pr_debug("[100percent-ever100],UI_SOC=%d SOC=%d\n",
			BMT_status.UI_SOC, BMT_status.UI_SOC);

	} else {
		/* charging is not full,  UI keep 99% if reaching 100%, */
		if (BMT_status.UI_SOC >= 99) {
			BMT_status.UI_SOC = 99;
			resetBatteryMeter = false;

			pr_debug("[100percent],UI_SOC = %d\n",
				BMT_status.UI_SOC);
		}
		timer_counter = (cust_sync_time / BAT_TASK_PERIOD);
	}

	return resetBatteryMeter;
}


static bool mt_battery_nPercent_tracking_check(void)
{
	bool resetBatteryMeter = false;
#if defined(SOC_BY_HW_FG)
	static unsigned int timer_counter;
	static int timer_counter_init;

	if (timer_counter_init == 0) {
		timer_counter_init = 1;
		timer_counter = (batt_cust_data.npercent_tracking_time /
			BAT_TASK_PERIOD);
	}


	if (BMT_status.nPrecent_UI_SOC_check_point == 0)
		return false;

	/* fuel gauge ZCV < 15%, but UI > 15%,  15% can be customized */
	if ((BMT_status.ZCV <= BMT_status.nPercent_ZCV)
	    && (BMT_status.UI_SOC >
		BMT_status.nPrecent_UI_SOC_check_point)) {
		if (timer_counter ==
			(batt_cust_data.npercent_tracking_time
			/ BAT_TASK_PERIOD)) {
			/* every x sec decrease UI percentage */
			BMT_status.UI_SOC--;
			timer_counter = 1;
		} else {
			timer_counter++;
			return resetBatteryMeter;
		}

		resetBatteryMeter = true;

		pr_debug(
		"[nPercent] ZCV %d <= nPercent_ZCV %d, UI_SOC=%d, tracking UI_SOC=%d\n",
		BMT_status.ZCV, BMT_status.nPercent_ZCV,
		BMT_status.UI_SOC, BMT_status.nPrecent_UI_SOC_check_point);
	} else if ((BMT_status.ZCV > BMT_status.nPercent_ZCV)
		   && (BMT_status.UI_SOC ==
			BMT_status.nPrecent_UI_SOC_check_point)) {

		/* hold UI 15% */
		timer_counter = (batt_cust_data.npercent_tracking_time /
				BAT_TASK_PERIOD);
		resetBatteryMeter = true;

		pr_notice(
			"[nPercent] ZCV %d > BMT_status.nPercent_ZCV %d and UI SOC=%d, then keep %d.\n",
		BMT_status.ZCV, BMT_status.nPercent_ZCV, BMT_status.UI_SOC,
		BMT_status.nPrecent_UI_SOC_check_point);
	} else {
		timer_counter =
			(batt_cust_data.npercent_tracking_time
			/ BAT_TASK_PERIOD);
	}
#endif
	return resetBatteryMeter;

}

static bool mt_battery_0Percent_tracking_check(void)
{
	bool resetBatteryMeter = true;

	if (BMT_status.UI_SOC <= 0) {
		BMT_status.UI_SOC = 0;
	} else {
		if ((BMT_status.bat_vol >
			batt_cust_data.system_off_voltage)
			&& (BMT_status.UI_SOC > 1))
			BMT_status.UI_SOC--;
		else if (BMT_status.bat_vol <=
			batt_cust_data.system_off_voltage)
			BMT_status.UI_SOC--;
	}

	pr_notice("0Percent, VBAT < %d UI_SOC=%d\r\n",
		batt_cust_data.system_off_voltage, BMT_status.UI_SOC);

	bat_metrics_critical_shutdown();

	return resetBatteryMeter;
}


static void mt_battery_Sync_UI_Percentage_to_Real(void)
{
	static unsigned int timer_counter;

	if ((BMT_status.UI_SOC > BMT_status.SOC)
		&& ((BMT_status.UI_SOC != 1))) {
#if !defined(SYNC_UI_SOC_IMM)
		/* reduce after xxs */
		if (chr_wake_up_bat == false) {
			if (timer_counter ==
			    (batt_cust_data.sync_to_real_tracking_time
				/ BAT_TASK_PERIOD)) {
				BMT_status.UI_SOC--;
				timer_counter = 0;
			}
#ifdef FG_BAT_INT
			else if (fg_wake_up_bat == true)
				BMT_status.UI_SOC--;

#endif				/* #ifdef FG_BAT_INT */
			else
				timer_counter++;

		} else
			pr_debug(
				"[Sync_Real] chr_wake_up_bat=1 , do not update UI_SOC\n");
#else
		BMT_status.UI_SOC--;
#endif
		pr_notice("[Sync_Real] UI_SOC=%d, SOC=%d, counter = %d\n",
			BMT_status.UI_SOC, BMT_status.SOC, timer_counter);
	} else {
		timer_counter = 0;

#if !defined(CUST_CAPACITY_OCV2CV_TRANSFORM)
		BMT_status.UI_SOC = BMT_status.SOC;
#else
		if (BMT_status.UI_SOC == -1)
			BMT_status.UI_SOC = BMT_status.SOC;
		else if (BMT_status.charger_exist &&
				BMT_status.bat_charging_state != CHR_ERROR) {
			if (BMT_status.UI_SOC < BMT_status.SOC
			    && (BMT_status.SOC - BMT_status.UI_SOC > 1))
				BMT_status.UI_SOC++;
			else
				BMT_status.UI_SOC = BMT_status.SOC;
		}
#endif
	}

	if (BMT_status.UI_SOC <= 0) {
		BMT_status.UI_SOC = 1;
		pr_notice("[Battery]UI_SOC get 0 first (%d)\r\n",
			    BMT_status.UI_SOC);
	}

	if (BMT_status.pre_UI_SOC != BMT_status.UI_SOC) {
		pr_notice("[%s] pre_ui_soc(%d), ui_soc(%d)\n", __func__,
			  BMT_status.pre_UI_SOC, BMT_status.UI_SOC);
		BMT_status.pre_UI_SOC = BMT_status.UI_SOC;
	}
}

static void battery_update(struct battery_data *bat_data)
{
	struct power_supply *bat_psy = bat_data->psy;
	bool resetBatteryMeter = false;
	unsigned char chg_fault_type;
	bat_data->BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
	bat_data->BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
	/* healthd reading voltage in microVoltage unit */
	bat_data->BAT_batt_vol = BMT_status.bat_vol * 1000;
	bat_data->BAT_batt_temp = BMT_status.temperature * 10;
	bat_data->BAT_PRESENT = BMT_status.bat_exist;

	if ((BMT_status.charger_exist == true) &&
		(BMT_status.bat_charging_state != CHR_ERROR)) {
		if (BMT_status.bat_exist) {	/* charging */
			if (BMT_status.bat_vol <=
				batt_cust_data.v_0percent_tracking)
				resetBatteryMeter =
					mt_battery_0Percent_tracking_check();
			else
				resetBatteryMeter =
					mt_battery_100Percent_tracking_check();

		{
			unsigned int status;

			battery_charging_control(
			CHARGING_CMD_GET_CHARGING_STATUS, &status);
			if (status == true) {
				bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_FULL;
				pr_notice("battery status: %s\n", "full");
			} else {
				bat_data->BAT_STATUS =
					POWER_SUPPLY_STATUS_CHARGING;
				pr_notice("battery status: %s\n", "charging");
			}
		}
		} else {
			/* No Battery, Only Charger */
			bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_UNKNOWN;
			pr_notice("battery status: %s\n", "unknown");
			BMT_status.UI_SOC = 0;
		}

	} else {		/* Only Battery */

		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_DISCHARGING;
		pr_notice("battery status: %s\n", "discharging");
		if (BMT_status.bat_vol <=
			batt_cust_data.v_0percent_tracking)
			resetBatteryMeter =
				mt_battery_0Percent_tracking_check();
		else
			resetBatteryMeter =
				mt_battery_nPercent_tracking_check();
	}

	if (resetBatteryMeter == true) {
		battery_meter_reset();
	} else {
		if (BMT_status.bat_full) {
			BMT_status.UI_SOC = 100;
			pr_info("%s: Force to set UI_SOC[%d, %d] to 100\n",
				__func__, BMT_status.UI_SOC, BMT_status.SOC);
		} else {
			mt_battery_Sync_UI_Percentage_to_Real();
		}
	}

	battery_charging_control(CHARGING_CMD_GET_FAULT_TYPE, &chg_fault_type);
	bat_metrics_chg_fault(chg_fault_type);

	pr_debug("UI_SOC=(%d), resetBatteryMeter=(%d)\n",
		    BMT_status.UI_SOC, resetBatteryMeter);

#if !defined(CUST_CAPACITY_OCV2CV_TRANSFORM)
	/* set RTC SOC to 1 to avoid SOC jump in charger boot.*/
	if (g_custom_rtc_soc != -1)
		set_rtc_spare_fg_value(g_custom_rtc_soc);
	else if (BMT_status.UI_SOC <= 1)
		set_rtc_spare_fg_value(1);
	else
		set_rtc_spare_fg_value(BMT_status.UI_SOC);

#else
	/* We store capacity before loading compenstation in RTC */
	if (g_custom_rtc_soc != -1)
		set_rtc_spare_fg_value(g_custom_rtc_soc);
	else if (battery_meter_get_battery_soc() <= 1)
		set_rtc_spare_fg_value(1);
	else
		set_rtc_spare_fg_value(battery_meter_get_battery_soc());

	pr_debug("RTC_SOC=(%d)\n", get_rtc_spare_fg_value());
#endif

	mt_battery_update_EM(bat_data);

	if (cmd_discharging == 1)
		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_CMD_DISCHARGING;

	if (adjust_power != -1) {
		bat_data->adjust_power = adjust_power;
		pr_notice("adjust_power=(%d)\n", adjust_power);
	}
#ifdef DLPT_POWER_OFF_EN
	/* extern int dlpt_check_power_off(void); */
	if (bat_data->BAT_CAPACITY <= DLPT_POWER_OFF_THD) {
		pr_notice("[DLPT_POWER_OFF_EN] run\n");

		if (bat_data->BAT_CAPACITY == 0) {
			bat_data->BAT_CAPACITY = 1;
			pr_notice("[DLPT_POWER_OFF_EN] SOC=0 but keep %d\n",
				    bat_data->BAT_CAPACITY);
		}
		if (dlpt_check_power_off() == 1) {
			bat_data->BAT_CAPACITY = 0;
			pr_notice("[DLPT_POWER_OFF_EN] SOC=%d to power off\n",
				    bat_data->BAT_CAPACITY);
		}
	} else {
		pr_notice("[DLPT_POWER_OFF_EN] disable(%d)\n",
			    bat_data->BAT_CAPACITY);
	}
#endif
	bat_metrics_chg_state(bat_data->BAT_STATUS);
	power_supply_changed(bat_psy);
}

void update_charger_info(int wireless_state)
{
#if defined(CONFIG_POWER_VERIFY)
	pr_debug("[update_charger_info] no support\n");
#else
	g_wireless_state = wireless_state;
	pr_debug("[update_charger_info] get wireless_state=%d\n",
		wireless_state);

	wake_up_bat();
#endif
}

static void wireless_update(struct wireless_data *wireless_data)
{
	static int wireless_status = -1;
	struct power_supply *wireless_psy = wireless_data->psy;
	struct power_supply_desc *wireless_psd = &wireless_data->psd;

	if (BMT_status.charger_exist == true || g_wireless_state) {
		if ((BMT_status.charger_type == WIRELESS_CHARGER)
			|| g_wireless_state) {
			wireless_data->WIRELESS_ONLINE = 1;
			wireless_psd->type = POWER_SUPPLY_TYPE_WIRELESS;
		} else
			wireless_data->WIRELESS_ONLINE = 0;
	} else {
		wireless_data->WIRELESS_ONLINE = 0;
	}

	if (wireless_status != wireless_data->WIRELESS_ONLINE) {
		wireless_status = wireless_data->WIRELESS_ONLINE;
		power_supply_changed(wireless_psy);
	}
}

static void ac_update(struct ac_data *ac_data)
{
	struct power_supply *ac_psy = ac_data->psy;
	struct power_supply_desc *ac_psd = &ac_data->psd;

	if (BMT_status.charger_exist == true) {
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		if ((BMT_status.charger_type == NONSTANDARD_CHARGER) ||
		    (BMT_status.charger_type == STANDARD_CHARGER) ||
		    (BMT_status.charger_type == APPLE_2_1A_CHARGER) ||
		    (BMT_status.charger_type == APPLE_1_0A_CHARGER) ||
		    (BMT_status.charger_type == APPLE_0_5A_CHARGER)) {
#else
		if ((BMT_status.charger_type == NONSTANDARD_CHARGER) ||
		    (BMT_status.charger_type == STANDARD_CHARGER) ||
		    (BMT_status.charger_type == APPLE_2_1A_CHARGER) ||
		    (BMT_status.charger_type == APPLE_1_0A_CHARGER) ||
		    (BMT_status.charger_type == APPLE_0_5A_CHARGER) ||
		    (DISO_data.diso_state.cur_vdc_state == DISO_ONLINE)) {
#endif
			ac_data->AC_ONLINE = 1;
			ac_psd->type = POWER_SUPPLY_TYPE_MAINS;
		} else
			ac_data->AC_ONLINE = 0;
	} else {
		ac_data->ac_present = 0;
		ac_data->AC_ONLINE = 0;
	}

	if ((ac_data->ac_old_state != ac_data->AC_ONLINE)
		|| (ac_data->ac_present != ac_data->ac_present_old)) {
		ac_data->ac_old_state = ac_data->AC_ONLINE;
		ac_data->ac_present_old = ac_data->ac_present;
		power_supply_changed(ac_psy);
	}
}

static void usb_update(struct usb_data *usb_data)
{
	struct power_supply *usb_psy = usb_data->psy;
	struct power_supply_desc *usb_psd = &usb_data->psd;

	if (BMT_status.charger_exist == true) {
		usb_data->usb_present = 1;
		if ((BMT_status.charger_type == STANDARD_HOST) ||
		    (BMT_status.charger_type == CHARGING_HOST)) {
			usb_data->USB_ONLINE = 1;
			usb_psd->type = POWER_SUPPLY_TYPE_USB;
		} else
			usb_data->USB_ONLINE = 0;
	} else {
		usb_data->usb_present = 0;
		usb_data->USB_ONLINE = 0;
	}

	if ((usb_data->usb_old_state != usb_data->USB_ONLINE)
		|| (usb_data->usb_present != usb_data->usb_present_old)) {
		usb_data->usb_old_state = usb_data->USB_ONLINE;
		usb_data->usb_present_old = usb_data->usb_present;
		power_supply_changed(usb_psy);
	}
}
#endif

/* /////////////////////////////////////////////////
 * // Battery Temprature Parameters and functions
 * ////////////////////////////////////////////////
 */
bool pmic_chrdet_status(void)
{
	if (upmu_is_chr_det() == true)
		return true;

	pr_notice("[pmic_chrdet_status] No charger\r\n");
	return false;
}

/* /////////////////////////////
 * // Pulse Charging Algorithm
 * /////////////////////////////
 */
bool bat_is_charger_exist(void)
{
	return get_charger_detect_status();
}


bool bat_is_charging_full(void)
{
	if ((BMT_status.bat_full == true) &&
		(BMT_status.bat_in_recharging_state == false))
		return true;
	else
		return false;
}


unsigned int bat_get_ui_percentage(void)
{
	/* for plugging out charger in recharge phase, using SOC as UI_SOC */
#if defined(CONFIG_POWER_EXT)
	pr_debug("[BATTERY] bat_get_ui_percentage return 100 !!\n\r");
	return 100;
#else
	if (chr_wake_up_bat == true)
		return BMT_status.SOC;
	else
		return BMT_status.UI_SOC;
#endif
}

/* Full state --> recharge voltage --> full state */
unsigned int bat_is_recharging_phase(void)
{
	return BMT_status.bat_in_recharging_state ||
			BMT_status.bat_full == true;
}


int get_bat_charging_current_level(void)
{
	unsigned int charging_current;

	battery_charging_control(CHARGING_CMD_GET_CURRENT, &charging_current);

	return charging_current;
}


unsigned int do_batt_temp_state_machine(void)
{
	if (BMT_status.temperature == batt_cust_data.err_charge_temperature)
		return PMU_STATUS_FAIL;



	if (batt_cust_data.bat_low_temp_protect_enable) {
		if (BMT_status.temperature <
			batt_cust_data.min_charge_temperature) {
			pr_err(
				    "[BATTERY] Battery Under Temperature or NTC fail !!\n\r");
			g_batt_temp_status = TEMP_POS_LOW;
			return PMU_STATUS_FAIL;
		} else if (g_batt_temp_status == TEMP_POS_LOW) {
			if (BMT_status.temperature >=
		    batt_cust_data.min_charge_temperature_plus_x_degree) {
			pr_notice(
			"[BATTERY] Battery Temperature raise from %d to %d(%d), allow charging!!\n\r",
			batt_cust_data.min_charge_temperature,
			BMT_status.temperature,
			batt_cust_data.min_charge_temperature_plus_x_degree);
			g_batt_temp_status = TEMP_POS_NORMAL;
			BMT_status.bat_charging_state = CHR_PRE;
			return PMU_STATUS_OK;
			} else
				return PMU_STATUS_FAIL;
		}
	}

	if (BMT_status.temperature >= batt_cust_data.max_charge_temperature) {
		pr_err("[BATTERY] Battery Over Temperature !!\n\r");
		g_batt_temp_status = TEMP_POS_HIGH;
		return PMU_STATUS_FAIL;
	} else if (g_batt_temp_status == TEMP_POS_HIGH) {
		if (BMT_status.temperature <
			batt_cust_data.max_charge_temperature_minus_x_degree) {
			pr_notice(
			"[BATTERY] Battery Temperature down from %d to %d(%d), allow charging!!\n\r",
			batt_cust_data.max_charge_temperature,
			BMT_status.temperature,
			batt_cust_data.max_charge_temperature_minus_x_degree);
			g_batt_temp_status = TEMP_POS_NORMAL;
			BMT_status.bat_charging_state = CHR_PRE;
			return PMU_STATUS_OK;
		} else
			return PMU_STATUS_FAIL;
	} else {
		g_batt_temp_status = TEMP_POS_NORMAL;
	}
	return PMU_STATUS_OK;
}


unsigned long BAT_Get_Battery_Voltage(int polling_mode)
{
	unsigned long ret_val = 0;

#if defined(CONFIG_POWER_EXT)
	ret_val = 4000;
#else
	ret_val = battery_meter_get_battery_voltage(false);
#endif

	return ret_val;
}


static void mt_battery_average_method_init(unsigned int type,
	unsigned int *bufferdata, unsigned int data, signed int *sum)
{
	unsigned int i;
	static bool batteryBufferFirst = true;
	static bool previous_charger_exist;
	static bool previous_in_recharge_state;

	static unsigned char index;

	/* reset charging current window while plug in/out { */
	if (type == BATTERY_AVG_CURRENT) {
		if (BMT_status.charger_exist == true) {
		if (previous_charger_exist == false) {
		batteryBufferFirst = true;
		previous_charger_exist = true;
		if (BMT_status.charger_type == STANDARD_CHARGER)
			data = batt_cust_data.ac_charger_current / 100;
		else if (BMT_status.charger_type == CHARGING_HOST)
			data =
			batt_cust_data.charging_host_charger_current / 100;
		else if (BMT_status.charger_type == NONSTANDARD_CHARGER)
			data =
			batt_cust_data.non_std_ac_charger_current / 100;
		else
			data = batt_cust_data.usb_charger_current / 100;
#ifdef AVG_INIT_WITH_R_SENSE
		data = AVG_INIT_WITH_R_SENSE(data);
#endif
		} else if ((previous_in_recharge_state == false)
		   && (BMT_status.bat_in_recharging_state == true)) {
		batteryBufferFirst = true;
		if (BMT_status.charger_type == STANDARD_CHARGER)
			data = batt_cust_data.ac_charger_current / 100;
		else if (BMT_status.charger_type == CHARGING_HOST)
			data =
			batt_cust_data.charging_host_charger_current / 100;
		else if (BMT_status.charger_type == NONSTANDARD_CHARGER)
			data =
			batt_cust_data.non_std_ac_charger_current / 100;
		else
			data = batt_cust_data.usb_charger_current / 100;
#ifdef AVG_INIT_WITH_R_SENSE
		data = AVG_INIT_WITH_R_SENSE(data);
#endif
		}

		previous_in_recharge_state =
			BMT_status.bat_in_recharging_state;
		} else {
			if (previous_charger_exist == true) {
				batteryBufferFirst = true;
				previous_charger_exist = false;
				data = 0;
			}
		}
	}

	/* reset charging current window while plug in/out } */
	pr_debug("batteryBufferFirst =%d, data= (%d)\n",
		batteryBufferFirst, data);

	if (batteryBufferFirst == true) {
		for (i = 0; i < BATTERY_AVERAGE_SIZE; i++)
			bufferdata[i] = data;

		*sum = data * BATTERY_AVERAGE_SIZE;
	}

	index++;
	if (index >= BATTERY_AVERAGE_DATA_NUMBER) {
		index = BATTERY_AVERAGE_DATA_NUMBER;
		batteryBufferFirst = false;
	}
}


static unsigned int mt_battery_average_method(unsigned int type,
	unsigned int *bufferdata, unsigned int data,
	signed int *sum, unsigned char batteryIndex)
{
	unsigned int avgdata;

	mt_battery_average_method_init(type, bufferdata, data, sum);

	*sum -= bufferdata[batteryIndex];
	*sum += data;
	bufferdata[batteryIndex] = data;
	avgdata = (*sum) / BATTERY_AVERAGE_SIZE;

	pr_debug("bufferdata[%d]= (%d)\n",
		batteryIndex, bufferdata[batteryIndex]);
	return avgdata;
}

static int filter_battery_temperature(int instant_temp)
{
	int check_count;

	/* recheck 3 times for critical temperature */
	for (check_count = 0; check_count < 3; check_count++) {
		if (instant_temp >= 60) {
			instant_temp = battery_meter_get_battery_temperature();
			pr_warn("recheck battery temperature result: %d\n",
				instant_temp);
			msleep(20);
			continue;
		}
	}

	return instant_temp;
}

void mt_battery_GetBatteryData(void)
{
	unsigned int bat_vol, charger_vol, Vsense, ZCV;
	signed int ICharging, temperature, temperatureR, temperatureV, SOC;
	static signed int bat_sum, icharging_sum;
	static signed int batteryVoltageBuffer[BATTERY_AVERAGE_SIZE];
	static signed int batteryCurrentBuffer[BATTERY_AVERAGE_SIZE];
	static unsigned char batteryIndex;
	static signed int previous_SOC = -1;

	bat_vol = battery_meter_get_battery_voltage(true);
	Vsense = battery_meter_get_VSense();
	if (upmu_is_chr_det() == true)
		ICharging = battery_meter_get_charging_current();
	else
		ICharging = 0;

	charger_vol = battery_meter_get_charger_voltage();
	temperature = battery_meter_get_battery_temperature();
	temperatureV = battery_meter_get_tempV();
	temperatureR = battery_meter_get_tempR(temperatureV);

	if (bat_meter_timeout == true ||
		bat_spm_timeout == true ||
		fg_wake_up_bat == true) {
		SOC = battery_meter_get_battery_percentage();
		bat_meter_timeout = false;
		bat_spm_timeout = false;
	} else {
		if (previous_SOC == -1)
			SOC = battery_meter_get_battery_percentage();
		else
			SOC = previous_SOC;
	}

	ZCV = battery_meter_get_battery_zcv();

	BMT_status.ICharging = mt_battery_average_method(BATTERY_AVG_CURRENT,
	&batteryCurrentBuffer[0], ICharging, &icharging_sum, batteryIndex);

	if (previous_SOC == -1 &&
		bat_vol <= batt_cust_data.v_0percent_tracking) {
		pr_notice(
			    "battery voltage too low, use ZCV to init average data.\n");
		BMT_status.bat_vol = mt_battery_average_method(BATTERY_AVG_VOLT,
			&batteryVoltageBuffer[0], ZCV, &bat_sum, batteryIndex);
	} else
		BMT_status.bat_vol = mt_battery_average_method(
			BATTERY_AVG_VOLT, &batteryVoltageBuffer[0],
			bat_vol, &bat_sum, batteryIndex);

	if (battery_cmd_thermal_test_mode == 1)
		pr_notice("test mode , battery temperature is fixed.\n");
	else
		BMT_status.temperature =
			filter_battery_temperature(temperature);

	BMT_status.Vsense = Vsense;
	BMT_status.charger_vol = charger_vol;
	BMT_status.temperatureV = temperatureV;
	BMT_status.temperatureR = temperatureR;
	BMT_status.SOC = SOC;
	BMT_status.ZCV = ZCV;

#if !defined(CUST_CAPACITY_OCV2CV_TRANSFORM)
	if (BMT_status.charger_exist == false) {
		if (BMT_status.SOC > previous_SOC && previous_SOC >= 0)
			BMT_status.SOC = previous_SOC;
	}
#endif

	batteryIndex++;
	if (batteryIndex >= BATTERY_AVERAGE_SIZE)
		batteryIndex = 0;


	if (g_battery_soc_ready == false)
		g_battery_soc_ready = true;

	pr_notice(
		    "AvgVbat=(%d),bat_vol=(%d),AvgI=(%d),I=(%d),VChr=(%d),AvgT=(%d),T=(%d),pre_SOC=(%d),SOC=(%d),ZCV=(%d)\n",
		    BMT_status.bat_vol, bat_vol, BMT_status.ICharging,
			ICharging, BMT_status.charger_vol,
			BMT_status.temperature, temperature,
		    previous_SOC, BMT_status.SOC, BMT_status.ZCV);

	previous_SOC = BMT_status.SOC;
}

static unsigned int mt_battery_CheckBatteryTemp(void)
{
	unsigned int status = PMU_STATUS_OK;

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)

	pr_debug("[BATTERY] support JEITA, temperature=%d\n",
		    BMT_status.temperature);

	if (do_jeita_state_machine() == PMU_STATUS_FAIL) {
		pr_notice("[BATTERY] JEITA : fail\n");
		status = PMU_STATUS_FAIL;
		g_batt_temp_status = TEMP_POS_LOW;
	} else if (g_batt_temp_status == TEMP_POS_LOW ||
				g_batt_temp_status == TEMP_POS_HIGH) {
		g_batt_temp_status = TEMP_POS_NORMAL;
		BMT_status.bat_charging_state = CHR_PRE;
	}
#else


	if (batt_cust_data.mtk_temperature_recharge_support) {
		if (do_batt_temp_state_machine() == PMU_STATUS_FAIL) {
			pr_notice("[BATTERY] Batt temp check : fail\n");
			status = PMU_STATUS_FAIL;
		}
	} else {
#ifdef BAT_LOW_TEMP_PROTECT_ENABLE
		if ((BMT_status.temperature <
			batt_cust_data.min_charge_temperature)
		    || (BMT_status.temperature == ERR_CHARGE_TEMPERATURE)) {
			pr_notice(
				    "[BATTERY] Battery Under Temperature or NTC fail !!\n\r");
			status = PMU_STATUS_FAIL;
		}
#endif
		if (BMT_status.temperature >=
			batt_cust_data.max_charge_temperature) {
			pr_notice("[BATTERY] Battery Over Temperature !!\n\r");
			status = PMU_STATUS_FAIL;
		}
	}
#endif

	return status;
}


static unsigned int mt_battery_CheckChargerVoltage(void)
{
	unsigned int status = PMU_STATUS_OK;

	if (BMT_status.charger_exist == true) {

		if (batt_cust_data.v_charger_enable) {
			if (BMT_status.charger_vol <=
				batt_cust_data.v_charger_min) {
				pr_notice("[BATTERY]Charger under voltage!!\r\n");
				BMT_status.bat_charging_state = CHR_ERROR;
				status = PMU_STATUS_FAIL;
			}
		}

		if (BMT_status.charger_vol >= batt_cust_data.v_charger_max) {
			pr_notice("[BATTERY]Charger over voltage !!\r\n");
			BMT_status.charger_protect_status = charger_OVER_VOL;
			BMT_status.bat_charging_state = CHR_ERROR;
			status = PMU_STATUS_FAIL;
		}
	}

	return status;
}


static unsigned int mt_battery_CheckChargingTime(void)
{
	unsigned int status = PMU_STATUS_OK;

	if ((g_battery_thermal_throttling_flag == 2) ||
		(g_battery_thermal_throttling_flag == 3)) {
		pr_debug(
		    "[TestMode] Disable Safety Timer. bat_tt_enable=%d, bat_thr_test_mode=%d, bat_thr_test_value=%d\n",
		    g_battery_thermal_throttling_flag,
		    battery_cmd_thermal_test_mode,
			battery_cmd_thermal_test_mode_value);
	} else {
		/* Charging OT */
		if (BMT_status.total_charging_time >=
			batt_cust_data.max_charging_time) {
			pr_notice("[BATTERY] Charging Over Time.\n");

			status = PMU_STATUS_FAIL;
		}
	}

	return status;

}

#if defined(STOP_CHARGING_IN_TAKLING)
static unsigned int mt_battery_CheckCallState(void)
{
	unsigned int status = PMU_STATUS_OK;

	if ((g_call_state == CALL_ACTIVE)
	    && (BMT_status.bat_vol > batt_cust_data.v_cc2topoff_thres))
		status = PMU_STATUS_FAIL;

	return status;
}
#endif

#if defined(CONFIG_MTK_MULTI_BAT_PROFILE_SUPPORT) && \
	defined(MTK_GET_BATTERY_ID_BY_AUXADC)
static unsigned int mt_battery_CheckBatteryConnect(void)
{
	unsigned int status = PMU_STATUS_OK;

	if (false == get_battery_id_status()) {
		pr_notice("[BATTERY] Battery ID disconnect.\n");
		status = PMU_STATUS_FAIL;
	}

	return status;
}
#endif

static void mt_battery_CheckBatteryStatus(void)
{
	pr_debug("[mt_battery_CheckBatteryStatus] cmd_discharging=(%d)\n",
		    cmd_discharging);
	if (cmd_discharging == 1) {
		pr_notice(
			    "[mt_battery_CheckBatteryStatus] cmd_discharging=(%d)\n",
			    cmd_discharging);
		BMT_status.bat_charging_state = CHR_ERROR;
		battery_charging_control(CHARGING_CMD_SET_ERROR_STATE,
			&cmd_discharging);
		return;
	} else if (cmd_discharging == 0) {
		BMT_status.bat_charging_state = CHR_PRE;
		battery_charging_control(CHARGING_CMD_SET_ERROR_STATE,
			&cmd_discharging);
		cmd_discharging = -1;
	}
	if (mt_battery_CheckBatteryTemp() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_ERROR;
		return;
	}

	if (mt_battery_CheckChargerVoltage() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_ERROR;
		return;
	}
#if defined(STOP_CHARGING_IN_TAKLING)
	if (mt_battery_CheckCallState() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_HOLD;
		return;
	}
#endif

	if (mt_battery_CheckChargingTime() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_ERROR;
		return;
	}

#if defined(CONFIG_MTK_MULTI_BAT_PROFILE_SUPPORT) && \
	defined(MTK_GET_BATTERY_ID_BY_AUXADC)
	if (mt_battery_CheckBatteryConnect() != PMU_STATUS_OK) {
		BMT_status.bat_charging_state = CHR_ERROR;
		return;
	}
#endif
}


static void mt_battery_notify_TotalChargingTime_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME)
	if ((g_battery_thermal_throttling_flag == 2) ||
		(g_battery_thermal_throttling_flag == 3)) {
		pr_debug("[TestMode] Disable Safety Timer : no UI display\n");
	} else {
		if (BMT_status.total_charging_time >=
			batt_cust_data.max_charging_time) {
			g_BatteryNotifyCode |= 0x0010;
			pr_notice("[BATTERY] Charging Over Time\n");
		} else
			g_BatteryNotifyCode &= ~(0x0010);
	}

	pr_notice(
		"[BATTERY] BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME (%x)\n",
		g_BatteryNotifyCode);
#endif
}


static void mt_battery_notify_VBat_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0004_VBAT)
	if (BMT_status.bat_vol > 4350) {
		g_BatteryNotifyCode |= 0x0008;
		pr_debug("[BATTERY] bat_vlot(%d) > 4350mV\n",
			BMT_status.bat_vol);
	} else
		g_BatteryNotifyCode &= ~(0x0008);

	pr_debug("[BATTERY] BATTERY_NOTIFY_CASE_0004_VBAT (%x)\n",
		g_BatteryNotifyCode);

#endif
}


static void mt_battery_notify_ICharging_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0003_ICHARGING)
	if ((BMT_status.ICharging > 1000) &&
	    (BMT_status.total_charging_time > 300)) {
		g_BatteryNotifyCode |= 0x0004;
		pr_debug("[BATTERY] I_charging(%ld) > 1000mA\n",
			BMT_status.ICharging);
	} else
		g_BatteryNotifyCode &= ~(0x0004);

	pr_debug("[BATTERY] BATTERY_NOTIFY_CASE_0003_ICHARGING (%x)\n",
		g_BatteryNotifyCode);

#endif
}

static void mt_battery_notify_VBatTemp_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0002_VBATTEMP)

	if (BMT_status.temperature >=
		batt_cust_data.max_charge_temperature) {
		g_BatteryNotifyCode |= 0x0002;
		pr_notice("[BATTERY] bat_temp(%d) out of range(too high)\n",
			    BMT_status.temperature);
	}
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	else if (BMT_status.temperature < TEMP_NEG_10_THRESHOLD) {
		g_BatteryNotifyCode |= 0x0020;
		pr_notice("[BATTERY] bat_temp(%d) out of range(too low)\n",
			    BMT_status.temperature);
	}
#else
#ifdef BAT_LOW_TEMP_PROTECT_ENABLE
	else if (BMT_status.temperature < MIN_CHARGE_TEMPERATURE) {
		g_BatteryNotifyCode |= 0x0020;
		pr_notice("[BATTERY] bat_temp(%d) out of range(too low)\n",
			    BMT_status.temperature);
	}
#endif
#endif

	pr_debug("[BATTERY] BATTERY_NOTIFY_CASE_0002_VBATTEMP (%x)\n",
		    g_BatteryNotifyCode);

#endif
}


static void mt_battery_notify_VCharger_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0001_VCHARGER)
	if (BMT_status.charger_vol > batt_cust_data.v_charger_max) {
		g_BatteryNotifyCode |= 0x0001;
		pr_notice("[BATTERY] BMT_status.charger_vol(%d) > %d mV\n",
			BMT_status.charger_vol, batt_cust_data.v_charger_max);
	} else
		g_BatteryNotifyCode &= ~(0x0001);

	pr_debug("[BATTERY] BATTERY_NOTIFY_CASE_0001_VCHARGER (%x)\n",
		g_BatteryNotifyCode);
#endif
}

static void mt_battery_notify_UI_test(void)
{
	if (g_BN_TestMode == 0x0001) {
		g_BatteryNotifyCode = 0x0001;
		pr_debug("[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0001_VCHARGER\n");
	} else if (g_BN_TestMode == 0x0002) {
		g_BatteryNotifyCode = 0x0002;
		pr_debug("[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0002_VBATTEMP\n");
	} else if (g_BN_TestMode == 0x0003) {
		g_BatteryNotifyCode = 0x0004;
		pr_debug("[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0003_ICHARGING\n");
	} else if (g_BN_TestMode == 0x0004) {
		g_BatteryNotifyCode = 0x0008;
		pr_debug("[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0004_VBAT\n");
	} else if (g_BN_TestMode == 0x0005) {
		g_BatteryNotifyCode = 0x0010;
		pr_debug("[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME\n");
	} else {
		pr_debug("[BATTERY] Unknown BN_TestMode Code : %x\n",
			g_BN_TestMode);
	}
}

void mt_battery_notify_check(void)
{
	g_BatteryNotifyCode = 0x0000;

	if (g_BN_TestMode == 0x0000) {
		/* for normal case */
		pr_debug("[BATTERY] mt_battery_notify_check\n");
		mt_battery_notify_VCharger_check();
		mt_battery_notify_VBatTemp_check();
		mt_battery_notify_ICharging_check();
		mt_battery_notify_VBat_check();
		mt_battery_notify_TotalChargingTime_check();
	} else
		mt_battery_notify_UI_test();
}

static void mt_battery_thermal_check(void)
{
	if ((g_battery_thermal_throttling_flag == 1) ||
		(g_battery_thermal_throttling_flag == 3)) {
		if (battery_cmd_thermal_test_mode == 1) {
			BMT_status.temperature =
				battery_cmd_thermal_test_mode_value;
			pr_debug("[Battery] In thermal_test_mode , Tbat=%d\n",
				BMT_status.temperature);
		}
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		/* ignore default rule */
		if (BMT_status.temperature <= -10) {
			pr_notice("[Battery] Tbat(%d)<= -10, system need power down.\n",
				BMT_status.temperature);
#ifdef CONFIG_AMAZON_SIGN_OF_LIFE
			life_cycle_set_thermal_shutdown_reason(
				THERMAL_SHUTDOWN_REASON_BATTERY);
#endif
			orderly_poweroff(true);
		}
#else
		if (BMT_status.temperature >= 60) {
#if defined(CONFIG_POWER_EXT)
			pr_notice(
				"[BATTERY] CONFIG_POWER_EXT, no update battery update power down.\n");
#else
{
			if ((g_platform_boot_mode == META_BOOT)
				|| (g_platform_boot_mode == ADVMETA_BOOT)
				|| (g_platform_boot_mode == ATE_FACTORY_BOOT)) {
				pr_debug(
						"[BATTERY] boot mode = %d, bypass temperature check\n",
						g_platform_boot_mode);
			} else {
				struct battery_data *bat_data = &battery_main;
				struct power_supply *bat_psy = bat_data->psy;

				pr_err(
					"[Battery] Tbat(%d)>=60, system need power down.\n",
					BMT_status.temperature);

				bat_data->BAT_CAPACITY = 0;
				power_supply_changed(bat_psy);

				if (BMT_status.charger_exist == true)
					battery_charging_control(
					CHARGING_CMD_SET_PLATFORM_RESET, NULL);

				/* avoid SW no feedback */
				battery_charging_control(
					CHARGING_CMD_SET_POWER_OFF, NULL);
			}
}
#endif
		}
#endif
	}
}

static void mt_battery_update_status(void)
{
#if defined(CONFIG_POWER_EXT)
	pr_notice("[BATTERY] CONFIG_POWER_EXT, no update Android.\n");
#else
	{
		if (skip_battery_update == false) {
			pr_debug("mt_battery_update_status.\n");
			usb_update(&usb_main);
			ac_update(&ac_main);
			wireless_update(&wireless_main);
			battery_update(&battery_main);
		} else {
			pr_debug("skip mt_battery_update_status.\n");
			skip_battery_update = false;
		}
	}

#endif
}

unsigned int mt_charger_type_detection(void)
{
	unsigned int CHR_Type_num = CHARGER_UNKNOWN;

	mutex_lock(&charger_type_mutex);

#if defined(CONFIG_MTK_WIRELESS_CHARGER_SUPPORT)
	battery_charging_control(
		CHARGING_CMD_GET_CHARGER_TYPE, &CHR_Type_num);
	BMT_status.charger_type = CHR_Type_num;
#else
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	if (BMT_status.charger_type == CHARGER_UNKNOWN) {
#else
	if ((BMT_status.charger_type == CHARGER_UNKNOWN) &&
	    (DISO_data.diso_state.cur_vusb_state == DISO_ONLINE)) {
#endif
		battery_charging_control(
			CHARGING_CMD_GET_CHARGER_TYPE, &CHR_Type_num);
		BMT_status.charger_type = CHR_Type_num;
	}
#endif
	mutex_unlock(&charger_type_mutex);

	return BMT_status.charger_type;
}

unsigned int mt_get_charger_type(void)
{
#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
	return STANDARD_HOST;
#else
	return BMT_status.charger_type;
#endif
}

#if defined(CONFIG_OF)
static void __batt_parse_node(const struct device_node *np,
		const char *node_srting, int *cust_val);
static int mt_battery_restore_current_setting(void)
{
	struct device_node *np = NULL;

	if (!batt_cust_data.aicl_enable)
		return 0;

	np = of_find_compatible_node(NULL, NULL, "mediatek,battery");
	if (!np) {
		pr_notice("Failed to find device-tree node: bat_comm\n");
		return -ENODEV;
	}

	__batt_parse_node(np, "ac_charger_input_current",
			&batt_cust_data.ac_charger_input_current);

	__batt_parse_node(np, "ac_charger_current",
			&batt_cust_data.ac_charger_current);

	return 0;
}
#else
static int mt_battery_restore_current_setting(void)
{
	return 0;
}
#endif

static void mt_battery_charger_detect_check(void)
{
	static bool chr_type_debounce;

	if (upmu_is_chr_det() == true) {
		__pm_stay_awake(battery_suspend_lock);
		BMT_status.charger_exist = true;

		if ((false == chr_type_debounce)
			&& (BMT_status.charger_type == NONSTANDARD_CHARGER)) {
			chr_type_debounce = true;
			BMT_status.charger_type = CHARGER_UNKNOWN;
			pr_debug("[%s] Chr Type Detect Again!\r\n", __func__);
		}

#if defined(CONFIG_MTK_WIRELESS_CHARGER_SUPPORT)
		mt_charger_type_detection();

		if ((BMT_status.charger_type == STANDARD_HOST)
		    || (BMT_status.charger_type == CHARGING_HOST)) {
			mt_usb_connect();
		}
#else
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		if (BMT_status.charger_type == CHARGER_UNKNOWN) {
#else
		if ((BMT_status.charger_type == CHARGER_UNKNOWN) &&
		    (DISO_data.diso_state.cur_vusb_state == DISO_ONLINE)) {
#endif
			getrawmonotonic(&chr_plug_in_time);
			g_custom_plugin_time = 0;
			g_custom_charging_cv = -1;
			bat_metrics_top_off_mode(false, 0);

			mt_charger_type_detection();

			if ((BMT_status.charger_type == STANDARD_HOST)
			    || (BMT_status.charger_type == CHARGING_HOST)) {
				mt_usb_connect();
			}
		}
#endif

		pr_notice("[BAT_thread]Cable in, CHR_Type_num=%d\r\n",
			    BMT_status.charger_type);
	} else {
		__pm_relax(battery_suspend_lock);

		BMT_status.charger_exist = false;
		BMT_status.charger_type = CHARGER_UNKNOWN;
		BMT_status.bat_full = false;
		BMT_status.bat_in_recharging_state = false;
		BMT_status.bat_charging_state = CHR_PRE;
		BMT_status.total_charging_time = 0;
		BMT_status.PRE_charging_time = 0;
		BMT_status.CC_charging_time = 0;
		BMT_status.TOPOFF_charging_time = 0;
		BMT_status.POSTFULL_charging_time = 0;
		chr_type_debounce = false;
		BMT_status.aicl_done = false;
		if (BMT_status.ap15_charger_detected) {
			BMT_status.ap15_charger_detected = false;
			BMT_status.aicl_result = 0;
			mt_battery_restore_current_setting();
		}
		pr_notice("[BAT_thread]Cable out \r\n");

		mt_usb_disconnect();
	}

	bat_metrics_chrdet(BMT_status.charger_type);
}

static void mt_kpoc_power_off_check(void)
{
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	pr_debug(
		    "[mt_kpoc_power_off_check] , chr_vol=%d, boot_mode=%d\r\n",
		    BMT_status.charger_vol, g_platform_boot_mode);
	if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
	    || g_platform_boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		if ((upmu_is_chr_det() == false) &&
			(BMT_status.charger_vol < 2500)) {
			/* vbus < 2.5V */
			pr_notice(
				    "[bat_thread_kthread] Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
			battery_charging_control(
				CHARGING_CMD_SET_POWER_OFF, NULL);
		}
	}
#endif
}

static void mt_otg_check(void)
{
	int vbus_stat, otg_en = 0;

	/*
	 * Workabound for abnormal otg status
	 * If OTG status in charger IC and USB mismatch when VBUS is valid,
	 * that could be a HW issue of charger IC, or someone set OTG status
	 * directly without OTG driver.
	 * Clean OTG stauts of charger IC.
	 */
	if (upmu_get_rgs_chrdet()) {
		battery_charging_control(
			CHARGING_CMD_GET_VBUS_STAT, &vbus_stat);
		if ((vbus_stat == VBUS_STAT_OTG) && mt_usb_is_device()) {
			pr_err("%s: incorrect OTG status, clean it and ignore vubs event\n",
				__func__);
			battery_charging_control(
				CHARGING_CMD_BOOST_ENABLE, &otg_en);
		}
	}
}

void update_battery_2nd_info(int status_smb, int capacity_smb, int present_smb)
{
#if defined(CONFIG_POWER_VERIFY)
	pr_debug("[update_battery_smb_info] no support\n");
#else
	g_status_smb = status_smb;
	g_capacity_smb = capacity_smb;
	g_present_smb = present_smb;
	pr_debug(
		    "[update_battery_smb_info] get status_smb=%d,capacity_smb=%d,present_smb=%d\n",
		    status_smb, capacity_smb, present_smb);

	wake_up_bat();
	g_smartbook_update = 1;
#endif
}

#ifdef CONFIG_USB_AMAZON_DOCK
extern void musb_rerun_dock_detection(void);
#endif
void do_chrdet_int_task(void)
{
	if (g_bat_init_flag == true) {

#ifdef CONFIG_USB_AMAZON_DOCK
		/* Recheck for unpowered dock */
		musb_rerun_dock_detection();
#endif

		if (upmu_is_chr_det() == true) {
			pr_notice("[do_chrdet_int_task] charger exist!\n");
			BMT_status.charger_exist = true;
			bat_metrics_vbus(true);
			if (!is_usb_rdy()) {
				usb_update(&usb_main);
				ac_update(&ac_main);
			}

			__pm_stay_awake(battery_suspend_lock);

#if defined(CONFIG_POWER_EXT)
			mt_usb_connect();
			pr_notice(
				    "[do_chrdet_int_task] call mt_usb_connect() in EVB\n");
#elif defined(CONFIG_MTK_POWER_EXT_DETECT)
			if (true == bat_is_ext_power()) {
				mt_usb_connect();
				pr_notice(
					    "[do_chrdet_int_task] call mt_usb_connect() in EVB\n");
				return;
			}
#endif
		} else {
			pr_notice("[do_chrdet_int_task] charger NOT exist!\n");

			BMT_status.charger_exist = false;
			bat_metrics_vbus(false);
			BMT_status.force_trigger_charging = true;
			if (!is_usb_rdy()) {
				usb_update(&usb_main);
				ac_update(&ac_main);
			}
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
			pr_notice(
				    "turn off charging for no available charging source\n");
			battery_charging_control(
				CHARGING_CMD_ENABLE, &BMT_status.charger_exist);
#endif

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
			if (g_platform_boot_mode ==
				KERNEL_POWER_OFF_CHARGING_BOOT
			    || g_platform_boot_mode ==
				LOW_POWER_OFF_CHARGING_BOOT) {
				pr_notice(
					    "[pmic_thread_kthread] Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
				battery_charging_control(
					CHARGING_CMD_SET_POWER_OFF, NULL);
			}
#endif

			__pm_relax(battery_suspend_lock);

#if defined(CONFIG_POWER_EXT)
			mt_usb_disconnect();
			pr_notice(
				    "[do_chrdet_int_task] call mt_usb_disconnect() in EVB\n");
#elif defined(CONFIG_MTK_POWER_EXT_DETECT)
			if (true == bat_is_ext_power()) {
				mt_usb_disconnect();
				pr_notice(
					    "[do_chrdet_int_task] call mt_usb_disconnect() in EVB\n");
				return;
			}
#endif
		}

		/* Place charger detection and battery update
		 * here is used to speed up charging icon display.
		 */

		/* Put time-consuming job in kthread
		 * instead of INT context
		 */
		/* mt_battery_charger_detect_check(); */
		if (BMT_status.UI_SOC == 100 &&
			BMT_status.charger_exist == true) {
			BMT_status.bat_charging_state = CHR_BATFULL;
			BMT_status.bat_full = true;
			g_charging_full_reset_bat_meter = true;
		}

		if (g_battery_soc_ready == false) {
			if (BMT_status.nPercent_ZCV == 0)
				battery_meter_initial();

			BMT_status.SOC = battery_meter_get_battery_percentage();
		}

		if (BMT_status.bat_vol > 0) {
			/* Put time-consuming job in
			 * kthread instead of INT context
			 */
			/* mt_battery_update_status(); */
			/* skip_battery_update = true; */

		}
		wake_up_bat();
	} else
		pr_debug(
			    "[do_chrdet_int_task] battery thread not ready, will do after bettery init.\n");
}


void BAT_thread(void)
{
	static bool battery_meter_initilized;
	struct timespec now_time;
	unsigned long total_time_plug_in;

	if (battery_meter_initilized == false) {
		/* move from battery_probe() to decrease booting time */
		battery_meter_initial();
		BMT_status.nPercent_ZCV =
			battery_meter_get_battery_nPercent_zcv();
		battery_meter_initilized = true;
	}

	mt_otg_check();
	mt_battery_charger_detect_check();
	if (fg_battery_shutdown)
		return;

	mt_battery_GetBatteryData();
	if (fg_battery_shutdown)
		return;

	if (BMT_status.charger_exist == true)
		check_battery_exist();

	mt_battery_thermal_check();
	mt_battery_notify_check();

	if (BMT_status.charger_exist == true && !fg_battery_shutdown) {
		getrawmonotonic(&now_time);
		total_time_plug_in =
			(now_time.tv_sec - chr_plug_in_time.tv_sec)
			+ g_custom_plugin_time;

		if (total_time_plug_in > PLUGIN_THRESHOLD) {
			g_custom_charging_cv = BATTERY_VOLT_04_100000_V;
			bat_metrics_top_off_mode(true, total_time_plug_in);

			if (!bat_7days_flag)
				bat_7days_flag = true;
		}

		if (total_time_plug_in <= PLUGIN_THRESHOLD && bat_7days_flag)
			bat_7days_flag = false;

		if (g_custom_charging_mode != 1 && bat_demo_flag)
			bat_demo_flag = false;

		if (g_custom_charging_mode == 1 && !bat_demo_flag) {
			bat_demo_flag = true;
			bat_metrics_demo_mode(true, total_time_plug_in);
		}
		pr_notice("total_time_plug_in(%lu), cv(%d)\r\n",
			total_time_plug_in, g_custom_charging_cv);

		mt_battery_CheckBatteryStatus();
		mt_battery_charging_algorithm();
	}

	mt_battery_update_status();
	mt_kpoc_power_off_check();
}

/* /////////////////
 * // Internal API
 * ////////////////
 */

int bat_thread_kthread(void *x)
{
	ktime_t ktime = ktime_set(3, 0);	/* 10s, 10* 1000 ms */

	if (is_usb_rdy() == false) {
		pr_notice("wait for usb ready, block\n");
		wait_event(bat_thread_wq, (is_usb_rdy() == true));
		pr_notice("usb ready, free\n");
	} else {
		pr_notice("usb ready, PASS\n");
	}

	/* Run on a process content */
	while (!fg_battery_shutdown) {
		mutex_lock(&bat_mutex);

		if (((chargin_hw_init_done == true) &&
			(battery_suspended == false))
		    || ((chargin_hw_init_done == true) &&
			(fg_wake_up_bat == true)))
			BAT_thread();

		mutex_unlock(&bat_mutex);

#ifdef FG_BAT_INT
		if (fg_wake_up_bat == true) {
			__pm_relax(battery_fg_lock);
			fg_wake_up_bat = false;
			pr_debug("unlock battery_fg_lock\n");
		}
#endif				/* #ifdef FG_BAT_INT */

		pr_debug("wait event\n");

		wait_event(bat_thread_wq,
			(bat_thread_timeout == true) &&
			((battery_suspended == false) ||
			(fg_wake_up_bat == true)));
		pr_debug("wait_event(bat_thread_wq) OK!\n");

		bat_thread_timeout = false;
		hrtimer_start(&battery_kthread_timer, ktime, HRTIMER_MODE_REL);
		if (!fg_battery_shutdown)
		ktime = ktime_set(BAT_TASK_PERIOD, 0);
		if (chr_wake_up_bat == true && g_smartbook_update != 1) {
			/* for charger plug in/ out */
			g_smartbook_update = 0;
#if defined(CUST_CAPACITY_OCV2CV_TRANSFORM)
			battery_meter_set_reset_soc(false);
#endif
			battery_meter_reset();
			chr_wake_up_bat = false;

			pr_debug(
				    "[BATTERY] Charger plug in/out, Call battery_meter_reset. (%d)\n",
				    BMT_status.UI_SOC);
		}

	}

	mutex_lock(&bat_mutex);
	fg_bat_thread = true;
	mutex_unlock(&bat_mutex);

	return 0;
}

void bat_thread_wakeup(void)
{
	pr_debug("******** battery : bat_thread_wakeup  ********\n");

	bat_thread_timeout = true;
	bat_meter_timeout = true;
#ifdef MTK_ENABLE_AGING_ALGORITHM
	suspend_time = 0;
#endif
	_g_bat_sleep_total_time = 0;
	wake_up(&bat_thread_wq);
}

/* ///////////////////////////////////////////////////////////////
 *     fop API
 * ///////////////////////////////////////////////////////////////
 */
static long adc_cali_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int *user_data_addr;
	int *naram_data_addr;
	int i = 0;
	int ret = 0;
	int adc_in_data[2] = {1, 1};
	int adc_out_data[2] = {1, 1};

	mutex_lock(&bat_mutex);

	switch (cmd) {
	case TEST_ADC_CALI_PRINT:
		g_ADC_Cali = false;
		break;

	case SET_ADC_CALI_Slop:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_slop, naram_data_addr, 36);
		g_ADC_Cali = false;
		/* enable calibration after setting ADC_CALI_Cal
		 * Protection
		 */
		for (i = 0; i < 14; i++) {
			if ((*(adc_cali_slop + i) == 0) ||
			    (*(adc_cali_slop + i) == 1))
				*(adc_cali_slop + i) = 1000;
		}
		for (i = 0; i < 14; i++)
			pr_notice("adc_cali_slop[%d] = %d\n", i,
				    *(adc_cali_slop + i));
		pr_debug("**** unlocked_ioctl : SET_ADC_CALI_Slop Done!\n");
		break;

	case SET_ADC_CALI_Offset:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_offset, naram_data_addr, 36);
		g_ADC_Cali = false;
		for (i = 0; i < 14; i++)
			pr_notice("adc_cali_offset[%d] = %d\n", i,
				    *(adc_cali_offset + i));
		pr_debug("**** unlocked_ioctl : SET_ADC_CALI_Offset Done!\n");
		break;

	case SET_ADC_CALI_Cal:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_cal, naram_data_addr, 4);
		g_ADC_Cali = true;
		if (adc_cali_cal[0] == 1)
			g_ADC_Cali = true;
		else
			g_ADC_Cali = false;

		for (i = 0; i < 1; i++)
			pr_notice("adc_cali_cal[%d] = %d\n", i,
				    *(adc_cali_cal + i));
		pr_debug(
			    "**** unlocked_ioctl : SET_ADC_CALI_Cal Done!\n");
		break;

	case ADC_CHANNEL_READ:
		user_data_addr = (int *)arg;
		/* 2*int = 2*4 */
		ret = copy_from_user(adc_in_data, user_data_addr, 8);

		if (adc_in_data[0] == 0) /* I_SENSE */
			adc_out_data[0] =
				battery_meter_get_VSense() * adc_in_data[1];
		else if (adc_in_data[0] == 1) /* BAT_SENSE */
			adc_out_data[0] =
				battery_meter_get_battery_voltage(true) *
				adc_in_data[1];
		else if (adc_in_data[0] == 3) /* V_Charger */
			adc_out_data[0] = battery_meter_get_charger_voltage() *
					  adc_in_data[1];
		else if (adc_in_data[0] == 30) /* V_Bat_temp magic number */
			adc_out_data[0] =
				battery_meter_get_battery_temperature() *
				adc_in_data[1];
		else if (adc_in_data[0] == 66) {
			adc_out_data[0] =
				(battery_meter_get_battery_current()) / 10;

			if (battery_meter_get_battery_current_sign() == true)
				adc_out_data[0] =
					0 - adc_out_data[0]; /* charging */

		} else
			pr_debug("unknown channel(%d,%d)\n",
				adc_in_data[0], adc_in_data[1]);

		if (adc_out_data[0] < 0)
			adc_out_data[1] = 1; /* failed */
		else
			adc_out_data[1] = 0; /* success */

		if (adc_in_data[0] == 30)
			adc_out_data[1] = 0; /* success */

		if (adc_in_data[0] == 66)
			adc_out_data[1] = 0; /* success */

		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		pr_debug("**** unlocked_ioctl : Channel %d * %d times = %d\n",
			adc_in_data[0], adc_in_data[1], adc_out_data[0]);
		break;

	case BAT_STATUS_READ:
		user_data_addr = (int *)arg;
		ret = copy_from_user(battery_in_data, user_data_addr, 4);
		/* [0] is_CAL */
		if (g_ADC_Cali)
			battery_out_data[0] = 1;
		else
			battery_out_data[0] = 0;

		ret = copy_to_user(user_data_addr, battery_out_data, 4);
		pr_debug("**** unlocked_ioctl : CAL:%d\n", battery_out_data[0]);
		break;

	case Set_Charger_Current: /* For Factory Mode */
		user_data_addr = (int *)arg;
		ret = copy_from_user(charging_level_data, user_data_addr, 4);
		g_ftm_battery_flag = true;
		if (charging_level_data[0] == 0)
			charging_level_data[0] = CHARGE_CURRENT_70_00_MA;
		else if (charging_level_data[0] == 1)
			charging_level_data[0] = CHARGE_CURRENT_200_00_MA;
		else if (charging_level_data[0] == 2)
			charging_level_data[0] = CHARGE_CURRENT_400_00_MA;
		else if (charging_level_data[0] == 3)
			charging_level_data[0] = CHARGE_CURRENT_450_00_MA;
		else if (charging_level_data[0] == 4)
			charging_level_data[0] = CHARGE_CURRENT_550_00_MA;
		else if (charging_level_data[0] == 5)
			charging_level_data[0] = CHARGE_CURRENT_650_00_MA;
		else if (charging_level_data[0] == 6)
			charging_level_data[0] = CHARGE_CURRENT_700_00_MA;
		else if (charging_level_data[0] == 7)
			charging_level_data[0] = CHARGE_CURRENT_800_00_MA;
		else if (charging_level_data[0] == 8)
			charging_level_data[0] = CHARGE_CURRENT_900_00_MA;
		else if (charging_level_data[0] == 9)
			charging_level_data[0] = CHARGE_CURRENT_1000_00_MA;
		else if (charging_level_data[0] == 10)
			charging_level_data[0] = CHARGE_CURRENT_1100_00_MA;
		else if (charging_level_data[0] == 11)
			charging_level_data[0] = CHARGE_CURRENT_1200_00_MA;
		else if (charging_level_data[0] == 12)
			charging_level_data[0] = CHARGE_CURRENT_1300_00_MA;
		else if (charging_level_data[0] == 13)
			charging_level_data[0] = CHARGE_CURRENT_1400_00_MA;
		else if (charging_level_data[0] == 14)
			charging_level_data[0] = CHARGE_CURRENT_1500_00_MA;
		else if (charging_level_data[0] == 15)
			charging_level_data[0] = CHARGE_CURRENT_1600_00_MA;
		else
			charging_level_data[0] = CHARGE_CURRENT_450_00_MA;

		wake_up_bat();
		pr_debug("**** unlocked_ioctl : set_Charger_Current:%d\n",
			charging_level_data[0]);
		break;
	/* add for meta tool------------------------------- */
	case Get_META_BAT_VOL:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = BMT_status.bat_vol;
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		break;
	case Get_META_BAT_SOC:
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);
		adc_out_data[0] = BMT_status.UI_SOC;
		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		break;
	/* add bing meta tool------------------------------- */

	default:
		g_ADC_Cali = false;
		break;
	}

	mutex_unlock(&bat_mutex);

	return 0;
}

static int adc_cali_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int adc_cali_release(struct inode *inode, struct file *file)
{
	return 0;
}


static const struct file_operations adc_cali_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = adc_cali_ioctl,
	.open = adc_cali_open,
	.release = adc_cali_release,
};


void check_battery_exist(void)
{
#if defined(CONFIG_DIS_CHECK_BATTERY) || defined(CONFIG_POWER_EXT)
	pr_notice("[BATTERY] Disable check battery exist.\n");
#else
	unsigned int baton_count = 0;
	unsigned int charging_enable = false;
	unsigned int battery_status;
	unsigned int i;

	for (i = 0; i < 3; i++) {
		battery_charging_control(
			CHARGING_CMD_GET_BATTERY_STATUS, &battery_status);
		baton_count += battery_status;

	}

	if (baton_count >= 3) {
		if ((g_platform_boot_mode == META_BOOT) ||
			(g_platform_boot_mode == ADVMETA_BOOT)
		    || (g_platform_boot_mode == ATE_FACTORY_BOOT)) {
			pr_debug(
				"[BATTERY] boot mode = %d, bypass battery check\n",
				g_platform_boot_mode);
		} else {
			pr_notice(
				"[BATTERY] Battery is not exist, power off system (%d)\n",
				baton_count);

			battery_charging_control(
				CHARGING_CMD_ENABLE, &charging_enable);
			battery_charging_control(
				CHARGING_CMD_SET_POWER_OFF, NULL);
			if (BMT_status.charger_exist == true)
				battery_charging_control(
				CHARGING_CMD_SET_PLATFORM_RESET, NULL);
		}
	}
#endif
}

int charger_hv_detect_sw_thread_handler(void *unused)
{
	ktime_t ktime;
	unsigned int charging_enable;
	unsigned int hv_voltage = batt_cust_data.v_charger_max * 1000;
	bool hv_status = false;

	do {
		ktime = ktime_set(0, BAT_MS_TO_NS(1000));

		if (chargin_hw_init_done)
			battery_charging_control(
			CHARGING_CMD_SET_HV_THRESHOLD, &hv_voltage);

		wait_event_interruptible(charger_hv_detect_waiter,
					 (charger_hv_detect_flag == true));

		if (fg_battery_shutdown)
			break;

		if (upmu_is_chr_det() == true)
			check_battery_exist();

		charger_hv_detect_flag = false;

		if (fg_battery_shutdown)
			break;

		if (chargin_hw_init_done)
			battery_charging_control(
			CHARGING_CMD_GET_HV_STATUS, &hv_status);

		if (fg_battery_shutdown)
			break;

		if (hv_status == true) {
			pr_err("[charger_hv_detect_sw_thread_handler] charger hv\n");

			charging_enable = false;
			if (chargin_hw_init_done)
				battery_charging_control(
				CHARGING_CMD_ENABLE, &charging_enable);
		}

		if (fg_battery_shutdown)
			break;

		if (chargin_hw_init_done)
			battery_charging_control(
			CHARGING_CMD_RESET_WATCH_DOG_TIMER, NULL);

		if (!fg_battery_shutdown)
			hrtimer_start(&charger_hv_detect_timer,
				ktime, HRTIMER_MODE_REL);

	} while (!kthread_should_stop() && !fg_battery_shutdown);

	fg_hv_thread = true;

	return 0;
}

enum hrtimer_restart charger_hv_detect_sw_workaround(struct hrtimer *timer)
{
	charger_hv_detect_flag = true;
	wake_up_interruptible(&charger_hv_detect_waiter);

	pr_debug("[charger_hv_detect_sw_workaround]\n");

	return HRTIMER_NORESTART;
}

void charger_hv_detect_sw_workaround_init(void)
{
	ktime_t ktime;

	ktime = ktime_set(0, BAT_MS_TO_NS(2000));
	hrtimer_init(&charger_hv_detect_timer,
		CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	charger_hv_detect_timer.function = charger_hv_detect_sw_workaround;
	hrtimer_start(&charger_hv_detect_timer, ktime, HRTIMER_MODE_REL);

	charger_hv_detect_thread =
	    kthread_run(charger_hv_detect_sw_thread_handler, 0,
			"mtk charger_hv_detect_sw_workaround");
	if (IS_ERR(charger_hv_detect_thread)) {
		pr_debug(
			"[%s]: failed to create charger_hv_detect_sw_workaround thread\n",
			__func__);
	}
	check_battery_exist();
	pr_debug("charger_hv_detect_sw_workaround_init : done\n");
}


enum hrtimer_restart battery_kthread_hrtimer_func(struct hrtimer *timer)
{
	bat_thread_wakeup();

	return HRTIMER_NORESTART;
}

void battery_kthread_hrtimer_init(void)
{
	ktime_t ktime;

	ktime = ktime_set(1, 0);
	hrtimer_init(&battery_kthread_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	battery_kthread_timer.function = battery_kthread_hrtimer_func;
	hrtimer_start(&battery_kthread_timer, ktime, HRTIMER_MODE_REL);

	pr_debug("battery_kthread_hrtimer_init : done\n");
}


static void get_charging_control(void)
{
	battery_charging_control = chr_control_interface;

	if (is_bq25601_exist())
		battery_charging_control = bq25601_control_interface;

#if defined(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT)
	if (batt_internal_charger_detect()) {
		battery_charging_control = chr_control_interface_internal;
		pr_notice("[%s] Use internal charger control\n", __func__);
	}
#endif
}

#if defined(CONFIG_OF)
static void __batt_parse_node(const struct device_node *np,
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

static int __ap15_charger_detection_read_dt(struct device_node *np)
{
	bool is_enable = false;

	is_enable = of_property_read_bool(np, "aicl_enable");
	if (!is_enable)
		return 0;

	batt_cust_data.aicl_enable = true;
	__batt_parse_node(np, "aicl_charging_current_max",
				&batt_cust_data.aicl_charging_current_max);
	__batt_parse_node(np, "aicl_step_current",
				&batt_cust_data.aicl_step_current);
	__batt_parse_node(np, "aicl_step_interval",
				&batt_cust_data.aicl_step_interval);
	__batt_parse_node(np, "aicl_vbus_valid",
				&batt_cust_data.aicl_vbus_valid);
	__batt_parse_node(np, "aicl_vbus_state_phase",
				&batt_cust_data.aicl_vbus_state_phase);
	__batt_parse_node(np, "ap15_charger_input_current_max",
				&batt_cust_data.ap15_charger_input_current_max);
	__batt_parse_node(np, "ap15_charger_input_current_min",
				&batt_cust_data.ap15_charger_input_current_min);
	__batt_parse_node(np, "ap15_dock_input_current_max",
				&batt_cust_data.ap15_dock_input_current_max);
	__batt_parse_node(np, "ap15_dock_input_current_min",
				&batt_cust_data.ap15_dock_input_current_min);

	batt_cust_data.aicl_input_current_max
		= batt_cust_data.ap15_charger_input_current_max;
	batt_cust_data.aicl_input_current_min
			= batt_cust_data.ap15_charger_input_current_min;
	if (!batt_cust_data.ap15_dock_input_current_max)
		batt_cust_data.ap15_dock_input_current_max
			= batt_cust_data.ap15_charger_input_current_max;
	if (!batt_cust_data.ap15_dock_input_current_min)
		batt_cust_data.ap15_dock_input_current_min
			= batt_cust_data.ap15_charger_input_current_min;

	pr_info("%s: %d [%d %d] [%d %d] [%d %d %d %d %d %d]\n", __func__,
				batt_cust_data.aicl_charging_current_max,
				batt_cust_data.ap15_charger_input_current_max,
				batt_cust_data.ap15_charger_input_current_min,
				batt_cust_data.ap15_dock_input_current_max,
				batt_cust_data.ap15_dock_input_current_min,
				batt_cust_data.aicl_input_current_max,
				batt_cust_data.aicl_input_current_min,
				batt_cust_data.aicl_step_current,
				batt_cust_data.aicl_step_interval,
				batt_cust_data.aicl_vbus_valid,
				batt_cust_data.aicl_vbus_state_phase);

	if (!batt_cust_data.aicl_charging_current_max
			|| !batt_cust_data.aicl_input_current_max
			|| !batt_cust_data.aicl_input_current_min
			|| !batt_cust_data.aicl_step_current
			|| !batt_cust_data.aicl_step_interval
			|| !batt_cust_data.aicl_vbus_valid
			|| !batt_cust_data.aicl_vbus_state_phase) {
		pr_info("%s: parameters is missing, disable this feature\n",
			__func__);
		batt_cust_data.aicl_enable = false;
	}

	return 0;
}

static int __batt_init_cust_data_from_dt(void)
{
	/* struct device_node *np = dev->dev.of_node; */
	struct device_node *np;
	batt_cust_data.charge_current_termination = 0;
	/* check customer setting */
	np = of_find_compatible_node(NULL, NULL, "mediatek,battery");
	if (!np) {
		pr_notice("Failed to find device-tree node: bat_comm\n");
		return -ENODEV;
	}

	__batt_parse_node(np, "stop_charging_in_takling",
		&batt_cust_data.stop_charging_in_takling);

	__batt_parse_node(np, "talking_recharge_voltage",
		&batt_cust_data.talking_recharge_voltage);

	__batt_parse_node(np, "talking_sync_time",
		&batt_cust_data.talking_sync_time);

	__batt_parse_node(np, "mtk_temperature_recharge_support",
		&batt_cust_data.mtk_temperature_recharge_support);

	__batt_parse_node(np, "max_charge_temperature",
		&batt_cust_data.max_charge_temperature);

	__batt_parse_node(np, "max_charge_temperature_minus_x_degree",
		&batt_cust_data.max_charge_temperature_minus_x_degree);

	__batt_parse_node(np, "min_charge_temperature",
		&batt_cust_data.min_charge_temperature);

	__batt_parse_node(np, "min_charge_temperature_plus_x_degree",
		&batt_cust_data.min_charge_temperature_plus_x_degree);

	__batt_parse_node(np, "err_charge_temperature",
		&batt_cust_data.err_charge_temperature);

	__batt_parse_node(np, "max_charging_time",
		&batt_cust_data.max_charging_time);

	__batt_parse_node(np, "v_pre2cc_thres",
		&batt_cust_data.v_pre2cc_thres);

	__batt_parse_node(np, "v_cc2topoff_thres",
		&batt_cust_data.v_cc2topoff_thres);

	__batt_parse_node(np, "recharging_voltage",
		&batt_cust_data.recharging_voltage);

	__batt_parse_node(np, "charging_full_current",
		&batt_cust_data.charging_full_current);

	__batt_parse_node(np, "config_usb_if",
		&batt_cust_data.config_usb_if);

	__batt_parse_node(np, "usb_charger_current_suspend",
		&batt_cust_data.usb_charger_current_suspend);

	__batt_parse_node(np, "usb_charger_current_unconfigured",
		&batt_cust_data.usb_charger_current_unconfigured);

	__batt_parse_node(np, "usb_charger_current_configured",
		&batt_cust_data.usb_charger_current_configured);

	__batt_parse_node(np, "usb_charger_current",
		&batt_cust_data.usb_charger_current);

	__batt_parse_node(np, "ac_charger_input_current",
		&batt_cust_data.ac_charger_input_current);

	__batt_parse_node(np, "ac_charger_current",
		&batt_cust_data.ac_charger_current);

	__batt_parse_node(np, "non_std_ac_charger_input_current",
		&batt_cust_data.non_std_ac_charger_input_current);

	__batt_parse_node(np, "non_std_ac_charger_current",
		&batt_cust_data.non_std_ac_charger_current);

	__batt_parse_node(np, "charging_host_charger_current",
		&batt_cust_data.charging_host_charger_current);

	__batt_parse_node(np, "apple_0_5a_charger_current",
		&batt_cust_data.apple_0_5a_charger_current);

	__batt_parse_node(np, "apple_1_0a_charger_current",
		&batt_cust_data.apple_1_0a_charger_current);

	__batt_parse_node(np, "apple_2_1a_charger_current",
		&batt_cust_data.apple_2_1a_charger_current);

	__batt_parse_node(np, "charge_current_termination",
		&batt_cust_data.charge_current_termination);

	__batt_parse_node(np, "bat_low_temp_protect_enable",
		&batt_cust_data.bat_low_temp_protect_enable);

	__batt_parse_node(np, "v_charger_enable",
		&batt_cust_data.v_charger_enable);

	__batt_parse_node(np, "v_charger_max",
		&batt_cust_data.v_charger_max);

	__batt_parse_node(np, "v_charger_min",
		&batt_cust_data.v_charger_min);

	__batt_parse_node(np, "onehundred_percent_tracking_time",
		&batt_cust_data.onehundred_percent_tracking_time);

	__batt_parse_node(np, "npercent_tracking_time",
		&batt_cust_data.npercent_tracking_time);

	__batt_parse_node(np, "sync_to_real_tracking_time",
		&batt_cust_data.sync_to_real_tracking_time);

	__batt_parse_node(np, "v_0percent_tracking",
		&batt_cust_data.v_0percent_tracking);

	__batt_parse_node(np, "system_off_voltage",
		&batt_cust_data.system_off_voltage);

	__batt_parse_node(np, "high_battery_voltage_support",
		&batt_cust_data.high_battery_voltage_support);

	__batt_parse_node(np, "mtk_jeita_standard_support",
		&batt_cust_data.mtk_jeita_standard_support);

	__batt_parse_node(np, "cust_soc_jeita_sync_time",
		&batt_cust_data.cust_soc_jeita_sync_time);

	__batt_parse_node(np, "jeita_recharge_voltage",
		&batt_cust_data.jeita_recharge_voltage);

	__batt_parse_node(np, "jeita_temp_above_pos_60_cv_voltage",
		&batt_cust_data.jeita_temp_above_pos_60_cv_voltage);

	__batt_parse_node(np, "jeita_temp_pos_45_to_pos_60_cv_voltage",
		&batt_cust_data.jeita_temp_pos_45_to_pos_60_cv_voltage);

	__batt_parse_node(np, "jeita_temp_pos_10_to_pos_45_cv_voltage",
		&batt_cust_data.jeita_temp_pos_10_to_pos_45_cv_voltage);

	__batt_parse_node(np, "jeita_temp_pos_0_to_pos_10_cv_voltage",
		&batt_cust_data.jeita_temp_pos_0_to_pos_10_cv_voltage);

	__batt_parse_node(np, "jeita_temp_neg_10_to_pos_0_cv_voltage",
		&batt_cust_data.jeita_temp_neg_10_to_pos_0_cv_voltage);

	__batt_parse_node(np, "jeita_temp_below_neg_10_cv_voltage",
		&batt_cust_data.jeita_temp_below_neg_10_cv_voltage);

	__batt_parse_node(np, "jeita_neg_10_to_pos_0_full_current",
		&batt_cust_data.jeita_neg_10_to_pos_0_full_current);

	__batt_parse_node(np, "jeita_temp_pos_45_to_pos_60_recharge_voltage",
		&batt_cust_data.jeita_temp_pos_45_to_pos_60_recharge_voltage);

	__batt_parse_node(np, "jeita_temp_pos_10_to_pos_45_recharge_voltage",
		&batt_cust_data.jeita_temp_pos_10_to_pos_45_recharge_voltage);

	__batt_parse_node(np, "jeita_temp_pos_0_to_pos_10_recharge_voltage",
		&batt_cust_data.jeita_temp_pos_0_to_pos_10_recharge_voltage);

	__batt_parse_node(np, "jeita_temp_neg_10_to_pos_0_recharge_voltage",
		&batt_cust_data.jeita_temp_neg_10_to_pos_0_recharge_voltage);

	__batt_parse_node(np, "jeita_temp_pos_45_to_pos_60_cc2topoff_threshold",
	&batt_cust_data.jeita_temp_pos_45_to_pos_60_cc2topoff_threshold);

	__batt_parse_node(np, "jeita_temp_pos_10_to_pos_45_cc2topoff_threshold",
	&batt_cust_data.jeita_temp_pos_10_to_pos_45_cc2topoff_threshold);

	__batt_parse_node(np, "jeita_temp_pos_0_to_pos_10_cc2topoff_threshold",
		&batt_cust_data.jeita_temp_pos_0_to_pos_10_cc2topoff_threshold);

	__batt_parse_node(np, "jeita_temp_neg_10_to_pos_0_cc2topoff_threshold",
		&batt_cust_data.jeita_temp_neg_10_to_pos_0_cc2topoff_threshold);

	/* For fast charger detection */
	__ap15_charger_detection_read_dt(np);

	__batt_parse_node(np, "toggle_charge_enable",
			&batt_cust_data.toggle_charge_enable);

	of_node_put(np);
	return 0;
}
#endif

int batt_init_cust_data(void)
{
#if defined(CONFIG_OF)
	pr_notice("battery custom init by DTS\n");
	__batt_init_cust_data_from_dt();
#endif

	if (batt_cust_data.system_off_voltage < 3200)
		batt_cust_data.system_off_voltage = SYSTEM_OFF_VOLTAGE;

	return 0;
}

#if defined(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT)
int batt_internal_charger_detect(void)
{
	static int chg_exist = 0xF;

	if (chg_exist != 0xF)
		return chg_exist;

	chg_exist = 1;

#if defined(CONFIG_MTK_BQ24296_SUPPORT) || defined(CONFIG_MTK_BQ24297_SUPPORT)
	if ((bq24297_get_pn() == 0x3) || (bq24297_get_pn() == 0x1))
		chg_exist = 0;
#endif

	if (is_bq25601_exist())
		chg_exist = 0;

	return chg_exist;
}
#endif

static int detemine_initial_status(void)
{
	usb_main.usb_present = bat_is_charger_exist();
	ac_main.ac_present = usb_main.usb_present;
	BMT_status.bat_vol = battery_meter_get_battery_voltage(true);
	power_supply_changed(ac_main.psy);
	power_supply_changed(usb_main.psy);

	pr_info("[%s] usb_present: %d, bat_vol: %d\n",
			__func__, usb_main.usb_present, BMT_status.bat_vol);
	return 0;
}

static int battery_probe(struct platform_device *dev)
{
	struct class_device *class_dev = NULL;
	int ret = 0;

	pr_notice("******** battery driver probe!! ********\n");

	/* Integrate with NVRAM */
	ret = alloc_chrdev_region(&adc_cali_devno, 0, 1, ADC_CALI_DEVNAME);
	if (ret)
		pr_notice("Error: Can't Get Major number for adc_cali\n");
	adc_cali_cdev = cdev_alloc();
	adc_cali_cdev->owner = THIS_MODULE;
	adc_cali_cdev->ops = &adc_cali_fops;
	ret = cdev_add(adc_cali_cdev, adc_cali_devno, 1);
	if (ret)
		pr_notice("adc_cali Error: cdev_add\n");
	adc_cali_major = MAJOR(adc_cali_devno);
	adc_cali_class = class_create(THIS_MODULE, ADC_CALI_DEVNAME);
	class_dev = (struct class_device *)device_create(adc_cali_class,
		NULL, adc_cali_devno, NULL, ADC_CALI_DEVNAME);
	pr_debug("[BAT_probe] adc_cali prepare : done !!\n ");

	get_charging_control();

	batt_init_cust_data();

	battery_charging_control(
		CHARGING_CMD_GET_PLATFORM_BOOT_MODE, &g_platform_boot_mode);
	pr_debug("[BAT_probe] g_platform_boot_mode = %d\n ",
		g_platform_boot_mode);

	battery_fg_lock = wakeup_source_register("battery fg wakelock");

	battery_suspend_lock =
		wakeup_source_register("battery suspend wakelock");

#ifdef CONFIG_USB_AMAZON_DOCK
	battery_main.dock_state.name = "dock";
	battery_main.dock_state.index = 0;
	battery_main.dock_state.state = TYPE_UNDOCKED;
	ret = switch_dev_register(&battery_main.dock_state);
	if (ret) {
		pr_notice("[BAT_probe] switch_dev_register dock_state Fail\n");
		return ret;
	}
#endif

	/* Integrate with Android Battery Service */
	ac_main.psy = power_supply_register(&(dev->dev), &ac_main.psd, NULL);
	if (IS_ERR(ac_main.psy)) {
		pr_debug("[BAT_probe] power_supply_register AC Fail !!\n");
		ret = PTR_ERR(ac_main.psy);
		return ret;
	}
	pr_debug("[BAT_probe] power_supply_register AC Success !!\n");

	usb_main.psy =
		power_supply_register(&(dev->dev), &usb_main.psd, NULL);
	if (IS_ERR(usb_main.psy)) {
		pr_debug("[BAT_probe] power_supply_register USB Fail !!\n");
		ret = PTR_ERR(usb_main.psy);
		return ret;
	}
	pr_debug("[BAT_probe] power_supply_register USB Success !!\n");

	wireless_main.psy =
		power_supply_register(&(dev->dev), &wireless_main.psd, NULL);
	if (IS_ERR(wireless_main.psy)) {
		pr_debug("[BAT_probe] power_supply_register WIRELESS Fail !!\n");
		ret = PTR_ERR(wireless_main.psy);
		return ret;
	}
	pr_debug("[BAT_probe] power_supply_register WIRELESS Success !!\n");

	battery_main.psy =
		power_supply_register(&(dev->dev), &battery_main.psd, NULL);
	if (IS_ERR(battery_main.psy)) {
		pr_debug("[BAT_probe] power_supply_register Battery Fail !!\n");
		ret = PTR_ERR(battery_main.psy);
		return ret;
	}
	pr_debug("[BAT_probe] power_supply_register Battery Success !!\n");

#if !defined(CONFIG_POWER_EXT)

#ifdef CONFIG_MTK_POWER_EXT_DETECT
	if (true == bat_is_ext_power()) {
		battery_main.BAT_STATUS = POWER_SUPPLY_STATUS_FULL;
		battery_main.BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
		battery_main.BAT_PRESENT = 1;
		battery_main.BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
		battery_main.BAT_CAPACITY = 100;
		battery_main.BAT_batt_vol = 4200;
		battery_main.BAT_batt_temp = 220;

		g_bat_init_flag = true;
		return 0;
	}
#endif
	/* For EM */
	{
		int ret_device_file = 0;

		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Charger_Voltage);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_0_Slope);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_1_Slope);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_2_Slope);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_3_Slope);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_4_Slope);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_5_Slope);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_6_Slope);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_7_Slope);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_8_Slope);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_9_Slope);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_10_Slope);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_11_Slope);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_12_Slope);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_13_Slope);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_0_Offset);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_1_Offset);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_2_Offset);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_3_Offset);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_4_Offset);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_5_Offset);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_6_Offset);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_7_Offset);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_8_Offset);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_9_Offset);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_10_Offset);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_11_Offset);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_12_Offset);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_13_Offset);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_ADC_Channel_Is_Calibration);

		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_Power_On_Voltage);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_Power_Off_Voltage);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_Charger_TopOff_Value);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_FG_Battery_CurrentConsumption);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_FG_SW_CoulombCounter);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_Charging_CallState);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_Charging_Enable);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_Custom_Charging_Current);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_Custom_PlugIn_Time);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_Custom_Charging_Mode);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_Custom_RTC_SOC);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_V_0Percent_Tracking);

		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_Charger_Type);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_usbin_now);

		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_aicl_result);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_aicl_vbus_valid);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_aicl_vbus_state_phase);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_aicl_input_current_max);
		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_aicl_input_current_min);

		ret_device_file = device_create_file(
			&(dev->dev), &dev_attr_battery_cmd);
	}

	/* move to mt_battery_GetBatteryData() to decrease booting time */
	/* battery_meter_initial(); */

	/* Initialization BMT Struct */
	/* phone must have battery */
	BMT_status.bat_exist = true;
	/* for default, no charger */
	BMT_status.charger_exist = false;
	BMT_status.bat_vol = 0;
	BMT_status.ICharging = 0;
	BMT_status.temperature = 0;
	BMT_status.charger_vol = 0;
	BMT_status.total_charging_time = 0;
	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.POSTFULL_charging_time = 0;
	BMT_status.SOC = 0;
#ifdef CUST_CAPACITY_OCV2CV_TRANSFORM
	BMT_status.UI_SOC = -1;
#else
	BMT_status.UI_SOC = 0;
#endif

	BMT_status.bat_charging_state = CHR_PRE;
	BMT_status.bat_in_recharging_state = false;
	BMT_status.bat_full = false;
	BMT_status.nPercent_ZCV = 0;
	BMT_status.nPrecent_UI_SOC_check_point =
		battery_meter_get_battery_nPercent_UI_SOC();

	detemine_initial_status();

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	dual_input_init();
#endif

	/* battery kernel thread for 10s check and charger in/out event */
	/* Replace GPT timer by hrtime */
	battery_kthread_hrtimer_init();

	kthread_run(bat_thread_kthread, NULL, "bat_thread_kthread");
	pr_debug("[battery_probe] bat_thread_kthread Done\n");

	charger_hv_detect_sw_workaround_init();

#else
	/* keep HW alive */
	/* charger_hv_detect_sw_workaround_init(); */
#endif
	g_bat_init_flag = true;

	bat_metrics_init();

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	if (g_vcdt_irq_delay_flag == true)
		do_chrdet_int_task();
#endif

	return 0;

}

static void battery_timer_pause(void)
{


	/* pr_debug("******** battery driver suspend!! ********\n" ); */
#ifdef CONFIG_POWER_EXT
#else

#ifdef CONFIG_MTK_POWER_EXT_DETECT
	if (true == bat_is_ext_power())
		return 0;
#endif
	mutex_lock(&bat_mutex);
	/* cancel timer */
	hrtimer_cancel(&battery_kthread_timer);
	hrtimer_cancel(&charger_hv_detect_timer);

	battery_suspended = true;
	mutex_unlock(&bat_mutex);

	pr_notice("@bs=1@\n");
#endif

	get_monotonic_boottime(&g_bat_time_before_sleep);
}

static void battery_timer_resume(void)
{
#if !defined(CONFIG_POWER_EXT)
	bool is_pcm_timer_trigger = false;
	struct timespec bat_time_after_sleep;
	ktime_t ktime, hvtime;

#ifdef CONFIG_MTK_POWER_EXT_DETECT
	if (true == bat_is_ext_power())
		return 0;
#endif

	ktime = ktime_set(BAT_TASK_PERIOD, 0);	/* 10s, 10* 1000 ms */
	hvtime = ktime_set(0, BAT_MS_TO_NS(2000));

	get_monotonic_boottime(&bat_time_after_sleep);
	battery_charging_control(
		CHARGING_CMD_GET_IS_PCM_TIMER_TRIGGER, &is_pcm_timer_trigger);

	if (is_pcm_timer_trigger == true || bat_spm_timeout) {
		mutex_lock(&bat_mutex);
		if (bat_spm_timeout && (BMT_status.UI_SOC > BMT_status.SOC)) {
			pr_warn("[%s] replace ui_soc(%d) with soc(%d)\n",
				__func__, BMT_status.UI_SOC, BMT_status.SOC);
			BMT_status.UI_SOC = BMT_status.SOC;
		}

		BAT_thread();
		mutex_unlock(&bat_mutex);
	} else {
		pr_debug("battery resume NOT by pcm timer!!\n");
	}

	if (g_call_state == CALL_ACTIVE &&
		(bat_time_after_sleep.tv_sec - g_bat_time_before_sleep.tv_sec
		>= batt_cust_data.talking_sync_time)) {
		/* phone call last than x min */
		BMT_status.UI_SOC = battery_meter_get_battery_percentage();
		pr_debug("Sync UI SOC to SOC immediately\n");
	}

	mutex_lock(&bat_mutex);

	/* restore timer */
	hrtimer_start(&battery_kthread_timer, ktime, HRTIMER_MODE_REL);
	hrtimer_start(&charger_hv_detect_timer, hvtime, HRTIMER_MODE_REL);

	battery_suspended = false;
	pr_notice("@bs=0@\n");
	mutex_unlock(&bat_mutex);
	wake_up(&bat_thread_wq);
#endif
}

static int battery_remove(struct platform_device *dev)
{
	bat_metrics_uninit();
#ifdef CONFIG_USB_AMAZON_DOCK
	switch_dev_unregister(&battery_main.dock_state);
#endif
	pr_notice("******** battery driver remove!! ********\n");

	return 0;
}

static void battery_shutdown(struct platform_device *dev)
{
#if !defined(CONFIG_POWER_EXT)
	int count = 0;
#endif

	pr_notice("******** battery driver shutdown!! ********\n");

	mutex_lock(&bat_mutex);
	fg_battery_shutdown = true;
	upmu_set_rg_int_en_chrdet(false);
	wake_up_bat();
	charger_hv_detect_flag = true;
	wake_up_interruptible(&charger_hv_detect_waiter);

#if !defined(CONFIG_POWER_EXT)
	while ((!fg_bat_thread || !fg_hv_thread) && count < 5) {
		mutex_unlock(&bat_mutex);
		msleep(20);
		count++;
		mutex_lock(&bat_mutex);
	}

	if (!fg_bat_thread || !fg_hv_thread)
		pr_err("failed to terminate battery related thread(%d, %d)\n",
			fg_bat_thread, fg_hv_thread);
#endif

	mutex_unlock(&bat_mutex);
}

/* //////////////////////////////////
 * // Battery Notify API
 * //////////////////////////////////
 */
static ssize_t show_BatteryNotify(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_notice("[Battery] show_BatteryNotify : %x\n", g_BatteryNotifyCode);

	return sprintf(buf, "%u\n", g_BatteryNotifyCode);
}

static ssize_t store_BatteryNotify(struct device *dev,
	struct device_attribute *attr,
				   const char *buf, size_t size)
{
	/*char *pvalue = NULL;*/
	int rv;
	unsigned long reg_BatteryNotifyCode = 0;

	pr_notice("[Battery] store_BatteryNotify\n");
	if (buf != NULL && size != 0) {
		pr_notice("[Battery] buf is %s and size is %Zu\n", buf, size);
		rv = kstrtoul(buf, 0, &reg_BatteryNotifyCode);
		if (rv != 0)
			return -EINVAL;
		g_BatteryNotifyCode = reg_BatteryNotifyCode;
		pr_notice("[Battery] store code : %x\n", g_BatteryNotifyCode);
	}
	return size;
}

static DEVICE_ATTR(BatteryNotify, 0664,
	show_BatteryNotify, store_BatteryNotify);

static ssize_t show_BN_TestMode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_notice("[Battery] show_BN_TestMode : %x\n", g_BN_TestMode);
	return sprintf(buf, "%u\n", g_BN_TestMode);
}

static ssize_t store_BN_TestMode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	/*char *pvalue = NULL;*/
	int rv;
	unsigned long reg_BN_TestMode = 0;

	pr_notice("[Battery] store_BN_TestMode\n");
	if (buf != NULL && size != 0) {
		pr_notice("[Battery] buf is %s and size is %Zu\n", buf, size);
		rv = kstrtoul(buf, 0, &reg_BN_TestMode);
		if (rv != 0)
			return -EINVAL;
		g_BN_TestMode = reg_BN_TestMode;
		pr_notice("[Battery] store g_BN_TestMode : %x\n",
			g_BN_TestMode);
	}
	return size;
}

static DEVICE_ATTR(BN_TestMode, 0664,
	show_BN_TestMode, store_BN_TestMode);


/* /////////////////////////
 * // platform_driver API
 * /////////////////////////
 */
static int mt_batteryNotify_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	pr_notice("******** mt_batteryNotify_probe!! ********\n");

	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_BatteryNotify);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_BN_TestMode);

	return 0;

}

#ifdef CONFIG_OF
static const struct of_device_id mt_battery_of_match[] = {
	{.compatible = "mediatek,battery",},
	{},
};

MODULE_DEVICE_TABLE(of, mt_battery_of_match);
#endif

static int battery_pm_suspend(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);
	bat_metrics_suspend();

	return ret;
}

static int battery_pm_resume(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);
	bat_metrics_resume();

	return ret;
}

static int battery_pm_freeze(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ret;
}

static int battery_pm_restore(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ret;
}

static int battery_pm_restore_noirq(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	return ret;
}

const struct dev_pm_ops battery_pm_ops = {
	.suspend = battery_pm_suspend,
	.resume = battery_pm_resume,
	.freeze = battery_pm_freeze,
	.thaw = battery_pm_restore,
	.restore = battery_pm_restore,
	.restore_noirq = battery_pm_restore_noirq,
};

#if defined(CONFIG_OF) || defined(BATTERY_MODULE_INIT)
struct platform_device battery_device = {
	.name = "battery",
	.id = -1,
};
#endif

static struct platform_driver battery_driver = {
	.probe = battery_probe,
	.remove = battery_remove,
	.shutdown = battery_shutdown,
	.driver = {
		   .name = "battery",
		   .pm = &battery_pm_ops,
		   },
};

#ifdef CONFIG_OF
static int battery_dts_probe(struct platform_device *dev)
{
	int ret = 0;

	pr_notice("******** battery_dts_probe!! ********\n");

	battery_device.dev.of_node = dev->dev.of_node;
	ret = platform_device_register(&battery_device);
	if (ret) {
		pr_notice(
		    "****[battery_dts_probe] Unable to register device (%d)\n",
			ret);
		return ret;
	}
	return 0;

}

static struct platform_driver battery_dts_driver = {
	.probe = battery_dts_probe,
	.remove = NULL,
	.shutdown = NULL,
	.driver = {
		   .name = "battery-dts",
#ifdef CONFIG_OF
		   .of_match_table = mt_battery_of_match,
#endif
		   },
};

/* -------------------------------------------------------- */

static const struct of_device_id mt_bat_notify_of_match[] = {
	{.compatible = "mediatek,bat_notify",},
	{},
};

MODULE_DEVICE_TABLE(of, mt_bat_notify_of_match);
#endif

struct platform_device MT_batteryNotify_device = {
	.name = "mt-battery",
	.id = -1,
};

static struct platform_driver mt_batteryNotify_driver = {
	.probe = mt_batteryNotify_probe,
	.driver = {
		   .name = "mt-battery",
		   },
};

#ifdef CONFIG_OF
static int mt_batteryNotify_dts_probe(struct platform_device *dev)
{
	int ret = 0;
	/* struct proc_dir_entry *entry = NULL; */

	pr_notice("******** mt_batteryNotify_dts_probe!! ********\n");

	MT_batteryNotify_device.dev.of_node = dev->dev.of_node;
	ret = platform_device_register(&MT_batteryNotify_device);
	if (ret) {
		pr_notice(
			"****[mt_batteryNotify_dts] Unable to register device (%d)\n",
			ret);
		return ret;
	}
	return 0;

}


static struct platform_driver mt_batteryNotify_dts_driver = {
	.probe = mt_batteryNotify_dts_probe,
	.driver = {
		   .name = "mt-dts-battery",
#ifdef CONFIG_OF
		   .of_match_table = mt_bat_notify_of_match,
#endif
		   },
};
#endif
/* -------------------------------------------------------- */

static int battery_pm_event(struct notifier_block *notifier,
	unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:	/* Going to hibernate */
	case PM_RESTORE_PREPARE:	/* Going to restore a saved image */
	case PM_SUSPEND_PREPARE:	/* Going to suspend the system */
		pr_warn("[%s] pm_event %lu\n", __func__, pm_event);
		battery_timer_pause();
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:	/* Hibernation finished */
	case PM_POST_SUSPEND:	/* Suspend finished */
	case PM_POST_RESTORE:	/* Restore failed */
		pr_warn("[%s] pm_event %lu\n", __func__, pm_event);
		battery_timer_resume();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block battery_pm_notifier_block = {
	.notifier_call = battery_pm_event,
	.priority = 0,
};

static int __init battery_init(void)
{
	int ret;

	pr_debug("battery_init\n");

#ifdef CONFIG_OF
	/*  */
#else

#ifdef BATTERY_MODULE_INIT
	ret = platform_device_register(&battery_device);
	if (ret) {
		pr_notice(
			"****[battery_device] Unable to device register(%d)\n",
			ret);
		return ret;
	}
#endif
#endif

	ret = platform_driver_register(&battery_driver);
	if (ret) {
		pr_notice(
			"****[battery_driver] Unable to register driver (%d)\n",
			ret);
		return ret;
	}
	/* battery notofy UI */
#ifdef CONFIG_OF
	/*  */
#else
	ret = platform_device_register(&MT_batteryNotify_device);
	if (ret) {
		pr_notice(
			"****[mt_batteryNotify] Unable to device register(%d)\n",
			ret);
		return ret;
	}
#endif
	ret = platform_driver_register(&mt_batteryNotify_driver);
	if (ret) {
		pr_notice(
			"****[mt_batteryNotify] Unable to register driver (%d)\n",
			ret);
		return ret;
	}
#ifdef CONFIG_OF
	ret = platform_driver_register(&battery_dts_driver);
	ret = platform_driver_register(&mt_batteryNotify_dts_driver);
#endif
	ret = register_pm_notifier(&battery_pm_notifier_block);
	if (ret)
		pr_debug("[%s] failed to register PM notifier %d\n",
			__func__, ret);

	pr_notice("****[battery_driver] Initialization : DONE !!\n");
	return 0;
}

#ifdef BATTERY_MODULE_INIT
late_initcall(battery_init);
#else
static void __exit battery_exit(void)
{
}
late_initcall(battery_init);

#endif

MODULE_AUTHOR("Oscar Liu");
MODULE_DESCRIPTION("Battery Device Driver");
MODULE_LICENSE("GPL");
