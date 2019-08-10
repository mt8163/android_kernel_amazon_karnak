/*****************************************************************************/
/*****************************************************************************/
#if defined(CONFIG_MTK_HDMI_SUPPORT)
#define _tx_c_

#include <linux/mm.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/rtpm_prio.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/switch.h>

#include <asm/uaccess.h>
#include <asm/atomic.h>
/* #include <asm/mach-types.h> */
#include <linux/types.h>

#include "extd_utils.h"
#include "mtk_ext_disp_mgr.h"
/*#include <linux/earlysuspend.h> */
#include <linux/suspend.h>


#define HDMI_DEVNAME "hdmitx"
#define EXTD_DEV_ID(id) (((id)>>16)&0x0ff)
#define EXTD_DEV_PARAM(id) ((id)&0x0ff)


static dev_t hdmi_devno;
static struct cdev *hdmi_cdev;
static struct class *hdmi_class;


static char *_hdmi_ioctl_spy(unsigned int cmd)
{
	switch (cmd) {
	case MTK_HDMI_AUDIO_VIDEO_ENABLE:
		return "MTK_HDMI_AUDIO_VIDEO_ENABLE";

	case MTK_HDMI_AUDIO_ENABLE:
		return "MTK_HDMI_AUDIO_ENABLE";

	case MTK_HDMI_VIDEO_ENABLE:
		return "MTK_HDMI_VIDEO_ENABLE";

	case MTK_HDMI_GET_CAPABILITY:
		return "MTK_HDMI_GET_CAPABILITY";

	case MTK_HDMI_GET_DEVICE_STATUS:
		return "MTK_HDMI_GET_DEVICE_STATUS";

	case MTK_HDMI_VIDEO_CONFIG:
		return "MTK_HDMI_VIDEO_CONFIG";

	case MTK_HDMI_AUDIO_CONFIG:
		return "MTK_HDMI_AUDIO_CONFIG";

	case MTK_HDMI_FORCE_FULLSCREEN_ON:
		return "MTK_HDMI_FORCE_FULLSCREEN_ON";

	case MTK_HDMI_FORCE_FULLSCREEN_OFF:
		return "MTK_HDMI_FORCE_FULLSCREEN_OFF";

	case MTK_HDMI_IPO_POWEROFF:
		return "MTK_HDMI_IPO_POWEROFF";

	case MTK_HDMI_IPO_POWERON:
		return "MTK_HDMI_IPO_POWERON";

	case MTK_HDMI_POWER_ENABLE:
		return "MTK_HDMI_POWER_ENABLE";

	case MTK_HDMI_PORTRAIT_ENABLE:
		return "MTK_HDMI_PORTRAIT_ENABLE";

	case MTK_HDMI_FORCE_OPEN:
		return "MTK_HDMI_FORCE_OPEN";

	case MTK_HDMI_FORCE_CLOSE:
		return "MTK_HDMI_FORCE_CLOSE";

	case MTK_HDMI_IS_FORCE_AWAKE:
		return "MTK_HDMI_IS_FORCE_AWAKE";

	case MTK_HDMI_POST_VIDEO_BUFFER:
		return "MTK_HDMI_POST_VIDEO_BUFFER";

	case MTK_HDMI_FACTORY_MODE_ENABLE:
		return "MTK_HDMI_FACTORY_MODE_ENABLE";

	case MTK_HDMI_WRITE_DEV:
		return "MTK_HDMI_WRITE_DEV";

	case MTK_HDMI_READ_DEV:
		return "MTK_HDMI_READ_DEV";

	case MTK_HDMI_ENABLE_LOG:
		return "MTK_HDMI_ENABLE_LOG";

	case MTK_HDMI_CHECK_EDID:
		return "MTK_HDMI_CHECK_EDID";

	case MTK_HDMI_INFOFRAME_SETTING:
		return "MTK_HDMI_INFOFRAME_SETTING";

	case MTK_HDMI_ENABLE_HDCP:
		return "MTK_HDMI_ENABLE_HDCP";

	case MTK_HDMI_STATUS:
		return "MTK_HDMI_STATUS";

	case MTK_HDMI_HDCP_KEY:
		return "MTK_HDMI_HDCP_KEY";

	case MTK_HDMI_GET_EDID:
		return "MTK_HDMI_GET_EDID";

	case MTK_HDMI_SETLA:
		return "MTK_HDMI_SETLA";

	case MTK_HDMI_GET_CECCMD:
		return "MTK_HDMI_GET_CECCMD";

	case MTK_HDMI_SET_CECCMD:
		return "MTK_HDMI_SET_CECCMD";

	case MTK_HDMI_CEC_ENABLE:
		return "MTK_HDMI_CEC_ENABLE";

	case MTK_HDMI_GET_CECADDR:
		return "MTK_HDMI_GET_CECADDR";

	case MTK_HDMI_CECRX_MODE:
		return "MTK_HDMI_CECRX_MODE";

	case MTK_HDMI_SENDSLTDATA:
		return "MTK_HDMI_SENDSLTDATA";

	case MTK_HDMI_GET_SLTDATA:
		return "MTK_HDMI_GET_SLTDATA";

	case MTK_HDMI_COLOR_DEEP:
		return "MTK_HDMI_COLOR_DEEP";

	case MTK_HDMI_GET_DEV_INFO:
		return "MTK_HDMI_GET_DEV_INFO";

	case MTK_HDMI_PREPARE_BUFFER:
		return "MTK_HDMI_PREPARE_BUFFER";

	case MTK_HDMI_FACTORY_GET_STATUS:
		return "MTK_HDMI_FACTORY_GET_STATUS";

	case MTK_HDMI_FACTORY_DPI_TEST:
		return "MTK_HDMI_FACTORY_DPI_TEST";

	case MTK_HDMI_SCREEN_CAPTURE:
		return "MTK_HDMI_SCREEN_CAPTURE";

	default:
		return "unknown ioctl command";
	}
}

