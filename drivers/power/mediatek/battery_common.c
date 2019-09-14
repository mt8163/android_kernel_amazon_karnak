/*****************************************************************************
 *
 * Filename:
 * ---------
 *    battery_common.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of mt6323 Battery charging algorithm
 *   and the Anroid Battery service for updating the battery status
 *
 * Author:
 * -------
 * Oscar Liu
 *
 ****************************************************************************/
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
#include <linux/wakelock.h>
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
#include <linux/thermal.h>
#include <linux/thermal_framework.h>
#include <linux/platform_data/mtk_thermal.h>

#include <asm/scatterlist.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <mt-plat/mt_boot.h>
#include <mt-plat/mtk_rtc.h>

#include <mt-plat/upmu_common.h>

#include <mt-plat/charging.h>
#include <mt-plat/battery_common.h>
#include <mt-plat/battery_meter.h>
#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
#include <mach/mt_pe.h>
#endif

#if defined(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT)
#include <mt-plat/internal_charging.h>
#endif

#if defined(CONFIG_AMAZON_METRICS_LOG)

#if defined(CONFIG_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#include <linux/metricslog.h>

#include <linux/irq.h>
/* #include <mach/mt_pmic_irq.h> */
#include <linux/reboot.h>

#ifdef CONFIG_AMAZON_SIGN_OF_LIFE
#include <linux/sign_of_life.h>
#endif

#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

extern signed int g_fg_dbg_bat_qmax;

enum BQ_FLAGS {
	BQ_STATUS_RESUMING = 0x2,
};

enum BCCT_STATE {
	bcct_false = 0,
	bcct_true
};

#ifdef CONFIG_USB_AMAZON_DOCK
enum DOCK_STATE_TYPE {
	TYPE_DOCKED = 5,
	TYPE_UNDOCKED = 6,
};
#endif


struct battery_info {
	struct mutex lock;

	int flags;

	/* Time when system enters full suspend */
	struct timespec suspend_time;
	/* Time when system enters early suspend */
	struct timespec early_suspend_time;
	/* Battery capacity when system enters full suspend */
	int suspend_capacity;
	/* Battery capacity, relative and high-precision, when system enters full suspend */
	int suspend_bat_car;
	/* Battery capacity when system enters early suspend */
	int early_suspend_capacity;
#if defined(CONFIG_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
#if defined(CONFIG_FB)
	struct notifier_block notifier;
#endif
};

struct battery_info BQ_info;
#endif /* CONFIG_AMAZON_METRICS_LOG */


/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Smart Battery Structure */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
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
int battery_cmd_thermal_test_mode = 0;
int battery_cmd_thermal_test_mode_value = 0;
int g_battery_tt_check_flag = 0;
/* 0:default enable check batteryTT, 1:default disable check batteryTT */


/*
 *  Global Variable
 */

struct wake_lock battery_suspend_lock;
struct wake_lock battery_fg_lock;
CHARGING_CONTROL battery_charging_control;
unsigned int g_BatteryNotifyCode = 0x0000;
unsigned int g_BN_TestMode = 0x0000;
bool g_bat_init_flag = 0;
unsigned int g_call_state = CALL_IDLE;
bool g_charging_full_reset_bat_meter = false;
int g_platform_boot_mode = 0;
struct timespec g_bat_time_before_sleep;
int g_smartbook_update = 0;
signed int g_custom_charging_current = -1;
signed int g_custom_rtc_soc = -1;
signed int g_custom_charging_cv = -1;
unsigned int g_custom_charging_mode = 0; /* 0=ratail unit, 1=demo unit */
static unsigned long g_custom_plugin_time;
int g_custom_aicl_input_current;

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
bool g_vcdt_irq_delay_flag = 0;
#endif

bool skip_battery_update = false;

unsigned int g_batt_temp_status = TEMP_POS_NORMAL;


bool battery_suspended = false;
struct timespec chr_plug_in_time;
#define PLUGIN_THRESHOLD (14*86400)

/*#ifdef MTK_ENABLE_AGING_ALGORITHM
extern unsigned int suspend_time;
#endif */

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

int adc_cali_slop[14] = { 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000 };

int adc_cali_offset[14] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int adc_cali_cal[1] = { 0 };
int battery_in_data[1] = { 0 };
int battery_out_data[1] = { 0 };
int charging_level_data[1] = { 0 };

bool g_ADC_Cali = false;
bool g_ftm_battery_flag = false;
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
#ifdef CONFIG_AMAZON_METRICS_LOG
static bool bat_full_flag;
static bool bat_empty_flag;
static struct timespec charging_time;
static struct timespec discharging_time;
static unsigned int init_charging_vol;
static unsigned int init_discharging_vol;

static bool bat_96_93_flag;
static bool bat_15_20_flag;
static bool bat_0_20_flag;
static bool bat_1_flag;
static bool bat_14days_flag;
static bool bat_demo_flag;

static struct timespec charging_96_93_time;
static struct timespec charging_15_20_time;
static struct timespec charging_0_20_time;

extern unsigned long get_virtualsensor_temp(void);
#endif
/*extern BOOL bat_spm_timeout;
extern unsigned int _g_bat_sleep_total_time;*/

/*
 * FOR ADB CMD
 */
/* Dual battery */
int g_status_smb = POWER_SUPPLY_STATUS_NOT_CHARGING;
int g_capacity_smb = 50;
int g_present_smb = 0;
/* ADB charging CMD */
static int cmd_discharging = -1;
static int adjust_power = -1;
static int suspend_discharging = -1;
static bool is_uisoc_ever_100;

#ifdef CONFIG_AMAZON_METRICS_LOG

#ifndef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT
signed int gFG_aging_factor = 0;
signed int gFG_battery_cycle = 0;
signed int gFG_columb_sum = 0;
#else
extern signed int gFG_aging_factor;
extern signed int gFG_battery_cycle;
extern signed int gFG_columb_sum;
#endif


#if defined(CONFIG_AMAZON_METRICS_LOG)
void metrics_charger(bool connect)
{
	int cap = BMT_status.UI_SOC;
	char buf[256] = {0};
	long elaps_sec;
	struct timespec diff;
	unsigned long virtual_temp = get_virtualsensor_temp();

	if (connect == true) {
		init_charging_vol = BMT_status.bat_vol;
		charging_time = current_kernel_time();
		diff = timespec_sub(charging_time, discharging_time);

		elaps_sec = diff.tv_sec + diff.tv_nsec/NSEC_PER_SEC;

		if (elaps_sec > 60) {
			snprintf(buf, sizeof(buf),
				"bq24297:def:Charger_Exist=%d;CT;1,Elaps_Sec=%ld;CT;1,Initial_Bat_Vol=%d;CT;1,"
				"Final_Bal_Vol=%d;CT;1,UI_SOC=%d;CT;1,SOC=%d;CT;1,Bat_Avg_Temp=%d;CT;1,"
				"Vir_Avg_Temp=%ld;CT;1,Bat_Cycle_Count=%d;CT;1:NA",
				BMT_status.charger_exist, elaps_sec,init_charging_vol, BMT_status.bat_vol, BMT_status.UI_SOC,
				BMT_status.SOC, BMT_status.temperature, virtual_temp, gFG_battery_cycle);

			log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
			memset(buf, 0, sizeof(buf));
		}

		snprintf(buf, sizeof(buf),
			"bq24297:def:POWER_STATUS_CHARGING=1;CT;1,"
			"cap=%u;CT;1,mv=%d;CT;1,current_avg=%d;CT;1:NR",
			cap, BMT_status.bat_vol, BMT_status.ICharging);

		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
	} else {
		init_discharging_vol = BMT_status.bat_vol;
		discharging_time = current_kernel_time();
		diff = timespec_sub(discharging_time, charging_time);

		elaps_sec = diff.tv_sec + diff.tv_nsec/NSEC_PER_SEC;

		if (elaps_sec > 60) {
			snprintf(buf,sizeof(buf),
				"bq24297:def:Charger_Exist=%d;CT;1,Elaps_Sec=%ld;CT;1,Initial_Bat_Vol=%d;CT;1,"
				"Final_Bal_Vol=%d;CT;1,UI_SOC=%d;CT;1,SOC=%d;CT;1,Bat_Avg_Temp=%d;CT;1,"
				"Vir_Avg_Temp=%ld;CT;1,Bat_Cycle_Count=%d;CT;1:NA",
				BMT_status.charger_exist, elaps_sec, init_charging_vol, BMT_status.bat_vol, BMT_status.UI_SOC,
				BMT_status.SOC, BMT_status.temperature, virtual_temp, gFG_battery_cycle);

			log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
			memset(buf, 0, sizeof(buf));
		}

		snprintf(buf, sizeof(buf),
			"bq24297:def:POWER_STATUS_DISCHARGING=1;CT;1,"
			"cap=%u;CT;1,mv=%d;CT;1,current_avg=%d;CT;1:NR",
			cap, BMT_status.bat_vol, BMT_status.ICharging);

		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
	}
}
void metrics_charger_update(int ac, int usb)
{
	static bool ischargeronline;
	int onceonline = 0;

	if (ac == 1)
		onceonline = 1;
	else if (usb == 1)
		onceonline = 1;
	if (ischargeronline != onceonline) {
		ischargeronline = onceonline;
		metrics_charger(ischargeronline);
	}
}
#endif


#ifdef CONFIG_AMAZON_METRICS_LOG
static void
battery_critical_voltage_check(void)
{
	static bool written;

	if (!BMT_status.charger_exist)
		return;
	if (BMT_status.bat_exist != true)
		return;
	if (BMT_status.UI_SOC != 0)
		return;
	if (written == false && BMT_status.bat_vol <= SYSTEM_OFF_VOLTAGE) {
		char buf[128];

		written = true;
		snprintf(buf, sizeof(buf),
			"bq24297:def:critical_shutdown=1;CT;1:HI");

		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
#ifdef CONFIG_AMAZON_SIGN_OF_LIFE
		life_cycle_set_special_mode(LIFE_CYCLE_SMODE_LOW_BATTERY);
#endif
	}
}

#if !defined(CONFIG_POWER_EXT)
static void
battery_metrics_locked(struct battery_info *info);

static void metrics_handle(void)
{
	struct battery_info *info = &BQ_info;

	mutex_lock(&info->lock);

	/* Check for critical battery voltage */
	battery_critical_voltage_check();


	if ((info->flags & BQ_STATUS_RESUMING)) {
		info->flags &= ~BQ_STATUS_RESUMING;
		battery_metrics_locked(info);
	}

	mutex_unlock(&info->lock);

	return;
}
#endif /* CONFIG_POWER_EXT */

#if defined(CONFIG_EARLYSUSPEND) || defined(CONFIG_FB)
static void bq_log_metrics(struct battery_info *info, char *msg,
	char *metricsmsg)
{
	int value = BMT_status.UI_SOC;
	struct timespec curr = current_kernel_time();
	/* Compute elapsed time and determine screen off or on drainage */
	struct timespec diff = timespec_sub(curr,
			info->early_suspend_time);
	struct timespec boot_time;
	long boot_msec, elaps_msec;

	getrawmonotonic(&boot_time);

	boot_msec = boot_time.tv_sec * 1000 + boot_time.tv_nsec / NSEC_PER_MSEC;
	elaps_msec = diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC;

	if (elaps_msec >= 0 && elaps_msec <= boot_msec) {
		if (info->early_suspend_capacity != -1) {
			char buf[512];

			snprintf(buf, sizeof(buf),
				"%s:def:value=%d;CT;1,elapsed=%ld;TI;1:NR",
				metricsmsg,
				info->early_suspend_capacity - value,
				elaps_msec);
			log_to_metrics(ANDROID_LOG_INFO, "drain_metrics", buf);
		}
	}
	/* Cache the current capacity */
	info->early_suspend_capacity = BMT_status.UI_SOC;
	/* Mark the suspend or resume time */
	info->early_suspend_time = curr;
}
#endif

#if defined(CONFIG_EARLYSUSPEND)
static void battery_early_suspend(struct early_suspend *handler)
{
	struct battery_info *info = &BQ_info;

	bq_log_metrics(info, "Screen on drainage", "screen_on_drain");

}

static void battery_late_resume(struct early_suspend *handler)
{
	struct battery_info *info = &BQ_info;
	bq_log_metrics(info, "Screen off drainage", "screen_off_drain");
}
#endif

#if defined(CONFIG_FB)
/* frame buffer notifier block control the suspend/resume procedure */
static int batt_fb_notifier_callback(struct notifier_block *noti, unsigned long event, void *data)
{
	struct fb_event *ev_data = data;
	int *blank;
	struct battery_info *info = &BQ_info;

	if (ev_data && ev_data->data && event == FB_EVENT_BLANK) {
		blank = ev_data->data;
		if (*blank == FB_BLANK_UNBLANK) {
			bq_log_metrics(info, "Screen off drainage", "screen_off_drain");

		} else if (*blank == FB_BLANK_POWERDOWN) {
			bq_log_metrics(info, "Screen on drainage", "screen_on_drain");
		}
	}

	return 0;
}
#endif

static int metrics_init(void)
{
	struct battery_info *info = &BQ_info;

	mutex_init(&info->lock);

	info->suspend_capacity = -1;
	info->early_suspend_capacity = -1;

#if defined(CONFIG_EARLYSUSPEND)
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	info->early_suspend.suspend = battery_early_suspend;
	info->early_suspend.resume = battery_late_resume;
	register_early_suspend(&info->early_suspend);
#endif
#if defined(CONFIG_FB)
	info->notifier.notifier_call = batt_fb_notifier_callback;
	fb_register_client(&info->notifier);
#endif
	mutex_lock(&info->lock);
	info->flags = 0;
	mutex_unlock(&info->lock);

	charging_time = current_kernel_time();
	discharging_time = current_kernel_time();

	return 0;
}
static void metrics_uninit(void)
{
#if defined(CONFIG_EARLYSUSPEND)
	struct battery_info *info = &BQ_info;
	unregister_early_suspend(&info->early_suspend);
#endif
#if defined(CONFIG_FB)
	struct battery_info *info = &BQ_info;
	fb_unregister_client(&info->notifier);
#endif
}
static void metrics_suspend(void)
{
	struct battery_info *info = &BQ_info;

	/* Cache the current capacity */
	info->suspend_capacity = BMT_status.UI_SOC;
	info->suspend_bat_car = battery_meter_get_car();

	pr_info("%s: setting suspend_bat_car to %d\n",
			__func__, info->suspend_bat_car);

	battery_critical_voltage_check();

	/* Mark the suspend time */
	info->suspend_time = current_kernel_time();
}

