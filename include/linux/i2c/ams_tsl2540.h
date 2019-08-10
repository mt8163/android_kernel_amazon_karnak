/*
 * Device driver for monitoring ambient light intensity (lux)
 * for device tsl2540.
 *
 * Copyright (c) 2017, ams AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

/*! \file
* \brief Device driver for monitoring ambient light intensity in (lux)
* proximity detection (prox), Gesture, and Beam functionality within the
* AMS-TAOS TSL2540 family of devices.
*/

#ifndef __TSL2540_H
#define __TSL2540_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/wait.h>

#define ABI_SET_GET_REGISTERS

#ifdef AMS_MUTEX_DEBUG
#define AMS_MUTEX_LOCK(m) { \
		pr_info("%s: Mutex Lock\n", __func__); \
		mutex_lock(m); \
	}
#define AMS_MUTEX_UNLOCK(m) { \
		pr_info("%s: Mutex Unlock\n", __func__); \
		mutex_unlock(m); \
	}
#else
#define AMS_MUTEX_LOCK(m) { \
		mutex_lock(m); \
	}
#define AMS_MUTEX_UNLOCK(m) { \
		mutex_unlock(m); \
	}
#endif

enum tsl2540_regs {
	TSL2540_REG_ENABLE     = 0x80,
	TSL2540_REG_ATIME      = 0x81,
	TSL2540_REG_PTIME      = 0x82,
	TSL2540_REG_WTIME      = 0x83,
	TSL2540_REG_AILT       = 0x84,
	TSL2540_REG_AILT_HI    = 0x85,
	TSL2540_REG_AIHT       = 0x86,
	TSL2540_REG_AIHT_HI    = 0x87,
	TSL2540_REG_PERS       = 0x8C,
	TSL2540_REG_CFG0       = 0x8D,
	TSL2540_REG_PGCFG0     = 0x8E,
	TSL2540_REG_PGCFG1     = 0x8F,

	TSL2540_REG_CFG1       = 0x90,
	TSL2540_REG_REVID      = 0x91,
	TSL2540_REG_ID         = 0x92,
	TSL2540_REG_STATUS     = 0x93,
	TSL2540_REG_CH0DATA    = 0x94,
	TSL2540_REG_CH0DATA_HI = 0x95,
	TSL2540_REG_CH1DATA    = 0x96,
	TSL2540_REG_CH1DATA_HI = 0x97,
	TSL2540_REG_PDATA      = 0x9C,

	TSL2540_REG_ADCDATA_L  = 0x9D,
	TSL2540_REG_AUXID      = 0x9E,
	TSL2540_REG_CFG2       = 0x9F,

	TSL2540_REG_CFG3       = 0xAB,
	TSL2540_REG_CFG4       = 0xAC,
	TSL2540_REG_CFG5       = 0xAD,

	TSL2540_REG_AZ_CONFIG  = 0xD6,
	TSL2540_REG_CALIB      = 0xD7,
	TSL2540_REG_CALIBCFG   = 0xD9,
	TSL2540_REG_CALIBSTAT  = 0xDC,
	TSL2540_REG_INTENAB    = 0xDD,
};

enum tsl2540__reg {
	TSL2540_MASK_BINSRCH_TARGET = 0x70,
	TSL2540_SHIFT_BINSRCH_TARGET = 4,

	TSL2540_MASK_START_OFFSET_CALIB = 0x01,
	TSL2540_SHIFT_START_OFFSET_CALIB = 0,

	TSL2540_MASK_AGAIN = 0x03,
	TSL2540_SHIFT_AGAIN = 0,

	TSL2540_MASK_AGAINMAX = 0x10,
	TSL2540_SHIFT_AGAINMAX = 4,

	TSL2540_MASK_AGAINL = 0x04,
	TSL2540_SHIFT_AGAINL = 2,

	TSL2540_MASK_APERS = 0x0f,
	TSL2540_SHIFT_APERS = 0,

	TSL2540_MASK_WLONG = 0x04,
	TSL2540_SHIFT_WLONG = 2,
};

enum tsl2540_gain_factor {
	TSL2540_100_PERCENTS = 0,
	TSL2540_200_PERCENTS = 1,
	TSL2540_50_PERCENTS = 2,
};

enum tsl2540_en_reg {
	TSL2540_PON  = (1 << 0),
	TSL2540_AEN  = (1 << 1),
	TSL2540_PEN  = (1 << 2),
	TSL2540_WEN  = (1 << 3),
	TSL2540_EN_ALL = (TSL2540_AEN |
			  TSL2540_PEN |
			  TSL2540_WEN),
};

