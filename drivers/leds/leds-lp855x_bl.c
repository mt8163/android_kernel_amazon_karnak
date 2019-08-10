
/*
 * TI LP855x Backlight Driver
 *
 *			Copyright (C) 2011 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
/* #define DEBUG */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/lp855x.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_data/mtk_thermal.h>
#include "../misc/mediatek/include/mt-plat/mt_boot_common.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/* LP8550/1/2/3/6 Registers */
#define LP855X_BRIGHTNESS_CTRL		0x00
#define LP855X_DEVICE_CTRL		0x01
#define LP855X_EEPROM_START		0xA0
#define LP855X_EEPROM_END		0xA7
#define LP8556_EPROM_START		0xA0
#define LP8556_EPROM_END		0xAF

/* LP8557 Registers */
#define LP8557_BL_CMD			0x00
#define LP8557_BL_MASK			0x01
#define LP8557_BL_ON			0x01
#define LP8557_BL_OFF			0x00
#define LP8557_BRIGHTNESS_CTRL		0x04
#define LP8557_CONFIG			0x10
#define LP8557_EPROM_START		0x10
#define LP8557_EPROM_END		0x1E
#define LP8557_VOLTAGE			0x16

#define BUF_SIZE		20
#define DEFAULT_BL_NAME		"lcd-backlight"
#define MAX_BRIGHTNESS		255

#define LCM_CM_N070ICE_DSI_VDO			"cm_n070ice_dsi_vdo"
#define LCM_NT35521_WXGA_DSI_VDO_HH060IA	"nt35521_wxga_dsi_vdo_hh060ia"

struct lp855x;

struct lp855x_device_config {
	u8 reg_brightness;
	u8 reg_devicectrl;
	int (*pre_init_device)(struct lp855x *);
	int (*post_init_device)(struct lp855x *);
};

struct  lp855x_led_data {
	struct led_classdev bl;
	struct lp855x *lp;
};
struct lp855x {
	const char *chipname;
	enum lp855x_chip_id chip_id;
	struct lp855x_device_config *cfg;
	struct i2c_client *client;
	struct lp855x_led_data *led_data;
	struct device *dev;
	u8 last_brightness;
	bool need_init;
	struct mutex bl_update_lock;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	/* platform device data */
	enum lp855x_brightness_ctrl_mode mode;
	u8 device_control;
	int initial_brightness;
	int max_brightness;
	int brightness_limit;
	struct lp855x_pwm_data pwm_data;
	u8 load_new_rom_data;
	int size_program;
	struct lp855x_rom_data *rom_data;
	int gpio_en;
	struct mtk_cooler_platform_data cool_dev;
};

static struct mtk_cooler_platform_data cooler = {
	.type = "lcd-backlight",
	.state = 0,
	.max_state = THERMAL_MAX_TRIPS,
	.level = 0,
	.levels = {
		175, 175, 175,
		175, 175, 175,
		175, 175, 175,
		175, 175, 175
	},
};

static unsigned char led_vmax;

static int __init setup_led_vmax(char *str)
{
	int vmax;

	if (get_option(&str, &vmax))
		led_vmax = (unsigned char)vmax;
	return 0;
}
__setup("lp855x_vmax=", setup_led_vmax);

void mtkfb_set_customized_backlight_callback(void (*func)(void *private, unsigned int),
					     void* private);

#ifdef CONFIG_HAS_EARLYSUSPEND
	static void lp855x_early_suspend(struct early_suspend *es);
	static void lp855x_late_resume(struct early_suspend *es);
#endif

static int lp855x_write_byte(struct lp855x *lp, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(lp->client, reg, data);
}

static int lp855x_read_byte(struct lp855x *lp, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(lp->client, reg);
	if (ret < 0) {
		dev_err(lp->dev, "failed to read 0x%.2x\n", reg);
		return ret;
	}

	*data = (u8)ret;

	return 0;
}

static int lp855x_update_bit(struct lp855x *lp, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	ret = i2c_smbus_read_byte_data(lp->client, reg);
	if (ret < 0) {
		dev_err(lp->dev, "failed to read 0x%.2x\n", reg);
		return ret;
	}

	tmp = (u8)ret;
	tmp &= ~mask;
	tmp |= data & mask;

	return lp855x_write_byte(lp, reg, tmp);
}

