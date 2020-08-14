/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>

#include "kd_camera_hw.h"

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_camera_feature.h"

/******************************************************************************
 * Debug configuration
 *****************************************************************************/
#undef pr_fmt
#ifndef pr_fmt
#define PFX "[kd_camera_hw]"
#define pr_fmt(fmt) PFX ":" fmt
#endif

#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, args...)    pr_debug(PFX  fmt, ##args)

/* #undef DEBUG_CAMERA_HW_K */
#define DEBUG_CAMERA_HW_K
#ifdef DEBUG_CAMERA_HW_K
#define PK_DBG PK_DBG_FUNC
#define PK_ERR(fmt, arg...) pr_debug(fmt, ##arg)
#define PK_XLOG_INFO(fmt, args...)  pr_debug(PFX  fmt, ##args)
#else
#define PK_DBG(a, ...)
#define PK_ERR(a, ...)
#define PK_XLOG_INFO(fmt, args...)
#endif


/* GPIO Pin control*/
struct platform_device *cam_plt_dev;
struct pinctrl *camctrl;
struct pinctrl_state *cam0_pnd_h;
struct pinctrl_state *cam0_pnd_l;
struct pinctrl_state *cam0_rst_h;
struct pinctrl_state *cam0_rst_l;
struct pinctrl_state *cam1_pnd_h;
struct pinctrl_state *cam1_pnd_l;
struct pinctrl_state *cam1_rst_h;
struct pinctrl_state *cam1_rst_l;
struct pinctrl_state *cam_ldo0_h;
struct pinctrl_state *cam_ldo0_l;

int mtkcam_gpio_init(struct platform_device *pdev)
{
	int ret = 0;

	camctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(camctrl)) {
		dev_err(&pdev->dev, "Cannot find camera pinctrl!");
		ret = PTR_ERR(camctrl);
	}
	/*Cam0 Power/Rst Ping initialization */
	cam0_pnd_h = pinctrl_lookup_state(camctrl, "cam0_pnd1");
	if (IS_ERR(cam0_pnd_h)) {
		ret = PTR_ERR(cam0_pnd_h);
		pr_err("%s : pinctrl err, cam0_pnd_h\n", __func__);
	}

	cam0_pnd_l = pinctrl_lookup_state(camctrl, "cam0_pnd0");
	if (IS_ERR(cam0_pnd_l)) {
		ret = PTR_ERR(cam0_pnd_l);
		pr_err("%s : pinctrl err, cam0_pnd_l\n", __func__);
	}


	cam0_rst_h = pinctrl_lookup_state(camctrl, "cam0_rst1");
	if (IS_ERR(cam0_rst_h)) {
		ret = PTR_ERR(cam0_rst_h);
		pr_err("%s : pinctrl err, cam0_rst_h\n", __func__);
	}

	cam0_rst_l = pinctrl_lookup_state(camctrl, "cam0_rst0");
	if (IS_ERR(cam0_rst_l)) {
		ret = PTR_ERR(cam0_rst_l);
		pr_err("%s : pinctrl err, cam0_rst_l\n", __func__);
	}

	/*Cam1 Power/Rst Ping initialization */
	cam1_pnd_h = pinctrl_lookup_state(camctrl, "cam1_pnd1");
	if (IS_ERR(cam1_pnd_h)) {
		ret = PTR_ERR(cam1_pnd_h);
		pr_err("%s : pinctrl err, cam1_pnd_h\n", __func__);
	}

	cam1_pnd_l = pinctrl_lookup_state(camctrl, "cam1_pnd0");
	if (IS_ERR(cam1_pnd_l)) {
		ret = PTR_ERR(cam1_pnd_l);
		pr_err("%s : pinctrl err, cam1_pnd_l\n", __func__);
	}


	cam1_rst_h = pinctrl_lookup_state(camctrl, "cam1_rst1");
	if (IS_ERR(cam1_rst_h)) {
		ret = PTR_ERR(cam1_rst_h);
		pr_err("%s : pinctrl err, cam1_rst_h\n", __func__);
	}


	cam1_rst_l = pinctrl_lookup_state(camctrl, "cam1_rst0");
	if (IS_ERR(cam1_rst_l)) {
		ret = PTR_ERR(cam1_rst_l);
		pr_err("%s : pinctrl err, cam1_rst_l\n", __func__);
	}
	/*externel LDO enable */
	cam_ldo0_h = pinctrl_lookup_state(camctrl, "cam_ldo0_1");
	if (IS_ERR(cam_ldo0_h)) {
		ret = PTR_ERR(cam_ldo0_h);
		pr_err("%s : pinctrl err, cam_ldo0_h\n", __func__);
	}


	cam_ldo0_l = pinctrl_lookup_state(camctrl, "cam_ldo0_0");
	if (IS_ERR(cam_ldo0_l)) {
		ret = PTR_ERR(cam_ldo0_l);
		pr_err("%s : pinctrl err, cam_ldo0_l\n", __func__);
	}
	return ret;
}

