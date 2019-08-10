/* BOSCH Gyroscope Sensor Driver
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
 * History: V1.0 --- [2013.01.29]Driver creation
 *          V1.1 --- [2013.03.28]
 *                   1.Instead late_resume, use resume to make sure
 *                     driver resume is ealier than processes resume.
 *                   2.Add power mode setting in read data.
 *          V1.2 --- [2013.06.28]Add self test function.
 *          V1.3 --- [2013.07.26]Fix the bug of wrong axis remapping
 *                   in rawdata inode.
 */
/*
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/hwmsen_helper.h>
*/
#include <linux/module.h>
#include <gyroscope.h>
#include <cust_gyro.h>
#include "bmi160_gyro.h"

#include <linux/kernel.h>
#include <upmu_hw.h>
#include <upmu_sw.h>
#include <upmu_common.h>

#define BMI160_DATA_BUF_NUM            3
#define BMI160_GYRO_AXES_NUM       3
#define BMI160_CALI_DATA_NUM           100
#define BMI160_CALI_DATA_BYPASS_NUM    10

#define BMI160_GYRO_CALI_IDME           "/proc/idme/sensorcal"
#define BMI160_GYRO_CALI_FILE          "/data/inv_cal_data.bin"

#define GYRO_MAX_OFFSET_VAL             164
#define GYRO_MIN_OFFSET_VAL             -164

#define BMI160_I2C_ADDR			0x68

static s16 gyro_xyz_offset[BMI160_GYRO_AXES_NUM] = {0};
static int gyro_self_test_flag = -1;

extern int idme_get_sensorcal(s16 *data);

/* sensor type */
enum SENSOR_TYPE_ENUM {
	BMI160_GYRO_TYPE = 0x0,
	INVALID_TYPE = 0xff
};

/* range */
enum BMG_RANGE_ENUM {
	BMG_RANGE_2000 = 0x0,	/* +/- 2000 degree/s */
	BMG_RANGE_1000,		/* +/- 1000 degree/s */
	BMG_RANGE_500,		/* +/- 500 degree/s */
	BMG_RANGE_250,		/* +/- 250 degree/s */
	BMG_RANGE_125,		/* +/- 125 degree/s */
	BMG_UNDEFINED_RANGE = 0xff
};

/* power mode */
enum BMG_POWERMODE_ENUM {
	BMG_SUSPEND_MODE = 0x0,
	BMG_NORMAL_MODE,
	BMG_UNDEFINED_POWERMODE = 0xff
};

/* debug infomation flags */
enum GYRO_TRC {
	GYRO_TRC_FILTER = 0x01,
	GYRO_TRC_RAWDATA = 0x02,
	GYRO_TRC_IOCTL = 0x04,
	GYRO_TRC_CALI = 0x08,
	GYRO_TRC_INFO = 0x10,
};

/* s/w data filter */
struct data_filter {
	s16 raw[C_MAX_FIR_LENGTH][BMG_AXES_NUM];
	int sum[BMG_AXES_NUM];
	int num;
	int idx;
};

/* bmg i2c client data */
struct bmg_i2c_data {
	struct i2c_client *client;
	struct gyro_hw *hw;
	struct hwmsen_convert cvt;
	/* sensor info */
	u8 sensor_name[MAX_SENSOR_NAME];
	enum SENSOR_TYPE_ENUM sensor_type;
	enum BMG_POWERMODE_ENUM power_mode;
	enum BMG_RANGE_ENUM range;
	int datarate;
	/* sensitivity = 2^bitnum/range
	   [+/-2000 = 4000; +/-1000 = 2000;
	   +/-500 = 1000; +/-250 = 500;
	   +/-125 = 250 ] */
	u16 sensitivity;
	/*misc */
	struct mutex lock;
	atomic_t trace;
	atomic_t suspend;
	atomic_t filter;
	/* unmapped axis value */
	s16 cali_sw[BMG_AXES_NUM + 1];
	/* hw offset */
	s8 offset[BMG_AXES_NUM + 1];	/* +1:for 4-byte alignment */

#if defined(CONFIG_BMG_LOWPASS)
	atomic_t firlen;
	atomic_t fir_en;
	struct data_filter fir;
#endif
};

static struct gyro_hw gyro_cust;
static struct gyro_hw *hw = &gyro_cust;
extern struct i2c_client *bmi160_acc_i2c_client;
static struct gyro_init_info bmi160_gyro_init_info;
/* 0=OK, -1=fail */
static int bmi160_gyro_init_flag = -1;
static struct i2c_driver bmg_i2c_driver;
static struct bmg_i2c_data *obj_i2c_data;
static int bmg_set_powermode(struct i2c_client *client, enum BMG_POWERMODE_ENUM power_mode);
static const struct i2c_device_id bmg_i2c_id[] = {
	{BMG_DEV_NAME, 0},
	{}
};

static int bmi160_set_command_register(u8 cmd_reg);

/* I2C operation functions */
#if 1
static int bmg_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	return bma_i2c_read_block(client, addr, data, len);
}
static int bmg_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	return bma_i2c_write_block(client, addr, data, len);
}
#else
static int bmg_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	int err;
	u8 beg = addr;
	struct i2c_msg msgs[2] = {
		{
		 .addr = client->addr, .flags = 0,
		 .len = 1, .buf = &beg},
		{
		 .addr = client->addr, .flags = I2C_M_RD,
		 .len = len, .buf = data,
		 }
	};

	if (!client)
		return -EINVAL;
	else if (len > C_I2C_FIFO_SIZE) {
		GYRO_ERROR("length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}
	err = i2c_transfer(client->adapter, msgs, sizeof(msgs) / sizeof(msgs[0]));
	if (err != 2) {
		GYRO_ERROR("i2c_transfer error: (%d %p %d) %d\n", addr, data, len, err);
		err = -EIO;
	} else {
		err = 0;
	}
	return err;
}

static int bmg_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	/*
	 *because address also occupies one byte,
	 *the maximum length for write is 7 bytes
	 */
	int err = 0;
	int idx = 0;
	int num = 0;
	char buf[C_I2C_FIFO_SIZE];

	if (!client)
		return -EINVAL;
	else if (len >= C_I2C_FIFO_SIZE) {
		GYRO_ERROR("length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++) {
		buf[num++] = data[idx];
	}
	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		GYRO_ERROR("send command error.\n");
		return -EFAULT;
	} else {
		err = 0;
	}
	return err;
}
#endif
static int bmg_read_raw_data(struct i2c_client *client, s16 data[BMG_AXES_NUM])
{
	int err = 0;
	struct bmg_i2c_data *priv = obj_i2c_data;

	if (priv->power_mode == BMG_SUSPEND_MODE) {
		err = bmg_set_powermode(client, (enum BMG_POWERMODE_ENUM)BMG_NORMAL_MODE);
		if (err < 0) {
			GYRO_ERROR("set power mode failed, err = %d\n", err);
			return err;
		}
	}
	if (priv->sensor_type == BMI160_GYRO_TYPE) {
		u8 buf_tmp[BMG_DATA_LEN] = { 0 };

		err = bmg_i2c_read_block(client, BMI160_USER_DATA_8_GYR_X_LSB__REG, buf_tmp, 6);
		if (err) {
			GYRO_ERROR("read gyro raw data failed.\n");
			return err;
		}
		/* Data X */
		data[BMG_AXIS_X] = (s16)
		    ((((s32) ((s8) buf_tmp[1]))
		      << BMI160_SHIFT_8_POSITION) | (buf_tmp[0]));
		/* Data Y */
		data[BMG_AXIS_Y] = (s16)
		    ((((s32) ((s8) buf_tmp[3]))
		      << BMI160_SHIFT_8_POSITION) | (buf_tmp[2]));
		/* Data Z */
		data[BMG_AXIS_Z] = (s16)
		    ((((s32) ((s8) buf_tmp[5]))
		      << BMI160_SHIFT_8_POSITION) | (buf_tmp[4]));
		if (atomic_read(&priv->trace) & GYRO_TRC_RAWDATA) {
			GYRO_DBG("[%s][16bit raw]"
				 "[%08X %08X %08X] => [%5d %5d %5d]\n",
				 priv->sensor_name,
				 data[BMG_AXIS_X],
				 data[BMG_AXIS_Y],
				 data[BMG_AXIS_Z],
				 data[BMG_AXIS_X], data[BMG_AXIS_Y], data[BMG_AXIS_Z]);
		}
	}
#ifdef CONFIG_BMG_LOWPASS
/*
*Example: firlen = 16, filter buffer = [0] ... [15],
*when 17th data come, replace [0] with this new data.
*Then, average this filter buffer and report average value to upper layer.
*/
	if (atomic_read(&priv->filter)) {
		if (atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend)) {
			int idx, firlen = atomic_read(&priv->firlen);

			if (priv->fir.num < firlen) {
				priv->fir.raw[priv->fir.num][BMG_AXIS_X] = data[BMG_AXIS_X];
				priv->fir.raw[priv->fir.num][BMG_AXIS_Y] = data[BMG_AXIS_Y];
				priv->fir.raw[priv->fir.num][BMG_AXIS_Z] = data[BMG_AXIS_Z];
				priv->fir.sum[BMG_AXIS_X] += data[BMG_AXIS_X];
				priv->fir.sum[BMG_AXIS_Y] += data[BMG_AXIS_Y];
				priv->fir.sum[BMG_AXIS_Z] += data[BMG_AXIS_Z];
				if (atomic_read(&priv->trace) & GYRO_TRC_FILTER) {
					GYRO_DBG("add [%2d]"
						 "[%5d %5d %5d] => [%5d %5d %5d]\n",
						 priv->fir.num,
						 priv->fir.raw
						 [priv->fir.num][BMG_AXIS_X],
						 priv->fir.raw
						 [priv->fir.num][BMG_AXIS_Y],
						 priv->fir.raw
						 [priv->fir.num][BMG_AXIS_Z],
						 priv->fir.sum[BMG_AXIS_X],
						 priv->fir.sum[BMG_AXIS_Y],
						 priv->fir.sum[BMG_AXIS_Z]);
				}
				priv->fir.num++;
				priv->fir.idx++;
			} else {
				idx = priv->fir.idx % firlen;
				priv->fir.sum[BMG_AXIS_X] -= priv->fir.raw[idx][BMG_AXIS_X];
				priv->fir.sum[BMG_AXIS_Y] -= priv->fir.raw[idx][BMG_AXIS_Y];
				priv->fir.sum[BMG_AXIS_Z] -= priv->fir.raw[idx][BMG_AXIS_Z];
				priv->fir.raw[idx][BMG_AXIS_X] = data[BMG_AXIS_X];
				priv->fir.raw[idx][BMG_AXIS_Y] = data[BMG_AXIS_Y];
				priv->fir.raw[idx][BMG_AXIS_Z] = data[BMG_AXIS_Z];
				priv->fir.sum[BMG_AXIS_X] += data[BMG_AXIS_X];
				priv->fir.sum[BMG_AXIS_Y] += data[BMG_AXIS_Y];
				priv->fir.sum[BMG_AXIS_Z] += data[BMG_AXIS_Z];
				priv->fir.idx++;
				data[BMG_AXIS_X] = priv->fir.sum[BMG_AXIS_X] / firlen;
				data[BMG_AXIS_Y] = priv->fir.sum[BMG_AXIS_Y] / firlen;
				data[BMG_AXIS_Z] = priv->fir.sum[BMG_AXIS_Z] / firlen;
				if (atomic_read(&priv->trace) & GYRO_TRC_FILTER) {
					GYRO_DBG("add [%2d]"
						 "[%5d %5d %5d] =>"
						 "[%5d %5d %5d] : [%5d %5d %5d]\n", idx,
						 priv->fir.raw[idx][BMG_AXIS_X],
						 priv->fir.raw[idx][BMG_AXIS_Y],
						 priv->fir.raw[idx][BMG_AXIS_Z],
						 priv->fir.sum[BMG_AXIS_X],
						 priv->fir.sum[BMG_AXIS_Y],
						 priv->fir.sum[BMG_AXIS_Z],
						 data[BMG_AXIS_X],
						 data[BMG_AXIS_Y], data[BMG_AXIS_Z]);
				}
			}
		}
	}
