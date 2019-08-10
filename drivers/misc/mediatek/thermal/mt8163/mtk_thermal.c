#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <mt-plat/sync_write.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include "mach/mt_typedefs.h"
#include "mach/mt_thermal.h"
#include <mach/wd_api.h>
#include <linux/time.h>
#include <linux/uidgid.h>

#include "inc/mtk_ts_cpu.h"

#ifdef CONFIG_AMAZON_SIGN_OF_LIFE
#include <linux/sign_of_life.h>
#endif

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#define __MT_MTK_TS_CPU_C__

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#define TSCPU_METRICS_STR_LEN 128
#endif

#include <linux/thermal_framework.h>
#include <linux/platform_data/mtk_thermal.h>

#ifdef CONFIG_OF

u32 thermal_irq_number = 0;
void __iomem *thermal_base;
void __iomem *auxadc_ts_base;
void __iomem *apmixed_base;
void __iomem *INFRACFG_AO_BASE;

int thermal_phy_base;
int auxadc_ts_phy_base;
int apmixed_phy_base;
int pericfg_phy_base;

struct clk *clk_peri_therm;
struct clk *clk_auxadc;
#endif

struct tscpu_thermal_zone {
	struct thermal_zone_device *tz;
	struct work_struct thermal_work;
	struct mtk_thermal_platform_data *pdata;
	struct thermal_dev *thermal_fw;
};

/* Workaround, it will remove after PTP driver ready. */
#if 1
/*
 * lock
 */
static DEFINE_SPINLOCK(ptp_spinlock);

void mt_ptp_lock(unsigned long *x)
{
	spin_lock_irqsave(&ptp_spinlock, *x);
	/*return 0;*/
};
EXPORT_SYMBOL(mt_ptp_lock);

void mt_ptp_unlock(unsigned long *x)
{
	spin_unlock_irqrestore(&ptp_spinlock, *x);
	/*return 0;*/
};
EXPORT_SYMBOL(mt_ptp_unlock);
#endif


/*==============*/
/*Configurations*/
/*==============*/

/*=============================================================
 * Macro definition
 *=============================================================*/

/*
 * CONFIG (SW related)
 */
#define THERMAL_TURNOFF_AUXADC_BEFORE_DEEPIDLE    (1)
#define THERMAL_PERFORMANCE_PROFILE         (0)
/* 1: turn on adaptive AP cooler; 0: turn off */
#define CPT_ADAPTIVE_AP_COOLER              (0)
/* 1: turn on supports to MET logging; 0: turn off */
#define CONFIG_SUPPORT_MET_MTKTSCPU         (0)
/* Thermal controller HW filtering function.
 * Only 1, 2, 4, 8, 16 are valid values, they means one reading is a avg ofi
 * X samples
 */
#define THERMAL_CONTROLLER_HW_FILTER        (1)	/* 1, 2, 4, 8, 16 */
/* 1: turn on thermal controller HW thermal protection; 0: turn off */
#define THERMAL_CONTROLLER_HW_TP            (1)
/* 1: turn on SW filtering in this sw module; 0: turn off */
#define MTK_TS_CPU_SW_FILTER                (1)
/* 1: turn on fast polling in this sw module; 0: turn off */
#define MTKTSCPU_FAST_POLLING               (0)

#if CPT_ADAPTIVE_AP_COOLER
#define MAX_CPT_ADAPTIVE_COOLERS            (3)
#define THERMAL_HEADROOM                    (1)
#endif

struct proc_dir_entry *mtk_thermal_get_proc_drv_therm_dir_entry(void);
static void tscpu_fast_initial_sw_workaround(void);

/* 1: thermal driver fast polling, use hrtimer; 0: turn off */
/* #define THERMAL_DRV_FAST_POLL_HRTIMER          (1) */

/* 1: thermal driver update temp to MET directly, use hrtimer; 0: turn off */
#define THERMAL_DRV_UPDATE_TEMP_DIRECT_TO_MET  (1)
#define MIN(_a_, _b_) ((_a_) > (_b_) ? (_b_) : (_a_))
#define MAX(_a_, _b_) ((_a_) > (_b_) ? (_a_) : (_b_))

/*=============================================================
 *REG ACCESS
 *=============================================================*/
#define thermal_readl(addr)         DRV_Reg32(addr)
#define thermal_writel(addr, val)   mt_reg_sync_writel((val), ((void *)addr))
#define thermal_setl(addr, val)     mt_reg_sync_writel(thermal_readl(addr) \
						       | (val), ((void *)addr))
#define thermal_clrl(addr, val)     mt_reg_sync_writel(thermal_readl(addr) \
						       & ~(val), ((void *)addr))

/*=============================================================
 *Local variable definition
 *=============================================================*/
#if MTKTSCPU_FAST_POLLING
/* Combined fast_polling_trip_temp and fast_polling_factor,
it means polling_delay will be 1/5 of original interval after
mtktscpu reports > 65C w/o exit point */
static int fast_polling_trip_temp = 60000;
static int fast_polling_factor = 5;
static int cur_fp_factor = 1;
static int next_fp_factor = 1;
#endif

static int g_max_temp = 50000;	/* default=50 deg */

static unsigned int interval = 1000;	/* mseconds, 0 : no auto polling */
/* trip_temp[0] must be initialized to the thermal HW protection point. */
static int trip_temp[10] = {
	117000, 100000, 85000, 75000, 65000, 55000, 45000, 35000, 25000, 15000
};
static struct thermal_zone_device *thz_dev;
/* -ASC- */
static int mtktscpu_debug_log;
static int num_trip = 5;
int MA_len_temp = 0;

#define CPU_COOLER_NUM 34

static DEFINE_MUTEX(TS_lock);

static int read_curr_temp;

/**
 * If curr_temp >= polling_trip_temp1, use interval
 * else if cur_temp >= polling_trip_temp2 && curr_temp < polling_trip_temp1,
 * use interval*polling_factor1
 * else, use interval*polling_factor2
 */
static int polling_trip_temp1 = 40000;
static int polling_trip_temp2 = 20000;
static int polling_factor1 = 2;
static int polling_factor2 = 4;

#define MTKTSCPU_TEMP_CRIT 120000	/* 120.000 degree Celsius */

static int tc_mid_trip = -275000;

/*
 * Bank0 : CPU (TS_MCU1)(TS1)
 * Bank1 : GPU (TS_MCU2)(TS2)
 * Bank2 : SOC (TS_MCU3)(TS13)
 * TS_MCU1: TS3 (9464.54,65.8)
 * TS_MCU2: TS4 (6745.06,2790.2)
 * TS_MCU3: TS5 (5673.50,6392.4)
 * TS_MCU4: TS1 (3349.22,4163.6)
 * TS_ABB:  TS2
 */

static int CPU_TS_MCU1_T = 0, CPU_TS_MCU2_T;
static int GPU_TS_MCU1_T = 0, GPU_TS_MCU2_T;
static int SOC_TS_MCU1_T = 0, SOC_TS_MCU2_T = 0, SOC_TS_MCU3_T;
#if 1
static int CPU_TS_MCU1_R = 0, CPU_TS_MCU2_R;
static int GPU_TS_MCU1_R = 0, GPU_TS_MCU2_R;
static int SOC_TS_MCU1_R = 0, SOC_TS_MCU2_R = 0, SOC_TS_MCU3_R;
#endif

int last_abb_t = 0;
int last_CPU1_t = 0;
int last_CPU2_t = 0;

static int g_tc_resume;		/* default=0,read temp */

static S32 g_adc_ge_t;
static S32 g_adc_oe_t;
static S32 g_o_vtsmcu1;
static S32 g_o_vtsmcu2;
static S32 g_o_vtsmcu3;
static S32 g_o_vtsmcu4;
static S32 g_o_vtsabb;
static S32 g_degc_cali;
static S32 g_adc_cali_en_t;
static S32 g_o_slope;
static S32 g_o_slope_sign;
static S32 g_id;

static S32 g_ge = 1;
static S32 g_oe = 1;
static S32 g_gain = 1;

static S32 g_x_roomt[THERMAL_SENSOR_NUM] = { 0 };

#define y_curr_repeat_times 1
#define THERMAL_NAME    "mt8163-thermal"

static struct mtk_cpu_power_info *mtk_cpu_power;
static int tscpu_num_opp;

static int tscpu_cpu_dmips[CPU_COOLER_NUM] = { 0 };

int mtktscpu_limited_dmips = 0;

static bool talking_flag;

#define TS_MS_TO_NS(x) (x * 1000 * 1000)
static struct hrtimer ts_tempinfo_hrtimer;

/*=============================================================
 *LOG
 *=============================================================*/

#define tscpu_dprintk(fmt, args...)   \
	do {                                    \
		if (mtktscpu_debug_log) {                \
			pr_debug("Power/CPU_Thermal" fmt, ##args); \
		}                                   \
	} while (0)

#define tscpu_printk(fmt, args...) pr_debug("Power/CPU_Thermal" fmt, ##args)

/*=============================================================
 * Local function declartation
 *=============================================================*/
static int mtktscpu_switch_bank(enum thermal_bank_name bank);
static void tscpu_reset_thermal(void);
static S32 temperature_to_raw_room(U32 ret);
static void set_tc_trigger_hw_protect(int temperature, int temperature2);
static void tscpu_config_all_tc_hw_protect(int temperature, int temperature2);
static void thermal_initial_all_bank(void);
static void read_each_bank_TS(enum thermal_bank_name bank_num);

static int thermal_fast_init(void);

static void tscpu_update_temperature_timer_init(void);

bool __attribute__ ((weak))
mtk_get_gpu_loading(unsigned int *pLoading)
{
	return 0;
}

void __attribute__ ((weak))
mt_cpufreq_thermal_protect(unsigned int limited_power)
{
}

void __attribute__ ((weak))
mt_gpufreq_thermal_protect(unsigned int limited_power)
{
}

unsigned int __attribute__ ((weak))
mt_gpufreq_get_cur_freq(void)
{
	return 0;
}

/*=============================================================
 * Local type definition
 *=============================================================*/
struct mtk_cpu_power_info {
	unsigned int cpufreq_khz;
	unsigned int cpufreq_ncpu;
	unsigned int cpufreq_power;
};
struct mtk_gpu_power_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_power;
};

/*=============================================================
 * Local function definition
 *=============================================================*/
#if THERMAL_DRV_UPDATE_TEMP_DIRECT_TO_MET
static int a_tscpu_all_temp[THERMAL_SENSOR_NUM] = { 0 };

static DEFINE_MUTEX(MET_GET_TEMP_LOCK);
static met_thermalsampler_funcMET g_pThermalSampler;
void mt_thermalsampler_registerCB(met_thermalsampler_funcMET pCB)
{
	g_pThermalSampler = pCB;
}
EXPORT_SYMBOL(mt_thermalsampler_registerCB);

static DEFINE_SPINLOCK(tscpu_met_spinlock);
void tscpu_met_lock(unsigned long *flags)
{
	spin_lock_irqsave(&tscpu_met_spinlock, *flags);
}
EXPORT_SYMBOL(tscpu_met_lock);

void tscpu_met_unlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&tscpu_met_spinlock, *flags);
}
EXPORT_SYMBOL(tscpu_met_unlock);
#endif

void set_taklking_flag(bool flag)
{
	talking_flag = flag;
	tscpu_printk("talking_flag=%d\n", talking_flag);
}

void tscpu_thermal_clock_on(void)
{
	pr_debug("tscpu_thermal_clock_on\n");
	clk_prepare(clk_auxadc);
	clk_enable(clk_auxadc);
	clk_prepare(clk_peri_therm);
	clk_enable(clk_peri_therm);
}

void tscpu_thermal_clock_off(void)
{
	pr_debug("tscpu_thermal_clock_off\n");
	clk_disable(clk_peri_therm);
	clk_unprepare(clk_peri_therm);
	clk_disable(clk_auxadc);
	clk_unprepare(clk_auxadc);
}

