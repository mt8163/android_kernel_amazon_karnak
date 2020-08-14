/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef _BATTERY_METER_HAL_H
#define _BATTERY_METER_HAL_H

/* ============================================================ */
/* ENUM */
/* ============================================================ */
enum BATTERY_METER_CTRL_CMD {
	BATTERY_METER_CMD_HW_FG_INIT,
	BATTERY_METER_CMD_GET_HW_FG_CURRENT,	/* fgauge_read_current */
	BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN,	/*  */
	BATTERY_METER_CMD_GET_HW_FG_CAR,	/* fgauge_read_columb */
	BATTERY_METER_CMD_HW_RESET,	/* FGADC_Reset_SW_Parameter */
	BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE,
	BATTERY_METER_CMD_GET_ADC_V_I_SENSE,
	BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP,
	BATTERY_METER_CMD_GET_ADC_V_CHARGER,
	BATTERY_METER_CMD_GET_HW_OCV,
	BATTERY_METER_CMD_DUMP_REGISTER,
	BATTERY_METER_CMD_SET_COLUMB_INTERRUPT,
	BATTERY_METER_CMD_GET_BATTERY_PLUG_STATUS,
	BATTERY_METER_CMD_GET_HW_FG_CAR_ACT,	/* fgauge_read_columb */
	BATTERY_METER_CMD_NUMBER
};

/* ============================================================ */
/* typedef */
/* ============================================================ */
typedef signed int(*BATTERY_METER_CONTROL) (int cmd, void *data);

/* ============================================================ */
/* External function */
/* ============================================================ */
extern signed int bm_ctrl_cmd(int cmd, void *data);

#endif				/* #ifndef _BATTERY_METER_HAL_H */