#endif
	return err;
}

#ifndef SW_CALIBRATION
/* get hardware offset value from chip register */
static int bmg_get_hw_offset(struct i2c_client *client, s8 offset[BMG_AXES_NUM + 1])
{
	int err = 0;
	/* HW calibration is under construction */
	GYRO_DBG("hw offset x=%x, y=%x, z=%x\n",
		 offset[BMG_AXIS_X], offset[BMG_AXIS_Y], offset[BMG_AXIS_Z]);
	return err;
}
#endif

#ifndef SW_CALIBRATION
/* set hardware offset value to chip register*/
static int bmg_set_hw_offset(struct i2c_client *client, s8 offset[BMG_AXES_NUM + 1])
{
	int err = 0;
	/* HW calibration is under construction */
	GYRO_DBG("hw offset x=%x, y=%x, z=%x\n",
		 offset[BMG_AXIS_X], offset[BMG_AXIS_Y], offset[BMG_AXIS_Z]);
	return err;
}
#endif

static int bmg_reset_calibration(struct i2c_client *client)
{
	int err = 0;
	struct bmg_i2c_data *obj = obj_i2c_data;
#ifdef SW_CALIBRATION

#else
	err = bmg_set_hw_offset(client, obj->offset);
	if (err) {
		GYRO_ERROR("read hw offset failed, %d\n", err);
		return err;
	}
#endif
	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	memset(obj->offset, 0x00, sizeof(obj->offset));
	return err;
}

static int bmg_read_calibration(struct i2c_client *client,
				int act[BMG_AXES_NUM], int raw[BMG_AXES_NUM])
{
	/*
	 *raw: the raw calibration data, unmapped;
	 *act: the actual calibration data, mapped
	 */
	int err = 0;
	int mul;
	struct bmg_i2c_data *obj = obj_i2c_data;

#ifdef SW_CALIBRATION
	/* only sw calibration, disable hw calibration */
	mul = 0;
#else
	err = bmg_get_hw_offset(client, obj->offset);
	if (err) {
		GYRO_ERROR("read hw offset failed, %d\n", err);
		return err;
	}
	mul = 1;		/* mul = sensor sensitivity / offset sensitivity */
#endif

	raw[BMG_AXIS_X] = obj->offset[BMG_AXIS_X] * mul + obj->cali_sw[BMG_AXIS_X];
	raw[BMG_AXIS_Y] = obj->offset[BMG_AXIS_Y] * mul + obj->cali_sw[BMG_AXIS_Y];
	raw[BMG_AXIS_Z] = obj->offset[BMG_AXIS_Z] * mul + obj->cali_sw[BMG_AXIS_Z];

	act[obj->cvt.map[BMG_AXIS_X]] = obj->cvt.sign[BMG_AXIS_X] * raw[BMG_AXIS_X];
	act[obj->cvt.map[BMG_AXIS_Y]] = obj->cvt.sign[BMG_AXIS_Y] * raw[BMG_AXIS_Y];
	act[obj->cvt.map[BMG_AXIS_Z]] = obj->cvt.sign[BMG_AXIS_Z] * raw[BMG_AXIS_Z];
	return err;
}

static int bmg_write_calibration(struct i2c_client *client, int dat[BMG_AXES_NUM])
{
	/* dat array : Android coordinate system, mapped, unit:LSB */
	int err = 0;
	int cali[BMG_AXES_NUM] = { 0 };
	int raw[BMG_AXES_NUM] = { 0 };
	struct bmg_i2c_data *obj = obj_i2c_data;
	/*offset will be updated in obj->offset */
	err = bmg_read_calibration(client, cali, raw);
	if (err) {
		GYRO_ERROR("read offset fail, %d\n", err);
		return err;
	}
	/* calculate the real offset expected by caller */
	cali[BMG_AXIS_X] += dat[BMG_AXIS_X];
	cali[BMG_AXIS_Y] += dat[BMG_AXIS_Y];
	cali[BMG_AXIS_Z] += dat[BMG_AXIS_Z];
	GYRO_DBG("UPDATE: add mapped data(%+3d %+3d %+3d)\n",
		 dat[BMG_AXIS_X], dat[BMG_AXIS_Y], dat[BMG_AXIS_Z]);

#ifdef SW_CALIBRATION
	/* obj->cali_sw array : chip coordinate system, unmapped,unit:LSB */
	obj->cali_sw[BMG_AXIS_X] = obj->cvt.sign[BMG_AXIS_X] * (cali[obj->cvt.map[BMG_AXIS_X]]);
	obj->cali_sw[BMG_AXIS_Y] = obj->cvt.sign[BMG_AXIS_Y] * (cali[obj->cvt.map[BMG_AXIS_Y]]);
	obj->cali_sw[BMG_AXIS_Z] = obj->cvt.sign[BMG_AXIS_Z] * (cali[obj->cvt.map[BMG_AXIS_Z]]);
#else
	/* divisor = sensor sensitivity / offset sensitivity */
	int divisor = 1;

	obj->offset[BMG_AXIS_X] = (s8) (obj->cvt.sign[BMG_AXIS_X] *
					(cali[obj->cvt.map[BMG_AXIS_X]]) / (divisor));
	obj->offset[BMG_AXIS_Y] = (s8) (obj->cvt.sign[BMG_AXIS_Y] *
					(cali[obj->cvt.map[BMG_AXIS_Y]]) / (divisor));
	obj->offset[BMG_AXIS_Z] = (s8) (obj->cvt.sign[BMG_AXIS_Z] *
					(cali[obj->cvt.map[BMG_AXIS_Z]]) / (divisor));

	/*convert software calibration using standard calibration */
	obj->cali_sw[BMG_AXIS_X] = obj->cvt.sign[BMG_AXIS_X] *
	    (cali[obj->cvt.map[BMG_AXIS_X]]) % (divisor);
	obj->cali_sw[BMG_AXIS_Y] = obj->cvt.sign[BMG_AXIS_Y] *
	    (cali[obj->cvt.map[BMG_AXIS_Y]]) % (divisor);
	obj->cali_sw[BMG_AXIS_Z] = obj->cvt.sign[BMG_AXIS_Z] *
	    (cali[obj->cvt.map[BMG_AXIS_Z]]) % (divisor);
	/* HW calibration is under construction */
	err = bmg_set_hw_offset(client, obj->offset);
	if (err) {
		GYRO_ERROR("read hw offset failed.\n");
		return err;
	}
#endif
	return err;
}

