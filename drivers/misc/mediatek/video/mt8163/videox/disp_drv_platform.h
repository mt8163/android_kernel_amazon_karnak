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

#ifndef __DISP_DRV_PLATFORM_H__
#define __DISP_DRV_PLATFORM_H__

#include <linux/dma-mapping.h>
#include <linux/types.h>
/* #include <mt-plat/mt_gpio.h> */
#include "m4u.h"
/* #include <mach/mt6585_pwm.h> */
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif
/* #include <mach/boot.h> */

#include "disp_assert_layer.h"

#include "ddp_drv.h"
#include "ddp_hal.h"
#include "ddp_ovl.h"
#include "ddp_path.h"
#include "ddp_rdma.h"

#include <mt-plat/sync_write.h>
#define MTK_DISP_IDLE_LP
/* #define MTKFB_NO_M4U */
/* #define MTK_NO_DISP_IN_LK  // do not enable display in LK */
/* #define MTK_LCD_HW_3D_SUPPORT */
#define DISP_ENABLE_SODI
#define ALIGN_TO(x, n) (((x) + ((n)-1)) & ~((n)-1))
#define MTK_FB_START_DSI_ISR
#define MTK_FB_OVERLAY_SUPPORT
#define MTK_FB_SYNC_SUPPORT
#define MTK_FB_ION_SUPPORT
#define MTK_FB_ESD_ENABLE
#ifndef HW_OVERLAY_COUNT
#define HW_OVERLAY_COUNT (OVL_LAYER_NUM)
#endif
#define RESERVED_LAYER_COUNT (2)
#define VIDEO_LAYER_COUNT (HW_OVERLAY_COUNT - RESERVED_LAYER_COUNT)

#define MTK_FB_DFO_DISABLE
#define DFO_USE_NEW_API
/* #define FPGA_DEBUG_PAN */
#ifdef CONFIG_FPGA_EARLY_PORTING
#define MTK_FB_ALIGNMENT 16
#else
#define MTK_FB_ALIGNMENT 32
#endif

#define PRIMARY_DISPLAY_HW_OVERLAY_LAYER_NUMBER (4)
#define PRIMARY_DISPLAY_HW_OVERLAY_ENGINE_COUNT (2)
#ifdef OVL_CASCADE_SUPPORT
#define PRIMARY_DISPLAY_HW_OVERLAY_CASCADE_COUNT (2)
#else
#define PRIMARY_DISPLAY_HW_OVERLAY_CASCADE_COUNT (1)
#endif
#define PRIMARY_DISPLAY_SESSION_LAYER_COUNT                                    \
	(PRIMARY_DISPLAY_HW_OVERLAY_LAYER_NUMBER *                             \
	 PRIMARY_DISPLAY_HW_OVERLAY_CASCADE_COUNT)

#define EXTERNAL_DISPLAY_SESSION_LAYER_COUNT                                   \
	(PRIMARY_DISPLAY_HW_OVERLAY_LAYER_NUMBER *                             \
	 PRIMARY_DISPLAY_HW_OVERLAY_CASCADE_COUNT)

#define DISP_SESSION_OVL_TIMELINE_ID(x) (x)
/*
 * #define DISP_SESSION_OUTPUT_TIMELINE_ID
 * (PRIMARY_DISPLAY_SESSION_LAYER_COUNT)
 */
/*
 * #define DISP_SESSION_PRESENT_TIMELINE_ID
 * (PRIMARY_DISPLAY_SESSION_LAYER_COUNT+1)
 */
/* #define DISP_SESSION_TIMELINE_COUNT (DISP_SESSION_PRESENT_TIMELINE_ID+1) */

enum DISP_SESSION_ENUM {
	DISP_SESSION_OUTPUT_TIMELINE_ID = PRIMARY_DISPLAY_SESSION_LAYER_COUNT,
	DISP_SESSION_PRESENT_TIMELINE_ID,
	DISP_SESSION_OUTPUT_INTERFACE_TIMELINE_ID,
	DISP_SESSION_TIMELINE_COUNT,
};

#define MAX_SESSION_COUNT 5
/* #define MTK_FB_CMDQ_DISABLE */
#endif /* __DISP_DRV_PLATFORM_H__ */
