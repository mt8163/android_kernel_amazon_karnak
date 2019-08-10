/*****************************************************************************
 *
 * Filename:
 * ---------
 *   sp2518yuv_Sensor.c
 *
 * Project:
 * --------
 *   MAUI
 *
 * Description:
 * ------------
 *   Image sensor driver function
 *   V1.0.0
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
#include "kd_camera_feature.h"
#include "sp2518yuv_Sensor.h"
#include "sp2518yuv_Camera_Sensor_para.h"
#include "sp2518yuv_CameraCustomized.h"

/*******************************************************************************
 * // Adapter for Winmo typedef
 ********************************************************************************/
#define WINMO_USE 0

#define Sleep(ms) mdelay(ms)
#define RETAILMSG(x,...)
#define TEXT

#define PFX         "SP2518YUV"
#define DEBUG	0
#if DEBUG
    #define LOG_INF(format, args...)	pr_debug( PFX "[%s] " format, __FUNCTION__, ##args)
#else
    #define LOG_INF(format, args...)   
#endif

//#define DEBUG_SENSOR_SP2518

#define SP2518_24M_72M

#define PREVIEW_USE_VGA     0
#define PREVIEW_USE_SVGA    1
// don't use 720p, sensor may not output data
#define PREVIEW_USE_720P    0

#if PREVIEW_USE_VGA
#undef IMAGE_SENSOR_PV_WIDTH
#undef IMAGE_SENSOR_PV_HEIGHT
#define IMAGE_SENSOR_PV_WIDTH					(IMAGE_SENSOR_VGA_WIDTH - 8)
#define IMAGE_SENSOR_PV_HEIGHT					(IMAGE_SENSOR_VGA_HEIGHT - 6)
#endif

#if PREVIEW_USE_SVGA
#undef IMAGE_SENSOR_PV_WIDTH
#undef IMAGE_SENSOR_PV_HEIGHT
#define IMAGE_SENSOR_PV_WIDTH					(IMAGE_SENSOR_SVGA_WIDTH - 8)
#define IMAGE_SENSOR_PV_HEIGHT					(IMAGE_SENSOR_SVGA_HEIGHT - 6)
#endif

#if PREVIEW_USE_720P
#undef IMAGE_SENSOR_PV_WIDTH
#undef IMAGE_SENSOR_PV_HEIGHT
#define IMAGE_SENSOR_PV_WIDTH					(IMAGE_SENSOR_720P_WIDTH - 8)
#define IMAGE_SENSOR_PV_HEIGHT					(IMAGE_SENSOR_720P_HEIGHT - 6)
#endif


#ifdef DEBUG_SENSOR_SP2518

kal_uint8 fromsd = 0;
kal_uint16 SP2518_write_cmos_sensor(kal_uint8 addr, kal_uint8 para);
kal_uint16 SP2518_read_cmos_sensor(kal_uint8 addr);

#define SP2518_OP_CODE_INI		0x00		/* Initial value. */
#define SP2518_OP_CODE_REG		0x01		/* Register */
#define SP2518_OP_CODE_DLY		0x02		/* Delay */
#define SP2518_OP_CODE_END		0x03		/* End of initial setting. */
	

typedef struct
{
	u16 init_reg;
	u16 init_val;	/* Save the register value and delay tick */
	u8 op_code;		/* 0 - Initial value, 1 - Register, 2 - Delay, 3 - End of setting. */
} SP2518_initial_set_struct;

SP2518_initial_set_struct SP2518_Init_Reg[1000];

u32 strtol(const char *nptr, u8 base)
{
	u8 ret;
	if(!nptr || (base!=16 && base!=10 && base!=8))
	{
		printk("%s(): NULL pointer input\n", __FUNCTION__);
		return -1;
	}
	for(ret=0; *nptr; nptr++)
	{
		if((base==16 && *nptr>='A' && *nptr<='F') || 
				(base==16 && *nptr>='a' && *nptr<='f') || 
				(base>=10 && *nptr>='0' && *nptr<='9') ||
				(base>=8 && *nptr>='0' && *nptr<='7') )
		{
			ret *= base;
			if(base==16 && *nptr>='A' && *nptr<='F')
				ret += *nptr-'A'+10;
			else if(base==16 && *nptr>='a' && *nptr<='f')
				ret += *nptr-'a'+10;
			else if(base>=10 && *nptr>='0' && *nptr<='9')
				ret += *nptr-'0';
			else if(base>=8 && *nptr>='0' && *nptr<='7')
				ret += *nptr-'0';
		}
		else
			return ret;
	}
	return ret;
}

u8 SP2518_Initialize_from_T_Flash()
{
	//FS_HANDLE fp = -1;				/* Default, no file opened. */
	//u8 *data_buff = NULL;
	u8 *curr_ptr = NULL;
	u32 file_size = 0;
	//u32 bytes_read = 0;
	u32 i = 0, j = 0;
	u8 func_ind[4] = {0};	/* REG or DLY */


	struct file *fp; 
	mm_segment_t fs; 
	loff_t pos = 0; 
	static u8 data_buff[10*1024] ;

    fp = filp_open("/storage/sdcard1/sp2518_sd.dat", O_RDONLY , 0); 
    if (IS_ERR(fp)) { 
		printk("create file error 0\n");
		fp = filp_open("/storage/sdcard0/sp2518_sd.dat", O_RDONLY , 0); 
    	if (IS_ERR(fp)) { 
	        printk("create file error 1\n"); 
	        return 2;//-1; 
    	}
    } 
    else
		printk("SP2518_Initialize_from_T_Flash Open File Success\n");
	fs = get_fs(); 
	set_fs(KERNEL_DS); 

	file_size = vfs_llseek(fp, 0, SEEK_END);
	vfs_read(fp, data_buff, file_size, &pos); 
	//printk("%s %d %d\n", buf,iFileLen,pos); 
	filp_close(fp, NULL); 
	set_fs(fs);


	/* Start parse the setting witch read from t-flash. */
	curr_ptr = data_buff;
	while (curr_ptr < (data_buff + file_size))
	{
		while ((*curr_ptr == ' ') || (*curr_ptr == '\t'))/* Skip the Space & TAB */
			curr_ptr++;				

		if (((*curr_ptr) == '/') && ((*(curr_ptr + 1)) == '*'))
		{
			while (!(((*curr_ptr) == '*') && ((*(curr_ptr + 1)) == '/')))
			{
				curr_ptr++;		/* Skip block comment code. */
			}

			while (!((*curr_ptr == 0x0D) && (*(curr_ptr+1) == 0x0A)))
			{
				curr_ptr++;
			}

			curr_ptr += 2;						/* Skip the enter line */

			continue ;
		}

		if (((*curr_ptr) == '/') || ((*curr_ptr) == '{') || ((*curr_ptr) == '}'))		/* Comment line, skip it. */
		{
			while (!((*curr_ptr == 0x0D) && (*(curr_ptr+1) == 0x0A)))
			{
				curr_ptr++;
			}

			curr_ptr += 2;						/* Skip the enter line */

			continue ;
		}
		/* This just content one enter line. */
		if (((*curr_ptr) == 0x0D) && ((*(curr_ptr + 1)) == 0x0A))
		{
			curr_ptr += 2;
			continue ;
		}
		//printk(" curr_ptr1 = %s\n",curr_ptr);
		memcpy(func_ind, curr_ptr, 3);


		if (strcmp((const char *)func_ind, "REG") == 0)		/* REG */
		{
			curr_ptr += 6;				/* Skip "REG(0x" or "DLY(" */
			SP2518_Init_Reg[i].op_code = SP2518_OP_CODE_REG;

			SP2518_Init_Reg[i].init_reg = strtol((const char *)curr_ptr, 16);
			curr_ptr += 5;	/* Skip "00, 0x" */

			SP2518_Init_Reg[i].init_val = strtol((const char *)curr_ptr, 16);
			curr_ptr += 4;	/* Skip "00);" */

		}
		else									/* DLY */
		{
			/* Need add delay for this setting. */
			curr_ptr += 4;	
			SP2518_Init_Reg[i].op_code = SP2518_OP_CODE_DLY;

			SP2518_Init_Reg[i].init_reg = 0xFF;
			SP2518_Init_Reg[i].init_val = strtol((const char *)curr_ptr,  10);	/* Get the delay ticks, the delay should less then 50 */
		}
		i++;


		/* Skip to next line directly. */
		while (!((*curr_ptr == 0x0D) && (*(curr_ptr+1) == 0x0A)))
		{
			curr_ptr++;
		}
		curr_ptr += 2;
	}

	/* (0xFFFF, 0xFFFF) means the end of initial setting. */
	SP2518_Init_Reg[i].op_code = SP2518_OP_CODE_END;
	SP2518_Init_Reg[i].init_reg = 0xFF;
	SP2518_Init_Reg[i].init_val = 0xFF;
	i++;
	//for (j=0; j<i; j++)
	//printk(" %x  ==  %x\n",SP2518_Init_Reg[j].init_reg, SP2518_Init_Reg[j].init_val);

	/* Start apply the initial setting to sensor. */
#if 1
	for (j=0; j<i; j++)
	{
		if (SP2518_Init_Reg[j].op_code == SP2518_OP_CODE_END)	/* End of the setting. */
		{
			break ;
		}
		else if (SP2518_Init_Reg[j].op_code == SP2518_OP_CODE_DLY)
		{
			msleep(SP2518_Init_Reg[j].init_val);		/* Delay */
		}
		else if (SP2518_Init_Reg[j].op_code == SP2518_OP_CODE_REG)
		{

			SP2518_write_cmos_sensor((kal_uint8)SP2518_Init_Reg[j].init_reg, (kal_uint8)SP2518_Init_Reg[j].init_val);
		}
		else
		{
			printk("REG ERROR!\n");
		}
	}
#endif
	return 1;	
}

#endif
//auto lum
#define SP2518_NORMAL_Y0ffset  0x18 //0x10
#define SP2518_LOWLIGHT_Y0ffset  0x20

kal_bool   SP2518_MPEG4_encode_mode = KAL_FALSE;
kal_uint16 SP2518_dummy_pixels = 0, SP2518_dummy_lines = 0;
kal_bool   SP2518_MODE_CAPTURE = KAL_FALSE;
static kal_bool SP2518_gPVmode = KAL_TRUE; //PV size or Full size


static kal_int8 SP2518_DELAY_AFTER_PREVIEW = -1;

kal_bool   SP2518_CAM_BANDING_50HZ = KAL_TRUE;//lj_test KAL_FALSE;
kal_bool   SP2518_CAM_Nightmode = 0;
kal_bool	setshutter = KAL_FALSE;
kal_uint32 SP2518_isp_master_clock;
static kal_uint32 SP2518_g_fPV_PCLK = 24;

kal_uint8 SP2518_sensor_write_I2C_address = SP2518_WRITE_ID;
kal_uint8 SP2518_sensor_read_I2C_address = SP2518_READ_ID;

UINT8 SP2518PixelClockDivider=0;

MSDK_SENSOR_CONFIG_STRUCT SP2518SensorConfigData;

#define SP2518_SET_PAGE0 	SP2518_write_cmos_sensor(0xfd, 0x00)
#define SP2518_SET_PAGE1 	SP2518_write_cmos_sensor(0xfd, 0x01)

#define SP2518YUV_DEBUG
#ifdef SP2518YUV_DEBUG
#define SENSORDB(fmt, args...)  pr_debug(PFX "%s: " fmt, __FUNCTION__ ,##args)
#else
#define SENSORDB(x,...)
#endif

#define WINDOW_SIZE_UXGA	0
#define WINDOW_SIZE_720P 	1
#define WINDOW_SIZE_SVGA 	2
#define WINDOW_SIZE_VGA	 	3

extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);


/* Sensor output window information */
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5]=
{{ 1600, 1200,      0,    0, 1600, 1200, 1600,  1200,      0,   0, 1600,  1200,  8,    6, 1584,  1188},  // Preview
 { 1600, 1200,      0,    0, 1600, 1200, 1600,  1200,      0,   0, 1600,  1200,  8,    6, 1584,  1188}, // capture
 { 1600, 1200,      0,    0, 1600, 1200, 1600,  1200,      0,   0, 1600,  1200,  8,    6, 1584,  1188},  // video
 { 1600, 1200,      0,    0, 1600, 1200, 1600,  1200,      0,   0, 1600,  1200,  160,    240, 1280,  720}, //hight speed video
 { 1600, 1200,      0,    0, 1600, 1200, 1600,  1200,      0,   0, 1600,  1200,  160,    240, 1280,  720}}; // slim video


void SP2518_write_cmos_sensor(kal_uint8 addr, kal_uint8 para)
{
	char puSendCmd[2] = {(char)(addr & 0xFF) , (char)(para & 0xFF)};

	kdSetI2CSpeed(400);
	iWriteRegI2C(puSendCmd , 2, SP2518_WRITE_ID);

}
kal_uint16 SP2518_read_cmos_sensor(kal_uint8 addr)
{
	kal_uint16 get_byte=0;
	char puSendCmd = { (char)(addr & 0xFF) };

	kdSetI2CSpeed(400);
	iReadRegI2C(&puSendCmd , 1, (u8*)&get_byte, 1, SP2518_WRITE_ID);
	
	return get_byte;
}

/*************************************************************************
 * FUNCTION
 *	SP2518_SetShutter
 *
 * DESCRIPTION
 *	This function set e-shutter of SP2518 to change exposure time.
 *
 * PARAMETERS
 *   iShutter : exposured lines
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
void SP2518_Set_Shutter(kal_uint16 iShutter)
{
	SENSORDB("Ronlus SP2518_Set_Shutter\r\n");
} /* Set_SP2518_Shutter */


/*************************************************************************
 * FUNCTION
 *	SP2518_read_Shutter
 *
 * DESCRIPTION
 *	This function read e-shutter of SP2518 .
 *
 * PARAMETERS
 *  None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
kal_uint16 SP2518_Read_Shutter(void)
{
	//kal_uint8 temp_reg1, temp_reg2;
	kal_uint16 shutter = 0;

	//temp_reg1 = SP2518_read_cmos_sensor(0x04);
	//temp_reg2 = SP2518_read_cmos_sensor(0x03);

	//shutter = (temp_reg1 & 0xFF) | (temp_reg2 << 8);
	SENSORDB("Ronlus SP2518_Read_Shutter\r\n");
	return shutter;
} /* SP2518_read_shutter */

#if 0

/*************************************************************************
 * FUNCTION
 *	SP2518_ae_enable
 *
 * DESCRIPTION
 *	This function enable or disable the awb (Auto White Balance).
 *
 * PARAMETERS
 *	1. kal_bool : KAL_TRUE - enable awb, KAL_FALSE - disable awb.
 *
 * RETURNS
 *	kal_bool : It means set awb right or not.
 *
 *************************************************************************/

static void SP2518_ae_enable(kal_bool enable)
{	 
	kal_uint16 temp_AWB_reg = 0;
	SENSORDB("Ronlus SP2518_ae_enable\r\n");
	temp_AWB_reg = SP2518_read_cmos_sensor(0x32);//Ronlus 20120511
	if (enable == 1)
	{
		SP2518_write_cmos_sensor(0x32, (temp_AWB_reg |0x05));
	}
	else if(enable == 0)
	{
		SP2518_write_cmos_sensor(0x32, (temp_AWB_reg & (~0x05)));
	}
}

/*************************************************************************
 * FUNCTION
 *	SP2518_awb_enable
 *
 * DESCRIPTION
 *	This function enable or disable the awb (Auto White Balance).
 *
 * PARAMETERS
 *	1. kal_bool : KAL_TRUE - enable awb, KAL_FALSE - disable awb.
 *
 * RETURNS
 *	kal_bool : It means set awb right or not.
 *
 *************************************************************************/
static void SP2518_awb_enable(kal_bool enalbe)
{	 
	//kal_uint16 temp_AWB_reg = 0;
	SENSORDB("Ronlus SP2518_awb_enable\r\n");
}


/*************************************************************************
 * FUNCTION
 *	SP2518_set_hb_shutter
 *
 * DESCRIPTION
 *	This function set the dummy pixels(Horizontal Blanking) when capturing, it can be
 *	used to adjust the frame rate  for back-end process.
 *	
 *	IMPORTANT NOTICE: the base shutter need re-calculate for some sensor, or else flicker may occur.
 *
 * PARAMETERS
 *	1. kal_uint32 : Dummy Pixels (Horizontal Blanking)
 *	2. kal_uint32 : shutter (Vertical Blanking)
 *
 * RETURNS
 *	None
 *
 *************************************************************************/
static void SP2518_set_hb_shutter(kal_uint32 hb_add,  kal_uint32 shutter)
{
//	kal_uint32 hb_ori, hb_total;
//	kal_uint32 temp_reg, banding_step;
	SENSORDB("Ronlus SP2518_set_hb_shutter\r\n");
}    /* SP2518_set_dummy */
#endif

