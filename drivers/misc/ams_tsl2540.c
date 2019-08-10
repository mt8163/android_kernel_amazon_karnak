/*
 * Device driver for monitoring ambient light intensity (lux)
 * for device tsl2540.
 *
 * Copyright (c) 2017, ams AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

/*! \file
* \brief Device driver for monitoring ambient light intensity in (lux)
* functionality within the AMS-TAOS TSL2540 family of devices.
*/

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>

#include <linux/i2c/ams_tsl2540.h>
#include "ams_i2c.h"
#include "ams_tsl2540_als.h"

/* TSL2540 Identifiers */
static u8 const tsl2540_ids[] = {
/* ID, AUXID, REV */
	0xE4,	0x01,	0x61
};

/* TSL2540 Device Names */
static char const *tsl2540_names[] = {
	"tsl2540"
};

/* Registers to restore */
static u8 const restorable_regs[] = {
	TSL2540_REG_PERS,
	TSL2540_REG_PGCFG0,
	TSL2540_REG_PGCFG1,
	TSL2540_REG_CFG1,
	TSL2540_REG_CFG2,
	TSL2540_REG_PTIME,
	TSL2540_REG_ATIME,
};

#ifdef TSL2540_ENABLE_INTERRUPT
static int tsl2540_irq_handler(struct tsl2540_chip *chip)
{
	u8 status;
	int ret;

	ret = ams_i2c_read(chip->client, TSL2540_REG_STATUS,
			&chip->shadow[TSL2540_REG_STATUS]);
	status = chip->shadow[TSL2540_REG_STATUS];

	if (status == 0)
		return 0;  /* not our interrupt */

	do {

		/* Clear the interrupts we'll process */
		ams_i2c_write_direct(chip->client, TSL2540_REG_STATUS, status);

		/*
		 * ALS
		 */
		if (status & TSL2540_ST_ALS_IRQ) {
			tsl2540_read_als(chip);
			tsl2540_report_als(chip);
		}

		/*
		 * Calibration
		 */
		if (status & TSL2540_ST_CAL_IRQ) {
			chip->amscalcomplete = true;

			/*
			 * Calibration has completed, no need for more
			 *  calibration interrupts. These events are one-shots.
			 *  next calibration start will re-enable.
			 */
			ams_i2c_modify(chip->client, chip->shadow,
				TSL2540_REG_INTENAB, TSL2540_CIEN, 0);
		}

		ret = ams_i2c_read(chip->client, TSL2540_REG_STATUS,
				&chip->shadow[TSL2540_REG_STATUS]);
		status = chip->shadow[TSL2540_REG_STATUS];
	} while (status != 0);

	return 1;  /* we handled the interrupt */
}

static irqreturn_t tsl2540_irq(int irq, void *handle)
{
	struct tsl2540_chip *chip = handle;
	struct device *dev = &chip->client->dev;
	int ret;

	if (chip->in_suspend) {
		dev_info(dev, "%s: in suspend\n", __func__);
		chip->irq_pending = 1;
		ret = 0;
		goto bypass;
	}
	ret = tsl2540_irq_handler(chip);

bypass:
	return ret ? IRQ_HANDLED : IRQ_NONE;
}
#endif /* TSL2540_ENABLE_INTERRUPT */

static int tsl2540_flush_regs(struct tsl2540_chip *chip)
{
	unsigned i;
	int rc;
	u8 reg;

	for (i = 0; i < ARRAY_SIZE(restorable_regs); i++) {
		reg = restorable_regs[i];
		rc = ams_i2c_write(chip->client, chip->shadow, reg,
				chip->shadow[reg]);
		if (rc) {
			dev_err(&chip->client->dev, "%s: err on reg 0x%02x\n",
					__func__, reg);
			break;
		}
	}

	return rc;
}

static int tsl2540_update_enable_reg(struct tsl2540_chip *chip)
{
	return ams_i2c_write(chip->client, chip->shadow, TSL2540_REG_ENABLE,
			chip->shadow[TSL2540_REG_ENABLE]);
}

