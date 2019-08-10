#ifdef BUILD_LK
#include <string.h>
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#else
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#endif

#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
#define GPIO_LCD_RST_EN      GPIO83
#define GPIO_LCD_LED_EN	     GPIO43
#define GPIO_LCD_LED_EN_N    GPIO45
#else
/*static unsigned int GPIO_LCD_PWR_EN;*/
static struct pinctrl *lcmctrl;
static struct pinctrl_state *lcd_pwr_high;
static struct pinctrl_state *lcd_pwr_low;
static struct pinctrl_state *lcd_pwr_n_high;
static struct pinctrl_state *lcd_pwr_n_low;
static struct pinctrl_state *lcd_rst_high;
static struct pinctrl_state *lcd_rst_low;
static struct regulator *lcm_vio_ldo;


static int lcm_get_vio_supply(struct device *dev)
{
	int ret;


	pr_debug("LCM: lcm_get_vgp_supply is going\n");

	lcm_vio_ldo = devm_regulator_get(dev, "reg-lcm");
	if (IS_ERR(lcm_vio_ldo)) {
		ret = PTR_ERR(lcm_vio_ldo);
		dev_err(dev, "failed to get reg-lcm LDO, %d\n", ret);
		return ret;
	}

	pr_debug("LCM: lcm get supply ok.\n");

	return ret;
}

static int lcm_get_gpio(struct device *dev)
{
	int ret = 0;

	lcmctrl = devm_pinctrl_get(dev);
	if (IS_ERR(lcmctrl)) {
		dev_err(dev, "Cannot find lcm pinctrl!");
		ret = PTR_ERR(lcmctrl);
	}
	/*lcm power pin lookup */
	lcd_pwr_high = pinctrl_lookup_state(lcmctrl, "lcm_pwr_high");
	if (IS_ERR(lcd_pwr_high)) {
		ret = PTR_ERR(lcd_pwr_high);
		pr_info("%s : pinctrl err, lcd_pwr_high\n", __func__);
	}
	lcd_pwr_low = pinctrl_lookup_state(lcmctrl, "lcm_pwr_low");
	if (IS_ERR(lcd_pwr_low)) {
		ret = PTR_ERR(lcd_pwr_low);
		pr_info("%s : pinctrl err, lcd_pwr_low\n", __func__);
	}
	lcd_rst_high = pinctrl_lookup_state(lcmctrl, "lcm_rst_high");
	if (IS_ERR(lcd_rst_high)) {
		ret = PTR_ERR(lcd_rst_high);
		pr_info("%s : pinctrl err, lcd_rst_high\n", __func__);
	}
	lcd_rst_low = pinctrl_lookup_state(lcmctrl, "lcm_rst_low");
	if (IS_ERR(lcd_rst_low)) {
		ret = PTR_ERR(lcd_rst_low);
		pr_info("%s : pinctrl err, lcd_rst_low\n", __func__);
	}

	lcd_pwr_n_high = pinctrl_lookup_state(lcmctrl, "lcm_pwr_n_high");
	if (IS_ERR(lcd_pwr_n_high)) {
		ret = PTR_ERR(lcd_pwr_n_high);
		pr_info("%s : pinctrl err, lcd_pwr_n_high\n", __func__);
	}

	lcd_pwr_n_low = pinctrl_lookup_state(lcmctrl, "lcm_pwr_n_low");
	if (IS_ERR(lcd_pwr_n_low)) {
		ret = PTR_ERR(lcd_pwr_n_low);
		pr_info("%s : pinctrl err, lcd_pwr_n_low\n", __func__);
	}
return ret;
}

static void lcm_set_pwr(int val)
{
	if (val == 0) {
		pinctrl_select_state(lcmctrl, lcd_pwr_low);
	} else {
		pinctrl_select_state(lcmctrl, lcd_pwr_high);
	}
}

static void lcm_set_rst(int val)
{
	if (val == 0) {
		pinctrl_select_state(lcmctrl, lcd_rst_low);
	} else {
		pinctrl_select_state(lcmctrl, lcd_rst_high);
	}
}


static void lcm_set_pwr_n(int val)
{
	if (val == 0 && (!IS_ERR(lcd_pwr_n_low)))  {
		pinctrl_select_state(lcmctrl, lcd_pwr_n_low);
	} else if (val == 1 && (!IS_ERR(lcd_pwr_n_high))) {
		pinctrl_select_state(lcmctrl, lcd_pwr_n_high);
	}
}

static int lcm_probe(struct device *dev)
{
	lcm_get_vio_supply(dev);
	lcm_get_gpio(dev);

	return 0;
}

static const struct of_device_id lcm_of_ids[] = {
	{.compatible = "jd,jd936x",},
	{}
};

static struct platform_driver lcm_driver = {
	.driver = {
		   .name = "jd936x",
		   .owner = THIS_MODULE,
		   .probe = lcm_probe,
#ifdef CONFIG_OF
		   .of_match_table = lcm_of_ids,
#endif
		   },
};

