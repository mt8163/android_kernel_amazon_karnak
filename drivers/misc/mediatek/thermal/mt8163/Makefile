#
# Copyright (C) 2017 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

#include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-y += -I$(srctree)

THERMAL_CHIP_DRIVER_DIR := $(srctree)/drivers/misc/mediatek/thermal/$(MTK_PLATFORM)
ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/gpu/hal/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/fakeHeader/
obj-  := dummy.o

obj-y += mtk_thermal_platform.o
obj-y += mtk_ts_pa_thput.o
obj-y += mtk_cooler_bcct.o

ifneq ($(CONFIG_MTK_COMBO_WIFI),)
    obj-y += mtk_ts_wmt.o
    obj-y += mtk_temp_sensor_wmt.o
endif

# Only included in non-EVB projects
ifeq ($(CONFIG_POWER_EXT), y)
else
    ifeq ($(CONFIG_MTK_SMART_BATTERY), y)
        # Always align the Makefile in mediatek/platform/<platform>/kernel/drivers/power/
        ifeq ($(CONFIG_MTK_NCP1851_SUPPORT),y)
            obj-y += mtk_cooler_bcct.o
        else
            ifeq ($(CONFIG_MTK_BQ24196_SUPPORT),y)
                obj-y += mtk_cooler_bcct.o
            else
                # FAN5405 or PMIC6320 pulse charging or new charger IC
                obj-y += mtk_cooler_bcct.o
            endif
        endif
    endif

    obj-y += mtk_ts_battery.o
    obj-y += mtk_temp_sensor_battery.o
endif


#obj-y += mtk_ts_btsmdpa.o
obj-y += mtk_ts_cpu.o

#obj-y += mtk_ts_abb.o

ifneq ($(CONFIG_AMAZON_THERMAL), y)
#obj-y += mtk_ts_pa.o
#obj-y += tm_nth.o
obj-y += mtk_temp_sensor_gpu.o
obj-y += mtk_thermal_budget.o
endif

obj-y += mtk_temp_sensor_pmic6323.o
obj-y += mtk_ts_pmic6323.o
obj-y += mtk_ts_bts.o
obj-y += mtk_ts_all_ts.o
obj-y += mtk_temp_sensor_cpu.o
obj-y += mtk_temp_thermistor.o
