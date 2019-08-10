/**
 * opt3001.c - Texas Instruments OPT3001 Light Sensor
 *
 * Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com
 *
 * Author: Andreas Dannenberg <dannenberg@ti.com>
 * Based on previous work from: Felipe Balbi <balbi@ti.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 of the License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <linux/string.h>

#include <linux/iio/events.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#ifdef CONFIG_MTK_SENSOR_SUPPORT
#include <alsps.h>
#include <cust_alsps.h>
#include <sensors_io.h>
#endif

#define OPT3001_RESULT		0x00
#define OPT3001_CONFIGURATION	0x01
#define OPT3001_LOW_LIMIT	0x02
#define OPT3001_HIGH_LIMIT	0x03
#define OPT3001_MANUFACTURER_ID	0x7e
#define OPT3001_DEVICE_ID	0x7f

#define OPT3001_CONFIGURATION_RN_MASK	(0xf << 12)
#define OPT3001_CONFIGURATION_RN_AUTO	(0xc << 12)

#define OPT3001_CONFIGURATION_CT	BIT(11)

#define OPT3001_CONFIGURATION_M_MASK	(3 << 9)
#define OPT3001_CONFIGURATION_M_SHUTDOWN (0 << 9)
#define OPT3001_CONFIGURATION_M_SINGLE	(1 << 9)
#define OPT3001_CONFIGURATION_M_CONTINUOUS (2 << 9) /* also 3 << 9 */

#define OPT3001_CONFIGURATION_OVF	BIT(8)
#define OPT3001_CONFIGURATION_CRF	BIT(7)
#define OPT3001_CONFIGURATION_FH	BIT(6)
#define OPT3001_CONFIGURATION_FL	BIT(5)
#define OPT3001_CONFIGURATION_L		BIT(4)
#define OPT3001_CONFIGURATION_POL	BIT(3)
#define OPT3001_CONFIGURATION_ME	BIT(2)

#define OPT3001_CONFIGURATION_FC_MASK	(3 << 0)

/* The end-of-conversion enable is located in the low-limit register */
#define OPT3001_LOW_LIMIT_EOC_ENABLE	0xc000

#define OPT3001_REG_EXPONENT(n)		((n) >> 12)
#define OPT3001_REG_MANTISSA(n)		((n) & 0xfff)

#define OPT3001_INT_TIME_LONG		800000
#define OPT3001_INT_TIME_SHORT		100000

#define MAX_MEASURED_LUX	300

/*
 * Time to wait for conversion result to be ready. The device datasheet
 * sect. 6.5 states results are ready after total integration time plus 3ms.
 * This results in worst-case max values of 113ms or 883ms, respectively.
 * Add some slack to be on the safe side.
 */
#define OPT3001_RESULT_READY_SHORT	150
#define OPT3001_RESULT_READY_LONG	1000
#define APS_TAG                  "[ALS/PS] "
#define APS_FUN(f)               printk(APS_TAG"%s\n", __FUNCTION__)
#define APS_ERR(fmt, args...)    printk(APS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define APS_LOG(fmt, args...)    printk(APS_TAG fmt, ##args)
#define APS_DBG(fmt, args...)    printk(fmt, ##args)

#ifdef CONFIG_MTK_SENSOR_SUPPORT
struct alsps_hw alsps_cust;
static struct alsps_hw *hw = &alsps_cust;
static int  opt3001_local_init(void);
static int  opt3001_local_uninit(void);
#endif

struct opt3001 {
#ifdef CONFIG_MTK_SENSOR_SUPPORT
	struct alsps_hw		*hw;
#endif
	struct i2c_client	*client;
	struct device		*dev;

	struct mutex		lock;
	bool			ok_to_ignore_lock;
	bool			result_ready;
	wait_queue_head_t	result_ready_queue;
	u16			result;

	u32			int_time;
	u32			mode;

	u16			high_thresh_mantissa;
	u16			low_thresh_mantissa;

	u8			high_thresh_exp;
	u8			low_thresh_exp;

	bool			use_irq;
};

