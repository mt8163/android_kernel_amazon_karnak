/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __MD32_HELPER_H__
#define __MD32_HELPER_H__

extern unsigned int flag_md32_addr;
extern unsigned int flag_apmcu_addr;
extern unsigned int apmcu_clk_init_count;

enum SEMAPHORE_FLAG {
	SEMAPHORE_CLK_CFG_5 = 0,
	SEMAPHORE_PTP,
	SEMAPHORE_I2C0,
	SEMAPHORE_I2C1,
	SEMAPHORE_TOUCH,
	SEMAPHORE_APDMA,
	SEMAPHORE_SENSOR,
	NR_FLAG,
};

void write_md32_cfgreg(u32 val, u32 offset);
u32 read_md32_cfgreg(u32 offset);
int get_md32_semaphore(int flag);
int release_md32_semaphore(int flag);
unsigned int is_md32_enable(void);
void apdma_spin_lock_irqsave(unsigned long *flags);
void apdma_spin_unlock_irqrestore(unsigned long *flags);
#endif
