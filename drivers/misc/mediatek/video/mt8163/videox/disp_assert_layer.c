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

#include "disp_assert_layer.h"
#include "ddp_hal.h"
#include "ddp_mmp.h"
#include "disp_drv_log.h"
#include "disp_session.h"
#include "primary_display.h"
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/types.h>
/* /common part */
#define DAL_BPP (2)
#define DAL_WIDTH (DISP_GetScreenWidth())
#define DAL_HEIGHT (DISP_GetScreenHeight())

/* #ifdef CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER */

#include <asm/cacheflush.h>
#include <linux/module.h>
#include <linux/semaphore.h>
#include <linux/string.h>

#include "disp_drv_platform.h"
#include "mtkfb_console.h"

/*
 * --------------------------------------------------------------------------
 */
#define DAL_FORMAT (DISP_FORMAT_RGB565)
#define DAL_BG_COLOR (dal_bg_color)
#define DAL_FG_COLOR (dal_fg_color)

#define RGB888_To_RGB565(x)                                                    \
	((((x)&0xF80000) >> 8) | (((x)&0x00FC00) >> 5) | (((x)&0x0000F8) >> 3))

#define MAKE_TWO_RGB565_COLOR(high, low) (((low) << 16) | (high))

#define DAL_LOCK()                                                             \
	do {                                                                   \
		if (down_interruptible(&dal_sem)) {                            \
			pr_warn("DISP/DAL "                                    \
				"Can't get semaphore in %s()\n",               \
				__func__);                                     \
		}                                                              \
	} while (0)

#define DAL_UNLOCK() up(&dal_sem)

#define DAL_CHECK_MFC_RET(expr)                                                \
	do {                                                                   \
		enum MFC_STATUS ret = (expr);                                  \
		if (ret != MFC_STATUS_OK) {                                    \
			pr_warn("DISP/DAL "                                    \
				"Warning: call MFC_XXX function failed "       \
				"in %s(), line: %d, ret: %x\n",                \
				__func__, __LINE__, ret);                      \
		}                                                              \
	} while (0)

#define DAL_CHECK_DISP_RET(expr)                                               \
	do {                                                                   \
		enum DISP_STATUS ret = (expr);                                 \
		if (ret != DISP_STATUS_OK) {                                   \
			pr_warn("DISP/DAL "                                    \
				"Warning: call DISP_XXX function failed "      \
				"in %s(), line: %d, ret: %x\n",                \
				__func__, __LINE__, ret);                      \
		}                                                              \
	} while (0)

#define DAL_LOG(fmt, arg...) pr_info("DISP/DAL " fmt, ##arg)

/*
 * --------------------------------------------------------------------------
 */

static void *mfc_handle;

static void *dal_fb_addr;
static unsigned long dal_fb_pa;
unsigned int isAEEEnabled;

/* static bool  dal_shown   = false; */
bool dal_shown;
static bool dal_disable_when_resume;
static unsigned int dal_fg_color = RGB888_To_RGB565(DAL_COLOR_WHITE);
static unsigned int dal_bg_color = RGB888_To_RGB565(DAL_COLOR_RED);

/* DECLARE_MUTEX(dal_sem); */
DEFINE_SEMAPHORE(dal_sem);

static char dal_print_buffer[1024];
/*
 * ---------------------------------------------------------------------------
 */

uint32_t DAL_GetLayerSize(void)
{
	/* xuecheng, avoid lcdc read buffersize+1 issue */
	return DAL_WIDTH * DAL_HEIGHT * DAL_BPP + 4096;
}

#if 0
static enum DAL_STATUS DAL_SetRedScreen(uint32_t *addr)
{
	uint32_t i;
	const uint32_t BG_COLOR =
		MAKE_TWO_RGB565_COLOR(DAL_BG_COLOR, DAL_BG_COLOR);

