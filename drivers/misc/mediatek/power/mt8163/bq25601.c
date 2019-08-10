#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/switch.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include <mt-plat/charging.h>
#include "bq25601.h"

struct bq25601_data_info {
	struct i2c_client	*client;
	struct switch_dev	bq_reg09;
	bool			init_done;
	u8			part_num;
	u8			reg[BQ25601_REG_NUM];
};
static struct bq25601_data_info bq25601_data = {
	.part_num = 0xF,
	.init_done = false,
};
static struct bq25601_data_info *bdata = &bq25601_data;

static DEFINE_MUTEX(bq25601_i2c_access);

int bq25601_read_byte(unsigned char cmd, unsigned char *returnData)
{
	int ret;
	struct i2c_msg msg[2];

	if (!bdata->client) {
		pr_notice("error: access bq25601 before driver ready\n");
		return 0;
	}

	msg[0].addr = bdata->client->addr;
	msg[0].buf = &cmd;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[1].addr = bdata->client->addr;
	msg[1].buf = returnData;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;

	ret = i2c_transfer(bdata->client->adapter, msg, 2);

	if (ret != 2)
		pr_notice("%s: err=%d\n", __func__, ret);

	return ret == 2 ? 1 : 0;
}

int bq25601_write_byte(unsigned char cmd, unsigned char writeData)
{
	char buf[2];
	int ret;

	if (!bdata->client) {
		pr_notice("error: access bq25601 before driver ready\n");
		return 0;
	}

	buf[0] = cmd;
	buf[1] = writeData;

	ret = i2c_master_send(bdata->client, buf, 2);

	if (ret != 2)
		pr_notice("%s: err=%d\n", __func__, ret);

	return ret == 2 ? 1 : 0;

}

/**********************************************************
  *
  *   [Read / Write Function]
  *
  *********************************************************/
int bq25601_read_interface(u8 reg, u8 *val, u8 mask, u8 shift)
{
	u8 reg_val = 0;
	int ret = 0;

	ret = bq25601_read_byte(reg, &reg_val);

	reg_val &= (mask << shift);
	*val = (reg_val >> shift);

	return ret;
}

int bq25601_config_interface(u8 reg, u8 val, u8 mask, u8 shift)
{
	u8 reg_val = 0;
	int ret = 0;

	ret = bq25601_read_byte(reg, &reg_val);

	reg_val &= ~(mask << shift);
	reg_val |= (val << shift);

	ret = bq25601_write_byte(reg, reg_val);

	return ret;
}

/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/
/* CON0---------------------------------------------------- */

void bq25601_set_en_hiz(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON0), (u8) (val),
			(u8) (CON0_EN_HIZ_MASK),
			(u8) (CON0_EN_HIZ_SHIFT));
}

void bq25601_set_en_ichg_mon(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON0), (u8) (val),
			(u8) (CON0_EN_ICHG_MON_MASK),
			(u8) (CON0_EN_ICHG_MON_SHIFT));
}

void bq25601_set_iindpm(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON0), (u8) (val),
			(u8) (CON0_IINDPM_MASK),
			(u8) (CON0_IINDPM_SHIFT));
}

u32 bq25601_get_iindpm(void)
{
	int ret = 0;
	u8 val = 0;

	ret = bq25601_read_interface((u8) (BQ25601_CON0), (&val),
			(u8) (CON0_IINDPM_MASK),
			(u8) (CON0_IINDPM_SHIFT));
	return val;
}

/* CON1---------------------------------------------------- */

void bq25601_set_fpm_dis(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON1), (u8) (val),
			(u8) (CON1_PFM_DIS_MASK),
			(u8) (CON1_PFM_DIS_SHIFT));
}

void bq25601_set_wd_rst(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON1), (u8) (val),
			(u8) (CON1_WD_RST_MASK),
			(u8) (CON1_WD_RST_SHIFT));
}

void bq25601_set_otg_config(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON1), (u8) (val),
			(u8) (CON1_OTG_CONFIG_MASK),
			(u8) (CON1_OTG_CONFIG_SHIFT));
}

void bq25601_set_chg_config(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON1), (u8) (val),
			(u8) (CON1_CHG_CONFIG_MASK),
			(u8) (CON1_CHG_CONFIG_SHIFT));
}