void get_thermal_all_register(void)
{
	tscpu_dprintk("get_thermal_all_register\n");

	tscpu_dprintk("TEMPMSR1		  = 0x%8x\n", DRV_Reg32(TEMPMSR1));
	tscpu_dprintk("TEMPMSR2            = 0x%8x\n", DRV_Reg32(TEMPMSR2));

	tscpu_dprintk("TEMPMONCTL0	  = 0x%8x\n", DRV_Reg32(TEMPMONCTL0));
	tscpu_dprintk("TEMPMONCTL1	  = 0x%8x\n", DRV_Reg32(TEMPMONCTL1));
	tscpu_dprintk("TEMPMONCTL2	  = 0x%8x\n", DRV_Reg32(TEMPMONCTL2));
	tscpu_dprintk("TEMPMONINT	  = 0x%8x\n", DRV_Reg32(TEMPMONINT));
	tscpu_dprintk("TEMPMONINTSTS	  = 0x%8x\n", DRV_Reg32(TEMPMONINTSTS));
	tscpu_dprintk("TEMPMONIDET0	  = 0x%8x\n", DRV_Reg32(TEMPMONIDET0));

	tscpu_dprintk("TEMPMONIDET1	  = 0x%8x\n", DRV_Reg32(TEMPMONIDET1));
	tscpu_dprintk("TEMPMONIDET2	  = 0x%8x\n", DRV_Reg32(TEMPMONIDET2));
	tscpu_dprintk("TEMPH2NTHRE	  = 0x%8x\n", DRV_Reg32(TEMPH2NTHRE));
	tscpu_dprintk("TEMPHTHRE	  = 0x%8x\n", DRV_Reg32(TEMPHTHRE));
	tscpu_dprintk("TEMPCTHRE	  = 0x%8x\n", DRV_Reg32(TEMPCTHRE));
	tscpu_dprintk("TEMPOFFSETH	  = 0x%8x\n", DRV_Reg32(TEMPOFFSETH));

	tscpu_dprintk("TEMPOFFSETL	  = 0x%8x\n", DRV_Reg32(TEMPOFFSETL));
	tscpu_dprintk("TEMPMSRCTL0	  = 0x%8x\n", DRV_Reg32(TEMPMSRCTL0));
	tscpu_dprintk("TEMPMSRCTL1	  = 0x%8x\n", DRV_Reg32(TEMPMSRCTL1));
	tscpu_dprintk("TEMPAHBPOLL	  = 0x%8x\n", DRV_Reg32(TEMPAHBPOLL));
	tscpu_dprintk("TEMPAHBTO	  = 0x%8x\n", DRV_Reg32(TEMPAHBTO));
	tscpu_dprintk("TEMPADCPNP0	  = 0x%8x\n", DRV_Reg32(TEMPADCPNP0));

	tscpu_dprintk("TEMPADCPNP1	  = 0x%8x\n", DRV_Reg32(TEMPADCPNP1));
	tscpu_dprintk("TEMPADCPNP2	  = 0x%8x\n", DRV_Reg32(TEMPADCPNP2));
	tscpu_dprintk("TEMPADCMUX	  = 0x%8x\n", DRV_Reg32(TEMPADCMUX));
	tscpu_dprintk("TEMPADCEXT	  = 0x%8x\n", DRV_Reg32(TEMPADCEXT));
	tscpu_dprintk("TEMPADCEXT1	  = 0x%8x\n", DRV_Reg32(TEMPADCEXT1));
	tscpu_dprintk("TEMPADCEN	  = 0x%8x\n", DRV_Reg32(TEMPADCEN));


	tscpu_dprintk("TEMPPNPMUXADDR      = 0x%8x\n",
		      DRV_Reg32(TEMPPNPMUXADDR));
	tscpu_dprintk("TEMPADCMUXADDR      = 0x%8x\n",
		      DRV_Reg32(TEMPADCMUXADDR));
	tscpu_dprintk("TEMPADCEXTADDR      = 0x%8x\n",
		      DRV_Reg32(TEMPADCEXTADDR));
	tscpu_dprintk("TEMPADCEXT1ADDR     = 0x%8x\n",
		      DRV_Reg32(TEMPADCEXT1ADDR));
	tscpu_dprintk("TEMPADCENADDR       = 0x%8x\n",
		      DRV_Reg32(TEMPADCENADDR));
	tscpu_dprintk("TEMPADCVALIDADDR    = 0x%8x\n",
		      DRV_Reg32(TEMPADCVALIDADDR));

	tscpu_dprintk("TEMPADCVOLTADDR     = 0x%8x\n",
		      DRV_Reg32(TEMPADCVOLTADDR));
	tscpu_dprintk("TEMPRDCTRL          = 0x%8x\n",
		      DRV_Reg32(TEMPRDCTRL));
	tscpu_dprintk("TEMPADCVALIDMASK    = 0x%8x\n",
		      DRV_Reg32(TEMPADCVALIDMASK));
	tscpu_dprintk("TEMPADCVOLTAGESHIFT = 0x%8x\n",
		      DRV_Reg32(TEMPADCVOLTAGESHIFT));
	tscpu_dprintk("TEMPADCWRITECTRL    = 0x%8x\n",
		      DRV_Reg32(TEMPADCWRITECTRL));
	tscpu_dprintk("TEMPMSR0            = 0x%8x\n", DRV_Reg32(TEMPMSR0));


	tscpu_dprintk("TEMPIMMD0           = 0x%8x\n", DRV_Reg32(TEMPIMMD0));
	tscpu_dprintk("TEMPIMMD1           = 0x%8x\n", DRV_Reg32(TEMPIMMD1));
	tscpu_dprintk("TEMPIMMD2           = 0x%8x\n", DRV_Reg32(TEMPIMMD2));
	tscpu_dprintk("TEMPPROTCTL         = 0x%8x\n", DRV_Reg32(TEMPPROTCTL));

	tscpu_dprintk("TEMPPROTTA          = 0x%8x\n", DRV_Reg32(TEMPPROTTA));
	tscpu_dprintk("TEMPPROTTB		  = 0x%8x\n",
		      DRV_Reg32(TEMPPROTTB));
	tscpu_dprintk("TEMPPROTTC		  = 0x%8x\n",
		      DRV_Reg32(TEMPPROTTC));
	tscpu_dprintk("TEMPSPARE0		  = 0x%8x\n",
		      DRV_Reg32(TEMPSPARE0));
	tscpu_dprintk("TEMPSPARE1		  = 0x%8x\n",
		      DRV_Reg32(TEMPSPARE1));
	tscpu_dprintk("TEMPSPARE2		  = 0x%8x\n",
		      DRV_Reg32(TEMPSPARE2));
	tscpu_dprintk("TEMPSPARE3		  = 0x%8x\n",
		      DRV_Reg32(TEMPSPARE3));

}

/* TODO: FIXME */
void get_thermal_slope_intercept(struct TS_PTPOD *ts_info,
				 enum thermal_bank_name ts_bank)
{
	unsigned int temp0, temp1, temp2;
	struct TS_PTPOD ts_ptpod;
	S32 x_roomt;

	tscpu_dprintk("get_thermal_slope_intercept\n");

	switch (ts_bank) {
	case THERMAL_BANK0:	/* CPU (TS_MCU1,TS_MCU2)            (TS1,2) */
			if (CPU_TS_MCU1_T > CPU_TS_MCU2_T)
				x_roomt = g_x_roomt[0];	/* TS_MCU1 */
			else
				x_roomt = g_x_roomt[1];	/* TS_MCU2 */
			break;
	case THERMAL_BANK1:	/* GPU (TS_MCU2) (TS2) */
			x_roomt = g_x_roomt[1];	/* TS_MCU2 */
			break;
	case THERMAL_BANK2:	/* SOC (TS_MCU3)    (TS3) */
			x_roomt = g_x_roomt[2];	/* TS_MCU2 */
			break;
	default:		/* choose high temp */
			x_roomt = g_x_roomt[1];
			break;
	}

	temp0 = (10000 * 100000 / g_gain) * 15 / 18;

	if (g_o_slope_sign == 0)
		temp1 = temp0 / (165 + g_o_slope);
	else
		temp1 = temp0 / (165 - g_o_slope);
	ts_ptpod.ts_MTS = temp1;

	temp0 = (g_degc_cali * 10 / 2);
	temp1 = ((10000 * 100000 / 4096 / g_gain)
		 * g_oe + x_roomt * 10) * 15 / 18;

	if (g_o_slope_sign == 0)
		temp2 = temp1 * 10 / (165 + g_o_slope);
	else
		temp2 = temp1 * 10 / (165 - g_o_slope);

	ts_ptpod.ts_BTS = (temp0 + temp2 - 250) * 4 / 10;


	ts_info->ts_MTS = ts_ptpod.ts_MTS;
	ts_info->ts_BTS = ts_ptpod.ts_BTS;
	tscpu_printk("ts_MTS=%d, ts_BTS=%d\n",
		     ts_ptpod.ts_MTS, ts_ptpod.ts_BTS);
}
EXPORT_SYMBOL(get_thermal_slope_intercept);

static void thermal_interrupt_handler(int bank)
{
	U32 ret = 0;
	unsigned long flags;

	mt_ptp_lock(&flags);

	mtktscpu_switch_bank(bank);

	ret = DRV_Reg32(TEMPMONINTSTS);
	tscpu_printk("thermal_interrupt_handler,bank=0x%08x,ret=0x%08x\n",
		     bank, ret);

	/* tscpu_printk("thermal_isr: [Interrupt trigger]: status = 0x%x\n",
	 * ret);
	 */
	if (ret & THERMAL_MON_CINTSTS0)
		tscpu_dprintk("thermal_isr: ts 0 - cold interrupt trigger\n");

	if (ret & THERMAL_MON_HINTSTS0)
		tscpu_dprintk("<<<thermal_isr>>>: ts0 hot interrupt trigger\n");

	if (ret & THERMAL_MON_HINTSTS1)
		tscpu_dprintk("<<<thermal_isr>>>: ts1 hot interrupt trigger\n");

	if (ret & THERMAL_MON_HINTSTS2)
		tscpu_dprintk("<<<thermal_isr>>>: ts2 hot interrupt trigger\n");


	if (ret & THERMAL_tri_SPM_State0)
		tscpu_dprintk("thermal_isr: ts0 to trigger SPM state0\n");
	if (ret & THERMAL_tri_SPM_State1)
		tscpu_dprintk("thermal_isr: ts st1 to trigger SPM state1\n");
	if (ret & THERMAL_tri_SPM_State2)
		tscpu_dprintk("thermal_isr: ts st2 to trigger SPM state2\n");

	mt_ptp_unlock(&flags);

}

static irqreturn_t thermal_all_bank_interrupt_handler(int irq, void *dev_id)
{
	U32 ret = 0;

	ret = DRV_Reg32(THERMINTST);
	ret = ret & 0xF;
	tscpu_printk("thermal_all_bank_interrupt_handler : THERMINTST = 0x%x\n",
		     ret);

	if ((ret & 0x1) == 0)	/* check bit0 */
		thermal_interrupt_handler(THERMAL_BANK0);

	if ((ret & 0x2) == 0)	/* check bit1 */
		thermal_interrupt_handler(THERMAL_BANK1);

	if ((ret & 0x4) == 0)	/* check bit2 */
		thermal_interrupt_handler(THERMAL_BANK2);

	return IRQ_HANDLED;
}