	for (i = 0; i < DAL_GetLayerSize() / sizeof(uint32_t); ++i)
		*addr++ = BG_COLOR;
	return DAL_STATUS_OK;
}
#endif
enum DAL_STATUS DAL_SetScreenColor(enum DAL_COLOR color)
{
#if 1
	uint32_t i;
	uint32_t size;
	uint32_t BG_COLOR;
	struct MFC_CONTEXT *ctxt = NULL;
	uint32_t offset;
	unsigned int *addr;

	color = RGB888_To_RGB565(color);
	BG_COLOR = MAKE_TWO_RGB565_COLOR(color, color);

	ctxt = (struct MFC_CONTEXT *)mfc_handle;
	if (!ctxt)
		return DAL_STATUS_FATAL_ERROR;
	if (ctxt->screen_color == color)
		return DAL_STATUS_OK;
	offset = MFC_Get_Cursor_Offset(mfc_handle);
	addr = (unsigned int *)(ctxt->fb_addr + offset);

	size = DAL_GetLayerSize() - offset;
	for (i = 0; i < size / sizeof(uint32_t); ++i)
		*addr++ = BG_COLOR;

	ctxt->screen_color = color;

	return DAL_STATUS_OK;
}
EXPORT_SYMBOL(DAL_SetScreenColor);

enum DAL_STATUS DAL_Init(unsigned long layerVA, unsigned long layerPA)
{
	pr_debug("%s, layerVA=0x%lx, layerPA=0x%lx\n", __func__, layerVA,
		 layerPA);

	dal_fb_addr = (void *)layerVA;
	dal_fb_pa = layerPA;
	DAL_CHECK_MFC_RET(MFC_Open(&mfc_handle, dal_fb_addr, DAL_WIDTH,
				   DAL_HEIGHT, DAL_BPP, DAL_FG_COLOR,
				   DAL_BG_COLOR));

	/* DAL_Clean(); */
	DAL_SetScreenColor(DAL_COLOR_RED);

	return DAL_STATUS_OK;
}

enum DAL_STATUS DAL_SetColor(unsigned int fgColor, unsigned int bgColor)
{
	if (mfc_handle == NULL)
		return DAL_STATUS_NOT_READY;

	DAL_LOCK();
	dal_fg_color = RGB888_To_RGB565(fgColor);
	dal_bg_color = RGB888_To_RGB565(bgColor);
	DAL_CHECK_MFC_RET(MFC_SetColor(mfc_handle, dal_fg_color, dal_bg_color));
	DAL_UNLOCK();

	return DAL_STATUS_OK;
}
EXPORT_SYMBOL(DAL_SetColor);

enum DAL_STATUS DAL_Dynamic_Change_FB_Layer(unsigned int isAEEEnabled)
{
#if 0
	static int ui_layer_tdshp;

	pr_debug(
		"[DDP] DAL_Dynamic_Change_FB_Layer, isAEEEnabled=%d\n",
		isAEEEnabled);

	if (DISP_DEFAULT_UI_LAYER_ID == DISP_CHANGED_UI_LAYER_ID) {
		pr_debug(
			"[DDP] DAL_Dynamic_Change_FB_Layer, no dynamic switch\n"
			);
		return DAL_STATUS_OK;
	}

