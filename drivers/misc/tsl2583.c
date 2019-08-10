/*
 * Device driver for monitoring ambient light intensity (lux)
 * within the TAOS tsl258x family of devices (tsl2580, tsl2581).
 *
 * Copyright (c) 2011, TAOS Corporation.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA	02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/iio/iio.h>

#define ID_UNKNOWN		0
#define ID_TSL2580		1 /* the TSL2580 is SMBus version of TSL2581 */
#define ID_TSL2581		2
#define ID_TSL2583		3
#define ID_TSL2584TSV		4

#define TSL258X_MAX_DEVICE_REGS		32

/* Triton register offsets */
#define	TSL258X_REG_MAX		8

/* Device Registers and Masks */
#define TSL258X_CNTRL			0x00
#define TSL258X_ALS_TIME		0X01
#define TSL258X_INTERRUPT		0x02
#define TSL258X_GAIN			0x07
#define TSL258X_STEP2			0x0a
#define TSL258X_STEP1			0x0b
#define TSL258X_AUTOZERO		0x0e
#define TSL258X_REVID			0x11
#define TSL258X_CHIPID			0x12
#define TSL258X_ALS_CHAN0LO		0x14
#define TSL258X_ALS_CHAN0HI		0x15
#define TSL258X_ALS_CHAN1LO		0x16
#define TSL258X_ALS_CHAN1HI		0x17
#define TSL258X_TMR_LO			0x18
#define TSL258X_TMR_HI			0x19
#define TSL258X_AZ_OFFSET_A		0x1a
#define TSL258X_ID2			0x1e

/* tsl258x cmd reg masks */
#define TSL258X_CMD_REG			0x80
#define TSL258X_CMD_SPL_FN		0x60
#define TSL258X_CMD_ALS_INT_CLR	0X01

/* tsl258x cntrl reg masks */
#define TSL258X_CNTL_ADC_ENBL	0x02
#define TSL258X_CNTL_PWR_ON		0x01

/* tsl258x status reg masks */
#define TSL258X_STA_ADC_VALID	0x01
#define TSL258X_STA_ADC_INTR	0x10

#define TSL258X_AUTOZERO_WAIT_MS	10
#define TSL258X_AUTOZERO_DEFAULT_PERIOD_MS	300000

/* Lux calculation constants */
#define	TSL258X_LUX_CALC_OVER_FLOW		65535

/* constants for lux calculations */
#define MPOW                    13
#define MFACT                   (1 << MPOW)

 /*start TSL2584TSV lux equation defines*/
 /*The lux equation is of the form:*/
 /*coff_1 * ((coff_ch0 * ch0) - (coff_ch1 * ch1)) / (atime * again)*/

 /*which can be rearranged as:*/

 /*((coff_1 * coff_ch0 * ch0) - (coff_1 * coff_ch1 * ch1)) / (atime * again)*/

 /*In order to maintain precision using integers, we multiply numerator and denominator by a power of 2*/

 /*(((coff_1 * coff_ch0 * (2 ^ MPOW)) * ch0) - ((coff_1 * coff_ch1 * (2 ^ MPOW)) * ch1)) / (atime * again * (2 ^ MPOW))*/

 /*which is the same as:*/

 /*(((coff_1 * coff_ch0 * (2 ^ MPOW)) * ch0) - ((coff_1 * coff_ch1 * (2 ^ MPOW)) * ch1)) / ((atime * again) << MPOW))*/

 /*since atime * again is an integer*/


 /*No glass Coefficients*/
#define COFF_1_NO_GLASS         105
#define COFF_CH0_NO_GLASS       (u32)(1    * MFACT)
#define COFF_CH1_NO_GLASS       (u32)(1.13 * MFACT)
#define CH0_COFF_NO_GLASS       (u32)((COFF_1_NO_GLASS * COFF_CH0_NO_GLASS))
#define CH1_COFF_NO_GLASS       (u32)((COFF_1_NO_GLASS * COFF_CH1_NO_GLASS))

 /*Add new glass coefficients here*/

 /*set the coefficients the TSL2584TSV equation will use*/
#define TSL2584TSV_CH0_COFF	CH0_COFF_NO_GLASS
#define TSL2584TSV_CH1_COFF	CH1_COFF_NO_GLASS
#ifdef CONFIG_abc123
#define MILILUX_COFF 235
#define CH1_COFF 295
#else /* All other products */
#define MILILUX_COFF 1000
#define CH1_COFF 1190
#endif /* CONFIG_abc123 */
#define NOISE_THRESHOLD 6
/*end TSL2584TSV lux equation defines*/

enum {
	TSL258X_CHIP_UNKNOWN = 0,
	TSL258X_CHIP_WORKING = 1,
	TSL258X_CHIP_SUSPENDED = 2
};

/* Per-device data */
struct taos_als_info {
	u16 als_ch0;
	u16 als_ch1;
	u16 lux;
};

struct taos_settings {
	int als_time;
	int als_gain;
	int als_gain_trim;
	int als_cal_target;
};

struct tsl258x_chip {
	struct mutex als_mutex;
	struct i2c_client *client;
	struct taos_als_info als_cur_info;
	struct taos_settings taos_settings;
	int als_time_scale;
	int als_saturation;
	int taos_chip_status;
	u8 taos_config[8];
	int id;
#ifdef CONFIG_ALS_TSL2584_AUTOZERO
	u32 autozero_period_ms;
	struct delayed_work autozero_work;
#endif
};

/*
 * Initial values for device - this values can/will be changed by driver.
 * and applications as needed.
 * These values are dynamic.
 */
