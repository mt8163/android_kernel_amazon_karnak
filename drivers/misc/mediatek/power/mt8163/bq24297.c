#include <linux/types.h>
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
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
#include "bq24297.h"

/**********************************************************
  *
  *   [I2C Slave Setting]
  *
  *********************************************************/
#define bq24297_SLAVE_ADDR_WRITE   0xD6
#define bq24297_SLAVE_ADDR_READ    0xD7

static struct i2c_client *new_client;
static struct switch_dev bq24297_reg09;
static const struct i2c_device_id bq24297_i2c_id[] = { {"bq24297", 0}, {} };

bool chargin_hw_init_done = false;
static int bq24297_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);
static unsigned int part_num = 0xF;

#ifdef CONFIG_OF
static const struct of_device_id bq24297_id[] = {
		{ .compatible = "ti,bq24297" },
		{},
};

MODULE_DEVICE_TABLE(of, bq24297_id);
#endif


static int bq24297_driver_suspend(struct i2c_client *client, pm_message_t mesg)
{
	pr_info("[bq24297_driver_suspend] client->irq(%d)\n", client->irq);
	if (client->irq > 0)
		disable_irq(client->irq);

	return 0;
}

static int bq24297_driver_resume(struct i2c_client *client)
{
	pr_info("[bq24297_driver_resume] client->irq(%d)\n", client->irq);
	if (client->irq > 0)
		enable_irq(client->irq);

	return 0;
}

static void bq24297_shutdown(struct i2c_client *client)
{
	pr_notice("[bq24297_shutdown] driver shutdown\n");
	bq24297_set_chg_config(0x0);
}
static struct i2c_driver bq24297_driver = {
		.driver = {
				.name    = "bq24297",
#ifdef CONFIG_OF
				.of_match_table = of_match_ptr(bq24297_id),
#endif
		},
		.probe       = bq24297_driver_probe,
		.id_table    = bq24297_i2c_id,
		.suspend = bq24297_driver_suspend,
		.resume = bq24297_driver_resume,
		.shutdown    = bq24297_shutdown,
};

/**********************************************************
 *
 *[Global Variable]
 *
 *********************************************************/
unsigned char bq24297_reg[bq24297_REG_NUM] = {0};

static DEFINE_MUTEX(bq24297_i2c_access);
/**********************************************************
 *
 *	[I2C Function For Read/Write bq24297]
 *
 *********************************************************/
int bq24297_read_byte(unsigned char cmd, unsigned char *returnData)
{
	int ret;

	struct i2c_msg msg[2];

	if (!new_client) {
		pr_notice("error: access bq24297 before driver ready\n");
		return 0;
	}

	msg[0].addr = new_client->addr;
	msg[0].buf = &cmd;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[1].addr = new_client->addr;
	msg[1].buf = returnData;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;

	ret = i2c_transfer(new_client->adapter, msg, 2);

	if (ret != 2)
		pr_notice("%s: err=%d\n", __func__, ret);

	return ret == 2 ? 1 : 0;
}

int bq24297_write_byte(unsigned char cmd, unsigned char writeData)
{
	char buf[2];
	int ret;

	if (!new_client) {
		pr_notice("error: access bq24297 before driver ready\n");
		return 0;
	}

	buf[0] = cmd;
	buf[1] = writeData;

	ret = i2c_master_send(new_client, buf, 2);

	if (ret != 2)
		pr_notice("%s: err=%d\n", __func__, ret);

	return ret == 2 ? 1 : 0;

}

/**********************************************************
  *
  *   [Read / Write Function]
  *
  *********************************************************/
unsigned int bq24297_read_interface(unsigned char RegNum, unsigned char *val, unsigned char MASK,
				  unsigned char SHIFT)
{
	unsigned char bq24297_reg = 0;
	int ret = 0;

	pr_debug("--------------------------------------------------\n");

	ret = bq24297_read_byte(RegNum, &bq24297_reg);

	pr_debug("[bq24297_read_interface] Reg[%x]=0x%x\n", RegNum, bq24297_reg);

	bq24297_reg &= (MASK << SHIFT);
	*val = (bq24297_reg >> SHIFT);

	pr_debug("[bq24297_read_interface] val=0x%x\n", *val);

	return ret;
}

