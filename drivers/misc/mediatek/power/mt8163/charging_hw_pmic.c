/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/types.h>
#include <mt-plat/charging.h>
#include <mt-plat/upmu_common.h>

#include <linux/delay.h>
#include <linux/reboot.h>
#include <mt-plat/mt_boot.h>
#include <mt-plat/battery_common.h>

/*============================================================*/
/*define*/
/*============================================================*/
#define STATUS_OK    0
#define STATUS_UNSUPPORTED    -1
#define GETARRAYNUM(array) (ARRAY_SIZE(array))

/*============================================================*/
/*global variable*/
/*============================================================*/
static const unsigned int VBAT_CV_VTH[] = {
	BATTERY_VOLT_04_200000_V,
	BATTERY_VOLT_04_212500_V,
	BATTERY_VOLT_04_225000_V,
	BATTERY_VOLT_04_237500_V,
	BATTERY_VOLT_04_250000_V,
	BATTERY_VOLT_04_262500_V,
	BATTERY_VOLT_04_275000_V,
	BATTERY_VOLT_04_300000_V,
	BATTERY_VOLT_04_325000_V,
	BATTERY_VOLT_04_350000_V,
	BATTERY_VOLT_04_375000_V,
	BATTERY_VOLT_04_400000_V,
	BATTERY_VOLT_04_425000_V,
	BATTERY_VOLT_04_162500_V,
	BATTERY_VOLT_04_175000_V,
	BATTERY_VOLT_02_200000_V,
	BATTERY_VOLT_04_050000_V,
	BATTERY_VOLT_04_100000_V,
	BATTERY_VOLT_04_125000_V,
	BATTERY_VOLT_03_775000_V,
	BATTERY_VOLT_03_800000_V,
	BATTERY_VOLT_03_850000_V,
	BATTERY_VOLT_03_900000_V,
	BATTERY_VOLT_04_000000_V,
	BATTERY_VOLT_04_050000_V,
	BATTERY_VOLT_04_100000_V,
	BATTERY_VOLT_04_125000_V,
	BATTERY_VOLT_04_137500_V,
	BATTERY_VOLT_04_150000_V,
	BATTERY_VOLT_04_162500_V,
	BATTERY_VOLT_04_175000_V,
	BATTERY_VOLT_04_187500_V

};

static const unsigned int CS_VTH[] = {
	CHARGE_CURRENT_1600_00_MA,
	CHARGE_CURRENT_1500_00_MA,
	CHARGE_CURRENT_1400_00_MA,
	CHARGE_CURRENT_1300_00_MA,
	CHARGE_CURRENT_1200_00_MA,
	CHARGE_CURRENT_1100_00_MA,
	CHARGE_CURRENT_1000_00_MA,
	CHARGE_CURRENT_900_00_MA,
	CHARGE_CURRENT_800_00_MA,
	CHARGE_CURRENT_700_00_MA,
	CHARGE_CURRENT_650_00_MA,
	CHARGE_CURRENT_550_00_MA,
	CHARGE_CURRENT_450_00_MA,
	CHARGE_CURRENT_300_00_MA,
	CHARGE_CURRENT_200_00_MA,
	CHARGE_CURRENT_70_00_MA
};

static const unsigned int VCDT_HV_VTH[] = {
	BATTERY_VOLT_04_200000_V,
	BATTERY_VOLT_04_250000_V,
	BATTERY_VOLT_04_300000_V,
	BATTERY_VOLT_04_350000_V,
	BATTERY_VOLT_04_400000_V,
	BATTERY_VOLT_04_450000_V,
	BATTERY_VOLT_04_500000_V,
	BATTERY_VOLT_04_550000_V,
	BATTERY_VOLT_04_600000_V,
	BATTERY_VOLT_06_000000_V,
	BATTERY_VOLT_06_500000_V,
	BATTERY_VOLT_07_000000_V,
	BATTERY_VOLT_07_500000_V,
	BATTERY_VOLT_08_500000_V,
	BATTERY_VOLT_09_500000_V,
	BATTERY_VOLT_10_500000_V
};

/*============================================================
 * function prototype
 *============================================================
 */

