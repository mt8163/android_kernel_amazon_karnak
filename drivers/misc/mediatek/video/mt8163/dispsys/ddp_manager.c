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

#define LOG_TAG "ddp_manager"

#include <linux/mutex.h>
#include <linux/slab.h>

#include "ddp_debug.h"
#include "ddp_drv.h"
#include "ddp_irq.h"
#include "ddp_manager.h"
#include "ddp_ovl.h"
#include "ddp_path.h"
#include "ddp_rdma.h"
#include "ddp_reg.h"
#include "lcm_drv.h"

#include "ddp_log.h"
#include "mtkfb_fence.h"
#include "primary_display.h"
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif
/* #pragma GCC optimize("O0") */

unsigned int gDDPError;
static int ddp_manager_init;
#define DDP_MAX_MANAGER_HANDLE (DISP_MUTEX_DDP_COUNT + DISP_MUTEX_DDP_FIRST)

unsigned int allLayerDisabled;

struct DPMGR_WQ_HANDLE {
	/* volatile */ unsigned int init;
	enum DISP_PATH_EVENT event;
	wait_queue_head_t wq;
	/* volatile */ unsigned long long data;
};

struct DDP_IRQ_EVENT_MAPPING {
	enum DDP_IRQ_BIT irq_bit;
};

struct ddp_path_handle_t {
	struct cmdqRecStruct *cmdqhandle;
	int hwmutexid;
	int power_sate;
	enum DDP_MODE mode;
	struct mutex mutex_lock;
	struct DDP_IRQ_EVENT_MAPPING irq_event_map[DISP_PATH_EVENT_NUM];
	struct DPMGR_WQ_HANDLE wq_list[DISP_PATH_EVENT_NUM];
	enum DDP_SCENARIO_ENUM scenario;
	enum DISP_MODULE_ENUM mem_module;
	struct disp_ddp_path_config last_config;
};

struct DDP_MANAGER_CONTEXT {
	int handle_cnt;
	int mutex_idx;
	int power_sate;
	struct mutex mutex_lock;
	int module_usage_table[DISP_MODULE_NUM];
	struct ddp_path_handle_t *module_path_table[DISP_MODULE_NUM];
	struct ddp_path_handle_t *handle_pool[DDP_MAX_MANAGER_HANDLE];
};

#define DEFAULT_IRQ_EVENT_SCENARIO (4)
static struct DDP_IRQ_EVENT_MAPPING
	ddp_irq_event_list[DEFAULT_IRQ_EVENT_SCENARIO][DISP_PATH_EVENT_NUM] = {
		{
			/* ovl0 path */
			{DDP_IRQ_RDMA0_DONE},		/*FRAME_DONE */
			{DDP_IRQ_RDMA0_START},		/*FRAME_START */
			{DDP_IRQ_RDMA0_REG_UPDATE},     /*FRAME_REG_UPDATE */
			{DDP_IRQ_RDMA0_TARGET_LINE},    /*FRAME_TARGET_LINE */
			{DDP_IRQ_WDMA0_FRAME_COMPLETE}, /*FRAME_COMPLETE */
			{DDP_IRQ_RDMA0_TARGET_LINE},    /*FRAME_STOP */
			{DDP_IRQ_RDMA0_REG_UPDATE},     /*IF_CMD_DONE */
			{DDP_IRQ_DSI0_EXT_TE},		/*IF_VSYNC */
			{DDP_IRQ_UNKNOWN},
			/*TRIGER*/
			{DDP_IRQ_AAL_OUT_END_FRAME}, /*AAL_OUT_END_EVENT */
		},
		{
			/* ovl1 path */
			{DDP_IRQ_RDMA1_DONE},		/*FRAME_DONE */
			{DDP_IRQ_RDMA1_START},		/*FRAME_START */
			{DDP_IRQ_RDMA1_REG_UPDATE},     /*FRAME_REG_UPDATE */
			{DDP_IRQ_RDMA1_TARGET_LINE},    /*FRAME_TARGET_LINE */
			{DDP_IRQ_WDMA1_FRAME_COMPLETE}, /*FRAME_COMPLETE */
			{DDP_IRQ_RDMA1_TARGET_LINE},    /*FRAME_STOP */
			{DDP_IRQ_RDMA1_REG_UPDATE},     /*IF_CMD_DONE */
			{DDP_IRQ_RDMA1_TARGET_LINE},    /*IF_VSYNC */
			{DDP_IRQ_UNKNOWN},
			/*TRIGER*/ {DDP_IRQ_UNKNOWN}, /*AAL_OUT_END_EVENT */
		},
		{
			/* rdma path */
			{DDP_IRQ_RDMA2_DONE},	/*FRAME_DONE */
			{DDP_IRQ_RDMA2_START},       /*FRAME_START */
			{DDP_IRQ_RDMA2_REG_UPDATE},  /*FRAME_REG_UPDATE */
			{DDP_IRQ_RDMA2_TARGET_LINE}, /*FRAME_TARGET_LINE */
			{DDP_IRQ_UNKNOWN},	   /*FRAME_COMPLETE */
			{DDP_IRQ_RDMA2_TARGET_LINE}, /*FRAME_STOP */
			{DDP_IRQ_RDMA2_REG_UPDATE},  /*IF_CMD_DONE */
			{DDP_IRQ_RDMA2_TARGET_LINE}, /*IF_VSYNC */
			{DDP_IRQ_UNKNOWN},
			/*TRIGER*/ {DDP_IRQ_UNKNOWN}, /*AAL_OUT_END_EVENT */
		},
		{
			/* ovl0 path */
			{DDP_IRQ_RDMA0_DONE},		/*FRAME_DONE */
			{DDP_IRQ_MUTEX1_SOF},		/*FRAME_START */
			{DDP_IRQ_RDMA0_REG_UPDATE},     /*FRAME_REG_UPDATE */
			{DDP_IRQ_RDMA0_TARGET_LINE},    /*FRAME_TARGET_LINE */
			{DDP_IRQ_WDMA0_FRAME_COMPLETE}, /*FRAME_COMPLETE */
			{DDP_IRQ_RDMA0_TARGET_LINE},    /*FRAME_STOP */
			{DDP_IRQ_RDMA0_REG_UPDATE},     /*IF_CMD_DONE */
			{DDP_IRQ_DSI0_EXT_TE},		/*IF_VSYNC */
			{DDP_IRQ_UNKNOWN},
			/*TRIGER*/
			{DDP_IRQ_AAL_OUT_END_FRAME}, /*AAL_OUT_END_EVENT */
		} };

static char *path_event_name(enum DISP_PATH_EVENT event)
{
	switch (event) {
	case DISP_PATH_EVENT_FRAME_START:
		return "FRAME_START";
	case DISP_PATH_EVENT_FRAME_DONE:
		return "FRAME_DONE";
	case DISP_PATH_EVENT_FRAME_REG_UPDATE:
		return "REG_UPDATE";
	case DISP_PATH_EVENT_FRAME_TARGET_LINE:
		return "TARGET_LINE";
	case DISP_PATH_EVENT_FRAME_COMPLETE:
		return "FRAME COMPLETE";
	case DISP_PATH_EVENT_FRAME_STOP:
		return "FRAME_STOP";
	case DISP_PATH_EVENT_IF_CMD_DONE:
		return "FRAME_STOP";
	case DISP_PATH_EVENT_IF_VSYNC:
		return "VSYNC";
	case DISP_PATH_EVENT_TRIGGER:
		return "TRIGGER";
	default:
		return "unknown event";
	}
	return "unknown event";
}

static struct DDP_MANAGER_CONTEXT *_get_context(void)
{
	static int is_context_inited;
	static struct DDP_MANAGER_CONTEXT context;

	if (!is_context_inited) {
		memset((void *)&context, 0, sizeof(struct DDP_MANAGER_CONTEXT));
		context.mutex_idx = (1 << DISP_MUTEX_DDP_COUNT) - 1;
		mutex_init(&context.mutex_lock);
		is_context_inited = 1;
	}
	return &context;
}

static int path_top_clock_off(void)
{
	int i = 0;
	struct DDP_MANAGER_CONTEXT *context = _get_context();

	if (context->power_sate) {
		for (i = 0; i < DDP_MAX_MANAGER_HANDLE; i++) {
			if (context->handle_pool[i] != NULL &&
			    context->handle_pool[i]->power_sate != 0) {
				return 0;
			}
		}
		context->power_sate = 0;
		ddp_path_top_clock_off();
	}
	return 0;
}

static int path_top_clock_on(void)
{
	struct DDP_MANAGER_CONTEXT *context = _get_context();

	if (!context->power_sate) {
		context->power_sate = 1;
		ddp_path_top_clock_on();
	}
	return 0;
}

static int module_power_off(enum DISP_MODULE_ENUM module)
{
	if (module == DISP_MODULE_DSI0)
		return 0;

	if (ddp_modules_driver[module] != 0) {
		if (ddp_modules_driver[module]->power_off != 0) {
			DISP_LOG_V("%s power off\n",
				   ddp_get_module_name(module));
			ddp_modules_driver[module]->power_off(
				module, NULL); /* now just 0; */
		}
	}
	return 0;
}

