#include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y  += -I$(THERMAL_CHIP_DRIVER_DIR)/inc
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/gpu/hal/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/thermal/fakeHeader/
obj-  := dummy.o

ifeq ($(CONFIG_THERMAL), y)
else
    obj-y += mtk_thermal_dummy.o
endif

obj-$(CONFIG_THERMAL) += mtk_thermal_platform.o
obj-$(CONFIG_THERMAL) += mtk_ts_pa_thput.o
obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o

ifneq ($(CONFIG_MTK_COMBO_WIFI),)
    obj-$(CONFIG_THERMAL) += mtk_ts_wmt.o
    obj-$(CONFIG_THERMAL) += mtk_temp_sensor_wmt.o
endif

# Only included in non-EVB projects
ifeq ($(CONFIG_POWER_EXT), y)
else
    ifeq ($(CONFIG_MTK_SMART_BATTERY), y)
        # Always align the Makefile in mediatek/platform/<platform>/kernel/drivers/power/
        ifeq ($(CONFIG_MTK_NCP1851_SUPPORT),y)
            obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
        else
            ifeq ($(CONFIG_MTK_BQ24196_SUPPORT),y)
                obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
            else
                # FAN5405 or PMIC6320 pulse charging or new charger IC
                obj-$(CONFIG_THERMAL) += mtk_cooler_bcct.o
            endif
        endif
    endif

    obj-$(CONFIG_THERMAL) += mtk_ts_battery.o
    obj-$(CONFIG_THERMAL) += mtk_temp_sensor_battery.o
endif


obj-$(CONFIG_THERMAL) += mtk_thermal_budget.o
obj-$(CONFIG_THERMAL) += mtk_ts_bts.o
#obj-$(CONFIG_THERMAL) += mtk_ts_btsmdpa.o
ifeq ($(CONFIG_roc123), y)
	# mtk_thermal interfaces generic solution, recommend to enable this for mt8163
	obj-$(CONFIG_THERMAL) += mtk_thermal.o
	obj-$(CONFIG_CPU_THERMAL) += mtk_cpufreq_cooling.o
else
	obj-$(CONFIG_THERMAL) += mtk_ts_cpu.o
endif

obj-$(CONFIG_THERMAL_ALT_CPU_ZONE) += mtk_alt_ts_cpu.o

#obj-$(CONFIG_THERMAL) += mtk_ts_abb.o
obj-$(CONFIG_THERMAL) += mtk_ts_all_ts.o
#obj-$(CONFIG_THERMAL) += mtk_ts_pa.o
obj-$(CONFIG_THERMAL) += mtk_ts_pmic6323.o
obj-$(CONFIG_THERMAL) += mtk_temp_sensor_pmic6323.o
obj-$(CONFIG_THERMAL) += mtk_temp_sensor_cpu.o
obj-$(CONFIG_THERMAL) += mtk_temp_sensor_gpu.o
obj-$(CONFIG_THERMAL) += mtk_temp_thermistor.o
#obj-$(CONFIG_THERMAL) += tm_nth.o

obj-$(CONFIG_THERMAL_WIFI_COOLER) += mtk_wifi_cooling.o
