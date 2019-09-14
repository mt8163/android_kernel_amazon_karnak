#include <linux/types.h>
#include <mt-plat/upmu_common.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <mt-plat/mt_boot.h>
#include <mt-plat/battery_common.h>

#include "bq25601.h"

/* ============================================================*/
/*define*/
/* ============================================================*/
#define STATUS_OK	0
#define STATUS_UNSUPPORTED	-1
#define GETARRAYNUM(array) (sizeof(array)/sizeof(array[0]))

/*============================================================*/
/*global variable*/
/*============================================================*/

static const u32 VBAT_CV_VTH[] = {
	3856000, 3888000, 3920000, 3952000,
	3984000, 4016000, 4048000, 4080000,
	4112000, 4144000, 4176000, 4208000,
	4240000, 4272000, 4304000, 4336000,
	4368000, 4400000, 4432000, 4464000,
	4496000, 4528000, 4560000, 4592000,
	4624000
};

static const u32 CS_VTH[] = {
	0, 6000, 12000, 18000,
	24000, 30000, 36000, 42000,
	48000, 54000, 60000, 66000,
	72000, 78000, 84000, 90000,
	96000, 102000, 108000, 114000,
	120000, 126000, 132000, 138000,
	144000, 150000, 156000, 162000,
	168000, 174000, 180000, 186000,
	192000, 198000, 204000, 210000,
	216000, 222000, 228000, 234000,
	240000, 246000, 252000, 258000,
	264000, 270000, 276000, 282000,
	288000, 294000, 300000
};

