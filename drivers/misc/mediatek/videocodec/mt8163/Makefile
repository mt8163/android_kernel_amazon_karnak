
vcodec_kernel_driver-objs :=        \
    videocodec_kernel_driver.o      \
    common/val/src/val.o			\
    common/drv/src/drv_base.o

#ifeq ($(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT),y)
  ccflags-y += -DCONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
#endif
ccflags-y += -DCONFIG_MTK_VIDEO_HEVC_SUPPORT

ifeq ($(CONFIG_ARCH_MT8163),y)
  ccflags-y += -DCONFIG_ARCH_MT8163
endif

obj-y += vcodec_kernel_driver.o

ccflags-y += \
    -I$(srctree)/drivers/misc/mediatek/videocodec/$(CONFIG_MTK_PLATFORM)/common/drv/inc		\
    -I$(srctree)/drivers/misc/mediatek/videocodec/include   \
    -I$(srctree)/drivers/misc/mediatek/videocodec/include/kernel   \
    -I$(srctree)/drivers/misc/mediatek/include/mt-plat   \
    -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach
