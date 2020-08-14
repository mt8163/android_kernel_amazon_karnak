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

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>

#include "m4u.h"
#include <linux/types.h>

#include "ddp_dpi.h"
#include "ddp_ovl.h"
#include "ddp_path.h"
#include "ddp_reg.h"
#include "debug.h"
#include "disp_drv_log.h"
#include "display_recorder.h"
#include "lcm_drv.h"
#include "mtkfb.h"
#include "primary_display.h"
/* #include <mt-plat/mt_gpio.h> */
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif
#include "ddp_manager.h"
#include "mtkfb_fence.h"

struct MTKFB_MMP_Events_t MTKFB_MMP_Events;

unsigned int g_mobilelog;

#define MTKFB_DEBUG_FS_CAPTURE_LAYER_CONTENT_SUPPORT

#ifdef MTKFB_DEBUG_FS_CAPTURE_LAYER_CONTENT_SUPPORT
struct dentry *mtkfb_layer_dbgfs[DDP_OVL_LAYER_MUN];

struct MTKFB_LAYER_DBG_OPTIONS {
	uint32_t layer_index;
	unsigned long working_buf;
	uint32_t working_size;
};

struct MTKFB_LAYER_DBG_OPTIONS mtkfb_layer_dbg_opt[DDP_OVL_LAYER_MUN];

#endif

/* ---------------------------------------------------------------------------
 */
/* Debug Options */
/* ---------------------------------------------------------------------------
 */

static const long int DEFAULT_LOG_FPS_WND_SIZE = 30;

struct DBG_OPTIONS {
	unsigned int en_fps_log;
	unsigned int en_touch_latency_log;
	unsigned int log_fps_wnd_size;
	unsigned int force_dis_layers;
};

static struct DBG_OPTIONS dbg_opt = {0};

static bool enable_ovl1_to_mem = true;

#if 0
static char STR_HELP[] =
	"\n"
	"USAGE\n"
	"        echo [ACTION]... > /d/mtkfb\n"
	"\n"
	"ACTION\n"
	"        mtkfblog:[on|off]\n"
	"             enable/disable [MTKFB] log\n"
	"\n"
	"        displog:[on|off]\n"
	"             enable/disable [DISP] log\n"
	"\n"
	"        mtkfb_vsynclog:[on|off]\n"
	"             enable/disable [VSYNC] log\n"
	"\n"
	"        log:[on|off]\n"
	"             enable/disable above all log\n"
	"\n"
	"        fps:[on|off]\n"
	"             enable fps and lcd update time log\n"
	"\n"
	"        tl:[on|off]\n"
	"             enable touch latency log\n"
	"\n"
	"        layer\n"
	"             dump lcd layer information\n"
	"\n"
	"        suspend\n"
	"             enter suspend mode\n"
	"\n"
	"        resume\n"
	"             leave suspend mode\n"
	"\n"
	"        lcm:[on|off|init]\n"
	"             power on/off lcm\n"
	"\n"
	"        cabc:[ui|mov|still]\n"
	"             cabc mode, UI/Moving picture/Still picture\n"
	"\n"
	"        lcd:[on|off]\n"
	"             power on/off display engine\n"
	"\n"
	"        te:[on|off]\n"
	"             turn on/off tearing-free control\n"
	"\n"
	"        tv:[on|off]\n"
	"             turn on/off tv-out\n"
	"\n"
	"        tvsys:[ntsc|pal]\n"
	"             switch tv system\n"
	"\n"
	"        reg:[lcd|dpi|dsi|tvc|tve]\n"
	"             dump hw register values\n"
	"\n"
	"       cpfbonly:[on|off]\n"
	"             capture UI layer only on/off\n"
	"\n"
	"       esd:[on|off]\n"
	"             esd kthread on/off\n"
	"       HQA:[NormalToFactory|FactoryToNormal]\n"
	"             for HQA requirement\n"
	"\n"
	"       cmd:[value]\n"
	"             set cmd\n"
	"\n"
	"       mmp\n"
	"             Register MMProfile events\n"
	"\n"
	"       dump_fb:[on|off[,down_sample_x[,down_sample_y,[delay]]]]\n"
	"             Start/end to capture framebuffer every delay(ms)\n"
	"\n"
	"       dump_ovl:[on|off[,down_sample_x[,down_sample_y]]]\n"
	"             Start to capture OVL only once\n"
	"\n"
	"dump_layer:[on|off[,down_sample_x[,down_sample_y]][,layer(0:L0,1:L1,2:L2,3:L3,4:L0-3)]\n"
	"             Start/end to capture current enabled OVL layer every frame\n"
	;
#endif

/* ---------------------------------------------------------------------------
 */
/* Information Dump Routines */
/* ---------------------------------------------------------------------------
 */

