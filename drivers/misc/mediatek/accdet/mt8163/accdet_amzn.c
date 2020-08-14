/*
* Copyright(C)2018 MediaTek Inc.
* Modification based on code covered by the below mentioned copyright
* and/or permission notice(S).
*/

/* accdet_amzn.c
 *
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
#include "accdet_amzn.h"

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <upmu_common.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#endif

#define DEBUG_THREAD 1

#define ACCDET_DEBUG(format, args...) pr_debug(format, ##args)
#define ACCDET_INFO(format, args...) pr_warn(format, ##args)
#define ACCDET_ERROR(format, args...) pr_err(format, ##args)

#define EINT_PIN_PLUG_IN        (1)
#define EINT_PIN_PLUG_OUT       (0)

/* Status of headset jack */
enum accdet_status {
	PLUG_OUT = 0,
	HEADSET = 1
};
/* Debounce values */
struct head_dts_data {
	int accdet_plugout_debounce;
	int eint_debounce;
};

/*******************************************************************************
 * Global static variables
 * ****************************************************************************/

/* Device tree handle */
struct head_dts_data accdet_dts_data;
/* IRQ value for gpio pin*/
static int accdet_irq;
/* GPIO pin connected to headset */
static int gpiopin = -1;
/* Global status of initialization. By default false */
static bool initialized;
/* Interrupt type*/
unsigned int accdet_eint_type;
/* h2w switch */
static struct switch_dev accdet_data;
/* key pad switch */
static struct input_dev *kpd_accdet_dev;
/* chr dev handle for driver */
static struct cdev *accdet_cdev;
/* class for driver */
static struct class *accdet_class;
/* device node */
static struct device *accdet_nor_device;
static dev_t accdet_devno;
/* Global status of connector */
static int jack_status = PLUG_OUT;
static DEFINE_MUTEX(accdet_eint_irq_sync_mutex);

/* Work queue to spin off status reporting */
static struct work_struct accdet_eint_work;
static struct workqueue_struct *accdet_eint_wq;

/* Used to let accdet know if the pin has been fully plugged-in */
int cur_eint_state = EINT_PIN_PLUG_OUT;
struct pinctrl *accdet_pinctrl1;
struct pinctrl_state *pins_eint_int;

char *accdet_status_string[5] = {
	"Plug_out",
	"Headset_plug_in"
};

#ifdef CONFIG_AMAZON_METRICS_LOG
static char *accdet_metrics_cable_string[3] = {
	"NOTHING",
	"HEADSET",
	"HEADPHONES"
};
#endif

/*******************************************************************************
 * accdet_check_status:
 * Checks status of connector. Report true if status has changed
 * ****************************************************************************/
static bool accdet_check_status(void)
{
	int current_status = 0;
	bool changed = false;

	ACCDET_DEBUG("[accdet]%s+ Plugged Before=%d\n", __func__, jack_status);

	if (gpiopin < 0) {
		ACCDET_ERROR("[accdet] gpiopin unprepared = %d\n", gpiopin);
		return false;
	}

	current_status = !gpio_get_value(gpiopin);
	mutex_lock(&accdet_eint_irq_sync_mutex);
	if (current_status != jack_status) {
		ACCDET_INFO("[accdet]%s- status switch:[%s]->[%s]\n", __func__,
			accdet_status_string[jack_status],
			accdet_status_string[current_status]);
		jack_status = current_status;
		changed = true;
	}
	mutex_unlock(&accdet_eint_irq_sync_mutex);

	ACCDET_DEBUG("[accdet]%s- Plugged After=%d\n", __func__,
		jack_status);

	return changed;
}

/*******************************************************************************
 * accdet_report_status:
 * Report status of connector in switch and log metrics.
 * ****************************************************************************/
static void accdet_report_status(void)
{
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[128];
#endif
	ACCDET_DEBUG("[accdet]%s+\n", __func__);

	/* Update status of connector */
	accdet_check_status();

	/* Update reporting status */
	switch_set_state((struct switch_dev *)&accdet_data,
		jack_status);

#ifdef CONFIG_AMAZON_METRICS_LOG
		if (jack_status == PLUG_OUT)
			snprintf(buf, sizeof(buf),
				"%s:jack:unplugged=1;CT;1:NR", __func__);
		else
			snprintf(buf, sizeof(buf),
			"%s:jack:plugged=1;CT;1,state_%s=1;CT;1:NR", __func__,
				accdet_metrics_cable_string[jack_status]);

		log_to_metrics(ANDROID_LOG_INFO, "AudioJackEvent", buf);
#endif

	ACCDET_DEBUG("[accdet]%s-\n", __func__);
}