static const u8 taos_config[8] = {
		0x00, 0xee, 0x00, 0x03, 0x00, 0xFF, 0xFF, 0x00
}; /*	cntrl atime intC  Athl0 Athl1 Athh0 Athh1 gain */

struct taos_lux {
	unsigned int ratio;
	unsigned int ch0;
	unsigned int ch1;
};

/* This structure is intentionally large to accommodate updates via sysfs. */
/* Sized to 11 = max 10 segments + 1 termination segment */
/* Assumption is is one and only one type of glass used  */
static struct taos_lux taos_device_lux[11];

 /*tsl2581, tsl2583*/
static struct taos_lux taos_device_lux_tsl258x[] = {
	{  9830,  8520, 15729 },
	{ 12452, 10807, 23344 },
	{ 14746,  6383, 11705 },
	{ 17695,  4063,  6554 }
};

 /*tsl2585tsv*/
static struct taos_lux taos_device_lux_tsl2584tsv[] = {
	{ 0, CH0_COFF_NO_GLASS, CH1_COFF_NO_GLASS }
};

struct gainadj {
	s16 ch0;
	s16 ch1;
};

/* Index = (0 - 3) Used to validate the gain selection index */
static const struct gainadj gainadj[] = {
	{ 1, 1 },
	{ 8, 8 },
	{ 16, 16 },
	{ 107, 115 }
};

#ifdef CONFIG_ALS_TSL2584_AUTOZERO
struct autozero_reg_config {
	u8 reg_address;
	u8 value;
};

static const struct autozero_reg_config autozero_regs[] = {
	{ TSL258X_CNTRL, TSL258X_CNTL_PWR_ON },
	{ TSL258X_ALS_TIME, 0xFF },
	{ TSL258X_GAIN, 0x03 },
	{ TSL258X_CNTRL, TSL258X_CNTL_PWR_ON | TSL258X_CNTL_ADC_ENBL },
	{ TSL258X_STEP1, 0x96 },
	{ TSL258X_STEP2, 0x20 },
	{ TSL258X_AUTOZERO, 0x30 }
};

static void perform_autozero(struct tsl258x_chip *chip);
#endif

static char *tsl2583x_get_name(struct tsl258x_chip *chip);

/*
 * Provides initial operational parameter defaults.
 * These defaults may be changed through the device's sysfs files.
 */
static void taos_defaults(struct tsl258x_chip *chip)
{
	/* Operational parameters */
	chip->taos_settings.als_time = 688;
	/* must be a multiple of 50mS */
	chip->taos_settings.als_gain = 3; /* MAX gain */
	/* this is actually an index into the gain table */
	/* assume clear glass as default */
	chip->taos_settings.als_gain_trim = 1000;
	/* default gain trim to account for aperture effects */
	chip->taos_settings.als_cal_target = 130;

	/* populate the default lux table */
	if (chip->id == ID_TSL2584TSV) {
		memcpy(&taos_device_lux[0],
			&taos_device_lux_tsl2584tsv[0],
			sizeof(taos_device_lux_tsl2584tsv));
	} else {
		memcpy(&taos_device_lux[0],
			&taos_device_lux_tsl258x[0],
			sizeof(taos_device_lux_tsl258x));
	}

	/* Known external ALS reading used for calibration */
}

/*
 * Read a number of bytes starting at register (reg) location.
 * Return 0, or i2c_smbus_write_byte ERROR code.
 */
static int
taos_i2c_read(struct i2c_client *client, u8 reg, u8 *val, unsigned int len)
{
	int i, ret;

	for (i = 0; i < len; i++) {
		/* select register to write */
		ret = i2c_smbus_write_byte(client, (TSL258X_CMD_REG | reg));
		if (ret < 0) {
			dev_err(&client->dev, "taos_i2c_read failed to write"
				" register %x\n", reg);
			return ret;
		}
		/* read the data */
		*val = i2c_smbus_read_byte(client);
		val++;
		reg++;
	}
	return 0;
}

#ifdef CONFIG_AMS_ADJUST_WITH_BASELINE
#define ALS_CAL_OF_PATH "/idme/alscal"

static int get_baseline(int *ch0_baseline, int *ch1_baseline)
{
	struct device_node *ap = NULL;
	char *alscal_idme = NULL;
	char *alscal = NULL;
	char *ptr = NULL;

	if (NULL == ch0_baseline || NULL == ch1_baseline) {
		pr_err("ALSCAL: invalid argument to get_baseline\n");
		return -1;
	}

	ap = of_find_node_by_path(ALS_CAL_OF_PATH);
	if (ap) {
		alscal_idme = (char *)of_get_property(ap, "value", NULL);
	} else {
		pr_err("ALSCAL: could not get alscal idme entry\n");
		return -1;
	}

	alscal = kstrdup(alscal_idme, GFP_KERNEL);

	/*
	 * Calibration format is
	 * ams_<expected lux>_0=<ch0_reading>,<actual_lux>,<ch1_reading> ..
	 * expected lux for the baseline is 0, so search for ams_0_0
	 */
	ptr = strstr(alscal, "ams_0_0");

	if (NULL == ptr) {
		pr_err("ALSCAL: unable to find baseline\n");
		return -1;
	}

	/*
	 * ch0_reading is before the first comma
	 * ch1_reading is after the second comma
	 */
	strsep(&ptr, "=");
	if (NULL == ptr) {
		pr_err("ALSCAL: alscal format incorrect\n");
		goto fail;
	}

	if (sscanf(ptr, "%d", ch0_baseline) != 1) {
		pr_err("ALSCAL: unable to parse ch0 baseline from idme string\n", ptr);
		goto fail;
	}

	strsep(&ptr, ",");
	if (NULL == ptr) {
		pr_err("ALSCAL: alscal format incorrect\n");
		goto fail;
	}

	strsep(&ptr, ",");
	if (NULL == ptr) {
		pr_err("ALSCAL: alscal format incorrect\n");
		goto fail;
	}

	if (sscanf(ptr, "%d", ch1_baseline) != 1) {
		pr_err("ALSCAL: unable to parse ch1 baseline from idme string\n");
		goto fail;
	}

	kfree(alscal);
	return 0;
fail:
	kfree(alscal);
	return -1;
}
#endif
/*
 * Reads and calculates current lux value.
 * The raw ch0 and ch1 values of the ambient light sensed in the last
 * integration cycle are read from the device.
 * Time scale factor array values are adjusted based on the integration time.
 * The raw values are multiplied by a scale factor, and device gain is obtained
 * using gain index. Limit checks are done next, then the ratio of a multiple
 * of ch1 value, to the ch0 value, is calculated. The array taos_device_lux[]
 * declared above is then scanned to find the first ratio value that is just
 * above the ratio we just calculated. The ch0 and ch1 multiplier constants in
 * the array are then used along with the time scale factor array values, to
 * calculate the lux.
 */
static int taos_get_lux(struct iio_dev *indio_dev, int *calculated_lux)
{
	int ch0, ch1; /* separated ch0/ch1 data from device */
	int lux; /* raw lux calculated from device data */
	u8 buf[5];
	struct tsl258x_chip *chip = iio_priv(indio_dev);
	int i, ret;
	int gain;
#ifdef CONFIG_AMS_ADJUST_WITH_BASELINE
	int ch0_baseline = 0, ch1_baseline = 0;
#endif

	if (mutex_trylock(&chip->als_mutex) == 0) {
		dev_info(&chip->client->dev, "taos_get_lux device is busy\n");
		return chip->als_cur_info.lux; /* busy, so return LAST VALUE */
	}

	if (chip->taos_chip_status != TSL258X_CHIP_WORKING) {
		/* device is not enabled */
		dev_err(&chip->client->dev, "taos_get_lux device is not enabled\n");
		ret = -EBUSY ;
		goto out_unlock;
	}

	ret = taos_i2c_read(chip->client, (TSL258X_CMD_REG), &buf[0], 1);
	if (ret < 0) {
		dev_err(&chip->client->dev, "taos_get_lux failed to read CMD_REG\n");
		goto out_unlock;
	}
	/* is data new & valid */
	if (!(buf[0] & TSL258X_STA_ADC_INTR)) {
		dev_dbg(&chip->client->dev, "taos_get_lux data not valid\n");
		ret = chip->als_cur_info.lux; /* return LAST VALUE */
		goto out_unlock;
	}

	for (i = 0; i < 4; i++) {
		int reg = TSL258X_CMD_REG | (TSL258X_ALS_CHAN0LO + i);
		ret = taos_i2c_read(chip->client, reg, &buf[i], 1);
		if (ret < 0) {
			dev_err(&chip->client->dev, "taos_get_lux failed to read"
				" register %x\n", reg);
			goto out_unlock;
		}
	}

	/* clear status, really interrupt status (interrupts are off), but
	 * we use the bit anyway - don't forget 0x80 - this is a command*/
	ret = i2c_smbus_write_byte(chip->client,
				   (TSL258X_CMD_REG | TSL258X_CMD_SPL_FN |
				    TSL258X_CMD_ALS_INT_CLR));

	if (ret < 0) {
		dev_err(&chip->client->dev,
			"taos_i2c_write_command failed in taos_get_lux, err = %d\n",
			ret);
		goto out_unlock; /* have no data, so return failure */
	}

	/* extract ALS/lux data */
	ch0 = le16_to_cpup((const __le16 *)&buf[0]);
	ch1 = le16_to_cpup((const __le16 *)&buf[2]);

	chip->als_cur_info.als_ch0 = ch0;
	chip->als_cur_info.als_ch1 = ch1;

	if ((ch0 >= chip->als_saturation) || (ch1 >= chip->als_saturation))
		goto return_max;

	if (ch0 == 0) {
		/* have no data, so return LAST VALUE */
		ret = chip->als_cur_info.lux = 0;
		goto out_unlock;
	}

	if (ch0 + ch1 <= NOISE_THRESHOLD) {
		*calculated_lux = 0;
		ret = 0;
		goto out_unlock;
	}

	switch (chip->taos_settings.als_gain) {
	case 0:
		gain = 1;
		break;
	case 1:
		gain = 8;
		break;
	case 2:
		gain = 16;
		break;
	case 3:
		gain = 111;
		break;
	}
#ifdef CONFIG_AMS_ADJUST_WITH_BASELINE
	if (get_baseline(&ch0_baseline, &ch1_baseline)) {
		pr_err("ALSCAL: baseline not found, defaulting to 0\n");
	}

	ch0 -= ch0_baseline;
	ch1 -= ch1_baseline;
#endif
	lux = COFF_1_NO_GLASS * (MILILUX_COFF * ch0 - CH1_COFF * ch1)/(gain*chip->taos_settings.als_time);

#ifdef CONFIG_AMS_ADJUST_WITH_BASELINE
	if (lux < 0)
		lux = 0;
#endif

	if (lux > TSL258X_LUX_CALC_OVER_FLOW) { /* check for overflow */
return_max:
		lux = TSL258X_LUX_CALC_OVER_FLOW;
	}

	/* Update the structure with the latest VALID lux. */
	chip->als_cur_info.lux = lux / MILILUX_COFF;
	*calculated_lux = lux;
	ret = 0;
out_unlock:
	mutex_unlock(&chip->als_mutex);
	return ret;
}