static int module_power_on(enum DISP_MODULE_ENUM module)
{
	if (module == DISP_MODULE_DSI0) {
		/*bypass dsi */
		return 0;
	}
	if (ddp_modules_driver[module] != 0) {
		if (ddp_modules_driver[module]->power_on != 0) {
			DISP_LOG_V("%s power on\n",
				   ddp_get_module_name(module));
			ddp_modules_driver[module]->power_on(
				module, NULL); /* now just 0; */
		}
	}
	return 0;
}

static struct ddp_path_handle_t *
find_handle_by_module(enum DISP_MODULE_ENUM module)
{
	return _get_context()->module_path_table[module];
}

static int dpmgr_module_notify(enum DISP_MODULE_ENUM module,
			       enum DISP_PATH_EVENT event)
{
	struct ddp_path_handle_t *handle = find_handle_by_module(module);

	mmprofile_log_ex(ddp_mmp_get_events()->primary_display_aalod_trigger,
			 MMPROFILE_FLAG_PULSE, module, 0);
	return dpmgr_signal_event(handle, event);
	return 0;
}

static int assign_default_irqs_table(enum DDP_SCENARIO_ENUM scenario,
				     struct DDP_IRQ_EVENT_MAPPING *irq_events)
{
	int idx = 0;

	switch (scenario) {
	case DDP_SCENARIO_PRIMARY_DISP:
	case DDP_SCENARIO_PRIMARY_RDMA0_COLOR0_DISP:
	case DDP_SCENARIO_PRIMARY_RDMA0_DISP:
	case DDP_SCENARIO_PRIMARY_BYPASS_RDMA:
	case DDP_SCENARIO_PRIMARY_DITHER_MEMOUT:
	case DDP_SCENARIO_PRIMARY_UFOE_MEMOUT:
	case DDP_SCENARIO_PRIMARY_ALL:
	case DDP_SCENARIO_MULTIPLE_OVL:
		idx = 0;
		break;
	case DDP_SCENARIO_SUB_DISP:
	case DDP_SCENARIO_SUB_RDMA1_DISP:
	case DDP_SCENARIO_SUB_OVL_MEMOUT:
	case DDP_SCENARIO_SUB_ALL:
		idx = 1;
		break;
	case DDP_SCENARIO_PRIMARY_OVL_MEMOUT:
		idx = 3;
		break;
	default:
		DISP_LOG_E("unknown scenario %d\n", scenario);
	}
	DISP_LOG_V("assign default irqs table index %d\n", idx);
	memcpy(irq_events, ddp_irq_event_list[idx],
	       sizeof(ddp_irq_event_list[idx]));
	return 0;
}

static int acquire_mutex(enum DDP_SCENARIO_ENUM scenario)
{
	/* /: primay use mutex 0 */
	int mutex_id = 0;
	struct DDP_MANAGER_CONTEXT *content = _get_context();
	int mutex_idx_free = content->mutex_idx;

	ASSERT(scenario >= 0 && scenario < DDP_SCENARIO_MAX);
	while (mutex_idx_free) {
		if (mutex_idx_free & 0x1) {
			content->mutex_idx &= (~(0x1 << mutex_id));
			mutex_id += DISP_MUTEX_DDP_FIRST;
			break;
		}
		mutex_idx_free >>= 1;
		++mutex_id;
	}
	ASSERT(mutex_id < (DISP_MUTEX_DDP_FIRST + DISP_MUTEX_DDP_COUNT));
	DISP_LOG_I("scenario %s acquire mutex %d , left mutex 0x%x!\n",
		   ddp_get_scenario_name(scenario), mutex_id,
		   content->mutex_idx);
	return mutex_id;
}

static int release_mutex(int mutex_idx)
{
	struct DDP_MANAGER_CONTEXT *content = _get_context();

	ASSERT(mutex_idx < (DISP_MUTEX_DDP_FIRST + DISP_MUTEX_DDP_COUNT));
	content->mutex_idx |= 1 << (mutex_idx - DISP_MUTEX_DDP_FIRST);
	DISP_LOG_I("release mutex %d , left mutex 0x%x!\n", mutex_idx,
		   content->mutex_idx);
	return 0;
}

int dpmgr_path_set_video_mode(void *dp_handle, int is_vdo_mode)
{
	struct ddp_path_handle_t *handle = NULL;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	handle->mode = is_vdo_mode ? DDP_VIDEO_MODE : DDP_CMD_MODE;
	DISP_LOG_I("set scenario %s mode: %s\n",
		   ddp_get_scenario_name(handle->scenario),
		   is_vdo_mode ? "Video_Mode" : "Cmd_Mode");
	return 0;
}

unsigned long hw_mutex_id_to_handle_map[128];
void *dpmgr_create_path(enum DDP_SCENARIO_ENUM scenario,
			struct cmdqRecStruct *cmdq_handle)
{
	int i = 0;
	int module_name;
	struct ddp_path_handle_t *path_handle = NULL;
	int *modules = ddp_get_scenario_list(scenario);
	int module_num = ddp_get_module_num(scenario);
	struct DDP_MANAGER_CONTEXT *content = _get_context();

	path_handle = kzalloc(sizeof(struct ddp_path_handle_t), GFP_KERNEL);
	if (path_handle != NULL) {
		path_handle->cmdqhandle = cmdq_handle;
		path_handle->scenario = scenario;
		path_handle->hwmutexid = acquire_mutex(scenario);
		assign_default_irqs_table(scenario, path_handle->irq_event_map);
		DISP_LOG_V("create handle %p on scenario %s\n", path_handle,
			   ddp_get_scenario_name(scenario));
		for (i = 0; i < module_num; i++) {
			module_name = modules[i];
			DISP_LOG_V(" scenario %s include module %s\n",
				   ddp_get_scenario_name(scenario),
				   ddp_get_module_name(module_name));
			content->module_usage_table[module_name]++;
			content->module_path_table[module_name] = path_handle;
		}
		hw_mutex_id_to_handle_map[path_handle->hwmutexid] =
			(unsigned long)path_handle;
		content->handle_cnt++;
		content->handle_pool[path_handle->hwmutexid] = path_handle;
	} else {
		DISP_LOG_E("Fail to create handle on scenario %s\n",
			   ddp_get_scenario_name(scenario));
	}
	return path_handle;
}

/* please ensure thread/irq safe by caller */
int dpmgr_modify_path(void *dp_handle, enum DDP_SCENARIO_ENUM new_scenario,
		      struct cmdqRecStruct *cmdq_handle,
		      enum DDP_MODE isvdomode)
{
	int i = 0;
	int module_name = 0;
	enum DISP_MODULE_ENUM module[DISP_MODULE_NUM] = {0};
	struct DDP_MANAGER_CONTEXT *content;
	struct ddp_path_handle_t *handle;
	enum DDP_SCENARIO_ENUM old_scenario;
	int *new_modules;
	int new_module_num;
	int *old_modules;
	int old_module_num;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	content = _get_context();
	old_scenario = handle->scenario;

	new_modules = ddp_get_scenario_list(new_scenario);
	new_module_num = ddp_get_module_num(new_scenario);

	old_modules = ddp_get_scenario_list(old_scenario);
	old_module_num = ddp_get_module_num(old_scenario);

	handle->cmdqhandle = cmdq_handle;
	handle->scenario = new_scenario;
	DISP_LOG_V("modify handle %p from %s to %s\n", handle,
		   ddp_get_scenario_name(old_scenario),
		   ddp_get_scenario_name(new_scenario));

	for (i = 0; i < new_module_num; i++) {
		module_name = new_modules[i];
		module[module_name]++;
		if (content->module_usage_table[module_name] == 0) {
			content->module_usage_table[module_name]++;
			DISP_LOG_V("add module %s\n",
				   ddp_get_module_name(module_name));

			content->module_path_table[module_name] = handle;
			module_power_on(module_name);
		}
	}

	for (i = 0; i < old_module_num; i++) {
		module_name = old_modules[i];
		if (module[module_name] == 0) {
			content->module_usage_table[module_name]--;
			DISP_LOG_V("remove module %s\n",
				   ddp_get_module_name(module_name));
			content->module_path_table[module_name] = NULL;
			module_power_off(module_name);
		}
	}

	/* mutex set will clear old settings */
	ddp_mutex_set(handle->hwmutexid, new_scenario, isvdomode, cmdq_handle);
	ddp_mutex_Interrupt_enable(handle->hwmutexid, cmdq_handle);
	/* disconnect old path first */
	ddp_disconnect_path(old_scenario, cmdq_handle);

	/* connect new path */
	ddp_connect_path(new_scenario, cmdq_handle);

	return 0;
}

int dpmgr_destroy_path(void *dp_handle, struct cmdqRecStruct *cmdq_handle)
{
	int i = 0;
	int module_name;
	int *modules;
	int module_num;
	struct DDP_MANAGER_CONTEXT *content;
	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;

	ASSERT(dp_handle != NULL);
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	content = _get_context();
	DISP_LOG_V("destroy path handle %p on scenario %s\n", handle,
		   ddp_get_scenario_name(handle->scenario));
	if (handle != NULL) {
		release_mutex(handle->hwmutexid);
		ddp_disconnect_path(handle->scenario, cmdq_handle);
		for (i = 0; i < module_num; i++) {
			module_name = modules[i];
			content->module_usage_table[module_name]--;
			content->module_path_table[module_name] = NULL;
		}
		content->handle_cnt--;
		ASSERT(content->handle_cnt >= 0);
		content->handle_pool[handle->hwmutexid] = NULL;
		kfree(handle);
	}
	return 0;
}

