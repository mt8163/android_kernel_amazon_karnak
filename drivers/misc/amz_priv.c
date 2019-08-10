/*
 * amz_priv.c
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mediatek/include/mt-plat/mt_boot_common.h>
#include <amz_priv.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#define DRIVER_NAME "amz_privacy"

static bool priv_dis;
static u32 hw_latch_set;

struct priv_work_data *pw_data;
#define GET_PW_DATA(w) (container_of((w), struct priv_work_data,	\
					dwork))

struct workqueue_struct *__attribute__((weak)) amz_priv_get_workq(void)
{
	return NULL;
}

static void priv_trig_all_cbs(int on)
{
	if (hw_latch_set) {
		int i;
		pr_debug("%s:%u\n", __func__, __LINE__);
		for (i = 0; i < PRIV_CB_MAX; i++) {
			if (pw_data->cb_arr[i])
				pw_data->cb_arr[i]->cb(
					pw_data->cb_arr[i]->cb_priv_data, on);
		}
	} else {
		return;
	}
}

/*
 * block until hw says it's private
 */
void block_hw_priv(void)
{
	struct device_node *node = NULL;
	node = of_find_node_by_name(NULL, AMZ_PRIVACY_OF_NODE);

	if (NULL == node) {
		pr_err("%s: could not find privacy device tree node\n",
			__func__);
	} else {
		pw_data->public_hw_st_gpio = of_get_named_gpio(node,
						"public_hw_st-gpio", 0);
		if (pw_data->public_hw_st_gpio <= 0) {
			pr_err("%s: gpio err\n", __func__);
			return;
		}
	}

	int ret = gpio_request(pw_data->public_hw_st_gpio, "public_hw_st-gpio");
	if (ret) {
		pr_err("%s:%u gpio req failed ret=%d\n",
				__func__, __LINE__, ret);
		pw_data->public_hw_st_gpio = -EINVAL;
	}

	/* Read PUBLIC_HW GPIO until it goes PRIVATE */
	while (gpio_get_value(pw_data->public_hw_st_gpio)) {
		pr_debug("%s: waiting...\n", __func__);
		msleep(10);
	}

	gpio_free(pw_data->public_hw_st_gpio);

	return;
}

/* gpio assert dance.
 * Empirically found that hw won't
 * latch/unlatch without this
 */
static void assert_priv(void)
{
	if (hw_latch_set) {
		block_hw_priv();
		gpio_set_value(pw_data->priv_gpio, 0);
	} else {
		return;
	}
}


int amz_priv_trigger(int on)
{
	/* if KPOC mode enabled, disable privacy driver */
	if (priv_dis) {
		return 0;
	}

	if (!pw_data) {
		pr_err("%s:%u null error\n", __func__, __LINE__);
		return -EINVAL;
	}

	/* pull/push the plug priv gpio */
	on &= 0x1;
	if (PRIV_GPIO_USR_CNTL != pw_data->priv_gpio) {
		gpio_set_value(pw_data->priv_gpio, on);
	}

	priv_trig_all_cbs(on);
	pr_debug("%s:%u [%d]\n", __func__, __LINE__, on);

	if (on)
		assert_priv();
	/*
	 * delay setting state until hw st asserted
	 * applicable only in hw latch implementation
	 */
	pw_data->cur_priv = on;
	sysfs_notify(pw_data->kobj, NULL, "privacy_state");

	pr_debug("%s:%u done [%d]\n", __func__, __LINE__, on);
	return 0;
}
EXPORT_SYMBOL(amz_priv_trigger);

static int privacy_mode_status;
int amz_priv_timer_sysfs(int on)
{
	/* if KPOC mode enabled, disable privacy driver */
	if (priv_dis) {
		return 0;
	}

	if (!pw_data) {
		pr_err("%s:%u null error\n", __func__, __LINE__);
		return -EINVAL;
	}
	pw_data->cur_timer_on = on;

	/*set to an invalid value so we can tell if it gets cancelled*/
	if (on)
		privacy_mode_status = -1;

	sysfs_notify(pw_data->kobj, NULL, "privacy_timer_on");
	return 0;
}
EXPORT_SYMBOL(amz_priv_timer_sysfs);

void start_privacy_timer_func(struct work_struct *work)
{
	/* if KPOC mode enabled, disable privacy driver */
	if (priv_dis) {
		return;
	}

	struct priv_work_data *pw = GET_PW_DATA(to_delayed_work(work));

	if (!pw) {
		pr_err("%s: %u: pw data is null\n", __func__, __LINE__);
		return;
	}
	amz_priv_trigger(1);
	amz_priv_timer_sysfs(0);
	return;
}
EXPORT_SYMBOL(start_privacy_timer_func);

/* ported from abc123 driver, currently not used
 * leaving in case required for future features
 */
