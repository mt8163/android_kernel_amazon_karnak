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

#ifndef __H_MTK_DISP_MGR__
#define __H_MTK_DISP_MGR__

#include "disp_session.h"
#include <linux/fs.h>

enum ePREPARE_FENCE_TYPE {
	PREPARE_INPUT_FENCE,
	PREPARE_OUTPUT_FENCE,
	PREPARE_PRESENT_FENCE
};

extern unsigned int is_hwc_enabled;
extern int is_DAL_Enabled(void);

long mtk_disp_mgr_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
const char *_disp_format_spy(enum DISP_FORMAT format);

#endif
