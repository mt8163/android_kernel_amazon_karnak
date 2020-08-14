/*
 *
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2017, FocalTech Systems, Ltd., all rights reserved.
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
/*****************************************************************************
*
* File Name: focaltech_core.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract: entrance for focaltech ts driver
*
* Version: V1.0
*
*****************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "focaltech_core.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_DRIVER_NAME                     "focaltech_ts"
#define INTERVAL_READ_REG                   100	/* interval time per read reg unit:ms */
#define TIMEOUT_READ_REG                    1000	/* timeout of read reg unit:ms */
#define FTS_I2C_SLAVE_ADDR                  0x38

#define FTS_CALIBRATION_DATA_SIZE	4
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static int tpd_flag;
#if FTS_POWER_SOURCE_CUST_EN
#else
static void fts_power_on(int flag);
#endif

#if (defined(CONFIG_TPD_HAVE_CALIBRATION) && !defined(CONFIG_TPD_CUSTOM_CALIBRATION))
static int tpd_def_calmat_local_normal[8]  = TPD_CALIBRATION_MATRIX_ROTATION_NORMAL;
static int tpd_def_calmat_local_factory[8] = TPD_CALIBRATION_MATRIX_ROTATION_FACTORY;
#endif

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct fts_ts_data *fts_data;
int tpd_rst_gpio_number = 0;
int tpd_int_gpio_number = 0;

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static int tpd_remove(struct i2c_client *client);
#if defined(CONFIG_FB)
#if defined(CONFIG_TOUCHSCREEN_MTK_VDD_SUSPEND)
static void fts_fb_registration(struct fts_ts_data *ts);
static void fts_fb_unregistration(struct fts_ts_data *ts);
#endif
#endif
/*****************************************************************************
* Focaltech ts i2c driver configuration
*****************************************************************************/
static const struct i2c_device_id fts_tpd_id[] = {{FTS_DRIVER_NAME, 0}, {} };
static const struct of_device_id fts_dt_match[] = {
	{.compatible = "mediatek,ft_cap_touch"},
	{},
};
MODULE_DEVICE_TABLE(of, fts_dt_match);

static struct i2c_driver tpd_i2c_driver = {
	.driver = {
		.name = FTS_DRIVER_NAME,
		.of_match_table = fts_dt_match,
	},
	.probe = tpd_probe,
	.remove = tpd_remove,
	.id_table = fts_tpd_id,
	.detect = tpd_i2c_detect,
};

/*****************************************************************************
*  Name: fts_wait_tp_to_valid
*  Brief: Read chip id until TP FW become valid(Timeout: TIMEOUT_READ_REG),
*         need call when reset/power on/resume...
*  Input:
*  Output:
*  Return: return 0 if tp valid, otherwise return error code
*****************************************************************************/
int fts_wait_tp_to_valid(struct i2c_client *client)
{
	int ret = 0;
	int cnt = 0;
	u8 reg_value = 0;
	u8 chip_id = fts_data->ic_info.ids.chip_idh;

	do {
	ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID, &reg_value);
	if ((ret < 0) || (reg_value != chip_id)) {
		FTS_DEBUG("TP Not Ready, ReadData = 0x%x", reg_value);
	} else if (reg_value == chip_id) {
		FTS_INFO("TP Ready, Device ID = 0x%x", reg_value);
		return 0;
	}
	cnt++;
	msleep(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

	return -EIO;
}

/************************************************************************
* Name: fts_get_chip_types
* Brief: verity chip id and get chip type data
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
static int fts_get_chip_types(
	struct fts_ts_data *ts_data,
	u8 id_h, u8 id_l, bool fw_valid)
	{
	int i = 0;
	struct ft_chip_t ctype[] = FTS_CHIP_TYPE_MAPPING;
	u32 ctype_entries = sizeof(ctype) / sizeof(struct ft_chip_t);

	if ((0x0 == id_h) || (0x0 == id_l)) {
	FTS_ERROR("id_h/id_l is 0");
	return -EINVAL;
	}

	FTS_DEBUG("verify id:0x%02x%02x", id_h, id_l);
	for (i = 0; i < ctype_entries; i++) {
		if (VALID == fw_valid) {
			if ((id_h == ctype[i].chip_idh) && (id_l == ctype[i].chip_idl))
				break;
		} else {
			if (((id_h == ctype[i].rom_idh) && (id_l == ctype[i].rom_idl))
				|| ((id_h == ctype[i].pb_idh) && (id_l == ctype[i].pb_idl))
				|| ((id_h == ctype[i].bl_idh) && (id_l == ctype[i].bl_idl)))
				break;
		}
	}

	if (i >= ctype_entries) {
	return -ENODATA;
	}

	ts_data->ic_info.ids = ctype[i];
	return 0;
}

/*****************************************************************************
*  Name: fts_get_ic_information
*  Brief:
*  Input:
*  Output:
*  Return: return 0 if success, otherwise return error code
*****************************************************************************/
static int fts_get_ic_information(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int cnt = 0;
	u8 chip_id[2] = { 0 };
	u8 id_cmd[4] = { 0 };
	struct i2c_client *client = ts_data->client;

	do {
		ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID, &chip_id[0]);
		ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID2, &chip_id[1]);
		if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
			FTS_DEBUG("i2c read error, read:0x%02x%02x", chip_id[0], chip_id[1]);
		} else {
			ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], VALID);
			if (!ret)
				break;
			else
				FTS_DEBUG("TP not ready, read:0x%02x%02x", chip_id[0], chip_id[1]);
		}

		cnt++;
		msleep(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

	if ((cnt * INTERVAL_READ_REG) >= TIMEOUT_READ_REG) {
		FTS_INFO("fw is invalid, need read boot id");
		#if FTS_HID_SUPPORTTED
		fts_i2c_hid2std(client);
		#endif
		id_cmd[0] = FTS_CMD_START1;
		id_cmd[1] = FTS_CMD_START2;
		ret = fts_i2c_write(client, id_cmd, 2);
		if (ret < 0) {
			FTS_ERROR("start cmd write fail");
			return ret;
		}
		id_cmd[0] = FTS_CMD_READ_ID;
		id_cmd[1] = id_cmd[2] = id_cmd[3] = 0x00;
		ret = fts_i2c_read(client, id_cmd, 4, chip_id, 2);
		if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
			FTS_ERROR("read boot id fail");
			return -EIO;
		}
		ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], INVALID);
		if (ret < 0) {
			FTS_ERROR("can't get ic informaton");
			return ret;
		}
	}

	ts_data->ic_info.is_incell = FTS_CHIP_IDC;
	ts_data->ic_info.hid_supported = FTS_HID_SUPPORTTED;
	FTS_INFO("get ic information, chip id = 0x%02x%02x",
		ts_data->ic_info.ids.chip_idh, ts_data->ic_info.ids.chip_idl);

	return 0;
}