/* get chip type */
static int bmg_get_chip_type(struct i2c_client *client)
{
	int err = 0;
	u8 chip_id = 0;
	struct bmg_i2c_data *obj = obj_i2c_data;
	/* twice */
	mdelay(1);
	err = bmg_i2c_read_block(client, BMI160_USER_CHIP_ID__REG, &chip_id, 1);
	err = bmg_i2c_read_block(client, BMI160_USER_CHIP_ID__REG, &chip_id, 1);
	mdelay(1);
	if (err != 0) {
		GYRO_ERROR("read chip id failed.\n");
		return err;
	}
	switch (chip_id) {
	case SENSOR_CHIP_ID_BMI:
	case SENSOR_CHIP_ID_BMI_C2:
	case SENSOR_CHIP_ID_BMI_C3:
		obj->sensor_type = BMI160_GYRO_TYPE;
		strcpy(obj->sensor_name, BMG_DEV_NAME);
		break;
	default:
		obj->sensor_type = INVALID_TYPE;
		strcpy(obj->sensor_name, UNKNOWN_DEV);
		break;
	}
	if (obj->sensor_type == INVALID_TYPE) {
		GYRO_ERROR("unknown gyroscope.\n");
		return -1;
	}
	return err;
}

static int bmg_set_powermode(struct i2c_client *client, enum BMG_POWERMODE_ENUM power_mode)
{
	int err = 0;
	u8 data = 0;
	u8 actual_power_mode = 0;
	struct bmg_i2c_data *obj = obj_i2c_data;

	if (power_mode == obj->power_mode) {
		return 0;
	}
	mutex_lock(&obj->lock);
	if (obj->sensor_type == BMI160_GYRO_TYPE) {
		if (power_mode == BMG_SUSPEND_MODE) {
			actual_power_mode = CMD_PMU_GYRO_SUSPEND;
		} else if (power_mode == BMG_NORMAL_MODE) {
			actual_power_mode = CMD_PMU_GYRO_NORMAL;
		} else {
			err = -EINVAL;
			GYRO_ERROR("invalid power mode = %d\n", power_mode);
			mutex_unlock(&obj->lock);
			return err;
		}
		data = actual_power_mode;
		err += bmg_i2c_write_block(client, BMI160_CMD_COMMANDS__REG, &data, 1);
		if (power_mode == CMD_PMU_GYRO_NORMAL)
			mdelay(80);
	}
	if (err < 0) {
		GYRO_ERROR("set power mode failed, err = %d, sensor name = %s\n",
			   err, obj->sensor_name);
	} else {
		obj->power_mode = power_mode;
	}
	mutex_unlock(&obj->lock);
	GYRO_DBG("set power mode = %d ok.\n", (int)data);
	return err;
}

static int bmg_set_range(struct i2c_client *client, enum BMG_RANGE_ENUM range)
{
	u8 err = 0;
	u8 data = 0;
	u8 actual_range = 0;
	struct bmg_i2c_data *obj = obj_i2c_data;

	if (range == obj->range) {
		return 0;
	}
	mutex_lock(&obj->lock);
	if (obj->sensor_type == BMI160_GYRO_TYPE) {
		if (range == BMG_RANGE_2000)
			actual_range = BMI160_RANGE_2000;
		else if (range == BMG_RANGE_1000)
			actual_range = BMI160_RANGE_1000;
		else if (range == BMG_RANGE_500)
			actual_range = BMI160_RANGE_500;
		else if (range == BMG_RANGE_500)
			actual_range = BMI160_RANGE_250;
		else if (range == BMG_RANGE_500)
			actual_range = BMI160_RANGE_125;
		else {
			err = -EINVAL;
			GYRO_ERROR("invalid range = %d\n", range);
			mutex_unlock(&obj->lock);
			return err;
		}
		mdelay(1);
		err = bmg_i2c_read_block(client, BMI160_USER_GYR_RANGE__REG, &data, 1);
		data = BMG_SET_BITSLICE(data, BMI160_USER_GYR_RANGE, actual_range);
		mdelay(1);
		err += bmg_i2c_write_block(client, BMI160_USER_GYR_RANGE__REG, &data, 1);
		mdelay(1);
		if (err < 0) {
			GYRO_ERROR("set range failed.\n");
		} else {
			obj->range = range;
			/* bitnum: 16bit */
			switch (range) {
			case BMG_RANGE_2000:
				obj->sensitivity = 16;
				break;
			case BMG_RANGE_1000:
				obj->sensitivity = 33;
				break;
			case BMG_RANGE_500:
				obj->sensitivity = 66;
				break;
			case BMG_RANGE_250:
				obj->sensitivity = 131;
				break;
			case BMG_RANGE_125:
				obj->sensitivity = 262;
				break;
			default:
				obj->sensitivity = 16;
				break;
			}
		}
	}
	mutex_unlock(&obj->lock);
	GYRO_DBG("set range ok.\n");
	return err;
}

static int bmg_set_datarate(struct i2c_client *client, int datarate)
{
	int err = 0;
	u8 data = 0;
	struct bmg_i2c_data *obj = obj_i2c_data;

	if (datarate == obj->datarate) {
		GYRO_DBG("set new data rate = %d, old data rate = %d\n", datarate, obj->datarate);
		return 0;
	}
	mutex_lock(&obj->lock);
	if (obj->sensor_type == BMI160_GYRO_TYPE) {
		mdelay(1);
		err = bmg_i2c_read_block(client, BMI160_USER_GYR_CONF_ODR__REG, &data, 1);
		data = BMG_SET_BITSLICE(data, BMI160_USER_GYR_CONF_ODR, datarate);
		mdelay(1);
		err += bmg_i2c_write_block(client, BMI160_USER_GYR_CONF_ODR__REG, &data, 1);
		mdelay(1);
	}
	if (err < 0) {
		GYRO_ERROR("set data rate failed.\n");
	} else {
		obj->datarate = datarate;
	}
	mutex_unlock(&obj->lock);
	GYRO_DBG("set data rate = %d ok.\n", datarate);
	return err;
}

static int bmg_init_client(struct i2c_client *client, int reset_cali)
{
#ifdef CONFIG_BMG_LOWPASS
	struct bmg_i2c_data *obj = (struct bmg_i2c_data *)obj_i2c_data;
#endif
	int err = 0;

	err = bmg_get_chip_type(client);
	if (err < 0) {
		return err;
	}
	err = bmg_set_datarate(client, BMI160_GYRO_ODR_100HZ);
	if (err < 0) {
		return err;
	}
	err = bmg_set_range(client, (enum BMG_RANGE_ENUM)BMG_RANGE_2000);
	if (err < 0) {
		return err;
	}
	err = bmg_set_powermode(client, (enum BMG_POWERMODE_ENUM)BMG_SUSPEND_MODE);
	if (err < 0) {
		return err;
	}
	if (0 != reset_cali) {
		/*reset calibration only in power on */
		err = bmg_reset_calibration(client);
		if (err < 0) {
			GYRO_ERROR("reset calibration failed.\n");
			return err;
		}
	}
#ifdef CONFIG_BMG_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif
	return 0;
}