static bool lp855x_is_valid_rom_area(struct lp855x *lp, u8 addr)
{
	u8 start, end;

	switch (lp->chip_id) {
	case LP8550:
	case LP8551:
	case LP8552:
	case LP8553:
		dev_dbg(lp->dev, "Define LP855x rom area\n");
		start = LP855X_EEPROM_START;
		end = LP855X_EEPROM_END;
		break;
	case LP8556:
		dev_dbg(lp->dev, "Define LP8556 rom area\n");
		start = LP8556_EPROM_START;
		end = LP8556_EPROM_END;
		break;
	case LP8557:
		dev_dbg(lp->dev, "Define LP8557 rom area\n");
		start = LP8557_EPROM_START;
		end = LP8557_EPROM_END;
		break;
	default:
		return false;
	}

	return (addr >= start && addr <= end);
}

static int lp855x_check_init_device(struct lp855x *lp)
{
	//struct lp855x_platform_data *pd = lp->pdata;
	int i, ret = 0;
	u8 addr, val;

	if (lp->need_init) {
		/*Check device control reg first*/
		ret = lp855x_read_byte(lp, lp->cfg->reg_devicectrl, &val);
		if (ret)
			goto err;

		if (val == lp->device_control) {
			lp->need_init = false;
			if (lp->load_new_rom_data && lp->size_program) {
				for (i = 0; i < lp->size_program; i++) {
					addr = lp->rom_data[i].addr;

					if (!lp855x_is_valid_rom_area(lp, addr))
						continue;

					ret = lp855x_read_byte(lp, addr, &val);
					if (ret)
						goto err;

					dev_dbg(lp->dev,
							"Check rom data 0x%02X 0x%02X\n",
							addr,
							val);

					if (val != lp->rom_data[i].val) {
						lp->need_init = true;
						break;
					}

				}
			}
		}
	}
	return 0;

err:
	return ret;
}

static int lp855x_init_device(struct lp855x *lp)
{
	int i;
	int ret = 1;
	u8 val;
	u8 addr;

	dev_dbg(lp->dev, "Power on LP855X\n");

	if (lp->gpio_en  >= 0) {
		gpio_direction_output(lp->gpio_en, 1);
		msleep(20);
	}

	ret = lp855x_check_init_device(lp);
	if (ret)
		goto err;

	if (lp->need_init) {

		if (lp->cfg->pre_init_device) {
			ret = lp->cfg->pre_init_device(lp);
			if (ret) {
				dev_err(lp->dev,
				"pre init device err: %d\n",
				ret);
				goto err;
			}
		}

		val = lp->device_control;
		dev_dbg(lp->dev,
				"Set %s configuration 0x%02x: 0x%02x\n",
				(lp->cfg->reg_devicectrl == LP8557_CONFIG)
				? "LP8557" : "LP8550TO6",
				lp->cfg->reg_devicectrl,
				val);
		ret = lp855x_write_byte(lp, lp->cfg->reg_devicectrl, val);
		if (ret)
			goto err;

		if (lp->load_new_rom_data && lp->size_program) {
			for (i = 0; i < lp->size_program; i++) {
				addr = lp->rom_data[i].addr;
				val = lp->rom_data[i].val;
				if (!lp855x_is_valid_rom_area(lp, addr))
					continue;
				dev_dbg(lp->dev,
						"Load new rom data 0x%02X 0x%02X\n",
						addr,
						val);
				ret = lp855x_write_byte(lp, addr, val);
				if (ret)
					goto err;
			}
		}
	}

	if (lp->cfg->post_init_device) {
		ret = lp->cfg->post_init_device(lp);
		if (ret) {
			dev_err(lp->dev, "post init device err: %d\n", ret);
			goto err;
		}
	}

err:
	return ret;
}

static int lp8557_bl_off(struct lp855x *lp)
{
	dev_dbg(lp->dev, "BL_ON = 0 before updating EPROM settings\n");
	/* BL_ON = 0 before updating EPROM settings */
	return lp855x_update_bit(lp, LP8557_BL_CMD, LP8557_BL_MASK,
				LP8557_BL_OFF);
}