void init_mtkfb_mmp_events(void)
{
	if (MTKFB_MMP_Events.MTKFB == 0) {
		MTKFB_MMP_Events.MTKFB =
			mmprofile_register_event(MMP_ROOT_EVENT, "MTKFB");
		MTKFB_MMP_Events.PanDisplay = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "PanDisplay");
		MTKFB_MMP_Events.CreateSyncTimeline = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "CreateSyncTimeline");
		MTKFB_MMP_Events.SetOverlayLayer = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "SetOverlayLayer");
		MTKFB_MMP_Events.SetOverlayLayers = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "SetOverlayLayers");
		MTKFB_MMP_Events.SetMultipleLayers = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "SetMultipleLayers");
		MTKFB_MMP_Events.CreateSyncFence = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "CreateSyncFence");
		MTKFB_MMP_Events.IncSyncTimeline = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "IncSyncTimeline");
		MTKFB_MMP_Events.SignalSyncFence = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "SignalSyncFence");
		MTKFB_MMP_Events.TrigOverlayOut = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "TrigOverlayOut");
		MTKFB_MMP_Events.UpdateScreenImpl = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "UpdateScreenImpl");
		MTKFB_MMP_Events.VSync = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "VSync");
		MTKFB_MMP_Events.UpdateConfig = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "UpdateConfig");
		MTKFB_MMP_Events.EsdCheck = mmprofile_register_event(
			MTKFB_MMP_Events.UpdateConfig, "EsdCheck");
		MTKFB_MMP_Events.ConfigOVL = mmprofile_register_event(
			MTKFB_MMP_Events.UpdateConfig, "ConfigOVL");
		MTKFB_MMP_Events.ConfigAAL = mmprofile_register_event(
			MTKFB_MMP_Events.UpdateConfig, "ConfigAAL");
		MTKFB_MMP_Events.ConfigMemOut = mmprofile_register_event(
			MTKFB_MMP_Events.UpdateConfig, "ConfigMemOut");
		MTKFB_MMP_Events.ScreenUpdate = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "ScreenUpdate");
		MTKFB_MMP_Events.CaptureFramebuffer = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "CaptureFB");
		MTKFB_MMP_Events.RegUpdate = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "RegUpdate");
		MTKFB_MMP_Events.EarlySuspend = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "EarlySuspend");
		MTKFB_MMP_Events.DispDone = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "DispDone");
		MTKFB_MMP_Events.DSICmd = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "DSICmd");
		MTKFB_MMP_Events.DSIIRQ = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "DSIIrq");
		MTKFB_MMP_Events.WaitVSync = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "WaitVSync");
		MTKFB_MMP_Events.LayerDump = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "LayerDump");
		MTKFB_MMP_Events.Layer[0] = mmprofile_register_event(
			MTKFB_MMP_Events.LayerDump, "Layer0");
		MTKFB_MMP_Events.Layer[1] = mmprofile_register_event(
			MTKFB_MMP_Events.LayerDump, "Layer1");
		MTKFB_MMP_Events.Layer[2] = mmprofile_register_event(
			MTKFB_MMP_Events.LayerDump, "Layer2");
		MTKFB_MMP_Events.Layer[3] = mmprofile_register_event(
			MTKFB_MMP_Events.LayerDump, "Layer3");
		MTKFB_MMP_Events.OvlDump = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "OvlDump");
		MTKFB_MMP_Events.FBDump = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "FBDump");
		MTKFB_MMP_Events.DSIRead = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "DSIRead");
		MTKFB_MMP_Events.GetLayerInfo = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "GetLayerInfo");
		MTKFB_MMP_Events.LayerInfo[0] = mmprofile_register_event(
			MTKFB_MMP_Events.GetLayerInfo, "LayerInfo0");
		MTKFB_MMP_Events.LayerInfo[1] = mmprofile_register_event(
			MTKFB_MMP_Events.GetLayerInfo, "LayerInfo1");
		MTKFB_MMP_Events.LayerInfo[2] = mmprofile_register_event(
			MTKFB_MMP_Events.GetLayerInfo, "LayerInfo2");
		MTKFB_MMP_Events.LayerInfo[3] = mmprofile_register_event(
			MTKFB_MMP_Events.GetLayerInfo, "LayerInfo3");
		MTKFB_MMP_Events.IOCtrl = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "IOCtrl");
		MTKFB_MMP_Events.Debug = mmprofile_register_event(
			MTKFB_MMP_Events.MTKFB, "Debug");
		mmprofile_enable_event_recursive(MTKFB_MMP_Events.MTKFB, 1);
	}
}

static inline int is_layer_enable(unsigned int roi_ctl, unsigned int layer)
{
	return (roi_ctl >> (31 - layer)) & 0x1;
}

/* ---------------------------------------------------------------------------
 */
/* FPS Log */
/* ---------------------------------------------------------------------------
 */

struct FPS_LOGGER {
	long int current_lcd_time_us;
	long int current_te_delay_time_us;
	long int total_lcd_time_us;
	long int total_te_delay_time_us;
	long int start_time_us;
	long int trigger_lcd_time_us;
	unsigned int trigger_lcd_count;