	if (isAEEEnabled == 1) {
		/*
		 * change ui layer from DISP_DEFAULT_UI_LAYER_ID
		 * to DISP_CHANGED_UI_LAYER_ID
		 */
		memcpy((void *)(&cached_layer_config[DISP_CHANGED_UI_LAYER_ID]),
		       (void *)(&cached_layer_config[DISP_DEFAULT_UI_LAYER_ID]),
		       sizeof(OVL_CONFIG_STRUCT));
		ui_layer_tdshp =
			cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].isTdshp;
		cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].isTdshp = 0;
		disp_path_change_tdshp_status(DISP_DEFAULT_UI_LAYER_ID, 0);
		/*
		 * change global variable value, else error-check will
		 * find layer 2, 3 enable tdshp together
		 */
		FB_LAYER = DISP_CHANGED_UI_LAYER_ID;
	} else {
		memcpy((void *)(&cached_layer_config[DISP_DEFAULT_UI_LAYER_ID]),
		       (void *)(&cached_layer_config[DISP_CHANGED_UI_LAYER_ID]),
		       sizeof(OVL_CONFIG_STRUCT));
		cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].isTdshp =
			ui_layer_tdshp;
		FB_LAYER = DISP_DEFAULT_UI_LAYER_ID;
		memset((void *)(&cached_layer_config[
					DISP_CHANGED_UI_LAYER_ID]), 0,
		       sizeof(OVL_CONFIG_STRUCT));
	}

	/* no matter memcpy or memset, layer ID should not be changed */
	cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].layer =
		DISP_DEFAULT_UI_LAYER_ID;
	cached_layer_config[DISP_CHANGED_UI_LAYER_ID].layer =
		DISP_CHANGED_UI_LAYER_ID;
	cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].isDirty = 1;
	cached_layer_config[DISP_CHANGED_UI_LAYER_ID].isDirty = 1;
#endif
	return DAL_STATUS_OK;
}

static struct disp_session_input_config session_input;
enum DAL_STATUS DAL_Clean(void)
{
	enum DAL_STATUS ret = DAL_STATUS_OK;

	static int dal_clean_cnt;
	struct MFC_CONTEXT *ctxt = (struct MFC_CONTEXT *)mfc_handle;

	pr_debug("[MTKFB_DAL] DAL_Clean\n");
	if (mfc_handle == NULL)
		return DAL_STATUS_NOT_READY;

	/* if (LCD_STATE_POWER_OFF == LCD_GetState()) */
	/* return DAL_STATUS_LCD_IN_SUSPEND; */
	mmprofile_log_ex(ddp_mmp_get_events()->dal_clean, MMPROFILE_FLAG_START,
			 0, 0);

	DAL_LOCK();

	DAL_CHECK_MFC_RET(MFC_ResetCursor(mfc_handle));

	ctxt->screen_color = 0;
	DAL_SetScreenColor(DAL_COLOR_RED);

/*
 * if (LCD_STATE_POWER_OFF == LCD_GetState()) {
 * pr_info("DISP/DAL " "dal_clean in power off\n");
 * dal_disable_when_resume = true;
 * ret = DAL_STATUS_LCD_IN_SUSPEND;
 * goto End;
 * }
 */
/* xuecheng, for debug */
#if 0
	if (is_early_suspended) {
		up(&sem_early_suspend);
		pr_info("DISP/DAL dal_clean in power off\n");
		goto End;
	}
#endif

	/*
	 * TODO: if dal_shown=false, and 3D enabled, mtkfb may disable UI layer,
	 * please modify 3D driver
	 */
	if (isAEEEnabled == 1) {
		struct disp_input_config *input;

		memset((void *)&session_input, 0, sizeof(session_input));

		session_input.setter = SESSION_USER_AEE;
		session_input.config_layer_num = 1;
		input = &session_input.config[0];

		input->src_phy_addr = (void *)dal_fb_pa;
		input->layer_id = primary_display_get_option("ASSERT_LAYER");
		input->layer_enable = 0;
		input->src_offset_x = 0;
		input->src_offset_y = 0;
		input->src_width = DAL_WIDTH;
		input->src_height = DAL_HEIGHT;
		input->tgt_offset_x = 0;
		input->tgt_offset_y = 0;
		input->tgt_width = DAL_WIDTH;
		input->tgt_height = DAL_HEIGHT;
		input->alpha = 0x80;
		input->alpha_enable = 1;
		input->next_buff_idx = -1;
		input->src_pitch = DAL_WIDTH;
		input->src_fmt = DAL_FORMAT;
		input->next_buff_idx = -1;

		ret = primary_display_config_input_multiple(&session_input);

		/* DAL disable, switch UI layer to default layer 3 */
		pr_debug("[DDP]* isAEEEnabled from 1 to 0, %d\n",
			 dal_clean_cnt++);
		isAEEEnabled = 0;
		/* restore UI layer to DEFAULT_UI_LAYER */
		DAL_Dynamic_Change_FB_Layer(isAEEEnabled);
	}