/*******************************************************************************
 * accdet_eint_work_callback:
 * Bottom half of irq. Checks and report status. Then renables irq for gpio.
 * ****************************************************************************/
static void accdet_eint_work_callback(struct work_struct *work)
{
	ACCDET_DEBUG("[accdet]%s+\n", __func__);

	accdet_report_status();
	enable_irq(accdet_irq);

	ACCDET_DEBUG("[accdet]%s-: enable_irq  !!!!!!\n", __func__);
}

/*******************************************************************************
 * accdet_irq_callback:
 * Callback from gpio irq. Queues work to check and report connector status.
 * ****************************************************************************/
static irqreturn_t accdet_irq_callback(int irq, void *data)
{
	int ret = 0;

	if (gpiopin < 0 || initialized == false) {
		ACCDET_ERROR("[accdet]%s Cannot proceed! gpiopin=%d init=%d\n",
			__func__, gpiopin, initialized);
		return IRQ_HANDLED;
	}

	if (cur_eint_state == EINT_PIN_PLUG_IN) {
		/*
		   To trigger EINT when the headset was plugged in
		   We set the polarity back as we initialed.
		 */
		if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
			irq_set_irq_type(accdet_irq, IRQ_TYPE_LEVEL_HIGH);
		else
			irq_set_irq_type(accdet_irq, IRQ_TYPE_LEVEL_LOW);
		gpio_set_debounce(gpiopin, accdet_dts_data.eint_debounce);
		/* update the eint status */
		cur_eint_state = EINT_PIN_PLUG_OUT;
	} else {
		/*
		   To trigger EINT when the headset was plugged out
		   We set the opposite polarity to what we initialed.
		 */
		if (accdet_eint_type == IRQ_TYPE_LEVEL_HIGH)
			irq_set_irq_type(accdet_irq, IRQ_TYPE_LEVEL_LOW);
		else
			irq_set_irq_type(accdet_irq, IRQ_TYPE_LEVEL_HIGH);

		gpio_set_debounce(gpiopin,
			accdet_dts_data.accdet_plugout_debounce);
		/* update the eint status */
		cur_eint_state = EINT_PIN_PLUG_IN;
	}
	disable_irq_nosync(accdet_irq);
	ACCDET_DEBUG("[accdet]accdet_eint_callback after cur_eint_state=%d\n",
		cur_eint_state);

	ret = queue_work(accdet_eint_wq, &accdet_eint_work);

	ACCDET_DEBUG("[accdet]%s-\n", __func__);

	return IRQ_HANDLED;
}

/*******************************************************************************
 * accdet_setup_eint:
 * Setup using device tree and pinctrl information.
 * ****************************************************************************/
