/*
 * lp5523.c - LP5523, LP55231 LED Driver
 *
 * Copyright (C) 2010 Nokia Corporation
 * Copyright (C) 2012 Texas Instruments
 *
 * Contact: Samu Onkalo <samu.p.onkalo@nokia.com>
 *          Milo(Woogyom) Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_data/leds-lp55xx.h>
#include <linux/slab.h>

#include "leds-lp55xx-common.h"

#define LP5523_PROGRAM_LENGTH		32	/* bytes */
/* Memory is used like this:
   0x00 engine 1 program
   0x10 engine 2 program
   0x20 engine 3 program
   0x30 engine 1 muxing info
   0x40 engine 2 muxing info
   0x50 engine 3 muxing info
*/
#define LP5523_MAX_LEDS			9

/* Registers */
#define LP5523_REG_ENABLE		0x00
#define LP5523_REG_OP_MODE		0x01
#define LP5523_REG_ENABLE_LEDS_MSB	0x04
#define LP5523_REG_ENABLE_LEDS_LSB	0x05
#define LP5523_REG_LED_PWM_BASE		0x16
#define LP5523_REG_LED_CURRENT_BASE	0x26
#define LP5523_REG_CONFIG		0x36
#define LP5523_REG_STATUS		0x3A
#define LP5523_REG_INTGPO		0x3B
#define LP5523_REG_RESET		0x3D
#define LP5523_REG_LED_TEST_CTRL	0x41
#define LP5523_REG_LED_TEST_ADC		0x42
#define LP5523_REG_CH1_PROG_START	0x4C
#define LP5523_REG_CH2_PROG_START	0x4D
#define LP5523_REG_CH3_PROG_START	0x4E
#define LP5523_REG_PROG_PAGE_SEL	0x4F
#define LP5523_REG_PROG_MEM		0x50

/* Bit description in registers */
#define LP5523_ENABLE			0x40
#define LP5523_AUTO_INC			0x40
#define LP5523_PWR_SAVE			0x20
#define LP5523_PWM_PWR_SAVE		0x04
#define LP5523_CP_AUTO			0x18
#define LP5523_CP_1_5			0x10
#define LP5523_CP_1			0x08
#define LP5523_AUTO_CLK			0x02
#define LP5523_INT_EN_GPO		0x04
#define LP5523_GPO_HIGH		0x1
#define LP5523_GPO_LOW		0x0

#define LP5523_EN_LEDTEST		0x80
#define LP5523_LEDTEST_DONE		0x80
#define LP5523_RESET			0xFF
#define LP5523_ADC_SHORTCIRC_LIM	80
#define LP5523_EXT_CLK_USED		0x08
#define LP5523_ENG_STATUS_MASK		0x07
#define LP5523_PAGE_SIZE		32
#define LP5523_SRAM_SIZE		(LP5523_PROGRAM_LENGTH * 6)

/* Memory Page Selection */
#define LP5523_PAGE_ENG1		0
#define LP5523_PAGE_ENG2		1
#define LP5523_PAGE_ENG3		2
#define LP5523_PAGE_MUX1		3
#define LP5523_PAGE_MUX2		4
#define LP5523_PAGE_MUX3		5

/* Program Memory Operations */
#define LP5523_MODE_ENG1_M		0x30	/* Operation Mode Register */
#define LP5523_MODE_ENG2_M		0x0C
#define LP5523_MODE_ENG3_M		0x03
#define LP5523_LOAD_ENG1		0x10
#define LP5523_LOAD_ENG2		0x04
#define LP5523_LOAD_ENG3		0x01

/* Program Commands */
#define LP5523_CMD_DISABLE		0x00
#define LP5523_CMD_LOAD			0x15
#define LP5523_CMD_RUN			0x2A

#define LP5523_ENG1_IS_LOADING(mode)	\
	((mode & LP5523_MODE_ENG1_M) == LP5523_LOAD_ENG1)
#define LP5523_ENG2_IS_LOADING(mode)	\
	((mode & LP5523_MODE_ENG2_M) == LP5523_LOAD_ENG2)
#define LP5523_ENG3_IS_LOADING(mode)	\
	((mode & LP5523_MODE_ENG3_M) == LP5523_LOAD_ENG3)

#define LP5523_EXEC_ENG1_M		0x30	/* Enable Register */
#define LP5523_EXEC_ENG2_M		0x0C
#define LP5523_EXEC_ENG3_M		0x03
#define LP5523_EXEC_M			0x3F
#define LP5523_RUN_ENG1			0x20
#define LP5523_RUN_ENG2			0x08
#define LP5523_RUN_ENG3			0x02

#define LP5523_CMD_LOAD			0x15 /* 00010101 */
#define LP5523_CMD_DISABLED		0x00 /* 00000000 */

#define LP5523_LEDTEST_VDD		0x10
#define LP5523_LEDTEST_VOUT		0x0F

#define LED_ACTIVE(mux, led)		(!!(mux & (0x0001 << led)))
#define TO_MILLI_VOLTS(x)		(((x) * 30) - 1478)

