/*****************************************************************************
 *
 * Filename:
 * ---------
 *     OV9734mipi_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "ov9734mipiraw_Sensor.h"

/****************************Modify Following Strings for Debug****************************/
#define PFX "OV9734_camera_sensor"
#define LOG_1 LOG_INF("OV9734,MIPI 1LANE\n")
#define LOG_2 LOG_INF("preview/capure/vedio 1280*720 30fps 360Mbps/lane \n")
/****************************   Modify end    *******************************************/

#define LOG_INF(format, args...)        pr_debug(PFX "[%s] " format, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static imgsensor_info_struct imgsensor_info = {
        .sensor_id = OV9734MIPI_SENSOR_ID,          /* record sensor id defined in Kd_imgsensor.h */

        .checksum_value = 0x8790037C,               /* checksum value for Camera Auto Test */

        .pre = {
                .pclk = 36000000,                   /* record different mode's pclk */
                .linelength = 1478,                 /* 0x5c6  record different mode's linelength */
                .framelength = 802,                 /* 0x322  record different mode's framelength */
                .startx = 0,                        /* record different mode's startx of grabwindow */
                .starty = 0,                        /* record different mode's starty of grabwindow */
                .grabwindow_width = 1280,           /* record different mode's width of gabwindow */
                .grabwindow_height = 720,           /* record different mode's height of grabwindow */
                /* following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario */
                .mipi_data_lp2hs_settle_dc = 85,    /* unit , ns */
                /* following for GetDefaultFramerateByScenario() */
                .max_framerate = 300,
        },
        .cap = {
                .pclk = 36000000,                   /* record different mode's pclk */
                .linelength = 1478,                 /* 0x5c6  record different mode's linelength */
                .framelength = 802,                 /* 0x322 record different mode's framelength */
                .startx = 0,                        /* record different mode's startx of grabwindow */
                .starty = 0,                        /* record different mode's starty of grabwindow */
                .grabwindow_width = 1280,           /* record different mode's width of grabwindow */
                .grabwindow_height = 720,           /* record different mode's height of grabwindow */
                /* following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario */
                .mipi_data_lp2hs_settle_dc = 85,    /* unit , ns */
                /* following for GetDefaultFramerateByScenario() */
                .max_framerate = 300,
        },
        .cap1 = {                                   /* capture for PIP 24fps relative information, capture1 mode must use same framelength, linelength with Capture mode for shutter calculate */
                .pclk = 36000000,                   /* record different mode's pclk */
                .linelength = 1478,                 /* 0x5c6  record different mode's linelength */
                .framelength = 802,                 /* 0x322  record different mode's framelength */
                .startx = 0,                        /* record different mode's startx of grabwindow */
                .starty = 0,                        /* record different mode's starty of grabwindow */
                .grabwindow_width = 1280,           /* record different mode's width of grabwindow */
                .grabwindow_height = 720,           /* record different mode's height of grabwindow */
                /* following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario */
                .mipi_data_lp2hs_settle_dc = 85,    /* unit , ns */
                /* following for GetDefaultFramerateByScenario() */
                .max_framerate = 300,
        },
        .normal_video = {
                .pclk = 36000000,                   /* record different mode's pclk */
                .linelength = 1478,                 /* 0x5c6 record different mode's linelength */
                .framelength = 802,                 /* 0x322 record different mode's framelength */
                .startx = 0,                        /* record different mode's startx of grabwindow */
                .starty = 0,                        /* record different mode's starty of grabwindow */
                .grabwindow_width = 1280,           /* record different mode's width of grabwindow */
                .grabwindow_height = 720,           /* record different mode's height of grabwindow */
                /* following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario */
                .mipi_data_lp2hs_settle_dc = 85,    /* unit , ns */
                /* following for GetDefaultFramerateByScenario() */
                .max_framerate = 300,
        },
        .hs_video = {
                .pclk = 36000000,                   /* record different mode's pclk */
                .linelength = 1478,                 /* 0x5c6 record different mode's linelength */
                .framelength = 802,                 /* 0x322 record different mode's framelength */
                .startx = 0,                        /* record different mode's startx of grabwindow */
                .starty = 0,                        /* record different mode's starty of grabwindow */
                .grabwindow_width = 1280,           /* record different mode's width of grabwindow */
                .grabwindow_height = 720,           /* record different mode's height of grabwindow */
                /* following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario */
                .mipi_data_lp2hs_settle_dc = 85,    /* unit , ns */
                /* following for GetDefaultFramerateByScenario() */
                .max_framerate = 300,
        },
        .slim_video = {
                .pclk = 36000000,                   /* record different mode's pclk */
                .linelength = 1478,                 /* 0x5c6 record different mode's linelength */
                .framelength = 802,                 /* 0x322 record different mode's framelength */
                .startx = 0,                        /* record different mode's startx of grabwindow */
                .starty = 0,                        /* record different mode's starty of grabwindow */
                .grabwindow_width = 1280,           /* record different mode's width of grabwindow */
                .grabwindow_height = 720,           /* record different mode's height of grabwindow */
                /*  following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario */
                .mipi_data_lp2hs_settle_dc = 85,    /* unit , ns */
                /*  following for GetDefaultFramerateByScenario() */
                .max_framerate = 300,
        },
        .margin = 4,                                               /* sensor framelength & shutter margin check */
        .min_shutter = 1,                                          /* min shutter */
        .max_frame_length = 0x7fff,                                /* max framelength by sensor register's limitation */
        .ae_shut_delay_frame = 0,                                  /* shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2 */
        .ae_sensor_gain_delay_frame = 0,                           /* sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2 */
        .ae_ispGain_delay_frame = 2,                               /* isp gain delay frame for AE cycle */
        .ihdr_support = 0,                                         /* 1, support; 0,not support */
        .ihdr_le_firstline = 0,                                    /* 1,le first ; 0, se first */
        .sensor_mode_num = 5,                                      /* support sensor mode num */

        .cap_delay_frame = 3,                                      /* enter capture delay frame num */
        .pre_delay_frame = 3,                                      /* enter preview delay frame num */
        .video_delay_frame = 3,                                    /* enter video delay frame num */
        .hs_video_delay_frame = 3,                                 /* enter high speed video  delay frame num */
        .slim_video_delay_frame = 3,                               /* enter slim video delay frame num */

        .isp_driving_current = ISP_DRIVING_6MA,                    /* mclk driving current */
        .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,       /* sensor_interface_type */
        .mipi_sensor_type = MIPI_OPHY_NCSI2,                       /* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
        .mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,           /* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
        .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,    /* sensor output first pixel color */
        .mclk = 24,                                                /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
        .mipi_lane_num = SENSOR_MIPI_1_LANE,                       /* mipi lane num */
        .i2c_addr_table = {0x6c, 0x20, 0xff},                      /* record sensor support all write id addr, only supprt 4must end with 0xff */
};