/*
*Returns compensated and mapped value. unit is :degree/second
*/
static int bmg_read_sensor_data(struct i2c_client *client, char *buf, int bufsize)
{
	s16 databuf[BMG_AXES_NUM] = { 0 };
	int gyro[BMG_AXES_NUM] = { 0 };
	int err = 0;
	struct bmg_i2c_data *obj = obj_i2c_data;

	err = bmg_read_raw_data(client, databuf);
	if (err) {
		GYRO_ERROR("bmg read raw data failed.\n");
		return err;
	} else {
		/* compensate data */
		databuf[BMG_AXIS_X] += obj->cali_sw[BMG_AXIS_X];
		databuf[BMG_AXIS_Y] += obj->cali_sw[BMG_AXIS_Y];
		databuf[BMG_AXIS_Z] += obj->cali_sw[BMG_AXIS_Z];

		/* remap coordinate */
		gyro[obj->cvt.map[BMG_AXIS_X]] = obj->cvt.sign[BMG_AXIS_X] * databuf[BMG_AXIS_X];
		gyro[obj->cvt.map[BMG_AXIS_Y]] = obj->cvt.sign[BMG_AXIS_Y] * databuf[BMG_AXIS_Y];
		gyro[obj->cvt.map[BMG_AXIS_Z]] = obj->cvt.sign[BMG_AXIS_Z] * databuf[BMG_AXIS_Z];

		/* convert: LSB -> degree/second(o/s) */
		gyro[BMG_AXIS_X] = gyro[BMG_AXIS_X] * BMI160_FS_MAX_LSB / obj->sensitivity;
		gyro[BMG_AXIS_Y] = gyro[BMG_AXIS_Y] * BMI160_FS_MAX_LSB / obj->sensitivity;
		gyro[BMG_AXIS_Z] = gyro[BMG_AXIS_Z] * BMI160_FS_MAX_LSB / obj->sensitivity;
		GYRO_LOG("gyro final xyz data: %d,%d,%d, sens:%d\n",
			 gyro[0], gyro[1], gyro[2], obj->sensitivity);

		sprintf(buf, "%04x %04x %04x",
			gyro[BMG_AXIS_X], gyro[BMG_AXIS_Y], gyro[BMG_AXIS_Z]);
		if (atomic_read(&obj->trace) & GYRO_TRC_IOCTL)
			GYRO_DBG("gyroscope data: %s\n", buf);
	}
	return 0;
}

static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct bmg_i2c_data *obj = obj_i2c_data;

	return snprintf(buf, PAGE_SIZE, "%s\n", obj->sensor_name);
}

/*
* sensor data format is hex, unit:degree/second
*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct bmg_i2c_data *obj = obj_i2c_data;
	char strbuf[BMG_BUFSIZE] = { 0 };

	bmg_read_sensor_data(obj->client, strbuf, BMG_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*
* raw data format is s16, unit:LSB, axis mapped
*/
static ssize_t show_rawdata_value(struct device_driver *ddri, char *buf)
{
	s16 databuf[BMG_AXES_NUM] = { 0 };
	s16 dataraw[BMG_AXES_NUM] = { 0 };
	struct bmg_i2c_data *obj = obj_i2c_data;

	bmg_read_raw_data(obj->client, dataraw);
	/*remap coordinate */
	databuf[obj->cvt.map[BMG_AXIS_X]] = obj->cvt.sign[BMG_AXIS_X] * dataraw[BMG_AXIS_X];
	databuf[obj->cvt.map[BMG_AXIS_Y]] = obj->cvt.sign[BMG_AXIS_Y] * dataraw[BMG_AXIS_Y];
	databuf[obj->cvt.map[BMG_AXIS_Z]] = obj->cvt.sign[BMG_AXIS_Z] * dataraw[BMG_AXIS_Z];
	return snprintf(buf, PAGE_SIZE, "%hd %hd %hd\n",
			databuf[BMG_AXIS_X], databuf[BMG_AXIS_Y], databuf[BMG_AXIS_Z]);
}

static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
	int err = 0;
	int len = 0;
	int mul;
	int act[BMG_AXES_NUM] = { 0 };
	int raw[BMG_AXES_NUM] = { 0 };
	struct bmg_i2c_data *obj = obj_i2c_data;

	err = bmg_read_calibration(obj->client, act, raw);
	if (err) {
		return -EINVAL;
	} else {
		mul = 1;	/* mul = sensor sensitivity / offset sensitivity */
		len += snprintf(buf + len, PAGE_SIZE - len,
				"[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n",
				mul,
				obj->offset[BMG_AXIS_X],
				obj->offset[BMG_AXIS_Y],
				obj->offset[BMG_AXIS_Z],
				obj->offset[BMG_AXIS_X],
				obj->offset[BMG_AXIS_Y], obj->offset[BMG_AXIS_Z]);
		len += snprintf(buf + len, PAGE_SIZE - len,
				"[SW ][%d] (%+3d, %+3d, %+3d)\n", 1,
				obj->cali_sw[BMG_AXIS_X],
				obj->cali_sw[BMG_AXIS_Y], obj->cali_sw[BMG_AXIS_Z]);
		len += snprintf(buf + len, PAGE_SIZE - len,
				"[ALL]unmapped(%+3d, %+3d, %+3d), mapped(%+3d, %+3d, %+3d)\n",
				raw[BMG_AXIS_X], raw[BMG_AXIS_Y], raw[BMG_AXIS_Z],
				act[BMG_AXIS_X], act[BMG_AXIS_Y], act[BMG_AXIS_Z]);
		return len;
	}
}

static ssize_t store_cali_value(struct device_driver *ddri, const char *buf, size_t count)
{
	int err = 0;
	int dat[BMG_AXES_NUM] = { 0 };
	struct bmg_i2c_data *obj = obj_i2c_data;

	if (!strncmp(buf, "rst", 3)) {
		err = bmg_reset_calibration(obj->client);
		if (err)
			GYRO_ERROR("reset offset err = %d\n", err);
	} else if (BMG_AXES_NUM == sscanf(buf, "0x%02X 0x%02X 0x%02X",
					  &dat[BMG_AXIS_X], &dat[BMG_AXIS_Y], &dat[BMG_AXIS_Z])) {
		err = bmg_write_calibration(obj->client, dat);
		if (err) {
			GYRO_ERROR("bmg write calibration failed, err = %d\n", err);
		}
	} else {
		GYRO_ERROR("invalid format\n");
	}
	return count;
}

static ssize_t show_firlen_value(struct device_driver *ddri, char *buf)
{
#ifdef CONFIG_BMG_LOWPASS
	struct i2c_client *client = bmg222_i2c_client;
	struct bmg_i2c_data *obj = obj_i2c_data;

	if (atomic_read(&obj->firlen)) {
		int idx, len = atomic_read(&obj->firlen);

		GYRO_DBG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);

		for (idx = 0; idx < len; idx++) {
			GYRO_DBG("[%5d %5d %5d]\n",
				 obj->fir.raw[idx][BMG_AXIS_X],
				 obj->fir.raw[idx][BMG_AXIS_Y], obj->fir.raw[idx][BMG_AXIS_Z]);
		}

		GYRO_DBG("sum = [%5d %5d %5d]\n",
			 obj->fir.sum[BMG_AXIS_X],
			 obj->fir.sum[BMG_AXIS_Y], obj->fir.sum[BMG_AXIS_Z]);
		GYRO_DBG("avg = [%5d %5d %5d]\n",
			 obj->fir.sum[BMG_AXIS_X] / len,
			 obj->fir.sum[BMG_AXIS_Y] / len, obj->fir.sum[BMG_AXIS_Z] / len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->firlen));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}

static ssize_t store_firlen_value(struct device_driver *ddri, const char *buf, size_t count)
{
#ifdef CONFIG_BMG_LOWPASS
	struct i2c_client *client = bmg222_i2c_client;
	struct bmg_i2c_data *obj = obj_i2c_data;
	int firlen;

	if (1 != sscanf(buf, "%d", &firlen)) {
		GYRO_ERROR("invallid format\n");
	} else if (firlen > C_MAX_FIR_LENGTH) {
		GYRO_ERROR("exceeds maximum filter length\n");
	} else {
		atomic_set(&obj->firlen, firlen);
		if (NULL == firlen) {
			atomic_set(&obj->fir_en, 0);
		} else {
			memset(&obj->fir, 0x00, sizeof(obj->fir));
			atomic_set(&obj->fir_en, 1);
		}
	}
#endif

	return count;
}

static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct bmg_i2c_data *obj = obj_i2c_data;

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	int trace;
	struct bmg_i2c_data *obj = obj_i2c_data;

	if (1 == sscanf(buf, "0x%x", &trace))
		atomic_set(&obj->trace, trace);
	else
		GYRO_ERROR("invalid content: '%s'\n", buf);
	return count;
}

static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct bmg_i2c_data *obj = obj_i2c_data;

	if (obj->hw)
		len += snprintf(buf + len, PAGE_SIZE - len,
				"CUST: %d %d (%d %d)\n",
				obj->hw->i2c_num, obj->hw->direction,
				obj->hw->power_id, obj->hw->power_vol);
	else
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");

	len += snprintf(buf + len, PAGE_SIZE - len, "i2c addr:%#x,ver:%s\n",
			obj->client->addr, BMG_DRIVER_VERSION);
	return len;
}