static ssize_t get_power_button_state(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	ret = snprintf(buf, PAGE_SIZE, "%d\n",
			gpio_get_value(pw_data->mute_gpio));
	return ret;
}

static ssize_t get_privacy_state(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	ret = snprintf(buf, PAGE_SIZE, "%d\n",
			pw_data->cur_priv);
	return ret;
}

static ssize_t get_privacy_timer_on(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	ret = snprintf(buf, PAGE_SIZE, "%d\n",
			pw_data->cur_timer_on);
	return ret;
}

static ssize_t get_privacy_trigger(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	ret = snprintf(buf, PAGE_SIZE, "%d\n", privacy_mode_status);
	return ret;
}

static ssize_t set_privacy_trigger(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	if (kstrtoint(buf, 10, &privacy_mode_status)) {
		pr_err("%s: kstrtoint failed\n", __func__);
		return -EPERM;
	}
	pr_debug("%s: %u: Privacy Mode set to [%d]\n", __func__,
			__LINE__, privacy_mode_status);
	if (privacy_mode_status == pw_data->cur_priv) {
		pr_debug("%s: not entering the same state\n", __func__);
		FLUSH_PRIV_WORKQ();
		return ret;
	}

	/* user space has triggered privacy:
	 * ignore exit privacy mode from software
	 */
	if ((privacy_mode_status == 1) && (!pw_data->cur_timer_on)) {
		pr_debug("%s: user space trigd priv\n", __func__);
		amz_priv_trigger(1);
		FLUSH_PRIV_WORKQ();
		return ret;
	}

	/* provide a mechanism for software to cancel queued
	   work items to debounce */
	if (privacy_mode_status == 2) {
		FLUSH_PRIV_WORKQ();
	}

	return ret;
}

static DEVICE_ATTR(privacy_trigger, S_IWUSR | S_IWGRP | S_IRUGO,
		   get_privacy_trigger,
		   set_privacy_trigger);
static DEVICE_ATTR(privacy_state, S_IRUGO,
		   get_privacy_state,
		   NULL);
static DEVICE_ATTR(privacy_timer_on, S_IRUGO,
		   get_privacy_timer_on,
		   NULL);
/* ported from abc123 driver, currently not used
 * leaving in case required for future features
 */
static DEVICE_ATTR(power_button_state, S_IWUSR | S_IWGRP | S_IRUGO,
		   get_power_button_state,
		   NULL);

static struct attribute *gpio_keys_attrs[] = {
	&dev_attr_privacy_trigger.attr,
	&dev_attr_privacy_state.attr,
	&dev_attr_privacy_timer_on.attr,
	&dev_attr_power_button_state.attr,
	NULL,
};

static struct attribute_group gpio_keys_attr_group = {
	.attrs = gpio_keys_attrs,
};

/* ported from abc123 driver, currently not used
 * leaving in case required for future features
 */
int amz_set_public_hw_pin(int gpio)
{
	int ret;

	if (gpio <= 0 || !pw_data) {
		pr_err("%s:%u: error %d\n", __func__, __LINE__, gpio);
		return -EINVAL;
	}

	pw_data->public_hw_st_gpio = gpio;
	ret = gpio_request(pw_data->public_hw_st_gpio, "amz_public_hw_st");
	if (ret) {
		pr_err("%s:%u gpio req failed ret=%d\n",
				__func__, __LINE__, ret);
		pw_data->public_hw_st_gpio = -EINVAL;
	} else {
		pw_data->cur_priv = !gpio_get_value(pw_data->public_hw_st_gpio);
	}
	return 0;
}
EXPORT_SYMBOL(amz_set_public_hw_pin);

/* ported from abc123 driver, currently not used
 * leaving in case required for future features
 */