enum lp5523_chip_id {
	LP5523,
	LP55231,
};

static int lp5523_init_program_engine(struct lp55xx_chip *chip);

static inline void lp5523_wait_opmode_done(void)
{
	usleep_range(1000, 2000);
}

static void lp5523_set_led_current(struct lp55xx_led *led, u8 led_current)
{
	led->led_current = led_current;
	lp55xx_write(led->chip, LP5523_REG_LED_CURRENT_BASE + led->chan_nr,
		led_current);
}

static int lp5523_post_init_device(struct lp55xx_chip *chip)
{
	int ret;

	ret = lp55xx_write(chip, LP5523_REG_ENABLE, LP5523_ENABLE);
	if (ret)
		return ret;

	/* Chip startup time is 500 us, 1 - 2 ms gives some margin */
	usleep_range(1000, 2000);

	ret = lp55xx_write(chip, LP5523_REG_CONFIG,
			    LP5523_AUTO_INC | LP5523_PWR_SAVE |
			    LP5523_CP_AUTO | LP5523_AUTO_CLK |
			    LP5523_PWM_PWR_SAVE);
	if (ret)
		return ret;

	/* turn on all leds */
	ret = lp55xx_write(chip, LP5523_REG_ENABLE_LEDS_MSB, 0x01);
	if (ret)
		return ret;

	ret = lp55xx_write(chip, LP5523_REG_ENABLE_LEDS_LSB, 0xff);
	if (ret)
		return ret;


	/* enable gpo */
	if (chip->pdata->gpo_enable) {
		ret = lp55xx_write(chip, LP5523_REG_INTGPO, LP5523_INT_EN_GPO);
		if (ret)
			return ret;
	}

	return lp5523_init_program_engine(chip);
}

static void lp5523_load_engine(struct lp55xx_chip *chip)
{
	enum lp55xx_engine_index idx = chip->engine_idx;
	u8 mask[] = {
		[LP55XX_ENGINE_1] = LP5523_MODE_ENG1_M,
		[LP55XX_ENGINE_2] = LP5523_MODE_ENG2_M,
		[LP55XX_ENGINE_3] = LP5523_MODE_ENG3_M,
	};

	u8 val[] = {
		[LP55XX_ENGINE_1] = LP5523_LOAD_ENG1,
		[LP55XX_ENGINE_2] = LP5523_LOAD_ENG2,
		[LP55XX_ENGINE_3] = LP5523_LOAD_ENG3,
	};

	lp55xx_update_bits(chip, LP5523_REG_OP_MODE, mask[idx], val[idx]);

	lp5523_wait_opmode_done();
}

static void lp5523_load_engine_and_select_page(struct lp55xx_chip *chip)
{
	enum lp55xx_engine_index idx = chip->engine_idx;
	u8 page_sel[] = {
		[LP55XX_ENGINE_1] = LP5523_PAGE_ENG1,
		[LP55XX_ENGINE_2] = LP5523_PAGE_ENG2,
		[LP55XX_ENGINE_3] = LP5523_PAGE_ENG3,
	};

	lp5523_load_engine(chip);

	lp55xx_write(chip, LP5523_REG_PROG_PAGE_SEL, page_sel[idx]);
}

static void lp5523_stop_all_engines(struct lp55xx_chip *chip)
{
	lp55xx_write(chip, LP5523_REG_OP_MODE, 0);
	lp5523_wait_opmode_done();
}

static void lp5523_stop_engine(struct lp55xx_chip *chip)
{
	enum lp55xx_engine_index idx = chip->engine_idx;
	u8 mask[] = {
		[LP55XX_ENGINE_1] = LP5523_MODE_ENG1_M,
		[LP55XX_ENGINE_2] = LP5523_MODE_ENG2_M,
		[LP55XX_ENGINE_3] = LP5523_MODE_ENG3_M,
	};

	lp55xx_update_bits(chip, LP5523_REG_OP_MODE, mask[idx], 0);

	lp5523_wait_opmode_done();
}

#define BIT_GET(var, pos) ((var) & (1<<(pos)))
#define BIT_CLR(var, pos) ((var) &= ~(1 << (pos)))

static int lp5523_pwm_write(struct lp55xx_led *led, u8 val)
{
	struct lp55xx_chip *chip = led->chip;
	/* Only write if cache is invalid or new value different from cache */
	if (BIT_GET(chip->cache_inv, led->chan_nr) || led->brightness != val) {
		int ret = lp55xx_write(chip, LP5523_REG_LED_PWM_BASE + led->chan_nr, val);
		if (ret) {
			return ret;
		}
		/* Validate the cache bit and update cache */
		BIT_CLR(chip->cache_inv, led->chan_nr);
		led->brightness = val;
	}
	return 0;
}