static int __init lcm_drv_init(void)
{
	pr_notice("LCM: Register lcm driver\n");
	if (platform_driver_register(&lcm_driver)) {
		pr_err("LCM: failed to register disp driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit lcm_drv_exit(void)
{
	platform_driver_unregister(&lcm_driver);
	pr_notice("LCM: Unregister lcm driver done\n");
}
late_initcall(lcm_drv_init);
module_exit(lcm_drv_exit);
MODULE_AUTHOR("mediatek");
MODULE_DESCRIPTION("Display subsystem Driver");
MODULE_LICENSE("GPL");
#endif

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#endif

/* ---------------------------------------------------
 *  Local Constants
 * --------------------------------------------------- */

#define FRAME_WIDTH				(800)
#define FRAME_HEIGHT				(1280)

#define REGFLAG_DELAY				0xFE
#define REGFLAG_END_OF_TABLE		0x00   /* END OF REGISTERS MARKER */


/* ID1 DAh - Vendor and Build Designation
 * Bit[7:5] - Vendor Code (KD, TPV)
 * Bit[4:2] - Reserved
 * Bit[2:0] - Build (proto, evt, dvt, etc.)
 */
#define PROTO		0x0
#define EVT1		0x1
#define EVT2		0x2
#define DVT			0x4
#define PVT			0x7
#define MP			0x6
#define BUILD_MASK	0x7


#define FITI_INX			0x2		/* INX, using FITI IC */
#define FITI_TXD			0x3		/* TXD, using FITI IC */
#define FITI_TPV			0x5		/* TPV */
#define FITI_STARRY			0x6		/* STARRY, using FITI IC */
#define FITI_KD				0x7		/* KD*/

/* CABC Mode Selection */
#define OFF			0x0
#define UI			0x1
#define STILL		0x2
#define MOVING		0x3
/* ---------------------------------------------------
 *  Global Variables
 * --------------------------------------------------- */

/* ---------------------------------------------------
 *  Local Variables
 * --------------------------------------------------- */
static unsigned char lcm_id;
static unsigned char vendor_id = 0xFF;
static unsigned char build_id = 0xFF;

static LCM_UTIL_FUNCS lcm_util = {
	.set_reset_pin = NULL,
	.udelay = NULL,
	.mdelay = NULL,
};

static void get_lcm_id(void);

#define SET_RESET_PIN(v)		(lcm_util.set_reset_pin((v)))

#define UDELAY(n)				(lcm_util.udelay(n))
#define MDELAY(n)				(lcm_util.mdelay(n))

/* ---------------------------------------------------
 *  Local Functions
 * --------------------------------------------------- */

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
		 (lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update))
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		 (lcm_util.dsi_set_cmdq(pdata, queue_size, force_update))
#define wrtie_cmd(cmd) \
		 (lcm_util.dsi_write_cmd(cmd))
#define write_regs(addr, pdata, byte_nums) \
		 (lcm_util.dsi_write_regs(addr, pdata, byte_nums))
#define read_reg \
		 (lcm_util.dsi_read_reg())
#define read_reg_v2(cmd, buffer, buffer_size) \
		 (lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size))


#ifdef BUILD_LK
static void power_on(void)
{
	#ifdef BUILD_LK
		dprintf(ALWAYS, "[LK/LCM] power_on() enter\n");
	#else
		pr_info("[jd936x] %s enter\n", __func__);
	#endif

	/* GHGL_EN/GPIO110 - Control AVDD/AVEE/VDD */
	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM] GPIO110 - Control AVDD/AVEE/VDD\n");
	#else
		pr_info("[jd936x] %s, GPIO110 - Control AVDD/AVEE/VDD\n",
				 __func__);
	#endif
#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO_LCD_LED_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_LED_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_LED_EN, GPIO_OUT_ONE);
	mt_set_gpio_mode(GPIO_LCD_LED_EN_N, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_LED_EN_N, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_LED_EN_N, GPIO_OUT_ONE);
#else
	regulator_enable(lcm_vio_ldo);
	lcm_set_pwr(1);
	lcm_set_pwr_n(1);
#endif

	/* GHGL_EN/GPIO110 waveform has a gentle rising,
	 * so enlarge delay time from 40ms to 50ms */
	MDELAY(50);

	/* Fixme - Turn on MIPI lanes after AVEE ready,
	 * before Reset pin Low->High */
	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);
	SET_RESET_PIN(1);

	/* GPIO65 - Reset JD936X */
	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM] GPIO65 - Reset\n");
	#else
		pr_info("[jd936x] %s, GPIO65 - Reset\n", __func__);
	#endif
#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
#else
	lcm_set_rst(1);
#endif

	MDELAY(10);
#ifdef BUILD_LK
	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
#else
	lcm_set_rst(0);
#endif

	MDELAY(10);
#ifdef BUILD_LK
	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
#else
	lcm_set_rst(1);
#endif
	MDELAY(20);
}

#endif