static void thermal_reset_and_initial(void)
{
	/* mtktscpu_thermal_clock_on(); */


	/* Calculating period unit in Module clock x 256, and the Module
	 * clock
	 *
	 * will be changed to 26M when Infrasys enters Sleep mode.
	 * THERMAL_WRAP_WR32(0x000003FF, TEMPMONCTL1);     counting unit is
	 * 1023 * 15.15ns ~ 15.5us
	 * bus clock 66M counting unit is 4*15.15ns* 256 = 15513.6 ms=15.5us
	 * THERMAL_WRAP_WR32(0x00000004, TEMPMONCTL1);
	 * bus clock 66M counting unit is 12*15.15ns* 256 = 46.540us
	 */
	THERMAL_WRAP_WR32(0x0000000C, TEMPMONCTL1);

	/* bus clock 66M counting unit is 4*15.15ns* 256 = 15513.6 ms=15.5us */
	/* THERMAL_WRAP_WR32(0x000001FF, TEMPMONCTL1);*/

#if THERMAL_CONTROLLER_HW_FILTER == 2
	/* both filt and sen interval is 2016*15.5us = 31.25ms */
	THERMAL_WRAP_WR32(0x07E007E0, TEMPMONCTL2);
	/* poll is set to 31.25ms */
	THERMAL_WRAP_WR32(0x001F7972, TEMPAHBPOLL);
	/* temperature sampling control, 2 out of 4 samples */
	THERMAL_WRAP_WR32(0x00000049, TEMPMSRCTL0);
#elif THERMAL_CONTROLLER_HW_FILTER == 4
	/* both filt and sen interval is 20ms */
	THERMAL_WRAP_WR32(0x050A050A, TEMPMONCTL2);
	/* poll is set to 20ms */
	THERMAL_WRAP_WR32(0x001424C4, TEMPAHBPOLL);
	/* temperature sampling control, 4 out of 6 samples */
	THERMAL_WRAP_WR32(0x000000DB, TEMPMSRCTL0);
#elif THERMAL_CONTROLLER_HW_FILTER == 8
	/* both filt and sen interval is 12.5ms */
	THERMAL_WRAP_WR32(0x03390339, TEMPMONCTL2);
	/* poll is set to 12.5ms */
	THERMAL_WRAP_WR32(0x000C96FA, TEMPAHBPOLL);
	/* temperature sampling control, 8 out of 10 samples */
	THERMAL_WRAP_WR32(0x00000124, TEMPMSRCTL0);
#elif THERMAL_CONTROLLER_HW_FILTER == 16
	/* both filt and sen interval is 6.94ms */
	THERMAL_WRAP_WR32(0x01C001C0, TEMPMONCTL2);
	/* poll is set to 458379*15.15= 6.94ms */
	THERMAL_WRAP_WR32(0x0006FE8B, TEMPAHBPOLL);
	/* temperature sampling control, 16 out of 18 samples */
	THERMAL_WRAP_WR32(0x0000016D, TEMPMSRCTL0);

#else
	/*
	 * Default-1 filt interval is 1 * 46.540us = 46.54us, sen interval is
	 * 429 * 46.540us = 19.96ms
	 */
	THERMAL_WRAP_WR32(0x000101AD, TEMPMONCTL2);
	/* poll is set to 10u */
	THERMAL_WRAP_WR32(0x00000300, TEMPAHBPOLL);
	/* temperature sampling control, 1 sample */
	THERMAL_WRAP_WR32(0x00000000, TEMPMSRCTL0);
#endif
	/* exceed this polling time, IRQ would be inserted */
	THERMAL_WRAP_WR32(0xFFFFFFFF, TEMPAHBTO);


	/* times for interrupt occurrance */
	THERMAL_WRAP_WR32(0x00000000, TEMPMONIDET0);
	/* times for interrupt occurrance */
	THERMAL_WRAP_WR32(0x00000000, TEMPMONIDET1);

	THERMAL_WRAP_WR32(0x800, TEMPADCMUX);
	/* this value will be stored to TEMPPNPMUXADDR (TEMPSPARE0)
	 * automatically by hw
	 */
	THERMAL_WRAP_WR32((UINT32) AUXADC_CON1_CLR_P, TEMPADCMUXADDR);
	/* AHB address for auxadc mux selection */
	/* THERMAL_WRAP_WR32(0x1100100C, TEMPADCMUXADDR); AHB address for
	 * auxadc mux selection
	 */

	/* AHB value for auxadc enable */
	THERMAL_WRAP_WR32(0x800, TEMPADCEN);
	THERMAL_WRAP_WR32((UINT32) AUXADC_CON1_SET_P, TEMPADCENADDR);

	/* AHB address for auxadc valid bit */
	THERMAL_WRAP_WR32((UINT32) AUXADC_DAT11_P, TEMPADCVALIDADDR);
	/* AHB address for auxadc voltage output */
	THERMAL_WRAP_WR32((UINT32) AUXADC_DAT11_P, TEMPADCVOLTADDR);

	/* read valid & voltage are at the same register */
	THERMAL_WRAP_WR32(0x0, TEMPRDCTRL);

	/* indicate where the valid bit is (the 12th bit is valid bit and 1
	 * is valid)
	 */
	THERMAL_WRAP_WR32(0x0000002C, TEMPADCVALIDMASK);
	/* do not need to shift */
	THERMAL_WRAP_WR32(0x0, TEMPADCVOLTAGESHIFT);
}

/* Bank0 : CPU (TS_MCU1,TS_MCU2) (TS3, TS4) */
static void thermal_enable_all_periodoc_sensing_point_Bank0(void)
{
	/* enable periodoc temperature sensing point 0, point 1 */
	THERMAL_WRAP_WR32(0x00000003, TEMPMONCTL0);
}

/* Bank1 : GPU (TS_MCU3) (TS5), ABB (TS_ABB) */
static void thermal_enable_all_periodoc_sensing_point_Bank1(void)
{
	/* enable periodoc temperature sensing point 0, point 1 */
	THERMAL_WRAP_WR32(0x00000003, TEMPMONCTL0);
}

/* Bank2 : SoC (TS_MCU4,TS_MCU2,TS_MCU3) (TS1, TS4, TS5) */
static void thermal_enable_all_periodoc_sensing_point_Bank2(void)
{
	/* enable periodoc temperature sensing point 0, point 1, point 2 */
	THERMAL_WRAP_WR32(0x0000000F, TEMPMONCTL0);
}


/*
 * Bank0 : CPU (TS_MCU1,TS_MCU2)        (TS3, TS4)
 * Bank1 : GPU (TS_MCU3)                (TS5)
 * Bank2 : SOC (TS_MCU4,TS_MCU2,TS_MCU3)(TS1, TS4, TS5)
 */

static void thermal_config_Bank0_TS(void)
{
	/* tscpu_dprintk( "thermal_config_Bank0_TS:\n"); */


	/* Bank0:CPU(TS_MCU1 and TS_MCU2) */
	/* TSCON1[5:4]=2'b00 */
	/* TSCON1[2:0]=3'b000 */
	THERMAL_WRAP_WR32(0x0, TEMPADCPNP0);
	/* TSCON1[5:4]=2'b00 */
	/* TSCON1[2:0]=3'b001 */
	THERMAL_WRAP_WR32(0x1, TEMPADCPNP1);
	/* AHB address for pnp sensor mux selection */
	THERMAL_WRAP_WR32(TS_CON1_P, TEMPPNPMUXADDR);
	THERMAL_WRAP_WR32(0x3, TEMPADCWRITECTRL);
	/*      enable periodoc temperature sensing point 0, point 1 */
	/* THERMAL_WRAP_WR32(0x00000003, TEMPMONCTL0);*/
}

static void thermal_config_Bank1_TS(void)
{
	/*
	 * this value will be stored to TEMPPNPMUXADDR (TEMPSPARE0)
	 * automatically by hw
	 */
	THERMAL_WRAP_WR32(0x2, TEMPADCPNP0);

	THERMAL_WRAP_WR32(0x10, TEMPADCPNP1);

	/* AHB address for pnp sensor mux selection */
	THERMAL_WRAP_WR32(TS_CON1_P, TEMPPNPMUXADDR);

	THERMAL_WRAP_WR32(0x3, TEMPADCWRITECTRL);
}

static void thermal_config_Bank2_TS(void)
{
	/* Bank1:SOC(TS_MCU4,TS_MCU2,TS_MCU3) */
	THERMAL_WRAP_WR32(0x3, TEMPADCPNP0);

	/* TSCON1[5:4]=2'b00 */
	/* TSCON1[2:0]=3'b001 */
	THERMAL_WRAP_WR32(0x1, TEMPADCPNP1);

	/* TSCON1[5:4]=2'b00 */
	/* TSCON1[2:0]=3'b010 */
	THERMAL_WRAP_WR32(0x2, TEMPADCPNP2);

	/* Bank1:ABB(TS_ABB) */
	/* TSCON1[5:4]=2'b01 */
	/* TSCON1[2:0]=3'b000 */
	THERMAL_WRAP_WR32(0x10, TEMPADCPNP3);

	/* AHB address for pnp sensor mux selection */
	THERMAL_WRAP_WR32(TS_CON1_P, TEMPPNPMUXADDR);

	THERMAL_WRAP_WR32(0x3, TEMPADCWRITECTRL);
	/* enable periodoc temperature sensing point 0, point 1, point 2 */
	/* THERMAL_WRAP_WR32(0x00000003, TEMPMONCTL0);*/
}



static void thermal_config_TS_in_banks(enum thermal_bank_name bank_num)
{
	switch (bank_num) {
	case THERMAL_BANK0:	/* CPU(TS_MCU1 and TS_MCU2) */
			thermal_config_Bank0_TS();
			break;
	case THERMAL_BANK1:	/* GPU (TS_MCU3), ABB(TS_ABB) */
			thermal_config_Bank1_TS();
			break;
	case THERMAL_BANK2:	/* SOC(TS_MCU4,TS_MCU2,TS_MCU3) */
			thermal_config_Bank2_TS();
			break;
	default:
			/* CPU(TS_MCU1 and TS_MCU2) */
			thermal_config_Bank0_TS();
			break;
	}
}



static void thermal_enable_all_periodoc_sensing_point(enum thermal_bank_name
						      bank_num)
{
	switch (bank_num) {
	case THERMAL_BANK0:	/* CPU(TS_MCU1 and TS_MCU2) */
			thermal_enable_all_periodoc_sensing_point_Bank0();
			break;
	case THERMAL_BANK1:	/* GPU (TS_MCU3), ABB(TS_ABB) */
			thermal_enable_all_periodoc_sensing_point_Bank1();
			break;
	case THERMAL_BANK2:	/* SOC(TS_MCU4,TS_MCU2,TS_MCU3) */
			thermal_enable_all_periodoc_sensing_point_Bank2();
			break;
	default:
			thermal_enable_all_periodoc_sensing_point_Bank0();
			break;
	}
}

/**
 *  temperature2 to set the middle threshold for interrupting CPU. -275000 to
 *  disable it.
 */
static void set_tc_trigger_hw_protect(int temperature, int temperature2)
{
	int temp = 0;
	int raw_high, raw_middle, raw_low;


	/* temperature2=80000;  test only */
	tscpu_dprintk("set_tc_trigger_hw_protect t1=%d t2=%d\n", temperature,
		      temperature2);


	/* temperature to trigger SPM state2 */
	raw_high = temperature_to_raw_room(temperature);
	if (temperature2 > -275000)
		raw_middle = temperature_to_raw_room(temperature2);
	raw_low = temperature_to_raw_room(5000);


	temp = DRV_Reg32(TEMPMONINT);
	/* disable trigger SPM interrupt */
	THERMAL_WRAP_WR32(temp & 0x00000000, TEMPMONINT);

	/* set hot to wakeup event control */
	THERMAL_WRAP_WR32(0x20000, TEMPPROTCTL);

	THERMAL_WRAP_WR32(raw_low, TEMPPROTTA);
	if (temperature2 > -275000)
		/* register will remain unchanged if -275000... */
		THERMAL_WRAP_WR32(raw_middle, TEMPPROTTB);


	/* set hot to HOT wakeup event */
	THERMAL_WRAP_WR32(raw_high, TEMPPROTTC);

	/*
	 * trigger cold ,normal and hot interrupt
	 * remove for temp
	 * THERMAL_WRAP_WR32(temp | 0xE0000000, TEMPMONINT);
	 * enable trigger SPM interrupt
	 * Only trigger hot interrupt
	 */
	if (temperature2 > -275000)
		/* enable trigger middle & Hot SPM interrupt */
		THERMAL_WRAP_WR32(temp | 0xC0000000, TEMPMONINT);
	else
		/* enable trigger Hot SPM interrupt */
		THERMAL_WRAP_WR32(temp | 0x80000000, TEMPMONINT);
}



int mtk_cpufreq_register(struct mtk_cpu_power_info *freqs, int num)
{
	int i = 0;
	int gpu_power = 0;

	tscpu_dprintk("mtk_cpufreq_register\n");

	tscpu_num_opp = num;

	mtk_cpu_power = kzalloc((num) * sizeof(struct mtk_cpu_power_info),
				GFP_KERNEL);
	if (mtk_cpu_power == NULL)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		int dmips = freqs[i].cpufreq_khz * freqs[i].cpufreq_ncpu / 1000;
		/* TODO: this line must be modified every time cooler mapping
		 * table changes
		 */
		int cl_id = (((freqs[i].cpufreq_power + gpu_power)
			      + 99) / 100) - 7;

		mtk_cpu_power[i].cpufreq_khz = freqs[i].cpufreq_khz;
		mtk_cpu_power[i].cpufreq_ncpu = freqs[i].cpufreq_ncpu;
		mtk_cpu_power[i].cpufreq_power = freqs[i].cpufreq_power;

		if (cl_id < CPU_COOLER_NUM) {
			if (tscpu_cpu_dmips[cl_id] < dmips)
				tscpu_cpu_dmips[cl_id] = dmips;
		}

	}

	{
		/* TODO: this assumes the last OPP consumes least power...
			need to check this every time OPP table changes */
		int base =
			(mtk_cpu_power[num - 1].cpufreq_khz *
			 mtk_cpu_power[num - 1].cpufreq_ncpu) / 1000;
		for (i = 0; i < CPU_COOLER_NUM; i++) {
			if (0 == tscpu_cpu_dmips[i] ||
			    tscpu_cpu_dmips[i] < base)
				tscpu_cpu_dmips[i] = base;
			else
				base = tscpu_cpu_dmips[i];
		}
		mtktscpu_limited_dmips = base;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_cpufreq_register);

