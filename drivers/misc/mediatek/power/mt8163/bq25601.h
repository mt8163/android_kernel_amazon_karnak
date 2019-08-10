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

/*****************************************************************************
*
* Filename:
* ---------
*   bq25601.h
*
* Project:
* --------
*   Android
*
* Description:
* ------------
*   bq25601 header file
*
* Author:
* -------
*
****************************************************************************/

#ifndef _BQ25601_SW_H_
#define _BQ25601_SW_H_

#define BQ25601_CON0		0x00
#define BQ25601_CON1		0x01
#define BQ25601_CON2		0x02
#define BQ25601_CON3		0x03
#define BQ25601_CON4		0x04
#define BQ25601_CON5		0x05
#define BQ25601_CON6		0x06
#define BQ25601_CON7		0x07
#define BQ25601_CON8		0x08
#define BQ25601_CON9		0x09
#define BQ25601_CON10		0x0A
#define BQ25601_CON11		0x0B
#define BQ25601_REG_NUM		12

#define BQ25601_PN		0x2


/**********************************************************
  *
  *   [MASK/SHIFT]
  *
  *********************************************************/
/* CON0 */
#define CON0_EN_HIZ_MASK		0x01
#define CON0_EN_HIZ_SHIFT		7

#define CON0_EN_ICHG_MON_MASK		0x3
#define CON0_EN_ICHG_MON_SHIFT		5

#define CON0_IINDPM_MASK		0x1F
#define CON0_IINDPM_SHIFT		0

/* CON1 */
#define CON1_PFM_DIS_MASK		0x01
#define CON1_PFM_DIS_SHIFT		7

#define CON1_WD_RST_MASK		0x01
#define CON1_WD_RST_SHIFT		6

#define CON1_OTG_CONFIG_MASK		0x01
#define CON1_OTG_CONFIG_SHIFT		5

#define CON1_CHG_CONFIG_MASK		0x01
#define CON1_CHG_CONFIG_SHIFT		4

#define CON1_SYS_MIN_MASK		0x07
#define CON1_SYS_MIN_SHIFT		1

#define CON1_MIN_VBAT_SEL_MASK		0x01
#define CON1_MIN_VBAT_SEL_SHIFT		0

/* CON2 */
#define CON2_BOOST_LIM_MASK		0x01
#define CON2_BOOST_LIM_SHIFT		7

#define CON2_Q1_FULLON_MASK		0x01
#define CON2_Q1_FULLON_SHIFT		6

#define CON2_ICHG_MASK			0x3F
#define CON2_ICHG_SHIFT			0

/* CON3 */
#define CON3_IPRECHG_MASK		0x0F
#define CON3_IPRECHG_SHIFT		4

#define CON3_ITERM_MASK			0x0F
#define CON3_ITERM_SHIFT		0

/* CON4 */
#define CON4_VREG_MASK			0x1F
#define CON4_VREG_SHIFT			3

#define CON4_TOPFF_TIMER_MASK		0x3
#define CON4_TOPFF_TIMER_SHIFT		1

#define CON4_VRECHG_MASK		0x01
#define CON4_VRECHG_SHIFT		0

/* CON5 */
#define CON5_EN_TERM_MASK		0x01
#define CON5_EN_TERM_SHIFT		7

#define CON5_WATCHDOG_MASK		0x03
#define CON5_WATCHDOG_SHIFT		4

#define CON5_EN_TIMER_MASK		0x01
#define CON5_EN_TIMER_SHIFT		3

#define CON5_CHG_TIMER_MASK		0x01
#define CON5_CHG_TIMER_SHIFT		2

#define CON5_TREG_MASK			0x01
#define CON5_TREG_SHIFT			1

#define CON5_JEITA_ISET_MASK		0x01
#define CON5_JEITA_ISET_SHIFT		0

/* CON6 */
#define CON6_OVP_MASK			0x03
#define CON6_OVP_SHIFT			6

#define CON6_BOOSTV_MASK		0x03
#define CON6_BOOSTV_SHIFT		4

#define CON6_VINDPM_MASK		0x0f
#define CON6_VINDPM_SHIFT		0