	long int current_hdmi_time_us;
	long int total_hdmi_time_us;
	long int hdmi_start_time_us;
	long int trigger_hdmi_time_us;
	unsigned int trigger_hdmi_count;
};

static struct FPS_LOGGER fps = {0};
static struct FPS_LOGGER hdmi_fps = {0};

static long int get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}

static inline void reset_fps_logger(void)
{
	memset(&fps, 0, sizeof(fps));
}

static inline void reset_hdmi_fps_logger(void)
{
	memset(&hdmi_fps, 0, sizeof(hdmi_fps));
}

void DBG_OnTriggerLcd(void)
{
	if (!dbg_opt.en_fps_log && !dbg_opt.en_touch_latency_log)
		return;

	fps.trigger_lcd_time_us = get_current_time_us();
	if (fps.trigger_lcd_count == 0)
		fps.start_time_us = fps.trigger_lcd_time_us;
}

void DBG_OnTriggerHDMI(void)
{
	if (!dbg_opt.en_fps_log && !dbg_opt.en_touch_latency_log)
		return;

	hdmi_fps.trigger_hdmi_time_us = get_current_time_us();
	if (hdmi_fps.trigger_hdmi_count == 0)
		hdmi_fps.hdmi_start_time_us = hdmi_fps.trigger_hdmi_time_us;
}

void DBG_OnTeDelayDone(void)
{
	long int time;

	if (!dbg_opt.en_fps_log && !dbg_opt.en_touch_latency_log)
		return;

	time = get_current_time_us();
	fps.current_te_delay_time_us = (time - fps.trigger_lcd_time_us);
	fps.total_te_delay_time_us += fps.current_te_delay_time_us;
}

void DBG_OnLcdDone(void)
{
	long int time;

	if (!dbg_opt.en_fps_log && !dbg_opt.en_touch_latency_log)
		return;

	/* deal with touch latency log */

	time = get_current_time_us();
	fps.current_lcd_time_us = (time - fps.trigger_lcd_time_us);

#if 0 /* FIXME */
	if (dbg_opt.en_touch_latency_log && tpd_start_profiling) {

		pr_info(
			"DISP/DBG Touch Latency: %ld ms\n",
			(time - tpd_last_down_time) / 1000);

		pr_info(
			"DISP/DBG LCD update time %ld ms (TE delay %ld ms + LCD %ld ms)\n",
			fps.current_lcd_time_us / 1000,
			fps.current_te_delay_time_us / 1000,
			(fps.current_lcd_time_us -
			 fps.current_te_delay_time_us) / 1000);

		tpd_start_profiling = 0;
	}
#endif

	if (!dbg_opt.en_fps_log)
		return;

	/* deal with fps log */

	fps.total_lcd_time_us += fps.current_lcd_time_us;
	++fps.trigger_lcd_count;

	if (fps.trigger_lcd_count >= dbg_opt.log_fps_wnd_size) {

		long int f = fps.trigger_lcd_count * 100 * 1000 * 1000 /
			     (time - fps.start_time_us);

		long int update = fps.total_lcd_time_us * 100 /
				  (1000 * fps.trigger_lcd_count);

		long int te = fps.total_te_delay_time_us * 100 /
			      (1000 * fps.trigger_lcd_count);

		long int lcd =
			(fps.total_lcd_time_us - fps.total_te_delay_time_us) *
			100 / (1000 * fps.trigger_lcd_count);

		pr_info(
			"DISP/DBG MTKFB FPS: %ld.%02ld, Avg. update time: %ld.%02ld ms ",
			f / 100, f % 100, update / 100, update % 100);
		pr_info("(TE delay %ld.%02ld ms, LCD %ld.%02ld ms)\n", te / 100,
			te % 100, lcd / 100, lcd % 100);
		reset_fps_logger();
	}
}

void DBG_OnHDMIDone(void)
{
	long int time;

	if (!dbg_opt.en_fps_log && !dbg_opt.en_touch_latency_log)
		return;

	/* deal with touch latency log */

	time = get_current_time_us();
	hdmi_fps.current_hdmi_time_us = (time - hdmi_fps.trigger_hdmi_time_us);

	if (!dbg_opt.en_fps_log)
		return;

	/* deal with fps log */

	hdmi_fps.total_hdmi_time_us += hdmi_fps.current_hdmi_time_us;
	++hdmi_fps.trigger_hdmi_count;

	if (hdmi_fps.trigger_hdmi_count >= dbg_opt.log_fps_wnd_size) {

		long int f = hdmi_fps.trigger_hdmi_count * 100 * 1000 * 1000 /
			     (time - hdmi_fps.hdmi_start_time_us);

		long int update = hdmi_fps.total_hdmi_time_us * 100 /
				  (1000 * hdmi_fps.trigger_hdmi_count);

		pr_info(
			"DISP/DBG [HDMI] FPS: %ld.%02ld, Avg. update time: %ld.%02ld ms\n",
			f / 100, f % 100, update / 100, update % 100);

		reset_hdmi_fps_logger();
	}
}