/*
 * Obtain single reading and calculate the als_gain_trim (later used
 * to derive actual lux).
 * Return updated gain_trim value.
 */
static int taos_als_calibrate(struct iio_dev *indio_dev)
{
	struct tsl258x_chip *chip = iio_priv(indio_dev);
	u8 reg_val;
	unsigned int gain_trim_val;
	int ret;
	int lux_val;

	ret = i2c_smbus_write_byte(chip->client,
				   (TSL258X_CMD_REG | TSL258X_CNTRL));
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"taos_als_calibrate failed to reach the CNTRL register, ret=%d\n",
			ret);
		return ret;
	}

	reg_val = i2c_smbus_read_byte(chip->client);
	if ((reg_val & (TSL258X_CNTL_ADC_ENBL | TSL258X_CNTL_PWR_ON))
			!= (TSL258X_CNTL_ADC_ENBL | TSL258X_CNTL_PWR_ON)) {
		dev_err(&chip->client->dev,
			"taos_als_calibrate failed: device not powered on with ADC enabled\n");
		return -1;
	}

	ret = i2c_smbus_write_byte(chip->client,
				   (TSL258X_CMD_REG | TSL258X_CNTRL));
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"taos_als_calibrate failed to reach the STATUS register, ret=%d\n",
			ret);
		return ret;
	}
	reg_val = i2c_smbus_read_byte(chip->client);

	if ((reg_val & TSL258X_STA_ADC_VALID) != TSL258X_STA_ADC_VALID) {
		dev_err(&chip->client->dev,
			"taos_als_calibrate failed: STATUS - ADC not valid.\n");
		return -ENODATA;
	}
	ret = taos_get_lux(indio_dev, &lux_val);
	if (ret < 0) {
		dev_err(&chip->client->dev, "taos_als_calibrate failed to get lux\n");
		return ret;
	}
	gain_trim_val = (unsigned int) (((chip->taos_settings.als_cal_target)
			* chip->taos_settings.als_gain_trim) / lux_val);

	if ((gain_trim_val < 250) || (gain_trim_val > 4000)) {
		dev_err(&chip->client->dev,
			"taos_als_calibrate failed: trim_val of %d is out of range\n",
			gain_trim_val);
		return -ENODATA;
	}
	chip->taos_settings.als_gain_trim = (int) gain_trim_val;

	return (int) gain_trim_val;
}

/*
 * Turn the device on.
 * Configuration must be set before calling this function.
 */
static int taos_chip_on(struct iio_dev *indio_dev)
{
	int i;
	int ret;
	u8 *uP;
	u8 utmp;
	int als_count;
	int als_time;
	struct tsl258x_chip *chip = iio_priv(indio_dev);

	/* and make sure we're not already on */
	if (chip->taos_chip_status == TSL258X_CHIP_WORKING) {
		/* if forcing a register update - turn off, then on */
		dev_info(&chip->client->dev, "device is already enabled\n");
		return -EINVAL;
	}

	/* determine als integration regster */
	als_count = (chip->taos_settings.als_time * 100 + 135) / 270;
	if (als_count == 0)
		als_count = 1; /* ensure at least one cycle */

	/* convert back to time (encompasses overrides) */
	als_time = (als_count * 27 + 5) / 10;
	chip->taos_config[TSL258X_ALS_TIME] = 256 - als_count;

	/* Set the gain based on taos_settings struct */
	chip->taos_config[TSL258X_GAIN] = chip->taos_settings.als_gain;

	/* set chip struct re scaling and saturation */
	chip->als_saturation = als_count * 922; /* 90% of full scale */
	chip->als_time_scale = (als_time + 25) / 50;

#ifdef CONFIG_ALS_TSL2584_AUTOZERO
	perform_autozero(chip);
#endif

	/* TSL258x Specific power-on / adc enable sequence
	 * Power on the device 1st. */
	utmp = TSL258X_CNTL_PWR_ON;
	ret = i2c_smbus_write_byte_data(chip->client,
					TSL258X_CMD_REG | TSL258X_CNTRL, utmp);
	if (ret < 0) {
		dev_err(&chip->client->dev, "taos_chip_on failed on CNTRL reg.\n");
		return -1;
	}

	/* Use the following shadow copy for our delay before enabling ADC.
	 * Write all the registers. */
	for (i = 0, uP = chip->taos_config; i < TSL258X_REG_MAX; i++) {
		ret = i2c_smbus_write_byte_data(chip->client,
						TSL258X_CMD_REG + i,
						*uP++);
		if (ret < 0) {
			dev_err(&chip->client->dev,
				"taos_chip_on failed on reg %d.\n", i);
			return -1;
		}
	}

	msleep(3);
	/* NOW enable the ADC
	 * initialize the desired mode of operation */
	utmp = TSL258X_CNTL_PWR_ON | TSL258X_CNTL_ADC_ENBL;
	ret = i2c_smbus_write_byte_data(chip->client,
					TSL258X_CMD_REG | TSL258X_CNTRL,
					utmp);
	if (ret < 0) {
		dev_err(&chip->client->dev, "taos_chip_on failed on 2nd CTRL reg.\n");
		return -1;
	}
	chip->taos_chip_status = TSL258X_CHIP_WORKING;

#ifdef CONFIG_ALS_TSL2584_AUTOZERO
	schedule_delayed_work(&chip->autozero_work,
			msecs_to_jiffies(chip->autozero_period_ms));
#endif
	return ret;
}

static int taos_chip_off(struct iio_dev *indio_dev)
{
	struct tsl258x_chip *chip = iio_priv(indio_dev);
	int ret;

	/* turn device off */
	chip->taos_chip_status = TSL258X_CHIP_SUSPENDED;
#ifdef CONFIG_ALS_TSL2584_AUTOZERO
	cancel_delayed_work_sync(&chip->autozero_work);
#endif
	ret = i2c_smbus_write_byte_data(chip->client,
					TSL258X_CMD_REG | TSL258X_CNTRL,
					0x00);
	return ret;
}

