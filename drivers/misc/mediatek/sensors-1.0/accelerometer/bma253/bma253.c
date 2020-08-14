/* BMA150 motion sensor driver
 *
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

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <linux/kthread.h>
#include <cust_acc.h>
#include <accel.h>
#include <hwmsensor.h>
#include <sensors_io.h>
#include <hwmsen_helper.h>
#include <linux/kernel.h>
#include <upmu_common.h>

#include "bma253.h"

#ifdef CUSTOM_KERNEL_SENSORHUB
	#include <SCP_sensorHub.h>
#endif

#define GSENSOR_CALI	0

/*----------------------------------------------------------------------------*/
#define I2C_DRIVERID_BMA250 250
/*----------------------------------------------------------------------------*/
#define DEBUG	1
#define BMA250_ACC_AXES_NUM	3
#define ACCEL_SELF_TEST_MIN_VAL		0
#define ACCEL_SELF_TEST_MAX_VAL		13000

#define BMA250_MODE_NORMAL      0
#define BMA250_MODE_SUSPEND     1
/* default tolenrance is 20% */
static int accel_cali_tolerance = 20;

/*----------------------------------------------------------------------------*/
#define SW_CALIBRATION

#if GSENSOR_CALI
	#define LIBHWM_GRAVITY_EARTH			(9.80665f)
typedef union {
	struct {
		int rx;
		int ry;
		int rz;
	};
	struct {
		float x;
		float y;
		float z;
	};

} HwmData;

struct manuf_gsensor_cali {
	int cali_x;
	int cali_y;
	int cali_z;
};
extern int fih_read_gsensor_cali(struct manuf_gsensor_cali *p);
extern int fih_write_gsensor_cali(struct manuf_gsensor_cali *p);
struct manuf_gsensor_cali partition_calidata;
#endif

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id bma250_i2c_id[] = {{BMA250_DEV_NAME, 0}, {} };
static int accel_self_test[BMA250_ACC_AXES_NUM] = {0};
static s16 accel_xyz_offset[BMA250_ACC_AXES_NUM] = {0};
/*the adapter id will be available in customization*/

/*----------------------------------------------------------------------------*/
static int bma250_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int bma250_i2c_remove(struct i2c_client *client);
#ifdef CUSTOM_KERNEL_SENSORHUB
static int gsensor_setup_irq(void);
#endif

#ifdef CONFIG_PM_SLEEP
static int bma250_i2c_suspend(struct device *dev);
static int bma250_i2c_resume(struct device *dev);
#endif

static int BMA250_ReadSensorData(struct i2c_client *client, char *buf, int bufsize);
static int gsensor_local_init(void);
static int gsensor_remove(void);
static int gsensor_set_delay(u64 ns);

static DEFINE_MUTEX(gsensor_mutex);
static DEFINE_MUTEX(gsensor_scp_en_mutex);


static bool enable_status;

static int gsensor_init_flag = -1;

static struct acc_init_info bma250_init_info = {
	.name = "bma250",
	.init = gsensor_local_init,
	.uninit = gsensor_remove,
};

/*----------------------------------------------------------------------------*/
typedef enum {
	ADX_TRC_FILTER	= 0x01,
	ADX_TRC_RAWDATA = 0x02,
	ADX_TRC_IOCTL	= 0x04,
	ADX_TRC_CALI	= 0X08,
	ADX_TRC_INFO	= 0X10,
} ADX_TRC;
/*----------------------------------------------------------------------------*/
struct scale_factor {
	u8	whole;
	u8	fraction;
};
/*----------------------------------------------------------------------------*/
struct data_resolution {
	struct scale_factor scalefactor;
	int					sensitivity;
};
/*----------------------------------------------------------------------------*/
#define C_MAX_FIR_LENGTH (32)
/*----------------------------------------------------------------------------*/
struct data_filter {
	s16 raw[C_MAX_FIR_LENGTH][BMA250_AXES_NUM];
	int sum[BMA250_AXES_NUM];
	int num;
	int idx;
};
/*----------------------------------------------------------------------------*/
struct bma250_i2c_data {
	struct i2c_client *client;
	struct acc_hw *hw;
	struct hwmsen_convert	cvt;

#ifdef CUSTOM_KERNEL_SENSORHUB
	struct work_struct	irq_work;
#endif

	/*misc*/
	struct data_resolution *reso;
	atomic_t				trace;
	atomic_t				suspend;
	atomic_t				selftest;
	atomic_t				filter;
	s16						cali_sw[BMA250_AXES_NUM+1];

	/*data*/
	s8						offset[BMA250_AXES_NUM+1];	/*+1: for 4-byte alignment*/
	s16						data[BMA250_AXES_NUM+1];

#ifdef CUSTOM_KERNEL_SENSORHUB
	int						SCP_init_done;
#endif

#if defined(CONFIG_BMA250_LOWPASS)
	atomic_t				firlen;
	atomic_t				fir_en;
	struct data_filter		fir;
#endif
	/*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend	early_drv;
#endif
};
enum bma250_rev {
	BMA250,
	BMA250E,
};

static struct acc_hw accel_cust;
static struct acc_hw *hw = &accel_cust;
#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops bma250_i2c_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(bma250_i2c_suspend, bma250_i2c_resume)
};
#endif
#ifdef CONFIG_OF
static const struct of_device_id accel_of_match[] = {
	{ .compatible = "mediatek,bma_gsensor",},
	{},
};
#endif
static int bma250_revesion = BMA250;
/*----------------------------------------------------------------------------*/
static struct i2c_driver bma250_i2c_driver = {
	.driver = {
		.name			= BMA250_DEV_NAME,
#ifdef CONFIG_OF
		.of_match_table = accel_of_match,
#endif
#ifdef CONFIG_PM_SLEEP
		.pm = &bma250_i2c_pm_ops,
#endif
	},
	.probe				= bma250_i2c_probe,
	.remove				= bma250_i2c_remove,
	.id_table = bma250_i2c_id,
};


/*----------------------------------------------------------------------------*/
static struct i2c_client *bma250_i2c_client;
static struct bma250_i2c_data *obj_i2c_data;

static bool sensor_power = true;
static struct GSENSOR_VECTOR3D gsensor_gain;
/* static char selftestRes[8]= {0}; */

#define BMA250_ACC_CALI_IDME           "/proc/idme/sensorcal"
#define BMA250_ACC_CALI_FILE           "/data/inv_cal_data.bin"
#define BMA250_DATA_BUF_NUM            3

/*----------------------------------------------------------------------------*/
#define GSE_TAG					 "[Gsensor] "
#define GSE_FUN(f)				 pr_debug(GSE_TAG"%s\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)	 pr_err(GSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)	 pr_debug(GSE_TAG fmt, ##args)
/*----------------------------------------------------------------------------*/
static struct data_resolution bma250_data_resolution[1] = {
	/* combination by {FULL_RES,RANGE}*/
	{{ 0, 98}, 1024},
};
/*----------------------------------------------------------------------------*/
static struct data_resolution bma250_offset_resolution = {{0, 98}, 1024};
/*--------------------BMA250 power control function----------------------------------*/
static void BMA250_power(struct acc_hw *hw, unsigned int on)
{
	static unsigned int power_on;
	power_on = on;
}
/*----------------------------------------------------------------------------*/

#ifdef CUSTOM_KERNEL_SENSORHUB
int BMA250_SCP_SetPowerMode(bool enable, int sensorType)
{
	static bool gsensor_scp_en_status;
	static unsigned int gsensor_scp_en_map;
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;

	mutex_lock(&gsensor_scp_en_mutex);

	if (sensorType >= 32) {
		GSE_ERR("Out of index!\n");
		return -1;
	}

	if (true == enable)
		gsensor_scp_en_map |= (1<<sensorType);
	else
		gsensor_scp_en_map &= ~(1<<sensorType);

	if (0 == gsensor_scp_en_map)
		enable = false;
	else
		enable = true;

	if (gsensor_scp_en_status != enable) {
		gsensor_scp_en_status = enable;

		req.activate_req.sensorType = ID_ACCELEROMETER;
		req.activate_req.action = SENSOR_HUB_ACTIVATE;
		req.activate_req.enable = enable;
		len = sizeof(req.activate_req);
		err = SCP_sensorHub_req_send(&req, &len, 1);
		if (err) {
			GSE_ERR("SCP_sensorHub_req_send fail!\n");
		}
	}

	mutex_unlock(&gsensor_scp_en_mutex);

	return err;
}
EXPORT_SYMBOL(BMA250_SCP_SetPowerMode);
#endif

/*----------------------------------------------------------------------------*/
static int BMA250_SetDataResolution(struct bma250_i2c_data *obj)
{

/*set g sensor dataresolution here*/

/*BMA250 only can set to 10-bit dataresolution, so do nothing in bma250 driver here*/

/*end of set dataresolution*/



	/*we set measure range from -2g to +2g in BMA150_SetDataFormat(client, BMA150_RANGE_2G),
													   and set 10-bit dataresolution BMA150_SetDataResolution()*/

	/*so bma250_data_resolution[0] set value as {{ 3, 9}, 256} when declaration, and assign the value to obj->reso here*/

	obj->reso = &bma250_data_resolution[0];
	return 0;

/*if you changed the measure range, for example call: BMA250_SetDataFormat(client, BMA150_RANGE_4G),
you must set the right value to bma250_data_resolution*/

}
/*----------------------------------------------------------------------------*/
static int BMA250_ReadData(struct i2c_client *client, s16 data[BMA250_AXES_NUM])
{
	struct bma250_i2c_data *priv = i2c_get_clientdata(client);
	u8 addr = BMA250_REG_DATAXLOW;
	u8 buf[BMA250_DATA_LEN] = {0};
	int err = 0;

	if (NULL == client || atomic_read(&priv->suspend)) {
		err = -EINVAL;
		return err;
	}

	err = hwmsen_read_block(client, addr, buf, 0x06);
	if (err) {
		GSE_ERR("error: %d\n", err);
	} else {
		/*	data[BMA250_AXIS_X] = (s16)((buf[BMA250_AXIS_X*2] >> 6) |
					 (buf[BMA250_AXIS_X*2+1] << 2));
			data[BMA250_AXIS_Y] = (s16)((buf[BMA250_AXIS_Y*2] >> 6) |
					 (buf[BMA250_AXIS_Y*2+1] << 2));
			data[BMA250_AXIS_Z] = (s16)((buf[BMA250_AXIS_Z*2] >> 6) |
					 (buf[BMA250_AXIS_Z*2+1] << 2));*/

		data[BMA250_AXIS_X] = (s16)(((u16)buf[BMA250_AXIS_X*2+1]) << 8 | buf[BMA250_AXIS_X*2]) >> 4;
		data[BMA250_AXIS_Y] = (s16)(((u16)buf[BMA250_AXIS_Y*2+1]) << 8 | buf[BMA250_AXIS_Y*2]) >> 4;
		data[BMA250_AXIS_Z] = (s16)(((u16)buf[BMA250_AXIS_Z*2+1]) << 8 | buf[BMA250_AXIS_Z*2]) >> 4;

		if (atomic_read(&priv->trace) & ADX_TRC_RAWDATA) {
			GSE_LOG("[%08X %08X %08X] => [%5d %5d %5d] after\n", data[BMA250_AXIS_X], data[BMA250_AXIS_Y], data[BMA250_AXIS_Z],
					data[BMA250_AXIS_X], data[BMA250_AXIS_Y], data[BMA250_AXIS_Z]);
		}
#ifdef CONFIG_BMA250_LOWPASS
		if (atomic_read(&priv->filter)) {
			if (atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend)) {
				int idx, firlen = atomic_read(&priv->firlen);
				if (priv->fir.num < firlen) {
					priv->fir.raw[priv->fir.num][BMA250_AXIS_X] = data[BMA250_AXIS_X];
					priv->fir.raw[priv->fir.num][BMA250_AXIS_Y] = data[BMA250_AXIS_Y];
					priv->fir.raw[priv->fir.num][BMA250_AXIS_Z] = data[BMA250_AXIS_Z];
					priv->fir.sum[BMA250_AXIS_X] += data[BMA250_AXIS_X];
					priv->fir.sum[BMA250_AXIS_Y] += data[BMA250_AXIS_Y];
					priv->fir.sum[BMA250_AXIS_Z] += data[BMA250_AXIS_Z];
					if (atomic_read(&priv->trace) & ADX_TRC_FILTER) {
						GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d]\n", priv->fir.num,
								priv->fir.raw[priv->fir.num][BMA250_AXIS_X], priv->fir.raw[priv->fir.num][BMA250_AXIS_Y], priv->fir.raw[priv->fir.num][BMA250_AXIS_Z],
								priv->fir.sum[BMA250_AXIS_X], priv->fir.sum[BMA250_AXIS_Y], priv->fir.sum[BMA250_AXIS_Z]);
					}
					priv->fir.num++;
					priv->fir.idx++;
				} else {
					idx = priv->fir.idx % firlen;
					priv->fir.sum[BMA250_AXIS_X] -= priv->fir.raw[idx][BMA250_AXIS_X];
					priv->fir.sum[BMA250_AXIS_Y] -= priv->fir.raw[idx][BMA250_AXIS_Y];
					priv->fir.sum[BMA250_AXIS_Z] -= priv->fir.raw[idx][BMA250_AXIS_Z];
					priv->fir.raw[idx][BMA250_AXIS_X] = data[BMA250_AXIS_X];
					priv->fir.raw[idx][BMA250_AXIS_Y] = data[BMA250_AXIS_Y];
					priv->fir.raw[idx][BMA250_AXIS_Z] = data[BMA250_AXIS_Z];
					priv->fir.sum[BMA250_AXIS_X] += data[BMA250_AXIS_X];
					priv->fir.sum[BMA250_AXIS_Y] += data[BMA250_AXIS_Y];
					priv->fir.sum[BMA250_AXIS_Z] += data[BMA250_AXIS_Z];
					priv->fir.idx++;
					data[BMA250_AXIS_X] = priv->fir.sum[BMA250_AXIS_X]/firlen;
					data[BMA250_AXIS_Y] = priv->fir.sum[BMA250_AXIS_Y]/firlen;
					data[BMA250_AXIS_Z] = priv->fir.sum[BMA250_AXIS_Z]/firlen;
					if (atomic_read(&priv->trace) & ADX_TRC_FILTER) {
						GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d] : [%5d %5d %5d]\n", idx,
								priv->fir.raw[idx][BMA250_AXIS_X], priv->fir.raw[idx][BMA250_AXIS_Y], priv->fir.raw[idx][BMA250_AXIS_Z],
								priv->fir.sum[BMA250_AXIS_X], priv->fir.sum[BMA250_AXIS_Y], priv->fir.sum[BMA250_AXIS_Z],
								data[BMA250_AXIS_X], data[BMA250_AXIS_Y], data[BMA250_AXIS_Z]);
					}
				}
			}
		}