static ssize_t show_power_mode_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct bmg_i2c_data *obj = obj_i2c_data;

	len += snprintf(buf + len, PAGE_SIZE - len, "%s mode\n",
			obj->power_mode == BMG_NORMAL_MODE ? "normal" : "suspend");
	return len;
}

static ssize_t store_power_mode_value(struct device_driver *ddri, const char *buf, size_t count)
{
	int err;
	unsigned long power_mode;
	struct bmg_i2c_data *obj = obj_i2c_data;

	err = kstrtoul(buf, 10, &power_mode);
	if (err < 0) {
		return err;
	}
	if (BMI_GYRO_PM_NORMAL == power_mode) {
		err = bmg_set_powermode(obj->client, BMG_NORMAL_MODE);
	} else {
		err = bmg_set_powermode(obj->client, BMG_SUSPEND_MODE);
	}
	if (err < 0) {
		GYRO_ERROR("set power mode failed.\n");
	}
	return err;
}

static ssize_t show_range_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct bmg_i2c_data *obj = obj_i2c_data;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", obj->range);
	return len;
}

static ssize_t store_range_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct bmg_i2c_data *obj = obj_i2c_data;
	unsigned long range;
	int err;

	err = kstrtoul(buf, 10, &range);
	if (err == 0) {
		if ((range == BMG_RANGE_2000)
		    || (range == BMG_RANGE_1000)
		    || (range == BMG_RANGE_500)
		    || (range == BMG_RANGE_250)
		    || (range == BMG_RANGE_125)) {
			err = bmg_set_range(obj->client, range);
			if (err) {
				GYRO_ERROR("set range value failed.\n");
				return err;
			}
		}
	}
	return count;
}

static ssize_t show_datarate_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct bmg_i2c_data *obj = obj_i2c_data;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", obj->datarate);
	return len;
}

static ssize_t store_datarate_value(struct device_driver *ddri, const char *buf, size_t count)
{
	int err;
	unsigned long datarate;
	struct bmg_i2c_data *obj = obj_i2c_data;

	err = kstrtoul(buf, 10, &datarate);
	if (err < 0) {
		return err;
	}
	err = bmg_set_datarate(obj->client, datarate);
	if (err < 0) {
		GYRO_ERROR("set data rate failed.\n");
		return err;
	}
	return count;
}

static int bmi160_set_command_register(u8 cmd_reg)
{
	int comres = 0;
	struct i2c_client *client = bmi160_acc_i2c_client;

	comres += bma_i2c_write_block(client, BMI160_CMD_COMMANDS__REG, &cmd_reg, 1);
	return comres;
}


BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_selftest_start(u8 v_gyro_selftest_start_u8)
{
	struct bmg_i2c_data *obj = obj_i2c_data;

	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (obj->client == BMI160_NULL)
	{
		return E_BMI160_NULL_PTR;
	}
	else
	{
		if (v_gyro_selftest_start_u8 <=
			BMI160_MAX_VALUE_SELFTEST_START)
		{
			/* write gyro self test start */
			com_rslt = bmg_i2c_read_block(
			obj->client,
			BMI160_USER_GYRO_SELFTEST_START__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS)
			{
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
					BMI160_USER_GYRO_SELFTEST_START,
					v_gyro_selftest_start_u8);
				com_rslt += bmg_i2c_write_block(
					obj->client,
					BMI160_USER_GYRO_SELFTEST_START__REG,
					&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		}
		else
		{
			com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}

	return com_rslt;
}

BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_selftest(u8*v_gyro_selftest_u8)
{
	struct bmg_i2c_data *obj = obj_i2c_data;

	/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL*/
	if (obj->client == BMI160_NULL)
	{
		return E_BMI160_NULL_PTR;
	}
	else
	{
		com_rslt =
			bmg_i2c_read_block(obj->client,
			BMI160_USER_STAT_GYRO_SELFTEST_OK__REG,
			&v_data_u8, BMI160_GEN_READ_WRITE_DATA_LENGTH);
		*v_gyro_selftest_u8 = BMI160_GET_BITSLICE(v_data_u8,
			BMI160_USER_STAT_GYRO_SELFTEST_OK);
	}

	return com_rslt;
}

static ssize_t set_gyro_self_test(struct device_driver *ddri, char *buf)
{
	int err = 0;

	u8 gyro_selftest = 0;
	u8 bmi_selftest = 0;

	gyro_self_test_flag = -1;

	/*soft reset*/
	err = bmi160_set_command_register(CMD_RESET_USER_REG);
	if (err < 0) {
		GSE_ERR("[Gyro Sensor] bmi160_set_command_register, CMD_RESET_USER_REG\n");
		gyro_self_test_flag = 1;
		return BMI160_ACC_ERR_I2C;
	}

	msleep(70);

	err = bmi160_set_command_register(CMD_PMU_GYRO_NORMAL);
	if (err < 0) {
		GSE_ERR("[Gyro Sensor] bmi160_set_command_register, CMD_PMU_GYRO_NORMAL\n");
		gyro_self_test_flag = 2;
		return BMI160_ACC_ERR_I2C;
	}
	mdelay(80);

	/* start gyro self test */
	err = bmi160_set_gyro_selftest_start(1);
	if (err < 0) {
		GSE_ERR("[Gyro Sensor] bmi160_set_gyro_selftest_start failed\n");
		gyro_self_test_flag = 3;
		return BMI160_ACC_ERR_I2C;
	}

	msleep(60);

	err = bmi160_get_gyro_selftest(&gyro_selftest);
	if (err < 0) {
		GSE_ERR("[Gyro Sensor] bmi160_get_gyro_selftest failed\n");
		gyro_self_test_flag = 4;
		return BMI160_ACC_ERR_I2C;
	}

	bmi_selftest = (!gyro_selftest) << AXIS_MAX;

	if(bmi_selftest != 0)
	{
		gyro_self_test_flag = 5;
		return sprintf(buf, "[Gyro Sensor] set_gyro_self_test FAIL, %x\n", bmi_selftest);
	}

	/*soft reset*/
	err = bmi160_set_command_register(CMD_RESET_USER_REG);
	if (err)
		return err;

	msleep(50);

	gyro_self_test_flag = 0;
	return sprintf(buf, "[Gyro Sensor] set_gyro_self_test PASS\n");
}

static ssize_t get_gyro_self_test(struct device_driver *ddri, char *buf)
{
    if (gyro_self_test_flag == 1)
    {
        return snprintf(buf, PAGE_SIZE, "X out of range\nFail\n");
    }
    else if (gyro_self_test_flag == 2)
    {
        return snprintf(buf, PAGE_SIZE, "Y out of range\nFail\n");
    }
    else if (gyro_self_test_flag == 3)
    {
        return snprintf(buf, PAGE_SIZE, "Z out of range\nFail\n");
    }
    else if (gyro_self_test_flag == 4 || gyro_self_test_flag == 5)
    {
        return snprintf(buf, PAGE_SIZE, "Gyro self test\nFail\n");
    }
    else if ( gyro_self_test_flag == 0 )
    {
        return snprintf(buf, PAGE_SIZE, "Gyro self test\nPass\n");
    }
    else
    {
        return snprintf(buf, PAGE_SIZE, "Gyro self test\nFail\n");
    }
}

static int gyro_store_offset_in_file(const char *filename, s16 *offset, int data_valid)
{
    struct file *cali_file;
    mm_segment_t fs;
    char w_buf[BMI160_DATA_BUF_NUM*sizeof(s16)*2+1] = {0} , r_buf[BMI160_DATA_BUF_NUM*sizeof(s16)*2+1] = {0};
    int i;
    char* dest = w_buf;

           GYRO_LOG("%s enter \n", __FUNCTION__ );

    cali_file = filp_open(filename, O_CREAT | O_RDWR, 0777);

    if(IS_ERR(cali_file))
    {
        //pr_info("[Sensor] %s: filp_open error!\n", __FUNCTION__);
        GYRO_LOG("%s open error! exit! \n", __FUNCTION__ );
        return -1;
    }
    else
    {
        fs = get_fs();
        set_fs(get_ds());


           for (i = 0; i<BMI160_DATA_BUF_NUM; i++)
                     {
                                sprintf(dest,  "%02X", offset[i] & 0x00FF);
                                dest += 2;
                                sprintf(dest,  "%02X", (offset[i] >> 8) & 0x00FF);
                                dest += 2;
                     };
           GYRO_LOG("w_buf: %s \n", w_buf );

        cali_file->f_pos=BMI160_DATA_BUF_NUM *sizeof(s16)*2;
        cali_file->f_op->write(cali_file, (void *)w_buf, BMI160_DATA_BUF_NUM*sizeof(s16)*2+1, &cali_file->f_pos);
        cali_file->f_pos=BMI160_DATA_BUF_NUM *sizeof(s16)*2;
        cali_file->f_op->read(cali_file, (void *)r_buf, BMI160_DATA_BUF_NUM*sizeof(s16)*2,&cali_file->f_pos);
        for (i=0; i<BMI160_DATA_BUF_NUM*sizeof(s16)*2; i++)
        {
            if(r_buf[i] != w_buf[i])
            {
                filp_close(cali_file,NULL);
                    GYRO_LOG("%s read back error! exit! \n", __FUNCTION__ );
                return -1;
            }
        }
        set_fs(fs);
    }

    filp_close(cali_file,NULL);

           GYRO_LOG("%s ok exit \n", __FUNCTION__ );
    return 0;
}

static ssize_t get_gyro_cali(struct device_driver *ddri, char *buf)
{

    if (gyro_xyz_offset[0]>GYRO_MAX_OFFSET_VAL || gyro_xyz_offset[0]<GYRO_MIN_OFFSET_VAL)
    {
        return sprintf(buf, "offset_x=%d , out of range\nFail\n", gyro_xyz_offset[0]);
    }
    else if (gyro_xyz_offset[1]>GYRO_MAX_OFFSET_VAL || gyro_xyz_offset[1]<GYRO_MIN_OFFSET_VAL)
    {
        return sprintf(buf, "offset_y=%d , out of range\nFail\n", gyro_xyz_offset[1]);
    }
    else if (gyro_xyz_offset[2]>GYRO_MAX_OFFSET_VAL || gyro_xyz_offset[2]<GYRO_MIN_OFFSET_VAL)
    {
        return sprintf(buf, "offset_z=%d , out of range\nFail\n", gyro_xyz_offset[2]);
    }

    return sprintf(buf, "x=%d , y=%d , z=%d\nPass\n", gyro_xyz_offset[0], gyro_xyz_offset[1], gyro_xyz_offset[2] );
}

static ssize_t set_gyro_cali(struct device_driver *ddri, char *buf)
{
    int err;
    int i;
    u8 data[BMI160_CALI_DATA_NUM+BMI160_CALI_DATA_BYPASS_NUM][6] = {{0}};
    static int xyz[BMI160_CALI_DATA_NUM+BMI160_CALI_DATA_BYPASS_NUM][3] = {{0}};
    int sum[3]= {0} , offset[3]= {0};
    s16 avg[3]= {0};
    struct i2c_client *client = bmi160_acc_i2c_client;

    gyro_xyz_offset[0] = gyro_xyz_offset[1] = gyro_xyz_offset[2] = 0;

    //u8 addr = BMI160_USER_DATA_8_GYR_X_LSB__REG;

    err = bmg_set_powermode(client, (enum BMG_POWERMODE_ENUM)BMG_NORMAL_MODE);
    if (err)
    {
        GYRO_ERROR("[Gyro Sensor] enable power fail\n");
        return sprintf(buf, "[Gyro Sensor] enable power fail\n");
    }

    /* wait 800ms for stable output */
    msleep(800);

    for (i=0; i<BMI160_CALI_DATA_NUM+BMI160_CALI_DATA_BYPASS_NUM; i++)
    {
        err = bmg_i2c_read_block(client, BMI160_USER_DATA_8_GYR_X_LSB__REG, data[i], BMI160_GYRO_DATA_LEN);
        if (err < 0) {
            GYRO_ERROR("read data failed.\n");
        } else {
	        xyz[i][0] = (s32)((s16)(data[i][0] | (data[i][1] << 8)));
	        xyz[i][1] = (s32)((s16)(data[i][2] | (data[i][3] << 8)));
	        xyz[i][2] = (s32)((s16)(data[i][4] | (data[i][5] << 8)));
	}

        if (i>=BMI160_CALI_DATA_BYPASS_NUM)
        {
            sum[0] += (s32)xyz[i][0];
            sum[1] += (s32)xyz[i][1];
            sum[2] += (s32)xyz[i][2];
        }
        msleep(40);    // 26HZ = 38ms update one data
    }

    err = bmg_set_powermode(client, (enum BMG_POWERMODE_ENUM)BMG_SUSPEND_MODE);
    if (err)
    {
        GYRO_ERROR("[Gyro Sensor] disable power fail\n");
        return sprintf(buf, "[Gyro Sensor] disable power fail\n");
    }

    avg[0] = (s16)(sum[0] / BMI160_CALI_DATA_NUM);
    avg[1] = (s16)(sum[1] / BMI160_CALI_DATA_NUM);
    avg[2] = (s16)(sum[2] / BMI160_CALI_DATA_NUM);

    if (avg[0]>GYRO_MAX_OFFSET_VAL || avg[0]<GYRO_MIN_OFFSET_VAL)
    {
        offset[0] = 0 - avg[0];
        offset[1] = 0 - avg[1];
        offset[2] = 0 - avg[2];

        return sprintf(buf, "[Gyro Sensor] %s: BMI160 X axis out of range %d\n", __FUNCTION__, avg[0]);
    }
    else if (avg[1]>GYRO_MAX_OFFSET_VAL || avg[1]<GYRO_MIN_OFFSET_VAL)
    {
        offset[0] = 0 - avg[0];
        offset[1] = 0 - avg[1];
        offset[2] = 0 - avg[2];

        return sprintf(buf, "[Gyro Sensor] %s: BMI160 Y axis out of range %d\n", __FUNCTION__, avg[1]);
    }
    else if (avg[2]>GYRO_MAX_OFFSET_VAL || avg[2]<GYRO_MIN_OFFSET_VAL)
    {
        offset[0] = 0 - avg[0];
        offset[1] = 0 - avg[1];
        offset[2] = 0 - avg[2];

        return sprintf(buf, "[Gyro Sensor] %s: BMI160 Z axis out of range %d\n", __FUNCTION__, avg[2]);
    }

    offset[0] = 0 - avg[0];
    offset[1] = 0 - avg[1];
    offset[2] = 0 - avg[2];

    gyro_xyz_offset[0] = offset[0];
    gyro_xyz_offset[1] = offset[1];
    gyro_xyz_offset[2] = offset[2];

    if(gyro_store_offset_in_file(BMI160_GYRO_CALI_FILE, gyro_xyz_offset, 1))
        {
            return sprintf(buf, "[Gyro Sensor] BMI160 set_gyro_cali ERROR %d, %d, %d\n",
        gyro_xyz_offset[0],
        gyro_xyz_offset[1],
        gyro_xyz_offset[2]);
        }

    return sprintf(buf, "[Gyro Sensor] set_gyro_cali PASS %d, %d, %d\n",
        gyro_xyz_offset[0],
        gyro_xyz_offset[1],
        gyro_xyz_offset[2]);
}

static DRIVER_ATTR(chipinfo, S_IWUSR | S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, S_IWUSR | S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(rawdata, S_IWUSR | S_IRUGO, show_rawdata_value, NULL);
static DRIVER_ATTR(cali, S_IWUSR | S_IRUGO, show_cali_value, store_cali_value);
static DRIVER_ATTR(firlen, S_IWUSR | S_IRUGO, show_firlen_value, store_firlen_value);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);
static DRIVER_ATTR(status, S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(gyro_op_mode, S_IWUSR | S_IRUGO, show_power_mode_value, store_power_mode_value);
static DRIVER_ATTR(gyro_range, S_IWUSR | S_IRUGO, show_range_value, store_range_value);
static DRIVER_ATTR(gyro_odr, S_IWUSR | S_IRUGO, show_datarate_value, store_datarate_value);

static DRIVER_ATTR(gyrogetselftest, S_IWUSR | S_IRUGO, get_gyro_self_test, NULL);
static DRIVER_ATTR(gyrosetselftest, S_IWUSR | S_IRUGO, set_gyro_self_test, NULL);
static DRIVER_ATTR(gyrogetcali, S_IWUSR | S_IRUGO, get_gyro_cali, NULL);
static DRIVER_ATTR(gyrosetcali, S_IWUSR | S_IRUGO, set_gyro_cali, NULL);

static struct driver_attribute *bmg_attr_list[] = {
	/* chip information */
	&driver_attr_chipinfo,
	/* dump sensor data */
	&driver_attr_sensordata,
	/* dump raw data */
	&driver_attr_rawdata,
	/* show calibration data */
	&driver_attr_cali,
	/* filter length: 0: disable, others: enable */
	&driver_attr_firlen,
	/* trace flag */
	&driver_attr_trace,
	/* get hw configuration */
	&driver_attr_status,
	/* get power mode */
	&driver_attr_gyro_op_mode,
	/* get range */
	&driver_attr_gyro_range,
	/* get data rate */
	&driver_attr_gyro_odr,
	/* get self test */
	&driver_attr_gyrogetselftest,
	&driver_attr_gyrosetselftest,
	&driver_attr_gyrogetcali,
	&driver_attr_gyrosetcali,
};

static int bmg_create_attr(struct device_driver *driver)
{
	int idx = 0;
	int err = 0;
	int num = (int)(sizeof(bmg_attr_list) / sizeof(bmg_attr_list[0]));

	if (driver == NULL) {
		return -EINVAL;
	}
	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, bmg_attr_list[idx]);
		if (err) {
			GYRO_ERROR("driver_create_file (%s) = %d\n",
				   bmg_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int bmg_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(bmg_attr_list) / sizeof(bmg_attr_list[0]));

	for (idx = 0; idx < num; idx++) {
		driver_remove_file(driver, bmg_attr_list[idx]);
	}
	return err;
}

int gyroscope_operate(void *self, uint32_t command, void *buff_in, int size_in,
		      void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value, sample_delay;
	char buff[BMG_BUFSIZE] = { 0 };
	struct bmg_i2c_data *priv = (struct bmg_i2c_data *)self;
	struct hwm_sensor_data *gyroscope_data;

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			GYRO_ERROR("set delay parameter error\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;

			/*
			 *Currently, fix data rate to 100Hz.
			 */
			sample_delay = BMI160_GYRO_ODR_100HZ;

			GYRO_DBG("sensor delay command: %d, sample_delay = %d\n",
				 value, sample_delay);

			err = bmg_set_datarate(priv->client, sample_delay);
			if (err < 0)
				GYRO_ERROR("set delay parameter error\n");

			if (value >= 40)
				atomic_set(&priv->filter, 0);
			else {
#if defined(CONFIG_BMG_LOWPASS)
				priv->fir.num = 0;
				priv->fir.idx = 0;
				priv->fir.sum[BMG_AXIS_X] = 0;
				priv->fir.sum[BMG_AXIS_Y] = 0;
				priv->fir.sum[BMG_AXIS_Z] = 0;
				atomic_set(&priv->filter, 1);
#endif
			}
		}
		break;
	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			GYRO_ERROR("enable sensor parameter error\n");
			err = -EINVAL;
		} else {
			/* value:[0--->suspend, 1--->normal] */
			value = *(int *)buff_in;
			GYRO_DBG("sensor enable/disable command: %s\n",
				 value ? "enable" : "disable");

			err = bmg_set_powermode(priv->client, (enum BMG_POWERMODE_ENUM)(!!value));
			if (err)
				GYRO_ERROR("set power mode failed, err = %d\n", err);
		}
		break;
	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(struct hwm_sensor_data))) {
			GYRO_ERROR("get sensor data parameter error\n");
			err = -EINVAL;
		} else {
			gyroscope_data = (struct hwm_sensor_data *)buff_out;
			bmg_read_sensor_data(priv->client, buff, BMG_BUFSIZE);
			sscanf(buff, "%x %x %x", &gyroscope_data->values[0],
			       &gyroscope_data->values[1], &gyroscope_data->values[2]);
			gyroscope_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
			gyroscope_data->value_divide = DEGREE_TO_RAD;
		}
		break;
	default:
		GYRO_ERROR("gyroscope operate function no this parameter %d\n", command);
		err = -1;
		break;
	}

	return err;
}