int dpmgr_path_memout_clock(void *dp_handle, int clock_switch)
{
#if 0
	ASSERT(dp_handle != NULL);
	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *) dp_handle;

	handle->mem_module =
	    ddp_is_scenario_on_primary(handle->scenario) ?
	    DISP_MODULE_WDMA0 : DISP_MODULE_WDMA1;
	if (handle->mem_module == DISP_MODULE_WDMA0 ||
			handle->mem_module == DISP_MODULE_WDMA1) {
		if (ddp_modules_driver[handle->mem_module] != 0) {
			if (clock_switch) {
				if (ddp_modules_driver[handle->mem_module]->
						power_on != 0) {
					ddp_modules_driver[handle->mem_module]
						->power_on(handle->
						 mem_module,
						 NULL);
				}
			} else {
				if (ddp_modules_driver[handle->mem_module]->
						power_off != 0) {
					ddp_modules_driver[handle->mem_module]
						->power_off(handle->
						mem_module,
						NULL);
				}
				handle->mem_module = DISP_MODULE_UNKNOWN;
			}
		}
		return 0;
	}
	return -1;
#endif
	return 0;
}

int dpmgr_path_add_memout(void *dp_handle, enum ENGINE_DUMP engine,
			  void *cmdq_handle)
{
	struct DDP_MANAGER_CONTEXT *context;
	enum DISP_MODULE_ENUM wdma;
	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;

	ASSERT(dp_handle != NULL);
	ASSERT(handle->scenario == DDP_SCENARIO_PRIMARY_DISP ||
	       handle->scenario == DDP_SCENARIO_SUB_DISP ||
	       handle->scenario == DDP_SCENARIO_PRIMARY_OVL_MEMOUT);
	wdma = ddp_is_scenario_on_primary(handle->scenario) ? DISP_MODULE_WDMA0
							    : DISP_MODULE_WDMA1;

	if (ddp_is_module_in_scenario(handle->scenario, wdma) == 1) {
		DDPERR(
			"dpmgr_path_add_memout error, wdma is already in scenario=%s\n",
		       ddp_get_scenario_name(handle->scenario));
		/* ASSERT(0); */
		return -1;
	}
	/* update contxt */
	context = _get_context();
	context->module_usage_table[wdma]++;
	context->module_path_table[wdma] = handle;
	handle->scenario = DDP_SCENARIO_PRIMARY_ALL;
	/* update connected */
	ddp_connect_path(handle->scenario, cmdq_handle);
	ddp_mutex_set(handle->hwmutexid, handle->scenario, handle->mode,
		      cmdq_handle);

	/* wdma just need start. */
	if (ddp_modules_driver[wdma] != 0) {
		if (ddp_modules_driver[wdma]->init != 0)
			ddp_modules_driver[wdma]->init(wdma, cmdq_handle);

		if (ddp_modules_driver[wdma]->start != 0)
			ddp_modules_driver[wdma]->start(wdma, cmdq_handle);
	}
	return 0;
}

int dpmgr_path_remove_memout(void *dp_handle, void *cmdq_handle)
{

	struct DDP_MANAGER_CONTEXT *context;
	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;
	enum DISP_MODULE_ENUM wdma;

	ASSERT(dp_handle != NULL);
	ASSERT(handle->scenario == DDP_SCENARIO_PRIMARY_DISP ||
	       handle->scenario == DDP_SCENARIO_PRIMARY_ALL ||
	       handle->scenario == DDP_SCENARIO_SUB_DISP ||
	       handle->scenario == DDP_SCENARIO_SUB_ALL);
	wdma = ddp_is_scenario_on_primary(handle->scenario) ? DISP_MODULE_WDMA0
							    : DISP_MODULE_WDMA1;

	if (ddp_is_module_in_scenario(handle->scenario, wdma) == 0) {
		DDPERR(
			"dpmgr_path_remove_memout error, wdma is not in scenario=%s\n",
		       ddp_get_scenario_name(handle->scenario));
		/* ASSERT(0); */
		return -1;
	}
	/* update contxt */
	context = _get_context();
	context->module_usage_table[wdma]--;
	context->module_path_table[wdma] = 0;
	/* wdma just need stop */
	if (ddp_modules_driver[wdma] != 0) {
		if (ddp_modules_driver[wdma]->stop != 0)
			ddp_modules_driver[wdma]->stop(wdma, cmdq_handle);

		if (ddp_modules_driver[wdma]->deinit != 0)
			ddp_modules_driver[wdma]->deinit(wdma, cmdq_handle);
	}
	ddp_disconnect_path(DDP_SCENARIO_PRIMARY_OVL_MEMOUT, cmdq_handle);

	handle->scenario = DDP_SCENARIO_PRIMARY_DISP;
	/* update connected */
	ddp_connect_path(handle->scenario, cmdq_handle);
	ddp_mutex_set(handle->hwmutexid, handle->scenario, handle->mode,
		      cmdq_handle);
	return 0;
}

int dpmgr_insert_ovl1_sub(void *dp_handle, void *cmdq_handle)
{

	struct DDP_MANAGER_CONTEXT *context;

	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;

	ASSERT(dp_handle != NULL);

	if ((handle->scenario != DDP_SCENARIO_SUB_RDMA1_DISP) &&
	    (handle->scenario != DDP_SCENARIO_SUB_DISP)) {
		DDPERR("dpmgr_insert_ovl1_sub error, handle->scenario=%s\n",
		       ddp_get_scenario_name(handle->scenario));
		return -1;
	}

	if (ddp_is_module_in_scenario(handle->scenario, DISP_MODULE_OVL1) == 1)
		return -1;
	DDPMSG("dpmgr_insert_ovl1_sub\n");
	/*update contxt */
	context = _get_context();
	context->module_usage_table[DISP_MODULE_OVL1]++;
	context->module_path_table[DISP_MODULE_OVL1] = handle;

	/* update connected */
	ddp_connect_path(DDP_SCENARIO_SUB_DISP, cmdq_handle);
	ddp_mutex_set(handle->hwmutexid, DDP_SCENARIO_SUB_DISP, handle->mode,
		      cmdq_handle);

	handle->scenario = DDP_SCENARIO_SUB_DISP;

	/* ovl1 just need start. */
	if (ddp_modules_driver[DISP_MODULE_OVL1] != 0) {
		if (ddp_modules_driver[DISP_MODULE_OVL1]->init != 0)
			ddp_modules_driver[DISP_MODULE_OVL1]->init(
				DISP_MODULE_OVL1, cmdq_handle);

		if (ddp_modules_driver[DISP_MODULE_OVL1]->start != 0)
			ddp_modules_driver[DISP_MODULE_OVL1]->start(
				DISP_MODULE_OVL1, cmdq_handle);
	}

	ovl_set_status(DDP_OVL1_STATUS_SUB);

	return 0;
}

int dpmgr_remove_ovl1_sub(void *dp_handle, void *cmdq_handle)
{

	struct DDP_MANAGER_CONTEXT *context;
	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;

	ASSERT(dp_handle != NULL);
	if ((handle->scenario != DDP_SCENARIO_SUB_RDMA1_DISP) &&
	    (handle->scenario != DDP_SCENARIO_SUB_DISP)) {
		DDPERR("dpmgr_remove_ovl1_sub error, handle->scenario=%s\n",
		       ddp_get_scenario_name(handle->scenario));
		return -1;
	}

	if (ddp_is_module_in_scenario(handle->scenario, DISP_MODULE_OVL1) == 0)
		return -1;
	DDPMSG("dpmgr_remove_ovl1_sub\n");
	/* update contxt */
	context = _get_context();
	context->module_usage_table[DISP_MODULE_OVL1]--;
	context->module_path_table[DISP_MODULE_OVL1] = 0;

	ddp_disconnect_path(handle->scenario, cmdq_handle);
	ddp_connect_path(DDP_SCENARIO_SUB_RDMA1_DISP, cmdq_handle);
	ddp_mutex_set(handle->hwmutexid, DDP_SCENARIO_SUB_RDMA1_DISP,
		      handle->mode, cmdq_handle);

	handle->scenario = DDP_SCENARIO_SUB_RDMA1_DISP;

	/* ovl1 just need stop */
	if (ddp_modules_driver[DISP_MODULE_OVL1] != 0) {
		if (ddp_modules_driver[DISP_MODULE_OVL1]->stop != 0)
			ddp_modules_driver[DISP_MODULE_OVL1]->stop(
				DISP_MODULE_OVL1, cmdq_handle);

		if (ddp_modules_driver[DISP_MODULE_OVL1]->deinit != 0)
			ddp_modules_driver[DISP_MODULE_OVL1]->deinit(
				DISP_MODULE_OVL1, cmdq_handle);
	}

	return 0;
}

void *g_dp_handle;
void *g_cmdq_handle;
int dpmgr_path_get_handle(unsigned long *dp_handle, unsigned long *cmdq_handle)
{
	*dp_handle = (unsigned long)g_dp_handle;
	*cmdq_handle = (unsigned long)g_cmdq_handle;
	return 0;
}