static unsigned int charging_error;
static unsigned int charging_get_error_state(void);
static unsigned int charging_set_error_state(void *data);
static unsigned int charging_status;
/* ============================================================ */
static unsigned int charging_get_csdac_value(void)
{
	unsigned int tempA, tempB, tempC;
	unsigned int sum;

	pmic_config_interface(CHR_CON11, 0xC, 0xF, 0);
	/*bit 1 and 2 mapping bit 8 and bit9*/
	pmic_read_interface(CHR_CON10, &tempC, 0xF, 0x0);

	pmic_config_interface(CHR_CON11, 0xA, 0xF, 0);
	/*bit 0 ~ 3 mapping bit 4 ~7*/
	pmic_read_interface(CHR_CON10, &tempA, 0xF, 0x0);

	pmic_config_interface(CHR_CON11, 0xB, 0xF, 0);
	/*bit 0~3 mapping bit 0~3*/
	pmic_read_interface(CHR_CON10, &tempB, 0xF, 0x0);

	sum =  (((tempC & 0x6) >> 1)<<8) | (tempA << 4) | tempB;

	pr_debug("tempC=%d,tempA=%d,tempB=%d, csdac=%d\n",
		tempC, tempA, tempB, sum);

	return sum;
}

static unsigned int charging_value_to_parameter(
const unsigned int *parameter, const unsigned int array_size,
const unsigned int val)
{
	if (val < array_size)
		return parameter[val];

	pr_debug("Can't find the parameter\r\n");
	return parameter[0];
}

static unsigned int charging_parameter_to_value(
const unsigned int *parameter, const unsigned int array_size,
const unsigned int val)
{
	unsigned int i;

	pr_debug("array_size = %d\r\n", array_size);

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	pr_debug("NO register value match. val=%d\r\n", val);
	/*TODO: ASSERT(0);*/
	return 0;
}

static unsigned int bmt_find_closest_level(const unsigned int *pList,
	unsigned int number, unsigned int level)
{
	unsigned int i;
	unsigned int max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = true;
	else
		max_value_in_last_element = false;

	if (max_value_in_last_element == true) {
		/*max value in the last element*/
		for (i = (number-1); i != 0; i--) {
			if (pList[i] <= level)
				return pList[i];
		}

		pr_debug("Can't find closest level, small value first\r\n");
		return pList[0];
	}

	/*max value in the first element*/
	for (i = 0; i < number; i++) {
		if (pList[i] <= level)
			return pList[i];
	}

	pr_debug("Can't find closest level, large value first\r\n");
	return pList[number - 1];
}

static unsigned int charging_hw_init(void *data)
{
	unsigned int status = STATUS_OK;

	upmu_set_rg_chrwdt_td(0x0);		/*CHRWDT_TD, 4s*/
	upmu_set_rg_chrwdt_int_en(1);		/*CHRWDT_INT_EN*/
	upmu_set_rg_chrwdt_en(1);		/*CHRWDT_EN*/
	upmu_set_rg_chrwdt_wr(1);		/*CHRWDT_WR*/

	upmu_set_rg_vcdt_mode(0);	/*VCDT_MODE*/
	upmu_set_rg_vcdt_hv_en(1);	/*VCDT_HV_EN*/

	upmu_set_rg_usbdl_set(0);	/*force leave USBDL mode*/
	upmu_set_rg_usbdl_rst(1);	/*force leave USBDL mode*/

	upmu_set_rg_bc11_bb_ctrl(1);	/*BC11_BB_CTRL*/
	upmu_set_rg_bc11_rst(1);	/*BC11_RST*/

	upmu_set_rg_csdac_mode(1);	/*CSDAC_MODE*/
	upmu_set_rg_vbat_ov_en(1);	/*VBAT_OV_EN*/
#ifdef HIGH_BATTERY_VOLTAGE_SUPPORT
	upmu_set_rg_vbat_ov_vth(0x2);	/*VBAT_OV_VTH, 4.4V,*/
#else
	upmu_set_rg_vbat_ov_vth(0x1);	/*VBAT_OV_VTH, 4.3V,*/
#endif
	upmu_set_rg_baton_en(1);	/*BATON_EN*/

	/*Tim, for TBAT*/
	/*upmu_set_rg_buf_pwd_b(1)	RG_BUF_PWD_B*/
	upmu_set_rg_baton_ht_en(0);	/*BATON_HT_EN*/

	upmu_set_rg_ulc_det_en(1);	/*RG_ULC_DET_EN=1*/
	upmu_set_rg_low_ich_db(1);	/*RG_LOW_ICH_DB=000001'b*/

#if defined(CONFIG_MTK_PUMP_EXPRESS_SUPPORT)
	upmu_set_rg_csdac_dly(0x0);		/*CSDAC_DLY*/
	upmu_set_rg_csdac_stp(0x1);		/*CSDAC_STP*/
	upmu_set_rg_csdac_stp_inc(0x1);		/*CSDAC_STP_INC*/
	upmu_set_rg_csdac_stp_dec(0x7);		/*CSDAC_STP_DEC*/
	upmu_set_rg_cs_en(1);			/*CS_EN*/

	upmu_set_rg_hwcv_en(1);
	upmu_set_rg_vbat_cv_en(1);		/*CV_EN*/
	upmu_set_rg_csdac_en(1);		/*CSDAC_EN*/
	upmu_set_rg_chr_en(1);			/*CHR_EN*/
#endif
	return status;
}