/* ---------------------------------------------------------------------------
 */
/* Command Processor */
/* ---------------------------------------------------------------------------
 */

bool get_ovl1_to_mem_on(void)
{
	return enable_ovl1_to_mem;
}

void switch_ovl1_to_mem(bool on)
{
	enable_ovl1_to_mem = on;
	pr_info("DISP/DBG switch_ovl1_to_mem %d\n", enable_ovl1_to_mem);
}

static int _draw_line(unsigned long addr, int l, int t, int r, int b,
		      int linepitch, unsigned int color)
{
	int i = 0;

	if (l > r || b < t)
		return -1;

	if (l == r) { /* vertical line */
		for (i = 0; i < (b - t); i++)
			*(unsigned long *)(addr + (t + i) * linepitch + l * 4) =
				color;
	} else if (t == b) { /* horizontal line */
		for (i = 0; i < (r - l); i++)
			*(unsigned long *)(addr + t * linepitch + (l + i) * 4) =
				color;
	} else { /* tile line, not support now */
		return -1;
	}

	return 0;
}

static int _draw_rect(unsigned int addr, int l, int t, int r, int b,
		      unsigned int linepitch, unsigned int color)
{
	int ret = 0;

	ret += _draw_line(addr, l, t, r, t, linepitch, color);
	ret += _draw_line(addr, l, t, l, b, linepitch, color);
	ret += _draw_line(addr, r, t, r, b, linepitch, color);
	ret += _draw_line(addr, l, b, r, b, linepitch, color);
	return ret;
}

static void _draw_block(unsigned long addr, unsigned int x, unsigned int y,
			unsigned int w, unsigned int h, unsigned int linepitch,
			unsigned int color)
{
	int i = 0;
	int j = 0;
	unsigned long start_addr = addr + linepitch * y + x * 4;

	DISPMSG(
		"addr=0x%lx, start_addr=0x%lx, x=%d,y=%d,w=%d,h=%d,linepitch=%d, color=0x%08x\n",
		addr, start_addr, x, y, w, h, linepitch, color);
	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++)
			*(unsigned long *)(start_addr + i * 4 + j * linepitch) =
				color;
	}
}

static int g_display_debug_pattern_index;
void _debug_pattern(unsigned int mva, unsigned long va, unsigned int w,
		    unsigned int h, unsigned int linepitch, unsigned int color,
		    unsigned int layerid, unsigned int bufidx)
{
	unsigned long addr = 0;
	unsigned int layer_size = 0;
	unsigned int mapped_size = 0;
	unsigned int bcolor = 0xff808080;

	if (g_display_debug_pattern_index == 0)
		return;

	if (layerid == 0)
		bcolor = 0x0000ffff;
	else if (layerid == 1)
		bcolor = 0x00ff00ff;
	else if (layerid == 2)
		bcolor = 0xff0000ff;
	else if (layerid == 3)
		bcolor = 0xffff00ff;

	if (va) {
		addr = va;
	} else {
		layer_size = linepitch * h;
		m4u_mva_map_kernel(mva, layer_size, &addr, &mapped_size);
		if (mapped_size == 0) {
			DISPERR("m4u_mva_map_kernel failed\n");
			return;
		}
	}

	switch (g_display_debug_pattern_index) {
	case 1: {
		unsigned int resize_factor = layerid + 1;

		_draw_rect(addr, w / 10 * resize_factor + 0,
			   h / 10 * resize_factor + 0,
			   w / 10 * (10 - resize_factor) - 0,
			   h / 10 * (10 - resize_factor) - 0, linepitch,
			   bcolor);
		_draw_rect(addr, w / 10 * resize_factor + 1,
			   h / 10 * resize_factor + 1,
			   w / 10 * (10 - resize_factor) - 1,
			   h / 10 * (10 - resize_factor) - 1, linepitch,
			   bcolor);
		_draw_rect(addr, w / 10 * resize_factor + 2,
			   h / 10 * resize_factor + 2,
			   w / 10 * (10 - resize_factor) - 2,
			   h / 10 * (10 - resize_factor) - 2, linepitch,
			   bcolor);
		_draw_rect(addr, w / 10 * resize_factor + 3,
			   h / 10 * resize_factor + 3,
			   w / 10 * (10 - resize_factor) - 3,
			   h / 10 * (10 - resize_factor) - 3, linepitch,
			   bcolor);
		break;
	}
	case 2: {
		int bw = 20;
		int bh = 20;

		_draw_block(addr, bufidx % (w / bw) * bw,
			    bufidx % (w * h / bh / bh) / (w / bh) * bh, bw, bh,
			    linepitch, bcolor);
		break;
	}
	case 3: {
		int bw = 20;
		int bh = 20;

		_draw_block(addr, bufidx % (w / bw) * bw,
			    bufidx % (w * h / bh / bh) / (w / bh) * bh, bw, bh,
			    linepitch, bcolor);
		break;
	}
	}

	/* smp_inner_dcache_flush_all(); */
	/* outer_flush_all(); */
	if (mapped_size)
		m4u_mva_unmap_kernel(addr, layer_size, addr);
}