/* Sysfs Interface Functions */

static ssize_t taos_power_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl258x_chip *chip = iio_priv(indio_dev);

	return sprintf(buf, "%d\n", chip->taos_chip_status);
}

static ssize_t taos_power_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value == 0)
		taos_chip_off(indio_dev);
	else
		taos_chip_on(indio_dev);

	return len;
}

static ssize_t taos_gain_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl258x_chip *chip = iio_priv(indio_dev);
	char gain[4] = {0};

	switch (chip->taos_settings.als_gain) {
	case 0:
		strcpy(gain, "001");
		break;
	case 1:
		strcpy(gain, "008");
		break;
	case 2:
		strcpy(gain, "016");
		break;
	case 3:
		strcpy(gain, "111");
		break;
	}

	return sprintf(buf, "%s\n", gain);
}

static ssize_t taos_gain_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl258x_chip *chip = iio_priv(indio_dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	switch (value) {
	case 1:
		chip->taos_settings.als_gain = 0;
		break;
	case 8:
		chip->taos_settings.als_gain = 1;
		break;
	case 16:
		chip->taos_settings.als_gain = 2;
		break;
	case 111:
		chip->taos_settings.als_gain = 3;
		break;
	default:
		dev_err(dev, "Invalid Gain Index (must be 1,8,16,111)\n");
		return -1;
	}

	return len;
}

static ssize_t taos_gain_available_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", "1 8 16 111");
}

static ssize_t taos_als_time_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl258x_chip *chip = iio_priv(indio_dev);

	return sprintf(buf, "%d\n", chip->taos_settings.als_time);
}

static ssize_t taos_als_time_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl258x_chip *chip = iio_priv(indio_dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if ((value < 50) || (value > 650))
		return -EINVAL;

	if (value % 50)
		return -EINVAL;

	chip->taos_settings.als_time = value;

	return len;
}

static ssize_t taos_als_time_available_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n",
		"50 100 150 200 250 300 350 400 450 500 550 600 650");
}

static ssize_t taos_als_trim_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl258x_chip *chip = iio_priv(indio_dev);

	return sprintf(buf, "%d\n", chip->taos_settings.als_gain_trim);
}

static ssize_t taos_als_trim_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl258x_chip *chip = iio_priv(indio_dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value)
		chip->taos_settings.als_gain_trim = value;

	return len;
}

static ssize_t taos_als_cal_target_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl258x_chip *chip = iio_priv(indio_dev);

	return sprintf(buf, "%d\n", chip->taos_settings.als_cal_target);
}

static ssize_t taos_als_cal_target_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl258x_chip *chip = iio_priv(indio_dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value)
		chip->taos_settings.als_cal_target = value;

	return len;
}

static ssize_t taos_lux_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	int ret;
	int lux = 0;

	ret = taos_get_lux(dev_get_drvdata(dev), &lux);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n",
		       lux);
}

static ssize_t taos_do_calibrate(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value == 1)
		taos_als_calibrate(indio_dev);

	return len;
}

static ssize_t taos_luxtable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i;
	int offset = 0;

	for (i = 0; i < ARRAY_SIZE(taos_device_lux); i++) {
		offset += sprintf(buf + offset, "%d,%d,%d,",
				  taos_device_lux[i].ratio,
				  taos_device_lux[i].ch0,
				  taos_device_lux[i].ch1);
		if (taos_device_lux[i].ratio == 0) {
			/* We just printed the first "0" entry.
			 * Now get rid of the extra "," and break. */
			offset--;
			break;
		}
	}

	offset += sprintf(buf + offset, "\n");
	return offset;
}

static ssize_t taos_luxtable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl258x_chip *chip = iio_priv(indio_dev);
	int value[ARRAY_SIZE(taos_device_lux)*3 + 1];
	int n;

	get_options(buf, ARRAY_SIZE(value), value);

	/* We now have an array of ints starting at value[1], and
	 * enumerated by value[0].
	 * We expect each group of three ints is one table entry,
	 * and the last table entry is all 0.
	 */
	n = value[0];
	if (chip->id == ID_TSL2584TSV) {
		if (n != 3) {
			dev_info(dev, "LUX TABLE INPUT ERROR "
				"TSL2584TSV input as 0,COFF_CH0,COFF_CH1\n");
			return -EINVAL;
		}

		if (value[(n - 2)] != 0) {
			dev_info(dev, "LUX TABLE INPUT ERROR, "
				"TSL2584TSV ratio must be 0, not %d\n",
				value[(n - 2)]);
			return -EINVAL;
		}
	} else {
		if ((n % 3) || n < 6 || n > ((ARRAY_SIZE(taos_device_lux) - 1) * 3)) {
			dev_info(dev, "LUX TABLE INPUT ERROR 1 Value[0]=%d\n", n);
			return -EINVAL;
		}

		if ((value[(n - 2)] | value[(n - 1)] | value[n]) != 0) {
			dev_info(dev, "LUX TABLE INPUT ERROR 2 Value[0]=%d\n", n);
			return -EINVAL;
		}
	}

	if (chip->taos_chip_status == TSL258X_CHIP_WORKING)
		taos_chip_off(indio_dev);

	/* Zero out the table */
	memset(taos_device_lux, 0, sizeof(taos_device_lux));
	memcpy(taos_device_lux, &value[1], (value[0] * 4));

	taos_chip_on(indio_dev);

	return len;
}
static ssize_t taos_channel1_show(struct device *dev,
								  struct device_attribute *attr,
								  const char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl258x_chip *chip = iio_priv(indio_dev);
	u8 value_bytes[2];
	u16 channel_value;
	int ret;
	int i;

	mutex_lock(&chip->als_mutex);

	for (i = 0; i < 2; i++) {
		int reg = TSL258X_CMD_REG | (TSL258X_ALS_CHAN0LO + i);
		ret = taos_i2c_read(chip->client, reg, &value_bytes[i], 1);
		if (ret < 0) {
			dev_err(&chip->client->dev, "taos_channel1_show failed to read register %x\n", reg);
			goto fail;
		}
	}
	channel_value = (value_bytes[1] << 8) | value_bytes[0];
	ret = sprintf(buf, "%d\n", channel_value);
fail:
	mutex_unlock(&chip->als_mutex);
	return ret;
}

