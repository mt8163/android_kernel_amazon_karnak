subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

#subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/accdet/$(MTK_PLATFORM)
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6391)+= -I$(srctree)/drivers/misc/mediatek/accdet/mt6391
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6355)+= -I$(srctree)/drivers/misc/mediatek/accdet/mt6355
subdir-ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6357)+= -I$(srctree)/drivers/misc/mediatek/accdet/mt6357
subdir-ccflags-$(CONFIG_MTK_AMZN_ACCDET)+= -I$(srctree)/drivers/misc/mediatek/accdet/mt8163
subdir-ccflags-$(CONFIG_MTK_ACCDET_PMIC_CHIP_MT6323)+= -I$(srctree)/drivers/misc/mediatek/accdet/mt6323

obj-$(CONFIG_MTK_ACCDET) := accdet_drv.o
#obj-$(CONFIG_MTK_ACCDET) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_MTK_AMZN_ACCDET) := accdet_drv.o
#obj-$(CONFIG_MTK_AMZN_ACCDET) += $(subst ",,$(CONFIG_MTK_PLATFORM))/

obj-$(CONFIG_MTK_PMIC_CHIP_MT6391) += mt6391/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6355) += mt6355/
obj-$(CONFIG_MTK_PMIC_CHIP_MT6357) += mt6357/
obj-$(CONFIG_MTK_AMZN_ACCDET) += mt8163/
obj-$(CONFIG_MTK_ACCDET_PMIC_CHIP_MT6323) += mt6323/