/*****************************************************************************
*  Name: fts_tp_state_recovery
*  Brief: Need execute this function when reset
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_tp_state_recovery(struct i2c_client *client)
{
	/* wait tp stable */
	fts_wait_tp_to_valid(client);
	/* recover TP charger state 0x8B */
	/* recover TP glove state 0xC0 */
	/* recover TP cover state 0xC1 */
	fts_ex_mode_recovery(client);
#if FTS_PSENSOR_EN
	fts_proximity_recovery(client);
#endif

	/* recover TP gesture state 0xD0 */
#if FTS_GESTURE_EN
	fts_gesture_recovery(client);
#endif
}

/*****************************************************************************
*  Name: fts_reset_proc
*  Brief: Execute reset operation
*  Input: hdelayms - delay time unit:ms
*  Output:
*  Return:
*****************************************************************************/
int fts_reset_proc(int hdelayms)
{
	FTS_FUNC_ENTER();

	gpio_direction_output(tpd_rst_gpio_number, 0);
	msleep(20);
	gpio_direction_output(tpd_rst_gpio_number, 1);
	if (hdelayms) {
		msleep(hdelayms);
	}

	FTS_FUNC_EXIT();
	return 0;
}

/*****************************************************************************
*  Name: fts_irq_disable
*  Brief: disable irq
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_irq_disable(void)
{
	unsigned long irqflags;

	FTS_FUNC_ENTER();
	spin_lock_irqsave(&fts_data->irq_lock, irqflags);

	if (!fts_data->irq_disabled) {
		disable_irq_nosync(fts_data->irq);
		fts_data->irq_disabled = true;
	}

	spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
	FTS_FUNC_EXIT();
}

/*****************************************************************************
*  Name: fts_irq_enable
*  Brief: enable irq
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_irq_enable(void)
{
	unsigned long irqflags = 0;

	FTS_FUNC_ENTER();
	spin_lock_irqsave(&fts_data->irq_lock, irqflags);

	if (fts_data->irq_disabled) {
		enable_irq(fts_data->irq);
		fts_data->irq_disabled = false;
	}

	spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
	FTS_FUNC_EXIT();
}

/* use focaltech official power control */
#if FTS_POWER_SOURCE_CUST_EN
/*****************************************************************************
* Power Control
*****************************************************************************/
int fts_power_init(void)
{

	struct regulator *vcc_io;/*1V8*/
	struct regulator *vdd;/*3V3*/

	vcc_io = devm_regulator_get(&pdev->dev, "vccio");
	if (IS_ERR(vcc_io))
		return PTR_ERR(vcc_io);

	vdd = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(vdd)) {
		devm_regulator_put(vcc_io);
		return PTR_ERR(vdd);
	}

	fts_data->io_reg = vcc_io;
	fts_data->reg = vdd;

	return 0;
}

void fts_power_suspend(void)
{

	int buf;
	struct regulator* vcc_io;
	struct regulator* vdd;

	vcc_io = fts_data->io_reg;
	vdd = fts_data->reg;

	if (regulator_is_enabled(vcc_io)) {
		buf = regulator_disable(vcc_io);
		mdelay(5);
		if (buf != 0)
			printk(KERN_ERR "%s<FTS_ERR>failed to turn off 1V8\n", __func__);
	}

	if (regulator_is_enabled(vdd)){
		buf = regulator_enable(vdd);
		mdelay(5);
		if (buf != 0)
			printk(KERN_ERR "%s<FTS_ERR>failed to turn off 3V3\n", __func__);
	}
}