/* CON7 */
#define CON7_IINDET_EN_MASK		0x01
#define CON7_IINDET_EN_SHIFT		7

#define CON7_TMR2X_EN_MASK		0x01
#define CON7_TMR2X_EN_SHIFT		6

#define CON7_BATFET_DIS_MASK		0x01
#define CON7_BATFET_DIS_SHIFT		5

#define CON7_JEITA_VSET_MASK		0x01
#define CON7_JEITA_VSET_SHIFT		4

#define CON7_BATFET_DLY_MASK		0x01
#define CON7_BATFET_DLY_SHIFT		3

#define CON7_BATFET_RST_EN_MASK		0x01
#define CON7_BATFET_RST_EN_SHIFT	2

#define CON7_VDPM_BAT_TRACK_MASK	0x03
#define CON7_VDPM_BAT_TRACK_SHIFT	0

/* CON8 */
#define CON8_VBUS_STAT_MASK		0x07
#define CON8_VBUS_STAT_SHIFT		5

#define CON8_CHRG_STAT_MASK		0x03
#define CON8_CHRG_STAT_SHIFT		3

#define CON8_PG_STAT_MASK		0x01
#define CON8_PG_STAT_SHIFT		2

#define CON8_THERM_STAT_MASK		0x01
#define CON8_THERM_STAT_SHIFT		1

#define CON8_VSYS_STAT_MASK		0x01
#define CON8_VSYS_STAT_SHIFT		0

/* CON9 */
#define CON9_WATCHDOG_FAULT_MASK	0x01
#define CON9_WATCHDOG_FAULT_SHIFT	7

#define CON9_BOOST_FAULT_MASK		0x01
#define CON9_BOOST_FAULT_SHIFT		6

#define CON9_CHRG_FAULT_MASK		0x03
#define CON9_CHRG_FAULT_SHIFT		4

#define CON9_BAT_FAULT_MASK		0x01
#define CON9_BAT_FAULT_SHIFT		3

#define CON9_NTC_FAULT_MASK		0x07
#define CON9_NTC_FAULT_SHIFT		0

/* CON10 */
#define CON10_VBUS_GD_MASK		0x01
#define CON10_VBUS_GD_SHIFT		7

#define CON10_VINDPM_STAT_MASK		0x01
#define CON10_VINDPM_STAT_SHIFT		6

#define CON10_IINDPM_STAT_MASK		0x01
#define CON10_IINDPM_STAT_SHIFT		5

#define CON10_TOPOFF_ACTIVE_MASK	0x01
#define CON10_TOPOFF_ACTIVE_SHIFT	3

#define CON10_ACOV_STAT_MASK		0x01
#define CON10_ACOV_STAT_SHIFT		2

#define CON10_VINDPM_INT_MASK_MASK	0x01
#define CON10_VINDPM_INT_MASK_SHIFT	1

#define CON10_IINDPM_INT_MASK_MASK	0x01
#define CON10_IINDPM_INT_MASK_SHIFT	0

/* CON11 */
#define CON11_REG_RST_MASK		0x01
#define CON11_REG_RST_SHIFT		7

#define CON11_PN_MASK			0x0f
#define CON11_PN_SHIFT			3

#define CON11_DEV_REV_MASK		0x03
#define CON11_DEV_REV_SHIFT		0

/* Mask/Bit helpers */
#define _BQ_MASK(BITS, POS) \
	((unsigned char)(((1 << (BITS)) - 1) << (POS)))
#define BQ_MASK(LEFT_BIT_POS, RIGHT_BIT_POS) \
	_BQ_MASK((LEFT_BIT_POS) - (RIGHT_BIT_POS) + 1, (RIGHT_BIT_POS))


/* Fault type of REG09 */
#define WATCHDOG_FAULT BIT(7)
#define BOOST_FAULT BIT(6)
#define CHGR_FAULT BQ_MASK(5, 4)
#define BAT_FAULT BIT(3)
#define NTC_FAULT BQ_MASK(2, 0)