static int lp8557_bl_on(struct lp855x *lp)
{
	dev_dbg(lp->dev, "BL_ON = 1 after updating EPROM settings\n");
	/* BL_ON = 1 after updating EPROM settings */
	return lp855x_update_bit(lp, LP8557_BL_CMD, LP8557_BL_MASK,
				LP8557_BL_ON);
}

static struct lp855x_device_config lp8550to6_cfg = {
	.reg_brightness = LP855X_BRIGHTNESS_CTRL,
	.reg_devicectrl = LP855X_DEVICE_CTRL,
};

static struct lp855x_device_config lp8557_cfg = {
	.reg_brightness = LP8557_BRIGHTNESS_CTRL,
	.reg_devicectrl = LP8557_CONFIG,
	.pre_init_device = lp8557_bl_off,
	.post_init_device = lp8557_bl_on,
};

static int lp855x_configure(struct lp855x *lp)
{
	switch (lp->chip_id) {
	case LP8550:
	case LP8551:
	case LP8552:
	case LP8553:
	case LP8556:
	   dev_dbg(lp->dev, "Load lp8550to6 configuration\n");
		lp->cfg = &lp8550to6_cfg;
		break;
	case LP8557:
	   dev_dbg(lp->dev, "Load lp8557 configuration\n");
		lp->cfg = &lp8557_cfg;
		break;
	default:
		return -EINVAL;
	}

	lp->need_init = true;

	return lp855x_init_device(lp);
}

static void _lp855x_led_set_brightness(
			struct lp855x *lp,
			enum led_brightness brightness)
{
	struct led_classdev *bl = &(lp->led_data->bl);
	enum lp855x_brightness_ctrl_mode mode = lp->mode;
	int gpio_en = lp->gpio_en;
	u8 last_brightness = lp->last_brightness;
	int ret;

	dev_dbg(lp->dev, "%s (last_br=%d new_br=%d)\n", __func__,
				last_brightness, brightness);

	if ((!last_brightness) && (brightness)) {
#if 0 /* TODO: Add synchronization with panel */
		/* Power Up */
		/* wait until LCD is panel is ready */
		wait_event_timeout(panel_init_queue, nt51012_panel_enabled,
						msecs_to_jiffies(500));
#endif

		/* last_brightness is zero, it means LP8557 ever cut power off,
		 * so need to re-init device.
		 */
		lp->need_init = true; /* force init sequence, rework required */
		ret = lp855x_init_device(lp);
		if (ret)
			goto exit;
		dev_dbg(lp->dev, "BL power up\n");
	} else if (last_brightness == brightness) {
		dev_dbg(lp->dev, "BL nothing to do\n");
		/*nothing to do*/
		goto exit;
	}

	/* make changes */
	if (mode == PWM_BASED) {
		struct lp855x_pwm_data *pd = &lp->pwm_data;
		int br = brightness;
		int max_br = bl->max_brightness;

		if (pd->pwm_set_intensity)
			pd->pwm_set_intensity(br, max_br);

	} else if (mode == REGISTER_BASED) {
		u8 val = brightness;
		if (lp->brightness_limit)
			val = brightness * lp->brightness_limit /
				lp->max_brightness;
		lp855x_write_byte(lp, lp->cfg->reg_brightness, val);
	}

	/* power down if required */
	if ((last_brightness) &&  (!brightness)) {
		lp8557_bl_off(lp);
		/* Power Down */
		if (gpio_en  >= 0)
			gpio_set_value(gpio_en, 0);

		/*backlight is off, so wake up lcd disable */

#if 0 /* TODO: Add notification mechanism to the panel */
		lp855x_bl_off  = 1;
		wake_up(&panel_fini_queue);
#endif
		dev_dbg(lp->dev, "BL power down\n");

		/*For suspend power sequence: T6 > 200 ms */
		msleep(200);
	}

	lp->last_brightness =  brightness;
	bl->brightness      =  brightness;
exit:
	return;
}

