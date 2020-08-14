/*
 * sign_of_life.c
 *
 * Device Sign of Life information driver
 * Please check Documentation/amazon/sign_of_life.txt for more information
 *
 * Copyright 2015-2017 Amazon Technologies, Inc. All Rights Reserved.
 *
 * Yang Liu (yangliu@lab126.com)
 * Jiangli Yuan (jly@amazon.com)
 * TODO: Add additional contributor's names.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/sign_of_life.h>
#include "sign_of_life_platform.h"

#define DEV_SOL_VERSION	"2.0"
#define DEV_SOL_PROC_NAME	"life_cycle_reason"
#define MAX_SIZE	10

static struct proc_dir_entry *life_cycle_reason_file;
static bool is_charger_mode;

struct dev_sol {
	char version[MAX_SIZE];
	u8 life_cycle_reason_idx;
	sign_of_life_ops sol_ops;
};
static struct dev_sol *p_dev_sol;

bool __attribute__((weak)) is_charging_mode(void)
{
	return false;
}

static int search_lcr_table(const struct sign_of_life_reason_data *table,
					int num, life_cycle_reason_t reason)
{
	int i;
	/* start from table[1], table[0] is the default value */
	for (i = 1; i < num; i++) {
			if (table[i].reason_value == reason)
				return i;
		}
	/* fails to match with any predefined reason, print a warnning msg */
	pr_warn("the lcr_reason %d is not in the pre-defined table\n", reason);

	/* fails to find a good reason, return the default index= 0 */
	return 0;
}

static int life_cycle_reason_lookup(void)
{
	life_cycle_reason_t boot_reason = LIFE_CYCLE_NOT_AVAILABLE;
	life_cycle_reason_t shutdown_reason = LIFE_CYCLE_NOT_AVAILABLE;
	life_cycle_reason_t thermal_shutdown_reason = LIFE_CYCLE_NOT_AVAILABLE;
	life_cycle_reason_t s_mode = LIFE_CYCLE_NOT_AVAILABLE;
	int table_num = ARRAY_SIZE(lcr_data);
	int index = 0;

	if (!p_dev_sol) {
			pr_err("the life cycle driver is not initialized\n");
			return -1;
		}

	if (!((p_dev_sol->sol_ops.read_boot_reason) &&
						(p_dev_sol->sol_ops.read_shutdown_reason) &&
								(p_dev_sol->sol_ops.read_thermal_shutdown_reason) &&
										(p_dev_sol->sol_ops.read_special_mode))) {
			pr_err("no platform supported\n");
			return -1;
		}

	p_dev_sol->sol_ops.read_boot_reason(&boot_reason);
	p_dev_sol->sol_ops.read_shutdown_reason(&shutdown_reason);
	p_dev_sol->sol_ops.read_thermal_shutdown_reason(
								&thermal_shutdown_reason);
	p_dev_sol->sol_ops.read_special_mode(&s_mode);

	/* step 1, find abnormal thermal shutdown reason if it's valid */
	if (thermal_shutdown_reason > 0) {
			index = search_lcr_table(lcr_data, table_num,
											thermal_shutdown_reason);
			if (index)
				goto found;
		}

	/* step 2, find shutdown reason if it's valid */
	if (shutdown_reason > 0) {
			index = search_lcr_table(lcr_data, table_num,
											shutdown_reason);
			if (index)
				goto found;
		}

	/* step 3, find reboot reason if it's valid */
	if (boot_reason > 0) {
			index = search_lcr_table(lcr_data, table_num,
											boot_reason);
			if (index)
				goto found;
		}
	/* step 4, find special mode reason if it's valid */
	if (s_mode > 0) {
			index = search_lcr_table(lcr_data, table_num,
											s_mode);
			if (index)
				goto found;
		}

found:
	p_dev_sol->life_cycle_reason_idx = index;
	return index;
}

static int life_cycle_reason_show(struct seq_file *m, void *v)
{
	if (p_dev_sol) {
			seq_printf(m, "lc_version:%s\n", p_dev_sol->version);
			seq_printf(m, "lc_reason:%s\n",
								lcr_data[p_dev_sol->life_cycle_reason_idx].life_cycle_reasons);
			seq_printf(m, "lc_type:%s\n",
								lcr_data[p_dev_sol->life_cycle_reason_idx].life_cycle_type);
		} else
		seq_printf(m, "life cycle reason driver is not initialized");
	return 0;
}

static int life_cycle_reason_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, life_cycle_reason_show, NULL);
}