void mtkts_dump_cali_info(void)
{
	tscpu_dprintk("[cal] g_adc_ge_t      = 0x%x\n", g_adc_ge_t);
	tscpu_dprintk("[cal] g_adc_oe_t      = 0x%x\n", g_adc_oe_t);
	tscpu_dprintk("[cal] g_degc_cali     = 0x%x\n", g_degc_cali);
	tscpu_dprintk("[cal] g_adc_cali_en_t = 0x%x\n", g_adc_cali_en_t);
	tscpu_dprintk("[cal] g_o_slope       = 0x%x\n", g_o_slope);
	tscpu_dprintk("[cal] g_o_slope_sign  = 0x%x\n", g_o_slope_sign);
	tscpu_dprintk("[cal] g_id            = 0x%x\n", g_id);

	tscpu_dprintk("[cal] g_o_vtsmcu2     = 0x%x\n", g_o_vtsmcu2);
	tscpu_dprintk("[cal] g_o_vtsmcu3     = 0x%x\n", g_o_vtsmcu3);
	tscpu_dprintk("[cal] g_o_vtsmcu4     = 0x%x\n", g_o_vtsmcu4);
}


static void thermal_cal_prepare(void)
{
	U32 temp0 = 0, temp1 = 0, temp2 = 0;


	/* temp0 = DRV_Reg32(0xF0206184); */
	/* temp1 = DRV_Reg32(0xF0206180); */
	/* temp2 = DRV_Reg32(0xF0206188); */

	temp0 = get_devinfo_with_index(8);
	temp1 = get_devinfo_with_index(7);
	temp2 = get_devinfo_with_index(9);


	tscpu_dprintk("[calibration] devinfo idx7=0x%x, idx8=0x%x, idx9=0x%x\n",
		      temp1, temp0, temp2);
	/* mtktscpu_dprintk("thermal_cal_prepare\n"); */

	/* ADC_GE_T    [9:0] *(0x10206184)[31:22] */
	g_adc_ge_t = ((temp0 & 0xFFC00000) >> 22);
	/* ADC_OE_T    [9:0] *(0x10206184)[21:12] */
	g_adc_oe_t = ((temp0 & 0x003FF000) >> 12);
	/* O_VTSMCU1    (9b) *(0x10206180)[25:17] */
	g_o_vtsmcu1 = (temp1 & 0x03FE0000) >> 17;
	/* O_VTSMCU2    (9b) *(0x10206180)[16:8] */
	g_o_vtsmcu2 = (temp1 & 0x0001FF00) >> 8;
	/* O_VTSMCU3    (9b) *(0x10206184)[8:0] */
	g_o_vtsmcu3 = (temp0 & 0x000001FF);
	/* O_VTSMCU4    (9b) *(0x10206188)[31:23] */
	g_o_vtsmcu4 = (temp2 & 0xFF800000) >> 23;
	/* O_VTSABB     (9b) *(0x10206188)[22:14] */
	g_o_vtsabb = (temp2 & 0x007FC000) >> 14;

	/* DEGC_cali    (6b) *(0x10206180)[6:1] */
	g_degc_cali = (temp1 & 0x0000007E) >> 1;
	/* ADC_CALI_EN_T(1b) *(0x10206180)[0] */
	g_adc_cali_en_t = (temp1 & 0x00000001);

	/* O_SLOPE_SIGN (1b) *(0x10206180)[7] */
	g_o_slope_sign = (temp1 & 0x00000080) >> 7;

	/* O_SLOPE      (6b) *(0x10206180)[31:26] */
	g_o_slope = (temp1 & 0xFC000000) >> 26;

	/* ID           (1b) *(0x10206184)[9] */
	g_id = (temp0 & 0x00000200) >> 9;

	/*
	 * Check ID bit
	 * If ID=0 (TSMC sample), ignore O_SLOPE EFuse value and set O_SLOPE=0.
	 * If ID=1 (non-TSMC sample), read O_SLOPE EFuse value for
	 * following calculation.
	 */
	if (g_id == 0)
		g_o_slope = 0;

	/* g_adc_cali_en_t=0;test only */
	if (g_adc_cali_en_t == 1) {
		/* thermal_enable = true; */
	} else {
		tscpu_dprintk("This sample is not Thermal calibrated\n");
		g_adc_ge_t = 512;
		g_adc_oe_t = 512;
		g_degc_cali = 40;
		g_o_slope = 0;
		g_o_slope_sign = 0;
		g_o_vtsmcu1 = 260;
		g_o_vtsmcu2 = 260;
		g_o_vtsmcu3 = 260;
		g_o_vtsmcu4 = 260;
		g_o_vtsabb = 260;

	}

	mtkts_dump_cali_info();
}

static void thermal_cal_prepare_2(U32 ret)
{
	S32 format_1 = 0, format_2 = 0, format_3 = 0, format_4 = 0;
	S32 format_5 = 0;

	/* tscpu_printk("thermal_cal_prepare_2\n"); */

	g_ge = ((g_adc_ge_t - 512) * 10000) / 4096;	/* ge * 10000 */
	g_oe = (g_adc_oe_t - 512);

	g_gain = (10000 + g_ge);

	format_1 = (g_o_vtsmcu1 + 3350 - g_oe);
	format_2 = (g_o_vtsmcu2 + 3350 - g_oe);
	format_3 = (g_o_vtsmcu3 + 3350 - g_oe);
	format_4 = (g_o_vtsmcu4 + 3350 - g_oe);
	format_5 = (g_o_vtsabb + 3350 - g_oe);

	g_x_roomt[0] = (((format_1 * 10000) / 4096) * 10000) / g_gain;
	g_x_roomt[1] = (((format_2 * 10000) / 4096) * 10000) / g_gain;
	g_x_roomt[2] = (((format_3 * 10000) / 4096) * 10000) / g_gain;

	tscpu_dprintk("[cal] g_ge         = 0x%x\n", g_ge);
	tscpu_dprintk("[cal] g_gain       = 0x%x\n", g_gain);

	tscpu_dprintk("[cal] g_x_roomt1   = 0x%x\n", g_x_roomt[0]);
	tscpu_dprintk("[cal] g_x_roomt2   = 0x%x\n", g_x_roomt[1]);
	tscpu_dprintk("[cal] g_x_roomt3   = 0x%x\n", g_x_roomt[2]);

}

#if THERMAL_CONTROLLER_HW_TP
static S32 temperature_to_raw_room(U32 ret)
{
	S32 t_curr = ret;
	S32 format_1 = 0;
	S32 format_2 = 0;
	S32 format_3[THERMAL_SENSOR_NUM] = { 0 };
	S32 format_4[THERMAL_SENSOR_NUM] = { 0 };
	S32 i, index = 0, temp = 0;

	/* tscpu_dprintk("temperature_to_raw_room\n"); */

	if (g_o_slope_sign == 0) {	/* O_SLOPE is Positive. */
		format_1 = t_curr - (g_degc_cali * 1000 / 2);
		format_2 = format_1 * (165 + g_o_slope) * 18 / 15;
		format_2 = format_2 - 2 * format_2;

		for (i = 0; i < THERMAL_SENSOR_NUM; i++) {
			format_3[i] = format_2 / 1000 + g_x_roomt[i] * 10;
			format_4[i] = (format_3[i] * 4096 / 10000 * g_gain)
					/ 100000 + g_oe;
		}
	} else {		/* O_SLOPE is Negative. */
		format_1 = t_curr - (g_degc_cali * 1000 / 2);
		format_2 = format_1 * (165 - g_o_slope) * 18 / 15;
		format_2 = format_2 - 2 * format_2;

		for (i = 0; i < THERMAL_SENSOR_NUM; i++) {
			format_3[i] = format_2 / 1000 + g_x_roomt[i] * 10;
			format_4[i] = (format_3[i] * 4096 / 10000 * g_gain)
					/ 100000 + g_oe;
		}
	}

	temp = 0;
	for (i = 0; i < THERMAL_SENSOR_NUM; i++) {
		if (temp < format_4[i]) {
			temp = format_4[i];
			index = i;
		}
	}

	return format_4[index];
}
#endif

static S32 raw_to_temperature_roomt(U32 ret, enum thermal_sensor_name ts_name)
{
	S32 t_current = 0;
	S32 y_curr = ret;
	S32 format_1 = 0;
	S32 format_2 = 0;
	S32 format_3 = 0;
	S32 format_4 = 0;
	S32 xtoomt = 0;

	xtoomt = g_x_roomt[ts_name];

	if (ret == 0)
		return 0;

	format_1 = ((g_degc_cali * 10) >> 1);
	format_2 = (y_curr - g_oe);

	format_3 = (((((format_2) * 10000) >> 12) * 10000) / g_gain) - xtoomt;
	format_3 = format_3 * 15 / 18;


	if (g_o_slope_sign == 0)
		format_4 = ((format_3 * 100) / (165 + g_o_slope));
	else
		format_4 = ((format_3 * 100) / (165 - g_o_slope));

	format_4 = format_4 - (format_4 << 1);


	t_current = format_1 + format_4;	/* uint = 0.1 deg */

	/* tscpu_dprintk("raw_to_temperature_room,t_current=%d\n",t_current); */
	return t_current;
}



static void thermal_calibration(void)
{
	if (g_adc_cali_en_t == 0)
		tscpu_printk("#####  Not Calibration  ######\n");

	/* tscpu_dprintk("thermal_calibration\n"); */
	thermal_cal_prepare_2(0);
}




int get_immediate_abb_temp_wrap(void)
{
	int curr_temp = 0;
	/* curr_temp = ABB_TS_ABB_T; */

	tscpu_dprintk("get_immediate_abb_temp_wrap curr_temp=%d\n", curr_temp);

	return curr_temp;
}


/*
 * Bank0 : CPU (TS_MCU1,TS_MCU2)        (TS3, TS4)
 * Bank1 : GPU (TS_MCU3)                (TS5)
 * Bank2 : SOC (TS_MCU4,TS_MCU2,TS_MCU3)(TS1, TS4, TS5)
 */
int get_immediate_cpu_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(CPU_TS_MCU1_T, CPU_TS_MCU2_T);

	tscpu_dprintk("get_immediate_cpu_wrap curr_temp=%d\n", curr_temp);

	return curr_temp;
}

int get_immediate_gpu_wrap(void)
{
	int curr_temp;

	curr_temp = GPU_TS_MCU2_T;

	tscpu_dprintk("get_immediate_gpu_wrap curr_temp=%d\n", curr_temp);

	return curr_temp;
}

int get_immediate_soc_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(SOC_TS_MCU1_T, SOC_TS_MCU2_T);
	curr_temp = MAX(SOC_TS_MCU3_T, curr_temp);

	tscpu_dprintk("get_immediate_soc_wrap curr_temp=%d\n", curr_temp);

	return curr_temp;
}

/*
 * Bank0 : CPU (TS_MCU1,TS_MCU2)        (TS3, TS4)
 * Bank1 : GPU (TS_MCU3)                (TS5)
 * Bank2 : SOC (TS_MCU4,TS_MCU2,TS_MCU3)(TS1, TS4, TS5)
 */
int get_immediate_ts1_wrap(void)
{
	int curr_temp;

	curr_temp = CPU_TS_MCU1_T;
	tscpu_dprintk("get_immediate_ts1_wrap curr_temp=%d\n", curr_temp);

	return curr_temp;
}

int get_immediate_ts2_wrap(void)
{
	int curr_temp;

	curr_temp = GPU_TS_MCU2_T;
	tscpu_dprintk("get_immediate_ts2_wrap curr_temp=%d\n", curr_temp);

	return curr_temp;
}

int get_immediate_ts3_wrap(void)
{
	int curr_temp;

	curr_temp = SOC_TS_MCU3_T;
	tscpu_dprintk("get_immediate_ts3_wrap curr_temp=%d\n", curr_temp);

	return curr_temp;
}

int get_immediate_ts4_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(CPU_TS_MCU2_T, SOC_TS_MCU2_T);
	tscpu_dprintk("get_immediate_ts4_wrap curr_temp=%d\n", curr_temp);

	return curr_temp;
}

int get_immediate_ts5_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(GPU_TS_MCU2_T, SOC_TS_MCU3_T);
	tscpu_dprintk("get_immediate_ts5_wrap curr_temp=%d\n", curr_temp);

	return curr_temp;
}

static int read_tc_raw_and_temp(u32 *tempmsr_name,
				enum thermal_sensor_name ts_name, int *ts_raw)
{
	int temp = 0, raw = 0;

	raw = (tempmsr_name != 0) ? (DRV_Reg32((tempmsr_name)) & 0x0fff) : 0;
	temp = (tempmsr_name != 0) ? raw_to_temperature_roomt(raw, ts_name) : 0;

	*ts_raw = raw;

	return temp * 100;
}


/*
 * Bank0 : CPU (TS_MCU1)
 * Bank1 : GPU (TS_MCU2)
 * Bank2 : SOC (TS_MCU3)
 */