static unsigned int charging_dump_register(void *data)
{
	unsigned int status = STATUS_OK;

	unsigned int reg_val = 0;
	unsigned int reg_num = CHR_CON0;
	unsigned int i = 0;

	for (i = reg_num; i <= CHR_CON29; i += 2) {
		reg_val = upmu_get_reg_value(i);
		pr_debug("Chr Reg[0x%x]=0x%x\r\n", i, reg_val);
	}

	return status;
}

static unsigned int charging_enable(void *data)
{
	unsigned int status = STATUS_OK;

	charging_status = *(unsigned int *)(data);
	if (charging_get_error_state())
		charging_status = false;

	if (true == charging_status) {
		upmu_set_rg_csdac_dly(0x4);             /*CSDAC_DLY*/
		upmu_set_rg_csdac_stp(0x1);             /*CSDAC_STP*/
		upmu_set_rg_csdac_stp_inc(0x1);         /*CSDAC_STP_INC*/
		upmu_set_rg_csdac_stp_dec(0x2);         /*CSDAC_STP_DEC*/
		upmu_set_rg_cs_en(1);                   /*CS_EN, check me*/

		upmu_set_rg_hwcv_en(1);

		upmu_set_rg_vbat_cv_en(1);              /*CV_EN*/
		upmu_set_rg_csdac_en(1);                /*CSDAC_EN*/

		upmu_set_rg_pchr_flag_en(1);	/*enable debug flag output*/
		upmu_set_rg_chr_en(1);		/*CHR_EN*/

		charging_dump_register(NULL);
	} else {
		upmu_set_rg_chrwdt_int_en(0);    /*CHRWDT_INT_EN*/
		upmu_set_rg_chrwdt_en(0);        /*CHRWDT_EN*/
		upmu_set_rg_chrwdt_flag_wr(0);   /*CHRWDT_FLAG*/

		upmu_set_rg_csdac_en(0);         /*CSDAC_EN*/
		upmu_set_rg_chr_en(0);           /*CHR_EN*/
		upmu_set_rg_hwcv_en(0);          /*RG_HWCV_EN*/
	}
	return status;
}

static unsigned int charging_set_cv_voltage(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned short register_value;

	register_value = charging_parameter_to_value(
		VBAT_CV_VTH, GETARRAYNUM(VBAT_CV_VTH),
	*(unsigned int *)(data));
	upmu_set_rg_vbat_cv_vth(register_value);
	return status;
}

static unsigned int charging_get_current(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int array_size;
	unsigned int reg_value;

	array_size = GETARRAYNUM(CS_VTH);
	reg_value = upmu_get_reg_value(0x8);	/*RG_CS_VTH*/
	*(unsigned int *)data = charging_value_to_parameter(
		CS_VTH, array_size, reg_value);
	return status;
}