static int tsl2540_pltf_power_on(struct tsl2540_chip *chip)
{
	int rc = 0;

	if (chip->pdata->platform_power) {
		rc = chip->pdata->platform_power(&chip->client->dev,
			POWER_ON);
		mdelay(10);
	}
	chip->unpowered = rc != 0;
	dev_err(&chip->client->dev, "\n\n%s: unpowered=%d\n", __func__,
			chip->unpowered);
	return rc;
}

static int tsl2540_pltf_power_off(struct tsl2540_chip *chip)
{
	int rc = 0;

	if (chip->pdata->platform_power) {
		rc = chip->pdata->platform_power(&chip->client->dev,
			POWER_OFF);
		chip->unpowered = rc == 0;
	} else {
		chip->unpowered = false;
	}
	dev_err(&chip->client->dev, "\n\n%s: unpowered=%d\n", __func__,
			chip->unpowered);
	return rc;
}

static void tsl2540_set_defaults(struct tsl2540_chip *chip)
{
	u8 *sh = chip->shadow;
	struct device *dev = &chip->client->dev;

	/* Clear the register shadow area */
	memset(chip->shadow, 0x00, sizeof(chip->shadow));

	/* If there is platform data use it */
	if (chip->pdata) {
		dev_info(dev, "%s: Loading pltform data\n", __func__);
		chip->params.persist = chip->pdata->parameters.persist;
		chip->params.als_gain = chip->pdata->parameters.als_gain;
		chip->params.als_gain_factor =
				chip->pdata->parameters.als_gain_factor;
		chip->params.als_deltap = chip->pdata->parameters.als_deltap;
		chip->params.als_time = chip->pdata->parameters.als_time;
		chip->params.d_factor = chip->pdata->parameters.d_factor;
		chip->params.lux_segment[0].ch0_coef =
				chip->pdata->parameters.lux_segment[0].ch0_coef;
		chip->params.lux_segment[0].ch1_coef =
				chip->pdata->parameters.lux_segment[0].ch1_coef;
		chip->params.lux_segment[1].ch0_coef =
				chip->pdata->parameters.lux_segment[1].ch0_coef;
		chip->params.lux_segment[1].ch1_coef =
				chip->pdata->parameters.lux_segment[1].ch1_coef;
		chip->params.az_iterations =
				chip->pdata->parameters.az_iterations;
	} else {
		dev_info(dev, "%s: use defaults\n", __func__);
		chip->params.persist = ALS_PERSIST(2);
		chip->params.als_gain = AGAIN_16;
		chip->params.als_gain_factor = 0;
		chip->params.als_deltap = 10;
		chip->params.als_time = AW_TIME_MS(200);
		chip->params.d_factor = 41;
		chip->params.lux_segment[0].ch0_coef = 1000;
		chip->params.lux_segment[0].ch1_coef = 260;
		chip->params.lux_segment[1].ch0_coef = 800;
		chip->params.lux_segment[1].ch1_coef = 270;
		chip->params.az_iterations = 64;
	}

	chip->als_gain_auto = false;

	/* Copy the default values into the register shadow area */
	sh[TSL2540_REG_PERS]    = chip->params.persist;
	sh[TSL2540_REG_ATIME]   = chip->params.als_time;
	sh[TSL2540_REG_CFG1]    = chip->params.als_gain;
	sh[TSL2540_REG_AZ_CONFIG] = chip->params.az_iterations;
	switch (chip->params.als_gain_factor) {
	case TSL2540_50_PERCENTS:
		sh[TSL2540_REG_CFG2] = 0x00;
		break;
	case TSL2540_200_PERCENTS:
		sh[TSL2540_REG_CFG2] = (0x1 << TSL2540_SHIFT_AGAINL) |
					(0x1 << TSL2540_SHIFT_AGAINMAX);
		break;
	case TSL2540_100_PERCENTS:
	default:
		sh[TSL2540_REG_CFG2] = 0x1 << TSL2540_SHIFT_AGAINL;
		break;
	}
	tsl2540_flush_regs(chip);
}