#endif /* CONFIG_AMAZON_METRICS_LOG */

/* ////////////////////////////////////////////////////////////////////////////// */
/* FOR ANDROID BATTERY SERVICE */
/* ////////////////////////////////////////////////////////////////////////////// */

struct wireless_data {
	struct power_supply psy;
	int WIRELESS_ONLINE;
	int wireless_old_state;
};

struct ac_data {
	struct power_supply psy;
	int AC_ONLINE;
	int ac_present;
	int ac_present_old;
	int ac_old_state;
};

struct usb_data {
	struct power_supply psy;
	int USB_ONLINE;
	int usb_present;
	int usb_present_old;
	int usb_old_state;
};

struct battery_data {
	struct power_supply psy;
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
#ifdef CONFIG_AMAZON_METRICS_LOG
	int old_CAR;    /* as read from hardware */
	int BAT_ChargeCounter;   /* monotonically declining */
	int BAT_ChargeFull;
	int BAT_SuspendDrain;
	int BAT_SuspendDrainHigh;
	int BAT_SuspendRealtime;
#endif
	/* Dual battery */
	int status_smb;
	int capacity_smb;
	int present_smb;
	int adjust_power;
	struct mtk_cooler_platform_data *cool_dev;
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
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	/* Add for Battery Service */
	POWER_SUPPLY_PROP_batt_vol,
	POWER_SUPPLY_PROP_batt_temp,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
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

	battery_charging_control(CHARGING_CMD_GET_CHARGER_DET_STATUS, &chr_status);
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
/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // PMIC PCHR Related APIs */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
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
			pr_debug("[upmu_is_chr_det] Charger exist but USB is host\n");

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

