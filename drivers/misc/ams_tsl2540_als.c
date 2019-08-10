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
* proximity detection (prox) functionality within the
* AMS-TAOS TSL2540 family of devices.
*/

#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#include <linux/i2c/ams_tsl2540.h>
#include "ams_i2c.h"

#undef LUX_DBG

#define GAIN1	0
#define GAIN4	1
#define GAIN16	2
#define GAIN64	3

static u8 const als_gains[] = {
	1,
	4,
	16,
	64
};

static u8 const restorable_als_regs[] = {
	TSL2540_REG_ATIME,
	TSL2540_REG_WTIME,
	TSL2540_REG_PERS,
	TSL2540_REG_CFG0,
	TSL2540_REG_CFG1,
	TSL2540_REG_CFG2,
	TSL2540_REG_AZ_CONFIG,
};

static int tsl2540_flush_als_regs(struct tsl2540_chip *chip)
{
	unsigned i;
	int rc;
	u8 reg;

	for (i = 0; i < ARRAY_SIZE(restorable_als_regs); i++) {
		reg = restorable_als_regs[i];
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

int tsl2540_read_als(struct tsl2540_chip *chip)
{
	int ret;

	ret = ams_i2c_blk_read(chip->client, TSL2540_REG_CH0DATA,
			&chip->shadow[TSL2540_REG_CH0DATA], 4 * sizeof(u8));

	if (ret >= 0) {
		chip->als_inf.als_ch0 = le16_to_cpu(
			*((const __le16 *) &chip->shadow[TSL2540_REG_CH0DATA]));
		chip->als_inf.als_ch1 = le16_to_cpu(
			*((const __le16 *) &chip->shadow[TSL2540_REG_CH1DATA]));
		ret = 0;
	}

	return ret;
}

static void tsl2540_calc_lux_coef(struct tsl2540_chip *chip)
{
	struct tsl2540_lux_segment *pls = &chip->params.lux_segment[0];

	chip->als_inf.lux1_ch0_coef = chip->params.d_factor * pls[0].ch0_coef;
	chip->als_inf.lux1_ch1_coef = chip->params.d_factor * pls[0].ch1_coef;
	chip->als_inf.lux2_ch0_coef = chip->params.d_factor * pls[1].ch0_coef;
	chip->als_inf.lux2_ch1_coef = chip->params.d_factor * pls[1].ch1_coef;
}

static void tsl2540_calc_cpl(struct tsl2540_chip *chip)
{
	u32 cpl;
	u32 sat;
	u8 atime;

	atime = chip->shadow[TSL2540_REG_ATIME];

	cpl = atime;
	cpl *= INTEGRATION_CYCLE;
	cpl *= als_gains[(chip->shadow[TSL2540_REG_CFG1] & TSL2540_MASK_AGAIN)];

	if (chip->shadow[TSL2540_REG_CFG2] & TSL2540_MASK_AGAINMAX)
		cpl = cpl << 1;
	else if (chip->shadow[TSL2540_REG_CFG2] & TSL2540_MASK_AGAINL)
		cpl = cpl >> 1;

	if (chip->params.d_factor > 0)
		cpl /= chip->params.d_factor;

	sat = min_t(u32, TSL2540_MAX_ALS_VALUE, (u32) atime << 10);
	sat = sat * 8 / 10;

	chip->als_inf.cpl = cpl * 1000;
	chip->als_inf.saturation = sat;
}

int tsl2540_configure_als_mode(struct tsl2540_chip *chip, u8 state)
{
	struct i2c_client *client = chip->client;
	u8 *sh = chip->shadow;

	 /* Turning on ALS */
	if (state) {
		tsl2540_calc_lux_coef(chip);

		chip->shadow[TSL2540_REG_ATIME] = chip->params.als_time;
		tsl2540_calc_cpl(chip);

		/* set PERS.apers to 2 consecutive ALS values out of range */
		chip->shadow[TSL2540_REG_PERS] &= (~TSL2540_MASK_APERS);
		chip->shadow[TSL2540_REG_PERS] |= 0x02;

		tsl2540_flush_als_regs(chip);

#ifdef TSL2540_ENABLE_INTERRUPT
		ams_i2c_modify(client, sh, TSL2540_REG_INTENAB,
				TSL2540_AIEN, TSL2540_AIEN);
#endif /* TSL2540_ENABLE_INTERRUPT */
		ams_i2c_modify(client, sh, TSL2540_REG_ENABLE,
				TSL2540_WEN | TSL2540_AEN | TSL2540_PON,
				TSL2540_WEN | TSL2540_AEN | TSL2540_PON);
		chip->als_enabled = true;
	} else {
		/* Disable ALS, Wait and ALS Interrupt */
#ifdef TSL2540_ENABLE_INTERRUPT
		ams_i2c_modify(client, sh, TSL2540_REG_INTENAB,
				TSL2540_AIEN, 0);
#endif /* TSL2540_ENABLE_INTERRUPT */
		ams_i2c_modify(client, sh, TSL2540_REG_ENABLE,
				TSL2540_WEN | TSL2540_AEN, 0);
		chip->als_enabled = false;

		/* If nothing else is enabled set PON = 0; */
		if (!(sh[TSL2540_REG_ENABLE] & TSL2540_EN_ALL))
			ams_i2c_modify(client, sh, TSL2540_REG_ENABLE,
			TSL2540_PON, 0);
	}

	return 0;
}

static int tsl2540_set_als_gain(struct tsl2540_chip *chip, int gain)
{
	int rc;
	u8 ctrl_reg;
	u8 saved_enable;

	switch (gain) {
	case 1:
		ctrl_reg = AGAIN_1;
		break;
	case 4:
		ctrl_reg = AGAIN_4;
		break;
	case 16:
		ctrl_reg = AGAIN_16;
		break;
	case 64:
		ctrl_reg = AGAIN_64;
		break;
	default:
		dev_err(&chip->client->dev, "%s: wrong als gain %d\n",
				__func__, gain);
		return -EINVAL;
	}

	/*
	 * Turn off ALS, so that new ALS gain value will take
	 * effect at start of new integration cycle.
	 * New ALS gain value will then be used in next lux calculation.
	 */
	ams_i2c_read(chip->client, TSL2540_REG_ENABLE, &saved_enable);
	ams_i2c_write(chip->client, chip->shadow, TSL2540_REG_ENABLE, 0);
	rc = ams_i2c_modify(chip->client, chip->shadow, TSL2540_REG_CFG1,
			TSL2540_MASK_AGAIN, ctrl_reg);
	ams_i2c_write(chip->client, chip->shadow, TSL2540_REG_ENABLE,
			saved_enable);

	if (rc >= 0) {
		chip->params.als_gain = chip->shadow[TSL2540_REG_CFG1];
		dev_info(&chip->client->dev, "%s: new als gain %d\n",
				__func__, ctrl_reg);
	}

	return rc;
}

static void tsl2540_inc_gain(struct tsl2540_chip *chip)
{
	int rc;
	u8 gain = (chip->shadow[TSL2540_REG_CFG1] & TSL2540_MASK_AGAIN);

	if (gain > GAIN16)
		return;
	else if (gain < GAIN4)
		gain = als_gains[GAIN4];
	else if (gain < GAIN16)
		gain = als_gains[GAIN16];
	else
		gain = als_gains[GAIN64];

	rc = tsl2540_set_als_gain(chip, gain);
	if (rc == 0)
		tsl2540_calc_cpl(chip);
}

static void tsl2540_dec_gain(struct tsl2540_chip *chip)
{
	int rc;
	u8 gain = (chip->shadow[TSL2540_REG_CFG1] & 0x03);

	if (gain == GAIN1)
		return;
	else if (gain > GAIN16)
		gain = als_gains[GAIN16];
	else if (gain > GAIN4)
		gain = als_gains[GAIN4];
	else
		gain = als_gains[GAIN1];

	rc = tsl2540_set_als_gain(chip, gain);
	if (rc == 0)
		tsl2540_calc_cpl(chip);
}

static int tsl2540_max_als_value(struct tsl2540_chip *chip)
{
	int val;

	val = chip->shadow[TSL2540_REG_ATIME];
	if (val > 63)
		val = 0xffff;
	else
		val = ((val * 1024) - 1);
	return val;
}

int tsl2540_get_lux(struct tsl2540_chip *chip)
{
	long ch0;
	long ch1;
	long coef_b;
	long coef_c;
	long coef_d;
	int lux1;
	int lux2;
	int lux;
#ifdef CONFIG_AMS_ADJUST_WITH_BASELINE
	struct tsl2540_i2c_platform_data *pdata = chip->pdata;
#endif

	ch0 = chip->als_inf.als_ch0;
	ch1 = chip->als_inf.als_ch1;
	coef_b = chip->params.lux_segment[0].ch1_coef;
	coef_c = chip->params.lux_segment[1].ch0_coef;
	coef_d = chip->params.lux_segment[1].ch1_coef;

#ifdef CONFIG_AMS_ADJUST_WITH_BASELINE
	ch0 -= pdata->lux0_ch0;
	if (ch0 < 0)
		ch0 = 0;
	ch1 -= pdata->lux0_ch1;
	if (ch1 < 0)
		ch1 = 0;
#endif
	/*
	 * Lux1 = (ch0 - (coef_b * ch1)) / CPL
	 * Lux2 = ((coef_c * ch0) - (coef_d * ch1)) / CPL
	 * Lux = Max(Lux1,Lux2,0)
	 *
	 * where for default NO GLASS:
	 *    coef_a = 1
	 *    coef_b = .26
	 *    coef_c = .8
	 *    coef_d = .27
	 */

	lux1 = 1000 * ((1000 * ch0 - (coef_b * ch1))) /
		(long)chip->als_inf.cpl;
	lux2 = 1000 * ((coef_c * ch0 - (coef_d * ch1))) /
		(long)chip->als_inf.cpl;
	lux = max(lux1, lux2);
	lux = min(TSL2540_MAX_LUX, max(0, lux));

#ifdef LUX_DBG
	dev_info(&chip->client->dev,
		"%s: lux:%d [%d, %d, %d, %d] %d [%d %d] [%d %d] %d [%d, ((%d*%d) / 1000)] (sat:%d)\n",
		__func__, lux,
		ch0, ch1, lux1, lux2,
		chip->params.d_factor,
		chip->params.lux_segment[0].ch0_coef,
		chip->params.lux_segment[0].ch1_coef,
		chip->params.lux_segment[1].ch0_coef,
		chip->params.lux_segment[1].ch1_coef,
		chip->als_inf.cpl,
		chip->shadow[TSL2540_REG_ATIME],
		als_gains[(chip->params.als_gain & TSL2540_MASK_AGAIN)],
			INTEGRATION_CYCLE, chip->als_inf.saturation);
#endif /* #ifdef LUX_DBG */

	if (lux < 0) {
		dev_info(&chip->client->dev,
				"%s: lux < 0 use prev.\n", __func__);
		return chip->als_inf.lux; /* use previous value */
	}

	chip->als_inf.lux = lux;

	if (!chip->als_gain_auto) {
		if (ch0 <= TSL2540_MIN_ALS_VALUE) {
			dev_info(&chip->client->dev, "%s: darkness (%d <= %d)\n",
				__func__, ch0, TSL2540_MIN_ALS_VALUE);
		} else if (ch0 >= chip->als_inf.saturation) {
			dev_info(&chip->client->dev, "%s: saturation (%d >= %d\n",
				__func__, ch0, chip->als_inf.saturation);
		}
	} else {
		if (ch0 < 100) {
			dev_info(&chip->client->dev,
					"%s: AUTOGAIN INC\n", __func__);
			tsl2540_inc_gain(chip);
			tsl2540_flush_als_regs(chip);
		} else if (ch0 > tsl2540_max_als_value(chip)) {
			dev_info(&chip->client->dev,
					"%s: AUTOGAIN DEC\n", __func__);
			tsl2540_dec_gain(chip);
			tsl2540_flush_als_regs(chip);
		}
	}

	return 0;
}

int tsl2540_update_als_thres(struct tsl2540_chip *chip, bool on_enable)
{
	s32 ret;
	u16 deltap = chip->params.als_deltap;
	u16 from, to, cur;
	u16 saturation = chip->als_inf.saturation;

	cur = chip->als_inf.als_ch0;

	if (on_enable) {
		/* move deltap far away from current position to force an irq */
		from = to = cur > (saturation / 2) ? 0 : saturation;
	} else {
		deltap = cur * deltap / 100;
		if (!deltap)
			deltap = 1;

		if (cur > deltap)
			from = cur - deltap;
		else
			from = 0;

		if (cur < (saturation - deltap))
			to = cur + deltap;
		else
			to = saturation;
	}

	*((__le16 *) &chip->shadow[TSL2540_REG_AILT]) = cpu_to_le16(from);
	*((__le16 *) &chip->shadow[TSL2540_REG_AIHT]) = cpu_to_le16(to);

	dev_info(&chip->client->dev,
			"%s: low:0x%x  hi:0x%x, oe:%d cur:%d deltap:%d (%d) sat:%d\n",
			__func__, from, to, on_enable, cur, deltap,
			chip->params.als_deltap, saturation);

	ret = ams_i2c_reg_blk_write(chip->client, TSL2540_REG_AILT,
			&chip->shadow[TSL2540_REG_AILT],
			(TSL2540_REG_AIHT_HI - TSL2540_REG_AILT) + 1);

	return (ret < 0) ? ret : 0;
}

void tsl2540_report_als(struct tsl2540_chip *chip)
{
	int lux;
	int rc;

	if (chip->a_idev) {
		rc = tsl2540_get_lux(chip);
		if (!rc) {
			lux = chip->als_inf.lux;
			input_report_abs(chip->a_idev, ABS_MISC, lux);
			input_sync(chip->a_idev);
			tsl2540_update_als_thres(chip, 0);
		} else {
			tsl2540_update_als_thres(chip, 1);
		}
	}
}

/*
 * ABI Functions
 */

static ssize_t tsl2540_device_als_lux(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	AMS_MUTEX_LOCK(&chip->lock);

	tsl2540_read_als(chip);
	tsl2540_get_lux(chip);

	AMS_MUTEX_UNLOCK(&chip->lock);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.lux);
}

static DEVICE_ATTR(als_lux, S_IRUGO, tsl2540_device_als_lux, NULL);

static ssize_t tsl2540_lux_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);
	int k;

	AMS_MUTEX_LOCK(&chip->lock);

	k = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d\n",
		chip->params.d_factor,
		chip->params.lux_segment[0].ch0_coef,
		chip->params.lux_segment[0].ch1_coef,
		chip->params.lux_segment[1].ch0_coef,
		chip->params.lux_segment[1].ch1_coef);

	AMS_MUTEX_UNLOCK(&chip->lock);

	return k;
}

