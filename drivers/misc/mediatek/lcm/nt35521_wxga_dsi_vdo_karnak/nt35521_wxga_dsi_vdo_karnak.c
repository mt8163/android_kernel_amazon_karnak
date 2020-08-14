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
		pr_info("failed to get reg-lcm LDO, %d\n", ret);
		return ret;
	}

	pr_debug("LCM: lcm get supply ok.\n");

	ret = regulator_enable(lcm_vio_ldo);

	return ret;
}

static int lcm_get_gpio(struct device *dev)
{
	int ret = 0;

	lcmctrl = devm_pinctrl_get(dev);
	if (IS_ERR(lcmctrl)) {
		pr_info("Cannot find lcm pinctrl!");
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
	if (val == 0)
		pinctrl_select_state(lcmctrl, lcd_pwr_low);
	else
		pinctrl_select_state(lcmctrl, lcd_pwr_high);
}

static void lcm_set_rst(int val)
{
	if (val == 0)
		pinctrl_select_state(lcmctrl, lcd_rst_low);
	else
		pinctrl_select_state(lcmctrl, lcd_rst_high);
}

static void lcm_set_pwr_n(int val)
{
	if (val == 0 && (!IS_ERR(lcd_pwr_n_low)))
		pinctrl_select_state(lcmctrl, lcd_pwr_n_low);
	else if (val == 1 && (!IS_ERR(lcd_pwr_n_high)))
		pinctrl_select_state(lcmctrl, lcd_pwr_n_high);
}

static int lcm_driver_probe(struct device *dev, void const *data)
{
	lcm_get_vio_supply(dev);
	lcm_get_gpio(dev);

	return 0;
}

static const struct of_device_id lcm_of_ids[] = {
	{.compatible = "nt,nt35521",},
	{}
};

static int lcm_platform_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;

	id = of_match_node(lcm_of_ids, pdev->dev.of_node);
	if (!id)
		return -ENODEV;

	return lcm_driver_probe(&pdev->dev, id->data);
}

static struct platform_driver lcm_driver = {
	.probe = lcm_platform_probe,
	.driver = {
		   .name = "nt35521",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = lcm_of_ids,
#endif
		   },
};

static int __init lcm_drv_init(void)
{
	pr_notice("LCM: Register lcm driver\n");
	if (platform_driver_register(&lcm_driver)) {
		pr_info("LCM: failed to register disp driver\n");
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
 * ---------------------------------------------------
 */

#define FRAME_WIDTH				(800)
#define FRAME_HEIGHT				(1280)

#define REGFLAG_DELAY				0xFE
#define REGFLAG_END_OF_TABLE		0x00   /* END OF REGISTERS MARKER */

/* ID1 DAh - Vendor and Build Designation
 * Bit[7:5] - Vendor Code (Innolux, LGD, Samsung)
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

#define KD			0x1		/* KD */

/* CABC Mode Selection */
#define OFF			0x0
#define UI			0x1
#define STILL		0x2
#define MOVING		0x3
/* ---------------------------------------------------
 *  Global Variables
 * ---------------------------------------------------
 */

/* ---------------------------------------------------
 *  Local Variables
 * ---------------------------------------------------
 */
static unsigned char lcm_id;
static unsigned char vendor_id = 0xFF;
static unsigned char build_id = 0xFF;

static struct LCM_UTIL_FUNCS lcm_util = { 0 };

static void get_lcm_id(void);

#define SET_RESET_PIN(v) (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

/* ---------------------------------------------------
 *  Local Functions
 * ---------------------------------------------------
 */

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
		pr_info("[nt35521] %s enter\n", __func__);
	#endif

	/* GHGL_EN/GPIO110 - Control AVDD/AVEE/VDD */
	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM] GPIO110 - Control AVDD/AVEE/VDD\n");
	#else
		pr_info("[nt35521] %s, GPIO110 - Control AVDD/AVEE/VDD\n",
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
	 * so enlarge delay time from 40ms to 50ms
	 */
	MDELAY(50);

	/* Fixme - Turn on MIPI lanes after AVEE ready,
	 * before Reset pin Low->High
	 */
	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);
	SET_RESET_PIN(1);

	/* GPIO65 - Reset NT35521 */
	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM] GPIO65 - Reset\n");
	#else
		pr_info("[nt35521] %s, GPIO65 - Reset\n", __func__);
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