#endif
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int BMA250_ReadOffset(struct i2c_client *client, s8 ofs[BMA250_AXES_NUM])
{
	int err = 0;
#ifdef SW_CALIBRATION
	ofs[0] = ofs[1] = ofs[2] = 0x0;
#else
	err = hwmsen_read_block(client, BMA250_REG_OFSX, ofs, BMA250_AXES_NUM);
	if (err)
		GSE_ERR("error: %d\n", err);
#endif

	return err;
}
/*----------------------------------------------------------------------------*/
static int BMA250_ResetCalibration(struct i2c_client *client)
{
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
#ifndef SW_CALIBRATION
	u8 ofs[4] = {0, 0, 0, 0};
#endif
	int err = 0;

#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA data;
	BMA250_CUST_DATA *pCustData;
	unsigned int len;

	if (0 != obj->SCP_init_done) {
		pCustData = (BMA250_CUST_DATA *)&data.set_cust_req.custData;
		data.set_cust_req.sensorType = ID_ACCELEROMETER;
		data.set_cust_req.action = SENSOR_HUB_SET_CUST;
		pCustData->resetCali.action = BMA250_CUST_ACTION_RESET_CALI;
		len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->resetCali);
		SCP_sensorHub_req_send(&data, &len, 1);
	}
#endif

#ifdef SW_CALIBRATION

#else
	err = hwmsen_write_block(client, BMA250_REG_OFSX, ofs, 4);
	if (err)
		GSE_ERR("error: %d\n", err);
#endif

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	memset(obj->offset, 0x00, sizeof(obj->offset));
	return err;
}
/*----------------------------------------------------------------------------*/
static int BMA250_ReadCalibration(struct i2c_client *client, int dat[BMA250_AXES_NUM])
{
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
#ifndef SW_CALIBRATION
	int err;
#endif
	int mul;

#ifdef SW_CALIBRATION
	mul = 0;
#else
	err = BMA250_ReadOffset(client, obj->offset);
	if (err) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}
	mul = obj->reso->sensitivity/bma250_offset_resolution.sensitivity;
#endif

	dat[obj->cvt.map[BMA250_AXIS_X]] = obj->cvt.sign[BMA250_AXIS_X]*(obj->offset[BMA250_AXIS_X]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[BMA250_AXIS_X]);
	dat[obj->cvt.map[BMA250_AXIS_Y]] = obj->cvt.sign[BMA250_AXIS_Y]*(obj->offset[BMA250_AXIS_Y]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[BMA250_AXIS_Y]);
	dat[obj->cvt.map[BMA250_AXIS_Z]] = obj->cvt.sign[BMA250_AXIS_Z]*(obj->offset[BMA250_AXIS_Z]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[BMA250_AXIS_Z]);

	return 0;
}
/*----------------------------------------------------------------------------*/
static int BMA250_ReadCalibrationEx(struct i2c_client *client, int act[BMA250_AXES_NUM], int raw[BMA250_AXES_NUM])
{
	/*raw: the raw calibration data; act: the actual calibration data*/
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
#ifndef SW_CALIBRATION
	int err;
#endif
	int mul;

#ifdef SW_CALIBRATION
	mul = 0;
#else
	err = BMA250_ReadOffset(client, obj->offset);
	if (err) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}
	mul = obj->reso->sensitivity/bma250_offset_resolution.sensitivity;
#endif

	raw[BMA250_AXIS_X] = obj->offset[BMA250_AXIS_X]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[BMA250_AXIS_X];
	raw[BMA250_AXIS_Y] = obj->offset[BMA250_AXIS_Y]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[BMA250_AXIS_Y];
	raw[BMA250_AXIS_Z] = obj->offset[BMA250_AXIS_Z]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[BMA250_AXIS_Z];

	act[obj->cvt.map[BMA250_AXIS_X]] = obj->cvt.sign[BMA250_AXIS_X]*raw[BMA250_AXIS_X];
	act[obj->cvt.map[BMA250_AXIS_Y]] = obj->cvt.sign[BMA250_AXIS_Y]*raw[BMA250_AXIS_Y];
	act[obj->cvt.map[BMA250_AXIS_Z]] = obj->cvt.sign[BMA250_AXIS_Z]*raw[BMA250_AXIS_Z];

	return 0;
}
/*----------------------------------------------------------------------------*/
static int BMA250_WriteCalibration(struct i2c_client *client, int dat[BMA250_AXES_NUM])
{
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	int cali[BMA250_AXES_NUM] = {0};
	int raw[BMA250_AXES_NUM] = {0};
#ifndef SW_CALIBRATION
	int lsb = bma250_offset_resolution.sensitivity;
	int divisor = obj->reso->sensitivity/lsb;
#endif

#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA data;
	BMA250_CUST_DATA *pCustData;
	unsigned int len;
#endif

	err = BMA250_ReadCalibrationEx(client, cali, raw);
	if (err) { /*offset will be updated in obj->offset*/
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}

	GSE_LOG("OLDOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n",
			raw[BMA250_AXIS_X], raw[BMA250_AXIS_Y], raw[BMA250_AXIS_Z],
			obj->offset[BMA250_AXIS_X], obj->offset[BMA250_AXIS_Y], obj->offset[BMA250_AXIS_Z],
			obj->cali_sw[BMA250_AXIS_X], obj->cali_sw[BMA250_AXIS_Y], obj->cali_sw[BMA250_AXIS_Z]);

#ifdef CUSTOM_KERNEL_SENSORHUB
	pCustData = (BMA250_CUST_DATA *)data.set_cust_req.custData;
	data.set_cust_req.sensorType = ID_ACCELEROMETER;
	data.set_cust_req.action = SENSOR_HUB_SET_CUST;
	pCustData->setCali.action = BMA250_CUST_ACTION_SET_CALI;
	pCustData->setCali.data[BMA250_AXIS_X] = dat[BMA250_AXIS_X];
	pCustData->setCali.data[BMA250_AXIS_Y] = dat[BMA250_AXIS_Y];
	pCustData->setCali.data[BMA250_AXIS_Z] = dat[BMA250_AXIS_Z];
	len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->setCali);
	SCP_sensorHub_req_send(&data, &len, 1);
