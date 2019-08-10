/*
 * amz_priv.h
 *
 * Copyright 2017 Amazon Technologies, Inc. All Rights Reserved.
 *
 * The code contained herein is licensed under the GNU General Public
 * License Version 2. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#ifndef __AMZ_PRIV_H__
#define __AMZ_PRIV_H__

#define AMZ_PRIVACY_OF_NODE "amz_privacy"
#define PRIV_GPIO_USR_CNTL -1

enum priv_cb_dest {
	PRIV_CB_MIC = 0,
	PRIV_CB_CAM = 1,
	PRIV_CB_MAX = 2
};

struct priv_cb_data {
	int (*cb)(void *, int);
	void *cb_priv_data;
	uint cb_dest; /* one of enum priv_cb_dest */
};
int amz_priv_cb_reg(struct priv_cb_data *);

struct priv_work_data {
	struct delayed_work dwork;
	int cur_priv;
	int cur_timer_on;
	int priv_gpio;
	int mute_gpio;
	int public_hw_st_gpio;
	struct kobject *kobj;
	struct priv_cb_data *cb_arr[PRIV_CB_MAX];
};

void start_privacy_timer_func(struct work_struct *work);
#ifdef CONFIG_OF
int amz_priv_kickoff(struct device *dev);
#else
int amz_priv_kickoff(int gpio, struct device *dev);
#endif
struct workqueue_struct *amz_priv_get_workq(void);
int amz_priv_trigger(int on);
int amz_priv_timer_sysfs(int on);
int amz_set_public_hw_pin(int gpio);
int amz_set_mute_pin(int gpio);
int amz_priv_mute_cb(void *, int);

#define FLUSH_PRIV_WORKQ() do {\
		amz_priv_timer_sysfs(0);\
		cancel_delayed_work(&pw_data->dwork);\
		flush_workqueue(amz_priv_get_workq()); \
} while (0)
#endif