/*************************************************************************
 * FUNCTION
 *	SP2518_config_window
 *
 * DESCRIPTION
 *	This function config the hardware window of SP2518 for getting specified
 *  data of that window.
 *
 * PARAMETERS
 *	start_x : start column of the interested window
 *  start_y : start row of the interested window
 *  width  : column widht of the itnerested window
 *  height : row depth of the itnerested window
 *
 * RETURNS
 *	the data that read from SP2518
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
//void SP2518_config_window(kal_uint16 startx, kal_uint16 starty, kal_uint16 width, kal_uint16 height)//Ronlus
void SP2518_config_window(kal_uint8 index)
{
	SENSORDB("Ronlus SP2518_config_window,index = %d\r\n",index);
	switch(index)
	{
		case WINDOW_SIZE_UXGA:
			/*1600x1200 this is for resize para*/
			SP2518_write_cmos_sensor(0xfd , 0x00);	
			SP2518_write_cmos_sensor(0x47 , 0x00);
			SP2518_write_cmos_sensor(0x48 , 0x00);
			SP2518_write_cmos_sensor(0x49 , 0x04);
			SP2518_write_cmos_sensor(0x4a , 0xb0);
			SP2518_write_cmos_sensor(0x4b , 0x00);
			SP2518_write_cmos_sensor(0x4c , 0x00);
			SP2518_write_cmos_sensor(0x4d , 0x06);
			SP2518_write_cmos_sensor(0x4e , 0x40);

			SP2518_write_cmos_sensor(0xfd , 0x01);
			SP2518_write_cmos_sensor(0x06 , 0x00);
			SP2518_write_cmos_sensor(0x07 , 0x40);
			SP2518_write_cmos_sensor(0x08 , 0x00);
			SP2518_write_cmos_sensor(0x09 , 0x40);
			SP2518_write_cmos_sensor(0x0a , 0x02);
			SP2518_write_cmos_sensor(0x0b , 0x58);
			SP2518_write_cmos_sensor(0x0c , 0x03);
			SP2518_write_cmos_sensor(0x0d , 0x20);
			SP2518_write_cmos_sensor(0x0e , 0x00);//resize_en
			SP2518_write_cmos_sensor(0x0f , 0x00);
			SP2518_write_cmos_sensor(0xfd , 0x00);
			SP2518_write_cmos_sensor(0x2f , 0x00);
			break;
		case WINDOW_SIZE_720P:
			/**1280*720  this is for crop para*/
			SP2518_write_cmos_sensor(0xfd , 0x01);
			SP2518_write_cmos_sensor(0x47 , 0x00);
			SP2518_write_cmos_sensor(0x48 , 0xf0);
			SP2518_write_cmos_sensor(0x49 , 0x02);
			SP2518_write_cmos_sensor(0x4a , 0xd0);
			SP2518_write_cmos_sensor(0x4b , 0x00);
			SP2518_write_cmos_sensor(0x4c , 0xa0);
			SP2518_write_cmos_sensor(0x4d , 0x05);
			SP2518_write_cmos_sensor(0x4e , 0x00);
			break;
		case WINDOW_SIZE_SVGA:
			/*SVGA this is for resize para*/							 			
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0x4b,0x00);
			SP2518_write_cmos_sensor(0x4c,0x00);
			SP2518_write_cmos_sensor(0x47,0x00);
			SP2518_write_cmos_sensor(0x48,0x00);
			SP2518_write_cmos_sensor(0xfd,0x01);
			SP2518_write_cmos_sensor(0x06,0x00);
			SP2518_write_cmos_sensor(0x07,0x40);
			SP2518_write_cmos_sensor(0x08,0x00);
			SP2518_write_cmos_sensor(0x09,0x40);
			SP2518_write_cmos_sensor(0x0a,0x02);
			SP2518_write_cmos_sensor(0x0b,0x58);
			SP2518_write_cmos_sensor(0x0c,0x03);
			SP2518_write_cmos_sensor(0x0d,0x20);
			SP2518_write_cmos_sensor(0x0e,0x01);
			SP2518_write_cmos_sensor(0xfd,0x00);
			break;
		case WINDOW_SIZE_VGA:
			/*VGA this is for resize para*/							 
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0x4b,0x00);
			SP2518_write_cmos_sensor(0x4c,0x00);
			SP2518_write_cmos_sensor(0x47,0x00);
			SP2518_write_cmos_sensor(0x48,0x00);
			SP2518_write_cmos_sensor(0xfd,0x01);
			SP2518_write_cmos_sensor(0x06,0x00);
			SP2518_write_cmos_sensor(0x07,0x50);
			SP2518_write_cmos_sensor(0x08,0x00);
			SP2518_write_cmos_sensor(0x09,0x50);
			SP2518_write_cmos_sensor(0x0a,0x01);
			SP2518_write_cmos_sensor(0x0b,0xe0);
			SP2518_write_cmos_sensor(0x0c,0x02);
			SP2518_write_cmos_sensor(0x0d,0x80);
			SP2518_write_cmos_sensor(0x0e,0x01);
			SP2518_write_cmos_sensor(0xfd,0x00);
			break;
		default:
			break;
	}
} /* SP2518_config_window */