void bq25601_set_sys_min(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON1), (u8) (val),
			(u8) (CON1_SYS_MIN_MASK),
			(u8) (CON1_SYS_MIN_SHIFT));
}

void bq25601_set_min_vbat_sel(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON1), (u8) (val),
			(u8) (CON1_MIN_VBAT_SEL_MASK),
			(u8) (CON1_MIN_VBAT_SEL_SHIFT));
}

/* CON2---------------------------------------------------- */

void bq25601_set_boost_lim(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON2), (u8) (val),
			(u8) (CON2_BOOST_LIM_MASK),
			(u8) (CON2_BOOST_LIM_SHIFT));
}

void bq25601_set_q1_fullon(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON2), (u8) (val),
			(u8) (CON2_Q1_FULLON_MASK),
			(u8) (CON2_Q1_FULLON_SHIFT));
}

void bq25601_set_ichg(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON2), (u8) (val),
			(u8) (CON2_ICHG_MASK),
			(u8) (CON2_ICHG_SHIFT));
}

u32 bq25601_get_ichg(void)
{
	int ret = 0;
	u8 val = 0;

	ret = bq25601_read_interface((u8) (BQ25601_CON2), (&val),
			(u8) (CON2_ICHG_MASK),
			(u8) (CON2_ICHG_SHIFT));
	return val;
}



/* CON3---------------------------------------------------- */

void bq25601_set_iprechg(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON3), (u8) (val),
			(u8) (CON3_IPRECHG_MASK),
			(u8) (CON3_IPRECHG_SHIFT));
}

void bq25601_set_iterm(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON3), (u8) (val),
			(u8) (CON3_ITERM_MASK),
			(u8) (CON3_ITERM_SHIFT));
}

/* CON4---------------------------------------------------- */

void bq25601_set_vreg(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON4), (u8) (val),
			(u8) (CON4_VREG_MASK),
			(u8) (CON4_VREG_SHIFT));
}

void bq25601_set_topoff_timer(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON4), (u8) (val),
		       (u8) (CON4_TOPFF_TIMER_MASK),
		       (u8) (CON4_TOPFF_TIMER_SHIFT));
}

void bq25601_set_vrechg(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON4), (u8) (val),
			(u8) (CON4_VRECHG_MASK),
			(u8) (CON4_VRECHG_SHIFT));
}

/* CON5---------------------------------------------------- */

void bq25601_set_en_term(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON5), (u8) (val),
			(u8) (CON5_EN_TERM_MASK),
			(u8) (CON5_EN_TERM_SHIFT));
}

void bq25601_set_watchdog(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON5), (u8) (val),
		       (u8) (CON5_WATCHDOG_MASK),
		       (u8) (CON5_WATCHDOG_SHIFT));
}

void bq25601_set_en_timer(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON5), (u8) (val),
		       (u8) (CON5_EN_TIMER_MASK),
		       (u8) (CON5_EN_TIMER_SHIFT));
}

void bq25601_set_chg_timer(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON5), (u8) (val),
		       (u8) (CON5_CHG_TIMER_MASK),
		       (u8) (CON5_CHG_TIMER_SHIFT));
}

void bq25601_set_treg(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON5), (u8) (val),
			(u8) (CON5_TREG_MASK),
			(u8) (CON5_TREG_SHIFT));
}

void bq25601_set_jeita_iset(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON5), (u8) (val),
			(u8) (CON5_JEITA_ISET_MASK),
			(u8) (CON5_JEITA_ISET_SHIFT));
}

/* CON6---------------------------------------------------- */

void bq25601_set_ovp(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON6), (u8) (val),
			(u8) (CON6_OVP_MASK),
			(u8) (CON6_OVP_SHIFT));
}

void bq25601_set_boostv(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON6), (u8) (val),
			(u8) (CON6_BOOSTV_MASK),
			(u8) (CON6_BOOSTV_SHIFT));
}

void bq25601_set_vindpm(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON6), (u8) (val),
			(u8) (CON6_VINDPM_MASK),
			(u8) (CON6_VINDPM_SHIFT));
}