	wake_lock(&battery_fg_lock);
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
	struct wireless_data *data = container_of(psy, struct wireless_data, psy);

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
	struct ac_data *data = container_of(psy, struct ac_data, psy);

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
	struct usb_data *data = container_of(psy, struct usb_data, psy);

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
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
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
	struct battery_data *data = container_of(psy, struct battery_data, psy);

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
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = data->BAT_ChargeCounter;
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
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		if (data->cool_dev)
			val->intval = data->cool_dev->state;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		if (data->cool_dev)
			val->intval = data->cool_dev->max_state;
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
	int level_up;
#ifdef CONFIG_USB_AMAZON_DOCK
	int state;
#endif
	static int level;
	static bool bcct_enable = bcct_false;
	struct battery_data *data = container_of(psy, struct battery_data, psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		if (!data->cool_dev)
			return 0;
		/*current state == last state*/
		if (data->cool_dev->state == val->intval)
			return 0;
		else {
			if (val->intval > data->cool_dev->state)
				level_up = 1;
			else
				level_up = 0;
		}
		/*last state = current state*/
		data->cool_dev->state = \
		(val->intval > data->cool_dev->max_state) ? data->cool_dev->max_state : val->intval;
		switch (bcct_enable) {
		case bcct_false:
			level = data->cool_dev->levels[data->cool_dev->state - 1];
			set_bat_charging_current_limit(level/100);
			bcct_enable = bcct_true;
			break;
		case bcct_true:
			if (!data->cool_dev->state) {
				set_bat_charging_current_limit(-1);
				bcct_enable = bcct_false;
			} else {
				data->cool_dev->level = level;
				level = data->cool_dev->levels[data->cool_dev->state - 1];
				if (level > data->cool_dev->level && 1 == level_up)
					return 0;
				set_bat_charging_current_limit(level/100);
				bcct_enable = bcct_true;
			}
			break;
		default:
			break;
		}
		break;
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
	.psy = {
		.name = "wireless",
		.type = POWER_SUPPLY_TYPE_WIRELESS,
		.properties = wireless_props,
		.num_properties = ARRAY_SIZE(wireless_props),
		.get_property = wireless_get_property,
		},
	.WIRELESS_ONLINE = 0,
	.wireless_old_state = -1,
};

/* ac_data initialization */
static struct ac_data ac_main = {
	.psy = {
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.properties = ac_props,
		.num_properties = ARRAY_SIZE(ac_props),
		.get_property = ac_get_property,
		},
	.AC_ONLINE = 0,
	.ac_present = 0,
	.ac_old_state = -1,
};

/* usb_data initialization */
static struct usb_data usb_main = {
	.psy = {
		.name = "usb",
		.type = POWER_SUPPLY_TYPE_USB,
		.properties = usb_props,
		.num_properties = ARRAY_SIZE(usb_props),
		.get_property = usb_get_property,
		},
	.USB_ONLINE = 0,
	.usb_present = 0,
	.usb_old_state = -1,
};


static struct mtk_cooler_platform_data cooler = {
	.type = "battery",
	.state = 0,
	.max_state = THERMAL_MAX_TRIPS,
	.level = 0,
	.levels = {
		CHARGE_CURRENT_1000_00_MA, CHARGE_CURRENT_1000_00_MA,
		CHARGE_CURRENT_1000_00_MA, CHARGE_CURRENT_1000_00_MA,
		CHARGE_CURRENT_500_00_MA, CHARGE_CURRENT_500_00_MA,
		CHARGE_CURRENT_500_00_MA, CHARGE_CURRENT_250_00_MA,
		CHARGE_CURRENT_250_00_MA, CHARGE_CURRENT_250_00_MA,
		CHARGE_CURRENT_250_00_MA, CHARGE_CURRENT_250_00_MA,
	},
};

/* battery_data initialization */
static struct battery_data battery_main = {
	.psy = {
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
#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT) || defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
	.BAT_CAPACITY = -1,
#else
	.BAT_CAPACITY = 50,
#endif
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
/* must be called with info->lock held */
static void
battery_metrics_locked(struct battery_info *info)
{
	struct timespec diff;

	diff = timespec_sub(current_kernel_time(), info->suspend_time);

	if (info->suspend_capacity != -1) {
		char buf[256];
		int drain_diff;
		int drain_diff_high;
		struct timespec boot_time;
		long boot_msec, elaps_msec;

		getrawmonotonic(&boot_time);

		boot_msec = boot_time.tv_sec * 1000
			   + boot_time.tv_nsec / NSEC_PER_MSEC;
		elaps_msec = diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC;

		if (elaps_msec >= 0 && elaps_msec <= boot_msec) {
			snprintf(buf, sizeof(buf),
				"suspend_drain:def:value=%d;CT;1,elapsed=%ld;TI;1:NR",
				info->suspend_capacity - BMT_status.UI_SOC,
				elaps_msec);
			log_to_metrics(ANDROID_LOG_INFO, "drain_metrics", buf);
		}


		snprintf(buf, sizeof(buf),
			"batt:def:cap=%d;CT;1,mv=%d;CT;1,current_avg=%d;CT;1,"
			"temp_g=%d;CT;1,charge=%d;CT;1,charge_design=%d;CT;1,aging_factor=%d;"
			"CT;1,battery_cycle=%d;CT;1,columb_sum=%d;CT;1:NR",
			BMT_status.UI_SOC, BMT_status.bat_vol,
			BMT_status.ICharging, BMT_status.temperature,
			gFG_BATT_CAPACITY_aging, /*battery_remaining_charge,?*/
			gFG_BATT_CAPACITY, /*battery_remaining_charge_design*/
			gFG_aging_factor, /* aging factor */
			gFG_battery_cycle,
			gFG_columb_sum
			);
		log_to_metrics(ANDROID_LOG_INFO, "bq24297", buf);

		/* These deltas may not always be positive.
		 * BMT_status.UI_SOC may be stale by as much as 10 seconds.
		 */
		drain_diff = info->suspend_capacity - BMT_status.UI_SOC;
		drain_diff_high = (info->suspend_bat_car - battery_meter_get_car())
				 * 10000 / g_fg_dbg_bat_qmax;
		if (battery_main.BAT_STATUS == POWER_SUPPLY_STATUS_NOT_CHARGING) {
			battery_main.BAT_SuspendDrain += drain_diff;
			battery_main.BAT_SuspendDrainHigh += drain_diff_high;
			battery_main.BAT_SuspendRealtime += diff.tv_sec;

			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf),
				"bq24297:def:drain_diff_high=%d;CT;1,"
				"BAT_SuspendDrainHigh=%d;CT;1:NR",
				drain_diff_high, battery_main.BAT_SuspendDrainHigh);

			log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
		}
	}
}
#endif /* CONFIG_POWER_EXT */
#endif /* CONFIG_AMAZON_METRICS_LOG */

#if !defined(CONFIG_POWER_EXT)
/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Charger_Voltage */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Charger_Voltage(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	pr_notice("[EM] show_ADC_Charger_Voltage : %d\n", BMT_status.charger_vol);
	return sprintf(buf, "%d\n", BMT_status.charger_vol);
}

static ssize_t store_ADC_Charger_Voltage(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Charger_Voltage, 0664, show_ADC_Charger_Voltage, store_ADC_Charger_Voltage);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_0_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_0_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 0));
	pr_notice("[EM] ADC_Channel_0_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_0_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_0_Slope, 0664, show_ADC_Channel_0_Slope, store_ADC_Channel_0_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_1_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_1_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 1));
	pr_notice("[EM] ADC_Channel_1_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_1_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_1_Slope, 0664, show_ADC_Channel_1_Slope, store_ADC_Channel_1_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_2_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_2_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 2));
	pr_notice("[EM] ADC_Channel_2_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_2_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_2_Slope, 0664, show_ADC_Channel_2_Slope, store_ADC_Channel_2_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_3_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_3_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 3));
	pr_notice("[EM] ADC_Channel_3_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_3_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_3_Slope, 0664, show_ADC_Channel_3_Slope, store_ADC_Channel_3_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_4_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_4_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 4));
	pr_notice("[EM] ADC_Channel_4_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_4_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_4_Slope, 0664, show_ADC_Channel_4_Slope, store_ADC_Channel_4_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_5_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_5_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 5));
	pr_notice("[EM] ADC_Channel_5_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_5_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_5_Slope, 0664, show_ADC_Channel_5_Slope, store_ADC_Channel_5_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_6_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_6_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 6));
	pr_notice("[EM] ADC_Channel_6_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_6_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_6_Slope, 0664, show_ADC_Channel_6_Slope, store_ADC_Channel_6_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_7_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_7_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 7));
	pr_notice("[EM] ADC_Channel_7_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_7_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_7_Slope, 0664, show_ADC_Channel_7_Slope, store_ADC_Channel_7_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_8_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_8_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 8));
	pr_notice("[EM] ADC_Channel_8_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_8_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_8_Slope, 0664, show_ADC_Channel_8_Slope, store_ADC_Channel_8_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_9_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_9_Slope(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 9));
	pr_notice("[EM] ADC_Channel_9_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_9_Slope(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_9_Slope, 0664, show_ADC_Channel_9_Slope, store_ADC_Channel_9_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_10_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_10_Slope(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 10));
	pr_notice("[EM] ADC_Channel_10_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_10_Slope(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_10_Slope, 0664, show_ADC_Channel_10_Slope,
		   store_ADC_Channel_10_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_11_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_11_Slope(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 11));
	pr_notice("[EM] ADC_Channel_11_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_11_Slope(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_11_Slope, 0664, show_ADC_Channel_11_Slope,
		   store_ADC_Channel_11_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_12_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_12_Slope(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 12));
	pr_notice("[EM] ADC_Channel_12_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_12_Slope(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_12_Slope, 0664, show_ADC_Channel_12_Slope,
		   store_ADC_Channel_12_Slope);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_13_Slope */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_13_Slope(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_slop + 13));
	pr_notice("[EM] ADC_Channel_13_Slope : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_13_Slope(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_13_Slope, 0664, show_ADC_Channel_13_Slope,
		   store_ADC_Channel_13_Slope);


/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_0_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_0_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 0));
	pr_notice("[EM] ADC_Channel_0_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_0_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_0_Offset, 0664, show_ADC_Channel_0_Offset,
		   store_ADC_Channel_0_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_1_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_1_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 1));
	pr_notice("[EM] ADC_Channel_1_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_1_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_1_Offset, 0664, show_ADC_Channel_1_Offset,
		   store_ADC_Channel_1_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_2_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_2_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 2));
	pr_notice("[EM] ADC_Channel_2_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_2_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_2_Offset, 0664, show_ADC_Channel_2_Offset,
		   store_ADC_Channel_2_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_3_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_3_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 3));
	pr_notice("[EM] ADC_Channel_3_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_3_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_3_Offset, 0664, show_ADC_Channel_3_Offset,
		   store_ADC_Channel_3_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_4_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_4_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 4));
	pr_notice("[EM] ADC_Channel_4_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_4_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_4_Offset, 0664, show_ADC_Channel_4_Offset,
		   store_ADC_Channel_4_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_5_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_5_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 5));
	pr_notice("[EM] ADC_Channel_5_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_5_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_5_Offset, 0664, show_ADC_Channel_5_Offset,
		   store_ADC_Channel_5_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_6_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_6_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 6));
	pr_notice("[EM] ADC_Channel_6_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_6_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_6_Offset, 0664, show_ADC_Channel_6_Offset,
		   store_ADC_Channel_6_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_7_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_7_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 7));
	pr_notice("[EM] ADC_Channel_7_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_7_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_7_Offset, 0664, show_ADC_Channel_7_Offset,
		   store_ADC_Channel_7_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_8_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_8_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 8));
	pr_notice("[EM] ADC_Channel_8_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_8_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_8_Offset, 0664, show_ADC_Channel_8_Offset,
		   store_ADC_Channel_8_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_9_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_9_Offset(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 9));
	pr_notice("[EM] ADC_Channel_9_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_9_Offset(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_9_Offset, 0664, show_ADC_Channel_9_Offset,
		   store_ADC_Channel_9_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_10_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_10_Offset(struct device *dev, struct device_attribute *attr,
					  char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 10));
	pr_notice("[EM] ADC_Channel_10_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_10_Offset(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_10_Offset, 0664, show_ADC_Channel_10_Offset,
		   store_ADC_Channel_10_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_11_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_11_Offset(struct device *dev, struct device_attribute *attr,
					  char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 11));
	pr_notice("[EM] ADC_Channel_11_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_11_Offset(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_11_Offset, 0664, show_ADC_Channel_11_Offset,
		   store_ADC_Channel_11_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_12_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_12_Offset(struct device *dev, struct device_attribute *attr,
					  char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 12));
	pr_notice("[EM] ADC_Channel_12_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_12_Offset(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_12_Offset, 0664, show_ADC_Channel_12_Offset,
		   store_ADC_Channel_12_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_13_Offset */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_13_Offset(struct device *dev, struct device_attribute *attr,
					  char *buf)
{
	int ret_value = 1;

	ret_value = (*(adc_cali_offset + 13));
	pr_notice("[EM] ADC_Channel_13_Offset : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_13_Offset(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_13_Offset, 0664, show_ADC_Channel_13_Offset,
		   store_ADC_Channel_13_Offset);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : ADC_Channel_Is_Calibration */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_ADC_Channel_Is_Calibration(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	int ret_value = 2;

	ret_value = g_ADC_Cali;
	pr_notice("[EM] ADC_Channel_Is_Calibration : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_ADC_Channel_Is_Calibration(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(ADC_Channel_Is_Calibration, 0664, show_ADC_Channel_Is_Calibration,
		   store_ADC_Channel_Is_Calibration);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : Power_On_Voltage */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_Power_On_Voltage(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 3400;
	pr_notice("[EM] Power_On_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_On_Voltage(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Power_On_Voltage, 0664, show_Power_On_Voltage, store_Power_On_Voltage);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : Power_Off_Voltage */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_Power_Off_Voltage(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret_value = 1;

	ret_value = 3400;
	pr_notice("[EM] Power_Off_Voltage : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Power_Off_Voltage(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Power_Off_Voltage, 0664, show_Power_Off_Voltage, store_Power_Off_Voltage);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : Charger_TopOff_Value */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_Charger_TopOff_Value(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	int ret_value = 1;

	ret_value = 4110;
	pr_notice("[EM] Charger_TopOff_Value : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_Charger_TopOff_Value(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Charger_TopOff_Value, 0664, show_Charger_TopOff_Value,
		   store_Charger_TopOff_Value);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : FG_Battery_CurrentConsumption */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_FG_Battery_CurrentConsumption(struct device *dev, struct device_attribute *attr,
						  char *buf)
{
	int ret_value = 8888;

	ret_value = battery_meter_get_battery_current();
	pr_notice("[EM] FG_Battery_CurrentConsumption : %d/10 mA\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_FG_Battery_CurrentConsumption(struct device *dev,
						   struct device_attribute *attr, const char *buf,
						   size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(FG_Battery_CurrentConsumption, 0664, show_FG_Battery_CurrentConsumption,
		   store_FG_Battery_CurrentConsumption);

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Create File For EM : FG_SW_CoulombCounter */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_FG_SW_CoulombCounter(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	signed int ret_value = 7777;

	ret_value = battery_meter_get_car();
	pr_notice("[EM] FG_SW_CoulombCounter : %d\n", ret_value);
	return sprintf(buf, "%u\n", ret_value);
}

static ssize_t store_FG_SW_CoulombCounter(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(FG_SW_CoulombCounter, 0664, show_FG_SW_CoulombCounter,
		   store_FG_SW_CoulombCounter);


static ssize_t show_Charging_CallState(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_notice("call state = %d\n", g_call_state);
	return sprintf(buf, "%u\n", g_call_state);
}

static ssize_t store_Charging_CallState(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t size)
{
	int rv;

	/*rv = sscanf(buf, "%u", &g_call_state);*/
	rv = kstrtouint(buf, 0, &g_call_state);
	if (rv != 0)
		return -EINVAL;
	pr_notice("call state = %d\n", g_call_state);
	return size;
}

static DEVICE_ATTR(Charging_CallState, 0664, show_Charging_CallState, store_Charging_CallState);

static ssize_t show_Charging_Enable(struct device *dev, struct device_attribute *attr, char *buf)
{
	int charging_enable = 1;

	if (cmd_discharging == 1)
		charging_enable = 0;
	else
		charging_enable = 1;

	pr_notice("hold charging = %d\n", !charging_enable);
	return sprintf(buf, "%u\n", charging_enable);
}

static ssize_t store_Charging_Enable(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	int charging_enable = 1;
	int ret;
	int call_wake_up_bat;

	ret = kstrtouint(buf, 0, &charging_enable);

	if((charging_enable == 1 && cmd_discharging != 1) ||
	   (charging_enable == 0 && cmd_discharging == 1)) {
		call_wake_up_bat = 0;
	}
	else{
		call_wake_up_bat = 1;
	}

	if (charging_enable == 1)
		cmd_discharging = 0;
	else if (charging_enable == 0)
		cmd_discharging = 1;

	if(call_wake_up_bat) {
		wake_up_bat();
	}else {
		pr_notice("skip repeated wake_up_bat() when store_Charging_Enable()\n");
	}

	pr_notice("hold charging = %d\n", cmd_discharging);
	return size;
}
static DEVICE_ATTR(Charging_Enable, 0664, show_Charging_Enable, store_Charging_Enable);

static ssize_t show_Custom_Charging_Current(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	pr_notice("custom charging current = %d\n",
			    g_custom_charging_current);
	return sprintf(buf, "%d\n", g_custom_charging_current);
}

static ssize_t store_Custom_Charging_Current(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	int ret, cur;

	ret = kstrtouint(buf, 0, &cur);
	g_custom_charging_current = cur;
	pr_notice("custom charging current = %d\n",
			    g_custom_charging_current);
	wake_up_bat();
	return size;
}

static DEVICE_ATTR(Custom_Charging_Current, 0664, show_Custom_Charging_Current,
		   store_Custom_Charging_Current);

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

static DEVICE_ATTR(Custom_PlugIn_Time, 0664, show_Custom_PlugIn_Time,
		   store_Custom_PlugIn_Time);

static DEVICE_ATTR(Custom_Charging_Mode, 0660, show_Custom_Charging_Mode,
		   store_Custom_Charging_Mode);

static ssize_t show_Custom_RTC_SOC(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	pr_notice("custom rtc soc = %d\n",
			    g_custom_rtc_soc);
	return sprintf(buf, "%d\n", g_custom_rtc_soc);
}

static ssize_t store_Custom_RTC_SOC(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	int ret, cur;

	ret = kstrtouint(buf, 0, &cur);

	if (cur >= 0 && cur <= 100) {
		g_custom_rtc_soc = cur;
		pr_notice("custom rtc soc = %d\n", g_custom_rtc_soc);
		wake_up_bat();
	}
	return size;
}

static DEVICE_ATTR(Custom_RTC_SOC, 0664, show_Custom_RTC_SOC,
		   store_Custom_RTC_SOC);

/* V_0Percent_Tracking */
static ssize_t show_V_0Percent_Tracking(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	pr_notice("V_0Percent_Tracking = %d\n", batt_cust_data.v_0percent_tracking);
	return sprintf(buf, "%u\n", batt_cust_data.v_0percent_tracking);
}

static ssize_t store_V_0Percent_Tracking(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	int rv;

	/*rv = sscanf(buf, "%u", &V_0Percent_Tracking);*/
	rv = kstrtouint(buf, 0, &batt_cust_data.v_0percent_tracking);
	if (rv != 0)
		return -EINVAL;
	pr_notice("V_0Percent_Tracking = %d\n", batt_cust_data.v_0percent_tracking);
	return size;
}

static DEVICE_ATTR(V_0Percent_Tracking, 0664, show_V_0Percent_Tracking, store_V_0Percent_Tracking);


static ssize_t show_Charger_Type(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int chr_ype = CHARGER_UNKNOWN;

	chr_ype = BMT_status.charger_exist ? BMT_status.charger_type : CHARGER_UNKNOWN;

	pr_notice("CHARGER_TYPE = %d\n", chr_ype);
	return sprintf(buf, "%u\n", chr_ype);
}

static ssize_t store_Charger_Type(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t size)
{
	pr_notice("[EM] Not Support Write Function\n");
	return size;
}

static DEVICE_ATTR(Charger_Type, 0664, show_Charger_Type, store_Charger_Type);

static ssize_t show_usbin_now(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int usbin_mv = 0;

	usbin_mv = battery_meter_get_charger_voltage();
	return sprintf(buf, "%d\n", usbin_mv);
}

static DEVICE_ATTR(usbin_now, 0444, show_usbin_now, NULL);


static ssize_t show_aicl_vbus_state_phase(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_debug("aicl_vbus_state_phase = %d\n", batt_cust_data.aicl_vbus_state_phase);
	return sprintf(buf, "%u\n", batt_cust_data.aicl_vbus_state_phase);
}

static ssize_t store_aicl_vbus_state_phase(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t size)
{
	int ret = 0, phase_ms = 0;

	ret = kstrtoint(buf, 0, &phase_ms);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	if (phase_ms > 0 && phase_ms < 100) {
		batt_cust_data.aicl_vbus_state_phase = phase_ms;
	} else {
		pr_info("[%s] out of range: 0x%x\n", __func__, phase_ms);
	}
	return size;
}

static DEVICE_ATTR(aicl_vbus_state_phase, 0664, show_aicl_vbus_state_phase, store_aicl_vbus_state_phase);

static ssize_t show_aicl_vbus_valid(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_debug("aicl_vbus_valid = %d\n", batt_cust_data.aicl_vbus_valid);
	return sprintf(buf, "%u\n", batt_cust_data.aicl_vbus_valid);
}

static ssize_t store_aicl_vbus_valid(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t size)
{
	int ret = 0, valid_mv = 0;

	ret = kstrtoint(buf, 0, &valid_mv);
	if (ret) {
		pr_err("wrong format!\n");
		return size;
	}

	if (valid_mv >= 4400 && valid_mv <= 4800) {
		batt_cust_data.aicl_vbus_valid = valid_mv;
	} else {
		pr_info("[%s] out of range: 0x%x\n", __func__, valid_mv);
	}
	return size;
}

static DEVICE_ATTR(aicl_vbus_valid, 0664, show_aicl_vbus_valid, store_aicl_vbus_valid);


static ssize_t show_aicl_input_current_max(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_debug("aicl_input_current_max = %d\n", batt_cust_data.aicl_input_current_max);
	return sprintf(buf, "%u\n", batt_cust_data.aicl_input_current_max / 100);
}

static ssize_t store_aicl_aicl_input_current_max(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t size)
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
	} else {
		pr_info("[%s] out of range: 0x%x\n", __func__, iusb_max);
	}
	return size;
}

static DEVICE_ATTR(aicl_input_current_max, 0664, show_aicl_input_current_max, store_aicl_aicl_input_current_max);

static ssize_t show_aicl_input_current_min(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_debug("ap15_charger_input_current_min = %d\n", batt_cust_data.aicl_input_current_min);
	return sprintf(buf, "%u\n", batt_cust_data.aicl_input_current_min / 100);
}

static ssize_t store_aicl_aicl_input_current_min(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t size)
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
	} else {
		pr_info("[%s] out of range: 0x%x\n", __func__, iusb_min);
	}
	return size;
}

static DEVICE_ATTR(aicl_input_current_min, 0664, show_aicl_input_current_min, store_aicl_aicl_input_current_min);



static ssize_t show_aicl_result(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_debug("BMT_status.aicl_result = %d\n", BMT_status.aicl_result / 100);
	return sprintf(buf, "%u\n", BMT_status.aicl_result / 100);
}

static DEVICE_ATTR(aicl_result, 0444, show_aicl_result, NULL);

static ssize_t show_recharge_counter(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", BMT_status.recharge_cnt);
}

static DEVICE_ATTR(recharge_counter, 0444, show_recharge_counter, NULL);

static ssize_t show_charger_plugin_counter(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", BMT_status.charger_plugin_cnt);
}

static DEVICE_ATTR(charger_plugin_counter, 0444, show_charger_plugin_counter, NULL);


static ssize_t show_battery_cmd(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	pr_notice("bat_tt_enable=%d, bat_thr_test_mode=%d, bat_thr_test_value=%d\n",
			g_battery_thermal_throttling_flag,
			battery_cmd_thermal_test_mode, battery_cmd_thermal_test_mode_value);

	return sprintf(buf, "%d, %d, %d\n", g_battery_thermal_throttling_flag,
		battery_cmd_thermal_test_mode, battery_cmd_thermal_test_mode_value);
}

static ssize_t store_battery_cmd(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	char* pvalue, *pvalue_buf[4];
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
			battery_cmd_thermal_test_mode, battery_cmd_thermal_test_mode_value);
	}

	return size;
}

static DEVICE_ATTR(battery_cmd, 0664, show_battery_cmd, store_battery_cmd);

#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT) || defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
static ssize_t show_Pump_Express(struct device *dev, struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
	int icount = 20;	/* max debouncing time 20 * 0.2 sec */

	if (true == ta_check_chr_type &&
	    STANDARD_CHARGER == BMT_status.charger_type &&
	    BMT_status.SOC >= TA_START_BATTERY_SOC && BMT_status.SOC < TA_STOP_BATTERY_SOC) {
		pr_notice("[%s]Wait for PE detection\n", __func__);
		do {
			icount--;
			msleep(200);
		} while (icount && ta_check_chr_type);
	}
#endif

#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)

	if ((true == ta_check_chr_type) && (STANDARD_CHARGER == BMT_status.charger_type)) {
		pr_notice("[%s]Wait for PE detection\n", __func__);
		do {
			msleep(200);
		} while (ta_check_chr_type);
	}
#endif


	pr_notice("Pump express = %d\n", is_ta_connect);
	return sprintf(buf, "%u\n", is_ta_connect);
}

static ssize_t store_Pump_Express(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t size)
{
	int rv;

	/*rv = sscanf(buf, "%u", &is_ta_connect);*/
	rv = kstrtouint(buf, 0, &is_ta_connect);
	if (rv != 0)
		return -EINVAL;
	pr_notice("Pump express= %d\n", is_ta_connect);
	return size;
}

static DEVICE_ATTR(Pump_Express, 0664, show_Pump_Express, store_Pump_Express);
#endif

static void mt_battery_update_EM(struct battery_data *bat_data)
{
	bat_data->BAT_CAPACITY = BMT_status.UI_SOC;
	bat_data->BAT_TemperatureR = BMT_status.temperatureR;	/* API */
	bat_data->BAT_TempBattVoltage = BMT_status.temperatureV;	/* API */
	bat_data->BAT_InstatVolt = BMT_status.bat_vol;	/* VBAT */
	bat_data->BAT_BatteryAverageCurrent = BMT_status.ICharging;
	bat_data->BAT_BatterySenseVoltage = BMT_status.bat_vol;
	bat_data->BAT_ISenseVoltage = BMT_status.Vsense;	/* API */
	bat_data->BAT_ChargerVoltage = BMT_status.charger_vol;
	/* Dual battery */
	bat_data->status_smb = g_status_smb;
	bat_data->capacity_smb = g_capacity_smb;
	bat_data->present_smb = g_present_smb;
	pr_debug("status_smb = %d, capacity_smb = %d, present_smb = %d\n",
		    bat_data->status_smb, bat_data->capacity_smb, bat_data->present_smb);
/*	if ((BMT_status.UI_SOC == 100) && (BMT_status.charger_exist == true))
		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_FULL;
*/
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
	unsigned int cust_sync_time = batt_cust_data.cust_soc_jeita_sync_time;
#else
	unsigned int cust_sync_time = batt_cust_data.onehundred_percent_tracking_time;
#endif
	static unsigned int timer_counter;
	static int timer_counter_init;

	/* Init timer_counter for 1st time */
	if (timer_counter_init == 0) {
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		timer_counter = (batt_cust_data.cust_soc_jeita_sync_time / BAT_TASK_PERIOD);
#else
		timer_counter = (batt_cust_data.onehundred_percent_tracking_time / BAT_TASK_PERIOD);
#endif
		timer_counter_init = 1;
	}

	if (BMT_status.bat_full == true) {	/* charging full first, UI tracking to 100% */
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
			if (timer_counter >= (cust_sync_time / BAT_TASK_PERIOD)) {
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

			pr_debug("[100percent],UI_SOC = %d\n", BMT_status.UI_SOC);
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
		timer_counter = (batt_cust_data.npercent_tracking_time / BAT_TASK_PERIOD);
	}


	if (BMT_status.nPrecent_UI_SOC_check_point == 0)
		return false;

	/* fuel gauge ZCV < 15%, but UI > 15%,  15% can be customized */
	if ((BMT_status.ZCV <= BMT_status.nPercent_ZCV)
	    && (BMT_status.UI_SOC > BMT_status.nPrecent_UI_SOC_check_point)) {
		if (timer_counter == (batt_cust_data.npercent_tracking_time / BAT_TASK_PERIOD)) {
			/* every x sec decrease UI percentage */
			BMT_status.UI_SOC--;
			timer_counter = 1;
		} else {
			timer_counter++;
			return resetBatteryMeter;
		}

		resetBatteryMeter = true;

		pr_debug(
			    "[nPercent] ZCV %d <= nPercent_ZCV %d, UI_SOC=%d., tracking UI_SOC=%d\n",
			    BMT_status.ZCV, BMT_status.nPercent_ZCV, BMT_status.UI_SOC,
			    BMT_status.nPrecent_UI_SOC_check_point);
	} else if ((BMT_status.ZCV > BMT_status.nPercent_ZCV)
		   && (BMT_status.UI_SOC == BMT_status.nPrecent_UI_SOC_check_point)) {
		/* UI less than 15 , but fuel gague is more than 15, hold UI 15% */
		timer_counter = (batt_cust_data.npercent_tracking_time / BAT_TASK_PERIOD);
		resetBatteryMeter = true;

		pr_notice(
			    "[nPercent] ZCV %d > BMT_status.nPercent_ZCV %d and UI SOC=%d, then keep %d.\n",
			    BMT_status.ZCV, BMT_status.nPercent_ZCV, BMT_status.UI_SOC,
			    BMT_status.nPrecent_UI_SOC_check_point);
	} else {
		timer_counter = (batt_cust_data.npercent_tracking_time / BAT_TASK_PERIOD);
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
		if ((BMT_status.bat_vol > batt_cust_data.system_off_voltage) && (BMT_status.UI_SOC > 1))
			BMT_status.UI_SOC--;
		else if (BMT_status.bat_vol <= batt_cust_data.system_off_voltage)
			BMT_status.UI_SOC--;

	}

	pr_notice("0Percent, VBAT < %d UI_SOC=%d\r\n", batt_cust_data.system_off_voltage,
		    BMT_status.UI_SOC);

	return resetBatteryMeter;
}


static void mt_battery_Sync_UI_Percentage_to_Real(void)
{
	static unsigned int timer_counter;

	if ((BMT_status.UI_SOC > BMT_status.SOC) && ((BMT_status.UI_SOC != 1))) {
#if !defined(SYNC_UI_SOC_IMM)
		/* reduce after xxs */
		if (chr_wake_up_bat == false) {
			if (timer_counter ==
			    (batt_cust_data.sync_to_real_tracking_time / BAT_TASK_PERIOD)) {
				BMT_status.UI_SOC--;
				timer_counter = 0;
			}
#ifdef FG_BAT_INT
			else if (fg_wake_up_bat == true)
				BMT_status.UI_SOC--;

#endif				/* #ifdef FG_BAT_INT */
			else
				timer_counter++;

		} else {
			pr_debug(
				    "[Sync_Real] chr_wake_up_bat=1 , do not update UI_SOC\n");
		}
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
		else if (BMT_status.charger_exist && BMT_status.bat_charging_state != CHR_ERROR) {
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
}

static void battery_update(struct battery_data *bat_data)
{
	struct power_supply *bat_psy = &bat_data->psy;
	bool resetBatteryMeter = false;
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[256] = {0};
	unsigned char charger_fault_type;
	unsigned long virtual_temp = get_virtualsensor_temp();
	long elaps_sec;
	long elaps_msec;
	struct timespec diff;
	struct timespec curr;
#endif
	static int bat_status_old = POWER_SUPPLY_STATUS_UNKNOWN;

	bat_data->BAT_TECHNOLOGY = POWER_SUPPLY_TECHNOLOGY_LION;
	bat_data->BAT_HEALTH = POWER_SUPPLY_HEALTH_GOOD;
	/* healthd reading voltage in microVoltage unit */
	bat_data->BAT_batt_vol = BMT_status.bat_vol * 1000;
	bat_data->BAT_batt_temp = BMT_status.temperature * 10;
	bat_data->BAT_PRESENT = BMT_status.bat_exist;

	if ((BMT_status.charger_exist == true) && (BMT_status.bat_charging_state != CHR_ERROR)) {
		if (BMT_status.bat_exist) {	/* charging */
			if (BMT_status.bat_vol <= batt_cust_data.v_0percent_tracking)
				resetBatteryMeter = mt_battery_0Percent_tracking_check();
			else
				resetBatteryMeter = mt_battery_100Percent_tracking_check();
#ifdef CONFIG_AMAZON_METRICS_LOG
			if (BMT_status.bat_full && !bat_full_flag) {
				bat_full_flag = 1;
				curr = current_kernel_time();
				diff = timespec_sub(curr,charging_time);

				elaps_sec = diff.tv_sec + diff.tv_nsec/NSEC_PER_SEC;

				snprintf(buf,sizeof(buf),
					"bq24297:def:Charger_Exist=%d;CT;1,Elaps_Sec=%ld;CT;1,Initial_Bat_Vol=%d;CT;1,"
					"Final_Bal_Vol=%d;CT;1,UI_SOC=%d;CT;1,SOC=%d;CT;1,Bat_Avg_Temp=%d;CT;1,"
					"Vir_Avg_Temp=%ld;CT;1,Bat_Cycle_Count=%d;CT;1:NA",
					BMT_status.charger_exist, elaps_sec, init_charging_vol, BMT_status.bat_vol, BMT_status.UI_SOC,
					BMT_status.SOC, BMT_status.temperature, virtual_temp, gFG_battery_cycle);

				log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
				memset(buf, 0, sizeof(buf));
			}

			if (!BMT_status.bat_full && bat_full_flag) {
				bat_full_flag = 0;
			}
#endif

			{
				unsigned int status;
				battery_charging_control(CHARGING_CMD_GET_CHARGING_STATUS, &status);
				if (status == true) {
					bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_FULL;
					pr_notice("battery status: %s\n", "full");
				} else {
					bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_CHARGING;
					pr_notice("battery status: %s\n", "charging");
				}
			}
		} else {	/* No Battery, Only Charger */

			bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_UNKNOWN;
			pr_notice("battery status: %s\n", "unknown");
			BMT_status.UI_SOC = 0;
		}

	} else {		/* Only Battery */

		bat_data->BAT_STATUS = POWER_SUPPLY_STATUS_DISCHARGING;
		pr_notice("battery status: %s\n", "discharging");
		if (BMT_status.bat_vol <= batt_cust_data.v_0percent_tracking)
			resetBatteryMeter = mt_battery_0Percent_tracking_check();
		else
			resetBatteryMeter = mt_battery_nPercent_tracking_check();
	}

	if (resetBatteryMeter == true) {
		battery_meter_reset();
	} else {
		if (BMT_status.bat_full) {
			BMT_status.UI_SOC = 100;
			pr_debug("[recharging] UI_SOC=%d, SOC=%d\n",
				    BMT_status.UI_SOC, BMT_status.SOC);
		} else {
			mt_battery_Sync_UI_Percentage_to_Real();
		}
	}

#ifdef CONFIG_AMAZON_METRICS_LOG
	if(BMT_status.SOC == 1 && !bat_empty_flag) {
		bat_empty_flag = 1;
		curr = current_kernel_time();
		diff = timespec_sub(curr,discharging_time);
		elaps_sec = diff.tv_sec + diff.tv_nsec/NSEC_PER_SEC;

		snprintf(buf, sizeof(buf),
			"bq24297:def:Charger_Exist=%d;CT;1,Elaps_Sec=%ld;CT;1,Initial_Bat_Vol=%d;CT;1,"
			"Final_Bal_Vol=%d;CT;1,UI_SOC=%d;CT;1,SOC=%d;CT;1,Bat_Avg_Temp=%d;CT;1,"
			"Vir_Avg_Temp=%ld;CT;1,Bat_Cycle_Count=%d;CT;1:NA",
			BMT_status.charger_exist, elaps_sec, init_charging_vol, BMT_status.bat_vol, BMT_status.UI_SOC,
			BMT_status.SOC, BMT_status.temperature, virtual_temp, gFG_battery_cycle);

		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
		memset(buf, 0, sizeof(buf));

		snprintf(buf, sizeof(buf),
			"bq24297:def:battery_soc=%d;CT;1:NA", BMT_status.SOC);
		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
		memset(buf, 0, sizeof(buf));
	}

	if (BMT_status.SOC > 0 && bat_empty_flag)
		bat_empty_flag = 0;

	if (BMT_status.SOC >= 96 && !bat_96_93_flag) {
		bat_96_93_flag = true;
		curr = current_kernel_time();
	}

	if (BMT_status.SOC <= 93 && bat_96_93_flag) {
		bat_96_93_flag = false;
		curr =  current_kernel_time();
		diff = timespec_sub(curr, charging_96_93_time);
		elaps_msec = diff.tv_sec*1000 + diff.tv_nsec/NSEC_PER_MSEC;

		snprintf(buf, sizeof(buf),
			"bq24297:def:time_soc96_soc93=%ld;CT;1:NA",
			elaps_msec);

		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
		memset(buf, 0, sizeof(buf));
	}

	if (BMT_status.SOC <= 15 && !bat_15_20_flag) {
		bat_15_20_flag = true;
		charging_15_20_time = current_kernel_time();
	}

	if (BMT_status.SOC == 1 && bat_15_20_flag && !bat_1_flag) {
		bat_1_flag = true;
		curr = current_kernel_time();
		diff = timespec_sub(curr,charging_15_20_time);
		elaps_msec = diff.tv_sec*1000 + diff.tv_nsec/NSEC_PER_MSEC;

		snprintf(buf, sizeof(buf),
			"bq24297:def:time_soc15_soc1=%ld;CT;1:NA",
			elaps_msec);

		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
		memset(buf, 0, sizeof(buf));
	}

	if (BMT_status.SOC >= 20 && bat_15_20_flag) {
		bat_15_20_flag = false;
		bat_1_flag = false;
		curr = current_kernel_time();
		diff = timespec_sub(curr,charging_15_20_time);
		elaps_msec = diff.tv_sec*1000 + diff.tv_nsec/NSEC_PER_MSEC;

		snprintf(buf, sizeof(buf),
			"bq24297:def:time_soc15_soc20=%ld;CT;1:NA",
			elaps_msec);

		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
		memset(buf, 0, sizeof(buf));
	}

	if (BMT_status.SOC >= 20 && bat_0_20_flag) {
		bat_0_20_flag = false;
		curr = current_kernel_time();
		diff = timespec_sub(curr,charging_0_20_time);
		elaps_msec = diff.tv_sec*1000 + diff.tv_nsec/NSEC_PER_MSEC;

		if(elaps_msec > 60000) {
			snprintf(buf, sizeof(buf),
				"bq24297:def:time_soc0_soc20=%ld;CT;1:NA",
				elaps_msec);

			log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
			memset(buf, 0, sizeof(buf));
		}
	}

	battery_charging_control(CHARGING_CMD_GET_FAULT_TYPE, &charger_fault_type);
	if(0!=charger_fault_type) {
		snprintf(buf, sizeof(buf),
			"bq24297:def:charger_fault_type=%u;CT;1:NA",
			charger_fault_type);

		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
	}
#endif

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
		set_rtc_spare_fg_value(battery_meter_get_battery_soc());	/*use battery_soc */

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
	/*extern int dlpt_check_power_off(void);*/
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

#if defined(CONFIG_AMAZON_METRICS_LOG)
	bat_data->BAT_ChargeFull = g_fg_dbg_bat_qmax;
	/*
	 * Correctness of the BatteryStats High-Precision drain metrics
	 * depend on the BatteryStatsExtension plug event handler
	 * reading a value from /sys/.../BAT_ChargeCounter that:
	 *   1. Differs from previous value read only by reflecting
	 *      battery discharge over the interim, not (a) any charging
	 *      or (b) counter resets.
	 *   2. Is reasonably up-to-date relative to the hardware.
	 * Correctness depends on the above for lack of a way to synchronize
	 * the BatteryStats reads with this driver's resets of the hardware
	 * Charge Counter (CAR).
	 *
	 * Satisfy (1a) by ignoring any non-negative new CAR value.
	 * Satisfy (1b) by noting that CAR resets will always be to zero, and
	 * ignoring any non-negative new CAR value.
	 * Satisfy (2) by reading directly from the CAR hardware -- battery_meter_get_car();.
	 */
	{
		int new_CAR = battery_meter_get_car();
/*
		battery_xlog_printk(BAT_LOG_FULL,
			"reading CAR: new_CAR %d, bat_data->old_CAR %d\n",
			new_CAR, bat_data->old_CAR);
*/
		{
			char buf[128];
			snprintf(buf, sizeof(buf),
				"reading CAR: new_CAR %d, bat_data->old_CAR %d\n",
			new_CAR, bat_data->old_CAR);

			/* log_to_metrics(ANDROID_LOG_INFO, "battery", buf); */
		}

		if (new_CAR < 0) {
			bat_data->BAT_ChargeCounter += (new_CAR - bat_data->old_CAR);
/*
			battery_xlog_printk(BAT_LOG_FULL,
				"setting BAT_ChargeCounter to %d\n",
				bat_data->BAT_ChargeCounter);
*/
			{
				char buf[128];
				snprintf(buf, sizeof(buf),
					"setting BAT_ChargeCounter to %d\n",
				bat_data->BAT_ChargeCounter);

				/* log_to_metrics(ANDROID_LOG_INFO, "battery", buf); */
			}
		}
		bat_data->old_CAR = new_CAR;
	}
	metrics_handle();
#endif


	/* Check recharge */
	if (bat_status_old == POWER_SUPPLY_STATUS_FULL
		&& bat_data->BAT_STATUS == POWER_SUPPLY_STATUS_CHARGING) {
		BMT_status.recharge_cnt += 1;
		pr_info("%s: start recharge: counter=%d, Vbat=%d\n", __func__,
				BMT_status.recharge_cnt, BMT_status.bat_vol);
#if defined(CONFIG_AMAZON_METRICS_LOG)
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf),
			"%s:recharge:detected=1;CT;1,vbat=%d;CT;1:NR",
			__func__, BMT_status.bat_vol);
		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
#endif
	}
	bat_status_old = bat_data->BAT_STATUS;
	power_supply_changed(bat_psy);
}

void update_charger_info(int wireless_state)
{
#if defined(CONFIG_POWER_VERIFY)
	pr_notice("[update_charger_info] no support\n");
#else
	g_wireless_state = wireless_state;
	pr_notice("[update_charger_info] get wireless_state=%d\n", wireless_state);

	wake_up_bat();
#endif
}

static void wireless_update(struct wireless_data *wireless_data)
{
	struct power_supply *wireless_psy = &wireless_data->psy;

	if (BMT_status.charger_exist == true || g_wireless_state) {
		if ((BMT_status.charger_type == WIRELESS_CHARGER) || g_wireless_state) {
			wireless_data->WIRELESS_ONLINE = 1;
			wireless_psy->type = POWER_SUPPLY_TYPE_WIRELESS;
		} else {
			wireless_data->WIRELESS_ONLINE = 0;
		}
	} else {
		wireless_data->WIRELESS_ONLINE = 0;
	}
	if (wireless_data->wireless_old_state != wireless_data->WIRELESS_ONLINE) {
		wireless_data->wireless_old_state = wireless_data->WIRELESS_ONLINE;
		power_supply_changed(wireless_psy);
	}
}

static void ac_update(struct ac_data *ac_data)
{
	struct power_supply *ac_psy = &ac_data->psy;

	if (BMT_status.charger_exist == true) {
		ac_data->ac_present = 1;
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
			ac_psy->type = POWER_SUPPLY_TYPE_MAINS;
		} else {
			ac_data->AC_ONLINE = 0;
		}
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
	struct power_supply *usb_psy = &usb_data->psy;

	if (BMT_status.charger_exist == true) {
		usb_data->usb_present = 1;
		if ((BMT_status.charger_type == STANDARD_HOST) ||
		    (BMT_status.charger_type == CHARGING_HOST)) {
			usb_data->USB_ONLINE = 1;
			usb_psy->type = POWER_SUPPLY_TYPE_USB;
		} else {
			usb_data->USB_ONLINE = 0;
		}
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

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Battery Temprature Parameters and functions */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
bool pmic_chrdet_status(void)
{
	if (upmu_is_chr_det() == true)
		return true;
	/*else {*/
		pr_notice("[pmic_chrdet_status] No charger\r\n");
		return false;
	/*}*/
}

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Pulse Charging Algorithm */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
bool bat_is_charger_exist(void)
{
	return get_charger_detect_status();
}


bool bat_is_charging_full(void)
{
	if ((BMT_status.bat_full == true) && (BMT_status.bat_in_recharging_state == false))
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
	return BMT_status.bat_in_recharging_state || BMT_status.bat_full == true;
}


int get_bat_charging_current_level(void)
{
	CHR_CURRENT_ENUM charging_current;

	battery_charging_control(CHARGING_CMD_GET_CURRENT, &charging_current);

	return charging_current;
}


PMU_STATUS do_batt_temp_state_machine(void)
{
	if (BMT_status.temperature == batt_cust_data.err_charge_temperature)
		return PMU_STATUS_FAIL;



	if (batt_cust_data.bat_low_temp_protect_enable) {
		if (BMT_status.temperature < batt_cust_data.min_charge_temperature) {
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
			} else {
				return PMU_STATUS_FAIL;
			}
		}
	}

	if (BMT_status.temperature >= batt_cust_data.max_charge_temperature) {
		pr_err("[BATTERY] Battery Over Temperature !!\n\r");
		g_batt_temp_status = TEMP_POS_HIGH;
		return PMU_STATUS_FAIL;
	} else if (g_batt_temp_status == TEMP_POS_HIGH) {
		if (BMT_status.temperature < batt_cust_data.max_charge_temperature_minus_x_degree) {
			pr_notice(
				    "[BATTERY] Battery Temperature down from %d to %d(%d), allow charging!!\n\r",
				    batt_cust_data.max_charge_temperature, BMT_status.temperature,
				    batt_cust_data.max_charge_temperature_minus_x_degree);
			g_batt_temp_status = TEMP_POS_NORMAL;
			BMT_status.bat_charging_state = CHR_PRE;
			return PMU_STATUS_OK;
		} else {
			return PMU_STATUS_FAIL;
		}
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


static void mt_battery_average_method_init(BATTERY_AVG_ENUM type, unsigned int *bufferdata,
					   unsigned int data, signed int *sum)
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
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
				if (BMT_status.charger_type == STANDARD_CHARGER) {
#else
				if ((BMT_status.charger_type == STANDARD_CHARGER) ||
				    (DISO_data.diso_state.cur_vdc_state == DISO_ONLINE)) {
#endif
					data = batt_cust_data.ac_charger_current / 100;
				} else if (BMT_status.charger_type == CHARGING_HOST) {
					data = batt_cust_data.charging_host_charger_current / 100;
				} else if (BMT_status.charger_type == NONSTANDARD_CHARGER)
					data = batt_cust_data.non_std_ac_charger_current / 100;	/* mA */
				else	/* USB */
					data = batt_cust_data.usb_charger_current / 100;	/* mA */
#ifdef AVG_INIT_WITH_R_SENSE
				data = AVG_INIT_WITH_R_SENSE(data);
#endif
			} else if ((previous_in_recharge_state == false)
				   && (BMT_status.bat_in_recharging_state == true)) {
				batteryBufferFirst = true;
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
				if (BMT_status.charger_type == STANDARD_CHARGER) {
#else
				if ((BMT_status.charger_type == STANDARD_CHARGER) ||
				    (DISO_data.diso_state.cur_vdc_state == DISO_ONLINE)) {
#endif
					data = batt_cust_data.ac_charger_current / 100;
				} else if (BMT_status.charger_type == CHARGING_HOST) {
					data = batt_cust_data.charging_host_charger_current / 100;
				} else if (BMT_status.charger_type == NONSTANDARD_CHARGER)
					data = batt_cust_data.non_std_ac_charger_current / 100;	/* mA */
				else	/* USB */
					data = batt_cust_data.usb_charger_current / 100;	/* mA */
#ifdef AVG_INIT_WITH_R_SENSE
				data = AVG_INIT_WITH_R_SENSE(data);
#endif
			}

			previous_in_recharge_state = BMT_status.bat_in_recharging_state;
		} else {
			if (previous_charger_exist == true) {
				batteryBufferFirst = true;
				previous_charger_exist = false;
				data = 0;
			}
		}
	}
	/* reset charging current window while plug in/out } */

	pr_debug("batteryBufferFirst =%d, data= (%d)\n", batteryBufferFirst, data);

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


static unsigned int mt_battery_average_method(BATTERY_AVG_ENUM type, unsigned int *bufferdata,
					    unsigned int data, signed int *sum,
					    unsigned char batteryIndex)
{
	unsigned int avgdata;

	mt_battery_average_method_init(type, bufferdata, data, sum);

	*sum -= bufferdata[batteryIndex];
	*sum += data;
	bufferdata[batteryIndex] = data;
	avgdata = (*sum) / BATTERY_AVERAGE_SIZE;

	pr_debug("bufferdata[%d]= (%d)\n", batteryIndex, bufferdata[batteryIndex]);
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

	if (bat_meter_timeout == true || bat_spm_timeout == true || fg_wake_up_bat == true) {
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

	BMT_status.ICharging =
	    mt_battery_average_method(BATTERY_AVG_CURRENT, &batteryCurrentBuffer[0], ICharging,
				      &icharging_sum, batteryIndex);

	if (previous_SOC == -1 && bat_vol <= batt_cust_data.v_0percent_tracking) {
		pr_notice(
			    "battery voltage too low, use ZCV to init average data.\n");
		BMT_status.bat_vol =
		    mt_battery_average_method(BATTERY_AVG_VOLT, &batteryVoltageBuffer[0], ZCV,
					      &bat_sum, batteryIndex);
	} else {
		BMT_status.bat_vol =
		    mt_battery_average_method(BATTERY_AVG_VOLT, &batteryVoltageBuffer[0], bat_vol,
					      &bat_sum, batteryIndex);
	}

	if (battery_cmd_thermal_test_mode == 1) {
		pr_notice("test mode , battery temperature is fixed.\n");
	} else {
		BMT_status.temperature =
			filter_battery_temperature(temperature);
	}

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

	previous_SOC = BMT_status.SOC;

	batteryIndex++;
	if (batteryIndex >= BATTERY_AVERAGE_SIZE)
		batteryIndex = 0;


	if (g_battery_soc_ready == false)
		g_battery_soc_ready = true;

	pr_notice(
		    "AvgVbat=(%d),bat_vol=(%d),AvgI=(%d),I=(%d),VChr=(%d),AvgT=(%d),T=(%d),pre_SOC=(%d),SOC=(%d),ZCV=(%d)\n",
		    BMT_status.bat_vol, bat_vol, BMT_status.ICharging, ICharging,
		    BMT_status.charger_vol, BMT_status.temperature, temperature,
		    previous_SOC, BMT_status.SOC, BMT_status.ZCV);
}

static PMU_STATUS mt_battery_CheckBatteryTemp(void)
{
	PMU_STATUS status = PMU_STATUS_OK;

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
		if ((BMT_status.temperature < batt_cust_data.min_charge_temperature)
		    || (BMT_status.temperature == ERR_CHARGE_TEMPERATURE)) {
			pr_notice(
				    "[BATTERY] Battery Under Temperature or NTC fail !!\n\r");
			status = PMU_STATUS_FAIL;
		}
#endif
		if (BMT_status.temperature >= batt_cust_data.max_charge_temperature) {
			pr_notice("[BATTERY] Battery Over Temperature !!\n\r");
			status = PMU_STATUS_FAIL;
		}
	}
#endif

	return status;
}


static PMU_STATUS mt_battery_CheckChargerVoltage(void)
{
	PMU_STATUS status = PMU_STATUS_OK;
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	unsigned int v_charger_max = DISO_data.hv_voltage;
#endif

	if (BMT_status.charger_exist == true) {

		if (batt_cust_data.v_charger_enable) {
			if (BMT_status.charger_vol <= batt_cust_data.v_charger_min) {
				pr_notice("[BATTERY]Charger under voltage!!\r\n");
				BMT_status.bat_charging_state = CHR_ERROR;
				status = PMU_STATUS_FAIL;
			}
		}
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		if (BMT_status.charger_vol >= batt_cust_data.v_charger_max) {
#else
		if (BMT_status.charger_vol >= v_charger_max) {
#endif
			pr_notice("[BATTERY]Charger over voltage !!\r\n");
			BMT_status.charger_protect_status = charger_OVER_VOL;
			BMT_status.bat_charging_state = CHR_ERROR;
			status = PMU_STATUS_FAIL;
		}
	}

	return status;
}


static PMU_STATUS mt_battery_CheckChargingTime(void)
{
	PMU_STATUS status = PMU_STATUS_OK;

	if ((g_battery_thermal_throttling_flag == 2) || (g_battery_thermal_throttling_flag == 3)) {
		pr_debug(
			    "[TestMode] Disable Safety Timer. bat_tt_enable=%d, bat_thr_test_mode=%d, bat_thr_test_value=%d\n",
			    g_battery_thermal_throttling_flag,
			    battery_cmd_thermal_test_mode, battery_cmd_thermal_test_mode_value);

	} else {
		/* Charging OT */
		if (BMT_status.total_charging_time >= batt_cust_data.max_charging_time) {
			pr_notice("[BATTERY] Charging Over Time.\n");

			status = PMU_STATUS_FAIL;
		}
	}

	return status;

}

#if defined(STOP_CHARGING_IN_TAKLING)
static PMU_STATUS mt_battery_CheckCallState(void)
{
	PMU_STATUS status = PMU_STATUS_OK;

	if ((g_call_state == CALL_ACTIVE)
	    && (BMT_status.bat_vol > batt_cust_data.v_cc2topoff_thres))
		status = PMU_STATUS_FAIL;

	return status;
}
#endif

#if defined(CONFIG_MTK_MULTI_BAT_PROFILE_SUPPORT) && \
	defined(MTK_GET_BATTERY_ID_BY_AUXADC)
static PMU_STATUS mt_battery_CheckBatteryConnect(void)
{
	PMU_STATUS status = PMU_STATUS_OK;

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
		battery_charging_control(CHARGING_CMD_SET_ERROR_STATE, &cmd_discharging);
		return;
	} else if (cmd_discharging == 0) {
		BMT_status.bat_charging_state = CHR_PRE;
		battery_charging_control(CHARGING_CMD_SET_ERROR_STATE, &cmd_discharging);
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
	if ((g_battery_thermal_throttling_flag == 2) || (g_battery_thermal_throttling_flag == 3)) {
		pr_debug("[TestMode] Disable Safety Timer : no UI display\n");
	} else {
		if (BMT_status.total_charging_time >= batt_cust_data.max_charging_time)
			/* if(BMT_status.total_charging_time >= 60) //test */
		{
			g_BatteryNotifyCode |= 0x0010;
			pr_notice("[BATTERY] Charging Over Time\n");
		} else {
			g_BatteryNotifyCode &= ~(0x0010);
		}
	}

	pr_notice(
		    "[BATTERY] BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME (%x)\n",
		    g_BatteryNotifyCode);
#endif
}


static void mt_battery_notify_VBat_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0004_VBAT)
	if (BMT_status.bat_vol > 4350)
		/* if(BMT_status.bat_vol > 3800) //test */
	{
		g_BatteryNotifyCode |= 0x0008;
		pr_notice("[BATTERY] bat_vlot(%ld) > 4350mV\n", BMT_status.bat_vol);
	} else {
		g_BatteryNotifyCode &= ~(0x0008);
	}

	pr_notice("[BATTERY] BATTERY_NOTIFY_CASE_0004_VBAT (%x)\n",
		    g_BatteryNotifyCode);

#endif
}


static void mt_battery_notify_ICharging_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0003_ICHARGING)
	if ((BMT_status.ICharging > 1000) && (BMT_status.total_charging_time > 300)) {
		g_BatteryNotifyCode |= 0x0004;
		pr_notice("[BATTERY] I_charging(%ld) > 1000mA\n",
			    BMT_status.ICharging);
	} else {
		g_BatteryNotifyCode &= ~(0x0004);
	}

	pr_notice("[BATTERY] BATTERY_NOTIFY_CASE_0003_ICHARGING (%x)\n",
		    g_BatteryNotifyCode);

#endif
}


static void mt_battery_notify_VBatTemp_check(void)
{
#if defined(BATTERY_NOTIFY_CASE_0002_VBATTEMP)

	if (BMT_status.temperature >= batt_cust_data.max_charge_temperature) {
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
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	unsigned int v_charger_max = DISO_data.hv_voltage;
#endif

#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	if (BMT_status.charger_vol > batt_cust_data.v_charger_max) {
#else
	if (BMT_status.charger_vol > v_charger_max) {
#endif
		g_BatteryNotifyCode |= 0x0001;
		pr_notice("[BATTERY] BMT_status.charger_vol(%d) > %d mV\n",
			    BMT_status.charger_vol, batt_cust_data.v_charger_max);
	} else {
		g_BatteryNotifyCode &= ~(0x0001);
	}
	if (g_BatteryNotifyCode != 0x0000)
		pr_notice(
			    "[BATTERY] BATTERY_NOTIFY_CASE_0001_VCHARGER (%x)\n",
			    g_BatteryNotifyCode);
#endif
}


static void mt_battery_notify_UI_test(void)
{
	if (g_BN_TestMode == 0x0001) {
		g_BatteryNotifyCode = 0x0001;
		pr_notice("[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0001_VCHARGER\n");
	} else if (g_BN_TestMode == 0x0002) {
		g_BatteryNotifyCode = 0x0002;
		pr_notice("[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0002_VBATTEMP\n");
	} else if (g_BN_TestMode == 0x0003) {
		g_BatteryNotifyCode = 0x0004;
		pr_notice(
			    "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0003_ICHARGING\n");
	} else if (g_BN_TestMode == 0x0004) {
		g_BatteryNotifyCode = 0x0008;
		pr_notice("[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0004_VBAT\n");
	} else if (g_BN_TestMode == 0x0005) {
		g_BatteryNotifyCode = 0x0010;
		pr_notice(
			    "[BATTERY_TestMode] BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME\n");
	} else {
		pr_notice("[BATTERY] Unknown BN_TestMode Code : %x\n",
			    g_BN_TestMode);
	}
}


void mt_battery_notify_check(void)
{
	g_BatteryNotifyCode = 0x0000;

	if (g_BN_TestMode == 0x0000) {	/* for normal case */
		pr_debug("[BATTERY] mt_battery_notify_check\n");

		mt_battery_notify_VCharger_check();

		mt_battery_notify_VBatTemp_check();

		mt_battery_notify_ICharging_check();

		mt_battery_notify_VBat_check();

		mt_battery_notify_TotalChargingTime_check();
	} else {		/* for UI test */

		mt_battery_notify_UI_test();
	}
}

static void mt_battery_thermal_check(void)
{
	if ((g_battery_thermal_throttling_flag == 1) || (g_battery_thermal_throttling_flag == 3)) {
		if (battery_cmd_thermal_test_mode == 1) {
			BMT_status.temperature = battery_cmd_thermal_test_mode_value;
			pr_debug(
				    "[Battery] In thermal_test_mode , Tbat=%d\n",
				    BMT_status.temperature);
		}
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		/* ignore default rule */
		if (BMT_status.temperature <= -10) {
			pr_notice("[Battery] Tbat(%d)<= -10, system need power down.\n", BMT_status.temperature);
			life_cycle_set_thermal_shutdown_reason(THERMAL_SHUTDOWN_REASON_BATTERY);
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
					struct power_supply *bat_psy = &bat_data->psy;

					pr_err(
						    "[Battery] Tbat(%d)>=60, system need power down.\n",
						    BMT_status.temperature);

					bat_data->BAT_CAPACITY = 0;

					power_supply_changed(bat_psy);

					if (BMT_status.charger_exist == true) {
						/* can not power down due to charger exist, so need reset system */
						battery_charging_control
						    (CHARGING_CMD_SET_PLATFORM_RESET, NULL);
					}
					/* avoid SW no feedback */
					battery_charging_control(CHARGING_CMD_SET_POWER_OFF, NULL);
					/* mt_power_off(); */
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
#if defined(CONFIG_AMAZON_METRICS_LOG)
			metrics_charger_update(ac_main.AC_ONLINE, usb_main.USB_ONLINE);
#endif
		} else {
			pr_debug("skip mt_battery_update_status.\n");
			skip_battery_update = false;
		}
	}

#endif
}


CHARGER_TYPE mt_charger_type_detection(void)
{
	CHARGER_TYPE CHR_Type_num = CHARGER_UNKNOWN;

	mutex_lock(&charger_type_mutex);

#if defined(CONFIG_MTK_WIRELESS_CHARGER_SUPPORT)
	battery_charging_control(CHARGING_CMD_GET_CHARGER_TYPE, &CHR_Type_num);
	BMT_status.charger_type = CHR_Type_num;
#else
#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	if (BMT_status.charger_type == CHARGER_UNKNOWN) {
#else
	if ((BMT_status.charger_type == CHARGER_UNKNOWN) &&
	    (DISO_data.diso_state.cur_vusb_state == DISO_ONLINE)) {
#endif
		battery_charging_control(CHARGING_CMD_GET_CHARGER_TYPE, &CHR_Type_num);
		BMT_status.charger_type = CHR_Type_num;

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING) &&\
	(defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT) || defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT))
		if (BMT_status.UI_SOC == 100) {
			BMT_status.bat_charging_state = CHR_BATFULL;
			BMT_status.bat_full = true;
			g_charging_full_reset_bat_meter = true;
		}

		if (g_battery_soc_ready == false) {
			if (BMT_status.nPercent_ZCV == 0)
				battery_meter_initial();

			BMT_status.SOC = battery_meter_get_battery_percentage();
		}

		if (BMT_status.bat_vol > 0)
			mt_battery_update_status();

#endif
	}
#endif
	mutex_unlock(&charger_type_mutex);

	return BMT_status.charger_type;
}

CHARGER_TYPE mt_get_charger_type(void)
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
	static bool chr_type_debounce = false;

	if (upmu_is_chr_det() == true) {
		wake_lock(&battery_suspend_lock);

#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		BMT_status.charger_exist = true;
#endif

	if ((false == chr_type_debounce)
		&& (NONSTANDARD_CHARGER == BMT_status.charger_type)) {
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

			mt_charger_type_detection();
			BMT_status.charger_plugin_cnt++;

			if ((BMT_status.charger_type == STANDARD_HOST)
			    || (BMT_status.charger_type == CHARGING_HOST)) {
				mt_usb_connect();
			}
		}
#endif

		pr_notice("[BAT_thread]Cable in, CHR_Type_num=%d\r\n",
			    BMT_status.charger_type);

	} else {
		wake_lock_timeout(&battery_suspend_lock, HZ / 2);

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
}

static void mt_kpoc_power_off_check(void)
{
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	pr_debug(
		    "[mt_kpoc_power_off_check] , chr_vol=%d, boot_mode=%d\r\n",
		    BMT_status.charger_vol, g_platform_boot_mode);
	if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
	    || g_platform_boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		if ((upmu_is_chr_det() == false) && (BMT_status.charger_vol < 2500)) {	/* vbus < 2.5V */
			pr_notice(
				    "[bat_thread_kthread] Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
			battery_charging_control(CHARGING_CMD_SET_POWER_OFF, NULL);
		}
	}
#endif
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
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[128] = {0};
	static const char * const charger_type_text[] = {
		"UNKNOWN", "STANDARD_HOST", "CHARGING_HOST",
		"NONSTANDARD_CHARGER", "STANDARD_CHARGER", "APPLE_2_1A_CHARGER",
		"APPLE_1_0A_CHARGER", "APPLE_0_5A_CHARGER", "WIRELESS_CHARGER"
	};
#endif
	int vbus_stat = 0;
	int otg_en = 0;

	if (g_bat_init_flag == true) {

#ifdef CONFIG_USB_AMAZON_DOCK
		/* Recheck for unpowered dock */
		musb_rerun_dock_detection();
#endif

#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		if (upmu_is_chr_det() == true) {
#else
		battery_charging_control(CHARGING_CMD_GET_DISO_STATE, &DISO_data);
		if ((DISO_data.diso_state.cur_vusb_state == DISO_ONLINE) ||
		    (DISO_data.diso_state.cur_vdc_state == DISO_ONLINE)) {
#endif

			/*
			 * Workabound for abnormal otg status
			 * if code run here and get OTG status form charger IC,
			 * that means someone set OTG status abnormally or HW
			 * issue happens in charger IC, therefor we need clean
			 * OTG mode and ignore this vbus event
			 */
			battery_charging_control(CHARGING_CMD_GET_VBUS_STAT, &vbus_stat);
			if (vbus_stat == VBUS_STAT_OTG) {
				pr_err("%s: incorrect OTG status, clean it and ignore vubs event\n", __func__);
				battery_charging_control(CHARGING_CMD_BOOST_ENABLE, &otg_en);
				return;
			}

			pr_notice("[do_chrdet_int_task] charger exist!\n");
#ifdef CONFIG_AMAZON_METRICS_LOG
			snprintf(buf, sizeof(buf),
				"%s:bq24297:vbus_on=1;CT;1:NR",
				__func__);
			log_to_metrics(ANDROID_LOG_INFO, "USBCableEvent", buf);
#endif

			BMT_status.charger_exist = true;
			if (!is_usb_rdy()) {
				usb_update(&usb_main);
				ac_update(&ac_main);
			}

			wake_lock(&battery_suspend_lock);

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

#ifdef CONFIG_AMAZON_METRICS_LOG
			memset(buf, '\0', sizeof(buf));
			snprintf(buf, sizeof(buf),
				"%s:bq24297:vbus_off=1;CT;1:NR",
				__func__);
			log_to_metrics(ANDROID_LOG_INFO, "USBCableEvent", buf);
#endif

			BMT_status.charger_exist = false;

			if (!is_usb_rdy()) {
				usb_update(&usb_main);
				ac_update(&ac_main);
			}
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
			pr_notice(
				    "turn off charging for no available charging source\n");
			battery_charging_control(CHARGING_CMD_ENABLE, &BMT_status.charger_exist);
#endif

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
			if (g_platform_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
			    || g_platform_boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
				pr_notice(
					    "[pmic_thread_kthread] Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
				battery_charging_control(CHARGING_CMD_SET_POWER_OFF, NULL);
				/* mt_power_off(); */
			}
#endif

			wake_lock_timeout(&battery_suspend_lock, HZ / 2);

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
#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT) || defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
			is_ta_connect = false;
			ta_check_chr_type = true;
			ta_cable_out_occur = true;
#endif

		}

#ifdef CONFIG_AMAZON_METRICS_LOG
		memset(buf, '\0', sizeof(buf));
		if (BMT_status.charger_type > CHARGER_UNKNOWN
			&& BMT_status.charger_type <= WIRELESS_CHARGER) {
			snprintf(buf, sizeof(buf),
				"%s:bq24297:chg_type_%s=1;CT;1:NR",
				__func__,
				charger_type_text[BMT_status.charger_type]);
			log_to_metrics(ANDROID_LOG_INFO, "USBCableEvent", buf);
		}
#endif


		/* Place charger detection and battery update here is used to speed up charging icon display. */

		/* Put time-consuming job in kthread instead of INT context */
		/* mt_battery_charger_detect_check(); */
		if (BMT_status.UI_SOC == 100 && BMT_status.charger_exist == true) {
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
			/* Put time-consuming job in kthread instead of INT context */
			/* mt_battery_update_status(); */
			/* skip_battery_update = true; */

		}
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		DISO_data.chr_get_diso_state = true;
#endif

		wake_up_bat();
	} else {
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		g_vcdt_irq_delay_flag = true;
#endif
		pr_debug(
			    "[do_chrdet_int_task] battery thread not ready, will do after bettery init.\n");
	}

}


void BAT_thread(void)
{
	static bool battery_meter_initilized;
	struct timespec now_time;
	unsigned long total_time_plug_in;
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[256] = {0};
	unsigned long virtual_temp = get_virtualsensor_temp();
#endif

	pr_debug("BAT_thread() called!\n");
	if (battery_meter_initilized == false) {
		battery_meter_initial();	/* move from battery_probe() to decrease booting time */
		BMT_status.nPercent_ZCV = battery_meter_get_battery_nPercent_zcv();
		battery_meter_initilized = true;

#ifdef CONFIG_AMAZON_METRICS_LOG
		if(BMT_status.SOC == 0) {
			bat_0_20_flag = true;
			charging_0_20_time = current_kernel_time();
		}
#endif
	}

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
			(now_time.tv_sec - chr_plug_in_time.tv_sec) + g_custom_plugin_time;

		if (total_time_plug_in > PLUGIN_THRESHOLD) {
			g_custom_charging_cv = BATTERY_VOLT_04_100000_V;
#ifdef CONFIG_AMAZON_METRICS_LOG
			if (!bat_14days_flag) {
				bat_14days_flag = true;
				snprintf(buf, sizeof(buf),
				"bq24297:def:Charging_Over_14days=%d;CT;1,Total_Plug_Time=%ld;CT;1,"
				"Bat_Vol=%d;CT;1,UI_SOC=%d;CT;1,SOC=%d;CT;1,Bat_Temp=%d;CT;1,"
				"Vir_Avg_Temp=%ld;CT;1,Bat_Cycle_count=%d;CT;1:NA",
				1, total_time_plug_in, BMT_status.bat_vol, BMT_status.UI_SOC, BMT_status.SOC, BMT_status.temperature,
				virtual_temp, gFG_battery_cycle);

				log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
				memset(buf, 0, sizeof(buf));
			}
		}

		if (total_time_plug_in <= PLUGIN_THRESHOLD && bat_14days_flag)
			bat_14days_flag = false;

		if (g_custom_charging_mode != 1 && bat_demo_flag)
			bat_demo_flag = false;

		if (g_custom_charging_mode == 1 && !bat_demo_flag) {
			bat_demo_flag = true;
			snprintf(buf, sizeof(buf),
				"bq24297:def:Store_Demo_Mode=%d;CT;1,Total_Plug_Time=%ld;CT;1,"
				"Bat_Vol=%d;CT;1,UI_SOC=%d;CT;1,SOC=%d;CT;1,Bat_Temp=%d;CT;1,"
				"Vir_Avg_Temp=%ld;CT;1,Bat_Cycle_Count=%d;CT;1:NA",
				g_custom_charging_mode, total_time_plug_in, BMT_status.bat_vol, BMT_status.UI_SOC,
				BMT_status.SOC, BMT_status.temperature, virtual_temp, gFG_battery_cycle);

			log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
		}
#endif

		pr_notice("total_time_plug_in(%lu), cv(%d)\r\n",
			total_time_plug_in, g_custom_charging_cv);

		mt_battery_CheckBatteryStatus();
		mt_battery_charging_algorithm();
	}

	mt_battery_update_status();
	mt_kpoc_power_off_check();
}

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Internal API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */

int bat_thread_kthread(void *x)
{
	ktime_t ktime = ktime_set(3, 0);	/* 10s, 10* 1000 ms */

	if (get_boot_mode() != RECOVERY_BOOT && is_usb_rdy() == false) {
		pr_notice("wait for usb ready, block\n");
		wait_event(bat_thread_wq, (is_usb_rdy() == true));
		pr_notice("usb ready, free\n");
	} else {
		pr_notice("usb ready, PASS\n");
	}

	/* Run on a process content */
	while (!fg_battery_shutdown) {
		mutex_lock(&bat_mutex);

		if (((chargin_hw_init_done == true) && (battery_suspended == false))
		    || ((chargin_hw_init_done == true) && (fg_wake_up_bat == true)))
			BAT_thread();

		mutex_unlock(&bat_mutex);

#ifdef FG_BAT_INT
		if (fg_wake_up_bat == true) {
			wake_unlock(&battery_fg_lock);
			fg_wake_up_bat = false;
			pr_debug("unlock battery_fg_lock\n");
		}
#endif				/* #ifdef FG_BAT_INT */

		pr_debug("wait event\n");

		wait_event(bat_thread_wq, (bat_thread_timeout == true) && ((battery_suspended == false) || (fg_wake_up_bat == true)));
		pr_debug("wait_event(bat_thread_wq) OK!\n");

		bat_thread_timeout = false;
		hrtimer_start(&battery_kthread_timer, ktime, HRTIMER_MODE_REL);
		/* 10s, 10* 1000 ms */
		if (!fg_battery_shutdown)
			ktime = ktime_set(BAT_TASK_PERIOD, 0);
		if (chr_wake_up_bat == true && g_smartbook_update != 1) {	/* for charger plug in/ out */
#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
			if (DISO_data.chr_get_diso_state) {
				DISO_data.chr_get_diso_state = false;
				battery_charging_control(CHARGING_CMD_GET_DISO_STATE, &DISO_data);
			}
#endif

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

#if !defined(CONFIG_POWER_EXT)
#if defined(CONFIG_AMAZON_METRICS_LOG)

static void metrics_resume(void)
{
	struct battery_info *info = &BQ_info;
	/* invalidate all the measurements */
	mutex_lock(&info->lock);
	info->flags |= BQ_STATUS_RESUMING;
	mutex_unlock(&info->lock);
}
#endif
#endif /* CONFIG_POWER_EXT */

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // fop API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static long adc_cali_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int *user_data_addr;
	int *naram_data_addr;
	int i = 0;
	int ret = 0;
	int adc_in_data[2] = { 1, 1 };
	int adc_out_data[2] = { 1, 1 };

	mutex_lock(&bat_mutex);

	switch (cmd) {
	case TEST_ADC_CALI_PRINT:
		g_ADC_Cali = false;
		break;

	case SET_ADC_CALI_Slop:
		naram_data_addr = (int *)arg;
		ret = copy_from_user(adc_cali_slop, naram_data_addr, 36);
		g_ADC_Cali = false;	/* enable calibration after setting ADC_CALI_Cal */
		/* Protection */
		for (i = 0; i < 14; i++) {
			if ((*(adc_cali_slop + i) == 0) || (*(adc_cali_slop + i) == 1))
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
		g_ADC_Cali = false;	/* enable calibration after setting ADC_CALI_Cal */
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
		pr_debug("**** unlocked_ioctl : SET_ADC_CALI_Cal Done!\n");
		break;

	case ADC_CHANNEL_READ:
		/* g_ADC_Cali = false; *//* 20100508 Infinity */
		user_data_addr = (int *)arg;
		ret = copy_from_user(adc_in_data, user_data_addr, 8);	/* 2*int = 2*4 */

		if (adc_in_data[0] == 0) {	/* I_SENSE */
			adc_out_data[0] = battery_meter_get_VSense() * adc_in_data[1];
		} else if (adc_in_data[0] == 1) {	/* BAT_SENSE */
			adc_out_data[0] =
			    battery_meter_get_battery_voltage(true) * adc_in_data[1];
		} else if (adc_in_data[0] == 3) {	/* V_Charger */
			adc_out_data[0] = battery_meter_get_charger_voltage() * adc_in_data[1];
			/* adc_out_data[0] = adc_out_data[0] / 100; */
		} else if (adc_in_data[0] == 30) {	/* V_Bat_temp magic number */
			adc_out_data[0] = battery_meter_get_battery_temperature() * adc_in_data[1];
		} else if (adc_in_data[0] == 66) {
			adc_out_data[0] = (battery_meter_get_battery_current()) / 10;

			if (battery_meter_get_battery_current_sign() == true)
				adc_out_data[0] = 0 - adc_out_data[0];	/* charging */

		} else {
			pr_debug("unknown channel(%d,%d)\n",
				    adc_in_data[0], adc_in_data[1]);
		}

		if (adc_out_data[0] < 0)
			adc_out_data[1] = 1;	/* failed */
		else
			adc_out_data[1] = 0;	/* success */

		if (adc_in_data[0] == 30)
			adc_out_data[1] = 0;	/* success */

		if (adc_in_data[0] == 66)
			adc_out_data[1] = 0;	/* success */

		ret = copy_to_user(user_data_addr, adc_out_data, 8);
		pr_notice(
			    "**** unlocked_ioctl : Channel %d * %d times = %d\n",
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
		pr_notice("**** unlocked_ioctl : CAL:%d\n", battery_out_data[0]);
		break;

	case Set_Charger_Current:	/* For Factory Mode */
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
		pr_notice("**** unlocked_ioctl : set_Charger_Current:%d\n",
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
		battery_charging_control(CHARGING_CMD_GET_BATTERY_STATUS, &battery_status);
		baton_count += battery_status;

	}

	if (baton_count >= 3) {
		if ((g_platform_boot_mode == META_BOOT) || (g_platform_boot_mode == ADVMETA_BOOT)
		    || (g_platform_boot_mode == ATE_FACTORY_BOOT)) {
			pr_debug(
				    "[BATTERY] boot mode = %d, bypass battery check\n",
				    g_platform_boot_mode);
		} else {
			pr_notice(
				    "[BATTERY] Battery is not exist, power off system (%d)\n", baton_count);

			battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
			battery_charging_control(CHARGING_CMD_SET_POWER_OFF, NULL);
			if (BMT_status.charger_exist == true)
				battery_charging_control(CHARGING_CMD_SET_PLATFORM_RESET, NULL);
		}
	}
#endif
}


#if defined(MTK_PLUG_OUT_DETECTION)

void charger_plug_out_sw_mode(void)
{
	signed int ICharging = 0;
	signed short i;
	signed short cnt = 0;
	bool enable;
	unsigned int charging_enable;
	signed int VCharger = 0;

	if (BMT_status.charger_exist == true) {
		if (chargin_hw_init_done && upmu_is_chr_det() == true) {

			for (i = 0; i < 4; i++) {
				enable = pmic_get_register_value(PMIC_RG_CHR_EN);

				if (enable == 1) {

					ICharging = battery_meter_get_charging_current_imm();
					VCharger = battery_meter_get_charger_voltage();
					if (ICharging < 70 && VCharger < 4400) {
						cnt++;
						pr_debug(
							    "[charger_hv_detect_sw_thread_handler] fail ICharging=%d , VCHR=%d cnt=%d\n",
							    ICharging, VCharger, cnt);
					} else {
						/* pr_notice(
						 * "[charger_hv_detect_sw_thread_handler] success ICharging=%d,
						 * VCHR=%d cnt=%d\n",ICharging,VCharger,cnt);
						 */
						break;
					}
				} else {
					break;
				}
			}

			if (cnt >= 3) {
				charging_enable = false;
				battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
				pr_debug(
					    "[charger_hv_detect_sw_thread_handler] ICharging=%d VCHR=%d cnt=%d turn off charging\n",
					    ICharging, VCharger, cnt);
			}

		}
	}

}


/*extern unsigned int upmu_get_reg_value(unsigned int reg);*/
void hv_sw_mode(void)
{
	bool hv_status;
	unsigned int charging_enable;

	if (upmu_is_chr_det() == true)
		check_battery_exist();



	if (chargin_hw_init_done)
		battery_charging_control(CHARGING_CMD_GET_HV_STATUS, &hv_status);

	if (hv_status == true) {
		pr_notice("[charger_hv_detect_sw_thread_handler] charger hv\n");

		charging_enable = false;
		if (chargin_hw_init_done)
			battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
	} else
		pr_debug(
			    "[charger_hv_detect_sw_thread_handler] upmu_chr_get_vcdt_hv_det() != 1\n");


	/* pr_notice(
	 * "[PMIC_BIAS_GEN_EN & PMIC_BIAS_GEN_EN_SEL] 0xa=0x%x\n",
	 * upmu_get_reg_value(0x000a));
	 */
	if (pmic_get_register_value(PMIC_BIAS_GEN_EN) == 1
	    || pmic_get_register_value(PMIC_BIAS_GEN_EN_SEL) == 0) {
		pr_debug(
			    "[PMIC_BIAS_GEN_EN & PMIC_BIAS_GEN_EN_SEL] be writen 0xa=0x%x\n",
			    upmu_get_reg_value(0x000a));
		BUG_ON(1);
	}

	if (chargin_hw_init_done)
		battery_charging_control(CHARGING_CMD_RESET_WATCH_DOG_TIMER, NULL);

}

int charger_hv_detect_sw_thread_handler(void *unused)
{
	ktime_t ktime;
	unsigned int hv_voltage = batt_cust_data.v_charger_max * 1000;


	unsigned char cnt = 0;

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	hv_voltage = DISO_data.hv_voltage;
#endif

	do {

		if (BMT_status.charger_exist == true)
			ktime = ktime_set(0, BAT_MS_TO_NS(200));
		else
			ktime = ktime_set(0, BAT_MS_TO_NS(1000));



		if (chargin_hw_init_done)
			battery_charging_control(CHARGING_CMD_SET_HV_THRESHOLD, &hv_voltage);

		wait_event_interruptible(charger_hv_detect_waiter,
					 (charger_hv_detect_flag == true));

		if (BMT_status.charger_exist == true) {
			if (cnt >= 5) {
				/* pr_notice(*/
				/* "[charger_hv_detect_sw_thread_handler] charger in do hv_sw_mode\n"); */
				hv_sw_mode();
				cnt = 0;
			} else {
				cnt++;
			}

			/* pr_notice(*/
			/* "[charger_hv_detect_sw_thread_handler] charger in cnt=%d\n",cnt); */
			charger_plug_out_sw_mode();
		} else {
			/* pr_notice(*/
			/* "[charger_hv_detect_sw_thread_handler] charger out do hv_sw_mode\n"); */
			hv_sw_mode();
		}


		charger_hv_detect_flag = false;
		hrtimer_start(&charger_hv_detect_timer, ktime, HRTIMER_MODE_REL);

	} while (!kthread_should_stop());

	return 0;
}

#else
int charger_hv_detect_sw_thread_handler(void *unused)
{
	ktime_t ktime;
	unsigned int charging_enable;
	unsigned int hv_voltage = batt_cust_data.v_charger_max * 1000;
	bool hv_status;

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	hv_voltage = DISO_data.hv_voltage;
#endif

	do {
		ktime = ktime_set(0, BAT_MS_TO_NS(1000));

		if (chargin_hw_init_done)
			battery_charging_control(CHARGING_CMD_SET_HV_THRESHOLD, &hv_voltage);

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
			battery_charging_control(CHARGING_CMD_GET_HV_STATUS, &hv_status);

		if (fg_battery_shutdown)
			break;

		if (hv_status == true) {
			pr_err("[charger_hv_detect_sw_thread_handler] charger hv\n");

			charging_enable = false;
			if (chargin_hw_init_done)
				battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
		}

		if (fg_battery_shutdown)
			break;

		if (chargin_hw_init_done)
			battery_charging_control(CHARGING_CMD_RESET_WATCH_DOG_TIMER, NULL);

		if (!fg_battery_shutdown)
			hrtimer_start(&charger_hv_detect_timer,
				ktime, HRTIMER_MODE_REL);

	} while (!kthread_should_stop() && !fg_battery_shutdown);

	fg_hv_thread = true;

	return 0;
}
#endif				/* #if defined(MTK_PLUG_OUT_DETECTION) */

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
	hrtimer_init(&charger_hv_detect_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
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

	ktime = ktime_set(1, 0);	/* 3s, 10* 1000 ms */
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

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
static irqreturn_t diso_auxadc_irq_thread(int irq, void *dev_id)
{
	int pre_diso_state = (DISO_data.diso_state.pre_otg_state |
			      (DISO_data.diso_state.pre_vusb_state << 1) |
			      (DISO_data.diso_state.pre_vdc_state << 2)) & 0x7;

	pr_debug(
		    "[DISO]auxadc IRQ threaded handler triggered, pre_diso_state is %s\n",
		    DISO_state_s[pre_diso_state]);

	switch (pre_diso_state) {
#ifdef MTK_DISCRETE_SWITCH	/*for DSC DC plugin handle */
	case USB_ONLY:
#endif
	case OTG_ONLY:
		BMT_status.charger_exist = true;
		wake_lock(&battery_suspend_lock);
		wake_up_bat();
		break;
	case DC_WITH_OTG:
		BMT_status.charger_exist = false;
		/* need stop charger quickly */
		battery_charging_control(CHARGING_CMD_ENABLE, &BMT_status.charger_exist);
		BMT_status.charger_exist = false;	/* reset charger status */
		BMT_status.charger_type = CHARGER_UNKNOWN;
		wake_unlock(&battery_suspend_lock);
		wake_up_bat();
		break;
	case DC_WITH_USB:
		/*usb delayed work will reflact BMT_staus , so need update state ASAP */
		if ((BMT_status.charger_type == STANDARD_HOST)
		    || (BMT_status.charger_type == CHARGING_HOST))
			mt_usb_disconnect();	/* disconnect if connected */
		BMT_status.charger_type = CHARGER_UNKNOWN;	/* reset chr_type */
		wake_up_bat();
		break;
	case DC_ONLY:
		BMT_status.charger_type = CHARGER_UNKNOWN;
		mt_battery_charger_detect_check();	/* plug in VUSB, check if need connect usb */
		break;
	default:
		pr_notice(
			    "[DISO]VUSB auxadc threaded handler triggered ERROR OR TEST\n");
		break;
	}
	return IRQ_HANDLED;
}

static void dual_input_init(void)
{
	DISO_data.irq_callback_func = diso_auxadc_irq_thread;
	battery_charging_control(CHARGING_CMD_DISO_INIT, &DISO_data);
}
#endif
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
			||!batt_cust_data.aicl_input_current_max
			||!batt_cust_data.aicl_input_current_min
			||!batt_cust_data.aicl_step_current
			||!batt_cust_data.aicl_step_interval
			||!batt_cust_data.aicl_vbus_valid
			| !batt_cust_data.aicl_vbus_state_phase) {
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

	/* check customer setting */
	np = of_find_compatible_node(NULL, NULL, "mediatek,battery");
	if (!np) {
		/* printk(KERN_ERR "(E) Failed to find device-tree node: %s\n", path); */
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

	__batt_parse_node(np, "mtk_pump_express_plus_support",
		&batt_cust_data.mtk_pump_express_plus_support);

	__batt_parse_node(np, "ta_start_battery_soc",
		&batt_cust_data.ta_start_battery_soc);

	__batt_parse_node(np, "ta_stop_battery_soc",
		&batt_cust_data.ta_stop_battery_soc);

	__batt_parse_node(np, "ta_ac_9v_input_current",
		&batt_cust_data.ta_ac_9v_input_current);

	__batt_parse_node(np, "ta_ac_7v_input_current",
		&batt_cust_data.ta_ac_7v_input_current);

	__batt_parse_node(np, "ta_ac_charging_current",
		&batt_cust_data.ta_ac_charging_current);

	__batt_parse_node(np, "ta_9v_support",
		&batt_cust_data.ta_9v_support);

	/* For fast charger detection */
	__ap15_charger_detection_read_dt(np);

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

	if (0xF != chg_exist)
		return chg_exist;

	chg_exist = 1;

#if defined(CONFIG_MTK_BQ24296_SUPPORT) || defined(CONFIG_MTK_BQ24297_SUPPORT)
	if ((0x3 == bq24297_get_pn()) || (0x1 == bq24297_get_pn()))
		chg_exist = 0;
#endif

	if (is_bq25601_exist())
		chg_exist = 0;

	return chg_exist;
}
#endif

static ssize_t levels_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct battery_data *data = &battery_main;
	struct mtk_cooler_platform_data *cool_dev = data->cool_dev;
	int i = 0;
	int offset = 0;

	if (!data || !cool_dev)
		return -EINVAL;
	for (i = 0; i < THERMAL_MAX_TRIPS; i++)
		offset += sprintf(buf + offset, "%d %d\n", i+1, cool_dev->levels[i]);
	return offset;
}

static ssize_t levels_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int level, state;
	struct battery_data *data = &battery_main;
	struct mtk_cooler_platform_data *cool_dev = data->cool_dev;

	if (!data || !cool_dev)
		return -EINVAL;
	if (sscanf(buf, "%d %d\n", &state, &level) != 2)
		return -EINVAL;
	if (state >= THERMAL_MAX_TRIPS)
		return -EINVAL;
	cool_dev->levels[state] = level;
	return count;
}
static DEVICE_ATTR(levels, S_IRUGO | S_IWUSR, levels_show, levels_store);

static int detemine_inital_status(void)
{
	usb_main.usb_present = bat_is_charger_exist();
	ac_main.ac_present = usb_main.usb_present;
	BMT_status.bat_vol = battery_meter_get_battery_voltage(true);
	power_supply_changed(&ac_main.psy);
	power_supply_changed(&usb_main.psy);

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
							 NULL,
							 adc_cali_devno, NULL, ADC_CALI_DEVNAME);
	pr_debug("[BAT_probe] adc_cali prepare : done !!\n ");

	get_charging_control();

	batt_init_cust_data();

	battery_charging_control(CHARGING_CMD_GET_PLATFORM_BOOT_MODE, &g_platform_boot_mode);
	pr_debug("[BAT_probe] g_platform_boot_mode = %d\n ", g_platform_boot_mode);

	wake_lock_init(&battery_fg_lock, WAKE_LOCK_SUSPEND, "battery fg wakelock");

	wake_lock_init(&battery_suspend_lock, WAKE_LOCK_SUSPEND, "battery suspend wakelock");
#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT) || defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
	wake_lock_init(&TA_charger_suspend_lock, WAKE_LOCK_SUSPEND, "TA charger suspend wakelock");
#endif

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
	ret = power_supply_register(&(dev->dev), &ac_main.psy);
	if (ret) {
		pr_notice("[BAT_probe] power_supply_register AC Fail !!\n");
		return ret;
	}
	pr_notice("[BAT_probe] power_supply_register AC Success !!\n");

	ret = power_supply_register(&(dev->dev), &usb_main.psy);
	if (ret) {
		pr_notice("[BAT_probe] power_supply_register USB Fail !!\n");
		return ret;
	}
	pr_debug("[BAT_probe] power_supply_register USB Success !!\n");

	ret = power_supply_register(&(dev->dev), &wireless_main.psy);
	if (ret) {
		pr_notice("[BAT_probe] power_supply_register WIRELESS Fail !!\n");
		return ret;
	}
	pr_debug("[BAT_probe] power_supply_register WIRELESS Success !!\n");

	battery_main.cool_dev = &cooler;
	ret = power_supply_register(&(dev->dev), &battery_main.psy);
	if (ret) {
		pr_notice("[BAT_probe] power_supply_register Battery Fail !!\n");
		return ret;
	}
	pr_debug("[BAT_probe] power_supply_register Battery Success !!\n");
	device_create_file(&battery_main.psy.tcd->device, &dev_attr_levels);

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

		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Charger_Voltage);

		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_0_Slope);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_1_Slope);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_2_Slope);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_3_Slope);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_4_Slope);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_5_Slope);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_6_Slope);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_7_Slope);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_8_Slope);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_9_Slope);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_10_Slope);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_11_Slope);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_12_Slope);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_13_Slope);

		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_0_Offset);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_1_Offset);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_2_Offset);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_3_Offset);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_4_Offset);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_5_Offset);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_6_Offset);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_7_Offset);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_8_Offset);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_9_Offset);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_10_Offset);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_11_Offset);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_12_Offset);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_ADC_Channel_13_Offset);

		ret_device_file =
		    device_create_file(&(dev->dev), &dev_attr_ADC_Channel_Is_Calibration);

		ret_device_file = device_create_file(&(dev->dev), &dev_attr_Power_On_Voltage);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_Power_Off_Voltage);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_Charger_TopOff_Value);

		ret_device_file =
		    device_create_file(&(dev->dev), &dev_attr_FG_Battery_CurrentConsumption);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_SW_CoulombCounter);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_Charging_CallState);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_Charging_Enable);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_Custom_Charging_Current);
		ret_device_file =
			device_create_file(&(dev->dev), &dev_attr_Custom_PlugIn_Time);
		ret_device_file =
			device_create_file(&(dev->dev), &dev_attr_Custom_Charging_Mode);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_Custom_RTC_SOC);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_V_0Percent_Tracking);

		ret_device_file = device_create_file(&(dev->dev), &dev_attr_Charger_Type);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_usbin_now);

		ret_device_file = device_create_file(&(dev->dev), &dev_attr_aicl_result);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_aicl_vbus_valid);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_aicl_vbus_state_phase);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_aicl_input_current_max);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_aicl_input_current_min);

		ret_device_file = device_create_file(&(dev->dev), &dev_attr_recharge_counter);
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_charger_plugin_counter);

		ret_device_file = device_create_file(&(dev->dev), &dev_attr_battery_cmd);

