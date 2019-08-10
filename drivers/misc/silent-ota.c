/* drivers/input/silent_ota.c
 *
 * Copyright (C) 2018 Amazon.com, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/input.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/keycombo.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>

#define SILENT_OTA_NAME "silent-ota"
#define PROP_KEYS_DOWN "keys_down"
#define PROP_KEYS_DOWN_DELAY "keys_down_delay"
#define PROP_KEYS_UP "keys_up"


static struct keys_config {
	struct keycombo_platform_data *pcombo_cfg;
	int combo_cfg_size;
	struct platform_device *pdev_keycombo;
	int first;
}silent_ota_keys_cfg;

struct label {
    const char *name;
    int value;
};

#define LABEL(constant) { #constant, constant }
#define LABEL_END { NULL, -1 }

static struct label key_labels[] = {
        LABEL(KEY_VOLUMEDOWN),
        LABEL(KEY_VOLUMEUP),
        LABEL(KEY_POWER),
        LABEL_END,
};

static struct silent_ota {
	int is_silent;
	struct led_classdev *led_cdev;
	int bl_init_level;
	struct power_supply *ac_psy;
	struct power_supply *usb_psy;
	struct power_supply *curr_psy;
	int ac_psy_present;
	int usb_psy_present;
} silent_ota_inst;

static int get_key_from_label(const char *name)
{
	int i;
	for (i = 0; key_labels[i].name; i++) {
		if (!strcmp(name, key_labels[i].name))
			return key_labels[i].value;
	}
	return -EINVAL;
}

static int __init silent_setup(char *str)
{
	silent_ota_inst.is_silent = simple_strtoul(str, NULL, 10);
	pr_info("%s: silent = %d\n", __func__, silent_ota_inst.is_silent);
	return 1;
}
__setup("silent=", silent_setup);

/* return if backlight is in silent mode */
int silent_ota_bl_is_silent(void)
{
	return !!silent_ota_inst.is_silent;
}

static int __init bl_setup(char *str)
{
	silent_ota_inst.bl_init_level = simple_strtoul(str, NULL, 10);
	pr_info("%s: bl_init_level = %d\n", __func__, silent_ota_inst.bl_init_level);
	return 1;
}
__setup("bl_level=", bl_setup);

static void __silent_ota_turn_on_bl(int on)
{
	int level;
	struct led_classdev *led_cdev = silent_ota_inst.led_cdev;

	if (led_cdev) {
		if (on)
			level = led_cdev->brightness?led_cdev->brightness:silent_ota_inst.bl_init_level;
		else
			level = 0;
		pr_info("%s: on = %d, level = %d\n", __func__, on, level);
		silent_ota_inst.is_silent = 0;
		led_set_brightness(led_cdev, level);
	}
}

static void silent_ota_key_turn_on_bl(struct work_struct *dummy)
{
	__silent_ota_turn_on_bl(1);
}
static DECLARE_WORK(silent_ota_key_turn_on_bl_work, silent_ota_key_turn_on_bl);

static void silent_ota_key_fn(void *data)
{
	if (silent_ota_inst.is_silent)
		queue_work(system_highpri_wq, &silent_ota_key_turn_on_bl_work);
}

static void error_free(struct platform_device *pdev)
{
	struct keycombo_platform_data *pcombo_cfg = silent_ota_keys_cfg.pcombo_cfg;
	if (pcombo_cfg) {
		if (pcombo_cfg->keys_up) {
			devm_kfree(&pdev->dev, pcombo_cfg->keys_up);
			pcombo_cfg->keys_up = NULL;
		}
		devm_kfree(&pdev->dev, pcombo_cfg);
		silent_ota_keys_cfg.pcombo_cfg = NULL;
	}
}