int mtkcam_gpio_set(int PinIdx, int PwrType, int Val)
{
	int ret = 0;

	switch (PwrType) {
	case CAMRST:
		if (PinIdx == 0) {
			if (Val == 0)
				pinctrl_select_state(camctrl, cam0_rst_l);
			else
				pinctrl_select_state(camctrl, cam0_rst_h);
		} else {
			if (Val == 0)
				pinctrl_select_state(camctrl, cam1_rst_l);
			else
				pinctrl_select_state(camctrl, cam1_rst_h);
		}
		break;
	case CAMPDN:
		if (PinIdx == 0) {
			if (Val == 0)
				pinctrl_select_state(camctrl, cam0_pnd_l);
			else
				pinctrl_select_state(camctrl, cam0_pnd_h);
		} else {
			if (Val == 0)
				pinctrl_select_state(camctrl, cam1_pnd_l);
			else
				pinctrl_select_state(camctrl, cam1_pnd_h);
		}

		break;
	case CAMLDO:
		if (Val == 0)
			pinctrl_select_state(camctrl, cam_ldo0_l);
		else
			pinctrl_select_state(camctrl, cam_ldo0_h);
		break;
	default:
		pr_debug("PwrType(%d) is invalid !!\n", PwrType);
		break;
	};

	pr_debug("PinIdx(%d) PwrType(%d) val(%d)\n", PinIdx, PwrType, Val);

	return ret;
}


int cntVCAMD;
int cntVCAMA;
int cntVCAMIO;
int cntVCAMAF;
int cntVCAMD_SUB;

static DEFINE_SPINLOCK(kdsensor_pw_cnt_lock);


bool _hwPowerOnCnt(int PinIdx, enum KD_REGULATOR_TYPE_T powerId, int powerVolt,
	char *mode_name)
{
	if (_hwPowerOn(PinIdx, powerId, powerVolt)) {
		spin_lock(&kdsensor_pw_cnt_lock);
		if (powerId == VCAMD)
			cntVCAMD += 1;
		else if (powerId == VCAMA)
			cntVCAMA += 1;
		else if (powerId == VCAMIO)
			cntVCAMIO += 1;
		else if (powerId == VCAMAF)
			cntVCAMAF += 1;
		else if (powerId == SUB_VCAMD)
			cntVCAMD_SUB += 1;
		spin_unlock(&kdsensor_pw_cnt_lock);
		return true;
	}
	return false;
}

bool _hwPowerDownCnt(int PinIdx, enum KD_REGULATOR_TYPE_T powerId,
	char *mode_name)
{

	if (_hwPowerDown(PinIdx, powerId)) {
		spin_lock(&kdsensor_pw_cnt_lock);
		if (powerId == VCAMD)
			cntVCAMD -= 1;
		else if (powerId == VCAMA)
			cntVCAMA -= 1;
		else if (powerId == VCAMIO)
			cntVCAMIO -= 1;
		else if (powerId == VCAMAF)
			cntVCAMAF -= 1;
		else if (powerId == SUB_VCAMD)
			cntVCAMD_SUB -= 1;
		spin_unlock(&kdsensor_pw_cnt_lock);
		return true;
	}
	return false;
}

