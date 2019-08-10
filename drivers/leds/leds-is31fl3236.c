/*
 * leds-is31fl3236.c
 *
 * Copyright (c) 2016 Amazon.com, Inc. or its affiliates. All Rights Reserved
 *
 * The code contained herein is licensed under the GNU General Public
 * License Version 2. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include "leds-is31fl3236.h"

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/notifier.h>
#include <linux/reboot.h>

#define REG_SW_SHUTDOWN 0x00
#define REG_PWM_BASE 0x01
#define REG_UPDATE 0x25
#define REG_CTRL_BASE 0x26
#define REG_G_CTRL 0x4A /* Global Control Register */
#define REG_RST 0x4F

#define LED_SW_ON 0x01
#define LED_CTRL_UPDATE 0x0
#define LED_CHAN_DISABLED 0x0
#define LED_CHAN_ENABLED 0x01

#define LED_CURRENT_1   0x00
#define LED_CURRENT_1_2 0x01
#define LED_CURRENT_1_3 0x02
#define LED_CURRENT_1_4 0x03
#define LED_CURRENT_DEFAULT LED_CURRENT_1_4

#define BOOT_ANIMATION_FRAME_DELAY 88

struct is31fl3236_data {
	struct mutex lock;
	bool play_boot_animation;
	bool setup_device;
	int enable_gpio;
	uint8_t ch_offset;
	struct task_struct *boot_anim_task;
	int enabled;
	uint8_t *state;
	uint8_t led_current;
	struct i2c_client *client;
	struct notifier_block reboot_notifier;
};

static int is31fl3236_write_reg(struct i2c_client *client,
				uint32_t reg,
				uint8_t value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

static int is31fl3236_update_led(struct i2c_client *client,
				 uint32_t led,
				 uint8_t value)
{
	return is31fl3236_write_reg(client, REG_PWM_BASE+led, value);
}

static int update_frame(struct is31fl3236_data *pdata,
						const uint8_t *buf)
{
	struct i2c_client *client = pdata->client;
	int ret;
	int i;

	mutex_lock(&pdata->lock);

	for (i = 0; i < NUM_CHANNELS; i++) {
		pdata->state[i] = buf[i];
		ret = is31fl3236_update_led(client,
				(i+pdata->ch_offset)%NUM_CHANNELS,
				pdata->state[i]);
		if (ret != 0)
			goto fail;
	}
	ret = is31fl3236_write_reg(client, REG_UPDATE, 0x0);
fail:
	mutex_unlock(&pdata->lock);
	return ret;
}

static int boot_anim_thread(void *data)
{
	struct is31fl3236_data *pdata = (struct is31fl3236_data *)data;
	int i = 0;

	while (!kthread_should_stop()) {
		update_frame(pdata, &frames[i][0]);
		msleep(BOOT_ANIMATION_FRAME_DELAY);
		i = (i + 1) % ARRAY_SIZE(frames);
	}
	update_frame(pdata, clear_frame);
	return 0;
}


static ssize_t boot_animation_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t len)
{
	struct is31fl3236_data *pdata = dev_get_platdata(dev);
	uint8_t val;
	ssize_t ret;
	struct task_struct *stop_struct = NULL;

	ret = kstrtou8(buf, 10, &val);
	if (ret) {
		pr_err("ISSI: Invalid input to store_boot_anim\n");
		goto fail;
	}

	mutex_lock(&pdata->lock);
	if (!val) {
		if (pdata->boot_anim_task) {
			stop_struct = pdata->boot_anim_task;
			pdata->boot_anim_task = NULL;
		}
	} else {
		if (!pdata->boot_anim_task) {
			pdata->boot_anim_task = kthread_run(boot_anim_thread,
							(void *)pdata,
							"boot_animation_thread");
			if (IS_ERR(pdata->boot_anim_task))
				pr_err("ISSI: could not create boot animation thread\n");
		}
	}
	mutex_unlock(&pdata->lock);

	if (stop_struct)
		kthread_stop(stop_struct);

	ret = len;
fail:
	return ret;
}

static ssize_t boot_animation_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct is31fl3236_data *pdata = dev_get_platdata(dev);
	int ret;

	mutex_lock(&pdata->lock);
	ret = sprintf(buf, "%d\n", pdata->boot_anim_task != NULL);
	mutex_unlock(&pdata->lock);

	return ret;
}
static DEVICE_ATTR_RW(boot_animation);