/*************************************************************************
 * FUNCTION
 *	SP2518_SetGain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *   iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
kal_uint16 SP2518_SetGain(kal_uint16 iGain)
{
	SENSORDB("Ronlus SP2518_SetGain\r\n");
	return iGain;
}


/*************************************************************************
 * FUNCTION
 *	SP2518_NightMode
 *
 * DESCRIPTION
 *	This function night mode of SP2518.
 *
 * PARAMETERS
 *	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
void SP2518_night_mode(kal_bool bEnable)
{
	//kal_uint16 night = SP2518_read_cmos_sensor(0x20);
	SENSORDB("Ronlus SP2518_night_mode\r\n");//\810\886\810\893\810\884\810\870\810\865\810\896\810\856\810\870\810\867\810\858sp2518      a8-10 \810\857\810\857\810\862\810\887\810\880\810\8A7810 3p  
	//sensorlist.cpp kd_imagesensor.h add related files  
#if 1//
	if (bEnable)/*night mode settings*/
	{
		SENSORDB("Ronlus night mode\r\n");
		SP2518_CAM_Nightmode = 1;
		SP2518_write_cmos_sensor(0xfd,0x00);
		SP2518_write_cmos_sensor(0xb2,SP2518_LOWLIGHT_Y0ffset);
		if(SP2518_MPEG4_encode_mode == KAL_TRUE) /*video night mode*/
		{
			SENSORDB("Ronlus video night mode\r\n");

			if(SP2518_CAM_BANDING_50HZ == KAL_TRUE)/*video night mode 50hz*/
			{	
#ifdef SP2518_24M_72M		
				// 3X pll   fix 6fps      video night mode 50hz                                
				SP2518_write_cmos_sensor(0xfd , 0x00);
				SP2518_write_cmos_sensor(0x03 , 0x01);
				SP2518_write_cmos_sensor(0x04 , 0xd4);
				SP2518_write_cmos_sensor(0x05 , 0x00);
				SP2518_write_cmos_sensor(0x06 , 0x00);
				SP2518_write_cmos_sensor(0x07 , 0x00);
				SP2518_write_cmos_sensor(0x08 , 0x00);
				SP2518_write_cmos_sensor(0x09 , 0x05);
				SP2518_write_cmos_sensor(0x0a , 0x66);
				SP2518_write_cmos_sensor(0xf0 , 0x4e);
				SP2518_write_cmos_sensor(0xf1 , 0x00);
				SP2518_write_cmos_sensor(0xfd , 0x01);
				SP2518_write_cmos_sensor(0x90 , 0x10);
				SP2518_write_cmos_sensor(0x92 , 0x01);
				SP2518_write_cmos_sensor(0x98 , 0x4e);
				SP2518_write_cmos_sensor(0x99 , 0x00);
				SP2518_write_cmos_sensor(0x9a , 0x01);
				SP2518_write_cmos_sensor(0x9b , 0x00);
				//Status                              
				SP2518_write_cmos_sensor(0xfd , 0x01);
				SP2518_write_cmos_sensor(0xce , 0xe0);
				SP2518_write_cmos_sensor(0xcf , 0x04);
				SP2518_write_cmos_sensor(0xd0 , 0xe0);
				SP2518_write_cmos_sensor(0xd1 , 0x04);
				SP2518_write_cmos_sensor(0xd7 , 0x50);
				SP2518_write_cmos_sensor(0xd8 , 0x00);
				SP2518_write_cmos_sensor(0xd9 , 0x54);
				SP2518_write_cmos_sensor(0xda , 0x00);
				SP2518_write_cmos_sensor(0xfd , 0x00);
#else
				//capture video night 36M 50hz fix 6FPS maxgain 
				SP2518_write_cmos_sensor(0xfd,0x00);
				SP2518_write_cmos_sensor(0x03,0x01);
				SP2518_write_cmos_sensor(0x04,0xd4);
				SP2518_write_cmos_sensor(0x05,0x00);
				SP2518_write_cmos_sensor(0x06,0x00);
				SP2518_write_cmos_sensor(0x07,0x00);
				SP2518_write_cmos_sensor(0x08,0x00);
				SP2518_write_cmos_sensor(0x09,0x00);
				SP2518_write_cmos_sensor(0x0a,0xe4);
				//SP2518_write_cmos_sensor(0x2f,0x00);
				//SP2518_write_cmos_sensor(0x30,0x09);
				SP2518_write_cmos_sensor(0xf0,0x4e);
				SP2518_write_cmos_sensor(0xf1,0x00);
				SP2518_write_cmos_sensor(0xfd,0x01);
				SP2518_write_cmos_sensor(0x90,0x10);
				SP2518_write_cmos_sensor(0x92,0x01);
				SP2518_write_cmos_sensor(0x98,0x4e);
				SP2518_write_cmos_sensor(0x99,0x00);
				SP2518_write_cmos_sensor(0x9a,0x01);
				SP2518_write_cmos_sensor(0x9b,0x00);
				SP2518_write_cmos_sensor(0xfd,0x01);
				SP2518_write_cmos_sensor(0xce,0xe0);
				SP2518_write_cmos_sensor(0xcf,0x04);
				SP2518_write_cmos_sensor(0xd0,0xe0);
				SP2518_write_cmos_sensor(0xd1,0x04);
				SP2518_write_cmos_sensor(0xd7,0x4a);
				SP2518_write_cmos_sensor(0xd8,0x00);
				SP2518_write_cmos_sensor(0xd9,0x4e);
				SP2518_write_cmos_sensor(0xda,0x00);
				SP2518_write_cmos_sensor(0xfd,0x00);
#endif  				
				SENSORDB("Ronlus video night mode 50hz\r\n");
			}
			else/*video night mode 60hz*/
			{ 
				SENSORDB("Ronlus video night mode 60hz\r\n");     
#ifdef SP2518_24M_72M		

				//X pll   fix 6fps    video night mode 60hz                                              
				SP2518_write_cmos_sensor(0xfd , 0x00);
				SP2518_write_cmos_sensor(0x03 , 0x01);
				SP2518_write_cmos_sensor(0x04 , 0x86);
				SP2518_write_cmos_sensor(0x05 , 0x00);
				SP2518_write_cmos_sensor(0x06 , 0x00);
				SP2518_write_cmos_sensor(0x07 , 0x00);
				SP2518_write_cmos_sensor(0x08 , 0x00);
				SP2518_write_cmos_sensor(0x09 , 0x05);
				SP2518_write_cmos_sensor(0x0a , 0x66);
				SP2518_write_cmos_sensor(0xf0 , 0x41);
				SP2518_write_cmos_sensor(0xf1 , 0x00);
				SP2518_write_cmos_sensor(0xfd , 0x01);
				SP2518_write_cmos_sensor(0x90 , 0x14);
				SP2518_write_cmos_sensor(0x92 , 0x01);
				SP2518_write_cmos_sensor(0x98 , 0x41);
				SP2518_write_cmos_sensor(0x99 , 0x00);
				SP2518_write_cmos_sensor(0x9a , 0x01);
				SP2518_write_cmos_sensor(0x9b , 0x00);
				//Status                              
				SP2518_write_cmos_sensor(0xfd , 0x01);
				SP2518_write_cmos_sensor(0xce , 0x14);
				SP2518_write_cmos_sensor(0xcf , 0x05);
				SP2518_write_cmos_sensor(0xd0 , 0x14);
				SP2518_write_cmos_sensor(0xd1 , 0x05);
				SP2518_write_cmos_sensor(0xd7 , 0x43);
				SP2518_write_cmos_sensor(0xd8 , 0x00);
				SP2518_write_cmos_sensor(0xd9 , 0x47);
				SP2518_write_cmos_sensor(0xda , 0x00);
				SP2518_write_cmos_sensor(0xfd , 0x00);
#else

				//capture video night 36M 60hz fix 6FPS maxgain
				SP2518_write_cmos_sensor(0xfd,0x00);
				SP2518_write_cmos_sensor(0x03,0x01);
				SP2518_write_cmos_sensor(0x04,0x86);
				SP2518_write_cmos_sensor(0x05,0x00);
				SP2518_write_cmos_sensor(0x06,0x00);
				SP2518_write_cmos_sensor(0x07,0x00);
				SP2518_write_cmos_sensor(0x08,0x00);
				SP2518_write_cmos_sensor(0x09,0x00);
				SP2518_write_cmos_sensor(0x0a,0xe4);
				//SP2518_write_cmos_sensor(0x2f,0x00);
				//SP2518_write_cmos_sensor(0x30,0x09);
				SP2518_write_cmos_sensor(0xf0,0x41);
				SP2518_write_cmos_sensor(0xf1,0x00);
				SP2518_write_cmos_sensor(0xfd,0x01);
				SP2518_write_cmos_sensor(0x90,0x14);
				SP2518_write_cmos_sensor(0x92,0x01);
				SP2518_write_cmos_sensor(0x98,0x41);
				SP2518_write_cmos_sensor(0x99,0x00);
				SP2518_write_cmos_sensor(0x9a,0x01);
				SP2518_write_cmos_sensor(0x9b,0x00);
				SP2518_write_cmos_sensor(0xfd,0x01);
				SP2518_write_cmos_sensor(0xce,0x14);
				SP2518_write_cmos_sensor(0xcf,0x05);
				SP2518_write_cmos_sensor(0xd0,0x14);
				SP2518_write_cmos_sensor(0xd1,0x05);
				SP2518_write_cmos_sensor(0xd7,0x3d);
				SP2518_write_cmos_sensor(0xd8,0x00);
				SP2518_write_cmos_sensor(0xd9,0x41);
				SP2518_write_cmos_sensor(0xda,0x00);
				SP2518_write_cmos_sensor(0xfd,0x00);

#endif
			}
		}/*capture night mode*/
		else 
		{   
			SENSORDB("Ronlus capture night mode\r\n");
			if(SP2518_CAM_BANDING_50HZ == KAL_TRUE)/*capture night mode 50hz*/
			{	
				SENSORDB("Ronlus capture night mode 50hz\r\n");//3
#ifdef SP2518_24M_72M		

				//X pll   fix 6fps     capture night mode 50hz                                
				SP2518_write_cmos_sensor(0xfd , 0x00);
				SP2518_write_cmos_sensor(0x03 , 0x01);
				SP2518_write_cmos_sensor(0x04 , 0xd4);
				SP2518_write_cmos_sensor(0x05 , 0x00);
				SP2518_write_cmos_sensor(0x06 , 0x00);
				SP2518_write_cmos_sensor(0x07 , 0x00);
				SP2518_write_cmos_sensor(0x08 , 0x00);
				SP2518_write_cmos_sensor(0x09 , 0x05);
				SP2518_write_cmos_sensor(0x0a , 0x66);
				SP2518_write_cmos_sensor(0xf0 , 0x4e);
				SP2518_write_cmos_sensor(0xf1 , 0x00);
				SP2518_write_cmos_sensor(0xfd , 0x01);
				SP2518_write_cmos_sensor(0x90 , 0x10);
				SP2518_write_cmos_sensor(0x92 , 0x01);
				SP2518_write_cmos_sensor(0x98 , 0x4e);
				SP2518_write_cmos_sensor(0x99 , 0x00);
				SP2518_write_cmos_sensor(0x9a , 0x01);
				SP2518_write_cmos_sensor(0x9b , 0x00);
				//Status                              
				SP2518_write_cmos_sensor(0xfd , 0x01);
				SP2518_write_cmos_sensor(0xce , 0xe0);
				SP2518_write_cmos_sensor(0xcf , 0x04);
				SP2518_write_cmos_sensor(0xd0 , 0xe0);
				SP2518_write_cmos_sensor(0xd1 , 0x04);
				SP2518_write_cmos_sensor(0xd7 , 0x50);
				SP2518_write_cmos_sensor(0xd8 , 0x00);
				SP2518_write_cmos_sensor(0xd9 , 0x54);
				SP2518_write_cmos_sensor(0xda , 0x00);
				SP2518_write_cmos_sensor(0xfd , 0x00);
#else
				//capture preview night 36M 50hz fix 6FPS maxgain 
				SP2518_write_cmos_sensor(0xfd,0x00);
				SP2518_write_cmos_sensor(0x03,0x01);
				SP2518_write_cmos_sensor(0x04,0xd4);
				SP2518_write_cmos_sensor(0x05,0x00);
				SP2518_write_cmos_sensor(0x06,0x00);
				SP2518_write_cmos_sensor(0x07,0x00);
				SP2518_write_cmos_sensor(0x08,0x00);
				SP2518_write_cmos_sensor(0x09,0x00);
				SP2518_write_cmos_sensor(0x0a,0xe4);
				//SP2518_write_cmos_sensor(0x2f,0x00);
				//SP2518_write_cmos_sensor(0x30,0x09);
				SP2518_write_cmos_sensor(0xf0,0x4e);
				SP2518_write_cmos_sensor(0xf1,0x00);
				SP2518_write_cmos_sensor(0xfd,0x01);
				SP2518_write_cmos_sensor(0x90,0x10);
				SP2518_write_cmos_sensor(0x92,0x01);
				SP2518_write_cmos_sensor(0x98,0x4e);
				SP2518_write_cmos_sensor(0x99,0x00);
				SP2518_write_cmos_sensor(0x9a,0x01);
				SP2518_write_cmos_sensor(0x9b,0x00);
				SP2518_write_cmos_sensor(0xfd,0x01);
				SP2518_write_cmos_sensor(0xce,0xe0);
				SP2518_write_cmos_sensor(0xcf,0x04);
				SP2518_write_cmos_sensor(0xd0,0xe0);
				SP2518_write_cmos_sensor(0xd1,0x04);
				SP2518_write_cmos_sensor(0xd7,0x4a);
				SP2518_write_cmos_sensor(0xd8,0x00);
				SP2518_write_cmos_sensor(0xd9,0x4e);
				SP2518_write_cmos_sensor(0xda,0x00);
				SP2518_write_cmos_sensor(0xfd,0x00);
#endif
			}
			else/*capture night mode 60hz*/
			{ 
				SENSORDB("Ronlus capture night mode 60hz\r\n");  
#ifdef SP2518_24M_72M		
				//X pll   fix 6fps  capture night mode 60hz                                              
				SP2518_write_cmos_sensor(0xfd , 0x00);
				SP2518_write_cmos_sensor(0x03 , 0x01);
				SP2518_write_cmos_sensor(0x04 , 0x86);
				SP2518_write_cmos_sensor(0x05 , 0x00);
				SP2518_write_cmos_sensor(0x06 , 0x00);
				SP2518_write_cmos_sensor(0x07 , 0x00);
				SP2518_write_cmos_sensor(0x08 , 0x00);
				SP2518_write_cmos_sensor(0x09 , 0x05);
				SP2518_write_cmos_sensor(0x0a , 0x66);
				SP2518_write_cmos_sensor(0xf0 , 0x41);
				SP2518_write_cmos_sensor(0xf1 , 0x00);
				SP2518_write_cmos_sensor(0xfd , 0x01);
				SP2518_write_cmos_sensor(0x90 , 0x14);
				SP2518_write_cmos_sensor(0x92 , 0x01);
				SP2518_write_cmos_sensor(0x98 , 0x41);
				SP2518_write_cmos_sensor(0x99 , 0x00);
				SP2518_write_cmos_sensor(0x9a , 0x01);
				SP2518_write_cmos_sensor(0x9b , 0x00);
				//Status                              
				SP2518_write_cmos_sensor(0xfd , 0x01);
				SP2518_write_cmos_sensor(0xce , 0x14);
				SP2518_write_cmos_sensor(0xcf , 0x05);
				SP2518_write_cmos_sensor(0xd0 , 0x14);
				SP2518_write_cmos_sensor(0xd1 , 0x05);
				SP2518_write_cmos_sensor(0xd7 , 0x43);
				SP2518_write_cmos_sensor(0xd8 , 0x00);
				SP2518_write_cmos_sensor(0xd9 , 0x47);
				SP2518_write_cmos_sensor(0xda , 0x00);
				SP2518_write_cmos_sensor(0xfd , 0x00);
#else
				//capture preview night 36M 60hz fix 6FPS maxgain
				SP2518_write_cmos_sensor(0xfd,0x00);
				SP2518_write_cmos_sensor(0x03,0x01);
				SP2518_write_cmos_sensor(0x04,0x86);
				SP2518_write_cmos_sensor(0x05,0x00);
				SP2518_write_cmos_sensor(0x06,0x00);
				SP2518_write_cmos_sensor(0x07,0x00);
				SP2518_write_cmos_sensor(0x08,0x00);
				SP2518_write_cmos_sensor(0x09,0x00);
				SP2518_write_cmos_sensor(0x0a,0xe4);
				//SP2518_write_cmos_sensor(0x2f,0x00);
				//SP2518_write_cmos_sensor(0x30,0x09);
				SP2518_write_cmos_sensor(0xf0,0x41);
				SP2518_write_cmos_sensor(0xf1,0x00);
				SP2518_write_cmos_sensor(0xfd,0x01);
				SP2518_write_cmos_sensor(0x90,0x14);
				SP2518_write_cmos_sensor(0x92,0x01);
				SP2518_write_cmos_sensor(0x98,0x41);
				SP2518_write_cmos_sensor(0x99,0x00);
				SP2518_write_cmos_sensor(0x9a,0x01);
				SP2518_write_cmos_sensor(0x9b,0x00);
				SP2518_write_cmos_sensor(0xfd,0x01);
				SP2518_write_cmos_sensor(0xce,0x14);
				SP2518_write_cmos_sensor(0xcf,0x05);
				SP2518_write_cmos_sensor(0xd0,0x14);
				SP2518_write_cmos_sensor(0xd1,0x05);
				SP2518_write_cmos_sensor(0xd7,0x3d);
				SP2518_write_cmos_sensor(0xd8,0x00);
				SP2518_write_cmos_sensor(0xd9,0x41);
				SP2518_write_cmos_sensor(0xda,0x00);
				SP2518_write_cmos_sensor(0xfd,0x00);
#endif
			}
		}
	}
	else /*normal mode settings*/
	{
		SENSORDB("Ronlus normal mode\r\n");
		SP2518_CAM_Nightmode = 0;
		SP2518_write_cmos_sensor(0xfd,0x00);
		SP2518_write_cmos_sensor(0xb2,SP2518_NORMAL_Y0ffset);
		if (SP2518_MPEG4_encode_mode == KAL_TRUE) 
		{
			SENSORDB("Ronlus video normal mode\r\n");
			if(SP2518_CAM_BANDING_50HZ == KAL_TRUE)/*video normal mode 50hz*/
			{
				SENSORDB("Ronlus video normal mode 50hz\r\n");

#ifdef SP2518_24M_72M
				//X pll   fix 13fps       video normal mode 50hz 
				SP2518_write_cmos_sensor(0xfd , 0x00);
				SP2518_write_cmos_sensor(0x03 , 0x03);
				SP2518_write_cmos_sensor(0x04 , 0xf6);
				SP2518_write_cmos_sensor(0x05 , 0x00);
				SP2518_write_cmos_sensor(0x06 , 0x00);
				SP2518_write_cmos_sensor(0x07 , 0x00);
				SP2518_write_cmos_sensor(0x08 , 0x00);
				SP2518_write_cmos_sensor(0x09 , 0x00);
				SP2518_write_cmos_sensor(0x0a , 0x8b);
				SP2518_write_cmos_sensor(0xf0 , 0xa9);
				SP2518_write_cmos_sensor(0xf1 , 0x00);
				SP2518_write_cmos_sensor(0xfd , 0x01);
				SP2518_write_cmos_sensor(0x90 , 0x07);
				SP2518_write_cmos_sensor(0x92 , 0x01);
				SP2518_write_cmos_sensor(0x98 , 0xa9);
				SP2518_write_cmos_sensor(0x99 , 0x00);
				SP2518_write_cmos_sensor(0x9a , 0x01);
				SP2518_write_cmos_sensor(0x9b , 0x00);
				//Status
				SP2518_write_cmos_sensor(0xfd , 0x01);
				SP2518_write_cmos_sensor(0xce , 0x9f);
				SP2518_write_cmos_sensor(0xcf , 0x04);
				SP2518_write_cmos_sensor(0xd0 , 0x9f);
				SP2518_write_cmos_sensor(0xd1 , 0x04);
				SP2518_write_cmos_sensor(0xd7 , 0xab);
				SP2518_write_cmos_sensor(0xd8 , 0x00);
				SP2518_write_cmos_sensor(0xd9 , 0xaf);
				SP2518_write_cmos_sensor(0xda , 0x00);
				SP2518_write_cmos_sensor(0xfd , 0x00);
#else

				//capture video daylight 36M 50hz fix 6FPS maxgain   
				SP2518_write_cmos_sensor(0xfd,0x00);
				SP2518_write_cmos_sensor(0x03,0x01);
				SP2518_write_cmos_sensor(0x04,0xd4);
				SP2518_write_cmos_sensor(0x05,0x00);
				SP2518_write_cmos_sensor(0x06,0x00);
				SP2518_write_cmos_sensor(0x07,0x00);
				SP2518_write_cmos_sensor(0x08,0x00);
				SP2518_write_cmos_sensor(0x09,0x00);
				SP2518_write_cmos_sensor(0x0a,0xe4);
				//SP2518_write_cmos_sensor(0x2f,0x00);
				//SP2518_write_cmos_sensor(0x30,0x09);
				SP2518_write_cmos_sensor(0xf0,0x4e);
				SP2518_write_cmos_sensor(0xf1,0x00);
				SP2518_write_cmos_sensor(0xfd,0x01);
				SP2518_write_cmos_sensor(0x90,0x10);
				SP2518_write_cmos_sensor(0x92,0x01);
				SP2518_write_cmos_sensor(0x98,0x4e);
				SP2518_write_cmos_sensor(0x99,0x00);
				SP2518_write_cmos_sensor(0x9a,0x01);
				SP2518_write_cmos_sensor(0x9b,0x00);
				SP2518_write_cmos_sensor(0xfd,0x01);
				SP2518_write_cmos_sensor(0xce,0xe0);
				SP2518_write_cmos_sensor(0xcf,0x04);
				SP2518_write_cmos_sensor(0xd0,0xe0);
				SP2518_write_cmos_sensor(0xd1,0x04);
				SP2518_write_cmos_sensor(0xd7,0x4a);
				SP2518_write_cmos_sensor(0xd8,0x00);
				SP2518_write_cmos_sensor(0xd9,0x4e);
				SP2518_write_cmos_sensor(0xda,0x00);
				SP2518_write_cmos_sensor(0xfd,0x00);				
#endif
			}
			else/*video normal mode 60hz*/
			{
				SENSORDB("Ronlus video normal mode 60hz\r\n");  

#ifdef SP2518_24M_72M
				//X pll   fix 13fps     video normal mode 60hz
				SP2518_write_cmos_sensor(0xfd , 0x00);
				SP2518_write_cmos_sensor(0x03 , 0x03);
				SP2518_write_cmos_sensor(0x04 , 0x4e);
				SP2518_write_cmos_sensor(0x05 , 0x00);
				SP2518_write_cmos_sensor(0x06 , 0x00);
				SP2518_write_cmos_sensor(0x07 , 0x00);
				SP2518_write_cmos_sensor(0x08 , 0x00);
				SP2518_write_cmos_sensor(0x09 , 0x00);
				SP2518_write_cmos_sensor(0x0a , 0x8a);
				SP2518_write_cmos_sensor(0xf0 , 0x8d);
				SP2518_write_cmos_sensor(0xf1 , 0x00);
				SP2518_write_cmos_sensor(0xfd , 0x01);
				SP2518_write_cmos_sensor(0x90 , 0x09);
				SP2518_write_cmos_sensor(0x92 , 0x01);
				SP2518_write_cmos_sensor(0x98 , 0x8d);
				SP2518_write_cmos_sensor(0x99 , 0x00);
				SP2518_write_cmos_sensor(0x9a , 0x01);
				SP2518_write_cmos_sensor(0x9b , 0x00);
				//Status
				SP2518_write_cmos_sensor(0xfd , 0x01);
				SP2518_write_cmos_sensor(0xce , 0xf5);
				SP2518_write_cmos_sensor(0xcf , 0x04);
				SP2518_write_cmos_sensor(0xd0 , 0xf5);
				SP2518_write_cmos_sensor(0xd1 , 0x04);
				SP2518_write_cmos_sensor(0xd7 , 0x8f);
				SP2518_write_cmos_sensor(0xd8 , 0x00);
				SP2518_write_cmos_sensor(0xd9 , 0x93);
				SP2518_write_cmos_sensor(0xda , 0x00);
				SP2518_write_cmos_sensor(0xfd , 0x00);	
#else
				//capture preview daylight 36M 60Hz fix 6FPS maxgain   
				SP2518_write_cmos_sensor(0xfd,0x00);
				SP2518_write_cmos_sensor(0x03,0x01);
				SP2518_write_cmos_sensor(0x04,0x86);
				SP2518_write_cmos_sensor(0x05,0x00);
				SP2518_write_cmos_sensor(0x06,0x00);
				SP2518_write_cmos_sensor(0x07,0x00);
				SP2518_write_cmos_sensor(0x08,0x00);
				SP2518_write_cmos_sensor(0x09,0x00);
				SP2518_write_cmos_sensor(0x0a,0xe4);
				//SP2518_write_cmos_sensor(0x2f,0x00);
				//SP2518_write_cmos_sensor(0x30,0x09);
				SP2518_write_cmos_sensor(0xf0,0x41);
				SP2518_write_cmos_sensor(0xf1,0x00);
				SP2518_write_cmos_sensor(0xfd,0x01);
				SP2518_write_cmos_sensor(0x90,0x14);
				SP2518_write_cmos_sensor(0x92,0x01);
				SP2518_write_cmos_sensor(0x98,0x41);
				SP2518_write_cmos_sensor(0x99,0x00);
				SP2518_write_cmos_sensor(0x9a,0x01);
				SP2518_write_cmos_sensor(0x9b,0x00);
				SP2518_write_cmos_sensor(0xfd,0x01);
				SP2518_write_cmos_sensor(0xce,0x14);
				SP2518_write_cmos_sensor(0xcf,0x05);
				SP2518_write_cmos_sensor(0xd0,0x14);
				SP2518_write_cmos_sensor(0xd1,0x05);
				SP2518_write_cmos_sensor(0xd7,0x3d);
				SP2518_write_cmos_sensor(0xd8,0x00);
				SP2518_write_cmos_sensor(0xd9,0x41);
				SP2518_write_cmos_sensor(0xda,0x00);
				SP2518_write_cmos_sensor(0xfd,0x00);
#endif
			}
		}
		else/*capture normal mode*/
		{
			SENSORDB("Ronlus capture normal mode\r\n");
			if(SP2518_CAM_BANDING_50HZ == KAL_TRUE)/*capture normal mode 50hz*/
			{
				SENSORDB("Ronlus capture normal mode 50hz\r\n");/*72M 8~13fps 50hz*/

#ifdef SP2518_24M_72M 
				//X pll   13~8fps       capture normal mode 50hz            
				SP2518_write_cmos_sensor(0xfd , 0x00);
				SP2518_write_cmos_sensor(0x03 , 0x03);
				SP2518_write_cmos_sensor(0x04 , 0xf6);
				SP2518_write_cmos_sensor(0x05 , 0x00);
				SP2518_write_cmos_sensor(0x06 , 0x00);
				SP2518_write_cmos_sensor(0x07 , 0x00);
				SP2518_write_cmos_sensor(0x08 , 0x00);
				SP2518_write_cmos_sensor(0x09 , 0x00);
				SP2518_write_cmos_sensor(0x0a , 0x8b);
				SP2518_write_cmos_sensor(0xf0 , 0xa9);
				SP2518_write_cmos_sensor(0xf1 , 0x00);
				SP2518_write_cmos_sensor(0xfd , 0x01);
				SP2518_write_cmos_sensor(0x90 , 0x0c);
				SP2518_write_cmos_sensor(0x92 , 0x01);
				SP2518_write_cmos_sensor(0x98 , 0xa9);
				SP2518_write_cmos_sensor(0x99 , 0x00);
				SP2518_write_cmos_sensor(0x9a , 0x01);
				SP2518_write_cmos_sensor(0x9b , 0x00);
				//Status
				SP2518_write_cmos_sensor(0xfd , 0x01);
				SP2518_write_cmos_sensor(0xce , 0xec);
				SP2518_write_cmos_sensor(0xcf , 0x07);
				SP2518_write_cmos_sensor(0xd0 , 0xec);
				SP2518_write_cmos_sensor(0xd1 , 0x07);
				SP2518_write_cmos_sensor(0xd7 , 0xab);
				SP2518_write_cmos_sensor(0xd8 , 0x00);
				SP2518_write_cmos_sensor(0xd9 , 0xaf);
				SP2518_write_cmos_sensor(0xda , 0x00);
				SP2518_write_cmos_sensor(0xfd , 0x00);
#else

				//capture preview daylight 36M 50hz fix 6FPS maxgain   
				SP2518_write_cmos_sensor(0xfd,0x00);
				SP2518_write_cmos_sensor(0x03,0x01);
				SP2518_write_cmos_sensor(0x04,0xd4);
				SP2518_write_cmos_sensor(0x05,0x00);
				SP2518_write_cmos_sensor(0x06,0x00);
				SP2518_write_cmos_sensor(0x07,0x00);
				SP2518_write_cmos_sensor(0x08,0x00);
				SP2518_write_cmos_sensor(0x09,0x00);
				SP2518_write_cmos_sensor(0x0a,0xe4);
				//SP2518_write_cmos_sensor(0x2f,0x00);
				//SP2518_write_cmos_sensor(0x30,0x09);
				SP2518_write_cmos_sensor(0xf0,0x4e);
				SP2518_write_cmos_sensor(0xf1,0x00);
				SP2518_write_cmos_sensor(0xfd,0x01);
				SP2518_write_cmos_sensor(0x90,0x10);
				SP2518_write_cmos_sensor(0x92,0x01);
				SP2518_write_cmos_sensor(0x98,0x4e);
				SP2518_write_cmos_sensor(0x99,0x00);
				SP2518_write_cmos_sensor(0x9a,0x01);
				SP2518_write_cmos_sensor(0x9b,0x00);
				SP2518_write_cmos_sensor(0xfd,0x01);
				SP2518_write_cmos_sensor(0xce,0xe0);
				SP2518_write_cmos_sensor(0xcf,0x04);
				SP2518_write_cmos_sensor(0xd0,0xe0);
				SP2518_write_cmos_sensor(0xd1,0x04);
				SP2518_write_cmos_sensor(0xd7,0x4a);
				SP2518_write_cmos_sensor(0xd8,0x00);
				SP2518_write_cmos_sensor(0xd9,0x4e);
				SP2518_write_cmos_sensor(0xda,0x00);
				SP2518_write_cmos_sensor(0xfd,0x00);
#endif
			}
			else/*video normal mode 60hz*/
			{
				SENSORDB("Ronlus capture normal mode 60hz\r\n"); /*72M 8~12fps 60hz*/

#ifdef SP2518_24M_72M

				//X pll   13~8fps       capture normal mode 60hz                 
				SP2518_write_cmos_sensor(0xfd , 0x00);
				SP2518_write_cmos_sensor(0x03 , 0x03);
				SP2518_write_cmos_sensor(0x04 , 0x4e);
				SP2518_write_cmos_sensor(0x05 , 0x00);
				SP2518_write_cmos_sensor(0x06 , 0x00);
				SP2518_write_cmos_sensor(0x07 , 0x00);
				SP2518_write_cmos_sensor(0x08 , 0x00);
				SP2518_write_cmos_sensor(0x09 , 0x00);
				SP2518_write_cmos_sensor(0x0a , 0x8a);
				SP2518_write_cmos_sensor(0xf0 , 0x8d);
				SP2518_write_cmos_sensor(0xf1 , 0x00);
				SP2518_write_cmos_sensor(0xfd , 0x01);
				SP2518_write_cmos_sensor(0x90 , 0x0f);
				SP2518_write_cmos_sensor(0x92 , 0x01);
				SP2518_write_cmos_sensor(0x98 , 0x8d);
				SP2518_write_cmos_sensor(0x99 , 0x00);
				SP2518_write_cmos_sensor(0x9a , 0x01);
				SP2518_write_cmos_sensor(0x9b , 0x00);
				//Status
				SP2518_write_cmos_sensor(0xfd , 0x01);
				SP2518_write_cmos_sensor(0xce , 0x43);
				SP2518_write_cmos_sensor(0xcf , 0x08);
				SP2518_write_cmos_sensor(0xd0 , 0x43);
				SP2518_write_cmos_sensor(0xd1 , 0x08);
				SP2518_write_cmos_sensor(0xd7 , 0x8f);
				SP2518_write_cmos_sensor(0xd8 , 0x00);
				SP2518_write_cmos_sensor(0xd9 , 0x93);
				SP2518_write_cmos_sensor(0xda , 0x00);
				SP2518_write_cmos_sensor(0xfd , 0x00);
#else
				//capture preview daylight 36M 60Hz fix 6FPS maxgain	 
				SP2518_write_cmos_sensor(0xfd,0x00);
				SP2518_write_cmos_sensor(0x03,0x01);
				SP2518_write_cmos_sensor(0x04,0x86);
				SP2518_write_cmos_sensor(0x05,0x00);
				SP2518_write_cmos_sensor(0x06,0x00);
				SP2518_write_cmos_sensor(0x07,0x00);
				SP2518_write_cmos_sensor(0x08,0x00);
				SP2518_write_cmos_sensor(0x09,0x00);
				SP2518_write_cmos_sensor(0x0a,0xe4);
				//SP2518_write_cmos_sensor(0x2f,0x00);
				//SP2518_write_cmos_sensor(0x30,0x09);
				SP2518_write_cmos_sensor(0xf0,0x41);
				SP2518_write_cmos_sensor(0xf1,0x00);
				SP2518_write_cmos_sensor(0xfd,0x01);
				SP2518_write_cmos_sensor(0x90,0x14);
				SP2518_write_cmos_sensor(0x92,0x01);
				SP2518_write_cmos_sensor(0x98,0x41);
				SP2518_write_cmos_sensor(0x99,0x00);
				SP2518_write_cmos_sensor(0x9a,0x01);
				SP2518_write_cmos_sensor(0x9b,0x00);
				SP2518_write_cmos_sensor(0xfd,0x01);
				SP2518_write_cmos_sensor(0xce,0x14);
				SP2518_write_cmos_sensor(0xcf,0x05);
				SP2518_write_cmos_sensor(0xd0,0x14);
				SP2518_write_cmos_sensor(0xd1,0x05);
				SP2518_write_cmos_sensor(0xd7,0x3d);
				SP2518_write_cmos_sensor(0xd8,0x00);
				SP2518_write_cmos_sensor(0xd9,0x41);
				SP2518_write_cmos_sensor(0xda,0x00);
				SP2518_write_cmos_sensor(0xfd,0x00);
#endif
			}
		}
	}