static int lp5523_pwm_read(struct lp55xx_led *led, u8 *val)
{
	struct lp55xx_chip *chip = led->chip;

	/* Only read if cache is invalid */
	if (BIT_GET(chip->cache_inv, led->chan_nr)) {
		int ret = lp55xx_read(chip, LP5523_REG_LED_PWM_BASE + led->chan_nr, &led->brightness);
		if (ret) {
			return ret;
		}
		/* Validate the cache bit */
		BIT_CLR(chip->cache_inv, led->chan_nr);
	}
	/* Copy brightness cache out to caller */
	if (val) {
		*val = led->brightness;
	}
	return 0;
}

static void lp5523_turn_off_channels(struct lp55xx_chip *chip)
{
	struct lp55xx_led *led = i2c_get_clientdata(chip->cl);
	int i;

	for (i = 0; i < LP5523_MAX_LEDS; i++)
		lp5523_pwm_write(&led[i], 0);
}

static void lp5523_run_engine(struct lp55xx_chip *chip, bool start)
{
	int ret;
	u8 mode;
	u8 exec;

	/* stop engine */
	if (!start) {
		lp5523_stop_engine(chip);
		lp5523_turn_off_channels(chip);
		return;
	}

	/*
	 * To run the engine,
	 * operation mode and enable register should updated at the same time
	 */

	ret = lp55xx_read(chip, LP5523_REG_OP_MODE, &mode);
	if (ret)
		return;

	ret = lp55xx_read(chip, LP5523_REG_ENABLE, &exec);
	if (ret)
		return;

	/* change operation mode to RUN only when each engine is loading */
	if (LP5523_ENG1_IS_LOADING(mode)) {
		mode = (mode & ~LP5523_MODE_ENG1_M) | LP5523_RUN_ENG1;
		exec = (exec & ~LP5523_EXEC_ENG1_M) | LP5523_RUN_ENG1;
	}

	if (LP5523_ENG2_IS_LOADING(mode)) {
		mode = (mode & ~LP5523_MODE_ENG2_M) | LP5523_RUN_ENG2;
		exec = (exec & ~LP5523_EXEC_ENG2_M) | LP5523_RUN_ENG2;
	}

	if (LP5523_ENG3_IS_LOADING(mode)) {
		mode = (mode & ~LP5523_MODE_ENG3_M) | LP5523_RUN_ENG3;
		exec = (exec & ~LP5523_EXEC_ENG3_M) | LP5523_RUN_ENG3;
	}

	lp55xx_write(chip, LP5523_REG_OP_MODE, mode);
	lp5523_wait_opmode_done();

	lp55xx_update_bits(chip, LP5523_REG_ENABLE, LP5523_EXEC_M, exec);
}

static bool lp5523_is_engine_active(struct lp55xx_chip *chip)
{
	int ret;
	u8 mode;

	ret = lp55xx_read(chip, LP5523_REG_OP_MODE, &mode);
	if (ret) {
		dev_err(&chip->cl->dev, "error reading mode: %d\n", ret);
		return false;
	}

	if (!(mode & LP5523_CMD_RUN))
		return false;

	/* All engines are running */
	return true;
}

static int lp5523_init_program_engine(struct lp55xx_chip *chip)
{
	int i;
	int j;
	int ret;
	u8 status;
	/* one pattern per engine setting LED MUX start and stop addresses */
	static const u8 pattern[][LP5523_PROGRAM_LENGTH] =  {
		{ 0x9c, 0x30, 0x9c, 0xb0, 0x9d, 0x80, 0xd8, 0x00, 0},
		{ 0x9c, 0x40, 0x9c, 0xc0, 0x9d, 0x80, 0xd8, 0x00, 0},
		{ 0x9c, 0x50, 0x9c, 0xd0, 0x9d, 0x80, 0xd8, 0x00, 0},
	};

	/* hardcode 32 bytes of memory for each engine from program memory */
	ret = lp55xx_write(chip, LP5523_REG_CH1_PROG_START, 0x00);
	if (ret)
		return ret;

	ret = lp55xx_write(chip, LP5523_REG_CH2_PROG_START, 0x10);
	if (ret)
		return ret;

	ret = lp55xx_write(chip, LP5523_REG_CH3_PROG_START, 0x20);
	if (ret)
		return ret;

	/* write LED MUX address space for each engine */
	for (i = LP55XX_ENGINE_1; i <= LP55XX_ENGINE_3; i++) {
		chip->engine_idx = i;
		lp5523_load_engine_and_select_page(chip);

		for (j = 0; j < LP5523_PROGRAM_LENGTH; j++) {
			ret = lp55xx_write(chip, LP5523_REG_PROG_MEM + j,
					pattern[i - 1][j]);
			if (ret)
				goto out;
		}
	}

	lp5523_run_engine(chip, true);

	/* Let the programs run for couple of ms and check the engine status */
	usleep_range(3000, 6000);
	lp55xx_read(chip, LP5523_REG_STATUS, &status);
	status &= LP5523_ENG_STATUS_MASK;

	if (status != LP5523_ENG_STATUS_MASK) {
		dev_err(&chip->cl->dev,
			"cound not configure LED engine, status = 0x%.2x\n",
			status);
		ret = -1;
	}

out:
	lp5523_stop_all_engines(chip);
	return ret;
}