#ifdef ABI_SET_GET_REGISTERS
/* bitmap of registers that are in use */
static u8 reginuse[MAX_REGS / 8] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 0x00 - 0x3f */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 0x40 - 0x7f */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	/* 0x80 - 0xbf */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	/* 0xc0 - 0xff */
};

static ssize_t tsl2540_regs_get(struct tsl2540_chip *chip, char *buf,
		int bufsiz)
{
	u8 regval[16];
	int i, j, cnt;

	/* find first */
	for (i = 0; i < sizeof(reginuse) / sizeof(reginuse[0]); i++) {
		if (reginuse[i] != 0)
			break;
	}

	/* round down to the start of a group of 16 */
	i &= ~1;

	/* set to actual register id */
	i *= 8;

	cnt = 0;
	for (; i < MAX_REGS; i += 16) {
		cnt += snprintf(buf + cnt, bufsiz - cnt, "%02x  ", i);

		ams_i2c_blk_read(chip->client, i, &regval[0], 16);

		for (j = 0; j < 16; j++) {

			if (reginuse[(i >> 3) + (j >> 3)] & (1 << (j & 7))) {
				cnt += snprintf(buf + cnt, bufsiz - cnt,
						" %02x", regval[j]);
			} else {
				cnt += snprintf(buf + cnt, bufsiz - cnt, " --");
			}

			if (j == 7)
				cnt += snprintf(buf + cnt, bufsiz - cnt, "  ");
		}

		cnt += snprintf(buf + cnt, bufsiz - cnt, "\n");
	}

	cnt += snprintf(buf + cnt, bufsiz - cnt, "\n");
	return cnt;

}

void tsl2540_reg_log(struct tsl2540_chip *chip)
{
	char *buf;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (buf) {
		tsl2540_regs_get(chip, &buf[0], PAGE_SIZE);
		pr_err("%s", buf);
		kfree(buf);
	} else {
		dev_err(&chip->client->dev, "%s: out of memory!\n", __func__);
	}
}

static ssize_t tsl2540_regs_dump(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	return tsl2540_regs_get(dev_get_drvdata(dev), buf, PAGE_SIZE);
}

static ssize_t tsl2540_reg_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	int preg;
	int pval;
	int pmask = -1;
	int numparams;
	int rc;
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	numparams = sscanf(buf, "0x%x:0x%x:0x%x", &preg, &pval, &pmask);
	if (numparams == 0) {
		/* try decimal */
		numparams = sscanf(buf, "%d:%d:%d", &preg, &pval, &pmask);
	}

	if ((numparams < 2) || (numparams > 3))
		return -EINVAL;
	if ((numparams >= 1) && ((preg < 0) || ((reginuse[(preg >> 3)] &
			(1 << (preg & 7))) == 0)))
		return -EINVAL;
	if ((numparams >= 2) && (preg < 0 || preg > 0xff))
		return -EINVAL;
	if ((numparams >= 3) && (pmask < 0 || pmask > 0xff))
		return -EINVAL;

	AMS_MUTEX_LOCK(&chip->lock);

	if (pmask == -1) {
		rc = ams_i2c_write(chip->client, chip->shadow, preg, pval);
	} else {
		rc = ams_i2c_modify(chip->client, chip->shadow,
			preg, pmask, pval);
	}

	AMS_MUTEX_UNLOCK(&chip->lock);

	return rc ? rc : size;
}

static DEVICE_ATTR(regs, S_IWUSR | S_IRUGO, tsl2540_regs_dump, tsl2540_reg_set);

static struct attribute *tsl2540_attributes[] = {
	&dev_attr_regs.attr,
	NULL
};

static const struct attribute_group tsl2540_attr_group = {
	.attrs = tsl2540_attributes,
};

