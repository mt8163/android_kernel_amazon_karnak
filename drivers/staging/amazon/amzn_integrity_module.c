/*
 * Copyright (C) 2019 Amazon.com, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/of_fdt.h>
#include <linux/vmalloc.h>
#include <linux/power_supply.h>
#include <linux/metricslog.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/amzn_integrity_module.h>

bool batt_metrics_first_run = 1;
static unsigned long elaps_sec_start;
static unsigned long elaps_sec_prev;

/*
 * @Function       init_stress_metric
 * @Description    Initialize battery stress metric
 * @Input          batt_data     Pointer to integrity structure
 * @Return         none
 */
void init_stress_metric(struct integrity_metrics_data *batt_data)
{
	int i;

	batt_data->stress_frame = 0;
	batt_data->stress_frame_count = 0;
	for (i = 0; i < STRESS_STRING_BUFF_SIZE - 1; i++)
		batt_data->stress_string[i] = STRESS_STRING_IDENTIFIER;
}

/*
 * @Function       int_to_char
 * @Description    Convert integer to single ASCII character
 * @Input          num           number to convert
 * @Return         char          ASCII character
 */
char int_to_char(int num)
{
	if (num > 61)
		return 43;
	if (num < 0)
		return 45;
	if (num < 10)
		return (num + 48);
	if (num < 36)
		return (num + 55);
	return (num + 61);
}

/*
 * @Function       batt_volt_char
 * @Description    Convert battery voltage to single ASCII character
 * @Input          batt_mili_volts  battery voltage to convert
 * @Return         char             ASCII character
 */
char batt_volt_char(int batt_mili_volts)
{
	if (batt_mili_volts < STRESS_BATT_VOLT_LOWER_LIMIT)
		return int_to_char(STRESS_INT_TO_CHAR_LOWER_LIMIT);
	if (batt_mili_volts > STRESS_BATT_VOLT_UPPER_LIMIT)
		return int_to_char(STRESS_INT_TO_CHAR_UPPER_LIMIT);
	return int_to_char((batt_mili_volts - STRESS_BATT_VOLT_LOWER_LIMIT) / 30);
}

/*
 * @Function       batt_temp_char
 * @Description    Convert battery temperature to single ASCII character
 * @Input          batt_mili_volts  battery temperature to convert
 * @Return         char             ASCII character
 */
char batt_temp_char(int batt_temp_mili_c)
{
	if (batt_temp_mili_c < STRESS_BATT_TEMP_LOWER_LIMIT)
		return int_to_char(STRESS_INT_TO_CHAR_LOWER_LIMIT);
	if (batt_temp_mili_c > STRESS_BATT_TEMP_UPPER_LIMIT)
		return int_to_char(STRESS_INT_TO_CHAR_UPPER_LIMIT);
	return int_to_char((batt_temp_mili_c - STRESS_BATT_TEMP_LOWER_LIMIT) / 1500);
}

/*
 * @Function       calc_weighted_avg
 * @Description    Calculate and return weighted average value
 * @Input          avg_val          existing avg value
 * @Input          val              new value
 * @Input          elapsed          number of events elapsed for existing avg
 * @Input          delta            number of events elapsed at new value
 * @Return         int              new weighted avg value
 */
int calc_weighted_avg(int avg_val, int val, unsigned long elapsed,
		      unsigned long delta)
{
	int new_avg;

	new_avg = ((long)((elapsed * avg_val) + (delta * val)))
			/ ((long)(elapsed + delta));
	return new_avg;
}

/*
 * @Function       soc_corner_check
 * @Description    Report time spent at low and high battery SOC
 * @Input          batt_data     Pointer to integrity structure
 * @Input          soc           Battery state of charge in percent
 * @Input          batt_volt     Battery voltage in mV
 * @Input          elaps_sec     Monotonic elapsed seconds since device power on
 * @Input          send_metrics  Pointer to function that logs metric data
 * @Return         none
 */