#endif
} /* SP2518_NightMode */


/*************************************************************************
 * FUNCTION
 *	SP2518_Sensor_Init
 *
 * DESCRIPTION
 *	This function apply all of the initial setting to sensor.
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 *************************************************************************/

void SP2518_Sensor_Init(void)
{
	SP2518_write_cmos_sensor(0xfd,0x00);
	SP2518_write_cmos_sensor(0x1b,0x0a);//maximum drv ability //modify to 0x1a if meet SD card lines issue xixi // 20131225
	SP2518_write_cmos_sensor(0x0e,0x01);
	SP2518_write_cmos_sensor(0x0f,0x2f);
	SP2518_write_cmos_sensor(0x10,0x2e);
	SP2518_write_cmos_sensor(0x11,0x00);
	SP2518_write_cmos_sensor(0x12,0x4f);
	SP2518_write_cmos_sensor(0x14,0x20);//0x40 moaidy by sp_yjp,20130821
	SP2518_write_cmos_sensor(0x16,0x02);
	SP2518_write_cmos_sensor(0x17,0x10);
	SP2518_write_cmos_sensor(0x1a,0x1f);
	SP2518_write_cmos_sensor(0x1e,0x81);
	SP2518_write_cmos_sensor(0x21,0x00);
	SP2518_write_cmos_sensor(0x22,0x1b);
	SP2518_write_cmos_sensor(0x25,0x10);
	SP2518_write_cmos_sensor(0x26,0x25);
	SP2518_write_cmos_sensor(0x27,0x6d);
	SP2518_write_cmos_sensor(0x2c,0x23);// 31 Ronlus remove balck dot0x45);
	SP2518_write_cmos_sensor(0x2d,0x75);
	SP2518_write_cmos_sensor(0x2e,0x38);//sxga 0x18
	SP2518_write_cmos_sensor(0x31,0x10);//mirror upside down//0x00 20131225
	SP2518_write_cmos_sensor(0x44,0x03);
	SP2518_write_cmos_sensor(0x6f,0x00);
	SP2518_write_cmos_sensor(0xa0,0x04);
	SP2518_write_cmos_sensor(0x5f,0x01);
	SP2518_write_cmos_sensor(0x32,0x00);
	SP2518_write_cmos_sensor(0xfd,0x01);
	SP2518_write_cmos_sensor(0x2c,0x00);
	SP2518_write_cmos_sensor(0x2d,0x00);
	SP2518_write_cmos_sensor(0xfd,0x00);
	SP2518_write_cmos_sensor(0xfb,0x83);
	SP2518_write_cmos_sensor(0xf4,0x09);
	//Pregain
	SP2518_write_cmos_sensor(0xfd,0x01);
	SP2518_write_cmos_sensor(0xc6,0x90);
	SP2518_write_cmos_sensor(0xc7,0x90);
	SP2518_write_cmos_sensor(0xc8,0x90);
	SP2518_write_cmos_sensor(0xc9,0x90);
	//blacklevel
	SP2518_write_cmos_sensor(0xfd,0x00);
	SP2518_write_cmos_sensor(0x65,0x08);
	SP2518_write_cmos_sensor(0x66,0x08);
	SP2518_write_cmos_sensor(0x67,0x08);
	SP2518_write_cmos_sensor(0x68,0x08);

	//bpc
	SP2518_write_cmos_sensor(0x46,0xff);
	//rpc
	SP2518_write_cmos_sensor(0xfd,0x00);
	SP2518_write_cmos_sensor(0xe0,0x6c);
	SP2518_write_cmos_sensor(0xe1,0x54);
	SP2518_write_cmos_sensor(0xe2,0x48);
	SP2518_write_cmos_sensor(0xe3,0x40);
	SP2518_write_cmos_sensor(0xe4,0x40);
	SP2518_write_cmos_sensor(0xe5,0x3e);
	SP2518_write_cmos_sensor(0xe6,0x3e);
	SP2518_write_cmos_sensor(0xe8,0x3a);
	SP2518_write_cmos_sensor(0xe9,0x3a);
	SP2518_write_cmos_sensor(0xea,0x3a);
	SP2518_write_cmos_sensor(0xeb,0x38);
	SP2518_write_cmos_sensor(0xf5,0x38);
	SP2518_write_cmos_sensor(0xf6,0x38);
	SP2518_write_cmos_sensor(0xfd,0x01);
	
	SP2518_write_cmos_sensor(0x94,0x90);	//0xc0 modify by sp_yjp,20130821
	SP2518_write_cmos_sensor(0x95,0x38);
	SP2518_write_cmos_sensor(0x9c,0x60);	//0x6c  modify by sp_yjp,20130821
	
	SP2518_write_cmos_sensor(0x9d,0x38);	
    // xi modify   //  original: #if 0///def SP2518_24M_72M
#ifdef SP2518_24M_72M
	/*24*3pll 8~13fps 50hz*/
	SP2518_write_cmos_sensor(0xfd , 0x00);
	SP2518_write_cmos_sensor(0x03 , 0x03);
	SP2518_write_cmos_sensor(0x04 , 0xf6);
	SP2518_write_cmos_sensor(0x05 , 0x00);
	SP2518_write_cmos_sensor(0x06 , 0x00);
	SP2518_write_cmos_sensor(0x07 , 0x00);
	SP2518_write_cmos_sensor(0x08 , 0x00);
	SP2518_write_cmos_sensor(0x09 , 0x00);
	SP2518_write_cmos_sensor(0x0a , 0x8b);
	SP2518_write_cmos_sensor(0x2f , 0x00);
	SP2518_write_cmos_sensor(0x30 , 0x08);
	SP2518_write_cmos_sensor(0xf0 , 0xa9);
	SP2518_write_cmos_sensor(0xf1 , 0x00);
	SP2518_write_cmos_sensor(0xfd , 0x01);
	SP2518_write_cmos_sensor(0x90 , 0x0c);
	SP2518_write_cmos_sensor(0x92 , 0x01);
	SP2518_write_cmos_sensor(0x98 , 0xa9);
	SP2518_write_cmos_sensor(0x99 , 0x00);
	SP2518_write_cmos_sensor(0x9a , 0x01);
	SP2518_write_cmos_sensor(0x9b , 0x00);

	//Status
	SP2518_write_cmos_sensor(0xfd , 0x01);
	SP2518_write_cmos_sensor(0xce , 0xec);
	SP2518_write_cmos_sensor(0xcf , 0x07);
	SP2518_write_cmos_sensor(0xd0 , 0xec);
	SP2518_write_cmos_sensor(0xd1 , 0x07);
	SP2518_write_cmos_sensor(0xd7 , 0xab);
	SP2518_write_cmos_sensor(0xd8 , 0x00);
	SP2518_write_cmos_sensor(0xd9 , 0xaf);
	SP2518_write_cmos_sensor(0xda , 0x00);
	SP2518_write_cmos_sensor(0xfd , 0x00);

	SP2518_write_cmos_sensor(0xfd,0x01);
	SP2518_write_cmos_sensor(0xca,0x30);//mean dummy2low
	SP2518_write_cmos_sensor(0xcb,0x50);//mean low2dummy
	SP2518_write_cmos_sensor(0xcc,0xc0);//f8;rpc low
	SP2518_write_cmos_sensor(0xcd,0xc0);//rpc dummy
	SP2518_write_cmos_sensor(0xd5,0x80);//mean normal2dummy
	SP2518_write_cmos_sensor(0xd6,0x90);//mean dummy2normal
	SP2518_write_cmos_sensor(0xfd,0x00);  

#else



#if 1		   
	//SI50_SP2518 UXGA 24MEclk 1.5PLL 1DIV 50Hz fix 6fps
	//ae setting
	SP2518_write_cmos_sensor(0xfd,0x00);
	SP2518_write_cmos_sensor(0x03,0x01);
	SP2518_write_cmos_sensor(0x04,0xd4);
	SP2518_write_cmos_sensor(0x05,0x00);
	SP2518_write_cmos_sensor(0x06,0x00);
	SP2518_write_cmos_sensor(0x07,0x00);
	SP2518_write_cmos_sensor(0x08,0x00);
	SP2518_write_cmos_sensor(0x09,0x00);
	SP2518_write_cmos_sensor(0x0a,0xe4);
	SP2518_write_cmos_sensor(0x2f,0x00);
	SP2518_write_cmos_sensor(0x30,0x09);
	SP2518_write_cmos_sensor(0xf0,0x4e);
	SP2518_write_cmos_sensor(0xf1,0x00);
	SP2518_write_cmos_sensor(0xfd,0x01);
	SP2518_write_cmos_sensor(0x90,0x10);
	SP2518_write_cmos_sensor(0x92,0x01);
	SP2518_write_cmos_sensor(0x98,0x4e);
	SP2518_write_cmos_sensor(0x99,0x00);
	SP2518_write_cmos_sensor(0x9a,0x01);
	SP2518_write_cmos_sensor(0x9b,0x00);
	SP2518_write_cmos_sensor(0xfd,0x01);
	SP2518_write_cmos_sensor(0xce,0xe0);
	SP2518_write_cmos_sensor(0xcf,0x04);
	SP2518_write_cmos_sensor(0xd0,0xe0);
	SP2518_write_cmos_sensor(0xd1,0x04);
	SP2518_write_cmos_sensor(0xd7,0x4a);
	SP2518_write_cmos_sensor(0xd8,0x00);
	SP2518_write_cmos_sensor(0xd9,0x4e);
	SP2518_write_cmos_sensor(0xda,0x00);
	SP2518_write_cmos_sensor(0xfd,0x00);	  
#endif

#endif

	//lens shading for \810\879\810\857\810\880\810\848979C-171A\181A
	SP2518_write_cmos_sensor(0xfd,0x00);
	SP2518_write_cmos_sensor(0xa1,0x20);
	SP2518_write_cmos_sensor(0xa2,0x20);
	SP2518_write_cmos_sensor(0xa3,0x20);
	SP2518_write_cmos_sensor(0xa4,0xff);
	SP2518_write_cmos_sensor(0xa5,0x80);
	SP2518_write_cmos_sensor(0xa6,0x80);
	SP2518_write_cmos_sensor(0xfd,0x01);
	SP2518_write_cmos_sensor(0x64,0x1e);//28
	SP2518_write_cmos_sensor(0x65,0x1c);//25
	SP2518_write_cmos_sensor(0x66,0x1c);//2a
	SP2518_write_cmos_sensor(0x67,0x16);//25
	SP2518_write_cmos_sensor(0x68,0x1c);//25
	SP2518_write_cmos_sensor(0x69,0x1c);//29
	SP2518_write_cmos_sensor(0x6a,0x1a);//28
	SP2518_write_cmos_sensor(0x6b,0x16);//20
	SP2518_write_cmos_sensor(0x6c,0x1a);//22
	SP2518_write_cmos_sensor(0x6d,0x1a);//22
	SP2518_write_cmos_sensor(0x6e,0x1a);//22
	SP2518_write_cmos_sensor(0x6f,0x16);//1c
	SP2518_write_cmos_sensor(0xb8,0x04);//0a
	SP2518_write_cmos_sensor(0xb9,0x13);//0a
	SP2518_write_cmos_sensor(0xba,0x00);//23
	SP2518_write_cmos_sensor(0xbb,0x03);//14
	SP2518_write_cmos_sensor(0xbc,0x03);//08
	SP2518_write_cmos_sensor(0xbd,0x11);//08
	SP2518_write_cmos_sensor(0xbe,0x00);//12
	SP2518_write_cmos_sensor(0xbf,0x02);//00
	SP2518_write_cmos_sensor(0xc0,0x04);//05
	SP2518_write_cmos_sensor(0xc1,0x0e);//05
	SP2518_write_cmos_sensor(0xc2,0x00);//18
	SP2518_write_cmos_sensor(0xc3,0x05);//08   
	//raw filter
	SP2518_write_cmos_sensor(0xfd,0x01);
	SP2518_write_cmos_sensor(0xde,0x0f);
	SP2518_write_cmos_sensor(0xfd,0x00);

	#if 1	//modify by sp_yjp,20130821
	SP2518_write_cmos_sensor(0x57,0x03);//raw_dif_thr
	SP2518_write_cmos_sensor(0x58,0x05);//a
	SP2518_write_cmos_sensor(0x56,0x06);//a
	SP2518_write_cmos_sensor(0x59,0x0e);

	//Gr\810\843\810\844Gb\810\881\A1\A7\810\858\810\868\810\8A0\810\884\810\890\810\858
	SP2518_write_cmos_sensor(0xfd,0x01);
	
	SP2518_write_cmos_sensor(0x50,0x06);//raw_grgb_thr
	SP2518_write_cmos_sensor(0x51,0x08);
	SP2518_write_cmos_sensor(0x52,0x0a);
	SP2518_write_cmos_sensor(0x53,0x0f);
	
	SP2518_write_cmos_sensor(0xfd,0x00);	
	
	//R\B\810\881\A1\A7\810\858\810\868\810\864\810\8A1\810\874\810\865\810\863\810\851
	SP2518_write_cmos_sensor(0x5a,0xe0);//raw_rb_fac_outdoor
	SP2518_write_cmos_sensor(0xc4,0xb0);//60raw_rb_fac_indoor
	SP2518_write_cmos_sensor(0x43,0x80);//40raw_rb_fac_dummy  
	SP2518_write_cmos_sensor(0xad,0x40);//raw_rb_fac_low  
	//Gr\810\843\810\844Gb \810\881\A1\A7\810\858\810\868\810\872\810\893\810\855\810\867\810\874\810\865\810\863\810\851
	SP2518_write_cmos_sensor(0x4f,0xe0);//raw_gf_fac_outdoor
	SP2518_write_cmos_sensor(0xc3,0xb0);//60raw_gf_fac_indoor
	SP2518_write_cmos_sensor(0x3f,0x80);//40raw_gf_fac_dummy
	SP2518_write_cmos_sensor(0x42,0x40);//raw_gf_fac_low
	
	SP2518_write_cmos_sensor(0xc2,0x15);
	//Gr\810\843\810\844Gb\810\881\A1\A7\810\858\810\868\810\864\810\8A1\810\874\810\865\810\863\810\851
	SP2518_write_cmos_sensor(0xb6,0xb0);//raw_gflt_fac_outdoor
	SP2518_write_cmos_sensor(0xb7,0xa0);//60raw_gflt_fac_normal
	SP2518_write_cmos_sensor(0xb8,0x80);//40raw_gflt_fac_dummy
	SP2518_write_cmos_sensor(0xb9,0x40);//raw_gflt_fac_low
	#else
	SP2518_write_cmos_sensor(0x57,0x08);//raw_dif_thr
	SP2518_write_cmos_sensor(0x58,0x08);//a
	SP2518_write_cmos_sensor(0x56,0x08);//a
	SP2518_write_cmos_sensor(0x59,0x10);

	//Gr\810\843\810\844Gb\810\881\A1\A7\810\858\810\868\810\8A0\810\884\810\890\810\858
	SP2518_write_cmos_sensor(0xfd,0x01);
	SP2518_write_cmos_sensor(0x50,0x0c);//raw_grgb_thr
	SP2518_write_cmos_sensor(0x51,0x0c);
	SP2518_write_cmos_sensor(0x52,0x10);
	SP2518_write_cmos_sensor(0x53,0x10);
	SP2518_write_cmos_sensor(0xfd,0x00);	
	
	//R\B\810\881\A1\A7\810\858\810\868\810\864\810\8A1\810\874\810\865\810\863\810\851
	SP2518_write_cmos_sensor(0x5a,0xa0);//raw_rb_fac_outdoor
	SP2518_write_cmos_sensor(0xc4,0xa0);//60raw_rb_fac_indoor
	SP2518_write_cmos_sensor(0x43,0xa0);//40raw_rb_fac_dummy  
	SP2518_write_cmos_sensor(0xad,0x40);//raw_rb_fac_low  
	//Gr\810\843\810\844Gb \810\881\A1\A7\810\858\810\868\810\872\810\893\810\855\810\867\810\874\810\865\810\863\810\851
	SP2518_write_cmos_sensor(0x4f,0xa0);//raw_gf_fac_outdoor
	SP2518_write_cmos_sensor(0xc3,0xa0);//60raw_gf_fac_indoor
	SP2518_write_cmos_sensor(0x3f,0xa0);//40raw_gf_fac_dummy
	SP2518_write_cmos_sensor(0x42,0x40);//raw_gf_fac_low
	SP2518_write_cmos_sensor(0xc2,0x15);
	//Gr\810\843\810\844Gb\810\881\A1\A7\810\858\810\868\810\864\810\8A1\810\874\810\865\810\863\810\851
	SP2518_write_cmos_sensor(0xb6,0x80);//raw_gflt_fac_outdoor
	SP2518_write_cmos_sensor(0xb7,0x80);//60raw_gflt_fac_normal
	SP2518_write_cmos_sensor(0xb8,0x40);//40raw_gflt_fac_dummy
	SP2518_write_cmos_sensor(0xb9,0x20);//raw_gflt_fac_low
	#endif
	// awb1
	SP2518_write_cmos_sensor(0xfd,0x01);
	SP2518_write_cmos_sensor(0x11,0x10);
	SP2518_write_cmos_sensor(0x12,0x1f);
	SP2518_write_cmos_sensor(0x16,0x1c);
	SP2518_write_cmos_sensor(0x18,0x00);
	SP2518_write_cmos_sensor(0x19,0x00);
	SP2518_write_cmos_sensor(0x1b,0x96);
	SP2518_write_cmos_sensor(0x1a,0x9a);//95
	SP2518_write_cmos_sensor(0x1e,0x2f);
	SP2518_write_cmos_sensor(0x1f,0x29);
	SP2518_write_cmos_sensor(0x20,0xff);
	SP2518_write_cmos_sensor(0x22,0xff);  
	SP2518_write_cmos_sensor(0x28,0xce);
	SP2518_write_cmos_sensor(0x29,0x8a);
	SP2518_write_cmos_sensor(0xfd,0x00);
	SP2518_write_cmos_sensor(0xe7,0x03);
	SP2518_write_cmos_sensor(0xe7,0x00);
	SP2518_write_cmos_sensor(0xfd,0x01);
	SP2518_write_cmos_sensor(0x2a,0xf0);
	SP2518_write_cmos_sensor(0x2b,0x10);
	SP2518_write_cmos_sensor(0x2e,0x04);
	SP2518_write_cmos_sensor(0x2f,0x18);
	SP2518_write_cmos_sensor(0x21,0x60);
	SP2518_write_cmos_sensor(0x23,0x60);
	SP2518_write_cmos_sensor(0x8b,0xab);
	SP2518_write_cmos_sensor(0x8f,0x12);
	//awb2
	SP2518_write_cmos_sensor(0xfd,0x01);
	SP2518_write_cmos_sensor(0x1a,0x80);
	SP2518_write_cmos_sensor(0x1b,0x80);
	SP2518_write_cmos_sensor(0x43,0x80);
#if 0
	//outdoor
	SP2518_write_cmos_sensor(0x00,0xd4);
	SP2518_write_cmos_sensor(0x01,0xb0);
	SP2518_write_cmos_sensor(0x02,0x90);
	SP2518_write_cmos_sensor(0x03,0x78);
#endif
	//d65
	SP2518_write_cmos_sensor(0x35,0xd6);//d6;b0
	SP2518_write_cmos_sensor(0x36,0xf0);//f0;d1;e9
	SP2518_write_cmos_sensor(0x37,0x7a);//8a;70
	SP2518_write_cmos_sensor(0x38,0x9a);//dc;9a;af
	//indoor
	SP2518_write_cmos_sensor(0x39,0xab);
	SP2518_write_cmos_sensor(0x3a,0xca);
	SP2518_write_cmos_sensor(0x3b,0xa3);
	SP2518_write_cmos_sensor(0x3c,0xc1);
	//f
	SP2518_write_cmos_sensor(0x31,0x82);//7d
	SP2518_write_cmos_sensor(0x32,0xa5);//a0;74
	SP2518_write_cmos_sensor(0x33,0xd6);//d2
	SP2518_write_cmos_sensor(0x34,0xec);//e8
	SP2518_write_cmos_sensor(0x3d,0xa5);//a7;88
	SP2518_write_cmos_sensor(0x3e,0xc2);//be;bb
	SP2518_write_cmos_sensor(0x3f,0xa7);//b3;ad
	SP2518_write_cmos_sensor(0x40,0xc5);//c5;d0
	//Color Correction				  
	SP2518_write_cmos_sensor(0xfd,0x01);
	SP2518_write_cmos_sensor(0x1c,0xc0);
	SP2518_write_cmos_sensor(0x1d,0x95);
	SP2518_write_cmos_sensor(0xa0,0xa6);//b8 
	SP2518_write_cmos_sensor(0xa1,0xda);//;d5
	SP2518_write_cmos_sensor(0xa2,0x00);//;f2
	SP2518_write_cmos_sensor(0xa3,0x06);//;e8
	SP2518_write_cmos_sensor(0xa4,0xb2);//;95
	SP2518_write_cmos_sensor(0xa5,0xc7);//;03
	SP2518_write_cmos_sensor(0xa6,0x00);//;f2
	SP2518_write_cmos_sensor(0xa7,0xce);//;c4
	SP2518_write_cmos_sensor(0xa8,0xb2);//;ca
	SP2518_write_cmos_sensor(0xa9,0x0c);//;3c
	SP2518_write_cmos_sensor(0xaa,0x30);//;03
	SP2518_write_cmos_sensor(0xab,0x0c);//;0f
	SP2518_write_cmos_sensor(0xac,0xc0);//b8 
	SP2518_write_cmos_sensor(0xad,0xc0);//d5
	SP2518_write_cmos_sensor(0xae,0x00);//f2
	SP2518_write_cmos_sensor(0xaf,0xf2);//e8
	SP2518_write_cmos_sensor(0xb0,0xa6);//95
	SP2518_write_cmos_sensor(0xb1,0xe8);//03
	SP2518_write_cmos_sensor(0xb2,0x00);//f2
	SP2518_write_cmos_sensor(0xb3,0xe7);//c4
	SP2518_write_cmos_sensor(0xb4,0x99);//ca
	SP2518_write_cmos_sensor(0xb5,0x0c);//3c
	SP2518_write_cmos_sensor(0xb6,0x33);//03
	SP2518_write_cmos_sensor(0xb7,0x0c);//0f
	//Saturation
	SP2518_write_cmos_sensor(0xfd,0x00);
	SP2518_write_cmos_sensor(0xbf,0x01);
	SP2518_write_cmos_sensor(0xbe,0xbb);
	SP2518_write_cmos_sensor(0xc0,0xb0);
	SP2518_write_cmos_sensor(0xc1,0xf0);

	#if 1	//modify by sp_yjp,20130822
	SP2518_write_cmos_sensor(0xd3,0x7c); //outdoor
	SP2518_write_cmos_sensor(0xd4,0x78); //indoor
	SP2518_write_cmos_sensor(0xd6,0x70); //dummy
	SP2518_write_cmos_sensor(0xd7,0x60); //lowlight
	
	SP2518_write_cmos_sensor(0xd8,0x7c); //outdoor
	SP2518_write_cmos_sensor(0xd9,0x78); //indoor
	SP2518_write_cmos_sensor(0xda,0x70); //dummy
	SP2518_write_cmos_sensor(0xdb,0x60); //lowlight
	#else
	SP2518_write_cmos_sensor(0xd3,0x77);
	SP2518_write_cmos_sensor(0xd4,0x77);
	SP2518_write_cmos_sensor(0xd6,0x77);
	SP2518_write_cmos_sensor(0xd7,0x77);
	SP2518_write_cmos_sensor(0xd8,0x77);
	SP2518_write_cmos_sensor(0xd9,0x77);
	SP2518_write_cmos_sensor(0xda,0x77);
	SP2518_write_cmos_sensor(0xdb,0x77);
	#endif
	//uv_dif
	SP2518_write_cmos_sensor(0xfd,0x00);
	SP2518_write_cmos_sensor(0xf3,0x03);
	SP2518_write_cmos_sensor(0xb0,0x00);
	SP2518_write_cmos_sensor(0xb1,0x23);
	#if 0
	//gamma1
		SP2518_write_cmos_sensor(0xfd,0x00);//
		SP2518_write_cmos_sensor(0x8b,0x0 );//0 ;0	
		SP2518_write_cmos_sensor(0x8c,0xA );//14;A 
		SP2518_write_cmos_sensor(0x8d,0x13);//24;13
		SP2518_write_cmos_sensor(0x8e,0x25);//3a;25
		SP2518_write_cmos_sensor(0x8f,0x43);//59;43
		SP2518_write_cmos_sensor(0x90,0x5D);//6f;5D
		SP2518_write_cmos_sensor(0x91,0x74);//84;74
		SP2518_write_cmos_sensor(0x92,0x88);//95;88
		SP2518_write_cmos_sensor(0x93,0x9A);//a3;9A
		SP2518_write_cmos_sensor(0x94,0xA9);//b1;A9
		SP2518_write_cmos_sensor(0x95,0xB5);//be;B5
		SP2518_write_cmos_sensor(0x96,0xC0);//c7;C0
		SP2518_write_cmos_sensor(0x97,0xCA);//d1;CA
		SP2518_write_cmos_sensor(0x98,0xD4);//d9;D4
		SP2518_write_cmos_sensor(0x99,0xDD);//e1;DD
		SP2518_write_cmos_sensor(0x9a,0xE6);//e9;E6
		SP2518_write_cmos_sensor(0x9b,0xEF);//f1;EF
		SP2518_write_cmos_sensor(0xfd,0x01);//01;01
		SP2518_write_cmos_sensor(0x8d,0xF7);//f9;F7
		SP2518_write_cmos_sensor(0x8e,0xFF);//ff;FF
	//gamma2   
	SP2518_write_cmos_sensor(0xfd,0x00);//
	SP2518_write_cmos_sensor(0x78,0x0 );//0   
	SP2518_write_cmos_sensor(0x79,0xA );//14
	SP2518_write_cmos_sensor(0x7a,0x13);//24
	SP2518_write_cmos_sensor(0x7b,0x25);//3a
	SP2518_write_cmos_sensor(0x7c,0x43);//59
	SP2518_write_cmos_sensor(0x7d,0x5D);//6f
	SP2518_write_cmos_sensor(0x7e,0x74);//84
	SP2518_write_cmos_sensor(0x7f,0x88);//95
	SP2518_write_cmos_sensor(0x80,0x9A);//a3
	SP2518_write_cmos_sensor(0x81,0xA9);//b1
	SP2518_write_cmos_sensor(0x82,0xB5);//be
	SP2518_write_cmos_sensor(0x83,0xC0);//c7
	SP2518_write_cmos_sensor(0x84,0xCA);//d1
	SP2518_write_cmos_sensor(0x85,0xD4);//d9
	SP2518_write_cmos_sensor(0x86,0xDD);//e1
	SP2518_write_cmos_sensor(0x87,0xE6);//e9
	SP2518_write_cmos_sensor(0x88,0xEF);//f1
	SP2518_write_cmos_sensor(0x89,0xF7);//f9
	SP2518_write_cmos_sensor(0x8a,0xFF);//ff
	#endif
	/*/\810\861\810\899\A1\E3\810\898\810\861\810\8B5\810\859\810\876\810\862\810\871
	//gamma1
	SP2518_write_cmos_sensor(0xfd,0x00);
	SP2518_write_cmos_sensor(0x8b,0x00);
	SP2518_write_cmos_sensor(0x8c,0x14);
	SP2518_write_cmos_sensor(0x8d,0x24);
	SP2518_write_cmos_sensor(0x8e,0x3A);
	SP2518_write_cmos_sensor(0x8f,0x59);
	SP2518_write_cmos_sensor(0x90,0x70);
	SP2518_write_cmos_sensor(0x91,0x85);
	SP2518_write_cmos_sensor(0x92,0x96);
	SP2518_write_cmos_sensor(0x93,0xA6);
	SP2518_write_cmos_sensor(0x94,0xB3);
	SP2518_write_cmos_sensor(0x95,0xBE);
	SP2518_write_cmos_sensor(0x96,0xC9);
	SP2518_write_cmos_sensor(0x97,0xD2);
	SP2518_write_cmos_sensor(0x98,0xDB);
	SP2518_write_cmos_sensor(0x99,0xE3);
	SP2518_write_cmos_sensor(0x9a,0xEB);
	SP2518_write_cmos_sensor(0x9b,0xF2);
	SP2518_write_cmos_sensor(0xfd,0x01);
	SP2518_write_cmos_sensor(0x8d,0xF9);
	SP2518_write_cmos_sensor(0x8e,0xFF);
	//gamma2                   
	SP2518_write_cmos_sensor(0xfd,0x00);
	SP2518_write_cmos_sensor(0x78,0x00);
	SP2518_write_cmos_sensor(0x79,0x14);
	SP2518_write_cmos_sensor(0x7a,0x24);
	SP2518_write_cmos_sensor(0x7b,0x3A);
	SP2518_write_cmos_sensor(0x7c,0x59);
	SP2518_write_cmos_sensor(0x7d,0x70);
	SP2518_write_cmos_sensor(0x7e,0x85);
	SP2518_write_cmos_sensor(0x7f,0x96);
	SP2518_write_cmos_sensor(0x80,0xA6);
	SP2518_write_cmos_sensor(0x81,0xB3);
	SP2518_write_cmos_sensor(0x82,0xBE);
	SP2518_write_cmos_sensor(0x83,0xC9);
	SP2518_write_cmos_sensor(0x84,0xD2);
	SP2518_write_cmos_sensor(0x85,0xDB);
	SP2518_write_cmos_sensor(0x86,0xE3);
	SP2518_write_cmos_sensor(0x87,0xEB);
	SP2518_write_cmos_sensor(0x88,0xF2);
	SP2518_write_cmos_sensor(0x89,0xF9);
	SP2518_write_cmos_sensor(0x8a,0xFF);  */
	#if 1
	//gamma1
		SP2518_write_cmos_sensor(0xfd,0x00);//
		SP2518_write_cmos_sensor(0x8b,0x0 );//0 ;0	
		SP2518_write_cmos_sensor(0x8c,0x14);//14;A 
		SP2518_write_cmos_sensor(0x8d,0x22);//24;13
		SP2518_write_cmos_sensor(0x8e,0x36);//3a;25
		SP2518_write_cmos_sensor(0x8f,0x51);//59;43
		SP2518_write_cmos_sensor(0x90,0x67);//6f;5D
		SP2518_write_cmos_sensor(0x91,0x7a);//84;74
		SP2518_write_cmos_sensor(0x92,0x8c);//95;88
		SP2518_write_cmos_sensor(0x93,0x9b);//a3;9A
		SP2518_write_cmos_sensor(0x94,0xA9);//b1;A9
		SP2518_write_cmos_sensor(0x95,0xB5);//be;B5
		SP2518_write_cmos_sensor(0x96,0xC0);//c7;C0
		SP2518_write_cmos_sensor(0x97,0xCA);//d1;CA
		SP2518_write_cmos_sensor(0x98,0xD4);//d9;D4
		SP2518_write_cmos_sensor(0x99,0xDD);//e1;DD
		SP2518_write_cmos_sensor(0x9a,0xE6);//e9;E6
		SP2518_write_cmos_sensor(0x9b,0xEF);//f1;EF
		SP2518_write_cmos_sensor(0xfd,0x01);//01;01
		SP2518_write_cmos_sensor(0x8d,0xF7);//f9;F7
		SP2518_write_cmos_sensor(0x8e,0xFF);//ff;FF
	//gamma2   
	SP2518_write_cmos_sensor(0xfd,0x00);//
	SP2518_write_cmos_sensor(0x78,0x0 );//0   
	SP2518_write_cmos_sensor(0x79,0x14);//14
	SP2518_write_cmos_sensor(0x7a,0x22);//24
	SP2518_write_cmos_sensor(0x7b,0x36);//3a
	SP2518_write_cmos_sensor(0x7c,0x51);//59
	SP2518_write_cmos_sensor(0x7d,0x67);//6f
	SP2518_write_cmos_sensor(0x7e,0x7a);//84
	SP2518_write_cmos_sensor(0x7f,0x8c);//95
	SP2518_write_cmos_sensor(0x80,0x9b);//a3
	SP2518_write_cmos_sensor(0x81,0xA9);//b1
	SP2518_write_cmos_sensor(0x82,0xB5);//be
	SP2518_write_cmos_sensor(0x83,0xC0);//c7
	SP2518_write_cmos_sensor(0x84,0xCA);//d1
	SP2518_write_cmos_sensor(0x85,0xD4);//d9
	SP2518_write_cmos_sensor(0x86,0xDD);//e1
	SP2518_write_cmos_sensor(0x87,0xE6);//e9
	SP2518_write_cmos_sensor(0x88,0xEF);//f1
	SP2518_write_cmos_sensor(0x89,0xF7);//f9
	SP2518_write_cmos_sensor(0x8a,0xFF);//ff
	#endif

	//gamma_ae  
	SP2518_write_cmos_sensor(0xfd,0x01);
	SP2518_write_cmos_sensor(0x96,0x46);
	SP2518_write_cmos_sensor(0x97,0x14);
	SP2518_write_cmos_sensor(0x9f,0x06);
	//HEQ
	SP2518_write_cmos_sensor(0xfd,0x00);//
	SP2518_write_cmos_sensor(0xdd,0x80);//0x80	//0x78 modify by sp_yjp,20130821
	SP2518_write_cmos_sensor(0xde,0x9c);//a0	//0x98 modify by sp_yjp,20130821
	SP2518_write_cmos_sensor(0xdf,0x80);//
	//Ytarget 
	
#if	1//modify by sp_yjp,20130822
	SP2518_write_cmos_sensor(0xfd,0x00);// 
	SP2518_write_cmos_sensor(0xec,0x78);//6a
	SP2518_write_cmos_sensor(0xed,0x8e);//7c
	SP2518_write_cmos_sensor(0xee,0x78);//65
	SP2518_write_cmos_sensor(0xef,0x8e);//78
	
	SP2518_write_cmos_sensor(0xf7,0x88);//78
	SP2518_write_cmos_sensor(0xf8,0x7c);//6e
	SP2518_write_cmos_sensor(0xf9,0x88);//74
	SP2518_write_cmos_sensor(0xfa,0x7c);//6a 
#else
	SP2518_write_cmos_sensor(0xfd,0x00);// 
	SP2518_write_cmos_sensor(0xec,0x68);//6a
	SP2518_write_cmos_sensor(0xed,0x7e);//7c
	SP2518_write_cmos_sensor(0xee,0x68);//65
	SP2518_write_cmos_sensor(0xef,0x7e);//78
	SP2518_write_cmos_sensor(0xf7,0x78);//78
	SP2518_write_cmos_sensor(0xf8,0x6c);//6e
	SP2518_write_cmos_sensor(0xf9,0x78);//74
	SP2518_write_cmos_sensor(0xfa,0x6c);//6a 

#endif
	//sharpen
	SP2518_write_cmos_sensor(0xfd,0x01);
	SP2518_write_cmos_sensor(0xdf,0x0f);
	SP2518_write_cmos_sensor(0xe5,0x10);
	SP2518_write_cmos_sensor(0xe7,0x10);

	#if 1//modify by sp_yjp,20130822
	SP2518_write_cmos_sensor(0xe8,0x38);
	SP2518_write_cmos_sensor(0xec,0x34);
	
	SP2518_write_cmos_sensor(0xe9,0x38);
	SP2518_write_cmos_sensor(0xed,0x34);
	
	SP2518_write_cmos_sensor(0xea,0x28);
	SP2518_write_cmos_sensor(0xef,0x24);
	
	SP2518_write_cmos_sensor(0xeb,0x14);
	SP2518_write_cmos_sensor(0xf0,0x10);
	#else
	SP2518_write_cmos_sensor(0xe8,0x20);
	SP2518_write_cmos_sensor(0xec,0x20);
	
	SP2518_write_cmos_sensor(0xe9,0x20);
	SP2518_write_cmos_sensor(0xed,0x20);
	
	SP2518_write_cmos_sensor(0xea,0x10);
	SP2518_write_cmos_sensor(0xef,0x10);
	
	SP2518_write_cmos_sensor(0xeb,0x10);
	SP2518_write_cmos_sensor(0xf0,0x10);
	#endif
	//;gw
	SP2518_write_cmos_sensor(0xfd,0x01);//
	SP2518_write_cmos_sensor(0x70,0x76);//
	SP2518_write_cmos_sensor(0x7b,0x40);//
	SP2518_write_cmos_sensor(0x81,0x30);//
	//;Y_offset
	SP2518_write_cmos_sensor(0xfd,0x00);
	SP2518_write_cmos_sensor(0xb2,SP2518_NORMAL_Y0ffset);
	SP2518_write_cmos_sensor(0xb3,0x1f);
	SP2518_write_cmos_sensor(0xb4,0x30);
	SP2518_write_cmos_sensor(0xb5,0x50);
	//;CNR
	SP2518_write_cmos_sensor(0xfd,0x00);
	SP2518_write_cmos_sensor(0x5b,0x20);
	SP2518_write_cmos_sensor(0x61,0x80);
	SP2518_write_cmos_sensor(0x77,0x80);
	SP2518_write_cmos_sensor(0xca,0x80); 
	//;YNR  
	SP2518_write_cmos_sensor(0xab,0x00);
	SP2518_write_cmos_sensor(0xac,0x02);
	SP2518_write_cmos_sensor(0xae,0x08);
	SP2518_write_cmos_sensor(0xaf,0x20);
	SP2518_write_cmos_sensor(0xfd,0x00);
	SP2518_write_cmos_sensor(0x35,0x40);    

	SP2518_write_cmos_sensor(0x36,0x00); 	//0x00 0x40 0x80 0xc0  

	
	SP2518_write_cmos_sensor(0x32,0x0d);
	SP2518_write_cmos_sensor(0x33,0xcf);//ef
	SP2518_write_cmos_sensor(0x34,0x7f);// 3f
	SP2518_write_cmos_sensor(0xe7,0x03);  
	SP2518_write_cmos_sensor(0xe7,0x00);

	SP2518_config_window(WINDOW_SIZE_SVGA);//WINDOW_SIZE_SVGA;WINDOW_SIZE_VGA; 
	//modify by sp_yjp,20130813
	///SP2518_config_window(WINDOW_SIZE_SVGA); 
       #if 0
	SP2518_write_cmos_sensor(0xfd,0x00);  
	SP2518_write_cmos_sensor(0x2f,0x08);	//0x08

	SP2518_write_cmos_sensor(0xfd,0x01);  
	SP2518_write_cmos_sensor(0x0f,0x01);	//0x01
	#endif

} 