#define DEBUG_FPS_METER_SHOW_COUNT 60
static int _fps_meter_array[DEBUG_FPS_METER_SHOW_COUNT] = {0};

static unsigned long long _last_ts;

void _debug_fps_meter(unsigned int mva, unsigned long va, unsigned int w,
		      unsigned int h, unsigned int linepitch,
		      unsigned int color, unsigned int layerid,
		      unsigned int bufidx)
{
	int i = 0;
	unsigned long addr = 0;
	unsigned int layer_size = 0;
	unsigned int mapped_size = 0;
	unsigned long long current_ts = sched_clock();
	unsigned long long t = current_ts;
	unsigned long mod = 0;
	unsigned long current_idx = 0;
	unsigned long long l = _last_ts;

	if (g_display_debug_pattern_index != 3)
		return;
	DISPMSG("layerid=%d\n", layerid);
	_last_ts = current_ts;

	if (va) {
		addr = va;
	} else {
		layer_size = linepitch * h;
		m4u_mva_map_kernel(mva, layer_size, &addr, &mapped_size);
		if (mapped_size == 0) {
			DISPERR("m4u_mva_map_kernel failed\n");
			return;
		}
	}

	mod = do_div(t, 1000 * 1000 * 1000);
	do_div(l, 1000 * 1000 * 1000);
	if (t != l) {
		memset((void *)_fps_meter_array, 0, sizeof(_fps_meter_array));
		_draw_block(addr, 0, 10, w, 36, linepitch, 0x00000000);
	}

	current_idx = mod / 1000 / 16666;
	DISPMSG("mod=%ld, current_idx=%ld\n", mod, current_idx);
	_fps_meter_array[current_idx]++;
	for (i = 0; i < DEBUG_FPS_METER_SHOW_COUNT; i++) {
		if (_fps_meter_array[i]) {
			_draw_block(addr, i * 18, 10, 18,
				    18 * _fps_meter_array[i], linepitch,
				    0xff0000ff);
		} else {
			/*
			 * _draw_block(addr, i*18, 10, 18, 18, linepitch,
			 * 0x00000000);
			 */
		}
	}

	/* smp_inner_dcache_flush_all(); */
	/* outer_flush_all(); */
	if (mapped_size)
		m4u_mva_unmap_kernel(addr, layer_size, addr);
}

