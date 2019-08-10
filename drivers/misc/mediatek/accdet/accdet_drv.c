#include <accdet.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

static struct platform_driver accdet_driver;

static int debug_enable_drv = 1;
#define ACCDET_DEBUG_DRV(format, args...) pr_warn(format, ##args)
static long accdet_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return mt_accdet_unlocked_ioctl(cmd, arg);
}

#ifdef CONFIG_COMPAT
static long accdet_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;

	switch (cmd) {
	case ACCDET_INIT:
	case SET_CALL_STATE:
	case GET_BUTTON_STATUS:
		 return mt_accdet_unlocked_ioctl(cmd, arg);
		 break;
	default:
		ACCDET_DEBUG_DRV("[Accdet]accdet_ioctl : default!\n");
		ret = -1;
		break;
	}
	return ret;
}
#endif

static int accdet_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int accdet_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations accdet_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = accdet_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = accdet_compat_ioctl,
#endif
	.open = accdet_open,
	.release = accdet_release,
};

const struct file_operations *accdet_get_fops(void)
{
	return &accdet_fops;
}

static int accdet_probe(struct platform_device *dev)
{
#ifdef CONFIG_MTK_AMZN_ACCDET
	return mt_accdet_probe(dev);
#else
	mt_accdet_probe(dev);
	return 0;
#endif
}

static int accdet_remove(struct platform_device *dev)
{
	mt_accdet_remove();
	return 0;
}

static int accdet_suspend(struct device *device)
{				/* wake up */
#ifndef CONFIG_MTK_AMZN_ACCDET
	mt_accdet_suspend();
#endif
	return 0;
}

static int accdet_resume(struct device *device)
{				/* wake up */
#ifndef CONFIG_MTK_AMZN_ACCDET
	mt_accdet_resume();
#endif
	return 0;
}

/**********************************************************************
//add for IPO-H need update headset state when resume

***********************************************************************/
#ifdef CONFIG_PM
static int accdet_pm_restore_noirq(struct device *device)
{
	mt_accdet_pm_restore_noirq();
	return 0;
}
struct of_device_id accdet_of_match[] = {
	{ .compatible = "mediatek,mt6735-accdet", },
	{ .compatible = "mediatek,mt6755-accdet", },
	{ .compatible = "mediatek,mt6580-accdet", },
	{ .compatible = "mediatek,mt8173-accdet", },
	{ .compatible = "mediatek,mt8163-accdet", },
	{},
};

static const struct dev_pm_ops accdet_pm_ops = {
	.suspend = accdet_suspend,
	.resume = accdet_resume,
	.restore_noirq = accdet_pm_restore_noirq,
};
#endif

static struct platform_driver accdet_driver = {
	.probe = accdet_probe,
	/* .suspend = accdet_suspend, */
	/* .resume = accdet_resume, */
	.remove = accdet_remove,
	.driver = {
			.name = "Accdet_Driver",
#ifndef CONFIG_MTK_AMZN_ACCDET
#ifdef CONFIG_PM
			.pm = &accdet_pm_ops,
#endif
#endif
			.of_match_table = accdet_of_match,
		   },
};

struct platform_driver accdet_driver_func(void)
{
	return accdet_driver;
}

static int accdet_mod_init(void)
{
	int ret = 0;

	ACCDET_DEBUG_DRV("[Accdet]accdet_mod_init begin!\n");
	ret = platform_driver_register(&accdet_driver);
	if (ret)
		ACCDET_DEBUG_DRV("[Accdet]platform_driver_register error:(%d)\n", ret);
	else
		ACCDET_DEBUG_DRV("[Accdet]platform_driver_register done!\n");

	ACCDET_DEBUG_DRV("[Accdet]accdet_mod_init done!\n");
	return ret;

}

static void accdet_mod_exit(void)
{
	ACCDET_DEBUG_DRV("[Accdet]accdet_mod_exit\n");
	platform_driver_unregister(&accdet_driver);

	ACCDET_DEBUG_DRV("[Accdet]accdet_mod_exit Done!\n");
}

#ifndef CONFIG_MTK_AMZN_ACCDET
/*Patch for CR ALPS00804150 & ALPS00804802 PMIC temp not correct issue*/
int accdet_cable_type_state(void)
{
	/* ACCDET_DEBUG("[ACCDET] accdet_cable_type_state=%d\n",accdet_get_cable_type()); */
	return accdet_get_cable_type();
}
EXPORT_SYMBOL(accdet_cable_type_state);
/*Patch for CR ALPS00804150 & ALPS00804802 PMIC temp not correct issue*/
#endif

module_init(accdet_mod_init);
module_exit(accdet_mod_exit);

module_param(debug_enable_drv, int, 0644);

MODULE_DESCRIPTION("MTK MT6588 ACCDET driver");
MODULE_AUTHOR("Anny <Anny.Hu@mediatek.com>");
MODULE_LICENSE("GPL");
