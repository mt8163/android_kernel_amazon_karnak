/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/


#ifndef _MTK_MFG_H_
#define _MTK_MFG_H_

void mfg_dump_regs(const char * prefix);

void mtk_mfg_enable_gpu(void);
void mtk_mfg_disable_gpu(void);

void mtk_mfg_wait_gpu_idle(void);

unsigned long mtk_mfg_get_frequence(void);

bool mtk_mfg_is_ready(void);

#endif