unsigned int bq24297_config_interface(unsigned char RegNum, unsigned char val, unsigned char MASK,
				    unsigned char SHIFT)
{
	unsigned char bq24297_reg = 0;
	int ret = 0;

	pr_debug("--------------------------------------------------\n");

	ret = bq24297_read_byte(RegNum, &bq24297_reg);
	pr_debug("[bq24297_config_interface] Reg[%x]=0x%x\n", RegNum, bq24297_reg);

	bq24297_reg &= ~(MASK << SHIFT);
	bq24297_reg |= (val << SHIFT);

	ret = bq24297_write_byte(RegNum, bq24297_reg);
	pr_debug("[bq24297_config_interface] write Reg[%x]=0x%x\n", RegNum, bq24297_reg);

	/* Check */
	/* bq24297_read_byte(RegNum, &bq24297_reg); */
	/* pr_debug("[bq24297_config_interface] Check Reg[%x]=0x%x\n", RegNum, bq24297_reg); */

	return ret;
}

/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/
/* CON0---------------------------------------------------- */

void bq24297_set_en_hiz(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON0),
				       (unsigned char) (val),
				       (unsigned char) (CON0_EN_HIZ_MASK),
				       (unsigned char) (CON0_EN_HIZ_SHIFT)
	    );
}

void bq24297_set_vindpm(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON0),
				       (unsigned char) (val),
				       (unsigned char) (CON0_VINDPM_MASK),
				       (unsigned char) (CON0_VINDPM_SHIFT)
	    );
}

void bq24297_set_iinlim(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON0),
				       (unsigned char) (val),
				       (unsigned char) (CON0_IINLIM_MASK),
				       (unsigned char) (CON0_IINLIM_SHIFT)
	    );
}

unsigned int bq24297_get_iinlim(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq24297_read_interface((unsigned char) (bq24297_CON0),
						(unsigned char *) (&val),
						(unsigned char) (CON0_IINLIM_MASK),
						(unsigned char) (CON0_IINLIM_SHIFT)
	    );
	return val;
}

/* CON1---------------------------------------------------- */

void bq24297_set_reg_rst(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_REG_RST_MASK),
				       (unsigned char) (CON1_REG_RST_SHIFT)
	    );
}

void bq24297_set_wdt_rst(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_WDT_RST_MASK),
				       (unsigned char) (CON1_WDT_RST_SHIFT)
	    );
}

void bq24297_set_otg_config(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_OTG_CONFIG_MASK),
				       (unsigned char) (CON1_OTG_CONFIG_SHIFT)
	    );
}

void bq24297_set_chg_config(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_CHG_CONFIG_MASK),
				       (unsigned char) (CON1_CHG_CONFIG_SHIFT)
	    );
}

void bq24297_set_sys_min(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_SYS_MIN_MASK),
				       (unsigned char) (CON1_SYS_MIN_SHIFT)
	    );
}

void bq24297_set_boost_lim(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_BOOST_LIM_MASK),
				       (unsigned char) (CON1_BOOST_LIM_SHIFT)
	    );
}

/* CON2---------------------------------------------------- */

void bq24297_set_ichg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_ICHG_MASK), (unsigned char) (CON2_ICHG_SHIFT)
	    );
}

void bq24297_set_force_20pct(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_FORCE_20PCT_MASK),
				       (unsigned char) (CON2_FORCE_20PCT_SHIFT)
	    );
}

/* CON3---------------------------------------------------- */

void bq24297_set_iprechg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON3),
				       (unsigned char) (val),
				       (unsigned char) (CON3_IPRECHG_MASK),
				       (unsigned char) (CON3_IPRECHG_SHIFT)
	    );
}

void bq24297_set_iterm(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON3),
				       (unsigned char) (val),
				       (unsigned char) (CON3_ITERM_MASK), (unsigned char) (CON3_ITERM_SHIFT)
	    );
}

/* CON4---------------------------------------------------- */

void bq24297_set_vreg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON4),
				       (unsigned char) (val),
				       (unsigned char) (CON4_VREG_MASK), (unsigned char) (CON4_VREG_SHIFT)
	    );
}