#endif /* #ifdef ABI_SET_GET_REGISTERS */


static int tsl2540_add_sysfs_interfaces(struct device *dev,
	struct device_attribute *a, int size)
{
	int i;

	for (i = 0; i < size; i++)
		if (device_create_file(dev, a + i))
			goto undo;
	return 0;
undo:
	for (; i >= 0 ; i--)
		device_remove_file(dev, a + i);
	dev_err(dev, "%s: failed to create sysfs interface\n", __func__);
	return -ENODEV;
}

static void tsl2540_remove_sysfs_interfaces(struct device *dev,
	struct device_attribute *a, int size)
{
	int i;

	for (i = 0; i < size; i++)
		device_remove_file(dev, a + i);
}

static int tsl2540_get_id(struct tsl2540_chip *chip, u8 *id, u8 *rev, u8 *auxid)
{
	ams_i2c_read(chip->client, TSL2540_REG_AUXID, auxid);
	ams_i2c_read(chip->client, TSL2540_REG_REVID, rev);
	ams_i2c_read(chip->client, TSL2540_REG_ID, id);

	return 0;
}

static int tsl2540_power_on(struct tsl2540_chip *chip)
{
	int rc;

	rc = tsl2540_pltf_power_on(chip);
	if (rc)
		return rc;
	dev_info(&chip->client->dev, "%s: chip was off, restoring regs\n",
			__func__);
	return tsl2540_flush_regs(chip);
}

#ifdef TSL2540_ENABLE_INPUT
static int tsl2540_als_idev_open(struct input_dev *idev)
{
	struct tsl2540_chip *chip = dev_get_drvdata(&idev->dev);
	bool als = chip->p_idev && chip->p_idev->users;
	int rc = 0;

	dev_info(&idev->dev, "%s\n", __func__);
	AMS_MUTEX_LOCK(&chip->lock);
	if (chip->unpowered) {
		rc = tsl2540_power_on(chip);
		if (rc)
			goto chip_on_err;
	}
	rc = tsl2540_configure_als_mode(chip, 1);
	if (rc && !als)
		tsl2540_pltf_power_off(chip);
chip_on_err:
	AMS_MUTEX_UNLOCK(&chip->lock);
	return 0;
}

static void tsl2540_als_idev_close(struct input_dev *idev)
{
	struct tsl2540_chip *chip = dev_get_drvdata(&idev->dev);

	dev_info(&idev->dev, "%s\n", __func__);

	AMS_MUTEX_LOCK(&chip->lock);
	tsl2540_configure_als_mode(chip, 0);
	if (!chip->p_idev || !chip->p_idev->users)
		tsl2540_pltf_power_off(chip);
	AMS_MUTEX_UNLOCK(&chip->lock);
}
#endif /* TSL2540_ENABLE_INPUT */

#ifdef CONFIG_OF
int tsl2540_init_dt(struct tsl2540_i2c_platform_data *pdata)
{
	struct device_node *np = pdata->of_node;
	const char *str;
	u32 val;

	if (!pdata->of_node)
		return 0;

	if (!of_property_read_string(np, "als_name", &str))
		pdata->als_name = str;

	if (!of_property_read_u32(np, "persist", &val))
		pdata->parameters.persist = val;

	if (!of_property_read_u32(np, "als_gain", &val))
		pdata->parameters.als_gain = val;

	if (!of_property_read_u32(np, "als_gain_factor", &val))
		pdata->parameters.als_gain_factor = val;

	if (!of_property_read_u32(np, "als_deltap", &val))
		pdata->parameters.als_deltap = val;

	if (!of_property_read_u32(np, "als_time", &val))
		pdata->parameters.als_time = val;

	if (!of_property_read_u32(np, "d_factor", &val))
		pdata->parameters.d_factor = val;

	if (!of_property_read_u32(np, "ch0_coef0", &val))
		pdata->parameters.lux_segment[0].ch0_coef = val;

	if (!of_property_read_u32(np, "ch1_coef0", &val))
		pdata->parameters.lux_segment[0].ch1_coef = val;

	if (!of_property_read_u32(np, "ch0_coef1", &val))
		pdata->parameters.lux_segment[1].ch0_coef = val;

	if (!of_property_read_u32(np, "ch1_coef1", &val))
		pdata->parameters.lux_segment[1].ch1_coef = val;

	if (!of_property_read_u32(np, "az_iterations", &val))
		pdata->parameters.az_iterations = val;

	if (!of_property_read_u32(np, "als_can_wake", &val))
		pdata->als_can_wake = (val == 0) ? false : true;

	pdata->boot_on = of_property_read_bool(np, "boot-on");

	return 0;
}
#endif