/*************************************************************************
 * FUNCTION
 *	SP2518_Write_More_Registers
 *
 * DESCRIPTION
 *	This function is served for FAE to modify the necessary Init Regs. Do not modify the regs
 *     in init_SP2518() directly.
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
void SP2518_Write_More_Registers(void)
{
	//Ronlus this function is used for FAE debug
}

#if 0

/*************************************************************************
 * FUNCTION
 *	SP2518_PV_setting
 *
 * DESCRIPTION
 *	This function apply the preview mode setting, normal the preview size is 1/4 of full size.
 *	Ex. 2M (1600 x 1200)
 *	Preview: 800 x 600 (Use sub-sample or binning to acheive it)
 *	Full Size: 1600 x 1200 (Output every effective pixels.)
 *
 * PARAMETERS
 *	1. image_sensor_exposure_window_struct : Set the grab start x,y and width,height.
 *	2. image_sensor_config_struct : Current operation mode.
 *
 * RETURNS
 *	None
 *
 *************************************************************************/
static void SP2518_PV_setting(void)
{
	SENSORDB("Ronlus SP2518_PV_setting\r\n");

} /* SP2518_PV_setting */


/*************************************************************************
 * FUNCTION
 *	SP2518_CAP_setting
 *
 * DESCRIPTION
 *	This function apply the full size mode setting.
 *	Ex. 2M (1600 x 1200)
 *	Preview: 800 x 600 (Use sub-sample or binning to acheive it)
 *	Full Size: 1600 x 1200 (Output every effective pixels.)
 *
 * PARAMETERS
 *	1. image_sensor_exposure_window_struct : Set the grab start x,y and width,height.
 *	2. image_sensor_config_struct : Current operation mode.
 *
 * RETURNS
 *	None
 *
 *************************************************************************/