static int dt_to_keycombo_cfg(struct platform_device *pdev, struct keycombo_platform_data **pcfg)
{
	struct device_node *node = pdev->dev.of_node;
	int down_size, up_size, size, ret, idx;
	struct keycombo_platform_data *pcombo_cfg;
	struct device *dev = &pdev->dev;

	/* get keys_down mapping */
	down_size = of_property_count_strings(dev->of_node, PROP_KEYS_DOWN);
	if (IS_ERR_VALUE(down_size)) {
		dev_err(dev, "%s: Failed to get keys_down mapping %d\n", __func__, down_size);
		return 0;
	}

	size = sizeof(struct keycombo_platform_data)
			+ sizeof(int) * (down_size + 1);
	pcombo_cfg = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!pcombo_cfg) {
		dev_err(dev, "%s: Failed to alloc memory %d for struct keycombo_platform_data\n", __func__, size);
		return 0;
	}

	for (idx  = 0; idx < down_size; idx++) {
		const char *name;
		int key;
		ret = of_property_read_string_index(dev->of_node,
						    PROP_KEYS_DOWN, idx,
						    &name);
		if (ret) {
			dev_err(dev, "%s: of read string %s idx %d error %d\n",
				__func__, PROP_KEYS_DOWN, idx, ret);
			goto err;
		}
		key = get_key_from_label(name);
		if (key < 0) {
			dev_err(dev, "%s: invalid down key name %s\n",
				__func__, name);
			goto err;
		}

		pcombo_cfg->keys_down[idx] = key;
	}
	pcombo_cfg->keys_down[idx] = 0;

	/* get keys_down_delay prop */
	ret = of_property_read_u32(node, PROP_KEYS_DOWN_DELAY,
				&pcombo_cfg->key_down_delay);
	if (ret) {
			dev_err(dev, "%s: %s not found\n",
						__func__, PROP_KEYS_DOWN_DELAY);
			goto err;
	}

	/*get keys_up mapping */
	up_size = of_property_count_strings(dev->of_node, PROP_KEYS_UP);
	if (!IS_ERR_VALUE(up_size)) {

		if (up_size > 0) {
			pcombo_cfg->keys_up = devm_kzalloc(&pdev->dev, up_size + 1,
									GFP_KERNEL);
			if (!pcombo_cfg->keys_up) {
				dev_err(dev, "%s: Failed to alloc memory %d for keys_up\n", __func__, up_size + 1);
				goto err;
			}

			for (idx  = 0; idx < up_size; idx++) {
				const char *name;
				int key;
				ret = of_property_read_string_index(dev->of_node,
								    PROP_KEYS_UP, idx,
								    &name);
				if (ret) {
					dev_err(dev, "%s: of read string %s idx %d error %d\n",
						__func__, PROP_KEYS_UP, idx, ret);
					break;
				}
				key = get_key_from_label(name);
				if (key < 0) {
					dev_err(dev, "%s: invalid up key name %s\n",
							__func__, name);
					break;
				}

				pcombo_cfg->keys_up[idx] = key;
			}
			pcombo_cfg->keys_up[idx] = 0;
		}

	}

	*pcfg = pcombo_cfg;
	return size;
err:
	error_free(pdev);
	return 0;
}

static void __silent_ota_psy_work(struct work_struct *dummy)
{
	struct power_supply *psy = silent_ota_inst.curr_psy;
	union power_supply_propval present;
	int ret;

	if (silent_ota_inst.is_silent && psy){
		ret = psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &present);
		if (ret)
			return;

		if (!strncmp(psy->name, "ac", 2) && (silent_ota_inst.ac_psy_present != present.intval)) {
			pr_info("silent: power supply %s present status changed from %d to %d\n", psy->name, silent_ota_inst.ac_psy_present, present.intval);
			silent_ota_inst.ac_psy_present = present.intval;
			__silent_ota_turn_on_bl(1);
		}

		if (!strncmp(psy->name, "usb", 3) && (silent_ota_inst.usb_psy_present != present.intval)) {
			pr_info("silent: power supply %s present status changed from %d to %d\n", psy->name, silent_ota_inst.usb_psy_present, present.intval);
			silent_ota_inst.usb_psy_present = present.intval;
			__silent_ota_turn_on_bl(1);
		}
	}
}
static DECLARE_WORK(silent_ota_psy_work, __silent_ota_psy_work);