#ifdef CONFIG_OF
static struct of_device_id tsl2540_i2c_dt_ids[] = {
		{ .compatible = "amstaos,tsl2540" },
		{}
};
MODULE_DEVICE_TABLE(of, tsl2540_i2c_dt_ids);
#endif

#ifdef CONFIG_AMS_ADJUST_WITH_BASELINE
#define ALS_CAL_OF_PATH "/idme/alscal"
#define ALS_LUX_400 400
/*
»  Calibration format is
»    ams_<input lux 0>_0=<ch0_reading>,<actual_lux>,<ch1_reading><SP>
»    ams_<input lux 400>_0=<ch0_reading>,<actual_lux>,<ch1_reading>
*    where <SP> = single space character and all values are integers.
*    e.g. For calibration at input lux=0 and lux=400 is
*      ams_0_0=1,2.000,1 ams_400_0=210,238.667,30
*/
#define ALS_CAL_FORMAT "ams_0_0=%d,%d,%d ams_400_0=%d,%d"

static void tsl2540_get_calibration(struct tsl2540_chip *chip)
{
	struct device *dev = &chip->client->dev;
	struct tsl2540_i2c_platform_data *pdata = chip->pdata;
	struct device_node *ap = NULL;
	char *alscal_idme = NULL;
	char *ptr;
	int n, dummy, ch0, ch1, lux;

	ap = of_find_node_by_path(ALS_CAL_OF_PATH);
	if (ap)
		alscal_idme = (char *)of_get_property(ap, "value", NULL);
	else {
		pr_warn("ALSCAL: could not get alscal idme entry\n");
		goto failed;
	}

	/* search for signature andskip the leading spaces */
	ptr = strstr(alscal_idme, "ams_0_0");
	if (NULL == ptr) {
		pr_warn("ALSCAL: unable to find calibration\n");
		goto failed;
	}

	n = sscanf(ptr, ALS_CAL_FORMAT, &ch0, &dummy, &ch1, &dummy, &lux);

	if (n != 5) {
		pr_warn("ALSCAL: alscal format incorrect\n");
		goto failed;
	}

	pdata->lux0_ch0 = ch0;
	pdata->lux0_ch1 = ch1;
	pdata->lux400_lux = lux;
	pr_info("ALSCAL: alscal ch0=%d ch1=%d lux=%d\n", ch0, ch1, lux);
	return;
failed:
	/* calibration data is not available */
	pdata->lux0_ch0 = 0;
	pdata->lux0_ch1 = 0;
	pdata->lux400_lux = ALS_LUX_400;
	pr_info("ALSCAL: alscal default used - ch0=0 ch1=0 lux=400\n");
}
#endif

static void tsl2540_setup(struct tsl2540_chip *chip)
{
	struct tsl2540_i2c_platform_data *pdata = chip->pdata;

#ifdef CONFIG_AMS_ADJUST_WITH_BASELINE
	tsl2540_get_calibration(chip);
#endif

	if (pdata->boot_on)
		tsl2540_configure_als_mode(chip, 1);
}

