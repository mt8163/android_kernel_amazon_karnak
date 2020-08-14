/* drivers/input/keypanic.c
 *
 * Copyright (C) 2017 Amazon.com, Inc.
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

#define KEYPANIC_NAME "keypanic"
#define PROP_KEYS_DOWN "keys_down"
#define PROP_KEYS_DOWN_DELAY "keys_down_delay"
#define PROP_KEYS_UP "keys_up"

#define PANIC_MSG_SIZE 100
struct keys_config {
	struct keycombo_platform_data *pcombo_cfg;
	int combo_cfg_size;
	struct platform_device *pdev_keycombo;
	char panic_msg[PANIC_MSG_SIZE];
} keys_cfg = {
	.panic_msg = "Trigger panic by press ",
};

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
/* Must comment out otherwise compile fails.
static const char *get_label_from_key(int key)
{
	int i;

	for (i = 0; key_labels[i].value; i++) {
		if (key == key_labels[i].value)
			return key_labels[i].name;
	}
	return NULL;
}
*/
static int get_key_from_label(const char *name)
{
	int i;

	for (i = 0; key_labels[i].name; i++) {
		if (!strcmp(name, key_labels[i].name))
			return key_labels[i].value;
	}
	return -EINVAL;
}

static void do_panic(void *data)
{
	panic(keys_cfg.panic_msg);
}

static void error_free(struct platform_device *pdev)
{
	struct keycombo_platform_data *pcombo_cfg = keys_cfg.pcombo_cfg;

	if (pcombo_cfg) {
		if (pcombo_cfg->keys_up) {
			devm_kfree(&pdev->dev, pcombo_cfg->keys_up);
			pcombo_cfg->keys_up = NULL;
		}
		devm_kfree(&pdev->dev, pcombo_cfg);
		keys_cfg.pcombo_cfg = NULL;
	}
}

static int dt_to_keycombo_cfg(struct platform_device *pdev,
					struct keycombo_platform_data **pcfg)
{
	struct device_node *node = pdev->dev.of_node;
	int down_size, up_size, size, ret, idx;
	struct keycombo_platform_data *pcombo_cfg;
	struct device *dev = &pdev->dev;

	pr_info("%s: entry\n", __func__);
	/* get keys_down mapping */
	down_size = of_property_count_strings(dev->of_node, PROP_KEYS_DOWN);
	if (IS_ERR_VALUE((unsigned long)down_size)) {
		dev_err(dev, "%s: Failed to get keys_down mapping %d\n",
					__func__, down_size);
		return 0;
	}

	size = sizeof(struct keycombo_platform_data)
				+ sizeof(int) * (down_size + 1);
	pcombo_cfg = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!pcombo_cfg) {
		dev_err(dev, "%s: Failed to alloc memory %d\n", __func__, size);
		return 0;
	}

	for (idx  = 0; idx < down_size; idx++) {
		const char *name;
		int key;

		ret = of_property_read_string_index(dev->of_node,
					PROP_KEYS_DOWN, idx, &name);
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
		strncat(keys_cfg.panic_msg + strlen(keys_cfg.panic_msg), name,
			PANIC_MSG_SIZE - strlen(keys_cfg.panic_msg));
		strncat(keys_cfg.panic_msg + strlen(keys_cfg.panic_msg), " ",
			PANIC_MSG_SIZE - strlen(keys_cfg.panic_msg));
	}
	pcombo_cfg->keys_down[idx] = 0;

	/* get keys_down_delay prop */
	ret = of_property_read_u32(node, PROP_KEYS_DOWN_DELAY,
				&pcombo_cfg->key_down_delay);
	if (ret) {
		dev_err(dev, "%s: %s not found\n", __func__,
			PROP_KEYS_DOWN_DELAY);
		goto err;
	}

	pr_info("%s: key_down_delay: %d\n", __func__,
				pcombo_cfg->key_down_delay);

	/*get keys_up mapping */
	up_size = of_property_count_strings(dev->of_node, PROP_KEYS_UP);
	if (IS_ERR_VALUE((unsigned long)up_size)) {
		dev_err(dev, "%s: keys_up mapping is not exist: %d\n",
					__func__, up_size);
		goto bypass;
	}

	pcombo_cfg->keys_up = devm_kzalloc(&pdev->dev, up_size + 1, GFP_KERNEL);
	if (!pcombo_cfg->keys_up) {
		dev_err(dev, "%s: Failed to alloc memory %d for keys_up\n",
			__func__, up_size + 1);
		goto err;
	}

	for (idx  = 0; idx < up_size; idx++) {
		const char *name;
		int key;

		ret = of_property_read_string_index(dev->of_node,
				PROP_KEYS_UP, idx, &name);
		if (ret) {
			dev_err(dev,
				"%s: of read string %s idx %d error %d\n",
				__func__, PROP_KEYS_UP, idx, ret);
			break;
		}
		key = get_key_from_label(name);
		if (key < 0) {
			dev_err(dev,
				"%s: invalid up key name %s\n",
				__func__, name);
			break;
		}

		pcombo_cfg->keys_up[idx] = key;
	}
	pcombo_cfg->keys_up[idx] = 0;

bypass:
	*pcfg = pcombo_cfg;
	pr_info("%s: exit\n", __func__);
	return size;
err:
	error_free(pdev);
	return 0;
}

static int keypanic_probe(struct platform_device *pdev)
{
	int ret = -ENOMEM;

	keys_cfg.combo_cfg_size = dt_to_keycombo_cfg(pdev,
						&keys_cfg.pcombo_cfg);
	if (!keys_cfg.combo_cfg_size)
		goto error;
	keys_cfg.pcombo_cfg->key_down_fn = do_panic;
	keys_cfg.pcombo_cfg->priv = &keys_cfg;


	keys_cfg.pdev_keycombo = platform_device_alloc(KEYCOMBO_NAME,
							PLATFORM_DEVID_AUTO);
	if (!keys_cfg.pdev_keycombo)
		goto error;

	keys_cfg.pdev_keycombo->dev.parent = &pdev->dev;

	ret = platform_device_add_data(keys_cfg.pdev_keycombo,
			keys_cfg.pcombo_cfg, keys_cfg.combo_cfg_size);
	if (ret)
		goto error;
	pr_info("%s: successed\n", __func__);
	return platform_device_add(keys_cfg.pdev_keycombo);
error:
	error_free(pdev);
	if (keys_cfg.pdev_keycombo)
		platform_device_put(keys_cfg.pdev_keycombo);
	return ret;
}

int keypanic_remove(struct platform_device *pdev)
{
	if (keys_cfg.pdev_keycombo)
		platform_device_put(keys_cfg.pdev_keycombo);
	return 0;
}

static struct of_device_id keypanic_dt_match[] = {
	{ .compatible = "amzn,keypanic", },
	{}
};

struct platform_driver keypanic_driver = {
	.remove = keypanic_remove,
	.driver = {
		.name = KEYPANIC_NAME,
		.owner = THIS_MODULE,
		.of_match_table = keypanic_dt_match,
	},
};

module_platform_driver_probe(keypanic_driver, keypanic_probe);

MODULE_LICENSE("GPL");