static imgsensor_struct imgsensor = {
        .mirror = IMAGE_NORMAL,                                    /* mirrorflip information */
        .sensor_mode = IMGSENSOR_MODE_INIT,                        /* IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video */
        .shutter = 0x3D0,                                          /* current shutter */
        .gain = 0x100,                                             /* current gain */
        .dummy_pixel = 0,                                          /* current dummypixel */
        .dummy_line = 0,                                           /* current dummyline */
        .current_fps = 300,                                        /* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
        .autoflicker_en = KAL_FALSE,                               /* auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker */
        .test_pattern = KAL_FALSE,                                 /* test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output */
        .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,    /* current scenario id */
        .ihdr_en = 0,                                              /* sensor need support LE, SE with HDR feature */
        .i2c_write_id = 0x6c,                                      /* record current sensor's i2c write id */
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] =
{
        {1280, 720, 0, 0, 1280, 720, 1280, 720, 0000, 0000, 1280, 720, 0, 0, 1280, 720},        /* Preview */
        {1280, 720, 0, 0, 1280, 720, 1280, 720, 0000, 0000, 1280, 720, 0, 0, 1280, 720},        /* capture */
        {1280, 720, 0, 0, 1280, 720, 1280, 720, 0000, 0000, 1280, 720, 0, 0, 1280, 720},        /* video */
        {1280, 720, 0, 0, 1280, 720, 1280, 720, 0000, 0000, 1280, 720, 0, 0, 1280, 720},        /* hight speed video */
        {1280, 720, 0, 0, 1280, 720, 1280, 720, 0000, 0000, 1280, 720, 0, 0, 1280, 720}};        /* slim video */



static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
        kal_uint16 get_byte=0;

        char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
        iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id);

        return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
        char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
        iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
        LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
        /* you can set dummy by imgsensor.dummy_line and imgsensor.dummy_pixel, or you can set dummy by imgsensor.frame_length and imgsensor.line_length */
    	write_cmos_sensor(0x380c, imgsensor.line_length >> 8);
        write_cmos_sensor(0x380d, imgsensor.line_length & 0xFF);
        write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
        write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
}        /* set_dummy */

static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
        kal_uint32 frame_length = imgsensor.frame_length;
        /* unsigned long flags; */

        LOG_INF("framerate = %d, min framelength should enable? %d\n", framerate,
           		min_framelength_en);

        frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.frame_length =
    	(frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;
        imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
        if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
                imgsensor.frame_length = imgsensor_info.max_frame_length;
                imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
        }
        if (min_framelength_en)
               imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        set_dummy();
}        /* set_max_framerate */



/*************************************************************************
* FUNCTION
*    set_shutter
*
* DESCRIPTION
*    This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*    iShutter : exposured lines
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
        unsigned long flags;
        kal_uint16 realtime_fps = 0;
        spin_lock_irqsave(&imgsensor_drv_lock, flags);
        imgsensor.shutter = shutter;
        spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
#if 1
        /* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
        /* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */
        /* OV Recommend Solution */
        /* if shutter bigger than frame_length, should extend frame length first */
        spin_lock(&imgsensor_drv_lock);
        if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
             imgsensor.frame_length = shutter + imgsensor_info.margin;
        else
                imgsensor.frame_length = imgsensor.min_frame_length;
        if (imgsensor.frame_length > imgsensor_info.max_frame_length)
                imgsensor.frame_length = imgsensor_info.max_frame_length;
        spin_unlock(&imgsensor_drv_lock);
        shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
        shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

        if (imgsensor.autoflicker_en) {
                realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
                if(realtime_fps >= 297 && realtime_fps <= 305)
                        set_max_framerate(296,0);
                else if(realtime_fps >= 147 && realtime_fps <= 150)
                        set_max_framerate(146,0);
                else {
                        /* Extend frame length */
                        write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
                        write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
                }
        } else {
                write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
                write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
        }

        /* Update Shutter */
        write_cmos_sensor(0x3500, (shutter >> 12) & 0x0F);
        write_cmos_sensor(0x3501, (shutter >> 4) & 0xFF);
        write_cmos_sensor(0x3502, (shutter<<4)  & 0xF0);