static unsigned int charging_set_current(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;

	array_size = GETARRAYNUM(CS_VTH);
	set_chr_current = bmt_find_closest_level(
		CS_VTH, array_size, *(unsigned int *)data);
	register_value = charging_parameter_to_value(
		CS_VTH, array_size, set_chr_current);
	upmu_set_rg_cs_vth(register_value);
	return status;
}

static unsigned int charging_get_charging_status(void *data)
{
	unsigned int status = STATUS_OK;

	if (charging_status)
		*(unsigned int *)data = false;
	else
		*(unsigned int *)data = true;

	return status;
}

static unsigned int charging_reset_watch_dog_timer(void *data)
{
	unsigned int status = STATUS_OK;

	upmu_set_rg_chrwdt_td(0x0);           /*CHRWDT_TD, 4s*/
	upmu_set_rg_chrwdt_wr(1);             /*CHRWDT_WR*/
	upmu_set_rg_chrwdt_int_en(1);         /*CHRWDT_INT_EN*/
	upmu_set_rg_chrwdt_en(1);             /*CHRWDT_EN*/
	upmu_set_rg_chrwdt_flag_wr(1);        /*CHRWDT_WR*/

	return status;
}


static unsigned int charging_set_hv_threshold(void *data)
{
	unsigned int status = STATUS_OK;

	unsigned int set_hv_voltage;
	unsigned int array_size;
	unsigned short register_value;
	unsigned int voltage = *(unsigned int *)(data);

	array_size = GETARRAYNUM(VCDT_HV_VTH);
	set_hv_voltage = bmt_find_closest_level(
		VCDT_HV_VTH, array_size, voltage);
	register_value = charging_parameter_to_value(
		VCDT_HV_VTH, array_size, set_hv_voltage);
	upmu_set_rg_vcdt_hv_vth(register_value);

	return status;
}

static unsigned int charging_get_hv_status(void *data)
{
	unsigned int status = STATUS_OK;
#if defined(CONFIG_POWER_EXT)
	*(bool *)(data) = 0;
#else
	*(bool *)(data) = upmu_get_rgs_vcdt_hv_det();
#endif
	return status;
}


static unsigned int charging_get_battery_status(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT)
	*(unsigned int *)(data) = 0;
#else
	upmu_set_baton_tdet_en(1);
	upmu_set_rg_baton_en(1);
	*(unsigned int *)(data) = upmu_get_rgs_baton_undet();
#endif
	return status;
}


static unsigned int charging_get_charger_det_status(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CHRDET_SW_MODE_EN)
	unsigned int vchr_val = 0;

	vchr_val = PMIC_IMM_GetOneChannelValue(4, 5, 1);
	vchr_val = (((330+39)*100*vchr_val)/39)/100;

	if (vchr_val > 4300) {
		pr_debug("[CHRDET_SW_WORKAROUND_EN] upmu_is_chr_det=Y (%d)\n",
			vchr_val);
		*(unsigned int *)data = true;
	} else {
		pr_debug("[CHRDET_SW_WORKAROUND_EN] upmu_is_chr_det=N (%d)\n",
			vchr_val);
		*(unsigned int *)data = false;
	}
#else
	*(bool *)(data) = upmu_get_rgs_chrdet();
#endif

	return status;
}

static unsigned int charging_get_charger_type(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
	*(CHARGER_TYPE *)(data) = STANDARD_HOST;
#else
	*(CHARGER_TYPE *)(data) = hw_charger_type_detection();
	pr_debug("charging_get_charger_type = %d\r\n", *(CHARGER_TYPE *)(data));
#endif
	return status;
}

static unsigned int charging_set_platform_reset(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
#else
	pr_notice("charging_set_platform_reset\n");
	kernel_restart("battery service reboot system");
	/* arch_reset(0,NULL); */
#endif
	return status;
}

static unsigned int charging_get_platform_boot_mode(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
#else
	*(unsigned int *)(data) = get_boot_mode();

	pr_debug("get_boot_mode=%d\n", get_boot_mode());
#endif

	return status;
}

static unsigned int charging_set_power_off(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
#else
	pr_notice("charging_set_power_off\n");
	kernel_power_off();
#endif

	return status;
}

