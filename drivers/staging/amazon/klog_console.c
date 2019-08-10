/*
 *
 * Copyright (C) 2014 Amazon Incorporated
 *
 * Yang Liu <yangliu@lab126.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>
#include <linux/console.h>
#include <linux/klog_console.h>

static void klog_console_write(struct console *con, const char *buf,
				unsigned count)
{
	if (count == 0)
		return;

	while (count) {
		unsigned int i;
		unsigned int lf;
		/* search for LF so we can insert CR if necessary */
		for (i = 0, lf = 0 ; i < count ; i++) {
			if (*(buf + i) == 10) {
				lf = 1;
				i++;
				break;
			}
		}

		logger_kmsg_write(buf, i);

		if (lf) {
			/* append CR after LF */
			unsigned char cr = 13;
			logger_kmsg_write(&cr, 1);
		}
		buf += i;
		count -= i;
	}

	return;
}

static struct console klog_console = {
	.name	= "klog",
	.write	= klog_console_write,
	.flags	= CON_PRINTBUFFER | CON_ENABLED | CON_ANYTIME | CON_RAMCONSOLE,
	.index	= -1,
};


static int __init klog_console_init(void)
{
	printk(KERN_ERR "Registering kernel log console\n");
	register_console(&klog_console);
	return 0;
}

device_initcall(klog_console_init);