#endif

	/*calculate the real offset expected by caller*/
	cali[BMA250_AXIS_X] += dat[BMA250_AXIS_X];
	cali[BMA250_AXIS_Y] += dat[BMA250_AXIS_Y];
	cali[BMA250_AXIS_Z] += dat[BMA250_AXIS_Z];

	GSE_LOG("UPDATE: (%+3d %+3d %+3d)\n",
			dat[BMA250_AXIS_X], dat[BMA250_AXIS_Y], dat[BMA250_AXIS_Z]);

#ifdef SW_CALIBRATION
	obj->cali_sw[BMA250_AXIS_X] = obj->cvt.sign[BMA250_AXIS_X]*(cali[obj->cvt.map[BMA250_AXIS_X]]);
	obj->cali_sw[BMA250_AXIS_Y] = obj->cvt.sign[BMA250_AXIS_Y]*(cali[obj->cvt.map[BMA250_AXIS_Y]]);
	obj->cali_sw[BMA250_AXIS_Z] = obj->cvt.sign[BMA250_AXIS_Z]*(cali[obj->cvt.map[BMA250_AXIS_Z]]);
#else
	obj->offset[BMA250_AXIS_X] = (s8)(obj->cvt.sign[BMA250_AXIS_X]*(cali[obj->cvt.map[BMA250_AXIS_X]]) * obj->reso->sensitivity / GRAVITY_EARTH_1000/(divisor));
	obj->offset[BMA250_AXIS_Y] = (s8)(obj->cvt.sign[BMA250_AXIS_Y]*(cali[obj->cvt.map[BMA250_AXIS_Y]]) * obj->reso->sensitivity / GRAVITY_EARTH_1000/(divisor));
	obj->offset[BMA250_AXIS_Z] = (s8)(obj->cvt.sign[BMA250_AXIS_Z]*(cali[obj->cvt.map[BMA250_AXIS_Z]]) * obj->reso->sensitivity / GRAVITY_EARTH_1000/(divisor));

	/*convert software calibration using standard calibration*/
	obj->cali_sw[BMA250_AXIS_X] = obj->cvt.sign[BMA250_AXIS_X]*(cali[obj->cvt.map[BMA250_AXIS_X]])%(divisor);
	obj->cali_sw[BMA250_AXIS_Y] = obj->cvt.sign[BMA250_AXIS_Y]*(cali[obj->cvt.map[BMA250_AXIS_Y]])%(divisor);
	obj->cali_sw[BMA250_AXIS_Z] = obj->cvt.sign[BMA250_AXIS_Z]*(cali[obj->cvt.map[BMA250_AXIS_Z]])%(divisor);

	GSE_LOG("NEWOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n",
			obj->offset[BMA250_AXIS_X]*divisor + obj->cali_sw[BMA250_AXIS_X],
			obj->offset[BMA250_AXIS_Y]*divisor + obj->cali_sw[BMA250_AXIS_Y],
			obj->offset[BMA250_AXIS_Z]*divisor + obj->cali_sw[BMA250_AXIS_Z],
			obj->offset[BMA250_AXIS_X], obj->offset[BMA250_AXIS_Y], obj->offset[BMA250_AXIS_Z],
			obj->cali_sw[BMA250_AXIS_X], obj->cali_sw[BMA250_AXIS_Y], obj->cali_sw[BMA250_AXIS_Z]);

	err = hwmsen_write_block(obj->client, BMA250_REG_OFSX, obj->offset, BMA250_AXES_NUM);
	if (err) {
		GSE_ERR("write offset fail: %d\n", err);
		return err;
	}
#endif

	return err;
}
/*----------------------------------------------------------------------------*/
static int BMA250_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf[2];
	int res = 0;

	memset(databuf, 0, sizeof(u8)*2);
	databuf[0] = BMA250_REG_DEVID;
	i2c_master_send(client, databuf, 0x1);
	mdelay(40);

	databuf[0] = 0x0;
	res = i2c_master_recv(client, databuf, 0x01);
	if (res <= 0) {
		i2c_master_send(client, databuf, 0x1);

		mdelay(40);

		databuf[0] = 0x0;
		res = i2c_master_recv(client, databuf, 0x01);
		if (res <= 0) {
			goto exit_BMA250_CheckDeviceID;
		}
	}

	if (databuf[0] == BMA250_FIXED_DEVID) {
		GSE_LOG("BMA250_CheckDeviceID %4xh pass!\n ", databuf[0]);
		bma250_revesion = BMA250;
	} else if (databuf[0] == BMA250E_FIXED_DEVID) {
		GSE_LOG("BMA253_CheckDeviceID %4xh pass!\n ", databuf[0]);
		bma250_revesion = BMA250E;
	} else {
		GSE_ERR("BMA253_CheckDeviceID %d fail!\n ", databuf[0]);
	}

	exit_BMA250_CheckDeviceID:
	if (res <= 0) {
		return BMA250_ERR_I2C;
	}

	return BMA250_SUCCESS;
}

#if GSENSOR_CALI
static int readcali(void *unused)
{
	int err = 0;
	struct bma250_i2c_data *obj = obj_i2c_data;
	while (err != 1) {
		msleep(2000);
		err = fih_read_gsensor_cali(&partition_calidata);
	}
	obj->cali_sw[0] = partition_calidata.cali_x;
	obj->cali_sw[1] = partition_calidata.cali_y;
	obj->cali_sw[2] = partition_calidata.cali_z;
	printk("FQ Use calibration data for gsensor %d %d %d %d \n", obj->cali_sw[0], obj->cali_sw[1], obj->cali_sw[2], err);
	return 0;
}
#endif

/*----------------------------------------------------------------------------*/
static int BMA250_SetPowerMode(struct i2c_client *client, int enable)
{
	u8 databuf[2] = {0};
	int res = 0;
	u8 temp, temp0, temp1;
	u8 actual_power_mode = 0;
	int count = 0;
	static int num_record;
	if ((enable == sensor_power) && (num_record)) {
		GSE_LOG("Sensor power status is newest!\n");
		return BMA250_SUCCESS;
	}
	num_record = 0;
	num_record++;
	if (enable == true)
		actual_power_mode = BMA250_MODE_NORMAL;
	else
		actual_power_mode = BMA250_MODE_SUSPEND;

	res = hwmsen_read_block(client, 0x3E, &temp, 0x1);
	udelay(1000);
	count++;
	while ((count < 3) && (res < 0)) {
		res = hwmsen_read_block(client, 0x3E, &temp, 0x1);
		udelay(1000);
		count++;
	}
	if (res < 0)
		GSE_ERR("read  config failed!\n");
	switch (actual_power_mode) {
	case BMA250_MODE_NORMAL:
		databuf[0] = 0x00;
		databuf[1] = 0x00;
		while (count < 10) {
			res = hwmsen_write_block(client,
				BMA250_REG_POWER_CTL, &databuf[0], 1);
			udelay(1000);
			if (res < 0)
				GSE_LOG("write MODE_CTRL_REG failed!\n");
			res = hwmsen_write_block(client,
				BMA250_LOW_POWER_CTRL_REG, &databuf[1], 1);
			udelay(2000);
			if (res < 0)
				GSE_LOG("write LOW_POWER_CTRL_REG failed!\n");
			res = hwmsen_read_block(client,
				BMA250_REG_POWER_CTL, &temp0, 0x1);
			if (res < 0)
				GSE_LOG("read MODE_CTRL_REG failed!\n");
			res = hwmsen_read_block(client,
				BMA250_LOW_POWER_CTRL_REG, &temp1, 0x1);
			if (res < 0)
				GSE_LOG("read LOW_POWER_CTRL_REG failed!\n");
			if (temp0 != databuf[0]) {
				GSE_LOG("readback MODE_CTRL failed!\n");
				count++;
				continue;
			} else if (temp1 != databuf[1]) {
				GSE_LOG("readback LOW_POWER_CTRL failed!\n");
				count++;
				continue;
			} else {
				GSE_LOG("configure powermode success\n");
				break;
			}
		}
		udelay(1000);
		break;
	case BMA250_MODE_SUSPEND:
		databuf[0] = 0x80;
		databuf[1] = 0x00;
		while (count < 10) {
			res = hwmsen_write_block(client,
				BMA250_LOW_POWER_CTRL_REG, &databuf[1], 1);
			udelay(1000);
			if (res < 0)
				GSE_LOG("write LOW_POWER_CTRL_REG failed!\n");
			res = hwmsen_write_block(client,
				BMA250_REG_POWER_CTL, &databuf[0], 1);
			udelay(1000);
			res = hwmsen_write_block(client, 0x3E, &temp, 0x1);
			if (res < 0)
				GSE_LOG("write  config failed!\n");
			udelay(1000);
			res = hwmsen_write_block(client, 0x3E, &temp, 0x1);
			if (res < 0)
				GSE_LOG("write  config failed!\n");
			udelay(2000);
			res = hwmsen_read_block(client,
				BMA250_REG_POWER_CTL, &temp0, 0x1);
			if (res < 0)
				GSE_LOG("read BMA250_MODE_CTRL_REG failed!\n");
			res = hwmsen_read_block(client,
				BMA250_LOW_POWER_CTRL_REG, &temp1, 0x1);
			if (res < 0)
				GSE_LOG("read BLOW_POWER_CTRL_REG failed!\n");
			if (temp0 != databuf[0]) {
				GSE_LOG("readback MODE_CTRL failed!\n");
				count++;
				continue;
			} else if (temp1 != databuf[1]) {
				GSE_LOG("readback LOW_POWER_CTRL failed!\n");
				count++;
				continue;
			} else {
				GSE_LOG("configure powermode success\n");
				break;
			}
		}
		udelay(1000);
	break;
	}

	if (res < 0) {
		GSE_ERR("set power mode failed, res = %d\n", res);
		return BMA250_ERR_I2C;
	}
	sensor_power = enable;
	return BMA250_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int BMA250_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[10], databuf1[2] = {0};
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);

	if (hwmsen_read_block(client, BMA250_REG_DATA_FORMAT, databuf, 0x01)) {
		GSE_ERR("bma250 read Dataformat failt \n");
		return BMA250_ERR_I2C;
	}

	databuf[0] &= ~BMA250_RANGE_MASK;
	databuf[0] |= dataformat;
	databuf[1] = databuf[0];
	databuf[0] = BMA250_REG_DATA_FORMAT;

	printk("gsensor write register 0x0f data:0x%x\n", databuf[1]);

	res = i2c_master_send(client, databuf, 0x2);
	mdelay(1);
	if (res <= 0) {
		return BMA250_ERR_I2C;
	}

	if (hwmsen_read_block(client, BMA250_REG_DATA_FORMAT, databuf1, 0x01)) {
		GSE_ERR("bma250 read Dataformat failt \n");
		return BMA250_ERR_I2C;
	} else {
		printk("gsensor read register 0x0f data:0x%x\n", databuf1[0]);
		if (databuf1[0] != dataformat) {
			res = i2c_master_send(client, databuf, 0x2);
			mdelay(1);
			if (res <= 0) {
				return BMA250_ERR_I2C;
			}
			printk("gsensor set 0x0f register again!\n");
		}
	}

	return BMA250_SetDataResolution(obj);
}
/*----------------------------------------------------------------------------*/
static int BMA250_SetBWRate(struct i2c_client *client, u8 bwrate)
{
	u8 databuf[10];
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);

	if (hwmsen_read_block(client, BMA250_REG_BW_RATE, databuf, 0x01)) {
		GSE_ERR("bma250 read rate failt \n");
		return BMA250_ERR_I2C;
	}

	databuf[0] &= ~BMA250_BW_MASK;
	databuf[0] |= bwrate;
	databuf[1] = databuf[0];
	databuf[0] = BMA250_REG_BW_RATE;


	res = i2c_master_send(client, databuf, 0x2);
	mdelay(1);
	if (res <= 0) {
		return BMA250_ERR_I2C;
	}

	/*GSE_LOG("BMA250_SetBWRate OK! \n");*/

	return BMA250_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int BMA250_SetIntEnable(struct i2c_client *client, u8 intenable)
{
	int res = 0;

	res = hwmsen_write_byte(client, BMA250_INT_REG_1, 0x00);
	mdelay(1);
	if (res != BMA250_SUCCESS) {
		return res;
	}
	res = hwmsen_write_byte(client, BMA250_INT_REG_2, 0x00);
	mdelay(1);
	if (res != BMA250_SUCCESS) {
		return res;
	}
	GSE_LOG("BMA250 disable interrupt ...\n");

	/*for disable interrupt function*/

	return BMA250_SUCCESS;
}