struct opt3001_scale {
	int	val;
	int	val2;
};

#ifdef CONFIG_MTK_SENSOR_SUPPORT
static struct alsps_init_info opt3001_init_info = {
	.name = "opt300x",
	.init = opt3001_local_init,
	.uninit = opt3001_local_uninit,
};
#endif

static const struct opt3001_scale opt3001_scales[] = {
	{
		.val = 40,
		.val2 = 950000,
	},
	{
		.val = 81,
		.val2 = 900000,
	},
	{
		.val = 163,
		.val2 = 800000,
	},
	{
		.val = 327,
		.val2 = 600000,
	},
	{
		.val = 655,
		.val2 = 200000,
	},
	{
		.val = 1310,
		.val2 = 400000,
	},
	{
		.val = 2620,
		.val2 = 800000,
	},
	{
		.val = 5241,
		.val2 = 600000,
	},
	{
		.val = 10483,
		.val2 = 200000,
	},
	{
		.val = 20966,
		.val2 = 400000,
	},
	{
		.val = 83865,
		.val2 = 600000,
	},
};

static s32 calibrated_lux_at_0 = -1;

/* Max Raw measurement */
static s32 calibrated_lux_at_300 = -1;

#ifdef CONFIG_MTK_SENSOR_SUPPORT
static struct opt3001 *opt3001_obj;
#endif

static int opt3001_configure(struct opt3001 *opt);

static int opt3001_find_scale(const struct opt3001 *opt, int val,
		int val2, u8 *exponent)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(opt3001_scales); i++) {
		const struct opt3001_scale *scale = &opt3001_scales[i];

		/*
		 * Combine the integer and micro parts for comparison
		 * purposes. Use milli lux precision to avoid 32-bit integer
		 * overflows.
		 */
		if ((val * 1000 + val2 / 1000) <=
				(scale->val * 1000 + scale->val2 / 1000)) {
			*exponent = i;
			return 0;
		}
	}

	return -EINVAL;
}

static void opt3001_to_iio_ret(struct opt3001 *opt, u8 exponent,
		u16 mantissa, int *val, int *val2)
{
	int lux;

	lux = 10 * (mantissa << exponent);
	*val = lux / 1000;
	*val2 = (lux - (*val * 1000)) * 1000;
}

static void opt3001_set_mode(struct opt3001 *opt, u16 *reg, u16 mode)
{
	*reg &= ~OPT3001_CONFIGURATION_M_MASK;
	*reg |= mode;
	opt->mode = mode;
}

static IIO_CONST_ATTR_INT_TIME_AVAIL("0.1 0.8");

static struct attribute *opt3001_attributes[] = {
	&iio_const_attr_integration_time_available.dev_attr.attr,
	NULL
};

static const struct attribute_group opt3001_attribute_group = {
	.attrs = opt3001_attributes,
};

static const struct iio_event_spec opt3001_event_spec[] = {
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_RISING,
		.mask_separate = BIT(IIO_EV_INFO_VALUE) |
			BIT(IIO_EV_INFO_ENABLE),
	},
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_FALLING,
		.mask_separate = BIT(IIO_EV_INFO_VALUE) |
			BIT(IIO_EV_INFO_ENABLE),
	},
};

static const struct iio_chan_spec opt3001_channels[] = {
	{
		.type = IIO_LIGHT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED) |
				BIT(IIO_CHAN_INFO_INT_TIME),
		.event_spec = opt3001_event_spec,
		.num_event_specs = ARRAY_SIZE(opt3001_event_spec),
	},
	IIO_CHAN_SOFT_TIMESTAMP(1),
};

#ifdef CONFIG_MTK_SENSOR_SUPPORT
static int als_set_delay(u64 ns)
{
	return 0;
}

static int als_open_report_data(int open)
{
	APS_LOG("Inside als_open_report_data\n");
	return 0;
}