#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT) || defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT)
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_Pump_Express);
#endif
	}

	/* battery_meter_initial();      //move to mt_battery_GetBatteryData() to decrease booting time */

	/* Initialization BMT Struct */
	BMT_status.bat_exist = true;	/* phone must have battery */
	BMT_status.charger_exist = false;	/* for default, no charger */
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
	BMT_status.nPrecent_UI_SOC_check_point = battery_meter_get_battery_nPercent_UI_SOC();

	detemine_inital_status();

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

#if defined(CONFIG_AMAZON_METRICS_LOG)
	metrics_init();
#endif

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	if (g_vcdt_irq_delay_flag == true)
		do_chrdet_int_task();
#endif

	return 0;

}

static void battery_timer_pause(void)
{


	/* pr_notice("******** battery driver suspend!! ********\n" ); */
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
#if defined(CONFIG_AMAZON_METRICS_LOG)
	metrics_suspend();
#endif
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
	battery_charging_control(CHARGING_CMD_GET_IS_PCM_TIMER_TRIGGER, &is_pcm_timer_trigger);

	if (is_pcm_timer_trigger == true || bat_spm_timeout) {
		mutex_lock(&bat_mutex);

		if (bat_spm_timeout && (BMT_status.UI_SOC > BMT_status.SOC))
			BMT_status.UI_SOC = BMT_status.SOC;

		BAT_thread();
		mutex_unlock(&bat_mutex);
	} else {
		pr_debug("battery resume NOT by pcm timer!!\n");
	}

	if (g_call_state == CALL_ACTIVE &&
		(bat_time_after_sleep.tv_sec - g_bat_time_before_sleep.tv_sec >= batt_cust_data.talking_sync_time)) {
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
#if defined(CONFIG_AMAZON_METRICS_LOG)
	metrics_resume();
#endif
#endif /* CONFIG_POWER_EXT */
}

static int battery_remove(struct platform_device *dev)
{
#if defined(CONFIG_AMAZON_METRICS_LOG)
	metrics_uninit();
#endif
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

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // Battery Notify API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static ssize_t show_BatteryNotify(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_notice("[Battery] show_BatteryNotify : %x\n", g_BatteryNotifyCode);

	return sprintf(buf, "%u\n", g_BatteryNotifyCode);
}

static ssize_t store_BatteryNotify(struct device *dev, struct device_attribute *attr,
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

static DEVICE_ATTR(BatteryNotify, 0664, show_BatteryNotify, store_BatteryNotify);

static ssize_t show_BN_TestMode(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_notice("[Battery] show_BN_TestMode : %x\n", g_BN_TestMode);
	return sprintf(buf, "%u\n", g_BN_TestMode);
}

static ssize_t store_BN_TestMode(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
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
		pr_notice("[Battery] store g_BN_TestMode : %x\n", g_BN_TestMode);
	}
	return size;
}

static DEVICE_ATTR(BN_TestMode, 0664, show_BN_TestMode, store_BN_TestMode);


/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // platform_driver API */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
static int mt_batteryNotify_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	pr_notice("******** mt_batteryNotify_probe!! ********\n");

	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BatteryNotify);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_BN_TestMode);

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

	BUG_ON(pdev == NULL);

	return ret;
}