void soc_corner_check(struct integrity_metrics_data *batt_data, int soc,
		      unsigned int batt_volt, unsigned long elaps_sec)
{
	unsigned long calc_above_95_time = 0;
	unsigned long calc_below_15_time = 0;
	char g_metrics_buf[BATTERY_METRICS_BUFF_SIZE] = {0};

	if (soc > 95 && !batt_data->batt_95_flag) {
		batt_data->batt_95_flag = 1;
		batt_data->above_95_time = elaps_sec;
	}

	if (soc < 95 && batt_data->batt_95_flag) {
		batt_data->batt_95_flag = 0;
		calc_above_95_time = elaps_sec - batt_data->above_95_time;
		if (calc_above_95_time > 0) {
			memset(g_metrics_buf, 0, BATTERY_METRICS_BUFF_SIZE);
			snprintf(g_metrics_buf, sizeof(g_metrics_buf),
				 SOC_CORNER_95_STRING, calc_above_95_time);
			log_to_metrics(ANDROID_LOG_INFO_LOCAL,
				       INTEGRITY_BATTERY_MODULE,
				       g_metrics_buf);
		}
	}

	if (soc > 0 && soc < 15 && !batt_data->batt_15_flag) {
		batt_data->batt_15_flag = 1;
		batt_data->below_15_time = elaps_sec;
		batt_data->low_batt_init_volt = batt_volt;
		batt_data->low_batt_init_soc = soc;
	}
	if (soc > 15 && batt_data->batt_15_flag) {
		batt_data->batt_15_flag = 0;
		batt_data->battery_below_15_fired = 0;
		calc_below_15_time = elaps_sec - batt_data->below_15_time;

		if (calc_below_15_time > 0) {
			memset(g_metrics_buf, 0, BATTERY_METRICS_BUFF_SIZE);
			snprintf(g_metrics_buf, sizeof(g_metrics_buf),
				 SOC_CORNER_15_1_STRING,
				 batt_data->low_batt_init_volt,
				 batt_data->low_batt_init_soc,
				 calc_below_15_time);
			log_to_metrics(ANDROID_LOG_INFO_LOCAL,
				       INTEGRITY_BATTERY_MODULE, g_metrics_buf);
		}
	}

	if ((soc < 5) && (soc > 0) && batt_data->battery_below_15_fired == 0) {
		batt_data->battery_below_15_fired = 1;
		calc_below_15_time = elaps_sec - batt_data->below_15_time;
		if (calc_below_15_time > 0) {
			memset(g_metrics_buf, 0, BATTERY_METRICS_BUFF_SIZE);
			snprintf(g_metrics_buf, sizeof(g_metrics_buf),
				 SOC_CORNER_15_2_STRING,
				 batt_data->low_batt_init_volt,
				 batt_data->low_batt_init_soc,
				 calc_below_15_time);
			log_to_metrics(ANDROID_LOG_INFO_LOCAL,
				       INTEGRITY_BATTERY_MODULE,
				       g_metrics_buf);
		}
	}
}

/*
 * @Function       stress_pulse
 * @Description    Calculate battery stress for the past reporting period
 * @Input          batt_data     Pointer to integrity structure
 * @Input          elaps_sec     Monotonic elapsed seconds since device power on
 * @Input          batt_volt_mv  Battery voltage in mV
 * @Input          batt_temp     Battery temperature in C
 * @Input          soc           Battery state of charge in percent
 * @Input          send_metrics  Pointer to function that logs metric data
 * @Return         none
 */