static ssize_t tsl2540_lux_table_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);
	u32 d_factor, ch0_coef1, ch1_coef1, ch0_coef2, ch1_coef2;

	if (5 != sscanf(buf, "%10d,%10d,%10d,%10d,%10d", &d_factor,
			&ch0_coef1, &ch1_coef1, &ch0_coef2, &ch1_coef2))
		return -EINVAL;

	AMS_MUTEX_LOCK(&chip->lock);

	chip->params.d_factor = d_factor;
	chip->params.lux_segment[0].ch0_coef = ch0_coef1;
	chip->params.lux_segment[0].ch1_coef = ch1_coef1;
	chip->params.lux_segment[1].ch0_coef = ch0_coef2;
	chip->params.lux_segment[1].ch1_coef = ch1_coef2;

	tsl2540_calc_lux_coef(chip);

	AMS_MUTEX_UNLOCK(&chip->lock);
	return size;
}

static DEVICE_ATTR(als_lux_table, S_IWUSR | S_IRUGO, tsl2540_lux_table_show,
			tsl2540_lux_table_store);

static ssize_t tsl2540_als_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_enabled);
}

static ssize_t tsl2540_als_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);
	bool value;

	if (strtobool(buf, &value))
		return -EINVAL;

	if (value)
		tsl2540_configure_als_mode(chip, 1);
	else
		tsl2540_configure_als_mode(chip, 0);

	return size;
}