#endif
LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);

}        /*  set_shutter */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
        kal_uint16 iReg = 0x0000;

    	iReg = gain*16/BASEGAIN;

    	if(iReg < 0x10)        /*  sensor 1xGain */
    	{
    		  iReg = 0x10;
    	}
    	if(iReg > 0xf8)        /*  sensor 15.5xGain */
    	{
    		  iReg = 0xf8;
    	}

    	return iReg;
}

/*************************************************************************
* FUNCTION
*    set_gain
*
* DESCRIPTION
*    This function is to set global gain to sensor.
*
* PARAMETERS
*    iGain : sensor global gain(base: 0x40)
*
* RETURNS
*    the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
        kal_uint16 reg_gain;
        reg_gain = gain2reg(gain);
        spin_lock(&imgsensor_drv_lock);
        imgsensor.gain = reg_gain;
        spin_unlock(&imgsensor_drv_lock);
        LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

        write_cmos_sensor(0x350b,reg_gain);
        return gain;
}        /* set_gain */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
        LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n",le,se,gain);
        if (imgsensor.ihdr_en) {

                spin_lock(&imgsensor_drv_lock);
                if (le > imgsensor.min_frame_length - imgsensor_info.margin)
                    imgsensor.frame_length = le + imgsensor_info.margin;
                else
                    imgsensor.frame_length = imgsensor.min_frame_length;
                if (imgsensor.frame_length > imgsensor_info.max_frame_length)
                    imgsensor.frame_length = imgsensor_info.max_frame_length;
                spin_unlock(&imgsensor_drv_lock);
                if (le < imgsensor_info.min_shutter) le = imgsensor_info.min_shutter;
                if (se < imgsensor_info.min_shutter) se = imgsensor_info.min_shutter;

                set_gain(gain);
        }

}

/*************************************************************************
* FUNCTION
*    night_mode
*
* DESCRIPTION
*    This function night mode of sensor.
*
* PARAMETERS
*    bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void night_mode(kal_bool enable)
{
/* No Need to implement this function */
}        /* night_mode */