static unsigned int charging_get_csdac_full_flag(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int csdac_value;

	csdac_value = charging_get_csdac_value();

	/*10 bit,  treat as full if csdac more than 800*/
	if (csdac_value > 800)
		*(bool *)data = true;
	else
		*(bool *)data = false;
	return status;
}

static unsigned int charging_set_ta_current_pattern(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int array_size;
	unsigned int ta_on_current = CHARGE_CURRENT_450_00_MA;
	unsigned int ta_off_current = CHARGE_CURRENT_70_00_MA;
	unsigned int set_ta_on_current_reg_value;
	unsigned int set_ta_off_current_reg_value;
	unsigned int increase = *(unsigned int *)(data);

	array_size = GETARRAYNUM(CS_VTH);
	set_ta_on_current_reg_value = charging_parameter_to_value(
		CS_VTH, array_size, ta_on_current);
	set_ta_off_current_reg_value = charging_parameter_to_value(
		CS_VTH, array_size, ta_off_current);

	if (increase == true) {
		pr_debug("mtk_ta_increase() start\n");

		upmu_set_rg_cs_vth(set_ta_off_current_reg_value);
		msleep(50);

		/*patent start*/
		upmu_set_rg_cs_vth(set_ta_on_current_reg_value);
		pr_debug("mtk_ta_increase() on 1");
		msleep(100);
		upmu_set_rg_cs_vth(set_ta_off_current_reg_value);
		pr_debug("mtk_ta_increase() off 1");
		msleep(100);

		upmu_set_rg_cs_vth(set_ta_on_current_reg_value);
		pr_debug("mtk_ta_increase() on 2");
		msleep(100);
		upmu_set_rg_cs_vth(set_ta_off_current_reg_value);
		pr_debug("mtk_ta_increase() off 2");
		msleep(100);

		upmu_set_rg_cs_vth(set_ta_on_current_reg_value);
		pr_debug("mtk_ta_increase() on 3");
		msleep(300);
		upmu_set_rg_cs_vth(set_ta_off_current_reg_value);
		pr_debug("mtk_ta_increase() off 3");
		msleep(100);

		upmu_set_rg_cs_vth(set_ta_on_current_reg_value);
		pr_debug("mtk_ta_increase() on 4");
		msleep(300);
		upmu_set_rg_cs_vth(set_ta_off_current_reg_value);
		pr_debug("mtk_ta_increase() off 4");
		msleep(100);

		upmu_set_rg_cs_vth(set_ta_on_current_reg_value);
		pr_debug("mtk_ta_increase() on 5");
		msleep(300);
		upmu_set_rg_cs_vth(set_ta_off_current_reg_value);
		pr_debug("mtk_ta_increase() off 5");
		msleep(100);

		upmu_set_rg_cs_vth(set_ta_on_current_reg_value);
		pr_debug("mtk_ta_increase() on 6");
		msleep(500);

		upmu_set_rg_cs_vth(set_ta_off_current_reg_value);
		pr_debug("mtk_ta_increase() off 6");
		msleep(50);
		/*patent end*/

		pr_debug("mtk_ta_increase() end\n");
	} else {
		pr_debug("mtk_ta_decrease() start\n");

		upmu_set_rg_cs_vth(set_ta_off_current_reg_value);
		msleep(50);

		/*patent start	*/
		upmu_set_rg_cs_vth(set_ta_on_current_reg_value);
		pr_debug("mtk_ta_decrease() on 1");
		msleep(300);
		upmu_set_rg_cs_vth(set_ta_off_current_reg_value);
		pr_debug("mtk_ta_decrease() off 1");
		msleep(100);

		upmu_set_rg_cs_vth(set_ta_on_current_reg_value);
		pr_debug("mtk_ta_decrease() on 2");
		msleep(300);
		upmu_set_rg_cs_vth(set_ta_off_current_reg_value);
		pr_debug("mtk_ta_decrease() off 2");
		msleep(100);

		upmu_set_rg_cs_vth(set_ta_on_current_reg_value);
		pr_debug("mtk_ta_decrease() on 3");
		msleep(300);
		upmu_set_rg_cs_vth(set_ta_off_current_reg_value);
		pr_debug("mtk_ta_decrease() off 3");
		msleep(100);

		upmu_set_rg_cs_vth(set_ta_on_current_reg_value);
		pr_debug("mtk_ta_decrease() on 4");
		msleep(100);
		upmu_set_rg_cs_vth(set_ta_off_current_reg_value);
		pr_debug("mtk_ta_decrease() off 4");
		msleep(100);

		upmu_set_rg_cs_vth(set_ta_on_current_reg_value);
		pr_debug("mtk_ta_decrease() on 5");
		msleep(100);
		upmu_set_rg_cs_vth(set_ta_off_current_reg_value);
		pr_debug("mtk_ta_decrease() off 5");
		msleep(100);


		upmu_set_rg_cs_vth(set_ta_on_current_reg_value);
		pr_debug("mtk_ta_decrease() on 6");
		msleep(500);

		upmu_set_rg_cs_vth(set_ta_off_current_reg_value);
		pr_debug("mtk_ta_decrease() off 6");
		msleep(50);
		/*patent end*/

		pr_debug("mtk_ta_decrease() end\n");
	}

	return status;
}