static void SP2518_CAP_setting(void)
{
	//kal_uint16 corr_r_offset,corr_g_offset,corr_b_offset,temp = 0;
	SENSORDB("Ronlus SP2518_CAP_setting\r\n");	
} /* SP2518_CAP_setting */

#endif
/*************************************************************************
 * FUNCTION
 *	SP2518Open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
UINT32 SP2518Open(void)
{
	kal_uint16 sensor_id=0;
	int i;
	SENSORDB("@@@SP:Ronlus SP2518Open\r\n");
	// check if sensor ID correct
	for(i = 0; i < 3; i++)
	{
		SP2518_write_cmos_sensor(0xfd,0x00);
		sensor_id = SP2518_read_cmos_sensor(0x02);
		SENSORDB("@@@SP:Ronlus SP2518 Sensor id = %x\n", sensor_id);

		if (sensor_id == SP2518_YUV_SENSOR_ID)
		{
			break;
		}
	}
	//mdelay(50);
	if(sensor_id != SP2518_YUV_SENSOR_ID)
	{
		SENSORDB("@@@SP:SP2518 Sensor id read failed, ID = %x\n", sensor_id);
		return ERROR_SENSOR_CONNECT_FAIL;
	}

#ifdef DEBUG_SENSOR_SP2518  //gepeiwei   120903
	struct file *fp; 
	mm_segment_t fs; 
	loff_t pos = 0; 
			//static char buf[10*1024] ;

			printk("SP2518 Open File Start\n");
			fp = filp_open("/storage/sdcard1/sp2518_sd.dat", O_RDONLY , 0); 
			if (IS_ERR(fp)) 
			{ 
				printk("open file error 0\n");
				fp = filp_open("/storage/sdcard0/sp2518_sd.dat", O_RDONLY , 0); 
				if (IS_ERR(fp)) { 
					fromsd = 0;   
					printk("open file error 1\n");
				}
				else{
					printk("open file success 1\n");
					fromsd = 1;
					filp_close(fp, NULL); 
				}
			} 
			else 
			{
				printk("open file success 0\n");
		fromsd = 1;
		printk("open file ok\n");

		//SP2518_Initialize_from_T_Flash();


		filp_close(fp, NULL); 
	}

	if(fromsd == 1)//是否\u017d覵D读取//gepeiwei   120903
	{
		printk("________________from t!\n");
		SP2518_Initialize_from_T_Flash();//\u017d覵D縗u0161读取的主要函数
	}
	else
	{
		SP2518_Sensor_Init();
		SP2518_Write_More_Registers();//added for FAE to debut
	}
#else  
	//RETAILMSG(1, (TEXT("Sensor Read ID OK \r\n")));
	// initail sequence write in
	SP2518_Sensor_Init();
	SP2518_Write_More_Registers();//added for FAE to debut
#endif	
return ERROR_NONE;
} /* SP2518Open */


/*************************************************************************
 * FUNCTION
 *	SP2518Close
 *
 * DESCRIPTION
 *	This function is to turn off sensor module power.
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
UINT32 SP2518Close(void)
{
	SENSORDB("@@@SP:SP2518Close\r\n");
	return ERROR_NONE;
} /* SP2518Close */