int dpmgr_path_enable_cascade(void *dp_handle, void *cmdq_handle)
{

	struct DDP_MANAGER_CONTEXT *context;
	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;

	ASSERT(dp_handle != NULL);

	/* update handle */
	{
		g_dp_handle = dp_handle;
		g_cmdq_handle = cmdq_handle;
	}

	if ((handle->scenario != DDP_SCENARIO_PRIMARY_DISP) &&
	    (handle->scenario != DDP_SCENARIO_PRIMARY_ALL) &&
	    (handle->scenario != DDP_SCENARIO_PRIMARY_DITHER_MEMOUT) &&
	    (handle->scenario != DDP_SCENARIO_PRIMARY_OVL_MEMOUT)) {
		DDPERR("dpmgr_path_enable_cascade error, handle->scenario=%s\n",
		       ddp_get_scenario_name(handle->scenario));
		/* ASSERT(0); */
		return -1;
	}

	if (ddp_is_module_in_scenario(handle->scenario, DISP_MODULE_OVL1) ==
	    1) {
		/* ASSERT(0); */
		ovl_set_status(DDP_OVL1_STATUS_PRIMARY);
		return -1;
	}
	DDPMSG("dpmgr_path_enable_cascade %s\n",
	       ddp_get_scenario_name(handle->scenario));
	/* update contxt */
	context = _get_context();
	context->module_usage_table[DISP_MODULE_OVL1]++;
	context->module_path_table[DISP_MODULE_OVL1] = handle;

	/* update connected */
	/*
	 * ddp_insert_module(handle->scenario, DISP_MODULE_OVL0,
	 * DISP_MODULE_OVL1);
	 */
	ddp_insert_module(DDP_SCENARIO_PRIMARY_DISP, DISP_MODULE_OVL0,
			  DISP_MODULE_OVL1);
	ddp_insert_module(DDP_SCENARIO_PRIMARY_ALL, DISP_MODULE_OVL0,
			  DISP_MODULE_OVL1);
	ddp_insert_module(DDP_SCENARIO_PRIMARY_OVL_MEMOUT, DISP_MODULE_OVL0,
			  DISP_MODULE_OVL1);
	ddp_insert_module(DDP_SCENARIO_PRIMARY_DITHER_MEMOUT, DISP_MODULE_OVL0,
			  DISP_MODULE_OVL1);
	ddp_connect_path(DDP_SCENARIO_MULTIPLE_OVL, cmdq_handle);
	ddp_connect_path(handle->scenario, cmdq_handle);

	ddp_mutex_add_module(handle->hwmutexid, DISP_MODULE_OVL1, cmdq_handle);

	/* ovl1 just need start. */
	if (ddp_modules_driver[DISP_MODULE_OVL1] != 0) {
		if (ddp_modules_driver[DISP_MODULE_OVL1]->cmd != 0) {
			ddp_modules_driver[DISP_MODULE_OVL1]->cmd(
				DISP_MODULE_OVL1, DISP_IOCTL_OVL_ENABLE_CASCADE,
				0, cmdq_handle);
		}
		if (ddp_modules_driver[DISP_MODULE_OVL1]->init != 0)
			ddp_modules_driver[DISP_MODULE_OVL1]->init(
				DISP_MODULE_OVL1, cmdq_handle);

		if (ddp_modules_driver[DISP_MODULE_OVL1]->start != 0)
			ddp_modules_driver[DISP_MODULE_OVL1]->start(
				DISP_MODULE_OVL1, cmdq_handle);
	}

	if (ovl_get_status() != DDP_OVL1_STATUS_PRIMARY &&
	    ovl_get_status() != DDP_OVL1_STATUS_SUB_REQUESTING) {
		/*
		 * from primary-with-OVL1 to primary_all-with-OVL1 should not
		 * rebuild trigger loop cmdq
		 */
		ovl_set_status(DDP_OVL1_STATUS_PRIMARY);
	}

	mmprofile_log_ex(ddp_mmp_get_events()->cascade_enable,
			 MMPROFILE_FLAG_PULSE, 0, 0);

	return 0;
}

int dpmgr_path_disable_cascade(void *dp_handle, void *cmdq_handle)
{

	struct DDP_MANAGER_CONTEXT *context;
	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;

	ASSERT(dp_handle != NULL);
	if ((handle->scenario != DDP_SCENARIO_PRIMARY_DISP) &&
	    (handle->scenario != DDP_SCENARIO_PRIMARY_ALL) &&
	    (handle->scenario != DDP_SCENARIO_PRIMARY_DITHER_MEMOUT) &&
	    (handle->scenario != DDP_SCENARIO_PRIMARY_OVL_MEMOUT)) {
		DDPERR(
			"dpmgr_path_disable_cascade error, handle->scenario=%s\n",
		       ddp_get_scenario_name(handle->scenario));
		/* ASSERT(0); */
		return -1;
	}

	if (ddp_is_module_in_scenario(handle->scenario, DISP_MODULE_OVL1) ==
	    0) {
		/* ASSERT(0); */
		return -1;
	}
	DDPMSG("dpmgr_path_disable_cascade %s\n",
	       ddp_get_scenario_name(handle->scenario));
	/* update contxt */
	context = _get_context();
	context->module_usage_table[DISP_MODULE_OVL1]--;
	context->module_path_table[DISP_MODULE_OVL1] = 0;

	ddp_disconnect_path(DDP_SCENARIO_MULTIPLE_OVL, cmdq_handle);
	/* ddp_remove_module(handle->scenario,  DISP_MODULE_OVL1); */
	ddp_remove_module(DDP_SCENARIO_PRIMARY_DISP, DISP_MODULE_OVL1);
	ddp_remove_module(DDP_SCENARIO_PRIMARY_ALL, DISP_MODULE_OVL1);
	ddp_remove_module(DDP_SCENARIO_PRIMARY_OVL_MEMOUT, DISP_MODULE_OVL1);
	ddp_remove_module(DDP_SCENARIO_PRIMARY_DITHER_MEMOUT, DISP_MODULE_OVL1);
	ddp_mutex_remove_module(handle->hwmutexid, DISP_MODULE_OVL1,
				cmdq_handle);
	/* ovl1 just need stop */
	if (ddp_modules_driver[DISP_MODULE_OVL1] != 0) {
		if (ddp_modules_driver[DISP_MODULE_OVL1]->cmd != 0) {
			ddp_modules_driver[DISP_MODULE_OVL1]->cmd(
				DISP_MODULE_OVL1,
				DISP_IOCTL_OVL_DISABLE_CASCADE, 0, cmdq_handle);
		}
		if (ddp_modules_driver[DISP_MODULE_OVL1]->stop != 0)
			ddp_modules_driver[DISP_MODULE_OVL1]->stop(
				DISP_MODULE_OVL1, cmdq_handle);

		if (ddp_modules_driver[DISP_MODULE_OVL1]->deinit != 0)
			ddp_modules_driver[DISP_MODULE_OVL1]->deinit(
				DISP_MODULE_OVL1, cmdq_handle);
	}

	mmprofile_log_ex(ddp_mmp_get_events()->cascade_disable,
			 MMPROFILE_FLAG_PULSE, 0, 0);
	return 0;
}

int dpmgr_path_release_8_layer_fence(void)
{
	unsigned int i = 0;

	for (i = OVL_LAYER_NUM_PER_OVL; i < OVL_LAYER_NUM; i++)
		mtkfb_release_layer_fence(
			MAKE_DISP_SESSION(DISP_SESSION_PRIMARY, 0), i);
	return 0;
}

int dpmgr_path_set_dst_module(void *dp_handle, enum DISP_MODULE_ENUM dst_module)
{

	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;

	ASSERT(dp_handle != NULL);
	ASSERT((handle->scenario >= 0 && handle->scenario < DDP_SCENARIO_MAX));
	DISP_LOG_I("set dst module on scenario %s, module %s\n",
		   ddp_get_scenario_name(handle->scenario),
		   ddp_get_module_name(dst_module));
	return ddp_set_dst_module(handle->scenario, dst_module);
}

int dpmgr_path_get_mutex(void *dp_handle)
{
	struct ddp_path_handle_t *handle = NULL;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	return handle->hwmutexid;
}

enum DISP_MODULE_ENUM dpmgr_path_get_dst_module(void *dp_handle)
{
	enum DISP_MODULE_ENUM dst_module;
	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;

	ASSERT(dp_handle != NULL);
	ASSERT((handle->scenario >= 0 && handle->scenario < DDP_SCENARIO_MAX));
	dst_module = ddp_get_dst_module(handle->scenario);
	DISP_LOG_V("get dst module on scenario %s, module %s\n",
		   ddp_get_scenario_name(handle->scenario),
		   ddp_get_module_name(dst_module));
	return dst_module;
}

