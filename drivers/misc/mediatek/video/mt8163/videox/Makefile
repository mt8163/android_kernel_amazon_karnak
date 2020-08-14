# drivers/video/mtk/Makefile


obj-$(CONFIG_MTK_FB) += mtkfb_console.o        \
						disp_assert_layer.o    \
						mtkfb.o                \
						mtkfb_vsync.o          \
						debug.o                \
						primary_display.o      \
						disp_lcm.o             \
						disp_utils.o           \
						mtkfb_fence.o          \
						mtk_disp_mgr.o         \
						compat_mtk_disp_mgr.o  \
						mtk_mira.o             \
						mtk_ovl.o              \
						fbconfig_kdebug_platform.o

#debug
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/include/     \
             -I$(srctree)/drivers/misc/mediatek/video/common/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys/   \
             -I$(srctree)/drivers/misc/mediatek/m4u/       \
             -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)       \
             -I$(srctree)/drivers/misc/mediatek/sync/      \
             -I$(srctree)/drivers/misc/mediatek/cmdq/v2/$(MTK_PLATFORM)/ \
             -I$(srctree)/drivers/misc/mediatek/cmdq/v2/ \
             -I$(srctree)/drivers/misc/mediatek/ext_disp/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/lcm/inc/      \
             -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include/mach/ \
             -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)

ccflags-y += -I$(srctree)/drivers/staging/android/ion \
             -I$(srctree)/drivers/staging/android/ion/mtk


ccflags-$(CONFIG_MTK_LCM) += -I$(srctree)/drivers/misc/mediatek/lcm/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ifeq ($(CONFIG_MTK_INTERNAL_HDMI_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hdmi/hdmitx/$(MTK_PLATFORM)/inc/
endif
ifeq ($(CONFIG_MTK_INTERNAL_MHL_SUPPORT), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mhltx/
endif
ifeq ($(CONFIG_MTK_DITHERING_SUPPORT), y)
ccflags-y += -DDITHERING_SUPPORT
endif

ifeq ($(CONFIG_MTK_GPU_SUPPORT), y)
ccflags-y += -DHWGPU_SUPPORT
endif

ifeq ($(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT),y)
ccflags-y += -I$(srctree)/include/trustzone
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/trustzone
ccflags-y += -I$(srctree)/include
endif