static void read_each_bank_TS(enum thermal_bank_name bank_num)
{

	/* tscpu_dprintk("read_each_bank_TS,bank_num=%d\n",bank_num); */

	switch (bank_num) {
	case THERMAL_BANK0:
			/* Bank0 : CPU (TS_MCU1) */
			CPU_TS_MCU1_T =
				read_tc_raw_and_temp((u32 *)TEMPMSR0,
						     THERMAL_SENSOR1,
						     &CPU_TS_MCU1_R);
			CPU_TS_MCU2_T =
				read_tc_raw_and_temp((u32 *)TEMPMSR1,
						     THERMAL_SENSOR2,
						     &CPU_TS_MCU2_R);
			break;
	case THERMAL_BANK1:
			/* Bank1 : GPU (TS_MCU2) */
			GPU_TS_MCU1_T =
				read_tc_raw_and_temp((u32 *)TEMPMSR0,
						     THERMAL_SENSOR1,
						     &GPU_TS_MCU1_R);
			GPU_TS_MCU2_T =
				read_tc_raw_and_temp((u32 *)TEMPMSR1,
						     THERMAL_SENSOR2,
						     &GPU_TS_MCU2_R);
			break;
	case THERMAL_BANK2:
			/* Bank2 : SOC (TS_MCU3) */
			SOC_TS_MCU1_T =
				read_tc_raw_and_temp((u32 *)TEMPMSR0,
						     THERMAL_SENSOR1,
						     &SOC_TS_MCU1_R);
			SOC_TS_MCU2_T =
				read_tc_raw_and_temp((u32 *)TEMPMSR1,
						     THERMAL_SENSOR2,
						     &SOC_TS_MCU2_R);
			SOC_TS_MCU3_T =
				read_tc_raw_and_temp((u32 *)TEMPMSR2,
						     THERMAL_SENSOR3,
						     &SOC_TS_MCU3_R);
			break;
	default:
			/* Bank0 : CPU (TS_MCU1,TS_MCU2) */
			CPU_TS_MCU1_T =
				read_tc_raw_and_temp((u32 *)TEMPMSR0,
						     THERMAL_SENSOR1,
						     &CPU_TS_MCU1_R);
			CPU_TS_MCU2_T =
				read_tc_raw_and_temp((u32 *)TEMPMSR1,
						     THERMAL_SENSOR2,
						     &CPU_TS_MCU2_R);
			break;
	}

}

static void read_all_bank_temperature(void)
{
	int i = 0;
	unsigned long flags;

	mt_ptp_lock(&flags);

	for (i = 0; i < THERMAL_BANK_NUM; i++) {
		mtktscpu_switch_bank(i);
		read_each_bank_TS(i);
	}

	mt_ptp_unlock(&flags);
}


static int tscpu_get_temp(struct thermal_zone_device *thermal,
			  unsigned long *t)
{
#if MTK_TS_CPU_SW_FILTER == 1
	int ret = 0;
	int curr_temp;
	int temp_temp;
	int bank0_T;
	int bank1_T;
	int bank2_T;
	static int last_cpu_real_temp;

	/*
	 * Bank0 : CPU (TS_MCU1,TS_MCU2)        (TS3, TS4)
	 * Bank1 : GPU (TS_MCU3)                (TS5)
	 * Bank2 : SOC (TS_MCU4,TS_MCU2,TS_MCU3)(TS1, TS4, TS5)
	 */
	bank0_T = MAX(CPU_TS_MCU1_T, CPU_TS_MCU2_T);
	bank1_T = GPU_TS_MCU2_T;
	bank2_T = MAX(SOC_TS_MCU1_T, SOC_TS_MCU2_T);
	bank2_T = MAX(bank2_T, SOC_TS_MCU3_T);

	curr_temp = MAX(bank0_T, bank1_T);
	curr_temp = MAX(curr_temp, bank2_T);



	tscpu_dprintk("\n tscpu_update_tempinfo, T=%d,%d,%d,%d,%d,%d\n",
		      CPU_TS_MCU1_T, CPU_TS_MCU2_T, GPU_TS_MCU2_T,
		      SOC_TS_MCU1_T, SOC_TS_MCU2_T, SOC_TS_MCU3_T);



	if ((curr_temp > (trip_temp[0] - 15000)) || (curr_temp < -30000)
	    || (curr_temp > 85000))
		tscpu_dprintk("CPU T=%d\n", curr_temp);

	temp_temp = curr_temp;
	/* not resumed from suspensio... */
	if (curr_temp != 0) {
		/* invalid range */
		if ((curr_temp > 150000) || (curr_temp < -20000)) {
			tscpu_dprintk("CPU temp invalid=%d\n", curr_temp);
			temp_temp = 50000;
			ret = -1;
		} else if (last_cpu_real_temp != 0) {
			/* delta 40C, invalid change */
			if ((curr_temp - last_cpu_real_temp > 40000)
			    || (last_cpu_real_temp - curr_temp > 40000)) {
				tscpu_dprintk("CPUtemp floattemp=%d,ltemp=%d\n",
						curr_temp, last_cpu_real_temp);
				temp_temp = 50000;
				ret = -1;
			}
		}
	}

	last_cpu_real_temp = curr_temp;
	curr_temp = temp_temp;
#else
	int ret = 0;
	int curr_temp;

	curr_temp = get_immediate_temp1();

	tscpu_dprintk("tscpu_get_temp CPU T1=%d\n", curr_temp);

	if ((curr_temp > (trip_temp[0] - 15000)) || (curr_temp < -30000))
		tscpu_dprintk("[Power/CPU_Thermal] CPU T=%d\n", curr_temp);
#endif

	read_curr_temp = curr_temp;
	*t = (unsigned long)curr_temp;

#if MTKTSCPU_FAST_POLLING
	cur_fp_factor = next_fp_factor;

	if (curr_temp >= fast_polling_trip_temp) {
		next_fp_factor = fast_polling_factor;
		/* it means next timeout will be in
		 * interval/fast_polling_factor
		 */
		thermal->polling_delay = interval / fast_polling_factor;
	} else {
		next_fp_factor = 1;
		thermal->polling_delay = interval;
	}
#endif

	/* for low power */
	if ((int)*t >= polling_trip_temp1)
		;
	else if ((int)*t < polling_trip_temp2)
		thermal->polling_delay = interval * polling_factor2;
	else
		thermal->polling_delay = interval * polling_factor1;

#if CPT_ADAPTIVE_AP_COOLER
	g_prev_temp = g_curr_temp;
	g_curr_temp = curr_temp;
#endif

	g_max_temp = curr_temp;

	tscpu_dprintk("tscpu_get_temp, current temp =%d\n", curr_temp);
	return ret;
}


int tscpu_get_temp_by_bank(enum thermal_bank_name ts_bank)
{
	int bank_T = 0;

	/* tscpu_dprintk("tscpu_get_temp_by_bank,bank=%d\n",ts_bank); */

	/*
	 * Bank0 : CPU (TS_MCU1,TS_MCU2)        (TS3, TS4)
	 * Bank1 : GPU (TS_MCU3)                (TS5)
	 * Bank2 : SOC (TS_MCU4,TS_MCU2,TS_MCU3)(TS1, TS4, TS5)
	 */
	if (ts_bank == THERMAL_BANK0) {
		bank_T = MAX(CPU_TS_MCU1_T, CPU_TS_MCU2_T);
	} else if (ts_bank == THERMAL_BANK1) {
		bank_T = GPU_TS_MCU2_T;
	} else if (ts_bank == THERMAL_BANK2) {
		bank_T = MAX(SOC_TS_MCU1_T, SOC_TS_MCU2_T);
		bank_T = MAX(bank_T, SOC_TS_MCU3_T);
	}

	return bank_T;
}

static int tscpu_get_mode(struct thermal_zone_device *thermal,
			  enum thermal_device_mode *mode)
{
	struct tscpu_thermal_zone *tz = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tz->pdata;

	if (!pdata)
		return -EINVAL;
	*mode = pdata->mode;
	return 0;
}

static int tscpu_set_mode(struct thermal_zone_device *thermal,
			  enum thermal_device_mode mode)
{
	struct tscpu_thermal_zone *tz = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tz->pdata;

	if (!pdata)
		return -EINVAL;

	pdata->mode = mode;
	schedule_work(&tz->thermal_work);
	return 0;
}

static int tscpu_get_trip_type(struct thermal_zone_device *thermal,
				  int trip,
				  enum thermal_trip_type *type)
{
	struct tscpu_thermal_zone *tz = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tz->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*type = pdata->trips[trip].type;
	return 0;
}

static int tscpu_get_trip_temp(struct thermal_zone_device *thermal,
				  int trip,
				  unsigned long *t)
{
	struct tscpu_thermal_zone *tz = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tz->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	*t = pdata->trips[trip].temp;
	return 0;
}

static int tscpu_set_trip_temp(struct thermal_zone_device *thermal,
				  int trip,
				  unsigned long t)
{
	struct tscpu_thermal_zone *tz = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tz->pdata;

	if (!pdata)
		return -EINVAL;
	if (trip >= pdata->num_trips)
		return -EINVAL;

	pdata->trips[trip].temp = t;
	return 0;
}

static int tscpu_get_crit_temp(struct thermal_zone_device *thermal,
			       unsigned long *t)
{
	int i;
	struct tscpu_thermal_zone *tz = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tz->pdata;

	if (!pdata)
		return -EINVAL;

	for (i = 0; i < pdata->num_trips; i++) {
		if (pdata->trips[i].type == THERMAL_TRIP_CRITICAL) {
			*t = pdata->trips[i].temp;
			return 0;
		}
	}
	return -EINVAL;
}

static int tscpu_get_trip_hyst(struct thermal_zone_device *thermal,
						int trip,
						unsigned long *hyst)
{
	struct tscpu_thermal_zone *tz = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tz->pdata;

	if (!tz || !pdata)
		return -EINVAL;
	*hyst = pdata->trips[trip].hyst;
	return 0;
}
static int tscpu_set_trip_hyst(struct thermal_zone_device *thermal,
						int trip,
						unsigned long hyst)
{
	struct tscpu_thermal_zone *tz = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tz->pdata;

	if (!tz || !pdata)
		return -EINVAL;
	pdata->trips[trip].hyst = hyst;
	return 0;
}

#define PREFIX "thermaltscpu:def"
static int tscpu_thermal_notify(struct thermal_zone_device *thermal,
				int trip, enum thermal_trip_type type)
{
#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[TSCPU_METRICS_STR_LEN];
#endif

#ifdef CONFIG_AMAZON_SIGN_OF_LIFE
	if (type == THERMAL_TRIP_CRITICAL) {
		tscpu_printk("[%s] Thermal shutdown CPU, temp=%d, trip=%d\n",
				__func__, thermal->temperature, trip);
		life_cycle_set_thermal_shutdown_reason(
						THERMAL_SHUTDOWN_REASON_SOC);
	}
#endif

#ifdef CONFIG_AMAZON_METRICS_LOG
	if (type == THERMAL_TRIP_CRITICAL) {
		snprintf(buf, TSCPU_METRICS_STR_LEN,
			"%s:tscpumonitor;CT;1,temp=%d;trip=%d;CT;1:NR",
			PREFIX, thermal->temperature, trip);
		log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", buf);
	}
#endif

	return 0;
}

/* pause ALL periodoc temperature sensing point */
static void thermal_pause_all_periodoc_temp_sensing(void)
{
	int i = 0;
	unsigned long flags;
	int temp;

	/* tscpu_printk("thermal_pause_all_periodoc_temp_sensing\n"); */

	mt_ptp_lock(&flags);

	/*config bank0,1,2 */
	for (i = 0; i < THERMAL_BANK_NUM; i++) {

		mtktscpu_switch_bank(i);
		temp = DRV_Reg32(TEMPMSRCTL1);
		/* set bit8=bit1=bit2=bit3=1 to pause sensing point 0,1,2,3 */
		DRV_WriteReg32(TEMPMSRCTL1, (temp | 0x10E));
	}

	mt_ptp_unlock(&flags);

}


/* release ALL periodoc temperature sensing point */
static void thermal_release_all_periodoc_temp_sensing(void)
{
	int i = 0;
	unsigned long flags;
	int temp;

	/* tscpu_printk("thermal_release_all_periodoc_temp_sensing\n"); */

	mt_ptp_lock(&flags);

	/*config bank0,1,2 */
	for (i = 0; i < THERMAL_BANK_NUM; i++) {

		mtktscpu_switch_bank(i);
		temp = DRV_Reg32(TEMPMSRCTL1);
		/* set bit1=bit2=bit3=0 to release sensing point 0,1,2 */
		DRV_WriteReg32(TEMPMSRCTL1, ((temp & (~0x10E))));
	}

	mt_ptp_unlock(&flags);

}