static void init_karnak_fiti_tpv_lcm(void)
{
	unsigned int data_array[64];

	/* Page0 */
	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	/* password */
	data_array[0] = 0x93E11500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x65E21500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xF8E31500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x03801500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Page1 */
	data_array[0] = 0x01E01500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Set vcom */
	data_array[0] = 0x00001500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xB7011500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Set Gamma power */
	data_array[0] = 0x00171500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xCF181500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x01191500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x001A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xCF1B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x011C1500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Set Gate power */
	data_array[0] = 0x3E1F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x28201500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x28211500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0E221500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xC8241500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Set Gate power */
	data_array[0] = 0x29371500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x05381500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x08391500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x123A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x783C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xFF3D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xFF3E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xFF3F1500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Set TCON */
	data_array[0] = 0x06401500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xA0411500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x15431500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x12441500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x50451500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x044B1500;
	dsi_set_cmdq(data_array, 1, 1);
	/* power voltage */
	data_array[0] = 0x0F551500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x01561500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x89571500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0A581500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2A591500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x315A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x155B1500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Gamma */
	data_array[0] = 0x7C5D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x505E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x3B5F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2B601500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x25611500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x15621500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1A631500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x04641500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1C651500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1A661500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x19671500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x36681500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x27691500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2F6A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x236B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x216C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x176D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x056E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x006F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x7C701500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x50711500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x3B721500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2B731500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x25741500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x15751500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1A761500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x04771500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1C781500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1A791500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x197A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x367B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x277C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2F7D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x237E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x217F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x17801500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x05811500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00821500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Page2, for GIP */
	data_array[0] = 0x02E01500;
	dsi_set_cmdq(data_array, 1, 1);
	/* GIP_L Pin mapping */
	data_array[0] = 0x00001500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x04011500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x08021500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x05031500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x09041500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06051500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0A061500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x07071500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0B081500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F091500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F0A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F0B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F0C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F0D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F0E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x170F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x37101500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x10111500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F121500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F131500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F141500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F151500;
	dsi_set_cmdq(data_array, 1, 1);
	/* GIP_R Pin mapping */
	data_array[0] = 0x00161500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x04171500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x08181500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x05191500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x091A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x061B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0A1C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x071D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0B1E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F1F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F201500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F211500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F221500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F231500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F241500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x17251500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x37261500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x10271500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F281500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F291500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F2A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1F2B1500;
	dsi_set_cmdq(data_array, 1, 1);
	/* GIP Timing */
	data_array[0] = 0x01581500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00591500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x005A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x005B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0C5C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x605D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x005E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x005F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x30601500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00611500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00621500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x03631500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x6A641500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x45651500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x14661500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x73671500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x10681500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06691500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x6A6A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x006B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x006C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x036D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x006E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x086F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00701500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00711500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06721500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x7B731500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00741500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x80751500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00761500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x05771500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1B781500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00791500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x007A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x007B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x007C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x037D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x7B7E1500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Page4 */
	data_array[0] = 0x04E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x003F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xFF411500;
	dsi_set_cmdq(data_array, 1, 1);
	/* ESD Function */
	data_array[0] = 0x10091500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2B2B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x442E1500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Page3 */
	data_array[0] = 0x03E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x059B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x20AF1500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Page0 enable CABC */
	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2C531500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Page0 */
	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Watch Dog Function */
	data_array[0] = 0x02E61500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06E71500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00111500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);

	data_array[0] = 0x00291500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(5);

	data_array[0] = 0x00351500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x04EC1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x03E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x81A91500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4FAC1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x33A01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
}

