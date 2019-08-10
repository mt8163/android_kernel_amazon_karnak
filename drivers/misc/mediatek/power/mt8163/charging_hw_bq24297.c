#include <linux/types.h>
#include <mt-plat/upmu_common.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <mt-plat/mt_boot.h>
#include <mt-plat/battery_common.h>

#include "bq24297.h"

/* ============================================================*/
/*define*/
/* ============================================================*/
#define STATUS_OK	0
#define STATUS_UNSUPPORTED	-1
#define GETARRAYNUM(array) (sizeof(array)/sizeof(array[0]))

/*============================================================*/
/*global variable*/
/*============================================================*/
bool charging_type_det_done = true;

static const unsigned int VBAT_CV_VTH[] = {
	3504000,    3520000,    3536000,    3552000,
	3568000,    3584000,    3600000,    3616000,
	3632000,    3648000,    3664000,    3680000,
	3696000,    3712000,	3728000,    3744000,
	3760000,    3776000,    3792000,    3808000,
	3824000,    3840000,    3856000,    3872000,
	3888000,    3904000,    3920000,    3936000,
	3952000,    3968000,    3984000,    4000000,
	4016000,    4032000,    4048000,    4064000,
	4080000,    4096000,    4112000,    4128000,
	4144000,    4160000,    4176000,    4192000,
	4208000,    4224000,    4240000,    4256000,
	4272000,    4288000,    4304000,    4320000,
	4336000,    4352000,    4368000
};

static const unsigned int CS_VTH[] = {
	51200,  57600,  64000,  70400,
	76800,  83200,  89600,  96000,
	102400, 108800, 115200, 121600,
	128000, 134400, 140800, 147200,
	153600, 160000, 166400, 172800,
	179200, 185600, 192000, 198400,
	204800, 211200, 217600, 224000
};

static const unsigned int INPUT_CS_VTH[] = {
	CHARGE_CURRENT_100_00_MA,  CHARGE_CURRENT_150_00_MA,	CHARGE_CURRENT_500_00_MA,
	CHARGE_CURRENT_900_00_MA,
	CHARGE_CURRENT_1000_00_MA, CHARGE_CURRENT_1500_00_MA,  CHARGE_CURRENT_2000_00_MA,
	CHARGE_CURRENT_MAX
};

static const unsigned int VCDT_HV_VTH[] = {
	BATTERY_VOLT_04_200000_V, BATTERY_VOLT_04_250000_V,	  BATTERY_VOLT_04_300000_V,
	BATTERY_VOLT_04_350000_V,
	BATTERY_VOLT_04_400000_V, BATTERY_VOLT_04_450000_V,	  BATTERY_VOLT_04_500000_V,
	BATTERY_VOLT_04_550000_V,
	BATTERY_VOLT_04_600000_V, BATTERY_VOLT_06_000000_V,	  BATTERY_VOLT_06_500000_V,
	BATTERY_VOLT_07_000000_V,
	BATTERY_VOLT_07_500000_V, BATTERY_VOLT_08_500000_V,	  BATTERY_VOLT_09_500000_V,
	BATTERY_VOLT_10_500000_V
};

/*============================================================*/
/*function prototype*/
/*============================================================*/
static unsigned int charging_error;
static unsigned int charging_get_error_state(void);
static unsigned int charging_set_error_state(void *data);
/*============================================================*/
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

static unsigned int bmt_find_closest_level(const unsigned int *pList, unsigned int number, unsigned int level)
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
		/*return CHARGE_CURRENT_0_00_MA;*/
	} else {
		/*max value in the first element*/
		for (i = 0; i < number; i++) {
			if (pList[i] <= level)
				return pList[i];
		}

		pr_debug("Can't find closest level, large value first\r\n");
		return pList[number - 1];
		/*return CHARGE_CURRENT_0_00_MA;*/
	}
}