int fts_power_resume(void)
{
	int buf;
	struct regulator* vcc_io;
	struct regulator* vdd;

	vcc_io = fts_data->io_reg;
	vdd = fts_data->reg;

	/* 1V8 regulator */
	if (!regulator_is_enabled(vcc_io)) {
		buf = regulator_enable(vcc_io);
		mdelay(5);
		if (buf != 0)
			printk(KERN_ERR "%s<FTS_ERR>failed to turn on 1V8\n", __func__);
	}

	/* 3V3 regulator */
	if (!regulator_is_enabled(vdd)) {
		buf = regulator_set_voltage(vdd, 3300000, 3300000);
		udelay(2);

		if (buf != 0) {
			printk(KERN_ERR "%s<FTS_ERR>failed to set voltage 3V3\n", __func__);
		} else {
			buf = regulator_enable(vdd);
			mdelay(5);
			if (buf != 0)
				printk(KERN_ERR "%s<FTS_ERR>failed to turn on 3V3\n", __func__);
		}
	}

}
#else
/* use custom power control */
static void fts_power_on(int flag)
{
	int buf;
	struct regulator* vcc_io;
	struct regulator* vdd;

	vcc_io = fts_data->io_reg;
	vdd = fts_data->reg;

	if (flag) {
		if (!regulator_is_enabled(vcc_io)) {
			buf = regulator_enable(vcc_io);
			mdelay(5);
			if (buf != 0)
				printk(KERN_ERR "%s<FTS_ERR>failed to turn on 1V8\n", __func__);
		}

		if (!regulator_is_enabled(vdd)) {
			buf = regulator_set_voltage(vdd, 3300000, 3300000);
			udelay(2);

			if (buf != 0) {
				printk(KERN_ERR "%s<FTS_ERR>failed to set voltage 3V3\n", __func__);
			} else {
				buf = regulator_enable(vdd);
				mdelay(5);
				if (buf != 0)
					printk(KERN_ERR "%s<FTS_ERR>failed to turn on 3V3\n", __func__);
			}
		}

	} else {
		if (regulator_is_enabled(vcc_io)) {
			buf = regulator_disable(vcc_io);
			mdelay(5);
			if (buf != 0)
				printk(KERN_ERR "%s<FTS_ERR>failed to turn off 1V8\n", __func__);
		}

		if (regulator_is_enabled(vdd)){
			buf = regulator_enable(vdd);
			mdelay(5);
			if (buf != 0)
				printk(KERN_ERR "%s<FTS_ERR>failed to turn off 3V3\n", __func__);
		}
	}
}
#endif

#if defined(CONFIG_FB)
#if defined(CONFIG_TOUCHSCREEN_MTK_VDD_SUSPEND)
void fts_ts_suspend(void)
{
	int buf;
	struct regulator* vdd;

	vdd = fts_data->reg;

	if (regulator_is_enabled(vdd)){
		buf = regulator_disable(vdd);
		mdelay(5);
		if (buf != 0)
			printk(KERN_ERR "%s<FTS_ERR>failed to turn off 3V3\n", __func__);
	}
}

void fts_ts_resume(void)
{
	int buf;
	struct regulator* vcc_io;
	struct regulator* vdd;

	vcc_io = fts_data->io_reg;
	vdd = fts_data->reg;

	/* 1V8 regulator */
	if (!regulator_is_enabled(vcc_io)) {
		buf = regulator_enable(vcc_io);
		mdelay(5);
		if (buf != 0)
			printk(KERN_ERR "%s<FTS_ERR>failed to turn on 1V8\n", __func__);
	}

	/* 3V3 regulator */
	if (!regulator_is_enabled(vdd)) {
		buf = regulator_set_voltage(vdd, 3300000, 3300000);
		udelay(2);

		if (buf != 0) {
			printk(KERN_ERR "%s<FTS_ERR>failed to set voltage 3V3\n", __func__);
		} else {
			buf = regulator_enable(vdd);
			mdelay(5);
			if (buf != 0)
				printk(KERN_ERR "%s<FTS_ERR>failed to turn on 3V3\n", __func__);
		}
	}

	/* According to spec power sequence, reset pin need to pull a puls*/
	fts_reset_proc(200);
}
#endif
#endif

/*****************************************************************************
*  Reprot related
*****************************************************************************/
#if (FTS_DEBUG_EN && (FTS_DEBUG_LEVEL == 2))
char g_sz_debug[1024] = {0};
static void fts_show_touch_buffer(u8 *buf, int point_num)
{
	int len = point_num * FTS_ONE_TCH_LEN;
	int count = 0;
	int i;

	memset(g_sz_debug, 0, 1024);
	if (len > (fts_data->pnt_buf_size - 3)) {
		len = fts_data->pnt_buf_size - 3;
	} else if (len == 0) {
		len += FTS_ONE_TCH_LEN;
	}
	count += sprintf(g_sz_debug, "%02X,%02X,%02X", buf[0], buf[1], buf[2]);
	for (i = 0; i < len; i++) {
		count += sprintf(g_sz_debug + count, ",%02X", buf[i + 3]);
	}
	FTS_DEBUG("buffer: %s", g_sz_debug);
}
#endif