static inline int accdet_setup_eint(struct platform_device *accdet_device)
{
	int ret = 0;
	u32 ints1[2] = { 0, 0 };
	struct device_node *node = NULL;

	/*configure to GPIO function, external interrupt */
	ACCDET_DEBUG("[accdet]%s+\n", __func__);
	accdet_pinctrl1 = devm_pinctrl_get(&accdet_device->dev);
	if (IS_ERR(accdet_pinctrl1)) {
		ret = PTR_ERR(accdet_pinctrl1);
		ACCDET_ERROR("[accdet]%s accdet_pinctrl1 error=%d\n", __func__,
			ret);
		return ret;
	}

	pins_eint_int = pinctrl_lookup_state(accdet_pinctrl1,
				"state_eint_as_int");
	if (IS_ERR(pins_eint_int)) {
		ret = PTR_ERR(pins_eint_int);
		ACCDET_ERROR("[accdet]%s accdet state_eint_int error=%d!\n",
			__func__, ret);
		return ret;
	}

	pinctrl_select_state(accdet_pinctrl1, pins_eint_int);
	ACCDET_INFO("[accdet]%s  Select pinctrl state done\n", __func__);

	node = of_find_matching_node(node, accdet_of_match);
	if (node) {
		of_property_read_u32(node, "accdet-plugout-debounce",
			&accdet_dts_data.accdet_plugout_debounce);
		of_property_read_u32(node, "eint-debounce",
			&accdet_dts_data.eint_debounce);
		of_property_read_u32_array(node, "interrupts", ints1,
			ARRAY_SIZE(ints1));

		gpiopin = of_get_named_gpio(node, "accdet-gpio", 0);
		if (gpiopin < 0) {
			ACCDET_ERROR("[accdet]%s not find accdet-gpio\n",
				__func__);
			return -EINVAL;
		}

		accdet_eint_type = ints1[1];

		ret = gpio_request(gpiopin, "accdet-gpio");
		if (ret) {
			ACCDET_ERROR("[accdet]%s gpio_request fail, ret(%d)\n",
				__func__, ret);
			return ret;
		}

		gpio_direction_input(gpiopin);
		gpio_set_debounce(gpiopin, accdet_dts_data.eint_debounce);

		accdet_irq = irq_of_parse_and_map(node, 0);
		ret = request_irq(accdet_irq, accdet_irq_callback,
			IRQF_TRIGGER_NONE, "accdet-eint", NULL);
		if (ret != 0) {
			ACCDET_ERROR("[accdet]%s EINT IRQ LINE NOT AVAILABLE\n",
				__func__);
		} else {
			ACCDET_INFO("[accdet]%s- EINT accdet_irq=%d, deb=%d\n",
				__func__, accdet_irq,
			accdet_dts_data.eint_debounce);
		}
	} else {
		ACCDET_ERROR("[accdet]%s- can't find compatible node\n",
			__func__);
	}

	return ret;
}

/*******************************************************************************
 * show_accdet_state:
 * Show status of connector in sysfs: 1=Plugged, 0=Unplugged
 * ****************************************************************************/
static ssize_t show_accdet_state(struct device_driver *ddri, char *buf)
{
	int current_status;

	if (gpiopin < 0) {
		ACCDET_ERROR("[accdet]%s gpiopin unassigned = %d\n", __func__,
			gpiopin);
		return -EINVAL;
	}

	current_status = gpio_get_value(gpiopin);
	ACCDET_DEBUG("[accdet]%s Plugged = %d\n", __func__,
		current_status);

	return sprintf(buf, "%u\n", current_status);
}

static DRIVER_ATTR(accdet_plugged, 0664, show_accdet_state, NULL);

static struct driver_attribute *accdet_attr_list[] = {
	&driver_attr_accdet_plugged,
};

/*******************************************************************************
 * accdet_create_attr:
 * Create sysfs attribute
 * ****************************************************************************/
static int accdet_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(accdet_attr_list) / sizeof(accdet_attr_list[0]));

	ACCDET_DEBUG("[accdet]%s+\n", __func__);
	if (driver == NULL)
		return -EINVAL;
	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, accdet_attr_list[idx]);
		if (err) {
			ACCDET_DEBUG("driver_create_file (%s) = %d\n",
				accdet_attr_list[idx]->attr.name, err);
			break;
		}
	}
	ACCDET_DEBUG("[accdet]%s-\n", __func__);
	return err;
}
/******************************************************************************/


/*******************************************************************************
 * mt_accdet_probe:
 * Initializes pinctrl, gpios, h2w switch, nodes
 * ****************************************************************************/