static int tsl2540_probe(struct i2c_client *client,
	const struct i2c_device_id *idp)
{
	int i, ret;
	u8 id, rev, auxid;
	struct device *dev = &client->dev;
	static struct tsl2540_chip *chip;
	struct tsl2540_i2c_platform_data *pdata = dev->platform_data;
	bool powered = 0;

	pr_info("\nTSL2540: probe()\n");

#ifdef CONFIG_OF
	if (!pdata) {
		pdata = kzalloc(sizeof(struct tsl2540_i2c_platform_data),
				GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;

		if (of_match_device(tsl2540_i2c_dt_ids, &client->dev)) {
			pdata->of_node = client->dev.of_node;
			ret = tsl2540_init_dt(pdata);
			if (ret)
				return ret;
		}
	}
#endif

	/*
	 * Validate bus and device registration
	 */

	dev_info(dev, "%s: client->irq = %d\n", __func__, client->irq);
	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "%s: i2c smbus byte data unsupported\n", __func__);
		ret = -EOPNOTSUPP;
		goto init_failed;
	}
	if (!pdata) {
		dev_err(dev, "%s: platform data required\n", __func__);
		ret = -EINVAL;
		goto init_failed;
	}

	if (!(pdata->als_name) || client->irq < 0) {
		dev_err(dev, "%s: no reason to run.\n", __func__);
		ret = -EINVAL;
		goto init_failed;
	}

	if (pdata->platform_init) {
		ret = pdata->platform_init();
		if (ret)
			goto init_failed;
	}
	if (pdata->platform_power) {
		ret = pdata->platform_power(dev, POWER_ON);
		if (ret) {
			dev_err(dev, "%s: pltf power on failed\n", __func__);
			goto pon_failed;
		}
		powered = true;
		mdelay(10);
	}

	chip = kzalloc(sizeof(struct tsl2540_chip), GFP_KERNEL);
	if (!chip) {
		ret = -ENOMEM;
		goto malloc_failed;
	}

	mutex_init(&chip->lock);
	chip->client = client;
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);

	/*
	 * Validate the appropriate ams device is available for this driver
	 */

	ret = tsl2540_get_id(chip, &id, &rev, &auxid);

	dev_info(dev, "%s: device id:%02x device aux id:%02x device rev:%02x\n",
			__func__, id, auxid, rev);

	id &= 0xfc; /* clear the 2 LSbits, they indicate the bus voltage */
	rev &= 0xe7; /* clear all but fuse bits */
	for (i = 0; i < ARRAY_SIZE(tsl2540_ids)/3; i++) {
		if (id == (tsl2540_ids[i*3+0]))
			if (auxid == (tsl2540_ids[i*3+1]))
				if (rev == (tsl2540_ids[i*3+2]))
					break;
	}
	if (i < ARRAY_SIZE(tsl2540_names)) {
		dev_info(dev, "%s: '%s rev. 0x%x' detected\n", __func__,
			tsl2540_names[i], rev);
		chip->device_index = i;
	} else {
		dev_err(dev, "%s: not supported chip id\n", __func__);
		ret = -EOPNOTSUPP;
		goto id_failed;
	}

	/*
	 * Set chip defaults
	 */

	tsl2540_set_defaults(chip);
	ret = tsl2540_flush_regs(chip);
	if (ret)
		goto flush_regs_failed;
	if (pdata->platform_power) {
		pdata->platform_power(dev, POWER_OFF);
		powered = false;
		chip->unpowered = true;
	}

	/*
	 * Initialize ALS
	 */

	if (!pdata->als_name)
		goto bypass_als_idev;