static int acc_store_offset_in_file(const char *filename,
s16 *offset, int data_valid)
{
	struct file *cali_file;
	char w_buf[BMA250_DATA_BUF_NUM*sizeof(s16)*2+1] = {0};
	char r_buf[BMA250_DATA_BUF_NUM*sizeof(s16)*2+1] = {0};
	int i;
	char *dest = w_buf;
	mm_segment_t fs;

	cali_file = filp_open(filename, O_CREAT | O_RDWR, 0777);
	if (IS_ERR(cali_file)) {
		GSE_LOG("open error! exit!\n");
		return -1;
	}
	fs = get_fs();
	set_fs(get_ds());
	for (i = 0; i < BMA250_DATA_BUF_NUM; i++) {
		sprintf(dest, "%02X", offset[i]&0x00FF);
		dest += 2;
		sprintf(dest, "%02X", (offset[i] >> 8)&0x00FF);
		dest += 2;
		};
	GSE_LOG("w_buf: %s\n", w_buf);
	vfs_write(cali_file, (void *)w_buf,
	BMA250_DATA_BUF_NUM*sizeof(s16)*2, &cali_file->f_pos);
	cali_file->f_pos = 0x00;
	vfs_read(cali_file, (void *)r_buf,
	BMA250_DATA_BUF_NUM*sizeof(s16)*2, &cali_file->f_pos);
	for (i = 0; i < BMA250_DATA_BUF_NUM*sizeof(s16)*2; i++) {
		if (r_buf[i] != w_buf[i]) {
			filp_close(cali_file, NULL);
			GSE_LOG("read back error! exit!\n");
			return -1;
		}
	}
	set_fs(fs);

	filp_close(cali_file, NULL);
	GSE_LOG("store_offset_in_file ok exit\n");
	return 0;
}

/*----------------------------------------------------------------------------*/
static int bma250_init_client(struct i2c_client *client, int reset_cali)
{
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	GSE_LOG("bma250_init_client \n");

	res = BMA250_SetPowerMode(client, true);
	if (res != BMA250_SUCCESS) {
		return res;
	}
	GSE_LOG("BMA250_SetPowerMode to normal OK!\n");

	res = BMA250_CheckDeviceID(client);
	if (res != BMA250_SUCCESS) {
		return res;
	}
	GSE_LOG("BMA250_CheckDeviceID ok \n");

	res = BMA250_SetBWRate(client, BMA250_BW_100HZ);
	if (res != BMA250_SUCCESS) {
		return res;
	}
	GSE_LOG("BMA250_SetBWRate OK!\n");

	res = BMA250_SetDataFormat(client, BMA250_RANGE_2G);
	if (res != BMA250_SUCCESS) {
		return res;
	}
	GSE_LOG("BMA250_SetDataFormat OK!\n");

	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = obj->reso->sensitivity;


#ifdef CUSTOM_KERNEL_SENSORHUB
	res = gsensor_setup_irq();
	if (res != BMA250_SUCCESS) {
		return res;
	}
#endif
	res = BMA250_SetIntEnable(client, 0x00);
	if (res != BMA250_SUCCESS) {
		return res;
	}
	GSE_LOG("BMA250 disable interrupt function!\n");

	res = BMA250_SetPowerMode(client, false);
	if (res != BMA250_SUCCESS) {
		return res;
	}
	GSE_LOG("BMA250_SetPowerMode OK!\n");


	if (0 != reset_cali) {
		/*reset calibration only in power on*/
		res = BMA250_ResetCalibration(client);
		if (res != BMA250_SUCCESS) {
			return res;
		}
	}
	GSE_LOG("bma250_init_client OK!\n");
#ifdef CONFIG_BMA250_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif

	mdelay(20);

	return BMA250_SUCCESS;
}

#if GSENSOR_CALI
static int gsensor_calibration(void)
{
	struct i2c_client *client = bma250_i2c_client;
	struct bma250_i2c_data *obj;
	struct manuf_gsensor_cali temp;
	char strbuf[BMA250_BUFSIZE];
	int data[3] = {0, 0, 0};
	int avg[3] = {0, 0, 0};
	int cali[3] = {0, 0, 0};
	int golden_x = 0;
	int golden_y = 0;
	int golden_z = -9800;
	int cali_last[3] = {0, 0, 0};
	int err = -1, num = 0, count = 5;

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);
	obj->cali_sw[BMA250_AXIS_X] = 0;
	obj->cali_sw[BMA250_AXIS_Y] = 0;
	obj->cali_sw[BMA250_AXIS_Z] = 0;

	while (num < count) {

		mdelay(20);

		/* read gsensor data */
		err = BMA250_ReadSensorData(client, strbuf, BMA250_BUFSIZE);

		if (err) {
			GSE_ERR("read data fail: %d\n", err);
			return 0;
		}

		sscanf(strbuf, "%x %x %x", &data[BMA250_AXIS_X],
		&data[BMA250_AXIS_Y], &data[BMA250_AXIS_Z]);

		if (data[BMA250_AXIS_Z] > 8500)
			golden_z = 9800;
		else if (data[BMA250_AXIS_Z] < -8500)
			golden_z = -9800;
		else
			return 0;

		avg[BMA250_AXIS_X] = data[BMA250_AXIS_X] + avg[BMA250_AXIS_X] ;
		avg[BMA250_AXIS_Y] = data[BMA250_AXIS_Y] + avg[BMA250_AXIS_Y];
		avg[BMA250_AXIS_Z] = data[BMA250_AXIS_Z] + avg[BMA250_AXIS_Z];

		num++;

	}

	avg[BMA250_AXIS_X] /= count;
	avg[BMA250_AXIS_Y] /= count;
	avg[BMA250_AXIS_Z] /= count;

	cali[BMA250_AXIS_X] = golden_x - avg[BMA250_AXIS_X];
	cali[BMA250_AXIS_Y] = golden_y - avg[BMA250_AXIS_Y];
	cali[BMA250_AXIS_Z] = golden_z - avg[BMA250_AXIS_Z];

	GSE_LOG("FQ cali data1 = %d %d %d\n", cali[BMA250_AXIS_X], cali[BMA250_AXIS_Y], cali[BMA250_AXIS_Z]);

	cali_last[0] = cali[BMA250_AXIS_X];
	cali_last[1] = cali[BMA250_AXIS_Y];
	cali_last[2] = cali[BMA250_AXIS_Z];

	GSE_LOG("FQ cali data2 = %d %d %d\n", cali_last[0], cali_last[1], cali_last[2]);

	err = BMA250_WriteCalibration(client, cali_last);

	temp.cali_x = obj->cali_sw[BMA250_AXIS_X];
	temp.cali_y = obj->cali_sw[BMA250_AXIS_Y];
	temp.cali_z = obj->cali_sw[BMA250_AXIS_Z];

	GSE_LOG("FQ cali data4 = %d %d %d\n", temp.cali_x, temp.cali_y, temp.cali_z);

	err = fih_write_gsensor_cali(&temp);
	return err;

}
#endif