static DEVICE_ATTR(als_power_state, S_IWUSR | S_IRUGO, tsl2540_als_enable_show,
			tsl2540_als_enable_store);

static ssize_t tsl2540_auto_gain_enable_show(
		struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			chip->als_gain_auto ? "auto" : "manual");
}

static ssize_t tsl2540_auto_gain_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);
	bool value;

	if (strtobool(buf, &value))
		return -EINVAL;

	if (value)
		chip->als_gain_auto = true;
	else
		chip->als_gain_auto = false;

	return size;
}

static DEVICE_ATTR(als_auto_gain, S_IWUSR | S_IRUGO,
		tsl2540_auto_gain_enable_show, tsl2540_auto_gain_enable_store);

static ssize_t tsl2540_als_gain_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d (%s)\n",
			als_gains[(chip->params.als_gain & TSL2540_MASK_AGAIN)],
			chip->als_gain_auto ? "auto" : "manual");
}

static ssize_t tsl2540_als_gain_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long gain;
	int i = 0;
	int rc;
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	rc = kstrtoul(buf, 10, &gain);

	if (rc)
		return -EINVAL;
	if (gain != 0 && gain != 1 && gain != 4 && gain != 16 &&
			gain != 60 && gain != 64)
		return -EINVAL;

	while (i < sizeof(als_gains)) {
		if (gain == als_gains[i])
			break;
		i++;
	}

	if (i > 3) {
		dev_err(&chip->client->dev, "%s: wrong als gain %d\n",
				__func__, (int)gain);
		return -EINVAL;
	}

	AMS_MUTEX_LOCK(&chip->lock);

	if (gain) {
		chip->als_gain_auto = false;
		rc = tsl2540_set_als_gain(chip, als_gains[i]);
		if (!rc)
			tsl2540_calc_cpl(chip);
	} else {
		chip->als_gain_auto = true;
	}
	tsl2540_flush_als_regs(chip);

	AMS_MUTEX_UNLOCK(&chip->lock);

	return rc ? -EIO : size;
}