void SP2518_AfterSnapshot(void)
{
    //SENSOR_MODE_E sensor_snapshot_mode = para;
 //   kal_uint16  Shutter;
//    kal_uint8  	tmp1,tmp2;
	
	//SCI_TRACE_LOW("SP2518: SP2518_before_snapshot %x", para);
    //if(sensor_snapshot_mode < SENSOR_MODE_SNAPSHOT_ONE_FIRST) //less than 640X480
    //{
    //     return SCI_SUCCESS;
		 
   // }  

//    Sensor_SetMode(sensor_snapshot_mode);
	SP2518_write_cmos_sensor(0xfd,0x00);
//	SP2518_WriteReg(0x1b,0x0f);
	 
	//tmp1=SP2518_read_cmos_sensor(0x03);
	//tmp2=SP2518_read_cmos_sensor(0x04);

	SP2518_write_cmos_sensor(0x32,0x0d);   // 0x68  0x68
	//SP2518_read_cmos_sensor(0x14,0x20); //0x20 luo 1515
	//SP2518_write_cmos_sensor(0x22,0x1b); // 0x1b // 0x19
	//SP2518_write_cmos_sensor(0x1e,0xe1); // 0x1b // 0x81  
	
	//SP2518_WriteReg(0x28,0x40); //0x18
	
	//Shutter= (tmp1<<8 )| tmp2;
	//Shutter = Shutter*36/72;    // 48
	//tmp1 = (Shutter >>8) & 0xff;
	//tmp2 = Shutter & 0xff;
	
	//SP2518_write_cmos_sensor(0x03,tmp1);
	//SP2518_write_cmos_sensor(0x04,tmp2);
	//SP2518_write_cmos_sensor(0x30 , 0x04);//PLL 2±??μ
	  SP2518_write_cmos_sensor(0x30 , 0x04);//PLL 1.5±??μ //130802
	//SP2518_write_cmos_sensor(0x31 , 0x10);//PLL 1.5±??μ
#if 1
//	SP2518_WriteReg(0xe7 , 0x03);	
//	SP2518_WriteReg(0xe7 , 0x00);
	SP2518_write_cmos_sensor(0xfd , 0x01); 
	SP2518_write_cmos_sensor(0x0f , 0x01);
	SP2518_write_cmos_sensor(0xfd , 0x00);
	SP2518_write_cmos_sensor(0x2f , 0x08);
	
	SP2518_write_cmos_sensor(0xfd , 0x01);
	SP2518_write_cmos_sensor(0x0e , 0x01);
#endif  
	//SP2518_WriteReg(0x1b,0x02);

#if 0
       //1280x960 window set
    SP2518_WriteReg(0xfd , 0x00);

    SP2518_WriteReg(0x4d , 0x05);
    SP2518_WriteReg(0x4e , 0x00);
    SP2518_WriteReg(0x49 , 0x03);
    SP2518_WriteReg(0x4a , 0xc0);
	
     SP2518_WriteReg(0x4b , 0x00);
     SP2518_WriteReg(0x4c , 0xa0) ; //0xa0

     SP2518_WriteReg(0x47 , 0x00);
     SP2518_WriteReg(0x48 , 0x78); //0x78
    #endif
//	return 0;
}  


void SP2518_BeforeSnapshot(void)
{
    //SENSOR_MODE_E sensor_snapshot_mode = para;
    kal_uint16  Shutter;
    kal_uint8  	tmp1,tmp2,tmp3,tmp;
	
	//SCI_TRACE_LOW("SP2518: SP2518_before_snapshot %x", para);
    //if(sensor_snapshot_mode < SENSOR_MODE_SNAPSHOT_ONE_FIRST) //less than 640X480
    //{
    //     return SCI_SUCCESS;
		 
   // }  

//    Sensor_SetMode(sensor_snapshot_mode);
	SP2518_write_cmos_sensor(0xfd,0x00);
	tmp=SP2518_read_cmos_sensor(0x32);
	 
	tmp1=SP2518_read_cmos_sensor(0x03);
	tmp2=SP2518_read_cmos_sensor(0x04);
	tmp3=SP2518_read_cmos_sensor(0x23);
	//printk("tangzibo sp2518 tmp = %x\n",tmp);
	//printk("tangzibo sp2518 tmp1 = %x\n",tmp1);
	//printk("tangzibo sp2518 tmp2 = %x\n",tmp2);
	//printk("tangzibo sp2518 tmp3 = %x\n",tmp3);
	//SP2518_write_cmos_sensor(0x32,0x68);   // 0x68
	//SP2518_read_cmos_sensor(0x14,0x40); //0x20 luo 1515 
	//SP2518_write_cmos_sensor(0x22,0x1b); // 0x1b // 0x19
	//SP2518_write_cmos_sensor(0x1e,0xe1); // 0x1b // 0x81
	
	tmp=(tmp|0x40)&0xfa;
	
	Shutter= (tmp1<<8 )| tmp2;
	Shutter = Shutter*24/48;   //20130820
	if(Shutter<1)
		{
		Shutter=1;
		}
	tmp1 = (Shutter >>8) & 0xff;
	tmp2 = Shutter & 0xff;   
	SP2518_write_cmos_sensor(0x32,tmp);   // 0x68
	  
	SP2518_write_cmos_sensor(0x03,tmp1);
	SP2518_write_cmos_sensor(0x04,tmp2);
	SP2518_write_cmos_sensor(0x24,tmp3);	
	//SP2518_WriteReg(0x30 , 0x04);//PLL 2±??μ
	//printk("tangzibo sp2518 tmp end = %x\n",tmp);
	//printk("tangzibo sp2518 tmp1 end= %x\n",tmp1);
	//printk("tangzibo sp2518 tmp2 end= %x\n",tmp2);
	//printk("tangzibo sp2518 tmp3 end= %x\n",tmp3);
	SP2518_write_cmos_sensor(0x30 , 0x20);//PLL 1.5±??μ
#if 1
//	SP2518_WriteReg(0xe7 , 0x03);	
//	SP2518_WriteReg(0xe7 , 0x00);
	SP2518_write_cmos_sensor(0xfd , 0x01);
	SP2518_write_cmos_sensor(0x0f , 0x00);
	SP2518_write_cmos_sensor(0xfd , 0x00); 
	SP2518_write_cmos_sensor(0x2f , 0x00);
	
	SP2518_write_cmos_sensor(0xfd , 0x01);
	SP2518_write_cmos_sensor(0x0e , 0x00); 

	SP2518_write_cmos_sensor(0xe7 , 0x03);	
      SP2518_write_cmos_sensor(0xe7 , 0x00);
	//mDELAY(125);

#endif
	//SP2518_WriteReg(0x1b,0x02);

#if 0
       //1280x960 window set
    SP2518_WriteReg(0xfd , 0x00);

    SP2518_WriteReg(0x4d , 0x05);
    SP2518_WriteReg(0x4e , 0x00);
    SP2518_WriteReg(0x49 , 0x03);
    SP2518_WriteReg(0x4a , 0xc0);
	
     SP2518_WriteReg(0x4b , 0x00);
     SP2518_WriteReg(0x4c , 0xa0) ; //0xa0 
     SP2518_WriteReg(0x47 , 0x00);
     SP2518_WriteReg(0x48 , 0x78); //0x78
    #endif
//	return 0;  
} 

/*************************************************************************
 * FUNCTION
 * SP2518Preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
UINT32 SP2518Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	//kal_uint32 iTemp;
	//kal_uint16 iStartX = 0, iStartY = 1;
	printk("@@@SP:Ronlus SP2518Preview fun start\r\n");

    SP2518_MODE_CAPTURE = KAL_FALSE;
    SP2518_gPVmode = KAL_TRUE;

    #if PREVIEW_USE_VGA
        SP2518_config_window(WINDOW_SIZE_VGA);
    #elif PREVIEW_USE_SVGA
        SP2518_config_window(WINDOW_SIZE_SVGA);
    #elif PREVIEW_USE_720P
        SP2518_config_window(WINDOW_SIZE_720P);
    #else
        SP2518_config_window(WINDOW_SIZE_SVGA);
    #endif
    
    if (setshutter) 
    {
	    ///SP2518_AfterSnapshot(); 
    }
	setshutter = KAL_FALSE;
	if(sensor_config_data->SensorOperationMode == MSDK_SENSOR_OPERATION_MODE_VIDEO)		// MPEG4 Encode Mode
	{
		printk("@@@SP:Ronlus video preview\r\n");
		SP2518_MPEG4_encode_mode = KAL_TRUE;
	}
	else
	{
		printk("@@@SP:Ronlus capture preview\r\n");
		SP2518_MPEG4_encode_mode = KAL_FALSE;
	}
	//SP2518_config_window(WINDOW_SIZE_SVGA);//add zch test(use this for SVGA)
    image_window->GrabStartX= 1;
    image_window->GrabStartY= 1;
    image_window->ExposureWindowWidth = IMAGE_SENSOR_PV_WIDTH;//800-16; //1280-16; //1600-32 ; //800-16; 
    image_window->ExposureWindowHeight = IMAGE_SENSOR_PV_HEIGHT;//600-12;//720-12; //1200-24; //600-12;

    SP2518_DELAY_AFTER_PREVIEW = 1;
	// copy sensor_config_data
	memcpy(&SP2518SensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));//rotation
	printk("@@@SP:Ronlus SP2518Preview fun end\r\n");
	//Sleep(300);
	printk("@@@SP:555555\r\n");
	return ERROR_NONE;
} /* SP2518Preview */


/*************************************************************************
 * FUNCTION
 *	SP2518Capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
UINT32 SP2518Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)

{
	SENSORDB("Ronlus SP2518Capture fun start\r\n");
	SP2518_MODE_CAPTURE=KAL_TRUE;

    // config: UXGA for capture
    SP2518_config_window(WINDOW_SIZE_UXGA);
	
     if ((image_window->ImageTargetWidth<=640-16)&&
        (image_window->ImageTargetHeight<=480-12))
		{    /* Less than PV Mode */
			image_window->GrabStartX = 0;
			image_window->GrabStartY = 1;
			image_window->ExposureWindowWidth= 640-16;
			image_window->ExposureWindowHeight = 480-12;
		}
     else

	{
//		kal_uint32 shutter, cap_dummy_pixels = 0; 
		if(!setshutter)
		{
			//// SP2518_BeforeSnapshot();
			image_window->GrabStartX = 1;//iStartX;
			image_window->GrabStartY = 1;//iStartY;
			image_window->ExposureWindowWidth = IMAGE_SENSOR_FULL_WIDTH; //16 ;
			image_window->ExposureWindowHeight = IMAGE_SENSOR_FULL_HEIGHT; //12;
		}
		else
		{
			image_window->GrabStartX = 1;
			image_window->GrabStartY = 1;
			image_window->ExposureWindowWidth = IMAGE_SENSOR_FULL_WIDTH; //16 ;
			image_window->ExposureWindowHeight = IMAGE_SENSOR_FULL_HEIGHT; //12;
		}

		setshutter = KAL_TRUE;
		
	}

	 SP2518_DELAY_AFTER_PREVIEW = 2;
	// copy sensor_config_data
	memcpy(&SP2518SensorConfigData, sensor_config_data, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
	SENSORDB("Ronlus SP2518Capture fun end\r\n");
	return ERROR_NONE;
} /* SP2518_Capture() */




UINT32 SP2518GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
	SENSORDB("Ronlus SP2518GetResolution\r\n");
    pSensorResolution->SensorFullWidth = IMAGE_SENSOR_FULL_WIDTH; //1600-96; //-16-32;
    pSensorResolution->SensorFullHeight = IMAGE_SENSOR_FULL_HEIGHT; //1200-72; //12-24;
    pSensorResolution->SensorPreviewWidth = IMAGE_SENSOR_PV_WIDTH; //1600-64; //800-16-8;//IMAGE_SENSOR_SVGA_WIDTH;(user this for SVGA)
    pSensorResolution->SensorPreviewHeight = IMAGE_SENSOR_PV_HEIGHT; //1200-48; //600-12-6;//IMAGE_SIMAGE_SENSOR_SVGA_HEIGHT;(use this for SVGA)
	pSensorResolution->SensorVideoWidth = IMAGE_SENSOR_FULL_WIDTH; //1600-64;  //800-16-8;
	pSensorResolution->SensorVideoHeight = IMAGE_SENSOR_FULL_HEIGHT; //1200-48; //600-12-6;
	
    pSensorResolution->SensorHighSpeedVideoWidth = IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorHighSpeedVideoHeight = IMAGE_SENSOR_PV_HEIGHT;
    
    pSensorResolution->SensorSlimVideoWidth = IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorSlimVideoHeight = IMAGE_SENSOR_PV_HEIGHT;  
	return ERROR_NONE;
} /* SP2518GetResolution() */ 


UINT32 SP2518GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
		MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
		MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	SENSORDB("@@@SP:Ronlus SP2518GetInfo\r\n");
    pSensorInfo->SensorPreviewResolutionX = IMAGE_SENSOR_PV_WIDTH; //1600-64; //800-16-8;//IMAGE_SENSOR_PV_WIDTH(use this for SVGA);
    pSensorInfo->SensorPreviewResolutionY = IMAGE_SENSOR_PV_HEIGHT; //1200-48; //600-12-6;//IMAGE_SENSOR_PV_HEIGHT(user this for SVGA);
    pSensorInfo->SensorFullResolutionX = IMAGE_SENSOR_FULL_WIDTH; //1600-96; //-16-32;
    pSensorInfo->SensorFullResolutionY = IMAGE_SENSOR_FULL_HEIGHT; //1200-72; //-12-24;

	pSensorInfo->SensorCameraPreviewFrameRate=30;  
	pSensorInfo->SensorVideoFrameRate=30;
	pSensorInfo->SensorStillCaptureFrameRate=10;
	pSensorInfo->SensorWebCamCaptureFrameRate=15; 
	pSensorInfo->SensorResetActiveHigh=FALSE;
	pSensorInfo->SensorResetDelayCount=1;
	pSensorInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_UYVY;
	pSensorInfo->SensorClockPolarity=SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
	
	pSensorInfo->SensorHsyncPolarity =SENSOR_CLOCK_POLARITY_LOW;
	pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_HIGH;
	pSensorInfo->SensorInterruptDelayLines = 1;
	pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_PARALLEL;
  
	pSensorInfo->CaptureDelayFrame = 1;
	pSensorInfo->PreviewDelayFrame = 1;
	pSensorInfo->VideoDelayFrame = 4;
	pSensorInfo->SensorMasterClockSwitch = 0;
	pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;///ISP_DRIVING_2MA; //modify by sp_yjp,20130813
	
	pSensorInfo->HighSpeedVideoDelayFrame = 4;
	pSensorInfo->SlimVideoDelayFrame = 4;
 	pSensorInfo->SensorModeNum = 5;


	switch (ScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			//case MSDK_SCENARIO_ID_VIDEO_CAPTURE_MPEG4:
			pSensorInfo->SensorClockFreq=24;
			pSensorInfo->SensorClockDividCount=	3;
			pSensorInfo->SensorClockRisingCount= 0;
			pSensorInfo->SensorClockFallingCount= 2;
			pSensorInfo->SensorPixelClockCount= 3;
			pSensorInfo->SensorDataLatchCount= 2;
        pSensorInfo->SensorGrabStartX = 1;
        pSensorInfo->SensorGrabStartY = 1;

			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			//  case MSDK_SCENARIO_ID_CAMERA_CAPTURE_MEM:
			pSensorInfo->SensorClockFreq=24;
			pSensorInfo->SensorClockDividCount= 3;
			pSensorInfo->SensorClockRisingCount=0;
			pSensorInfo->SensorClockFallingCount=2;
			pSensorInfo->SensorPixelClockCount=3;
			pSensorInfo->SensorDataLatchCount=2;
        pSensorInfo->SensorGrabStartX = 1;
        pSensorInfo->SensorGrabStartY = 1;
			break;
		default:
			pSensorInfo->SensorClockFreq=24;
			pSensorInfo->SensorClockDividCount= 3;
			pSensorInfo->SensorClockRisingCount=0;
			pSensorInfo->SensorClockFallingCount=2;
			pSensorInfo->SensorPixelClockCount=3;
			pSensorInfo->SensorDataLatchCount=2;
        pSensorInfo->SensorGrabStartX = 1;
        pSensorInfo->SensorGrabStartY = 1;
			break;
	}
	SP2518PixelClockDivider=pSensorInfo->SensorPixelClockCount;
	memcpy(pSensorConfigData, &SP2518SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
	return ERROR_NONE;
} /* SP2518GetInfo() */


UINT32 SP2518Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
		MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	pr_debug("@@@SP:Ronlus SP2518Control\r\n");
	
	switch (ScenarioId)
	{
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			//case MSDK_SCENARIO_ID_VIDEO_CAPTURE_MPEG4:
			SP2518Preview(pImageWindow, pSensorConfigData);
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			//case MSDK_SCENARIO_ID_CAMERA_CAPTURE_MEM:
			SP2518Capture(pImageWindow, pSensorConfigData);
			break;
		default:
            SENSORDB("Error ScenarioId setting");
            SP2518Preview(pImageWindow, pSensorConfigData);
            return ERROR_INVALID_SCENARIO_ID;
	}


	return TRUE;
}	/* SP2518Control() */