	dal_shown = false;
	dal_disable_when_resume = false;

	primary_display_trigger(0, NULL, 0);

	DAL_UNLOCK();
	mmprofile_log_ex(ddp_mmp_get_events()->dal_clean, MMPROFILE_FLAG_END, 0,
			 0);
	return ret;
}
EXPORT_SYMBOL(DAL_Clean);

int is_DAL_Enabled(void)
{
	int ret = 0;

	DAL_LOCK();
	ret = isAEEEnabled;
	DAL_UNLOCK();

	return ret;
}

enum DAL_STATUS DAL_Printf(const char *fmt, ...)
{
	va_list args;
	uint i;
	enum DAL_STATUS ret = DAL_STATUS_OK;
	struct disp_input_config *input;

	/*
	 * pr_debug("[MTKFB_DAL] DAL_Printf mfc_handle=0x%08X, fmt=0x%08X\n",
	 * mfc_handle, fmt);
	 */

	DISPFUNC();

	if (mfc_handle == NULL)
		return DAL_STATUS_NOT_READY;

	if (fmt == NULL)
		return DAL_STATUS_INVALID_ARGUMENT;

	mmprofile_log_ex(ddp_mmp_get_events()->dal_printf, MMPROFILE_FLAG_START,
			 0, 0);
	DAL_LOCK();
	if (isAEEEnabled == 0) {
		pr_debug(
			"[DDP] isAEEEnabled from 0 to 1, ASSERT_LAYER=%d, dal_fb_pa %lx\n",
			primary_display_get_option("ASSERT_LAYER"), dal_fb_pa);

		isAEEEnabled = 1;
		/* default_ui_ layer coniig to changed_ui_layer */
		DAL_Dynamic_Change_FB_Layer(
			isAEEEnabled);

		DAL_CHECK_MFC_RET(MFC_Open(&mfc_handle, dal_fb_addr, DAL_WIDTH,
					   DAL_HEIGHT, DAL_BPP, DAL_FG_COLOR,
					   DAL_BG_COLOR));

		/* DAL_Clean(); */
		memset((void *)&session_input, 0, sizeof(session_input));

		session_input.setter = SESSION_USER_AEE;
		session_input.config_layer_num = 1;
		input = &session_input.config[0];

		input->src_phy_addr = (void *)dal_fb_pa;
		input->layer_id = primary_display_get_option("ASSERT_LAYER");
		input->layer_enable = 1;
		input->src_offset_x = 0;
		input->src_offset_y = 0;
		input->src_width = DAL_WIDTH;
		input->src_height = DAL_HEIGHT;
		input->tgt_offset_x = 0;
		input->tgt_offset_y = 0;
		input->tgt_width = DAL_WIDTH;
		input->tgt_height = DAL_HEIGHT;
		input->alpha = 0x80;
		input->alpha_enable = 1;
		input->next_buff_idx = -1;
		input->src_pitch = DAL_WIDTH;
		input->src_fmt = DAL_FORMAT;
		input->next_buff_idx = -1;

		ret = primary_display_config_input_multiple(&session_input);
	}

	va_start(args, fmt);
	i = vsprintf(dal_print_buffer, fmt, args);
	WARN_ON(i >= ARRAY_SIZE(dal_print_buffer));
	va_end(args);
	DAL_CHECK_MFC_RET(MFC_Print(mfc_handle, dal_print_buffer));

/* flush_cache_all(); kernel 4.9*/

#if 0
	if (is_early_suspended) {
		up(&sem_early_suspend);
		pr_info("DISP/DAL DAL_Printf in power off\n");
		goto End;
	}
#endif

	if (!dal_shown)
		dal_shown = true;
	ret = primary_display_trigger(0, NULL, 0);

	DAL_UNLOCK();