static ssize_t set_accel_cali(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma250_i2c_client;
	struct bma250_i2c_data *obj;
	char strbuf[BMA250_BUFSIZE];
	int data[3] = {0, 0, 0};
	int avg[3] = {0, 0, 0};
	int cali[3] = {0, 0, 0};
	int golden_x = 0;
	int golden_y = 0;
	int golden_z = -9800;
	int cali_last[3] = {0, 0, 0};
	int err = -1, num = 0, times = 20;

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);
	obj->cali_sw[BMA250_AXIS_X] = 0;
	obj->cali_sw[BMA250_AXIS_Y] = 0;
	obj->cali_sw[BMA250_AXIS_Z] = 0;

	while (num < times) {
		mdelay(20);
		/* read gsensor data */
		err = BMA250_ReadSensorData(client, strbuf, BMA250_BUFSIZE);

		if (err) {
			GSE_ERR("read data fail: %d\n", err);
			return 0;
		}

		err = sscanf(strbuf, "%x %x %x", &data[BMA250_AXIS_X],
				&data[BMA250_AXIS_Y], &data[BMA250_AXIS_Z]);
		if (3 != err) {
			GSE_ERR("invalid content: '%s'\n", strbuf);
			return 0;
		}

		if (data[BMA250_AXIS_Z] > 8500)
			golden_z = 9800;
		else if (data[BMA250_AXIS_Z] < -8500)
			golden_z = -9800;
		else
			return 0;

		avg[BMA250_AXIS_X] = data[BMA250_AXIS_X] + avg[BMA250_AXIS_X];
		avg[BMA250_AXIS_Y] = data[BMA250_AXIS_Y] + avg[BMA250_AXIS_Y];
		avg[BMA250_AXIS_Z] = data[BMA250_AXIS_Z] + avg[BMA250_AXIS_Z];

		num++;

	}

	avg[BMA250_AXIS_X] /= times;
	avg[BMA250_AXIS_Y] /= times;
	avg[BMA250_AXIS_Z] /= times;

	cali[BMA250_AXIS_X] = golden_x - avg[BMA250_AXIS_X];
	cali[BMA250_AXIS_Y] = golden_y - avg[BMA250_AXIS_Y];
	cali[BMA250_AXIS_Z] = golden_z - avg[BMA250_AXIS_Z];


	if ((abs(cali[BMA250_AXIS_X]) >
		abs(accel_cali_tolerance * golden_z / 100))
		|| (abs(cali[BMA250_AXIS_Y]) >
		abs(accel_cali_tolerance * golden_z / 100))
		|| (abs(cali[BMA250_AXIS_Z]) >
		abs(accel_cali_tolerance * golden_z / 100))) {

		GSE_ERR("X/Y/Z out of range  tolerance:[%d] avg_x:[%d] avg_y:[%d] avg_z:[%d]\n",
				accel_cali_tolerance,
				avg[BMA250_AXIS_X],
				avg[BMA250_AXIS_Y],
				avg[BMA250_AXIS_Z]);

		return snprintf(buf, PAGE_SIZE,
				"Please place the Pad to a horizontal level.\ntolerance:[%d] avg_x:[%d] avg_y:[%d] avg_z:[%d]\n",
				accel_cali_tolerance,
				avg[BMA250_AXIS_X],
				avg[BMA250_AXIS_Y],
				avg[BMA250_AXIS_Z]);
	}

	cali_last[0] = cali[BMA250_AXIS_X];
	cali_last[1] = cali[BMA250_AXIS_Y];
	cali_last[2] = cali[BMA250_AXIS_Z];

	BMA250_WriteCalibration(client, cali_last);

	cali[BMA250_AXIS_X] = obj->cali_sw[BMA250_AXIS_X];
	cali[BMA250_AXIS_Y] = obj->cali_sw[BMA250_AXIS_Y];
	cali[BMA250_AXIS_Z] = obj->cali_sw[BMA250_AXIS_Z];

	accel_xyz_offset[0] = (s16)cali[BMA250_AXIS_X];
	accel_xyz_offset[1] = (s16)cali[BMA250_AXIS_Y];
	accel_xyz_offset[2] = (s16)cali[BMA250_AXIS_Z];

	if (acc_store_offset_in_file(BMA250_ACC_CALI_FILE,
	accel_xyz_offset, 1)) {
		return snprintf(buf, PAGE_SIZE,
		"[G Sensor] set_accel_cali ERROR %d, %d, %d\n",
		accel_xyz_offset[0],
		accel_xyz_offset[1],
		accel_xyz_offset[2]);
	}

	return snprintf(buf, PAGE_SIZE,
	"[G Sensor] set_accel_cali PASS  %d, %d, %d\n",
	accel_xyz_offset[0],
	accel_xyz_offset[1],
	accel_xyz_offset[2]);
}

static ssize_t get_accel_cali(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"x=%d , y=%d , z=%d\nx=0x%04x , y=0x%04x , z=0x%04x\nPass\n",
			accel_xyz_offset[0], accel_xyz_offset[1],
			accel_xyz_offset[2], accel_xyz_offset[0],
			accel_xyz_offset[1], accel_xyz_offset[2]);
}

/*----------------------------------------------------------------------------*/
static int BMA250_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
{
	u8 databuf[10];

	memset(databuf, 0, sizeof(u8)*10);

	if ((NULL == buf) || (bufsize <= 30)) {
		return -1;
	}

	if (NULL == client) {
		*buf = 0;
		return -2;
	}

	sprintf(buf, "BMA253 Chip");
	return 0;
}
/*----------------------------------------------------------------------------*/
static int BMA250_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
	struct bma250_i2c_data *obj = (struct bma250_i2c_data *)i2c_get_clientdata(client);
	u8 databuf[20];
	int acc[BMA250_AXES_NUM];
	int res = 0;
	memset(databuf, 0, sizeof(u8)*10);

	if (NULL == buf) {
		return -1;
	}
	if (NULL == client || atomic_read(&obj->suspend)) {
		*buf = 0;
		return -2;
	}

	if (sensor_power == false) {
		res = BMA250_SetPowerMode(client, true);
		if (res) {
			GSE_ERR("Power on bma250 error %d!\n", res);
		}
	}


	res = BMA250_ReadData(client, obj->data);
	if (res) {
		GSE_ERR("I2C error: ret value=%d", res);
		return -3;
	} else {
#if 1
		obj->data[BMA250_AXIS_X] = obj->data[BMA250_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		obj->data[BMA250_AXIS_Y] = obj->data[BMA250_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		obj->data[BMA250_AXIS_Z] = obj->data[BMA250_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
#endif
		obj->data[BMA250_AXIS_X] += obj->cali_sw[BMA250_AXIS_X];
		obj->data[BMA250_AXIS_Y] += obj->cali_sw[BMA250_AXIS_Y];
		obj->data[BMA250_AXIS_Z] += obj->cali_sw[BMA250_AXIS_Z];

		GSE_LOG("cali_sw x=%d, y=%d, z=%d \n", obj->cali_sw[BMA250_AXIS_X], obj->cali_sw[BMA250_AXIS_Y], obj->cali_sw[BMA250_AXIS_Z]);

		/*remap coordinate*/
		acc[obj->cvt.map[BMA250_AXIS_X]] = obj->cvt.sign[BMA250_AXIS_X]*obj->data[BMA250_AXIS_X];
		acc[obj->cvt.map[BMA250_AXIS_Y]] = obj->cvt.sign[BMA250_AXIS_Y]*obj->data[BMA250_AXIS_Y];
		acc[obj->cvt.map[BMA250_AXIS_Z]] = obj->cvt.sign[BMA250_AXIS_Z]*obj->data[BMA250_AXIS_Z];

		GSE_LOG("gsensor data x=%d, y=%d, z=%d, sensitivity:%d\n",
			acc[BMA250_AXIS_X], acc[BMA250_AXIS_Y], acc[BMA250_AXIS_Z], obj->reso->sensitivity);
		sprintf(buf, "%04x %04x %04x", acc[BMA250_AXIS_X], acc[BMA250_AXIS_Y], acc[BMA250_AXIS_Z]);
		if (atomic_read(&obj->trace) & ADX_TRC_IOCTL) {
			GSE_LOG("gsensor data: %s!\n", buf);
		}
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
static int BMA250_ReadRawData(struct i2c_client *client, char *buf)
{
	struct bma250_i2c_data *obj = (struct bma250_i2c_data *)i2c_get_clientdata(client);
	int res = 0;

	if (!buf || !client) {
		return EINVAL;
	}

	res = BMA250_ReadData(client, obj->data);
	if (res) {
		GSE_ERR("I2C error: ret value=%d", res);
		return EIO;
	} else {
		sprintf(buf, "%04x %04x %04x", obj->data[BMA250_AXIS_X],
				obj->data[BMA250_AXIS_Y], obj->data[BMA250_AXIS_Z]);

	}

	return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma250_i2c_client;
	char strbuf[BMA250_BUFSIZE];
	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	BMA250_ReadChipInfo(client, strbuf, BMA250_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}


/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma250_i2c_client;
	char strbuf[BMA250_BUFSIZE] = {0};
	int data[3] = {0, 0, 0};
	int res;

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	BMA250_ReadSensorData(client, strbuf, BMA250_BUFSIZE);

	res = sscanf(strbuf, "%x %x %x", &data[BMA250_AXIS_X],
			&data[BMA250_AXIS_Y], &data[BMA250_AXIS_Z]);
	if (res == 0)
		return 0;
	else
		return snprintf(buf, PAGE_SIZE, "%d %d %d\n",
				data[BMA250_AXIS_X], data[BMA250_AXIS_Y], data[BMA250_AXIS_Z]);
}


/*----------------------------------------------------------------------------*/
static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma250_i2c_client;
	struct bma250_i2c_data *obj;
	int err, len = 0, mul;
	int tmp[BMA250_AXES_NUM] = {0};

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);

	err = BMA250_ReadOffset(client, obj->offset);
	if (err) {
		return -EINVAL;
	}
	err = BMA250_ReadCalibration(client, tmp);
	if (err) {
		return -EINVAL;
	} else {
		mul = obj->reso->sensitivity/bma250_offset_resolution.sensitivity;
		len += snprintf(buf+len, PAGE_SIZE-len, "[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n", mul,
						obj->offset[BMA250_AXIS_X], obj->offset[BMA250_AXIS_Y], obj->offset[BMA250_AXIS_Z],
						obj->offset[BMA250_AXIS_X], obj->offset[BMA250_AXIS_Y], obj->offset[BMA250_AXIS_Z]);
		len += snprintf(buf+len, PAGE_SIZE-len, "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1,
						obj->cali_sw[BMA250_AXIS_X], obj->cali_sw[BMA250_AXIS_Y], obj->cali_sw[BMA250_AXIS_Z]);

		len += snprintf(buf+len, PAGE_SIZE-len, "[ALL]	  (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n",
						obj->offset[BMA250_AXIS_X]*mul + obj->cali_sw[BMA250_AXIS_X],
						obj->offset[BMA250_AXIS_Y]*mul + obj->cali_sw[BMA250_AXIS_Y],
						obj->offset[BMA250_AXIS_Z]*mul + obj->cali_sw[BMA250_AXIS_Z],
						tmp[BMA250_AXIS_X], tmp[BMA250_AXIS_Y], tmp[BMA250_AXIS_Z]);

		return len;
	}
}
/*----------------------------------------------------------------------------*/
static ssize_t store_cali_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct i2c_client *client = bma250_i2c_client;

#if GSENSOR_CALI
	int value;
	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	} else if (1 != sscanf(buf, "%x", &value)) {
		GSE_ERR("invalid format: '%s'\n", buf);
		return 0;
	}

	if (value == 1)
		gsensor_calibration();
#else
	int err, x, y, z;
	int dat[BMA250_AXES_NUM];

	if (!strncmp(buf, "rst", 3)) {
		err = BMA250_ResetCalibration(client);
		if (err) {
			GSE_ERR("reset offset err = %d\n", err);
		}
	} else if (3 == sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z)) {
		dat[BMA250_AXIS_X] = x;
		dat[BMA250_AXIS_Y] = y;
		dat[BMA250_AXIS_Z] = z;
		err = BMA250_WriteCalibration(client, dat);
		if (err) {
			GSE_ERR("write calibration err = %d\n", err);
		}
	} else {
		GSE_ERR("invalid format\n");
	}
