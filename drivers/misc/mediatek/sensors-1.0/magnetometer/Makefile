# In case the platform does NOT support this type of sensors
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/accelerometer/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/magnetometer/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/include
# Vanzo:yucheng on: Mon, 13 Mar 2017 16:37:39 +0800
# added for aosp management to import our variable
project_name:=$(shell echo $(VANZO_INNER_PROJECT_NAME))
ifneq ($(strip $(project_name)),)
-include $(srctree)/../zprojects/$(project_name)/$(project_name).mk
VANZO_PROJECT_HEADERS:=$(srctree)/../zprojects/$(project_name)/kernel-headers
ccflags-y += -I$(VANZO_PROJECT_HEADERS)
endif
# End of Vanzo: yucheng
ifeq ($(CONFIG_CUSTOM_KERNEL_GYROSCOPE_MODULE),y)
obj-y   +=  mag_module.o
else
obj-y   +=  mag.o
endif
obj-y   +=  mag_factory.o
# Vanzo:yucheng on: Mon, 13 Mar 2017 16:38:03 +0800
project_name:=$(shell echo $(VANZO_INNER_PROJECT_NAME))
 
ifneq ($(project_name),)
custom_kernel_magnetometer:= $(addsuffix /, $(shell echo $(CUSTOM_KERNEL_MAGNETOMETER)))
$(info "custom_kernel_magnetometer:$(custom_kernel_magnetometer)")
ifneq ($(custom_kernel_magnetometer),)
obj-y += $(custom_kernel_magnetometer)
endif
else
# End of Vanzo: yucheng
obj-$(CONFIG_MTK_MAGHUB)        +=  maghub/
obj-$(CONFIG_MTK_AKM09915_NEW)         +=  akm09915/
obj-$(CONFIG_MTK_AKM09911)         +=  akm09911/
obj-$(CONFIG_MTK_AKM8963)	+=  akm8963/
# Vanzo:yucheng on: Mon, 13 Mar 2017 16:38:32 +0800
# TODO: replace this line with your comment
# added for new structure config alsps
endif
# End of Vanzo: yucheng