static ssize_t taos_channel2_show(struct device *dev,
								  struct device_attribute *attr,
								  const char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl258x_chip *chip = iio_priv(indio_dev);
	u8 value_bytes[2];
	u16 channel_value;
	int ret;
	int i;

	mutex_lock(&chip->als_mutex);

	for (i = 0; i < 2; i++) {
		int reg = TSL258X_CMD_REG | (TSL258X_ALS_CHAN1LO + i);
		ret = taos_i2c_read(chip->client, reg, &value_bytes[i], 1);
		if (ret < 0) {
			dev_err(&chip->client->dev, "taos_channel2_show failed to read register %x\n", reg);
			goto fail;
		}
	}
	channel_value = (value_bytes[1] << 8) | value_bytes[0];
	ret = sprintf(buf, "%d\n", channel_value);
fail:
	mutex_unlock(&chip->als_mutex);
	return ret;
}


#ifdef CONFIG_AMZN_AMS_ALS
#define ALS_CAL_OF_PATH "/idme/alscal"
#define ALS_MAX_LUX 400
#define ALS_MIN_LUX 0

static int parse_alscal_idme(int *coeff)
{
	struct device_node *ap = NULL;
	char *alscal_idme = NULL;
	char *alscal = NULL;
	int dummy;

	ap = of_find_node_by_path(ALS_CAL_OF_PATH);
	if (ap) {
		alscal_idme = (char *)of_get_property(ap, "value", NULL);
	} else {
		pr_err("ALSCAL: could not get alscal idme entry\n");
		return -1;
	}

	alscal = kstrdup(alscal_idme, GFP_KERNEL);

	/* Move pointer to ams_400_0 */
	strsep(&alscal, " ");
	strsep(&alscal, " ");
	if (sscanf(alscal, "ams_400_0=%d,%d", &dummy, coeff) != 2) {
		pr_err("ALSCAL: unable to parse alscal idme string\n", alscal);
		return -1;
	}

	kfree(alscal);
	return 0;
}

static ssize_t taos_calibrated_lux_show(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf)
{
	int coeff = 0;
	int lux = 0;
	int calibrated_lux = 0;

	if (parse_alscal_idme(&coeff) || !coeff) {
		pr_err("ALSCAL: could not parse alscal idme string\n");
		return -EINVAL;
	}

	taos_get_lux(dev_get_drvdata(dev), &lux);

	calibrated_lux = ALS_MAX_LUX * lux / coeff;

	if (calibrated_lux > ALS_MAX_LUX)
		calibrated_lux = ALS_MAX_LUX;

	if (calibrated_lux < ALS_MIN_LUX)
		calibrated_lux = ALS_MIN_LUX;

	return sprintf(buf, "%d\n", calibrated_lux);
}
#endif
static DEVICE_ATTR(power_state, S_IRUGO | S_IWUSR,
		taos_power_state_show, taos_power_state_store);

static DEVICE_ATTR(illuminance0_calibscale, S_IRUGO | S_IWUSR,
		taos_gain_show, taos_gain_store);
static DEVICE_ATTR(illuminance0_calibscale_available, S_IRUGO,
		taos_gain_available_show, NULL);

static DEVICE_ATTR(illuminance0_integration_time, S_IRUGO | S_IWUSR,
		taos_als_time_show, taos_als_time_store);
static DEVICE_ATTR(illuminance0_integration_time_available, S_IRUGO,
		taos_als_time_available_show, NULL);

static DEVICE_ATTR(illuminance0_calibbias, S_IRUGO | S_IWUSR,
		taos_als_trim_show, taos_als_trim_store);

static DEVICE_ATTR(illuminance0_input_target, S_IRUGO | S_IWUSR,
		taos_als_cal_target_show, taos_als_cal_target_store);

static DEVICE_ATTR(illuminance0_input, S_IRUGO, taos_lux_show, NULL);
static DEVICE_ATTR(illuminance0_calibrate, S_IWUSR, NULL, taos_do_calibrate);
static DEVICE_ATTR(illuminance0_lux_table, S_IRUGO | S_IWUSR,
		taos_luxtable_show, taos_luxtable_store);
static DEVICE_ATTR(channel_1_raw, S_IRUGO, taos_channel1_show, NULL);
static DEVICE_ATTR(channel_2_raw, S_IRUGO, taos_channel2_show, NULL);
#ifdef CONFIG_AMZN_AMS_ALS
static DEVICE_ATTR(calibrated_lux, (S_IRUGO), taos_calibrated_lux_show, NULL);
#endif