/* CON7---------------------------------------------------- */
void bq25601_set_iindet_en(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON7), (u8) (val),
			(u8) (CON7_IINDET_EN_MASK),
			(u8) (CON7_IINDET_EN_SHIFT));
}

void bq25601_set_tmr2x_en(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON7), (u8) (val),
			(u8) (CON7_TMR2X_EN_MASK),
			(u8) (CON7_TMR2X_EN_SHIFT));
}

void bq25601_set_batfet_dis(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON7), (u8) (val),
			(u8) (CON7_BATFET_DIS_MASK),
			(u8) (CON7_BATFET_DIS_SHIFT));
}

void bq25601_set_jeita_vset(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON7), (u8) (val),
			(u8) (CON7_JEITA_VSET_MASK),
			(u8) (CON7_JEITA_VSET_SHIFT));
}

void bq25601_set_batfet_dly(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON7), (u8) (val),
			(u8) (CON7_BATFET_DLY_MASK),
			(u8) (CON7_BATFET_DLY_SHIFT));
}

void bq25601_set_batfet_rst_en(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON7), (u8) (val),
			(u8) (CON7_BATFET_RST_EN_MASK),
			(u8) (CON7_BATFET_RST_EN_SHIFT));
}

void bq25601_set_vdpm_bat_track(u32 val)
{
	int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON7), (u8) (val),
			(u8) (CON7_VDPM_BAT_TRACK_MASK),
			(u8) (CON7_VDPM_BAT_TRACK_SHIFT));
}

/* CON8---------------------------------------------------- */

u32 bq25601_get_vbus_stat(void)
{
	int ret = 0;
	u8 val = 0;

	ret = bq25601_read_interface((u8) (BQ25601_CON8), (&val),
			(u8) (CON8_VBUS_STAT_MASK),
			(u8) (CON8_VBUS_STAT_SHIFT));
	return val;
}

u32 bq25601_get_chrg_stat(void)
{
	int ret = 0;
	u8 val = 0;

	ret = bq25601_read_interface((u8) (BQ25601_CON8), (&val),
			(u8) (CON8_CHRG_STAT_MASK),
			(u8) (CON8_CHRG_STAT_SHIFT));
	return val;
}

u32 bq25601_get_pg_stat(void)
{
	int ret = 0;
	u8 val = 0;

	ret = bq25601_read_interface((u8) (BQ25601_CON8), (&val),
			(u8) (CON8_PG_STAT_MASK),
			(u8) (CON8_PG_STAT_SHIFT));
	return val;
}

u32 bq25601_get_therm_stat(void)
{
	int ret = 0;
	u8 val = 0;

	ret = bq25601_read_interface((u8) (BQ25601_CON8), (&val),
			(u8) (CON8_THERM_STAT_MASK),
			(u8) (CON8_THERM_STAT_SHIFT));
	return val;
}

u32 bq25601_get_vsys_stat(void)
{
	int ret = 0;
	u8 val = 0;

	ret = bq25601_read_interface((u8) (BQ25601_CON8), (&val),
			(u8) (CON8_VSYS_STAT_MASK),
			(u8) (CON8_VSYS_STAT_SHIFT));
	return val;
}

/* CON10---------------------------------------------------- */
u32 bq25601_get_vbus_gd(void)
{
	int ret = 0;
	u8 val = 0;

	ret = bq25601_read_interface((u8) (BQ25601_CON10), (&val),
			(u8) (CON10_VBUS_GD_MASK),
			(u8) (CON10_VBUS_GD_SHIFT));
	return val;
}

u32 bq25601_get_vindpm_stat(void)
{
	int ret = 0;
	u8 val = 0;

	ret = bq25601_read_interface((u8) (BQ25601_CON10), (&val),
			(u8) (CON10_VINDPM_STAT_MASK),
			(u8) (CON10_VINDPM_STAT_SHIFT));
	return val;
}

u32 bq25601_get_iindpm_stat(void)
{
	int ret = 0;
	u8 val = 0;

	ret = bq25601_read_interface((u8) (BQ25601_CON10), (&val),
			(u8) (CON10_IINDPM_STAT_MASK),
			(u8) (CON10_IINDPM_STAT_SHIFT));
	return val;
}

/* CON11---------------------------------------------------- */