static unsigned int charging_hw_init(void *data)
{
	unsigned int status = STATUS_OK;

	upmu_set_rg_bc11_bb_ctrl(1);    /*BC11_BB_CTRL*/
	upmu_set_rg_bc11_rst(1);        /*BC11_RST*/

#ifdef GPIO_CHR_PSEL_PIN
	/*pull PSEL low*/
	mt_set_gpio_mode(GPIO_CHR_PSEL_PIN, GPIO_MODE_GPIO);
	mt_set_gpio_dir(GPIO_CHR_PSEL_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CHR_PSEL_PIN, GPIO_OUT_ZERO);
#endif

#ifdef GPIO_CHR_CE_PIN
	/*pull CE low*/
	mt_set_gpio_mode(GPIO_CHR_CE_PIN, GPIO_MODE_GPIO);
	mt_set_gpio_dir(GPIO_CHR_CE_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CHR_CE_PIN, GPIO_OUT_ZERO);
#endif

	bq24297_set_en_hiz(0x0);
	bq24297_set_vindpm(0x9);	/* VIN DPM check 4.60V */
	bq24297_set_reg_rst(0x0);
	bq24297_set_wdt_rst(0x1);	/* Kick watchdog */
	bq24297_set_sys_min(0x5);	/* Minimum system voltage 3.5V */
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	pr_notice("Kernel:bq24297_set_iprechg(0x3); 384mA\n");
	bq24297_set_iprechg(0x3);	/* Precharge current 384mA */
#else
	bq24297_set_iprechg(0x3);	/* Precharge current 512mA */
#endif
	bq24297_set_iterm(0x1);		/* Termination current 256mA (C/20) */
	pr_notice("Kernel:bq24297_set_iterm(0x1); 256mA\n");

#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	bq24297_set_vreg(0x2C);	/* VREG 4.208V */
#endif
	bq24297_set_batlowv(0x1);	/* BATLOWV 3.0V */
	bq24297_set_vrechg(0x0);	/* VRECHG 0.1V (4.108V) */
	bq24297_set_en_term(0x1);	/* Enable termination */
	bq24297_set_term_stat(0x0);	/* Match ITERM */
	bq24297_set_watchdog(0x1);	/* WDT 40s */
#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	bq24297_set_en_timer(0x0);	/* Disable charge timer */
#endif
	bq24297_set_int_mask(0x1);	/* Disable CHRG fault interrupt */

	return status;
}

static unsigned int charging_dump_register(void *data)
{
	unsigned int status = STATUS_OK;

	pr_debug("charging_dump_register\r\n");

	bq24297_dump_register();

	return status;
}

static unsigned int charging_enable(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int enable = *(unsigned int *)(data);

	if (true == enable) {
		bq24297_set_en_hiz(0x0);
		bq24297_set_chg_config(0x1); /*charger enable*/
	} else {
	#if defined(CONFIG_USB_MTK_HDRC_HCD)
		if (mt_usb_is_device())
	#endif
			bq24297_set_chg_config(0x0);
		if (charging_get_error_state()) {
			pr_debug("[charging_enable] bq24297_set_en_hiz(0x1)\n");
			bq24297_set_en_hiz(0x1);	/* disable power path */
		}
	}
	return status;
}

static unsigned int charging_boost_enable(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int enable = *(unsigned int *)(data);
	pr_debug("%s: enable: %d\n", __func__, enable);
	if (enable) {
		bq24297_set_otg_config(0x1);    /* OTG */
		bq24297_set_boost_lim(0x1);     /* 1.5A on VBUS */
		bq24297_set_en_hiz(0x0);
	} else {
		bq24297_set_otg_config(0x0);
	}
	return status;
}

static unsigned int charging_set_cv_voltage(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned short register_value;
	unsigned int cv_value = *(unsigned int *)(data);

	if (cv_value == BATTERY_VOLT_04_200000_V) {
		/* use nearest value */
		cv_value = 4208000;
	} else if (cv_value == BATTERY_VOLT_04_350000_V) {
		/* use nearest value */
		cv_value = 4352000;
	} else
		cv_value = bmt_find_closest_level(VBAT_CV_VTH, GETARRAYNUM(VBAT_CV_VTH), *(unsigned int *)(data));

	register_value = charging_parameter_to_value(VBAT_CV_VTH, GETARRAYNUM(VBAT_CV_VTH), cv_value);
	bq24297_set_vreg(register_value);

	return status;
}

static unsigned int charging_get_current(void *data)
{
	unsigned int status = STATUS_OK;

	unsigned char ret_val = 0;
	unsigned char ret_force_20pct = 0;

	/*Get current level*/
	bq24297_read_interface(bq24297_CON2, &ret_val, CON2_ICHG_MASK, CON2_ICHG_SHIFT);

	/*Get Force 20% option*/
	bq24297_read_interface(bq24297_CON2, &ret_force_20pct, CON2_FORCE_20PCT_MASK, CON2_FORCE_20PCT_SHIFT);

	/*Parsing*/
	ret_val = (ret_val*64) + 512;

	if (ret_force_20pct == 0)
		*(unsigned int *)data = ret_val;
	else
		*(unsigned int *)data = (int)(ret_val<<1)/10;
	return status;
}