void stress_pulse(struct integrity_metrics_data *batt_data,
		  unsigned long elaps_sec, unsigned int batt_volt_mv,
		  int batt_temp, int soc)
{
	unsigned int stress_period;
	unsigned int stress_frame;
	char g_metrics_buf[BATTERY_METRICS_BUFF_SIZE] = {0};

	stress_period = elaps_sec / STRESS_REPORT_PERIOD;
	stress_frame = (elaps_sec -
			(stress_period * STRESS_REPORT_PERIOD)) / STRESS_PERIOD;

	if (batt_data->stress_frame != stress_frame) {
		if ((batt_data->stress_frame * 2 + 1) < STRESS_STRING_BUFF_SIZE) {
			batt_data->stress_string[batt_data->stress_frame * 2]
			= batt_temp_char(batt_data->batt_stress_temp_mili_c);
			batt_data->stress_string[batt_data->stress_frame * 2 + 1]
			= batt_volt_char(batt_data->batt_stress_volts_mv);
		}
		batt_data->stress_frame_count = 1;
		batt_data->stress_frame = stress_frame;

		batt_data->batt_stress_temp_mili_c = (batt_temp * 1000);
		batt_data->batt_stress_volts_mv = batt_volt_mv;

#ifdef INTEGRITY_MODULE_DEBUG
		memset(g_metrics_buf, 0, BATTERY_METRICS_BUFF_SIZE);
		snprintf(g_metrics_buf, sizeof(g_metrics_buf),
			 STRESS_PULSE_DEBUG_STRING,
		stress_frame, stress_period, batt_volt_mv,
		batt_temp, batt_data->stress_string, INTEGRITY_MODULE_VERSION);
		log_to_metrics(ANDROID_LOG_INFO_LOCAL,
			       INTEGRITY_BATTERY_MODULE, g_metrics_buf);
#endif

	} else {
		batt_data->batt_stress_temp_mili_c =
		calc_weighted_avg(batt_data->batt_stress_temp_mili_c,
				  (batt_temp * 1000),
				  batt_data->stress_frame_count, 1);
		batt_data->batt_stress_volts_mv =
		calc_weighted_avg(batt_data->batt_stress_volts_mv,
				  batt_volt_mv,
				  batt_data->stress_frame_count, 1);
		batt_data->stress_frame_count++;
	}

	if (((soc < 5) && (soc > 0) && (batt_data->stress_frame > 4) && (batt_data->stress_below_5_fired == 0))
		|| batt_data->stress_period != stress_period) {
		memset(g_metrics_buf, 0, BATTERY_METRICS_BUFF_SIZE);
		snprintf(g_metrics_buf, sizeof(g_metrics_buf),
			 STRESS_REPORT_STRING, stress_period,
			 batt_data->stress_string, INTEGRITY_MODULE_VERSION);
		log_to_metrics(ANDROID_LOG_INFO_LOCAL,
			       INTEGRITY_BATTERY_MODULE, g_metrics_buf);

		batt_data->stress_period = stress_period;
		init_stress_metric(batt_data);
		if (batt_data->stress_below_5_fired == 0)
			batt_data->stress_frame = stress_frame;
		batt_data->stress_below_5_fired = 1;
	}
	if (soc >= 5)
		batt_data->stress_below_5_fired = 0;
}

/*
 * @Function      init_integrity_batt_data
 * @Description   Initialize integrity battery data structure at device power on
 * @Input         batt_data     Pointer to integrity structure
 * @Input         batt_volt     Battery voltage in mV
 * @Input         vusb          USB charger voltage in mV
 * @Input         soc           Battery state of charge in percent
 * @Input         temp          Battery temperature in C
 * @Input         temp_virtual  Virtual Sensor temperature in C
 * @Input         chg_sts       Charger status
 * @Input         elaps_sec     Monotonic elapsed seconds since device power on
 * @Return        none
 */
void init_integrity_batt_data(struct integrity_metrics_data *batt_data,
			      unsigned int batt_volt, unsigned int vusb,
			      int soc, int temp, int temp_virtual,
			      unsigned int chg_sts, unsigned long elaps_sec)
{
	batt_data->batt_volt_avg = 0;
	batt_data->batt_volt_init = batt_volt;
	batt_data->batt_volt_final = batt_volt;

	batt_data->usb_volt_avg = 0;
	batt_data->usb_volt = vusb;
	batt_data->usb_volt_peak = vusb;
	batt_data->usb_volt_min = VUSB_CEILING;
	batt_data->chg_sts = chg_sts;
	batt_data->soc = soc;
	batt_data->fsoc = soc;

	batt_data->batt_temp_peak = temp;
	batt_data->batt_temp_virtual_peak = temp_virtual;
	batt_data->batt_temp_avg = temp * 1000;
	batt_data->batt_temp_avg_virtual = temp_virtual * 1000;

	elaps_sec_start = elaps_sec_prev;
}