int dpmgr_path_connect(void *dp_handle, int encmdq)
{

	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;
	struct cmdqRecStruct *cmdqHandle;

	ASSERT(dp_handle != NULL);
	cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
	DISP_LOG_I("dpmgr_path_connect on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));

	ddp_connect_path(handle->scenario, cmdqHandle);
	ddp_mutex_set(handle->hwmutexid, handle->scenario, handle->mode,
		      cmdqHandle);
	ddp_mutex_Interrupt_enable(handle->hwmutexid, cmdqHandle);
	return 0;
}

int dpmgr_path_disconnect(void *dp_handle, int encmdq)
{

	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;
	struct cmdqRecStruct *cmdqHandle;

	ASSERT(dp_handle != NULL);
	cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
	DISP_LOG_I("dpmgr_path_disconnect on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));
	ddp_mutex_clear(handle->hwmutexid, cmdqHandle);
	ddp_mutex_Interrupt_disable(handle->hwmutexid, cmdqHandle);
	ddp_disconnect_path(handle->scenario, cmdqHandle);
	return 0;
}

int dpmgr_path_init(void *dp_handle, int encmdq)
{
	int i = 0;
	int module_name;
	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;
	int *modules;
	int module_num;
	struct cmdqRecStruct *cmdqHandle;

	ASSERT(dp_handle != NULL);
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
	DISP_LOG_V("path init on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));
	/* open top clock */
	path_top_clock_on();
	/* seting mutex */
	ddp_mutex_set(handle->hwmutexid, handle->scenario, handle->mode,
		      cmdqHandle);
	ddp_mutex_Interrupt_enable(handle->hwmutexid, cmdqHandle);
	/* connect path; */
	ddp_connect_path(handle->scenario, cmdqHandle);

	/* each module init */
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		if (ddp_modules_driver[module_name] != 0) {
			if (ddp_modules_driver[module_name]->init != 0) {
				DISP_LOG_V(
					"scenario %s init module  %s\n",
					ddp_get_scenario_name(handle->scenario),
					ddp_get_module_name(module_name));
				ddp_modules_driver[module_name]->init(
					module_name, cmdqHandle);
			}
			if (ddp_modules_driver[module_name]->set_listener !=
			    0) {
				ddp_modules_driver[module_name]->set_listener(
					module_name, dpmgr_module_notify);
			}
		}
	}
	/* after init this path will power on; */
	handle->power_sate = 1;
	return 0;
}

int dpmgr_path_deinit(void *dp_handle, int encmdq)
{
	int i = 0;
	int module_name;
	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;
	int *modules;
	int module_num;
	struct cmdqRecStruct *cmdqHandle;

	ASSERT(dp_handle != NULL);
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
	DISP_LOG_V("path deinit on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));
	ddp_mutex_Interrupt_disable(handle->hwmutexid, cmdqHandle);
	ddp_mutex_clear(handle->hwmutexid, cmdqHandle);
	ddp_disconnect_path(handle->scenario, cmdqHandle);
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		if (ddp_modules_driver[module_name] != 0) {
			if (ddp_modules_driver[module_name]->deinit != 0) {
				DISP_LOG_I(
					"scenario %s deinit module  %s\n",
					ddp_get_scenario_name(handle->scenario),
					ddp_get_module_name(module_name));
				ddp_modules_driver[module_name]->deinit(
					module_name, cmdqHandle);
			}
			if (ddp_modules_driver[module_name]->set_listener != 0)
				ddp_modules_driver[module_name]->set_listener(
					module_name, NULL);
		}
	}
	handle->power_sate = 0;
	/* close top clock when last path init */
	path_top_clock_off();
	return 0;
}

int dpmgr_path_start(void *dp_handle, int encmdq)
{
	int i = 0;
	int module_name;
	struct ddp_path_handle_t *handle;
	int *modules;
	int module_num;
	struct cmdqRecStruct *cmdqHandle;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
	DISP_LOG_V("path start on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		if (ddp_modules_driver[module_name] != 0) {
			if (ddp_modules_driver[module_name]->start != 0) {
				DISP_LOG_V(
					"scenario %s start module  %s\n",
					ddp_get_scenario_name(handle->scenario),
					ddp_get_module_name(module_name));
				ddp_modules_driver[module_name]->start(
					module_name, cmdqHandle);
			}
		}
	}
	return 0;
}

int dpmgr_path_stop(void *dp_handle, int encmdq)
{

	int i = 0;
	int module_name;
	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;
	int *modules;
	int module_num;
	struct cmdqRecStruct *cmdqHandle;

	ASSERT(dp_handle != NULL);
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
	DISP_LOG_I("path stop on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));
	for (i = module_num - 1; i >= 0; i--) {
		module_name = modules[i];
		if (ddp_modules_driver[module_name] != 0) {
			if (ddp_modules_driver[module_name]->stop != 0) {
				DISP_LOG_V(
					"scenario %s  stop module %s\n",
					ddp_get_scenario_name(handle->scenario),
					ddp_get_module_name(module_name));
				ddp_modules_driver[module_name]->stop(
					module_name, cmdqHandle);
			}
		}
	}
	return 0;
}

int dpmgr_path_ioctl(void *dp_handle, void *cmdq_handle,
		     enum DDP_IOCTL_NAME ioctl_cmd, unsigned long *params)
{
	int i = 0;
	int ret = 0;
	int module_name;
	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;
	int *modules;
	int module_num;

	ASSERT(dp_handle != NULL);
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	DISP_LOG_I("path IOCTL on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));
	for (i = module_num - 1; i >= 0; i--) {
		module_name = modules[i];
		if (ddp_modules_driver[module_name] != 0) {
			if (ddp_modules_driver[module_name]->ioctl != 0) {
				pr_debug(
					"scenario %s, module %s ioctl\n",
					ddp_get_scenario_name(handle->scenario),
					ddp_get_module_name(module_name));
				ret += ddp_modules_driver[module_name]->ioctl(
					module_name, cmdq_handle,
					(unsigned int)ioctl_cmd, params);
			}
		}
	}
	return ret;
}

int dpmgr_path_reset(void *dp_handle, int encmdq)
{
	int i = 0;
	int ret = 0;
	int error = 0;
	int module_name;
	int *modules;
	int module_num;
	struct ddp_path_handle_t *handle;
	struct cmdqRecStruct *cmdqHandle;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	cmdqHandle = encmdq ? handle->cmdqhandle : NULL;

	DISP_LOG_I("path reset on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));
	/* first reset mutex */
	ddp_mutex_reset(handle->hwmutexid, cmdqHandle);

	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		if (ddp_modules_driver[module_name] != 0) {
			if (ddp_modules_driver[module_name]->reset != 0) {
				DISP_LOG_I(
					"scenario %s  reset module %s\n",
					ddp_get_scenario_name(handle->scenario),
					ddp_get_module_name(module_name));
				ret = ddp_modules_driver[module_name]->reset(
					module_name, cmdqHandle);
				if (ret != 0)
					error++;
			}
		}
	}
	return error > 0 ? -1 : 0;
}

static int dpmgr_layer_num(struct disp_ddp_path_config *config)
{
	int i = 0;
	int max_layer = 0;

	for (i = 0; i < OVL_LAYER_NUM; i++) {
		if (config->ovl_config[i].layer_en != 0) {

			if (config->ovl_config[i].layer > max_layer)
				max_layer = config->ovl_config[i].layer;
		}
	}
	return max_layer;
}

static unsigned int dpmgr_is_PQ(enum DISP_MODULE_ENUM module)
{
	unsigned int isPQ = 0;

	switch (module) {
	case DISP_MODULE_COLOR0:
	case DISP_MODULE_CCORR:
	case DISP_MODULE_AAL:
	case DISP_MODULE_GAMMA:
	case DISP_MODULE_DITHER:
		/* case DISP_MODULE_UFOE  : */
		/* case DISP_MODULE_PWM0  : */
		isPQ = 1;
		break;
	default:
		isPQ = 0;
	}

	return isPQ;
}

int dpmgr_path_config(void *dp_handle, struct disp_ddp_path_config *config,
		      void *cmdq_handle)
{
	int i = 0;
	int ret = 0;
	int module_name;
	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;
	int *modules;
	int module_num;

	ASSERT(dp_handle != NULL);
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	memcpy(&handle->last_config, config,
	       sizeof(struct disp_ddp_path_config));

/* switch between single and dual ovl mode */
#ifdef OVL_CASCADE_SUPPORT
	if ((config->ovl_dirty == 1) &&
	    (ovl_get_status() == DDP_OVL1_STATUS_IDLE) &&
	    dpmgr_layer_num(config) >= 4) {
		/*
		 * DDPMSG("cascade switch, call dpmgr_path_enable_cascade() in
		 * dpmgr_path_config()");
		 */
		dpmgr_path_enable_cascade(dp_handle, cmdq_handle);
		module_num = ddp_get_module_num(handle->scenario);
	} else if ((config->ovl_dirty == 1) &&
		   (ovl_get_status() == DDP_OVL1_STATUS_SUB_REQUESTING)) {
		int enable_layers = dpmgr_layer_num(config);

		if (enable_layers <= 4) {
			DDPMSG(
				"cascade switch, call dpmgr_path_disable_cascade() in dpmgr_path_config() %d\n",
			       enable_layers);
			dpmgr_path_disable_cascade(dp_handle, cmdq_handle);
			module_num = ddp_get_module_num(handle->scenario);
			ovl_set_status(DDP_OVL1_STATUS_PRIMARY_RELEASING);
		} else {
			DDPMSG(
				"cascade: in path_config(), split ovl1 fail, enable_layers=%d\n",
			       enable_layers);
		}
	}
#endif

	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		if (ddp_modules_driver[module_name] != 0) {
			if (gDDPError == 1 && dpmgr_is_PQ(module_name) == 1) {
				if (ddp_modules_driver[module_name]->bypass ==
				    NULL) {
					DDPERR(
						"ddp_modules_driver[module_name]->bypass==NULL, module=%s\n",
					       ddp_get_module_name(
						       module_name));
				} else {
					ddp_modules_driver[module_name]->bypass(
						module_name, 1);
					/*
					 * DDPMSG("-- bypass %s\n",
					 * ddp_get_module_name(module_name));
					 */
				}
			} else if (ddp_modules_driver[module_name]->config !=
				   0) {
				DISP_LOG_V(
					"scenario %s  config module %s\n",
					ddp_get_scenario_name(handle->scenario),
					ddp_get_module_name(module_name));
				ddp_modules_driver[module_name]->config(
					module_name, config, cmdq_handle);
			}
		}
	}

#ifdef OVL_CASCADE_SUPPORT
	if (ovl_get_status() == DDP_OVL1_STATUS_PRIMARY_RELEASING)
		ovl_set_status(DDP_OVL1_STATUS_PRIMARY_RELEASED);

	if (config->ovl_dirty == 1 && dpmgr_layer_num(config) == 0 &&
	    disp_get_session_number() > 1 &&
	    ovl_get_status() != DDP_OVL1_STATUS_SUB &&
	    ovl_get_status() != DDP_OVL1_STATUS_IDLE) {
		/* all 8 layer disabled */
		DDPMSG("disable cascade if 8 layer all disabled!\n");
		allLayerDisabled = 1;
		ret = dpmgr_path_disable_cascade(dp_handle, cmdq_handle);
		allLayerDisabled = 0;
		ALL_LAYER_DISABLE_STEP = 1;

		if (ret == 0 && ovl_get_status() == DDP_OVL1_STATUS_PRIMARY)
			ovl_set_status(DDP_OVL1_STATUS_PRIMARY_DISABLE);
	}
#endif

	return 0;
}

struct disp_ddp_path_config *dpmgr_path_get_last_config(void *dp_handle)
{
	struct ddp_path_handle_t *handle;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	handle->last_config.ovl_dirty = 0;
	handle->last_config.rdma_dirty = 0;
	handle->last_config.wdma_dirty = 0;
	handle->last_config.dst_dirty = 0;
	return &handle->last_config;
}

void dpmgr_get_input_address(void *dp_handle, unsigned long *addr)
{
	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;
	int *modules = ddp_get_scenario_list(handle->scenario);

	if (modules[0] == DISP_MODULE_OVL0 || modules[0] == DISP_MODULE_OVL1) {
		ovl_get_address(modules[0], addr);
	} else if (modules[0] == DISP_MODULE_RDMA0 ||
		   modules[0] == DISP_MODULE_RDMA1 ||
		   modules[0] == DISP_MODULE_RDMA2) {
		rdma_get_address(modules[0], addr);
	}
}

int dpmgr_path_build_cmdq(void *dp_handle, void *trigger_loop_handle,
			  enum CMDQ_STATE state)
{
	int ret = 0;
	int i = 0;
	int module_name;
	int *modules;
	int module_num;
	struct ddp_path_handle_t *handle =
		(struct ddp_path_handle_t *)dp_handle;

	ASSERT(dp_handle != NULL);
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);

	DISP_LOG_D("path build cmdq on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		if (ddp_modules_driver[module_name] != 0) {
			if (ddp_modules_driver[module_name]->build_cmdq != 0) {
				DISP_LOG_D("%s build cmdq, state=%d\n",
					   ddp_get_module_name(module_name),
					   state);
				ret = ddp_modules_driver[module_name]
					      ->build_cmdq(module_name,
							   trigger_loop_handle,
							   state);
			}
		}
	}
	return ret;
}