/* disable ALL periodoc temperature sensing point */
static void thermal_disable_all_periodoc_temp_sensing(void)
{
	int i = 0;
	unsigned long flags;

	/* tscpu_printk("thermal_disable_all_periodoc_temp_sensing\n"); */

	mt_ptp_lock(&flags);

	/*config bank0,1,2 */
	for (i = 0; i < THERMAL_BANK_NUM; i++) {

		mtktscpu_switch_bank(i);
		THERMAL_WRAP_WR32(0x00000000, TEMPMONCTL0);
	}

	mt_ptp_unlock(&flags);

}

static void tscpu_clear_all_temp(void)
{
	CPU_TS_MCU1_T = 0;
	CPU_TS_MCU2_T = 0;
	GPU_TS_MCU1_T = 0;
	GPU_TS_MCU2_T = 0;
	SOC_TS_MCU1_T = 0;
	SOC_TS_MCU2_T = 0;
	SOC_TS_MCU3_T = 0;



}

/*tscpu_thermal_suspend spend 1000us~1310us*/
static int tscpu_thermal_suspend(struct platform_device *dev,
				 pm_message_t state)
{
	int cnt = 0;
	int temp = 0, wd_api_ret;
	struct wd_api *wd_api;

	tscpu_printk("tscpu_thermal_suspend, talking_flag=%d\n", talking_flag);
#if THERMAL_PERFORMANCE_PROFILE
	struct timeval begin, end;
	unsigned long val;

	do_gettimeofday(&begin);
#endif

	g_tc_resume = 1;	/* set "1", don't read temp during suspend */

	if (talking_flag == false) {
		tscpu_dprintk("tscpu_thermal_suspend no talking\n");

		wd_api_ret = get_wd_api(&wd_api);
		if (wd_api_ret >= 0) {
			/* reset mode */
			wd_api->wd_thermal_direct_mode_config(WD_REQ_DIS,
							      WD_REQ_IRQ_MODE);
		}

		while (cnt < 50) {
			temp = (DRV_Reg32(THAHBST0) >> 16);
			if (cnt > 10)
				tscpu_printk(KERN_CRIT
					     "THAHBST0 = 0x%x,cnt=%d, %d\n",
					     temp, cnt, __LINE__);
			if (temp == 0x0) {
				/* pause all periodoc temperature sensing
				 * point 0~2
				 */
				thermal_pause_all_periodoc_temp_sensing();
				break;
			}
			udelay(2);
			cnt++;
		}

		/* disable periodic temp measurement on sensor 0~2 */
		thermal_disable_all_periodoc_temp_sensing(); /* TEMPMONCTL0 */

		tscpu_thermal_clock_off();

		/*TSCON1[5:4]=2'b11, Buffer off */
		/* turn off the sensor buffer to save power */
		THERMAL_WRAP_WR32(DRV_Reg32(TS_CON1) | 0x00000030, TS_CON1);
	}
#if THERMAL_PERFORMANCE_PROFILE
	do_gettimeofday(&end);

	/* Get milliseconds */
	tscpu_printk("suspend time spent,sec : %lu , usec : %lu\n",
		     (end.tv_sec - begin.tv_sec),
		     (end.tv_usec - begin.tv_usec));
#endif
	return 0;
}

/*tscpu_thermal_suspend spend 3000us~4000us*/
static int tscpu_thermal_resume(struct platform_device *dev)
{
	int temp = 0;
	int cnt = 0;

	tscpu_printk("tscpu_thermal_resume,talking_flag=%d\n", talking_flag);

	g_tc_resume = 1; /* set "1", don't read temp during start resume */

	if (talking_flag == false) {

		tscpu_reset_thermal();

		temp = DRV_Reg32(TS_CON1);
		/* TS_CON1[5:4]=2'b00,   00: Buffer on, TSMCU to AUXADC */
		temp &= ~(0x00000030);
		THERMAL_WRAP_WR32(temp, TS_CON1);	/* read abb need */
		/* RG_TS2AUXADC < set from 2'b11 to 2'b00 when resume.
				wait 100uS than turn on thermal controller. */
		udelay(200);

		/* add this function to read all temp first to avoid
		 * write TEMPPROTTC first time will issue an fake signal
		 * to RGU
		 */
		tscpu_fast_initial_sw_workaround();

		while (cnt < 50) {
			temp = (DRV_Reg32(THAHBST0) >> 16);
			if (cnt > 10)
				tscpu_printk(KERN_CRIT "ST0 = 0x%x,cnt=%d,%d\n",
					     temp, cnt, __LINE__);
			if (temp == 0x0) {
				/* pause all periodoc temperature sensing
				 * point 0~2
				 * TEMPMSRCTL1
				 */
				thermal_pause_all_periodoc_temp_sensing();

				break;
			}
			udelay(2);
			cnt++;
		}
		thermal_disable_all_periodoc_temp_sensing(); /* TEMPMONCTL0 */

		thermal_initial_all_bank();
		/* must release before start */
		thermal_release_all_periodoc_temp_sensing();

		tscpu_clear_all_temp();

		read_all_bank_temperature();
	}

	g_tc_resume = 2;	/* set "2", resume finish,can read temp */

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_thermal_of_match[] = {
	{.compatible = "mediatek,mt8163-thermal",},
	{},
};
#endif

#if CPT_ADAPTIVE_AP_COOLER

#if THERMAL_HEADROOM
static int tscpu_thp_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_thp, NULL);
}

static const struct file_operations mtktscpu_thp_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_thp_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_write_thp,
	.release = single_release,
};
#endif

#endif

#if MTKTSCPU_FAST_POLLING
static int tscpu_fastpoll_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_fastpoll, NULL);
}

static const struct file_operations mtktscpu_fastpoll_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_fastpoll_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_write_fastpoll,
	.release = single_release,
};
#endif


static int mtktscpu_switch_bank(enum thermal_bank_name bank)
{
	/* tscpu_dprintk( "mtktscpu_switch_bank =bank=%d\n",bank); */

	switch (bank) {
	case THERMAL_BANK0:	/* CPU (TS_MCU1,TS_MCU2)(TS3, TS4) */
			thermal_clrl(PTPCORESEL, 0xF);	/* bank0 */
			break;
	case THERMAL_BANK1:	/* GPU (TS_MCU3) (TS5) */
			thermal_clrl(PTPCORESEL, 0xF);
			thermal_setl(PTPCORESEL, 0x1);	/* bank1 */
			break;
	case THERMAL_BANK2: /* SOC (TS_MCU4,TS_MCU2,TS_MCU3)(TS1, TS4, TS5) */
			thermal_clrl(PTPCORESEL, 0xF);
			thermal_setl(PTPCORESEL, 0x2);	/* bank2 */
			break;
	default:
			thermal_clrl(PTPCORESEL, 0xF);	/* bank0 */
			break;
	}
	return 0;
}



static void thermal_initial_all_bank(void)
{
	int i = 0;
	unsigned long flags;
	UINT32 temp = 0;

	/* tscpu_thermal_clock_on(); */

	mt_ptp_lock(&flags);

	/* AuxADC Initialization,ref MT6592_AUXADC.doc  TODO: check this line */
	temp = DRV_Reg32(AUXADC_CON0_V);	/* Auto set enable for CH11 */
	temp &= 0xFFFFF7FF;	/* 0: Not AUTOSET mode */
	/* disable auxadc channel 11 synchronous mode */
	THERMAL_WRAP_WR32(temp, AUXADC_CON0_V);

	/* disable auxadc channel 11 immediate mode */
	THERMAL_WRAP_WR32(0x800, AUXADC_CON1_CLR_V);

	for (i = 0; i < THERMAL_BANK_NUM; i++) {
		mtktscpu_switch_bank(i);
		thermal_reset_and_initial();
		thermal_config_TS_in_banks(i);
	}

	/* enable auxadc channel 11 immediate mode */
	THERMAL_WRAP_WR32(0x800, AUXADC_CON1_SET_V);

	for (i = 0; i < THERMAL_BANK_NUM; i++) {
		mtktscpu_switch_bank(i);
		thermal_enable_all_periodoc_sensing_point(i);
	}

	mt_ptp_unlock(&flags);

}


static void tscpu_config_all_tc_hw_protect(int temperature, int temperature2)
{
	int i = 0, wd_api_ret;
	unsigned long flags;
	struct wd_api *wd_api;

	tscpu_dprintk("tscpu_config_all_tc_hw_protect,temp=%d, temp2=%d,\n",
		      temperature, temperature2);

#if THERMAL_PERFORMANCE_PROFILE
	struct timeval begin, end;
	unsigned long val;

	do_gettimeofday(&begin);
#endif
	/*spend 860~1463 us */
	/*Thermal need to config to direct reset mode
	  this API provide by Weiqi Fu(RGU SW owner). */

	wd_api_ret = get_wd_api(&wd_api);
	if (wd_api_ret >= 0) {
		/* reset mode */
		wd_api->wd_thermal_direct_mode_config(WD_REQ_DIS,
						      WD_REQ_RST_MODE);
	} else {
		pr_err("%d FAILED TO GET WD API\n", __LINE__);
		BUG();
	}

#if THERMAL_PERFORMANCE_PROFILE
	do_gettimeofday(&end);

	/* Get milliseconds */
	tscpu_printk("resume time spent, sec : %lu , usec : %lu\n",
		     (end.tv_sec - begin.tv_sec),
		     (end.tv_usec - begin.tv_usec));
#endif

	mt_ptp_lock(&flags);


	for (i = 0; i < THERMAL_BANK_NUM; i++) {

		mtktscpu_switch_bank(i);

		/* Move thermal HW protection ahead... */
		set_tc_trigger_hw_protect(temperature, temperature2);
	}

	mt_ptp_unlock(&flags);

	/*
	 * Thermal need to config to direct reset mode
	 * this API provide by Weiqi Fu(RGU SW owner).
	 * reset mode
	 */
	wd_api->wd_thermal_direct_mode_config(WD_REQ_EN, WD_REQ_RST_MODE);

}

static void tscpu_reset_thermal(void)
{
	int temp = 0;
	/* reset thremal ctrl */
	temp = DRV_Reg32(INFRA_GLOBALCON_RST_0_SET);
	temp |= 0x00000001;	/* 1: Enables thermal control software reset */
	THERMAL_WRAP_WR32(temp, INFRA_GLOBALCON_RST_0_SET);

	/* un reset */
	temp = DRV_Reg32(INFRA_GLOBALCON_RST_0_CLR);
	/* 1: Enable reset Disables thermal control software reset */
	temp |= 0x00000001;
	THERMAL_WRAP_WR32(temp, INFRA_GLOBALCON_RST_0_CLR);

	tscpu_thermal_clock_on();
}

static void tscpu_fast_initial_sw_workaround(void)
{
	int i = 0;
	unsigned long flags;
	/* tscpu_printk("tscpu_fast_initial_sw_workaround\n"); */

	/* tscpu_thermal_clock_on(); */

	mt_ptp_lock(&flags);

	for (i = 0; i < THERMAL_BANK_NUM; i++) {
		mtktscpu_switch_bank(i);
		thermal_fast_init();
	}

	mt_ptp_unlock(&flags);
}


#if THERMAL_DRV_UPDATE_TEMP_DIRECT_TO_MET
int tscpu_get_cpu_temp_met(enum MTK_THERMAL_SENSOR_CPU_ID_MET id)
{
	return 0;
}
EXPORT_SYMBOL(tscpu_get_cpu_temp_met);
#endif



static enum hrtimer_restart tscpu_update_tempinfo(struct hrtimer *timer)
{
	ktime_t ktime;
	unsigned long flags;
	int resume_ok = 0;
	/* tscpu_printk("tscpu_update_tempinfo\n"); */
	tscpu_dprintk("%s: g_tc_resume=%d\n", __func__, g_tc_resume);

	if (g_tc_resume == 0) {
		read_all_bank_temperature();
		if (resume_ok) {
			tscpu_config_all_tc_hw_protect(trip_temp[0],
						       tc_mid_trip);
			resume_ok = 0;
		}
	} else if (g_tc_resume == 2) {	/* resume ready */

		/* tscpu_printk("tscpu_update_tempinfo g_tc_resume==2\n"); */
		g_tc_resume = 0;
		resume_ok = 1;
	}
#if THERMAL_DRV_UPDATE_TEMP_DIRECT_TO_MET

	tscpu_met_lock(&flags);
	a_tscpu_all_temp[0] = CPU_TS_MCU1_T;	/* temp of TS_MCU1 */
	a_tscpu_all_temp[1] = GPU_TS_MCU2_T;	/* temp of TS_MCU2 */
	a_tscpu_all_temp[2] = SOC_TS_MCU3_T;	/* temp of TS_MCU3 */
	tscpu_met_unlock(&flags);

	if (NULL != g_pThermalSampler)
		g_pThermalSampler();
#endif
	ktime = ktime_set(0, 50000000);	/* 50ms */
	hrtimer_forward_now(timer, ktime);