static int als_enable_nodata(int en)
{
	int ret = 0;
	u16 reg;
	APS_LOG("opt3001_obj als enable value = %d\n", en);

	if (en == 0) {
		ret = i2c_smbus_read_word_swapped(opt3001_obj->client, OPT3001_CONFIGURATION);
		if (ret < 0) {
			dev_err(opt3001_obj->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
			return ret;
		}

		reg = ret;
		opt3001_set_mode(opt3001_obj, &reg, OPT3001_CONFIGURATION_M_SHUTDOWN);

		ret = i2c_smbus_write_word_swapped(opt3001_obj->client, OPT3001_CONFIGURATION,
						reg);
		if (ret < 0) {
			dev_err(opt3001_obj->dev, "failed to write register %02x\n",
				OPT3001_CONFIGURATION);
			return ret;
		}
	} else {
		ret = opt3001_configure(opt3001_obj);
	}

	return ret;
}
#endif

static int opt3001_get_lux(struct opt3001 *opt, int *val, int *val2)
{
	int ret;
	u16 mantissa;
	u16 reg;
	u8 exponent;
	u16 value;
	long timeout;

	if (opt->use_irq) {
		/*
		 * Enable the end-of-conversion interrupt mechanism. Note that
		 * doing so will overwrite the low-level limit value however we
		 * will restore this value later on.
		 */
		ret = i2c_smbus_write_word_swapped(opt->client,
					OPT3001_LOW_LIMIT,
					OPT3001_LOW_LIMIT_EOC_ENABLE);
		if (ret < 0) {
			dev_err(opt->dev, "failed to write register %02x\n",
					OPT3001_LOW_LIMIT);
			return ret;
		}

		/* Allow IRQ to access the device despite lock being set */
		opt->ok_to_ignore_lock = true;
	}

	/* Reset data-ready indicator flag */
	opt->result_ready = false;

	/* Configure for single-conversion mode and start a new conversion */
	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
		goto err;
	}

	reg = ret;
	opt3001_set_mode(opt, &reg, OPT3001_CONFIGURATION_M_SINGLE);

	ret = i2c_smbus_write_word_swapped(opt->client, OPT3001_CONFIGURATION,
			reg);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n",
				OPT3001_CONFIGURATION);
		goto err;
	}

	if (opt->use_irq) {
		/* Wait for the IRQ to indicate the conversion is complete */
		ret = wait_event_timeout(opt->result_ready_queue,
				opt->result_ready,
				msecs_to_jiffies(OPT3001_RESULT_READY_LONG));
	} else {
		/* Sleep for result ready time */
		timeout = (opt->int_time == OPT3001_INT_TIME_SHORT) ?
			OPT3001_RESULT_READY_SHORT : OPT3001_RESULT_READY_LONG;
		msleep(timeout);

		/* Check result ready flag */
		ret = i2c_smbus_read_word_swapped(opt->client,
						  OPT3001_CONFIGURATION);
		if (ret < 0) {
			dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
			goto err;
		}

		if (!(ret & OPT3001_CONFIGURATION_CRF)) {
			ret = -ETIMEDOUT;
			goto err;
		}

		/* Obtain value */
		ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_RESULT);
		if (ret < 0) {
			dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_RESULT);
			goto err;
		}
		opt->result = ret;
		opt->result_ready = true;
	}

err:
	if (opt->use_irq)
		/* Disallow IRQ to access the device while lock is active */
		opt->ok_to_ignore_lock = false;

	if (!opt->result_ready) {
		if (ret == 0)
			return -ETIMEDOUT;
		else if (ret < 0)
			return ret;
	}

	if (opt->use_irq) {
		/*
		 * Disable the end-of-conversion interrupt mechanism by
		 * restoring the low-level limit value (clearing
		 * OPT3001_LOW_LIMIT_EOC_ENABLE). Note that selectively clearing
		 * those enable bits would affect the actual limit value due to
		 * bit-overlap and therefore can't be done.
		 */
		value = (opt->low_thresh_exp << 12) | opt->low_thresh_mantissa;
		ret = i2c_smbus_write_word_swapped(opt->client,
						   OPT3001_LOW_LIMIT,
						   value);
		if (ret < 0) {
			dev_err(opt->dev, "failed to write register %02x\n",
					OPT3001_LOW_LIMIT);
			return ret;
		}
	}

	exponent = OPT3001_REG_EXPONENT(opt->result);
	mantissa = OPT3001_REG_MANTISSA(opt->result);

	opt3001_to_iio_ret(opt, exponent, mantissa, val, val2);

	return IIO_VAL_INT_PLUS_MICRO;
}