static void init_karnak_fiti_kd_lcm(void)
{
	unsigned int data_array[64];

	/* Page0 */
	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	/* password */
	data_array[0] = 0x93E11500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x65E21500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xF8E31500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x03801500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Page4 */
	data_array[0] = 0x04E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x032D1500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Page1 */
	data_array[0] = 0x01E01500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Set vcom */
	data_array[0] = 0x00001500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x6F011500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00031500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x6F041500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Set Gamma power */
	data_array[0] = 0x00171500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xD7181500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x01191500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x001A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xD71B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x011C1500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Set Gate power */
	data_array[0] = 0x791F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2D201500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2D211500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0F221500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xF1261500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Set Panel */
	data_array[0] = 0x09371500;
	dsi_set_cmdq(data_array, 1, 1);

	/*SET RGBCYC*/
	data_array[0] = 0x04381500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00391500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x013A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x703C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xFF3D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xFF3E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x7F3F1500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Set TCON */
	data_array[0] = 0x06401500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xA0411500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x1E431500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0D441500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x28451500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x044B1500;
	dsi_set_cmdq(data_array, 1, 1);

	/* power voltage */
	data_array[0] = 0x0F551500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x01561500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xA8571500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0A581500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2A591500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x375A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x195B1500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Gamma */
	data_array[0] = 0x785D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x5B5E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x4A5F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x3D601500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x39611500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x29621500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2E631500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x17641500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2F651500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2D661500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2C671500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x45681500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x32691500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x406A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x396B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2C6C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x226D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0E6E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x026F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x78701500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x5B711500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x4A721500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x3D731500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x39741500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x29751500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2E761500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x17771500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2F781500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2D791500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2C7A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x457B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x327C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x407D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x397E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2C7F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x22801500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0E811500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x02821500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Page2, for GIP */
	data_array[0] = 0x02E01500;
	dsi_set_cmdq(data_array, 1, 1);
	/* GIP_L Pin mapping */
	data_array[0] = 0x40001500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x44011500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x46021500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x48031500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x4A041500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x4C051500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x4E061500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x57071500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x77081500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x55091500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x500A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x550B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x550C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x550D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x550E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x550F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x55101500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x55111500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x55121500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x52131500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x55141500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x55151500;
	dsi_set_cmdq(data_array, 1, 1);
	/* GIP_R Pin mapping */
	data_array[0] = 0x41161500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x45171500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x47181500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x49191500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x4B1A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x4D1B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x4F1C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x571D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x771E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x551F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x51201500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x55211500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x55221500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x55231500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x55241500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x55251500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x55261500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x55271500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x55281500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x53291500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x552A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x552B1500;
	dsi_set_cmdq(data_array, 1, 1);

	/*GIP_L_GS Pin mapping*/
	data_array[0] = 0x112C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0F2D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0D2E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0B2F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x09301500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x07311500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x05321500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x17331500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x17341500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x15351500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x01361500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x15371500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x15381500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x15391500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x153A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x153B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x153C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x153D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x153E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x133F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x15401500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x15411500;
	dsi_set_cmdq(data_array, 1, 1);

	/*GIP_R_GS Pin mapping*/
	data_array[0] = 0x10421500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0E431500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0C441500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0A451500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x08461500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06471500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x04481500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x17491500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x174A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x154B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x004C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x154D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x154E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x154F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x15501500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x15511500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x15521500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x15531500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x15541500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x12551500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x15561500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x15571500;
	dsi_set_cmdq(data_array, 1, 1);
	/* GIP Timing */
	data_array[0] = 0x40581500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00591500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x005A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x105B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x065C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x405D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x005E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x005F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x40601500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x03611500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x04621500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x60631500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x60641500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x75651500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0C661500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xB4671500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x08681500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x60691500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x606A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x106B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x006C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x046D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x006E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x886F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00701500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00711500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06721500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x7B731500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00741500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xBC751500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00761500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x0D771500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2C781500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00791500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x007A1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x007B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x007C1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x037D1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x7B7E1500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Page4_ESD ADD*/
	data_array[0] = 0x04E01500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x10091500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x480E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x48271500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2B2B1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x442E1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x003F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xFF411500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Page5 */
	data_array[0] = 0x05E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x72121500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Page3 */
	data_array[0] = 0x03E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x059B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x20AF1500;
	dsi_set_cmdq(data_array, 1, 1);
	/*Page0 enable CABC*/
	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2C531500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Page0 */
	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	/* Watch Dog Function */
	data_array[0] = 0x02E61500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06E71500;
	dsi_set_cmdq(data_array, 1, 1);

	/*SLP OUT*/
	data_array[0] = 0x00111500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);

	data_array[0] = 0x00291500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(5);

	/*TE*/
	data_array[0] = 0x00351500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x04EC1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x03E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x81A91500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4FAC1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x33A01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
}

static void init_karnak_fiti_inx_lcm(void)
{
	unsigned int data_array[64];

	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x93E11500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x65E21500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xF8E31500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x03801500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x01E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00001500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x6E011500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00031500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x82041500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00171500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xD7181500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x01191500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x001A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xD71B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x011C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x791F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2D201500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2D211500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0F221500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x09371500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x04381500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00391500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x013A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x703C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xFF3D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xFF3E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x7F3F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x06401500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xA0411500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1E431500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0D441500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x28451500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x044B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0F551500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x01561500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xA8571500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0A581500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2A591500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x375A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x195B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x785D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x5B5E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4A5F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3D601500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x39611500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x29621500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2E631500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x17641500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2F651500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2D661500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2C671500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x45681500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x32691500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x406A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x396B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2C6C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x226D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0E6E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x026F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x78701500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x5B711500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4A721500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3D731500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x39741500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x29751500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2E761500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x17771500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2F781500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2D791500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2C7A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x457B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x327C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x407D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x397E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2C7F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x22801500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0E811500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x02821500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x02E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x40001500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x44011500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x46021500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x48031500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4A041500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4C051500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4E061500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x57071500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x77081500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x55091500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x500A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x550B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x550C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x550D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x550E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x550F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x55101500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x55111500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x55121500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x52131500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x55141500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x55151500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x41161500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x45171500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x47181500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x49191500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4B1A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4D1B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4F1C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x571D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x771E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x551F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x51201500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x55211500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x55221500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x55231500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x55241500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x55251500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x55261500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x55271500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x55281500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x53291500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x552A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x552B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x112C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0F2D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0D2E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0B2F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x09301500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x07311500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x05321500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x17331500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x17341500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x15351500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x01361500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x15371500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x15381500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x15391500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x153A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x153B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x153C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x153D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x153E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x133F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x15401500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x15411500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x10421500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0E431500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0C441500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0A451500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x08461500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x06471500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x04481500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x17491500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x174A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x154B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x004C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x154D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x154E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x154F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x15501500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x15511500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x15521500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x15531500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x15541500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x12551500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x15561500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x15571500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x40581500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00591500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x005A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x105B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x065C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x405D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x005E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x005F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x40601500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x03611500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x04621500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x60631500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x60641500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x75651500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0C661500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xB4671500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x08681500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x60691500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x606A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x106B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x006C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x046D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x006E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x886F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00701500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00711500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x06721500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x7B731500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00741500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xBC751500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00761500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0D771500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2C781500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00791500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x007A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x007B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x007C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x037D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x7B7E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x04E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x003F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xFF411500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x032D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x10091500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x480E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x48271500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2B2B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x442E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x05E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x72121500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Page3 */
	data_array[0] = 0x03E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x059B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x20AF1500;
	dsi_set_cmdq(data_array, 1, 1);
	/*Page0 enable CABC*/
	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2C531500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x02E61500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x02E71500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00111500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);
	data_array[0] = 0x00291500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(5);
	data_array[0] = 0x00351500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x04EC1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x03E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x81A91500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4FAC1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x33A01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
}