void bq24297_set_batlowv(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON4),
				       (unsigned char) (val),
				       (unsigned char) (CON4_BATLOWV_MASK),
				       (unsigned char) (CON4_BATLOWV_SHIFT)
	    );
}

void bq24297_set_vrechg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON4),
				       (unsigned char) (val),
				       (unsigned char) (CON4_VRECHG_MASK),
				       (unsigned char) (CON4_VRECHG_SHIFT)
	    );
}

/* CON5---------------------------------------------------- */

void bq24297_set_en_term(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_EN_TERM_MASK),
				       (unsigned char) (CON5_EN_TERM_SHIFT)
	    );
}

void bq24297_set_term_stat(unsigned int val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON5),
						(unsigned char) (val),
						(unsigned char) (CON5_TERM_STAT_MASK),
						(unsigned char) (CON5_TERM_STAT_SHIFT)
	    );
}

void bq24297_set_watchdog(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_WATCHDOG_MASK),
				       (unsigned char) (CON5_WATCHDOG_SHIFT)
	    );
}

void bq24297_set_en_timer(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_EN_TIMER_MASK),
				       (unsigned char) (CON5_EN_TIMER_SHIFT)
	    );
}

void bq24297_set_chg_timer(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_CHG_TIMER_MASK),
				       (unsigned char) (CON5_CHG_TIMER_SHIFT)
	    );
}

/* CON6---------------------------------------------------- */

void bq24297_set_treg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_TREG_MASK),
				       (unsigned char) (CON6_TREG_SHIFT)
	    );
}

/* CON7---------------------------------------------------- */

unsigned int bq24297_get_dpdm_status(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq24297_read_interface((unsigned char) (bq24297_CON7),
						(unsigned char *) (&val),
						(unsigned char) (CON7_DPDM_EN_MASK),
						(unsigned char) (CON7_DPDM_EN_SHIFT)
	    );
	return val;
}

void bq24297_set_dpdm_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON7),
						(unsigned char) (val),
						(unsigned char) (CON7_DPDM_EN_MASK),
						(unsigned char) (CON7_DPDM_EN_SHIFT)
	    );
}

void bq24297_set_tmr2x_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_TMR2X_EN_MASK),
				       (unsigned char) (CON7_TMR2X_EN_SHIFT)
	    );
}

void bq24297_set_batfet_disable(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_BATFET_Disable_MASK),
				       (unsigned char) (CON7_BATFET_Disable_SHIFT)
	    );
}

void bq24297_set_int_mask(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq24297_config_interface((unsigned char) (bq24297_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_INT_MASK_MASK),
				       (unsigned char) (CON7_INT_MASK_SHIFT)
	    );
}

/* CON8---------------------------------------------------- */

unsigned int bq24297_get_system_status(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq24297_read_interface((unsigned char) (bq24297_CON8),
								(unsigned char *) (&val),
								(unsigned char) (0xFF),
								(unsigned char) (0x0)
	    );
	return val;
}

unsigned int bq24297_get_vbus_stat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq24297_read_interface((unsigned char) (bq24297_CON8),
								(unsigned char *) (&val),
								(unsigned char) (CON8_VBUS_STAT_MASK),
								(unsigned char) (CON8_VBUS_STAT_SHIFT)
	    );
	return val;
}

unsigned int bq24297_get_chrg_stat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq24297_read_interface((unsigned char) (bq24297_CON8),
							(&val),
							(unsigned char) (CON8_CHRG_STAT_MASK),
							(unsigned char) (CON8_CHRG_STAT_SHIFT)
	    );
	return val;
}

unsigned int bq24297_get_pg_stat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq24297_read_interface((unsigned char) (bq24297_CON8),
							(unsigned char *) (&val),
							(unsigned char) (CON8_PG_STAT_MASK),
							(unsigned char) (CON8_PG_STAT_SHIFT)
	    );
	return val;
}

unsigned int bq24297_get_vsys_stat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq24297_read_interface((unsigned char) (bq24297_CON8),
							(&val),
							(unsigned char) (CON8_VSYS_STAT_MASK),
							(unsigned char) (CON8_VSYS_STAT_SHIFT)
	    );
	return val;
}