int dpmgr_path_trigger(void *dp_handle, void *trigger_loop_handle, int encmdq)
{
	int i = 0;
	struct ddp_path_handle_t *handle;
	int *modules;
	int module_num;
	int module_name;

	handle = (struct ddp_path_handle_t *)dp_handle;
	ASSERT(dp_handle != NULL);
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	DISP_LOG_V("dpmgr_path_trigger on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));
	ddp_mutex_enable(handle->hwmutexid, handle->scenario,
			 trigger_loop_handle);
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		if (ddp_modules_driver[module_name] != 0) {
			if (ddp_modules_driver[module_name]->trigger != 0) {
				DISP_LOG_V("%s trigger\n",
					   ddp_get_module_name(module_name));
				ddp_modules_driver[module_name]->trigger(
					module_name, trigger_loop_handle);
			}
		}
	}
	return 0;
}

int dpmgr_path_flush(void *dp_handle, int encmdq)
{
	struct ddp_path_handle_t *handle;
	struct cmdqRecStruct *cmdqHandle;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	cmdqHandle = encmdq ? handle->cmdqhandle : NULL;
	DISP_LOG_I("path flush on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));
	ovl_reset(DISP_MODULE_OVL0, cmdqHandle);
	return ddp_mutex_enable(handle->hwmutexid, handle->scenario,
				cmdqHandle);
}

int dpmgr_path_power_off(void *dp_handle, enum CMDQ_SWITCH encmdq)
{
	int i = 0;
	int module_name;
	struct ddp_path_handle_t *handle;
	int *modules;
	int module_num;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	DISP_LOG_I("path power off on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		if (ddp_modules_driver[module_name] != 0) {
			if (ddp_modules_driver[module_name]->power_off != 0) {
				DISP_LOG_V(" %s power off\n",
					   ddp_get_module_name(module_name));
				ddp_modules_driver[module_name]->power_off(
					module_name,
					encmdq ? handle->cmdqhandle : NULL);
			}
		}
	}
	handle->power_sate = 0;
	path_top_clock_off();
	return 0;
}

int dpmgr_path_power_on(void *dp_handle, enum CMDQ_SWITCH encmdq)
{
	int i = 0;
	int module_name;
	struct ddp_path_handle_t *handle;
	int *modules;
	int module_num;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	DISP_LOG_I("path power on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));
	path_top_clock_on();
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		if (ddp_modules_driver[module_name] != 0) {
			if (ddp_modules_driver[module_name]->power_on != 0) {
				DISP_LOG_V("%s power on\n",
					   ddp_get_module_name(module_name));
				ddp_modules_driver[module_name]->power_on(
					module_name,
					encmdq ? handle->cmdqhandle : NULL);
			}
		}
	}
	/* modules on this path will resume power on; */
	handle->power_sate = 1;
	return 0;
}

int dpmgr_path_dsi_off(void *dp_handle, void *cmdq_handle, unsigned int level)
{
	int module_name;
	struct ddp_path_handle_t *handle;
	enum DISP_MODULE_ENUM dst_module;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	dst_module = ddp_get_dst_module(handle->scenario);
	module_name = dst_module;
	/*
	 * printk("ddp_idle dsi_off on scenario
	 * %s\n",ddp_get_scenario_name(handle->scenario));
	 */

	if ((module_name >= 0) && (module_name < DISP_MODULE_UNKNOWN)) {
		ddp_modules_driver[module_name]->ioctl(module_name, cmdq_handle,
						       DDP_DSI_IDLE_CLK_CLOSED,
						       (unsigned long *)&level);
	} else {
		DISP_LOG_E("dpmgr_path_dsi_off unknown module %d", module_name);
	}
	ddp_path_lp_top_clock_off();
	return 0;
}

int dpmgr_path_dsi_on(void *dp_handle, void *cmdq_handle, unsigned int level)
{

	int module_name;
	enum DISP_MODULE_ENUM dst_module;
	struct ddp_path_handle_t *handle;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	dst_module = ddp_get_dst_module(handle->scenario);
	module_name = dst_module;
	/*
	 * printk("ddp_idle dsi_on scenario
	 * %s\n",ddp_get_scenario_name(handle->scenario));
	 */
	ddp_path_lp_top_clock_on();
	if (module_name >= 0 && module_name < DISP_MODULE_UNKNOWN) {
		ddp_modules_driver[module_name]->ioctl(module_name, cmdq_handle,
						       DDP_DSI_IDLE_CLK_OPEN,
						       (unsigned long *)&level);
	} else {
		DISP_LOG_E("dpmgr_path_dsi_on unknown module %d", module_name);
	}
	return 0;
}

int dpmgr_path_idle_off(void *dp_handle, void *cmdq_handle, unsigned int level)
{
	int i = 0;
	int module_name;
	struct ddp_path_handle_t *handle;
	int *modules;
	int module_num;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	DISP_LOG_I("ddp_idle off on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));

	/*
	 * For DSI CMD mode, do not clock pwm_26M, used to make sure MM clock
	 * off but MTCMOS on
	 */
	if (primary_display_is_video_mode() == 0)
		ddp_module_clock_enable(MM_CLK_DISP_PWM_26M, true);

	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		if (ddp_modules_driver[module_name] != 0) {
			if (ddp_modules_driver[module_name]->power_off != 0) {
				DISP_LOG_V(" %s power off\n",
					   ddp_get_module_name(module_name));
				if (!((module_name == DISP_MODULE_DSIDUAL) ||
				      (module_name == DISP_MODULE_DSI0) ||
				      (module_name == DISP_MODULE_DSI1)))
					ddp_modules_driver[module_name]
						->power_off(module_name,
							    cmdq_handle);
				else
					ddp_modules_driver[module_name]->ioctl(
						module_name, cmdq_handle,
						DDP_DSI_IDLE_CLK_CLOSED,
						(unsigned long *)&level);
			}
		}
	}
	/* dpmgr_path_dsi_off(dp_handle,cmdq_handle,level); */
	handle->power_sate = 0;

	path_top_clock_off();
	return 0;
}