static unsigned int charging_get_error_state(void)
{
	return charging_error;
}

static unsigned int charging_set_error_state(void *data)
{
	unsigned int status = STATUS_OK;

	charging_error = *(unsigned int *)(data);
	return status;
}

static unsigned int(*charging_func[CHARGING_CMD_NUMBER]) (void *data);

/*
 *FUNCTION
 *	Internal_chr_control_handler
 *
 *DESCRI
 *	 This function is called to set the charger hw
 *
 *CALLS
 *
 * PARAMETERS
 *	None
 *
 *RETURNS
 *
 * GLOBALS AFFECTED
 *	   None
 */
signed int chr_control_interface_internal(CHARGING_CTRL_CMD cmd, void *data)
{
	static signed int init = -1;

	if (init == -1) {
		init = 0;
		charging_func[CHARGING_CMD_INIT]
			= charging_hw_init;
		charging_func[CHARGING_CMD_DUMP_REGISTER]
			= charging_dump_register;
		charging_func[CHARGING_CMD_ENABLE]
			= charging_enable;
		charging_func[CHARGING_CMD_SET_CV_VOLTAGE]
			= charging_set_cv_voltage;
		charging_func[CHARGING_CMD_GET_CURRENT]
			= charging_get_current;
		charging_func[CHARGING_CMD_SET_CURRENT]
			= charging_set_current;
		charging_func[CHARGING_CMD_GET_CHARGING_STATUS]
			= charging_get_charging_status;
		charging_func[CHARGING_CMD_RESET_WATCH_DOG_TIMER]
			= charging_reset_watch_dog_timer;
		charging_func[CHARGING_CMD_SET_HV_THRESHOLD]
			= charging_set_hv_threshold;
		charging_func[CHARGING_CMD_GET_HV_STATUS]
			= charging_get_hv_status;
		charging_func[CHARGING_CMD_GET_BATTERY_STATUS]
			= charging_get_battery_status;
		charging_func[CHARGING_CMD_GET_CHARGER_DET_STATUS]
			= charging_get_charger_det_status;
		charging_func[CHARGING_CMD_GET_CHARGER_TYPE]
			= charging_get_charger_type;
		charging_func[CHARGING_CMD_SET_PLATFORM_RESET]
			= charging_set_platform_reset;
		charging_func[CHARGING_CMD_GET_PLATFORM_BOOT_MODE]
			= charging_get_platform_boot_mode;
		charging_func[CHARGING_CMD_SET_POWER_OFF]
			= charging_set_power_off;
		charging_func[CHARGING_CMD_GET_CSDAC_FALL_FLAG]
			= charging_get_csdac_full_flag;
		charging_func[CHARGING_CMD_SET_TA_CURRENT_PATTERN]
			= charging_set_ta_current_pattern;
		charging_func[CHARGING_CMD_SET_ERROR_STATE]
			= charging_set_error_state;
	}

	if (cmd < CHARGING_CMD_NUMBER) {
		if (charging_func[cmd] != NULL)
			return charging_func[cmd](data);
	}

	pr_debug("[%s]UNSUPPORT Function: %d\n", __func__, cmd);

	return STATUS_UNSUPPORTED;
}
