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

#ifndef _DDP_RDMA_API_H_
#define _DDP_RDMA_API_H_
#include "ddp_info.h"

#define RDMA_INSTANCES 2
#define RDMA_MAX_WIDTH 4095
#define RDMA_MAX_HEIGHT 4095

extern unsigned int gRDMAUltraSetting;
extern unsigned long long rdma_start_time[2];
extern unsigned long long rdma_end_time[2];

extern unsigned int rdma_start_irq_cnt[2];
extern unsigned int rdma_done_irq_cnt[2];
extern unsigned int rdma_underflow_irq_cnt[2];
extern unsigned int rdma_targetline_irq_cnt[2];
extern unsigned int gUltraEnable;

enum RDMA_OUTPUT_FORMAT {
	RDMA_OUTPUT_FORMAT_ARGB = 0,
	RDMA_OUTPUT_FORMAT_YUV444 = 1,
};

enum RDMA_MODE {
	RDMA_MODE_DIRECT_LINK = 0,
	RDMA_MODE_MEMORY = 1,
};

/* start module */
int rdma_start(enum DISP_MODULE_ENUM module, void *handle);

/* stop module */
int rdma_stop(enum DISP_MODULE_ENUM module, void *handle);

/* reset module */
int rdma_reset(enum DISP_MODULE_ENUM module, void *handle);

/* configu module */
int rdma_config(enum DISP_MODULE_ENUM module, enum RDMA_MODE mode,
		unsigned long address, enum DP_COLOR_ENUM inFormat,
		unsigned int pitch, unsigned int width, unsigned int height,
		unsigned int ufoe_enable, enum DISP_BUFFER_TYPE sec,
		void *handle); /* ourput setting */

void rdma_set_target_line(enum DISP_MODULE_ENUM module, unsigned int line,
			  void *handle);

void rdma_get_address(enum DISP_MODULE_ENUM module, unsigned long *data);
void rdma_dump_reg(enum DISP_MODULE_ENUM module);
void rdma_dump_analysis(enum DISP_MODULE_ENUM module);
void rdma_get_info(int idx, struct RDMA_BASIC_STRUCT *info);

#endif