static ssize_t led_current_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t len)
{
	struct is31fl3236_data *pdata = dev_get_platdata(dev);
	struct i2c_client *client = pdata->client;
	uint8_t val;
	int i;
	ssize_t ret;

	ret = kstrtou8(buf, 10, &val);
	if (ret) {
		pr_err("ISSI: Invalid input to store_led_current\n");
		goto fail;
	}
	if (val < LED_CURRENT_1 || val > LED_CURRENT_1_4) {
		pr_err("ISSI: Invalid led current division\n");
		ret = -EINVAL;
		goto fail;
	}

	mutex_lock(&pdata->lock);
	pdata->led_current = val;
	val = (val << 1) | 0x1;
	for (i = 0; i < NUM_CHANNELS; i++)
		is31fl3236_write_reg(client, REG_CTRL_BASE + i, val);

	is31fl3236_write_reg(client, REG_UPDATE, 0x0);
	mutex_unlock(&pdata->lock);
	ret = len;
fail:
	return ret;
}

static ssize_t led_current_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct is31fl3236_data *pdata = dev_get_platdata(dev);
	int ret;

	mutex_lock(&pdata->lock);
	ret = sprintf(buf, "%d\n", pdata->led_current);
	mutex_unlock(&pdata->lock);

	return ret;
}
static DEVICE_ATTR_RW(led_current);

static ssize_t frame_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	struct is31fl3236_data *pdata = dev_get_platdata(dev);
	int len = 0;
	int i = 0;

	mutex_lock(&pdata->lock);
	for (i = 0; i < NUM_CHANNELS; i++) {
		len += sprintf(buf, "%s%02x",
				buf, pdata->state[i]);
	}
	mutex_unlock(&pdata->lock);
	return sprintf(buf, "%s\n", buf);
}

static ssize_t frame_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t len)
{
	struct is31fl3236_data *pdata = dev_get_platdata(dev);
	uint8_t new_state[NUM_CHANNELS];
	char val[3];
	int count = 0;
	int ret, i;

	val[2] = '\0';
	for (i = 0; i < NUM_CHANNELS * 2; i += 2) {
		val[0] = buf[i];
		val[1] = buf[i + 1];
		ret = kstrtou8(val, 16, &new_state[count]);
		if (ret) {
			pr_err("ISSI: Invalid input for frame_store: %d\n",
			       i);
			return ret;
		}
		count++;
	}

	ret = update_frame(pdata, &new_state[0]);
	if (ret < 0) {
		pr_err("ISSI: could not update frame\n");
		return ret;
	}

	return len;
}

static DEVICE_ATTR_RW(frame);

static void is31fl3236_parse_dt(struct is31fl3236_data *pdata,
				struct device_node *node)
{
	pdata->play_boot_animation = of_property_read_bool(node,
							"play-boot-animation");
	pdata->setup_device = of_property_read_bool(node, "setup-device");
	pdata->enable_gpio = of_get_named_gpio(node, "enable-gpio", 0);
	/*Read a channel offset, if one is available*/
	if (of_property_read_u8(node, "channel-offset", &pdata->ch_offset))
		pdata->ch_offset = 0;
	/*Offset should not exceed the number of channels available*/
	if (pdata->ch_offset >= NUM_CHANNELS) {
		pr_err("ISSI: Invalid channel offset=%d\n", pdata->ch_offset);
		pdata->ch_offset = 0;
	}
}

static int is31fl3236_power_on_device(struct i2c_client *client)
{
	struct is31fl3236_data *pdata = dev_get_platdata(&client->dev);
	int i;
	int ret = 0;

	if (pdata->setup_device) {
		if (gpio_is_valid(pdata->enable_gpio)) {
			ret = devm_gpio_request_one(&client->dev,
						    pdata->enable_gpio,
						    GPIOF_DIR_OUT,
						    "is31fl3236_enable");
			if (ret < 0) {
				pr_err("ISSI: could not request enable gpio\n");
				return ret;
			}
			/*Device goes into shutdown if enable*/
			/*gpio is low so force it high always*/
			gpio_set_value(pdata->enable_gpio, 1);
		}

		ret = is31fl3236_write_reg(client, REG_RST, 0x0);
		if (ret < 0) {
			pr_err("ISSI: Could not reset registers\n");
			goto fail;
		}

		ret = is31fl3236_write_reg(client, REG_SW_SHUTDOWN, LED_SW_ON);
		if (ret < 0) {
			pr_err("ISSI: Could not start device\n");
			goto fail;
		}
	}
	pdata->enabled = LED_CHAN_ENABLED;

	for (i = 0; i < NUM_CHANNELS; i++) {
		ret = is31fl3236_write_reg(client, REG_CTRL_BASE + i, 0x01);
		if (ret < 0) {
			pr_err("ISSI: Could not enabled led: %d\n", i);
			goto fail;
		}
	}
	ret = is31fl3236_write_reg(client, REG_UPDATE, 0x0);

	return ret;
fail:
	if (gpio_is_valid(pdata->enable_gpio)) {
		gpio_set_value(pdata->enable_gpio, 0);
		devm_gpio_free(&client->dev, pdata->enable_gpio);
	}
	return ret;
}