int amz_set_mute_pin(int gpio)
{
	int ret;

	if (gpio <= 0 || !pw_data) {
		pr_err("%s:%u: error %d\n", __func__, __LINE__, gpio);
		return -EINVAL;
	}

	pw_data->mute_gpio = gpio;
	ret = gpio_request(pw_data->mute_gpio, "amz_mute_pin");
	if (ret) {
		pr_err("%s:%u gpio req failed ret=%d\n",
				__func__, __LINE__, ret);
		pw_data->mute_gpio = -EINVAL;
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL(amz_set_mute_pin);

/* mic mute callback */
int amz_priv_mute_cb(void *data, int value)
{
	struct device_node *node = (struct device_node *)data;
	int ret;

	pw_data->mute_gpio = of_get_named_gpio(node, "mute-gpio", 0);
	ret = gpio_request(pw_data->mute_gpio, "amz_mute_pin");
	if (ret) {
		pr_err("%s:%u gpio req failed ret=%d\n",
				__func__, __LINE__, ret);
		pw_data->mute_gpio = -EINVAL;
		return ret;
	}

	pr_info("%s:%u turning mics %s\n", __func__, __LINE__,
			value ? "off" : "on");
	gpio_set_value(pw_data->mute_gpio, (~value & 0x1));
	gpio_free(pw_data->mute_gpio);
	return 0;
}

#ifdef CONFIG_OF
int amz_priv_kickoff(struct device *dev)
{
	priv_dis = false;
#if defined(CONFIG_roc123) && defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT) {
		priv_dis = true;
		return 0;
	}
#endif
	int ret;
	struct device_node *node = NULL;
	static struct priv_cb_data pcd;

	if (!pw_data) {
		pr_err("%s: pw_data is null\n", __func__);
		return -EINVAL;
	}

	node = of_find_node_by_name(NULL, AMZ_PRIVACY_OF_NODE);
	if (NULL == node) {
		pr_err("%s: could not find privacy device tree node\n",
		       __func__);
		pw_data->priv_gpio = PRIV_GPIO_USR_CNTL;
	} else {
		pw_data->priv_gpio = of_get_gpio(node, 0);
		if (pw_data->priv_gpio <= 0) {
			pr_err("%s: could not find privacy gpio: %d\n",
			       __func__,
			       pw_data->priv_gpio);
			pw_data->priv_gpio = PRIV_GPIO_USR_CNTL;
		}
	}
	pw_data->kobj = &dev->kobj;

	ret = gpio_request(pw_data->priv_gpio, "amz_priv_trig");
	if (ret)
		pr_err("%s:%u gpio req failed ret=%d\n",
				__func__, __LINE__, ret);

	ret = sysfs_create_group(&dev->kobj, &gpio_keys_attr_group);
	if (ret) {
		pr_err("%s:%u:Unable to export priv node, err: %d\n",
					__func__, __LINE__, ret);
		return -EINVAL;
	}

	/* determines whether or not this particular board version has */
	/* the hardware privacy change */
	ret = of_property_read_u32(node, "hw_latch", &hw_latch_set);
	if (ret) {
		pr_err("%s:%u: could not read property from node, "
				"err: %d\n", __func__, __LINE__, ret);
	}
	pr_info("hw latch set to: %d", hw_latch_set);

	/* register mic mute callback */
	pcd.cb_priv_data = (void *)node;
	pcd.cb = amz_priv_mute_cb;
	pcd.cb_dest = PRIV_CB_MIC;
	if (amz_priv_cb_reg(&pcd)) {
		pr_info("amz_priv_cb_reg for mute failed!");
	}

	pr_debug("%s:%u:success!\n", __func__, __LINE__);
	return 0;
}
#else
int amz_priv_kickoff(int gpio, struct device *dev)
{
	int ret;

	if (gpio <= 0 && PRIV_GPIO_USR_CNTL != gpio) {
		pr_err("%s: invalid gpio %d\n", __func__, gpio);
		return -EINVAL;
	}

	if (!pw_data) {
		pr_err("%s: pw_data is null\n", __func__);
		return -EINVAL;
	}
	pw_data->priv_gpio = gpio;
	pw_data->kobj = &dev->kobj;

	if (PRIV_GPIO_USR_CNTL != gpio) {
		ret = gpio_request(pw_data->priv_gpio, "amz_priv_trig");
		if (ret)
			pr_err("%s:%u gpio req failed ret=%d\n",
					__func__, __LINE__, ret);
	}

	ret = sysfs_create_group(&dev->kobj, &gpio_keys_attr_group);
	if (ret) {
		pr_err("%s:%u:Unable to export priv node, err: %d\n",
					__func__, __LINE__, ret);
		return -EINVAL;
	}

	pr_debug("%s:%u:success!\n", __func__, __LINE__);
	return 0;
}
#endif
EXPORT_SYMBOL(amz_priv_kickoff);


int amz_priv_cb_reg(struct priv_cb_data *pcd)
{
	int i;

	if (!pw_data || !pcd || !pcd->cb) {
		pr_err("%s: priv driver not init or null data\n",
			__func__);
		return -EINVAL;
	}

	for (i = 0; i < PRIV_CB_MAX; i++) {
		if (!pw_data->cb_arr[i])
			break;
		if (i == PRIV_CB_MAX - 1) {
			pr_err("%s: all cb registered already\n",
				__func__);
			return -EINVAL;
		}
	}
	if (pcd->cb_dest >= PRIV_CB_MAX) {
		pr_err("%s: invalid cb dest\n", __func__);
		return -EINVAL;
	}

	if (!pw_data->cb_arr[pcd->cb_dest])
		pw_data->cb_arr[pcd->cb_dest] = pcd;
	return 0;
}
EXPORT_SYMBOL(amz_priv_cb_reg);

static int __init amz_priv_init(void)
{
	pw_data = kzalloc(sizeof(struct priv_work_data), GFP_KERNEL);
	if (!pw_data) {
		pr_err("%s: out of mem\n", __func__);
		return -EINVAL;
	}

	return 0;
}
early_initcall(amz_priv_init);