#endif

	return count;
}


/*----------------------------------------------------------------------------*/
static ssize_t show_firlen_value(struct device_driver *ddri, char *buf)
{
#ifdef CONFIG_BMA250_LOWPASS
	struct i2c_client *client = bma250_i2c_client;
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
	if (atomic_read(&obj->firlen)) {
		int idx, len = atomic_read(&obj->firlen);
		GSE_LOG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);

		for (idx = 0; idx < len; idx++) {
			GSE_LOG("[%5d %5d %5d]\n", obj->fir.raw[idx][BMA250_AXIS_X], obj->fir.raw[idx][BMA250_AXIS_Y], obj->fir.raw[idx][BMA250_AXIS_Z]);
		}

		GSE_LOG("sum = [%5d %5d %5d]\n", obj->fir.sum[BMA250_AXIS_X], obj->fir.sum[BMA250_AXIS_Y], obj->fir.sum[BMA250_AXIS_Z]);
		GSE_LOG("avg = [%5d %5d %5d]\n", obj->fir.sum[BMA250_AXIS_X]/len, obj->fir.sum[BMA250_AXIS_Y]/len, obj->fir.sum[BMA250_AXIS_Z]/len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->firlen));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}
/*----------------------------------------------------------------------------*/
static ssize_t store_firlen_value(struct device_driver *ddri, const char *buf, size_t count)
{
#ifdef CONFIG_BMA250_LOWPASS
	struct i2c_client *client = bma250_i2c_client;
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
	int firlen;

	if (1 != sscanf(buf, "%d", &firlen)) {
		GSE_ERR("invallid format\n");
	} else if (firlen > C_MAX_FIR_LENGTH) {
		GSE_ERR("exceeds maximum filter length\n");
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
/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct bma250_i2c_data *obj = obj_i2c_data;
	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct bma250_i2c_data *obj = obj_i2c_data;
	int trace;
	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
	} else {
		GSE_ERR("invalid content: '%s', length = %d\n", buf, (int)count);
	}

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct bma250_i2c_data *obj = obj_i2c_data;
	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (obj->hw) {
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d %d (%d %d)\n",
						obj->hw->i2c_num, obj->hw->direction, obj->hw->power_id, obj->hw->power_vol);
	} else {
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}
	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_power_status_value(struct device_driver *ddri, char *buf)
{
	if (sensor_power)
		GSE_LOG("G sensor is in work mode, sensor_power = %d\n", sensor_power);
	else
		GSE_LOG("G sensor is in standby mode, sensor_power = %d\n", sensor_power);

	return 0;
}

#if GSENSOR_CALI
static ssize_t show_gcalidata(struct device_driver *ddri, char *buf)
{

	struct bma250_i2c_data *obj = obj_i2c_data;
	return snprintf(buf, PAGE_SIZE, "%d %d %d\n", obj->cali_sw[0], obj->cali_sw[1], obj->cali_sw[2]);

}
#endif

static ssize_t set_accel_self_test(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma250_i2c_client;
	struct bma250_i2c_data *obj;
	char strbuf[BMA250_BUFSIZE];
	int data[3] = {0, 0, 0};
	int avg[3] = {0}, out_nost[3] = {0};
	int err = -1, num = 0, count = 5;

	accel_self_test[0] = accel_self_test[1] = accel_self_test[2] = 0;
	obj = i2c_get_clientdata(client);

	err = BMA250_SetPowerMode(obj_i2c_data->client, 1);
	if (err) {
		GSE_ERR("enable gsensor fail: %d\n", err);
		goto BMA250_accel_self_test_exit;
	}

	msleep(100);

	while (num < count) {

		mdelay(20);

		/* read gsensor data */
		err = BMA250_ReadSensorData(client, strbuf, BMA250_BUFSIZE);

		if (err) {
			GSE_ERR("read data fail: %d\n", err);
			goto BMA250_accel_self_test_exit;
		}

		err = sscanf(strbuf, "%x %x %x", &data[BMA250_AXIS_X],
				&data[BMA250_AXIS_Y], &data[BMA250_AXIS_Z]);
		if (3 != err) {
			GSE_ERR("invalid content: '%s'\n", strbuf);
			goto BMA250_accel_self_test_exit;
		}

		avg[BMA250_AXIS_X] = data[BMA250_AXIS_X] + avg[BMA250_AXIS_X];
		avg[BMA250_AXIS_Y] = data[BMA250_AXIS_Y] + avg[BMA250_AXIS_Y];
		avg[BMA250_AXIS_Z] = data[BMA250_AXIS_Z] + avg[BMA250_AXIS_Z];

		num++;

	}

	out_nost[0] = avg[BMA250_AXIS_X] / count;
	out_nost[1] = avg[BMA250_AXIS_Y] / count;
	out_nost[2] = avg[BMA250_AXIS_Z] / count;

	accel_self_test[0] = abs(out_nost[0]);
	accel_self_test[1] = abs(out_nost[1]);
	accel_self_test[2] = abs(out_nost[2]);

    /* disable sensor */
	err = BMA250_SetPowerMode(obj_i2c_data->client, 0);
	if (err < 0)
		goto BMA250_accel_self_test_exit;

	return snprintf(buf, PAGE_SIZE,
			"[G Sensor] set_accel_self_test PASS\n");

BMA250_accel_self_test_exit:

	BMA250_SetPowerMode(obj_i2c_data->client, 0);

	return snprintf(buf, PAGE_SIZE,
			"[G Sensor] exit - Fail , err=%d\n", err);

}

static ssize_t get_accel_self_test(struct device_driver *ddri, char *buf)
{
	if (accel_self_test[0] < ACCEL_SELF_TEST_MIN_VAL ||
		accel_self_test[0] > ACCEL_SELF_TEST_MAX_VAL)
		return sprintf(buf, "X=%d , out of range\nFail\n",
				accel_self_test[0]);

	if (accel_self_test[1] < ACCEL_SELF_TEST_MIN_VAL ||
		accel_self_test[1] > ACCEL_SELF_TEST_MAX_VAL)
		return sprintf(buf, "Y=%d , out of range\nFail\n",
				accel_self_test[1]);

	if (accel_self_test[2] < ACCEL_SELF_TEST_MIN_VAL ||
		accel_self_test[2] > ACCEL_SELF_TEST_MAX_VAL)
		return sprintf(buf, "Z=%d , out of range\nFail\n",
				accel_self_test[2]);
	else
		return sprintf(buf, "%d , %d , %d\nPass\n", accel_self_test[0],
				accel_self_test[1], accel_self_test[2]);
}

static void get_accel_idme_cali(void)
{

	s16 idmedata[6] = {0};
	idme_get_sensorcal(idmedata);

	accel_xyz_offset[0] = idmedata[0];
	accel_xyz_offset[1] = idmedata[1];
	accel_xyz_offset[2] = idmedata[2];

	GSE_LOG("accel_xyz_offset =%d, %d, %d\n", accel_xyz_offset[0],
			accel_xyz_offset[1], accel_xyz_offset[2]);
}

static ssize_t get_idme_cali(struct device_driver *ddri, char *buf)
{
	get_accel_idme_cali();
	return snprintf(buf, PAGE_SIZE,
			"offset_x=%d , offset_y=%d , offset_z=%d\nPass\n",
			accel_xyz_offset[0],
			accel_xyz_offset[1],
			accel_xyz_offset[2]);
}



static ssize_t show_power_mode(struct device_driver *ptDevDrv, char *pbBuf)
{
	ssize_t _tLength = 0;

	GSE_LOG("[%s] mc3xxx_sensor_power: %d\n", __func__, sensor_power);
	_tLength = snprintf(pbBuf, PAGE_SIZE, "%d\n", sensor_power);

	return _tLength;
}

/*****************************************
 *** store_power_mode
 *****************************************/
static ssize_t store_power_mode(struct device_driver *ptDevDrv,
				const char *pbBuf, size_t tCount)
{
	int power_mode = 0;
	bool power_enable = false;
	int ret = 0;

	struct i2c_client *client = obj_i2c_data->client;

	if (NULL == client)
		return 0;

	ret = kstrtoint(pbBuf, 10, &power_mode);

	power_enable = (power_mode ? true:false);

	if (0 == ret)
		ret = BMA250_SetPowerMode(client, power_enable);

	if (ret) {
		GSE_ERR("set power %s failed %d\n", (power_enable?"on":"off"), ret);
		return 0;
	} else
		GSE_ERR("set power %s ok\n", (sensor_power?"on":"off"));

	return tCount;
}

static ssize_t show_cali_tolerance(struct device_driver *ptDevDrv, char *pbBuf)
{
	ssize_t _tLength = 0;

	_tLength = snprintf(pbBuf, PAGE_SIZE, "accel_cali_tolerance=%d\n",
						accel_cali_tolerance);

	return _tLength;
}

/*****************************************
 *** store_cali_tolerance
 *****************************************/