static void sensor_init(void)
{
        LOG_INF("E\n");

    	write_cmos_sensor(0x0103,0x01);
    	write_cmos_sensor(0x0100,0x00);
    	write_cmos_sensor(0x3001,0x00);
    	write_cmos_sensor(0x3002,0x00);
    	write_cmos_sensor(0x3007,0x00);
    	write_cmos_sensor(0x3010,0x00);
    	write_cmos_sensor(0x3011,0x08);
    	write_cmos_sensor(0x3014,0x22);
    	write_cmos_sensor(0x301e,0x15);
    	write_cmos_sensor(0x3030,0x19);
    	write_cmos_sensor(0x3080,0x02);
    	write_cmos_sensor(0x3081,0x3c);
    	write_cmos_sensor(0x3082,0x04);
    	write_cmos_sensor(0x3083,0x00);
    	write_cmos_sensor(0x3084,0x02);
    	write_cmos_sensor(0x3085,0x01);
    	write_cmos_sensor(0x3086,0x01);
    	write_cmos_sensor(0x3089,0x01);
    	write_cmos_sensor(0x308a,0x00);
    	write_cmos_sensor(0x3103,0x01);
    	write_cmos_sensor(0x3600,0x55);
    	write_cmos_sensor(0x3601,0x02);
    	write_cmos_sensor(0x3605,0x22);
    	write_cmos_sensor(0x3611,0xe7);
    	write_cmos_sensor(0x3654,0x10);
    	write_cmos_sensor(0x3655,0x77);
    	write_cmos_sensor(0x3656,0x77);
    	write_cmos_sensor(0x3657,0x07);
    	write_cmos_sensor(0x3658,0x22);
    	write_cmos_sensor(0x3659,0x22);
    	write_cmos_sensor(0x365a,0x02);
    	write_cmos_sensor(0x3784,0x05);
    	write_cmos_sensor(0x3785,0x55);
    	write_cmos_sensor(0x37c0,0x07);
    	write_cmos_sensor(0x3800,0x00);
    	write_cmos_sensor(0x3801,0x04);
    	write_cmos_sensor(0x3802,0x00);
    	write_cmos_sensor(0x3803,0x04);
    	write_cmos_sensor(0x3804,0x05);
    	write_cmos_sensor(0x3805,0x0b);
    	write_cmos_sensor(0x3806,0x02);
    	write_cmos_sensor(0x3807,0xdb);
    	write_cmos_sensor(0x3808,0x05);
    	write_cmos_sensor(0x3809,0x00);
    	write_cmos_sensor(0x380a,0x02);
    	write_cmos_sensor(0x380b,0xd0);
    	write_cmos_sensor(0x380c,0x05);
    	write_cmos_sensor(0x380d,0xc6);
    	write_cmos_sensor(0x380e,0x03);
    	write_cmos_sensor(0x380f,0x22);
    	write_cmos_sensor(0x3810,0x00);
    	write_cmos_sensor(0x3811,0x04);
    	write_cmos_sensor(0x3812,0x00);
    	write_cmos_sensor(0x3813,0x04);
    	write_cmos_sensor(0x3816,0x00);
    	write_cmos_sensor(0x3817,0x00);
    	write_cmos_sensor(0x3818,0x00);
    	write_cmos_sensor(0x3819,0x04);
    	write_cmos_sensor(0x3820,MIRROR);
    	write_cmos_sensor(0x3821,0x00);
    	write_cmos_sensor(0x382c,0x06);
    	write_cmos_sensor(0x3500,0x00);
    	write_cmos_sensor(0x3501,0x31);
    	write_cmos_sensor(0x3502,0x00);
    	write_cmos_sensor(0x3503,0x03);
    	write_cmos_sensor(0x3504,0x00);
    	write_cmos_sensor(0x3505,0x00);
    	write_cmos_sensor(0x3509,0x10);
    	write_cmos_sensor(0x350a,0x00);
    	write_cmos_sensor(0x350b,0x40);
    	write_cmos_sensor(0x3d00,0x00);
    	write_cmos_sensor(0x3d01,0x00);
    	write_cmos_sensor(0x3d02,0x00);
    	write_cmos_sensor(0x3d03,0x00);
    	write_cmos_sensor(0x3d04,0x00);
    	write_cmos_sensor(0x3d05,0x00);
    	write_cmos_sensor(0x3d06,0x00);
    	write_cmos_sensor(0x3d07,0x00);
    	write_cmos_sensor(0x3d08,0x00);
    	write_cmos_sensor(0x3d09,0x00);
    	write_cmos_sensor(0x3d0a,0x00);
    	write_cmos_sensor(0x3d0b,0x00);
    	write_cmos_sensor(0x3d0c,0x00);
    	write_cmos_sensor(0x3d0d,0x00);
    	write_cmos_sensor(0x3d0e,0x00);
    	write_cmos_sensor(0x3d0f,0x00);
    	write_cmos_sensor(0x3d80,0x00);
    	write_cmos_sensor(0x3d81,0x00);
    	write_cmos_sensor(0x3d82,0x38);
    	write_cmos_sensor(0x3d83,0xa4);
    	write_cmos_sensor(0x3d84,0x00);
    	write_cmos_sensor(0x3d85,0x00);
    	write_cmos_sensor(0x3d86,0x1f);
    	write_cmos_sensor(0x3d87,0x03);
    	write_cmos_sensor(0x3d8b,0x00);
    	write_cmos_sensor(0x3d8f,0x00);
    	write_cmos_sensor(0x4001,0xe0);
    	write_cmos_sensor(0x4009,0x0b);
    	write_cmos_sensor(0x4300,0x03);
    	write_cmos_sensor(0x4301,0xff);
    	write_cmos_sensor(0x4304,0x00);
    	write_cmos_sensor(0x4305,0x00);
    	write_cmos_sensor(0x4309,0x00);
    	write_cmos_sensor(0x4600,0x00);
    	write_cmos_sensor(0x4601,0x80);
    	write_cmos_sensor(0x4800,0x00);
    	write_cmos_sensor(0x4805,0x00);
    	write_cmos_sensor(0x4821,0x50);
    	write_cmos_sensor(0x4823,0x50);
    	write_cmos_sensor(0x4837,0x2d);
    	write_cmos_sensor(0x4a00,0x00);
    	write_cmos_sensor(0x4f00,0x80);
    	write_cmos_sensor(0x4f01,0x10);
    	write_cmos_sensor(0x4f02,0x00);
    	write_cmos_sensor(0x4f03,0x00);
    	write_cmos_sensor(0x4f04,0x00);
    	write_cmos_sensor(0x4f05,0x00);
    	write_cmos_sensor(0x4f06,0x00);
    	write_cmos_sensor(0x4f07,0x00);
    	write_cmos_sensor(0x4f08,0x00);
    	write_cmos_sensor(0x4f09,0x00);
    	write_cmos_sensor(0x5000,0x2f);
    	write_cmos_sensor(0x500c,0x00);
    	write_cmos_sensor(0x500d,0x00);
    	write_cmos_sensor(0x500e,0x00);
    	write_cmos_sensor(0x500f,0x00);
    	write_cmos_sensor(0x5010,0x00);
    	write_cmos_sensor(0x5011,0x00);
    	write_cmos_sensor(0x5012,0x00);
    	write_cmos_sensor(0x5013,0x00);
    	write_cmos_sensor(0x5014,0x00);
    	write_cmos_sensor(0x5015,0x00);
    	write_cmos_sensor(0x5016,0x00);
    	write_cmos_sensor(0x5017,0x00);
    	write_cmos_sensor(0x5080,0x00);
    	write_cmos_sensor(0x5180,0x01);
    	write_cmos_sensor(0x5181,0x00);
    	write_cmos_sensor(0x5182,0x01);
    	write_cmos_sensor(0x5183,0x00);
    	write_cmos_sensor(0x5184,0x01);
    	write_cmos_sensor(0x5185,0x00);
    	write_cmos_sensor(0x5708,0x06);
    	write_cmos_sensor(0x380f,0x2a);
    	write_cmos_sensor(0x5780,0x3e);
    	write_cmos_sensor(0x5781,0x0f);
    	write_cmos_sensor(0x5782,0x44);
    	write_cmos_sensor(0x5783,0x02);
    	write_cmos_sensor(0x5784,0x01);
    	write_cmos_sensor(0x5785,0x01);
    	write_cmos_sensor(0x5786,0x00);
    	write_cmos_sensor(0x5787,0x04);
    	write_cmos_sensor(0x5788,0x02);
    	write_cmos_sensor(0x5789,0x0f);
    	write_cmos_sensor(0x578a,0xfd);
    	write_cmos_sensor(0x578b,0xf5);
    	write_cmos_sensor(0x578c,0xf5);
    	write_cmos_sensor(0x578d,0x03);
    	write_cmos_sensor(0x578e,0x08);
    	write_cmos_sensor(0x578f,0x0c);
    	write_cmos_sensor(0x5790,0x08);
    	write_cmos_sensor(0x5791,0x04);
    	write_cmos_sensor(0x5792,0x00);
    	write_cmos_sensor(0x5793,0x52);
    	write_cmos_sensor(0x5794,0xa3);
    	write_cmos_sensor(0x5000,0x3f);
}        /* sensor_init */


