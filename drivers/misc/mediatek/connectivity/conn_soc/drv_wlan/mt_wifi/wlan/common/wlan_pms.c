/*
 * Copyright © 2016, Varun Chitre "varun.chitre15" <varun.chitre15@gmail.com>
 *
 * Mediatek WLAN Power Mode Switching driver
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
 
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/kernel.h>
#include "precomp.h"
#include "mgmt/rsn.h"
#include "wlan_pms.h"

PARAM_POWER_MODE custPowerMode = 2;
bool lock = false;

static ssize_t pm_mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", custPowerMode);
}

static ssize_t pm_mode_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int newval;
	sscanf(buf, "%d", &newval);
	switch(newval) {
		case 0:
		case 1:
		case 2:
			lock = true;
			custPowerMode = newval;
		break;
		default:
			pr_info("pm: invalid power mode\n");
		break;
	}
	return count;
}

static struct kobj_attribute pm_mode_attribute =
	__ATTR(pm_mode,
		0660,
		pm_mode_show,
		pm_mode_store);

static struct attribute *pm_mode_attrs[] =
	{
		&pm_mode_attribute.attr,
		NULL,
	};

static struct attribute_group pm_mode_attr_group =
	{
		.attrs = pm_mode_attrs,
	};

static struct kobject *pm_mode_kobj;

static int pm_mode_probe(void)
{
	int sysfs_result;
	printk(KERN_DEBUG "[%s]\n",__func__);

	pm_mode_kobj = kobject_create_and_add("mt_wifi", kernel_kobj);

	if (!pm_mode_kobj) {
		pr_err("%s Interface create failed!\n",
			__FUNCTION__);
		return -ENOMEM;
        }

	sysfs_result = sysfs_create_group(pm_mode_kobj,
			&pm_mode_attr_group);

	if (sysfs_result) {
		pr_info("%s sysfs create failed!\n", __FUNCTION__);
		kobject_put(pm_mode_kobj);
	}
	return sysfs_result;
}

static void pm_mode_remove(void)
{
	if (pm_mode_kobj != NULL)
		kobject_put(pm_mode_kobj);
}

module_init(pm_mode_probe);
module_exit(pm_mode_remove);
MODULE_LICENSE("GPL and additional rights");
MODULE_AUTHOR("Varun Chitre <varun.chitre15@gmail.com>");
MODULE_DESCRIPTION("MTK WLAN Power Management driver");