static void external_display_enable(unsigned long param)
{
	EXTD_DEV_ID device_id = EXTD_DEV_ID(param);
	int enable = EXTD_DEV_PARAM(param);

	switch (device_id) {
	case DEV_MHL:
		hdmi_enable(enable);
		break;
	case DEV_WFD:
		/*enable or disable wifi display */
		break;
	default:
		break;
	}
}

static void external_display_power_enable(unsigned long param)
{
	EXTD_DEV_ID device_id = EXTD_DEV_ID(param);
	int enable = EXTD_DEV_PARAM(param);

	switch (device_id) {
	case DEV_MHL:
		hdmi_power_enable(enable);
		break;
	case DEV_WFD:
		/*enable or disable wifi display power */
		break;
	default:
		break;
	}
}

#if 0
static void external_display_force_disable(unsigned long param)
{
	EXTD_DEV_ID device_id = EXTD_DEV_ID(param);
	int enable = EXTD_DEV_PARAM(param);

	switch (device_id) {
	case DEV_MHL:
		hdmi_force_disable(enable);
		break;
	case DEV_WFD:
		/*enable or disable wifi display power */
		break;
	default:
		break;
	}
}
#endif
static void external_display_set_resolution(unsigned long param)
{
	EXTD_DEV_ID device_id = EXTD_DEV_ID(param);
	int res = EXTD_DEV_PARAM(param);

	switch (device_id) {
	case DEV_MHL:
		hdmi_set_resolution(res);
		break;
	case DEV_WFD:
		/*enable or disable wifi display power */
		break;
	default:
		break;
	}
}

static void external_display_set_audio_param(HDMITX_AUDIO_PARA *audio_param)
{
	hdmi_set_audio_param(audio_param);

}

static void external_display_set_deepcolor(unsigned int colorspace, unsigned int deep)
{
	hdmi_set_colordeep(colorspace, deep);

}




void external_display_debug_output(unsigned long param)
{
	hdmi_config_pll(param);
	dpi_setting_res(param);
	hdmi_internal_video_config(param, 0, 0);
}


static int external_display_get_dev_info(unsigned long param, void *info)
{
	int ret = 0;
	EXTD_DEV_ID device_id = EXTD_DEV_ID(param);

	switch (device_id) {
	case DEV_MHL:
		ret = hdmi_get_dev_info(info);
		break;
	case DEV_WFD:
		/*enable or disable wifi display power */
		break;
	default:
		break;
	}

	return ret;
}

static int external_display_get_capability(unsigned long param, void *info)
{
	int ret = 0;
	EXTD_DEV_ID device_id = EXTD_DEV_ID(param);

	device_id = DEV_MHL;
	switch (device_id) {
	case DEV_MHL:
		ret = hdmi_get_capability(info);
		break;
	case DEV_WFD:
		/*enable or disable wifi display power */
		break;
	default:
		break;
	}

	return ret;
}

static int external_display_read_reg(READ_REG_VALUE reg_val)
{
	hdmi_read_reg(reg_val);
	return 0;
}

static int external_display_write_reg(hdmi_device_write reg_val)
{
	hdmi_write_reg(reg_val);
	return 0;
}