static void init_karnak_kd_lcm(void)
{
	unsigned int data_array[64];

	data_array[0] = 0x00053902;
	data_array[1] = 0xA555AAFF;
	data_array[2] = 0x00000080;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x0000116F;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000020F7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x066F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xA0F71500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x196F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x12F71500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000008;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00042902;
	/*data_array[1] = 0xFF0803D9;//26.67KHz */
	data_array[1] = 0x200903D9;//23.97KHz
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x80C81500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00016CB1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x08B61500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x026F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x08B81500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x004474BB;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000000BC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x0CB002BD;
	data_array[2] = 0x0000000E;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000108;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000D0DB0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000D0DB1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000000B2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000190BC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000190BD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00CA1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x04C01500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003737B3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001919B4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x005555B9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003535BA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000208;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x01EE1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x150609EF;
	data_array[2] = 0x00000018;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00073902;
	data_array[1] = 0x000000B0;
	data_array[2] = 0x00520034;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x066F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00073902;
	data_array[1] = 0x006300B0;
	data_array[2] = 0x00A9007B;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x0C6F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x01CD00B0;
	data_array[2] = 0x00000009;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00073902;
	data_array[1] = 0x013901B1;
	data_array[2] = 0x00BA0181;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x066F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00073902;
	data_array[1] = 0x021302B1;
	data_array[2] = 0x005D025A;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x0C6F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x02A602B1;
	data_array[2] = 0x000000F6;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00073902;
	data_array[1] = 0x032603B2;
	data_array[2] = 0x0083035F;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x066F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00073902;
	data_array[1] = 0x03A003B2;
	data_array[2] = 0x00C403B3;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x0C6F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x03CD03B2;
	data_array[2] = 0x000000E4;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x03FC03B3;
	data_array[2] = 0x000000FF;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000608;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001000B0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001412B1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001816B2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00291AB3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00082AB4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131B5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131B6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131B7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000A31B8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131B9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131BA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00310BBB;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131BC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131BD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131BE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002A09BF;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001B29C0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001719C1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001315C2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000111C3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131E5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001B09C4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001719C5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001315C6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002911C7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00012AC8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131C9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131CA;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131CB;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000B31CC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131CD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131CE;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00310ACF;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131D0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131D1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131D2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x002A00D3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001029D4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001412D5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x001816D6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x00081AD7;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003131E6;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000000D8;
	data_array[2] = 0x00000054;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x001500D9;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00E71500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000308;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000020B0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000020B1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000005B2;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000005B6;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000005B7;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000057BA;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x000057BB;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x000000C0;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = 0x000000C1;
	data_array[2] = 0x00000000;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x60C41500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x40C51500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x52AA55F0;
	data_array[2] = 0x00000508;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x030103BD;
	data_array[2] = 0x00000303;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B0;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B1;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B2;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B3;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B4;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x000617B5;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00B81500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00B91500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00BA1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x02BB1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00BC1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x07C01500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x80C41500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xA4C51500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003005C8;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00033902;
	data_array[1] = 0x003101C9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
	data_array[1] = 0x3C0000CC;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00043902;
	data_array[1] = 0x3C0000CD;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x070500D1;
	data_array[2] = 0x00001007;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00063902;
	data_array[1] = 0x0D0500D2;
	data_array[2] = 0x00001007;
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x06E51500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06E61500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06E71500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06E81500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06E91500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x06EA1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x30ED1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x116F1500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x01F31500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0xFF511500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x2C531500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00350500;
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00110500;
	dsi_set_cmdq(data_array, 1, 1);

	MDELAY(120);

	data_array[0] = 0x00290500;
	dsi_set_cmdq(data_array, 1, 1);
}

/* -----------------------------------------
 *  LCM Driver Implementations
 * -----------------------------------------
 */
static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

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

	params->dsi.vertical_sync_active		= 4;
	params->dsi.vertical_backporch			= 40;
	params->dsi.vertical_frontporch			= 40;
	params->dsi.vertical_active_line		= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active		= 4;
	params->dsi.horizontal_backporch		= 32;
	params->dsi.horizontal_frontporch		= 32;
	params->dsi.horizontal_active_pixel		= FRAME_WIDTH;

	params->dsi.PLL_CLOCK = 234;
	params->dsi.clk_lp_per_line_enable = 1;

	//params->dsi.ssc_disable = 1;
	params->dsi.cont_clock = 0;
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
	pr_info("[nt35521] %s, vendor id = 0x%x, board id = 0x%x\n",
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
	case KD: return "KD\0";
	default: return "Unknown\0";
	}
}