	mmprofile_log_ex(ddp_mmp_get_events()->dal_printf, MMPROFILE_FLAG_END,
			 0, 0);

	return ret;
}
EXPORT_SYMBOL(DAL_Printf);

enum DAL_STATUS DAL_OnDispPowerOn(void)
{
#if 0
	DAL_LOCK();

	/* Re-enable assertion layer when display resumes */

	if (is_early_suspended) {
		if (dal_enable_when_resume) {
			dal_enable_when_resume = false;
			if (!dal_shown) {
				mutex_lock(&OverlaySettingMutex);
				cached_layer_config[ASSERT_LAYER].src_x = 0;
				cached_layer_config[ASSERT_LAYER].src_y = 0;
				cached_layer_config[ASSERT_LAYER].src_w =
					DAL_WIDTH;
				cached_layer_config[ASSERT_LAYER].src_h =
					DAL_HEIGHT;
				cached_layer_config[ASSERT_LAYER].dst_x = 0;
				cached_layer_config[ASSERT_LAYER].dst_y = 0;
				cached_layer_config[ASSERT_LAYER].dst_w =
					DAL_WIDTH;
				cached_layer_config[ASSERT_LAYER].dst_h =
					DAL_HEIGHT;
				cached_layer_config[ASSERT_LAYER].layer_en =
					true;
				cached_layer_config[ASSERT_LAYER].isDirty =
					true;
				atomic_set(&OverlaySettingDirtyFlag, 1);
				atomic_set(&OverlaySettingApplied, 0);
				mutex_unlock(&OverlaySettingMutex);
				dal_shown = true;
			}
			goto End;
		} else if (dal_disable_when_resume) {
			dal_disable_when_resume = false;
			mutex_lock(&OverlaySettingMutex);
			cached_layer_config[ASSERT_LAYER].layer_en = false;
			cached_layer_config[ASSERT_LAYER].isDirty = true;
			atomic_set(&OverlaySettingDirtyFlag, 1);
			atomic_set(&OverlaySettingApplied, 0);
			mutex_unlock(&OverlaySettingMutex);
			dal_shown = false;
			goto End;

		}
	}

End:
	DAL_UNLOCK();
#endif
	return DAL_STATUS_OK;
}

/* ########################################################################## */
/* !CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER */
/* ########################################################################## */
#else
	unsigned int isAEEEnabled = 0;

	uint32_t DAL_GetLayerSize(void)
	{
		/* xuecheng, avoid lcdc read buffersize+1 issue */
		return DAL_WIDTH * DAL_HEIGHT * DAL_BPP + 4096;
	}

	enum DAL_STATUS DAL_Init(uint32_t layerVA, uint32_t layerPA)
	{
		NOT_REFERENCED(layerVA);
		NOT_REFERENCED(layerPA);

		return DAL_STATUS_OK;
	}
	enum DAL_STATUS DAL_SetColor(unsigned int fgColor, unsigned int bgColor)
	{
		NOT_REFERENCED(fgColor);
		NOT_REFERENCED(bgColor);

		return DAL_STATUS_OK;
	}
	enum DAL_STATUS DAL_Clean(void)
	{
		pr_debug("[MTKFB_DAL] DAL_Clean is not implemented\n");
		return DAL_STATUS_OK;
	}
	enum DAL_STATUS DAL_Printf(const char *fmt, ...)
	{
		NOT_REFERENCED(fmt);
		pr_debug("[MTKFB_DAL] DAL_Printf is not implemented\n");
		return DAL_STATUS_OK;
	}
	enum DAL_STATUS DAL_OnDispPowerOn(void) { return DAL_STATUS_OK; }
	enum DAL_STATUS DAL_SetScreenColor(enum DAL_COLOR color)
	{
		return DAL_STATUS_OK;
	}
#endif /* CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER */

#ifdef DAL_LOWMEMORY_ASSERT
/*EXPORT_SYMBOL(DAL_LowMemoryOn);*/
/*EXPORT_SYMBOL(DAL_LowMemoryOff);*/
#endif