static unsigned int charging_set_current(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;
	unsigned int current_value = *(unsigned int *)data;

	if (current_value == 25600) {
		bq24297_set_force_20pct(0x1);
		bq24297_set_ichg(0xC);
		return status;
	}
	bq24297_set_force_20pct(0x0);

	array_size = GETARRAYNUM(CS_VTH);
	set_chr_current = bmt_find_closest_level(CS_VTH, array_size, current_value);
	register_value = charging_parameter_to_value(CS_VTH, array_size, set_chr_current);
	bq24297_set_ichg(register_value);

	return status;
}

static unsigned int charging_set_input_current(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;

	array_size = GETARRAYNUM(INPUT_CS_VTH);
	set_chr_current = bmt_find_closest_level(INPUT_CS_VTH, array_size, *(unsigned int *)data);
	register_value = charging_parameter_to_value(INPUT_CS_VTH, array_size, set_chr_current);

	bq24297_set_iinlim(register_value);
	return status;
}

#if 0
static unsigned int charging_get_input_current(void *data)
{
	unsigned int register_value;

	register_value = bq24297_get_iinlim();
	*(unsigned int *) data = INPUT_CS_VTH[register_value];
	return STATUS_OK;
}
#endif

static unsigned int charging_get_charging_status(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int ret_val;

	ret_val = bq24297_get_chrg_stat();

	if (ret_val == 0x3)
		*(unsigned int *)data = true;
	else
		*(unsigned int *)data = false;

	return status;
}

static unsigned int charging_reset_watch_dog_timer(void *data)
{
	unsigned int status = STATUS_OK;

	pr_debug("charging_reset_watch_dog_timer\r\n");

	bq24297_set_wdt_rst(0x1); /*Kick watchdog*/

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
	set_hv_voltage = bmt_find_closest_level(VCDT_HV_VTH, array_size, voltage);
	register_value = charging_parameter_to_value(VCDT_HV_VTH, array_size, set_hv_voltage);
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

	*(bool *)(data) = upmu_get_rgs_chrdet();

	return status;
}

static unsigned int charging_get_charger_type(void *data)
{
#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
	*(CHARGER_TYPE *) (data) = STANDARD_HOST;
#else
	CHARGER_TYPE charger_type = CHARGER_UNKNOWN;
	unsigned int ret_val;
	unsigned int vbus_state;
	unsigned char reg_val = 0;
	unsigned int count = 0;

	charging_type_det_done = false;

	if (bq24297_get_pn() == 0x1) {
		pr_debug("[BQ24296] use pmic charger detection\r\n");
		*(CHARGER_TYPE *) (data) = hw_charger_type_detection();
		charging_type_det_done = true;
		return STATUS_OK;
	}

	pr_debug("use BQ24297 charger detection\r\n");

	Charger_Detect_Init();

	while (bq24297_get_pg_stat() == 0) {
		pr_debug("wait pg_state ready.\n");
		count++;
		msleep(20);
		if (count > 500) {
			pr_warn("wait BQ24297 pg_state ready timeout!\n");
			break;
		}

		if (!upmu_get_rgs_chrdet())
			break;
	}

	ret_val = bq24297_get_vbus_stat();

	/* if detection is not finished or non-standard charger detected. */
	if (ret_val == 0x0) {
		count = 0;
		bq24297_set_dpdm_en(1);
		while (bq24297_get_dpdm_status() == 1) {
			count++;
			mdelay(1);
			pr_debug("polling BQ24297 charger detection\r\n");
			if (count > 1000)
				break;
			if (!upmu_get_rgs_chrdet())
				break;
		}
	}

	vbus_state = bq24297_get_vbus_stat();

	/* We might not be able to switch on RG_USB20_BC11_SW_EN in time. */
	/* We detect again to confirm its type */
	if (upmu_get_rgs_chrdet()) {
		count = 0;
		bq24297_set_dpdm_en(1);
		while (bq24297_get_dpdm_status() == 1) {
			count++;
			mdelay(1);
			pr_debug("polling again BQ24297 charger detection\r\n");
			if (count > 1000)
				break;
			if (!upmu_get_rgs_chrdet())
				break;
		}
	}

	ret_val = bq24297_get_vbus_stat();

	if (ret_val != vbus_state)
		pr_warn("Update VBUS state from %d to %d!\n", vbus_state, ret_val);

	switch (ret_val) {
	case 0x1:
		charger_type = STANDARD_HOST;
		break;
	case 0x2:
		charger_type = STANDARD_CHARGER;
		break;
	default:
		charger_type = NONSTANDARD_CHARGER;
		break;
	}

	if (charger_type == STANDARD_CHARGER) {
		bq24297_read_interface(bq24297_CON0, &reg_val, CON0_IINLIM_MASK, CON0_IINLIM_SHIFT);
		if (reg_val < 0x4) {
			pr_debug(
				    "Set to Non-standard charger due to 1A input limit.\r\n");
			charger_type = NONSTANDARD_CHARGER;
		} else if (reg_val == 0x4) {	/* APPLE_1_0A_CHARGER - 1A apple charger */
			pr_debug("Set to APPLE_1_0A_CHARGER.\r\n");
			charger_type = APPLE_1_0A_CHARGER;
		} else if (reg_val == 0x6) {	/* APPLE_2_1A_CHARGER,  2.1A apple charger */
			pr_debug("Set to APPLE_2_1A_CHARGER.\r\n");
			charger_type = APPLE_2_1A_CHARGER;
		}
	}

	Charger_Detect_Release();

	pr_notice("charging_get_charger_type = %d\n", charger_type);

	*(CHARGER_TYPE *)(data) = charger_type;

	charging_type_det_done = true;
	pr_debug("charging_get_charger_type = %d\r\n", *(CHARGER_TYPE *)(data));
#endif
	return STATUS_OK;
}