void lp855x_led_set_brightness(
		struct led_classdev *led_cdev,
		enum led_brightness brightness)
{
	struct lp855x_led_data *led_data = container_of(led_cdev,
			struct lp855x_led_data, bl);

	struct lp855x *lp = led_data->lp;
	unsigned long state;

	mutex_lock(&(lp->bl_update_lock));

	state = lp->cool_dev.state;

	/* Record the latest value from userspace */
	lp->cool_dev.level = brightness;

	if (state) {
		if (brightness > lp->cool_dev.levels[state - 1]) {
			/*
			 * LED core set led_cdev->brightness already before reaching here.
			 *
			 * We set it as the actual/throttled value.
			 * Otherwise, the one read from sys node will NOT be real.
			 */
			led_cdev->brightness = lp->cool_dev.levels[state - 1];
			goto out;
		}
	}

	_lp855x_led_set_brightness(lp, brightness);

out:
	mutex_unlock(&(lp->bl_update_lock));
}
EXPORT_SYMBOL(lp855x_led_set_brightness);

static void lp855x_led_set_brightness_for_fb(
		void *private,
		unsigned int brightness)
{
	lp855x_led_set_brightness((struct led_classdev *)private, brightness);
}

static enum led_brightness _lp855x_led_get_brightness(struct lp855x *lp)
{
	struct led_classdev *bl = &(lp->led_data->bl);

	return bl->brightness;
}

enum led_brightness lp855x_led_get_brightness(struct led_classdev *led_cdev)
{
	struct lp855x_led_data *led_data = container_of(led_cdev,
			struct lp855x_led_data, bl);
	struct lp855x *lp = led_data->lp;
	enum led_brightness ret = 0;

	mutex_lock(&(lp->bl_update_lock));

	ret = _lp855x_led_get_brightness(lp);

	mutex_unlock(&(lp->bl_update_lock));

	return ret;
}
EXPORT_SYMBOL(lp855x_led_get_brightness);

static int lp855x_backlight_register(struct lp855x *lp)
{
	int ret;

	struct lp855x_led_data *led_data;
	struct led_classdev *bl;

	char *name = DEFAULT_BL_NAME;

	led_data = kzalloc(sizeof(struct lp855x_led_data), GFP_KERNEL);

	if (led_data == NULL) {
		ret = -ENOMEM;
		return ret;
	}

	bl = &(led_data->bl);

	bl->name = name;
	bl->max_brightness = MAX_BRIGHTNESS;
	bl->brightness_set = lp855x_led_set_brightness;
	bl->brightness_get = lp855x_led_get_brightness;
	led_data->lp = lp;
	lp->led_data = led_data;

	if (lp->initial_brightness > bl->max_brightness)
		lp->initial_brightness = bl->max_brightness;

	bl->brightness = lp->initial_brightness;

	ret = led_classdev_register(lp->dev, bl);
	if (ret < 0) {
		lp->led_data = NULL;
		kfree(led_data);
		return ret;
	}

	return 0;
}

static void lp855x_backlight_unregister(struct lp855x *lp)
{
	if (lp->led_data) {
		led_classdev_unregister(&(lp->led_data->bl));
		kfree(lp->led_data);
		lp->led_data = NULL;
	}
}

static ssize_t lp855x_get_chip_id(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lp855x *lp = dev_get_drvdata(dev);
	return scnprintf(buf, BUF_SIZE, "%s\n", lp->chipname);
}

static ssize_t lp855x_get_bl_ctl_mode(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct lp855x *lp = dev_get_drvdata(dev);
	enum lp855x_brightness_ctrl_mode mode = lp->mode;
	char *strmode = NULL;

	if (mode == PWM_BASED)
		strmode = "pwm based";
	else if (mode == REGISTER_BASED)
		strmode = "register based";

	return scnprintf(buf, BUF_SIZE, "%s\n", strmode);
}

static int lp855x_get_max_state(struct thermal_cooling_device *cdev,
			   unsigned long *state)
{
	struct lp855x *lp = cdev->devdata;
	*state = lp->cool_dev.max_state;
	return 0;
}

static int lp855x_get_cur_state(struct thermal_cooling_device *cdev,
			   unsigned long *state)
{
	struct lp855x *lp = cdev->devdata;
	*state = lp->cool_dev.state;
	return 0;
}