int dpmgr_path_idle_on(void *dp_handle, void *cmdq_handle, unsigned int level)
{
	int i = 0;
	int module_name;
	struct ddp_path_handle_t *handle;
	int *modules;
	int module_num;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	DISP_LOG_I("ddp_idle on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));
	path_top_clock_on();
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		if (ddp_modules_driver[module_name] != 0) {
			if (ddp_modules_driver[module_name]->power_on != 0) {
				DISP_LOG_V("%s power on\n",
					   ddp_get_module_name(module_name));
				if (!((module_name == DISP_MODULE_DSIDUAL) ||
				      (module_name == DISP_MODULE_DSI0) ||
				      (module_name == DISP_MODULE_DSI1)))
					ddp_modules_driver[module_name]
						->power_on(module_name,
							   cmdq_handle);
				else
					ddp_modules_driver[module_name]->ioctl(
						module_name, cmdq_handle,
						DDP_DSI_IDLE_CLK_OPEN,
						(unsigned long *)&level);
			}
		}
	}

	/*
	 * clock on PEM_26M one more time when idle off, so need clock off it
	 * here
	 */
	if (primary_display_is_video_mode() == 0)
		ddp_module_clock_enable(MM_CLK_DISP_PWM_26M, false);

	handle->power_sate = 1;
	return 0;
}

static int is_module_in_path(enum DISP_MODULE_ENUM module,
			     struct ddp_path_handle_t *handle)
{
	struct DDP_MANAGER_CONTEXT *context = _get_context();

	ASSERT(module < DISP_MODULE_UNKNOWN);
	if (context->module_path_table[module] == handle)
		return 1;
	return 0;
}

int dpmgr_path_user_cmd(void *dp_handle, int msg, unsigned long arg,
			void *cmdqhandle)
{
	int ret = -1;
	enum DISP_MODULE_ENUM dst = DISP_MODULE_UNKNOWN;
	struct ddp_path_handle_t *handle = NULL;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	/* DISP_LOG_W("dpmgr_path_user_cmd msg 0x%08x\n",msg); */
	switch (msg) {
	case DISP_IOCTL_AAL_EVENTCTL:
	case DISP_IOCTL_AAL_GET_HIST:
	case DISP_IOCTL_AAL_INIT_REG:
	case DISP_IOCTL_AAL_SET_PARAM: {
		if (is_module_in_path(DISP_MODULE_AAL, handle)) {
			if (ddp_modules_driver[DISP_MODULE_AAL]->cmd != NULL) {
				ret = ddp_modules_driver[DISP_MODULE_AAL]->cmd(
					DISP_MODULE_AAL, msg, arg, cmdqhandle);
			}
		}
		break;
	}
	case DISP_IOCTL_SET_GAMMALUT:
		if (ddp_modules_driver[DISP_MODULE_GAMMA]->cmd != NULL) {
			ret = ddp_modules_driver[DISP_MODULE_GAMMA]->cmd(
				DISP_MODULE_GAMMA, msg, arg, cmdqhandle);
		}
		break;
	case DISP_IOCTL_SET_CCORR:
		if (ddp_modules_driver[DISP_MODULE_CCORR]->cmd != NULL) {
			ret = ddp_modules_driver[DISP_MODULE_CCORR]->cmd(
				DISP_MODULE_CCORR, msg, arg, cmdqhandle);
		}
		break;

	case DISP_IOCTL_SET_PQPARAM:
	case DISP_IOCTL_GET_PQPARAM:
	case DISP_IOCTL_SET_PQINDEX:
	case DISP_IOCTL_GET_PQINDEX:
	case DISP_IOCTL_SET_COLOR_REG:
	case DISP_IOCTL_SET_TDSHPINDEX:
	case DISP_IOCTL_GET_TDSHPINDEX:
	case DISP_IOCTL_SET_PQ_CAM_PARAM:
	case DISP_IOCTL_GET_PQ_CAM_PARAM:
	case DISP_IOCTL_SET_PQ_GAL_PARAM:
	case DISP_IOCTL_GET_PQ_GAL_PARAM:
	case DISP_IOCTL_PQ_SET_BYPASS_COLOR:
	case DISP_IOCTL_PQ_SET_WINDOW:
	case DISP_IOCTL_WRITE_REG:
	case DISP_IOCTL_READ_REG:
	case DISP_IOCTL_MUTEX_CONTROL:
	case DISP_IOCTL_PQ_GET_TDSHP_FLAG:
	case DISP_IOCTL_PQ_SET_TDSHP_FLAG:
	case DISP_IOCTL_PQ_GET_DC_PARAM:
	case DISP_IOCTL_PQ_SET_DC_PARAM:
	case DISP_IOCTL_WRITE_SW_REG:
	case DISP_IOCTL_READ_SW_REG: {
		if (is_module_in_path(DISP_MODULE_COLOR0, handle))
			dst = DISP_MODULE_COLOR0;
		else if (is_module_in_path(DISP_MODULE_COLOR1, handle))
			dst = DISP_MODULE_COLOR1;
		else
			DISP_LOG_W(
				"dpmgr_path_user_cmd color is not on this path\n"
				);

		if (dst != DISP_MODULE_UNKNOWN) {

			if (ddp_modules_driver[dst]->cmd != NULL) {
				ret = ddp_modules_driver[dst]->cmd(
					dst, msg, arg, cmdqhandle);
			}
		}
		break;
	}

	default: {
		DISP_LOG_W("dpmgr_path_user_cmd io not supported\n");
		break;
	}
	}
	return ret;
}

int dpmgr_path_set_parameter(void *dp_handle, int io_evnet, void *data)
{
	return 0;
}

int dpmgr_path_get_parameter(void *dp_handle, int io_evnet, void *data)
{
	return 0;
}

int dpmgr_path_is_idle(void *dp_handle)
{
	ASSERT(dp_handle != NULL);
	return !dpmgr_path_is_busy(dp_handle);
}

int dpmgr_path_is_busy(void *dp_handle)
{
	int i = 0;
	int module_name;
	struct ddp_path_handle_t *handle;
	int *modules;
	int module_num;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	DISP_LOG_V("path check busy on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));
	for (i = module_num - 1; i >= 0; i--) {
		module_name = modules[i];
		if (ddp_modules_driver[module_name] != 0) {
			if (ddp_modules_driver[module_name]->is_busy != 0) {
				if (ddp_modules_driver[module_name]->is_busy(
					    module_name)) {
					DISP_LOG_V("%s is busy\n",
						   ddp_get_module_name(
							   module_name));
					return 1;
				}
			}
		}
	}
	return 0;
}

int dpmgr_set_lcm_utils(void *dp_handle, void *lcm_drv)
{
	int i = 0;
	struct ddp_path_handle_t *handle;
	int module_name;
	int module_num;
	int *modules;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);

	DISP_LOG_V("path set lcm drv handle %p\n", handle);
	for (i = 0; i < module_num; i++) {
		module_name = modules[i];
		if (ddp_modules_driver[module_name] != 0) {
			if ((ddp_modules_driver[module_name]->set_lcm_utils !=
			     0) &&
			    lcm_drv) {
				DISP_LOG_I("%s set lcm utils\n",
					   ddp_get_module_name(module_name));
				ddp_modules_driver[module_name]->set_lcm_utils(
					module_name, lcm_drv);
			}
		}
	}
	return 0;
}

int dpmgr_enable_event(void *dp_handle, enum DISP_PATH_EVENT event)
{
	struct ddp_path_handle_t *handle;
	struct DPMGR_WQ_HANDLE *wq_handle;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	wq_handle = &handle->wq_list[event];
	DISP_LOG_V("enable event %s on scenario %s, irtbit 0x%x\n",
		   path_event_name(event),
		   ddp_get_scenario_name(handle->scenario),
		   handle->irq_event_map[event].irq_bit);
	if (!wq_handle->init) {
		init_waitqueue_head(&(wq_handle->wq));
		wq_handle->init = 1;
		wq_handle->data = 0;
		wq_handle->event = event;
	}
	return 0;
}

int dpmgr_map_event_to_irq(void *dp_handle, enum DISP_PATH_EVENT event,
			   enum DDP_IRQ_BIT irq_bit)
{
	struct ddp_path_handle_t *handle;
	struct DDP_IRQ_EVENT_MAPPING *irq_table;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	irq_table = handle->irq_event_map;
	if (event < DISP_PATH_EVENT_NUM) {
		DISP_LOG_V("map event %s to irq 0x%x on scenario %s\n",
			   path_event_name(event), irq_bit,
			   ddp_get_scenario_name(handle->scenario));
		irq_table[event].irq_bit = irq_bit;
		return 0;
	}
	DISP_LOG_E("fail to map event %s to irq 0x%x on scenario %s\n",
		   path_event_name(event), irq_bit,
		   ddp_get_scenario_name(handle->scenario));
	return -1;
}