/************************************************************************
* Name: fts_input_report_key
* Brief: report key event
* Input: events info
* Output:
* Return: return 0 if success
***********************************************************************/
static int fts_input_report_key(struct fts_ts_data *data, int index)
{
	int i = 0;
	struct ts_event *events = data->events;
	u32 key_num = fts_data->fts_key_number;

	if (!KEY_EN) {
		return -EINVAL;
	}
	for (i = 0; i < key_num; i++) {
		if (EVENT_DOWN(events[index].flag)) {
			data->key_down = true;
			FTS_DEBUG("Key(%d, %d) DOWN", events[index].x, events[index].y);
		} else {
			data[index].key_down = false;
			FTS_DEBUG("Key(%d, %d) UP", events[index].x, events[index].y);
		}
	}

	return 0;
}

#if FTS_MT_PROTOCOL_B_EN
static int fts_input_report_b(struct fts_ts_data *data)
{
	int i = 0;
	int uppoint = 0;
	int touchs = 0;
	bool va_reported = false;
	u32 max_touch_num = FTS_MAX_POINTS_SUPPORT;
	u32 key_y_coor = data->y_axis_max;
	struct ts_event *events = data->events;

	for (i = 0; i < data->touch_point; i++) {
		if (KEY_EN && TOUCH_IS_KEY(events[i].y, key_y_coor)) {
			fts_input_report_key(data, i);
			continue;
		}

		if (events[i].id >= max_touch_num) {
			break;
		}

		va_reported = true;
		input_mt_slot(data->input_dev, events[i].id);
		if (EVENT_DOWN(events[i].flag)) {
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 1);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);
#if FTS_REPORT_PRESSURE_EN
			if (events[i].p <= 0) {
				events[i].p = 0x3f;
			}
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
			if (events[i].area <= 0) {
				events[i].area = 0x09;
			}
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);

			touchs |= BIT(events[i].id);
			data->touchs |= BIT(events[i].id);

			FTS_DEBUG("[FTS_DBG]MT protocal B tracking_id-%d finger DOWN! (%d,%d),pressure:%d, area: %d",
				events[i].id, events[i].x, events[i].y, events[i].p, events[i].area);
		} else {
			uppoint++;
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0);
			data->touchs &= ~BIT(events[i].id);
			FTS_DEBUG("[FTS_DBG]MT protocol B tracking_id-%d finger UP!", events[i].id);
		}
	}

	if (unlikely(data->touchs ^ touchs)) {
		for (i = 0; i < data->touch_point; i++)  {
			if (BIT(i) & (data->touchs ^ touchs)) {
				FTS_DEBUG("[FTS_DBG]MT protocol B, finger-%d UP!", i);
				va_reported = true;
				input_mt_slot(data->input_dev, i);
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
			}
		}
	}
	data->touchs = touchs;

	if (va_reported) {
		if (EVENT_NO_DOWN(data) || (!touchs)) {
			FTS_DEBUG("[FTS_DBG]MT protocol B, All fingers Up!");
			input_report_key(data->input_dev, BTN_TOUCH, 0);
		} else {
			input_report_key(data->input_dev, BTN_TOUCH, 1);
		}
		input_sync(data->input_dev);
	}

	return 0;
}

#else
static int fts_input_report_a(struct fts_ts_data *data)
{
	int i = 0;
	int touchs = 0;
	bool va_reported = false;
	u32 key_y_coor = data->y_axis_max;
	struct ts_event *events = data->events;

	for (i = 0; i < data->touch_point; i++) {
		if (KEY_EN && TOUCH_IS_KEY(events[i].y, key_y_coor)) {
			fts_input_report_key(data, i);
			continue;
		}

		va_reported = true;
		if (EVENT_DOWN(events[i].flag)) {
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, events[i].id);
#if FTS_REPORT_PRESSURE_EN
			if (events[i].p <= 0) {
				events[i].p = 0x3f;
			}
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
			if (events[i].area <= 0) {
				events[i].area = 0x09;
			}
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);

			input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);

			input_mt_sync(data->input_dev);

			FTS_DEBUG("[A]P%d(%d, %d)[p:%d,tm:%d] DOWN!", events[i].id, events[i].x,
				events[i].y, events[i].p, events[i].area);
			touchs++;
		}
	}

/* last point down, current no point but key */
	if (data->touchs && !touchs) {
		va_reported = true;
	}
	data->touchs = touchs;

	if (va_reported) {
		if (EVENT_NO_DOWN(data)) {
			FTS_DEBUG("[A]Points All Up!");
			input_report_key(data->input_dev, BTN_TOUCH, 0);
			input_mt_sync(data->input_dev);
		} else {
			input_report_key(data->input_dev, BTN_TOUCH, 1);
		}

		input_sync(data->input_dev);
	}
	return 0;
}
#endif