#ifdef CONFIG_MTK_SENSOR_SUPPORT
static int als_get_data(int *value, int *status)
{
	int ret = 0;
	int val1;
	int val2;

	int lux_val = 0;

	if (!opt3001_obj) {
		APS_ERR("opt3001_obj is NULL!\n");
		return -1;
	}

	mutex_lock(&opt3001_obj->lock);
	ret = opt3001_get_lux(opt3001_obj, &val1, &val2);
	mutex_unlock(&opt3001_obj->lock);

	lux_val = val2/1000 + val1*1000;

	if (calibrated_lux_at_300 > 0) {

		long calibrated_val = (lux_val * MAX_MEASURED_LUX) / calibrated_lux_at_300;
		*value = (int)calibrated_val;
	} else {
		*value = val1;
	}
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	return 0;
}
#endif

static int opt3001_get_int_time(struct opt3001 *opt, int *val, int *val2)
{
	*val = 0;
	*val2 = opt->int_time;

	return IIO_VAL_INT_PLUS_MICRO;
}

static int opt3001_set_int_time(struct opt3001 *opt, int time)
{
	int ret;
	u16 reg;

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
		return ret;
	}

	reg = ret;

	switch (time) {
	case OPT3001_INT_TIME_SHORT:
		reg &= ~OPT3001_CONFIGURATION_CT;
		opt->int_time = OPT3001_INT_TIME_SHORT;
		break;
	case OPT3001_INT_TIME_LONG:
		reg |= OPT3001_CONFIGURATION_CT;
		opt->int_time = OPT3001_INT_TIME_LONG;
		break;
	default:
		return -EINVAL;
	}

	return i2c_smbus_write_word_swapped(opt->client, OPT3001_CONFIGURATION,
			reg);
}

static int opt3001_read_raw(struct iio_dev *iio,
		struct iio_chan_spec const *chan, int *val, int *val2,
		long mask)
{
	struct opt3001 *opt = iio_priv(iio);
	int ret;

	if (opt->mode == OPT3001_CONFIGURATION_M_CONTINUOUS)
		return -EBUSY;

	if (chan->type != IIO_LIGHT)
		return -EINVAL;

	mutex_lock(&opt->lock);

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		ret = opt3001_get_lux(opt, val, val2);
		break;
	case IIO_CHAN_INFO_INT_TIME:
		ret = opt3001_get_int_time(opt, val, val2);
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&opt->lock);

	return ret;
}

static int opt3001_write_raw(struct iio_dev *iio,
		struct iio_chan_spec const *chan, int val, int val2,
		long mask)
{
	struct opt3001 *opt = iio_priv(iio);
	int ret;

	if (opt->mode == OPT3001_CONFIGURATION_M_CONTINUOUS)
		return -EBUSY;

	if (chan->type != IIO_LIGHT)
		return -EINVAL;

	if (mask != IIO_CHAN_INFO_INT_TIME)
		return -EINVAL;

	if (val != 0)
		return -EINVAL;

	mutex_lock(&opt->lock);
	ret = opt3001_set_int_time(opt, val2);
	mutex_unlock(&opt->lock);

	return ret;
}

static int opt3001_read_event_value(struct iio_dev *iio,
		const struct iio_chan_spec *chan, enum iio_event_type type,
		enum iio_event_direction dir, enum iio_event_info info,
		int *val, int *val2)
{
	struct opt3001 *opt = iio_priv(iio);
	int ret = IIO_VAL_INT_PLUS_MICRO;

	mutex_lock(&opt->lock);

	switch (dir) {
	case IIO_EV_DIR_RISING:
		opt3001_to_iio_ret(opt, opt->high_thresh_exp,
				opt->high_thresh_mantissa, val, val2);
		break;
	case IIO_EV_DIR_FALLING:
		opt3001_to_iio_ret(opt, opt->low_thresh_exp,
				opt->low_thresh_mantissa, val, val2);
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&opt->lock);

	return ret;
}