static DEVICE_ATTR(als_gain, S_IWUSR | S_IRUGO, tsl2540_als_gain_show,
			tsl2540_als_gain_store);

static ssize_t tsl2540_als_cpl_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.cpl);
}

static DEVICE_ATTR(als_cpl, S_IRUGO, tsl2540_als_cpl_show, NULL);

static ssize_t tsl2540_als_persist_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			(((chip->shadow[TSL2540_REG_PERS]) &
					TSL2540_MASK_APERS)));
}

static ssize_t tsl2540_als_persist_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	long persist;
	int rc;
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	rc = kstrtoul(buf, 10, &persist);
	if (rc)
		return -EINVAL;

	AMS_MUTEX_LOCK(&chip->lock);
	chip->shadow[TSL2540_REG_PERS] &= ~TSL2540_MASK_APERS;
	chip->shadow[TSL2540_REG_PERS] |= ((u8)persist & TSL2540_MASK_APERS);

	tsl2540_flush_als_regs(chip);

	AMS_MUTEX_UNLOCK(&chip->lock);
	return size;
}

static DEVICE_ATTR(als_persist, S_IWUSR | S_IRUGO, tsl2540_als_persist_show,
			tsl2540_als_persist_store);

static ssize_t tsl2540_als_itime_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);
	int t;

	t = chip->shadow[TSL2540_REG_ATIME];
	t *= INTEGRATION_CYCLE;
	return snprintf(buf, PAGE_SIZE, "%dms (%dus)\n", t / 1000, t);
}