	return HRTIMER_RESTART;
}

static void tscpu_update_temperature_timer_init(void)
{
	ktime_t ktime;

	tscpu_dprintk("tscpu_update_temperature_timer_init\n");

	ktime = ktime_set(0, 50000000);	/* 50ms */

	hrtimer_init(&ts_tempinfo_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	ts_tempinfo_hrtimer.function = tscpu_update_tempinfo;
	hrtimer_start(&ts_tempinfo_hrtimer, ktime, HRTIMER_MODE_REL);

}

#if THERMAL_TURNOFF_AUXADC_BEFORE_DEEPIDLE
/*&#include <mach/mt_clkmgr.h>*/
/* #define MT_PDN_PERI_AUXADC MT_CG_INFRA_AUXADC */

void tscpu_pause_tc(void)
{
	int cnt = 0;
	int temp = 0;

	/* tscpu_printk("tscpu_pause_tc\n"); */


	g_tc_resume = 1;	/* set "1", don't read temp during suspend */

	/* if(talking_flag==false) */
	{
		/* tscpu_printk("tscpu_thermal_suspend no talking\n"); */

		while (cnt < 50) {
			temp = (DRV_Reg32(THAHBST0) >> 16);
			if (cnt > 10)
				tscpu_printk(KERN_CRIT "ST0 = 0x%x,cnt=%d %d\n",
					     temp, cnt, __LINE__);
			if (temp == 0x0) {
				/* pause all periodoc temperature sensing
				 * point 0~2
				 */
				thermal_pause_all_periodoc_temp_sensing();
				break;
			}
			tscpu_printk(KERN_CRIT "1 THAHBST0 = 0x%x, %d\n",
				     temp, __LINE__);
			udelay(2);
			cnt++;
		}

		/* disable periodic temp measurement on sensor 0~2 */
		thermal_disable_all_periodoc_temp_sensing(); /* TEMPMONCTL0 */
	}
}

void tscpu_release_tc(void)
{
	int temp = 0;
	int cnt = 0;

	/* tscpu_printk("tscpu_release_tc\n"); */

	g_tc_resume = 1; /* set "1", don't read temp during start resume */

	/* if(talking_flag==false) */
	{

		tscpu_reset_thermal();

		/* add this function to read all temp first to avoid
		 * write TEMPPROTTC first time will issue an fake signal to RGU
		 */
		tscpu_fast_initial_sw_workaround();


		while (cnt < 50) {
			temp = (DRV_Reg32(THAHBST0) >> 16);
			if (cnt > 10)
				tscpu_printk(KERN_CRIT "ST0 = 0x%x,cnt=%d,%d\n",
					     temp, cnt, __LINE__);
			if (temp == 0x0) {
				/* pause all periodoc temperature sensing point
				 * 0~2
				 * TEMPMSRCTL1
				 */
				thermal_pause_all_periodoc_temp_sensing();
				break;
			}
			udelay(2);
			cnt++;
		}
		thermal_disable_all_periodoc_temp_sensing(); /* TEMPMONCTL0 */

		thermal_initial_all_bank();

		/* must release before start */
		thermal_release_all_periodoc_temp_sensing();

		tscpu_clear_all_temp();

		tscpu_config_all_tc_hw_protect(trip_temp[0], tc_mid_trip);
	}

	g_tc_resume = 2;	/* set "2", resume finish,can read temp */
}
#endif

void tscpu_cancel_thermal_timer(void)
{
	/* cancel timer */
	/* tscpu_dprintk("tscpu_update_temperature_timer_init\n"); */
	hrtimer_cancel(&ts_tempinfo_hrtimer);
#if THERMAL_TURNOFF_AUXADC_BEFORE_DEEPIDLE
	tscpu_pause_tc();

#if 1
	clk_disable_unprepare(clk_auxadc);
#else
	if (disable_clock(MT_CG_INFRA_AUXADC, "AUXADC"))
		tscpu_printk("hwEnableClock AUXADC failed\n");
#endif

#if defined(CONFIG_MTK_CLKMGR)
	if (clock_is_on(MT_CG_INFRA_AUXADC))
		tscpu_printk("hwEnableClock AUXADC still on\n");
#endif
#endif

	/* stop thermal framework polling when entering deep idle */
	if (thz_dev)
		cancel_delayed_work(&(thz_dev->poll_queue));
}


void tscpu_start_thermal_timer(void)
{
	ktime_t ktime;
#if THERMAL_TURNOFF_AUXADC_BEFORE_DEEPIDLE
#if 1
	if (clk_prepare_enable(clk_auxadc))
#else
	if (enable_clock(MT_CG_INFRA_AUXADC, "AUXADC"))
#endif
		tscpu_printk("hwEnableClock AUXADC failed\n");

	udelay(15);
#if defined(CONFIG_MTK_CLKMGR)
	if ((clock_is_on(MT_CG_INFRA_AUXADC) == 0x0))
		tscpu_printk("hwEnableClock AUXADC failed\n");
#endif

	/* if((DRV_Reg32(TS_CON1)& 0x00000030) == 0x11) */
	/* tscpu_printk("TS_CON1=0x%x\n",DRV_Reg32(TS_CON1)); */


	tscpu_release_tc();
#endif

	/* start timer */
	ktime = ktime_set(0, 50000000);	/* 50ms */
	hrtimer_start(&ts_tempinfo_hrtimer, ktime, HRTIMER_MODE_REL);

	/* resume thermal framework polling when leaving deep idle */
	if (thz_dev != NULL && interval != 0)
		/* 60ms */
		mod_delayed_work(system_freezable_wq, &(thz_dev->poll_queue),
				 round_jiffies(msecs_to_jiffies(60)));
}

static int thermal_fast_init(void)
{
	UINT32 temp = 0;
	UINT32 cunt = 0;
	/* UINT32 temp1 = 0,temp2 = 0,temp3 = 0,count=0; */

	/* tscpu_printk( "thermal_fast_init\n"); */


	temp = 0xDA1;
	/* write temp to spare register */
	DRV_WriteReg32(PTPSPARE2, (0x00001000 + temp));

	/* counting unit is 320 * 31.25us = 10ms */
	DRV_WriteReg32(TEMPMONCTL1, 1);

	/* sensing interval is 200 * 10ms = 2000ms */
	DRV_WriteReg32(TEMPMONCTL2, 1);

	/* polling interval to check if temperature sense is ready */
	DRV_WriteReg32(TEMPAHBPOLL, 1);

	/* exceed this polling time, IRQ would be inserted */
	DRV_WriteReg32(TEMPAHBTO, 0x000000FF);

	/* times for interrupt occurrance */
	DRV_WriteReg32(TEMPMONIDET0, 0x00000000);

	/* times for interrupt occurrance */
	DRV_WriteReg32(TEMPMONIDET1, 0x00000000);

	/* temperature measurement sampling control */
	DRV_WriteReg32(TEMPMSRCTL0, 0x0000000);

	/* this value will be stored to TEMPPNPMUXADDR
	 * (TEMPSPARE0) automatically by hw
	 */
	DRV_WriteReg32(TEMPADCPNP0, 0x1);
	DRV_WriteReg32(TEMPADCPNP1, 0x2);
	DRV_WriteReg32(TEMPADCPNP2, 0x3);
	DRV_WriteReg32(TEMPADCPNP3, 0x4);

	/* read valid & voltage are at the same register */
	DRV_WriteReg32(TEMPRDCTRL, 0x0);

	/* indicate where the valid bit is (the 12th bit is valid bit
	 * and 1 is valid)
	 */
	DRV_WriteReg32(TEMPADCVALIDMASK, 0x0000002C);
	/* do not need to shift */
	DRV_WriteReg32(TEMPADCVOLTAGESHIFT, 0x0);

	/* enable auxadc mux & pnp write transaction */
	DRV_WriteReg32(TEMPADCWRITECTRL, 0x3);

	/* enable all interrupt except filter sense and immediate sense
	 * interrupt
	 */
	DRV_WriteReg32(TEMPMONINT, 0x00000000);

	/* enable all sensing point (sensing point 2 is unused) */
	DRV_WriteReg32(TEMPMONCTL0, 0x0000000F);

	cunt = 0;
	temp = DRV_Reg32(TEMPMSR0) & 0x0fff;
	while (temp != 0xDA1 && cunt < 20) {
		cunt++;
		tscpu_dprintk("[Power/CPU_Thermal]0 temp=%d,cunt=%d\n",
			      temp, cunt);
		temp = DRV_Reg32(TEMPMSR0) & 0x0fff;
	}

	cunt = 0;
	temp = DRV_Reg32(TEMPMSR1) & 0x0fff;
	while (temp != 0xDA1 && cunt < 20) {
		cunt++;
		tscpu_dprintk("[Power/CPU_Thermal]1 temp=%d,cunt=%d\n",
			      temp, cunt);
		temp = DRV_Reg32(TEMPMSR1) & 0x0fff;
	}

	cunt = 0;
	temp = DRV_Reg32(TEMPMSR2) & 0x0fff;
	while (temp != 0xDA1 && cunt < 20) {
		cunt++;
		tscpu_dprintk("[Power/CPU_Thermal]2 temp=%d,cunt=%d\n",
			      temp, cunt);
		temp = DRV_Reg32(TEMPMSR2) & 0x0fff;
	}

	cunt = 0;
	temp = DRV_Reg32(TEMPMSR3) & 0x0fff;
	while (temp != 0xDA1 && cunt < 20) {
		cunt++;
		tscpu_dprintk("[Power/CPU_Thermal]3 temp=%d,cunt=%d\n",
			      temp, cunt);
		temp = DRV_Reg32(TEMPMSR3) & 0x0fff;
	}

	return 0;
}


#ifdef CONFIG_OF
static u64 of_get_phys_base(struct device_node *np)
{
	u64 size64;
	const __be32 *regaddr_p;

	regaddr_p = of_get_address(np, 0, &size64, NULL);
	if (!regaddr_p)
		return OF_BAD_ADDR;

	return of_translate_address(np, regaddr_p);
}

static int get_io_reg_base(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;

	if (node) {
		/* Setup IO addresses */
		thermal_base = of_iomap(node, 0);
		tscpu_printk("[THERM_CTRL] thermal_base=0x%lx\n",
			     (unsigned long)thermal_base);

		/* get thermal phy base */
		thermal_phy_base = of_get_phys_base(node);
		tscpu_printk("[THERM_CTRL] thermal_phy_base=0x%lx\n",
			     (unsigned long)thermal_phy_base);

		/* get thermal irq num */
		thermal_irq_number = irq_of_parse_and_map(node, 0);
		tscpu_printk("[THERM_CTRL] thermal_irq_number=0x%lx\n",
				(unsigned long)thermal_irq_number);

		apmixed_base = of_parse_phandle(node, "apmixedsys", 0);
		tscpu_printk("[THERM_CTRL] apmixed_base=0x%lx\n",
			     (unsigned long)apmixed_base);

		apmixed_phy_base = of_get_phys_base(apmixed_base);
		tscpu_printk("[THERM_CTRL] apmixed_phy_base=0x%lx\n",
			     (unsigned long)apmixed_phy_base);
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8163-auxadc");
	if (!node) {
		pr_err("[THERM_CTRL] find node failed\n");
		return -ENODEV;
	}

	auxadc_ts_base = of_iomap(node, 0);
	if (!auxadc_ts_base) {
		pr_err("[THERM_CTRL] get auxadc base failed\n");
		return -ENOMEM;
	}
	tscpu_printk("auxadc_base=0x%lx\n", (unsigned long)auxadc_ts_base);

	auxadc_ts_phy_base = of_get_phys_base(node);
		tscpu_printk("[THERM_CTRL] auxadc_ts_phy_base=0x%lx\n",
				(unsigned long)auxadc_ts_phy_base);

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8163-infracfg");
	if (!node) {
		tscpu_printk("get INFRACFG_AO failed\n");
		return -ENODEV;
	}
	INFRACFG_AO_BASE = of_iomap(node, 0);
	if (!INFRACFG_AO_BASE) {
		tscpu_printk("INFRACFG_AO iomap failed\n");
		return -ENOMEM;
	}
	tscpu_printk("[THERM_CTRL] INFRACFG_AO_BASE=0x%lx\n",
		     (unsigned long)INFRACFG_AO_BASE);

	return 1;
}
#endif

#ifdef CONFIG_OF
const long tscpu_dev_alloc_module_base_by_name(const char *name)
{
	unsigned long VA;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, name);
	if (!node) {
		tscpu_printk("find node failed\n");
		return 0;
	}
	VA = (unsigned long)of_iomap(node, 0);
	tscpu_printk("DEV: VA(%s): 0x%lx\n", name, VA);

	return VA;
}
#endif

/*
 * Generic Thermal framework interface
 */
static DEFINE_MUTEX(thermal_lock);

/* TODO: move this to dt */
static struct mtk_thermal_platform_data tscpu_thermal_data = {
	.num_trips = THERMAL_MAX_TRIPS,
	.mode = THERMAL_DEVICE_DISABLED,
	.polling_delay = 500,
	.num_cdevs = 2,
	.trips[0] = {.temp = 95000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 0, .lower = 0},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 1, .lower = 0},
	},
	.trips[1] = {.temp = 97000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 0, .lower = 0},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 2, .lower = 1},
	},
	.trips[2] = {.temp = 99000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 0, .lower = 0},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 3, .lower = 2},
	},
	.trips[3] = {.temp = 101000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 0, .lower = 0},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 4, .lower = 3},
	},
	.trips[4] = {.temp = 103000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 1, .lower = 0},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 5, .lower = 4},
	},
	.trips[5] = {.temp = 105000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 2, .lower = 1},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 6, .lower = 5},
	},
	.trips[6] = {.temp = 107000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 3, .lower = 2},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 7, .lower = 6},
	},
	.trips[7] = {.temp = 109000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 4, .lower = 3},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 7, .lower = 7},
	},
	.trips[8] = {.temp = 111000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 5, .lower = 4},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 7, .lower = 7},
	},
	.trips[9] = {.temp = 113000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 6, .lower = 5},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 7, .lower = 7},
	},
	.trips[10] = {.temp = 115000, .type = THERMAL_TRIP_ACTIVE, .hyst = 0,
		     .cdev[0] = {
			.type = "thermal-cpufreq-0", .upper = 7, .lower = 6},
		     .cdev[1] = {
			.type = "thermal-cpufreq-1", .upper = 7, .lower = 7},
	},
	.trips[11] = {.temp = MTKTSCPU_TEMP_CRIT, .type = THERMAL_TRIP_CRITICAL,
			.hyst = 0},
};