static int lp855x_set_cur_state(struct thermal_cooling_device *cdev,
			   unsigned long state)
{
	struct lp855x *lp = cdev->devdata;
	int level;
	unsigned long max_state;

	if (!lp)
		return 0;

	mutex_lock(&(lp->bl_update_lock));

	if (!lp->cool_dev.state)
		lp->cool_dev.level = lp->last_brightness;

	if (lp->cool_dev.state == state)
		goto out;

	max_state = lp->cool_dev.max_state;
	lp->cool_dev.state = (state > max_state) ? max_state : state;

	if (!lp->cool_dev.state)
		level = lp->cool_dev.level;
	else {
		level = lp->cool_dev.levels[lp->cool_dev.state - 1];
		if (level > lp->cool_dev.level)
			goto out;
	}
	_lp855x_led_set_brightness(lp, level);

out:
	mutex_unlock(&(lp->bl_update_lock));

	return 0;
}

static ssize_t levels_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct thermal_cooling_device *cdev = container_of(dev, struct thermal_cooling_device, device);
	struct lp855x *lp = cdev->devdata;
	struct mtk_cooler_platform_data *cool_dev = &(lp->cool_dev);
	int i;
	int offset = 0;

	if (!lp)
		return -EINVAL;
	for (i = 0; i < THERMAL_MAX_TRIPS; i++)
		offset += sprintf(buf + offset, "%d %d\n", i+1, cool_dev->levels[i]);
	return offset;
}

static ssize_t levels_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int level, state;
	struct thermal_cooling_device *cdev = container_of(dev, struct thermal_cooling_device, device);
	struct lp855x *lp = cdev->devdata;
	struct mtk_cooler_platform_data *cool_dev = &(lp->cool_dev);

	if (!lp)
		return -EINVAL;
	if (sscanf(buf, "%d %d\n", &state, &level) != 2)
		return -EINVAL;
	if (state >= THERMAL_MAX_TRIPS)
		return -EINVAL;
	cool_dev->levels[state] = level;
	return count;
}

static struct thermal_cooling_device_ops cooling_ops = {
	.get_max_state = lp855x_get_max_state,
	.get_cur_state = lp855x_get_cur_state,
	.set_cur_state = lp855x_set_cur_state,
};

static DEVICE_ATTR(chip_id, S_IRUGO, lp855x_get_chip_id, NULL);
static DEVICE_ATTR(bl_ctl_mode, S_IRUGO, lp855x_get_bl_ctl_mode, NULL);
static DEVICE_ATTR(levels, S_IRUGO | S_IWUSR, levels_show, levels_store);

static struct attribute *lp855x_attributes[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_bl_ctl_mode.attr,
	NULL,
};

static const struct attribute_group lp855x_attr_group = {
	.attrs = lp855x_attributes,
};

static struct lp855x_rom_data lp8557_eeprom[6];
#if 0
{	{0x14, 0xCF}, /* 4V OV, 4 LED string enabled */
	{0x13, 0x01}, /* Boost frequency @1MHz */
	{0x11, 0x06}, /* full scale 23 mA, but user is not allowed to set full scale */
	{0x12, 0x2B}, /* 19.5 kHz */
	{0x15, 0x42}, /* light smoothing, 100 ms */
	{0x16, 0xA0}, /* Max boost voltage 25V, enable adaptive voltage */
};
#endif

