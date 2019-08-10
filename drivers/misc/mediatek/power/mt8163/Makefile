#include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-$(CONFIG_MTK_PMIC)          += pmic_mt6323.o pmic_chr_type_det.o

obj-$(CONFIG_MTK_SMART_BATTERY) += battery_meter_hal.o

ifeq ($(CONFIG_MTK_BQ24296_SUPPORT),y)
	obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_bq24297.o bq24297.o tbl_helper_func.o
else
	obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_bq24297.o bq24297.o tbl_helper_func.o
endif

obj-$(CONFIG_MTK_BQ25601_SUPPORT) += charging_hw_bq25601.o bq25601.o

ifeq ($(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT),y)
	obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_pmic.o
endif