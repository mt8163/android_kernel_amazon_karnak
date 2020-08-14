
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat

obj-$(CONFIG_MTK_SMART_BATTERY) += battery_meter.o battery_common.o switch_charging.o
obj-$(CONFIG_AMAZON_METRICS_LOG) += battery_metrics.o

ifeq ($(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT),y)
	obj-$(CONFIG_MTK_SMART_BATTERY) += linear_charging.o
endif