static int opt3001_write_event_value(struct iio_dev *iio,
		const struct iio_chan_spec *chan, enum iio_event_type type,
		enum iio_event_direction dir, enum iio_event_info info,
		int val, int val2)
{
	struct opt3001 *opt = iio_priv(iio);
	int ret;

	u16 mantissa;
	u16 value;
	u16 reg;

	u8 exponent;

	if (val < 0)
		return -EINVAL;

	mutex_lock(&opt->lock);

	ret = opt3001_find_scale(opt, val, val2, &exponent);
	if (ret < 0) {
		dev_err(opt->dev, "can't find scale for %d.%06u\n", val, val2);
		goto err;
	}

	mantissa = (((val * 1000) + (val2 / 1000)) / 10) >> exponent;
	value = (exponent << 12) | mantissa;

	switch (dir) {
	case IIO_EV_DIR_RISING:
		reg = OPT3001_HIGH_LIMIT;
		opt->high_thresh_mantissa = mantissa;
		opt->high_thresh_exp = exponent;
		break;
	case IIO_EV_DIR_FALLING:
		reg = OPT3001_LOW_LIMIT;
		opt->low_thresh_mantissa = mantissa;
		opt->low_thresh_exp = exponent;
		break;
	default:
		ret = -EINVAL;
		goto err;
	}

	ret = i2c_smbus_write_word_swapped(opt->client, reg, value);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n", reg);
		goto err;
	}

err:
	mutex_unlock(&opt->lock);

	return ret;
}

static int opt3001_read_event_config(struct iio_dev *iio,
		const struct iio_chan_spec *chan, enum iio_event_type type,
		enum iio_event_direction dir)
{
	struct opt3001 *opt = iio_priv(iio);

	return opt->mode == OPT3001_CONFIGURATION_M_CONTINUOUS;
}

static int opt3001_write_event_config(struct iio_dev *iio,
		const struct iio_chan_spec *chan, enum iio_event_type type,
		enum iio_event_direction dir, int state)
{
	struct opt3001 *opt = iio_priv(iio);
	int ret;
	u16 mode;
	u16 reg;

	if (state && opt->mode == OPT3001_CONFIGURATION_M_CONTINUOUS)
		return 0;

	if (!state && opt->mode == OPT3001_CONFIGURATION_M_SHUTDOWN)
		return 0;

	mutex_lock(&opt->lock);

	mode = state ? OPT3001_CONFIGURATION_M_CONTINUOUS
		: OPT3001_CONFIGURATION_M_SHUTDOWN;

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
		goto err;
	}

	reg = ret;
	opt3001_set_mode(opt, &reg, mode);

	ret = i2c_smbus_write_word_swapped(opt->client, OPT3001_CONFIGURATION,
			reg);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n",
				OPT3001_CONFIGURATION);
		goto err;
	}

err:
	mutex_unlock(&opt->lock);

	return ret;
}

static const struct iio_info opt3001_info = {
	.driver_module = THIS_MODULE,
	.attrs = &opt3001_attribute_group,
	.read_raw = opt3001_read_raw,
	.write_raw = opt3001_write_raw,
	.read_event_value = opt3001_read_event_value,
	.write_event_value = opt3001_write_event_value,
	.read_event_config = opt3001_read_event_config,
	.write_event_config = opt3001_write_event_config,
};

static int opt3001_read_id(struct opt3001 *opt)
{
	char manufacturer[2];
	u16 device_id;
	int ret;

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_MANUFACTURER_ID);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_MANUFACTURER_ID);
		return ret;
	}

	manufacturer[0] = ret >> 8;
	manufacturer[1] = ret & 0xff;

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_DEVICE_ID);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_DEVICE_ID);
		return ret;
	}

	device_id = ret;

	dev_info(opt->dev, "Found %c%c OPT%04x\n", manufacturer[0],
			manufacturer[1], device_id);

	return 0;
}

