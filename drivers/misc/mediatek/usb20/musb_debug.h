/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MUSB_LINUX_DEBUG_H__
#define __MUSB_LINUX_DEBUG_H__

#define yprintk(facility, format, args...) \
		printk(facility "%s %d: " format, \
		__func__, __LINE__, ## args)

/* workaroud for redefine warning in usb_dump.c */
#ifdef WARNING
#undef WARNING
#endif

#ifdef INFO
#undef INFO
#endif

#ifdef ERR
#undef ERR
#endif

#define WARNING(fmt, args...) yprintk(KERN_WARNING, fmt, ## args)
#define INFO(fmt, args...) yprintk(KERN_INFO, fmt, ## args)
#define ERR(fmt, args...) yprintk(KERN_ERR, fmt, ## args)

#define xprintk(level,  format, args...) do { \
	if (_dbg_level(level)) { \
		if (musb_uart_debug) {\
			pr_notice("[MUSB]%s %d: " format, \
				__func__, __LINE__, ## args); \
		} \
		else{\
			pr_debug("[MUSB]%s %d: " format, \
				__func__, __LINE__, ## args); \
		} \
	} } while (0)

extern int musb_debug;
extern int musb_uart_debug;

static inline int _dbg_level(unsigned int level)
{
	return level <= musb_debug;
}

#ifdef DBG
#undef DBG
#endif
#define DBG(level, fmt, args...) xprintk(level, fmt, ## args)

/* extern const char *otg_state_string(struct musb *); */

extern int musb_init_debugfs(struct musb *musb);
extern void musb_exit_debugfs(struct musb *musb);

#endif				/*  __MUSB_LINUX_DEBUG_H__ */