static int bmi160_gyro_open(struct inode *inode, struct file *file)
{
	file->private_data = obj_i2c_data;
	if (file->private_data == NULL) {
		GYRO_ERROR("file->private_data is null pointer.\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

static int bmi160_gyro_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long bmi160_gyro_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	char strbuf[BMG_BUFSIZE] = { 0 };
	int raw_offset[BMG_BUFSIZE] = { 0 };
	void __user *data;
	struct SENSOR_DATA sensor_data;
	int cali[BMG_AXES_NUM] = { 0 };
	struct bmg_i2c_data *obj = (struct bmg_i2c_data *)file->private_data;
	struct i2c_client *client = obj->client;

	if (obj == NULL)
		return -EFAULT;
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err) {
		GYRO_ERROR("access error: %08x, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}
	switch (cmd) {
	case GYROSCOPE_IOCTL_INIT:
		bmg_init_client(client, 0);
		err = bmg_set_powermode(client, BMG_NORMAL_MODE);
		if (err) {
			err = -EFAULT;
			break;
		}
		break;
	case GYROSCOPE_IOCTL_READ_SENSORDATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		bmg_read_sensor_data(client, strbuf, BMG_BUFSIZE);
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;
	case GYROSCOPE_IOCTL_SET_CALI:
		/* data unit is degree/second */
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if (copy_from_user(&sensor_data, data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		if (atomic_read(&obj->suspend)) {
			GYRO_ERROR("perform calibration in suspend mode\n");
			err = -EINVAL;
		} else {
			/* convert: degree/second -> LSB */
			cali[BMG_AXIS_X] = sensor_data.x * obj->sensitivity / BMI160_FS_MAX_LSB;
			cali[BMG_AXIS_Y] = sensor_data.y * obj->sensitivity / BMI160_FS_MAX_LSB;
			cali[BMG_AXIS_Z] = sensor_data.z * obj->sensitivity / BMI160_FS_MAX_LSB;
			err = bmg_write_calibration(client, cali);
		}
		break;
	case GYROSCOPE_IOCTL_CLR_CALI:
		err = bmg_reset_calibration(client);
		break;
	case GYROSCOPE_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = bmg_read_calibration(client, cali, raw_offset);
		if (err)
			break;

		sensor_data.x = cali[BMG_AXIS_X] * BMI160_FS_MAX_LSB / obj->sensitivity;
		sensor_data.y = cali[BMG_AXIS_Y] * BMI160_FS_MAX_LSB / obj->sensitivity;
		sensor_data.z = cali[BMG_AXIS_Z] * BMI160_FS_MAX_LSB / obj->sensitivity;
		if (copy_to_user(data, &sensor_data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		break;
	default:
		GYRO_ERROR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}
	return err;
}

#ifdef CONFIG_COMPAT
static long bmi160_gyro_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;

	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	/* GYRO_DBG("bmi160_gyro_compat_ioctl, cmd = 0x%x\n", cmd); */

	switch (cmd) {
	case COMPAT_GYROSCOPE_IOCTL_INIT:
		if (arg32 == NULL) {
			GYRO_ERROR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_INIT, (unsigned long)arg32);
		if (ret) {
			GYRO_ERROR("GYROSCOPE_IOCTL_INIT unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_SET_CALI:
		if (arg32 == NULL) {
			GYRO_ERROR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_SET_CALI,
						 (unsigned long)arg32);
		if (ret) {
			GYRO_ERROR("GYROSCOPE_IOCTL_SET_CALI unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_CLR_CALI:
		if (arg32 == NULL) {
			GYRO_ERROR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_CLR_CALI,
						 (unsigned long)arg32);
		if (ret) {
			GYRO_ERROR("GYROSCOPE_IOCTL_CLR_CALI unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_GET_CALI:
		if (arg32 == NULL) {
			GYRO_ERROR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_GET_CALI,
						 (unsigned long)arg32);
		if (ret) {
			GYRO_ERROR("GYROSCOPE_IOCTL_GET_CALI unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_READ_SENSORDATA:
		if (arg32 == NULL) {
			GYRO_ERROR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_READ_SENSORDATA,
						 (unsigned long)arg32);
		if (ret) {
			GYRO_ERROR("GYROSCOPE_IOCTL_READ_SENSORDATA unlocked_ioctl failed.");
			return ret;
		}

		break;

	default:
		GYRO_ERROR("0x%04x cmd is not supported!", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}
#endif

static const struct file_operations bmg_fops = {
	.owner = THIS_MODULE,
	.open = bmi160_gyro_open,
	.release = bmi160_gyro_release,
	.unlocked_ioctl = bmi160_gyro_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = bmi160_gyro_compat_ioctl,
#endif
};

static struct miscdevice bmg_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gyroscope",
	.fops = &bmg_fops,
};

static int bmg_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct bmg_i2c_data *obj = obj_i2c_data;
	int err = 0;

	if (msg.event == PM_EVENT_SUSPEND) {
		if (obj == NULL) {
			GYRO_ERROR("null pointer\n");
			return -EINVAL;
		}
		atomic_set(&obj->suspend, 1);
		err = bmg_set_powermode(obj->client, BMG_SUSPEND_MODE);
		if (err) {
			GYRO_ERROR("bmg set suspend mode failed.\n");
		}
	}
	return err;
}

static int bmg_resume(struct i2c_client *client)
{
	int err;
	struct bmg_i2c_data *obj = obj_i2c_data;

	err = bmg_init_client(client, 0);
	if (err) {
		GYRO_ERROR("initialize client failed, err = %d\n", err);
		return err;
	}
	atomic_set(&obj->suspend, 0);
	return 0;
}

static int bmg_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, BMG_DEV_NAME);
	return 0;
}

static int bmi160_gyro_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

static int bmi160_gyro_enable_nodata(int en)
{
	int err = 0;
	int retry = 0;
	bool power = false;

	if (1 == en) {
		power = true;
	} else {
		power = false;
	}
	for (retry = 0; retry < 3; retry++) {
		err = bmg_set_powermode(obj_i2c_data->client, power);
		if (err == 0) {
			GYRO_DBG("bmi160_gyro_SetPowerMode ok.\n");
			break;
		}
	}
	if (err < 0) {
		GYRO_DBG("bmi160_gyro_SetPowerMode fail!\n");
	}
	return err;
}

static int bmi160_gyro_set_delay(u64 ns)
{
	int err;
	int value = (int)ns / 1000 / 1000;
	/* Currently, fix data rate to 100Hz. */
	int sample_delay = BMI160_GYRO_ODR_100HZ;
	struct bmg_i2c_data *priv = obj_i2c_data;

	err = bmg_set_datarate(priv->client, sample_delay);
	if (err < 0)
		GYRO_ERROR("set data rate failed.\n");
	if (value >= 40)
		atomic_set(&priv->filter, 0);
	else {
#if defined(CONFIG_BMG_LOWPASS)
		priv->fir.num = 0;
		priv->fir.idx = 0;
		priv->fir.sum[BMG_AXIS_X] = 0;
		priv->fir.sum[BMG_AXIS_Y] = 0;
		priv->fir.sum[BMG_AXIS_Z] = 0;
		atomic_set(&priv->filter, 1);
#endif
	}
	GYRO_DBG("set gyro delay = %d\n", sample_delay);
	return 0;
}

static int bmi160_gyro_get_data(int *x, int *y, int *z, int *status)
{
	char buff[BMG_BUFSIZE] = { 0 };

	bmg_read_sensor_data(obj_i2c_data->client, buff, BMG_BUFSIZE);
	sscanf(buff, "%x %x %x", x, y, z);
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	return 0;
}

static int bmg_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	unsigned int ret=0;
	s16 idmedata[6] = {0};
	struct bmg_i2c_data *obj;
	struct gyro_control_path ctl = { 0 };
	struct gyro_data_path data = { 0 };

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}
	obj->hw = hw;
	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if (err) {
		GYRO_ERROR("invalid direction: %d\n", obj->hw->direction);
		goto exit_hwmsen_get_convert_failed;
	}
	obj_i2c_data = obj;
	obj->client = bmi160_acc_i2c_client;
	i2c_set_clientdata(obj->client, obj);
	client->addr = BMI160_I2C_ADDR;
	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	obj->power_mode = BMG_UNDEFINED_POWERMODE;
	obj->range = BMG_UNDEFINED_RANGE;
	obj->datarate = BMI160_GYRO_ODR_RESERVED;
	mutex_init(&obj->lock);
#ifdef CONFIG_BMG_LOWPASS
	if (obj->hw->firlen > C_MAX_FIR_LENGTH)
		atomic_set(&obj->firlen, C_MAX_FIR_LENGTH);
	else
		atomic_set(&obj->firlen, obj->hw->firlen);

	if (atomic_read(&obj->firlen) > 0)
		atomic_set(&obj->fir_en, 1);
#endif
	err = bmg_init_client(obj->client, 1);
	if (err)
		goto exit_init_client_failed;

	err = misc_register(&bmg_device);
	if (err) {
		GYRO_ERROR("misc device register failed, err = %d\n", err);
		goto exit_misc_device_register_failed;
	}
	err = bmg_create_attr(&bmi160_gyro_init_info.platform_diver_addr->driver);
	if (err) {
		GYRO_ERROR("create attribute failed, err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = bmi160_gyro_open_report_data;
	ctl.enable_nodata = bmi160_gyro_enable_nodata;
	ctl.set_delay = bmi160_gyro_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = obj->hw->is_batch_supported;
	err = gyro_register_control_path(&ctl);
	if (err) {
		GYRO_ERROR("register gyro control path err\n");
		goto exit_create_attr_failed;
	}

	data.get_data = bmi160_gyro_get_data;
	data.vender_div = DEGREE_TO_RAD;
	err = gyro_register_data_path(&data);
	if (err) {
		GYRO_ERROR("gyro_register_data_path fail = %d\n", err);
		goto exit_create_attr_failed;
	}

	bmi160_gyro_init_flag = 0;
	GYRO_DBG("%s: OK\n", __func__);
	GYRO_DBG("idme_get_sensorcal() \n");
	ret = idme_get_sensorcal(idmedata);

	GYRO_DBG("lsm6ds3_gyro_i2c_probe idmedata %x, %x, %x, %x, %x, %x\n",
	idmedata[0], idmedata[1], idmedata[2], idmedata[3], idmedata[4], idmedata[5]);

	gyro_xyz_offset[0] = idmedata[3];
	gyro_xyz_offset[1] = idmedata[4];
	gyro_xyz_offset[2] = idmedata[5];

	obj->cali_sw[0] = gyro_xyz_offset[0];
	obj->cali_sw[1] = gyro_xyz_offset[1];
	obj->cali_sw[2] = gyro_xyz_offset[2];
	return 0;

exit_create_attr_failed:
	misc_deregister(&bmg_device);
exit_misc_device_register_failed:
exit_init_client_failed:
exit_hwmsen_get_convert_failed:
	kfree(obj);
exit:
	bmi160_gyro_init_flag = -1;
	GYRO_ERROR("err = %d\n", err);
	return err;
}

static int bmg_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	err = bmg_delete_attr(&bmi160_gyro_init_info.platform_diver_addr->driver);
	if (err)
		GYRO_ERROR("bmg_delete_attr failed, err = %d\n", err);

	err = misc_deregister(&bmg_device);
	if (err)
		GYRO_ERROR("misc_deregister failed, err = %d\n", err);

	obj_i2c_data = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id gyro_of_match[] = {
	{.compatible = "mediatek,gyro"},
	{},
};
#endif

static struct i2c_driver bmg_i2c_driver = {
	.driver = {
		   .name = BMG_DEV_NAME,
#ifdef CONFIG_OF
		   .of_match_table = gyro_of_match,
#endif
		   },
	.probe = bmg_i2c_probe,
	.remove = bmg_i2c_remove,
	.detect = bmg_i2c_detect,
	.suspend = bmg_suspend,
	.resume = bmg_resume,
	.id_table = bmg_i2c_id,
};

static int bmi160_gyro_remove(void)
{
	i2c_del_driver(&bmg_i2c_driver);
	return 0;
}

static int bmi160_gyro_local_init(struct platform_device *pdev)
{
	/* compulsory registration for testing
	   struct i2c_adapter *adapter;
	   struct i2c_client *client;
	   struct i2c_board_info info;
	   adapter = i2c_get_adapter(3);
	   if(adapter == NULL){
	   printk(KERN_ERR"gsensor_local_init error");
	   }
	   memset(&info,0,sizeof(struct i2c_board_info));
	   info.addr = 0x61;
	   //info.type = "mediatek,gsensor";
	   strlcpy(info.type,"bmi160_gyro",I2C_NAME_SIZE);
	   client = i2c_new_device(adapter,&info);
	   strlcpy(client->name,"bmi160_gyro",I2C_NAME_SIZE);
	 */
	if (i2c_add_driver(&bmg_i2c_driver)) {
		GYRO_ERROR("add gyro driver error.\n");
		return -1;
	}
	if (-1 == bmi160_gyro_init_flag) {
		return -1;
	}
	GYRO_DBG("bmi160 gyro init ok.\n");
	return 0;
}

static struct gyro_init_info bmi160_gyro_init_info = {
	.name = BMG_DEV_NAME,
	.init = bmi160_gyro_local_init,
	.uninit = bmi160_gyro_remove,
};

static int __init bmg_init(void)
{
	const char *name = "mediatek,bmi160_gyro";

	hw = get_gyro_dts_func(name, hw);
	if (!hw) {
		GYRO_ERROR("Gyro device tree configuration error.\n");
		return 0;
	}
	GYRO_DBG("%s: bosch gyroscope driver version: %s\n", __func__, BMG_DRIVER_VERSION);
	gyro_driver_add(&bmi160_gyro_init_info);
	return 0;
}

static void __exit bmg_exit(void)
{
	GYRO_FUNC();
}
module_init(bmg_init);
module_exit(bmg_exit);

MODULE_LICENSE("GPLv2");
MODULE_DESCRIPTION("BMG I2C Driver");
MODULE_AUTHOR("xiaogang.fan@cn.bosch.com");
MODULE_VERSION(BMG_DRIVER_VERSION);