static void init_karnak_fiti_txd_lcm(void)
{
	unsigned int data_array[64];

	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x93E11500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x65E21500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xF8E31500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x03801500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x01E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00001500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x49011500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00031500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xB9041500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00171500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xB1181500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x01191500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x001A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xB11B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x011C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3E1F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x28201500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x28211500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0E221500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x19371500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x05381500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x08391500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x123A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x783C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xFF3D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xFF3E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xFF3F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x06401500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xA0411500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x08431500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x07441500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x40451500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x044B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0F551500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x01561500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x89571500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0A581500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0A591500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x285A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x145B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x7F5D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x6B5E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x5C5F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x50601500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4C611500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3D621500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x41631500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2A641500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x41651500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3E661500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3C671500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x57681500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x43691500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x486A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x386B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x316C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x246D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x156E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0A6F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x7F701500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x6B711500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x5C721500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x50731500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4C741500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3D751500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x41761500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2A771500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x41781500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3E791500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3C7A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x577B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x437C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x487D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x387E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x317F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x24801500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x15811500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0A821500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x02E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x47001500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x47011500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x45021500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x45031500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4B041500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4B051500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x49061500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x49071500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x41081500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F091500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F0A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F0B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F0C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F0D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F0E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x430F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F101500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F111500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F121500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F131500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F141500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F151500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x46161500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x46171500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x44181500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x44191500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4A1A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4A1B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x481C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x481D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x401E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F1F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F201500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F211500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F221500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F231500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F241500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x42251500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F261500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F271500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F281500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F291500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F2A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F2B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x112C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0F2D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0D2E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0B2F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x09301500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x07311500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x05321500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x18331500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x17341500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F351500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x01361500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F371500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F381500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F391500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F3A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F3B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F3C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F3D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F3E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x133F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F401500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F411500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x10421500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0E431500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0C441500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0A451500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x08461500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x06471500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x04481500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x18491500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x174A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F4B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x004C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F4D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F4E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F4F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F501500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F511500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F521500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F531500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F541500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x12551500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F561500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F571500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x40581500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x305B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x085C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x405D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x015E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x025F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x62631500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x62641500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x74671500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0a681500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x62691500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x666A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x086B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x006C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x006D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x006E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x086F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x04E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x003F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xFF411500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x10091500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x480E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2B2B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x442E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x032D1500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Page3 */
	data_array[0] = 0x03E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x059B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x20AF1500;
	dsi_set_cmdq(data_array, 1, 1);

	/*Page0 enable CABC*/
	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2C531500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x02E61500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x06E71500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00111500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);

	data_array[0] = 0x00291500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(5);

	data_array[0] = 0x04EC1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x03E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x81A91500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4FAC1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x33A01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
}