/*
 * @Function       push_integrity_batt_data
 * @Description    Push function from kernel thread which provides data to
		   the integrity module
 * @Input          elaps_sec     Monotonic elapsed seconds since device power on
 * @Input          batt_volt     Battery voltage in mV
 * @Input          vusb          USB charger voltage in mV
 * @Input          temp          Battery temperature in C
 * @Input          temp_virtual  Virtual Sensor temperature in C
 * @Input          soc           Battery state of charge in percent
 * @Input          cycle_count   Battery cycle count
 * @Input          chg_sts       Charger status
 * @Input          batt_data     Pointer to integrity structure
 * @Input          send_metrics  Pointer to function that logs metric data
 * @Return         none
 */
void push_integrity_batt_data(unsigned long elaps_sec, unsigned int batt_volt,
			      unsigned int vusb, int temp, int virtual_temp,
			      int soc, unsigned int cycle_count,
			      unsigned int chg_sts,
			      struct integrity_metrics_data *batt_data)
{
	unsigned long delta_elaps_sec;
	unsigned long calc_elaps_sec = 0;
	char g_metrics_buf[BATTERY_METRICS_BUFF_SIZE] = {0};

	if (batt_metrics_first_run) {
		batt_metrics_first_run = 0;
		batt_data->n_count = 1;
		batt_data->elaps_sec = 0;
		elaps_sec_prev = elaps_sec;
		elaps_sec_start = elaps_sec;
		batt_data->stress_period = 0;
		init_integrity_batt_data(batt_data, batt_volt, vusb, soc, temp,
					 virtual_temp, chg_sts, elaps_sec);
		init_stress_metric(batt_data);
	}

	if (batt_data->soc == 0)
		batt_data->soc = soc;

	stress_pulse(batt_data, elaps_sec, batt_volt, temp, soc);
	soc_corner_check(batt_data, soc, batt_volt, elaps_sec);

	delta_elaps_sec = elaps_sec - elaps_sec_prev;

	if ((chg_sts != batt_data->chg_sts)
		|| (batt_data->elaps_sec > MAX_REPORTING_PERIOD)) {
		if (batt_data->elaps_sec > 30) {
			memset(g_metrics_buf, 0, BATTERY_METRICS_BUFF_SIZE);
			snprintf(g_metrics_buf, sizeof(g_metrics_buf),
				CHARGE_STATE_REPORT_STRING, batt_data->chg_sts,
				batt_data->elaps_sec, batt_data->batt_volt_init,
				batt_data->batt_volt_final, batt_volt,
				batt_data->soc,
				(batt_data->batt_temp_avg / 1000),
				(batt_data->batt_temp_avg_virtual / 1000),
				batt_data->batt_temp_peak,
				batt_data->batt_temp_virtual_peak,
				temp, cycle_count,
				batt_data->usb_volt_peak,
				batt_data->usb_volt,
				batt_data->usb_volt_min,
				(int)(batt_data->usb_volt_avg / batt_data->elaps_sec),
				(int)(batt_data->batt_volt_avg / batt_data->elaps_sec),
				batt_data->fsoc,
				batt_data->n_count,
				INTEGRITY_MODULE_VERSION);

			log_to_metrics(ANDROID_LOG_INFO_LOCAL,
					INTEGRITY_BATTERY_MODULE,
					g_metrics_buf);
		}
		init_integrity_batt_data(batt_data, batt_volt,
					 vusb, soc, temp,
					 virtual_temp, chg_sts,
					 elaps_sec);
		batt_data->n_count++;
	}

	calc_elaps_sec = elaps_sec - elaps_sec_start;
	if (calc_elaps_sec == 0)
		return;

	batt_data->chg_sts = chg_sts;

	batt_data->batt_volt_avg += (batt_volt * delta_elaps_sec);