static struct disp_session_input_config session_input;
static void process_dbg_opt(const char *opt)
{
	int ret = 0;

	if (strncmp(opt, "dsipattern:", 11) == 0) {
		char *p = (char *)opt + 11;
		unsigned long pattern = 0;

		ret = kstrtoul(p, 16, &pattern);
		if (ret)
			pr_err("DISP/%s: errno %d\n", __func__, ret);

		if (pattern) {
			DSI_BIST_Pattern_Test(DISP_MODULE_DSI0, NULL, true,
					      pattern);
			DISPMSG("enable dsi pattern: 0x%08x\n", (int)pattern);
		} else {
			primary_display_manual_lock();
			DSI_BIST_Pattern_Test(DISP_MODULE_DSI0, NULL, false, 0);
			primary_display_manual_unlock();
			return;
		}
	} else if (strncmp(opt, "dpipattern:", 11) == 0) {
		char *p = (char *)opt + 11;
		unsigned long dpi = 0;

		ret = kstrtoul(p, 10, &dpi);
		if (ret)
			pr_err("DISP/%s: errno %d\n", __func__, ret);

		if (dpi == 0)
			ddp_dpi_EnableColorBar(DISP_MODULE_DPI0);
		else
			ddp_dpi_EnableColorBar(DISP_MODULE_DPI1);

		return;
	} else if (strncmp(opt, "diagnose", 8) == 0) {
		primary_display_diagnose();
		return;
	}
#ifdef CONFIG_MTK_SEGMENT_TEST
	else if (strncmp(opt, "_efuse_test", 11) == 0)
		primary_display_check_test();
#endif
	else if (strncmp(opt, "trigger", 7) == 0) {
		struct display_primary_path_context *ctx =
			primary_display_path_lock("debug");

		if (ctx)
			dpmgr_signal_event(ctx->dpmgr_handle,
					   DISP_PATH_EVENT_TRIGGER);
		primary_display_path_unlock("debug");

		return;
	} else if (strncmp(opt, "dprec_reset", 11) == 0) {
		dprec_logger_reset_all();
		return;
	} else if (strncmp(opt, "suspend", 4) == 0) {
		int i = 0;
		int totalLayer = primary_display_get_option("ASSERT_LAYER");

		memset((void *)&session_input, 0, sizeof(session_input));
		for (i = 0; i <= totalLayer; i++) {
			session_input.config[i].layer_id = i;
			session_input.config[i].next_buff_idx = -1;
			session_input.config[i].layer_enable = 0;
			session_input.config_layer_num++;
		}
		primary_display_config_input_multiple(&session_input);
		primary_display_trigger(true, NULL, 0);
		primary_display_suspend();
		return;
	} else if (strncmp(opt, "ata", 3) == 0) {
		mtkfb_fm_auto_test();
		return;
	} else if (strncmp(opt, "resume", 4) == 0)
		primary_display_resume();
	else if (strncmp(opt, "dalprintf", 9) == 0)
		DAL_Printf("display aee layer test\n");
	else if (strncmp(opt, "dalclean", 8) == 0)
		DAL_Clean();
	else if (strncmp(opt, "DP", 2) == 0) {
		char *p = (char *)opt + 3;
		unsigned long pattern = 0;

		ret = kstrtoul(p, 16, &pattern);
		if (ret)
			pr_err("DISP/%s: errno %d\n", __func__, ret);

		g_display_debug_pattern_index = (int)pattern;
		return;
	} else if (strncmp(opt, "dsi0_clk:", 9) == 0) {
		char *p = (char *)opt + 9;
		unsigned long clk = 0;

		ret = kstrtoul(p, 10, &clk);
		if (ret)
			pr_err("DISP/%s: errno %d\n", __func__, ret);

		DSI_ChangeClk(DISP_MODULE_DSI0, (uint32_t)clk);
	} else if (strncmp(opt, "diagnose", 8) == 0) {
		primary_display_diagnose();
		return;
	} else if (strncmp(opt, "switch:", 7) == 0) {
		char *p = (char *)opt + 7;
		unsigned long mode = 0;

		ret = kstrtoul(p, 10, &mode);
		if (ret)
			pr_err("DISP/%s: errno %d\n", __func__, ret);

		primary_display_switch_dst_mode((uint32_t)mode % 2);
		return;

	} else if (strncmp(opt, "cmmva_dprec", 11) == 0) {
		dprec_handle_option(0x7);
	} else if (strncmp(opt, "cmmpa_dprec", 11) == 0) {
		dprec_handle_option(0x3);
	} else if (strncmp(opt, "dprec", 5) == 0) {
		char *p = (char *)opt + 6;
		unsigned long option = 0;

		ret = kstrtoul(p, 16, &option);
		if (ret)
			pr_err("DISP/%s: errno %d\n", __func__, ret);

		dprec_handle_option((int)option);
	} else if (strncmp(opt, "cmdq", 4) == 0) {
		char *p = (char *)opt + 5;
		unsigned long option = 0;

		ret = kstrtoul(p, 16, &option);
		if (ret)
			pr_err("DISP/%s: errno %d\n", __func__, ret);

		if (option)
			primary_display_switch_cmdq_cpu(CMDQ_ENABLE);
		else
			primary_display_switch_cmdq_cpu(CMDQ_DISABLE);
	} else if (strncmp(opt, "maxlayer", 8) == 0) {
		char *p = (char *)opt + 9;
		unsigned long maxlayer = 0;

		ret = kstrtoul(p, 10, &maxlayer);
		if (ret)
			pr_err("DISP/%s: errno %d\n", __func__, ret);

		if (maxlayer)
			primary_display_set_max_layer((int)maxlayer);
		else
			DISPERR("can't set max layer to 0\n");
	} else if (strncmp(opt, "primary_reset", 13) == 0) {
		primary_display_reset();
	} else if (strncmp(opt, "esd_check", 9) == 0) {
		char *p = (char *)opt + 10;
		unsigned long enable = 0;

		ret = kstrtoul(p, 10, &enable);
		if (ret)
			pr_err("DISP/%s: errno %d\n", __func__, ret);

		primary_display_esd_check_enable((int)enable);
	} else if (strncmp(opt, "cmd:", 4) == 0) {
		char *p = (char *)opt + 4;
		unsigned long value = 0;

		int lcm_cmd[5];
		unsigned int cmd_num = 1;

		ret = kstrtoul(p, 5, &value);
		if (ret)
			pr_err("DISP/%s: errno %d\n", __func__, ret);

		lcm_cmd[0] = (int)value;
		primary_display_set_cmd(lcm_cmd, cmd_num);
	} else if (strncmp(opt, "esd_recovery", 12) == 0) {
		primary_display_esd_recovery();
	} else if (strncmp(opt, "lcm0_reset", 10) == 0) {
		;
	} else if (strncmp(opt, "lcm0_reset0", 11) == 0) {
		DISP_CPU_REG_SET(DDP_REG_BASE_MMSYS_CONFIG + 0x150, 0);
	} else if (strncmp(opt, "lcm0_reset1", 11) == 0) {
		DISP_CPU_REG_SET(DDP_REG_BASE_MMSYS_CONFIG + 0x150, 1);
	} else if (strncmp(opt, "cg", 2) == 0) {
		char *p = (char *)opt + 2;
		unsigned long enable = 0;

		ret = kstrtoul(p, 10, &enable);
		if (ret)
			pr_err("DISP/%s: errno %d\n", __func__, ret);

		primary_display_enable_path_cg((int)enable);
	} else if (strncmp(opt, "ovl2mem:", 8) == 0) {
		if (strncmp(opt + 8, "on", 2) == 0)
			switch_ovl1_to_mem(true);
		else
			switch_ovl1_to_mem(false);
	} else if (strncmp(opt, "dump_layer:", 11) == 0) {
		if (strncmp(opt + 11, "on", 2) == 0) {
			char *p = (char *)opt + 14;
			unsigned long int temp;

			ret = kstrtoul(p, 10, &temp);
			gCapturePriLayerDownX = (int)temp;
			if (ret)
				pr_err("DISP/%s: errno %d\n", __func__, ret);
			ret = kstrtoul(p + 1, 10, &temp);
			gCapturePriLayerDownY = (int)temp;
			if (ret)
				pr_err("DISP/%s: errno %d\n", __func__, ret);
			ret = kstrtoul(p + 1, 10, &temp);
			gCapturePriLayerNum = (int)temp;
			if (ret)
				pr_err("DISP/%s: errno %d\n", __func__, ret);

			gCapturePriLayerEnable = 1;
			gCaptureWdmaLayerEnable = 1;
			if (gCapturePriLayerDownX == 0)
				gCapturePriLayerDownX = 20;
			if (gCapturePriLayerDownY == 0)
				gCapturePriLayerDownY = 20;
			pr_debug("dump_layer En %d DownX %d DownY %d,Num %d",
				 gCapturePriLayerEnable, gCapturePriLayerDownX,
				 gCapturePriLayerDownY, gCapturePriLayerNum);

		} else if (strncmp(opt + 11, "off", 3) == 0) {
			gCapturePriLayerEnable = 0;
			gCaptureWdmaLayerEnable = 0;
			gCapturePriLayerNum = OVL_LAYER_NUM;
			pr_debug("dump_layer En %d\n", gCapturePriLayerEnable);
		}
	} else if (strncmp(opt, "bkl:", 4) == 0) {
		char *p = (char *)opt + 4;
		unsigned long int level = 0;

		ret = kstrtoul(p, 10, &level);
		if (ret)
			pr_err("DISP/%s: errno %d\n", __func__, ret);

		pr_debug("process_dbg_opt(), set backlight level = %ld\n",
			 level);
		primary_display_setbacklight(level);
	} else if (strncmp(opt, "dsc:", 4) == 0) {
		if (strncmp(opt + 4, "rd:", 3) == 0) {
			char *p = (char *)opt + 7;
			uint8_t dsc_cmd;
			unsigned long dsc_cmd_ul;
			/* dsc_cmd = (uint8_t)simple_strtoul(p, &p, 16); */
			uint8_t Val = 0;

			ret = kstrtoul(p, 16, &dsc_cmd_ul);
			if (ret)
				pr_err("DISP/%s: errno %d\n", __func__, ret);
			dsc_cmd = (uint8_t)dsc_cmd_ul;
			Val = DSCRead(dsc_cmd);
			pr_err("dsc_cmd: Read: cmd = 0x%02X  Val = 0x%02X\n",
			       dsc_cmd, Val);
		}
	} else if (strncmp(opt, "cabc:", 5) == 0) {
		if (strncmp(opt + 5, "ui", 2) == 0)
			primary_display_setbacklight_mode(1);
		else if (strncmp(opt + 5, "mov", 3) == 0)
			primary_display_setbacklight_mode(3);
		else if (strncmp(opt + 5, "still", 5) == 0)
			primary_display_setbacklight_mode(2);
		else
			pr_err("cabc do not support this mode\n");
	}
}

