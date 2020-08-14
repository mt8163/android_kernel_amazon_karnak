/*******************************************************************************
 *  PWM Drvier
 *
 * Mediatek Pulse Width Modulator driver
 *
 * Copyright (C) 2015 John Crispin <blogic at openwrt.org>
 * Copyright (C) 2017 Zhi Mao <zhi.mao@mediatek.com>
 * Copyright (C) 2018 Xi Chen <xixi.chen@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public Licence,
 * version 2, as publish by the Free Software Foundation.
 *
 * This program is distributed and in hope it will be useful, but WITHOUT
 * ANY WARRNTY; without even the implied warranty of MERCHANTABITLITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#ifdef CONFIG_PWM_MUTEBUTTON
#include "pwm-mutebutton.h"
#endif

#ifdef PWM_ERR
#undef PWM_ERR
#endif
#define	PWM_ERR(dev, fmt, args...)	dev_info(dev, fmt, ##args)

#define PWM_EN_REG          0x0000
#define PWMCON              0x00
#define PWMGDUR             0x0c
#define PWMWAVENUM          0x28
#define PWMDWIDTH           0x2c
#define PWMTHRES            0x30
#define PWM_SEND_WAVENUM    0x34

#define PWM_CLK_DIV_MAX     7
#define PWM_NUM_MAX         8

#define PWM_CLK_NAME_MAIN   "main"
#define PWM_CLK_NAME_TOP    "top"

static const char * const mtk_pwm_clk_name[PWM_NUM_MAX] = {
	"pwm1", "pwm2", "pwm3", "pwm4", "pwm5", "pwm6", "pwm7", "pwm8"
};

struct mtk_com_pwm_data {
	const unsigned long *pwm_register;
	unsigned int pwm_nums;
	int no_main_clk;
	int no_top_clk;
};

struct mtk_com_pwm {
	struct pwm_chip chip;
	struct device *dev;
	void __iomem *base;
	struct clk *clks[PWM_NUM_MAX];
	struct clk *clk_main;
	struct clk *clk_top;
	const struct mtk_com_pwm_data *data;
#ifdef CONFIG_PWM_MUTEBUTTON
	struct led_mutebutton_priv* led_priv;
#endif
};

static const unsigned long common_pwm_register[PWM_NUM_MAX] = {
	0x0010, 0x0050, 0x0090, 0x00D0, 0x0110,
};

static struct mtk_com_pwm_data common_pwm_data = {
	.pwm_register = &common_pwm_register[0],
	.pwm_nums = 3,
	.no_top_clk = 1,
};

static const struct of_device_id pwm_of_match[] = {
	{.compatible = "mediatek,pwm", .data = &common_pwm_data},
	{},
};

#ifdef CONFIG_PWM_MUTEBUTTON
static void mutebutton_convert_to_rgb(uint8_t *rgbarray, int bitmap)
{
	if (rgbarray == NULL) {
		pr_err("Invalid Input to %s, rgbarray is NULL", __func__);
		return;
	}
	rgbarray[0] = (bitmap & (BYTEMASK << 8 * 2)) >> 8 * 2;
}
#endif

static inline struct mtk_com_pwm *to_mtk_pwm(struct pwm_chip *chip)
{
	return container_of(chip, struct mtk_com_pwm, chip);
}

static int mtk_pwm_is_clk_prepared(struct clk *clk)
{
	int r = 0;
	struct clk_hw *c_hw;

	if (clk) {
		c_hw = __clk_get_hw(clk);
		r = clk_hw_is_prepared(c_hw);
	}

	return r;
}

static unsigned int mtk_pwm_is_clk_enabled(struct clk *clk)
{
	unsigned int r = 0;

	if (clk)
		r = __clk_get_enable_count(clk);

	return r;
}

static int mtk_pwm_clk_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_com_pwm *mt_pwm = to_mtk_pwm(chip);
	int ret = 0;

	if (!mt_pwm->data->no_top_clk) {
		ret = clk_prepare_enable(mt_pwm->clk_top);
		if (ret < 0)
			return ret;
	}

	if (!mt_pwm->data->no_main_clk) {
		ret = clk_prepare_enable(mt_pwm->clk_main);
		if (ret < 0) {
			if (!mt_pwm->data->no_top_clk)
				clk_disable_unprepare(mt_pwm->clk_top);
			return ret;
		}
	}

	ret = clk_prepare_enable(mt_pwm->clks[pwm->hwpwm]);
	if (ret < 0) {
		if (!mt_pwm->data->no_main_clk)
			clk_disable_unprepare(mt_pwm->clk_main);
		if (!mt_pwm->data->no_top_clk)
			clk_disable_unprepare(mt_pwm->clk_top);
		return ret;
	}

	return ret;
}

static void mtk_pwm_clk_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_com_pwm *mt_pwm = to_mtk_pwm(chip);

	clk_disable_unprepare(mt_pwm->clks[pwm->hwpwm]);
	if (!mt_pwm->data->no_main_clk)
		clk_disable_unprepare(mt_pwm->clk_main);
	if (!mt_pwm->data->no_top_clk)
		clk_disable_unprepare(mt_pwm->clk_top);
}

static inline void mtk_pwm_writel(struct mtk_com_pwm *pwm,
				u32 pwm_no, unsigned long offset,
				unsigned long val)
{
	void __iomem *reg = pwm->base + pwm->data->pwm_register[pwm_no] + offset;

	writel(val, reg);
}

static inline u32 mtk_pwm_readl(struct mtk_com_pwm *pwm,
				u32 pwm_no, unsigned long offset)
{
	u32 value;
	void __iomem *reg = pwm->base + pwm->data->pwm_register[pwm_no] + offset;

	value = readl(reg);
	return value;
}


static int mtk_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			int duty_ns, int period_ns)
{
	struct mtk_com_pwm *mt_pwm = to_mtk_pwm(chip);
	u32 value = 0;
	u32 resolution = 100 / 4;
	u32 clkdiv = 0;
	u32 clksrc_rate = 0;

	u32 data_width, thresh;

#ifdef CONFIG_PWM_MUTEBUTTON
	int i, scaledvalue;

	struct led_mutebutton_priv* led_priv = mt_pwm->led_priv;
	struct led_mutebutton_data *led_data = ((led_priv) ? ( (led_priv->leds) ? &led_priv->leds[0] : NULL ) : NULL );

	pr_debug("mute button: input duty_ns : %d\n",duty_ns);
	pr_debug("mute button: input period_ns : %d\n",period_ns);
#endif
	mtk_pwm_clk_enable(chip, pwm);

	clksrc_rate = clk_get_rate(mt_pwm->clks[pwm->hwpwm]);
	if (clksrc_rate == 0) {
		PWM_ERR(mt_pwm->dev, "clksrc_rate %d is invalid\n", clksrc_rate);
		return -EINVAL;
	}
	resolution = 1000000000/clksrc_rate;

#ifdef CONFIG_PWM_MUTEBUTTON
	if (led_data) {
		mutex_lock(&led_data->lock);
		for (i = 0; i < NUM_CHANNELS; i++) {
			if (led_data->ledparams) {
				pr_debug("mute button: Enable Mute LED Brightness Calibration\n");
				mutebutton_convert_to_rgb(led_data->ledpwmscalingrgb, ledcalibparams[INDEX_PWMSCALING]);
				mutebutton_convert_to_rgb(led_data->ledpwmmaxlimiterrgb, ledcalibparams[INDEX_PWMMAXLIMIT]);
				pr_debug("mute button: pwmscalingrgb: %x\n", (int)led_data->ledpwmscalingrgb[i%NUM_LED_COLORS]);
				pr_debug("mute button: pwmmaxlimiterrgb: %x\n", led_data->ledpwmmaxlimiterrgb[i%NUM_LED_COLORS]);
				scaledvalue = ((int)duty_ns *
					       (int)led_data->ledpwmscalingrgb[i %
						NUM_LED_COLORS])
						/ LED_PWM_MAX_SCALING;
				pr_debug("mute button: scaledvalue: %d\n",scaledvalue);
				led_data->state[i] =
					min(scaledvalue,
					(int) led_data->ledpwmmaxlimiterrgb[i %
					NUM_LED_COLORS]);
				duty_ns = led_data->state[i];
				pr_debug("mute button: state[%d]: %d\n",i, led_data->state[i]);
			} else {
				pr_debug("mute button: Disable Mute LED Brightness Calibration\n");
				led_data->state[i] = duty_ns;
			}
		}
		mutex_unlock(&led_data->lock);
	}

	pr_debug("mute button: duty_ns : %d\n",duty_ns);
	pr_debug("mute button: period_ns : %d\n",period_ns);
	duty_ns *=  PWM_DUTY_CYCLE_MAX;
	duty_ns /= BRIGHTNESS_LEVEL_MAX;
	period_ns *= PWM_PERIOD_MAX;
	period_ns /= BRIGHTNESS_LEVEL_MAX;
	pr_debug("mute button: duty_ns * max duty cycle(40000) / max brightness level(255) : %d\n",duty_ns);
	pr_debug("mute button: period_ns * max period(40000) / max brightness level(255) : %d\n",period_ns);
#endif

	while (period_ns / resolution  > 8191) {
		clkdiv++;
		resolution *= 2;
	}

	if (clkdiv > PWM_CLK_DIV_MAX) {
		PWM_ERR(mt_pwm->dev, "period %d not supported\n", period_ns);
		return -EINVAL;
	}

	data_width = period_ns / resolution;
	thresh = duty_ns / resolution;

	value = mtk_pwm_readl(mt_pwm, pwm->hwpwm, PWMCON);
	value = value | BIT(15) | clkdiv;
	mtk_pwm_writel(mt_pwm, pwm->hwpwm, PWMCON, value);

	mtk_pwm_writel(mt_pwm, pwm->hwpwm, PWMDWIDTH, data_width);
	mtk_pwm_writel(mt_pwm, pwm->hwpwm, PWMTHRES, thresh);

	mtk_pwm_clk_disable(chip, pwm);

	return 0;
}

static int mtk_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_com_pwm *mt_pwm = to_mtk_pwm(chip);
	u32 value;

	pr_debug("mtk_pwm_enable +\n");
	mtk_pwm_clk_enable(chip, pwm);

	value = readl(mt_pwm->base + PWM_EN_REG);
	value |= BIT(pwm->hwpwm);
	writel(value, mt_pwm->base + PWM_EN_REG);

	pr_debug("PWM_EN_REG:0x%x\n", readl(mt_pwm->base + PWM_EN_REG));
	return 0;
}

static void mtk_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct mtk_com_pwm *mt_pwm = to_mtk_pwm(chip);
	u32 value;

	value = readl(mt_pwm->base + PWM_EN_REG);
	value &= ~BIT(pwm->hwpwm);
	writel(value, mt_pwm->base + PWM_EN_REG);

	mtk_pwm_clk_disable(chip, pwm);
}

#ifdef CONFIG_PWM_MUTEBUTTON
static ssize_t ledcalibparams_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t len)
{
	int ret;
	int temp;
	struct mtk_com_pwm *mt_pwm = dev_get_drvdata(dev);
	struct led_mutebutton_priv *led_priv = mt_pwm->led_priv;
	struct led_mutebutton_data *led_data = ((led_priv) ? ( (led_priv->leds) ? &led_priv->leds[0] : NULL ) : NULL );
	struct pwm_device *pwm = (mt_pwm->chip.pwms) ? &mt_pwm->chip.pwms[0] : NULL;
	unsigned int duty_cycle = (pwm) ? pwm_get_duty_cycle(pwm) : 0;
	unsigned int period = (pwm) ? pwm_get_period(pwm) : 0;

	if (!led_data) {
		pr_debug("mute button: led_data return NULL at %s:%d\n", __func__, __LINE__);
		return 0;
	}

	ret = kstrtoint(buf, 10, &temp);
	if (ret) {
		pr_debug("mute button: kstrtoint return NULL at %s:%d\n", __func__, __LINE__);
		return ret;
	}

	mutex_lock(&led_data->lock);
	led_data->ledparams = temp;
	mutex_unlock(&led_data->lock);

	mtk_pwm_config(&mt_pwm->chip, mt_pwm->chip.pwms, duty_cycle, period);

	return len;
}

static ssize_t ledcalibparams_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mtk_com_pwm *mt_pwm = dev_get_drvdata(dev);
	struct led_mutebutton_priv *led_priv = mt_pwm->led_priv;
	struct led_mutebutton_data *led_data = ((led_priv) ? ( (led_priv->leds) ? &led_priv->leds[0] : NULL ) : NULL );
	int ret;

	if (!led_data) {
		pr_debug("mute button: led_data return NULL at %s:%d\n", __func__, __LINE__);
		return 0;
	}

	mutex_lock(&led_data->lock);
	ret = sprintf(buf, "%d\n", led_data->ledparams);
	mutex_unlock(&led_data->lock);

	if (ret) {
		pr_debug("mute button: sprintf return %d at %s:%d\n", ret, __func__, __LINE__);
		return ret;
	}

	return ret;
}

static DEVICE_ATTR_RW(ledcalibparams);

static ssize_t ledpwmscaling_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t len)
{
	int ret;
	int temp;
	struct mtk_com_pwm *mt_pwm = dev_get_drvdata(dev);
	struct led_mutebutton_priv *led_priv = mt_pwm->led_priv;
	struct led_mutebutton_data *led_data = ((led_priv) ? ( (led_priv->leds) ? &led_priv->leds[0] : NULL ) : NULL );
	struct pwm_device *pwm = (mt_pwm->chip.pwms) ? &mt_pwm->chip.pwms[0] : NULL;
	unsigned int duty_cycle = (pwm) ? pwm_get_duty_cycle(pwm) : 0;
	unsigned int period = (pwm) ? pwm_get_period(pwm) : 0;

	if (!led_data) {
		pr_debug("mute button: led_data return NULL at %s:%d\n", __func__, __LINE__);
		return 0;
	}

	ret = kstrtoint(buf, 16, &temp);
	if (ret) {
		pr_debug("mute button: kstrtoint return %d at %s:%d\n", ret, __func__, __LINE__);
		return ret;
	}

	mutex_lock(&led_data->lock);
#ifdef CONFIG_PWM_INVERTED
	pr_debug("mute button: temp_invert_before =  %x\n",temp);
	temp = (2 * LED_PWM_MAX_SCALING - (temp >> 16)) << 16;
	pr_debug("mute button: temp_invert_after =  %x\n",temp);
#endif
	led_data->ledpwmscaling = temp;
	ledcalibparams[INDEX_PWMSCALING] = temp;
	mutebutton_convert_to_rgb(led_data->ledpwmscalingrgb, temp);
	mutex_unlock(&led_data->lock);
	pr_debug("mute button: ledpwmscaling %x\n",led_data->ledpwmscaling);
	pr_debug("mute button: ledpwmscalingrgb %x\n",led_data->ledpwmscalingrgb[0]);
	pr_debug("mute button: ledcalibparams[INDEX_PWMSCALING]%x\n",ledcalibparams[INDEX_PWMSCALING]);

	mtk_pwm_config(&mt_pwm->chip, mt_pwm->chip.pwms, duty_cycle, period);

	return len;
}

static ssize_t ledpwmscaling_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int ret;
	struct mtk_com_pwm *mt_pwm = dev_get_drvdata(dev);
	struct led_mutebutton_priv *led_priv = mt_pwm->led_priv;
	struct led_mutebutton_data *led_data = ((led_priv) ? ( (led_priv->leds) ? &led_priv->leds[0] : NULL ) : NULL );

	if (!led_data) {
		pr_debug("mute button: led_data return NULL at %s:%d\n", __func__, __LINE__);
		return 0;
	}

	mutex_lock(&led_data->lock);
#ifdef CONFIG_PWM_INVERTED
	pr_debug("mute button: ledpwmscaling_inverted = %x\n",led_data->ledpwmscaling);
	ret = sprintf(buf, "%x\n",(2 * LED_PWM_MAX_SCALING - (led_data->ledpwmscaling >> 16)) << 16);
#else
	ret = sprintf(buf, "%x\n", led_data->ledpwmscaling);
#endif
	mutex_unlock(&led_data->lock);
	pr_debug("mute button: ledpwmscaling %x\n",led_data->ledpwmscaling);

	return ret;
}

static DEVICE_ATTR_RW(ledpwmscaling);

static ssize_t ledpwmmaxlimit_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t len)
{
	int ret;
	int temp;

	struct mtk_com_pwm *mt_pwm = dev_get_drvdata(dev);
	struct led_mutebutton_priv *led_priv = mt_pwm->led_priv;
	struct led_mutebutton_data *led_data = ((led_priv) ? ( (led_priv->leds) ? &led_priv->leds[0] : NULL ) : NULL );
	struct pwm_device *pwm = (mt_pwm->chip.pwms) ? &mt_pwm->chip.pwms[0] : NULL;
	unsigned int duty_cycle = (pwm) ? pwm_get_duty_cycle(pwm) : 0;
	unsigned int period = (pwm) ? pwm_get_period(pwm) : 0;

	if (!led_data) {
		pr_debug("mute button: led_data return NULL at %s:%d\n", __func__, __LINE__);
		return 0;
	}

	ret = kstrtoint(buf, 16, &temp);
	if (ret) {
		pr_debug("mute button: kstrtoint return %d at %s:%d\n", ret, __func__, __LINE__);
		return ret;
	}

	mutex_lock(&led_data->lock);
	led_data->ledpwmmaxlimiter = temp;
	mutebutton_convert_to_rgb(led_data->ledpwmmaxlimiterrgb, temp);
	ledcalibparams[INDEX_PWMMAXLIMIT] = temp;
	mutex_unlock(&led_data->lock);
	pr_debug("mute button: ledpwmmaxlimiter %x\n",led_data->ledpwmmaxlimiter);
	pr_debug("mute button: ledpwmmaxlimiterrgb %x\n",led_data->ledpwmmaxlimiterrgb[0]);
	pr_debug("mute button: ledcalibparams[INDEX_PWMMAXLIMIT]%x\n", ledcalibparams[INDEX_PWMMAXLIMIT]);

	mtk_pwm_config(&mt_pwm->chip, mt_pwm->chip.pwms, duty_cycle, period);

	return len;
}

static ssize_t ledpwmmaxlimit_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mtk_com_pwm *mt_pwm = dev_get_drvdata(dev);
	struct led_mutebutton_priv *led_priv = mt_pwm->led_priv;
	struct led_mutebutton_data *led_data = ((led_priv) ? ( (led_priv->leds) ? &led_priv->leds[0] : NULL ) : NULL );

	int ret;

	if (!led_data) {
		pr_debug("mute button: led_data return NULL at %s:%d\n", __func__, __LINE__);
		return 0;
	}

	mutex_lock(&led_data->lock);
	ret = sprintf(buf, "%x\n", led_data->ledpwmmaxlimiter);
	mutex_unlock(&led_data->lock);
	pr_debug("mute button: ledpwmmaxlimiter %x",led_data->ledpwmmaxlimiter);

	return ret;
}

static DEVICE_ATTR_RW(ledpwmmaxlimit);
#endif

#ifdef CONFIG_DEBUG_FS
static void mtk_pwm_dbg_show(struct pwm_chip *chip, struct seq_file *s)
{
	struct mtk_com_pwm *mt_pwm = to_mtk_pwm(chip);
	u32 value;
	int i;

	seq_printf(s, "enable[0x%x]:0x%x, clk(top:%d) prepared:%d, enabled:%u, clk(main:%d) prepared:%d, enabled:%u\n",
			PWM_EN_REG, readl(mt_pwm->base + PWM_EN_REG), !mt_pwm->data->no_top_clk,
			mtk_pwm_is_clk_prepared(mt_pwm->clk_top),
			mtk_pwm_is_clk_enabled(mt_pwm->clk_top),
			!mt_pwm->data->no_main_clk,
			mtk_pwm_is_clk_prepared(mt_pwm->clk_main),
			mtk_pwm_is_clk_enabled(mt_pwm->clk_main));

	for (i = 0; i < mt_pwm->data->pwm_nums; ++i) {
		value = mtk_pwm_readl(mt_pwm, i, PWM_SEND_WAVENUM);
		seq_printf(s, " pwm%d: send wavenum:%u\n", i, value);
		seq_printf(s, "\tenable_bit[%d], clk prepared:%d, enabled:%u\n",
			(readl(mt_pwm->base + PWM_EN_REG) & (1 << i)) ? 1 : 0,
			mtk_pwm_is_clk_prepared(mt_pwm->clks[i]),
			mtk_pwm_is_clk_enabled(mt_pwm->clks[i]));
		seq_printf(s, "\tPWM_CON:0x%x, PWM_DWIDTH:0x%x, PWM_THRESH:0x%x, PWM_CONTINUOUS:%d\n",
			mtk_pwm_readl(mt_pwm, i, PWMCON),
			mtk_pwm_readl(mt_pwm, i, PWMDWIDTH),
			mtk_pwm_readl(mt_pwm, i, PWMTHRES),
			mtk_pwm_readl(mt_pwm, i, PWMWAVENUM) == 0 ? 1 : 0);
	}
}
#endif

static const struct pwm_ops mtk_pwm_ops = {
	.config = mtk_pwm_config,
	.enable = mtk_pwm_enable,
	.disable = mtk_pwm_disable,
#ifdef CONFIG_DEBUG_FS
	.dbg_show = mtk_pwm_dbg_show,
#endif
	.owner = THIS_MODULE,
};

#ifdef CONFIG_PWM_MUTEBUTTON
static size_t sizeof_mutebutton_leds_priv(int num_leds)
{
	size_t size =0;

	size = sizeof(struct led_mutebutton_priv);
	size += (sizeof(struct led_mutebutton_data) + sizeof(uint8_t)*NUM_CHANNELS + sizeof(uint8_t)*NUM_LED_COLORS + sizeof(uint8_t)*NUM_LED_COLORS) * num_leds;

	return size;
}

static int led_mutebutton_add(struct device *dev, struct led_mutebutton_priv *priv,
		       struct led_mutebutton *led)
{
	struct led_mutebutton_data *led_data = &priv->leds[priv->num_leds];
	int ret = 0;

	led_data->ledparams = led->ledparams;
	led_data->ledpwmscaling = led->ledpwmscaling;
	led_data->ledpwmmaxlimiter = led->ledpwmmaxlimiter;
	priv->num_leds++;

	return ret;
}

static int led_mutebutton_create_of(struct device *dev, struct led_mutebutton_priv *priv)
{
	struct led_mutebutton led;
	int ret = 0;
	int ledparams = 0;

	memset(&led, 0, sizeof(led));

	ledparams = idme_get_ledparams_value();
	if (ledparams) {
		ledcalibparams[INDEX_LEDCALIBENABLE] = ledparams;
		ledcalibparams[INDEX_PWMSCALING] = idme_get_ledcal_value() << 16;
		ledcalibparams[INDEX_PWMMAXLIMIT] = idme_get_ledpwmmaxlimit_value() << 16;

		led.ledparams = ledcalibparams[INDEX_LEDCALIBENABLE];
		led.ledpwmscaling = ledcalibparams[INDEX_PWMSCALING];
		led.ledpwmmaxlimiter = ledcalibparams[INDEX_PWMMAXLIMIT];
		pr_debug("mute button: Enable ledparams, ledpwmscaling %d, ledpwmmaxlimiter %d\n", led.ledpwmscaling, led.ledpwmmaxlimiter);
	} else {
		led.ledparams = 0;
		led.ledpwmscaling = LED_PWM_SCALING_DEFAULT;
		led.ledpwmmaxlimiter = LED_PWM_MAX_LIMIT_DEFAULT;
		pr_debug("mute button: Disable ledparams, ledpwmscaling %d, ledpwmmaxlimiter %d\n", led.ledpwmscaling, led.ledpwmmaxlimiter);
	}

	ret = led_mutebutton_add(dev, priv, &led);

	return ret;
}
#endif

static int mtk_pwm_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;
	struct mtk_com_pwm *mt_pwm;
	struct resource *res;
	int ret;
	int i;

#ifdef CONFIG_PWM_MUTEBUTTON
	struct led_mutebutton_data *led_data;
	int count = 1;
#endif

	pr_info("[upstream] mtk_pwm_probe +\n");

	id = of_match_device(pwm_of_match, &pdev->dev);
	if (!id)
		return -EINVAL;

	mt_pwm = devm_kzalloc(&pdev->dev, sizeof(*mt_pwm), GFP_KERNEL);
	if (!mt_pwm)
		return -ENOMEM;

	mt_pwm->data = id->data;
	mt_pwm->dev = &pdev->dev;

#ifdef CONFIG_PWM_MUTEBUTTON
	mt_pwm->led_priv = devm_kzalloc(&pdev->dev, sizeof_mutebutton_leds_priv(count),GFP_KERNEL);

	if (mt_pwm->led_priv) {
		led_data = devm_kzalloc(&pdev->dev,
						(sizeof(struct led_mutebutton_data) + sizeof(uint8_t)*NUM_CHANNELS + sizeof(uint8_t)*NUM_LED_COLORS + sizeof(uint8_t)*NUM_LED_COLORS) * count,
						GFP_KERNEL);
		if (led_data) {
			led_data = &mt_pwm->led_priv->leds[0];
			mutex_init(&led_data->lock);
		} else {
			pr_debug("mute button: led_mutebutton_data is NULL (%s:%d)\n",__func__,__LINE__);
			devm_kfree(&pdev->dev, led_data);
		}
	}
	else
		pr_debug("mute button: led_priv is NULL (%s:%d)\n",__func__,__LINE__);


	led_mutebutton_create_of(&pdev->dev, mt_pwm->led_priv);
#endif

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mt_pwm->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mt_pwm->base))
		return PTR_ERR(mt_pwm->base);

	for (i = 0; i < mt_pwm->data->pwm_nums; i++) {
		mt_pwm->clks[i] = devm_clk_get(&pdev->dev, mtk_pwm_clk_name[i]);
		if (IS_ERR(mt_pwm->clks[i])) {
			PWM_ERR(&pdev->dev, "[PWM] clock: %s fail\n", mtk_pwm_clk_name[i]);
			return PTR_ERR(mt_pwm->clks[i]);
		}
	}

	if (!mt_pwm->data->no_main_clk) {
		mt_pwm->clk_main = devm_clk_get(&pdev->dev, PWM_CLK_NAME_MAIN);
		if (IS_ERR(mt_pwm->clk_main))
			return PTR_ERR(mt_pwm->clk_main);
	}

	if (!mt_pwm->data->no_top_clk) {
		mt_pwm->clk_top = devm_clk_get(&pdev->dev, PWM_CLK_NAME_TOP);
		if (IS_ERR(mt_pwm->clk_top))
			return PTR_ERR(mt_pwm->clk_top);
	}

	mt_pwm->chip.dev = &pdev->dev;
	mt_pwm->chip.ops = &mtk_pwm_ops;
	mt_pwm->chip.npwm = mt_pwm->data->pwm_nums;

	platform_set_drvdata(pdev, mt_pwm);

	ret = pwmchip_add(&mt_pwm->chip);
	if (ret < 0) {
		PWM_ERR(&pdev->dev, "---- pwmchip_add() fail, ret = %d\n", ret);
		return ret;
	}

#ifdef CONFIG_PWM_MUTEBUTTON
	ret = device_create_file(mt_pwm->dev, &dev_attr_ledcalibparams);
	if (ret) {
		pr_info("mute button: Could not create ledcalibparams sysfs entry at %s:%d\n", __func__, __LINE__);
	}
	ret = device_create_file(mt_pwm->dev, &dev_attr_ledpwmscaling);
	if (ret) {
		pr_info("mute button: Could not create ledpwmscaling sysfs entry at %s:%d\n", __func__, __LINE__);
	}
	ret = device_create_file(mt_pwm->dev, &dev_attr_ledpwmmaxlimit);
	if (ret) {
		pr_info("mute button: Could not create ledpwmmaxlimit sysfs entry at %s:%d\n", __func__, __LINE__);
	}
#endif

	return 0;
}

static int mtk_pwm_remove(struct platform_device *pdev)
{
	struct mtk_com_pwm *mt_pwm = platform_get_drvdata(pdev);
	return pwmchip_remove(&mt_pwm->chip);
}

struct platform_driver mtk_pwm_driver = {
	.probe = mtk_pwm_probe,
	.remove = mtk_pwm_remove,
	.driver = {
		.name = "mtk-pwm",
		.of_match_table = pwm_of_match,
	},
};
MODULE_DEVICE_TABLE(of, pwm_of_match);

module_platform_driver(mtk_pwm_driver);

MODULE_AUTHOR("Xi Chen <xixi.chen@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC PWM driver");
MODULE_LICENSE("GPL v2");