static ssize_t store_cali_tolerance(struct device_driver *ptDevDrv,
					const char *pbBuf, size_t tCount)
{
	int temp_cali_tolerance = 0;
	int ret = 0;

	ret = kstrtoint(pbBuf, 10, &temp_cali_tolerance);

	if (0 == ret) {
		if (temp_cali_tolerance > 100)
			temp_cali_tolerance = 100;
		if (temp_cali_tolerance <= 0)
			temp_cali_tolerance = 1;

		accel_cali_tolerance = temp_cali_tolerance;
	}

	if (ret) {
		GSE_ERR("set accel_cali_tolerance failed %d\n", ret);
		return 0;
	} else
		GSE_ERR("set accel_cali_tolerance %d ok\n", accel_cali_tolerance);

	return tCount;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_registers(struct device_driver *ddri, char *buf)
{
	unsigned char temp = 0;
	int count = 0;
	int num = 0;

	if (buf == NULL)
		return -EINVAL;
	if (bma250_i2c_client == NULL)
		return -EINVAL;
	mutex_lock(&gsensor_mutex);
	while (count <= 0x3F) {
		temp = 0;
		if (hwmsen_read_block(bma250_i2c_client,
			count,
			&temp,
			1) < 0) {
			num =
			 snprintf(buf, PAGE_SIZE, "Read byte block error\n");
			return num;
		} else {
			num +=
			 snprintf(buf + num, PAGE_SIZE, "0x%x\n", temp);
		}
		count++;
	}
	mutex_unlock(&gsensor_mutex);
	return num;
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo, S_IWUSR | S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, S_IWUSR | S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(cali, S_IWUSR | S_IRUGO, show_cali_value, store_cali_value);
static DRIVER_ATTR(firlen, S_IWUSR | S_IRUGO,
		show_firlen_value, store_firlen_value);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO,
		show_trace_value, store_trace_value);
static DRIVER_ATTR(status, S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(powerstatus, S_IRUGO, show_power_status_value, NULL);
static DRIVER_ATTR(accelsetselftest, S_IRUGO, set_accel_self_test, NULL);
static DRIVER_ATTR(accelgetselftest, S_IRUGO, get_accel_self_test, NULL);
static DRIVER_ATTR(accelgetcali, S_IRUGO, get_accel_cali, NULL);
static DRIVER_ATTR(accelsetcali, S_IRUGO, set_accel_cali, NULL);
static DRIVER_ATTR(accelgetidme, S_IRUGO, get_idme_cali, NULL);
#if GSENSOR_CALI
static DRIVER_ATTR(gcalidata, S_IRUGO, show_gcalidata, NULL);
#endif

static DRIVER_ATTR(power_mode, S_IWUSR | S_IRUGO, show_power_mode, store_power_mode);
static DRIVER_ATTR(cali_tolerance,  S_IWUSR | S_IRUGO, show_cali_tolerance, store_cali_tolerance);
static DRIVER_ATTR(dump_registers, S_IRUGO, show_registers, NULL);


/*----------------------------------------------------------------------------*/
static struct driver_attribute *bma250_attr_list[] = {
	&driver_attr_chipinfo,	   /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/
	&driver_attr_cali,		   /*show calibration data*/
	&driver_attr_firlen,	   /*filter length: 0: disable, others: enable*/
	&driver_attr_trace,		   /*trace log*/
	&driver_attr_status,
	&driver_attr_powerstatus,
	&driver_attr_accelgetcali,
	&driver_attr_accelsetcali,
	&driver_attr_accelsetselftest,
	&driver_attr_accelgetselftest,
	&driver_attr_accelgetidme,
#if GSENSOR_CALI
	&driver_attr_gcalidata,
#endif
	&driver_attr_power_mode,
	&driver_attr_cali_tolerance,
	&driver_attr_dump_registers,

};
/*----------------------------------------------------------------------------*/
static int bma250_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(bma250_attr_list)/sizeof(bma250_attr_list[0]));
	if (driver == NULL) {
		return -EINVAL;
	}

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, bma250_attr_list[idx]);
		if (err) {
			GSE_ERR("driver_create_file (%s) = %d\n", bma250_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int bma250_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(bma250_attr_list)/sizeof(bma250_attr_list[0]));

	if (driver == NULL) {
		return -EINVAL;
	}

	for (idx = 0; idx < num; idx++) {
		driver_remove_file(driver, bma250_attr_list[idx]);
	}

	return err;
}

/*----------------------------------------------------------------------------*/
#ifdef CUSTOM_KERNEL_SENSORHUB
static void gsensor_irq_work(struct work_struct *work)
{
	struct bma250_i2c_data *obj = obj_i2c_data;
	struct scp_acc_hw scp_hw;
	BMA250_CUST_DATA *p_cust_data;
	SCP_SENSOR_HUB_DATA data;
	int max_cust_data_size_per_packet;
	int i;
	uint sizeOfCustData;
	uint len;
	char *p = (char *)&scp_hw;

	GSE_FUN();

	scp_hw.i2c_num = obj->hw->i2c_num;
	scp_hw.direction = obj->hw->direction;
	scp_hw.power_id = obj->hw->power_id;
	scp_hw.power_vol = obj->hw->power_vol;
	scp_hw.firlen = obj->hw->firlen;
	memcpy(scp_hw.i2c_addr, obj->hw->i2c_addr, sizeof(obj->hw->i2c_addr));
	scp_hw.power_vio_id = obj->hw->power_vio_id;
	scp_hw.power_vio_vol = obj->hw->power_vio_vol;
	scp_hw.is_batch_supported = obj->hw->is_batch_supported;

	p_cust_data = (BMA250_CUST_DATA *)data.set_cust_req.custData;
	sizeOfCustData = sizeof(scp_hw);
	max_cust_data_size_per_packet = sizeof(data.set_cust_req.custData) - offsetof(BMA250_SET_CUST, data);

	for (i = 0; sizeOfCustData > 0; i++) {
		data.set_cust_req.sensorType = ID_ACCELEROMETER;
		data.set_cust_req.action = SENSOR_HUB_SET_CUST;
		p_cust_data->setCust.action = BMA250_CUST_ACTION_SET_CUST;
		p_cust_data->setCust.part = i;
		if (sizeOfCustData > max_cust_data_size_per_packet) {
			len = max_cust_data_size_per_packet;
		} else {
			len = sizeOfCustData;
		}

		memcpy(p_cust_data->setCust.data, p, len);
		sizeOfCustData -= len;
		p += len;

		len += offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + offsetof(BMA250_SET_CUST, data);
		SCP_sensorHub_req_send(&data, &len, 1);
	}

	p_cust_data = (BMA250_CUST_DATA *)&data.set_cust_req.custData;

	data.set_cust_req.sensorType = ID_ACCELEROMETER;
	data.set_cust_req.action = SENSOR_HUB_SET_CUST;
	p_cust_data->resetCali.action = BMA250_CUST_ACTION_RESET_CALI;
	len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(p_cust_data->resetCali);
	SCP_sensorHub_req_send(&data, &len, 1);

	obj->SCP_init_done = 1;
}
/*----------------------------------------------------------------------------*/
static int gsensor_irq_handler(void *data, uint len)
{
	struct bma250_i2c_data *obj = obj_i2c_data;
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P)data;

	GSE_FUN();
	GSE_LOG("len = %d, type = %d, action = %d, errCode = %d\n", len, rsp->rsp.sensorType, rsp->rsp.action, rsp->rsp.errCode);

	if (!obj) {
		return -1;
	}

	switch (rsp->rsp.action) {
	case SENSOR_HUB_NOTIFY:
		switch (rsp->notify_rsp.event) {
		case SCP_INIT_DONE:
			schedule_work(&obj->irq_work);
			break;
		default:
			GSE_ERR("Error sensor hub notify");
			break;
		}
		break;
	default:
		GSE_ERR("Error sensor hub action");
		break;
	}

	return 0;
}

static int gsensor_setup_irq(void)
{
	int err = 0;

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	err = SCP_sensorHub_rsp_registration(ID_ACCELEROMETER, gsensor_irq_handler);

	return err;
}
#endif


/******************************************************************************
 * Function Configuration
******************************************************************************/
static int bma250_open(struct inode *inode, struct file *file)
{
	file->private_data = bma250_i2c_client;

	if (file->private_data == NULL) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int bma250_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
static long bma250_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct bma250_i2c_data *obj = (struct bma250_i2c_data *)i2c_get_clientdata(client);
	char strbuf[BMA250_BUFSIZE] = {0};
	void __user *data;
	struct SENSOR_DATA sensor_data;
	long err = 0;
	int cali[3] = {0};

	if (_IOC_DIR(cmd) & _IOC_READ) {
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	if (err) {
		GSE_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case GSENSOR_IOCTL_INIT:
		bma250_init_client(client, 0);
		break;

	case GSENSOR_IOCTL_READ_CHIPINFO:
		data = (void __user *) arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		BMA250_ReadChipInfo(client, strbuf, BMA250_BUFSIZE);
		if (copy_to_user(data, strbuf, strlen(strbuf)+1)) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_READ_SENSORDATA:
		data = (void __user *) arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		BMA250_ReadSensorData(client, strbuf, BMA250_BUFSIZE);
		if (copy_to_user(data, strbuf, strlen(strbuf)+1)) {
			err = -EFAULT;
			break;
		}
		break;
	case GSENSOR_IOCTL_READ_RAW_DATA:
		data = (void __user *) arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		BMA250_ReadRawData(client, strbuf);
		if (copy_to_user(data, &strbuf, strlen(strbuf)+1)) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_SET_CALI:
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
			GSE_ERR("Perform calibration in suspend state!!\n");
			err = -EINVAL;
		} else {
			cali[BMA250_AXIS_X] = sensor_data.x;
			cali[BMA250_AXIS_Y] = sensor_data.y;
			cali[BMA250_AXIS_Z] = sensor_data.z;

			err = BMA250_WriteCalibration(client, cali);
		}
		break;

	case GSENSOR_IOCTL_CLR_CALI:
		err = BMA250_ResetCalibration(client);
		break;

	case GSENSOR_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = BMA250_ReadCalibration(client, cali);
		if (err) {
			break;
		}
		sensor_data.x = cali[BMA250_AXIS_X];
		sensor_data.y = cali[BMA250_AXIS_Y];
		sensor_data.z = cali[BMA250_AXIS_Z];

		if (copy_to_user(data, &sensor_data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		break;

	default:
		GSE_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;

	}

	return err;
}


/*----------------------------------------------------------------------------*/
static struct file_operations bma250_fops = {
	.open = bma250_open,
	.release = bma250_release,
	.unlocked_ioctl = bma250_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice bma250_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &bma250_fops,
};
#ifdef CONFIG_PM_SLEEP
/*----------------------------------------------------------------------------*/
static int bma250_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	GSE_LOG("suspend function entrance");

	if (!obj) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}

	acc_driver_pause_polling(1);
	atomic_set(&obj->suspend, 1);
	mutex_lock(&gsensor_mutex);
	err = BMA250_SetPowerMode(client, false);
	mutex_unlock(&gsensor_mutex);
	if (err) {
                acc_driver_pause_polling(0);
                atomic_set(&obj->suspend, 0);
                GSE_ERR("write power control fail!!\n");
        }
	return err;
}