void checkPowerBeforClose(int PinIdx, char *mode_name)
{

	int i = 0;

	pr_debug
	    ("[checkPowerBeforClose]cntVCAMD:%d, cntVCAMA:%d,cntVCAMIO:%d, ",
	    cntVCAMD, cntVCAMA, cntVCAMIO);
	pr_debug("cntVCAMAF:%d, cntVCAMD_SUB:%d,\n",
	     cntVCAMAF, cntVCAMD_SUB);


	for (i = 0; i < cntVCAMD; i++)
		_hwPowerDown(PinIdx, VCAMD);
	for (i = 0; i < cntVCAMA; i++)
		_hwPowerDown(PinIdx, VCAMA);
	for (i = 0; i < cntVCAMIO; i++)
		_hwPowerDown(PinIdx, VCAMIO);
	for (i = 0; i < cntVCAMAF; i++)
		_hwPowerDown(PinIdx, VCAMAF);
	for (i = 0; i < cntVCAMD_SUB; i++)
		_hwPowerDown(PinIdx, SUB_VCAMD);

	cntVCAMD = 0;
	cntVCAMA = 0;
	cntVCAMIO = 0;
	cntVCAMAF = 0;
	cntVCAMD_SUB = 0;

	mtkcam_gpio_set(PinIdx, CAMRST, 0);
}