static int lp855x_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct lp855x *lp;
	enum lp855x_brightness_ctrl_mode mode;
	int ret;
	u8 val;
	int i;


	if (!i2c_check_functionality(cl->adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -EIO;

	lp = devm_kzalloc(&cl->dev, sizeof(struct lp855x), GFP_KERNEL);
	if (!lp)
		return -ENOMEM;

	lp->client = cl;
	lp->dev = &cl->dev;
	/* lp->chipname = id->name; */
	/* lp->chip_id = id->driver_data;*/
	lp->chipname = "lp8557_led";
	lp->chip_id = LP8557;
	i2c_set_clientdata(cl, lp);
	mutex_init(&(lp->bl_update_lock));

	/*
	 * FIXME : Will read from device tree later
	 */
	lp->mode = REGISTER_BASED;
	lp->initial_brightness = 100;
	lp->device_control = (LP8557_COMB2_CONFIG | LP8557_PWM_FILTER | LP8557_DISABLE_LEDS);
	lp->max_brightness = 255;
	lp->brightness_limit = 0;
	lp->load_new_rom_data = 1;
	lp->size_program = ARRAY_SIZE(lp8557_eeprom);
	lp->rom_data = lp8557_eeprom;
	if(of_property_read_u8_array(cl->dev.of_node, "eeprom_data", (u8*)lp8557_eeprom, 12))
		lp->load_new_rom_data = 0;

	if (lp->load_new_rom_data && led_vmax) {
		for (i = 0; i < lp->size_program; i++) {
			if (lp->rom_data[i].addr == LP8557_VOLTAGE) {
				val = lp->rom_data[i].val;
				switch (led_vmax) {
				case 18:
					val = (val & 0x3F);
					break;
				case 22:
					val = (val & 0x3F) | 0x40;
					break;
				case 25:
					val = (val & 0x3F) | 0x80;
					break;
				case 28:
					val = (val & 0x3F) | 0xC0;
					break;
				}
				lp->rom_data[i].val = val;

			}
		}
	}

	lp->gpio_en = of_get_named_gpio_flags(cl->dev.of_node, "gpio_en", 0, NULL);
	lp->cool_dev = cooler;

	mode = lp->mode;

	if (lp->gpio_en >= 0) {
		ret = gpio_request(lp->gpio_en, "backlight_lp855x_gpio");
		if (ret != 0) {
			pr_err("backlight gpio request failed\n");
			 /* serve as a flag as well */
			lp->gpio_en = -EINVAL;
		}
	}

	ret = lp855x_configure(lp);
	if (ret) {
		dev_err(lp->dev, "device config err: %d\n", ret);
		goto err_dev;
	}

	val = lp->initial_brightness;
	dev_dbg(lp->dev, "Initial %s brightness\n",
			(lp->cfg->reg_brightness == LP855X_BRIGHTNESS_CTRL)
			? "LP8550TO6" : "LP8557");

	ret = lp855x_write_byte(lp, lp->cfg->reg_brightness, val);
	if (ret)
		goto err_dev;

	ret = lp855x_backlight_register(lp);
	if (ret) {
		dev_err(lp->dev,
			" failed to register backlight. err: %d\n", ret);
		goto err_dev;
	}

	ret = sysfs_create_group(&lp->dev->kobj, &lp855x_attr_group);
	if (ret) {
		dev_err(lp->dev, " failed to register sysfs. err: %d\n", ret);
		goto err_sysfs;
	}


#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	if (get_boot_mode() != KERNEL_POWER_OFF_CHARGING_BOOT &&
	    get_boot_mode() != LOW_POWER_OFF_CHARGING_BOOT)
		lp855x_led_set_brightness(&(lp->led_data->bl), lp->initial_brightness);
	else {
		lp855x_led_set_brightness(&(lp->led_data->bl), 1);
		lp855x_led_set_brightness(&(lp->led_data->bl), 0);
	}
#else
	lp855x_led_set_brightness(&(lp->led_data->bl), lp->initial_brightness);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	lp->early_suspend.suspend = lp855x_early_suspend;
	lp->early_suspend.resume = lp855x_late_resume;
	register_early_suspend(&lp->early_suspend);
#endif

	lp->cool_dev.cdev = thermal_cooling_device_register(lp->cool_dev.type,
							     (void *)lp,
							     &cooling_ops);
	if (!lp->cool_dev.cdev)
		return -EINVAL;
	device_create_file(&lp->cool_dev.cdev->device, &dev_attr_levels);

	//lp855x_led_disp_register(&(lp->led_data->bl));

#ifdef CONFIG_MTK_FB
	mtkfb_set_customized_backlight_callback(lp855x_led_set_brightness_for_fb,
						&(lp->led_data->bl));
#endif
	return 0;

err_sysfs:
	lp855x_backlight_unregister(lp);
err_dev:
	mutex_destroy(&(lp->bl_update_lock));
	return ret;
}