static int lp5523_update_program_memory(struct lp55xx_chip *chip,
					const u8 *data, size_t size)
{
	u8 pattern[LP5523_PROGRAM_LENGTH] = {0};
	unsigned cmd;
	char c[3];
	int nrchars;
	int ret;
	int offset = 0;
	int i = 0;

	while ((offset < size - 1) && (i < LP5523_PROGRAM_LENGTH)) {
		/* separate sscanfs because length is working only for %s */
		ret = sscanf(data + offset, "%2s%n ", c, &nrchars);
		if (ret != 1)
			goto err;

		ret = sscanf(c, "%2x", &cmd);
		if (ret != 1)
			goto err;

		pattern[i] = (u8)cmd;
		offset += nrchars;
		i++;
	}

	/* Each instruction is 16bit long. Check that length is even */
	if (i % 2)
		goto err;

	for (i = 0; i < LP5523_PROGRAM_LENGTH; i++) {
		ret = lp55xx_write(chip, LP5523_REG_PROG_MEM + i, pattern[i]);
		if (ret)
			return -EINVAL;
	}

	return size;

err:
	dev_err(&chip->cl->dev, "wrong pattern format\n");
	return -EINVAL;
}

static void lp5523_firmware_loaded(struct lp55xx_chip *chip)
{
	const struct firmware *fw = chip->fw;

	if (fw->size > LP5523_PROGRAM_LENGTH) {
		dev_err(&chip->cl->dev, "firmware data size overflow: %zu\n",
			fw->size);
		return;
	}

	/*
	 * Program momery sequence
	 *  1) set engine mode to "LOAD"
	 *  2) write firmware data into program memory
	 */

	lp5523_load_engine_and_select_page(chip);
	lp5523_update_program_memory(chip, fw->data, fw->size);
}

static ssize_t show_engine_mode(struct device *dev,
				struct device_attribute *attr,
				char *buf, int nr)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	enum lp55xx_engine_mode mode = chip->engines[nr - 1].mode;

	switch (mode) {
	case LP55XX_ENGINE_RUN:
		return sprintf(buf, "run\n");
	case LP55XX_ENGINE_LOAD:
		return sprintf(buf, "load\n");
	case LP55XX_ENGINE_DISABLED:
	default:
		return sprintf(buf, "disabled\n");
	}
}
show_mode(1)
show_mode(2)
show_mode(3)

static ssize_t store_engine_mode(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t len, int nr)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	struct lp55xx_engine *engine = &chip->engines[nr - 1];

	mutex_lock(&chip->lock);

	chip->engine_idx = nr;

	/* The LED engine modifies these registers independently of our cache,
	 * effectively making them volatile registers. */
	chip->cache_inv = 0xFF;

	if (!strncmp(buf, "run", 3)) {
		lp5523_run_engine(chip, true);
		engine->mode = LP55XX_ENGINE_RUN;
	} else if (!strncmp(buf, "load", 4)) {
		lp5523_stop_engine(chip);
		lp5523_load_engine(chip);
		engine->mode = LP55XX_ENGINE_LOAD;
	} else if (!strncmp(buf, "disabled", 8)) {
		lp5523_stop_engine(chip);
		engine->mode = LP55XX_ENGINE_DISABLED;
	}

	mutex_unlock(&chip->lock);

	return len;
}
store_mode(1)
store_mode(2)
store_mode(3)

static int lp5523_mux_parse(const char *buf, u16 *mux, size_t len)
{
	u16 tmp_mux = 0;
	int i;

	len = min_t(int, len, LP5523_MAX_LEDS);

	for (i = 0; i < len; i++) {
		switch (buf[i]) {
		case '1':
			tmp_mux |= (1 << i);
			break;
		case '0':
			break;
		case '\n':
			i = len;
			break;
		default:
			return -1;
		}
	}
	*mux = tmp_mux;

	return 0;
}

static void lp5523_mux_to_array(u16 led_mux, char *array)
{
	int i, pos = 0;
	for (i = 0; i < LP5523_MAX_LEDS; i++)
		pos += sprintf(array + pos, "%x", LED_ACTIVE(led_mux, i));

	array[pos] = '\0';
}

static int lp5523_wait_test_complete(struct lp55xx_chip *chip)
{
	u8 status = 0;
	int ret = -1;
	int retry = 3; // Wait test retry count

	if (!chip)
		return ret;
	/* ADC conversion time is typically 2.7 ms */
	usleep_range(3000, 6000);

	while (retry--) {
		ret = lp55xx_read(chip, LP5523_REG_STATUS, &status);
		if (!(status & LP5523_LEDTEST_DONE))
			usleep_range(3000, 6000); /* Was not ready. Wait little bit */
		else
			break;
	}

	return ret | !(status & LP5523_LEDTEST_DONE);
}