static ssize_t tsl2540_als_itime_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	long itime;
	int rc;
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	rc = kstrtoul(buf, 10, &itime);
	if (rc)
		return -EINVAL;
	itime *= 1000;
	itime /= INTEGRATION_CYCLE;

	AMS_MUTEX_LOCK(&chip->lock);

	chip->shadow[TSL2540_REG_ATIME] = (u8) itime;
	chip->params.als_time = chip->shadow[TSL2540_REG_ATIME];
	tsl2540_calc_cpl(chip);
	tsl2540_flush_als_regs(chip);

	AMS_MUTEX_UNLOCK(&chip->lock);

	return size;
}

static DEVICE_ATTR(als_itime, S_IWUSR | S_IRUGO, tsl2540_als_itime_show,
			tsl2540_als_itime_store);

static ssize_t tsl2540_als_wtime_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int t;
	u8 wlongcurr;
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	AMS_MUTEX_LOCK(&chip->lock);

	t = chip->shadow[TSL2540_REG_WTIME];

	wlongcurr = chip->shadow[TSL2540_REG_CFG0] & TSL2540_MASK_WLONG;
	if (wlongcurr)
		t *= 12;

	t *= INTEGRATION_CYCLE;
	t /= 1000;

	AMS_MUTEX_UNLOCK(&chip->lock);

	return snprintf(buf, PAGE_SIZE, "%d (in ms)\n", t);
}