	batt_data->usb_volt = vusb;
	batt_data->usb_volt_avg += (vusb * delta_elaps_sec);
	if (vusb > batt_data->usb_volt_peak)
		batt_data->usb_volt_peak = vusb;
	if (vusb < batt_data->usb_volt_min)
		batt_data->usb_volt_min = vusb;

	batt_data->batt_temp_avg =
	(int)calc_weighted_avg(batt_data->batt_temp_avg,
		(temp * 1000), (calc_elaps_sec - delta_elaps_sec), delta_elaps_sec);
	if (temp > batt_data->batt_temp_peak)
		batt_data->batt_temp_peak = temp;

	batt_data->batt_temp_avg_virtual =
	(int)calc_weighted_avg(batt_data->batt_temp_avg_virtual,
		(virtual_temp * 1000), (calc_elaps_sec - delta_elaps_sec), delta_elaps_sec);
	if (virtual_temp > batt_data->batt_temp_virtual_peak)
		batt_data->batt_temp_virtual_peak = virtual_temp;

	batt_data->elaps_sec = calc_elaps_sec;

#ifdef INTEGRITY_MODULE_DEBUG
	memset(g_metrics_buf, 0, BATTERY_METRICS_BUFF_SIZE);
	snprintf(g_metrics_buf, sizeof(g_metrics_buf),
		 CHARGE_STATE_DEBUG_STRING,
		 batt_data->chg_sts,
		 batt_data->batt_volt_avg,
		 batt_data->usb_volt_avg,
		 batt_data->usb_volt_min,
		 batt_data->n_count,
		 batt_data->elaps_sec,
		 virtual_temp,
		 elaps_sec,
		 elaps_sec_start,
		 elaps_sec_prev,
		 delta_elaps_sec,
		 calc_elaps_sec);

	log_to_metrics(ANDROID_LOG_INFO_LOCAL, INTEGRITY_BATTERY_MODULE,
		       g_metrics_buf);
#endif

	elaps_sec_prev = elaps_sec;
	batt_data->batt_volt_final = batt_volt;
	batt_data->fsoc = soc;
}

static int get_battery_property(struct integrity_driver_data *data, int type)
{
	union power_supply_propval val;
	struct power_supply *psy = NULL;
	int property = 0;
	int ret = 0;

	if (!data->batt_psy)
		return -1;

	psy = data->batt_psy;
	switch (type) {
	case TYPE_SOC:
		property = POWER_SUPPLY_PROP_CAPACITY;
		break;
	case TYPE_VBAT:
		property = POWER_SUPPLY_PROP_InstatVolt;
		break;
	case TYPE_TBAT:
		property = POWER_SUPPLY_PROP_TEMP;
		break;
	case TYPE_CHG_STATE:
		property = POWER_SUPPLY_PROP_STATUS;
		break;
	case TYPE_VBUS:
		property = POWER_SUPPLY_PROP_ChargerVoltage;
		break;
	default:
		pr_err("%s: not support type: %d\n", __func__, type);
		return -1;
	}

	ret = psy->get_property(psy, property, &val);
	if (ret) {
		pr_err("%s: can't find property[%d]\n", __func__, property);
		return -1;
	}

	return val.intval;
}

