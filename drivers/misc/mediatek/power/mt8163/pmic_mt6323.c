#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>

#include <asm/uaccess.h>

#include <mt-plat/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <linux/syscore_ops.h>
#include <linux/time.h>
#include <linux/reboot.h>

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
#include <mt-plat/mt_boot.h>
#include <mt-plat/mt_reboot.h>
#include <mt-plat/mt_boot_common.h>
#include <mt-plat/mt_gpt.h>
#endif
#ifdef CONFIG_MTK_RTC
#include <mt-plat/mtk_rtc.h>
#endif
#include <linux/version.h>

#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/mfd/mt6323/core.h>
#include <linux/mfd/mt6323/registers.h>
#include <mt_pmic_wrap.h>
#include "pmic_mt6323.h"

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#endif

#define PMIC6323_E1_CID_CODE    0x1023
#define PMIC6323_E2_CID_CODE    0x2023

#define VOLTAGE_FULL_RANGE     1800
#define ADC_PRECISE         32768	/* 10 bits */
#define ADC_COUNT_TIMEOUT    100

static void deferred_restart(struct work_struct *dummy);
static void long_press_restart(struct work_struct *dummy);

static DEFINE_MUTEX(pmic_lock_mutex);
static DEFINE_MUTEX(pmic_adc_mutex);
static DEFINE_SPINLOCK(pmic_adc_lock);
static DECLARE_WORK(restart_work, deferred_restart);
static DECLARE_WORK(long_press_restart_work, long_press_restart);

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
static bool long_pwrkey_press;
static unsigned long timer_pre;
static unsigned long timer_pos;
#define LONG_PWRKEY_PRESS_TIME	500000000
#endif

#ifdef CONFIG_AMAZON_METRICS_LOG
static struct work_struct metrics_work;
static bool pwrkey_press;
static void pwrkey_log_to_metrics(struct work_struct *data);
#endif

static atomic_t pmic_suspend;

#define RELEASE_PWRKEY_TIME		(3)	/* 3sec */
#define PWRKEY_INITIAL_STATE (0)
#define LONG_PRESS_PWRKEY_SHUTDOWN_TIME		(6)	/* 6sec */

struct wake_lock pmicAuxadc_irq_lock;

__attribute__ ((weak))
void rtc_irq_handler(void)
{
	pr_notice("need rtc porting!\n");
}

__attribute__ ((weak))
void kpd_pwrkey_pmic_handler(unsigned long pressed)
{
	pr_notice("need kpd porting!\n");
}

__attribute__ ((weak))
void do_chrdet_int_task(void)
{
	pr_notice("need chrdet porting!\n");
}

__attribute__ ((weak))
void kpd_pmic_rstkey_handler(unsigned long pressed)
{
	pr_notice("need kpd porting!\n");
}

__attribute__ ((weak))
int accdet_irq_handler(void)
{
	pr_notice("need accdet porting!\n");
	return 0;
}

void pmic_lock(void)
{
	mutex_lock(&pmic_lock_mutex);
}

void pmic_unlock(void)
{
	mutex_unlock(&pmic_lock_mutex);
}

static DEFINE_MUTEX(pmic_access_mutex);

u32 pmic_read_interface(u32 RegNum, u32 *val, u32 MASK, u32 SHIFT)
{
	u32 return_value = 0;
	u32 pmic6323_reg = 0;
	u32 rdata;

	mutex_lock(&pmic_access_mutex);

	return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);
	pmic6323_reg = rdata;
	if (return_value != 0) {
		pr_notice("Reg[%x]= pmic_wrap read data fail\n", RegNum);
		mutex_unlock(&pmic_access_mutex);
		return return_value;
	}

	pmic6323_reg &= (MASK << SHIFT);
	*val = (pmic6323_reg >> SHIFT);

	mutex_unlock(&pmic_access_mutex);

	return return_value;
}

u32 pmic_config_interface(u32 RegNum, u32 val, u32 MASK, u32 SHIFT)
{
	u32 return_value = 0;
	u32 pmic6323_reg = 0;
	u32 rdata;

	mutex_lock(&pmic_access_mutex);

	return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);
	pmic6323_reg = rdata;
	if (return_value != 0) {
		pr_notice("Reg[%x]= pmic_wrap read data fail\n", RegNum);
		mutex_unlock(&pmic_access_mutex);
		return return_value;
	}

	pmic6323_reg &= ~(MASK << SHIFT);
	pmic6323_reg |= (val << SHIFT);

	return_value = pwrap_wacs2(1, (RegNum), pmic6323_reg, &rdata);
	if (return_value != 0) {
		pr_notice("Reg[%x]= pmic_wrap read data fail\n", RegNum);
		mutex_unlock(&pmic_access_mutex);
		return return_value;
	}

	mutex_unlock(&pmic_access_mutex);

	return return_value;
}

u32 pmic_read_interface_nolock(u32 RegNum, u32 *val, u32 MASK, u32 SHIFT)
{
	u32 return_value = 0;
	u32 pmic6323_reg = 0;
	u32 rdata;

	return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);
	pmic6323_reg = rdata;
	if (return_value != 0) {
		pr_notice("Reg[%x]= pmic_wrap read data fail\n",
			  RegNum);
		return return_value;
	}

	pmic6323_reg &= (MASK << SHIFT);
	*val = (pmic6323_reg >> SHIFT);

	return return_value;
}