static const struct file_operations life_cycle_reason_proc_fops = {
	.open = life_cycle_reason_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void life_cycle_reason_proc_init(void)
{
	life_cycle_reason_file = proc_create(DEV_SOL_PROC_NAME,
							  0444, NULL, &life_cycle_reason_proc_fops);
	if (life_cycle_reason_file == NULL)
		pr_err("can't create life cycle reason proc entry\n");
}

static void life_cycle_reason_proc_done(void)
{
	remove_proc_entry(DEV_SOL_PROC_NAME, NULL);
}

/*
 *  * life_cycle_set_boot_reason
 *   * Description: this function will set the boot reason which
 *    * causing device booting
 *     * @boot_reason	next boot reason
 *      * @Return	0 on success, -1 on failure
 *       */
int life_cycle_set_boot_reason(life_cycle_reason_t boot_reason)
{
	if (!p_dev_sol) {
			pr_err("the life cycle driver is not initialized\n");
			return -1;
		}

	if (!p_dev_sol->sol_ops.write_boot_reason) {
			pr_err("no platform supported\n");
			return -1;
		} else
		p_dev_sol->sol_ops.write_boot_reason(boot_reason);

	return 0;
}
EXPORT_SYMBOL(life_cycle_set_boot_reason);

/*
 *  * life_cycle_set_shutdown_reason
 *   * Description: this function will set the Shutdown reason
 *    * which causing device shutdown
 *     * @shutdown_reason	the shutdown reason excluded thermal shutdown
 *      * @Return	0 on success, -1 on failure
 *       */
int life_cycle_set_shutdown_reason(life_cycle_reason_t shutdown_reason)
{
	if (!p_dev_sol) {
			pr_err("the life cycle driver is not initialized\n");
			return -1;
		}

	if (!p_dev_sol->sol_ops.write_shutdown_reason) {
			pr_err("no platform supported\n");
			return -1;
		} else
		p_dev_sol->sol_ops.write_shutdown_reason(shutdown_reason);

	return 0;
}
EXPORT_SYMBOL(life_cycle_set_shutdown_reason);

/*
 *  * life_cycle_set_thermal_shutdown_reason
 *   * Description: this function will set the Thermal Shutdown reason
 *    * which causing device shutdown
 *     * @thermal_shutdown_reason the thermal shutdown reason
 *      * @Return	0 on success, -1 on failure
 *       */
int life_cycle_set_thermal_shutdown_reason(life_cycle_reason_t
				thermal_shutdown_reason)
{
	if (!p_dev_sol) {
			pr_err("the life cycle driver is not initialized\n");
			return -1;
		}

	if (!p_dev_sol->sol_ops.write_thermal_shutdown_reason) {
			pr_err("no platform supported\n");
			return -1;
		} else
		p_dev_sol->sol_ops.write_thermal_shutdown_reason(
										thermal_shutdown_reason);

	return 0;
}
EXPORT_SYMBOL(life_cycle_set_thermal_shutdown_reason);

/*
 *  * life_cycle_set_special_mode
 *   * Description: this function will set the special mode which causing
 *    * device life cycle change
 *     * @life_cycle_special_mode	the special mode value
 *      * @Return	0 on success, -1 on failure
 *       */
int life_cycle_set_special_mode(life_cycle_reason_t life_cycle_special_mode)
{
	if (!p_dev_sol) {
			pr_err("the life cycle driver is not initialized\n");
			return -1;
		}

	if (!p_dev_sol->sol_ops.write_special_mode) {
			pr_err("no platform supported\n");
			return -1;
		} else
		p_dev_sol->sol_ops.write_special_mode(life_cycle_special_mode);

	return 0;
}
EXPORT_SYMBOL(life_cycle_set_special_mode);

extern int life_cycle_platform_init(sign_of_life_ops *sol_ops);
int __weak life_cycle_platform_init(sign_of_life_ops *sol_ops)
{
	pr_err("no supported platform is configured\n");
	return 0;
}

static int __init dev_sol_init(void)
{
	pr_info("Amazon: sign of life device driver init\n");
	p_dev_sol = kzalloc(sizeof(struct dev_sol), GFP_KERNEL);
	if (!p_dev_sol) {
			pr_err("kmalloc allocation failed\n");
			return -ENOMEM;
		}
	strncpy(p_dev_sol->version, DEV_SOL_VERSION, MAX_SIZE);
	/* set the default life cycle reason */
	p_dev_sol->life_cycle_reason_idx = 0;

	/* create the proc entry for life cycle reason */
	life_cycle_reason_proc_init();

	/* initialize the platform dependent life cycle
	 * 	 * operation implementation
	 * 	 	 */
	life_cycle_platform_init(&p_dev_sol->sol_ops);

	/* look up the life cycle reason
	 * 	 * if fails, prints out a warnning but continues.
	 * 	 	 */
	if (!life_cycle_reason_lookup())
		pr_warn("didn't find the right lcr reason for previous booting\n");

	/* Judge charger mode in case of not using androidboot.mode */
	if (is_charging_mode())
		is_charger_mode = true;

	/* clean up the life cycle reason settings */
	if (!is_charger_mode) {
		pr_info("not in charger mode. Reset LCR registers.\n");
		if (p_dev_sol->sol_ops.lcr_reset)
			p_dev_sol->sol_ops.lcr_reset();
	} else
		pr_info("in charger mode. Don't reset LCR registers.\n");

	pr_info("life cycle reason is %s\n",
					lcr_data[p_dev_sol->life_cycle_reason_idx].life_cycle_reasons);

	return 0;
}

static void __exit dev_sol_cleanup(void)
{
	life_cycle_reason_proc_done();
	kfree(p_dev_sol);
}

static int __init sol_bootmode(char *options)
{
	pr_info("bootmode=%s\n", options);
	if (0 == strcmp("charger", options))
		is_charger_mode = true;

	return 0;
}
__setup("androidboot.mode=", sol_bootmode);

late_initcall(dev_sol_init);
module_exit(dev_sol_cleanup);
MODULE_LICENSE("GPL v2");