static void lcm_reset(void)
{
	/* GPIO65 - Reset NT35521 */
	#ifdef BUILD_LK
		dprintf(ALWAYS, "[LK/LCM] GPIO65 - Reset\n");
	#else
		pr_info("[nt35521] %s, GPIO65 - Reset\n", __func__);
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

static void lcm_init(void)
{
#ifdef BUILD_LK
	power_on();
#endif

#ifdef BUILD_LK
	dprintf(CRITICAL,
		"[LK/LCM] %s enter, build type: %s, vendor type: %s\n",
		__func__, lcm_get_build_type(), lcm_get_vendor_type());
	init_karnak_kd_lcm(); /* KD panel */
#else
	get_lcm_id();

	pr_info("[nt35521]%s skip power_on & init lcm since it's done by lk\n",
		__func__);
	pr_info("[nt35521] build type: %s, vendor type: %s\n",
		lcm_get_build_type(), lcm_get_vendor_type());
#endif
}

static void lcm_suspend(void)
{
	unsigned int data_array[16];
	#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[128];

	snprintf(buf, sizeof(buf), "%s:lcd:suspend=1;CT;1:NR", __func__);
	log_to_metrics(ANDROID_LOG_INFO, "LCDEvent", buf);
	#endif

	#ifdef BUILD_LK
		dprintf(INFO, "[LK/LCM] %s\n", __func__);
	#else
		pr_info("[nt35521] %s\n", __func__);
	#endif

	data_array[0] = 0x00280500; /* Display Off */
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(20);

	data_array[0] = 0x00100500; /* Sleep In */
	dsi_set_cmdq(data_array, 1, 1);
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
	pr_info("[nt35521] %s\n", __func__);
	#endif

	MDELAY(10);
	lcm_reset();

	init_karnak_kd_lcm(); /* KD panel */
}

/* Separate lcm_resume_power and lcm_reset from power_on func,
 * to meet turn on MIPI lanes after AVEE ready,
 * before Reset pin Low->High
 */
static void lcm_resume_power(void)
{
	int ret;

	/* GHGL_EN/GPIO110 - Control AVDD/AVEE/VDD,
	 * their sequence rests entirely on NT50357
	 */
#ifdef BUILD_LK
	dprintf(ALWAYS, "[LK/LCM] GPIO110 - Control AVDD/AVEE/VDD power\n");
	mt_set_gpio_mode(GPIO_LCD_LED_EN, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_LED_EN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_LED_EN, GPIO_OUT_ONE);
	mt_set_gpio_mode(GPIO_LCD_LED_EN_N, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_LCD_LED_EN_N, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_LCD_LED_EN_N, GPIO_OUT_ONE);
	/* T6[ms] VDDI ~ MIPI LP11 */
	MDELAY(30);
#else
	pr_info("[nt35521] %s, GPIO110 - Control AVDD/AVEE/VDD\n",
		__func__);
	ret = regulator_enable(lcm_vio_ldo);
	MDELAY(6);
	lcm_set_pwr(1);
	lcm_set_pwr_n(1);
	MDELAY(30);
#endif
}

/* Separate lcm_suspend_power from lcm_suspend
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
	lcm_set_pwr(0);
	lcm_set_pwr_n(0);
	MDELAY(5);
	ret = regulator_disable(lcm_vio_ldo);
#endif

	MDELAY(10);
}

static void lcm_set_backlight(unsigned int level)
{
	if (level > 255)	{
		pr_debug("%s invalid brightness level=%d\n", __func__, level);
		return;
	}
	dsi_set_cmdq_V2(0x51, 1, ((unsigned char *)&level), 1);
}

static void lcm_set_backlight_mode(unsigned int mode)
{
	if (mode > 3) {
		pr_debug("%s invalid CABC mode=%d\n", __func__, mode);
		return;
	}
	dsi_set_cmdq_V2(0x55, 1, ((unsigned char *)&mode), 1);
}

struct LCM_DRIVER nt35521_wxga_dsi_vdo_karnak_kd_lcm_drv = {
	.name			= "nt35521_wxga_dsi_vdo_karnak_kd",
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