u32 pmic_config_interface_nolock(u32 RegNum, u32 val, u32 MASK, u32 SHIFT)
{
	u32 return_value = 0;
	u32 pmic6323_reg = 0;
	u32 rdata;

	return_value = pwrap_wacs2(0, (RegNum), 0, &rdata);
	pmic6323_reg = rdata;
	if (return_value != 0) {
		pr_notice("Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}

	pmic6323_reg &= ~(MASK << SHIFT);
	pmic6323_reg |= (val << SHIFT);

	return_value = pwrap_wacs2(1, (RegNum), pmic6323_reg, &rdata);
	if (return_value != 0) {
		pr_notice("Reg[%x]= pmic_wrap read data fail\n", RegNum);
		return return_value;
	}

	return return_value;
}

u32 upmu_get_reg_value(u32 reg)
{
	u32 reg_val = 0;

	pmic_read_interface(reg, &reg_val, 0xFFFF, 0x0);

	return reg_val;
}
EXPORT_SYMBOL(upmu_get_reg_value);

void upmu_set_reg_value(u32 reg, u32 reg_val)
{
	pmic_config_interface(reg, reg_val, 0xFFFF, 0x0);
}

struct wake_lock pmicAuxadc_irq_lock;

u32 pmic_is_auxadc_busy(void)
{
	u32 ret = 0;
	u32 int_status_val_0 = 0;

	ret = pmic_read_interface_nolock(0x73a, (&int_status_val_0), 0x7FFF, 0x1);
	return int_status_val_0;
}

void PMIC_IMM_PollingAuxadcChannel(void)
{
	u32 ret = 0;

	if (upmu_get_rg_adc_deci_gdly() == 1) {
		while (upmu_get_rg_adc_deci_gdly() == 1) {
			unsigned long flags;

			spin_lock_irqsave(&pmic_adc_lock, flags);
			if (pmic_is_auxadc_busy() == 0) {
				/* upmu_set_rg_adc_deci_gdly(0); */
				ret =
				    pmic_config_interface_nolock(AUXADC_CON19, 0,
								 PMIC_RG_ADC_DECI_GDLY_MASK,
								 PMIC_RG_ADC_DECI_GDLY_SHIFT);
			}
			spin_unlock_irqrestore(&pmic_adc_lock, flags);
		}
	}
}

unsigned int PMIC_IMM_GetOneChannelValue(pmic_adc_ch_list_enum dwChannel, int deCount, int trimd)
{

	s32 ret_data;
	s32 count = 0;
	s32 u4Sample_times = 0;
	s32 u4channel = 0;
	s32 adc_result_temp = 0;
	s32 r_val_temp = 0;
	s32 adc_result = 0;
	s32 ret = 0;
	s32 adc_reg_val = 0;

	/*
	   0 : BATON2 **
	   1 : CH6
	   2 : THR SENSE2 **
	   3 : THR SENSE1
	   4 : VCDT
	   5 : BATON1
	   6 : ISENSE
	   7 : BATSNS
	   8 : ACCDET
	   9-16 : audio
	 */

	/* do not support BATON2 and THR SENSE2 for sw workaround */
	if (dwChannel == 0 || dwChannel == 2)
		return 0;

	if (atomic_read(&pmic_suspend) && (dwChannel == 5)) {
		pr_notice("[%s] suspend(%d)\n", __func__, atomic_read(&pmic_suspend));
		return -1;
	}

	wake_lock(&pmicAuxadc_irq_lock);

	mutex_lock(&pmic_adc_mutex);

	do {

		PMIC_IMM_PollingAuxadcChannel();


		if (dwChannel < 9) {
			upmu_set_rg_vbuf_en(1);

			/* set 0 */
			ret =
			    pmic_read_interface(AUXADC_CON22, &adc_reg_val,
						PMIC_RG_AP_RQST_LIST_MASK,
						PMIC_RG_AP_RQST_LIST_SHIFT);
			adc_reg_val = adc_reg_val & (~(1 << dwChannel));
			ret =
			    pmic_config_interface(AUXADC_CON22, adc_reg_val,
						  PMIC_RG_AP_RQST_LIST_MASK,
						  PMIC_RG_AP_RQST_LIST_SHIFT);

			/* set 1 */
			ret =
			    pmic_read_interface(AUXADC_CON22, &adc_reg_val,
						PMIC_RG_AP_RQST_LIST_MASK,
						PMIC_RG_AP_RQST_LIST_SHIFT);
			adc_reg_val = adc_reg_val | (1 << dwChannel);
			ret =
			    pmic_config_interface(AUXADC_CON22, adc_reg_val,
						  PMIC_RG_AP_RQST_LIST_MASK,
						  PMIC_RG_AP_RQST_LIST_SHIFT);
		} else if (dwChannel >= 9 && dwChannel <= 16) {
			ret =
			    pmic_read_interface(AUXADC_CON23, &adc_reg_val,
						PMIC_RG_AP_RQST_LIST_RSV_MASK,
						PMIC_RG_AP_RQST_LIST_RSV_SHIFT);
			adc_reg_val = adc_reg_val & (~(1 << (dwChannel - 9)));
			ret =
			    pmic_config_interface(AUXADC_CON23, adc_reg_val,
						  PMIC_RG_AP_RQST_LIST_RSV_MASK,
						  PMIC_RG_AP_RQST_LIST_RSV_SHIFT);

			/* set 1 */
			ret =
			    pmic_read_interface(AUXADC_CON23, &adc_reg_val,
						PMIC_RG_AP_RQST_LIST_RSV_MASK,
						PMIC_RG_AP_RQST_LIST_RSV_SHIFT);
			adc_reg_val = adc_reg_val | (1 << (dwChannel - 9));
			ret =
			    pmic_config_interface(AUXADC_CON23, adc_reg_val,
						  PMIC_RG_AP_RQST_LIST_RSV_MASK,
						  PMIC_RG_AP_RQST_LIST_RSV_SHIFT);
		}

		/* Duo to HW limitation */
		if (dwChannel != 8)
			udelay(300);

		count = 0;
		ret_data = 0;

		switch (dwChannel) {
		case 0:
			while (upmu_get_rg_adc_rdy_baton2() != 1) {
				usleep_range(1000, 2000);
				if ((count++) > ADC_COUNT_TIMEOUT) {
					pr_err("[IMM_GetOneChannelValue_PMIC] (%d) Time out!\n",
					       dwChannel);
					break;
				}
			}
			ret_data = upmu_get_rg_adc_out_baton2();
			break;

		case 1:
			while (upmu_get_rg_adc_rdy_ch6() != 1) {
				usleep_range(1000, 2000);
				if ((count++) > ADC_COUNT_TIMEOUT) {
					pr_err("[IMM_GetOneChannelValue_PMIC] (%d) Time out!\n",
					       dwChannel);
					break;
				}
			}
			ret_data = upmu_get_rg_adc_out_ch6();
			pr_notice("[upmu_get_rg_adc_out_ch6] 0x%x\n", ret_data);
			break;
		case 2:
			while (upmu_get_rg_adc_rdy_thr_sense2() != 1) {
				usleep_range(1000, 2000);
				if ((count++) > ADC_COUNT_TIMEOUT) {
					pr_err("[IMM_GetOneChannelValue_PMIC] (%d) Time out!\n",
					       dwChannel);
					break;
				}
			}
			ret_data = upmu_get_rg_adc_out_thr_sense2();
			break;
		case 3:
			while (upmu_get_rg_adc_rdy_thr_sense1() != 1) {
				usleep_range(1000, 2000);
				if ((count++) > ADC_COUNT_TIMEOUT) {
					pr_err("[IMM_GetOneChannelValue_PMIC] (%d) Time out!\n",
					       dwChannel);
					break;
				}
			}
			ret_data = upmu_get_rg_adc_out_thr_sense1();
			break;
		case 4:
			while (upmu_get_rg_adc_rdy_vcdt() != 1) {
				usleep_range(1000, 2000);
				if ((count++) > ADC_COUNT_TIMEOUT) {
					pr_err("[IMM_GetOneChannelValue_PMIC] (%d) Time out!\n",
					       dwChannel);
					break;
				}
			}
			ret_data = upmu_get_rg_adc_out_vcdt();
			break;
		case 5:
			while (upmu_get_rg_adc_rdy_baton1() != 1) {
				usleep_range(1000, 2000);
				if ((count++) > ADC_COUNT_TIMEOUT) {
					pr_err("[IMM_GetOneChannelValue_PMIC] (%d) Time out!\n",
					       dwChannel);
					break;
				}
			}
			ret_data = upmu_get_rg_adc_out_baton1();
			break;
		case 6:
			while (upmu_get_rg_adc_rdy_isense() != 1) {
				usleep_range(1000, 2000);
				if ((count++) > ADC_COUNT_TIMEOUT) {
					pr_err("[IMM_GetOneChannelValue_PMIC] (%d) Time out!\n",
					       dwChannel);
					break;
				}
			}
			ret_data = upmu_get_rg_adc_out_isense();
			break;
		case 7:
			while (upmu_get_rg_adc_rdy_batsns() != 1) {
				usleep_range(1000, 2000);
				if ((count++) > ADC_COUNT_TIMEOUT) {
					pr_err("[IMM_GetOneChannelValue_PMIC] (%d) Time out!\n",
					       dwChannel);
					break;
				}
			}
			ret_data = upmu_get_rg_adc_out_batsns();
			break;

		case 8:
			while (upmu_get_rg_adc_rdy_ch5() != 1)
				;
			ret_data = upmu_get_rg_adc_out_ch5();
			break;
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
			while (upmu_get_rg_adc_rdy_int() != 1) {
				usleep_range(1000, 2000);
				if ((count++) > ADC_COUNT_TIMEOUT) {
					pr_err("[IMM_GetOneChannelValue_PMIC] (%d) Time out!\n",
					       dwChannel);
					break;
				}
			}
			ret_data = upmu_get_rg_adc_out_int();
			break;

		default:
			pr_err("[AUXADC] Invalid channel value(%d,%d)\n", dwChannel, trimd);
			mutex_unlock(&pmic_adc_mutex);
			wake_unlock(&pmicAuxadc_irq_lock);
			return -1;
		}

		u4channel += ret_data;

		u4Sample_times++;

	} while (u4Sample_times < deCount);

	mutex_unlock(&pmic_adc_mutex);

	/* Value averaging  */
	adc_result_temp = u4channel / deCount;

	switch (dwChannel) {
	case 0:
		r_val_temp = 1;
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	case 1:
		r_val_temp = 1;
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	case 2:
		r_val_temp = 1;
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	case 3:
		r_val_temp = 1;
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	case 4:
		r_val_temp = 1;
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	case 5:
		r_val_temp = 1;
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	case 6:
		r_val_temp = 4;
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	case 7:
		r_val_temp = 4;
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	case 8:
		r_val_temp = 1;
		adc_result = (adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
		break;
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
		adc_result = adc_result_temp;
		break;
	default:
		pr_err("[AUXADC] Invalid channel value(%d,%d)\n", dwChannel, trimd);
		wake_unlock(&pmicAuxadc_irq_lock);
		return -1;
	}

	pr_debug("[%s] CH%d adc_result_temp=%d, adc_result=%d\n",
		__func__, dwChannel, adc_result_temp, adc_result);

	wake_unlock(&pmicAuxadc_irq_lock);

	return adc_result;

}

int pmic_get_buck_current(int avg_times)
{
	int raw_data = 0;
	int val = 0;
	int offset = 0;		/* internal offset voltage = XmV, 80 */

	upmu_set_rg_smps_testmode_b(0x10);
	pr_notice
	    ("[pmic_get_buck_current] before meter, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
	     0x200, upmu_get_reg_value(0x200), 0x758, upmu_get_reg_value(0x758), 0x76E,
	     upmu_get_reg_value(0x76E)
	    );

	raw_data = PMIC_IMM_GetOneChannelValue(1, avg_times, 1);	/* Vdac = code / 32768 * 1800mV */
	val = raw_data - offset;
	if (val > 0)
		val = (val * 10) / 6;	/* Iload = Vdac / 0.6ohm */
	else
		val = 0;

	pr_notice("[pmic_get_buck_current] raw_data=%d, val=%d, avg_times=%d\n",
		  raw_data, val, avg_times);

	upmu_set_rg_smps_testmode_b(0x0);
	pr_notice
	    ("[pmic_get_buck_current] after meter, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, Reg[0x%x]=0x%x\n",
	     0x200, upmu_get_reg_value(0x200), 0x758, upmu_get_reg_value(0x758), 0x76E,
	     upmu_get_reg_value(0x76E)
	    );

	return val;
}
EXPORT_SYMBOL(pmic_get_buck_current);

void upmu_interrupt_chrdet_int_en(u32 val)
{
	pr_notice("[upmu_interrupt_chrdet_int_en] val=%d.\r\n", val);

	upmu_set_rg_int_en_chrdet(val);
}
EXPORT_SYMBOL(upmu_interrupt_chrdet_int_en);

static DEFINE_MUTEX(pmic_mutex);

/* mt6323 irq chip event read. */
static unsigned int mt6323_get_events_locked(struct mt6323_chip_priv *chip)
{
	/* value does not change in case of pwrap_read() error,
	 * so we must have valid defaults */
	unsigned int events[2] = { 0 };

	pwrap_read(chip->int_stat[0], &events[0]);
	pwrap_read(chip->int_stat[1], &events[1]);

	return (events[1] << 16) | (events[0] & 0xFFFF);
}

static unsigned int mt6323_get_events(struct mt6323_chip_priv *chip)
{
	unsigned int event;

	pmic_lock();
	event = mt6323_get_events_locked(chip);
	pmic_unlock();

	return event;
}

/* mt6323 irq chip event mask read: debugging only */
static unsigned int mt6323_get_event_mask_locked(struct mt6323_chip_priv *chip)
{
	/* value does not change in case of pwrap_read() error,
	 * so we must have valid defaults */
	unsigned int event_mask[2] = { 0 };

	pwrap_read(chip->int_con[0], &event_mask[0]);
	pwrap_read(chip->int_con[1], &event_mask[1]);

	return (event_mask[1] << 16) | (event_mask[0] & 0xFFFF);
}

static unsigned int mt6323_get_event_mask(struct mt6323_chip_priv *chip)
{
	unsigned int ret;

	pmic_lock();
	ret = mt6323_get_event_mask_locked(chip);
	pmic_unlock();

	return ret;
}

/* mt6323 irq chip event mask write: initial setup */
static void mt6323_set_event_mask_locked(struct mt6323_chip_priv *chip, unsigned int event_mask)
{
	unsigned int val;

	pwrap_write(chip->int_con[0], event_mask & 0xFFFF);
	pwrap_write(chip->int_con[1], (event_mask >> 16) & 0xFFFF);
	pwrap_read(chip->int_con[0], &val);
	pwrap_read(chip->int_con[1], &val);
	chip->event_mask = event_mask;
}

static void mt6323_set_event_mask(struct mt6323_chip_priv *chip, unsigned int event_mask)
{
	pmic_lock();
	mt6323_set_event_mask_locked(chip, event_mask);
	pmic_unlock();
}

/* this function is only called by generic IRQ framework, and it is always
 * called with pmic_lock held by IRQ framework. */
static void mt6323_irq_mask_unmask_locked(struct irq_data *d, bool enable)
{
	struct mt6323_chip_priv *mt_chip = d->chip_data;
	int hw_irq = d->hwirq;
	u16 port = (hw_irq >> 4) & 1;
	unsigned int val;

	if (enable)
		set_bit(hw_irq, (unsigned long *)&mt_chip->event_mask);
	else
		clear_bit(hw_irq, (unsigned long *)&mt_chip->event_mask);

	if (port) {
		pwrap_write(mt_chip->int_con[1], (mt_chip->event_mask >> 16) & 0xFFFF);
		pwrap_read(mt_chip->int_con[1], &val);
	} else {
		pwrap_write(mt_chip->int_con[0], mt_chip->event_mask & 0xFFFF);
		pwrap_read(mt_chip->int_con[0], &val);
#if defined(CONFIG_MTK_BATTERY_PROTECT)
		/* lbat irq src need toggle to enable again. */
		toggle_lbat_irq_src(enable, hw_irq);
#endif
	}
}

static void mt6323_irq_disable(struct irq_data *d)
{
	mt6323_irq_mask_unmask_locked(d, false);
	mdelay(5);
}

static void mt6323_irq_enable(struct irq_data *d)
{
	mt6323_irq_mask_unmask_locked(d, true);
}

static void mt6323_irq_ack_locked(struct mt6323_chip_priv *chip, unsigned int event_mask)
{
	unsigned int val[2];

	pwrap_write(chip->int_stat[0], event_mask & 0xFFFF);
	pwrap_write(chip->int_stat[1], (event_mask >> 16) & 0xFFFF);
	pwrap_read(chip->int_stat[0], &val[0]);
	pwrap_read(chip->int_stat[1], &val[1]);
}

#ifdef CONFIG_AMAZON_POWEROFF_LOG
static void log_long_press_power_key(void)
{
	int rc;
	char *argv[] = {
		"/sbin/crashreport",
		"long_press_power_key",
		NULL
	};

	rc = call_usermodehelper(argv[0], argv, NULL, UMH_WAIT_EXEC);

	if (rc < 0)
		pr_err("call /sbin/crashreport failed, rc = %d\n", rc);

	msleep(6000); /* 6s */
}
#endif /* CONFIG_AMAZON_POWEROFF_LOG */

static void long_press_restart(struct work_struct *dummy)
{
	unsigned int pwrkey_deb = 0;
	mutex_lock(&pmic_mutex);

	pwrkey_deb = upmu_get_pwrkey_deb();
	if (pwrkey_deb == 1) {
		pr_err("[check_pwrkey_release_timer] pwrkey release, do nothing\n");
	} else {
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
		if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT) {
			pr_notice("Long Press Power Key Pressed during kernel power off charging, reboot OS\r\n");
			arch_reset(0, NULL);
		}
#endif
		pr_err("Long key press power off\n");
#ifdef CONFIG_AMAZON_POWEROFF_LOG
		log_long_press_power_key();
#endif /* CONFIG_AMAZON_POWEROFF_LOG */
		if (upmu_get_pwrkey_deb())
			goto done;
		sys_sync();
		rtc_mark_enter_lprst();  /* for metrics */
		rtc_mark_enter_sw_lprst(); /* for long press power off */
		#if defined(CONFIG_MTK_AUTO_POWER_ON_WITH_CHARGER)
		if (upmu_get_rgs_chrdet())
			kernel_restart("enter_kpoc");
		#endif

		kernel_restart("kernel power off by long press power key");
	}
done:
	mutex_unlock(&pmic_mutex);
}

static void deferred_restart(struct work_struct *dummy)
{
	unsigned int pwrkey_deb = 0;

	/* Double check if pwrkey is still pressed */
	pwrkey_deb = upmu_get_pwrkey_deb();
	if (pwrkey_deb == 1) {
		pr_info("[check_pwrkey_release_timer] Release pwrkey\n");
		kpd_pwrkey_pmic_handler(0x0);
	} else
		pr_info("[check_pwrkey_release_timer] Still press pwrkey, do nothing\n");

}

enum hrtimer_restart long_press_pwrkey_shutdown_timer_func(struct hrtimer *timer)
{
	queue_work(system_highpri_wq, &long_press_restart_work);
	return HRTIMER_NORESTART;
}

enum hrtimer_restart check_pwrkey_release_timer_func(struct hrtimer *timer)
{
	queue_work(system_highpri_wq, &restart_work);
	return HRTIMER_NORESTART;
}

static irqreturn_t spkl_ab_int_handler(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

static irqreturn_t spkl_d_int_handler(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

static irqreturn_t bat_l_int_handler(int irq, void *dev_id)
{
	upmu_set_rg_lbat_irq_en_min(0);
	return IRQ_HANDLED;
}

static irqreturn_t bat_h_int_handler(int irq, void *dev_id)
{
	upmu_set_rg_lbat_irq_en_max(0);
	return IRQ_HANDLED;
}

static irqreturn_t watchdog_int_handler(int irq, void *dev_id)
{
	int arg = 0;

	/* upmu_set_rg_chrwdt_td(tmo);*/
	upmu_set_rg_chrwdt_en(arg);
	upmu_set_rg_chrwdt_int_en(arg);
	upmu_set_rg_chrwdt_wr(arg);
	upmu_set_rg_chrwdt_flag_wr(1);

	return IRQ_HANDLED;
}

#ifdef CONFIG_AMAZON_METRICS_LOG
#define PWRKEY_METRICS_STR_LEN 128
static void pwrkey_log_to_metrics(struct work_struct *data)
{
	char *action;
	char buf[PWRKEY_METRICS_STR_LEN];

	action = (pwrkey_press) ? "press" : "release";
	snprintf(buf, PWRKEY_METRICS_STR_LEN,
		"%s:powi%c:report_action_is_%s=1;CT;1:NR", __func__,
		action[0], action);
	log_to_metrics(ANDROID_LOG_INFO, "PowerKeyEvent", buf);

}
#endif

static irqreturn_t pwrkey_int_handler(int irq, void *dev_id)
{
	struct mt6323_chip_priv *chip = (struct mt6323_chip_priv *)dev_id;
	ktime_t ktime, ktime_lp;

	if (upmu_get_pwrkey_deb() == 1) {
		pr_notice("[pwrkey_int_handler] Release pwrkey\n");
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
		if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT && timer_pre != 0) {
			timer_pos = sched_clock();
			if (timer_pos - timer_pre >= LONG_PWRKEY_PRESS_TIME)
				long_pwrkey_press = true;
			pr_notice
			    ("timer_pos = %ld, timer_pre = %ld, timer_pos-timer_pre = %ld, long_pwrkey_press = %d\r\n",
			     timer_pos, timer_pre, timer_pos - timer_pre, long_pwrkey_press);
			if (long_pwrkey_press) {	/* 500ms */
				pr_notice
				    ("Power Key Pressed during kernel power off charging, reboot OS\r\n");
				arch_reset(0, NULL);
			}
		}
#endif
		hrtimer_cancel(&chip->check_pwrkey_release_timer);
		hrtimer_cancel(&chip->long_press_pwrkey_shutdown_timer);
		if (chip->pressed == 0) {
			kpd_pwrkey_pmic_handler(0x1);
			kpd_pwrkey_pmic_handler(0x0);
		} else {
			kpd_pwrkey_pmic_handler(0x0);
		}
		upmu_set_rg_pwrkey_int_sel(0);
		chip->pressed = 0;
	} else {
		chip->pressed = 1;
		pr_notice("[pwrkey_int_handler] Press pwrkey\n");
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
		#if defined(CONFIG_abe123)
		if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT) {
			pr_notice
				("Power Key Pressed during kernel power off charging, reboot OS\r\n");
			arch_reset(0, NULL);
		}
		#else
		if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT)
			timer_pre = sched_clock();
		#endif
#endif
		ktime = ktime_set(RELEASE_PWRKEY_TIME, 0);
		hrtimer_start(&chip->check_pwrkey_release_timer, ktime, HRTIMER_MODE_REL);
		ktime_lp = ktime_set(chip->shutdown_time, 0);
		hrtimer_start(&chip->long_press_pwrkey_shutdown_timer, ktime_lp,
			HRTIMER_MODE_REL);
		kpd_pwrkey_pmic_handler(0x1);
		upmu_set_rg_pwrkey_int_sel(1);
	}

#ifdef CONFIG_AMAZON_METRICS_LOG
	if (chip->pressed == 1)
		pwrkey_press = true;
	else
		pwrkey_press = false;
	schedule_work(&metrics_work);
#endif

	return IRQ_HANDLED;
}

static irqreturn_t thr_l_int_handler(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

static irqreturn_t thr_h_int_handler(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

static irqreturn_t vbaton_undet_int_handler(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

static irqreturn_t bvalid_det_int_handler(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

static irqreturn_t chrdet_int_handler(int irq, void *dev_id)
{
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
#ifndef CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT
	if (!upmu_get_rgs_chrdet()) {
		int boot_mode = 0;

		boot_mode = get_boot_mode();

		if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
		    || boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_notice
			    ("Unplug Charger/USB In Kernel Power Off Charging Mode!  Shutdown OS!\r\n");
			kernel_power_off();
		}
	}
#endif
#endif
	do_chrdet_int_task();
	return IRQ_HANDLED;
}

static irqreturn_t vbat_ov_int_handler(int irq, void *dev_id)
{
	pr_notice("[vbat_ov_int_handler] Ignore VBAT_OV interrupt\n");
	/* Turn off OV detect */
	upmu_set_rg_vbat_ov_en(0);
	return IRQ_HANDLED;
}

static irqreturn_t ldo_oc_int_handler(int irq, void *dev_id)
{
	pr_notice("[PMIC] Reg[0x%x]=0x%x\n", OCSTATUS0, upmu_get_reg_value(OCSTATUS0));
	pr_notice("[PMIC] Reg[0x%x]=0x%x\n", OCSTATUS1, upmu_get_reg_value(OCSTATUS1));
	return IRQ_HANDLED;
}

static irqreturn_t fchr_key_int_handler(int irq, void *dev_id)
{
	if (upmu_get_fchrkey_deb() == 1) {
		pr_notice("[fchr_key_int_handler] Release fchrkey\n");
		kpd_pmic_rstkey_handler(0x0);
		upmu_set_rg_fchrkey_int_sel(0);
	} else {
		pr_notice("[fchr_key_int_handler] Press fchrkey\n");
		kpd_pmic_rstkey_handler(0x1);
		upmu_set_rg_fchrkey_int_sel(1);
	}
	return IRQ_HANDLED;
}

static irqreturn_t accdet_int_handler(int irq, void *dev_id)
{
	u32 ret;

	pr_notice("[accdet_int_handler]....\n");
	ret = accdet_irq_handler();
	if (0 == ret)
		pr_notice("[accdet_int_handler] don't finished\n");
	return IRQ_HANDLED;
}

static irqreturn_t audio_int_handler(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

static irqreturn_t rtc_int_handler(int irq, void *dev_id)
{
	rtc_irq_handler();
	return IRQ_HANDLED;
}

static irqreturn_t vproc_oc_int_handler(int irq, void *dev_id)
{
	pr_notice("[PMIC] Reg[0x%x]=0x%x\n", OCSTATUS0, upmu_get_reg_value(OCSTATUS0));
	pr_notice("[PMIC] Reg[0x%x]=0x%x\n", OCSTATUS1, upmu_get_reg_value(OCSTATUS1));

	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vproc(0);
	return IRQ_HANDLED;
}

static irqreturn_t vsys_oc_int_handler(int irq, void *dev_id)
{
	pr_notice("[PMIC] Reg[0x%x]=0x%x\n", OCSTATUS0, upmu_get_reg_value(OCSTATUS0));
	pr_notice("[PMIC] Reg[0x%x]=0x%x\n", OCSTATUS1, upmu_get_reg_value(OCSTATUS1));

	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vsys(0);
	return IRQ_HANDLED;
}

static irqreturn_t vpa_oc_int_handler(int irq, void *dev_id)
{
	pr_notice("[PMIC] Reg[0x%x]=0x%x\n", OCSTATUS0, upmu_get_reg_value(OCSTATUS0));
	pr_notice("[PMIC] Reg[0x%x]=0x%x\n", OCSTATUS1, upmu_get_reg_value(OCSTATUS1));

	upmu_set_rg_pwmoc_ck_pdn(1);
	upmu_set_rg_int_en_vpa(0);
	return IRQ_HANDLED;
}

static struct mt6323_irq_data mt6323_irqs[] = {
	{
	 .name = "mt6323_spkl_ab",
	 .irq_id = RG_INT_STATUS_SPKL_AB,
	 .action_fn = spkl_ab_int_handler,
	 },
	{
	 .name = "mt6323_spkl_d",
	 .irq_id = RG_INT_STATUS_SPKL,
	 .action_fn = spkl_d_int_handler,
	 },
	{
	 .name = "mt6323_bat_l",
	 .irq_id = RG_INT_STATUS_BAT_L,
	 .action_fn = bat_l_int_handler,
	 },
	{
	 .name = "mt6323_bat_h",
	 .irq_id = RG_INT_STATUS_BAT_H,
	 .action_fn = bat_h_int_handler,
	 },
	{
	 .name = "mt6323_watchdog",
	 .irq_id = RG_INT_STATUS_WATCHDOG,
	 .action_fn = watchdog_int_handler,
	 .enabled = true,
	 },
	{
	 .name = "mt6323_pwrkey",
	 .irq_id = RG_INT_STATUS_PWRKEY,
	 .action_fn = pwrkey_int_handler,
	 .enabled = true,
	 .wake_src = true,
	 },
	{
	 .name = "mt6323_thr_l",
	 .irq_id = RG_INT_STATUS_THR_L,
	 .action_fn = thr_l_int_handler,
	 },
	{
	 .name = "mt6323_thr_h",
	 .irq_id = RG_INT_STATUS_THR_H,
	 .action_fn = thr_h_int_handler,
	 },
	{
	 .name = "mt6323_vbaton_undet",
	 .irq_id = RG_INT_STATUS_VBATON_UNDET,
	 .action_fn = vbaton_undet_int_handler,
	 },
	{
	 .name = "mt6323_bvalid_det",
	 .irq_id = RG_INT_STATUS_BVALID_DET,
	 .action_fn = bvalid_det_int_handler,
	 },
	{
	 .name = "mt6323_chrdet",
	 .irq_id = RG_INT_STATUS_CHRDET,
	 .action_fn = chrdet_int_handler,
	 .enabled = true,
	 .wake_src = true,
	 },
	{
	 .name = "mt6323_ov",
	 .irq_id = RG_INT_STATUS_OV,
	 .action_fn = vbat_ov_int_handler,
	 .enabled = true,
	 },
	{
	 .name = "mt6323_ldo",
	 .irq_id = RG_INT_STATUS_LDO,
	 .action_fn = ldo_oc_int_handler,
	 },
	{
	 .name = "mt6323_fchrkey",
	 .irq_id = RG_INT_STATUS_FCHRKEY,
	 .action_fn = fchr_key_int_handler,
	 .enabled = true,
	 },
	{
	 .name = "mt6323_accdet",
	 .irq_id = RG_INT_STATUS_ACCDET,
	 .action_fn = accdet_int_handler,
	 .enabled = true,
	 },
	{
	 .name = "mt6323_audio",
	 .irq_id = RG_INT_STATUS_AUDIO,
	 .action_fn = audio_int_handler,
	 },
	{
	 .name = "mt6323_rtc",
	 .irq_id = RG_INT_STATUS_RTC,
	 .action_fn = rtc_int_handler,
	 .enabled = true,
	 .wake_src = true,
	 },
	{
	 .name = "mt6323_vproc",
	 .irq_id = RG_INT_STATUS_VPROC,
	 .action_fn = vproc_oc_int_handler,
	 },
	{
	 .name = "mt6323_vsys",
	 .irq_id = RG_INT_STATUS_VSYS,
	 .action_fn = vsys_oc_int_handler,
	 },
	{
	 .name = "mt6323_vpa",
	 .irq_id = RG_INT_STATUS_VPA,
	 .action_fn = vpa_oc_int_handler,
	 },
};

static inline void mt6323_do_handle_events(struct mt6323_chip_priv *chip, unsigned int events)
{
	int event_hw_irq;
	int e = events;

	for (event_hw_irq = __ffs(events); events;
	     events &= ~(1 << event_hw_irq), event_hw_irq = __ffs(events)) {
		int event_irq = irq_find_mapping(chip->domain, event_hw_irq);

		pr_debug("%s: event=%d, event_irq %d\n", __func__, event_hw_irq, event_irq);

		if (event_irq)
			handle_nested_irq(event_irq);
	}
	mt6323_irq_ack_locked(chip, e);
}

static irqreturn_t mt6323_irq(int irq, void *d)
{
	struct mt6323_chip_priv *chip = (struct mt6323_chip_priv *)d;
	unsigned int events = mt6323_get_events(chip);

	wake_lock(&chip->irq_lock);
	/* if event happens when it is masked, it is a HW bug,
	 * unless it is a wakeup interrupt */
	if (events & ~(chip->event_mask | chip->wake_mask)) {
		pr_err("%s: PMIC is raising events %08X which are not enabled\n"
		       "\t(mask 0x%lx, wakeup 0x%lx). HW BUG. Stop\n",
		       __func__, events, chip->event_mask, chip->wake_mask);
		pr_err("int ctrl: %08x, status: %08x\n",
		       mt6323_get_event_mask_locked(chip), mt6323_get_events(chip));
		pr_err("int ctrl: %08x, status: %08x\n",
		       mt6323_get_event_mask_locked(chip), mt6323_get_events(chip));
		BUG();
	}

	mt6323_do_handle_events(chip, events);
	wake_unlock(&chip->irq_lock);

	return IRQ_HANDLED;
}

static void mt6323_irq_bus_lock(struct irq_data *d)
{
	pmic_lock();
}

static void mt6323_irq_bus_sync_unlock(struct irq_data *d)
{
	pmic_unlock();
}

static void mt6323_irq_chip_suspend(struct mt6323_chip_priv *chip)
{
	chip->saved_mask = mt6323_get_event_mask(chip);
	pr_debug("%s: read event mask=%08X\n", __func__, chip->saved_mask);
	mt6323_set_event_mask(chip, chip->wake_mask);
	pr_debug("%s: write event mask= 0x%lx\n", __func__, chip->wake_mask);
}

static void mt6323_irq_chip_resume(struct mt6323_chip_priv *chip)
{
	u32 events = mt6323_get_events(chip);
	int event = 0;

	if (events)
		event = __ffs(events);

	mt6323_set_event_mask(chip, chip->saved_mask);
	pr_debug("%s: saved_mask = %08X, events = %x\n", __func__, chip->saved_mask, events);
}

static int mt6323_irq_set_wake_locked(struct irq_data *d, unsigned int on)
{
	struct mt6323_chip_priv *chip = irq_data_get_irq_chip_data(d);

	if (on)
		set_bit(d->hwirq, (unsigned long *)&chip->wake_mask);
	else
		clear_bit(d->hwirq, (unsigned long *)&chip->wake_mask);
	return 0;
}

static struct irq_chip mt6323_irq_chip = {
	.name = "mt6323-irqchip",
	.irq_disable = mt6323_irq_disable,
	.irq_enable = mt6323_irq_enable,
	.irq_set_wake = mt6323_irq_set_wake_locked,
	.irq_bus_lock = mt6323_irq_bus_lock,
	.irq_bus_sync_unlock = mt6323_irq_bus_sync_unlock,
};

static int mt6323_irq_domain_map(struct irq_domain *d, unsigned int irq,
					irq_hw_number_t hw)
{
	struct mt6323_chip_priv *mt6323 = d->host_data;

	irq_set_chip_data(irq, mt6323);
	irq_set_chip_and_handler(irq, &mt6323_irq_chip, handle_simple_irq);
	irq_set_nested_thread(irq, 1);
#ifdef CONFIG_ARM
	set_irq_flags(irq, IRQF_VALID);
#else
	irq_set_noprobe(irq);
#endif

	return 0;
}

static struct irq_domain_ops mt6323_irq_domain_ops = {
	.map = mt6323_irq_domain_map,
};

static int mt6323_irq_init(struct mt6323_chip_priv *chip)
{
	int ret;

	chip->domain = irq_domain_add_linear(chip->dev->of_node->parent,
		MT6323_IRQ_NR, &mt6323_irq_domain_ops, chip);
	if (!chip->domain) {
		dev_err(chip->dev, "could not create irq domain\n");
		return -ENOMEM;
	}

	mt6323_set_event_mask(chip, 0);

	ret = request_threaded_irq(chip->irq, NULL, mt6323_irq,
				    IRQF_ONESHOT, mt6323_irq_chip.name, chip);
	if (ret < 0) {
		pr_err("%s: PMIC master irq request err: %d\n", __func__, ret);
		goto err_free_domain;
	}

	irq_set_irq_wake(chip->irq, true);
	return 0;
 err_free_domain:
	irq_domain_remove(chip->domain);
	return ret;
}

static int mt6323_irq_handler_init(struct mt6323_chip_priv *chip)
{
	int i;
	/*AP:
	 * I register all the non-default vectors,
	 * and disable all vectors that were not enabled by original code;
	 * threads are created for all non-default vectors.
	 */
	for (i = 0; i < ARRAY_SIZE(mt6323_irqs); i++) {
		int ret, irq;
		struct mt6323_irq_data *data = &mt6323_irqs[i];

		irq = irq_create_mapping(chip->domain, data->irq_id);
		ret = request_threaded_irq(irq, NULL, data->action_fn,
					   IRQF_TRIGGER_HIGH | IRQF_ONESHOT, data->name, chip);
		if (ret) {
			pr_err("%s: failed to register irq=%d (%d); name='%s'; err: %d\n",
				__func__, irq, data->irq_id, data->name, ret);
			continue;
		}
		if (!data->enabled)
			disable_irq(irq);
		if (data->wake_src)
			irq_set_irq_wake(irq, 1);
	}
	return 0;
}

u32 g_reg_value = 0;
static ssize_t show_pmic_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_notice("[show_pmic_access] 0x%x\n", g_reg_value);
	return sprintf(buf, "%u\n", g_reg_value);
}

static ssize_t store_pmic_access(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
{
	int ret = 0;
	char temp_buf[32];
	char *pvalue;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	strncpy(temp_buf, buf, sizeof(temp_buf));
	temp_buf[sizeof(temp_buf) - 1] = 0;
	pvalue = temp_buf;

	if (size != 0) {
		if (size > 5) {
			ret = kstrtouint(strsep(&pvalue, " "), 16, &reg_address);
			if (ret)
				return ret;
			ret = kstrtouint(pvalue, 16, &reg_value);
			if (ret)
				return ret;
			pr_notice("[store_pmic_access] write PMU reg 0x%x with value 0x%x !\n",
				  reg_address, reg_value);
			ret = pmic_config_interface(reg_address, reg_value, 0xFFFF, 0x0);
		} else {
			ret = kstrtouint(pvalue, 16, &reg_address);
			if (ret)
				return ret;
			ret = pmic_read_interface(reg_address, &g_reg_value, 0xFFFF, 0x0);
			pr_notice("[store_pmic_access] read PMU reg 0x%x with value 0x%x !\n",
				  reg_address, g_reg_value);
			pr_notice
			    ("[store_pmic_access] Please use \"cat pmic_access\" to get value\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(pmic_access, 0664, show_pmic_access, store_pmic_access);	/* 664 */

void PMIC_INIT_SETTING_V1(void)
{
	u32 chip_version = 0;
	u32 ret = 0;

	chip_version = upmu_get_cid();

	if (chip_version >= PMIC6323_E2_CID_CODE) {
		pr_notice("[Kernel_PMIC_INIT_SETTING_V1] PMIC Chip = %x\n", chip_version);
		pr_notice("[Kernel_PMIC_INIT_SETTING_V1] 20130604_0.85v\n");

		/* put init setting from DE/SA */

		ret = pmic_config_interface(0x2, 0xB, 0xF, 4);	/* [7:4]: RG_VCDT_HV_VTH; 7V OVP */
		ret = pmic_config_interface(0xC, 0x3, 0x7, 1);	/* [3:1]: RG_VBAT_OV_VTH; VBAT_OV=4.45V */
		ret = pmic_config_interface(0x1A, 0x3, 0xF, 0);	/* [3:0]: RG_CHRWDT_TD; align to 6250's */
		ret = pmic_config_interface(0x24, 0x1, 0x1, 1);	/* [1:1]: RG_BC11_RST; Reset BC11 detection */
		ret = pmic_config_interface(0x2A, 0x0, 0x7, 4);	/* [6:4]: RG_CSDAC_STP; align to 6250's setting */
		ret = pmic_config_interface(0x2E, 0x1, 0x1, 2);	/* [2:2]: RG_CSDAC_MODE; */
		ret = pmic_config_interface(0x2E, 0x1, 0x1, 6);	/* [6:6]: RG_HWCV_EN; */
		ret = pmic_config_interface(0x2E, 0x1, 0x1, 7);	/* [7:7]: RG_ULC_DET_EN; */
		ret = pmic_config_interface(0x3C, 0x1, 0x1, 5);	/* [5:5]: THR_HWPDN_EN; */
		ret = pmic_config_interface(0x40, 0x1, 0x1, 4);	/* [4:4]: RG_EN_DRVSEL; */
		ret = pmic_config_interface(0x40, 0x1, 0x1, 5);	/* [5:5]: RG_RST_DRVSEL; */
		ret = pmic_config_interface(0x46, 0x1, 0x1, 1);	/* [1:1]: PWRBB_DEB_EN; */
		ret = pmic_config_interface(0x48, 0x1, 0x1, 8);	/* [8:8]: VPROC_PG_H2L_EN; */
		ret = pmic_config_interface(0x48, 0x1, 0x1, 9);	/* [9:9]: VSYS_PG_H2L_EN; */
		ret = pmic_config_interface(0x4E, 0x1, 0x1, 5);	/* [5:5]: STRUP_AUXADC_RSTB_SW; */
		ret = pmic_config_interface(0x4E, 0x1, 0x1, 7);	/* [7:7]: STRUP_AUXADC_RSTB_SEL; */
		ret = pmic_config_interface(0x50, 0x1, 0x1, 0);	/* [0:0]: STRUP_PWROFF_SEQ_EN; */
		ret = pmic_config_interface(0x50, 0x1, 0x1, 1);	/* [1:1]: STRUP_PWROFF_PREOFF_EN; */
		ret = pmic_config_interface(0x52, 0x1, 0x1, 9);	/* [9:9]: SPK_THER_SHDN_L_EN; */
		ret = pmic_config_interface(0x56, 0x1, 0x1, 0);	/* [0:0]: RG_SPK_INTG_RST_L; */
		ret = pmic_config_interface(0x64, 0x1, 0xF, 8);	/* [11:8]: RG_SPKPGA_GAIN; */
		ret = pmic_config_interface(0x102, 0x1, 0x1, 6);	/* [6:6]: RG_RTC_75K_CK_PDN; */
		ret = pmic_config_interface(0x102, 0x0, 0x1, 11);	/* [11:11]: RG_DRV_32K_CK_PDN; */
		ret = pmic_config_interface(0x102, 0x1, 0x1, 15);	/* [15:15]: RG_BUCK32K_PDN; */
		ret = pmic_config_interface(0x108, 0x1, 0x1, 12);	/* [12:12]: RG_EFUSE_CK_PDN; */
		ret = pmic_config_interface(0x10E, 0x1, 0x1, 5);	/* [5:5]: RG_AUXADC_CTL_CK_PDN; */
		ret = pmic_config_interface(0x120, 0x1, 0x1, 4);	/* [4:4]: RG_SRCLKEN_HW_MODE; */
		ret = pmic_config_interface(0x120, 0x1, 0x1, 5);	/* [5:5]: RG_OSC_HW_MODE; */
		ret = pmic_config_interface(0x148, 0x1, 0x1, 1);	/* [1:1]: RG_SMT_INT; */
		ret = pmic_config_interface(0x148, 0x1, 0x1, 3);	/* [3:3]: RG_SMT_RTC_32K1V8; */
		ret = pmic_config_interface(0x160, 0x1, 0x1, 2);	/* [2:2]: RG_INT_EN_BAT_L; */
		ret = pmic_config_interface(0x160, 0x1, 0x1, 6);	/* [6:6]: RG_INT_EN_THR_L; */
		ret = pmic_config_interface(0x160, 0x1, 0x1, 7);	/* [7:7]: RG_INT_EN_THR_H; */
		ret = pmic_config_interface(0x166, 0x1, 0x1, 1);	/* [1:1]: RG_INT_EN_FCHRKEY; */
		ret = pmic_config_interface(0x212, 0x2, 0x3, 4);	/* [5:4]: QI_VPROC_VSLEEP; */
		ret = pmic_config_interface(0x216, 0x1, 0x1, 1);	/* [1:1]: VPROC_VOSEL_CTRL; */
		ret = pmic_config_interface(0x21C, 0x17, 0x7F, 0);	/* [6:0]: VPROC_SFCHG_FRATE; */
		ret = pmic_config_interface(0x21C, 0x1, 0x1, 7);	/* [7:7]: VPROC_SFCHG_FEN; */
		ret = pmic_config_interface(0x21C, 0x1, 0x1, 15);	/* [15:15]: VPROC_SFCHG_REN; */
		ret = pmic_config_interface(0x222, 0x18, 0x7F, 0);	/* [6:0]: VPROC_VOSEL_SLEEP; */
		ret = pmic_config_interface(0x230, 0x3, 0x3, 0);	/* [1:0]: VPROC_TRANSTD; */
		ret = pmic_config_interface(0x230, 0x1, 0x1, 8);	/* [8:8]: VPROC_VSLEEP_EN; */
		ret = pmic_config_interface(0x238, 0x3, 0x3, 0);	/* [1:0]: RG_VSYS_SLP; */
		ret = pmic_config_interface(0x238, 0x2, 0x3, 4);	/* [5:4]: QI_VSYS_VSLEEP; */
		ret = pmic_config_interface(0x23C, 0x1, 0x1, 1);	/* [1:1]: VSYS_VOSEL_CTRL; after 0x0256 */
		ret = pmic_config_interface(0x242, 0x23, 0x7F, 0);	/* [6:0]: VSYS_SFCHG_FRATE; */
		ret = pmic_config_interface(0x242, 0x11, 0x7F, 8);	/* [14:8]: VSYS_SFCHG_RRATE; */
		ret = pmic_config_interface(0x242, 0x1, 0x1, 15);	/* [15:15]: VSYS_SFCHG_REN; */
		ret = pmic_config_interface(0x256, 0x3, 0x3, 0);	/* [1:0]: VSYS_TRANSTD; */
		ret = pmic_config_interface(0x256, 0x1, 0x3, 4);	/* [5:4]: VSYS_VOSEL_TRANS_EN; */
		ret = pmic_config_interface(0x256, 0x1, 0x1, 8);	/* [8:8]: VSYS_VSLEEP_EN; */
		ret = pmic_config_interface(0x302, 0x2, 0x3, 8);	/* [9:8]: RG_VPA_CSL; OC limit */
		ret = pmic_config_interface(0x302, 0x1, 0x3, 14);	/* [15:14]: RG_VPA_ZX_OS; ZX limit */
		ret = pmic_config_interface(0x310, 0x1, 0x1, 7);	/* [7:7]: VPA_SFCHG_FEN; */
		ret = pmic_config_interface(0x310, 0x1, 0x1, 15);	/* [15:15]: VPA_SFCHG_REN; */
		ret = pmic_config_interface(0x326, 0x1, 0x1, 0);	/* [0:0]: VPA_DLC_MAP_EN; */
		ret = pmic_config_interface(0x402, 0x1, 0x1, 0);	/* [0:0]: VTCXO_LP_SEL; */
		ret = pmic_config_interface(0x402, 0x0, 0x1, 11);	/* [11:11]: VTCXO_ON_CTRL; */
		ret = pmic_config_interface(0x404, 0x1, 0x1, 0);	/* [0:0]: VA_LP_SEL; */
		ret = pmic_config_interface(0x404, 0x2, 0x3, 8);	/* [9:8]: RG_VA_SENSE_SEL; */
		ret = pmic_config_interface(0x500, 0x1, 0x1, 0);	/* [0:0]: VIO28_LP_SEL; */
		ret = pmic_config_interface(0x502, 0x1, 0x1, 0);	/* [0:0]: VUSB_LP_SEL; */
		ret = pmic_config_interface(0x504, 0x1, 0x1, 0);	/* [0:0]: VMC_LP_SEL; */
		ret = pmic_config_interface(0x506, 0x1, 0x1, 0);	/* [0:0]: VMCH_LP_SEL; */
		ret = pmic_config_interface(0x508, 0x1, 0x1, 0);	/* [0:0]: VEMC_3V3_LP_SEL; */
		ret = pmic_config_interface(0x514, 0x1, 0x1, 0);	/* [0:0]: RG_STB_SIM1_SIO; */
		ret = pmic_config_interface(0x516, 0x1, 0x1, 0);	/* [0:0]: VSIM1_LP_SEL; */
		ret = pmic_config_interface(0x518, 0x1, 0x1, 0);	/* [0:0]: VSIM2_LP_SEL; */
		ret = pmic_config_interface(0x524, 0x1, 0x1, 0);	/* [0:0]: RG_STB_SIM2_SIO; */
		ret = pmic_config_interface(0x542, 0x1, 0x1, 2);	/* [2:2]: VIBR_THER_SHEN_EN; */
		ret = pmic_config_interface(0x54E, 0x1, 0x1, 15);	/* [15:15]: RG_VRF18_EN; */
		/* [1:1]: VRF18_ON_CTRL; */
		ret = pmic_config_interface(0x550, 0x0, 0x1, 1);
		ret = pmic_config_interface(0x552, 0x1, 0x1, 0);	/* [0:0]: VM_LP_SEL; */
		ret = pmic_config_interface(0x552, 0x1, 0x1, 14);	/* [14:14]: RG_VM_EN; */
		ret = pmic_config_interface(0x556, 0x1, 0x1, 0);	/* [0:0]: VIO18_LP_SEL; */
		ret = pmic_config_interface(0x756, 0x1, 0x3, 2);	/* [3:2]: RG_ADC_TRIM_CH6_SEL; */
		ret = pmic_config_interface(0x756, 0x1, 0x3, 4);	/* [5:4]: RG_ADC_TRIM_CH5_SEL; */
		ret = pmic_config_interface(0x756, 0x1, 0x3, 8);	/* [9:8]: RG_ADC_TRIM_CH3_SEL; */
		ret = pmic_config_interface(0x756, 0x1, 0x3, 10);	/* [11:10]: RG_ADC_TRIM_CH2_SEL; */
		ret = pmic_config_interface(0x778, 0x1, 0x1, 15);	/* [15:15]: RG_VREF18_ENB_MD; */
#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXT
		/* Adjust the CAL Register adding 0.16v more on VGP2 to be 3.16v for ATMEL mxT1066T2 voltage requirement */
		ret = pmic_config_interface(0x532, 0x8, 0xF, 8);	/* [11:8]: RG_VGP2_CAL; */
#endif

	} else if (chip_version >= PMIC6323_E1_CID_CODE) {
		pr_notice("[Kernel_PMIC_INIT_SETTING_V1] PMIC Chip = %x\n", chip_version);
		pr_notice("[Kernel_PMIC_INIT_SETTING_V1] 20130328_0.85v\n");

		/* put init setting from DE/SA */

		ret = pmic_config_interface(0x2, 0xB, 0xF, 4);	/* [7:4]: RG_VCDT_HV_VTH; 7V OVP */
		ret = pmic_config_interface(0xC, 0x3, 0x7, 1);	/* [3:1]: RG_VBAT_OV_VTH; VBAT_OV=4.45V */
		ret = pmic_config_interface(0x1A, 0x3, 0xF, 0);	/* [3:0]: RG_CHRWDT_TD; align to 6250's */
		ret = pmic_config_interface(0x24, 0x1, 0x1, 1);	/* [1:1]: RG_BC11_RST; Reset BC11 detection */
		ret = pmic_config_interface(0x2A, 0x0, 0x7, 4);	/* [6:4]: RG_CSDAC_STP; align to 6250's setting */
		ret = pmic_config_interface(0x2E, 0x1, 0x1, 2);	/* [2:2]: RG_CSDAC_MODE; */
		ret = pmic_config_interface(0x2E, 0x1, 0x1, 6);	/* [6:6]: RG_HWCV_EN; */
		ret = pmic_config_interface(0x2E, 0x1, 0x1, 7);	/* [7:7]: RG_ULC_DET_EN; */
		ret = pmic_config_interface(0x3C, 0x1, 0x1, 5);	/* [5:5]: THR_HWPDN_EN; */
		ret = pmic_config_interface(0x40, 0x1, 0x1, 4);	/* [4:4]: RG_EN_DRVSEL; */
		ret = pmic_config_interface(0x40, 0x1, 0x1, 5);	/* [5:5]: RG_RST_DRVSEL; */
		ret = pmic_config_interface(0x46, 0x1, 0x1, 1);	/* [1:1]: PWRBB_DEB_EN; */
		ret = pmic_config_interface(0x48, 0x1, 0x1, 8);	/* [8:8]: VPROC_PG_H2L_EN; */
		ret = pmic_config_interface(0x48, 0x1, 0x1, 9);	/* [9:9]: VSYS_PG_H2L_EN; */
		ret = pmic_config_interface(0x4E, 0x1, 0x1, 5);	/* [5:5]: STRUP_AUXADC_RSTB_SW; */
		ret = pmic_config_interface(0x4E, 0x1, 0x1, 7);	/* [7:7]: STRUP_AUXADC_RSTB_SEL; */
		ret = pmic_config_interface(0x50, 0x1, 0x1, 0);	/* [0:0]: STRUP_PWROFF_SEQ_EN; */
		ret = pmic_config_interface(0x50, 0x1, 0x1, 1);	/* [1:1]: STRUP_PWROFF_PREOFF_EN; */
		ret = pmic_config_interface(0x52, 0x1, 0x1, 9);	/* [9:9]: SPK_THER_SHDN_L_EN; */
		ret = pmic_config_interface(0x56, 0x1, 0x1, 0);	/* [0:0]: RG_SPK_INTG_RST_L; */
		ret = pmic_config_interface(0x64, 0x1, 0xF, 8);	/* [11:8]: RG_SPKPGA_GAIN; */
		ret = pmic_config_interface(0x102, 0x1, 0x1, 6);	/* [6:6]: RG_RTC_75K_CK_PDN; */
		ret = pmic_config_interface(0x102, 0x0, 0x1, 11);	/* [11:11]: RG_DRV_32K_CK_PDN; */
		ret = pmic_config_interface(0x102, 0x1, 0x1, 15);	/* [15:15]: RG_BUCK32K_PDN; */
		ret = pmic_config_interface(0x108, 0x1, 0x1, 12);	/* [12:12]: RG_EFUSE_CK_PDN; */
		ret = pmic_config_interface(0x10E, 0x1, 0x1, 5);	/* [5:5]: RG_AUXADC_CTL_CK_PDN; */
		ret = pmic_config_interface(0x120, 0x1, 0x1, 4);	/* [4:4]: RG_SRCLKEN_HW_MODE; */
		ret = pmic_config_interface(0x120, 0x1, 0x1, 5);	/* [5:5]: RG_OSC_HW_MODE; */
		ret = pmic_config_interface(0x148, 0x1, 0x1, 1);	/* [1:1]: RG_SMT_INT; */
		ret = pmic_config_interface(0x148, 0x1, 0x1, 3);	/* [3:3]: RG_SMT_RTC_32K1V8; */
		ret = pmic_config_interface(0x160, 0x1, 0x1, 2);	/* [2:2]: RG_INT_EN_BAT_L; */
		ret = pmic_config_interface(0x160, 0x1, 0x1, 6);	/* [6:6]: RG_INT_EN_THR_L; */
		ret = pmic_config_interface(0x160, 0x1, 0x1, 7);	/* [7:7]: RG_INT_EN_THR_H; */
		ret = pmic_config_interface(0x166, 0x1, 0x1, 1);	/* [1:1]: RG_INT_EN_FCHRKEY; */
		ret = pmic_config_interface(0x212, 0x2, 0x3, 4);	/* [5:4]: QI_VPROC_VSLEEP; */
		ret = pmic_config_interface(0x216, 0x1, 0x1, 1);	/* [1:1]: VPROC_VOSEL_CTRL; */
		ret = pmic_config_interface(0x21C, 0x17, 0x7F, 0);	/* [6:0]: VPROC_SFCHG_FRATE; */
		ret = pmic_config_interface(0x21C, 0x1, 0x1, 7);	/* [7:7]: VPROC_SFCHG_FEN; */
		ret = pmic_config_interface(0x21C, 0x1, 0x1, 15);	/* [15:15]: VPROC_SFCHG_REN; */
		ret = pmic_config_interface(0x222, 0x18, 0x7F, 0);	/* [6:0]: VPROC_VOSEL_SLEEP; */
		ret = pmic_config_interface(0x230, 0x3, 0x3, 0);	/* [1:0]: VPROC_TRANSTD; */
		ret = pmic_config_interface(0x230, 0x1, 0x1, 8);	/* [8:8]: VPROC_VSLEEP_EN; */
		ret = pmic_config_interface(0x238, 0x3, 0x3, 0);	/* [1:0]: RG_VSYS_SLP; */
		ret = pmic_config_interface(0x238, 0x2, 0x3, 4);	/* [5:4]: QI_VSYS_VSLEEP; */
		ret = pmic_config_interface(0x23C, 0x1, 0x1, 1);	/* [1:1]: VSYS_VOSEL_CTRL; after 0x0256 */
		ret = pmic_config_interface(0x242, 0x23, 0x7F, 0);	/* [6:0]: VSYS_SFCHG_FRATE; */
		ret = pmic_config_interface(0x242, 0x11, 0x7F, 8);	/* [14:8]: VSYS_SFCHG_RRATE; */
		ret = pmic_config_interface(0x242, 0x1, 0x1, 15);	/* [15:15]: VSYS_SFCHG_REN; */
		ret = pmic_config_interface(0x256, 0x3, 0x3, 0);	/* [1:0]: VSYS_TRANSTD; */
		ret = pmic_config_interface(0x256, 0x1, 0x3, 4);	/* [5:4]: VSYS_VOSEL_TRANS_EN; */
		ret = pmic_config_interface(0x256, 0x1, 0x1, 8);	/* [8:8]: VSYS_VSLEEP_EN; */
		ret = pmic_config_interface(0x302, 0x2, 0x3, 8);	/* [9:8]: RG_VPA_CSL; OC limit */
		ret = pmic_config_interface(0x302, 0x1, 0x3, 14);	/* [15:14]: RG_VPA_ZX_OS; ZX limit */
		ret = pmic_config_interface(0x310, 0x1, 0x1, 7);	/* [7:7]: VPA_SFCHG_FEN; */
		ret = pmic_config_interface(0x310, 0x1, 0x1, 15);	/* [15:15]: VPA_SFCHG_REN; */
		ret = pmic_config_interface(0x326, 0x1, 0x1, 0);	/* [0:0]: VPA_DLC_MAP_EN; */
		ret = pmic_config_interface(0x402, 0x1, 0x1, 0);	/* [0:0]: VTCXO_LP_SEL; */
		ret = pmic_config_interface(0x402, 0x0, 0x1, 11);	/* [11:11]: VTCXO_ON_CTRL; */
		ret = pmic_config_interface(0x404, 0x1, 0x1, 0);	/* [0:0]: VA_LP_SEL; */
		ret = pmic_config_interface(0x404, 0x2, 0x3, 8);	/* [9:8]: RG_VA_SENSE_SEL; */
		ret = pmic_config_interface(0x500, 0x1, 0x1, 0);	/* [0:0]: VIO28_LP_SEL; */
		ret = pmic_config_interface(0x502, 0x1, 0x1, 0);	/* [0:0]: VUSB_LP_SEL; */
		ret = pmic_config_interface(0x504, 0x1, 0x1, 0);	/* [0:0]: VMC_LP_SEL; */
		ret = pmic_config_interface(0x506, 0x1, 0x1, 0);	/* [0:0]: VMCH_LP_SEL; */
		ret = pmic_config_interface(0x508, 0x1, 0x1, 0);	/* [0:0]: VEMC_3V3_LP_SEL; */
		ret = pmic_config_interface(0x514, 0x1, 0x1, 0);	/* [0:0]: RG_STB_SIM1_SIO; */
		ret = pmic_config_interface(0x516, 0x1, 0x1, 0);	/* [0:0]: VSIM1_LP_SEL; */
		ret = pmic_config_interface(0x518, 0x1, 0x1, 0);	/* [0:0]: VSIM2_LP_SEL; */
		ret = pmic_config_interface(0x524, 0x1, 0x1, 0);	/* [0:0]: RG_STB_SIM2_SIO; */
		ret = pmic_config_interface(0x542, 0x1, 0x1, 2);	/* [2:2]: VIBR_THER_SHEN_EN; */
		ret = pmic_config_interface(0x54E, 0x1, 0x1, 15);	/* [15:15]: RG_VRF18_EN; */
		/* [1:1]: VRF18_ON_CTRL; */
		ret = pmic_config_interface(0x550, 0x0, 0x1, 1);
		ret = pmic_config_interface(0x552, 0x1, 0x1, 0);	/* [0:0]: VM_LP_SEL; */
		ret = pmic_config_interface(0x552, 0x1, 0x1, 14);	/* [14:14]: RG_VM_EN; */
		ret = pmic_config_interface(0x556, 0x1, 0x1, 0);	/* [0:0]: VIO18_LP_SEL; */
		ret = pmic_config_interface(0x756, 0x1, 0x3, 2);	/* [3:2]: RG_ADC_TRIM_CH6_SEL; */
		ret = pmic_config_interface(0x756, 0x1, 0x3, 4);	/* [5:4]: RG_ADC_TRIM_CH5_SEL; */
		ret = pmic_config_interface(0x756, 0x1, 0x3, 8);	/* [9:8]: RG_ADC_TRIM_CH3_SEL; */
		ret = pmic_config_interface(0x756, 0x1, 0x3, 10);	/* [11:10]: RG_ADC_TRIM_CH2_SEL; */
		ret = pmic_config_interface(0x778, 0x1, 0x1, 15);	/* [15:15]: RG_VREF18_ENB_MD; */

	} else {
		pr_notice("[Kernel_PMIC_INIT_SETTING_V1] Unknown PMIC Chip (%x)\n", chip_version);
	}


	upmu_set_rg_adc_gps_status(1);
	upmu_set_rg_adc_md_status(1);
	upmu_set_rg_deci_gdly_vref18_selb(1);
	upmu_set_rg_deci_gdly_sel_mode(1);
	upmu_set_rg_osr(3);

	upmu_set_rg_chrwdt_en(0);
	upmu_set_rg_chrwdt_int_en(0);
	upmu_set_rg_chrwdt_wr(0);
	upmu_set_rg_chrwdt_flag_wr(1);
}

void pmic_setting_depends_rtc(void)
{
#ifdef CONFIG_MTK_RTC
	u32 ret = 0;

	if (crystal_exist_status()) {
		/* with 32K */
		ret = pmic_config_interface(ANALDO_CON1, 1, 0x1, 11);	/* [11]=1(VTCXO_ON_CTRL), */
		ret = pmic_config_interface(ANALDO_CON1, 0, 0x1, 0);	/* [0] =0(VTCXO_LP_SEL), */

		pr_notice("[pmic_setting_depends_rtc] With 32K. Reg[0x%x]=0x%x\n",
			  ANALDO_CON1, upmu_get_reg_value(ANALDO_CON1));
	} else {
		/* without 32K */
		ret = pmic_config_interface(ANALDO_CON1, 0, 0x1, 11);	/* [11]=0(VTCXO_ON_CTRL), */
		ret = pmic_config_interface(ANALDO_CON1, 1, 0x1, 0);	/* [0] =1(VTCXO_LP_SEL), */

		pr_notice("[pmic_setting_depends_rtc] Without 32K. Reg[0x%x]=0x%x\n",
			  ANALDO_CON1, upmu_get_reg_value(ANALDO_CON1));
	}
#else
	pr_notice("[pmic_setting_depends_rtc] no define CONFIG_MTK_RTC\n");
#endif
}

static int pmic_mt6323_probe(struct platform_device *dev)
{
	int ret_val = 0;
	struct mt6323_chip_priv *chip;
	struct mt6323_chip *mt6323 = dev_get_drvdata(dev->dev.parent);
	struct device_node *np = dev->dev.of_node;

	/* get PMIC CID */
	ret_val = upmu_get_cid();
	pr_notice("Power/PMIC MT6323 PMIC CID=0x%x\n", ret_val);

	/* pmic initial setting */
	PMIC_INIT_SETTING_V1();

	chip = kzalloc(sizeof(struct mt6323_chip_priv), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &dev->dev;
	mt6323->irq = platform_get_irq(dev, 0);

	if (mt6323->irq <= 0) {
		dev_err(&dev->dev, "failed to get irq: %d\n", mt6323->irq);
		ret_val = -EINVAL;
		goto err_free;
	}
	chip->irq = mt6323->irq; /* hw irq of EINT */
	chip->irq_hw_id = (int)irqd_to_hwirq(irq_get_irq_data(mt6323->irq)); /* EINT num */

	chip->num_int = MT6323_IRQ_NR;
	chip->int_con[0] = INT_CON0;
	chip->int_con[1] = INT_CON1;
	chip->int_stat[0] = INT_STATUS0;
	chip->int_stat[1] = INT_STATUS1;

	chip->shutdown_time = LONG_PRESS_PWRKEY_SHUTDOWN_TIME;
	of_property_read_u32(np, "long-press-shutdown-time", (u32 *)&chip->shutdown_time);
	pr_info("long press shutdown time is %ds\n", chip->shutdown_time);
	dev_set_drvdata(chip->dev, chip);

	wake_lock_init(&chip->irq_lock, WAKE_LOCK_SUSPEND, "pmic irq wakelock");
	device_init_wakeup(chip->dev, true);

	/* Disable bvalid since we don't use it */
	upmu_set_rg_otg_bvalid_en(0);

	ret_val = mt6323_irq_init(chip);
	if (ret_val)
		goto err_free_domain;

	mt6323_irq_handler_init(chip);

	pmic_config_interface(0x402, 0x0, 0x1, 0);	/* [0:0]: VTCXO_LP_SEL; */
	pmic_config_interface(0x402, 0x1, 0x1, 11);	/* [11:11]: VTCXO_ON_CTRL; */

	hrtimer_init(&chip->long_press_pwrkey_shutdown_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	chip->long_press_pwrkey_shutdown_timer.function = long_press_pwrkey_shutdown_timer_func;

	hrtimer_init(&chip->check_pwrkey_release_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	chip->check_pwrkey_release_timer.function = check_pwrkey_release_timer_func;
#ifdef CONFIG_AMAZON_METRICS_LOG
	INIT_WORK(&metrics_work, pwrkey_log_to_metrics);
#endif

	atomic_set(&pmic_suspend, 0);

	device_create_file(&(dev->dev), &dev_attr_pmic_access);
	return 0;

err_free_domain:
	irq_domain_remove(chip->domain);
err_free:
	kfree(chip);
	return ret_val;
}

static int pmic_mt6323_suspend(struct platform_device *dev, pm_message_t state)
{
	struct mt6323_chip_priv *chip = dev_get_drvdata(&dev->dev);

	atomic_set(&pmic_suspend, 1);

	upmu_set_rg_vref18_enb(1);
	upmu_set_rg_adc_deci_gdly(1);
	upmu_set_rg_clksq_en_aux(1);
	upmu_set_rg_aud26m_div4_ck_pdn(0);
	upmu_set_rg_auxadc_sdm_sel_hw_mode(1);
	upmu_set_rg_auxadc_sdm_ck_hw_mode(1);
	upmu_set_rg_auxadc_sdm_ck_sel(0);
	upmu_set_rg_auxadc_sdm_ck_pdn(0);
	upmu_set_rg_auxadc_sdm_ck_wake_pdn(0);

	mt6323_irq_chip_suspend(chip);

	return 0;
}

static int pmic_mt6323_resume(struct platform_device *dev)
{
	struct mt6323_chip_priv *chip = dev_get_drvdata(&dev->dev);

	mt6323_irq_chip_resume(chip);

	upmu_set_rg_vref18_enb(0);
	/* upmu_set_rg_adc_deci_gdly(0); */
	upmu_set_rg_clksq_en_aux(1);
	upmu_set_rg_aud26m_div4_ck_pdn(0);
	upmu_set_rg_auxadc_sdm_sel_hw_mode(0);
	upmu_set_rg_auxadc_sdm_ck_hw_mode(1);
	upmu_set_rg_auxadc_sdm_ck_sel(0);
	upmu_set_rg_auxadc_sdm_ck_pdn(0);
	upmu_set_rg_auxadc_sdm_ck_wake_pdn(1);

	atomic_set(&pmic_suspend, 0);

	return 0;
}
#ifdef CONFIG_OF
static const struct platform_device_id mt6323_id[] = {
	{"mt6323-pmic", 0},
	{},
};
#endif
static struct platform_driver pmic_mt6323_driver = {
	.probe = pmic_mt6323_probe,
	/* #ifdef CONFIG_PM */
	.suspend = pmic_mt6323_suspend,
	.resume = pmic_mt6323_resume,
	/* #endif */
	.driver = {
		   .name = "mt6323-pmic",
	},
#ifdef CONFIG_OF
	.id_table = mt6323_id,
#endif
};

static int __init pmic_mt6323_init(void)
{
	int ret;

	wake_lock_init(&pmicAuxadc_irq_lock, WAKE_LOCK_SUSPEND, "pmicAuxadc irq wakelock");

	ret = platform_driver_register(&pmic_mt6323_driver);
	if (ret) {
		pr_err("%s: Unable to register driver (%d)\n", __func__, ret);
		return ret;
	}

	return 0;
}

static void __exit pmic_mt6323_exit(void)
{
}
fs_initcall(pmic_mt6323_init);
module_exit(pmic_mt6323_exit);

MODULE_AUTHOR("James Lo");
MODULE_DESCRIPTION("MT6323 PMIC Device Driver");
MODULE_LICENSE("GPL");