enum tsl2540_status {
	TSL2540_ST_PGSAT_AMBIENT  = (1 << 0),
	TSL2540_ST_PGSAT_RELFLECT = (1 << 1),
	TSL2540_ST_ZERODET    = (1 << 1),
	TSL2540_ST_CAL_IRQ    = (1 << 3),
	TSL2540_ST_ALS_IRQ    = (1 << 4),
	TSL2540_ST_PRX_IRQ    = (1 << 5),
	TSL2540_ST_PRX_SAT    = (1 << 6),
	TSL2540_ST_ALS_SAT    = (1 << 7),
};

enum tsl2540_intenab_reg {
	TSL2540_ZIEN = (1 << 2),
	TSL2540_CIEN = (1 << 3),
	TSL2540_AIEN = (1 << 4),
	TSL2540_PIEN = (1 << 5),
	TSL2540_PSIEN = (1 << 6),
	TSL2540_ASIEN = (1 << 7),
};

#define MAX_REGS 256
struct device;

enum tsl2540_pwr_state {
	POWER_ON,
	POWER_OFF,
	POWER_STANDBY,
};

enum tsl2540_ctrl_reg {
	AGAIN_1        = (0 << 0),
	AGAIN_4        = (1 << 0),
	AGAIN_16       = (2 << 0),
	AGAIN_64       = (3 << 0),
	PG_PULSE_4US   = (0 << 6),
	PG_PULSE_8US   = (1 << 6),
	PG_PULSE_16US  = (2 << 6),
	PG_PULSE_32US  = (3 << 6),
};

/* pldrive */
#define PDRIVE_MA(p)   (((u8)((p) / 6) - 1) & 0x1f)
#define P_TIME_US(p)   ((((p) / 88) - 1.0) + 0.5)
#define PRX_PERSIST(p) (((p) & 0xf) << 4)

#define INTEGRATION_CYCLE 2816
#define AW_TIME_MS(p)  ((((p) * 1000) +\
	(INTEGRATION_CYCLE - 1)) / INTEGRATION_CYCLE)
#define ALS_PERSIST(p) (((p) & 0xf) << 0)

/* lux */
#define INDOOR_LUX_TRIGGER	6000
#define OUTDOOR_LUX_TRIGGER	10000
#define TSL2540_MAX_LUX		0xffff
#define TSL2540_MAX_ALS_VALUE	0xffff
#define TSL2540_MIN_ALS_VALUE	10

struct tsl2540_lux_segment {
	u32 ch0_coef;
	u32 ch1_coef;
};

struct tsl2540_parameters {
	u8 persist;
	u8 als_time;
	u16 als_deltap;
	u8 als_gain;
	u8 az_iterations;
	u32 d_factor;
	struct tsl2540_lux_segment lux_segment[2];
	u8 als_gain_factor;
};

struct tsl2540_als_info {
	u16 als_ch0; /* photopic channel */
	u16 als_ch1; /* ir channel */
	u32 cpl;
	u32 lux1_ch0_coef;
	u32 lux1_ch1_coef;
	u32 lux2_ch0_coef;
	u32 lux2_ch1_coef;
	u32 saturation;
	u16 lux;
};

struct tsl2540_chip {
	struct mutex lock;
	struct i2c_client *client;
	struct tsl2540_als_info als_inf;
	struct tsl2540_parameters params;
	struct tsl2540_i2c_platform_data *pdata;
	u8 shadow[MAX_REGS];

	struct input_dev *p_idev;
	struct input_dev *a_idev;
#ifdef ABI_SET_GET_REGISTERS
	struct input_dev *d_idev;
#endif /* #ifdef ABI_SET_GET_REGISTERS */

	int in_suspend;
	int wake_irq;
	int irq_pending;

	bool unpowered;
	bool als_enabled;
	bool als_gain_auto;
	bool prx_enabled;
	bool amscalcomplete;
	bool amsindoormode;

	u8 device_index;
};

/* Must match definition in ../arch file */
struct tsl2540_i2c_platform_data {
	/* The following callback for power events received and handled by
	   the driver.  Currently only for SUSPEND and RESUME */
	int (*platform_power)(struct device *dev, enum tsl2540_pwr_state state);
	int (*platform_init)(void);
	void (*platform_teardown)(struct device *dev);

	char const *als_name;
	struct tsl2540_parameters parameters;
	bool als_can_wake;
	bool boot_on;
#ifdef CONFIG_AMS_ADJUST_WITH_BASELINE
	int lux0_ch0;
	int lux0_ch1;
	int lux400_lux;
#endif
#ifdef CONFIG_OF
	struct device_node  *of_node;
#endif
};

#endif /* __TSL2540_H */