static long mtk_ext_disp_mgr_ioctl_compat(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
#if 0
	case COMPAT_MTK_HDMI_AUDIO_SETTING:
		{
			struct COMPAT_HDMITX_AUDIO_PARA __user *data32;	/* userspace passed argument */
			HDMITX_AUDIO_PARA __user *data;	/* kernel used */

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));

			if (data == NULL)
				return -EFAULT;

			/*For hdmi_audio_setting, unsigned char type dont need to be convert */
			ret =
			    file->f_op->unlocked_ioctl(file, MTK_HDMI_AUDIO_SETTING,
						       (unsigned long)data32);
			return ret;
		}
	case COMPAT_MTK_HDMI_GET_EDID:
		{
			struct COMPAT_HDMI_EDID_T __user *data32;

			HDMI_EDID_T __user *data;
			int err = 0;

			data32 = compat_ptr(arg);	/* userspace passed argument */
			data = compat_alloc_user_space(sizeof(*data));

			if (data == NULL)
				return -EFAULT;

			ret =
			    file->f_op->unlocked_ioctl(file, MTK_HDMI_GET_EDID,
						       (unsigned long)data);
			err = compat_put_edid(data32, data);

			return ret ? ret : err;
		}
	default:
		HDMI_LOG("Unknown ioctl compat\n");
		break;
#endif

	}
	return ret;
}


static long mtk_ext_disp_mgr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int r = 0;
	READ_REG_VALUE regval;
	hdmi_para_setting data_info;
#if    defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
	hdmi_device_write w_info;
#endif

#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT) \
	&& defined(CONFIG_MTK_DRM_KEY_MNG_SUPPORT))
	hdmi_hdcp_drmkey key;
#else
	hdmi_hdcp_key key;
#endif