static void init_karnak_fiti_starry_lcm(void)
{
	unsigned int data_array[64];

	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x93E11500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x65E21500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xF8E31500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x03801500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x01E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00001500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00171500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xB1181500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x01191500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x001A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xB11B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x011C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3E1F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x28201500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x28211500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0E221500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x19371500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x05381500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x08391500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x123A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x783C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xFF3D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xFF3E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xFF3F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x06401500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xA0411500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x08431500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x07441500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x40451500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x044B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0F551500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x01561500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x89571500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0A581500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0A591500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x285A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x145B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x7C5D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x665E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x575F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x49601500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x46611500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x38621500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3C631500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x25641500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3E651500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3B661500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3A671500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x55681500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x40691500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x476A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x386B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x316C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x226D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x116E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x006F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x7C701500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x66711500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x57721500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x49731500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x46741500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x38751500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3C761500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x25771500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3E781500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3B791500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x3A7A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x557B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x407C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x477D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x387E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x317F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x22801500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x11811500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00821500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x02E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x47001500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x47011500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x45021500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x45031500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4B041500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4B051500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x49061500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x49071500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x41081500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F091500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F0A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F0B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F0C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F0D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F0E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x430F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F101500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F111500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F121500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F131500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F141500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F151500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x46161500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x46171500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x44181500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x44191500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4A1A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4A1B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x481C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x481D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x401E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F1F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F201500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F211500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F221500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F231500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F241500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x42251500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F261500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F271500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F281500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F291500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F2A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F2B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x112C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0F2D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0D2E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0B2F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x09301500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x07311500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x05321500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x18331500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x17341500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F351500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x01361500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F371500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F381500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F391500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F3A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F3B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F3C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F3D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F3E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x133F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F401500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F411500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x10421500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0E431500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0C441500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x0A451500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x08461500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x06471500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x04481500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x18491500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x174A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F4B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x004C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F4D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F4E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F4F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F501500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F511500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F521500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F531500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F541500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x12551500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F561500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x1F571500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x40581500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x305B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x025C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x405D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x015E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x025F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x62631500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x62641500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x74671500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x04681500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x62691500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x666A1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x086B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x006C1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x006D1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x006E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x086F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x03E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x049B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x20AF1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x04E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x10091500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x480E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2B2B1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x442E1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x003F1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xFF411500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0xFF4B1500;
	dsi_set_cmdq(data_array, 1, 1);

	/* Page3 */
	data_array[0] = 0x03E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x059B1500;
	dsi_set_cmdq(data_array, 1, 1);

	/*Page0 enable CABC*/
	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x2C531500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x02E61500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x06E71500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00110500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(120);
	data_array[0] = 0x00290500;
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(5);

	data_array[0] = 0x04EC1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x03E01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x81A91500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x4FAC1500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x33A01500;
	dsi_set_cmdq(data_array, 1, 1);
	data_array[0] = 0x00E01500;
	dsi_set_cmdq(data_array, 1, 1);

}

/* -----------------------------------------
 *  LCM Driver Implementations
 * ----------------------------------------- */
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;

	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->dsi.mode   = SYNC_EVENT_VDO_MODE;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM				= LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	/* Not support in MT6573 */
	params->dsi.packet_size = 256;

	/* Video mode setting */
	params->dsi.intermediat_buffer_num = 0;

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	/* we can get vendor_id in cmdline passed from lk using key : nt35521_id=xxx */
	get_lcm_id();

	if (vendor_id == FITI_KD) {
		params->dsi.vertical_sync_active		= 4;
		params->dsi.vertical_backporch			= 10;
		params->dsi.vertical_frontporch			= 20;
		params->dsi.vertical_active_line		= FRAME_HEIGHT;

		params->dsi.horizontal_sync_active		= 18;
		params->dsi.horizontal_backporch		= 53;
		params->dsi.horizontal_frontporch		= 52;
		params->dsi.horizontal_active_pixel		= FRAME_WIDTH;
	} else if (vendor_id == FITI_INX) {
		params->dsi.vertical_sync_active		= 4;
		params->dsi.vertical_backporch			= 10;
		params->dsi.vertical_frontporch			= 20;
		params->dsi.vertical_active_line		= FRAME_HEIGHT;

		params->dsi.horizontal_sync_active		= 20;
		params->dsi.horizontal_backporch		= 43;
		params->dsi.horizontal_frontporch		= 43;
		params->dsi.horizontal_active_pixel		= FRAME_WIDTH;
	} else if (vendor_id == FITI_TXD) {
		params->dsi.vertical_sync_active		= 4;
		params->dsi.vertical_backporch			= 10;
		params->dsi.vertical_frontporch			= 20;
		params->dsi.vertical_active_line		= FRAME_HEIGHT;

		params->dsi.horizontal_sync_active		= 18;
		params->dsi.horizontal_backporch		= 53;
		params->dsi.horizontal_frontporch		= 53;
		params->dsi.horizontal_active_pixel		= FRAME_WIDTH;
	} else if (vendor_id == FITI_STARRY) {
		params->dsi.vertical_sync_active		= 4;
		params->dsi.vertical_backporch			= 4;
		params->dsi.vertical_frontporch			= 8;
		params->dsi.vertical_active_line		= FRAME_HEIGHT;

		params->dsi.horizontal_sync_active		= 20;
		params->dsi.horizontal_backporch		= 63;
		params->dsi.horizontal_frontporch		= 63;
		params->dsi.horizontal_active_pixel		= FRAME_WIDTH;
	} else {
		params->dsi.vertical_sync_active		= 4;
		params->dsi.vertical_backporch			= 15;
		params->dsi.vertical_frontporch 		= 21;
		params->dsi.vertical_active_line		= FRAME_HEIGHT;

		params->dsi.horizontal_sync_active		= 26;
		params->dsi.horizontal_backporch		= 40;
		params->dsi.horizontal_frontporch		= 40;
		params->dsi.horizontal_active_pixel 		= FRAME_WIDTH;
	}


	params->dsi.PLL_CLOCK = 234;
	params->dsi.clk_lp_per_line_enable = 1;

	//params->dsi.ssc_disable = 1;
	params->dsi.cont_clock= 0;
	params->dsi.DA_HS_EXIT = 1;
	params->dsi.CLK_ZERO = 16;
	params->dsi.HS_ZERO = 9;
	params->dsi.HS_TRAIL = 5;
	params->dsi.CLK_TRAIL = 5;
	params->dsi.CLK_HS_POST = 8;
	params->dsi.CLK_HS_EXIT = 6;
	/* params->dsi.CLK_HS_PRPR = 1; */

	params->dsi.TA_GO = 8;
	params->dsi.TA_GET = 10;

	params->physical_width = 108;
	params->physical_height = 172;
}