static int opt3001_configure(struct opt3001 *opt)
{
	int ret;
	u16 reg;

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
		return ret;
	}

	reg = ret;

	/* Enable automatic full-scale setting mode */
	reg &= ~OPT3001_CONFIGURATION_RN_MASK;
	reg |= OPT3001_CONFIGURATION_RN_AUTO;

	/* Reflect status of the device's integration time setting */
	if (reg & OPT3001_CONFIGURATION_CT)
		opt->int_time = OPT3001_INT_TIME_LONG;
	else
		opt->int_time = OPT3001_INT_TIME_SHORT;

	/* Ensure device is in shutdown initially */
	opt3001_set_mode(opt, &reg, OPT3001_CONFIGURATION_M_SHUTDOWN);

	/* Configure for latched window-style comparison operation */
	reg |= OPT3001_CONFIGURATION_L;
	reg &= ~OPT3001_CONFIGURATION_POL;
	reg &= ~OPT3001_CONFIGURATION_ME;
	reg &= ~OPT3001_CONFIGURATION_FC_MASK;

	ret = i2c_smbus_write_word_swapped(opt->client, OPT3001_CONFIGURATION,
			reg);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n",
				OPT3001_CONFIGURATION);
		return ret;
	}

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_LOW_LIMIT);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_LOW_LIMIT);
		return ret;
	}

	opt->low_thresh_mantissa = OPT3001_REG_MANTISSA(ret);
	opt->low_thresh_exp = OPT3001_REG_EXPONENT(ret);

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_HIGH_LIMIT);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_HIGH_LIMIT);
		return ret;
	}

	opt->high_thresh_mantissa = OPT3001_REG_MANTISSA(ret);
	opt->high_thresh_exp = OPT3001_REG_EXPONENT(ret);

	return 0;
}

#if CONFIG_IDME
#define IDME_OF_ALSCAL 		"/idme/alscal"
void idme_set_alscal_calibrated_values(void)
{
    struct device_node *ap = NULL;
    char *alscal_idme = NULL;
	char alscal_copy[20];
	char *alscal = alscal_copy;
	char *lux_at_0_str, *lux_at_300_str;
	char *lux_at_300_str_integer = NULL;
	char *lux_at_300_str_decimal = NULL;
	int lux_at_300_integer = 0;
	int lux_at_300_decimal = 0;
	const char delimiters[] = "., ";

	APS_LOG("fetching calibration values for ALSCAL\n");
	ap = of_find_node_by_path(IDME_OF_ALSCAL);
	if (ap)
		alscal_idme = (char *)of_get_property(ap, "value", NULL);
	else {
		pr_err("of_find_node_by_path failed\n");
		return;
	}

	if (alscal_idme == NULL)
		return;

	strcpy(alscal, alscal_idme);

	/* alscal is stored in the format alscal=00,06 or
	 * alscal=00,06.23
	 */
	lux_at_0_str = strsep(&alscal, ", ");

	if (alscal == NULL) {
		return;
	}

	lux_at_300_str = strsep(&alscal, "");

	lux_at_300_str_integer = strsep(&lux_at_300_str, delimiters);

	if (lux_at_300_str != NULL)
		lux_at_300_str_decimal = strsep(&lux_at_300_str, "");

	if (kstrtos32(lux_at_300_str_integer, 16, &lux_at_300_integer) != 0) {
		APS_ERR("Calibration data not found\n");
		return;
	}

	if (lux_at_300_str_decimal != NULL) {
		if (kstrtos32(lux_at_300_str_decimal, 10, &lux_at_300_decimal) != 0) {
			APS_ERR("Calibration data not found\n");
			return;
		}
	}

	/* figuring out the nearest multiplier for the decimal part */
	if (lux_at_300_decimal < 10)
		lux_at_300_decimal *= 100;
	else if (lux_at_300_decimal < 100)
		lux_at_300_decimal *= 10;
	else if (lux_at_300_decimal < 1000)
		lux_at_300_decimal *= 1;

	/* Multiply by 1000 to aid in fixed point division when
	 * scaling the values */
	calibrated_lux_at_300 = lux_at_300_integer * 1000 + lux_at_300_decimal;
	calibrated_lux_at_0 *= 1000;

	APS_LOG("Raw value at 300 lux is %d\n", calibrated_lux_at_300);
}
#endif