int dpmgr_disable_event(void *dp_handle, enum DISP_PATH_EVENT event)
{
	struct DPMGR_WQ_HANDLE *wq_handle;
	struct ddp_path_handle_t *handle;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	DISP_LOG_I("disable event %s on scenario %s\n", path_event_name(event),
		   ddp_get_scenario_name(handle->scenario));
	wq_handle = &handle->wq_list[event];
	wq_handle->init = 0;
	wq_handle->data = 0;
	return 0;
}

int dpmgr_check_status(void *dp_handle)
{
	int i = 0;
	struct ddp_path_handle_t *handle;
	int *modules;
	int module_num;

	if (dp_handle == NULL)
		return 0;

	handle = (struct ddp_path_handle_t *)dp_handle;
	modules = ddp_get_scenario_list(handle->scenario);
	module_num = ddp_get_module_num(handle->scenario);
	DISP_LOG_I("check status on scenario %s\n",
		   ddp_get_scenario_name(handle->scenario));

	ddp_dump_analysis(DISP_MODULE_CONFIG);
	ddp_check_path(handle->scenario);
	ddp_check_mutex(handle->hwmutexid, handle->scenario, handle->mode);

	/* dump path */
	{
		DDPMSG("path:");
		for (i = 0; i < module_num; i++)
			pr_debug("%s-", ddp_get_module_name(modules[i]));
		pr_debug("\n");
	}
	ddp_dump_analysis(DISP_MODULE_MUTEX);

	for (i = 0; i < module_num; i++)
		ddp_dump_analysis(modules[i]);

	for (i = 0; i < module_num; i++)
		ddp_dump_reg(modules[i]);

	ddp_dump_reg(DISP_MODULE_CONFIG);
	ddp_dump_reg(DISP_MODULE_MUTEX);

	return 0;
}

void dpmgr_debug_path_status(int mutex_id)
{
	int i = 0;
	struct DDP_MANAGER_CONTEXT *content = _get_context();
	void *handle = NULL;

	if (mutex_id >= DISP_MUTEX_DDP_FIRST &&
	    mutex_id < (DISP_MUTEX_DDP_FIRST + DISP_MUTEX_DDP_COUNT)) {
		handle = (void *)content->handle_pool[mutex_id];
		if (handle)
			dpmgr_check_status(handle);
	} else {
		for (i = DISP_MUTEX_DDP_FIRST;
		     i < (DISP_MUTEX_DDP_FIRST + DISP_MUTEX_DDP_COUNT); i++) {
			handle = (void *)content->handle_pool[i];
			if (handle)
				dpmgr_check_status(handle);
		}
	}
}

int dpmgr_wait_event_timeout(void *dp_handle, enum DISP_PATH_EVENT event,
			     int timeout)
{
	int ret = -1;
	struct ddp_path_handle_t *handle;
	struct DPMGR_WQ_HANDLE *wq_handle;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	wq_handle = &handle->wq_list[event];
	if (wq_handle->init) {
		unsigned long long cur_time;

		DISP_LOG_V("wait event %s on scenario %s\n",
			   path_event_name(event),
			   ddp_get_scenario_name(handle->scenario));
		cur_time = ktime_to_ns(ktime_get());
		ret = wait_event_interruptible_timeout(
			wq_handle->wq, cur_time < wq_handle->data, timeout);
		if (ret == 0) {
			DISP_LOG_E("wait %s timeout on scenario %s\n",
				   path_event_name(event),
				   ddp_get_scenario_name(handle->scenario));
			mmprofile_log_ex(
				ddp_mmp_get_events()->dpmgr_wait_event_timeout,
				MMPROFILE_FLAG_PULSE,
				(unsigned long int)ddp_get_scenario_name(
					handle->scenario),
				event);
			dpmgr_check_status(dp_handle);
			/* dpmgr_path_reset(dp_handle, 0); */
			/* gDDPError = 1; */
		} else if (ret < 0) {
			DISP_LOG_E(
				"wait %s interrupt by other timeleft %d on scenario %s\n",
				path_event_name(event), ret,
				ddp_get_scenario_name(handle->scenario));
		} else {
			DISP_LOG_V(
				"received event %s timeleft %d on scenario %s\n",
				path_event_name(event), ret,
				ddp_get_scenario_name(handle->scenario));
		}
		return ret;
	}
	DISP_LOG_E("wait event %s not initialized on scenario %s\n",
		   path_event_name(event),
		   ddp_get_scenario_name(handle->scenario));
	return ret;
}

int _dpmgr_wait_event(void *dp_handle, enum DISP_PATH_EVENT event,
		      unsigned long long *event_ts)
{
	int ret = -1;
	struct ddp_path_handle_t *handle;
	struct DPMGR_WQ_HANDLE *wq_handle;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	wq_handle = &handle->wq_list[event];
	if (wq_handle->init) {
		unsigned long long cur_time;

		DISP_LOG_V("wait event %s on scenario %s\n",
			   path_event_name(event),
			   ddp_get_scenario_name(handle->scenario));
		cur_time = ktime_to_ns(ktime_get());
		ret = wait_event_interruptible(wq_handle->wq,
					       cur_time < wq_handle->data);
		if (ret < 0) {
			DISP_LOG_E(
				"wait %s interrupt by other ret %d on scenario %s\n",
				path_event_name(event), ret,
				ddp_get_scenario_name(handle->scenario));
		} else {
			DISP_LOG_V("received event %s ret %d on scenario %s\n",
				   path_event_name(event), ret,
				   ddp_get_scenario_name(handle->scenario));
		}

		if (event_ts)
			*event_ts = wq_handle->data;

		return ret;
	}
	DISP_LOG_E("wait event %s not initialized on scenario %s\n",
		   path_event_name(event),
		   ddp_get_scenario_name(handle->scenario));
	return ret;
}

int dpmgr_wait_event(void *dp_handle, enum DISP_PATH_EVENT event)
{
	return _dpmgr_wait_event(dp_handle, event, NULL);
}

int dpmgr_wait_event_ts(void *dp_handle, enum DISP_PATH_EVENT event,
			unsigned long long *event_ts)
{
	return _dpmgr_wait_event(dp_handle, event, event_ts);
}

int dpmgr_signal_event(void *dp_handle, enum DISP_PATH_EVENT event)
{
	struct ddp_path_handle_t *handle;
	struct DPMGR_WQ_HANDLE *wq_handle;

	ASSERT(dp_handle != NULL);
	handle = (struct ddp_path_handle_t *)dp_handle;
	wq_handle = &handle->wq_list[event];
	if (handle->wq_list[event].init) {
		wq_handle->data = ktime_to_ns(ktime_get());
		DISP_LOG_V("wake up evnet %s on scenario %s\n",
			   path_event_name(event),
			   ddp_get_scenario_name(handle->scenario));
		wake_up_interruptible(&(handle->wq_list[event].wq));
	}
	return 0;
}

static void dpmgr_irq_handler(enum DISP_MODULE_ENUM module,
			      unsigned int regvalue)
{
	int i = 0;
	int j = 0;
	int irq_bits_num = 0;
	int irq_bit = 0;
	struct ddp_path_handle_t *handle = NULL;

	irq_bits_num = ddp_get_module_max_irq_bit(module);
	for (i = 0; i <= irq_bits_num; i++) {
		if (regvalue & (0x1 << i)) {
			if (module == DISP_MODULE_MUTEX &&
			    (irq_bits_num / 2) > 0) {
				if (i < irq_bits_num / 2)
					handle = (struct ddp_path_handle_t *)
						hw_mutex_id_to_handle_map[i];
				else
					handle = (struct ddp_path_handle_t *)
						hw_mutex_id_to_handle_map
							[i %
							 (irq_bits_num / 2)];
			} else {
				handle = find_handle_by_module(module);
			}

			if (handle == NULL)
				continue;

			irq_bit = MAKE_DDP_IRQ_BIT(module, i);
			dprec_stub_irq(irq_bit);
			for (j = 0; j < DISP_PATH_EVENT_NUM; j++) {
				if (handle->wq_list[j].init &&
				    irq_bit ==
					    handle->irq_event_map[j].irq_bit) {
					dprec_stub_event(j);
					handle->wq_list[j].data =
						ktime_to_ns(ktime_get());
					DDPIRQ(
						"irq signal event %s on cycle %llu on scenario %s\n",
					       path_event_name(j),
					       handle->wq_list[j].data,
					       ddp_get_scenario_name(
						       handle->scenario));
					wake_up_interruptible(
						&(handle->wq_list[j].wq));
				}
			}
		}
	}
}

int dpmgr_init(void)
{
	DISP_LOG_I("ddp manager init\n");
	if (ddp_manager_init)
		return 0;
	ddp_manager_init = 1;
	ddp_debug_init();
	disp_init_irq();
	disp_register_irq_callback(dpmgr_irq_handler);

	memset((void *)hw_mutex_id_to_handle_map, 0,
	       sizeof(hw_mutex_id_to_handle_map));
	return 0;
}

int dpmgr_factory_mode_test(int module_name, void *cmdqhandle, void *config)
{
	if (ddp_modules_driver[module_name] != 0) {
		if (ddp_modules_driver[module_name]->ioctl != 0) {
			DISP_LOG_I(" %s factory_mode_test\n",
				   ddp_get_module_name(module_name));
			ddp_modules_driver[module_name]->ioctl(
				module_name, cmdqhandle, DDP_DPI_FACTORY_TEST,
				config);
		}
	}

	return 0;
}