/* CON10---------------------------------------------------- */

unsigned int bq24297_get_pn(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	if (chargin_hw_init_done)
		return part_num;
	else if (0xF == part_num)
		pr_notice("[%s]BQ24297 driver is NOT ready!!\r\n", __func__);

	ret = bq24297_read_interface((unsigned char) (bq24297_CON10),
							(unsigned char *) (&val),
							(unsigned char) (CON10_PN_MASK),
							(unsigned char) (CON10_PN_SHIFT)
	    );
	return val;
}

/* CON11---------------------------------------------------- */

#if defined(CONFIG_MTK_BQ25601_SUPPORT)
unsigned int bq24297_get_con11(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq24297_read_interface((unsigned char) (bq24297_CON11),
					(unsigned char *) (&val),
					(unsigned char) (CON11_PN_MASK),
					(unsigned char) (CON11_PN_SHIFT)
	    );
	return val;
}
#endif

/**********************************************************
  *
  *   [Internal Function]
  *
 *********************************************************/
static unsigned char bq24297_get_reg9_fault_type(unsigned char reg9_fault)
{
	unsigned char ret = 0;

	if ((reg9_fault & (CON9_OTG_FAULT_MASK << CON9_OTG_FAULT_SHIFT)) != 0) {
		ret = BQ_OTG_FAULT;
	} else if ((reg9_fault & (CON9_CHRG_FAULT_MASK << CON9_CHRG_FAULT_SHIFT)) != 0) {
		if ((reg9_fault & (CON9_CHRG_INPUT_FAULT_MASK << CON9_CHRG_FAULT_SHIFT)) != 0)
			ret = BQ_CHRG_INPUT_FAULT;
		else if ((reg9_fault &
			  (CON9_CHRG_THERMAL_SHUTDOWN_FAULT_MASK << CON9_CHRG_FAULT_SHIFT)) != 0)
			ret = BQ_CHRG_THERMAL_FAULT;
		else if ((reg9_fault &
			  (CON9_CHRG_TIMER_EXPIRATION_FAULT_MASK << CON9_CHRG_FAULT_SHIFT)) != 0)
			ret = BQ_CHRG_TIMER_EXPIRATION_FAULT;
	} else if ((reg9_fault & (CON9_BAT_FAULT_MASK << CON9_BAT_FAULT_SHIFT)) != 0)
		ret = BQ_BAT_FAULT;
	else if ((reg9_fault & (CON9_NTC_FAULT_MASK << CON9_NTC_FAULT_SHIFT)) != 0) {
		if ((reg9_fault & (CON9_NTC_COLD_FAULT_MASK << CON9_NTC_FAULT_SHIFT)) != 0)
			ret = BQ_NTC_COLD_FAULT;
		else if ((reg9_fault & (CON9_NTC_HOT_FAULT_MASK << CON9_NTC_FAULT_SHIFT)) != 0)
			ret = BQ_NTC_HOT_FAULT;
	}
	return ret;
}

u8 bq24297_get_fault_type(void)
{
	unsigned char type = 0;

	type = bq24297_get_reg9_fault_type(bq24297_reg[bq24297_CON9]);
	return type;
}

void bq24297_polling_reg09(void)
{
	int i, i2;
	unsigned char reg1;

	for (i2 = i = 0; i < 4 && i2 < 10; i++, i2++) {
		bq24297_read_byte((unsigned char) (bq24297_CON9), &bq24297_reg[bq24297_CON9]);
		if ((bq24297_reg[bq24297_CON9] & 0x40) != 0) {	/* OTG_FAULT bit */
			/* Disable OTG */
			bq24297_read_byte(1, &reg1);
			reg1 &= ~0x20;	/* 0 = OTG Disable */
			bq24297_write_byte(1, reg1);
			msleep(20);
			/* Enable OTG */
			reg1 |= 0x20;	/* 1 = OTG Enable */
			bq24297_write_byte(1, reg1);
		}
		if (bq24297_reg[bq24297_CON9] != 0) {
			i = 0;	/* keep on polling if reg9 is not 0. This is to filter noises */
			/* need filter fault type here */
			switch_set_state(&bq24297_reg09,
					 bq24297_get_reg9_fault_type(bq24297_reg[bq24297_CON9]));
		}
		msleep(20);
	}
	/* send normal fault state to UI */
	switch_set_state(&bq24297_reg09, BQ_NORMAL_FAULT);
}