/*----------------------------------------------------------------------------*/
static int bma250_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);

	int err = 0;

	GSE_LOG("resume function entrance");

	if (!obj) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	if (acc_driver_query_polling_state() == 1) {
		mutex_lock(&gsensor_mutex);
		err = BMA250_SetPowerMode(client, true);
		mutex_unlock(&gsensor_mutex);
		if (err)
                        GSE_ERR("initialize client fail!!\n");
        }
	atomic_set(&obj->suspend, 0);
	acc_driver_pause_polling(0);
	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static int gsensor_open_report_data(int open)
{
	return 0;
}
/*----------------------------------------------------------------------------*/
#ifndef CUSTOM_KERNEL_SENSORHUB
static int gsensor_enable_nodata(int en)
{
	int err = 0;

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	mutex_lock(&gsensor_mutex);
	if (((en == 0) && (sensor_power == false)) || ((en == 1) && (sensor_power == true))) {
		enable_status = sensor_power;
		GSE_LOG("Gsensor device have updated!\n");
	} else {
		enable_status = !sensor_power;
		if (atomic_read(&obj_i2c_data->suspend) == 0) {
			err = BMA250_SetPowerMode(obj_i2c_data->client, enable_status);
			printk("Gsensor not in suspend gsensor_SetPowerMode!, enable_status = %d\n", enable_status);
		} else {
			printk("Gsensor in suspend and can not enable or disable!enable_status = %d\n", enable_status);
		}
	}
	mutex_unlock(&gsensor_mutex);

	if (err != BMA250_SUCCESS) {
		GSE_ERR("gsensor_enable_nodata fail!\n");
		return -1;
	}

	GSE_LOG("gsensor_enable_nodata OK!!!\n");
	return 0;
}
#else
static int scp_gsensor_enable_nodata(int en)
{
	int err = 0;

	mutex_lock(&gsensor_mutex);
	if (((en == 0) && (scp_sensor_power == false)) || ((en == 1) && (scp_sensor_power == true))) {
		enable_status = scp_sensor_power;
		GSE_LOG("Gsensor device have updated!\n");
	} else {
		enable_status = !scp_sensor_power;
		if (atomic_read(&obj_i2c_data->suspend) == 0) {
			err = BMA250_SCP_SetPowerMode(enable_status, ID_ACCELEROMETER);
			if (0 == err) {
				scp_sensor_power = enable_status;
			}
			GSE_LOG("Gsensor not in suspend gsensor_SetPowerMode!, enable_status = %d\n", enable_status);
		} else {
			GSE_LOG("Gsensor in suspend and can not enable or disable!enable_status = %d\n", enable_status);
		}
	}
	mutex_unlock(&gsensor_mutex);

	if (err != BMA250_SUCCESS) {
		printk("scp_sensor_enable_nodata fail!\n");
		return -1;
	}

	printk("scp_gsensor_enable_nodata OK!!!\n");
	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static int gsensor_set_delay(u64 ns)
{
	int err = 0;
	int value;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#else
	int sample_delay;
#endif

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	value = (int)ns/1000/1000;

#ifdef CUSTOM_KERNEL_SENSORHUB
	req.set_delay_req.sensorType = ID_ACCELEROMETER;
	req.set_delay_req.action = SENSOR_HUB_SET_DELAY;
	req.set_delay_req.delay = value;
	len = sizeof(req.activate_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err) {
		GSE_ERR("SCP_sensorHub_req_send!\n");
		return err;
	}
#else
	if (value <= 5) {
		sample_delay = BMA250_BW_200HZ;
	} else if (value <= 10) {
		sample_delay = BMA250_BW_100HZ;
	} else {
		sample_delay = BMA250_BW_50HZ;
	}

	mutex_lock(&gsensor_mutex);
	err = BMA250_SetBWRate(obj_i2c_data->client, sample_delay);
	mutex_unlock(&gsensor_mutex);
	if (err != BMA250_SUCCESS) {
		GSE_ERR("Set delay parameter error!\n");
		return -1;
	}

	if (value >= 50) {
		atomic_set(&obj_i2c_data->filter, 0);
	} else {
#if defined(CONFIG_BMA250_LOWPASS)
		obj_i2c_data->fir.num = 0;
		obj_i2c_data->fir.idx = 0;
		obj_i2c_data->fir.sum[BMA250_AXIS_X] = 0;
		obj_i2c_data->fir.sum[BMA250_AXIS_Y] = 0;
		obj_i2c_data->fir.sum[BMA250_AXIS_Z] = 0;
		atomic_set(&obj_i2c_data->filter, 1);
#endif
	}
#endif

	GSE_LOG("gsensor_set_delay (%d)\n", value);

	return 0;
}
/*----------------------------------------------------------------------------*/
static int gsensor_get_data(int *x, int *y, int *z, int *status)
{
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;
#else
	char buff[BMA250_BUFSIZE] = {0};
#endif

#ifdef CUSTOM_KERNEL_SENSORHUB
	req.get_data_req.sensorType = ID_ACCELEROMETER;
	req.get_data_req.action = SENSOR_HUB_GET_DATA;
	len = sizeof(req.get_data_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err) {
		GSE_ERR("SCP_sensorHub_req_send!\n");
		return err;
	}

	if (ID_ACCELEROMETER != req.get_data_rsp.sensorType ||
		SENSOR_HUB_GET_DATA != req.get_data_rsp.action ||
		0 != req.get_data_rsp.errCode) {
		GSE_ERR("error : %d\n", req.get_data_rsp.errCode);
		return req.get_data_rsp.errCode;
	}

	*x = (int)req.get_data_rsp.int16_Data[0]*GRAVITY_EARTH_1000/1000;
	*y = (int)req.get_data_rsp.int16_Data[1]*GRAVITY_EARTH_1000/1000;
	*z = (int)req.get_data_rsp.int16_Data[2]*GRAVITY_EARTH_1000/1000;
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	if (atomic_read(&obj_i2c_data->trace) & ADX_TRC_IOCTL) {
		GSE_LOG("x = %d, y = %d, z = %d\n", *x, *y, *z);
	}

#else
	mutex_lock(&gsensor_mutex);
	BMA250_ReadSensorData(obj_i2c_data->client, buff, BMA250_BUFSIZE);
	mutex_unlock(&gsensor_mutex);
	sscanf(buff, "%x %x %x", x, y, z);
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;
#endif

	return 0;
}



static int gsensor_acc_batch(int flag, int64_t samplingPeriodNs,
					int64_t maxBatchReportLatencyNs)
{
	int value = 0;

	value = (int)samplingPeriodNs / 1000 / 1000;
	GSE_ERR("gsensor acc set delay = (%d) ok.\n", value);
	return gsensor_set_delay(samplingPeriodNs);
}
static int gsensor_acc_flush(void)
{
	return acc_flush_report();
}
/*----------------------------------------------------------------------------*/
static int bma250_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct bma250_i2c_data *obj;
	struct acc_control_path ctl = {0};
	struct acc_data_path data = {0};
	s16 idmedata[6] = {0};
	int err = 0;
	GSE_FUN();
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}
        err = get_accel_dts_func(client->dev.of_node, hw);
        if (err < 0)
        {
                GSE_ERR("get dts info fail\n");
                err = -EFAULT;
                goto exit_kfree;
        }


	memset(obj, 0, sizeof(struct bma250_i2c_data));

	obj->hw = hw;
	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if (err) {
		GSE_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit_kfree;
	}

#ifdef CUSTOM_KERNEL_SENSORHUB
	INIT_WORK(&obj->irq_work, gsensor_irq_work);
#endif
	obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);

#ifdef CUSTOM_KERNEL_SENSORHUB
	obj->SCP_init_done = 0;
#endif

#ifdef CONFIG_BMA250_LOWPASS
	if (obj->hw->firlen > C_MAX_FIR_LENGTH) {
		atomic_set(&obj->firlen, C_MAX_FIR_LENGTH);
	} else {
		atomic_set(&obj->firlen, obj->hw->firlen);
	}

	if (atomic_read(&obj->firlen) > 0) {
		atomic_set(&obj->fir_en, 1);
	}

#endif
	bma250_i2c_client = new_client;

	err = bma250_init_client(new_client, 1);
	if (err) {
		goto exit_init_failed;
	}

	err = misc_register(&bma250_device);
	if (err) {
		GSE_ERR("bma250_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	err = bma250_create_attr(&bma250_init_info.platform_diver_addr->driver);
	if (err) {
		GSE_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = gsensor_open_report_data;

	ctl.enable_nodata = gsensor_enable_nodata;

	ctl.set_delay  = gsensor_set_delay;
	ctl.is_report_input_direct = false;

	ctl.is_support_batch = false;

	ctl.batch = gsensor_acc_batch;
	ctl.flush = gsensor_acc_flush;

	err = acc_register_control_path(&ctl);
	if (err) {
		GSE_ERR("register acc control path err\n");
		goto exit_create_attr_failed;
	}

	GSE_LOG("acc_register_control_path sucess\n");

	data.get_data = gsensor_get_data;
	data.vender_div = 1000;
	err = acc_register_data_path(&data);
	if (err) {
		GSE_ERR("register acc data path err\n");
		goto exit_create_attr_failed;
	}

	GSE_LOG("acc_register_data_path sucess\n");

	gsensor_init_flag = 0;

	idme_get_sensorcal(idmedata);

/*	printk("lsm6ds3_i2c_probe idmedata %x, %x, %x, %x, %x, %x\n",
			idmedata[0], idmedata[1], idmedata[2],
			idmedata[3], idmedata[4], idmedata[5]);*/

	accel_xyz_offset[0] = idmedata[0];
	accel_xyz_offset[1] = idmedata[1];
	accel_xyz_offset[2] = idmedata[2];
	obj->cali_sw[BMA250_AXIS_X] = accel_xyz_offset[0];
	obj->cali_sw[BMA250_AXIS_Y] = accel_xyz_offset[1];
	obj->cali_sw[BMA250_AXIS_Z] = accel_xyz_offset[2];

	GSE_LOG("%s: OK\n", __func__);
	GSE_LOG("bma250_i2c_probe sucess\n");

#if GSENSOR_CALI
	kthread_run(readcali, NULL, "readcali");
#endif
	return 0;

	exit_create_attr_failed:
	misc_deregister(&bma250_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	exit_kfree:
	kfree(obj);
	exit:
	GSE_ERR("%s: err = %d\n", __func__, err);
	gsensor_init_flag = -1;
	return err;
}

/*----------------------------------------------------------------------------*/
static int bma250_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	err = bma250_delete_attr(&bma250_init_info.platform_diver_addr->driver);
	if (err) {
		GSE_ERR("bma150_delete_attr fail: %d\n", err);
	}

	misc_deregister(&bma250_device);
	bma250_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}
/*----------------------------------------------------------------------------*/
static int gsensor_local_init(void)
{

	BMA250_power(hw, 1);
	if (i2c_add_driver(&bma250_i2c_driver)) {
		GSE_ERR("add driver error\n");
		return -1;
	}
	if (-1 == gsensor_init_flag) {
		return -1;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int gsensor_remove(void)
{
	GSE_FUN();
	BMA250_power(hw, 0);
	i2c_del_driver(&bma250_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init bma250_init(void)
{

	GSE_FUN();
	acc_driver_add(&bma250_init_info);

	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit bma250_exit(void)
{
	GSE_FUN();
}
/*----------------------------------------------------------------------------*/
module_init(bma250_init);
module_exit(bma250_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BMA250 I2C driver");
MODULE_AUTHOR("Xiaoli.li@mediatek.com");