static int lp855x_remove(struct i2c_client *cl)
{
	struct lp855x *lp = i2c_get_clientdata(cl);

	mutex_lock(&(lp->bl_update_lock));

	_lp855x_led_set_brightness(lp, 0);
	sysfs_remove_group(&lp->dev->kobj, &lp855x_attr_group);
	lp855x_backlight_unregister(lp);
	mutex_destroy(&(lp->bl_update_lock));

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&lp->early_suspend);
#endif

	return 0;
}

#ifdef CONFIG_PM
static int lp855x_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct lp855x *lp = i2c_get_clientdata(client);

	mutex_lock(&(lp->bl_update_lock));

	_lp855x_led_set_brightness(lp, 0);

	mutex_unlock(&(lp->bl_update_lock));

	if (lp->gpio_en >= 0)
		gpio_set_value(lp->gpio_en, 0);

	return 0;
}

static int lp855x_resume(struct i2c_client *client)
{
#if 0
	struct lp855x *lp = i2c_get_clientdata(client);
	struct lp855x_platform_data *pdata = lp->pdata;

	if (pdata->gpio_en == -1)
		return 0;

	mutex_lock(&(lp->bl_update_lock));
	gpio_set_value(pdata->gpio_en, 1);
	msleep(20);

	if (pdata->load_new_rom_data && pdata->size_program) {
		int i;
		for (i = 0; i < pdata->size_program; i++) {
			addr = pdata->rom_data[i].addr;
			val = pdata->rom_data[i].val;
			if (!lp855x_is_valid_rom_area(lp, addr))
				continue;

			ret |= lp855x_write_byte(lp, addr, val);
		}
	}

	if (ret)
		dev_err(lp->dev, "i2c write err\n");

	mutex_unlock(&(lp->bl_update_lock));
#endif

	return 0;
}

static void lp855x_shutdown(struct i2c_client *cl)
{
	struct lp855x *lp = i2c_get_clientdata(cl);

	mutex_lock(&(lp->bl_update_lock));
	_lp855x_led_set_brightness(lp, 0);
	printk("%s shutdown\n", __func__);
	mutex_unlock(&(lp->bl_update_lock));

}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void lp855x_early_suspend(struct early_suspend *es)
{

	struct lp855x *lp;
	lp = container_of(es, struct lp855x, early_suspend);

	if (lp855x_suspend(lp->client, PMSG_SUSPEND) != 0)
		dev_err(lp->dev, "%s: failed\n", __func__);

}

static void lp855x_late_resume(struct early_suspend *es)
{

	struct lp855x *lp;
	lp = container_of(es, struct lp855x, early_suspend);

	if (lp855x_resume(lp->client) != 0)
		dev_err(lp->dev, "%s: failed\n", __func__);
}

#else
static const struct dev_pm_ops lp855x_pm_ops = {
	.suspend	= lp855x_suspend,
	.resume		= lp855x_resume,
};
#endif
#endif

static const struct i2c_device_id lp855x_ids[] = {
	{"lp8550_led", LP8550},
	{"lp8551_led", LP8551},
	{"lp8552_led", LP8552},
	{"lp8553_led", LP8553},
	{"lp8556_led", LP8556},
	{"lp8557_led", LP8557},
	{ }
};
MODULE_DEVICE_TABLE(i2c, lp855x_ids);

static const struct of_device_id lp855x_id[] = {
	{.compatible = "ti,lp855x-led"},
	{},
};
MODULE_DEVICE_TABLE(OF, lp855x_id);

static struct i2c_driver lp855x_driver = {
	.driver = {
		.name = "lp855x_led",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(lp855x_id),
	},
	.probe = lp855x_probe,
	.remove = lp855x_remove,
	.shutdown = lp855x_shutdown,
	.id_table = lp855x_ids,
};

module_i2c_driver(lp855x_driver);

MODULE_DESCRIPTION("Texas Instruments LP855x Backlight driver");
MODULE_AUTHOR("Milo Kim <milo.kim@ti.com>");
MODULE_LICENSE("GPL");
