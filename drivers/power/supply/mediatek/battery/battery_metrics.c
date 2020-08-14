#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/metricslog.h>
#include <linux/power_supply.h>
#include <mt-plat/charging.h>
#include <mt-plat/battery_common.h>
#include <mt-plat/battery_meter.h>
#ifdef CONFIG_AMAZON_SIGN_OF_LIFE
#include <linux/sign_of_life.h>
#endif
#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif


#define BATTERY_METRICS_BUFF_SIZE 512
char g_metrics_buf[BATTERY_METRICS_BUFF_SIZE];

#define bat_metrics_log(domain, fmt, ...)				\
do {									\
	memset(g_metrics_buf, 0 , BATTERY_METRICS_BUFF_SIZE);		\
	snprintf(g_metrics_buf, sizeof(g_metrics_buf),	fmt, ##__VA_ARGS__);\
	log_to_metrics(ANDROID_LOG_INFO, domain, g_metrics_buf);	\
} while (0)

struct screen_state {
	struct timespec screen_on_time;
	struct timespec screen_off_time;
	int screen_on_soc;
	int screen_off_soc;
};

struct pm_state {
	struct timespec suspend_ts;
	struct timespec resume_ts;
	int suspend_soc;
	int resume_soc;
	int suspend_bat_car;
	int resume_bat_car;
};

struct fg_slp_current_data {
	struct timespec last_ts;
	int slp_ma;
};

struct bat_metrics_data {
	bool is_top_off_mode;
	bool is_demo_mode;
	bool vbus_on_old;
	u8 fault_type_old;
	u32 chg_sts_old;
	u32 chg_type_old;

	struct screen_state screen;
	struct pm_state pm;
	struct battery_meter_data bat_data;
	struct fg_slp_current_data slp_curr_data;
#if defined(CONFIG_FB)
	struct notifier_block pm_notifier;
#endif
};
static struct bat_metrics_data metrics_data;

int bat_metrics_slp_current(u32 ma)
{
	struct fg_slp_current_data *slp = &metrics_data.slp_curr_data;
	struct timespec now_ts, gap_ts;
	long elapsed;

	get_monotonic_boottime(&now_ts);
	if (slp->slp_ma == 0)
		goto exit;

	gap_ts = timespec_sub(now_ts, slp->last_ts);
	elapsed = gap_ts.tv_sec;
	bat_metrics_log("battery",
			"FGSleepCurrent:def:Slp_mA_%d=1;CT;1,elapsed=%ld;TI;1:NR",
			ma, elapsed);

exit:
	slp->slp_ma = ma;
	slp->last_ts = now_ts;
	return 0;
}

int bat_metrics_aicl(bool is_detected, u32 aicl_result)
{

	bat_metrics_log("AICL",
		"%s:aicl:detected=%d;CT;1,aicl_result=%d;CT;1:NR",
		__func__, is_detected, aicl_result);

	return 0;
}

int bat_metrics_vbus(bool is_on)
{
	if (metrics_data.vbus_on_old == is_on)
		return 0;

	metrics_data.vbus_on_old = is_on;
	bat_metrics_log("USBCableEvent",
		"bq24297:vbus_%s=1;CT;1:NR", is_on ? "on" : "off");

	return 0;
}

int bat_metrics_chrdet(u32 chr_type)
{
	static const char * const charger_type_text[] = {
		"UNKNOWN", "STANDARD_HOST", "CHARGING_HOST",
		"NONSTANDARD_CHARGER", "STANDARD_CHARGER", "APPLE_2_1A_CHARGER",
		"APPLE_1_0A_CHARGER", "APPLE_0_5A_CHARGER", "WIRELESS_CHARGER"
	};

	if (metrics_data.chg_type_old == chr_type)
		return 0;

	metrics_data.chg_type_old = chr_type;
	if (chr_type > CHARGER_UNKNOWN && chr_type <= WIRELESS_CHARGER) {
		bat_metrics_log("USBCableEvent",
			"%s:bq24297:chg_type_%s=1;CT;1:NR",
			__func__, charger_type_text[chr_type]);
	}

	return 0;
}

int bat_metrics_chg_fault(u8 fault_type)
{
	if (metrics_data.fault_type_old == fault_type)
		return 0;

	metrics_data.fault_type_old = fault_type;
	if (fault_type != 0)
		bat_metrics_log("battery",
			"bq24297:def:charger_fault_type=%u;CT;1:NA",
			fault_type);

	return 0;
}

int bat_metrics_chg_state(u32 chg_sts)
{
	int soc = BMT_status.UI_SOC;
	int vbat = BMT_status.bat_vol;
	int ibat = BMT_status.ICharging;

	if (metrics_data.chg_sts_old == chg_sts)
		return 0;

	metrics_data.chg_sts_old = chg_sts;
	bat_metrics_log("battery",
		"bq24297:def:POWER_STATUS_%s=1;CT;1,cap=%u;CT;1,mv=%d;CT;1," \
		"current_avg=%d;CT;1:NR",
		(chg_sts == POWER_SUPPLY_STATUS_CHARGING) ?
		"CHARGING" : "DISCHARGING", soc, vbat, ibat);

	return 0;
}

int bat_metrics_critical_shutdown(void)
{
	static bool written;

	if (BMT_status.charger_exist)
		return 0;

	if (BMT_status.bat_exist != true)
		return 0;

	if (BMT_status.UI_SOC != 0)
		return 0;

	if (written == false &&
		BMT_status.bat_vol <= batt_cust_data.system_off_voltage) {
		written = true;
		bat_metrics_log("battery",
			"bq24297:def:critical_shutdown=1;CT;1:HI");
#ifdef CONFIG_AMAZON_SIGN_OF_LIFE
		life_cycle_set_special_mode(LIFE_CYCLE_SMODE_LOW_BATTERY);
#endif
	}

	return 0;
}

extern unsigned long get_virtualsensor_temp(void);
int bat_metrics_top_off_mode(bool is_on, long total_time_plug_in)
{
	struct battery_meter_data *bat_data = &metrics_data.bat_data;
	unsigned long virtual_temp = get_virtualsensor_temp();

	if (metrics_data.is_top_off_mode == is_on)
		return 0;

	battery_meter_get_data(bat_data);
	metrics_data.is_top_off_mode = is_on;
	if (is_on) {
		bat_metrics_log("battery",
		"bq24297:def:Charging_Over_14days=1;CT;1," \
		"Total_Plug_Time=%ld;CT;1,Bat_Vol=%d;CT;1,UI_SOC=%d;CT;1," \
		"SOC=%d;CT;1,Bat_Temp=%d;CT;1,Vir_Avg_Temp=%ld;CT;1," \
		"Bat_Cycle_count=%d;CT;1:NA",
		total_time_plug_in, BMT_status.bat_vol, BMT_status.UI_SOC,
		BMT_status.SOC, BMT_status.temperature, virtual_temp,
		bat_data->battery_cycle);
	}

	return 0;
}

int bat_metrics_demo_mode(bool is_on, long total_time_plug_in)
{
	unsigned long virtual_temp = get_virtualsensor_temp();
	struct battery_meter_data *bat_data = &metrics_data.bat_data;

	if (metrics_data.is_demo_mode == is_on)
		return 0;

	battery_meter_get_data(bat_data);
	metrics_data.is_demo_mode = is_on;
	if (is_on) {
		bat_metrics_log("battery",
		"bq24297:def:Store_Demo_Mode=1;CT;1,Total_Plug_Time=%ld;CT;1," \
		"Bat_Vol=%d;CT;1,UI_SOC=%d;CT;1,SOC=%d;CT;1,Bat_Temp=%d;CT;1," \
		"Vir_Avg_Temp=%ld;CT;1,Bat_Cycle_Count=%d;CT;1:NA",
		total_time_plug_in, BMT_status.bat_vol, BMT_status.UI_SOC,
		BMT_status.SOC, BMT_status.temperature, virtual_temp,
		bat_data->battery_cycle);
	}

	return 0;
}

#define SUSPEND_RESUME_INTEVAL_MIN 1
int bat_metrics_suspend(void)
{
	struct pm_state *pm = &metrics_data.pm;

	get_monotonic_boottime(&pm->suspend_ts);
	pm->suspend_soc = BMT_status.UI_SOC;
	pm->suspend_bat_car = battery_meter_get_car();

	return 0;
}

int bat_metrics_resume(void)
{
	struct battery_meter_data *bat_data = &metrics_data.bat_data;
	struct pm_state *pm = &metrics_data.pm;
	struct timespec resume_ts;
	struct timespec sleep_ts;
	int soc, diff_soc, resume_bat_car, diff_bat_car;
	long elaps_msec;

	soc = BMT_status.UI_SOC;
	get_monotonic_boottime(&resume_ts);
	resume_bat_car = battery_meter_get_car();
	if (pm->resume_soc == -1 || pm->suspend_soc == -1)
		goto exit;

	diff_soc = pm->suspend_soc - soc;
	diff_bat_car = pm->suspend_bat_car - resume_bat_car;
	sleep_ts = timespec_sub(resume_ts, pm->suspend_ts);
	elaps_msec = sleep_ts.tv_sec * 1000 + sleep_ts.tv_nsec / NSEC_PER_MSEC;
	pr_debug("%s: diff_soc: %d sleep_time(s): %ld [%ld - %ld]\n",
			__func__, diff_soc, sleep_ts.tv_sec,
			resume_ts.tv_sec, pm->suspend_ts.tv_sec);

	if (sleep_ts.tv_sec > SUSPEND_RESUME_INTEVAL_MIN) {
		pr_debug("%s: sleep_ts: %ld diff_soc: %d, diff_bat_car: %d\n",
			__func__, sleep_ts.tv_sec, diff_soc, diff_bat_car);
		bat_metrics_log("drain_metrics",
			"suspend_drain:def:value=%d;CT;1,elapsed=%ld;TI;1:NR",
			diff_soc, elaps_msec);
	}

	battery_meter_get_data(bat_data);
	bat_metrics_log("bq24297",
		"batt:def:cap=%d;CT;1,mv=%d;CT;1,current_avg=%d;CT;1," \
		"temp_g=%d;CT;1,charge=%d;CT;1,charge_design=%d;CT;1," \
		"aging_factor=%d;CT;1,battery_cycle=%d;CT;1,"
		"columb_sum=%d;CT;1:NR",
		BMT_status.UI_SOC, BMT_status.bat_vol,
		BMT_status.ICharging, BMT_status.temperature,
		bat_data->batt_capacity_aging, /*battery_remaining_charge,?*/
		bat_data->batt_capacity, /*battery_remaining_charge_design*/
		bat_data->aging_factor, /* aging factor */
		bat_data->battery_cycle,
		bat_data->columb_sum);
exit:
	pm->resume_bat_car = resume_bat_car;
	pm->resume_ts = resume_ts;
	pm->resume_soc = soc;
	return 0;
}

#if defined(CONFIG_FB)
static int bat_metrics_screen_on(void)
{
	struct timespec screen_on_time;
	struct timespec diff;
	struct screen_state *screen = &metrics_data.screen;
	long elaps_msec;
	int soc = BMT_status.UI_SOC, diff_soc;

	get_monotonic_boottime(&screen_on_time);
	if (screen->screen_on_soc == -1 || screen->screen_off_soc == -1)
		goto exit;

	diff_soc = screen->screen_off_soc - soc;
	diff = timespec_sub(screen_on_time, screen->screen_off_time);
	elaps_msec = diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC;
	bat_metrics_log("drain_metrics",
		"screen_off_drain:def:value=%d;CT;1,elapsed=%ld;TI;1:NR",
		diff_soc, elaps_msec);

exit:
	screen->screen_on_time = screen_on_time;
	screen->screen_on_soc = soc;
	return 0;
}

static int bat_metrics_screen_off(void)
{
	struct timespec screen_off_time;
	struct timespec diff;
	struct screen_state *screen = &metrics_data.screen;
	long elaps_msec;
	int soc = BMT_status.UI_SOC, diff_soc;

	get_monotonic_boottime(&screen_off_time);
	if (screen->screen_on_soc == -1 || screen->screen_off_soc == -1)
		goto exit;

	diff_soc = screen->screen_on_soc - soc;
	diff = timespec_sub(screen_off_time, screen->screen_on_time);
	elaps_msec = diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC;
	bat_metrics_log("drain_metrics",
		"screen_on_drain:def:value=%d;CT;1,elapsed=%ld;TI;1:NR",
		diff_soc, elaps_msec);

exit:
	screen->screen_off_time = screen_off_time;
	screen->screen_off_soc = soc;
	return 0;
}

static int pm_notifier_callback(struct notifier_block *notify,
			unsigned long event, void *data)
{
	struct fb_event *ev_data = data;
	int *blank;

	if (ev_data && ev_data->data && event == FB_EVENT_BLANK) {
		blank = ev_data->data;
		if (*blank == FB_BLANK_UNBLANK)
			bat_metrics_screen_on();

		else if (*blank == FB_BLANK_POWERDOWN)
			bat_metrics_screen_off();
	}

	return 0;
}
#endif

int bat_metrics_init(void)
{
	int ret = 0;

	metrics_data.screen.screen_off_soc = -1;
	metrics_data.screen.screen_off_soc = -1;
	metrics_data.pm.suspend_soc = -1;
	metrics_data.pm.resume_soc = -1;
#if defined(CONFIG_FB)
	metrics_data.pm_notifier.notifier_call = pm_notifier_callback;
	ret= fb_register_client(&metrics_data.pm_notifier);
	if (ret)
		pr_err("%s: fail to register pm notifier\n", __func__);
#endif

	return 0;
}

void bat_metrics_uninit(void)
{
#if defined(CONFIG_FB)
	fb_unregister_client(&metrics_data.pm_notifier);
#endif
}