static int __init setup_lcm_id(char *str)
{
	int id;

	if (get_option(&str, &id))
		lcm_id = (unsigned char)id;
	return 0;
}
__setup("nt35521_id=", setup_lcm_id);

static void get_lcm_id(void)
{
	build_id = lcm_id & BUILD_MASK;
	vendor_id = lcm_id >> 5;

	#ifdef BUILD_LK
	dprintf(ALWAYS, "%s, vendor id = 0x%x, board id = 0x%x\n",
		__func__, vendor_id, build_id);
	#else
	pr_info("[jd936x] %s, vendor id = 0x%x, board id = 0x%x\n",
		__func__, vendor_id, build_id);
	#endif
}

static char *lcm_get_build_type(void)
{
	switch (build_id) {
	case PROTO: return "PROTO\0";
	case EVT1: return "EVT1\0";
	case EVT2: return "EVT2\0";
	case DVT: return "DVT\0";
	case PVT: return "PVT\0";
	case MP: return "MP\0";
	default: return "Unknown\0";
	}
}

static char *lcm_get_vendor_type(void)
{
	switch (vendor_id) {
	case FITI_TPV: return "FITI_TPV\0";
	case FITI_KD: return "FITI_KD\0";
	case FITI_INX: return "FITI_INX\0";
	case FITI_TXD: return "FITI_TXD\0";
	case FITI_STARRY: return "FITI_STARRY\0";
	default: return "Unknown\0";
	}
}

static void lcm_reset(void)
{
	/* GPIO65 - Reset JD936X */
	#ifdef BUILD_LK
		dprintf(ALWAYS, "[LK/LCM] GPIO65 - Reset\n");
	#else
		pr_info("[jd936x] %s, GPIO65 - Reset\n", __func__);
	#endif
#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
#else
	lcm_set_rst(1);
#endif
	MDELAY(10);
#ifdef BUILD_LK
	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
#else
	lcm_set_rst(0);
#endif
	MDELAY(2);
#ifdef BUILD_LK
	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ONE);
#else
	lcm_set_rst(1);
#endif
	MDELAY(5);
}


static void lcm_init(void)
{

#ifdef BUILD_LK
	power_on();
#endif


#ifdef BUILD_LK

	dprintf(CRITICAL,
			 "[LK/LCM] %s enter, build type: %s, vendor type: %s\n",
			 __func__,
			 lcm_get_build_type(),
			 lcm_get_vendor_type());


	if (vendor_id == FITI_KD)
		init_karnak_fiti_kd_lcm();
	else if (vendor_id == FITI_INX)
		init_karnak_fiti_inx_lcm();
	else if (vendor_id == FITI_TXD)
		init_karnak_fiti_txd_lcm();
	else if (vendor_id == FITI_STARRY)
		init_karnak_fiti_starry_lcm();
	else
		init_karnak_fiti_tpv_lcm();

#else
	get_lcm_id();

	pr_info("[jd936x] %s enter, skip power_on & init lcm since it's done by lk\n",
			 __func__);
	pr_info("[jd936x] build type: %s, vendor type: %s\n",
			 lcm_get_build_type(),
			 lcm_get_vendor_type());
#endif
}

static void lcm_suspend(void)
{
	#ifdef CONFIG_AMAZON_METRICS_LOG
		char buf[128];
		snprintf(buf, sizeof(buf), "%s:lcd:suspend=1;CT;1:NR", __func__);
		log_to_metrics(ANDROID_LOG_INFO, "LCDEvent", buf);
	#endif

	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM] %s\n", __func__);
	#else
		pr_info("[jd936x] %s\n", __func__);
	#endif

	lcm_set_rst(0);

	MDELAY(120);

}

static void lcm_resume(void)
{
	#ifdef CONFIG_AMAZON_METRICS_LOG
		char buf[128];
		snprintf(buf, sizeof(buf), "%s:lcd:resume=1;CT;1:NR", __func__);
		log_to_metrics(ANDROID_LOG_INFO, "LCDEvent", buf);
	#endif

	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM] %s\n", __func__);
	#else
		pr_info("[jd936x] %s\n", __func__);
	#endif

	MDELAY(5);
	lcm_reset();
	get_lcm_id();

	if(vendor_id == FITI_KD)
		init_karnak_fiti_kd_lcm();
	else if (vendor_id == FITI_INX)
		init_karnak_fiti_inx_lcm();
	else if (vendor_id == FITI_TXD)
		init_karnak_fiti_txd_lcm();
	else if (vendor_id == FITI_STARRY)
		init_karnak_fiti_starry_lcm();
	else
		init_karnak_fiti_tpv_lcm(); /* TPV panel */

}

