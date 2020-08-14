/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/types.h>
#ifdef CONFIG_MTK_HIBERNATION
#include <mtk_hibernate_dpm.h>
#endif
#include <linux/clk.h>
#include "sync_write.h"
#include "devapc.h"

static struct clk *dapc_clk;
static struct cdev *g_devapc_ctrl;
static unsigned int devapc_irq;
static void __iomem *devapc_ao_base;
static void __iomem *devapc_pd_base;

#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
static int clear_vio_status(unsigned int module);
#endif
static int devapc_ioremap(void);

int mt_devapc_check_emi_violation(void)
{
	return 0;
}

int mt_devapc_emi_initial(void)
{
	return 0;
}

int mt_devapc_clear_emi_violation(void)
{
	return 0;
}

static int devapc_ioremap(void)
{
	struct device_node *node = NULL;
	/*IO remap*/
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8163-devapc_ao");
	if (node) {
		devapc_ao_base = of_iomap(node, 0);
		pr_debug("[DEVAPC] AO_ADDRESS %p\n", devapc_ao_base);
	} else {
		pr_err("[DEVAPC] can't find DAPC_AO compatible node\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8163-devapc");
	if (node) {
		devapc_pd_base = of_iomap(node, 0);
		devapc_irq = irq_of_parse_and_map(node, 0);
		pr_debug("[DEVAPC] PD_ADDRESS %p, IRD: %d\n",
			devapc_pd_base, devapc_irq);
	} else {
		pr_err("[DEVAPC] can't find DAPC_PD compatible node\n");
		return -1;
	}

	return 0;
}

#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
#ifdef CONFIG_MTK_HIBERNATION
static int devapc_pm_restore_noirq(struct device *device)
{
	if (devapc_irq != 0) {
		mt_irq_set_sens(devapc_irq, MT_LEVEL_SENSITIVE);
		mt_irq_set_polarity(devapc_irq, MT_POLARITY_LOW);
	}

	return 0;
}
#endif
#endif

/*
 * set_module_apc: set module permission on device apc.
 * @module: the moudle to specify permission
 * @devapc_num: device apc index number (device apc 0 or 1)
 * @domain_num: domain index number (AP or MD domain)
 * @permission_control: specified permission
 * no return value.
 */
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
static void start_devapc(void)
{
}
#else
#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
static void set_module_apc(unsigned int module, E_MASK_DOM domain_num,
				APC_ATTR permission_control)
{
}

/*
 * unmask_module_irq: unmask device apc irq for specified module.
 * @module: the moudle to unmask
 * @devapc_num: device apc index number (device apc 0 or 1)
 * @domain_num: domain index number (AP or MD domain)
 * no return value.
 */
static int unmask_module_irq(unsigned int module)
{
	return 0;
}

static void init_devpac(void)
{
}


/*
 * start_devapc: start device apc for MD
 */
static int start_devapc(void)
{
	return 0;
}

#endif/*ifnef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT*/
#endif


/*
 * clear_vio_status: clear violation status for each module.
 * @module: the moudle to clear violation status
 * @devapc_num: device apc index number (device apc 0 or 1)
 * @domain_num: domain index number (AP or MD domain)
 * no return value.
 */
#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
static int clear_vio_status(unsigned int module)
{
	return 0;
}


static irqreturn_t devapc_violation_irq(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}
#endif

static int devapc_probe(struct platform_device *dev)
{
#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
	int ret;
#endif
	pr_debug("[DEVAPC] module probe.\n");
	/*IO remap*/
	devapc_ioremap();
	/*
	 * Interrupts of vilation (including SPC in SMI, or EMI MPU)
	 * are triggered by the device APC.
	 * need to share the interrupt with the SPC driver.
	 */
#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
	pr_warn("[DEVAPC] Disable TEE...Request IRQ in REE.\n");

	ret = request_irq(devapc_irq, (irq_handler_t)devapc_violation_irq,
		IRQF_TRIGGER_LOW | IRQF_SHARED, "devapc", &g_devapc_ctrl);
	if (ret) {
		pr_err("[DEVAPC] Failed to request irq! (%d)\n", ret);
		return ret;
	}

	dapc_clk = devm_clk_get(&dev->dev, "main");
	if (IS_ERR(dapc_clk)) {
		pr_err("[DEVAPC] cannot get dapc clock.\n");
		return PTR_ERR(dapc_clk);
	}
	clk_prepare_enable(dapc_clk);

	start_devapc();
#else/*ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT*/
	dapc_clk = devm_clk_get(&dev->dev, "main");
	if (IS_ERR(dapc_clk)) {
		pr_err("[DEVAPC] cannot get dapc clock.\n");
		return PTR_ERR(dapc_clk);
	}
	clk_prepare_enable(dapc_clk);
	pr_debug("[DEVAPC] Enable TEE...Request IRQ in TEE.\n");
#endif

	return 0;
}


static int devapc_remove(struct platform_device *dev)
{
	return 0;
}

static int devapc_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int devapc_resume(struct platform_device *dev)
{
#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
	pr_debug("[DEVAPC] module resume.\n");
	start_devapc();
#else
	pr_debug("[DEVAPC] Enable TEE...module resume.\n");
#endif
	return 0;
}

#ifndef CONFIG_OF
struct platform_device devapc_device = {
	.name = "mt8163-devapc",
	.id = -1,
};
#endif

static const struct of_device_id mt_dapc_of_match[] = {
	{ .compatible = "mediatek,mt8163-devapc", },
	{/* sentinel */},
};

MODULE_DEVICE_TABLE(of, mt_dapc_of_match);

static struct platform_driver devapc_driver = {
	.probe = devapc_probe,
	.remove = devapc_remove,
	.suspend = devapc_suspend,
	.resume = devapc_resume,
	.driver = {
	.name = "mt8163-devapc",
	.owner = THIS_MODULE,
#ifdef CONFIG_OF
	.of_match_table = mt_dapc_of_match,
#endif
	},
};

/*
 * devapc_init: module init function.
 */
static int __init devapc_init(void)
{
	int ret;

	pr_debug("[DEVAPC] module init.\n");
#ifndef CONFIG_OF
	ret = platform_device_register(&devapc_device);
	if (ret) {
		pr_err("[DEVAPC] Unable to do device register(%d)\n", ret);
		return ret;
	}
#endif

	ret = platform_driver_register(&devapc_driver);
	if (ret) {
		pr_err("[DEVAPC] Unable to register driver (%d)\n", ret);
#ifndef CONFIG_OF
		platform_device_unregister(&devapc_device);
#endif
		return ret;
	}

	g_devapc_ctrl = cdev_alloc();
	if (!g_devapc_ctrl) {
		pr_err("[DEVAPC] Failed to add devapc device! (%d)\n", ret);
		platform_driver_unregister(&devapc_driver);
#ifndef CONFIG_OF
		platform_device_unregister(&devapc_device);
#endif
		return ret;
	}
	g_devapc_ctrl->owner = THIS_MODULE;

	return 0;
}

/*
 * devapc_exit: module exit function.
 */
static void __exit devapc_exit(void)
{
	pr_debug("[DEVAPC] DEVAPC module exit\n");
#ifdef CONFIG_MTK_HIBERNATION
	unregister_swsusp_restore_noirq_func(ID_M_DEVAPC);
#endif

}

late_initcall(devapc_init);
module_exit(devapc_exit);
MODULE_LICENSE("GPL");