BOOL SP2518_set_param_wb(UINT16 para)
{
	SENSORDB("Ronlus SP2518_set_param_wb\r\n");
	switch (para)
	{
		case AWB_MODE_OFF:
			//SP2518_write_cmos_sensor(0xfd,0x00);
			// SP2518_write_cmos_sensor(0x32,0x05);
			break; 					
		case AWB_MODE_AUTO:
			SP2518_write_cmos_sensor(0xfd,0x01);
			SP2518_write_cmos_sensor(0x28,0xce);
			SP2518_write_cmos_sensor(0x29,0x8a);	   
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0x32,0x0d);
			break;
		case AWB_MODE_CLOUDY_DAYLIGHT: //cloudy
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0x32,0x05);
			SP2518_write_cmos_sensor(0xfd,0x01);
			SP2518_write_cmos_sensor(0x28,0xe2);
			SP2518_write_cmos_sensor(0x29,0x82);
			SP2518_write_cmos_sensor(0xfd,0x00);
			break;
		case AWB_MODE_DAYLIGHT: //sunny
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0x32,0x05);
			SP2518_write_cmos_sensor(0xfd,0x01);
			SP2518_write_cmos_sensor(0x28,0xc1);
			SP2518_write_cmos_sensor(0x29,0x88);
			SP2518_write_cmos_sensor(0xfd,0x00);
			break;
		case AWB_MODE_INCANDESCENT: //office
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0x32,0x05);
			SP2518_write_cmos_sensor(0xfd,0x01);
			SP2518_write_cmos_sensor(0x28,0x7b);
			SP2518_write_cmos_sensor(0x29,0xd3);
			SP2518_write_cmos_sensor(0xfd,0x00);
			break;
		case AWB_MODE_TUNGSTEN: //home
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0x32,0x05);
			SP2518_write_cmos_sensor(0xfd,0x01);
			SP2518_write_cmos_sensor(0x28,0xae);
			SP2518_write_cmos_sensor(0x29,0xcc);
			SP2518_write_cmos_sensor(0xfd,0x00);
			break;
		case AWB_MODE_FLUORESCENT:
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0x32,0x05);
			SP2518_write_cmos_sensor(0xfd,0x01);
			SP2518_write_cmos_sensor(0x28,0xb4);
			SP2518_write_cmos_sensor(0x29,0xc4);
			SP2518_write_cmos_sensor(0xfd,0x00);
		default:
			return FALSE;
	}  
	return TRUE;
} /* SP2518_set_param_wb */


BOOL SP2518_set_param_effect(UINT16 para)
{
	SENSORDB("Ronlus SP2518_set_param_effect\r\n");
	switch (para)
	{
		case MEFFECT_OFF:
			SP2518_write_cmos_sensor(0xfd, 0x00);
			SP2518_write_cmos_sensor(0x62, 0x00);
			SP2518_write_cmos_sensor(0x63, 0x80);
			SP2518_write_cmos_sensor(0x64, 0x80);
			break;
		case MEFFECT_SEPIA:
			SP2518_write_cmos_sensor(0xfd, 0x00);
			SP2518_write_cmos_sensor(0x62, 0x10);
			SP2518_write_cmos_sensor(0x63, 0xb0);
			SP2518_write_cmos_sensor(0x64, 0x40);
			break;
		case MEFFECT_NEGATIVE://----datasheet
			SP2518_write_cmos_sensor(0xfd, 0x00);
			SP2518_write_cmos_sensor(0x62, 0x04);
			SP2518_write_cmos_sensor(0x63, 0x80);
			SP2518_write_cmos_sensor(0x64, 0x80);
			break;
		case MEFFECT_SEPIAGREEN://----datasheet aqua
			SP2518_write_cmos_sensor(0xfd, 0x00);
			SP2518_write_cmos_sensor(0x62, 0x10);
			SP2518_write_cmos_sensor(0x63, 0x50);
			SP2518_write_cmos_sensor(0x64, 0x50);
			break;
		case MEFFECT_SEPIABLUE:
			SP2518_write_cmos_sensor(0xfd, 0x00);
			SP2518_write_cmos_sensor(0x62, 0x10);
			SP2518_write_cmos_sensor(0x63, 0x80);
			SP2518_write_cmos_sensor(0x64, 0xb0);
			break;
		case MEFFECT_MONO: //----datasheet black & white
			SP2518_write_cmos_sensor(0xfd, 0x00);
			SP2518_write_cmos_sensor(0x62, 0x20);
			SP2518_write_cmos_sensor(0x63, 0x80);
			SP2518_write_cmos_sensor(0x64, 0x80);
			break;
		default:
			return FALSE;
	}
	return TRUE;
} /* SP2518_set_param_effect */

//UINT8 index = 1;
BOOL SP2518_set_param_banding(UINT16 para)
{
	//UINT16 buffer = 0;
	UINT8 index = 1;
	SENSORDB("Ronlus SP2518_set_param_banding para = %d ---- index = %d\r\n",para,index); 
	SENSORDB("Ronlus SP2518_set_param_banding ---- SP2518_MPEG4_encode_mode = %d\r\n",SP2518_MPEG4_encode_mode);
	switch (para)
	{
		case AE_FLICKER_MODE_50HZ:
			SP2518_CAM_BANDING_50HZ = KAL_TRUE;
			break;
		case AE_FLICKER_MODE_60HZ:
			SP2518_CAM_BANDING_50HZ = KAL_FALSE;
			break;
		default:
			//SP2518_write_cmos_sensor(0xfd,0x00);//ronlus test
			//buffer = SP2518_read_cmos_sensor(0x35);
			return FALSE;
	}
#if 0/*Superpix Ronlus some vertical line when switching*/
	SP2518_config_window(WINDOW_SIZE_SVGA);
	SP2518_write_cmos_sensor(0xfd,0x00);
	SP2518_write_cmos_sensor(0x35,0x00);
#endif
	return TRUE;
} /* SP2518_set_param_banding */


BOOL SP2518_set_param_exposure(UINT16 para)
{
	SENSORDB("Ronlus SP2518_set_param_exposure\r\n");
	switch (para)
	{
		case AE_EV_COMP_n30:              /* EV -2 */
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0xdc,0x80);
			break;
		case AE_EV_COMP_n20:              /* EV -1.5 */
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0xdc,0xc0);
			break;
		case AE_EV_COMP_n10:              /* EV -1 */
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0xdc,0xe0);
			break;
	/*	case AE_EV_COMP_n03:              
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0xdc,0xf0);
			break;  
		*/
		case AE_EV_COMP_00:                /* EV 0 */
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0xdc,0x00);
			break;
		case AE_EV_COMP_10:              /* EV +0.5 */
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0xdc,0x20);
			break;
		case AE_EV_COMP_20:              /* EV +1 */
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0xdc,0x40);
			break;
		case AE_EV_COMP_30:              /* EV +1.5 */
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0xdc,0x60);
			break;
	/*	case AE_EV_COMP_13:             
			SP2518_write_cmos_sensor(0xfd,0x00);
			SP2518_write_cmos_sensor(0xdc,0x40);
			break;  */
		default:
			return FALSE;
	}
	return TRUE;

} /* SP2518_set_param_exposure */


UINT32 SP2518YUVSensorSetting(FEATURE_ID iCmd, UINT16 iPara)
{

#ifdef DEBUG_SENSOR_SP2518
	return TRUE;
#endif
	SENSORDB("Ronlus SP2518YUVSensorSetting\r\n");
	switch (iCmd) 
	{
		case FID_SCENE_MODE:	
			#if 0 //del by sp_yjp for test,20130822
			if (iPara == SCENE_MODE_OFF)
			{
				SP2518_night_mode(0); 
			}
			else if (iPara == SCENE_MODE_NIGHTSCENE)
			{
				SP2518_night_mode(1); 
			}
			#endif
			break; 	    
		case FID_AWB_MODE:
			SP2518_set_param_wb(iPara);
			break;
		case FID_COLOR_EFFECT:
			SP2518_set_param_effect(iPara);
			break;
		case FID_AE_EV:
			SP2518_set_param_exposure(iPara);
			break;
		case FID_AE_FLICKER:
			SP2518_set_param_banding(iPara);
			SP2518_night_mode(SP2518_CAM_Nightmode); 
			break;
		default:
			break;
	}
	return TRUE;
} /* SP2518YUVSensorSetting */

UINT32 sp2518_get_sensor_id(UINT32 *sensorID) 
{
	volatile signed char i;
	kal_uint16 sensor_id=0;
	printk("@@@SP:xieyang SP2518GetSensorID ");
	///SENSORDB("@@@SP:xieyang in GPIO_CAMERA_CMPDN_PIN=%d,GPIO_CAMERA_CMPDN1_PIN=%d"
	///		, mt_get_gpio_out(GPIO_CAMERA_CMPDN_PIN),mt_get_gpio_out(GPIO_CAMERA_CMPDN1_PIN));

	SP2518_write_cmos_sensor(0xfd, 0x00);
	for(i=0;i<10;i++)
	{
		sensor_id = SP2518_read_cmos_sensor(0x02);
		if(sensor_id == SP2518_YUV_SENSOR_ID)
			break;
	}
	printk("@@@SP:SP2518GetSensorID=%#x\r\n ",sensor_id);

	while(0)	//add by sp_yjp for test,20130813
	{
		mdelay(50);	
		sensor_id = SP2518_read_cmos_sensor(0x02);
		printk("@@@SP:DeadReadSP2518ID=%#x\r\n ",sensor_id);
		if(sensor_id == SP2518_YUV_SENSOR_ID)
			break;
	}
	
	if(sensor_id != SP2518_YUV_SENSOR_ID)
	{
		printk("@@@SP:>>>>>>>>>>SP2518 read ID error<<<<<<<<<<<");			
		*sensorID = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	
	*sensorID = sensor_id;
	return ERROR_NONE;
	
}


UINT32 SP2518_SetTestPatternMode(kal_bool bEnable)
{
 //   kal_uint16 temp;

    SENSORDB("bEnable: %d", bEnable);

    if(bEnable) 
	{
		SP2518_write_cmos_sensor(0xfd,0x00);
		SP2518_write_cmos_sensor(0x32,0x00);
		SP2518_write_cmos_sensor(0xfd,0x01);
		SP2518_write_cmos_sensor(0x28,0x80);
		SP2518_write_cmos_sensor(0x29,0x80);

		SP2518_write_cmos_sensor(0xfd,0x00);
		SP2518_write_cmos_sensor(0x1b,0x00);
		SP2518_write_cmos_sensor(0x1c,0x00);
		SP2518_write_cmos_sensor(0x30,0x20);
		SP2518_write_cmos_sensor(0xe7,0x03);
		SP2518_write_cmos_sensor(0xe7,0x00);
		SP2518_write_cmos_sensor(0x5f,0x01);
		SP2518_write_cmos_sensor(0x1e,0x30);
		SP2518_write_cmos_sensor(0x2c,0x00);
		SP2518_write_cmos_sensor(0xfb,0x00);
		SP2518_write_cmos_sensor(0x33,0x00);
		SP2518_write_cmos_sensor(0x34,0x00);
		SP2518_write_cmos_sensor(0x0d,0x14);
		SP2518_write_cmos_sensor(0xd5,0x00);
#if 1
		//SP2518_write_cmos_sensor(0xd6,0x00);
		SP2518_write_cmos_sensor(0xbf,0x00);
		SP2518_write_cmos_sensor(0xd3,0x00);
		SP2518_write_cmos_sensor(0xd4,0x00);
		SP2518_write_cmos_sensor(0xd6,0x00);
		SP2518_write_cmos_sensor(0xd7,0x00);
		SP2518_write_cmos_sensor(0xd8,0x00);
		SP2518_write_cmos_sensor(0xd9,0x00);
		SP2518_write_cmos_sensor(0xda,0x00);
		SP2518_write_cmos_sensor(0xdb,0x00);
#else
		SP2518_write_cmos_sensor(0xbf,0x00);
		SP2518_write_cmos_sensor(0xd3,0x60);
		SP2518_write_cmos_sensor(0xd4,0x60);
		SP2518_write_cmos_sensor(0xd6,0x60);
		SP2518_write_cmos_sensor(0xd7,0x60);
		SP2518_write_cmos_sensor(0xd8,0x60);
		SP2518_write_cmos_sensor(0xd9,0x60);
		SP2518_write_cmos_sensor(0xda,0x60);
		SP2518_write_cmos_sensor(0xdb,0x60);
#endif
		SP2518_write_cmos_sensor(0xca,0x00);
		SP2518_write_cmos_sensor(0xcb,0x00);
		SP2518_write_cmos_sensor(0xfd,0x00);
		//SP2518_write_cmos_sensor(0x32,0x08);
	}
	else        
	{
        	//SP2518_write_cmos_sensor(0xfd, 0x00);
		//SP2518_write_cmos_sensor(0x0d, 0x00);
	}
    
    return ERROR_NONE;
}

//#define SP2518_TEST_PATTERN_CHECKSUM (0x4d1d696d)
//#define SP2518_TEST_PATTERN_CHECKSUM (0x96f31f11)
#define SP2518_TEST_PATTERN_CHECKSUM (0x3f4b965a)


UINT32 SP2518FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
		UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
	UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
//	UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
	UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
	UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
	
    unsigned long long *pFeatureData=(unsigned long long *) pFeaturePara;
 //   unsigned long long *pFeatureReturnPara=(unsigned long long *) pFeaturePara;
//	UINT32 SP2518SensorRegNumber;
//	UINT32 i;

	SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	
	
	MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
	MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;

	pr_debug("@@@SP:Ronlus SP2518FeatureControl.---FeatureId = %d\r\n",FeatureId);
	RETAILMSG(1, (_T("gaiyang SP2518FeatureControl FeatureId=%d\r\n"), FeatureId));

	switch (FeatureId)
	{
		case SENSOR_FEATURE_GET_RESOLUTION:
			*pFeatureReturnPara16++=UXGA_PERIOD_PIXEL_NUMS;
			*pFeatureReturnPara16=UXGA_PERIOD_LINE_NUMS;
			*pFeatureParaLen=4;
			break;
		case SENSOR_FEATURE_GET_PERIOD:
			*pFeatureReturnPara16++=(SVGA_PERIOD_PIXEL_NUMS)+SP2518_dummy_pixels;
			*pFeatureReturnPara16=(SVGA_PERIOD_LINE_NUMS)+SP2518_dummy_lines;
			*pFeatureParaLen=4;
			break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			*pFeatureReturnPara32 = SP2518_g_fPV_PCLK;
			*pFeatureParaLen=4;
			break;
		case SENSOR_FEATURE_SET_ESHUTTER:
			break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
#ifndef DEBUG_SENSOR_SP2518		
			SP2518_night_mode((BOOL) *pFeatureData);
#endif
			break;
		case SENSOR_FEATURE_SET_GAIN:
		case SENSOR_FEATURE_SET_FLASHLIGHT:
			break;
		case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
			SP2518_isp_master_clock=*pFeatureData32;
			break;
		case SENSOR_FEATURE_SET_REGISTER:
			SP2518_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
			break;
		case SENSOR_FEATURE_GET_REGISTER:
			pSensorRegData->RegData = SP2518_read_cmos_sensor(pSensorRegData->RegAddr);
			break;
		case SENSOR_FEATURE_GET_CONFIG_PARA:
			memcpy(pSensorConfigData, &SP2518SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
			*pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
			break;
		case SENSOR_FEATURE_SET_CCT_REGISTER:
		case SENSOR_FEATURE_GET_CCT_REGISTER:
		case SENSOR_FEATURE_SET_ENG_REGISTER:
		case SENSOR_FEATURE_GET_ENG_REGISTER:
		case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
		case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
		case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
		case SENSOR_FEATURE_GET_GROUP_COUNT:
		case SENSOR_FEATURE_GET_GROUP_INFO:
		case SENSOR_FEATURE_GET_ITEM_INFO:
		case SENSOR_FEATURE_SET_ITEM_INFO:
		case SENSOR_FEATURE_GET_ENG_INFO:
			break;
		case SENSOR_FEATURE_CHECK_SENSOR_ID:
			sp2518_get_sensor_id(pFeatureReturnPara32); 
			break; 
		case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
			// get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
			// if EEPROM does not exist in camera module.
			*pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
			*pFeatureParaLen=4;
			break;
		case SENSOR_FEATURE_SET_YUV_CMD:
			SP2518YUVSensorSetting((FEATURE_ID)*pFeatureData, *(pFeatureData+1));//test ,del by sp_yjp,20130813
			break;
		case SENSOR_FEATURE_SET_TEST_PATTERN:
        		SP2518_SetTestPatternMode((BOOL)*pFeatureData);
       			break;
    		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE://for factory mode auto testing             
        		*pFeatureReturnPara32= SP2518_TEST_PATTERN_CHECKSUM;
        		*pFeatureParaLen=4; 
			break;
		case SENSOR_FEATURE_GET_CROP_INFO:
            //LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", *pFeatureData);
            // For pointer address in user space is 32-bit, need use compat_ptr to convert it to 64-bit in kernel space    

            wininfo = (SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(pFeatureData+1));

            switch (*pFeatureData32) {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[3],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[4],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                default:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
            }
			break;
			
		default:
			break;
	}
	return ERROR_NONE;
}	/* SP2518FeatureControl() */


SENSOR_FUNCTION_STRUCT	SensorFuncSP2518YUV=
{
	SP2518Open,
	SP2518GetInfo,
	SP2518GetResolution,
	SP2518FeatureControl,
	SP2518Control,
	SP2518Close
};


UINT32 SP2518_YUV_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */	
	if (pfFunc!=NULL)
	{
		SENSORDB("@@@SP:SP2518_YUV_SensorInit\r\n");
		*pfFunc=&SensorFuncSP2518YUV;
	}
	return ERROR_NONE;
} /* SensorInit() */