#ifdef TSL2540_ENABLE_INPUT
	chip->a_idev = input_allocate_device();
	if (!chip->a_idev) {
		dev_err(dev, "%s: no memory for input_dev '%s'\n",
				__func__, pdata->als_name);
		ret = -ENODEV;
		goto input_a_alloc_failed;
	}
	chip->a_idev->name = pdata->als_name;
	chip->a_idev->id.bustype = BUS_I2C;
	set_bit(EV_ABS, chip->a_idev->evbit);
	set_bit(ABS_MISC, chip->a_idev->absbit);
	input_set_abs_params(chip->a_idev, ABS_MISC, 0, 65535, 0, 0);
	chip->a_idev->open = tsl2540_als_idev_open;
	chip->a_idev->close = tsl2540_als_idev_close;
	dev_set_drvdata(&chip->a_idev->dev, chip);
	ret = input_register_device(chip->a_idev);
	if (ret) {
		input_free_device(chip->a_idev);
		dev_err(dev, "%s: cant register input '%s'\n",
				__func__, pdata->als_name);
		goto input_a_alloc_failed;
	}
#endif /* TSL2540_ENABLE_INPUT */
bypass_als_idev:

#ifdef ABI_SET_GET_REGISTERS
#ifdef TSL2540_ENABLE_INPUT
	/*
	 * Allocate device
	 */
	chip->d_idev = input_allocate_device();
	if (!chip->d_idev) {
		dev_err(dev, "%s: no memory for input_dev '%s'\n",
				__func__, tsl2540_names[chip->device_index]);
		ret = -ENODEV;
		goto input_d_alloc_failed;
	}

	chip->d_idev->name = tsl2540_names[chip->device_index];
	chip->d_idev->id.bustype = BUS_I2C;
	set_bit(EV_ABS, chip->d_idev->evbit);
	set_bit(ABS_DISTANCE, chip->d_idev->absbit);
	input_set_abs_params(chip->d_idev, ABS_DISTANCE, 0, 1, 0, 0);
	dev_set_drvdata(&chip->d_idev->dev, chip);
	ret = input_register_device(chip->d_idev);
	if (ret) {
		input_free_device(chip->d_idev);
		dev_err(dev, "%s: cant register input '%s'\n",
				__func__, tsl2540_names[chip->device_index]);
		goto input_d_alloc_failed;
	}
#endif /* TSL2540_ENABLE_INPUT */
	ret = sysfs_create_group(&chip->client->dev.kobj, &tsl2540_attr_group);
	if (ret)
		goto input_d_alloc_failed;
#endif /* #ifdef ABI_SET_GET_REGISTERS */

	ret = sysfs_create_group(&chip->client->dev.kobj,
			&tsl2540_als_attr_group);
	if (ret)
		goto base_sysfs_failed;

#ifdef TSL2540_ENABLE_INTERRUPT
	/*
	 * Initialize IRQ & Handler
	 */

	ret = request_threaded_irq(client->irq, NULL, &tsl2540_irq,
		      IRQF_TRIGGER_FALLING|IRQF_SHARED|IRQF_ONESHOT,
		      dev_name(dev), chip);
	if (ret) {
		dev_info(dev, "Failed to request irq %d\n", client->irq);
		goto irq_register_fail;
	}
#endif /* TSL2540_ENABLE_INTERRUPT */

	/* Power up device */
	ams_i2c_write(chip->client, chip->shadow, TSL2540_REG_ENABLE, 0x01);

	tsl2540_update_enable_reg(chip);

	tsl2540_setup(chip);

	dev_info(dev, "Probe ok.\n");
	return 0;

	/*
	 * Exit points for device functional failures (ALS)
	 * This must be unwound in the correct order, reverse
	 * from initialization above
	 */
#ifdef ABI_SET_GET_REGISTERS
base_sysfs_failed:
	sysfs_remove_group(&chip->client->dev.kobj, &tsl2540_attr_group);
#endif /* ABI_SET_GET_REGISTERS */

irq_register_fail:
	sysfs_remove_group(&chip->client->dev.kobj, &tsl2540_als_attr_group);

#ifdef ABI_SET_GET_REGISTERS
#ifdef TSL2540_ENABLE_INPUT
	if (chip->d_idev)
		input_unregister_device(chip->d_idev);
#endif /* TSL2540_ENABLE_INPUT */