static void process_dbg_cmd(char *cmd)
{
	char *tok;

	pr_info("DISP/DBG [mtkfb_dbg] %s\n", cmd);

	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}

/* ---------------------------------------------------------------------------
 */
/* Debug FileSystem Routines */
/* ---------------------------------------------------------------------------
 */

struct dentry *mtkfb_dbgfs;

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static char debug_buffer[4096 + DPREC_ERROR_LOG_BUFFER_LENGTH];

int debug_get_info(unsigned char *stringbuf, int buf_len)
{
	int n = 0;

	DISPFUNC();

	n += mtkfb_get_debug_state(stringbuf + n, buf_len - n);
	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += primary_display_get_debug_state(stringbuf + n, buf_len - n);
	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += disp_sync_get_debug_info(stringbuf + n, buf_len - n);
	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += dprec_logger_get_result_string_all(stringbuf + n, buf_len - n);
	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += primary_display_check_path(stringbuf + n, buf_len - n);
	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += dprec_logger_get_buf(DPREC_LOGGER_ERROR, stringbuf + n,
				  buf_len - n);
	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += dprec_logger_get_buf(DPREC_LOGGER_FENCE, stringbuf + n,
				  buf_len - n);
	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += dprec_logger_get_buf(DPREC_LOGGER_HWOP, stringbuf + n,
				  buf_len - n);
	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	stringbuf[n++] = 0;
	return n;
}