#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	struct HDMIDRV_AUDIO_PARA audio_para;
#endif



	pr_debug("[EXTD]ioctl= %s(%d), arg = %lu\n", _hdmi_ioctl_spy(cmd), cmd & 0xff, arg);

	switch (cmd) {
	case MTK_HDMI_AUDIO_VIDEO_ENABLE:
		{
			/* 0xXY
			 * the low 16 bits(Y) are for disable and enable, and the high 16 bits(X) are for device id
			 * the device id:
			 * X = 0 - mhl
			 * X = 1 - wifi display
			 */
			external_display_enable(arg);
			break;
		}

	case MTK_HDMI_POWER_ENABLE:
		{
			/* 0xXY
			 * the low 16 bits(Y) are for disable and enable, and the high 16 bits(X) are for device id
			 * the device id:
			 * X = 0 - mhl
			 * X = 1 - wifi display
			 */
			external_display_power_enable(arg);
			break;
		}

	case MTK_HDMI_VIDEO_CONFIG:
		{
			/* 0xXY
			 * the low 16 bits(Y) are for disable and enable, and the high 16 bits(X) are for device id
			 * the device id:
			 * X = 0 - mhl
			 * X = 1 - wifi display
			 */
			if (hdmi_is_debug_output()) {
				pr_err("[EXTD] Debug Output\n");
				external_display_debug_output(arg);
			} else {
				pr_err("[EXTD] Normal Output\n");
				external_display_set_resolution(arg);
			}
			break;
		}


#if defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT) || defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT)
	case MTK_HDMI_AUDIO_SETTING:
		{
			if (copy_from_user(&audio_para, (void __user *)arg, sizeof(audio_para))) {
				pr_err("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				external_display_set_audio_param((HDMITX_AUDIO_PARA *) &
								 audio_para);
			}
			break;
		}
#endif


	case MTK_HDMI_COLOR_DEEP:
		{
			if (copy_from_user(&data_info, (void __user *)arg, sizeof(data_info))) {
				pr_err("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {

				pr_err("MTK_HDMI_COLOR_DEEP: %d %d\n", data_info.u4Data1,
				       data_info.u4Data2);
				external_display_set_deepcolor(data_info.u4Data1 & 0xFF,
							       data_info.u4Data2 & 0xFF);
			}
#if (defined(CONFIG_MTK_INTERNAL_MHL_SUPPORT) || defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT))
			/* hdmi_colorspace = (unsigned char)data_info.u4Data1; */
			/* DPI_CHECK_RET(HDMI_DPI(_Config_ColorSpace)(hdmi_colorspace, hdmi_res)); */
#endif
			break;
		}

	case MTK_HDMI_FORCE_FULLSCREEN_ON:
		/* case MTK_HDMI_FORCE_CLOSE: */
		{
			/* 0xXY
			 * the low 16 bits(Y) are for disable and enable, and the high 16 bits(X) are for device id
			 * the device id:
			 * X = 0 - mhl
			 * X = 1 - wifi display
			 */
			arg = arg | 0x1;
			external_display_power_enable(arg);
			break;
		}

	case MTK_HDMI_FORCE_FULLSCREEN_OFF:
		/* case MTK_HDMI_FORCE_OPEN: */
		{
			/* 0xXY
			 * the low 16 bits(Y) are for disable and enable, and the high 16 bits(X) are for device id
			 * the device id:
			 * X = 0 - mhl
			 * X = 1 - wifi display
			 */
			arg = arg & 0x0FF0000;
			external_display_power_enable(arg);
			break;
		}
#if    defined(CONFIG_MTK_INTERNAL_HDMI_SUPPORT)
	case MTK_HDMI_WRITE_DEV:
		{
			if (copy_from_user(&w_info, (void __user *)arg, sizeof(w_info))) {
				pr_debug("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				external_display_write_reg(w_info);
			}
			break;
		}
#endif

	case MTK_HDMI_READ_DEV:
		{
			if (copy_from_user(&regval, (void __user *)arg, sizeof(regval))) {
				pr_debug("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				/* hdmi_drv->read(regval.u1address, &regval.pu1Data); */
				external_display_read_reg(regval);
			}

			if (copy_to_user((void __user *)arg, &regval, sizeof(regval))) {
				pr_debug("copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}
			break;
		}

	case MTK_HDMI_GET_DEV_INFO:
		{
			/* 0xXY
			 * the low 16 bits(Y) are for disable and enable, and the high 16 bits(X) are for device id
			 * the device id:
			 * X = 0 - mhl
			 * X = 1 - wifi display
			 */
			r = external_display_get_dev_info(*((unsigned long *)argp), argp);
			break;
		}

	case MTK_HDMI_USBOTG_STATUS:
		{
			/* hdmi_set_USBOTG_status(arg); */
			break;
		}

	case MTK_HDMI_AUDIO_ENABLE:
		{
			pr_debug("[EXTD]hdmi_set_audio_enable, arg = %lu\n", arg);
			hdmi_set_audio_enable(arg);
			break;
		}

	case MTK_HDMI_VIDEO_ENABLE:
		{
			break;
		}

	case MTK_HDMI_AUDIO_CONFIG:
		{
			break;
		}

	case MTK_HDMI_IS_FORCE_AWAKE:
		{
			r = hdmi_is_force_awake(argp);
			break;
		}

	case MTK_HDMI_GET_EDID:
		{
			hdmi_get_edid(argp);
			break;
		}

	case MTK_HDMI_GET_CAPABILITY:
		{
			/* 0xXY
			 * the low 16 bits(Y) are for disable and enable, and the high 16 bits(X) are for device id
			 * the device id:
			 * X = 0 - mhl
			 * X = 1 - wifi display
			 */
			r = external_display_get_capability(*((unsigned long *)argp), argp);
			break;
		}

	case MTK_HDMI_SCREEN_CAPTURE:
		{
			r = hdmi_screen_capture(argp);
			break;
		}

	case MTK_HDMI_FACTORY_MODE_ENABLE:
		{
			r = hdmi_factory_mode_test(STEP1_ENABLE, NULL);
			break;
		}

	case MTK_HDMI_FACTORY_GET_STATUS:
		{
			r = hdmi_factory_mode_test(STEP2_GET_STATUS, argp);
			break;
		}

	case MTK_HDMI_FACTORY_DPI_TEST:
		{
			r = hdmi_factory_mode_test(STEP3_START, NULL);
			break;
		}
#if 0
	case MTK_HDMI_FAKE_PLUG_IN:
		{
			if (arg)
				hdmi_cable_fake_plug_in();
			else
				hdmi_cable_fake_plug_out();
			break;
		}
#endif
	case MTK_HDMI_HDCP_KEY:
		{
			if (copy_from_user(&key, (void __user *)arg, sizeof(key))) {
				pr_err("copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				hdmi_install_hdcpkey(&key);
			}
			break;
		}
	default:
		{
			pr_debug("[EXTD]ioctl(%d) arguments is not support\n", cmd & 0x0ff);
			r = -EFAULT;
			break;
		}
	}

	pr_debug("[EXTD]ioctl = %s(%d) done\n", _hdmi_ioctl_spy(cmd), cmd & 0x0ff);
	return r;
}

static int mtk_ext_disp_mgr_open(struct inode *inode, struct file *file)
{
	pr_debug("[EXTD]%s\n", __func__);
	return 0;
}

static int mtk_ext_disp_mgr_release(struct inode *inode, struct file *file)
{
	pr_debug("[EXTD]%s\n", __func__);
	return 0;
}


#if CONFIG_COMPAT
static long mtk_ext_disp_mgr_ioctl_compat(struct file *file, unsigned int cmd, unsigned long arg);
#endif


const struct file_operations external_display_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mtk_ext_disp_mgr_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mtk_ext_disp_mgr_ioctl_compat,
#endif
	.open = mtk_ext_disp_mgr_open,
	.release = mtk_ext_disp_mgr_release,
};

static const struct of_device_id hdmi_of_ids[] = {
	{.compatible = "mediatek,mt8163-hdmitx",},
	{},
};

static int mtk_ext_disp_mgr_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct class_device *class_dev = NULL;

	pr_debug("[EXTD]%s\n", __func__);

	/* Allocate device number for hdmi driver */
	ret = alloc_chrdev_region(&hdmi_devno, 0, 1, HDMI_DEVNAME);

	if (ret) {
		pr_debug("[EXTD]alloc_chrdev_region fail\n");
		return -1;
	}

	/* For character driver register to system, device number binded to file operations */
	hdmi_cdev = cdev_alloc();
	hdmi_cdev->owner = THIS_MODULE;
	hdmi_cdev->ops = &external_display_fops;
	ret = cdev_add(hdmi_cdev, hdmi_devno, 1);

	/* For device number binded to device name(hdmitx), one class is corresponeded to one node */
	hdmi_class = class_create(THIS_MODULE, HDMI_DEVNAME);
	/* mknod /dev/hdmitx */
	class_dev =
	    (struct class_device *)device_create(hdmi_class, NULL, hdmi_devno, NULL, HDMI_DEVNAME);

#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT
	ret = hdmi_internal_probe(pdev);
	if (ret) {
		pr_err("Fail to probe internal hdmi\n");
		return -1;
	}
#endif

	pr_debug("[EXTD][%s] out\n", __func__);

	return 0;
}

static int mtk_ext_disp_mgr_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM
int hdmi_pm_suspend(struct device *device)
{
	pr_debug("hdmi_pm_suspend()\n");

	return 0;
}

int hdmi_pm_resume(struct device *device)
{
	pr_debug("HDMI_Npm_resume()\n");

	return 0;
}

const struct dev_pm_ops hdmi_pm_ops = {
	.suspend = hdmi_pm_suspend,
	.resume = hdmi_pm_resume,
};
#endif

#if 0
static struct platform_device external_display_device = {
	.name = HDMI_DEVNAME,
	.id = 0,
};
#endif
static struct platform_driver external_display_driver = {
	.probe = mtk_ext_disp_mgr_probe,
	.remove = mtk_ext_disp_mgr_remove,
	.driver = {
		   .name = HDMI_DEVNAME,

		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &hdmi_pm_ops,
#endif
		   .of_match_table = hdmi_of_ids,
		   }
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void hdmi_early_suspend(struct early_suspend *h)
{
	pr_debug(" hdmi_early_suspend\n");
	hdmi_power_enable(0);
	hdmi_factory_mode_test(STEP1_ENABLE, NULL);
	hdmi_factory_mode_test(STEP3_SUSPEND, NULL);
}

static void hdmi_late_resume(struct early_suspend *h)
{
	/* /pr_debug(" hdmi_late_resume\n"); */
	hdmi_power_enable(1);
}

static struct early_suspend hdmi_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1,
	.suspend = hdmi_early_suspend,
	.resume = hdmi_late_resume,
};
#endif

static int __init mtk_ext_disp_mgr_init(void)
{
	pr_err("[EXTD]%s\n", __func__);
#if 0
	if (platform_device_register(&external_display_device)) {
		pr_debug("[EXTD]failed to register hdmitx device\n");
		return -1;
	}
#endif

	if (platform_driver_register(&external_display_driver)) {
		pr_debug("[EXTD]failed to register hdmitx driver\n");
		return -1;
	}

	hdmi_init();

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&hdmi_early_suspend_handler);
#endif
	pr_err("<< mtk_ext_disp_mgr_init\n");

	return 0;
}

static void __exit mtk_ext_disp_mgr_exit(void)
{

	device_destroy(hdmi_class, hdmi_devno);
	class_destroy(hdmi_class);
	cdev_del(hdmi_cdev);
	unregister_chrdev_region(hdmi_devno, 1);

}
module_init(mtk_ext_disp_mgr_init);
module_exit(mtk_ext_disp_mgr_exit);
MODULE_AUTHOR("www.mediatek.com>");
MODULE_DESCRIPTION("External Display Driver");
MODULE_LICENSE("GPL");

#endif