int mt_accdet_probe(struct platform_device *dev)
{
	int ret = 0;

#if DEBUG_THREAD
	struct platform_driver accdet_driver_hal = accdet_driver_func();
#endif

	ACCDET_INFO("[accdet]%s+\n", __func__);

	/*--------------------------------------------------------------------
	// below register accdet as switch class
	//------------------------------------------------------------------*/
	accdet_data.name = "h2w";
	accdet_data.index = 0;
	accdet_data.state = PLUG_OUT;
	ret = switch_dev_register(&accdet_data);
	if (ret) {
		ACCDET_ERROR("[accdet]%s switch_dev_register returned:%d!\n",
			__func__, ret);
		return ret;
	}
	/*----------------------------------------------------------------------
	// Create normal device for auido use
	//--------------------------------------------------------------------*/
	ret = alloc_chrdev_region(&accdet_devno, 0, 1, ACCDET_DEVNAME);
	if (ret) {
		ACCDET_ERROR("[accdet]%s Get Major number error!\n",
			__func__);
		return ret;
	}

	accdet_cdev = cdev_alloc();
	accdet_cdev->owner = THIS_MODULE;
	accdet_cdev->ops = accdet_get_fops();
	ret = cdev_add(accdet_cdev, accdet_devno, 1);
	if (ret) {
		ACCDET_ERROR("[accdet]%s error: cdev_add\n", __func__);
		goto clean_chrdev;
	}

	accdet_class = class_create(THIS_MODULE, ACCDET_DEVNAME);

	/* if we want auto creat device node, we must call this*/
	accdet_nor_device = device_create(accdet_class, NULL, accdet_devno,
			NULL, ACCDET_DEVNAME);

	/*--------------------------------------------------------------------
	 * Create input device
	 *------------------------------------------------------------------*/
	kpd_accdet_dev = input_allocate_device();
	if (!kpd_accdet_dev) {
		ACCDET_ERROR("[accdet]%s kpd_accdet_dev : fail!\n", __func__);
		ret = -ENOMEM;
		goto clean_class;
	}

	/*define multi-key keycode*/
	__set_bit(EV_KEY, kpd_accdet_dev->evbit);

	kpd_accdet_dev->id.bustype = BUS_HOST;
	kpd_accdet_dev->name = "ACCDET";
	ret = input_register_device(kpd_accdet_dev);
	if (ret) {
		ACCDET_ERROR("[accdet]%s kpd_accdet_dev register error=%d!\n",
			__func__, ret);
		input_free_device(kpd_accdet_dev);
		goto clean_class;
	}
#if DEBUG_THREAD
	ret = accdet_create_attr(&accdet_driver_hal.driver);
	if (ret) {
		ACCDET_ERROR("[accdet]%s create attribute error = %d\n",
			__func__, ret);
		goto clean_kpd;
	}
#endif

	ret = accdet_setup_eint(dev);
	if (ret) {
		ACCDET_ERROR("[accdet]%s setup error=%d\n", __func__,
			ret);
		goto clean_kpd;
	}
	/* Work thread of gpio interrupts */
	accdet_eint_wq = create_singlethread_workqueue("accdet_eint");
	INIT_WORK(&accdet_eint_work, accdet_eint_work_callback);

	initialized = true;

	accdet_report_status();

	ACCDET_INFO("[accdet]%s-\n", __func__);

	/* Successfully loaded */
	return 0;

	/* Cleanup in error case */
clean_kpd:
	input_unregister_device(kpd_accdet_dev);
clean_class:
	class_destroy(accdet_class);
	cdev_del(accdet_cdev);
clean_chrdev:
	unregister_chrdev_region(accdet_devno, 1);

	return ret;
}

/**********************************************************************
 * mt_accdet_remove:
 * clean up
 * ********************************************************************/
void mt_accdet_remove(void)
{
	ACCDET_DEBUG("[accdet]%s+\n", __func__);

	initialized = false;
	destroy_workqueue(accdet_eint_wq);
	switch_dev_unregister(&accdet_data);
	device_del(accdet_nor_device);
	class_destroy(accdet_class);
	cdev_del(accdet_cdev);
	unregister_chrdev_region(accdet_devno, 1);
	input_unregister_device(kpd_accdet_dev);
	ACCDET_DEBUG("[accdet]%s-\n", __func__);
}

/**********************************************************************
 * mt_accdet_pm_restore_noirq:
 * Read and update state on resume
***********************************************************************/
void mt_accdet_pm_restore_noirq(void)
{
	int current_status_restore = 0;

	ACCDET_DEBUG("[accdet]%s+\n", __func__);

	current_status_restore = gpio_get_value(gpiopin);

	if (gpiopin < 0)
		ACCDET_ERROR("[accdet] gpiopin unprepared = %d\n", gpiopin);

	if (current_status_restore)
		jack_status = HEADSET;
	else
		jack_status = PLUG_OUT;

	switch_set_state((struct switch_dev *)&accdet_data, jack_status);

	ACCDET_DEBUG("[accdet]%s-\n", __func__);
}

/**********************************************************************
 * mt_accdet_unlocked_ioctl
 * Needed by userspace services. Has no use in the driver.
 * TODO: Remove it once userspace stops calling these ioctls
***********************************************************************/
long mt_accdet_unlocked_ioctl(unsigned int cmd, unsigned long arg)
{
	ACCDET_DEBUG("[accdet]%s cmd=%u, arg=%lu\n", __func__, cmd, arg);
	return 0;
}
