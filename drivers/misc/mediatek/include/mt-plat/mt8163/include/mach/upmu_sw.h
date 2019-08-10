#ifndef _MT_PMIC_UPMU_SW_H_
#define _MT_PMIC_UPMU_SW_H_

#include <mach/upmu_hw.h>

/*#define SOC_BY_AUXADC*/
/*fg auxadc*/
/*#define SOC_BY_HW_FG*/
#define SOC_BY_SW_FG /*oam*/

#ifdef CONFIG_MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION
#define CONFIG_DIS_CHECK_BATTERY
#endif

/*
 * Low battery level define
 */
enum LOW_BATTERY_LEVEL {
	LOW_BATTERY_LEVEL_0 = 0,
	LOW_BATTERY_LEVEL_1 = 1,
	LOW_BATTERY_LEVEL_2 = 2
};

enum LOW_BATTERY_PRIO {
	LOW_BATTERY_PRIO_CPU_B      = 0,
	LOW_BATTERY_PRIO_CPU_L      = 1,
	LOW_BATTERY_PRIO_GPU        = 2,
	LOW_BATTERY_PRIO_MD         = 3,
	LOW_BATTERY_PRIO_MD5        = 4,
	LOW_BATTERY_PRIO_FLASHLIGHT = 5,
	LOW_BATTERY_PRIO_VIDEO      = 6,
	LOW_BATTERY_PRIO_WIFI       = 7,
	LOW_BATTERY_PRIO_BACKLIGHT  = 8
};

extern void (*low_battery_callback)(enum LOW_BATTERY_LEVEL);
extern void register_low_battery_notify(void (*low_battery_callback)(enum LOW_BATTERY_LEVEL),
		enum LOW_BATTERY_PRIO prio_val);


/*
 * Battery OC level define
 */
enum BATTERY_OC_LEVEL {
	BATTERY_OC_LEVEL_0 = 0,
	BATTERY_OC_LEVEL_1 = 1
};

enum BATTERY_OC_PRIO {
	BATTERY_OC_PRIO_CPU_B      = 0,
	BATTERY_OC_PRIO_CPU_L      = 1,
	BATTERY_OC_PRIO_GPU        = 2,
	BATTERY_OC_PRIO_FLASHLIGHT = 3
};

extern void (*battery_oc_callback)(enum BATTERY_OC_LEVEL);
extern void register_battery_oc_notify(void (*battery_oc_callback)(enum BATTERY_OC_LEVEL),
	enum BATTERY_OC_PRIO prio_val);

/*
 * Battery percent define
 */
enum BATTERY_PERCENT_LEVEL {
	BATTERY_PERCENT_LEVEL_0 = 0,
	BATTERY_PERCENT_LEVEL_1 = 1
};

enum BATTERY_PERCENT_PRIO {
	BATTERY_PERCENT_PRIO_CPU_B      = 0,
	BATTERY_PERCENT_PRIO_CPU_L      = 1,
	BATTERY_PERCENT_PRIO_GPU        = 2,
	BATTERY_PERCENT_PRIO_MD         = 3,
	BATTERY_PERCENT_PRIO_MD5        = 4,
	BATTERY_PERCENT_PRIO_FLASHLIGHT = 5,
	BATTERY_PERCENT_PRIO_VIDEO      = 6,
	BATTERY_PERCENT_PRIO_WIFI       = 7,
	BATTERY_PERCENT_PRIO_BACKLIGHT  = 8
};

extern void (*battery_percent_callback)(enum BATTERY_PERCENT_LEVEL);
extern void register_battery_percent_notify(void (*battery_percent_callback)(enum BATTERY_PERCENT_LEVEL),
	enum BATTERY_PERCENT_PRIO prio_val);

extern unsigned int pmic_config_interface(unsigned int regnum, unsigned int val, unsigned int mask,
	unsigned int shift);
extern unsigned int pmic_read_interface(unsigned int regnum, unsigned int *val, unsigned int mask, unsigned int shift);
extern void pmic_lock(void);
extern void pmic_unlock(void);
extern void pmic_smp_lock(void);
extern void pmic_smp_unlock(void);