void bq25601_set_reg_rst(u8 val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((u8) (BQ25601_CON11),
			(u8) (val),
			(u8) (CON11_REG_RST_MASK),
			(u8) (CON11_REG_RST_SHIFT));
}

u8 bq25601_get_pn(void)
{
	int ret = 0;
	u8 val = 0;

	if (bdata->init_done)
		return bdata->part_num;

	ret = bq25601_read_interface((u8) (BQ25601_CON11),
			(u8 *) (&val),
			(u8) (CON11_PN_MASK),
			(u8) (CON11_PN_SHIFT));

	return val;
}

/* [External Function] -------------------------------------- */

bool is_bq25601_exist(void)
{
	if (bq25601_get_pn() == BQ25601_PN)
		return true;

	return false;
}

/* [Internal Function] -------------------------------------- */
static u8 bq25601_get_reg9_fault_type(u8 reg9)
{
	u8 ret = 0, type = 0;

	if (reg9 & WATCHDOG_FAULT) {
		ret = BQ_WATCHDOG_FAULT;
	} else if (reg9 & BOOST_FAULT) {
		ret = BQ_BOOST_FAULT;
	} else if (reg9 & CHGR_FAULT) {
		type = (reg9 & CHGR_FAULT) >> CON9_CHRG_FAULT_SHIFT;
		if (type == CHGR_FAULT_INPUT)
			ret = BQ_CHRG_INPUT_FAULT;
		else if (type == CHGR_FAULT_THERMAL)
			ret = BQ_CHRG_THERMAL_FAULT;
		else if (type == CHGR_FAULT_TIMER)
			ret = BQ_CHRG_TIMER_EXPIRATION_FAULT;
	} else if (reg9 & BAT_FAULT)
		ret = BQ_BAT_FAULT;
	else if (reg9 & NTC_FAULT) {
		type = (reg9 & NTC_FAULT) >> CON9_NTC_FAULT_SHIFT;
		if (type == NTC_FAULT_WARM)
			ret = BQ_NTC_WARM_FAULT;
		else if (type == NTC_FAULT_COOL)
			ret = BQ_NTC_COOL_FAULT;
		else if (type == NTC_FAULT_COLD)
			ret = BQ_NTC_COLD_FAULT;
		else if (type == NTC_FAULT_HOT)
			ret = BQ_NTC_HOT_FAULT;
	}

	pr_info("%s: Fault type: %d\n", __func__, ret);
	return ret;
}

u8 bq25601_get_fault_type(void)
{
	unsigned char type = 0;

	type = bq25601_get_reg9_fault_type(bdata->reg[BQ25601_CON9]);
	return type;
}

#define OTG_CONFIG BIT(5)
void bq25601_polling_reg09(void)
{
	int i, i2;
	u8 reg1 = 0, reg9 = 0, err_code = 0;

	for (i2 = i = 0; i < 4 && i2 < 10; i++, i2++) {
		bq25601_read_byte((u8)(BQ25601_CON9), &reg9);
		bdata->reg[BQ25601_CON9] = reg9;
		/* BOOST_FAULT bit */
		if ((reg9 & BOOST_FAULT) != 0) {
			/* Disable OTG */
			bq25601_read_byte(BQ25601_CON1, &reg1);
			reg1 &= ~OTG_CONFIG;
			bq25601_read_byte(BQ25601_CON1, &reg1);
			msleep(20);
			/* Enable OTG */
			reg1 |= OTG_CONFIG;
			bq25601_write_byte(1, reg1);
		}

		if (reg9 != 0) {
			/*
			 * Keep on polling if REG09 is not 0.
			 * This is to filter noises
			 */
			i = 0;
			/* need filter fault type here */
			err_code =  bq25601_get_reg9_fault_type(reg9);
			switch_set_state(&bdata->bq_reg09,  err_code);
		}
		msleep(20);
	}

	/* send normal fault state to UI */
	switch_set_state(&bdata->bq_reg09, BQ_NORMAL_FAULT);
}

static irqreturn_t ops_bq25601_int_handler(int irq, void *dev_id)
{
	bq25601_polling_reg09();
	return IRQ_HANDLED;
}