/*****************************************************************************
*  Name: fts_read_touchdata
*  Brief:
*  Input:
*  Output:
*  Return: return 0 if succuss
*****************************************************************************/
static int fts_read_touchdata(struct fts_ts_data *data)
{
	int ret = 0;
	int i = 0;
	u8 pointid;
	int base;
	struct ts_event *events = data->events;
	int max_touch_num = FTS_MAX_POINTS_SUPPORT;
	u8 *buf = data->point_buf;

	data->point_num = 0;
	data->touch_point = 0;

	memset(buf, 0xFF, data->pnt_buf_size);
	buf[0] = 0x00;

	ret = fts_i2c_read(data->client, buf, 1, buf, data->pnt_buf_size);
	if (ret < 0) {
		FTS_ERROR("read touchdata failed, ret:%d", ret);
		return ret;
	}
	data->point_num = buf[FTS_TOUCH_POINT_NUM] & 0x0F;
	if (data->point_num > max_touch_num) {
		FTS_INFO("invalid point_num(%d)", data->point_num);
		return -EIO;
	}

#if (FTS_DEBUG_EN && (FTS_DEBUG_LEVEL == 2))
	fts_show_touch_buffer(buf, data->point_num);
#endif

	for (i = 0; i < max_touch_num; i++) {
		base = FTS_ONE_TCH_LEN * i;

		pointid = (buf[FTS_TOUCH_ID_POS + base]) >> 4;
		if (pointid >= FTS_MAX_ID)
			break;
		else if (pointid >= max_touch_num) {
			FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
			return -EINVAL;
		}

		data->touch_point++;

		events[i].x = ((buf[FTS_TOUCH_X_H_POS + base] & 0x0F) << 8) +
		(buf[FTS_TOUCH_X_L_POS + base] & 0xFF);
		events[i].y = ((buf[FTS_TOUCH_Y_H_POS + base] & 0x0F) << 8) +
		(buf[FTS_TOUCH_Y_L_POS + base] & 0xFF);
		events[i].flag = buf[FTS_TOUCH_EVENT_POS + base] >> 6;
		events[i].id = buf[FTS_TOUCH_ID_POS + base] >> 4;
		events[i].area = buf[FTS_TOUCH_AREA_POS + base] >> 4;
		events[i].p =  buf[FTS_TOUCH_PRE_POS + base];

		if (EVENT_DOWN(events[i].flag) && (data->point_num == 0)) {
			FTS_INFO("abnormal touch data from fw");
			return -EIO;
		}
	}
	if (data->touch_point == 0) {
		FTS_INFO("no touch point information");
		return -EIO;
	}

	return 0;
}

/*****************************************************************************
*  Name: tpd_eint_interrupt_handler
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static irqreturn_t tpd_eint_interrupt_handler(int irq, void *dev_id)
{
	tpd_flag = 1;
	printk(KERN_INFO "%s Focal Interrupt received!\n", __func__);
	wake_up_interruptible(&waiter);
	return IRQ_HANDLED;
}

/*****************************************************************************
*  Name: fts_irq_registration
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_irq_registration(struct fts_ts_data *ts_data)
{
	int ret = 0;
	struct device_node *node = NULL;
	/* use it directly if the former function has alread read int-gpio from dts */
	if (tpd_int_gpio_number) {

		ts_data->irq = gpio_to_irq(tpd_int_gpio_number);
	} else {
	/* read int-gpio from dts */
		node = of_find_matching_node(node, fts_dt_match);
		if (NULL == node) {
			FTS_ERROR("Can not find touch eint device node!");
			return -ENODATA;
		}

		ts_data->irq = of_get_named_gpio(node, "focaltech,irq-gpio", 0);
		gpio_direction_input(ts_data->irq);

	}

	ts_data->client->irq = ts_data->irq;
	FTS_INFO("IRQ request succussfully, ts_data->irq:%d i2c_client->irq:%d", ts_data->irq, ts_data->client->irq);
	ret = devm_request_irq(&ts_data->client->dev, ts_data->irq, tpd_eint_interrupt_handler,
		IRQF_TRIGGER_FALLING, "TOUCH_PANEL-eint", NULL);
	if (!ret) {
		printk(KERN_INFO "%s<FTS_INFO>focaltech irq registeration success!\n", __func__);
	} else {
		printk(KERN_ERR "%s<FTS_ERR>focaltech irq registeration fail!\n", __func__);
	}

	return ret;
}

#if defined(CONFIG_FB)
#if defined(CONFIG_TOUCHSCREEN_MTK_VDD_SUSPEND)
/*****************************************************************************
*  Name: fts_fb_registration
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
/* frame buffer notifier block control the suspend/resume procedure */
static int fts_fb_notifier_callback(struct notifier_block *noti,
		unsigned long event, void *data)
{
	struct fb_event *ev_data = data;
	struct fts_ts_data *ts = container_of(noti, struct fts_ts_data, notifier);
	int *blank;

	if (ev_data && ev_data->data && event == FB_EVENT_BLANK && ts) {
		blank = ev_data->data;
		if (*blank == FB_BLANK_UNBLANK) {
			printk(KERN_INFO "Resume by fb notifier.\n");
			fts_ts_resume();
		} else if (*blank == FB_BLANK_POWERDOWN) {
			printk(KERN_INFO "Suspend by fb notifier.\n");
			fts_ts_suspend();
		}
	}

	return 0;
}