static irqreturn_t opt3001_irq(int irq, void *_iio)
{
	struct iio_dev *iio = _iio;
	struct opt3001 *opt = iio_priv(iio);
	int ret;

	if (!opt->ok_to_ignore_lock)
		mutex_lock(&opt->lock);

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
		goto out;
	}

	if ((ret & OPT3001_CONFIGURATION_M_MASK) ==
			OPT3001_CONFIGURATION_M_CONTINUOUS) {
		if (ret & OPT3001_CONFIGURATION_FH)
			iio_push_event(iio,
					IIO_UNMOD_EVENT_CODE(IIO_LIGHT, 0,
							IIO_EV_TYPE_THRESH,
							IIO_EV_DIR_RISING),
					iio_get_time_ns());
		if (ret & OPT3001_CONFIGURATION_FL)
			iio_push_event(iio,
					IIO_UNMOD_EVENT_CODE(IIO_LIGHT, 0,
							IIO_EV_TYPE_THRESH,
							IIO_EV_DIR_FALLING),
					iio_get_time_ns());
	} else if (ret & OPT3001_CONFIGURATION_CRF) {
		ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_RESULT);
		if (ret < 0) {
			dev_err(opt->dev, "failed to read register %02x\n",
					OPT3001_RESULT);
			goto out;
		}
		opt->result = ret;
		opt->result_ready = true;
		wake_up(&opt->result_ready_queue);
	}

out:
	if (!opt->ok_to_ignore_lock)
		mutex_unlock(&opt->lock);

	return IRQ_HANDLED;
}

static int opt3001_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;

	struct iio_dev *iio;
	struct opt3001 *opt;
	int irq = client->irq;
	int ret;
#ifdef CONFIG_MTK_SENSOR_SUPPORT
	static struct als_data_path als_data = {0};
	static struct als_control_path als_ctl = {0};
#endif
	APS_LOG("%s: opt3001_probe \n", __FUNCTION__);

	iio = devm_iio_device_alloc(dev, sizeof(*opt));
	if (!iio)
		return -ENOMEM;

	opt = iio_priv(iio);
#ifdef CONFIG_MTK_SENSOR_SUPPORT
	opt3001_obj = opt;
	opt->hw = &alsps_cust;
#endif
	opt->client = client;
	opt->dev = dev;


	mutex_init(&opt->lock);
	init_waitqueue_head(&opt->result_ready_queue);
	i2c_set_clientdata(client, iio);

	ret = opt3001_read_id(opt);
	if (ret)
		return ret;

	ret = opt3001_configure(opt);
	if (ret)
		return ret;

	iio->name = client->name;
	iio->channels = opt3001_channels;
	iio->num_channels = ARRAY_SIZE(opt3001_channels);
	iio->dev.parent = dev;
	iio->modes = INDIO_DIRECT_MODE;
	iio->info = &opt3001_info;

	ret = devm_iio_device_register(dev, iio);
	if (ret) {
		dev_err(dev, "failed to register IIO device\n");
		return ret;
	}

#ifdef CONFIG_MTK_SENSOR_SUPPORT
	als_ctl.open_report_data = als_open_report_data;
	als_ctl.enable_nodata = als_enable_nodata;
	als_ctl.set_delay = als_set_delay;
	als_ctl.is_report_input_direct = false;
	als_ctl.is_support_batch = false;

	ret = als_register_control_path(&als_ctl);
	if (ret) {
		APS_ERR("register fail = %d\n", ret);
		return ret;
	}
	als_data.get_data = als_get_data;
	als_data.vender_div = 100;
	ret = als_register_data_path(&als_data);
	if (ret) {
		APS_ERR("register data fail = %d\n", ret);
		return ret;
	}

	ret = batch_register_support_info(ID_LIGHT, als_ctl.is_support_batch, 100, 0);
	if (ret) {
		APS_ERR("register light batch support ret = %d\n", ret);
	}