void bq25601_dump_register(void)
{
	int i = 0;

	for (i = 0; i < BQ25601_REG_NUM; i++) {
		bq25601_read_byte(i, &bdata->reg[i]);
		pr_debug("%s: [0x%x]=0x%x\n", __func__, i, bdata->reg[i]);
	}
}

static ssize_t show_dump_register(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i = 0;
	char temp_info[200] = "";

	for (i = 0; i < BQ25601_REG_NUM; i++) {
		bq25601_read_byte(i, &bdata->reg[i]);
		sprintf(temp_info, "reg[%x]=0x%x\n", i, bdata->reg[i]);
		strcat(buf, temp_info);
	}
	return strlen(buf);
}

static DEVICE_ATTR(dump_register, 0440, show_dump_register, NULL);

static void bq25601_parse_customer_setting(void)
{
#ifdef CONFIG_OF
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_chg_en;
	struct pinctrl_state *pinctrl_interrupt_init;

	pinctrl = devm_pinctrl_get(&bdata->client->dev);
	if (IS_ERR(pinctrl)) {
		pr_debug("[%s]Cannot find bq24297 pinctrl, err=%d\n",
			__func__, (int)PTR_ERR(pinctrl));
		return;
	}

	pinctrl_chg_en = pinctrl_lookup_state(pinctrl, "bq24297_chg_en");
	if (IS_ERR(pinctrl_chg_en)) {
		pr_err("[%s]Cannot find bq24297_chg_en state, err=%d\n",
			__func__, (int)PTR_ERR(pinctrl_chg_en));
	} else
		pinctrl_select_state(pinctrl, pinctrl_chg_en);

	pinctrl_interrupt_init =
		pinctrl_lookup_state(pinctrl, "bq24297_interrupt_init");
	if (IS_ERR(pinctrl_interrupt_init)) {
		pr_err("[%s]Can't find bq24297_interrupt_init state, err=%d\n",
			__func__, (int)PTR_ERR(pinctrl_interrupt_init));
	} else
		pinctrl_select_state(pinctrl, pinctrl_interrupt_init);

	pr_debug("[%s]pinctrl_select_state done\n", __func__);
#endif
}

unsigned char g_reg_value_bq25601 = 0;
static ssize_t show_bq25601_access(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	pr_info("[%s] 0x%x\n", __func__, g_reg_value_bq25601);
	return sprintf(buf, "0x%x\n", g_reg_value_bq25601);
}

static ssize_t store_bq25601_access(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL, *addr, *val;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	pr_info("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		pr_info("[%s] buf: %s, size: %zu\n", __func__, buf, size);
		/*reg_address = kstrtoul(buf, 16, &pvalue);*/

		pvalue = (char *)buf;
		if (size > 3) {
			addr = strsep(&pvalue, " ");
			ret = kstrtou32(addr, 16,
					(unsigned int *)&reg_address);
		} else
			ret = kstrtou32(pvalue, 16,
					(unsigned int *)&reg_address);

		if (size > 3) {
			val = strsep(&pvalue, " ");
			ret = kstrtou32(val, 16, (unsigned int *)&reg_value);

			pr_info("[%s] write REG 0x%x with value 0x%x\n",
				__func__, reg_address, reg_value);
			ret = bq25601_config_interface(reg_address,
					reg_value, 0xFF, 0x0);
		} else {
			ret = bq25601_read_interface(reg_address,
					&g_reg_value_bq25601, 0xFF, 0x0);
			pr_info("[%s] read REG 0x%x with value 0x%x !\n",
				__func__, reg_address, g_reg_value_bq25601);
			pr_info(
			"[%s] Please use \"cat bq25601_access\" to get value\n",
				__func__);
		}
	}
	return size;
}

static DEVICE_ATTR(bq25601_access, 0664,
		show_bq25601_access, store_bq25601_access);

static int bq25601_user_space_probe(struct platform_device *dev)
{
	int ret = 0;

	pr_debug("%s entry\n", __func__);
	ret = device_create_file(&(dev->dev), &dev_attr_bq25601_access);
	ret = device_create_file(&(dev->dev), &dev_attr_dump_register);

	return 0;
}

struct platform_device bq25601_user_space_device = {
	.name = "bq25601-user",
	.id = -1,
};