void debug_info_dump_to_printk(char *buf, int buf_len)
{
	int i = 0;
	int n = buf_len;

	for (i = 0; i < n; i += 256)
		DISPMSG("%s", buf + i);
}

static ssize_t debug_read(struct file *file, char __user *ubuf, size_t count,
			  loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	int n = 0;

	DISPFUNC();

	n += debug_get_info(debug_buffer + n, debug_bufmax - n);
	/* debug_info_dump_to_printk(); */
	return simple_read_from_buffer(ubuf, count, ppos, debug_buffer, n);
}

static ssize_t debug_write(struct file *file, const char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&debug_buffer, ubuf, count))
		return -EFAULT;

	debug_buffer[count] = 0;

	process_dbg_cmd(debug_buffer);

	return ret;
}

static const struct file_operations debug_fops = {
	.read = debug_read, .write = debug_write, .open = debug_open,
};

#ifdef MTKFB_DEBUG_FS_CAPTURE_LAYER_CONTENT_SUPPORT

static int layer_debug_open(struct inode *inode, struct file *file)
{
	struct MTKFB_LAYER_DBG_OPTIONS *dbgopt;
	/* /record the private data */
	file->private_data = inode->i_private;
	dbgopt = (struct MTKFB_LAYER_DBG_OPTIONS *)file->private_data;

	dbgopt->working_size =
		DISP_GetScreenWidth() * DISP_GetScreenHeight() * 2 + 32;
	dbgopt->working_buf = (unsigned long)vmalloc(dbgopt->working_size);
	if (dbgopt->working_buf == 0)
		pr_info("DISP/DBG Vmalloc to get temp buffer failed\n");

	return 0;
}

static ssize_t layer_debug_read(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t layer_debug_write(struct file *file, const char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	struct MTKFB_LAYER_DBG_OPTIONS *dbgopt =
		(struct MTKFB_LAYER_DBG_OPTIONS *)file->private_data;

	pr_info("DISP/DBG mtkfb_layer%d write is not implemented yet\n",
		dbgopt->layer_index);

	return count;
}

static int layer_debug_release(struct inode *inode, struct file *file)
{
	struct MTKFB_LAYER_DBG_OPTIONS *dbgopt;

	dbgopt = (struct MTKFB_LAYER_DBG_OPTIONS *)file->private_data;

	if (dbgopt->working_buf != 0)
		vfree((void *)dbgopt->working_buf);

	dbgopt->working_buf = 0;

	return 0;
}

static const struct file_operations layer_debug_fops = {
	.read = layer_debug_read,
	.write = layer_debug_write,
	.open = layer_debug_open,
	.release = layer_debug_release,
};

#endif

void DBG_Init(void)
{
	mtkfb_dbgfs = debugfs_create_file("mtkfb", S_IFREG | 0444, NULL,
					  (void *)0, &debug_fops);

	memset(&dbg_opt, 0, sizeof(dbg_opt));
	memset(&fps, 0, sizeof(fps));

	dbg_opt.log_fps_wnd_size = DEFAULT_LOG_FPS_WND_SIZE;
	/* xuecheng, enable fps log by default */
	dbg_opt.en_fps_log = 1;

#ifdef MTKFB_DEBUG_FS_CAPTURE_LAYER_CONTENT_SUPPORT
	{
		unsigned int i;
		unsigned char a[13];

		a[0] = 'm';
		a[1] = 't';
		a[2] = 'k';
		a[3] = 'f';
		a[4] = 'b';
		a[5] = '_';
		a[6] = 'l';
		a[7] = 'a';
		a[8] = 'y';
		a[9] = 'e';
		a[10] = 'r';
		a[11] = '0';
		a[12] = '\0';

		for (i = 0; i < DDP_OVL_LAYER_MUN; i++) {
			a[11] = '0' + i;
			mtkfb_layer_dbg_opt[i].layer_index = i;
			mtkfb_layer_dbgfs[i] = debugfs_create_file(
				a, S_IFREG | 0444, NULL,
				(void *)&mtkfb_layer_dbg_opt[i],
				&layer_debug_fops);
		}
	}
#endif
}

void DBG_Deinit(void)
{
	debugfs_remove(mtkfb_dbgfs);
#ifdef MTKFB_DEBUG_FS_CAPTURE_LAYER_CONTENT_SUPPORT
	{
		unsigned int i;

		for (i = 0; i < DDP_OVL_LAYER_MUN; i++)
			debugfs_remove(mtkfb_layer_dbgfs[i]);
	}
#endif
}