#endif

	/* Make use of INT pin only if valid IRQ no. is given */
	if (irq > 0) {
		ret = request_threaded_irq(irq, NULL, opt3001_irq,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"opt300x", iio);
		if (ret) {
			dev_err(dev, "failed to request IRQ #%d\n", irq);
			return ret;
		}
		opt->use_irq = true;
	} else {
		dev_dbg(opt->dev, "enabling interrupt-less operation\n");
	}

	return 0;
}

static int opt3001_remove(struct i2c_client *client)
{
	struct iio_dev *iio = i2c_get_clientdata(client);
	struct opt3001 *opt = iio_priv(iio);
	int ret;
	u16 reg;

	if (opt->use_irq)
		free_irq(client->irq, iio);

	ret = i2c_smbus_read_word_swapped(opt->client, OPT3001_CONFIGURATION);
	if (ret < 0) {
		dev_err(opt->dev, "failed to read register %02x\n",
				OPT3001_CONFIGURATION);
		return ret;
	}

	reg = ret;
	opt3001_set_mode(opt, &reg, OPT3001_CONFIGURATION_M_SHUTDOWN);

	ret = i2c_smbus_write_word_swapped(opt->client, OPT3001_CONFIGURATION,
			reg);
	if (ret < 0) {
		dev_err(opt->dev, "failed to write register %02x\n",
				OPT3001_CONFIGURATION);
		return ret;
	}

	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct i2c_device_id opt3001_id[] = {
	{ "opt300x", 0 },
	{ } /* Terminating Entry */
};
MODULE_DEVICE_TABLE(i2c, opt3001_id);

static const struct of_device_id opt3001_of_match[] = {
#ifdef CONFIG_MTK_SENSOR_SUPPORT
	{ .compatible = "mediatek,alsps" },
#else
	{ .compatible = "ti,opt3001" },
#endif
	{ }
};

static struct i2c_driver opt3001_driver = {
	.probe = opt3001_probe,
	.remove = opt3001_remove,
	.id_table = opt3001_id,

	.driver = {
		.name = "opt300x",
		.of_match_table = of_match_ptr(opt3001_of_match),
	},
};

#ifdef CONFIG_MTK_SENSOR_SUPPORT
static int opt3001_local_init(void)
{
	if (i2c_add_driver(&opt3001_driver)) {
		APS_ERR("Add driver error\n");
		return -1;
	}
	return 0;
}

static int opt3001_local_uninit(void)
{
	APS_FUN();
	i2c_del_driver(&opt3001_driver);
	return 0;
}
#endif

static int __init opt3001_init(void)
{
#ifdef CONFIG_MTK_SENSOR_SUPPORT
	const char *name = "mediatek,opt300x";
	hw = get_alsps_dts_func(name, hw);
	APS_LOG("%s: i2c_number=%d\n", __func__, hw->i2c_num);
	alsps_driver_add(&opt3001_init_info);
#else
	 if (i2c_add_driver(&opt3001_driver)) {
		APS_ERR("Add driver error\n");
		return -1;
	 }
#endif
#if CONFIG_IDME
	idme_set_alscal_calibrated_values();
#endif
	return 0;
}

static void __exit opt3001_exit(void)
{
#ifndef CONFIG_MTK_SENSOR_SUPPORT
	i2c_del_driver(&opt3001_driver);
#else
	APS_FUN();
#endif
}

module_init(opt3001_init);
module_exit(opt3001_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Andreas Dannenberg <dannenberg@ti.com>");
MODULE_DESCRIPTION("Texas Instruments OPT3001 Light Sensor Driver");