static struct platform_driver bq25601_user_space_driver = {
	.probe = bq25601_user_space_probe,
	.driver = {
		.name = "bq25601-user",
	},
};

static int bq25601_driver_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;

	pr_info("[%s] entry\n", __func__);

	bdata->client = client;

	bdata->part_num = bq25601_get_pn();
	if (bdata->part_num == BQ25601_PN) {
		pr_info("%s: BQ25601 device is found.\n", __func__);
	} else {
		pr_info("%s: BQ25601 device is not found, PN=0x%x, exit\n",
			__func__, bdata->part_num);
		bdata->init_done = true;
		ret = -ENODEV;
		goto out_0;
	}

	bq25601_parse_customer_setting();
	bq25601_dump_register();

	bdata->bq_reg09.name = "bq25601_reg09";
	bdata->bq_reg09.index = 0;
	bdata->bq_reg09.state = 0;
	ret = switch_dev_register(&bdata->bq_reg09);
	if (ret < 0) {
		pr_err("[%s] switch_dev_register() error(%d)\n", __func__, ret);
		goto out_0;
	}

	/*bq25601 user space access interface*/
	ret = platform_device_register(&bq25601_user_space_device);
	if (ret) {
		pr_err("[%s] Unable to device register(%d)\n", __func__, ret);
		goto out_0;
	}
	ret = platform_driver_register(&bq25601_user_space_driver);
	if (ret) {
		pr_err("[%s] Unable to register driver (%d)\n", __func__, ret);
		goto out_1;
	}


	if (client->irq > 0) {
		pr_debug("[%s] enable interrupt: %d\n", __func__, client->irq);
		ret = request_threaded_irq(client->irq, NULL,
				ops_bq25601_int_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"ops_bq25601_int_handler", NULL);
		if (ret) {
			pr_err("[%s] request_threaded_irq failed err = %d\n",
				__func__, ret);
			goto out_2;
		}
	}

	bdata->init_done = true;
	pr_info("[%s] success\n", __func__);

	return 0;
out_2:
	platform_driver_unregister(&bq25601_user_space_driver);
out_1:
	platform_device_unregister(&bq25601_user_space_device);
out_0:
	return ret;
}

static int bq25601_remove(struct i2c_client *client)
{
	bq25601_set_chg_config(0x0);
	platform_device_unregister(&bq25601_user_space_device);
	platform_driver_unregister(&bq25601_user_space_driver);
	return 0;
}

static void bq25601_shutdown(struct i2c_client *client)
{
	pr_notice("[%s] driver shutdown\n", __func__);
	bq25601_set_otg_config(0x0);
	bq25601_set_chg_config(0x0);
}

#ifdef CONFIG_PM_SLEEP
static int bq25601_pm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	pr_info("[%s] client->irq(%d)\n", __func__, client->irq);
	if (client->irq > 0)
		disable_irq(client->irq);

	return 0;
}

static int bq25601_pm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	pr_info("[%s] client->irq(%d)\n", __func__, client->irq);
	if (client->irq > 0)
		enable_irq(client->irq);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(bq25601_pm_ops,
			bq25601_pm_suspend, bq25601_pm_resume);


static const struct i2c_device_id bq25601_i2c_id[] = { {"bq25601", 0}, {} };

/* compatible must be use same with bq24297.c, "ti,bq24297" */
#ifdef CONFIG_OF
static const struct of_device_id bq25601_of_match[] = {
	{ .compatible = "ti,bq24297", },
	{ },
};
MODULE_DEVICE_TABLE(of, bq25601_of_match);
#else
static const struct of_device_id bq25601_of_match[] = {
	{ },
};
#endif

static struct i2c_driver bq25601_driver = {
	.probe		= bq25601_driver_probe,
	.remove		= bq25601_remove,
	.shutdown	= bq25601_shutdown,
	.id_table	= bq25601_i2c_id,
	.driver = {
		.name		= "bq25601",
		.owner		= THIS_MODULE,
		.pm		= &bq25601_pm_ops,
		.of_match_table	= of_match_ptr(bq25601_of_match),
	},
};

module_i2c_driver(bq25601_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq25601 Driver");
MODULE_AUTHOR("Lu Xiaoguang<luxiaogu@amazon.com>");