/* Seperate lcm_resume_power and lcm_reset from power_on func,
 * to meet turn on MIPI lanes after AVEE ready,
 * before Reset pin Low->High */
static void lcm_resume_power(void)
{
	int ret;
	/* GHGL_EN/GPIO110 - Control AVDD/AVEE/VDD,
	 * their sequence rests entirely on NT50357 */
#ifdef BUILD_LK
	dprintf(ALWAYS, "[LK/LCM] GPIO110 - Control AVDD/AVEE/VDD resume_power\n");
	mt_set_gpio_mode(GPIO_LCD_LED_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_LED_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_LED_EN, GPIO_OUT_ONE);
	mt_set_gpio_mode(GPIO_LCD_LED_EN_N, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_LED_EN_N, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_LED_EN_N, GPIO_OUT_ONE);
	/* T6[ms] VDDI ~ MIPI LP11 */
	MDELAY(30);
#else
	pr_info("[jd936x] %s, GPIO110 - Control AVDD/AVEE/VDD\n",
		 __func__);
	ret = regulator_enable(lcm_vio_ldo);
	MDELAY(6);
	lcm_set_pwr(1);
	lcm_set_pwr_n(1);
#endif
}


/* Seperate lcm_suspend_power from lcm_suspend
 * to meet turn off MIPI lanes after after sleep in cmd,
 * then Reset High->Low, GHGL_EN High->Low
 */
static void lcm_suspend_power(void)
{
	int ret;
#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_RST_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
#else
	if (vendor_id != FITI_TPV)
		lcm_set_rst(0);
#endif

	MDELAY(15);

#ifdef BUILD_LK
	mt_set_gpio_mode(GPIO_LCD_LED_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_LED_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_LED_EN, GPIO_OUT_ZERO);
	mt_set_gpio_mode(GPIO_LCD_LED_EN_N, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_LED_EN_N, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_LED_EN_N, GPIO_OUT_ZERO);
#else
	lcm_set_pwr_n(0);
	lcm_set_pwr(0);
	MDELAY(5);
	ret = regulator_disable(lcm_vio_ldo);
#endif

	MDELAY(10);
}

static void lcm_set_backlight(unsigned int level)
{
       if(level > 255)
       {
           pr_debug("%s invalid brightness level=%d\n", __FUNCTION__, level);
	   return ;
       }
       dsi_set_cmdq_V2(0x51, 1, ((unsigned char *)&level), 1);
}

static void lcm_set_backlight_mode(unsigned int mode)
{
       if(mode > 3)
       {
           pr_debug("%s invalid CABC mode=%d\n", __FUNCTION__, mode);
	   return ;
       }
       dsi_set_cmdq_V2(0x55, 1, ((unsigned char *)&mode), 1);
}

LCM_DRIVER jd9366_wxga_dsi_vdo_karnak_fiti_tpv_lcm_drv = {
	.name			= "jd9366_wxga_dsi_vdo_karnak_tpv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.resume_power	= lcm_resume_power,
	.suspend_power	= lcm_suspend_power,
	.set_backlight  = lcm_set_backlight,
	.set_backlight_mode     = lcm_set_backlight_mode,

};

LCM_DRIVER jd9367_wxga_dsi_vdo_karnak_fiti_kd_lcm_drv = {
	.name			= "jd9367_wxga_dsi_vdo_karnak_fiti_kd",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.resume_power	= lcm_resume_power,
	.suspend_power	= lcm_suspend_power,
	.set_backlight  = lcm_set_backlight,
	.set_backlight_mode     = lcm_set_backlight_mode,
};

LCM_DRIVER jd9367_wxga_dsi_vdo_karnak_fiti_inx_lcm_drv = {
	.name			= "jd9367_wxga_dsi_vdo_karnak_fiti_inx",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.resume_power	= lcm_resume_power,
	.suspend_power	= lcm_suspend_power,
	.set_backlight  = lcm_set_backlight,
	.set_backlight_mode     = lcm_set_backlight_mode,
};

LCM_DRIVER jd9366_wxga_dsi_vdo_karnak_fiti_txd_lcm_drv = {
	.name			= "jd9366_wxga_dsi_vdo_karnak_fiti_txd",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.resume_power	= lcm_resume_power,
	.suspend_power	= lcm_suspend_power,
	.set_backlight  = lcm_set_backlight,
	.set_backlight_mode     = lcm_set_backlight_mode,
};

LCM_DRIVER jd9366_wxga_dsi_vdo_karnak_fiti_starry_lcm_drv = {
	.name			= "jd9366_wxga_dsi_vdo_karnak_fiti_starry",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.resume_power	= lcm_resume_power,
	.suspend_power	= lcm_suspend_power,
	.set_backlight  = lcm_set_backlight,
	.set_backlight_mode     = lcm_set_backlight_mode,
};