static void preview_setting(void)
{
    	LOG_INF("E!\n");
    	write_cmos_sensor(0x0100,0x01);        /*Stream on */
}

static void capture_setting(kal_uint16 currefps)
{
        LOG_INF("E! currefps:%d\n",currefps);
    	write_cmos_sensor(0x0100,0x01);        /*Stream on */
}

static void normal_video_setting(kal_uint16 currefps)
{
        LOG_INF("E! currefps:%d\n",currefps);
    	write_cmos_sensor(0x0100,0x01);        /*Stream on */
}
static void hs_video_setting(void)
{
        LOG_INF("E\n");
    	write_cmos_sensor(0x0100,0x01);        /*Stream on */
}

static void slim_video_setting(void)
{
        LOG_INF("E\n");
    	write_cmos_sensor(0x0100,0x01);        /*Stream on */
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
        LOG_INF("enable: %d\n", enable);

        if (enable) {
                write_cmos_sensor(0x5080, 0x10);        /* open standard color bar */
        } else {
                write_cmos_sensor(0x5080, 0x00);
        }
        spin_lock(&imgsensor_drv_lock);
        imgsensor.test_pattern = enable;
        spin_unlock(&imgsensor_drv_lock);
        return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*    get_imgsensor_id
*
* DESCRIPTION
*    This function get the sensor ID
*
* PARAMETERS
*    *sensorID : return the sensor ID
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
        *sensor_id = OV9734MIPI_SENSOR_ID;
        return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*    open
*
* DESCRIPTION
*    This function initialize the registers of CMOS sensor
*
* PARAMETERS
*    None
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
        LOG_1;
        LOG_2;

        /* initial sequence write in */
        sensor_init();

        spin_lock(&imgsensor_drv_lock);

        imgsensor.autoflicker_en= KAL_FALSE;
        imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
        imgsensor.pclk = imgsensor_info.pre.pclk;
        imgsensor.frame_length = imgsensor_info.pre.framelength;
        imgsensor.line_length = imgsensor_info.pre.linelength;
        imgsensor.min_frame_length = imgsensor_info.pre.framelength;
        imgsensor.dummy_pixel = 0;
        imgsensor.dummy_line = 0;
        imgsensor.ihdr_en = 0;
        imgsensor.test_pattern = KAL_FALSE;
        imgsensor.current_fps = imgsensor_info.pre.max_framerate;
        spin_unlock(&imgsensor_drv_lock);

        return ERROR_NONE;
}        /*    open  */



/*************************************************************************
* FUNCTION
*    close
*
* DESCRIPTION
*    This dummy function is corresponding to open() function
*    This function doesn't need implementing, because it is only read/write register in open() function
* PARAMETERS
*    None
*
* RETURNS
*    ERROR_NONE
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
        LOG_INF("E\n");

        /* No Need to implement this function*/

        return ERROR_NONE;
}        /* close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*    This function start the sensor preview.
*
* PARAMETERS
*    *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
        LOG_INF("E\n");

        spin_lock(&imgsensor_drv_lock);
        imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
        imgsensor.pclk = imgsensor_info.pre.pclk;
        imgsensor.line_length = imgsensor_info.pre.linelength;
        imgsensor.frame_length = imgsensor_info.pre.framelength;
        imgsensor.min_frame_length = imgsensor_info.pre.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
        spin_unlock(&imgsensor_drv_lock);
        preview_setting();
        return ERROR_NONE;
}        /* preview  */