static void integrity_module_routine(struct work_struct *work)
{
	struct integrity_driver_data *data = container_of(work,
				struct integrity_driver_data, dwork.work);
	struct timespec diff, current_kernel_time;
	int soc, vbus, vbat, temp, cycle, temp_virtual, chg_sts;
	unsigned long elaps_sec;

	soc = get_battery_property(data, TYPE_SOC);
	vbus = get_battery_property(data, TYPE_VBUS);
	vbat = get_battery_property(data, TYPE_VBAT);
	temp = get_battery_property(data, TYPE_TBAT) / 10;
	cycle = gFG_battery_cycle;
	chg_sts = get_battery_property(data, TYPE_CHG_STATE);
	if (soc < 0 || vbus < 0 || vbat < 0 || cycle < 0 || chg_sts < 0)
		goto skip;

	temp_virtual = get_virtualsensor_temp();
	get_monotonic_boottime(&current_kernel_time);
	diff = timespec_sub(current_kernel_time, data->init_time);
	elaps_sec = diff.tv_sec;
	pr_debug("integrity: soc[%d] vbus[%d] vbat[%d] tbat[%d] cycle[%d] chg_sts[%d] t_v[%d] elaps[%lu]\n",
		 soc, vbus, vbat, temp, cycle,
		 chg_sts, temp_virtual, elaps_sec);
	push_integrity_batt_data(elaps_sec, vbat, vbus, temp,
				 temp_virtual, soc, cycle,
				 chg_sts, &data->metrics);

skip:
	if (data->is_suspend)
		pr_err("%s: skip next schedule\n", __func__);
	else
		schedule_delayed_work(&data->dwork,
				      msecs_to_jiffies(THREAD_SLEEP_TIME_MSEC));
}

static int integrity_module_pm_event(struct notifier_block *notifier,
				     unsigned long pm_event, void *unused)
{
	struct integrity_driver_data *data = container_of(notifier,
				struct integrity_driver_data, notifier);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		pr_debug("[%s] cancel routine work\n", __func__);
		data->is_suspend = true;
		cancel_delayed_work_sync(&data->dwork);
		break;

	case PM_POST_SUSPEND:
		pr_debug("[%s] restore routine work\n", __func__);
		data->is_suspend = false;
		schedule_delayed_work(&data->dwork,
				      msecs_to_jiffies(THREAD_SLEEP_TIME_MSEC));
		break;
	}

	return NOTIFY_OK;
}

static int integrity_module_probe(struct platform_device *pdev)
{
	struct integrity_driver_data *data = NULL;
	int ret = -ENOMEM;

	data = devm_kzalloc(&pdev->dev, sizeof(struct integrity_driver_data),
			    GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto out;
	}

	data->batt_psy = power_supply_get_by_name("battery");
	if (!data->batt_psy) {
		ret = -EPROBE_DEFER;
		goto out_mem;
	}

	data->notifier.notifier_call = integrity_module_pm_event;
	ret = register_pm_notifier(&data->notifier);
	if (ret) {
		pr_err("%s: failed to register PM notifier\n", __func__);
		goto out_mem;
	}

	platform_set_drvdata(pdev, data);
	data->pdev = pdev;

	get_monotonic_boottime(&data->init_time);
	INIT_DELAYED_WORK(&data->dwork, integrity_module_routine);
	schedule_delayed_work(&data->dwork,
			      msecs_to_jiffies(THREAD_SLEEP_TIME_MSEC));
	pr_info("%s: success\n", __func__);
	return 0;

out_mem:
	if (data)
		devm_kfree(&pdev->dev, data);
out:
	return ret;
}

static int integrity_module_remove(struct platform_device *pdev)
{
	return 0;
}

static void integrity_module_shutdown(struct platform_device *pdev)
{
	struct integrity_driver_data *data = platform_get_drvdata(pdev);

	unregister_pm_notifier(&data->notifier);
	cancel_delayed_work(&data->dwork);
}

static const struct of_device_id integrity_module_dt_match[] = {
	{.compatible = "amzn,integrity-module",},
	{},
};
MODULE_DEVICE_TABLE(of, integrity_module_dt_match);

struct platform_driver integrity_module_driver = {
	.probe	= integrity_module_probe,
	.remove = integrity_module_remove,
	.shutdown = integrity_module_shutdown,
	.driver = {
		.name = "integrity_module",
		.owner = THIS_MODULE,
		.of_match_table = integrity_module_dt_match,
	},
};

static int __init integrity_module_init(void)
{
	return platform_driver_register(&integrity_module_driver);
}
late_initcall(integrity_module_init);

static void __exit integrity_module_exit(void)
{
	platform_driver_unregister(&integrity_module_driver);
}
module_exit(integrity_module_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Integrity Module Driver");
MODULE_AUTHOR("Amazon");
