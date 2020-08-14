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

#ifndef __MTK_FB_CONSOLE_H__
#define __MTK_FB_CONSOLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MFC_CHECK_RET(expr)                                                    \
	do {                                                                   \
		MFC_STATUS ret = (expr);                                       \
		ASSERT(ret == MFC_STATUS_OK);                                  \
	} while (0)

enum MFC_STATUS {
	MFC_STATUS_OK = 0,

	MFC_STATUS_INVALID_ARGUMENT = -1,
	MFC_STATUS_NOT_IMPLEMENTED = -2,
	MFC_STATUS_OUT_OF_MEMORY = -3,
	MFC_STATUS_LOCK_FAIL = -4,
	MFC_STATUS_FATAL_ERROR = -5,
};

/* typedef void *MFC_HANDLE; */

/* ---------------------------------------------------------------------------
 */

struct MFC_CONTEXT {
	struct semaphore sem;

	uint8_t *fb_addr;
	uint32_t fb_width;
	uint32_t fb_height;
	uint32_t fb_bpp;
	uint32_t fg_color;
	uint32_t bg_color;
	uint32_t screen_color;
	uint32_t rows;
	uint32_t cols;
	uint32_t cursor_row;
	uint32_t cursor_col;
	uint32_t font_width;
	uint32_t font_height;
};

/* MTK Framebuffer Console API */

enum MFC_STATUS MFC_Open(void **handle, void *fb_addr, unsigned int fb_width,
			 unsigned int fb_height, unsigned int fb_bpp,
			 unsigned int fg_color, unsigned int bg_color);

enum MFC_STATUS MFC_Open_Ex(void **handle, void *fb_addr, unsigned int fb_width,
			    unsigned int fb_height, unsigned int fb_pitch,
			    unsigned int fb_bpp, unsigned int fg_color,
			    unsigned int bg_color);

enum MFC_STATUS MFC_Close(void *handle);

enum MFC_STATUS MFC_SetColor(void *handle, unsigned int fg_color,
			     unsigned int bg_color);

enum MFC_STATUS MFC_ResetCursor(void *handle);

enum MFC_STATUS MFC_Print(void *handle, const char *str);

enum MFC_STATUS MFC_LowMemory_Printf(void *handle, const char *str,
				     uint32_t fg_color, uint32_t bg_color);

enum MFC_STATUS MFC_SetMem(void *handle, const char *str, uint32_t color);
uint32_t MFC_Get_Cursor_Offset(void *handle);
#ifdef __cplusplus
}
#endif
#endif /* __MTK_FB_CONSOLE_H__ */