static ssize_t show_engine_leds(struct device *dev,
			    struct device_attribute *attr,
			    char *buf, int nr)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	char mux[LP5523_MAX_LEDS + 1];

	lp5523_mux_to_array(chip->engines[nr - 1].led_mux, mux);

	return sprintf(buf, "%s\n", mux);
}
show_leds(1)
show_leds(2)
show_leds(3)

static int lp5523_load_mux(struct lp55xx_chip *chip, u16 mux, int nr)
{
	struct lp55xx_engine *engine = &chip->engines[nr - 1];
	int ret;
	u8 mux_page[] = {
		[LP55XX_ENGINE_1] = LP5523_PAGE_MUX1,
		[LP55XX_ENGINE_2] = LP5523_PAGE_MUX2,
		[LP55XX_ENGINE_3] = LP5523_PAGE_MUX3,
	};

	lp5523_load_engine(chip);

	ret = lp55xx_write(chip, LP5523_REG_PROG_PAGE_SEL, mux_page[nr]);
	if (ret)
		return ret;

	ret = lp55xx_write(chip, LP5523_REG_PROG_MEM , (u8)(mux >> 8));
	if (ret)
		return ret;

	ret = lp55xx_write(chip, LP5523_REG_PROG_MEM + 1, (u8)(mux));
	if (ret)
		return ret;

	engine->led_mux = mux;
	return 0;
}

static ssize_t store_engine_leds(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len, int nr)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	struct lp55xx_engine *engine = &chip->engines[nr - 1];
	u16 mux = 0;
	ssize_t ret;

	if (lp5523_mux_parse(buf, &mux, len))
		return -EINVAL;

	mutex_lock(&chip->lock);

	chip->engine_idx = nr;
	ret = -EINVAL;

	if (engine->mode != LP55XX_ENGINE_LOAD)
		goto leave;

	if (lp5523_load_mux(chip, mux, nr))
		goto leave;

	ret = len;
leave:
	mutex_unlock(&chip->lock);
	return ret;
}
store_leds(1)
store_leds(2)
store_leds(3)

static ssize_t store_engine_load(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len, int nr)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	int ret;

	mutex_lock(&chip->lock);

	chip->engine_idx = nr;
	lp5523_load_engine_and_select_page(chip);
	ret = lp5523_update_program_memory(chip, buf, len);

	mutex_unlock(&chip->lock);

	return ret;
}
store_load(1)
store_load(2)
store_load(3)


static ssize_t show_engine_prog_start(struct device *dev,
				   struct device_attribute *attr,
				   char *buf, int nr)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	u8 val = 0;
	int ret;

	ret = lp55xx_read(chip, LP5523_REG_CH1_PROG_START + nr - 1, &val);

	ret = scnprintf(buf, PAGE_SIZE, "%02hhX\n", val);

	return ret;
}
show_prog_start(1)
show_prog_start(2)
show_prog_start(3)

static ssize_t store_engine_prog_start(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t len, int nr)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	int ret;

	u8 pc_start = 0;
	ret = sscanf(buf, "%hhX", &pc_start);
	if (ret != 1)
		return -EINVAL;

	lp55xx_write(chip, LP5523_REG_CH1_PROG_START + nr - 1, pc_start);

	return len;
}
store_prog_start(1)
store_prog_start(2)
store_prog_start(3)

static int test_pattern(struct lp55xx_chip *chip, u8* pattern,
			unsigned int pattern_size, unsigned int read_wait_us)
{
	unsigned int ret, i = 0;
	unsigned int prog_page = 0;
	unsigned int offset = 0;
	u8 data;

	if (!chip || !pattern || pattern_size == 0
			|| pattern_size > LP5523_SRAM_SIZE)
		return -1;

	/* Disable running engine */
	ret = lp55xx_write(chip, LP5523_REG_OP_MODE, LP5523_CMD_DISABLED);
	if (ret)
		return -1;
	/* Set the engine mode to load */
	ret = lp55xx_write(chip, LP5523_REG_OP_MODE, LP5523_CMD_LOAD);
	if (ret)
		return -1;

	/* Write the pattern to SRAM */
	for (i = 0; i < pattern_size; i++) {
		/* Write the page select */
		if (!(i % LP5523_PAGE_SIZE)) {
			/* Select page to write */
			lp55xx_write(chip, LP5523_REG_PROG_PAGE_SEL, prog_page);
			prog_page++;
			offset = 0;
		}

		lp55xx_write(chip, LP5523_REG_PROG_MEM + offset, pattern[i]);
		offset++;
	}

	/* Reset the page and offset */
	prog_page = 0;
	offset = 0;

	if (read_wait_us)
		usleep_range(read_wait_us, read_wait_us+1000);

	/* Read back the pattern and verify written data */
	for (i = 0; i < pattern_size; i++) {
		/* Write the page select */
		if (!(i % LP5523_PAGE_SIZE)) {
			/* Select page to write */
			lp55xx_write(chip, LP5523_REG_PROG_PAGE_SEL, prog_page);
			prog_page++;
			offset = 0;
		}

		ret = lp55xx_read(chip, LP5523_REG_PROG_MEM + offset, &data);
		if (ret < 0)
			return ret;

		/* Actual data verification */
		if (data != pattern[i]) {
			printk(KERN_ERR " SRAM Pattern [%d] failed @ 0x%x != 0x%x \n",
					i, data, pattern[i]);
			return -1;
		}

		offset++;
	}

	return 0;
}

