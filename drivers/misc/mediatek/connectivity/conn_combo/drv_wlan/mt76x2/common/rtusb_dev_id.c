/****************************************************************************
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 ****************************************************************************

    Module Name:
    rtusb_dev_id.c

    Abstract:

    Revision History:
    Who        When          What
    ---------  ----------    ----------------------------------------------
 */

#define RTMP_MODULE_OS

#include "rtmp_comm.h"
#include "rt_os_util.h"
#include "rt_os_net.h"


/* module table */
USB_DEVICE_ID rtusb_dev_id[] = {
#ifdef MT76x2
	{USB_DEVICE(0x0E8D, 0x7612), .driver_info = RLT_MAC_BASE}
	,
	{USB_DEVICE_AND_INTERFACE_INFO(0x0E8D, 0x7632, 0xff, 0xff, 0xff), .driver_info = RLT_MAC_BASE}
	,
	{USB_DEVICE_AND_INTERFACE_INFO(0x0E8D, 0x7662, 0xff, 0xff, 0xff), .driver_info = RLT_MAC_BASE}
	,
	{USB_DEVICE_AND_INTERFACE_INFO(0x0E8D, 0x7600, 0xff, 0xff, 0xff), .driver_info = RLT_MAC_BASE}
	,
	{USB_DEVICE_AND_INTERFACE_INFO(0x0471, 0x7632, 0xff, 0xff, 0xff), .driver_info = RLT_MAC_BASE}
	,			/* ALPHA */
#endif /* endif */
	{}			/* Terminating entry */
};

INT const rtusb_usb_id_len = sizeof(rtusb_dev_id) / sizeof(USB_DEVICE_ID);
MODULE_DEVICE_TABLE(usb, rtusb_dev_id);