static struct attribute *sysfs_attrs_ctrl[] = {
	&dev_attr_power_state.attr,
	&dev_attr_illuminance0_calibscale.attr,			/* Gain  */
	&dev_attr_illuminance0_calibscale_available.attr,
	&dev_attr_illuminance0_integration_time.attr,	/* I time*/
	&dev_attr_illuminance0_integration_time_available.attr,
	&dev_attr_illuminance0_calibbias.attr,			/* trim  */
	&dev_attr_illuminance0_input_target.attr,
	&dev_attr_illuminance0_input.attr,
	&dev_attr_illuminance0_calibrate.attr,
	&dev_attr_illuminance0_lux_table.attr,
	&dev_attr_channel_1_raw.attr,
	&dev_attr_channel_2_raw.attr,
#ifdef CONFIG_AMZN_AMS_ALS
	&dev_attr_calibrated_lux.attr,
#endif
	NULL
};

static struct attribute_group tsl258x_attribute_group = {
	.attrs = sysfs_attrs_ctrl,
};

/* Use the default register values to identify the actual chip */
static int taos_tsl258x_chip_id(unsigned char *bufp)
{
	int id;

	if ((bufp[TSL258X_ID2] & 0x80) == 0x80)
		id = ID_TSL2584TSV;
	else if ((bufp[TSL258X_ID2] & 0x30) == 0x30)
		id = ID_TSL2583;
	else if ((bufp[TSL258X_ID2] & 0x30) == 0)
		id = ID_TSL2581;
	else
		id = ID_UNKNOWN;

	return id;
}

/* Use the default register values to identify the Taos device */
static int taos_tsl258x_device(unsigned char *bufp)
{
	return ((bufp[TSL258X_CHIPID] & 0xf0) == 0x90);
}

static const struct iio_info tsl258x_info = {
	.attrs = &tsl258x_attribute_group,
	.driver_module = THIS_MODULE,
};

#ifdef CONFIG_ALS_TSL2584_AUTOZERO
static void perform_autozero(struct tsl258x_chip *chip)
{
	int i, ret;
	struct i2c_client *clientp = chip->client;
	u8 utmp, buf[2];

	dev_dbg(&clientp->dev, "Running ALS autozero sequence\n");

	for (i = 0; i < ARRAY_SIZE(autozero_regs); i++) {
		ret = i2c_smbus_write_byte_data(chip->client,
				TSL258X_CMD_REG | autozero_regs[i].reg_address,
				autozero_regs[i].value);
		if (ret < 0) {
			dev_err(&clientp->dev, "i2c_smbus_write_byte() to reg "
					"%x failed, err = %d\n",
					autozero_regs[i].reg_address, ret);
			return;
		}
	}

	usleep_range(TSL258X_AUTOZERO_WAIT_MS*1000,
			(TSL258X_AUTOZERO_WAIT_MS+1)*1000);

	/* Restore atime and gain */
	/* Power on, ADC disabled */
	utmp = TSL258X_CNTL_PWR_ON;
	ret = i2c_smbus_write_byte_data(chip->client,
					TSL258X_CMD_REG | TSL258X_CNTRL, utmp);
	if (ret < 0) {
		dev_err(&chip->client->dev, "i2c_smbus_write_byte() to reg %x "
				"failed, err = %d\n",
				TSL258X_CMD_REG | TSL258X_CNTRL, ret);
		return;
	}

	/* Restore original atime */
	utmp = chip->taos_config[TSL258X_ALS_TIME];
	ret = i2c_smbus_write_byte_data(chip->client,
				TSL258X_CMD_REG | TSL258X_ALS_TIME, utmp);
	if (ret < 0) {
		dev_err(&chip->client->dev, "i2c_smbus_write_byte() to reg %x "
				"failed, err = %d\n",
				TSL258X_CMD_REG | TSL258X_ALS_TIME, ret);
		return;
	}

	/* Restore gain */
	utmp = chip->taos_config[TSL258X_GAIN];
	ret = i2c_smbus_write_byte_data(chip->client,
			TSL258X_CMD_REG | TSL258X_GAIN, utmp);
	if (ret < 0) {
		dev_err(&chip->client->dev, "i2c_smbus_write_byte() to reg %x "
				"failed, err = %d\n",
				TSL258X_CMD_REG | TSL258X_GAIN, ret);
		return;
	}

	/* Power on, ADC enabled */
	utmp = TSL258X_CNTL_PWR_ON | TSL258X_CNTL_ADC_ENBL;
	ret = i2c_smbus_write_byte_data(chip->client,
					TSL258X_CMD_REG | TSL258X_CNTRL,
					utmp);
	if (ret < 0) {
		dev_err(&chip->client->dev, "i2c_smbus_write_byte() to reg %x "
				"failed, err = %d\n",
				TSL258X_CMD_REG | TSL258X_CNTRL, ret);
		return;
	}
}

static void autozero_work(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct tsl258x_chip *chip = container_of(delayed_work,
						 struct tsl258x_chip,
						 autozero_work);

	mutex_lock(&chip->als_mutex);
	perform_autozero(chip);
	schedule_delayed_work(&chip->autozero_work,
			msecs_to_jiffies(chip->autozero_period_ms));
	mutex_unlock(&chip->als_mutex);
	return;
}
#endif

/*
 * Client probe function - When a valid device is found, the driver's device
 * data structure is updated, and initialization completes successfully.
 */