/*************************************************************************
* FUNCTION
*    capture
*
* DESCRIPTION
*    This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                          MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
        LOG_INF("E\n");

        spin_lock(&imgsensor_drv_lock);
        imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
        if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {        /* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
                imgsensor.pclk = imgsensor_info.cap1.pclk;
                imgsensor.line_length = imgsensor_info.cap1.linelength;
                imgsensor.frame_length = imgsensor_info.cap1.framelength;
                imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
                imgsensor.autoflicker_en = KAL_FALSE;
        } else {
                if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
                        LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",imgsensor.current_fps,imgsensor_info.cap.max_framerate/10);
                imgsensor.pclk = imgsensor_info.cap.pclk;
                imgsensor.line_length = imgsensor_info.cap.linelength;
                imgsensor.frame_length = imgsensor_info.cap.framelength;
                imgsensor.min_frame_length = imgsensor_info.cap.framelength;
                imgsensor.autoflicker_en = KAL_FALSE;
        }
        spin_unlock(&imgsensor_drv_lock);
        capture_setting(imgsensor.current_fps);
        return ERROR_NONE;
}        /* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
        LOG_INF("E\n");

        spin_lock(&imgsensor_drv_lock);
        imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
        imgsensor.pclk = imgsensor_info.normal_video.pclk;
        imgsensor.line_length = imgsensor_info.normal_video.linelength;
        imgsensor.frame_length = imgsensor_info.normal_video.framelength;
        imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
        spin_unlock(&imgsensor_drv_lock);
        normal_video_setting(imgsensor.current_fps);
        return ERROR_NONE;
}        /*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
        LOG_INF("E\n");

        spin_lock(&imgsensor_drv_lock);
        imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
        imgsensor.pclk = imgsensor_info.hs_video.pclk;
        imgsensor.line_length = imgsensor_info.hs_video.linelength;
        imgsensor.frame_length = imgsensor_info.hs_video.framelength;
        imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
        imgsensor.dummy_line = 0;
        imgsensor.dummy_pixel = 0;
        imgsensor.autoflicker_en = KAL_FALSE;
        spin_unlock(&imgsensor_drv_lock);
        hs_video_setting();
        return ERROR_NONE;
}        /*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
        LOG_INF("E\n");

        spin_lock(&imgsensor_drv_lock);
        imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
        imgsensor.pclk = imgsensor_info.slim_video.pclk;
        imgsensor.line_length = imgsensor_info.slim_video.linelength;
        imgsensor.frame_length = imgsensor_info.slim_video.framelength;
        imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
        imgsensor.dummy_line = 0;
        imgsensor.dummy_pixel = 0;
        imgsensor.autoflicker_en = KAL_FALSE;
        spin_unlock(&imgsensor_drv_lock);
        slim_video_setting();
        return ERROR_NONE;
}        /* slim_video */

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
        LOG_INF("E\n");
        sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
        sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

        sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
        sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

        sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
        sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;

        sensor_resolution->SensorHighSpeedVideoWidth     = imgsensor_info.hs_video.grabwindow_width;
        sensor_resolution->SensorHighSpeedVideoHeight     = imgsensor_info.hs_video.grabwindow_height;

        sensor_resolution->SensorSlimVideoWidth     = imgsensor_info.slim_video.grabwindow_width;
        sensor_resolution->SensorSlimVideoHeight     = imgsensor_info.slim_video.grabwindow_height;
        return ERROR_NONE;
}        /* get_resolution */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                      MSDK_SENSOR_INFO_STRUCT *sensor_info,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
        LOG_INF("scenario_id = %d\n", scenario_id);
        sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
        sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;    /* not use */
        sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;           /* inverse with datasheet */
        sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
        sensor_info->SensorInterruptDelayLines = 4;                             /* not use */
        sensor_info->SensorResetActiveHigh = FALSE;                             /* not use */
        sensor_info->SensorResetDelayCount = 5;                                 /* not use */

        sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
        sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
        sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
        sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

        sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
        sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
        sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
        sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
        sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

        sensor_info->SensorMasterClockSwitch = 0;                                           /* not use */
        sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

        sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;                 /* The frame of setting shutter default 0 for TG int */
        sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;    /* The frame of setting sensor gain */
        sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
        sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
        sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
        sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

        sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
        sensor_info->SensorClockFreq = imgsensor_info.mclk;
        sensor_info->SensorClockDividCount = 3;      /* not use */
        sensor_info->SensorClockRisingCount = 0;
        sensor_info->SensorClockFallingCount = 2;    /* not use */
        sensor_info->SensorPixelClockCount = 3;      /* not use */
        sensor_info->SensorDataLatchCount = 2;       /* not use */

        sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
        sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
        sensor_info->SensorWidthSampling = 0;        /* 0 is default 1x */
        sensor_info->SensorHightSampling = 0;        /* 0 is default 1x */
        sensor_info->SensorPacketECCOrder = 1;

        switch (scenario_id) {
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                        sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
                        sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

                        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

                        break;
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                        sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
                        sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

                        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

                        break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

                        sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
                        sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

                        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

                        break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                        sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
                        sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

                        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

                        break;
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                        sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
                        sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

                        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

                        break;
                default:
                        sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
                        sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

                        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
                        break;
        }

        return ERROR_NONE;
}        /*    get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
        LOG_INF("scenario_id = %d\n", scenario_id);
        spin_lock(&imgsensor_drv_lock);
        imgsensor.current_scenario_id = scenario_id;
        spin_unlock(&imgsensor_drv_lock);
        switch (scenario_id) {
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                        preview(image_window, sensor_config_data);
                        break;
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                        capture(image_window, sensor_config_data);
                        break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                        normal_video(image_window, sensor_config_data);
                        break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                        hs_video(image_window, sensor_config_data);
                        break;
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                        slim_video(image_window, sensor_config_data);
                        break;
                default:
                        LOG_INF("Error ScenarioId setting");
                        preview(image_window, sensor_config_data);
                        return ERROR_INVALID_SCENARIO_ID;
        }
        return ERROR_NONE;
}        /* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{        /* This Function not used after ROME */
    	LOG_INF("framerate = %d\n ", framerate);
        /*  SetVideoMode Function should fix framerate */
        if (framerate == 0)
                /*  Dynamic frame rate */
                return ERROR_NONE;
        spin_lock(&imgsensor_drv_lock);
        if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
                imgsensor.current_fps = 296;
        else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
                imgsensor.current_fps = 146;
        else
                imgsensor.current_fps = framerate;
        spin_unlock(&imgsensor_drv_lock);
        set_max_framerate(imgsensor.current_fps,1);

        return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
        LOG_INF("enable = %d, framerate = %d \n", enable, framerate);
        spin_lock(&imgsensor_drv_lock);
        if (enable)        /* enable auto flicker */
                imgsensor.autoflicker_en = KAL_TRUE;
        else              /* Cancel Auto flick */
                imgsensor.autoflicker_en = KAL_FALSE;
        spin_unlock(&imgsensor_drv_lock);
        return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
        kal_uint32 frame_length;

        LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

        switch (scenario_id) {
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                        frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
                        spin_lock(&imgsensor_drv_lock);
                        imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
                        imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
                        imgsensor.min_frame_length = imgsensor.frame_length;
                        spin_unlock(&imgsensor_drv_lock);
                        break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                        if(framerate == 0)
                                return ERROR_NONE;
                        frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
                        spin_lock(&imgsensor_drv_lock);
                        imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
                        imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
                        imgsensor.min_frame_length = imgsensor.frame_length;
                        spin_unlock(&imgsensor_drv_lock);
                        break;
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                        if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
                                frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
                                spin_lock(&imgsensor_drv_lock);
            		            imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ? (frame_length - imgsensor_info.cap1.framelength) : 0;
            		            imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
            		            imgsensor.min_frame_length = imgsensor.frame_length;
            		            spin_unlock(&imgsensor_drv_lock);
                        } else {
                                if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
                                        LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",framerate,imgsensor_info.cap.max_framerate/10);
                                frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
                                spin_lock(&imgsensor_drv_lock);
            		            imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
            		            imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
            		            imgsensor.min_frame_length = imgsensor.frame_length;
            		            spin_unlock(&imgsensor_drv_lock);
                        }
                        break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                        frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
                        spin_lock(&imgsensor_drv_lock);
                        imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
                        imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
                        imgsensor.min_frame_length = imgsensor.frame_length;
                        spin_unlock(&imgsensor_drv_lock);
                        break;
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                        frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
                        spin_lock(&imgsensor_drv_lock);
                        imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
                        imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
                        imgsensor.min_frame_length = imgsensor.frame_length;
                        spin_unlock(&imgsensor_drv_lock);
                        break;
                default:        /* coding with  preview scenario by default */
                        frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
                        spin_lock(&imgsensor_drv_lock);
                        imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
                        imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
                        imgsensor.min_frame_length = imgsensor.frame_length;
                        spin_unlock(&imgsensor_drv_lock);
                        LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
                        break;
        }
        return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
        LOG_INF("scenario_id = %d\n", scenario_id);

        if (framerate == NULL)
        {
                LOG_INF("---- framerate is NULL ---- check here \n");
                return ERROR_NONE;
        }

        switch (scenario_id) {
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                        *framerate = imgsensor_info.pre.max_framerate;
                        break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                        *framerate = imgsensor_info.normal_video.max_framerate;
                        break;
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                        *framerate = imgsensor_info.cap.max_framerate;
                        break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                        *framerate = imgsensor_info.hs_video.max_framerate;
                        break;
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                        *framerate = imgsensor_info.slim_video.max_framerate;
                        break;
                default:
                        break;
        }
        return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
                             UINT8 *feature_para,UINT32 *feature_para_len)
{
        UINT16 *feature_return_para_16=(UINT16 *) feature_para;
        UINT16 *feature_data_16=(UINT16 *) feature_para;
        UINT32 *feature_return_para_32=(UINT32 *) feature_para;
        UINT32 *feature_data_32=(UINT32 *) feature_para;
        unsigned long long *feature_data=(unsigned long long *) feature_para;
        struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
        MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

        LOG_INF("feature_id = %d\n", feature_id);
        switch (feature_id) {
                case SENSOR_FEATURE_GET_PERIOD:
                        *feature_return_para_16++ = imgsensor.line_length;
                        *feature_return_para_16 = imgsensor.frame_length;
                        *feature_para_len=4;
                        break;
                case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
                        *feature_return_para_32 = imgsensor.pclk;
                        *feature_para_len=4;
                        break;
                case SENSOR_FEATURE_SET_ESHUTTER:
                        set_shutter(*feature_data);
                        break;
                case SENSOR_FEATURE_SET_NIGHTMODE:
                        night_mode((BOOL) *feature_data);
                        break;
                case SENSOR_FEATURE_SET_GAIN:
                        set_gain((UINT16) *feature_data);
                        break;
                case SENSOR_FEATURE_SET_FLASHLIGHT:
                        break;
                case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
                        break;
                case SENSOR_FEATURE_SET_REGISTER:
                        write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
                        break;
                case SENSOR_FEATURE_GET_REGISTER:
                        sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
                        break;
                case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
                        /*  get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
                        /*  if EEPROM does not exist in camera module. */
                        *feature_return_para_32=LENS_DRIVER_ID_DO_NOT_CARE;
                        *feature_para_len=4;
                        break;
                case SENSOR_FEATURE_SET_VIDEO_MODE:
                        set_video_mode(*feature_data);
                        break;
                case SENSOR_FEATURE_CHECK_SENSOR_ID:
                        get_imgsensor_id(feature_return_para_32);
                        break;
                case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
                        set_auto_flicker_mode((BOOL)*feature_data_16,*(feature_data_16+1));
                        break;
                case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
                        set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
                        break;
                case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
                        get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)
                            *(feature_data), (MUINT32 *) (uintptr_t) (*(feature_data + 1)));
                        break;
                case SENSOR_FEATURE_SET_TEST_PATTERN:
                        set_test_pattern_mode((BOOL)*feature_data);
                        break;
                case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:        /* for factory mode auto testing */
                        *feature_return_para_32 = imgsensor_info.checksum_value;
                        *feature_para_len=4;
                        break;
                case SENSOR_FEATURE_GET_SENSOR_ID:
                        *feature_return_para_32 = imgsensor_info.sensor_id;
                        *feature_para_len = 4;
                        break;
                case SENSOR_FEATURE_SET_FRAMERATE:
                        LOG_INF("current fps :%d\n", (UINT32)*feature_data);
                        spin_lock(&imgsensor_drv_lock);
                        imgsensor.current_fps = *feature_data;
                        spin_unlock(&imgsensor_drv_lock);
                        break;
                case SENSOR_FEATURE_SET_HDR:
                        LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data);
                        spin_lock(&imgsensor_drv_lock);
                        imgsensor.ihdr_en = (BOOL)*feature_data;
                        spin_unlock(&imgsensor_drv_lock);
                        break;
                case SENSOR_FEATURE_GET_CROP_INFO:
                        LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
                            (UINT32) * feature_data);

                        wininfo =
                            (struct SENSOR_WINSIZE_INFO_STRUCT
                            *) (uintptr_t) (*(feature_data + 1));

                        switch (*feature_data_32)
                        {
                        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                                if (copy_to_user((void __user *) wininfo,
                                        (void *) &imgsensor_winsize_info[1],
                                        sizeof(struct SENSOR_WINSIZE_INFO_STRUCT)))
                                {
                                        LOG_INF("copy to user failed\n");
                                        return -EFAULT;
                                }
                                break;
                        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                                if (copy_to_user((void __user *) wininfo,
                                        (void *) &imgsensor_winsize_info[2],
                                        sizeof(struct SENSOR_WINSIZE_INFO_STRUCT)))
                                {
                                        LOG_INF("copy to user failed\n");
                                        return -EFAULT;
                                }
                                break;
                        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                                if (copy_to_user((void __user *) wininfo,
                                        (void *) &imgsensor_winsize_info[3],
                                        sizeof(struct SENSOR_WINSIZE_INFO_STRUCT)))
                                {
                                        LOG_INF("copy to user failed\n");
                                        return -EFAULT;
                                }
                                break;
                        case MSDK_SCENARIO_ID_SLIM_VIDEO:
                                if (copy_to_user((void __user *) wininfo,
                                        (void *) &imgsensor_winsize_info[4],
                                        sizeof(struct SENSOR_WINSIZE_INFO_STRUCT)))
                                {
                                        LOG_INF("copy to user failed\n");
                                        return -EFAULT;
                                }
                                break;
                        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                        default:
                                if (copy_to_user((void __user *) wininfo,
                                        (void *) &imgsensor_winsize_info[0],
                                        sizeof(struct SENSOR_WINSIZE_INFO_STRUCT)))
                                {
                                        LOG_INF("copy to user failed\n");
                                        return -EFAULT;
                                }
                                break;
                        }
                        break;
                case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
                        LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",(UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
                        ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
                        break;
                default:
                        break;
        }

        return ERROR_NONE;
}        /* feature_control() */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
        open,
        get_info,
        get_resolution,
        feature_control,
        control,
        close
};

UINT32 OV9734_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
        /* To Do : Check Sensor status here */
        if (pfFunc!=NULL)
                *pfFunc=&sensor_func;
        return ERROR_NONE;
}        /* OV9734_MIPI_RAW_SensorInit */
