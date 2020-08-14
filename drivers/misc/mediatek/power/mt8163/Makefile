#
# Copyright (C) 2015 MediaTek Inc.
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

obj-$(CONFIG_MTK_PMIC)          += pmic_mt6323.o pmic_chr_type_det.o
obj-$(CONFIG_MTK_SMART_BATTERY) += battery_meter_hal.o

obj-$(CONFIG_MTK_BQ24296_SUPPORT) += charging_hw_bq24297.o bq24297.o tbl_helper_func.o
obj-$(CONFIG_MTK_BQ25601_SUPPORT) += charging_hw_bq25601.o bq25601.o

ifeq ($(CONFIG_MTK_INTERNAL_CHARGER_SUPPORT),y)
	obj-$(CONFIG_MTK_SMART_BATTERY) += charging_hw_pmic.o
endif
