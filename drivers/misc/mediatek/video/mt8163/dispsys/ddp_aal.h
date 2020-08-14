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

#ifndef __DDP_AAL_H__
#define __DDP_AAL_H__

#define AAL_HIST_BIN 33 /* [0..32] */
#define AAL_DRE_POINT_NUM 29

#define AAL_SERVICE_FORCE_UPDATE 0x1

struct DISP_AAL_INITREG {
	/* DRE */
	int dre_map_bypass;
	/* CABC */
	int cabc_gainlmt[33];
};

struct DISP_AAL_HIST {
	unsigned int serviceFlags;
	int backlight;
	int colorHist;
	unsigned int maxHist[AAL_HIST_BIN];
	int requestPartial;
};

enum DISP_AAL_REFRESH_LATENCY {
	AAL_REFRESH_17MS = 17,
	AAL_REFRESH_33MS = 33
};

struct DISP_AAL_PARAM {
	int DREGainFltStatus[AAL_DRE_POINT_NUM];
	int cabc_fltgain_force; /* 10-bit ; [0,1023] */
	int cabc_gainlmt[33];
	int FinalBacklight; /* 10-bit ; [0,1023] */
	int allowPartial;
	int refreshLatency;	/* DISP_AAL_REFRESH_LATENCY */
};

void disp_aal_on_end_of_frame(void);
int disp_aal_write_init_regs(void *cmdq);

extern int aal_dbg_en;

#endif