static ssize_t tsl2540_als_wtime_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);
	unsigned long wtime;
	int wlong;
	int rc;

	rc = kstrtoul(buf, 10, &wtime);
	if (rc)
		return -EINVAL;

	wtime *= 1000;
	if (wtime > (256 * INTEGRATION_CYCLE)) {
		wlong = 1;
		wtime /= 12;
	} else {
		wlong = 0;
	}
	wtime /= INTEGRATION_CYCLE;

	AMS_MUTEX_LOCK(&chip->lock);

	chip->shadow[TSL2540_REG_WTIME] = (u8) wtime;
	if (wlong)
		chip->shadow[TSL2540_REG_CFG0] |= TSL2540_MASK_WLONG;
	else
		chip->shadow[TSL2540_REG_CFG0] &= ~TSL2540_MASK_WLONG;

	tsl2540_flush_als_regs(chip);

	AMS_MUTEX_UNLOCK(&chip->lock);
	return size;
}

static DEVICE_ATTR(als_wtime, S_IWUSR | S_IRUGO, tsl2540_als_wtime_show,
			tsl2540_als_wtime_store);

static ssize_t tsl2540_als_deltap_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE,
			"%d (in %%)\n", chip->params.als_deltap);
}

static ssize_t tsl2540_als_deltap_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long deltap;
	int rc;
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	rc = kstrtoul(buf, 10, &deltap);
	if (rc || deltap > 100)
		return -EINVAL;
	AMS_MUTEX_LOCK(&chip->lock);
	chip->params.als_deltap = deltap;
	AMS_MUTEX_UNLOCK(&chip->lock);
	return size;
}

static DEVICE_ATTR(als_thresh_deltap, S_IWUSR | S_IRUGO,
			tsl2540_als_deltap_show, tsl2540_als_deltap_store);

static ssize_t tsl2540_als_ch0_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	tsl2540_get_lux(chip);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.als_ch0);
}

static DEVICE_ATTR(als_ch0, S_IRUGO, tsl2540_als_ch0_show, NULL);

static ssize_t tsl2540_als_ch1_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	tsl2540_get_lux(chip);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.als_ch1);
}

static DEVICE_ATTR(als_ch1, S_IRUGO, tsl2540_als_ch1_show, NULL);

static ssize_t tsl2540_als_az_iterations_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);
	int iterations;

	iterations = chip->shadow[TSL2540_REG_AZ_CONFIG];
	return snprintf(buf, PAGE_SIZE, "%d\n", iterations);
}