int kdCISModulePowerOn(enum CAMERA_DUAL_CAMERA_SENSOR_ENUM SensorIdx,
	char *currSensorName, bool On, char *mode_name)
{

	u32 pinSetIdx = 0;

#define IDX_PS_CMRST 0
#define IDX_PS_CMPDN 4
#define IDX_PS_MODE 1
#define IDX_PS_ON   2
#define IDX_PS_OFF  3

#define VOL_2800 2800000
#define VOL_1800 1800000
#define VOL_1500 1500000
#define VOL_1200 1200000
#define VOL_1000 1000000

	u32 pinSet[3][8] = {

		{
			CAMERA_CMRST_PIN,
			CAMERA_CMRST_PIN_M_GPIO,	/* mode */
			GPIO_OUT_ONE,	/* ON state */
			GPIO_OUT_ZERO,	/* OFF state */
			CAMERA_CMPDN_PIN,
			CAMERA_CMPDN_PIN_M_GPIO,
			GPIO_OUT_ONE,
			GPIO_OUT_ZERO,
		},
		{
			CAMERA_CMRST1_PIN,
			CAMERA_CMRST1_PIN_M_GPIO,
			GPIO_OUT_ONE,
			GPIO_OUT_ZERO,
			CAMERA_CMPDN1_PIN,
			CAMERA_CMPDN1_PIN_M_GPIO,
			GPIO_OUT_ONE,
			GPIO_OUT_ZERO,
		},
		{
			GPIO_CAMERA_INVALID,
			GPIO_CAMERA_INVALID,	/* mode */
			GPIO_OUT_ONE,	/* ON state */
			GPIO_OUT_ZERO,	/* OFF state */
			GPIO_CAMERA_INVALID,
			GPIO_CAMERA_INVALID,
			GPIO_OUT_ONE,
			GPIO_OUT_ZERO,
		}
	};


	if (SensorIdx == DUAL_CAMERA_MAIN_SENSOR)
		pinSetIdx = 0;
	else if (SensorIdx == DUAL_CAMERA_SUB_SENSOR)
		pinSetIdx = 1;
	else if (SensorIdx == DUAL_CAMERA_MAIN_2_SENSOR)
		pinSetIdx = 2;

	if (On) {

         /* VCAM_I2C */
        if (TRUE != _hwPowerOnCnt(pinSetIdx, VCAMI2C, VOL_1800, mode_name)) {
            PK_ERR("[CAMERA SENSOR] Fail to enable digital power (VCAMI2C), power id = %d \n", VCAMI2C);
            goto _kdCISModulePowerOn_exit_;
        }
		/* depend on ISP module ready and open */
		ISP_MCLK1_EN(1);
		/* end */
		if (pinSetIdx == 0 && currSensorName && (strcmp(currSensorName,
						SENSOR_DRVNAME_GC2375MIPI_RAW_CHXT_REAR) == 0)) {

			PK_ERR("[PowerON]pinSetIdx:%d, currSensorName: %s\n",
					pinSetIdx, currSensorName);
			if (pinSet[pinSetIdx][IDX_PS_CMPDN] != GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMPDN, 1);

			if (pinSet[pinSetIdx][IDX_PS_CMRST] != GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMRST, 0);

			mdelay(5);
			/* VCAM_IO */
			if (_hwPowerOnCnt(pinSetIdx, VCAMIO, VOL_1800, mode_name) != TRUE) {
				PK_ERR("[CAMERA SENSOR] Fail to enable digital power (VCAM_IO), power id = %d\n", VCAMIO);
				goto _kdCISModulePowerOn_exit_;
			}

			mdelay(5);

			if (_hwPowerOnCnt(pinSetIdx, VCAMD, VOL_1800, mode_name) != TRUE) {
				PK_ERR("[CAMERA SENSOR] Fail to enable digital power (VCAMD), power id = %d\n", VCAMD);
				goto _kdCISModulePowerOn_exit_;
			}
			mdelay(5);
			/* VCAM_A */
			if (_hwPowerOnCnt(pinSetIdx, VCAMA, VOL_2800, mode_name) != TRUE) {
				PK_ERR("[CAMERA SENSOR] Fail to enable analog power (VCAM_A), power id = %d\n", VCAMA);
				goto _kdCISModulePowerOn_exit_;
			}

			mdelay(10);

			/* enable active sensor */
			if (pinSet[pinSetIdx][IDX_PS_CMPDN]
					!= GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMPDN, 0);
			mdelay(5);
			if (pinSet[pinSetIdx][IDX_PS_CMRST] != GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMRST, 1);

			mdelay(10);
		} else if (pinSetIdx == 0 && currSensorName && (strcmp(currSensorName,
							SENSOR_DRVNAME_SP2509MIPI_RAW_BLX_REAR) == 0)) {

			PK_ERR("[PowerON]pinSetIdx:%d, currSensorName: %s\n", pinSetIdx,currSensorName);
			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMPDN])
				mtkcam_gpio_set(pinSetIdx, CAMPDN, 1);

			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST])
				mtkcam_gpio_set(pinSetIdx, CAMRST, 0);

			mdelay(5);
			/* VCAM_IO */
			if (TRUE != _hwPowerOnCnt(pinSetIdx, VCAMIO, VOL_1800, mode_name)) {
				PK_ERR("[CAMERA SENSOR] Fail to enable digital power (VCAM_IO), power id = %d \n", VCAMIO);
				goto _kdCISModulePowerOn_exit_;
			}

			mdelay(5);
