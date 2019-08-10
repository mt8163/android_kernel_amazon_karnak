/* --------------------------------------------------------------------------- */

#ifndef EXTD_HDMI_DRV_H
#define     EXTD_HDMI_DRV_H

/* /#include "mtkfb.h" */
#include "external_display.h"
#include "disp_session.h"
#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT
#include "internal_hdmi_drv.h"
#endif

#define MHL_UART_SHARE_PIN

typedef enum {
	STEP1_ENABLE,
	STEP2_GET_STATUS,
	STEP3_START,
	STEP3_SUSPEND,
	STEP_MAX_NUM
} HDMI_FACTORY_MODE_TEST;

/* ~~~~~~~~~~~~~~~~~~~~~~~~extern declare~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#ifdef EXTD_DBG_USE_INNER_BUF
extern unsigned char kara_1280x720[2764800];
#endif

extern int ovl1_remove;

extern void HDMI_DBG_Init(void);
extern const struct HDMI_DRIVER *HDMI_GetDriver(void);
extern unsigned long get_dim_layer_mva_addr(void);
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


/*the following APIs are for MHL control*/
void hdmi_resume(void);
void hdmi_suspend(void);
int hdmi_video_config(HDMI_VIDEO_RESOLUTION vformat, enum HDMI_VIDEO_INPUT_FORMAT vin,
		      enum HDMI_VIDEO_OUTPUT_FORMAT vout);
bool is_hdmi_active(void);

void hdmi_enable(int enable);
void hdmi_power_enable(int enable);
void hdmi_force_disable(int enable);
void hdmi_force_on(int enable);
bool is_hdmi_active(void);
void hdmi_set_USBOTG_status(int status);
void hdmi_set_audio_enable(int enable);
void hdmi_set_video_enable(int enable);
void hdmi_set_resolution(int res);
void hdmi_init(void);
void hdmi_set_layer_num(int layer_num);
int hdmi_get_dev_info(void *info);
int hdmi_get_capability(void *info);
int hdmi_get_edid(void *edid_info);
int hdmi_screen_capture(void *info);
int hdmi_is_force_awake(void *argp);
int hdmi_factory_mode_test(HDMI_FACTORY_MODE_TEST test_step, void *info);

void extd_hdmi_path_init(EXT_DISP_PATH_MODE mode, int session);
void extd_hdmi_path_deinit(void);
void extd_hdmi_path_resume(void);
void extd_hdmi_path_set_mode(EXT_DISP_PATH_MODE mode);
int extd_hdmi_trigger(int blocking, void *callback, unsigned int session);
int extd_hdmi_config_input_multiple(ext_disp_input_config *input, int idx);
int extd_hdmi_get_dev_info(void *info);
int extd_get_path_handle(disp_path_handle *dp_handle, cmdqRecHandle *pHandle);
int extd_get_device_type(void);
EXT_DISP_PATH_MODE extd_hdmi_path_get_mode(void);

void hdmi_cable_fake_plug_in(void);
void hdmi_cable_fake_plug_out(void);
void hdmi_install_hdcpkey(hdmi_hdcp_key *key);

int hdmi_recompute_bg(int src_w, int src_h);

void hdmi_read_reg(READ_REG_VALUE regval);
void hdmi_write_reg(hdmi_device_write regval);
bool hdmi_is_debug_output(void);

void hdmi_config_pll(unsigned long resolutionmode);
void dpi_setting_res(unsigned long arg);
void hdmi_set_colordeep(unsigned int colorspace, unsigned int deep);
void hdmi_set_audio_param(HDMITX_AUDIO_PARA *audio_param);

void hdmi_cable_fake_plug_out(void);
void hdmi_log_enable(int enable);
void hdmi_cable_fake_plug_in(void);
void hdmi_cable_fake_plug_out(void);
void hdmi_mmp_enable(int enable);
void hdmi_hwc_enable(int enable);


extern void hdmi_force_on(int from_uart_drv);




#endif
