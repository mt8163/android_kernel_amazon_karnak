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

#ifndef __DDP_GAMMA_H__
#define __DDP_GAMMA_H__

/* #include <asm/uaccess.h> */
#include <linux/uaccess.h>

enum disp_gamma_id_t { DISP_GAMMA0 = 0, DISP_GAMMA_TOTAL };

/* typedef unsigned int gamma_entry; */
#define GAMMA_ENTRY(r10, g10, b10) (((r10) << 20) | ((g10) << 10) | (b10))

#define DISP_GAMMA_LUT_SIZE 512

struct DISP_GAMMA_LUT_T {
	enum disp_gamma_id_t hw_id;
	unsigned int lut[DISP_GAMMA_LUT_SIZE];
};

enum disp_ccorr_id_t { DISP_CCORR0 = 0, DISP_CCORR_TOTAL };

struct DISP_CCORR_COEF_T {
	enum disp_ccorr_id_t hw_id;
	unsigned int coef[3][3];
};

#endif
