
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat

ifeq ($(CONFIG_MTK_SMART_BATTERY),y)
	obj-$(CONFIG_MTK_SMART_BATTERY) += battery_meter.o battery_common.o switch_charging.o
endif

ifeq ($(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT),y)
	obj-$(CONFIG_MTK_SMART_BATTERY) += linear_charging.o
endif