static int sram_selftest(struct lp55xx_chip *chip)
{
	u8 pattern[LP5523_SRAM_SIZE];
	unsigned int ret, i;
	unsigned int sleep_us = 50000;

	/* Create a checker board pattern */
	for (i = 0; i < ARRAY_SIZE(pattern); i++) {
		if (i % 2)
			pattern[i] = 0x55;
		else
			pattern[i] = 0xAA;
	}

	ret = test_pattern(chip, pattern, ARRAY_SIZE(pattern), sleep_us);
	if (ret)
		return ret;

	/* Check for stuck bits with 0 pattern */
	memset(pattern, 0x0, ARRAY_SIZE(pattern));
	ret = test_pattern(chip, pattern, ARRAY_SIZE(pattern), sleep_us);
	if (ret)
		return ret;

	/* Test with 0xA5 pattern */
	memset(pattern, 0xA5, ARRAY_SIZE(pattern));
	ret = test_pattern(chip, pattern, ARRAY_SIZE(pattern), sleep_us);
	if (ret)
		return ret;

	/* Test with 0x5A pattern */
	memset(pattern, 0x5A, ARRAY_SIZE(pattern));
	ret = test_pattern(chip, pattern, ARRAY_SIZE(pattern), sleep_us);
	if (ret)
		return ret;

	/* Check for open bits with 1 pattern */
	memset(pattern, 0xFF, ARRAY_SIZE(pattern));
	ret = test_pattern(chip, pattern, ARRAY_SIZE(pattern), sleep_us);
	if (ret)
		return ret;

	return 0;
}

static ssize_t lp5523_selftest(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	struct lp55xx_platform_data *pdata = chip->pdata;
	int i, ret, pos = 0;
	u8 status, adc;
	u8 config_mode = 0;
	u8 v_in = 0;

	mutex_lock(&chip->lock);

	ret = lp55xx_read(chip, LP5523_REG_STATUS, &status);
	if (ret < 0)
		goto fail;

	/* Check that ext clock is really in use if requested */
	if (pdata->clock_mode == LP55XX_CLOCK_EXT) {
		if ((status & LP5523_EXT_CLK_USED) == 0)
			goto fail;
	}

	/* By default, the charge pump is in Auto mode. For tests,
	 * force to 1.5X mode */
	ret = lp55xx_read(chip, LP5523_REG_CONFIG, &config_mode);
	if (ret < 0)
		goto fail;
	lp55xx_write(chip, LP5523_REG_CONFIG,
			(config_mode & ~LP5523_CP_AUTO) | LP5523_CP_1_5);

	/* Measure VDD (i.e. VBAT) first (channel 16 corresponds to VDD) */
	lp55xx_write(chip, LP5523_REG_LED_TEST_CTRL, LP5523_EN_LEDTEST | 16);
	usleep_range(3000, 6000); /* ADC conversion time is typically 2.7 ms */
	ret = lp55xx_read(chip, LP5523_REG_STATUS, &status);
	if (ret < 0)
		goto fail;

	for (i = 0; i < chip->num_leds && !ret; i++) {
		/* Skip non-existing channels */
		if (pdata->led_config[i].led_current == 0)
			continue;

		/* Set default current */
		lp55xx_write(chip, LP5523_REG_LED_CURRENT_BASE + i, led->led_current);

		lp5523_pwm_write(&led[i], 0xff);
		/* let current stabilize 3 - 4ms before measurements start */
		usleep_range(3000, 4000);
		lp55xx_write(chip, LP5523_REG_LED_TEST_CTRL, LP5523_EN_LEDTEST | i);

		if (lp5523_wait_test_complete(chip))
			goto restore_config;

		ret |= lp55xx_read(chip, LP5523_REG_LED_TEST_ADC, &adc);

		/* Unconditionally print out measurement results */
		if (i == 6 || i == 7 || i == 8) {
			/* LEDs 6, 7 & 8 (Red) use Vdd for Vin */
			/* Measure VDD (i.e. VBAT) first (channel 16 corresponds to VDD) */
			lp55xx_write(chip, LP5523_REG_LED_TEST_CTRL,
					LP5523_EN_LEDTEST | LP5523_LEDTEST_VDD);
			if (lp5523_wait_test_complete(chip))
				goto restore_config;

			ret |= lp55xx_read(chip, LP5523_REG_LED_TEST_ADC, &v_in);
		} else {
			/* All others use Vout of the charge pump */
			/* Measure VOUT */
			lp55xx_write(chip, LP5523_REG_LED_TEST_CTRL,
					LP5523_EN_LEDTEST | LP5523_LEDTEST_VOUT);
			if (lp5523_wait_test_complete(chip))
				goto restore_config;

			ret |= lp55xx_read(chip, LP5523_REG_LED_TEST_ADC, &v_in);
		}

		pos += sprintf(buf + pos, "LED=%d V-IN=%d V-ADC=%d\n", i + 1,
				TO_MILLI_VOLTS(v_in), TO_MILLI_VOLTS(adc));
		adc = 0;

		lp5523_pwm_write(&led[i], 0x00);

		led++;
	}
	/* Perform SRAM Self Test */
	ret = sram_selftest(chip);
	if (ret)
		pos += sprintf(buf + pos, "SRAM Test=FAIL\n");
	else
		pos += sprintf(buf + pos, "SRAM Test=PASS\n");

restore_config:
	/* Restore the default mode */
	lp55xx_write(chip, LP5523_REG_CONFIG, config_mode);

fail:
	if (pos == 0 || ret)
		pos = sprintf(buf, "Failed to run test - %d %d\n", pos, ret);

	mutex_unlock(&chip->lock);

	return pos;
}