static ssize_t tsl2540_als_az_iterations_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	long iterations;
	int rc;
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	rc = kstrtoul(buf, 10, &iterations);
	if (rc)
		return -EINVAL;

	AMS_MUTEX_LOCK(&chip->lock);

	chip->shadow[TSL2540_REG_AZ_CONFIG] = (u8) iterations;
	chip->params.az_iterations = chip->shadow[TSL2540_REG_AZ_CONFIG];
	tsl2540_flush_als_regs(chip);

	AMS_MUTEX_UNLOCK(&chip->lock);

	return size;
}

static DEVICE_ATTR(als_az_iterations, S_IWUSR | S_IRUGO,
		tsl2540_als_az_iterations_show, tsl2540_als_az_iterations_store);

#ifdef LUX_DBG
static ssize_t tsl2540_als_adc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);

	tsl2540_get_lux(chip);

	return snprintf(buf, PAGE_SIZE,
			"LUX: %d CH0: %d CH1:%d\n", chip->als_inf.lux,
			chip->als_inf.als_ch0, chip->als_inf.als_ch1);
}

static ssize_t tsl2540_als_adc_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);
	u32 ch0, ch1;

	if (2 != sscanf(buf, "%10d,%10d", &ch0, &ch1))
		return -EINVAL;

	AMS_MUTEX_LOCK(&chip->lock);

	chip->als_inf.als_ch0 = ch0;
	chip->als_inf.als_ch1 = ch1;

	AMS_MUTEX_UNLOCK(&chip->lock);
	return size;
}

static DEVICE_ATTR(als_adc, S_IWUSR | S_IRUGO, tsl2540_als_adc_show,
			tsl2540_als_adc_store);
#endif /* #ifdef LUX_DBG */

#ifdef CONFIG_AMZN_AMS_ALS
#define ALS_MAX_LUX 400
#define ALS_MIN_LUX 0

static ssize_t als_calibrated_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2540_chip *chip = dev_get_drvdata(dev);
	struct tsl2540_i2c_platform_data *pdata = chip->pdata;
	int coeff = pdata->lux400_lux;
	int lux = 0;
	int calibrated_lux = 0;

	AMS_MUTEX_LOCK(&chip->lock);

	tsl2540_read_als(chip);
	tsl2540_get_lux(chip);

	AMS_MUTEX_UNLOCK(&chip->lock);

	lux = chip->als_inf.lux;

	calibrated_lux = ALS_MAX_LUX * lux / coeff;

	if (calibrated_lux > ALS_MAX_LUX)
		calibrated_lux = ALS_MAX_LUX;

	if (calibrated_lux < ALS_MIN_LUX)
		calibrated_lux = ALS_MIN_LUX;
	return snprintf(buf, PAGE_SIZE, "%d\n", calibrated_lux);
}

static DEVICE_ATTR(als_calibrated_lux, S_IRUGO, als_calibrated_lux_show, NULL);
#endif

static struct attribute *tsl2540_als_attributes[] = {
	&dev_attr_als_itime.attr,
	&dev_attr_als_wtime.attr,
	&dev_attr_als_lux.attr,
	&dev_attr_als_gain.attr,
	&dev_attr_als_az_iterations.attr,
	&dev_attr_als_cpl.attr,
	&dev_attr_als_thresh_deltap.attr,
	&dev_attr_als_auto_gain.attr,
	&dev_attr_als_lux_table.attr,
	&dev_attr_als_power_state.attr,
	&dev_attr_als_persist.attr,
	&dev_attr_als_ch0.attr,
	&dev_attr_als_ch1.attr,
#ifdef CONFIG_AMZN_AMS_ALS
	&dev_attr_als_calibrated_lux.attr,
#endif
#ifdef LUX_DBG
	&dev_attr_als_adc.attr,
#endif /* #ifdef LUX_DBG */
	NULL
};

const struct attribute_group tsl2540_als_attr_group = {
	.attrs = tsl2540_als_attributes,
};