static int taos_probe(struct i2c_client *clientp,
		      const struct i2c_device_id *idp)
{
	int i, ret;
	unsigned char buf[TSL258X_MAX_DEVICE_REGS];
	struct tsl258x_chip *chip;
	struct iio_dev *indio_dev;

	if (!i2c_check_functionality(clientp->adapter,
		I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&clientp->dev,
			"taos_probe() - i2c smbus byte data "
			"functions unsupported\n");
		return -EOPNOTSUPP;
	}

	indio_dev = iio_device_alloc(sizeof(*chip));
	if (indio_dev == NULL) {
		ret = -ENOMEM;
		dev_err(&clientp->dev, "iio allocation failed\n");
		goto fail1;
	}
	chip = iio_priv(indio_dev);
	chip->client = clientp;
	i2c_set_clientdata(clientp, indio_dev);

	mutex_init(&chip->als_mutex);
	chip->taos_chip_status = TSL258X_CHIP_UNKNOWN;
	memcpy(chip->taos_config, taos_config, sizeof(chip->taos_config));

	for (i = 0; i < TSL258X_MAX_DEVICE_REGS; i++) {
		ret = i2c_smbus_write_byte(clientp,
				(TSL258X_CMD_REG | (TSL258X_CNTRL + i)));
		if (ret < 0) {
			dev_err(&clientp->dev, "i2c_smbus_write_bytes() to cmd "
				"reg failed in taos_probe(), err = %d\n", ret);
			goto fail2;
		}
		ret = i2c_smbus_read_byte(clientp);
		if (ret < 0) {
			dev_err(&clientp->dev, "i2c_smbus_read_byte from "
				"reg failed in taos_probe(), err = %d\n", ret);

			goto fail2;
		}
		buf[i] = ret;
	}

	if (!taos_tsl258x_device(buf)) {
		dev_info(&clientp->dev, "i2c device found but does not match "
			"expected id in taos_probe()\n");
		goto fail2;
	}

	if (idp->driver_data != ID_UNKNOWN) {
		chip->id = (int) idp->driver_data;
	} else {
		chip->id = taos_tsl258x_chip_id(buf);
	}

	ret = i2c_smbus_write_byte(clientp, (TSL258X_CMD_REG | TSL258X_CNTRL));
	if (ret < 0) {
		dev_err(&clientp->dev, "i2c_smbus_write_byte() to cmd reg "
			"failed in taos_probe(), err = %d\n", ret);
		goto fail2;
	}

	indio_dev->info = &tsl258x_info;
	indio_dev->dev.parent = &clientp->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->name = chip->client->name;
	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(&clientp->dev, "iio registration failed\n");
		goto fail2;
	}

#ifdef CONFIG_ALS_TSL2584_AUTOZERO
	INIT_DELAYED_WORK(&chip->autozero_work, autozero_work);
	ret = of_property_read_u32(clientp->dev.of_node, "autozero-period-ms",
			&chip->autozero_period_ms);
	if (ret < 0) {
		chip->autozero_period_ms = TSL258X_AUTOZERO_DEFAULT_PERIOD_MS;
		dev_warn(&clientp->dev, "Autozero period not specified in "
				"device tree, defaulting to %u\n",
				chip->autozero_period_ms);
	}
	dev_info(&clientp->dev, "Autozero period set to %u\n",
			chip->autozero_period_ms);
#endif

	/* Load up the V2 defaults (these are hard coded defaults for now) */
	taos_defaults(chip);

	/* Make sure the chip is on */
	taos_chip_on(indio_dev);

	dev_info(&clientp->dev, "Light sensor found, %s.\n",
		tsl2583x_get_name(chip));
	return 0;
fail1:
	iio_device_free(indio_dev);
fail2:
	return ret;
}

static int taos_suspend(struct i2c_client *client, pm_message_t state)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct tsl258x_chip *chip = iio_priv(indio_dev);
	int ret = 0;

	mutex_lock(&chip->als_mutex);

	if (chip->taos_chip_status == TSL258X_CHIP_WORKING) {
		ret = taos_chip_off(indio_dev);
		chip->taos_chip_status = TSL258X_CHIP_SUSPENDED;
	}

	mutex_unlock(&chip->als_mutex);
	return ret;
}

static int taos_resume(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct tsl258x_chip *chip = iio_priv(indio_dev);
	int ret = 0;

	mutex_lock(&chip->als_mutex);

	if (chip->taos_chip_status == TSL258X_CHIP_SUSPENDED)
		ret = taos_chip_on(indio_dev);

	mutex_unlock(&chip->als_mutex);
	return ret;
}


static int taos_remove(struct i2c_client *client)
{
	iio_device_unregister(i2c_get_clientdata(client));
	iio_device_free(i2c_get_clientdata(client));

	return 0;
}

static struct i2c_device_id taos_idtable[] = {
	{ "tsl258x", ID_UNKNOWN },
	{ "tsl2580", ID_TSL2580 },
	{ "tsl2581", ID_TSL2581 },
	{ "tsl2583", ID_TSL2583 },
	{ "tsl2584tsv", ID_TSL2584TSV },
	{}
};
MODULE_DEVICE_TABLE(i2c, taos_idtable);

static char *tsl2583x_get_name(struct tsl258x_chip *chip)
{
	int i;

	for (i = 0; i < sizeof(taos_idtable) / sizeof(taos_idtable[0]); i++) {
		if ((int) taos_idtable[i].driver_data ==  chip->id)
			return taos_idtable[i].name;
	}
	return "unknown sensor";
}

/* Driver definition */
static struct i2c_driver taos_driver = {
	.driver = {
		.name = "tsl258x",
	},
	.id_table = taos_idtable,
	.suspend	= taos_suspend,
	.resume		= taos_resume,
	.probe = taos_probe,
	.remove = taos_remove,
};

static int __init taos_init(void)
{
	return i2c_add_driver(&taos_driver);
}

static void __exit taos_exit(void)
{
	i2c_del_driver(&taos_driver);
}

module_init(taos_init);
module_exit(taos_exit);

MODULE_AUTHOR("J. August Brenner<jbrenner@taosinc.com>");
MODULE_DESCRIPTION("TAOS tsl258x ambient light sensor driver");
MODULE_LICENSE("GPL");