static ssize_t lp5523_showGPO(struct device *dev,
							  struct device_attribute *attr,
							  char *buf){
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	u8 reg;

	mutex_lock(&chip->lock);
	lp55xx_read(chip, LP5523_REG_INTGPO, &reg);
	mutex_unlock(&chip->lock);
	reg &= LP5523_GPO_HIGH;
	return scnprintf(buf, PAGE_SIZE, "%d\n", reg);
}

static ssize_t lp5523_storeGPO(struct device *dev,
							   struct device_attribute *attr,
							   char *buf, size_t len){
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	u8 reg;
	int ret = -EINVAL;

	reg = (buf[0] == '0') ? LP5523_GPO_LOW : LP5523_GPO_HIGH;
	mutex_lock(&chip->lock);
	ret = lp55xx_write(chip, LP5523_REG_INTGPO, LP5523_INT_EN_GPO | reg);

	if (ret < 0)
		goto writefail;

	ret = len;
writefail:
	mutex_unlock(&chip->lock);
	return ret;
}

static ssize_t show_all_leds(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	int i;

	mutex_lock(&chip->lock);

	for (i = 0; i < LP5523_MAX_LEDS; ++i) {
		lp5523_pwm_read(&led[i], NULL);
	}

	mutex_unlock(&chip->lock);

	return scnprintf(buf, PAGE_SIZE,
			"%hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu\n",
			led[0].brightness,
			led[1].brightness,
			led[2].brightness,
			led[3].brightness,
			led[4].brightness,
			led[5].brightness,
			led[6].brightness,
			led[7].brightness,
			led[8].brightness);
}

static ssize_t store_all_leds(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	u8 vals[LP5523_MAX_LEDS];
	int ret = 0;
	int i;

	ret = sscanf(buf, "%hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu",
			&vals[0],
			&vals[1],
			&vals[2],
			&vals[3],
			&vals[4],
			&vals[5],
			&vals[6],
			&vals[7],
			&vals[8]);
	if (ret != LP5523_MAX_LEDS)
		return -EINVAL;

	mutex_lock(&chip->lock);

	for (i = 0; i < LP5523_MAX_LEDS; ++i) {
		lp5523_pwm_write(&led[i], vals[i]);
	}

	mutex_unlock(&chip->lock);

	return len;
}

static void lp5523_led_brightness_work(struct work_struct *work)
{
	struct lp55xx_led *led = container_of(work, struct lp55xx_led,
					      brightness_work);
	struct lp55xx_chip *chip = led->chip;

	mutex_lock(&chip->lock);
	lp5523_pwm_write(led, led->brightness_new);
	mutex_unlock(&chip->lock);
}

static LP55XX_DEV_ATTR_RW(engine1_mode, show_engine1_mode, store_engine1_mode);
static LP55XX_DEV_ATTR_RW(engine2_mode, show_engine2_mode, store_engine2_mode);
static LP55XX_DEV_ATTR_RW(engine3_mode, show_engine3_mode, store_engine3_mode);
static LP55XX_DEV_ATTR_RW(engine1_leds, show_engine1_leds, store_engine1_leds);
static LP55XX_DEV_ATTR_RW(engine2_leds, show_engine2_leds, store_engine2_leds);
static LP55XX_DEV_ATTR_RW(engine3_leds, show_engine3_leds, store_engine3_leds);
static LP55XX_DEV_ATTR_WO(engine1_load, store_engine1_load);
static LP55XX_DEV_ATTR_WO(engine2_load, store_engine2_load);
static LP55XX_DEV_ATTR_WO(engine3_load, store_engine3_load);
static LP55XX_DEV_ATTR_RW(
	engine1_prog_start, show_engine1_prog_start, store_engine1_prog_start);
static LP55XX_DEV_ATTR_RW(
	engine2_prog_start, show_engine2_prog_start, store_engine2_prog_start);
static LP55XX_DEV_ATTR_RW(
	engine3_prog_start, show_engine3_prog_start, store_engine3_prog_start);
