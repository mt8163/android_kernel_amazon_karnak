/*****************************************************************************
 *
 * Filename:
 * ---------
 *   sp2518yuv_Sensor.h
 *
 * Project:
 * --------
 *   MAUI
 *
 * Description:
 * ------------
 *   Image sensor driver declare and macro define in the header file.
 *
 * Author:
 * -------
 *   Mormo
 *
 *=============================================================
 *             HISTORY
 * Below this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Log$
 * 2011/11/03 Firsty Released By Mormo;
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *=============================================================
 ******************************************************************************/
 
#ifndef __SP2518YUV_SENSOR_H
#define __SP2518YUV_SENSOR_H


#define UXGA_PERIOD_PIXEL_NUMS						(1600)
#define UXGA_PERIOD_LINE_NUMS						(1200)

#define SVGA_PERIOD_PIXEL_NUMS						(800)
#define SVGA_PERIOD_LINE_NUMS						(600)

#define IMAGE_SENSOR_SVGA_GRAB_PIXELS		0
#define IMAGE_SENSOR_SVGA_GRAB_LINES		1

#define IMAGE_SENSOR_UXGA_GRAB_PIXELS		0
#define IMAGE_SENSOR_UXGA_GRAB_LINES		1

#define IMAGE_SENSOR_VGA_WIDTH					(640)
#define IMAGE_SENSOR_VGA_HEIGHT				    (480)

#define IMAGE_SENSOR_SVGA_WIDTH					(800)
#define IMAGE_SENSOR_SVGA_HEIGHT				(600)

#define IMAGE_SENSOR_720P_WIDTH					(1280)
#define IMAGE_SENSOR_720P_HEIGHT				(720)

#define IMAGE_SENSOR_UXGA_WIDTH					(1600)
#define IMAGE_SENSOR_UXGA_HEIGHT				(1200)

#define IMAGE_SENSOR_PV_WIDTH					(IMAGE_SENSOR_SVGA_WIDTH - 8)
#define IMAGE_SENSOR_PV_HEIGHT					(IMAGE_SENSOR_SVGA_HEIGHT - 6)

#define IMAGE_SENSOR_FULL_WIDTH					(IMAGE_SENSOR_UXGA_WIDTH - 16)
#define IMAGE_SENSOR_FULL_HEIGHT				(IMAGE_SENSOR_UXGA_HEIGHT - 12)

#define SP2518_WRITE_ID							0x60
#define SP2518_READ_ID							0x61



typedef enum
{
	Gamma_1_8 = 1,
	Gamma_2_0,
	Gamma_2_2,
	Gamma_dm_3,
	Gamma_dm_5,
	Gamma_dm_6,
	Gamma_dm_7,
	Gamma_dm_8,
	Gamma_dm_9
}SP2518_GAMMA_TAG;

UINT32 SP2518Open(void);
UINT32 SP2518Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 SP2518FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId, UINT8 *pFeaturePara,UINT32 *pFeatureParaLen);
UINT32 SP2518GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_INFO_STRUCT *pSensorInfo, MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData);
UINT32 SP2518GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution);
UINT32 SP2518Close(void);

extern void kdSetI2CSpeed(u32 i2cSpeed);


#endif /* __SENSOR_H */