static int battery_pm_resume(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	BUG_ON(pdev == NULL);

	return ret;
}

static int battery_pm_freeze(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	BUG_ON(pdev == NULL);

	return ret;
}

static int battery_pm_restore(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	BUG_ON(pdev == NULL);

	return ret;
}

static int battery_pm_restore_noirq(struct device *device)
{
	int ret = 0;

	struct platform_device *pdev = to_platform_device(device);

	BUG_ON(pdev == NULL);

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
			    "****[battery_dts_probe] Unable to register device (%d)\n", ret);
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
			    "****[mt_batteryNotify_dts] Unable to register device (%d)\n", ret);
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

static int battery_pm_event(struct notifier_block *notifier, unsigned long pm_event, void *unused)
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
			    "****[battery_device] Unable to device register(%d)\n", ret);
		return ret;
	}
#endif
#endif

	ret = platform_driver_register(&battery_driver);
	if (ret) {
		pr_notice(
			    "****[battery_driver] Unable to register driver (%d)\n", ret);
		return ret;
	}
	/* battery notofy UI */
#ifdef CONFIG_OF
	/*  */
#else
	ret = platform_device_register(&MT_batteryNotify_device);
	if (ret) {
		pr_notice(
			    "****[mt_batteryNotify] Unable to device register(%d)\n", ret);
		return ret;
	}
#endif
	ret = platform_driver_register(&mt_batteryNotify_driver);
	if (ret) {
		pr_notice(
			    "****[mt_batteryNotify] Unable to register driver (%d)\n", ret);
		return ret;
	}
#ifdef CONFIG_OF
	ret = platform_driver_register(&battery_dts_driver);
	ret = platform_driver_register(&mt_batteryNotify_dts_driver);
#endif
	ret = register_pm_notifier(&battery_pm_notifier_block);
	if (ret)
		pr_debug("[%s] failed to register PM notifier %d\n", __func__, ret);

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
