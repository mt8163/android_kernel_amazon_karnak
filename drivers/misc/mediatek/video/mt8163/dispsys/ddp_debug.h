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

#ifndef __DDP_DEBUG_H__
#define __DDP_DEBUG_H__

#include "ddp_dump.h"
#include "ddp_mmp.h"
#include "ddp_path.h"
#include <linux/kernel.h>

extern unsigned int gDDPError;
extern unsigned int g_mobilelog;
extern unsigned int force_sec;
extern int disp_create_session(struct disp_session_config *config);
extern int disp_destroy_session(struct disp_session_config *config);

extern struct OVL_CONFIG_STRUCT cached_layer_config[DDP_OVL_LAYER_MUN];

#define DISP_ENABLE_SODI_FOR_VIDEO_MODE
void ddp_debug_init(void);
void ddp_debug_exit(void);

unsigned int ddp_debug_analysis_to_buffer(void);
unsigned int ddp_debug_dbg_log_level(void);
unsigned int ddp_debug_irq_log_level(void);

int ddp_mem_test(void);
int ddp_lcd_test(void);

#endif /* __DDP_DEBUG_H__ */