static const u32 INPUT_CS_VTH[] = {
	10000, 20000, 30000, 40000,
	50000, 60000, 70000, 80000,
	90000, 100000, 110000, 120000,
	130000, 140000, 150000, 160000,
	170000, 180000, 190000, 200000,
	210000, 220000, 230000, 240000,
	250000, 260000, 270000, 280000,
	290000, 300000, 310000, 320000
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

/*============================================================*/
/*function prototype*/
/*============================================================*/
static unsigned int charging_error;
static unsigned int charging_get_error_state(void);
static unsigned int charging_set_error_state(void *data);
/*
 * finds the index of the closest value in the array. If there are two that
 * are equally close, the lower index will be returned
 */
static u32 find_closest_in_array(const u32 *arr, u32 len, u32 val)
{
	int i, closest = 0;

	pr_debug("%s: vaule: %d\n", __func__, val);
	if (len == 0)
		return closest;
	for (i = 0; i < len; i++)
		if (abs(val - arr[i]) < abs(val - arr[closest]))
			closest = i;

	pr_debug("%s: closest index: %d\n", __func__, closest);
	return closest;
}

static unsigned int charging_hw_init(void *data)
{
	unsigned int status = STATUS_OK;

	pr_debug("%s\n", __func__);
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

	/*
	 * TODO: need confirm with Ti
	 * BQ25601 need disable BATFET_RST_EN otherwise BATFET will be reset
	 * after 8s if charger is not plugged in, then power of system will
	 * be cut down.
	 */
	bq25601_set_batfet_rst_en(0);

	bq25601_set_en_hiz(0x0);
	bq25601_set_vindpm(0x5);	/*VINDPM 4400 mV*/
	bq25601_set_reg_rst(0x0);
	bq25601_set_wd_rst(0x1);	/* Kick watchdog */
	bq25601_set_sys_min(0x5);	/* Minimum system voltage 3.5V */
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	bq25601_set_iprechg(0x5);	/* Precharge current 360mA */
#else
	bq25601_set_iprechg(0x8);	/* Precharge current 540mA */
#endif
	bq25601_set_iterm(0x3);		/* Termination current 240mA (C/20)*/

#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	bq25601_set_vreg(0xB);		/* VREG 4.208V */
#endif
	bq25601_set_vrechg(0x0);	/* VRECHG 0.1V (4.108V) */
	bq25601_set_en_term(0x1);	/* Enable termination */
	bq25601_set_watchdog(0x1);	/* WDT 40s */
#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	bq25601_set_en_timer(0x0);	/* Disable charge timer */
#endif

	return status;
}

static unsigned int charging_dump_register(void *data)
{
	unsigned int status = STATUS_OK;

	pr_debug("charging_dump_register\r\n");

	bq25601_dump_register();

	return status;
}

static unsigned int charging_enable(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int enable = *(unsigned int *)(data);

	pr_debug("%s: enable: %d\n", __func__, enable);
	if (true == enable) {
		bq25601_set_en_hiz(0x0);
		/*charger enable*/
		bq25601_set_chg_config(0x1);
		/* Force to disable OTG */
		bq25601_set_otg_config(0x0);
	} else {
#if defined(CONFIG_USB_MTK_HDRC_HCD)
		if (mt_usb_is_device())
#endif
			bq25601_set_chg_config(0x0);
		if (charging_get_error_state()) {
			/* disable power path */
			bq25601_set_en_hiz(0x1);
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
		bq25601_set_otg_config(0x1); /* OTG */
		bq25601_set_boost_lim(0x1);  /* limit current 1.2A on VBUS */
		bq25601_set_en_hiz(0x0);
	} else {
		bq25601_set_otg_config(0x0);
	}

	return status;
}

static unsigned int charging_set_cv_voltage(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned short register_value;
	unsigned int cv_value = *(unsigned int *)(data);

	register_value = find_closest_in_array(
			VBAT_CV_VTH, GETARRAYNUM(VBAT_CV_VTH), cv_value);
	pr_debug("%s: register_value = 0x%x\n", __func__, register_value);
	bq25601_set_vreg(register_value);

	return status;
}

static unsigned int charging_get_current(void *data)
{
	u32 status = STATUS_OK;
	u32 data_val = 0;
	u8 ret_val = 0;

	ret_val = (u8) bq25601_get_ichg();

	data_val = (ret_val * 60);
	*(u32 *) data = data_val;

	return status;
}

static unsigned int charging_set_current(void *data)
{
	u32 status = STATUS_OK;
	u32 register_value;
	u32 set_chr_current = *(u32 *) data;

	register_value = find_closest_in_array(
				CS_VTH, GETARRAYNUM(CS_VTH), set_chr_current);
	pr_debug("%s: register_value = 0x%x\n", __func__, register_value);
	bq25601_set_ichg(register_value);

	return status;
}

static unsigned int charging_set_input_current(void *data)
{
	u32 status = STATUS_OK;
	u32 set_chr_current = *(u32 *) data;
	u32 register_value;

	register_value = find_closest_in_array(
				INPUT_CS_VTH, GETARRAYNUM(INPUT_CS_VTH),
				set_chr_current);

	pr_debug("%s: register_value = 0x%x\n", __func__, register_value);
	bq25601_set_iindpm(register_value);
	return status;
}

static unsigned int charging_get_charging_status(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int ret_val;

	ret_val = bq25601_get_chrg_stat();

	if (ret_val == 0x3)
		*(unsigned int *)data = true;
	else
		*(unsigned int *)data = false;

	return status;
}

static unsigned int charging_reset_watch_dog_timer(void *data)
{
	unsigned int status = STATUS_OK;

	pr_debug("%s\n", __func__);
	/*Kick watchdog*/
	bq25601_set_wd_rst(0x1);

	return status;
}

static unsigned int charging_set_hv_threshold(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned short register_value;
	unsigned int set_hv_voltage = *(unsigned int *)(data);

	register_value = find_closest_in_array(VCDT_HV_VTH,
				GETARRAYNUM(VCDT_HV_VTH), set_hv_voltage);
	pr_debug("%s: register_value = 0x%x\n", __func__, register_value);
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
	*(CHARGER_TYPE *) (data) = hw_charger_type_detection();
	return STATUS_OK;
}

static unsigned int charging_set_platform_reset(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
#else
	pr_info("%s\n", __func__);
	orderly_reboot(true);
#endif
	return status;
}

static unsigned int charging_get_platform_boot_mode(void *data)
{
	unsigned int status = STATUS_OK;

	*(unsigned int *)(data) = get_boot_mode();

	pr_debug("%s: get_boot_mode=%d\n", __func__, get_boot_mode());

	return status;
}

static unsigned int charging_set_power_off(void *data)
{
	unsigned int status = STATUS_OK;
#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
#else
	pr_info("%s\n", __func__);
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
		bq25601_set_ichg(0x0); /* Set charging current 500ma */
		bq25601_set_chg_config(0x1); /* Enable Charging */
	}

	if (increase == true) {
		bq25601_set_iindpm(0x0); /* 100mA */
		msleep(85);

		bq25601_set_iindpm(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 1");
		msleep(85);

		bq25601_set_iindpm(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 1");
		msleep(85);

		bq25601_set_iindpm(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 2");
		msleep(85);

		bq25601_set_iindpm(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 2");
		msleep(85);

		bq25601_set_iindpm(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 3");
		msleep(281);

		bq25601_set_iindpm(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 3");
		msleep(85);

		bq25601_set_iindpm(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 4");
		msleep(281);

		bq25601_set_iindpm(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 4");
		msleep(85);

		bq25601_set_iindpm(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 5");
		msleep(281);

		bq25601_set_iindpm(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 5");
		msleep(85);

		bq25601_set_iindpm(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 6");
		msleep(485);

		bq25601_set_iindpm(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 6");
		msleep(50);

		pr_debug("mtk_ta_increase() end\n");

		bq25601_set_iindpm(0x2); /* 500mA */
		msleep(200);
	} else {
		bq25601_set_iindpm(0x0); /* 100mA */
		msleep(85);

		bq25601_set_iindpm(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 1");
		msleep(281);

		bq25601_set_iindpm(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 1");
		msleep(85);

		bq25601_set_iindpm(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 2");
		msleep(281);

		bq25601_set_iindpm(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 2");
		msleep(85);

		bq25601_set_iindpm(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 3");
		msleep(281);

		bq25601_set_iindpm(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 3");
		msleep(85);

		bq25601_set_iindpm(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 4");
		msleep(85);

		bq25601_set_iindpm(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 4");
		msleep(85);

		bq25601_set_iindpm(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 5");
		msleep(85);

		bq25601_set_iindpm(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 5");
		msleep(85);

		bq25601_set_iindpm(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 6");
		msleep(485);

		bq25601_set_iindpm(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 6");
		msleep(50);

		pr_debug("mtk_ta_decrease() end\n");

		bq25601_set_iindpm(0x2); /* 500mA */
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
	unsigned char type;

	type = bq25601_get_fault_type();
	*(unsigned char *)data = type;
	return status;
}

static unsigned int charging_get_sw_aicl_support(void *data)
{
	unsigned int status = STATUS_OK;

	/* This information is for SW AICL, LSB of IINLIM is 100mA
	 * between 1A and 2A, this chip can support SW AICL algorithm.
	 */
	*(u32 *) data = 1;
	return status;
}


static unsigned int charging_get_vbus_status(void *data)
{
	unsigned int status = STATUS_OK;
	u32 val;

	*(bool *)(data) = bq25601_get_vbus_stat();
	val = bq25601_get_vbus_stat();
	pr_debug("%s: VBUS_STAT: 0x%x\n", __func__, val);
	switch (val) {
	case 0x7:
		*(unsigned int *)(data) = VBUS_STAT_OTG;
		break;
	case 0x2:
		*(unsigned int *)(data) = VBUS_STAT_AC;
		break;
	case 0x1:
		*(unsigned int *)(data) = VBUS_STAT_SDP;
		break;
	default:
		*(unsigned int *)(data) = VBUS_STAT_NONE;
		break;
	}

	return status;
}


static unsigned int(*charging_func[CHARGING_CMD_NUMBER]) (void *data);

 /*
 *FUNCTION
 *		bq25601_control_interface
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
signed int bq25601_control_interface(CHARGING_CTRL_CMD cmd, void *data)
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
		charging_func[CHARGING_CMD_SET_INPUT_CURRENT]
			= charging_set_input_current;
		charging_func[CHARGING_CMD_GET_CHARGING_STATUS]
			=  charging_get_charging_status;
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
		charging_func[CHARGING_CMD_SET_TA_CURRENT_PATTERN]
			= charging_set_ta_current_pattern;
		charging_func[CHARGING_CMD_SET_ERROR_STATE]
			= charging_set_error_state;
		charging_func[CHARGING_CMD_BOOST_ENABLE]
			= charging_boost_enable;
		charging_func[CHARGING_CMD_GET_FAULT_TYPE]
			= charging_get_fault_type;
		charging_func[CHARGING_CMD_GET_SW_AICL_SUPPORT]
			= charging_get_sw_aicl_support;
		charging_func[CHARGING_CMD_GET_VBUS_STAT]
			= charging_get_vbus_status;
	}

	if (cmd < CHARGING_CMD_NUMBER) {
		if (charging_func[cmd] != NULL)
			return charging_func[cmd](data);
	}

	pr_info("[%s]UNSUPPORT Function: %d\n", __func__, cmd);

	return STATUS_UNSUPPORTED;
}
