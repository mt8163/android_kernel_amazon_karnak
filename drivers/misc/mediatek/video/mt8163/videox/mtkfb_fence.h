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

#ifndef __MTKFB_FENCE_H__
#define __MTKFB_FENCE_H__

#include "disp_drv_platform.h"
#include "disp_session.h"
#include "display_recorder.h"
#include <linux/list.h>
#include <linux/mutex.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_mem.h"
#include <tz_cross/tz_ddp.h>
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
#endif

#define MTK_FB_INVALID_ION_FD (-1)
#define MTK_FB_INVALID_FENCE_FD (-1)

#define MTK_FB_KERNEL_NO_ION_FD (-99)

struct fb_overlay_buffer_t {
	/* Input */
	int layer_id;
	unsigned int layer_en;
	int ion_fd;
	unsigned int cache_sync;
	/* Output */
	unsigned int index;
	int fence_fd;
};

enum BUFFER_STATE { create, insert, reg_configed, reg_updated, read_done };

struct mtkfb_fence_buf_info {
	struct list_head list;
	unsigned int idx;
	int fence;
	struct ion_handle *hnd;
	unsigned int mva;
	unsigned long va;
	unsigned int size;
	unsigned int mva_offset;
	enum BUFFER_STATE buf_state;
	unsigned int cache_sync;
	unsigned int set_input_ticket;
	unsigned int trigger_ticket;
	unsigned int release_ticket;
	unsigned int enable;
	unsigned long long ts_create;
	unsigned long long ts_period_keep;
	unsigned int seq;
#ifdef CONFIG_MTK_HDMI_3D_SUPPORT
	unsigned int layer_type;
#endif
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
unsigned int secure_handle;
unsigned int isScure;
#endif

};

struct mtkfb_fence_sync_info {
	unsigned int inited;
	struct mutex mutex_lock;
	unsigned int layer_id;
	unsigned int fence_idx;
	unsigned int timeline_idx;
	unsigned int inc;
	unsigned int cur_idx;
	struct sync_timeline *timeline;
	struct list_head buf_list;
};

/* use another struct to avoid fence dependency with ddp_ovl.h */
struct FENCE_LAYER_INFO {
	unsigned int layer;
	unsigned int layer_en;
	unsigned int fmt;
	unsigned long addr;
	unsigned long vaddr;
	unsigned int src_x;
	unsigned int src_y;
	unsigned int src_w;
	unsigned int src_h;
	unsigned int src_pitch;
	unsigned int dst_x;
	unsigned int dst_y;
	unsigned int dst_w;
	unsigned int dst_h; /* clip region */
	unsigned int keyEn;
	unsigned int key;
	unsigned int aen;
	unsigned char alpha;

	unsigned int isDirty;

	unsigned int buff_idx;
	unsigned int security;
};

struct disp_sync_info {
	unsigned int inited;
	struct mutex sync_lock;
	unsigned int layer_id;
	unsigned int fence_idx;
	unsigned int timeline_idx;
	unsigned int fence_fd;
	unsigned int inc;
	unsigned int cur_idx;
	struct sync_timeline *timeline;
	struct list_head buf_list;
	struct FENCE_LAYER_INFO cached_config;
};

struct disp_session_sync_info {
	unsigned int session_id;
	struct disp_sync_info session_layer_info[DISP_SESSION_TIMELINE_COUNT];
	struct dprec_logger_event event_prepare;
	struct dprec_logger_event event_setinput;
	struct dprec_logger_event event_setoutput;
	struct dprec_logger_event event_trigger;
	struct dprec_logger_event event_findidx;
	struct dprec_logger_event event_release;
	struct dprec_logger_event event_waitvsync;
	struct dprec_logger_event event_err;
};

extern char *disp_session_mode_spy(unsigned int session_id);
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
extern KREE_SESSION_HANDLE
primary_display_secure_memory_session_handle(void);
#endif
void mtkfb_init_fence(void);
unsigned int mtkfb_query_buf_mva(unsigned int session_id, unsigned int layer_id,
				 unsigned int idx);