static int tscpu_match_cdev(struct thermal_cooling_device *cdev,
			       struct trip_t *trip,
			       int *index)
{
	int i;
	if (!strlen(cdev->type))
		return -EINVAL;

	for (i = 0; i < THERMAL_MAX_TRIPS; i++)
		if (!strcmp(cdev->type, trip->cdev[i].type)) {
			*index = i;
			return 0;
		}
	return -ENODEV;
}

static int tscpu_cdev_bind(struct thermal_zone_device *thermal,
			      struct thermal_cooling_device *cdev)
{
	struct tscpu_thermal_zone *tz = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tz->pdata;
	struct trip_t *trip = NULL;
	int index = -1;
	struct cdev_t *cool_dev;
	unsigned long max_state, upper, lower;
	int i, ret = -EINVAL;

	cdev->ops->get_max_state(cdev, &max_state);

	for (i = 0; i < pdata->num_trips; i++) {
		trip = &pdata->trips[i];

		if (tscpu_match_cdev(cdev, trip, &index))
			continue;

		if (index == -1)
			return -EINVAL;

		cool_dev = &(trip->cdev[index]);
		lower = cool_dev->lower;
		upper =  cool_dev->upper > max_state ? max_state :
			cool_dev->upper;
		ret = thermal_zone_bind_cooling_device(thermal,
						       i,
						       cdev,
						       upper,
						       lower);
		dev_info(&cdev->device, "%s bind to %d: idx=%d: u=%ld:"
			 "l=%ld: %d-%s\n", cdev->type,
			 i, index, upper, lower, ret, ret ? "fail" : "succeed");
	}
	return ret;
}

static int tscpu_cdev_unbind(struct thermal_zone_device *thermal,
				struct thermal_cooling_device *cdev)
{
	struct tscpu_thermal_zone *tz = thermal->devdata;
	struct mtk_thermal_platform_data *pdata = tz->pdata;
	struct trip_t *trip;
	int i, ret = -EINVAL;
	int index = -1;

	for (i = 0; i < pdata->num_trips; i++) {
		trip = &pdata->trips[i];
		if (tscpu_match_cdev(cdev, trip, &index))
			continue;
		ret = thermal_zone_unbind_cooling_device(thermal, i, cdev);
		dev_info(&cdev->device, "%s unbind from %d: %s\n", cdev->type,
			 i, ret ? "fail" : "succeed");
	}
	return ret;
}

static void tscpu_work(struct work_struct *work)
{
	struct tscpu_thermal_zone *tz;
	struct mtk_thermal_platform_data *pdata;

	mutex_lock(&thermal_lock);
	tz = container_of(work, struct tscpu_thermal_zone, thermal_work);
	if (!tz)
		return;
	pdata = tz->pdata;
	if (!pdata)
		return;
	if (pdata->mode == THERMAL_DEVICE_ENABLED)
		thermal_zone_device_update(tz->tz);
	mutex_unlock(&thermal_lock);
}

static ssize_t trips_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	return sprintf(buf, "%d\n", thz_dev->trips);
}

static ssize_t trips_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int trips = 0;

	if (sscanf(buf, "%d\n", &trips) != 1)
		return -EINVAL;
	if (trips < 0)
		return -EINVAL;

	num_trip = trips;

	return count;
}

static ssize_t polling_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return sprintf(buf, "%d\n", thz_dev->polling_delay);
}

static ssize_t polling_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int polling_delay = 0;

	if (sscanf(buf, "%d\n", &polling_delay) != 1)
		return -EINVAL;
	if (polling_delay < 0)
		return -EINVAL;

	thz_dev->polling_delay = polling_delay;
	thermal_zone_device_update(thz_dev);

	return count;
}

#if 1
static DEVICE_ATTR(trips, S_IRUGO | S_IWUSR, trips_show, trips_store);
static DEVICE_ATTR(polling, S_IRUGO | S_IWUSR, polling_show, polling_store);

static int tscpu_create_sysfs(void)
{
	int ret = 0;

	ret = device_create_file(&thz_dev->device, &dev_attr_polling);
	if (ret)
		pr_err("%s Failed to create polling attr\n", __func__);

	ret = device_create_file(&thz_dev->device, &dev_attr_trips);
	if (ret)
		pr_err("%s Failed to create trips attr\n", __func__);

	return ret;
}
#endif


static int tscpu_init(struct platform_device *pdev)
{
	int err = 0;
	int temp = 0;
	int cnt = 0;

	pr_debug("tscpu_thermal_probe\n");

	clk_peri_therm = devm_clk_get(&pdev->dev, "therm");
	BUG_ON(IS_ERR(clk_peri_therm));

	clk_auxadc = devm_clk_get(&pdev->dev, "auxadc");
	BUG_ON(IS_ERR(clk_auxadc));

#ifdef CONFIG_OF
	if (get_io_reg_base(pdev) == 0)
		return 0;
#endif
	thermal_cal_prepare();
	thermal_calibration();

	tscpu_reset_thermal();

	/*
	 * TS_CON1 default is 0x30, this is buffer off
	 * we should turn on this buffer berore we use thermal sensor,
	 * or this buffer off will let TC read a very small value from auxadc
	 * and this small value will trigger thermal reboot
	 */
	temp = DRV_Reg32(TS_CON1);
	pr_debug("tscpu_thermal_probe :TS_CON1=0x%x\n", temp);
	/* TS_CON1[5:4]=2'b00,   00: Buffer on, TSMCU to AUXADC */
	temp &= ~(0x00000030);
	/* read abb need */
	THERMAL_WRAP_WR32(temp, TS_CON1);

	udelay(200);

	/*
	 * RG_TS2AUXADC < set from 2'b11 to 2'b00 when resume.
	 * wait 100uS than turn on thermal controller.
	 * add this function to read all temp first to avoid
	 * write TEMPPROTTC first will issue an fake signal to RGU
	 */
	tscpu_fast_initial_sw_workaround();

	while (cnt < 50) {
		temp = (DRV_Reg32(THAHBST0) >> 16);
		if (cnt > 10)
			pr_debug("THAHBST0 = 0x%x,cnt=%d, %d\n", temp, cnt,
								 __LINE__);
		if (temp == 0x0) {
			/*
			 * pause all periodoc temperature sensing point 0~2
			 * TEMPMSRCTL1
			 */
			thermal_pause_all_periodoc_temp_sensing();
			break;
		}
		udelay(2);
		cnt++;
	}

	thermal_disable_all_periodoc_temp_sensing();	/* TEMPMONCTL0 */

	pr_debug("cnt = %d, %d\n", cnt, __LINE__);

	/* Normal initial */
	thermal_initial_all_bank();

	/* TEMPMSRCTL1 must release before start */
	thermal_release_all_periodoc_temp_sensing();

	read_all_bank_temperature();

	tscpu_update_temperature_timer_init();

#ifdef CONFIG_OF
	err = request_irq(thermal_irq_number,
			  thermal_all_bank_interrupt_handler, IRQF_TRIGGER_LOW,
			  THERMAL_NAME, NULL);
	if (err)
		pr_err("tscpu_init IRQ register fail\n");
#else
	err = request_irq(THERM_CTRL_IRQ_BIT_ID,
			   thermal_all_bank_interrupt_handler, IRQF_TRIGGER_LOW,
			   THERMAL_NAME, NULL);
	if (err)
		pr_err("tscpu_init IRQ register fail\n");
#endif

	tscpu_config_all_tc_hw_protect(trip_temp[0], tc_mid_trip);

	if (err) {
		pr_err("thermal driver callback register failed..\n");
		return err;
	}

	return err;
}

static int tscpu_get_trend(struct thermal_zone_device *thermal,
					    int trip, enum thermal_trend *trend)
{
	int ret;
	unsigned long trip_temp;

	ret = tscpu_get_trip_temp(thermal, trip, &trip_temp);
	if (ret < 0)
		return ret;

	if (thermal->temperature >= trip_temp)
		*trend = THERMAL_TREND_RAISE_FULL;
	else
		*trend = THERMAL_TREND_DROPPING;

	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops tscpu_dev_ops = {
	.bind = tscpu_cdev_bind,
	.unbind = tscpu_cdev_unbind,
	.get_temp = tscpu_get_temp,
	.get_mode = tscpu_get_mode,
	.set_mode = tscpu_set_mode,
	.get_trip_type = tscpu_get_trip_type,
	.get_trip_temp = tscpu_get_trip_temp,
	.set_trip_temp = tscpu_set_trip_temp,
	.get_trip_hyst = tscpu_get_trip_hyst,
	.set_trip_hyst = tscpu_set_trip_hyst,
	.get_crit_temp = tscpu_get_crit_temp,
	.get_trend = tscpu_get_trend,
	.notify = tscpu_thermal_notify,
};

static int tscpu_thermal_probe(struct platform_device *pdev)
{
	int ret = 0, mask = 0;
	struct tscpu_thermal_zone *tz;
	struct mtk_thermal_platform_data *pdata = &tscpu_thermal_data;

	if (!pdata)
		return -EINVAL;

	ret = tscpu_init(pdev);
	if (ret) {
		pr_err("%s Error tscpu_int\n", __func__);
		return -EINVAL;
	}

	tz = devm_kzalloc(&pdev->dev, sizeof(*tz), GFP_KERNEL);
	if (!tz)
		return -ENOMEM;

	memset(tz, 0, sizeof(*tz));
	tz->pdata = pdata;
	mask = BIT(pdata->num_trips) - 1;
	tz->tz = thermal_zone_device_register("mtktscpu", pdata->num_trips,
					      mask, tz, &tscpu_dev_ops, NULL, 0,
					      pdata->polling_delay);
	if (IS_ERR(tz->tz)) {
		pr_err("%s Failed to register tscpu thermal zone device\n",
			 __func__);
		return -EINVAL;
	}

	thz_dev = tz->tz;

	ret = tscpu_create_sysfs();
	if (ret)
		pr_err("%s Failed to create thermal sysfs attr\n", __func__);

	INIT_WORK(&tz->thermal_work, tscpu_work);
	pdata->mode = THERMAL_DEVICE_DISABLED;
	platform_set_drvdata(pdev, tz);
	return ret;
}

static struct platform_driver mtk_thermal_driver = {
	.remove = NULL,
	.shutdown = NULL,
	.probe = tscpu_thermal_probe,
	.suspend = tscpu_thermal_suspend,
	.resume = tscpu_thermal_resume,
	.driver = {
		.name = THERMAL_NAME,
#ifdef CONFIG_OF
		.of_match_table = mt_thermal_of_match,
#endif
	},
};


static int __init mtk_thermal_init(void)
{
	return platform_driver_register(&mtk_thermal_driver);
}

static void __exit mtk_thermal_exit(void)
{

#if THERMAL_DRV_UPDATE_TEMP_DIRECT_TO_MET
	mt_thermalsampler_registerCB(NULL);
#endif
}
module_init(mtk_thermal_init);
module_exit(mtk_thermal_exit);
