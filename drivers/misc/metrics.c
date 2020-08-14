/*
 * metrics.c
 *
 * Copyright 2017-2019 Amazon Technologies, Inc. All Rights Reserved
 *
 * The code contained herein is licensed under the GNU General Public
 * License Version 2. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/of.h>


#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#endif

#ifdef CONFIG_AMAZON_METRICS_LOG
#define METRICS_STR_LEN		128

#if CONFIG_IDME
#define IDME_OF_MFG_LOCALE	"/idme/mfg.locale"
#endif

static int powersave_flag;

static int log_metric(void)
{
	char buf[METRICS_STR_LEN];
	char *mfg_locale_idme = NULL;
	struct device_node *ap = NULL;

	if (powersave_flag) {
		snprintf(buf, METRICS_STR_LEN,
			 "Power_save_mode_metric:mode=lowpower;CT;1:NR");
		log_to_metrics(ANDROID_LOG_INFO, "PowerSaveMode", buf);
	}
	powersave_flag = 0;

#if CONFIG_IDME
	ap = of_find_node_by_path(IDME_OF_MFG_LOCALE);
	if (ap)
		mfg_locale_idme = (char *)of_get_property(ap, "value", NULL);
	else {
		pr_err("of_find_node_by_path for mfg.locale failed\n");
		return -1;
	}
#endif

	if (mfg_locale_idme != NULL) {
		snprintf(buf, METRICS_STR_LEN,
			"mfg_locale_metric:locale=%s;CT;1:NR", mfg_locale_idme);
		log_to_metrics(ANDROID_LOG_INFO, "mfglocale", buf);
	}

	return 0;
}
late_initcall(log_metric);
#endif

static int __init check_device_coming_out_of_powersave(char *str)
{
	get_option(&str, &powersave_flag);

	return 0;
}

__setup("powersave_metric=", check_device_coming_out_of_powersave);