static void fts_fb_registration(struct fts_ts_data *ts)
{
	ts->notifier.notifier_call = fts_fb_notifier_callback;
	fb_register_client(&ts->notifier);
}

/*****************************************************************************
*  Name: fts_fb_unregistration
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void fts_fb_unregistration(struct fts_ts_data *ts)
{
	fb_unregister_client(&ts->notifier);
}
#endif
#endif

/*****************************************************************************
*  Name: touch_event_handler
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int touch_event_handler(void *unused)
{
	int ret;
	struct fts_ts_data *ts_data = fts_data;
	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };

	sched_setscheduler(current, SCHED_RR, &param);
	do {
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tpd_flag != 0);

		tpd_flag = 0;

		set_current_state(TASK_RUNNING);

		FTS_DEBUG("touch_event_handler start");
#if FTS_PSENSOR_EN
		if (fts_proximity_readdata(ts_data->client) == 0)
			continue;
#endif

#if FTS_GESTURE_EN
		if (0 == fts_gesture_readdata(ts_data)) {
			FTS_INFO("succuss to get gesture data in irq handler");
			continue;
		}
#endif

#if FTS_POINT_REPORT_CHECK_EN
		fts_prc_queue_work(ts_data);
#endif

#if FTS_ESDCHECK_EN
		fts_esdcheck_set_intr(1);
#endif

		ret = fts_read_touchdata(ts_data);
#if FTS_MT_PROTOCOL_B_EN
		if (ret == 0) {
			mutex_lock(&ts_data->report_mutex);
			fts_input_report_b(ts_data);
			mutex_unlock(&ts_data->report_mutex);
		}
#else
		if (ret == 0) {
			mutex_lock(&ts_data->report_mutex);
			fts_input_report_a(ts_data);
			mutex_unlock(&ts_data->report_mutex);
		}
#endif

#if FTS_ESDCHECK_EN
		fts_esdcheck_set_intr(0);
#endif
	} while (!kthread_should_stop());

	return 0;
}

/*****************************************************************************
*  Name: fts_input_init
*  Brief: input device init
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_input_init(struct fts_ts_data *ts_data)
{
	int point_num = 0;
	int ret = 0;

	FTS_FUNC_ENTER();

	point_num = FTS_MAX_POINTS_SUPPORT;
	ts_data->pnt_buf_size = point_num * FTS_ONE_TCH_LEN + 3;
	ts_data->point_buf = (u8 *)kzalloc(ts_data->pnt_buf_size, GFP_KERNEL);
	if (!ts_data->point_buf) {
		FTS_ERROR("failed to alloc memory for point buf!");
		ret = -ENOMEM;
		goto err_point_buf;
	}

	ts_data->events = (struct ts_event *)kzalloc(point_num * sizeof(struct ts_event), GFP_KERNEL);
	if (!ts_data->events) {

		FTS_ERROR("failed to alloc memory for point events!");
		ret = -ENOMEM;
		goto err_event_buf;
	}

	FTS_FUNC_EXIT();
	return 0;

	err_event_buf:
	kfree_safe(ts_data->point_buf);

	err_point_buf:
	FTS_FUNC_EXIT();
	return ret;
}


/************************************************************************
* Name: tpd_i2c_detect
* Brief:
* Input:
* Output:
* Return:
***********************************************************************/
static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, TPD_DEVICE);

	return 0;
}

/************************************************************************
* Name: fts_probe
* Brief:
* Input:
* Output:
* Return:
***********************************************************************/
int tpd_get_regulator_focal(struct i2c_client *pdev)
{
	struct regulator *vcc_io;/* 1V8 */
	struct regulator *vdd;/* 3V3 */

	vcc_io = devm_regulator_get(&pdev->dev, "vccio");
	if (IS_ERR(vcc_io))
		return PTR_ERR(vcc_io);

	vdd = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(vdd)) {
		devm_regulator_put(vcc_io);
		return PTR_ERR(vdd);
	}

	fts_data->io_reg = vcc_io;
	fts_data->reg = vdd;

	return 0;
}

int tpd_get_gpio_info_focal(struct i2c_client *pdev)
{
	u32 vals[FTS_CALIBRATION_DATA_SIZE];
	u32 val;

	tpd_rst_gpio_number = of_get_named_gpio(pdev->dev.of_node, "focaltech,rst-gpio", 0);
	if (!gpio_is_valid(tpd_rst_gpio_number)) {
		printk(KERN_ERR "%s<FTS_ERR>failed to get rst-gpio\n", __func__);
		return -1;
	}

	tpd_int_gpio_number = of_get_named_gpio(pdev->dev.of_node, "focaltech,irq-gpio", 0);
	if(!gpio_is_valid(tpd_int_gpio_number)) {
		printk(KERN_ERR "%s<FTS_ERR>failed to get irq-gpio\n", __func__);
		return -1;
	}

	if(!of_property_read_u32_array(pdev->dev.of_node, "focaltech,display-data",
		vals, FTS_CALIBRATION_DATA_SIZE)) {
		fts_data->x_axis_min = vals[0];
	fts_data->x_axis_max = vals[1];
	fts_data->y_axis_min = vals[2];
	fts_data->y_axis_max = vals[3];
}

if(!of_property_read_u32(pdev->dev.of_node, "focaltech,fts-key-number", &val))
	fts_data->fts_key_number = val;

gpio_direction_output(tpd_rst_gpio_number, 1);
msleep(20);
gpio_direction_input(tpd_int_gpio_number);
msleep(5);

return 0;
}