static int is31fl3236_reboot_callback(struct notifier_block *self,
				      unsigned long val,
				      void *data)
{
	int ret;

	pr_err("ISSI: resetting device before reboot!\n");
	struct is31fl3236_data *pdata =
		container_of(self, struct is31fl3236_data, reboot_notifier);
	ret = is31fl3236_write_reg(pdata->client, REG_RST, 0x0);
	if (ret < 0) {
		pr_err("ISSI: Could not reset registers\n");
		return notifier_from_errno(-EIO);
	}
	return NOTIFY_DONE;
}

static int is31fl3236_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct is31fl3236_data *pdata = devm_kzalloc(&client->dev,
						sizeof(struct is31fl3236_data),
						GFP_KERNEL);
	int ret = 0;

	if (!pdata) {
		pr_err("ISSI: Could not allocate memory for platform data\n");
		ret = -ENOMEM;
		goto fail;
	}

	pdata->client = client;
	pdata->state = devm_kzalloc(&client->dev,
				    sizeof(uint8_t) * NUM_CHANNELS,
								GFP_KERNEL);
	if (!pdata->state) {
		pr_err("ISSI: Could not allocate memory for state data\n");
		ret = -ENOMEM;
		devm_kfree(&client->dev, pdata);
		goto fail;
	}
	pdata->led_current = LED_CURRENT_DEFAULT;
	pdata->enabled = LED_CHAN_DISABLED;
	mutex_init(&pdata->lock);
	pdata->boot_anim_task = NULL;
	pdata->reboot_notifier.notifier_call = is31fl3236_reboot_callback;
	is31fl3236_parse_dt(pdata, client->dev.of_node);

	client->dev.platform_data = pdata;
	ret = is31fl3236_power_on_device(client);
	if (ret < 0) {
		pr_err("ISSI: Could not power on device: %d\n", ret);
		goto fail;
	}

	ret = device_create_file(&client->dev, &dev_attr_frame);
	if (ret) {
		pr_err("ISSI: Could not create frame sysfs entry\n");
		goto fail;
	}
	ret = device_create_file(&client->dev, &dev_attr_led_current);
	if (ret) {
		pr_err("ISSI: Could not create brightness sysfs entry\n");
		goto fail;
	}

	ret = device_create_file(&client->dev, &dev_attr_boot_animation);
	if (ret) {
		pr_err("ISSI: Could not create boot animation entry\n");
		goto fail;
	}

	ret = register_reboot_notifier(&pdata->reboot_notifier);
	if (ret) {
		pr_err("ISSI: Could not register reboot callback\n");
		goto fail;
	}

	if (pdata->play_boot_animation) {
		mutex_lock(&pdata->lock);
		pdata->boot_anim_task = kthread_run(boot_anim_thread,
						    (void *)pdata,
						    "boot_animation_thread");
		if (IS_ERR(pdata->boot_anim_task))
			pr_err("ISSI: could not start boot animation thread");
		mutex_unlock(&pdata->lock);
	}
fail:
	return ret;
}

static int is31fl3236_remove(struct i2c_client *client)
{
	struct is31fl3236_data *pdata = dev_get_platdata(&client->dev);
	int ret = 0;

	mutex_lock(&pdata->lock);
	if (pdata->boot_anim_task) {
		kthread_stop(pdata->boot_anim_task);
		pdata->boot_anim_task = NULL;
	}

	ret = is31fl3236_write_reg(client, REG_RST, 0x0);
	if (ret < 0) {
		pr_err("ISSI: Could not reset registers\n");
		goto fail;
	}

fail:
	if (gpio_is_valid(pdata->enable_gpio))
		gpio_set_value(pdata->enable_gpio, 0);
	mutex_unlock(&pdata->lock);
	return ret;
}

static struct i2c_device_id is31fl3236_i2c_match[] = {
	{"issi,is31fl3236", 0},
};
MODULE_DEVICE_TABLE(i2c, is31fl3236_i2c_match);

static struct of_device_id is31fl3236_of_match[] = {
	{ .compatible = "issi,is31fl3236"},
};
MODULE_DEVICE_TABLE(of, is31fl3236_of_match);

static struct i2c_driver is31fl3236_driver = {
	.driver = {
		.name = "is31fl3236",
		.of_match_table = of_match_ptr(is31fl3236_of_match),
	},
	.probe = is31fl3236_probe,
	.remove = is31fl3236_remove,
	.id_table = is31fl3236_i2c_match,
};

module_i2c_driver(is31fl3236_driver);

MODULE_AUTHOR("Amazon.com");
MODULE_DESCRIPTION("ISSI IS31FL3236 LED Driver");
MODULE_LICENSE("GPL");