static irqreturn_t ops_bq24297_int_handler(int irq, void *dev_id)
{
	bq24297_polling_reg09();
	return IRQ_HANDLED;
}

void bq24297_dump_register(void)
{
	int i = 0;

	pr_debug("[bq24297] ");
	for (i = 0; i < bq24297_REG_NUM; i++) {
		bq24297_read_byte(i, &bq24297_reg[i]);
		pr_debug("[0x%x]=0x%x ", i, bq24297_reg[i]);
	}
	pr_debug("\n");
}

static ssize_t show_dump_register(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i = 0;
	char temp_info[200] = "";

	for (i = 0; i < bq24297_REG_NUM; i++) {
		bq24297_read_byte(i, &bq24297_reg[i]);
		sprintf(temp_info, "reg[%x]=0x%x\n", i, bq24297_reg[i]);
		strcat(buf, temp_info);
	}
	return strlen(buf);
}

static DEVICE_ATTR(dump_register, 0440, show_dump_register, NULL);

static void bq24297_parse_customer_setting(void)
{
#ifdef CONFIG_OF
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_chg_en;
	struct pinctrl_state *pinctrl_interrupt_init;

	pinctrl = devm_pinctrl_get(&new_client->dev);
	if (IS_ERR(pinctrl)) {
		pr_debug("[%s]Cannot find bq24297 pinctrl, err=%d\n",
			__func__, (int)PTR_ERR(pinctrl));
		return;
	}

	pinctrl_chg_en = pinctrl_lookup_state(pinctrl, "bq24297_chg_en");
	if (IS_ERR(pinctrl_chg_en)) {
		pr_debug("[%s]Cannot find bq24297_chg_en state, err=%d\n",
			__func__, (int)PTR_ERR(pinctrl_chg_en));
	} else
		pinctrl_select_state(pinctrl, pinctrl_chg_en);

	pinctrl_interrupt_init = pinctrl_lookup_state(pinctrl, "bq24297_interrupt_init");
	if (IS_ERR(pinctrl_interrupt_init)) {
		pr_debug("[%s]Cannot find bq24297_interrupt_init state, err=%d\n",
			__func__, (int)PTR_ERR(pinctrl_interrupt_init));
	} else
		pinctrl_select_state(pinctrl, pinctrl_interrupt_init);

	pr_debug("[%s]pinctrl_select_state done\n", __func__);
#endif
}

/**********************************************************
  *
  *   [platform_driver API]
  *
  *********************************************************/
unsigned char g_reg_value_bq24297 = 0;
static ssize_t show_bq24297_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_notice("[show_bq24297_access] 0x%x\n", g_reg_value_bq24297);
	return sprintf(buf, "%u\n", g_reg_value_bq24297);
}