static unsigned int charging_set_platform_reset(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
#else
	pr_notice("charging_set_platform_reset\n");
	orderly_reboot(true);
#endif
	return status;
}

static unsigned int charging_get_platform_boot_mode(void *data)
{
	unsigned int status = STATUS_OK;

	*(unsigned int *)(data) = get_boot_mode();

	pr_debug("get_boot_mode=%d\n", get_boot_mode());

	return status;
}

static unsigned int charging_set_power_off(void *data)
{
	unsigned int status = STATUS_OK;
#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
#else
	pr_notice("charging_set_power_off\n");
	orderly_poweroff(true);
#endif
	return status;
}

static unsigned int charging_set_ta_current_pattern(void *data)
{
	unsigned int increase = *(unsigned int *)(data);
	unsigned int charging_status = false;

	#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)
	BATTERY_VOLTAGE_ENUM cv_voltage = BATTERY_VOLT_04_340000_V;
	#else
	BATTERY_VOLTAGE_ENUM cv_voltage = BATTERY_VOLT_04_200000_V;
	#endif

	charging_get_charging_status(&charging_status);
	if (false == charging_status) {
		charging_set_cv_voltage(&cv_voltage); /* Set CV */
		bq24297_set_ichg(0x0); /* Set charging current 500ma */
		bq24297_set_chg_config(0x1); /* Enable Charging */
	}

	if (increase == true) {
		bq24297_set_iinlim(0x0); /* 100mA */
		msleep(85);

		bq24297_set_iinlim(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 1");
		msleep(85);

		bq24297_set_iinlim(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 1");
		msleep(85);

		bq24297_set_iinlim(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 2");
		msleep(85);

		bq24297_set_iinlim(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 2");
		msleep(85);

		bq24297_set_iinlim(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 3");
		msleep(281);

		bq24297_set_iinlim(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 3");
		msleep(85);

		bq24297_set_iinlim(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 4");
		msleep(281);

		bq24297_set_iinlim(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 4");
		msleep(85);

		bq24297_set_iinlim(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 5");
		msleep(281);

		bq24297_set_iinlim(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 5");
		msleep(85);

		bq24297_set_iinlim(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 6");
		msleep(485);

		bq24297_set_iinlim(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 6");
		msleep(50);

		pr_debug("mtk_ta_increase() end\n");

		bq24297_set_iinlim(0x2); /* 500mA */
		msleep(200);
	} else {
		bq24297_set_iinlim(0x0); /* 100mA */
		msleep(85);

		bq24297_set_iinlim(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 1");
		msleep(281);

		bq24297_set_iinlim(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 1");
		msleep(85);

		bq24297_set_iinlim(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 2");
		msleep(281);

		bq24297_set_iinlim(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 2");
		msleep(85);

		bq24297_set_iinlim(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 3");
		msleep(281);

		bq24297_set_iinlim(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 3");
		msleep(85);

		bq24297_set_iinlim(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 4");
		msleep(85);

		bq24297_set_iinlim(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 4");
		msleep(85);

		bq24297_set_iinlim(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 5");
		msleep(85);

		bq24297_set_iinlim(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 5");
		msleep(85);

		bq24297_set_iinlim(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 6");
		msleep(485);

		bq24297_set_iinlim(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 6");
		msleep(50);

		pr_debug("mtk_ta_decrease() end\n");

		bq24297_set_iinlim(0x2); /* 500mA */
	}

	return STATUS_OK;
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

static unsigned int charging_get_fault_type(void *data)
{
	unsigned int status = STATUS_OK;

	u8 type = 0;
	type = bq24297_get_fault_type();
	*(unsigned int *)data = type;
	return status;
}

#if 0
static unsigned int charging_enable_powerpath(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int enable = *(unsigned int *) (data);

	if (true == enable)
		bq24297_set_en_hiz(0x0);
	else
		bq24297_set_en_hiz(0x1);

	return status;
}

static unsigned int charging_boost_enable(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int enable = *(unsigned int *) (data);

	if (true == enable) {
		bq24297_set_boost_lim(0x1);	/* 1.5A on VBUS */
		bq24297_set_en_hiz(0x0);
		bq24297_set_chg_config(0);	/* Charge disabled */
		bq24297_set_otg_config(0x1);	/* OTG */
#ifdef CONFIG_POWER_EXT
		bq24297_set_watchdog(0);
#endif
	} else {
		bq24297_set_otg_config(0x0);	/* OTG & Charge disabled */
#ifdef CONFIG_POWER_EXT
		bq24297_set_watchdog(1);
#endif
	}

	return status;
}
#endif

static unsigned int(*charging_func[CHARGING_CMD_NUMBER]) (void *data);

 /*
 *FUNCTION
 *		Internal_chr_control_handler
 *
 *DESCRI
 *		 This function is called to set the charger hw
 *
 *CALLS
 *
 * PARAMETERS
 *		None
 *
 *RETURNS
 *
 * GLOBALS AFFECTED
 *	   None
 */
signed int chr_control_interface(CHARGING_CTRL_CMD cmd, void *data)
{
	static signed int init = -1;

	if (init == -1) {
		init = 0;
		charging_func[CHARGING_CMD_INIT] = charging_hw_init;
		charging_func[CHARGING_CMD_DUMP_REGISTER] = charging_dump_register;
		charging_func[CHARGING_CMD_ENABLE] = charging_enable;
		charging_func[CHARGING_CMD_SET_CV_VOLTAGE] = charging_set_cv_voltage;
		charging_func[CHARGING_CMD_GET_CURRENT] = charging_get_current;
		charging_func[CHARGING_CMD_SET_CURRENT] = charging_set_current;
		charging_func[CHARGING_CMD_SET_INPUT_CURRENT] = charging_set_input_current;
		charging_func[CHARGING_CMD_GET_CHARGING_STATUS] =  charging_get_charging_status;
		charging_func[CHARGING_CMD_RESET_WATCH_DOG_TIMER] = charging_reset_watch_dog_timer;
		charging_func[CHARGING_CMD_SET_HV_THRESHOLD] = charging_set_hv_threshold;
		charging_func[CHARGING_CMD_GET_HV_STATUS] = charging_get_hv_status;
		charging_func[CHARGING_CMD_GET_BATTERY_STATUS] = charging_get_battery_status;
		charging_func[CHARGING_CMD_GET_CHARGER_DET_STATUS] = charging_get_charger_det_status;
		charging_func[CHARGING_CMD_GET_CHARGER_TYPE] = charging_get_charger_type;
		charging_func[CHARGING_CMD_SET_PLATFORM_RESET] = charging_set_platform_reset;
		charging_func[CHARGING_CMD_GET_PLATFORM_BOOT_MODE] = charging_get_platform_boot_mode;
		charging_func[CHARGING_CMD_SET_POWER_OFF] = charging_set_power_off;
		charging_func[CHARGING_CMD_SET_TA_CURRENT_PATTERN] = charging_set_ta_current_pattern;
		charging_func[CHARGING_CMD_SET_ERROR_STATE] = charging_set_error_state;
		charging_func[CHARGING_CMD_BOOST_ENABLE] = charging_boost_enable;
		charging_func[CHARGING_CMD_GET_FAULT_TYPE] = charging_get_fault_type;
	}

	if (cmd < CHARGING_CMD_NUMBER) {
		if (charging_func[cmd] != NULL)
			return charging_func[cmd](data);
	}

	pr_notice("[%s]UNSUPPORT Function: %d\n", __func__, cmd);

	return STATUS_UNSUPPORTED;
}
