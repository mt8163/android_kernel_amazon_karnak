#
# Makefile for misc devices that really don't fit anywhere else.

ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))

define FILTER_PROJECT_DRV
$(info project:$(ARCH_MTK_PROJECT), platform:$(MTK_PLATFORM))
ifeq ($(wildcard $(srctree)/drivers/misc/mediatek/imgsensor/src/$(MTK_PLATFORM)/camera_sensor_proj/$(ARCH_MTK_PROJECT)/$(1)),)
platform_drv += $(1)
else
platform_drv += camera_sensor_proj/$(ARCH_MTK_PROJECT)/$(1)
endif
endef

obj-y				+= kd_sensorlist.o
obj-y				+= camera_hw/

$(foreach drv,$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)),$(eval $(call FILTER_PROJECT_DRV,$(drv))))
custom_kernel_imgsensor := $(addsuffix /, $(shell echo $(platform_drv)))
obj-y += $(custom_kernel_imgsensor)
$(info imgsensor drv by platform $(platform_drv))