enum CHGR_FAULT_TYPE {
	CHGR_FAULT_NORMAL = 0x0,
	CHGR_FAULT_INPUT = 0x1,
	CHGR_FAULT_THERMAL = 0x2,
	CHGR_FAULT_TIMER = 0x3,
};

enum NTC_FAULT_TYPE {
	NTC_FAULT_NORMAL = 0x0,
	NTC_FAULT_WARM = 0x2,
	NTC_FAULT_COOL = 0x3,
	NTC_FAULT_COLD = 0x5,
	NTC_FAULT_HOT = 0x6,
};


/* REG09 report status */
enum BQ_FAULT {
	BQ_NORMAL_FAULT = 0,
	BQ_WATCHDOG_FAULT,
	BQ_BOOST_FAULT,
	BQ_CHRG_NORMAL_FAULT,
	BQ_CHRG_INPUT_FAULT,
	BQ_CHRG_THERMAL_FAULT,
	BQ_CHRG_TIMER_EXPIRATION_FAULT,
	BQ_BAT_FAULT,
	BQ_NTC_WARM_FAULT,
	BQ_NTC_COOL_FAULT,
	BQ_NTC_COLD_FAULT,
	BQ_NTC_HOT_FAULT,
	BQ_FAULT_MAX
};

/* CON0---------------------------------------------------- */
void bq25601_set_en_hiz(u32 val);
void bq25601_set_en_ichg_mon(u32 val);
void bq25601_set_iindpm(u32 val);
u32 bq25601_get_iindpm(void);
/* CON1---------------------------------------------------- */
void bq25601_set_fpm_dis(u32 val);
void bq25601_set_wd_rst(u32 val);
void bq25601_set_otg_config(u32 val);
void bq25601_set_chg_config(u32 val);
void bq25601_set_sys_min(u32 val);
void bq25601_set_min_vbat_sel(u32 val);
/* CON2---------------------------------------------------- */
void bq25601_set_boost_lim(u32 val);
void bq25601_set_q1_fullon(u32 val);
void bq25601_set_ichg(u32 val);
u32 bq25601_get_ichg(void);
/* CON3---------------------------------------------------- */
void bq25601_set_iprechg(u32 val);
void bq25601_set_iterm(u32 val);
/* CON4---------------------------------------------------- */
void bq25601_set_vreg(u32 val);
void bq25601_set_topoff_timer(u32 val);
void bq25601_set_vrechg(u32 val);
/* CON5---------------------------------------------------- */
void bq25601_set_en_term(u32 val);
void bq25601_set_watchdog(u32 val);
void bq25601_set_en_timer(u32 val);
void bq25601_set_chg_timer(u32 val);
void bq25601_set_treg(u32 val);
void bq25601_set_jeita_iset(u32 val);
/* CON6---------------------------------------------------- */
void bq25601_set_ovp(u32 val);
void bq25601_set_boostv(u32 val);
void bq25601_set_vindpm(u32 val);
/* CON7---------------------------------------------------- */
void bq25601_set_iindet_en(u32 val);
void bq25601_set_tmr2x_en(u32 val);
void bq25601_set_batfet_dis(u32 val);
void bq25601_set_jeita_vset(u32 val);
void bq25601_set_batfet_dly(u32 val);
void bq25601_set_batfet_rst_en(u32 val);
void bq25601_set_vdpm_bat_track(u32 val);
/* CON8---------------------------------------------------- */
u32 bq25601_get_vbus_stat(void);
u32 bq25601_get_chrg_stat(void);
u32 bq25601_get_pg_stat(void);
u32 bq25601_get_therm_stat(void);
u32 bq25601_get_vsys_stat(void);
/* CON10---------------------------------------------------- */
u32 bq25601_get_vbus_gd(void);
u32 bq25601_get_vindpm_stat(void);
u32 bq25601_get_iindpm_stat(void);
/* CON11---------------------------------------------------- */
void bq25601_set_reg_rst(u8 val);
u8 bq25601_get_pn(void);

void bq25601_dump_register(void);
u8 bq25601_get_fault_type(void);


#endif	/* _BQ25601_SW_H_ */