unsigned int mtkfb_query_buf_va(unsigned int session_id, unsigned int layer_id,
				unsigned int idx);
unsigned int mtkfb_update_buf_ticket(unsigned int session_id,
				     unsigned int layer_id, unsigned int idx,
				     unsigned int ticket);
unsigned int mtkfb_query_idx_by_ticket(unsigned int session_id,
				       unsigned int layer_id,
				       unsigned int ticket);
unsigned int mtkfb_query_release_idx(unsigned int session_id,
				     unsigned int layer_id,
				     unsigned long phy_addr);
unsigned int mtkfb_query_frm_seq_by_addr(unsigned int session_id,
					 unsigned int layer_id,
					 unsigned long phy_addr);
bool mtkfb_update_buf_info(unsigned int session_id,
			unsigned int layer_id, unsigned int idx,
			unsigned int mva_offset, unsigned int seq
			#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
			, unsigned int secure_handle,
			unsigned int isSecure
			#endif
			);
struct mtkfb_fence_buf_info *
mtkfb_init_buf_info(struct mtkfb_fence_buf_info *buf);
void mtkfb_release_fence(unsigned int session_id, unsigned int layer_id,
			 int fence);
int mtkfb_find_fence_by_ticket(unsigned int session_id, int layer_id,
			       int ticket);
void mtkfb_update_fence_set_input_ticket(unsigned int session_id,
					 unsigned int layer_id, int fence,
					 unsigned int ticket, int enable);
void mtkfb_update_present_fence_ticket(unsigned int session_id, int fence,
				       unsigned int ticket);
void mtkfb_update_fence_trigger_ticket(unsigned int session_id,
				       unsigned int layer_id, int fence,
				       unsigned int ticket);
void mtkfb_release_present_fence(unsigned int session_id, int fence);
void mtkfb_release_layer_fence(unsigned int session_id, unsigned int layer_id);
int mtkfb_get_present_fence(struct disp_present_fence_info *buf,
			    unsigned int *fence_fd, unsigned int *idx);
int mtkfb_fence_clean_thread(void *data);
int mtkfb_fence_timeline_index(void);

struct mtkfb_fence_buf_info *
disp_sync_prepare_buf(struct disp_buffer_info *buf);
int disp_sync_init(void);
int disp_sync_get_cached_layer_info(unsigned int session_id,
				    unsigned int timeline_idx,
				    unsigned int *layer_en, unsigned long *addr,
				    unsigned int *fence_idx);
int disp_sync_put_cached_layer_info(unsigned int session_id,
				    unsigned int timeline_idx,
				    struct disp_input_config *src,
				    unsigned int mva);

int disp_sync_convert_input_to_fence_layer_info(struct disp_input_config *src,
						struct FENCE_LAYER_INFO *dst,
						unsigned int dst_mva);
int disp_sync_find_fence_idx_by_addr(unsigned int session_id,
				     unsigned int timeline_id,
				     unsigned long phy_addr);
unsigned int disp_sync_query_buf_info(unsigned int session_id,
				      unsigned int timeline_id,
				      unsigned int idx, unsigned int *mva,
				      unsigned int *size);
int disp_sync_get_debug_info(char *stringbuf, int buf_len);
int disp_sync_get_ovl_timeline_id(int layer_id);
int disp_sync_get_output_timeline_id(void);
int disp_sync_get_output_interface_timeline_id(void);
int disp_sync_get_present_timeline_id(void);
struct disp_session_sync_info *
disp_get_session_sync_info_for_debug(unsigned int session_id);

void mtkfb_release_session_fence(unsigned int session_id);
struct disp_sync_info *_get_sync_info(unsigned int session_id,
				      unsigned int timeline_id);
#ifdef __cplusplus
}
#endif
#endif