static int fts_request_input_dev(void)
{

	struct input_dev *buf_input_dev;

	if (!fts_data) {
		printk(KERN_ERR "%s<FTS_ERR>not found the focaltech base memory: fts_data\n", __func__);
		return -ENOMEM;

	}

	buf_input_dev = input_allocate_device();
	if (!buf_input_dev) {
		printk(KERN_ERR "%s<FTS_ERR>failed to alloc input_dev\n", __func__);
		return -ENOMEM;
	}

	/* set driver info */
	buf_input_dev->name = "focaltech_tp";
	buf_input_dev->phys = "input/ts";
	buf_input_dev->id.bustype = BUS_I2C;

	/* set basic command */
	set_bit(BTN_TOUCH, buf_input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, buf_input_dev->propbit);

	/* set general abs_paras */
	buf_input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	set_bit(ABS_MT_TRACKING_ID, buf_input_dev->absbit);
	set_bit(ABS_MT_POSITION_X, buf_input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, buf_input_dev->absbit);
#ifdef FTS_REPORT_PRESSURE_EN
	set_bit(ABS_MT_PRESSURE, buf_input_dev->absbit);
#endif
	set_bit(ABS_MT_TOUCH_MAJOR, buf_input_dev->absbit);

	/* init general abs_paras */
	input_set_abs_params(buf_input_dev, ABS_MT_POSITION_X, (unsigned long)fts_data->x_axis_min, (unsigned long)fts_data->x_axis_max, 0, 0);
	input_set_abs_params(buf_input_dev, ABS_MT_POSITION_Y, (unsigned long)fts_data->y_axis_min, (unsigned long)fts_data->y_axis_max, 0, 0);
#ifdef FTS_REPORT_PRESSURE_EN
	input_set_abs_params(buf_input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
#endif
	input_set_abs_params(buf_input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);

	/* special paras init, various from protocol A&B */
#ifdef FTS_MT_PROTOCOL_B_EN
	input_mt_init_slots(buf_input_dev, FTS_MAX_POINTS_SUPPORT, INPUT_MT_DIRECT);
	input_set_abs_params(buf_input_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);
#else
	input_set_abs_params(buf_input_dev, ABS_MT_TRACKING_ID, 0, FTS_MAX_POINTS_SUPPORT, 0, 0);
#endif

	if (input_register_device(buf_input_dev)) {
		printk(KERN_ERR "<FTS_ERR> Register %s input device failed", buf_input_dev->name);
		return -ENODEV;
	}

	fts_data->input_dev = buf_input_dev;

	return 0;
}

static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct fts_ts_data *ts_data;

	printk(KERN_ERR "%s<FTS>Enter focaltech probe\n", __func__);
	FTS_FUNC_ENTER();
	ts_data = devm_kzalloc(&client->dev, sizeof(*ts_data), GFP_KERNEL);
	if (!ts_data) {
		FTS_ERROR("Failed to allocate memory for fts_data");
		return -ENOMEM;
	}

	fts_data = ts_data;
	ts_data->client = client;

	ts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");
	if (NULL == ts_data->ts_workqueue) {
		FTS_ERROR("failed to create fts workqueue");
	}
	/* get gpio and regulator */
	tpd_get_gpio_info_focal(client);
#if FTS_POWER_SOURCE_CUST_EN
	fts_power_init();
#else
	tpd_get_regulator_focal(client);
#endif

	/* turn on power */
#if FTS_POWER_SOURCE_CUST_EN
	fts_power_resume();
#else
	fts_power_on(1);
#endif

	spin_lock_init(&ts_data->irq_lock);
	mutex_init(&ts_data->report_mutex);

	/* allocate and register input_dev */
	if(fts_request_input_dev() != 0) {
		printk(KERN_ERR "%s<FTS>failed to register fts_data->input_dev\n", __func__);
		goto err_input_init;
	}

	/* Configure gpio to irq and request irq */
	ret = fts_irq_registration(ts_data);
	if (ret) {
		FTS_ERROR("request irq failed");
		goto err_irq_req;
	}

#if defined(CONFIG_FB)
#if defined(CONFIG_TOUCHSCREEN_MTK_VDD_SUSPEND)
	fts_fb_registration(ts_data);
#endif
#endif

	mdelay(10);

	/* Init I2C */
	fts_i2c_init();

	ret = fts_input_init(ts_data);
	if (ret) {
		FTS_ERROR("fts input initialize fail");
		goto err_input_init;
	}

	fts_reset_proc(200);
	ret = fts_get_ic_information(ts_data);
	if (ret) {
		FTS_ERROR("not focal IC, unregister driver");
		goto err_input_init;
	}

	ts_data->thread_tpd = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(ts_data->thread_tpd)) {
		ret = PTR_ERR(ts_data->thread_tpd);
		FTS_ERROR("[TPD]Failed to create kernel thread_tpd,ret:%d", ret);
		ts_data->thread_tpd = NULL;
		goto err_input_init;
	}

	/* check the macro in focaltech_config.h */