/*
			if (TRUE != _hwPowerOnCnt(pinSetIdx, VCAMD, VOL_1200, mode_name)) {
				PK_ERR("[CAMERA SENSOR] Fail to enable digital power (VCAMD), power id = %d\n", VCAMD);
				goto _kdCISModulePowerOn_exit_;
			}
			mdelay(5);
*/			/* VCAM_A */
			if (TRUE != _hwPowerOnCnt(pinSetIdx, VCAMA, VOL_2800, mode_name)) {
				PK_ERR("[CAMERA SENSOR] Fail to enable analog power (VCAM_A), power id = %d\n", VCAMA);
				goto _kdCISModulePowerOn_exit_;
			}

			mdelay(5);

			/* enable active sensor */
			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMPDN])
				mtkcam_gpio_set(pinSetIdx, CAMPDN, 0);
			mdelay(5);
			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST])
				mtkcam_gpio_set(pinSetIdx, CAMRST, 1);
			mdelay(10);
		} else if (pinSetIdx == 1 && currSensorName && (0 ==
					strcmp(currSensorName,
						SENSOR_DRVNAME_GC2375MIPI_RAW_BLX_FRONT))) {

			PK_ERR("[PowerON]pinSetIdx:%d, currSensorName: %s\n",
					pinSetIdx, currSensorName);
			if (pinSet[pinSetIdx][IDX_PS_CMPDN] != GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMPDN, 1);

			if (pinSet[pinSetIdx][IDX_PS_CMRST] != GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMRST, 0);

			mdelay(5);
			/* VCAM_IO */
			if (_hwPowerOnCnt(pinSetIdx, VCAMIO, VOL_1800, mode_name)
					!= TRUE) {
				PK_ERR("[CAMERA SENSOR] Fail to enable digital power (VCAM_IO), power id = %d\n", VCAMIO);
				goto _kdCISModulePowerOn_exit_;
			}

			mdelay(5);

			if (_hwPowerOnCnt(pinSetIdx, VCAMD, VOL_1800, mode_name)
					!= TRUE) {
				PK_ERR("[CAMERA SENSOR] Fail to enable digital power (VCAMD), power id = %d\n", VCAMD);
				goto _kdCISModulePowerOn_exit_;
			}
			mdelay(5);
			/* VCAM_A */
			if (_hwPowerOnCnt(pinSetIdx, VCAMA, VOL_2800, mode_name)
					!= TRUE) {
				PK_ERR("[CAMERA SENSOR] Fail to enable analog power (VCAM_A), power id = %d\n", VCAMA);
				goto _kdCISModulePowerOn_exit_;
			}

			mdelay(10);

			/* enable active sensor */
			if (pinSet[pinSetIdx][IDX_PS_CMPDN]
					!= GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMPDN, 0);
			mdelay(5);
			if (pinSet[pinSetIdx][IDX_PS_CMRST] != GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMRST, 1);

			mdelay(10);
		} else if (pinSetIdx == 1 && currSensorName && (strcmp(currSensorName,
							SENSOR_DRVNAME_SP2509MIPI_RAW_CHXT_FRONT) == 0)) {

			PK_ERR("[PowerON]pinSetIdx:%d, currSensorName: %s\n", pinSetIdx,currSensorName);
			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMPDN])
				mtkcam_gpio_set(pinSetIdx, CAMPDN, 1);

			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST])
				mtkcam_gpio_set(pinSetIdx, CAMRST, 0);

			mdelay(5);
			/* VCAM_IO */
			if (TRUE != _hwPowerOnCnt(pinSetIdx, VCAMIO, VOL_1800, mode_name)) {
				PK_ERR("[CAMERA SENSOR] Fail to enable digital power (VCAM_IO), power id = %d \n", VCAMIO);
				goto _kdCISModulePowerOn_exit_;
			}

			mdelay(5);