static ssize_t store_bq24297_access(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL, *addr, *val;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	pr_notice("[store_bq24297_access]\n");

	if (buf != NULL && size != 0) {
		pr_notice("[store_bq24297_access] buf is %s and size is %zu\n", buf, size);
		/*reg_address = kstrtoul(buf, 16, &pvalue);*/

		pvalue = (char *)buf;
		if (size > 3) {
			addr = strsep(&pvalue, " ");
			ret = kstrtou32(addr, 16, (unsigned int *)&reg_address);
		} else
			ret = kstrtou32(pvalue, 16, (unsigned int *)&reg_address);

		if (size > 3) {
			val = strsep(&pvalue, " ");
			ret = kstrtou32(val, 16, (unsigned int *)&reg_value);

			pr_notice("[store_bq24297_access] write bq24297 reg 0x%x with value 0x%x !\n",
			    reg_address, reg_value);
			ret = bq24297_config_interface(reg_address, reg_value, 0xFF, 0x0);
		} else {
			ret = bq24297_read_interface(reg_address, &g_reg_value_bq24297, 0xFF, 0x0);
			pr_notice("[store_bq24297_access] read bq24297 reg 0x%x with value 0x%x !\n",
			    reg_address, g_reg_value_bq24297);
			pr_notice(
			    "[store_bq24297_access] Please use \"cat bq24297_access\" to get value\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(bq24297_access, 0664, show_bq24297_access, store_bq24297_access);	/* 664 */

static int bq24297_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	pr_debug("bq24297_user_space_probe!\n");
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_bq24297_access);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_dump_register);

	return 0;
}

struct platform_device bq24297_user_space_device = {
	.name = "bq24297-user",
	.id = -1,
};

static struct platform_driver bq24297_user_space_driver = {
	.probe = bq24297_user_space_probe,
	.driver = {
		   .name = "bq24297-user",
		   },
};

static int bq24297_driver_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;

	pr_notice("[bq24297_driver_probe]\n");

	new_client = client;

#if defined(CONFIG_MTK_BQ25601_SUPPORT)
	/* BQ25601 detetion
	 * Try to read CON11, if we get PN of BQ25601
	 * exit from this driver
	 */
	part_num = bq24297_get_con11();
	if (part_num == BQ25601_PN) {
		pr_notice("%s: BQ25601 device is found, exit\n", __func__);
		chargin_hw_init_done = true;
		new_client = NULL;
		return -ENODEV;
	}
#endif

	part_num = bq24297_get_pn();

	if (part_num == 0x3) {
		pr_notice("BQ24297 device is found.\n");
	} else if (part_num == 0x1) {
		pr_notice("BQ24296 device is found.\n");
	} else {
		pr_notice("No BQ24297 device part number is found.\n");
		chargin_hw_init_done = true;
		return -ENODEV;
	}

	/*bq24297 user space access interface*/
	ret = platform_device_register(&bq24297_user_space_device);
	if (ret) {
		pr_notice(
		"****[bq24297_init] Unable to device register(%d)\n", ret);
		return ret;
	}
	ret = platform_driver_register(&bq24297_user_space_driver);
	if (ret) {
		pr_notice(
		"****[bq24297_init] Unable to register driver (%d)\n", ret);
		return ret;
	}

	bq24297_dump_register();

	bq24297_parse_customer_setting();

	bq24297_reg09.name = "bq24297_reg09";
	bq24297_reg09.index = 0;
	bq24297_reg09.state = 0;
	ret = switch_dev_register(&bq24297_reg09);

	if (ret < 0)
		pr_notice(
		"[bq24297_driver_probe] switch_dev_register() error(%d)\n",
		ret);

	if (client->irq > 0) {

		pr_debug("[bq24297_driver_probe] enable interrupt: %d\n",
			client->irq);

		/* make sure we clean REG9 before enable fault interrupt */
		bq24297_read_byte(
		(unsigned char) (bq24297_CON9), &bq24297_reg[9]);

		if (bq24297_reg[9] != 0)
			bq24297_polling_reg09();

		/* Disable CHRG fault interrupt */
		bq24297_set_int_mask(0x1);
		ret = request_threaded_irq(client->irq,
			NULL, ops_bq24297_int_handler, IRQF_TRIGGER_FALLING |
			   IRQF_ONESHOT, "ops_bq24297_int_handler", NULL);
		if (ret)
			pr_notice(
			"[bq24297_driver_probe] fault interrupt registration failed err = %d\n",
			ret);
	}

	chargin_hw_init_done = true;

	return ret;
}
static int __init bq24297_init(void)
{
		pr_notice("[bq24297_init] init start\n");

#ifndef CONFIG_OF
		i2c_register_board_info(BQ24297_BUSNUM, &i2c_bq24297, 1);
#endif

		if (i2c_add_driver(&bq24297_driver) != 0)
			pr_notice("[bq24297_init] failed to register bq24297 i2c driver.\n");
		else
			pr_notice("[bq24297_init] Success to register bq24297 i2c driver.\n");

		return 0;
}

static void __exit bq24297_exit(void)
{
		i2c_del_driver(&bq24297_driver);
}
module_init(bq24297_init);
module_exit(bq24297_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq24297 Driver");
MODULE_AUTHOR("YT Lee<yt.lee@mediatek.com>");