static int silent_ota_psy_notification(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct power_supply *psy = (struct power_supply*)data;
	char *psy_list[] = {"ac", "usb"};
	int i = 0;

	if (silent_ota_inst.is_silent && psy){
		while (i++ < sizeof(psy_list)) {
			if (!strncmp(psy->name, psy_list[i], strlen(psy_list[i]))) {
				silent_ota_inst.curr_psy = psy;
				queue_work(system_highpri_wq, &silent_ota_psy_work);
				break;
			}
		}
	}
	return NOTIFY_OK;
}

static struct notifier_block silent_ota_psy_nb = {
	.notifier_call = silent_ota_psy_notification,
};

extern struct led_classdev* led_get_device_by_name(char *name);

static int silent_ota_probe(struct platform_device *pdev)
{
	int ret = -ENOMEM;
	struct power_supply *psy;
	union power_supply_propval present;

	silent_ota_inst.led_cdev = led_get_device_by_name("lcd-backlight");
	if (!silent_ota_inst.led_cdev)
		return -EPROBE_DEFER;

	psy = power_supply_get_by_name("usb");
	if (!psy)
		return -EPROBE_DEFER;
	ret = psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &present);
	if (ret)
		return -EPROBE_DEFER;
	silent_ota_inst.usb_psy_present = present.intval;
	silent_ota_inst.usb_psy = psy;

	psy = power_supply_get_by_name("ac");
	if (!psy)
		return -EPROBE_DEFER;
	ret = psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &present);
	if (ret)
		return -EPROBE_DEFER;
	silent_ota_inst.ac_psy_present = present.intval;
	silent_ota_inst.ac_psy = psy;

	ret =  power_supply_reg_notifier(&silent_ota_psy_nb);
	if (ret)
		return ret;

	silent_ota_keys_cfg.combo_cfg_size = dt_to_keycombo_cfg(pdev, &silent_ota_keys_cfg.pcombo_cfg);
	silent_ota_keys_cfg.first = 1;
	if (!silent_ota_keys_cfg.combo_cfg_size)
		goto error;
	silent_ota_keys_cfg.pcombo_cfg->key_down_fn = silent_ota_key_fn;
	silent_ota_keys_cfg.pcombo_cfg->priv = &silent_ota_keys_cfg;


	silent_ota_keys_cfg.pdev_keycombo = platform_device_alloc(KEYCOMBO_NAME,
							PLATFORM_DEVID_AUTO);
	if (!silent_ota_keys_cfg.pdev_keycombo)
		goto error;

	silent_ota_keys_cfg.pdev_keycombo->dev.parent = &pdev->dev;

	ret = platform_device_add_data(silent_ota_keys_cfg.pdev_keycombo, silent_ota_keys_cfg.pcombo_cfg, silent_ota_keys_cfg.combo_cfg_size);
	if (ret)
		goto error;
	return platform_device_add(silent_ota_keys_cfg.pdev_keycombo);
error:
	error_free(pdev);
	if (silent_ota_keys_cfg.pdev_keycombo)
		platform_device_put(silent_ota_keys_cfg.pdev_keycombo);
	return ret;
}

int silent_ota_remove(struct platform_device *pdev)
{
	if (silent_ota_keys_cfg.pdev_keycombo)
		platform_device_put(silent_ota_keys_cfg.pdev_keycombo);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id silent_ota_dt_match[] = {
	{	.compatible = "amzn,silent-ota",
	},
	{}
};
#endif

struct platform_driver silent_ota_driver = {
	.probe	= silent_ota_probe,
	.remove = silent_ota_remove,
	.driver = {
		.name = SILENT_OTA_NAME,
		.owner = THIS_MODULE,
		.of_match_table = silent_ota_dt_match,
	},
};

static int __init silent_ota_init(void)
{
	if (silent_ota_inst.is_silent)
		return platform_driver_register(&silent_ota_driver);
	else
		return 0;
}

static void __exit silent_ota_exit(void)
{
	platform_driver_unregister(&silent_ota_driver);
}

module_init(silent_ota_init);
module_exit(silent_ota_exit);
MODULE_LICENSE("GPL");