input_d_alloc_failed:
#endif /* ABI_SET_GET_REGISTERS */

#ifdef TSL2540_ENABLE_INPUT
	if (chip->a_idev)
		input_unregister_device(chip->a_idev);
input_a_alloc_failed:
	if (chip->p_idev)
		input_unregister_device(chip->p_idev);
#endif /* TSL2540_ENABLE_INPUT */

	/*
	 * Exit points for general device initialization failures
	 */

flush_regs_failed:
id_failed:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
malloc_failed:
	if (powered && pdata->platform_power)
		pdata->platform_power(dev, POWER_OFF);
pon_failed:
	if (pdata->platform_teardown)
		pdata->platform_teardown(dev);
init_failed:
	dev_err(dev, "Probe failed.\n");
	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int tsl2540_suspend(struct device *dev)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	pr_info("\nTSL2540: suspend()\n");
	dev_info(dev, "%s\n", __func__);
	AMS_MUTEX_LOCK(&chip->lock);
	chip->in_suspend = 1;

	if (chip->wake_irq) {
		irq_set_irq_wake(chip->client->irq, 1);
	} else if (!chip->unpowered) {
		dev_info(dev, "powering off\n");
		tsl2540_pltf_power_off(chip);
	}
	AMS_MUTEX_UNLOCK(&chip->lock);

	return 0;
}

static int tsl2540_resume(struct device *dev)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);
	bool als_on;

	return 0;
	pr_info("\nTSL2540: resume()\n");
	AMS_MUTEX_LOCK(&chip->lock);
	chip->in_suspend = 0;

	dev_info(dev, "%s: powerd %d, als: needed %d  enabled %d",
			__func__, !chip->unpowered, als_on,
			chip->als_enabled);

	if (chip->wake_irq) {
		irq_set_irq_wake(chip->client->irq, 0);
		chip->wake_irq = 0;
	}

/* err_power: */
	AMS_MUTEX_UNLOCK(&chip->lock);

	return 0;
}

static SIMPLE_DEV_PM_OPS(tsl2540_pm_ops, tsl2540_suspend, tsl2540_resume);
#define TSL2540_PM_OPS (&tsl2540_pm_ops)

#else

#define TSL2540_PM_OPS NULL

#endif /* CONFIG_PM_SLEEP */


static int tsl2540_remove(struct i2c_client *client)
{
	struct tsl2540_chip *chip = i2c_get_clientdata(client);

	pr_info("\nTSL2540: REMOVE()\n");
#ifdef TSL2540_ENABLE_INTERRUPT
	free_irq(client->irq, chip);
#endif /* TSL2540_ENABLE_INTERRUPT */
	if (chip->p_idev)
		input_unregister_device(chip->p_idev);
	if (chip->pdata->platform_teardown)
		chip->pdata->platform_teardown(&client->dev);
	i2c_set_clientdata(client, NULL);
#ifdef CONFIG_OF
	kfree(chip->pdata);
#endif
	kfree(chip);
	return 0;
}

static struct i2c_device_id tsl2540_idtable[] = {
	{ "tsl2540", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, tsl2540_idtable);

static struct i2c_driver tsl2540_driver = {
	.driver = {
		.name = "tsl2540",
		.pm = TSL2540_PM_OPS,
	},
	.id_table = tsl2540_idtable,
	.probe = tsl2540_probe,
	.remove = tsl2540_remove,
};

static int __init tsl2540_init(void)
{
	int rc;

	pr_info("\nTSL2540: INIT()\n");

	rc = i2c_add_driver(&tsl2540_driver);
	return rc;
}

static void __exit tsl2540_exit(void)
{
	pr_info("\nTSL2540: exit()\n");
	i2c_del_driver(&tsl2540_driver);
}

module_init(tsl2540_init);
module_exit(tsl2540_exit);

MODULE_DESCRIPTION("AMS-TAOS tsl2540 ALS sensor driver");
MODULE_LICENSE("GPL");