static inline void upmu_set_rg_vcdt_hv_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCDT_HV_EN_MASK),
				    (unsigned int) (PMIC_RG_VCDT_HV_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_chr_ldo_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON0),
				  (&val),
				  (unsigned int) (PMIC_RGS_CHR_LDO_DET_MASK),
				  (unsigned int) (PMIC_RGS_CHR_LDO_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_pchr_automode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_AUTOMODE_MASK),
				    (unsigned int) (PMIC_RG_PCHR_AUTOMODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_csdac_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CSDAC_EN_MASK),
				    (unsigned int) (PMIC_RG_CSDAC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chr_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHR_EN_MASK),
				    (unsigned int) (PMIC_RG_CHR_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_chrdet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON0),
				  (&val),
				  (unsigned int) (PMIC_RGS_CHRDET_MASK),
				  (unsigned int) (PMIC_RGS_CHRDET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_vcdt_lv_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON0),
				  (&val),
				  (unsigned int) (PMIC_RGS_VCDT_LV_DET_MASK),
				  (unsigned int) (PMIC_RGS_VCDT_LV_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_vcdt_hv_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON0),
				  (&val),
				  (unsigned int) (PMIC_RGS_VCDT_HV_DET_MASK),
				  (unsigned int) (PMIC_RGS_VCDT_HV_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vcdt_lv_vth(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCDT_LV_VTH_MASK),
				    (unsigned int) (PMIC_RG_VCDT_LV_VTH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcdt_hv_vth(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCDT_HV_VTH_MASK),
				    (unsigned int) (PMIC_RG_VCDT_HV_VTH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbat_cv_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBAT_CV_EN_MASK),
				    (unsigned int) (PMIC_RG_VBAT_CV_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbat_cc_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBAT_CC_EN_MASK),
				    (unsigned int) (PMIC_RG_VBAT_CC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_cs_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CS_EN_MASK),
				    (unsigned int) (PMIC_RG_CS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_cs_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON2),
				  (&val),
				  (unsigned int) (PMIC_RGS_CS_DET_MASK),
				  (unsigned int) (PMIC_RGS_CS_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_vbat_cv_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON2),
				  (&val),
				  (unsigned int) (PMIC_RGS_VBAT_CV_DET_MASK),
				  (unsigned int) (PMIC_RGS_VBAT_CV_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_vbat_cc_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON2),
				  (&val),
				  (unsigned int) (PMIC_RGS_VBAT_CC_DET_MASK),
				  (unsigned int) (PMIC_RGS_VBAT_CC_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vbat_cv_vth(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBAT_CV_VTH_MASK),
				    (unsigned int) (PMIC_RG_VBAT_CV_VTH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbat_cc_vth(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBAT_CC_VTH_MASK),
				    (unsigned int) (PMIC_RG_VBAT_CC_VTH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_cs_vth(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CS_VTH_MASK),
				    (unsigned int) (PMIC_RG_CS_VTH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_tohtc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_TOHTC_MASK),
				    (unsigned int) (PMIC_RG_PCHR_TOHTC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_toltc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_TOLTC_MASK),
				    (unsigned int) (PMIC_RG_PCHR_TOLTC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbat_ov_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBAT_OV_EN_MASK),
				    (unsigned int) (PMIC_RG_VBAT_OV_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbat_ov_vth(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBAT_OV_VTH_MASK),
				    (unsigned int) (PMIC_RG_VBAT_OV_VTH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbat_ov_deg(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBAT_OV_DEG_MASK),
				    (unsigned int) (PMIC_RG_VBAT_OV_DEG_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_vbat_ov_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON6),
				  (&val),
				  (unsigned int) (PMIC_RGS_VBAT_OV_DET_MASK),
				  (unsigned int) (PMIC_RGS_VBAT_OV_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_baton_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BATON_EN_MASK),
				    (unsigned int) (PMIC_RG_BATON_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_baton_ht_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BATON_HT_EN_MASK),
				    (unsigned int) (PMIC_RG_BATON_HT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_baton_tdet_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_BATON_TDET_EN_MASK),
				    (unsigned int) (PMIC_BATON_TDET_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_baton_ht_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BATON_HT_TRIM_MASK),
				    (unsigned int) (PMIC_RG_BATON_HT_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_baton_ht_trim_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BATON_HT_TRIM_SET_MASK),
				    (unsigned int) (PMIC_RG_BATON_HT_TRIM_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_baton_undet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON7),
				  (&val),
				  (unsigned int) (PMIC_RGS_BATON_UNDET_MASK),
				  (unsigned int) (PMIC_RGS_BATON_UNDET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_csdac_data(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CSDAC_DATA_MASK),
				    (unsigned int) (PMIC_RG_CSDAC_DATA_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_frc_csvth_usbdl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FRC_CSVTH_USBDL_MASK),
				    (unsigned int) (PMIC_RG_FRC_CSVTH_USBDL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_pchr_flag_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON10),
				  (&val),
				  (unsigned int) (PMIC_RGS_PCHR_FLAG_OUT_MASK),
				  (unsigned int) (PMIC_RGS_PCHR_FLAG_OUT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_pchr_flag_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_FLAG_EN_MASK),
				    (unsigned int) (PMIC_RG_PCHR_FLAG_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_otg_bvalid_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OTG_BVALID_EN_MASK),
				    (unsigned int) (PMIC_RG_OTG_BVALID_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_otg_bvalid_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON10),
				  (&val),
				  (unsigned int) (PMIC_RGS_OTG_BVALID_DET_MASK),
				  (unsigned int) (PMIC_RGS_OTG_BVALID_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_pchr_flag_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_FLAG_SEL_MASK),
				    (unsigned int) (PMIC_RG_PCHR_FLAG_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_testmode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_TESTMODE_MASK),
				    (unsigned int) (PMIC_RG_PCHR_TESTMODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_csdac_testmode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CSDAC_TESTMODE_MASK),
				    (unsigned int) (PMIC_RG_CSDAC_TESTMODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_RST_MASK),
				    (unsigned int) (PMIC_RG_PCHR_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_ft_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_FT_CTRL_MASK),
				    (unsigned int) (PMIC_RG_PCHR_FT_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chrwdt_td(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHRWDT_TD_MASK),
				    (unsigned int) (PMIC_RG_CHRWDT_TD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chrwdt_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHRWDT_EN_MASK),
				    (unsigned int) (PMIC_RG_CHRWDT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chrwdt_wr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHRWDT_WR_MASK),
				    (unsigned int) (PMIC_RG_CHRWDT_WR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_rv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_RV_MASK),
				    (unsigned int) (PMIC_RG_PCHR_RV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chrwdt_int_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHRWDT_INT_EN_MASK),
				    (unsigned int) (PMIC_RG_CHRWDT_INT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chrwdt_flag_wr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHRWDT_FLAG_WR_MASK),
				    (unsigned int) (PMIC_RG_CHRWDT_FLAG_WR_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_chrwdt_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON15),
				  (&val),
				  (unsigned int) (PMIC_RGS_CHRWDT_OUT_MASK),
				  (unsigned int) (PMIC_RGS_CHRWDT_OUT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_uvlo_vthl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_UVLO_VTHL_MASK),
				    (unsigned int) (PMIC_RG_UVLO_VTHL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_usbdl_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_USBDL_RST_MASK),
				    (unsigned int) (PMIC_RG_USBDL_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_usbdl_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_USBDL_SET_MASK),
				    (unsigned int) (PMIC_RG_USBDL_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_adcin_vsen_mux_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ADCIN_VSEN_MUX_EN_MASK),
				    (unsigned int) (PMIC_ADCIN_VSEN_MUX_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adcin_vsen_ext_baton_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADCIN_VSEN_EXT_BATON_EN_MASK),
				    (unsigned int) (PMIC_RG_ADCIN_VSEN_EXT_BATON_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_adcin_vbat_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ADCIN_VBAT_EN_MASK),
				    (unsigned int) (PMIC_ADCIN_VBAT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_adcin_vsen_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ADCIN_VSEN_EN_MASK),
				    (unsigned int) (PMIC_ADCIN_VSEN_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_adcin_vchr_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ADCIN_VCHR_EN_MASK),
				    (unsigned int) (PMIC_ADCIN_VCHR_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bgr_rsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BGR_RSEL_MASK),
				    (unsigned int) (PMIC_RG_BGR_RSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bgr_unchop_ph(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BGR_UNCHOP_PH_MASK),
				    (unsigned int) (PMIC_RG_BGR_UNCHOP_PH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bgr_unchop(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BGR_UNCHOP_MASK),
				    (unsigned int) (PMIC_RG_BGR_UNCHOP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bc11_bb_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_BB_CTRL_MASK),
				    (unsigned int) (PMIC_RG_BC11_BB_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bc11_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_RST_MASK),
				    (unsigned int) (PMIC_RG_BC11_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bc11_vsrc_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_VSRC_EN_MASK),
				    (unsigned int) (PMIC_RG_BC11_VSRC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_bc11_cmp_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON18),
				  (&val),
				  (unsigned int) (PMIC_RGS_BC11_CMP_OUT_MASK),
				  (unsigned int) (PMIC_RGS_BC11_CMP_OUT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_bc11_vref_vth(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_VREF_VTH_MASK),
				    (unsigned int) (PMIC_RG_BC11_VREF_VTH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bc11_cmp_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_CMP_EN_MASK),
				    (unsigned int) (PMIC_RG_BC11_CMP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bc11_ipd_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_IPD_EN_MASK),
				    (unsigned int) (PMIC_RG_BC11_IPD_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bc11_ipu_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_IPU_EN_MASK),
				    (unsigned int) (PMIC_RG_BC11_IPU_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bc11_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_BIAS_EN_MASK),
				    (unsigned int) (PMIC_RG_BC11_BIAS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_csdac_stp_inc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CSDAC_STP_INC_MASK),
				    (unsigned int) (PMIC_RG_CSDAC_STP_INC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_csdac_stp_dec(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CSDAC_STP_DEC_MASK),
				    (unsigned int) (PMIC_RG_CSDAC_STP_DEC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_csdac_dly(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CSDAC_DLY_MASK),
				    (unsigned int) (PMIC_RG_CSDAC_DLY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_csdac_stp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CSDAC_STP_MASK),
				    (unsigned int) (PMIC_RG_CSDAC_STP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_low_ich_db(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON22),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LOW_ICH_DB_MASK),
				    (unsigned int) (PMIC_RG_LOW_ICH_DB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chrind_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON22),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHRIND_ON_MASK),
				    (unsigned int) (PMIC_RG_CHRIND_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chrind_dimming(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON22),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHRIND_DIMMING_MASK),
				    (unsigned int) (PMIC_RG_CHRIND_DIMMING_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_cv_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CV_MODE_MASK),
				    (unsigned int) (PMIC_RG_CV_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcdt_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCDT_MODE_MASK),
				    (unsigned int) (PMIC_RG_VCDT_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_csdac_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CSDAC_MODE_MASK),
				    (unsigned int) (PMIC_RG_CSDAC_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_tracking_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TRACKING_EN_MASK),
				    (unsigned int) (PMIC_RG_TRACKING_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hwcv_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HWCV_EN_MASK),
				    (unsigned int) (PMIC_RG_HWCV_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ulc_det_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ULC_DET_EN_MASK),
				    (unsigned int) (PMIC_RG_ULC_DET_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bgr_trim_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BGR_TRIM_EN_MASK),
				    (unsigned int) (PMIC_RG_BGR_TRIM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ichrg_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ICHRG_TRIM_MASK),
				    (unsigned int) (PMIC_RG_ICHRG_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bgr_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON25),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BGR_TRIM_MASK),
				    (unsigned int) (PMIC_RG_BGR_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ovp_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON26),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OVP_TRIM_MASK),
				    (unsigned int) (PMIC_RG_OVP_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chr_osc_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHR_OSC_TRIM_MASK),
				    (unsigned int) (PMIC_RG_CHR_OSC_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_qi_bgr_ext_buf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_QI_BGR_EXT_BUF_EN_MASK),
				    (unsigned int) (PMIC_QI_BGR_EXT_BUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bgr_test_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BGR_TEST_EN_MASK),
				    (unsigned int) (PMIC_RG_BGR_TEST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bgr_test_rstb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BGR_TEST_RSTB_MASK),
				    (unsigned int) (PMIC_RG_BGR_TEST_RSTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dac_usbdl_max(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON28),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DAC_USBDL_MAX_MASK),
				    (unsigned int) (PMIC_RG_DAC_USBDL_MAX_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON29),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_RSV_MASK),
				    (unsigned int) (PMIC_RG_PCHR_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_thr_det_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_THR_DET_DIS_MASK),
				    (unsigned int) (PMIC_THR_DET_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_thr_tmode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_THR_TMODE_MASK),
				    (unsigned int) (PMIC_RG_THR_TMODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_thr_temp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_THR_TEMP_SEL_MASK),
				    (unsigned int) (PMIC_RG_THR_TEMP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_strup_thr_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STRUP_THR_SEL_MASK),
				    (unsigned int) (PMIC_RG_STRUP_THR_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_thr_hwpdn_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_THR_HWPDN_EN_MASK),
				    (unsigned int) (PMIC_THR_HWPDN_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_thrdet_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_THRDET_SEL_MASK),
				    (unsigned int) (PMIC_RG_THRDET_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_strup_iref_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STRUP_IREF_TRIM_MASK),
				    (unsigned int) (PMIC_RG_STRUP_IREF_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_usbdl_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_USBDL_EN_MASK),
				    (unsigned int) (PMIC_RG_USBDL_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_fchr_keydet_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FCHR_KEYDET_EN_MASK),
				    (unsigned int) (PMIC_RG_FCHR_KEYDET_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_fchr_pu_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FCHR_PU_EN_MASK),
				    (unsigned int) (PMIC_RG_FCHR_PU_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_en_drvsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EN_DRVSEL_MASK),
				    (unsigned int) (PMIC_RG_EN_DRVSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rst_drvsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RST_DRVSEL_MASK),
				    (unsigned int) (PMIC_RG_RST_DRVSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vref_bg(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VREF_BG_MASK),
				    (unsigned int) (PMIC_RG_VREF_BG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pmu_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PMU_RSV_MASK),
				    (unsigned int) (PMIC_RG_PMU_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_thr_test(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_THR_TEST_MASK),
				    (unsigned int) (PMIC_THR_TEST_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_pmu_thr_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STRUP_CON5),
				  (&val),
				  (unsigned int) (PMIC_PMU_THR_DEB_MASK),
				  (unsigned int) (PMIC_PMU_THR_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_pmu_thr_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STRUP_CON5),
				  (&val),
				  (unsigned int) (PMIC_PMU_THR_STATUS_MASK),
				  (unsigned int) (PMIC_PMU_THR_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_dduvlo_deb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DDUVLO_DEB_EN_MASK),
				    (unsigned int) (PMIC_DDUVLO_DEB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_pwrbb_deb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_PWRBB_DEB_EN_MASK),
				    (unsigned int) (PMIC_PWRBB_DEB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_osc_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_OSC_EN_MASK),
				    (unsigned int) (PMIC_STRUP_OSC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_osc_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_OSC_EN_SEL_MASK),
				    (unsigned int) (PMIC_STRUP_OSC_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_ft_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_FT_CTRL_MASK),
				    (unsigned int) (PMIC_STRUP_FT_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_pwron_force(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_PWRON_FORCE_MASK),
				    (unsigned int) (PMIC_STRUP_PWRON_FORCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_bias_gen_en_force(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_BIAS_GEN_EN_FORCE_MASK),
				    (unsigned int) (PMIC_BIAS_GEN_EN_FORCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_pwron(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_PWRON_MASK),
				    (unsigned int) (PMIC_STRUP_PWRON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_pwron_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_PWRON_SEL_MASK),
				    (unsigned int) (PMIC_STRUP_PWRON_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_bias_gen_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_BIAS_GEN_EN_MASK),
				    (unsigned int) (PMIC_BIAS_GEN_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_bias_gen_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_BIAS_GEN_EN_SEL_MASK),
				    (unsigned int) (PMIC_BIAS_GEN_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rtc_xosc32_enb_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RTC_XOSC32_ENB_SW_MASK),
				    (unsigned int) (PMIC_RTC_XOSC32_ENB_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rtc_xosc32_enb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RTC_XOSC32_ENB_SEL_MASK),
				    (unsigned int) (PMIC_RTC_XOSC32_ENB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_dig_io_pg_force(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_DIG_IO_PG_FORCE_MASK),
				    (unsigned int) (PMIC_STRUP_DIG_IO_PG_FORCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_PG_ENB_MASK),
				    (unsigned int) (PMIC_VPROC_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_PG_ENB_MASK),
				    (unsigned int) (PMIC_VSYS_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vm_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VM_PG_ENB_MASK),
				    (unsigned int) (PMIC_VM_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_PG_ENB_MASK),
				    (unsigned int) (PMIC_VIO18_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vtcxo_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VTCXO_PG_ENB_MASK),
				    (unsigned int) (PMIC_VTCXO_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_va_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VA_PG_ENB_MASK),
				    (unsigned int) (PMIC_VA_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio28_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO28_PG_ENB_MASK),
				    (unsigned int) (PMIC_VIO28_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp2_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP2_PG_ENB_MASK),
				    (unsigned int) (PMIC_VGP2_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_pg_h2l_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_PG_H2L_EN_MASK),
				    (unsigned int) (PMIC_VPROC_PG_H2L_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_pg_h2l_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_PG_H2L_EN_MASK),
				    (unsigned int) (PMIC_VSYS_PG_H2L_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_con6_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_CON6_RSV0_MASK),
				    (unsigned int) (PMIC_STRUP_CON6_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_clr_just_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_CLR_JUST_RST_MASK),
				    (unsigned int) (PMIC_CLR_JUST_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_uvlo_l2h_deb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_UVLO_L2H_DEB_EN_MASK),
				    (unsigned int) (PMIC_UVLO_L2H_DEB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_just_pwrkey_rst(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STRUP_CON8),
				  (&val),
				  (unsigned int) (PMIC_JUST_PWRKEY_RST_MASK),
				  (unsigned int) (PMIC_JUST_PWRKEY_RST_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_osc_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STRUP_CON8),
				  (&val),
				  (unsigned int) (PMIC_QI_OSC_EN_MASK),
				  (unsigned int) (PMIC_QI_OSC_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_strup_ext_pmic_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_EXT_PMIC_EN_MASK),
				    (unsigned int) (PMIC_STRUP_EXT_PMIC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_ext_pmic_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_EXT_PMIC_SEL_MASK),
				    (unsigned int) (PMIC_STRUP_EXT_PMIC_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_con8_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_CON8_RSV0_MASK),
				    (unsigned int) (PMIC_STRUP_CON8_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_ext_pmic_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STRUP_CON9),
				  (&val),
				  (unsigned int) (PMIC_QI_EXT_PMIC_EN_MASK),
				  (unsigned int) (PMIC_QI_EXT_PMIC_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_strup_auxadc_start_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_AUXADC_START_SW_MASK),
				    (unsigned int) (PMIC_STRUP_AUXADC_START_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_auxadc_rstb_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_AUXADC_RSTB_SW_MASK),
				    (unsigned int) (PMIC_STRUP_AUXADC_RSTB_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_auxadc_start_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_AUXADC_START_SEL_MASK),
				    (unsigned int) (PMIC_STRUP_AUXADC_START_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_auxadc_rstb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_AUXADC_RSTB_SEL_MASK),
				    (unsigned int) (PMIC_STRUP_AUXADC_RSTB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_pwroff_seq_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_PWROFF_SEQ_EN_MASK),
				    (unsigned int) (PMIC_STRUP_PWROFF_SEQ_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_pwroff_preoff_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_PWROFF_PREOFF_EN_MASK),
				    (unsigned int) (PMIC_STRUP_PWROFF_PREOFF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_en_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_EN_L_MASK),
				    (unsigned int) (PMIC_SPK_EN_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spkmode_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPKMODE_L_MASK),
				    (unsigned int) (PMIC_SPKMODE_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_trim_en_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TRIM_EN_L_MASK),
				    (unsigned int) (PMIC_SPK_TRIM_EN_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_oc_shdn_dl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OC_SHDN_DL_MASK),
				    (unsigned int) (PMIC_SPK_OC_SHDN_DL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_ther_shdn_l_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_THER_SHDN_L_EN_MASK),
				    (unsigned int) (PMIC_SPK_THER_SHDN_L_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_gainl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_GAINL_MASK),
				    (unsigned int) (PMIC_RG_SPK_GAINL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_da_spk_offset_l(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (SPK_CON1),
				  (&val),
				  (unsigned int) (PMIC_DA_SPK_OFFSET_L_MASK),
				  (unsigned int) (PMIC_DA_SPK_OFFSET_L_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_da_spk_lead_dglh_l(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (SPK_CON1),
				  (&val),
				  (unsigned int) (PMIC_DA_SPK_LEAD_DGLH_L_MASK),
				  (unsigned int) (PMIC_DA_SPK_LEAD_DGLH_L_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_spk_lead_l(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (SPK_CON1),
				  (&val),
				  (unsigned int) (PMIC_NI_SPK_LEAD_L_MASK),
				  (unsigned int) (PMIC_NI_SPK_LEAD_L_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_spk_offset_l_ov(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (SPK_CON1),
				  (&val),
				  (unsigned int) (PMIC_SPK_OFFSET_L_OV_MASK),
				  (unsigned int) (PMIC_SPK_OFFSET_L_OV_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_spk_offset_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OFFSET_L_SW_MASK),
				    (unsigned int) (PMIC_SPK_OFFSET_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_lead_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_LEAD_L_SW_MASK),
				    (unsigned int) (PMIC_SPK_LEAD_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_offset_l_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OFFSET_L_MODE_MASK),
				    (unsigned int) (PMIC_SPK_OFFSET_L_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_spk_trim_done_l(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (SPK_CON1),
				  (&val),
				  (unsigned int) (PMIC_SPK_TRIM_DONE_L_MASK),
				  (unsigned int) (PMIC_SPK_TRIM_DONE_L_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_spk_intg_rst_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_INTG_RST_L_MASK),
				    (unsigned int) (PMIC_RG_SPK_INTG_RST_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_force_en_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_FORCE_EN_L_MASK),
				    (unsigned int) (PMIC_RG_SPK_FORCE_EN_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_slew_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_SLEW_L_MASK),
				    (unsigned int) (PMIC_RG_SPK_SLEW_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spkab_obias_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPKAB_OBIAS_L_MASK),
				    (unsigned int) (PMIC_RG_SPKAB_OBIAS_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spkrcv_en_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPKRCV_EN_L_MASK),
				    (unsigned int) (PMIC_RG_SPKRCV_EN_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_drc_en_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_DRC_EN_L_MASK),
				    (unsigned int) (PMIC_RG_SPK_DRC_EN_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_test_en_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_TEST_EN_L_MASK),
				    (unsigned int) (PMIC_RG_SPK_TEST_EN_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spkab_oc_en_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPKAB_OC_EN_L_MASK),
				    (unsigned int) (PMIC_RG_SPKAB_OC_EN_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_oc_en_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_OC_EN_L_MASK),
				    (unsigned int) (PMIC_RG_SPK_OC_EN_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_trim_wnd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TRIM_WND_MASK),
				    (unsigned int) (PMIC_SPK_TRIM_WND_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_trim_thd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TRIM_THD_MASK),
				    (unsigned int) (PMIC_SPK_TRIM_THD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_oc_wnd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OC_WND_MASK),
				    (unsigned int) (PMIC_SPK_OC_WND_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_oc_thd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OC_THD_MASK),
				    (unsigned int) (PMIC_SPK_OC_THD_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_spk_d_oc_l_deg(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (SPK_CON6),
				  (&val),
				  (unsigned int) (PMIC_SPK_D_OC_L_DEG_MASK),
				  (unsigned int) (PMIC_SPK_D_OC_L_DEG_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_spk_ab_oc_l_deg(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (SPK_CON6),
				  (&val),
				  (unsigned int) (PMIC_SPK_AB_OC_L_DEG_MASK),
				  (unsigned int) (PMIC_SPK_AB_OC_L_DEG_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_spk_td1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TD1_MASK),
				    (unsigned int) (PMIC_SPK_TD1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_td2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TD2_MASK),
				    (unsigned int) (PMIC_SPK_TD2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_td3(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TD3_MASK),
				    (unsigned int) (PMIC_SPK_TD3_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_trim_div(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TRIM_DIV_MASK),
				    (unsigned int) (PMIC_SPK_TRIM_DIV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_btl_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BTL_SET_MASK),
				    (unsigned int) (PMIC_RG_BTL_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_ibias_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_IBIAS_SEL_MASK),
				    (unsigned int) (PMIC_RG_SPK_IBIAS_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_ccode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_CCODE_MASK),
				    (unsigned int) (PMIC_RG_SPK_CCODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_en_view_vcm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_EN_VIEW_VCM_MASK),
				    (unsigned int) (PMIC_RG_SPK_EN_VIEW_VCM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_en_view_clk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_EN_VIEW_CLK_MASK),
				    (unsigned int) (PMIC_RG_SPK_EN_VIEW_CLK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_vcm_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_VCM_SEL_MASK),
				    (unsigned int) (PMIC_RG_SPK_VCM_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_vcm_ibsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_VCM_IBSEL_MASK),
				    (unsigned int) (PMIC_RG_SPK_VCM_IBSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_fbrc_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_FBRC_EN_MASK),
				    (unsigned int) (PMIC_RG_SPK_FBRC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spkab_ovdrv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPKAB_OVDRV_MASK),
				    (unsigned int) (PMIC_RG_SPKAB_OVDRV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_octh_d(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_OCTH_D_MASK),
				    (unsigned int) (PMIC_RG_SPK_OCTH_D_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_RSV_MASK),
				    (unsigned int) (PMIC_RG_SPK_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spkpga_gain(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPKPGA_GAIN_MASK),
				    (unsigned int) (PMIC_RG_SPKPGA_GAIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_RSV0_MASK),
				    (unsigned int) (PMIC_SPK_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_vcm_fast_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_VCM_FAST_EN_MASK),
				    (unsigned int) (PMIC_SPK_VCM_FAST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_test_mode0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TEST_MODE0_MASK),
				    (unsigned int) (PMIC_SPK_TEST_MODE0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_test_mode1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TEST_MODE1_MASK),
				    (unsigned int) (PMIC_SPK_TEST_MODE1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_isense_refsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_ISENSE_REFSEL_MASK),
				    (unsigned int) (PMIC_RG_SPK_ISENSE_REFSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_isense_gainsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_ISENSE_GAINSEL_MASK),
				    (unsigned int) (PMIC_RG_SPK_ISENSE_GAINSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_isense_pd_reset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ISENSE_PD_RESET_MASK),
				    (unsigned int) (PMIC_RG_ISENSE_PD_RESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_isense_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_ISENSE_EN_MASK),
				    (unsigned int) (PMIC_RG_SPK_ISENSE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_isense_test_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_ISENSE_TEST_EN_MASK),
				    (unsigned int) (PMIC_RG_SPK_ISENSE_TEST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_td_wait(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TD_WAIT_MASK),
				    (unsigned int) (PMIC_SPK_TD_WAIT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_td_done(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TD_DONE_MASK),
				    (unsigned int) (PMIC_SPK_TD_DONE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_en_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_EN_MODE_MASK),
				    (unsigned int) (PMIC_SPK_EN_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_vcm_fast_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_VCM_FAST_SW_MASK),
				    (unsigned int) (PMIC_SPK_VCM_FAST_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_rst_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_RST_L_SW_MASK),
				    (unsigned int) (PMIC_SPK_RST_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spkmode_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPKMODE_L_SW_MASK),
				    (unsigned int) (PMIC_SPKMODE_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_depop_en_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_DEPOP_EN_L_SW_MASK),
				    (unsigned int) (PMIC_SPK_DEPOP_EN_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_en_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_EN_L_SW_MASK),
				    (unsigned int) (PMIC_SPK_EN_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_outstg_en_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OUTSTG_EN_L_SW_MASK),
				    (unsigned int) (PMIC_SPK_OUTSTG_EN_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_trim_en_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TRIM_EN_L_SW_MASK),
				    (unsigned int) (PMIC_SPK_TRIM_EN_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_trim_stop_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TRIM_STOP_L_SW_MASK),
				    (unsigned int) (PMIC_SPK_TRIM_STOP_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_cid(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CID),
				  (&val),
				  (unsigned int) (PMIC_CID_MASK), (unsigned int) (PMIC_CID_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_clksq_en_aud(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CLKSQ_EN_AUD_MASK),
				    (unsigned int) (PMIC_RG_CLKSQ_EN_AUD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_clksq_en_aux(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CLKSQ_EN_AUX_MASK),
				    (unsigned int) (PMIC_RG_CLKSQ_EN_AUX_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_clksq_en_fqr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CLKSQ_EN_FQR_MASK),
				    (unsigned int) (PMIC_RG_CLKSQ_EN_FQR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_strup_75k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STRUP_75K_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_STRUP_75K_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_strup_32k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STRUP_32K_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_STRUP_32K_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rtc_75k_div4_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RTC_75K_DIV4_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_RTC_75K_DIV4_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rtc_75k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RTC_75K_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_RTC_75K_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rtc_32k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RTC_32K_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_RTC_32K_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_32k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_32K_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_PCHR_32K_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ldostb_1m_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LDOSTB_1M_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_LDOSTB_1M_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_intrp_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INTRP_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_INTRP_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_drv_32k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DRV_32K_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_DRV_32K_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_buck_1m_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BUCK_1M_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_BUCK_1M_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_buck_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BUCK_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_BUCK_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_buck_ana_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BUCK_ANA_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_BUCK_ANA_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_buck32k_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BUCK32K_PDN_MASK),
				    (unsigned int) (PMIC_RG_BUCK32K_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_strup_6m_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STRUP_6M_PDN_MASK),
				    (unsigned int) (PMIC_RG_STRUP_6M_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_pwm_div_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_PWM_DIV_PDN_MASK),
				    (unsigned int) (PMIC_RG_SPK_PWM_DIV_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_div_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_DIV_PDN_MASK),
				    (unsigned int) (PMIC_RG_SPK_DIV_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_SPK_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pwmoc_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PWMOC_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_PWMOC_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_fqmtr_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FQMTR_PDN_MASK),
				    (unsigned int) (PMIC_RG_FQMTR_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_drv_2m_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DRV_2M_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_DRV_2M_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_drv_1m_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DRV_1M_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_DRV_1M_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aud_26m_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUD_26M_PDN_MASK),
				    (unsigned int) (PMIC_RG_AUD_26M_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_accdet_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ACCDET_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_ACCDET_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rtc_mclk_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RTC_MCLK_PDN_MASK),
				    (unsigned int) (PMIC_RG_RTC_MCLK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smps_ck_div_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMPS_CK_DIV_PDN_MASK),
				    (unsigned int) (PMIC_RG_SMPS_CK_DIV_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rtc32k_1v8_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RTC32K_1V8_PDN_MASK),
				    (unsigned int) (PMIC_RG_RTC32K_1V8_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_clksq_en_aux_md(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CLKSQ_EN_AUX_MD_MASK),
				    (unsigned int) (PMIC_RG_CLKSQ_EN_AUX_MD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_sdm_ck_wake_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_SDM_CK_WAKE_PDN_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_SDM_CK_WAKE_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_isink0_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ISINK0_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_ISINK0_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_isink1_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ISINK1_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_ISINK1_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_isink2_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ISINK2_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_ISINK2_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_isink3_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ISINK3_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_ISINK3_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_sdm_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_SDM_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_SDM_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_ctl_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_CTL_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_CTL_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_32k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_32K_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_32K_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aud26m_div4_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUD26M_DIV4_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_AUD26M_DIV4_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_man_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_MAN_RST_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_MAN_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_RST_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audio_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDIO_RST_MASK),
				    (unsigned int) (PMIC_RG_AUDIO_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_accdet_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ACCDET_RST_MASK),
				    (unsigned int) (PMIC_RG_ACCDET_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_RST_MASK),
				    (unsigned int) (PMIC_RG_SPK_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_driver_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DRIVER_RST_MASK),
				    (unsigned int) (PMIC_RG_DRIVER_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rtc_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RTC_RST_MASK),
				    (unsigned int) (PMIC_RG_RTC_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_fqmtr_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FQMTR_RST_MASK),
				    (unsigned int) (PMIC_RG_FQMTR_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_top_rst_con_rsv_15_9(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TOP_RST_CON_RSV_15_9_MASK),
				    (unsigned int) (PMIC_RG_TOP_RST_CON_RSV_15_9_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ap_rst_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AP_RST_DIS_MASK),
				    (unsigned int) (PMIC_RG_AP_RST_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_sysrstb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SYSRSTB_EN_MASK),
				    (unsigned int) (PMIC_RG_SYSRSTB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_strup_man_rst_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STRUP_MAN_RST_EN_MASK),
				    (unsigned int) (PMIC_RG_STRUP_MAN_RST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_newldo_rstb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_NEWLDO_RSTB_EN_MASK),
				    (unsigned int) (PMIC_RG_NEWLDO_RSTB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rst_part_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RST_PART_SEL_MASK),
				    (unsigned int) (PMIC_RG_RST_PART_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_homekey_rst_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HOMEKEY_RST_EN_MASK),
				    (unsigned int) (PMIC_RG_HOMEKEY_RST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pwrkey_rst_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PWRKEY_RST_EN_MASK),
				    (unsigned int) (PMIC_RG_PWRKEY_RST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pwrrst_tmr_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PWRRST_TMR_DIS_MASK),
				    (unsigned int) (PMIC_RG_PWRRST_TMR_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pwrkey_rst_td(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PWRKEY_RST_TD_MASK),
				    (unsigned int) (PMIC_RG_PWRKEY_RST_TD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_srclken_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SRCLKEN_EN_MASK),
				    (unsigned int) (PMIC_RG_SRCLKEN_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_osc_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OSC_SEL_MASK),
				    (unsigned int) (PMIC_RG_OSC_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_sdm_sel_hw_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_SDM_SEL_HW_MODE_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_SDM_SEL_HW_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_srclken_hw_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SRCLKEN_HW_MODE_MASK),
				    (unsigned int) (PMIC_RG_SRCLKEN_HW_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_osc_hw_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OSC_HW_MODE_MASK),
				    (unsigned int) (PMIC_RG_OSC_HW_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_osc_hw_src_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OSC_HW_SRC_SEL_MASK),
				    (unsigned int) (PMIC_RG_OSC_HW_SRC_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_sdm_ck_hw_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_SDM_CK_HW_MODE_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_SDM_CK_HW_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smps_autoff_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMPS_AUTOFF_DIS_MASK),
				    (unsigned int) (PMIC_RG_SMPS_AUTOFF_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_buck_1m_autoff_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BUCK_1M_AUTOFF_DIS_MASK),
				    (unsigned int) (PMIC_RG_BUCK_1M_AUTOFF_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_buck_ana_autoff_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BUCK_ANA_AUTOFF_DIS_MASK),
				    (unsigned int) (PMIC_RG_BUCK_ANA_AUTOFF_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_regck_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_REGCK_SEL_MASK),
				    (unsigned int) (PMIC_RG_REGCK_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_pwm_div_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_PWM_DIV_SEL_MASK),
				    (unsigned int) (PMIC_RG_SPK_PWM_DIV_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_div_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_DIV_SEL_MASK),
				    (unsigned int) (PMIC_RG_SPK_DIV_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_fqmtr_cksel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FQMTR_CKSEL_MASK),
				    (unsigned int) (PMIC_RG_FQMTR_CKSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_accdet_cksel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ACCDET_CKSEL_MASK),
				    (unsigned int) (PMIC_RG_ACCDET_CKSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_isink0_ck_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ISINK0_CK_SEL_MASK),
				    (unsigned int) (PMIC_RG_ISINK0_CK_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_isink1_ck_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ISINK1_CK_SEL_MASK),
				    (unsigned int) (PMIC_RG_ISINK1_CK_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_isink2_ck_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ISINK2_CK_SEL_MASK),
				    (unsigned int) (PMIC_RG_ISINK2_CK_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_isink3_ck_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ISINK3_CK_SEL_MASK),
				    (unsigned int) (PMIC_RG_ISINK3_CK_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_sdm_ck_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_SDM_CK_SEL_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_SDM_CK_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audio_ck_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDIO_CK_SEL_MASK),
				    (unsigned int) (PMIC_RG_AUDIO_CK_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rtc32k_tst_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RTC32K_TST_DIS_MASK),
				    (unsigned int) (PMIC_RG_RTC32K_TST_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_tst_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_TST_DIS_MASK),
				    (unsigned int) (PMIC_RG_SPK_TST_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smps_tst_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMPS_TST_DIS_MASK),
				    (unsigned int) (PMIC_RG_SMPS_TST_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pmu75k_tst_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PMU75K_TST_DIS_MASK),
				    (unsigned int) (PMIC_RG_PMU75K_TST_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aud26m_tst_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUD26M_TST_DIS_MASK),
				    (unsigned int) (PMIC_RG_AUD26M_TST_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_TSTSEL_MASK),
				    (unsigned int) (PMIC_RG_SPK_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smps_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMPS_TSTSEL_MASK),
				    (unsigned int) (PMIC_RG_SMPS_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rtc32k_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RTC32K_TSTSEL_MASK),
				    (unsigned int) (PMIC_RG_RTC32K_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pmu75k_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PMU75K_TSTSEL_MASK),
				    (unsigned int) (PMIC_RG_PMU75K_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aud26m_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUD26M_TSTSEL_MASK),
				    (unsigned int) (PMIC_RG_AUD26M_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rtcdet_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RTCDET_TSTSEL_MASK),
				    (unsigned int) (PMIC_RG_RTCDET_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pwmoc_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PWMOC_TSTSEL_MASK),
				    (unsigned int) (PMIC_RG_PWMOC_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ldostb_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LDOSTB_TSTSEL_MASK),
				    (unsigned int) (PMIC_RG_LDOSTB_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_isink_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ISINK_TSTSEL_MASK),
				    (unsigned int) (PMIC_RG_ISINK_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_fqmtr_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FQMTR_TSTSEL_MASK),
				    (unsigned int) (PMIC_RG_FQMTR_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_classd_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CLASSD_TSTSEL_MASK),
				    (unsigned int) (PMIC_RG_CLASSD_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_sdm_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_SDM_TSTSEL_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_SDM_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aud26m_div4_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUD26M_DIV4_TSTSEL_MASK),
				    (unsigned int) (PMIC_RG_AUD26M_DIV4_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audif_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDIF_TSTSEL_MASK),
				    (unsigned int) (PMIC_RG_AUDIF_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bgr_test_ck_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BGR_TEST_CK_SEL_MASK),
				    (unsigned int) (PMIC_RG_BGR_TEST_CK_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_test_ck_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_TEST_CK_SEL_MASK),
				    (unsigned int) (PMIC_RG_PCHR_TEST_CK_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_strup_75k_26m_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STRUP_75K_26M_SEL_MASK),
				    (unsigned int) (PMIC_RG_STRUP_75K_26M_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bgr_testmode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BGR_TESTMODE_MASK),
				    (unsigned int) (PMIC_RG_BGR_TESTMODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_top_cktst2_rsv_15_8(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TOP_CKTST2_RSV_15_8_MASK),
				    (unsigned int) (PMIC_RG_TOP_CKTST2_RSV_15_8_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_test_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (TEST_OUT),
				  (&val),
				  (unsigned int) (PMIC_TEST_OUT_MASK),
				  (unsigned int) (PMIC_TEST_OUT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_mon_flag_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_MON_FLAG_SEL_MASK),
				    (unsigned int) (PMIC_RG_MON_FLAG_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_mon_grp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_MON_GRP_SEL_MASK),
				    (unsigned int) (PMIC_RG_MON_GRP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_driver(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_DRIVER_MASK),
				    (unsigned int) (PMIC_RG_TEST_DRIVER_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_classd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_CLASSD_MASK),
				    (unsigned int) (PMIC_RG_TEST_CLASSD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_aud(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_AUD_MASK),
				    (unsigned int) (PMIC_RG_TEST_AUD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_auxadc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_AUXADC_MASK),
				    (unsigned int) (PMIC_RG_TEST_AUXADC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_nandtree_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_NANDTREE_MODE_MASK),
				    (unsigned int) (PMIC_RG_NANDTREE_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_MODE_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_strup(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_STRUP_MASK),
				    (unsigned int) (PMIC_RG_TEST_STRUP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_spk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_SPK_MASK),
				    (unsigned int) (PMIC_RG_TEST_SPK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_spk_pwm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_SPK_PWM_MASK),
				    (unsigned int) (PMIC_RG_TEST_SPK_PWM_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_en_status_vproc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VPROC_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VPROC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vsys(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VSYS_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VSYS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vpa(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VPA_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VPA_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vrtc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VRTC_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VRTC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_va(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VA_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VA_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vcama(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VCAMA_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VCAMA_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vcamd(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VCAMD_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VCAMD_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vcam_af(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VCAM_AF_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VCAM_AF_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vcam_io(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VCAM_IO_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VCAM_IO_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vcn28(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VCN28_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VCN28_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vcn33(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VCN33_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VCN33_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vcn_1v8(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VCN_1V8_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VCN_1V8_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vemc_3v3(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VEMC_3V3_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VEMC_3V3_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vgp1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VGP1_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VGP1_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vgp2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VGP2_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VGP2_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vgp3(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VGP3_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VGP3_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vibr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VIBR_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VIBR_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vio18(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VIO18_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VIO18_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vio28(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VIO28_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VIO28_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vm(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VM_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VM_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vmc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VMC_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VMC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vmch(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VMCH_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VMCH_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vrf18(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VRF18_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VRF18_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vsim1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VSIM1_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VSIM1_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vsim2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VSIM2_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VSIM2_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vtcxo(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VTCXO_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VTCXO_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_en_status_vusb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EN_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_EN_STATUS_VUSB_MASK),
				  (unsigned int) (PMIC_EN_STATUS_VUSB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vproc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VPROC_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VPROC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vsys(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VSYS_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VSYS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vpa(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VPA_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VPA_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_va(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VA_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VA_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vcama(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VCAMA_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VCAMA_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vcamd(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VCAMD_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VCAMD_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vcam_af(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VCAM_AF_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VCAM_AF_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vcam_io(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VCAM_IO_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VCAM_IO_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vcn28(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VCN28_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VCN28_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vcn33(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VCN33_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VCN33_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vcn_1v8(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VCN_1V8_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VCN_1V8_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vemc_3v3(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VEMC_3V3_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VEMC_3V3_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vgp1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VGP1_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VGP1_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vgp2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VGP2_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VGP2_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vgp3(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VGP3_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VGP3_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vibr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VIBR_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VIBR_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vio18(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VIO18_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VIO18_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vio28(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VIO28_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VIO28_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vm(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VM_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VM_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vmc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VMC_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VMC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vmch(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VMCH_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VMCH_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vrf18(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VRF18_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VRF18_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vsim1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VSIM1_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VSIM1_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vsim2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VSIM2_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VSIM2_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vtcxo(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VTCXO_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VTCXO_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vusb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VUSB_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VUSB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_spk_oc_det_d_l(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_NI_SPK_OC_DET_D_L_MASK),
				  (unsigned int) (PMIC_NI_SPK_OC_DET_D_L_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_spk_oc_det_ab_l(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_NI_SPK_OC_DET_AB_L_MASK),
				  (unsigned int) (PMIC_NI_SPK_OC_DET_AB_L_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vproc_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VPROC_PG_DEB_MASK),
				  (unsigned int) (PMIC_VPROC_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vsys_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VSYS_PG_DEB_MASK),
				  (unsigned int) (PMIC_VSYS_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vm_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VM_PG_DEB_MASK),
				  (unsigned int) (PMIC_VM_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vio18_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VIO18_PG_DEB_MASK),
				  (unsigned int) (PMIC_VIO18_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vtcxo_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VTCXO_PG_DEB_MASK),
				  (unsigned int) (PMIC_VTCXO_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_va_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VA_PG_DEB_MASK),
				  (unsigned int) (PMIC_VA_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vio28_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VIO28_PG_DEB_MASK),
				  (unsigned int) (PMIC_VIO28_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vgp2_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VGP2_PG_DEB_MASK),
				  (unsigned int) (PMIC_VGP2_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_pmu_test_mode_scan(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHRSTATUS),
				  (&val),
				  (unsigned int) (PMIC_PMU_TEST_MODE_SCAN_MASK),
				  (unsigned int) (PMIC_PMU_TEST_MODE_SCAN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_pwrkey_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHRSTATUS),
				  (&val),
				  (unsigned int) (PMIC_PWRKEY_DEB_MASK),
				  (unsigned int) (PMIC_PWRKEY_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_fchrkey_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHRSTATUS),
				  (&val),
				  (unsigned int) (PMIC_FCHRKEY_DEB_MASK),
				  (unsigned int) (PMIC_FCHRKEY_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vbat_ov(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHRSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VBAT_OV_MASK),
				  (unsigned int) (PMIC_VBAT_OV_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_pchr_chrdet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHRSTATUS),
				  (&val),
				  (unsigned int) (PMIC_PCHR_CHRDET_MASK),
				  (unsigned int) (PMIC_PCHR_CHRDET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ro_baton_undet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHRSTATUS),
				  (&val),
				  (unsigned int) (PMIC_RO_BATON_UNDET_MASK),
				  (unsigned int) (PMIC_RO_BATON_UNDET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rtc_xtal_det_done(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHRSTATUS),
				  (&val),
				  (unsigned int) (PMIC_RTC_XTAL_DET_DONE_MASK),
				  (unsigned int) (PMIC_RTC_XTAL_DET_DONE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_xosc32_enb_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHRSTATUS),
				  (&val),
				  (unsigned int) (PMIC_XOSC32_ENB_DET_MASK),
				  (unsigned int) (PMIC_XOSC32_ENB_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rtc_xtal_det_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHRSTATUS),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RTC_XTAL_DET_RSV_MASK),
				    (unsigned int) (PMIC_RTC_XTAL_DET_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_simap_tdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SIMAP_TDSEL_MASK),
				    (unsigned int) (PMIC_RG_SIMAP_TDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aud_tdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUD_TDSEL_MASK),
				    (unsigned int) (PMIC_RG_AUD_TDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spi_tdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPI_TDSEL_MASK),
				    (unsigned int) (PMIC_RG_SPI_TDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pmu_tdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PMU_TDSEL_MASK),
				    (unsigned int) (PMIC_RG_PMU_TDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_simls_tdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SIMLS_TDSEL_MASK),
				    (unsigned int) (PMIC_RG_SIMLS_TDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_simap_rdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SIMAP_RDSEL_MASK),
				    (unsigned int) (PMIC_RG_SIMAP_RDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aud_rdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUD_RDSEL_MASK),
				    (unsigned int) (PMIC_RG_AUD_RDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spi_rdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPI_RDSEL_MASK),
				    (unsigned int) (PMIC_RG_SPI_RDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pmu_rdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PMU_RDSEL_MASK),
				    (unsigned int) (PMIC_RG_PMU_RDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_simls_rdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SIMLS_RDSEL_MASK),
				    (unsigned int) (PMIC_RG_SIMLS_RDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_sysrstb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_SYSRSTB_MASK),
				    (unsigned int) (PMIC_RG_SMT_SYSRSTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_int(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_INT_MASK),
				    (unsigned int) (PMIC_RG_SMT_INT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_srclken(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_SRCLKEN_MASK),
				    (unsigned int) (PMIC_RG_SMT_SRCLKEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_rtc_32k1v8(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_RTC_32K1V8_MASK),
				    (unsigned int) (PMIC_RG_SMT_RTC_32K1V8_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_spi_clk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_SPI_CLK_MASK),
				    (unsigned int) (PMIC_RG_SMT_SPI_CLK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_spi_csn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_SPI_CSN_MASK),
				    (unsigned int) (PMIC_RG_SMT_SPI_CSN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_spi_mosi(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_SPI_MOSI_MASK),
				    (unsigned int) (PMIC_RG_SMT_SPI_MOSI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_spi_miso(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_SPI_MISO_MASK),
				    (unsigned int) (PMIC_RG_SMT_SPI_MISO_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_aud_clk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_AUD_CLK_MASK),
				    (unsigned int) (PMIC_RG_SMT_AUD_CLK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_aud_mosi(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_AUD_MOSI_MASK),
				    (unsigned int) (PMIC_RG_SMT_AUD_MOSI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_aud_miso(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_AUD_MISO_MASK),
				    (unsigned int) (PMIC_RG_SMT_AUD_MISO_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_sim1_ap_sclk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_SIM1_AP_SCLK_MASK),
				    (unsigned int) (PMIC_RG_SMT_SIM1_AP_SCLK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_sim1_ap_srst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_SIM1_AP_SRST_MASK),
				    (unsigned int) (PMIC_RG_SMT_SIM1_AP_SRST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_simls1_sclk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_SIMLS1_SCLK_MASK),
				    (unsigned int) (PMIC_RG_SMT_SIMLS1_SCLK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_simls1_srst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_SIMLS1_SRST_MASK),
				    (unsigned int) (PMIC_RG_SMT_SIMLS1_SRST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_sim2_ap_sclk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_SIM2_AP_SCLK_MASK),
				    (unsigned int) (PMIC_RG_SMT_SIM2_AP_SCLK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_sim2_ap_srst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_SIM2_AP_SRST_MASK),
				    (unsigned int) (PMIC_RG_SMT_SIM2_AP_SRST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_simls2_sclk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_SIMLS2_SCLK_MASK),
				    (unsigned int) (PMIC_RG_SMT_SIMLS2_SCLK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt_simls2_srst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SMT_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT_SIMLS2_SRST_MASK),
				    (unsigned int) (PMIC_RG_SMT_SIMLS2_SRST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_int(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_INT_MASK),
				    (unsigned int) (PMIC_RG_OCTL_INT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_srclken(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SRCLKEN_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SRCLKEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_rtc_32k1v8(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_RTC_32K1V8_MASK),
				    (unsigned int) (PMIC_RG_OCTL_RTC_32K1V8_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_spi_clk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SPI_CLK_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SPI_CLK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_spi_csn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SPI_CSN_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SPI_CSN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_spi_mosi(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SPI_MOSI_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SPI_MOSI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_spi_miso(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SPI_MISO_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SPI_MISO_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_aud_clk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_AUD_CLK_MASK),
				    (unsigned int) (PMIC_RG_OCTL_AUD_CLK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_aud_mosi(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_AUD_MOSI_MASK),
				    (unsigned int) (PMIC_RG_OCTL_AUD_MOSI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_aud_miso(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_AUD_MISO_MASK),
				    (unsigned int) (PMIC_RG_OCTL_AUD_MISO_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_sim1_ap_sclk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SIM1_AP_SCLK_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SIM1_AP_SCLK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_sim1_ap_srst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SIM1_AP_SRST_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SIM1_AP_SRST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_simls1_sclk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SIMLS1_SCLK_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SIMLS1_SCLK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_simls1_srst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SIMLS1_SRST_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SIMLS1_SRST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_sim2_ap_sclk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SIM2_AP_SCLK_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SIM2_AP_SCLK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_sim2_ap_srst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SIM2_AP_SRST_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SIM2_AP_SRST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_simls2_sclk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SIMLS2_SCLK_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SIMLS2_SCLK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_simls2_srst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SIMLS2_SRST_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SIMLS2_SRST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_simls1_sclk_conf(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SIMLS1_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SIMLS1_SCLK_CONF_MASK),
				    (unsigned int) (PMIC_RG_SIMLS1_SCLK_CONF_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_simls1_srst_conf(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SIMLS1_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SIMLS1_SRST_CONF_MASK),
				    (unsigned int) (PMIC_RG_SIMLS1_SRST_CONF_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_simls2_sclk_conf(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SIMLS2_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SIMLS2_SCLK_CONF_MASK),
				    (unsigned int) (PMIC_RG_SIMLS2_SCLK_CONF_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_simls2_srst_conf(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SIMLS2_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SIMLS2_SRST_CONF_MASK),
				    (unsigned int) (PMIC_RG_SIMLS2_SRST_CONF_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_spkl_ab(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_SPKL_AB_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_SPKL_AB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_spkl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_SPKL_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_SPKL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_bat_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_BAT_L_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_BAT_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_bat_h(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_BAT_H_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_BAT_H_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_watchdog(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_WATCHDOG_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_WATCHDOG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_pwrkey(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_PWRKEY_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_PWRKEY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_thr_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_THR_L_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_THR_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_thr_h(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_THR_H_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_THR_H_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_vbaton_undet(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_VBATON_UNDET_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_VBATON_UNDET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_bvalid_det(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_BVALID_DET_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_BVALID_DET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_chrdet(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_CHRDET_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_CHRDET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_ov(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_OV_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_OV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_ldo(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_LDO_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_LDO_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_fchrkey(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_FCHRKEY_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_FCHRKEY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_accdet(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_ACCDET_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_ACCDET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_audio(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_AUDIO_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_AUDIO_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_rtc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_RTC_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_RTC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_vproc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_VPROC_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_VPROC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_vsys(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_VSYS_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_VSYS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_vpa(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_VPA_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_VPA_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_polarity(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_MISC_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_POLARITY_MASK),
				    (unsigned int) (PMIC_POLARITY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_polarity_vbaton_undet(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_MISC_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_POLARITY_VBATON_UNDET_MASK),
				    (unsigned int) (PMIC_POLARITY_VBATON_UNDET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_polarity_bvalid_det(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_MISC_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_POLARITY_BVALID_DET_MASK),
				    (unsigned int) (PMIC_POLARITY_BVALID_DET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_fchrkey_int_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_MISC_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FCHRKEY_INT_SEL_MASK),
				    (unsigned int) (PMIC_RG_FCHRKEY_INT_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pwrkey_int_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_MISC_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PWRKEY_INT_SEL_MASK),
				    (unsigned int) (PMIC_RG_PWRKEY_INT_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_ivgen_ext_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_MISC_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_IVGEN_EXT_EN_MASK),
				    (unsigned int) (PMIC_IVGEN_EXT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rg_int_status_spkl_ab(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_SPKL_AB_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_SPKL_AB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_spkl(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_SPKL_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_SPKL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_bat_l(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_BAT_L_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_BAT_L_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_bat_h(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_BAT_H_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_BAT_H_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_watchdog(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_WATCHDOG_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_WATCHDOG_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_pwrkey(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_PWRKEY_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_PWRKEY_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_thr_l(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_THR_L_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_THR_L_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_thr_h(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_THR_H_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_THR_H_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_vbaton_undet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_VBATON_UNDET_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_VBATON_UNDET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_bvalid_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_BVALID_DET_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_BVALID_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_chrdet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_CHRDET_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_CHRDET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_ov(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_OV_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_OV_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_ldo(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_LDO_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_LDO_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_fchrkey(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_FCHRKEY_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_FCHRKEY_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_accdet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_ACCDET_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_ACCDET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_audio(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_AUDIO_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_AUDIO_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_rtc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_RTC_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_RTC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_vproc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_VPROC_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_VPROC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_vsys(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_VSYS_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_VSYS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_vpa(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_VPA_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_VPA_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_oc_gear_bvalid_det(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_GEAR_0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_OC_GEAR_BVALID_DET_MASK),
				    (unsigned int) (PMIC_OC_GEAR_BVALID_DET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_oc_gear_vbaton_undet(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_GEAR_1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_OC_GEAR_VBATON_UNDET_MASK),
				    (unsigned int) (PMIC_OC_GEAR_VBATON_UNDET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_oc_gear_ldo(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_GEAR_2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_OC_GEAR_LDO_MASK),
				    (unsigned int) (PMIC_OC_GEAR_LDO_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_oc_thd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL_VPROC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_OC_THD_MASK),
				    (unsigned int) (PMIC_VPROC_OC_THD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_oc_wnd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL_VPROC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_OC_WND_MASK),
				    (unsigned int) (PMIC_VPROC_OC_WND_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_deg_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL_VPROC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_DEG_EN_MASK),
				    (unsigned int) (PMIC_VPROC_DEG_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_oc_thd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL_VSYS),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_OC_THD_MASK),
				    (unsigned int) (PMIC_VSYS_OC_THD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_oc_wnd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL_VSYS),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_OC_WND_MASK),
				    (unsigned int) (PMIC_VSYS_OC_WND_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_deg_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL_VSYS),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_DEG_EN_MASK),
				    (unsigned int) (PMIC_VSYS_DEG_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_oc_thd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL_VPA),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_OC_THD_MASK),
				    (unsigned int) (PMIC_VPA_OC_THD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_oc_wnd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL_VPA),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_OC_WND_MASK),
				    (unsigned int) (PMIC_VPA_OC_WND_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_deg_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL_VPA),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_DEG_EN_MASK),
				    (unsigned int) (PMIC_VPA_DEG_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fqmtr_tcksel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FQMTR_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FQMTR_TCKSEL_MASK),
				    (unsigned int) (PMIC_FQMTR_TCKSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_fqmtr_busy(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FQMTR_CON0),
				  (&val),
				  (unsigned int) (PMIC_FQMTR_BUSY_MASK),
				  (unsigned int) (PMIC_FQMTR_BUSY_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_fqmtr_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FQMTR_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FQMTR_EN_MASK),
				    (unsigned int) (PMIC_FQMTR_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fqmtr_winset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FQMTR_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FQMTR_WINSET_MASK),
				    (unsigned int) (PMIC_FQMTR_WINSET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_fqmtr_data(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FQMTR_CON2),
				  (&val),
				  (unsigned int) (PMIC_FQMTR_DATA_MASK),
				  (unsigned int) (PMIC_FQMTR_DATA_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_spi_con(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_SPI_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPI_CON_MASK),
				    (unsigned int) (PMIC_RG_SPI_CON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_dew_dio_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DEW_DIO_EN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DEW_DIO_EN_MASK),
				    (unsigned int) (PMIC_DEW_DIO_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_dew_read_test(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DEW_READ_TEST),
				  (&val),
				  (unsigned int) (PMIC_DEW_READ_TEST_MASK),
				  (unsigned int) (PMIC_DEW_READ_TEST_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_dew_write_test(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DEW_WRITE_TEST),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DEW_WRITE_TEST_MASK),
				    (unsigned int) (PMIC_DEW_WRITE_TEST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_dew_crc_swrst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DEW_CRC_SWRST),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DEW_CRC_SWRST_MASK),
				    (unsigned int) (PMIC_DEW_CRC_SWRST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_dew_crc_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DEW_CRC_EN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DEW_CRC_EN_MASK),
				    (unsigned int) (PMIC_DEW_CRC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_dew_crc_val(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DEW_CRC_VAL),
				  (&val),
				  (unsigned int) (PMIC_DEW_CRC_VAL_MASK),
				  (unsigned int) (PMIC_DEW_CRC_VAL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_dew_dbg_mon_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DEW_DBG_MON_SEL),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DEW_DBG_MON_SEL_MASK),
				    (unsigned int) (PMIC_DEW_DBG_MON_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_dew_cipher_key_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DEW_CIPHER_KEY_SEL),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DEW_CIPHER_KEY_SEL_MASK),
				    (unsigned int) (PMIC_DEW_CIPHER_KEY_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_dew_cipher_iv_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DEW_CIPHER_IV_SEL),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DEW_CIPHER_IV_SEL_MASK),
				    (unsigned int) (PMIC_DEW_CIPHER_IV_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_dew_cipher_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DEW_CIPHER_EN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DEW_CIPHER_EN_MASK),
				    (unsigned int) (PMIC_DEW_CIPHER_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_dew_cipher_rdy(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DEW_CIPHER_RDY),
				  (&val),
				  (unsigned int) (PMIC_DEW_CIPHER_RDY_MASK),
				  (unsigned int) (PMIC_DEW_CIPHER_RDY_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_dew_cipher_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DEW_CIPHER_MODE),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DEW_CIPHER_MODE_MASK),
				    (unsigned int) (PMIC_DEW_CIPHER_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_dew_cipher_swrst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DEW_CIPHER_SWRST),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DEW_CIPHER_SWRST_MASK),
				    (unsigned int) (PMIC_DEW_CIPHER_SWRST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_dew_rddmy_no(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DEW_RDDMY_NO),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DEW_RDDMY_NO_MASK),
				    (unsigned int) (PMIC_DEW_RDDMY_NO_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_dew_rdata_dly_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DEW_RDATA_DLY_SEL),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DEW_RDATA_DLY_SEL_MASK),
				    (unsigned int) (PMIC_DEW_RDATA_DLY_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smps_testmode_b(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMPS_TESTMODE_B_MASK),
				    (unsigned int) (PMIC_RG_SMPS_TESTMODE_B_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vproc_dig_mon(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_CON1),
				  (&val),
				  (unsigned int) (PMIC_QI_VPROC_DIG_MON_MASK),
				  (unsigned int) (PMIC_QI_VPROC_DIG_MON_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsys_dig_mon(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_CON1),
				  (&val),
				  (unsigned int) (PMIC_QI_VSYS_DIG_MON_MASK),
				  (unsigned int) (PMIC_QI_VSYS_DIG_MON_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsleep_src0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSLEEP_SRC0_MASK),
				    (unsigned int) (PMIC_VSLEEP_SRC0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsleep_src1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSLEEP_SRC1_MASK),
				    (unsigned int) (PMIC_VSLEEP_SRC1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_r2r_src0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_R2R_SRC0_MASK),
				    (unsigned int) (PMIC_R2R_SRC0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_r2r_src1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_R2R_SRC1_MASK),
				    (unsigned int) (PMIC_R2R_SRC1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_buck_osc_sel_src0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_BUCK_OSC_SEL_SRC0_MASK),
				    (unsigned int) (PMIC_BUCK_OSC_SEL_SRC0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_srclken_dly_src1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SRCLKEN_DLY_SRC1_MASK),
				    (unsigned int) (PMIC_SRCLKEN_DLY_SRC1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_buck_con5_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_BUCK_CON5_RSV0_MASK),
				    (unsigned int) (PMIC_BUCK_CON5_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vproc_triml(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPROC_TRIML_MASK),
				    (unsigned int) (PMIC_RG_VPROC_TRIML_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vproc_trimh(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPROC_TRIMH_MASK),
				    (unsigned int) (PMIC_RG_VPROC_TRIMH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vproc_csm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPROC_CSM_MASK),
				    (unsigned int) (PMIC_RG_VPROC_CSM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vproc_zxos_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPROC_ZXOS_TRIM_MASK),
				    (unsigned int) (PMIC_RG_VPROC_ZXOS_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vproc_rzsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPROC_RZSEL_MASK),
				    (unsigned int) (PMIC_RG_VPROC_RZSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vproc_cc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPROC_CC_MASK),
				    (unsigned int) (PMIC_RG_VPROC_CC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vproc_csr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPROC_CSR_MASK),
				    (unsigned int) (PMIC_RG_VPROC_CSR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vproc_csl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPROC_CSL_MASK),
				    (unsigned int) (PMIC_RG_VPROC_CSL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vproc_zx_os(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPROC_ZX_OS_MASK),
				    (unsigned int) (PMIC_RG_VPROC_ZX_OS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vproc_avp_os(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPROC_AVP_OS_MASK),
				    (unsigned int) (PMIC_RG_VPROC_AVP_OS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vproc_avp_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPROC_AVP_EN_MASK),
				    (unsigned int) (PMIC_RG_VPROC_AVP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vproc_modeset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPROC_MODESET_MASK),
				    (unsigned int) (PMIC_RG_VPROC_MODESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vproc_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPROC_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VPROC_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vproc_slp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPROC_SLP_MASK),
				    (unsigned int) (PMIC_RG_VPROC_SLP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_qi_vproc_vsleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_QI_VPROC_VSLEEP_MASK),
				    (unsigned int) (PMIC_QI_VPROC_VSLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vproc_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPROC_RSV_MASK),
				    (unsigned int) (PMIC_RG_VPROC_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_en_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_EN_CTRL_MASK),
				    (unsigned int) (PMIC_VPROC_EN_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_vosel_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_VOSEL_CTRL_MASK),
				    (unsigned int) (PMIC_VPROC_VOSEL_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_dlc_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_DLC_CTRL_MASK),
				    (unsigned int) (PMIC_VPROC_DLC_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_burst_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_BURST_CTRL_MASK),
				    (unsigned int) (PMIC_VPROC_BURST_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_EN_MASK),
				    (unsigned int) (PMIC_VPROC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vproc_stb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPROC_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VPROC_STB_MASK),
				  (unsigned int) (PMIC_QI_VPROC_STB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vproc_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPROC_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VPROC_EN_MASK),
				  (unsigned int) (PMIC_QI_VPROC_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vproc_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPROC_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VPROC_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VPROC_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vproc_sfchg_frate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_SFCHG_FRATE_MASK),
				    (unsigned int) (PMIC_VPROC_SFCHG_FRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_sfchg_fen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_SFCHG_FEN_MASK),
				    (unsigned int) (PMIC_VPROC_SFCHG_FEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_sfchg_rrate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_SFCHG_RRATE_MASK),
				    (unsigned int) (PMIC_VPROC_SFCHG_RRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_sfchg_ren(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_SFCHG_REN_MASK),
				    (unsigned int) (PMIC_VPROC_SFCHG_REN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_VOSEL_MASK),
				    (unsigned int) (PMIC_VPROC_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_vosel_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_VOSEL_ON_MASK),
				    (unsigned int) (PMIC_VPROC_VOSEL_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_vosel_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_VOSEL_SLEEP_MASK),
				    (unsigned int) (PMIC_VPROC_VOSEL_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vproc_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPROC_CON12),
				  (&val),
				  (unsigned int) (PMIC_NI_VPROC_VOSEL_MASK),
				  (unsigned int) (PMIC_NI_VPROC_VOSEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vproc_burst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_BURST_MASK),
				    (unsigned int) (PMIC_VPROC_BURST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_burst_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_BURST_ON_MASK),
				    (unsigned int) (PMIC_VPROC_BURST_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_burst_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_BURST_SLEEP_MASK),
				    (unsigned int) (PMIC_VPROC_BURST_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vproc_burst(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPROC_CON13),
				  (&val),
				  (unsigned int) (PMIC_QI_VPROC_BURST_MASK),
				  (unsigned int) (PMIC_QI_VPROC_BURST_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vproc_dlc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_DLC_MASK),
				    (unsigned int) (PMIC_VPROC_DLC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_dlc_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_DLC_ON_MASK),
				    (unsigned int) (PMIC_VPROC_DLC_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_dlc_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_DLC_SLEEP_MASK),
				    (unsigned int) (PMIC_VPROC_DLC_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vproc_dlc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPROC_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VPROC_DLC_MASK),
				  (unsigned int) (PMIC_QI_VPROC_DLC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vproc_dlc_n(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_DLC_N_MASK),
				    (unsigned int) (PMIC_VPROC_DLC_N_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_dlc_n_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_DLC_N_ON_MASK),
				    (unsigned int) (PMIC_VPROC_DLC_N_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_dlc_n_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_DLC_N_SLEEP_MASK),
				    (unsigned int) (PMIC_VPROC_DLC_N_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vproc_dlc_n(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPROC_CON15),
				  (&val),
				  (unsigned int) (PMIC_QI_VPROC_DLC_N_MASK),
				  (unsigned int) (PMIC_QI_VPROC_DLC_N_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vproc_transtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_TRANSTD_MASK),
				    (unsigned int) (PMIC_VPROC_TRANSTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_vosel_trans_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_VOSEL_TRANS_EN_MASK),
				    (unsigned int) (PMIC_VPROC_VOSEL_TRANS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_vosel_trans_once(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_VOSEL_TRANS_ONCE_MASK),
				    (unsigned int) (PMIC_VPROC_VOSEL_TRANS_ONCE_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vproc_vosel_trans(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPROC_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VPROC_VOSEL_TRANS_MASK),
				  (unsigned int) (PMIC_NI_VPROC_VOSEL_TRANS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vproc_vsleep_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_VSLEEP_EN_MASK),
				    (unsigned int) (PMIC_VPROC_VSLEEP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_r2r_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_R2R_PDN_MASK),
				    (unsigned int) (PMIC_VPROC_R2R_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vproc_vsleep_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPROC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPROC_VSLEEP_SEL_MASK),
				    (unsigned int) (PMIC_VPROC_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vproc_r2r_pdn(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPROC_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VPROC_R2R_PDN_MASK),
				  (unsigned int) (PMIC_NI_VPROC_R2R_PDN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_vproc_vsleep_sel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPROC_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VPROC_VSLEEP_SEL_MASK),
				  (unsigned int) (PMIC_NI_VPROC_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vsys_triml(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSYS_TRIML_MASK),
				    (unsigned int) (PMIC_RG_VSYS_TRIML_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsys_trimh(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSYS_TRIMH_MASK),
				    (unsigned int) (PMIC_RG_VSYS_TRIMH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsys_csm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSYS_CSM_MASK),
				    (unsigned int) (PMIC_RG_VSYS_CSM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsys_zxos_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSYS_ZXOS_TRIM_MASK),
				    (unsigned int) (PMIC_RG_VSYS_ZXOS_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsys_rzsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSYS_RZSEL_MASK),
				    (unsigned int) (PMIC_RG_VSYS_RZSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsys_cc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSYS_CC_MASK),
				    (unsigned int) (PMIC_RG_VSYS_CC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsys_csr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSYS_CSR_MASK),
				    (unsigned int) (PMIC_RG_VSYS_CSR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsys_csl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSYS_CSL_MASK),
				    (unsigned int) (PMIC_RG_VSYS_CSL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsys_zx_os(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSYS_ZX_OS_MASK),
				    (unsigned int) (PMIC_RG_VSYS_ZX_OS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsys_avp_os(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSYS_AVP_OS_MASK),
				    (unsigned int) (PMIC_RG_VSYS_AVP_OS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsys_avp_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSYS_AVP_EN_MASK),
				    (unsigned int) (PMIC_RG_VSYS_AVP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsys_modeset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSYS_MODESET_MASK),
				    (unsigned int) (PMIC_RG_VSYS_MODESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsys_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSYS_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VSYS_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsys_slp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSYS_SLP_MASK),
				    (unsigned int) (PMIC_RG_VSYS_SLP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_qi_vsys_vsleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_QI_VSYS_VSLEEP_MASK),
				    (unsigned int) (PMIC_QI_VSYS_VSLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsys_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSYS_RSV_MASK),
				    (unsigned int) (PMIC_RG_VSYS_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_en_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_EN_CTRL_MASK),
				    (unsigned int) (PMIC_VSYS_EN_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_vosel_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_VOSEL_CTRL_MASK),
				    (unsigned int) (PMIC_VSYS_VOSEL_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_dlc_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_DLC_CTRL_MASK),
				    (unsigned int) (PMIC_VSYS_DLC_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_burst_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_BURST_CTRL_MASK),
				    (unsigned int) (PMIC_VSYS_BURST_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_EN_MASK),
				    (unsigned int) (PMIC_VSYS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsys_stb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSYS_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VSYS_STB_MASK),
				  (unsigned int) (PMIC_QI_VSYS_STB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsys_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSYS_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VSYS_EN_MASK),
				  (unsigned int) (PMIC_QI_VSYS_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsys_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSYS_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VSYS_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VSYS_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsys_sfchg_frate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_SFCHG_FRATE_MASK),
				    (unsigned int) (PMIC_VSYS_SFCHG_FRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_sfchg_fen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_SFCHG_FEN_MASK),
				    (unsigned int) (PMIC_VSYS_SFCHG_FEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_sfchg_rrate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_SFCHG_RRATE_MASK),
				    (unsigned int) (PMIC_VSYS_SFCHG_RRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_sfchg_ren(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_SFCHG_REN_MASK),
				    (unsigned int) (PMIC_VSYS_SFCHG_REN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_VOSEL_MASK),
				    (unsigned int) (PMIC_VSYS_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_vosel_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_VOSEL_ON_MASK),
				    (unsigned int) (PMIC_VSYS_VOSEL_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_vosel_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_VOSEL_SLEEP_MASK),
				    (unsigned int) (PMIC_VSYS_VOSEL_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vsys_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSYS_CON12),
				  (&val),
				  (unsigned int) (PMIC_NI_VSYS_VOSEL_MASK),
				  (unsigned int) (PMIC_NI_VSYS_VOSEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsys_burst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_BURST_MASK),
				    (unsigned int) (PMIC_VSYS_BURST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_burst_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_BURST_ON_MASK),
				    (unsigned int) (PMIC_VSYS_BURST_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_burst_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_BURST_SLEEP_MASK),
				    (unsigned int) (PMIC_VSYS_BURST_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsys_burst(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSYS_CON13),
				  (&val),
				  (unsigned int) (PMIC_QI_VSYS_BURST_MASK),
				  (unsigned int) (PMIC_QI_VSYS_BURST_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsys_dlc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_DLC_MASK),
				    (unsigned int) (PMIC_VSYS_DLC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_dlc_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_DLC_ON_MASK),
				    (unsigned int) (PMIC_VSYS_DLC_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_dlc_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_DLC_SLEEP_MASK),
				    (unsigned int) (PMIC_VSYS_DLC_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsys_dlc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSYS_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VSYS_DLC_MASK),
				  (unsigned int) (PMIC_QI_VSYS_DLC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsys_dlc_n(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_DLC_N_MASK),
				    (unsigned int) (PMIC_VSYS_DLC_N_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_dlc_n_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_DLC_N_ON_MASK),
				    (unsigned int) (PMIC_VSYS_DLC_N_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_dlc_n_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_DLC_N_SLEEP_MASK),
				    (unsigned int) (PMIC_VSYS_DLC_N_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsys_dlc_n(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSYS_CON15),
				  (&val),
				  (unsigned int) (PMIC_QI_VSYS_DLC_N_MASK),
				  (unsigned int) (PMIC_QI_VSYS_DLC_N_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsys_transtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_TRANSTD_MASK),
				    (unsigned int) (PMIC_VSYS_TRANSTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_vosel_trans_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_VOSEL_TRANS_EN_MASK),
				    (unsigned int) (PMIC_VSYS_VOSEL_TRANS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_vosel_trans_once(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_VOSEL_TRANS_ONCE_MASK),
				    (unsigned int) (PMIC_VSYS_VOSEL_TRANS_ONCE_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vsys_vosel_trans(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSYS_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VSYS_VOSEL_TRANS_MASK),
				  (unsigned int) (PMIC_NI_VSYS_VOSEL_TRANS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsys_vsleep_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_VSLEEP_EN_MASK),
				    (unsigned int) (PMIC_VSYS_VSLEEP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_r2r_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_R2R_PDN_MASK),
				    (unsigned int) (PMIC_VSYS_R2R_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsys_vsleep_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSYS_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSYS_VSLEEP_SEL_MASK),
				    (unsigned int) (PMIC_VSYS_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vsys_r2r_pdn(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSYS_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VSYS_R2R_PDN_MASK),
				  (unsigned int) (PMIC_NI_VSYS_R2R_PDN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_vsys_vsleep_sel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSYS_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VSYS_VSLEEP_SEL_MASK),
				  (unsigned int) (PMIC_NI_VSYS_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vpa_triml(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPA_TRIML_MASK),
				    (unsigned int) (PMIC_RG_VPA_TRIML_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpa_trimh(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPA_TRIMH_MASK),
				    (unsigned int) (PMIC_RG_VPA_TRIMH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpa_rzsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPA_RZSEL_MASK),
				    (unsigned int) (PMIC_RG_VPA_RZSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpa_cc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPA_CC_MASK),
				    (unsigned int) (PMIC_RG_VPA_CC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpa_csr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPA_CSR_MASK),
				    (unsigned int) (PMIC_RG_VPA_CSR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpa_csl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPA_CSL_MASK),
				    (unsigned int) (PMIC_RG_VPA_CSL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpa_slew_nmos(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPA_SLEW_NMOS_MASK),
				    (unsigned int) (PMIC_RG_VPA_SLEW_NMOS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpa_slew(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPA_SLEW_MASK),
				    (unsigned int) (PMIC_RG_VPA_SLEW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpa_zx_os(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPA_ZX_OS_MASK),
				    (unsigned int) (PMIC_RG_VPA_ZX_OS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpa_modeset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPA_MODESET_MASK),
				    (unsigned int) (PMIC_RG_VPA_MODESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpa_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPA_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VPA_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpa_csmir(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPA_CSMIR_MASK),
				    (unsigned int) (PMIC_RG_VPA_CSMIR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpa_vbat_del(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPA_VBAT_DEL_MASK),
				    (unsigned int) (PMIC_RG_VPA_VBAT_DEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpa_slp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPA_SLP_MASK),
				    (unsigned int) (PMIC_RG_VPA_SLP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpa_gpu_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPA_GPU_EN_MASK),
				    (unsigned int) (PMIC_RG_VPA_GPU_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpa_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPA_RSV_MASK),
				    (unsigned int) (PMIC_RG_VPA_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_en_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_EN_CTRL_MASK),
				    (unsigned int) (PMIC_VPA_EN_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_vosel_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_VOSEL_CTRL_MASK),
				    (unsigned int) (PMIC_VPA_VOSEL_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_dlc_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_DLC_CTRL_MASK),
				    (unsigned int) (PMIC_VPA_DLC_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_burst_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_BURST_CTRL_MASK),
				    (unsigned int) (PMIC_VPA_BURST_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_EN_MASK),
				    (unsigned int) (PMIC_VPA_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vpa_stb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPA_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VPA_STB_MASK),
				  (unsigned int) (PMIC_QI_VPA_STB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vpa_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPA_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VPA_EN_MASK),
				  (unsigned int) (PMIC_QI_VPA_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vpa_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPA_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VPA_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VPA_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vpa_sfchg_frate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_SFCHG_FRATE_MASK),
				    (unsigned int) (PMIC_VPA_SFCHG_FRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_sfchg_fen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_SFCHG_FEN_MASK),
				    (unsigned int) (PMIC_VPA_SFCHG_FEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_sfchg_rrate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_SFCHG_RRATE_MASK),
				    (unsigned int) (PMIC_VPA_SFCHG_RRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_sfchg_ren(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_SFCHG_REN_MASK),
				    (unsigned int) (PMIC_VPA_SFCHG_REN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_VOSEL_MASK),
				    (unsigned int) (PMIC_VPA_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_vosel_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_VOSEL_ON_MASK),
				    (unsigned int) (PMIC_VPA_VOSEL_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_vosel_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_VOSEL_SLEEP_MASK),
				    (unsigned int) (PMIC_VPA_VOSEL_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vpa_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPA_CON12),
				  (&val),
				  (unsigned int) (PMIC_NI_VPA_VOSEL_MASK),
				  (unsigned int) (PMIC_NI_VPA_VOSEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vpa_dlc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_DLC_MASK),
				    (unsigned int) (PMIC_VPA_DLC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_dlc_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_DLC_ON_MASK),
				    (unsigned int) (PMIC_VPA_DLC_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_dlc_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_DLC_SLEEP_MASK),
				    (unsigned int) (PMIC_VPA_DLC_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vpa_dlc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPA_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VPA_DLC_MASK),
				  (unsigned int) (PMIC_QI_VPA_DLC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vpa_bursth(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_BURSTH_MASK),
				    (unsigned int) (PMIC_VPA_BURSTH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_bursth_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_BURSTH_ON_MASK),
				    (unsigned int) (PMIC_VPA_BURSTH_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_bursth_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_BURSTH_SLEEP_MASK),
				    (unsigned int) (PMIC_VPA_BURSTH_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vpa_bursth(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPA_CON16),
				  (&val),
				  (unsigned int) (PMIC_QI_VPA_BURSTH_MASK),
				  (unsigned int) (PMIC_QI_VPA_BURSTH_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vpa_burstl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_BURSTL_MASK),
				    (unsigned int) (PMIC_VPA_BURSTL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_burstl_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_BURSTL_ON_MASK),
				    (unsigned int) (PMIC_VPA_BURSTL_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_burstl_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_BURSTL_SLEEP_MASK),
				    (unsigned int) (PMIC_VPA_BURSTL_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vpa_burstl(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPA_CON17),
				  (&val),
				  (unsigned int) (PMIC_QI_VPA_BURSTL_MASK),
				  (unsigned int) (PMIC_QI_VPA_BURSTL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vpa_transtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_TRANSTD_MASK),
				    (unsigned int) (PMIC_VPA_TRANSTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_vosel_trans_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_VOSEL_TRANS_EN_MASK),
				    (unsigned int) (PMIC_VPA_VOSEL_TRANS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_vosel_trans_once(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_VOSEL_TRANS_ONCE_MASK),
				    (unsigned int) (PMIC_VPA_VOSEL_TRANS_ONCE_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vpa_dvs_bw(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPA_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VPA_DVS_BW_MASK),
				  (unsigned int) (PMIC_NI_VPA_DVS_BW_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vpa_dlc_map_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_DLC_MAP_EN_MASK),
				    (unsigned int) (PMIC_VPA_DLC_MAP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_vosel_dlc001(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_VOSEL_DLC001_MASK),
				    (unsigned int) (PMIC_VPA_VOSEL_DLC001_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_vosel_dlc011(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_VOSEL_DLC011_MASK),
				    (unsigned int) (PMIC_VPA_VOSEL_DLC011_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpa_vosel_dlc111(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPA_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPA_VOSEL_DLC111_MASK),
				    (unsigned int) (PMIC_VPA_VOSEL_DLC111_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_rst_done(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_RST_DONE_MASK),
				    (unsigned int) (PMIC_K_RST_DONE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_map_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_MAP_SEL_MASK),
				    (unsigned int) (PMIC_K_MAP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_once_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_ONCE_EN_MASK),
				    (unsigned int) (PMIC_K_ONCE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_once(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_ONCE_MASK),
				    (unsigned int) (PMIC_K_ONCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_start_manual(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_START_MANUAL_MASK),
				    (unsigned int) (PMIC_K_START_MANUAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_src_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_SRC_SEL_MASK),
				    (unsigned int) (PMIC_K_SRC_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_auto_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_AUTO_EN_MASK),
				    (unsigned int) (PMIC_K_AUTO_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_inv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_INV_MASK), (unsigned int) (PMIC_K_INV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_control_smps(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_CONTROL_SMPS_MASK),
				    (unsigned int) (PMIC_K_CONTROL_SMPS_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_k_result(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_K_CON2),
				  (&val),
				  (unsigned int) (PMIC_K_RESULT_MASK),
				  (unsigned int) (PMIC_K_RESULT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_k_done(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_K_CON2),
				  (&val),
				  (unsigned int) (PMIC_K_DONE_MASK), (unsigned int) (PMIC_K_DONE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_k_control(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_K_CON2),
				  (&val),
				  (unsigned int) (PMIC_K_CONTROL_MASK),
				  (unsigned int) (PMIC_K_CONTROL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_smps_osc_cal(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_K_CON2),
				  (&val),
				  (unsigned int) (PMIC_QI_SMPS_OSC_CAL_MASK),
				  (unsigned int) (PMIC_QI_SMPS_OSC_CAL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_isink_ch0_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK0_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_CH0_MODE_MASK),
				    (unsigned int) (PMIC_ISINK_CH0_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink0_rsv1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK0_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK0_RSV1_MASK),
				    (unsigned int) (PMIC_ISINK0_RSV1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_dim0_duty(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK0_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_DIM0_DUTY_MASK),
				    (unsigned int) (PMIC_ISINK_DIM0_DUTY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink0_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK0_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK0_RSV0_MASK),
				    (unsigned int) (PMIC_ISINK0_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_dim0_fsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK0_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_DIM0_FSEL_MASK),
				    (unsigned int) (PMIC_ISINK_DIM0_FSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_sfstr0_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK0_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_SFSTR0_EN_MASK),
				    (unsigned int) (PMIC_ISINK_SFSTR0_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_sfstr0_tc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK0_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_SFSTR0_TC_MASK),
				    (unsigned int) (PMIC_ISINK_SFSTR0_TC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_ch0_step(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK0_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_CH0_STEP_MASK),
				    (unsigned int) (PMIC_ISINK_CH0_STEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_breath0_toff_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK0_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_BREATH0_TOFF_SEL_MASK),
				    (unsigned int) (PMIC_ISINK_BREATH0_TOFF_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_breath0_ton_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK0_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_BREATH0_TON_SEL_MASK),
				    (unsigned int) (PMIC_ISINK_BREATH0_TON_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_breath0_trf_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK0_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_BREATH0_TRF_SEL_MASK),
				    (unsigned int) (PMIC_ISINK_BREATH0_TRF_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_ch1_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_CH1_MODE_MASK),
				    (unsigned int) (PMIC_ISINK_CH1_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink1_rsv1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK1_RSV1_MASK),
				    (unsigned int) (PMIC_ISINK1_RSV1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_dim1_duty(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_DIM1_DUTY_MASK),
				    (unsigned int) (PMIC_ISINK_DIM1_DUTY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink1_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK1_RSV0_MASK),
				    (unsigned int) (PMIC_ISINK1_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_dim1_fsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK1_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_DIM1_FSEL_MASK),
				    (unsigned int) (PMIC_ISINK_DIM1_FSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_sfstr1_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK1_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_SFSTR1_EN_MASK),
				    (unsigned int) (PMIC_ISINK_SFSTR1_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_sfstr1_tc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK1_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_SFSTR1_TC_MASK),
				    (unsigned int) (PMIC_ISINK_SFSTR1_TC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_ch1_step(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK1_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_CH1_STEP_MASK),
				    (unsigned int) (PMIC_ISINK_CH1_STEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_breath1_toff_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK1_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_BREATH1_TOFF_SEL_MASK),
				    (unsigned int) (PMIC_ISINK_BREATH1_TOFF_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_breath1_ton_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK1_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_BREATH1_TON_SEL_MASK),
				    (unsigned int) (PMIC_ISINK_BREATH1_TON_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_breath1_trf_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK1_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_BREATH1_TRF_SEL_MASK),
				    (unsigned int) (PMIC_ISINK_BREATH1_TRF_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_ch2_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_CH2_MODE_MASK),
				    (unsigned int) (PMIC_ISINK_CH2_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink2_rsv1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK2_RSV1_MASK),
				    (unsigned int) (PMIC_ISINK2_RSV1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_dim2_duty(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_DIM2_DUTY_MASK),
				    (unsigned int) (PMIC_ISINK_DIM2_DUTY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink2_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK2_RSV0_MASK),
				    (unsigned int) (PMIC_ISINK2_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_dim2_fsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_DIM2_FSEL_MASK),
				    (unsigned int) (PMIC_ISINK_DIM2_FSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_sfstr2_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK2_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_SFSTR2_EN_MASK),
				    (unsigned int) (PMIC_ISINK_SFSTR2_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_sfstr2_tc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK2_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_SFSTR2_TC_MASK),
				    (unsigned int) (PMIC_ISINK_SFSTR2_TC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_ch2_step(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK2_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_CH2_STEP_MASK),
				    (unsigned int) (PMIC_ISINK_CH2_STEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_breath2_toff_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK2_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_BREATH2_TOFF_SEL_MASK),
				    (unsigned int) (PMIC_ISINK_BREATH2_TOFF_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_breath2_ton_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK2_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_BREATH2_TON_SEL_MASK),
				    (unsigned int) (PMIC_ISINK_BREATH2_TON_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_breath2_trf_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK2_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_BREATH2_TRF_SEL_MASK),
				    (unsigned int) (PMIC_ISINK_BREATH2_TRF_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_ch3_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_CH3_MODE_MASK),
				    (unsigned int) (PMIC_ISINK_CH3_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink3_rsv1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK3_RSV1_MASK),
				    (unsigned int) (PMIC_ISINK3_RSV1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_dim3_duty(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_DIM3_DUTY_MASK),
				    (unsigned int) (PMIC_ISINK_DIM3_DUTY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink3_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK3_RSV0_MASK),
				    (unsigned int) (PMIC_ISINK3_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_dim3_fsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK3_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_DIM3_FSEL_MASK),
				    (unsigned int) (PMIC_ISINK_DIM3_FSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_sfstr3_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK3_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_SFSTR3_EN_MASK),
				    (unsigned int) (PMIC_ISINK_SFSTR3_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_sfstr3_tc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK3_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_SFSTR3_TC_MASK),
				    (unsigned int) (PMIC_ISINK_SFSTR3_TC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_ch3_step(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK3_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_CH3_STEP_MASK),
				    (unsigned int) (PMIC_ISINK_CH3_STEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_breath3_toff_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK3_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_BREATH3_TOFF_SEL_MASK),
				    (unsigned int) (PMIC_ISINK_BREATH3_TOFF_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_breath3_ton_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK3_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_BREATH3_TON_SEL_MASK),
				    (unsigned int) (PMIC_ISINK_BREATH3_TON_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_breath3_trf_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK3_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_BREATH3_TRF_SEL_MASK),
				    (unsigned int) (PMIC_ISINK_BREATH3_TRF_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_isinks_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_ANA0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ISINKS_RSV_MASK),
				    (unsigned int) (PMIC_RG_ISINKS_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_isink3_double_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_ANA0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ISINK3_DOUBLE_EN_MASK),
				    (unsigned int) (PMIC_RG_ISINK3_DOUBLE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_isink2_double_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_ANA0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ISINK2_DOUBLE_EN_MASK),
				    (unsigned int) (PMIC_RG_ISINK2_DOUBLE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_isink1_double_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_ANA0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ISINK1_DOUBLE_EN_MASK),
				    (unsigned int) (PMIC_RG_ISINK1_DOUBLE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_isink0_double_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_ANA0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ISINK0_DOUBLE_EN_MASK),
				    (unsigned int) (PMIC_RG_ISINK0_DOUBLE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_trim_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_ANA0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TRIM_SEL_MASK),
				    (unsigned int) (PMIC_RG_TRIM_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_trim_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_ANA0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TRIM_EN_MASK),
				    (unsigned int) (PMIC_RG_TRIM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_isink3_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ISINK_ANA1),
				  (&val),
				  (unsigned int) (PMIC_NI_ISINK3_STATUS_MASK),
				  (unsigned int) (PMIC_NI_ISINK3_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_isink2_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ISINK_ANA1),
				  (&val),
				  (unsigned int) (PMIC_NI_ISINK2_STATUS_MASK),
				  (unsigned int) (PMIC_NI_ISINK2_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_isink1_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ISINK_ANA1),
				  (&val),
				  (unsigned int) (PMIC_NI_ISINK1_STATUS_MASK),
				  (unsigned int) (PMIC_NI_ISINK1_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_isink0_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ISINK_ANA1),
				  (&val),
				  (unsigned int) (PMIC_NI_ISINK0_STATUS_MASK),
				  (unsigned int) (PMIC_NI_ISINK0_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_isink_phase0_dly_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_PHASE_DLY),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_PHASE0_DLY_EN_MASK),
				    (unsigned int) (PMIC_ISINK_PHASE0_DLY_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_phase1_dly_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_PHASE_DLY),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_PHASE1_DLY_EN_MASK),
				    (unsigned int) (PMIC_ISINK_PHASE1_DLY_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_phase2_dly_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_PHASE_DLY),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_PHASE2_DLY_EN_MASK),
				    (unsigned int) (PMIC_ISINK_PHASE2_DLY_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_phase3_dly_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_PHASE_DLY),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_PHASE3_DLY_EN_MASK),
				    (unsigned int) (PMIC_ISINK_PHASE3_DLY_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_phase_dly_tc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_PHASE_DLY),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_PHASE_DLY_TC_MASK),
				    (unsigned int) (PMIC_ISINK_PHASE_DLY_TC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_ch0_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_EN_CTRL),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_CH0_EN_MASK),
				    (unsigned int) (PMIC_ISINK_CH0_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_ch1_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_EN_CTRL),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_CH1_EN_MASK),
				    (unsigned int) (PMIC_ISINK_CH1_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_ch2_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_EN_CTRL),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_CH2_EN_MASK),
				    (unsigned int) (PMIC_ISINK_CH2_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_ch3_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_EN_CTRL),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_CH3_EN_MASK),
				    (unsigned int) (PMIC_ISINK_CH3_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_chop0_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_EN_CTRL),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_CHOP0_EN_MASK),
				    (unsigned int) (PMIC_ISINK_CHOP0_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_chop1_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_EN_CTRL),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_CHOP1_EN_MASK),
				    (unsigned int) (PMIC_ISINK_CHOP1_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_chop2_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_EN_CTRL),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_CHOP2_EN_MASK),
				    (unsigned int) (PMIC_ISINK_CHOP2_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_chop3_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINK_EN_CTRL),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_CHOP3_EN_MASK),
				    (unsigned int) (PMIC_ISINK_CHOP3_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_analdorsv1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ANALDORSV1_MASK),
				    (unsigned int) (PMIC_RG_ANALDORSV1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vtcxo_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VTCXO_LP_SEL_MASK),
				    (unsigned int) (PMIC_VTCXO_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vtcxo_lp_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VTCXO_LP_SET_MASK),
				    (unsigned int) (PMIC_VTCXO_LP_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vtcxo_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON1),
				  (&val),
				  (unsigned int) (PMIC_QI_VTCXO_MODE_MASK),
				  (unsigned int) (PMIC_QI_VTCXO_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vtcxo_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VTCXO_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VTCXO_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vtcxo_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VTCXO_EN_MASK),
				    (unsigned int) (PMIC_RG_VTCXO_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vtcxo_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VTCXO_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VTCXO_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vtcxo_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON1),
				  (&val),
				  (unsigned int) (PMIC_QI_VTCXO_EN_MASK),
				  (unsigned int) (PMIC_QI_VTCXO_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_va_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VA_LP_SEL_MASK),
				    (unsigned int) (PMIC_VA_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_va_lp_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VA_LP_SET_MASK),
				    (unsigned int) (PMIC_VA_LP_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_va_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON2),
				  (&val),
				  (unsigned int) (PMIC_QI_VA_MODE_MASK),
				  (unsigned int) (PMIC_QI_VA_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_va_sense_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VA_SENSE_SEL_MASK),
				    (unsigned int) (PMIC_RG_VA_SENSE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_va_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VA_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VA_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_va_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VA_EN_MASK),
				    (unsigned int) (PMIC_RG_VA_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_va_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON2),
				  (&val),
				  (unsigned int) (PMIC_QI_VA_EN_MASK),
				  (unsigned int) (PMIC_QI_VA_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_analdorsv2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ANALDORSV2_MASK),
				    (unsigned int) (PMIC_RG_ANALDORSV2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcama_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMA_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VCAMA_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcama_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMA_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMA_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcama_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMA_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMA_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_va_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VA_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VA_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vtcxo_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VTCXO_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VTCXO_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcama_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON5),
				  (&val),
				  (unsigned int) (PMIC_QI_VCAMA_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VCAMA_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_va_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON5),
				  (&val),
				  (unsigned int) (PMIC_QI_VA_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VA_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vtcxo_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON5),
				  (&val),
				  (unsigned int) (PMIC_QI_VTCXO_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VTCXO_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_analdorsv3(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ANALDORSV3_MASK),
				    (unsigned int) (PMIC_RG_ANALDORSV3_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vtcxo_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VTCXO_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VTCXO_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vtcxo_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VTCXO_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VTCXO_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vtcxo_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VTCXO_CAL_MASK),
				    (unsigned int) (PMIC_RG_VTCXO_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_va_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VA_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VA_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_va_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VA_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VA_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_va_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VA_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VA_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_va_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VA_CAL_MASK),
				    (unsigned int) (PMIC_RG_VA_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcama_fbsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMA_FBSEL_MASK),
				    (unsigned int) (PMIC_RG_VCAMA_FBSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcama_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMA_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMA_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcama_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMA_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VCAMA_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcama_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMA_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VCAMA_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcama_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMA_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VCAMA_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcama_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMA_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VCAMA_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcama_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMA_CAL_MASK),
				    (unsigned int) (PMIC_RG_VCAMA_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_reserve_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RESERVE_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_RESERVE_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aldo_reserve(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ALDO_RESERVE_MASK),
				    (unsigned int) (PMIC_RG_ALDO_RESERVE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn33_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN33_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VCN33_VOSEL_SHIFT)
	    );
	pmic_unlock();
}



static inline void upmu_set_rg_vcn33_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN33_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VCN33_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcn33_on_ctrl_bt(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCN33_ON_CTRL_BT_MASK),
				    (unsigned int) (PMIC_VCN33_ON_CTRL_BT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn33_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN33_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VCN33_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn33_en_bt(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN33_EN_BT_MASK),
				    (unsigned int) (PMIC_RG_VCN33_EN_BT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn33_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN33_CAL_MASK),
				    (unsigned int) (PMIC_RG_VCN33_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn33_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN33_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VCN33_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn33_en_wifi(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN33_EN_WIFI_MASK),
				    (unsigned int) (PMIC_RG_VCN33_EN_WIFI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcn33_on_ctrl_wifi(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCN33_ON_CTRL_WIFI_MASK),
				    (unsigned int) (PMIC_VCN33_ON_CTRL_WIFI_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcn33_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON17),
				  (&val),
				  (unsigned int) (PMIC_QI_VCN33_EN_MASK),
				  (unsigned int) (PMIC_QI_VCN33_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vcn28_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN28_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VCN28_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn28_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN28_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VCN28_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn28_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN28_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VCN28_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn28_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN28_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VCN28_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn28_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN28_CAL_MASK),
				    (unsigned int) (PMIC_RG_VCN28_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn28_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN28_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VCN28_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn28_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN28_EN_MASK),
				    (unsigned int) (PMIC_RG_VCN28_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcn28_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCN28_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VCN28_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcn28_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON19),
				  (&val),
				  (unsigned int) (PMIC_QI_VCN28_EN_MASK),
				  (unsigned int) (PMIC_QI_VCN28_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vcn28_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCN28_LP_SEL_MASK),
				    (unsigned int) (PMIC_VCN28_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcn28_lp_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCN28_LP_SET_MASK),
				    (unsigned int) (PMIC_VCN28_LP_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcn28_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON20),
				  (&val),
				  (unsigned int) (PMIC_QI_VCN28_MODE_MASK),
				  (unsigned int) (PMIC_QI_VCN28_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vcn28_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON20),
				  (&val),
				  (unsigned int) (PMIC_QI_VCN28_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VCN28_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vcn33_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCN33_LP_SEL_MASK),
				    (unsigned int) (PMIC_VCN33_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcn33_lp_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCN33_LP_SET_MASK),
				    (unsigned int) (PMIC_VCN33_LP_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcn33_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON21),
				  (&val),
				  (unsigned int) (PMIC_QI_VCN33_MODE_MASK),
				  (unsigned int) (PMIC_QI_VCN33_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vcn33_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN33_EN_MASK),
				    (unsigned int) (PMIC_RG_VCN33_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn33_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN33_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VCN33_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcn33_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON21),
				  (&val),
				  (unsigned int) (PMIC_QI_VCN33_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VCN33_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vio28_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO28_LP_SEL_MASK),
				    (unsigned int) (PMIC_VIO28_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio28_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO28_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VIO28_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vio28_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON0),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO28_MODE_MASK),
				  (unsigned int) (PMIC_QI_VIO28_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vio28_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO28_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VIO28_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio28_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO28_EN_MASK),
				    (unsigned int) (PMIC_VIO28_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vio28_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON0),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO28_EN_MASK),
				  (unsigned int) (PMIC_QI_VIO28_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vusb_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VUSB_LP_SEL_MASK),
				    (unsigned int) (PMIC_VUSB_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vusb_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VUSB_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VUSB_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vusb_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON2),
				  (&val),
				  (unsigned int) (PMIC_QI_VUSB_MODE_MASK),
				  (unsigned int) (PMIC_QI_VUSB_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vusb_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VUSB_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VUSB_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vusb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VUSB_EN_MASK),
				    (unsigned int) (PMIC_RG_VUSB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vusb_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON2),
				  (&val),
				  (unsigned int) (PMIC_QI_VUSB_EN_MASK),
				  (unsigned int) (PMIC_QI_VUSB_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vmc_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VMC_LP_SEL_MASK),
				    (unsigned int) (PMIC_VMC_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vmc_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VMC_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VMC_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vmc_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON3),
				  (&val),
				  (unsigned int) (PMIC_QI_VMC_MODE_MASK),
				  (unsigned int) (PMIC_QI_VMC_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vmc_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VMC_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmc_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_EN_MASK),
				    (unsigned int) (PMIC_RG_VMC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmc_int_dis_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_INT_DIS_SEL_MASK),
				    (unsigned int) (PMIC_RG_VMC_INT_DIS_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vmc_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON3),
				  (&val),
				  (unsigned int) (PMIC_QI_VMC_EN_MASK),
				  (unsigned int) (PMIC_QI_VMC_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vmch_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VMCH_LP_SEL_MASK),
				    (unsigned int) (PMIC_VMCH_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vmch_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VMCH_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VMCH_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vmch_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON5),
				  (&val),
				  (unsigned int) (PMIC_QI_VMCH_MODE_MASK),
				  (unsigned int) (PMIC_QI_VMCH_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vmch_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VMCH_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_EN_MASK),
				    (unsigned int) (PMIC_RG_VMCH_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vmch_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON5),
				  (&val),
				  (unsigned int) (PMIC_QI_VMCH_EN_MASK),
				  (unsigned int) (PMIC_QI_VMCH_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vemc_3v3_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VEMC_3V3_LP_SEL_MASK),
				    (unsigned int) (PMIC_VEMC_3V3_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vemc_3v3_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VEMC_3V3_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VEMC_3V3_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vemc_3v3_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON6),
				  (&val),
				  (unsigned int) (PMIC_QI_VEMC_3V3_MODE_MASK),
				  (unsigned int) (PMIC_QI_VEMC_3V3_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vemc_3v3_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vemc_3v3_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_EN_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vemc_3v3_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON6),
				  (&val),
				  (unsigned int) (PMIC_QI_VEMC_3V3_EN_MASK),
				  (unsigned int) (PMIC_QI_VEMC_3V3_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vgp1_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP1_LP_SEL_MASK),
				    (unsigned int) (PMIC_VGP1_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp1_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP1_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VGP1_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vgp1_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VGP1_MODE_MASK),
				  (unsigned int) (PMIC_QI_VGP1_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vgp1_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP1_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VGP1_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp1_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP1_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP1_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp2_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP2_LP_SEL_MASK),
				    (unsigned int) (PMIC_VGP2_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp2_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP2_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VGP2_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vgp2_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON8),
				  (&val),
				  (unsigned int) (PMIC_QI_VGP2_MODE_MASK),
				  (unsigned int) (PMIC_QI_VGP2_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vgp2_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP2_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VGP2_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp2_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP2_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP2_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp3_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP3_LP_SEL_MASK),
				    (unsigned int) (PMIC_VGP3_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp3_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP3_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VGP3_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vgp3_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON9),
				  (&val),
				  (unsigned int) (PMIC_QI_VGP3_MODE_MASK),
				  (unsigned int) (PMIC_QI_VGP3_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vgp3_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP3_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VGP3_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp3_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP3_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP3_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn_1v8_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN_1V8_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VCN_1V8_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcn_1v8_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCN_1V8_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VCN_1V8_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn_1v8_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN_1V8_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VCN_1V8_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn_1v8_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN_1V8_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VCN_1V8_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn_1v8_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN_1V8_CAL_MASK),
				    (unsigned int) (PMIC_RG_VCN_1V8_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcn_1v8_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCN_1V8_LP_SEL_MASK),
				    (unsigned int) (PMIC_VCN_1V8_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcn_1v8_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCN_1V8_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VCN_1V8_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcn_1v8_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON11),
				  (&val),
				  (unsigned int) (PMIC_QI_VCN_1V8_MODE_MASK),
				  (unsigned int) (PMIC_QI_VCN_1V8_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vcn_1v8_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN_1V8_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VCN_1V8_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn_1v8_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN_1V8_EN_MASK),
				    (unsigned int) (PMIC_RG_VCN_1V8_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcn_1v8_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON11),
				  (&val),
				  (unsigned int) (PMIC_QI_VCN_1V8_EN_MASK),
				  (unsigned int) (PMIC_QI_VCN_1V8_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_stb_sim1_sio(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STB_SIM1_SIO_MASK),
				    (unsigned int) (PMIC_RG_STB_SIM1_SIO_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_re_digldorsv1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RE_DIGLDORSV1_MASK),
				    (unsigned int) (PMIC_RE_DIGLDORSV1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsim1_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSIM1_LP_SEL_MASK),
				    (unsigned int) (PMIC_VSIM1_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsim1_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSIM1_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VSIM1_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsim1_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON13),
				  (&val),
				  (unsigned int) (PMIC_QI_VSIM1_MODE_MASK),
				  (unsigned int) (PMIC_QI_VSIM1_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vsim1_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSIM1_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VSIM1_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsim1_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSIM1_EN_MASK),
				    (unsigned int) (PMIC_RG_VSIM1_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsim2_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSIM2_LP_SEL_MASK),
				    (unsigned int) (PMIC_VSIM2_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsim2_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSIM2_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VSIM2_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsim2_ther_shdn_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSIM2_THER_SHDN_EN_MASK),
				    (unsigned int) (PMIC_VSIM2_THER_SHDN_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsim2_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VSIM2_MODE_MASK),
				  (unsigned int) (PMIC_QI_VSIM2_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vsim2_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSIM2_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VSIM2_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsim2_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSIM2_EN_MASK),
				    (unsigned int) (PMIC_RG_VSIM2_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vrtc_force_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VRTC_FORCE_ON_MASK),
				    (unsigned int) (PMIC_RG_VRTC_FORCE_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vrtc_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VRTC_EN_MASK),
				    (unsigned int) (PMIC_VRTC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vrtc_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON15),
				  (&val),
				  (unsigned int) (PMIC_QI_VRTC_EN_MASK),
				  (unsigned int) (PMIC_QI_VRTC_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vemc_3v3_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VMCH_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmc_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VMC_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vusb_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VUSB_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VUSB_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio28_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO28_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VIO28_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vrtc_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VRTC_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VRTC_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsim2_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSIM2_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VSIM2_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsim1_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSIM1_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VSIM1_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VIBR_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp3_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP3_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP3_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp2_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP2_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP2_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp1_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP1_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP1_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vemc_3v3_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON18),
				  (&val),
				  (unsigned int) (PMIC_QI_VEMC_3V3_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VEMC_3V3_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vmch_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON18),
				  (&val),
				  (unsigned int) (PMIC_QI_VMCH_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VMCH_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vmc_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON18),
				  (&val),
				  (unsigned int) (PMIC_QI_VMC_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VMC_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vusb_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON18),
				  (&val),
				  (unsigned int) (PMIC_QI_VUSB_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VUSB_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vio28_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON18),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO28_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VIO28_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsim2_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON19),
				  (&val),
				  (unsigned int) (PMIC_QI_VSIM2_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VSIM2_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsim1_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON19),
				  (&val),
				  (unsigned int) (PMIC_QI_VSIM1_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VSIM1_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vibr_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON19),
				  (&val),
				  (unsigned int) (PMIC_QI_VIBR_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VIBR_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vgp3_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON19),
				  (&val),
				  (unsigned int) (PMIC_QI_VGP3_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VGP3_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vgp2_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON19),
				  (&val),
				  (unsigned int) (PMIC_QI_VGP2_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VGP2_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vgp1_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON19),
				  (&val),
				  (unsigned int) (PMIC_QI_VGP1_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VGP1_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_stb_sim2_sio(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STB_SIM2_SIO_MASK),
				    (unsigned int) (PMIC_RG_STB_SIM2_SIO_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_re_digldorsv2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RE_DIGLDORSV2_MASK),
				    (unsigned int) (PMIC_RE_DIGLDORSV2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio28_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO28_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VIO28_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio28_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO28_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VIO28_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio28_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO28_CAL_MASK),
				    (unsigned int) (PMIC_RG_VIO28_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vusb_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VUSB_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VUSB_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vusb_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VUSB_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VUSB_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vusb_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VUSB_CAL_MASK),
				    (unsigned int) (PMIC_RG_VUSB_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmc_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VMC_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vmc_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VMC_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VMC_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmc_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VMC_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmc_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VMC_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmc_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VMC_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmc_stb_sel_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_STB_SEL_CAL_MASK),
				    (unsigned int) (PMIC_RG_VMC_STB_SEL_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmc_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_CAL_MASK),
				    (unsigned int) (PMIC_RG_VMC_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON26),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VMCH_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vmch_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON26),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VMCH_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VMCH_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON26),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VMCH_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_db_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON26),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_DB_EN_MASK),
				    (unsigned int) (PMIC_RG_VMCH_DB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON26),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VMCH_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON26),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VMCH_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_stb_sel_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON26),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_STB_SEL_CAL_MASK),
				    (unsigned int) (PMIC_RG_VMCH_STB_SEL_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON26),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_CAL_MASK),
				    (unsigned int) (PMIC_RG_VMCH_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vemc_3v3_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vemc_3v3_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VEMC_3V3_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VEMC_3V3_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vemc_3v3_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vemc_3v3_dl_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_DL_EN_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_DL_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vemc_3v3_db_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_DB_EN_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_DB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vemc_3v3_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vemc_3v3_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vemc_3v3_stb_sel_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_STB_SEL_CAL_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_STB_SEL_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vemc_3v3_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_CAL_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp1_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON28),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP1_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP1_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp1_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON28),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP1_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VGP1_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp1_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON28),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP1_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VGP1_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp1_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON28),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP1_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VGP1_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp1_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON28),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP1_CAL_MASK),
				    (unsigned int) (PMIC_RG_VGP1_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp2_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON29),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP2_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP2_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp2_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON29),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP2_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VGP2_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp2_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON29),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP2_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VGP2_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp2_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON29),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP2_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VGP2_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp2_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON29),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP2_CAL_MASK),
				    (unsigned int) (PMIC_RG_VGP2_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp3_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON30),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP3_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP3_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp3_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON30),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP3_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VGP3_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp3_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON30),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP3_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VGP3_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp3_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON30),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP3_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VGP3_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp3_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON30),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP3_CAL_MASK),
				    (unsigned int) (PMIC_RG_VGP3_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcam_af_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON31),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAM_AF_LP_SEL_MASK),
				    (unsigned int) (PMIC_VCAM_AF_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcam_af_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON31),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAM_AF_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VCAM_AF_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcam_af_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON31),
				  (&val),
				  (unsigned int) (PMIC_QI_VCAM_AF_MODE_MASK),
				  (unsigned int) (PMIC_QI_VCAM_AF_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vcam_af_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON31),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAM_AF_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VCAM_AF_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcam_af_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON31),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAM_AF_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAM_AF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcam_af_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON32),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAM_AF_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAM_AF_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcam_af_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON32),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAM_AF_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VCAM_AF_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcam_af_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON32),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAM_AF_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VCAM_AF_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcam_af_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON32),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAM_AF_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VCAM_AF_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcam_af_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON32),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAM_AF_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VCAM_AF_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcam_af_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON32),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAM_AF_CAL_MASK),
				    (unsigned int) (PMIC_RG_VCAM_AF_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_re_digldorsv3(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON33),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RE_DIGLDORSV3_MASK),
				    (unsigned int) (PMIC_RE_DIGLDORSV3_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsim1_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON34),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSIM1_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VSIM1_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsim1_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON34),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSIM1_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VSIM1_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsim1_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON34),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSIM1_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VSIM1_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsim1_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON34),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSIM1_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VSIM1_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsim1_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON34),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSIM1_CAL_MASK),
				    (unsigned int) (PMIC_RG_VSIM1_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsim2_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON35),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSIM2_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VSIM2_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsim2_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON35),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSIM2_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VSIM2_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsim2_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON35),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSIM2_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VSIM2_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsim2_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON35),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSIM2_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VSIM2_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsim2_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON35),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSIM2_CAL_MASK),
				    (unsigned int) (PMIC_RG_VSIM2_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsysldo_reserve(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON36),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSYSLDO_RESERVE_MASK),
				    (unsigned int) (PMIC_RG_VSYSLDO_RESERVE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vibr_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON39),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIBR_LP_SEL_MASK),
				    (unsigned int) (PMIC_VIBR_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vibr_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON39),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIBR_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VIBR_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vibr_ther_shen_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON39),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIBR_THER_SHEN_EN_MASK),
				    (unsigned int) (PMIC_VIBR_THER_SHEN_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vibr_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON39),
				  (&val),
				  (unsigned int) (PMIC_QI_VIBR_MODE_MASK),
				  (unsigned int) (PMIC_QI_VIBR_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vibr_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON39),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VIBR_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON39),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_EN_MASK),
				    (unsigned int) (PMIC_RG_VIBR_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON40),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VIBR_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON40),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VIBR_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON40),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VIBR_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON40),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VIBR_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_stb_sel_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON40),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_STB_SEL_CAL_MASK),
				    (unsigned int) (PMIC_RG_VIBR_STB_SEL_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON40),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_CAL_MASK),
				    (unsigned int) (PMIC_RG_VIBR_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_digldo_rsv1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON41),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DIGLDO_RSV1_MASK),
				    (unsigned int) (PMIC_DIGLDO_RSV1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_digldo_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON41),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DIGLDO_RSV0_MASK),
				    (unsigned int) (PMIC_DIGLDO_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ldo_ft(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON41),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LDO_FT_MASK),
				    (unsigned int) (PMIC_RG_LDO_FT_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcam_io_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON42),
				  (&val),
				  (unsigned int) (PMIC_QI_VCAM_IO_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VCAM_IO_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vcamd_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON42),
				  (&val),
				  (unsigned int) (PMIC_QI_VCAMD_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VCAMD_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vcn_1v8_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON42),
				  (&val),
				  (unsigned int) (PMIC_QI_VCN_1V8_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VCN_1V8_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vio18_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON42),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO18_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VIO18_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vrf18_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON42),
				  (&val),
				  (unsigned int) (PMIC_QI_VRF18_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VRF18_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vm_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON42),
				  (&val),
				  (unsigned int) (PMIC_QI_VM_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VM_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vcam_af_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON42),
				  (&val),
				  (unsigned int) (PMIC_QI_VCAM_AF_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VCAM_AF_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vcam_af_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON43),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAM_AF_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAM_AF_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcamd_io_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON43),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMD_IO_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMD_IO_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcn_1v8_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON43),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCN_1V8_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VCN_1V8_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcamd_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON43),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMD_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMD_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON43),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VIO18_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vm_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON43),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VM_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VM_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vrf18_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON43),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VRF18_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VRF18_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vibr_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON44),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIBR_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VIBR_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsim2_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON44),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSIM2_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VSIM2_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsim1_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON44),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSIM1_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VSIM1_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp3_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON44),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP3_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VGP3_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp2_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON44),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP2_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VGP2_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp1_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON44),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP1_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VGP1_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vrf18_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON45),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VRF18_LP_SEL_MASK),
				    (unsigned int) (PMIC_VRF18_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vrf18_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON45),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VRF18_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VRF18_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vrf18_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON45),
				  (&val),
				  (unsigned int) (PMIC_QI_VRF18_MODE_MASK),
				  (unsigned int) (PMIC_QI_VRF18_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vrf18_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON45),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VRF18_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VRF18_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vrf18_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON45),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VRF18_EN_MASK),
				    (unsigned int) (PMIC_RG_VRF18_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vrf18_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON46),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VRF18_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VRF18_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vrf18_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON46),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VRF18_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VRF18_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vrf18_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON46),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VRF18_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VRF18_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vrf18_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON46),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VRF18_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VRF18_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vrf18_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON46),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VRF18_CAL_MASK),
				    (unsigned int) (PMIC_RG_VRF18_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vm_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON47),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VM_LP_SEL_MASK),
				    (unsigned int) (PMIC_VM_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vm_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON47),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VM_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VM_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vm_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON47),
				  (&val),
				  (unsigned int) (PMIC_QI_VM_MODE_MASK),
				  (unsigned int) (PMIC_QI_VM_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vm_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON47),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VM_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VM_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON47),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VM_EN_MASK),
				    (unsigned int) (PMIC_RG_VM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vm_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON47),
				  (&val),
				  (unsigned int) (PMIC_QI_VM_EN_MASK),
				  (unsigned int) (PMIC_QI_VM_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vm_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON48),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VM_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VM_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vm_plcur_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON48),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VM_PLCUR_EN_MASK),
				    (unsigned int) (PMIC_RG_VM_PLCUR_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vm_plcur_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON48),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VM_PLCUR_CAL_MASK),
				    (unsigned int) (PMIC_RG_VM_PLCUR_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vm_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON48),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VM_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VM_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vm_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON48),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VM_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VM_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vm_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON48),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VM_CAL_MASK),
				    (unsigned int) (PMIC_RG_VM_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON49),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_LP_SEL_MASK),
				    (unsigned int) (PMIC_VIO18_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON49),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VIO18_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vio18_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON49),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO18_MODE_MASK),
				  (unsigned int) (PMIC_QI_VIO18_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vio18_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON49),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VIO18_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON49),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_EN_MASK),
				    (unsigned int) (PMIC_RG_VIO18_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vio18_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON49),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO18_EN_MASK),
				  (unsigned int) (PMIC_QI_VIO18_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vio18_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON50),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VIO18_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON50),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VIO18_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON50),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VIO18_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON50),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VIO18_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON50),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_CAL_MASK),
				    (unsigned int) (PMIC_RG_VIO18_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcamd_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON51),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMD_LP_SEL_MASK),
				    (unsigned int) (PMIC_VCAMD_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcamd_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON51),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMD_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VCAMD_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcamd_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON51),
				  (&val),
				  (unsigned int) (PMIC_QI_VCAMD_MODE_MASK),
				  (unsigned int) (PMIC_QI_VCAMD_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vcamd_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON51),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMD_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VCAMD_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcamd_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON51),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMD_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMD_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcamd_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON51),
				  (&val),
				  (unsigned int) (PMIC_QI_VCAMD_EN_MASK),
				  (unsigned int) (PMIC_QI_VCAMD_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vcamd_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON52),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMD_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMD_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcamd_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON52),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMD_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VCAMD_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcamd_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON52),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMD_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VCAMD_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcamd_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON52),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMD_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VCAMD_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcamd_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON52),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMD_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VCAMD_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcamd_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON52),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMD_CAL_MASK),
				    (unsigned int) (PMIC_RG_VCAMD_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcam_io_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON53),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAM_IO_LP_SEL_MASK),
				    (unsigned int) (PMIC_VCAM_IO_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcam_io_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON53),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAM_IO_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VCAM_IO_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcam_io_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON53),
				  (&val),
				  (unsigned int) (PMIC_QI_VCAM_IO_MODE_MASK),
				  (unsigned int) (PMIC_QI_VCAM_IO_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vcam_io_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON53),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAM_IO_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VCAM_IO_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcam_io_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON53),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAM_IO_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAM_IO_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcam_io_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON53),
				  (&val),
				  (unsigned int) (PMIC_QI_VCAM_IO_EN_MASK),
				  (unsigned int) (PMIC_QI_VCAM_IO_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vcam_io_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON54),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAM_IO_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAM_IO_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcam_io_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON54),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAM_IO_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VCAM_IO_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcam_io_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON54),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAM_IO_OCFB_MASK),
				    (unsigned int) (PMIC_RG_VCAM_IO_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcam_io_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON54),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAM_IO_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VCAM_IO_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcam_io_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON54),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAM_IO_CAL_MASK),
				    (unsigned int) (PMIC_RG_VCAM_IO_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_addr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_ADDR_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_ADDR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_prog(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_PROG_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_PROG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_EN_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_pkey(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_PKEY_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_PKEY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_rd_trig(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_RD_TRIG_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_RD_TRIG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_prog_src(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_PROG_SRC_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_PROG_SRC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_skip_efuse_out(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SKIP_EFUSE_OUT_MASK),
				    (unsigned int) (PMIC_RG_SKIP_EFUSE_OUT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rd_rdy_bypass(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RD_RDY_BYPASS_MASK),
				    (unsigned int) (PMIC_RG_RD_RDY_BYPASS_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rg_efuse_rd_ack(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_CON6),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_RD_ACK_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_RD_ACK_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_efuse_busy(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_CON6),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_BUSY_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_BUSY_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_efuse_val_0_15(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_0_15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_0_15_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_0_15_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_16_31(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_16_31),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_16_31_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_16_31_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_32_47(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_32_47),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_32_47_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_32_47_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_48_63(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_48_63),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_48_63_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_48_63_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_64_79(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_64_79),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_64_79_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_64_79_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_80_95(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_80_95),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_80_95_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_80_95_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_96_111(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_96_111),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_96_111_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_96_111_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_112_127(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_112_127),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_112_127_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_112_127_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_128_143(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_128_143),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_128_143_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_128_143_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_144_159(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_144_159),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_144_159_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_144_159_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_160_175(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_160_175),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_160_175_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_160_175_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_176_191(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_176_191),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_176_191_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_176_191_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rg_efuse_dout_0_15(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_0_15),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_0_15_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_0_15_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_efuse_dout_16_31(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_16_31),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_16_31_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_16_31_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_efuse_dout_32_47(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_32_47),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_32_47_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_32_47_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_efuse_dout_48_63(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_48_63),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_48_63_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_48_63_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_efuse_dout_64_79(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_64_79),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_64_79_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_64_79_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_efuse_dout_80_95(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_80_95),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_80_95_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_80_95_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_efuse_dout_96_111(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_96_111),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_96_111_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_96_111_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_efuse_dout_112_127(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_112_127),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_112_127_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_112_127_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_efuse_dout_128_143(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_128_143),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_128_143_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_128_143_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_efuse_dout_144_159(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_144_159),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_144_159_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_144_159_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_efuse_dout_160_175(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_160_175),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_160_175_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_160_175_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_efuse_dout_176_191(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_176_191),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_176_191_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_176_191_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_otp_pa(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OTP_PA_MASK),
				    (unsigned int) (PMIC_RG_OTP_PA_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_otp_pdin(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OTP_PDIN_MASK),
				    (unsigned int) (PMIC_RG_OTP_PDIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_otp_ptm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OTP_PTM_MASK),
				    (unsigned int) (PMIC_RG_OTP_PTM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_eosc32_opt(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_EOSC32_OPT_MASK),
				    (unsigned int) (PMIC_MIX_EOSC32_OPT_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_mix_xosc32_stp_cpdtb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RTC_MIX_CON0),
				  (&val),
				  (unsigned int) (PMIC_MIX_XOSC32_STP_CPDTB_MASK),
				  (unsigned int) (PMIC_MIX_XOSC32_STP_CPDTB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_mix_xosc32_stp_pwdb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_XOSC32_STP_PWDB_MASK),
				    (unsigned int) (PMIC_MIX_XOSC32_STP_PWDB_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_mix_xosc32_stp_lpdtb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RTC_MIX_CON0),
				  (&val),
				  (unsigned int) (PMIC_MIX_XOSC32_STP_LPDTB_MASK),
				  (unsigned int) (PMIC_MIX_XOSC32_STP_LPDTB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_mix_xosc32_stp_lpden(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_XOSC32_STP_LPDEN_MASK),
				    (unsigned int) (PMIC_MIX_XOSC32_STP_LPDEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_xosc32_stp_lpdrst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_XOSC32_STP_LPDRST_MASK),
				    (unsigned int) (PMIC_MIX_XOSC32_STP_LPDRST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_xosc32_stp_cali(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_XOSC32_STP_CALI_MASK),
				    (unsigned int) (PMIC_MIX_XOSC32_STP_CALI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_stmp_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STMP_MODE_MASK),
				    (unsigned int) (PMIC_STMP_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_eosc32_stp_chop_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_EOSC32_STP_CHOP_EN_MASK),
				    (unsigned int) (PMIC_MIX_EOSC32_STP_CHOP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_dcxo_stp_lvsh_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_DCXO_STP_LVSH_EN_MASK),
				    (unsigned int) (PMIC_MIX_DCXO_STP_LVSH_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_pmu_stp_ddlo_vrtc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_PMU_STP_DDLO_VRTC_MASK),
				    (unsigned int) (PMIC_MIX_PMU_STP_DDLO_VRTC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_pmu_stp_ddlo_vrtc_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_PMU_STP_DDLO_VRTC_EN_MASK),
				    (unsigned int) (PMIC_MIX_PMU_STP_DDLO_VRTC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_rtc_stp_xosc32_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_RTC_STP_XOSC32_ENB_MASK),
				    (unsigned int) (PMIC_MIX_RTC_STP_XOSC32_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_dcxo_stp_test_deglitch_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_DCXO_STP_TEST_DEGLITCH_MODE_MASK),
				    (unsigned int) (PMIC_MIX_DCXO_STP_TEST_DEGLITCH_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_eosc32_stp_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_EOSC32_STP_RSV_MASK),
				    (unsigned int) (PMIC_MIX_EOSC32_STP_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_eosc32_vct_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_EOSC32_VCT_EN_MASK),
				    (unsigned int) (PMIC_MIX_EOSC32_VCT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_stp_bbwakeup(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_STP_BBWAKEUP_MASK),
				    (unsigned int) (PMIC_MIX_STP_BBWAKEUP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_mix_stp_rtc_ddlo(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RTC_MIX_CON1),
				  (&val),
				  (unsigned int) (PMIC_MIX_STP_RTC_DDLO_MASK),
				  (unsigned int) (PMIC_MIX_STP_RTC_DDLO_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_mix_rtc_xosc32_enb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RTC_MIX_CON1),
				  (&val),
				  (unsigned int) (PMIC_MIX_RTC_XOSC32_ENB_MASK),
				  (unsigned int) (PMIC_MIX_RTC_XOSC32_ENB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_mix_efuse_xosc32_enb_opt(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_EFUSE_XOSC32_ENB_OPT_MASK),
				    (unsigned int) (PMIC_MIX_EFUSE_XOSC32_ENB_OPT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audull_vcfg(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULL_VCFG_MASK),
				    (unsigned int) (PMIC_RG_AUDULL_VCFG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audull_vupg(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULL_VUPG_MASK),
				    (unsigned int) (PMIC_RG_AUDULL_VUPG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audull_vpwdb_pga(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULL_VPWDB_PGA_MASK),
				    (unsigned int) (PMIC_RG_AUDULL_VPWDB_PGA_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audull_vpwdb_adc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULL_VPWDB_ADC_MASK),
				    (unsigned int) (PMIC_RG_AUDULL_VPWDB_ADC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audull_vadc_denb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULL_VADC_DENB_MASK),
				    (unsigned int) (PMIC_RG_AUDULL_VADC_DENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audull_vadc_dvref_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULL_VADC_DVREF_CAL_MASK),
				    (unsigned int) (PMIC_RG_AUDULL_VADC_DVREF_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audull_vref24_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULL_VREF24_EN_MASK),
				    (unsigned int) (PMIC_RG_AUDULL_VREF24_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audull_vcm14_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULL_VCM14_EN_MASK),
				    (unsigned int) (PMIC_RG_AUDULL_VCM14_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audull_vcmsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULL_VCMSEL_MASK),
				    (unsigned int) (PMIC_RG_AUDULL_VCMSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audull_chs_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULL_CHS_EN_MASK),
				    (unsigned int) (PMIC_RG_AUDULL_CHS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audull_vcali(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULL_VCALI_MASK),
				    (unsigned int) (PMIC_RG_AUDULL_VCALI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audulr_vcfg(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULR_VCFG_MASK),
				    (unsigned int) (PMIC_RG_AUDULR_VCFG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audulr_vupg(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULR_VUPG_MASK),
				    (unsigned int) (PMIC_RG_AUDULR_VUPG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audulr_vpwdb_pga(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULR_VPWDB_PGA_MASK),
				    (unsigned int) (PMIC_RG_AUDULR_VPWDB_PGA_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audulr_vpwdb_adc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULR_VPWDB_ADC_MASK),
				    (unsigned int) (PMIC_RG_AUDULR_VPWDB_ADC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audulr_vadc_denb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULR_VADC_DENB_MASK),
				    (unsigned int) (PMIC_RG_AUDULR_VADC_DENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audulr_vadc_dvref_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULR_VADC_DVREF_CAL_MASK),
				    (unsigned int) (PMIC_RG_AUDULR_VADC_DVREF_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audulr_vref24_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULR_VREF24_EN_MASK),
				    (unsigned int) (PMIC_RG_AUDULR_VREF24_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audulr_vcm14_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULR_VCM14_EN_MASK),
				    (unsigned int) (PMIC_RG_AUDULR_VCM14_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audulr_vcmsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULR_VCMSEL_MASK),
				    (unsigned int) (PMIC_RG_AUDULR_VCMSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audulr_chs_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULR_CHS_EN_MASK),
				    (unsigned int) (PMIC_RG_AUDULR_CHS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audulr_vcali(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDULR_VCALI_MASK),
				    (unsigned int) (PMIC_RG_AUDULR_VCALI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aud_igbias_cali(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUD_IGBIAS_CALI_MASK),
				    (unsigned int) (PMIC_RG_AUD_IGBIAS_CALI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aud_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUD_RSV_MASK),
				    (unsigned int) (PMIC_RG_AUD_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_amuter(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AMUTER_MASK),
				    (unsigned int) (PMIC_RG_AMUTER_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_amutel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AMUTEL_MASK),
				    (unsigned int) (PMIC_RG_AMUTEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adacl_pwdb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADACL_PWDB_MASK),
				    (unsigned int) (PMIC_RG_ADACL_PWDB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adacr_pwdb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADACR_PWDB_MASK),
				    (unsigned int) (PMIC_RG_ADACR_PWDB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_abias_pwdb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ABIAS_PWDB_MASK),
				    (unsigned int) (PMIC_RG_ABIAS_PWDB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aoutl_pwdb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AOUTL_PWDB_MASK),
				    (unsigned int) (PMIC_RG_AOUTL_PWDB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aoutr_pwdb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AOUTR_PWDB_MASK),
				    (unsigned int) (PMIC_RG_AOUTR_PWDB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_acali(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ACALI_MASK),
				    (unsigned int) (PMIC_RG_ACALI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_apgr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_APGR_MASK),
				    (unsigned int) (PMIC_RG_APGR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_apgl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_APGL_MASK),
				    (unsigned int) (PMIC_RG_APGL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_abuf_bias(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ABUF_BIAS_MASK),
				    (unsigned int) (PMIC_RG_ABUF_BIAS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_abuf_inshort(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ABUF_INSHORT_MASK),
				    (unsigned int) (PMIC_RG_ABUF_INSHORT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ahfmode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AHFMODE_MASK),
				    (unsigned int) (PMIC_RG_AHFMODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adacck_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADACCK_EN_MASK),
				    (unsigned int) (PMIC_RG_ADACCK_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dacref(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DACREF_MASK),
				    (unsigned int) (PMIC_RG_DACREF_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adepopx_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADEPOPX_EN_MASK),
				    (unsigned int) (PMIC_RG_ADEPOPX_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adepopx(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADEPOPX_MASK),
				    (unsigned int) (PMIC_RG_ADEPOPX_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_depop_vcm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DEPOP_VCM_EN_MASK),
				    (unsigned int) (PMIC_RG_DEPOP_VCM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_depop_vcmsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DEPOP_VCMSEL_MASK),
				    (unsigned int) (PMIC_RG_DEPOP_VCMSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_depop_cursel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DEPOP_CURSEL_MASK),
				    (unsigned int) (PMIC_RG_DEPOP_CURSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chargeoption_depop(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHARGEOPTION_DEPOP_MASK),
				    (unsigned int) (PMIC_RG_CHARGEOPTION_DEPOP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_avcmgen_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AVCMGEN_EN_MASK),
				    (unsigned int) (PMIC_RG_AVCMGEN_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auddl_vref24_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDDL_VREF24_EN_MASK),
				    (unsigned int) (PMIC_RG_AUDDL_VREF24_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_abirsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ABIRSV_MASK),
				    (unsigned int) (PMIC_RG_ABIRSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbuf_float(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBUF_FLOAT_MASK),
				    (unsigned int) (PMIC_RG_VBUF_FLOAT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdpg(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDPG_MASK),
				    (unsigned int) (PMIC_RG_VDPG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbuf_pwdb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBUF_PWDB_MASK),
				    (unsigned int) (PMIC_RG_VBUF_PWDB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbuf_bias(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBUF_BIAS_MASK),
				    (unsigned int) (PMIC_RG_VBUF_BIAS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdepop(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDEPOP_MASK),
				    (unsigned int) (PMIC_RG_VDEPOP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_v2spk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_V2SPK_MASK),
				    (unsigned int) (PMIC_RG_V2SPK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hsoutstbenh(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HSOUTSTBENH_MASK),
				    (unsigned int) (PMIC_RG_HSOUTSTBENH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_audtop_con8_rsv_0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_AUDTOP_CON8_RSV_0_MASK),
				    (unsigned int) (PMIC_AUDTOP_CON8_RSV_0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_clksq_monen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CLKSQ_MONEN_MASK),
				    (unsigned int) (PMIC_RG_CLKSQ_MONEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auddigmicen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDDIGMICEN_MASK),
				    (unsigned int) (PMIC_RG_AUDDIGMICEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpwdbmicbias(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPWDBMICBIAS_MASK),
				    (unsigned int) (PMIC_RG_AUDPWDBMICBIAS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auddigmicpduty(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDDIGMICPDUTY_MASK),
				    (unsigned int) (PMIC_RG_AUDDIGMICPDUTY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auddigmicnduty(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDDIGMICNDUTY_MASK),
				    (unsigned int) (PMIC_RG_AUDDIGMICNDUTY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auddigmicbias(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDDIGMICBIAS_MASK),
				    (unsigned int) (PMIC_RG_AUDDIGMICBIAS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audmicbiasvref(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDMICBIASVREF_MASK),
				    (unsigned int) (PMIC_RG_AUDMICBIASVREF_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audsparevmic(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDSPAREVMIC_MASK),
				    (unsigned int) (PMIC_RG_AUDSPAREVMIC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbirx_zcd_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBIRX_ZCD_EN_MASK),
				    (unsigned int) (PMIC_RG_VBIRX_ZCD_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbirx_zcd_cali(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBIRX_ZCD_CALI_MASK),
				    (unsigned int) (PMIC_RG_VBIRX_ZCD_CALI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbirx_zcd_hys_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDTOP_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBIRX_ZCD_HYS_ENB_MASK),
				    (unsigned int) (PMIC_RG_VBIRX_ZCD_HYS_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rg_vbirx_zcd_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUDTOP_CON9),
				  (&val),
				  (unsigned int) (PMIC_RG_VBIRX_ZCD_STATUS_MASK),
				  (unsigned int) (PMIC_RG_VBIRX_ZCD_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_batsns(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC0),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_BATSNS_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_BATSNS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_batsns(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC0),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_BATSNS_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_BATSNS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_isense(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC1),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_ISENSE_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_ISENSE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_isense(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC1),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_ISENSE_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_ISENSE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_vcdt(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC2),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_VCDT_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_VCDT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_vcdt(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC2),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_VCDT_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_VCDT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_baton1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC3),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_BATON1_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_BATON1_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_baton1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC3),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_BATON1_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_BATON1_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_thr_sense1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC4),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_THR_SENSE1_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_THR_SENSE1_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_thr_sense1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC4),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_THR_SENSE1_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_THR_SENSE1_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_thr_sense2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC5),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_THR_SENSE2_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_THR_SENSE2_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_thr_sense2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC5),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_THR_SENSE2_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_THR_SENSE2_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_baton2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC6),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_BATON2_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_BATON2_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_baton2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC6),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_BATON2_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_BATON2_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_ch5(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC7),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_CH5_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_CH5_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_ch5(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC7),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_CH5_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_CH5_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_wakeup_pchr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC8),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_WAKEUP_PCHR_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_WAKEUP_PCHR_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_wakeup_pchr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC8),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_WAKEUP_PCHR_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_WAKEUP_PCHR_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_wakeup_swchr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC9),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_WAKEUP_SWCHR_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_WAKEUP_SWCHR_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_wakeup_swchr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC9),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_WAKEUP_SWCHR_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_WAKEUP_SWCHR_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_lbat(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC10),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_LBAT_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_LBAT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_lbat(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC10),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_LBAT_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_LBAT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_ch6(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC11),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_CH6_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_CH6_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_ch6(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC11),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_CH6_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_CH6_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_gps(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC12),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_GPS_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_GPS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_gps(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC13),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_GPS_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_GPS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_gps_lsb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC14),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_GPS_LSB_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_GPS_LSB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_md(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC15),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_MD_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_MD_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_md_lsb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC16),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_MD_LSB_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_MD_LSB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_md(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC16),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_MD_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_MD_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_int(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC17),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_INT_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_INT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_int(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC17),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_INT_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_INT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_rsv1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC18),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_RSV1_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_RSV1_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_rsv2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC19),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_RSV2_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_RSV2_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_rsv3(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC20),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_RSV3_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_RSV3_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_sw_gain_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_RSV1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SW_GAIN_TRIM_MASK),
				    (unsigned int) (PMIC_RG_SW_GAIN_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_sw_offset_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_RSV2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SW_OFFSET_TRIM_MASK),
				    (unsigned int) (PMIC_RG_SW_OFFSET_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_pwdb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_PWDB_MASK),
				    (unsigned int) (PMIC_RG_ADC_PWDB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_pwdb_swctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_PWDB_SWCTRL_MASK),
				    (unsigned int) (PMIC_RG_ADC_PWDB_SWCTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_cali_rate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_CALI_RATE_MASK),
				    (unsigned int) (PMIC_RG_ADC_CALI_RATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_cali_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_CALI_EN_MASK),
				    (unsigned int) (PMIC_RG_ADC_CALI_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_cali_force(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_CALI_FORCE_MASK),
				    (unsigned int) (PMIC_RG_ADC_CALI_FORCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_autorst_range(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_AUTORST_RANGE_MASK),
				    (unsigned int) (PMIC_RG_ADC_AUTORST_RANGE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_autorst_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_AUTORST_EN_MASK),
				    (unsigned int) (PMIC_RG_ADC_AUTORST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_latch_edge(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_LATCH_EDGE_MASK),
				    (unsigned int) (PMIC_RG_ADC_LATCH_EDGE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_filter_order(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_FILTER_ORDER_MASK),
				    (unsigned int) (PMIC_RG_ADC_FILTER_ORDER_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_swctrl_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_SWCTRL_EN_MASK),
				    (unsigned int) (PMIC_RG_ADC_SWCTRL_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adcin_vsen_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADCIN_VSEN_EN_MASK),
				    (unsigned int) (PMIC_RG_ADCIN_VSEN_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adcin_vsen_mux_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADCIN_VSEN_MUX_EN_MASK),
				    (unsigned int) (PMIC_RG_ADCIN_VSEN_MUX_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adcin_vbat_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADCIN_VBAT_EN_MASK),
				    (unsigned int) (PMIC_RG_ADCIN_VBAT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adcin_chr_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADCIN_CHR_EN_MASK),
				    (unsigned int) (PMIC_RG_ADCIN_CHR_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_chsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_CHSEL_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_CHSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_debt_max(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_DEBT_MAX_MASK),
				    (unsigned int) (PMIC_RG_LBAT_DEBT_MAX_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_debt_min(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_DEBT_MIN_MASK),
				    (unsigned int) (PMIC_RG_LBAT_DEBT_MIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_det_prd_15_0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_DET_PRD_15_0_MASK),
				    (unsigned int) (PMIC_RG_LBAT_DET_PRD_15_0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_det_prd_19_16(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_DET_PRD_19_16_MASK),
				    (unsigned int) (PMIC_RG_LBAT_DET_PRD_19_16_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_volt_max(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_VOLT_MAX_MASK),
				    (unsigned int) (PMIC_RG_LBAT_VOLT_MAX_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_irq_en_max(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_IRQ_EN_MAX_MASK),
				    (unsigned int) (PMIC_RG_LBAT_IRQ_EN_MAX_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_en_max(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_EN_MAX_MASK),
				    (unsigned int) (PMIC_RG_LBAT_EN_MAX_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rg_lbat_max_irq_b(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_CON5),
				  (&val),
				  (unsigned int) (PMIC_RG_LBAT_MAX_IRQ_B_MASK),
				  (unsigned int) (PMIC_RG_LBAT_MAX_IRQ_B_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_lbat_volt_min(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_VOLT_MIN_MASK),
				    (unsigned int) (PMIC_RG_LBAT_VOLT_MIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_irq_en_min(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_IRQ_EN_MIN_MASK),
				    (unsigned int) (PMIC_RG_LBAT_IRQ_EN_MIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_en_min(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_EN_MIN_MASK),
				    (unsigned int) (PMIC_RG_LBAT_EN_MIN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rg_lbat_min_irq_b(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_CON6),
				  (&val),
				  (unsigned int) (PMIC_RG_LBAT_MIN_IRQ_B_MASK),
				  (unsigned int) (PMIC_RG_LBAT_MIN_IRQ_B_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_lbat_debounce_count_max(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_CON7),
				  (&val),
				  (unsigned int) (PMIC_RG_LBAT_DEBOUNCE_COUNT_MAX_MASK),
				  (unsigned int) (PMIC_RG_LBAT_DEBOUNCE_COUNT_MAX_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_lbat_debounce_count_min(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_CON8),
				  (&val),
				  (unsigned int) (PMIC_RG_LBAT_DEBOUNCE_COUNT_MIN_MASK),
				  (unsigned int) (PMIC_RG_LBAT_DEBOUNCE_COUNT_MIN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_data_reuse_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DATA_REUSE_SEL_MASK),
				    (unsigned int) (PMIC_RG_DATA_REUSE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_bist_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_BIST_ENB_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_BIST_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_osr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OSR_MASK),
				    (unsigned int) (PMIC_RG_OSR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_osr_gps(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OSR_GPS_MASK),
				    (unsigned int) (PMIC_RG_OSR_GPS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_trim_ch7_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_TRIM_CH7_SEL_MASK),
				    (unsigned int) (PMIC_RG_ADC_TRIM_CH7_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_trim_ch6_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_TRIM_CH6_SEL_MASK),
				    (unsigned int) (PMIC_RG_ADC_TRIM_CH6_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_trim_ch5_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_TRIM_CH5_SEL_MASK),
				    (unsigned int) (PMIC_RG_ADC_TRIM_CH5_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_trim_ch4_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_TRIM_CH4_SEL_MASK),
				    (unsigned int) (PMIC_RG_ADC_TRIM_CH4_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_trim_ch3_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_TRIM_CH3_SEL_MASK),
				    (unsigned int) (PMIC_RG_ADC_TRIM_CH3_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_trim_ch2_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_TRIM_CH2_SEL_MASK),
				    (unsigned int) (PMIC_RG_ADC_TRIM_CH2_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_trim_ch0_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_TRIM_CH0_SEL_MASK),
				    (unsigned int) (PMIC_RG_ADC_TRIM_CH0_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbuf_calen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBUF_CALEN_MASK),
				    (unsigned int) (PMIC_RG_VBUF_CALEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbuf_exten(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBUF_EXTEN_MASK),
				    (unsigned int) (PMIC_RG_VBUF_EXTEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbuf_byp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBUF_BYP_MASK),
				    (unsigned int) (PMIC_RG_VBUF_BYP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_VBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_source_lbat_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SOURCE_LBAT_SEL_MASK),
				    (unsigned int) (PMIC_RG_SOURCE_LBAT_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_efuse_gain_ch0_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_EFUSE_GAIN_CH0_TRIM_MASK),
				    (unsigned int) (PMIC_EFUSE_GAIN_CH0_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_efuse_offset_ch0_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_EFUSE_OFFSET_CH0_TRIM_MASK),
				    (unsigned int) (PMIC_EFUSE_OFFSET_CH0_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_efuse_gain_ch4_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_EFUSE_GAIN_CH4_TRIM_MASK),
				    (unsigned int) (PMIC_EFUSE_GAIN_CH4_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_efuse_offset_ch4_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_EFUSE_OFFSET_CH4_TRIM_MASK),
				    (unsigned int) (PMIC_EFUSE_OFFSET_CH4_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_efuse_gain_ch7_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_EFUSE_GAIN_CH7_TRIM_MASK),
				    (unsigned int) (PMIC_EFUSE_GAIN_CH7_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_efuse_offset_ch7_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_EFUSE_OFFSET_CH7_TRIM_MASK),
				    (unsigned int) (PMIC_EFUSE_OFFSET_CH7_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_ibias(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_IBIAS_MASK),
				    (unsigned int) (PMIC_RG_ADC_IBIAS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_RST_MASK),
				    (unsigned int) (PMIC_RG_ADC_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_lp_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_LP_EN_MASK),
				    (unsigned int) (PMIC_RG_ADC_LP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_input_short(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_INPUT_SHORT_MASK),
				    (unsigned int) (PMIC_RG_ADC_INPUT_SHORT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_chopper_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_CHOPPER_EN_MASK),
				    (unsigned int) (PMIC_RG_ADC_CHOPPER_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpwdb_adc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPWDB_ADC_MASK),
				    (unsigned int) (PMIC_RG_VPWDB_ADC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vref18_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VREF18_EN_MASK),
				    (unsigned int) (PMIC_RG_VREF18_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_chs_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_CHS_SEL_MASK),
				    (unsigned int) (PMIC_RG_ADC_CHS_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_dvref_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_DVREF_CAL_MASK),
				    (unsigned int) (PMIC_RG_ADC_DVREF_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_denb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_DENB_MASK),
				    (unsigned int) (PMIC_RG_ADC_DENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_sleep_mode_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_SLEEP_MODE_EN_MASK),
				    (unsigned int) (PMIC_RG_ADC_SLEEP_MODE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_gps_status(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_GPS_STATUS_MASK),
				    (unsigned int) (PMIC_RG_ADC_GPS_STATUS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_rsv_bit(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_RSV_BIT_MASK),
				    (unsigned int) (PMIC_RG_ADC_RSV_BIT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_test_mode_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_TEST_MODE_EN_MASK),
				    (unsigned int) (PMIC_RG_ADC_TEST_MODE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_test_out_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_TEST_OUT_SEL_MASK),
				    (unsigned int) (PMIC_RG_ADC_TEST_OUT_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_deci_bypass_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DECI_BYPASS_EN_MASK),
				    (unsigned int) (PMIC_RG_DECI_BYPASS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_clk_aon(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_CLK_AON_MASK),
				    (unsigned int) (PMIC_RG_ADC_CLK_AON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_deci_force(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_DECI_FORCE_MASK),
				    (unsigned int) (PMIC_RG_ADC_DECI_FORCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_deci_gdly(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_DECI_GDLY_MASK),
				    (unsigned int) (PMIC_RG_ADC_DECI_GDLY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_md_rqst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_MD_RQST_MASK),
				    (unsigned int) (PMIC_RG_MD_RQST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_gps_rqst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_GPS_RQST_MASK),
				    (unsigned int) (PMIC_RG_GPS_RQST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ap_rqst_list(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON22),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AP_RQST_LIST_MASK),
				    (unsigned int) (PMIC_RG_AP_RQST_LIST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ap_rqst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON22),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AP_RQST_MASK),
				    (unsigned int) (PMIC_RG_AP_RQST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ap_rqst_list_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AP_RQST_LIST_RSV_MASK),
				    (unsigned int) (PMIC_RG_AP_RQST_LIST_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_out_trim_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_OUT_TRIM_ENB_MASK),
				    (unsigned int) (PMIC_RG_ADC_OUT_TRIM_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_trim_comp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_TRIM_COMP_MASK),
				    (unsigned int) (PMIC_RG_ADC_TRIM_COMP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_2s_comp_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_2S_COMP_ENB_MASK),
				    (unsigned int) (PMIC_RG_ADC_2S_COMP_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_cic_out_raw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CIC_OUT_RAW_MASK),
				    (unsigned int) (PMIC_RG_CIC_OUT_RAW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_data_skip_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DATA_SKIP_ENB_MASK),
				    (unsigned int) (PMIC_RG_DATA_SKIP_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_data_skip_num(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DATA_SKIP_NUM_MASK),
				    (unsigned int) (PMIC_RG_DATA_SKIP_NUM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_rev(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON25),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_REV_MASK),
				    (unsigned int) (PMIC_RG_ADC_REV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_deci_gdly_sel_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON26),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DECI_GDLY_SEL_MODE_MASK),
				    (unsigned int) (PMIC_RG_DECI_GDLY_SEL_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_deci_gdly_vref18_selb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON26),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DECI_GDLY_VREF18_SELB_MASK),
				    (unsigned int) (PMIC_RG_DECI_GDLY_VREF18_SELB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_rsv1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON26),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_RSV1_MASK),
				    (unsigned int) (PMIC_RG_ADC_RSV1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vref18_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON26),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VREF18_ENB_MASK),
				    (unsigned int) (PMIC_RG_VREF18_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_md_status(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_MD_STATUS_MASK),
				    (unsigned int) (PMIC_RG_ADC_MD_STATUS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_rsv2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_RSV2_MASK),
				    (unsigned int) (PMIC_RG_ADC_RSV2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vref18_enb_md(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VREF18_ENB_MD_MASK),
				    (unsigned int) (PMIC_RG_VREF18_ENB_MD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audaccdetvthcal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDACCDETVTHCAL_MASK),
				    (unsigned int) (PMIC_RG_AUDACCDETVTHCAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audaccdetswctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDACCDETSWCTRL_MASK),
				    (unsigned int) (PMIC_RG_AUDACCDETSWCTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audaccdettvdet(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDACCDETTVDET_MASK),
				    (unsigned int) (PMIC_RG_AUDACCDETTVDET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audaccdetvin1pulllow(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDACCDETVIN1PULLLOW_MASK),
				    (unsigned int) (PMIC_RG_AUDACCDETVIN1PULLLOW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_audaccdetauxadcswctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_AUDACCDETAUXADCSWCTRL_MASK),
				    (unsigned int) (PMIC_AUDACCDETAUXADCSWCTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_audaccdetauxadcswctrl_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_AUDACCDETAUXADCSWCTRL_SEL_MASK),
				    (unsigned int) (PMIC_AUDACCDETAUXADCSWCTRL_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audaccdetrsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDACCDETRSV_MASK),
				    (unsigned int) (PMIC_RG_AUDACCDETRSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_EN_MASK),
				    (unsigned int) (PMIC_ACCDET_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_seq_init(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_SEQ_INIT_MASK),
				    (unsigned int) (PMIC_ACCDET_SEQ_INIT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_cmp_pwm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_CMP_PWM_EN_MASK),
				    (unsigned int) (PMIC_ACCDET_CMP_PWM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_vth_pwm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_VTH_PWM_EN_MASK),
				    (unsigned int) (PMIC_ACCDET_VTH_PWM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_mbias_pwm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_MBIAS_PWM_EN_MASK),
				    (unsigned int) (PMIC_ACCDET_MBIAS_PWM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_cmp_pwm_idle(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_CMP_PWM_IDLE_MASK),
				    (unsigned int) (PMIC_ACCDET_CMP_PWM_IDLE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_vth_pwm_idle(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_VTH_PWM_IDLE_MASK),
				    (unsigned int) (PMIC_ACCDET_VTH_PWM_IDLE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_mbias_pwm_idle(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_MBIAS_PWM_IDLE_MASK),
				    (unsigned int) (PMIC_ACCDET_MBIAS_PWM_IDLE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_pwm_width(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_PWM_WIDTH_MASK),
				    (unsigned int) (PMIC_ACCDET_PWM_WIDTH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_pwm_thresh(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_PWM_THRESH_MASK),
				    (unsigned int) (PMIC_ACCDET_PWM_THRESH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_rise_delay(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_RISE_DELAY_MASK),
				    (unsigned int) (PMIC_ACCDET_RISE_DELAY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_fall_delay(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_FALL_DELAY_MASK),
				    (unsigned int) (PMIC_ACCDET_FALL_DELAY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_debounce0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_DEBOUNCE0_MASK),
				    (unsigned int) (PMIC_ACCDET_DEBOUNCE0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_debounce1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_DEBOUNCE1_MASK),
				    (unsigned int) (PMIC_ACCDET_DEBOUNCE1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_debounce2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_DEBOUNCE2_MASK),
				    (unsigned int) (PMIC_ACCDET_DEBOUNCE2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_debounce3(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_DEBOUNCE3_MASK),
				    (unsigned int) (PMIC_ACCDET_DEBOUNCE3_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_ival_cur_in(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_IVAL_CUR_IN_MASK),
				    (unsigned int) (PMIC_ACCDET_IVAL_CUR_IN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_ival_sam_in(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_IVAL_SAM_IN_MASK),
				    (unsigned int) (PMIC_ACCDET_IVAL_SAM_IN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_ival_mem_in(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_IVAL_MEM_IN_MASK),
				    (unsigned int) (PMIC_ACCDET_IVAL_MEM_IN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_ival_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_IVAL_SEL_MASK),
				    (unsigned int) (PMIC_ACCDET_IVAL_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_accdet_irq(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON11),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_IRQ_MASK),
				  (unsigned int) (PMIC_ACCDET_IRQ_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_accdet_irq_clr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_IRQ_CLR_MASK),
				    (unsigned int) (PMIC_ACCDET_IRQ_CLR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_test_mode0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE0_MASK),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_test_mode1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE1_MASK),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_test_mode2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE2_MASK),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_test_mode3(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE3_MASK),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE3_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_test_mode4(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE4_MASK),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE4_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_test_mode5(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE5_MASK),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE5_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_pwm_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_PWM_SEL_MASK),
				    (unsigned int) (PMIC_ACCDET_PWM_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_in_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_IN_SW_MASK),
				    (unsigned int) (PMIC_ACCDET_IN_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_cmp_en_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_CMP_EN_SW_MASK),
				    (unsigned int) (PMIC_ACCDET_CMP_EN_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_vth_en_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_VTH_EN_SW_MASK),
				    (unsigned int) (PMIC_ACCDET_VTH_EN_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_mbias_en_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_MBIAS_EN_SW_MASK),
				    (unsigned int) (PMIC_ACCDET_MBIAS_EN_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_pwm_en_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_PWM_EN_SW_MASK),
				    (unsigned int) (PMIC_ACCDET_PWM_EN_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_accdet_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_IN_MASK),
				  (unsigned int) (PMIC_ACCDET_IN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_accdet_cur_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_CUR_IN_MASK),
				  (unsigned int) (PMIC_ACCDET_CUR_IN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_accdet_sam_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_SAM_IN_MASK),
				  (unsigned int) (PMIC_ACCDET_SAM_IN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_accdet_mem_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_MEM_IN_MASK),
				  (unsigned int) (PMIC_ACCDET_MEM_IN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_accdet_state(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_STATE_MASK),
				  (unsigned int) (PMIC_ACCDET_STATE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_accdet_mbias_clk(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_MBIAS_CLK_MASK),
				  (unsigned int) (PMIC_ACCDET_MBIAS_CLK_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_accdet_vth_clk(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_VTH_CLK_MASK),
				  (unsigned int) (PMIC_ACCDET_VTH_CLK_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_accdet_cmp_clk(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_CMP_CLK_MASK),
				  (unsigned int) (PMIC_ACCDET_CMP_CLK_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_da_audaccdetauxadcswctrl(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val),
				  (unsigned int) (PMIC_DA_AUDACCDETAUXADCSWCTRL_MASK),
				  (unsigned int) (PMIC_DA_AUDACCDETAUXADCSWCTRL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_accdet_cur_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON14),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_CUR_DEB_MASK),
				  (unsigned int) (PMIC_ACCDET_CUR_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_deci_gdly(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_CON19),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_DECI_GDLY_MASK),
				  (unsigned int) (PMIC_RG_ADC_DECI_GDLY_SHIFT)
	    );
	pmic_unlock();
	return val;
}

static inline void upmu_set_accdet_rsv_con0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_RSV_CON0_MASK),
				    (unsigned int) (PMIC_ACCDET_RSV_CON0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_rsv_con1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_RSV_CON1_MASK),
				    (unsigned int) (PMIC_ACCDET_RSV_CON1_SHIFT)
	    );
	pmic_unlock();
}

/* ADC Channel Number */
typedef enum {
	/* MT6397 */
	MT6323_AUX_BATON2 = 0x000,
	MT6397_AUX_CH6,
	MT6397_AUX_THR_SENSE2,
	MT6397_AUX_THR_SENSE1,
	MT6397_AUX_VCDT,
	MT6397_AUX_BATON1,
	MT6397_AUX_ISENSE,
	MT6397_AUX_BATSNS,
	MT6397_AUX_ACCDET,
	MT6397_AUX_AUDIO0,
	MT6397_AUX_AUDIO1,
	MT6397_AUX_AUDIO2,
	MT6397_AUX_AUDIO3,
	MT6397_AUX_AUDIO4,
	MT6397_AUX_AUDIO5,
	MT6397_AUX_AUDIO6,
	MT6397_AUX_AUDIO7,
} pmic_adc_ch_list_enum;

typedef enum MT65XX_POWER_TAG {

	/* MT6323 Digital LDO */
	MT6323_POWER_LDO_VIO28 = 0,
	MT6323_POWER_LDO_VUSB,
	MT6323_POWER_LDO_VMC,
	MT6323_POWER_LDO_VMCH,
	MT6323_POWER_LDO_VEMC_3V3,
	MT6323_POWER_LDO_VGP1,
	MT6323_POWER_LDO_VGP2,
	MT6323_POWER_LDO_VGP3,
	MT6323_POWER_LDO_VCN_1V8,
	MT6323_POWER_LDO_VSIM1,
	MT6323_POWER_LDO_VSIM2,
	MT6323_POWER_LDO_VRTC,
	MT6323_POWER_LDO_VCAM_AF,
	MT6323_POWER_LDO_VIBR,
	MT6323_POWER_LDO_VM,
	MT6323_POWER_LDO_VRF18,
	MT6323_POWER_LDO_VIO18,
	MT6323_POWER_LDO_VCAMD,
	MT6323_POWER_LDO_VCAM_IO,

	/* MT6323 Analog LDO */
	MT6323_POWER_LDO_VTCXO,
	MT6323_POWER_LDO_VA,
	MT6323_POWER_LDO_VCAMA,
	MT6323_POWER_LDO_VCN33_BT,
	MT6323_POWER_LDO_VCN33_WIFI,
	MT6323_POWER_LDO_VCN28,

	/* MT6320 Digital LDO */
	MT65XX_POWER_LDO_VIO28,
	MT65XX_POWER_LDO_VUSB,
	MT65XX_POWER_LDO_VMC,
	MT65XX_POWER_LDO_VMCH,
	MT65XX_POWER_LDO_VMC1,
	MT65XX_POWER_LDO_VMCH1,
	MT65XX_POWER_LDO_VEMC_3V3,
	MT65XX_POWER_LDO_VEMC_1V8,
	MT65XX_POWER_LDO_VCAMD,
	MT65XX_POWER_LDO_VCAMIO,
	MT65XX_POWER_LDO_VCAMAF,
	MT65XX_POWER_LDO_VGP1,
	MT65XX_POWER_LDO_VGP2,
	MT65XX_POWER_LDO_VGP3,
	MT65XX_POWER_LDO_VGP4,
	MT65XX_POWER_LDO_VGP5,
	MT65XX_POWER_LDO_VGP6,
	MT65XX_POWER_LDO_VSIM1,
	MT65XX_POWER_LDO_VSIM2,
	MT65XX_POWER_LDO_VIBR,
	MT65XX_POWER_LDO_VRTC,
	MT65XX_POWER_LDO_VAST,

	/* MT6320 Analog LDO */
	MT65XX_POWER_LDO_VRF28,
	MT65XX_POWER_LDO_VRF28_2,
	MT65XX_POWER_LDO_VTCXO,
	MT65XX_POWER_LDO_VTCXO_2,
	MT65XX_POWER_LDO_VA,
	MT65XX_POWER_LDO_VA28,
	MT65XX_POWER_LDO_VCAMA,

	MT6323_POWER_LDO_DEFAULT,
	MT65XX_POWER_LDO_DEFAULT,
	MT65XX_POWER_COUNT_END,
	MT65XX_POWER_NONE = -1
} MT65XX_POWER, MT_POWER;

#endif