static LP55XX_DEV_ATTR_RO(selftest, lp5523_selftest);
static LP55XX_DEV_ATTR_RW(gpo, lp5523_showGPO, lp5523_storeGPO);
static LP55XX_DEV_ATTR_RW(all_leds, show_all_leds, store_all_leds);

static struct attribute *lp5523_attributes[] = {
	&dev_attr_engine1_mode.attr,
	&dev_attr_engine2_mode.attr,
	&dev_attr_engine3_mode.attr,
	&dev_attr_engine1_load.attr,
	&dev_attr_engine2_load.attr,
	&dev_attr_engine3_load.attr,
	&dev_attr_engine1_prog_start.attr,
	&dev_attr_engine2_prog_start.attr,
	&dev_attr_engine3_prog_start.attr,
	&dev_attr_engine1_leds.attr,
	&dev_attr_engine2_leds.attr,
	&dev_attr_engine3_leds.attr,
	&dev_attr_selftest.attr,
	&dev_attr_all_leds.attr,
	NULL,
};

static struct attribute *gpo_attributes[] = {
	&dev_attr_gpo.attr,
	NULL,
};

static const struct attribute_group lp5523_group = {
	.attrs = lp5523_attributes,
};

/* Chip specific configurations */
static struct lp55xx_device_config lp5523_cfg = {
	.reset = {
		.addr = LP5523_REG_RESET,
		.val  = LP5523_RESET,
	},
	.enable = {
		.addr = LP5523_REG_ENABLE,
		.val  = LP5523_ENABLE,
	},
	.max_channel  = LP5523_MAX_LEDS,
	.post_init_device   = lp5523_post_init_device,
	.brightness_work_fn = lp5523_led_brightness_work,
	.set_led_current    = lp5523_set_led_current,
	.firmware_cb        = lp5523_firmware_loaded,
	.run_engine         = lp5523_run_engine,
	.is_engine_active   = lp5523_is_engine_active,
	.dev_attr_group     = &lp5523_group,
};

static int lp5523_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;
	struct lp55xx_chip *chip;
	struct lp55xx_led *led;
	struct lp55xx_platform_data *pdata;
	struct device_node *np = client->dev.of_node;

	if (!dev_get_platdata(&client->dev)) {
		if (np) {
			ret = lp55xx_of_populate_pdata(&client->dev, np);
			if (ret < 0)
				return ret;
		} else {
			dev_err(&client->dev, "no platform data\n");
			return -EINVAL;
		}
	}
	pdata = dev_get_platdata(&client->dev);

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	led = devm_kzalloc(&client->dev,
			sizeof(*led) * pdata->num_channels, GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	chip->cl = client;
	chip->pdata = pdata;
	chip->cfg = &lp5523_cfg;
	chip->cache_inv = 0xFF; /* Invalidate brightness cache */

	mutex_init(&chip->lock);

	i2c_set_clientdata(client, led);

	ret = lp55xx_init_device(chip);
	if (ret)
		goto err_init;

	dev_info(&client->dev, "%s Programmable led chip found\n", id->name);

	ret = lp55xx_register_leds(led, chip);
	if (ret)
		goto err_register_leds;

	ret = lp55xx_register_sysfs(chip);
	if (ret) {
		dev_err(&client->dev, "registering sysfs failed\n");
		goto err_register_sysfs;
	}

	if (pdata->gpo_enable) {
		sysfs_create_files(&client->dev.kobj, &gpo_attributes);
	}

	return 0;

err_register_sysfs:
	lp55xx_unregister_leds(led, chip);
err_register_leds:
	lp55xx_deinit_device(chip);
err_init:
	return ret;
}

static int lp5523_remove(struct i2c_client *client)
{
	struct lp55xx_led *led = i2c_get_clientdata(client);
	struct lp55xx_chip *chip = led->chip;

	lp5523_stop_all_engines(chip);
	lp55xx_unregister_sysfs(chip);
	lp55xx_unregister_leds(led, chip);
	lp55xx_deinit_device(chip);

	return 0;
}

static const struct i2c_device_id lp5523_id[] = {
	{ "lp5523",  LP5523 },
	{ "lp55231", LP55231 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, lp5523_id);

#ifdef CONFIG_OF
static const struct of_device_id of_lp5523_leds_match[] = {
	{ .compatible = "national,lp5523", },
	{ .compatible = "ti,lp55231", },
	{},
};

MODULE_DEVICE_TABLE(of, of_lp5523_leds_match);
#endif

static struct i2c_driver lp5523_driver = {
	.driver = {
		.name	= "lp5523x",
		.of_match_table = of_match_ptr(of_lp5523_leds_match),
	},
	.probe		= lp5523_probe,
	.remove		= lp5523_remove,
	.id_table	= lp5523_id,
};

module_i2c_driver(lp5523_driver);

MODULE_AUTHOR("Mathias Nyman <mathias.nyman@nokia.com>");
MODULE_AUTHOR("Milo Kim <milo.kim@ti.com>");
MODULE_DESCRIPTION("LP5523 LED engine");
MODULE_LICENSE("GPL");