/*
			if (TRUE != _hwPowerOnCnt(pinSetIdx, VCAMD, VOL_1200, mode_name)) {
				PK_ERR("[CAMERA SENSOR] Fail to enable digital power (VCAMD), power id = %d\n", VCAMD);
				goto _kdCISModulePowerOn_exit_;
			}
			mdelay(5);
*/			/* VCAM_A */
			if (TRUE != _hwPowerOnCnt(pinSetIdx, VCAMA, VOL_2800, mode_name)) {
				PK_ERR("[CAMERA SENSOR] Fail to enable analog power (VCAM_A), power id = %d\n", VCAMA);
				goto _kdCISModulePowerOn_exit_;
			}

			mdelay(5);

			/* enable active sensor */
			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMPDN])
				mtkcam_gpio_set(pinSetIdx, CAMPDN, 0);
			mdelay(5);
			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST])
				mtkcam_gpio_set(pinSetIdx, CAMRST, 1);
			mdelay(10);
		} else if (pinSetIdx == 0 && currSensorName && (
			strcmp(currSensorName,
			SENSOR_DRVNAME_GC2145_MIPI_YUV) == 0)) {
			/* First Power Pin low and Reset Pin Low */
			if (pinSet[pinSetIdx][IDX_PS_CMPDN]
					!= GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMPDN,
				pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_ON]);

			if (pinSet[pinSetIdx][IDX_PS_CMRST]
					!= GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMRST,
				pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_OFF]);

			mdelay(1);
			/* VCAM_IO */
			if (_hwPowerOnCnt(pinSetIdx, VCAMIO, VOL_1800,
				mode_name) != TRUE) {
				pr_err("[CAMERA SENSOR] Fail to enable ");
				pr_err("digital power (VCAM_IO), ");
				pr_err("power id = %d\n", VCAMIO);
				goto _kdCISModulePowerOn_exit_;
			}

			mdelay(2);

			/* VCAM_A */
			if (_hwPowerOnCnt(pinSetIdx, VCAMA, VOL_2800,
				mode_name) != TRUE) {
				pr_err("[CAMERA SENSOR] Fail to enable analog");
				pr_err(" power (VCAM_A), power id = %d\n",
					VCAMA);
				goto _kdCISModulePowerOn_exit_;
			}

			mdelay(1);

			/* enable active sensor */
			if (pinSet[pinSetIdx][IDX_PS_CMPDN] != GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMPDN,
				pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_OFF]);

			if (pinSet[pinSetIdx][IDX_PS_CMRST] != GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMRST,
				pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_ON]);
		} else if (pinSetIdx == 1 && currSensorName &&
			(strcmp(currSensorName,
			SENSOR_DRVNAME_GC0310_MIPI_YUV) == 0)) {
			/* First Power Pin low and Reset Pin Low */
			if (pinSet[pinSetIdx][IDX_PS_CMPDN]
					!= GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMPDN,
				pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_OFF]);

			/* VCAM_IO */
			if (_hwPowerOnCnt(pinSetIdx, VCAMIO, VOL_1800,
				mode_name) != TRUE) {
				pr_err("[CAMERA SENSOR] Fail to enable ");
				pr_err("digital power (VCAM_IO), ");
				pr_err("power id = %d\n", VCAMIO);
				goto _kdCISModulePowerOn_exit_;
			}

			mdelay(1);

			/* VCAM_A */
			if (_hwPowerOnCnt(pinSetIdx, VCAMA, VOL_2800,
				mode_name) != TRUE) {
				pr_err("[CAMERA SENSOR] Fail to enable ");
				pr_err("analog power (VCAM_A), ");
				pr_err("power id = %d\n", VCAMA);
				goto _kdCISModulePowerOn_exit_;
			}

			mdelay(1);

			/* enable active sensor */
			if (pinSet[pinSetIdx][IDX_PS_CMPDN] != GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMPDN,
				pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_OFF]);
			mdelay(1);
		} else {
			/* VCAM_IO */
			if (_hwPowerOnCnt(pinSetIdx, VCAMIO, VOL_1800,
				mode_name) != TRUE) {
				pr_err("[CAMERA SENSOR] Fail to enable ");
				pr_err("IO power (VCAM_IO), power id = %d\n",
					VCAMIO);
				goto _kdCISModulePowerOn_exit_;
			}
		}
	} else { /* power OFF */

		pr_debug("[PowerOFF]pinSetIdx:%d\n", pinSetIdx);
		/* depend on ISP module ready and open */
		ISP_MCLK1_EN(0);
		/* end */

		if (pinSetIdx == 0 && currSensorName && (
					strcmp(currSensorName,
						SENSOR_DRVNAME_GC2375MIPI_RAW_CHXT_REAR) == 0)) {

			PK_ERR("[PowerDown]pinSetIdx:%d, currSensorName:%s\n", pinSetIdx, currSensorName);
			/* Set Power Pin low and Reset Pin Low */
			if (pinSet[pinSetIdx][IDX_PS_CMPDN] != GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMPDN, 1);
			mdelay(5);
			if (pinSet[pinSetIdx][IDX_PS_CMRST] != GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMRST, 0);
			mdelay(10);

			if (_hwPowerDownCnt(pinSetIdx, VCAMA, mode_name) != TRUE) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF analog power (VCAM_A), power id = %d\n", VCAMA);
				goto _kdCISModulePowerOn_exit_;
			}
			mdelay(5);
			if (_hwPowerDownCnt(pinSetIdx, VCAMD, mode_name) != TRUE) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF digital power (VCAMD), power id = %d\n", VCAMD);
				goto _kdCISModulePowerOn_exit_;
			}
			mdelay(5);
			/* VCAM_IO */
			if (_hwPowerDownCnt(pinSetIdx, VCAMIO, mode_name) != TRUE) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF digital power (VCAM_IO), power id = %d\n", VCAMIO);
				goto _kdCISModulePowerOn_exit_;
			}
		} else if (pinSetIdx == 0 && currSensorName && (strcmp(currSensorName,
								SENSOR_DRVNAME_SP2509MIPI_RAW_BLX_REAR) == 0)) {

			PK_ERR("[PowerDown]pinSetIdx:%d, currSensorName: %s\n", pinSetIdx,currSensorName);
			/* Set Power Pin low and Reset Pin Low */
			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST])
				mtkcam_gpio_set(pinSetIdx, CAMRST, 0);
			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMPDN])
				mtkcam_gpio_set(pinSetIdx, CAMPDN, 1);

			if (TRUE != _hwPowerDownCnt(pinSetIdx, VCAMA, mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF analog power (VCAM_A), power id = %d\n", VCAMA);
				goto _kdCISModulePowerOn_exit_;
			}
/*			if (TRUE != _hwPowerDownCnt(pinSetIdx, VCAMD, mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF digital power (VCAMD), power id = %d \n", VCAMD);
				goto _kdCISModulePowerOn_exit_;
			}
			mdelay(5);
*/			/* VCAM_IO */
			if (TRUE != _hwPowerDownCnt(pinSetIdx, VCAMIO, mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF digital power (VCAM_IO), power id = %d \n", VCAMIO);
				goto _kdCISModulePowerOn_exit_;
			}
			mdelay(5);
		} else if (pinSetIdx == 1 && currSensorName &&
				(strcmp(currSensorName, SENSOR_DRVNAME_GC2375MIPI_RAW_BLX_FRONT) == 0)) {
			PK_ERR("[PowerDown]pinSetIdx:%d, currSensorName: %s\n", pinSetIdx, currSensorName);
			/* Set Power Pin low and Reset Pin Low */
			if (pinSet[pinSetIdx][IDX_PS_CMPDN] != GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMPDN, 1);
			mdelay(5);
			if (pinSet[pinSetIdx][IDX_PS_CMRST] != GPIO_CAMERA_INVALID)
				mtkcam_gpio_set(pinSetIdx, CAMRST, 0);
			mdelay(10);

			if (_hwPowerDownCnt(pinSetIdx, VCAMA, mode_name) != TRUE) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF analog power (VCAM_A), power id = %d\n", VCAMA);
				goto _kdCISModulePowerOn_exit_;
			}
			mdelay(5);
			if (_hwPowerDownCnt(pinSetIdx, VCAMD, mode_name) != TRUE) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF digital power (VCAMD), power id = %d\n", VCAMD);
				goto _kdCISModulePowerOn_exit_;
			}
			mdelay(5);
			/* VCAM_IO */
			if (_hwPowerDownCnt(pinSetIdx, VCAMIO, mode_name) != TRUE) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF digital power (VCAM_IO), power id = %d\n", VCAMIO);
				goto _kdCISModulePowerOn_exit_;
			}
		} else if (pinSetIdx == 1 && currSensorName && (strcmp(currSensorName,
								SENSOR_DRVNAME_SP2509MIPI_RAW_CHXT_FRONT) == 0)) {

			PK_ERR("[PowerDown]pinSetIdx:%d, currSensorName: %s\n", pinSetIdx,currSensorName);
			/* Set Power Pin low and Reset Pin Low */
			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMRST])
				mtkcam_gpio_set(pinSetIdx, CAMRST, 0);
			if (GPIO_CAMERA_INVALID != pinSet[pinSetIdx][IDX_PS_CMPDN])
				mtkcam_gpio_set(pinSetIdx, CAMPDN, 1);

			if (TRUE != _hwPowerDownCnt(pinSetIdx, VCAMA, mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF analog power (VCAM_A), power id = %d\n", VCAMA);
				goto _kdCISModulePowerOn_exit_;
			}
/*			if (TRUE != _hwPowerDownCnt(pinSetIdx, VCAMD, mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF digital power (VCAMD), power id = %d \n", VCAMD);
				goto _kdCISModulePowerOn_exit_;
			}
			mdelay(5);
*/			/* VCAM_IO */
			if (TRUE != _hwPowerDownCnt(pinSetIdx, VCAMIO, mode_name)) {
				PK_DBG("[CAMERA SENSOR] Fail to OFF digital power (VCAM_IO), power id = %d \n", VCAMIO);
				goto _kdCISModulePowerOn_exit_;
			}
			mdelay(5);
		} else if (pinSetIdx == 0 && currSensorName && (
			strcmp(currSensorName,
			SENSOR_DRVNAME_GC2145_MIPI_YUV) == 0)) {
			/* Set Power Pin low and Reset Pin Low */
			if (GPIO_CAMERA_INVALID !=
				pinSet[pinSetIdx][IDX_PS_CMPDN])
				mtkcam_gpio_set(pinSetIdx, CAMPDN,
				pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_ON]);
			/* Set Reset Pin Low */
			if (GPIO_CAMERA_INVALID !=
				pinSet[pinSetIdx][IDX_PS_CMRST])
				mtkcam_gpio_set(pinSetIdx, CAMRST,
				pinSet[pinSetIdx][IDX_PS_CMRST + IDX_PS_OFF]);
			if (_hwPowerDownCnt(pinSetIdx, VCAMA,
				mode_name) != TRUE) {
				pr_err("[CAMERA SENSOR] Fail to OFF analog ");
				pr_err("power (VCAM_A), power id = %d\n",
					VCAMA);
				goto _kdCISModulePowerOn_exit_;
			}

			/* VCAM_IO */
			if (_hwPowerDownCnt(pinSetIdx, VCAMIO,
				mode_name) != TRUE) {
				pr_err("[CAMERA SENSOR] Fail to OFF digital ");
				pr_err("power (VCAM_IO), power id = %d\n",
					VCAMIO);
				goto _kdCISModulePowerOn_exit_;
			}

		} else if (pinSetIdx == 1 && currSensorName && (
			strcmp(currSensorName,
			SENSOR_DRVNAME_GC0310_MIPI_YUV) == 0)) {
			/* Set Power Pin low and Reset Pin Low */
			if (GPIO_CAMERA_INVALID !=
				pinSet[pinSetIdx][IDX_PS_CMPDN])
				mtkcam_gpio_set(pinSetIdx, CAMPDN,
				pinSet[pinSetIdx][IDX_PS_CMPDN + IDX_PS_ON]);

			if (_hwPowerDownCnt(pinSetIdx, VCAMA,
				mode_name) != TRUE) {
				pr_err("[CAMERA SENSOR] Fail to OFF analog ");
				pr_err("power (VCAM_A), power id = %d\n",
					VCAMA);
				goto _kdCISModulePowerOn_exit_;
			}

			/* VCAM_IO */
			if (_hwPowerDownCnt(pinSetIdx, VCAMIO,
				mode_name) != TRUE) {
				pr_err("[CAMERA SENSOR] Fail to OFF digital ");
				pr_err("power (VCAM_IO), power id = %d\n",
					VCAMIO);
				goto _kdCISModulePowerOn_exit_;
			}
		} else {
			/* VCAM_IO */
			if (_hwPowerDownCnt(pinSetIdx, VCAMIO,
				mode_name) != TRUE) {
				pr_err("[CAMERA SENSOR] Fail to OFF digital ");
				pr_err("power (VCAM_IO), power id = %d\n",
					VCAMIO);
				/* return -EIO; */
				goto _kdCISModulePowerOn_exit_;
			}
		}
	}

	return 0;

_kdCISModulePowerOn_exit_:
	return -EIO;

}
EXPORT_SYMBOL(kdCISModulePowerOn);

