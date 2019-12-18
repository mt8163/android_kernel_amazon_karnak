#ifndef _INTEGRITY_MODULE_H
#define _INTEGRITY_MODULE_H

#define INTEGRITY_PLATFORM_LINUX 1

#define MAX_REPORTING_PERIOD                  (3600*24)
#define THREAD_SLEEP_TIME_MSEC                    10000
#define INTEGRITY_MODULE_VERSION                     10
#define BATTERY_METRICS_BUFF_SIZE                   512
#define ANDROID_LOG_INFO_LOCAL                        4
#define STRESS_PERIOD                               450
#define STRESS_REPORT_PERIOD                      28800
#define VUSB_CEILING                              20000
#define STRESS_STRING_IDENTIFIER                     '_'
#define STRESS_BATT_VOLT_LOWER_LIMIT               2700
#define STRESS_BATT_VOLT_UPPER_LIMIT               4550
#define STRESS_BATT_TEMP_LOWER_LIMIT            (-5000)
#define STRESS_BATT_TEMP_UPPER_LIMIT              88000
#define STRESS_INT_TO_CHAR_LOWER_LIMIT             (-1)
#define STRESS_INT_TO_CHAR_UPPER_LIMIT               63
#define STRESS_STRING_BUFF_SIZE    ((2*STRESS_REPORT_PERIOD/STRESS_PERIOD) + 1)

#define INTEGRITY_BATTERY_MODULE   "integrity_batt"
#define STRESS_PULSE_DEBUG_STRING  "integrity_batt:def:PULSEDEBUG=1;CT;1,stressFrame=%d;CT;1,stressPeriod=%d;CT;1,Vol=%d;CT;1,temp=%d;CT;1,stress=%s;DV;1,iVer=%d;CT;1:NA"
#define STRESS_REPORT_STRING       "integrity_batt:def:BattStress=1;CT;1,stress_period=%d;CT;1,stress=%s;DV;1,iVer=%d;CT;1:NA"
#define CHARGE_STATE_REPORT_STRING "integrity_batt:def:UNLOAD=1;CT;1,Charger_status=%d;CT;1,Elaps_Sec=%ld;CT;1,iVol=%d;CT;1,fVol=%d;CT;1,lVol=%d;CT;1,iSOC=%d;CT;1,Bat_aTemp=%d;CT;1,Vir_aTemp=%d;CT;1,Bat_pTemp=%d;CT;1,Vir_pTemp=%d;CT;1,bTemp=%d;CT;1,Cycles=%d;CT;1,pVUsb=%d;CT;1,fVUsb=%d;CT;1,mVUsb=%d;CT;1,aVUsb=%d;CT;1,aVol=%d;CT;1,fSOC=%d;CT;1,ct=%lu;CT;1,iVer=%d;CT;1:NA"
#define CHARGE_STATE_DEBUG_STRING  "integrity_batt:def:ThreadLocal=1;CT;1,Charger_status=%d;CT;1,AvgVbatt=%d;CT;1,AvgVUSB=%d;CT;1,mVUsb=%d;CT;1,n_count=%d;CT;1,Elaps_Sec=%ld;CT;1,Bat_vTemp=%d;CT;1,elaps_sec=%ld;CT;1,elaps_sec_start=%ld;CT;1,elaps_sec_prev=%ld;CT;1,delta_elaps_sec=%ld;CT;1,calc_elaps_sec=%ld;CT;1:NA"
#define SOC_CORNER_95_STRING       "integrity_batt:def:time_soc95=1;CT;1,Elaps_Sec=%ld;CT;1:NA"
#define SOC_CORNER_15_1_STRING     "integrity_batt:def:time_soc15_soc20=1;CT;1,Init_Vol=%d;CT;1,Init_SOC=%d;CT;1,Elaps_Sec=%ld;CT;1:NA"
#define SOC_CORNER_15_2_STRING     "integrity_batt:def:time_soc15_soc0=1;CT;1,Init_Vol=%d;CT;1,Init_SOC=%d;CT;1,Elaps_Sec=%ld;CT;1:NA"


enum battery_metrics_info {
	TYPE_VBAT = 0,
	TYPE_VBUS,
	TYPE_TBAT,
	TYPE_SOC,
	TYPE_BAT_CYCLE,
	TYPE_CHG_STATE,
	TYPE_MAX,
};


struct integrity_metrics_data {

	unsigned int chg_sts;

	unsigned int batt_volt_init;
	unsigned int batt_volt_final;
	unsigned int batt_volt_avg;
	unsigned int usb_volt;
	unsigned int usb_volt_avg;
	unsigned int usb_volt_peak;
	unsigned int usb_volt_min;

	int soc;
	int fsoc;

	int batt_temp_peak;
	int batt_temp_virtual_peak;
	int batt_temp_avg;
	int batt_temp_avg_virtual;

	unsigned long n_count;
	unsigned long elaps_sec;
	unsigned long above_95_time;
	unsigned long below_15_time;
	bool batt_95_flag;
	bool batt_15_flag;
	bool battery_below_15_fired;
	unsigned int low_batt_init_volt;
	int low_batt_init_soc;

	unsigned int stress_frame;
	unsigned int stress_period;
	unsigned int stress_frame_count;
	char stress_string[STRESS_STRING_BUFF_SIZE];
	int batt_stress_temp_mili_c;
	unsigned int batt_stress_volts_mv;
	bool stress_below_5_fired;
};

struct integrity_driver_data {
	struct device *dev;
	struct platform_device *pdev;
	struct power_supply *batt_psy;
	struct timespec init_time;
	struct notifier_block notifier;
	struct delayed_work dwork;
	struct integrity_metrics_data metrics;
	bool is_suspend;
};

extern unsigned long get_virtualsensor_temp(void);
extern signed int gFG_battery_cycle;
#endif