#if FTS_APK_NODE_EN
	ret = fts_create_apk_debug_channel(ts_data);
	if (ret) {
		FTS_ERROR("create apk debug node fail");
	}
#endif

#if FTS_SYSFS_NODE_EN
	ret = fts_create_sysfs(client);
	if (ret) {
		FTS_ERROR("create sysfs node fail");
	}
#endif

#if FTS_POINT_REPORT_CHECK_EN
	ret = fts_point_report_check_init(ts_data);
	if (ret) {
		FTS_ERROR("init point report check fail");
	}
#endif
	
	ret = fts_ex_mode_init(client);
	if (ret) {
		FTS_ERROR("init glove/cover/charger fail");
	}

#if FTS_GESTURE_EN
	ret = fts_gesture_init(ts_data);
	if (ret) {
		FTS_ERROR("init gesture fail");
	}
#endif

#if FTS_PSENSOR_EN
	fts_proximity_init(client);
#endif

#if FTS_TEST_EN
	ret = fts_test_init(client);
	if (ret) {
		FTS_ERROR("init production test fail");
	}
#endif

#if FTS_ESDCHECK_EN
	ret = fts_esdcheck_init(ts_data);
	if (ret) {
		FTS_ERROR("init esd check fail");
	}
#endif

#if FTS_AUTO_UPGRADE_EN
	ret = fts_fwupg_init(ts_data);
	if (ret) {
		FTS_ERROR("init fw upgrade fail");
	}
#endif

	FTS_DEBUG("[TPD]Touch Panel Device Probe %s!", (ret < 0) ? "FAIL" : "PASS");
	tpd_load_status = 1;
	FTS_DEBUG("FTS_RES_Y:%d", (int)fts_data->y_axis_max);
	FTS_FUNC_EXIT();
	return 0;

	err_input_init:
	err_irq_req:
	if (ts_data->thread_tpd) {
		kthread_stop(ts_data->thread_tpd);
		ts_data->thread_tpd = NULL;
	}
	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);

	#if defined(CONFIG_FB)
	#if defined(CONFIG_TOUCHSCREEN_MTK_VDD_SUSPEND)
	fts_fb_unregistration(ts_data);
	#endif
	#endif
	kfree_safe(ts_data->point_buf);
	kfree_safe(ts_data->events);
	devm_kfree(&client->dev, ts_data);

	FTS_FUNC_EXIT();
	return ret;
}

/*****************************************************************************
*  Name: tpd_remove
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int tpd_remove(struct i2c_client *client)
{

	struct fts_ts_data *ts_data = i2c_get_clientdata(client);

	FTS_FUNC_ENTER();

#if FTS_TEST_EN
	fts_test_exit(client);
#endif

#if FTS_POINT_REPORT_CHECK_EN
	fts_point_report_check_exit();
#endif

#if FTS_SYSFS_NODE_EN
	fts_remove_sysfs(client);
#endif

	fts_ex_mode_exit(client);

#if FTS_PSENSOR_EN
	fts_proximity_exit(client);
#endif
#if FTS_APK_NODE_EN
	fts_release_apk_debug_channel(ts_data);
#endif

#if FTS_AUTO_UPGRADE_EN
	fts_fwupg_exit(ts_data);
#endif

#if FTS_ESDCHECK_EN
	fts_esdcheck_exit(ts_data);
#endif

#if FTS_GESTURE_EN
	fts_gesture_exit(client);
#endif

#if defined(CONFIG_FB)
#if defined(CONFIG_TOUCHSCREEN_MTK_VDD_SUSPEND)
	fts_fb_unregistration(ts_data);
#endif
#endif

	fts_i2c_exit();

	kfree_safe(ts_data->point_buf);
	kfree_safe(ts_data->events);

	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);

	FTS_FUNC_EXIT();

	return 0;
}

/*****************************************************************************
*  Name: tpd_driver_init
*  Brief: 1. Get dts information
*         2. call tpd_driver_add to add tpd_device_driver
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int __init tpd_driver_init(void)
{

	FTS_FUNC_ENTER();
	FTS_INFO("Driver version: %s", FTS_DRIVER_VERSION);
	if (i2c_add_driver(&tpd_i2c_driver) != 0) {
		printk(KERN_ERR "%s<FTS_ERR>focatech i2c_add_driver failed\n", __func__);
		return -1;
	}

	FTS_FUNC_EXIT();
	return 0;
}

/*****************************************************************************
*  Name: tpd_driver_exit
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void __exit tpd_driver_exit(void)
{
	FTS_FUNC_ENTER();
	i2c_del_driver(&tpd_i2c_driver);
	FTS_FUNC_EXIT();
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver for Mediatek");
MODULE_LICENSE("GPL v2");
