/******************************************************************************
 * pwrap_hal.c - Linux pmic_wrapper Driver,hardware_dependent driver
 *
 *
 * DESCRIPTION:
 *     This file provid the other drivers PMIC wrapper relative functions
 *
 ******************************************************************************/

#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/io.h>
#ifdef CONFIG_OF
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include <mt_pmic_wrap.h>
#include "pwrap_hal.h"
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/module.h>

#define PMIC_WRAP_DEVICE "pmic_wrap"

/*-----start-- global variable-------------------------------------------------*/
#ifdef CONFIG_OF
u32 pwrap_irq;
void __iomem *pwrap_base;
void __iomem *infracfg_ao_base;
static void __iomem *topckgen_base;
static void __iomem *pwrap_p2p_base;
static void __iomem *scp_clk_ctrl_base;
#endif

static struct mt_pmic_wrap_driver *mt_wrp;

static DEFINE_SPINLOCK(wrp_lock);
static DEFINE_SPINLOCK(wrp_lock_isr);
/* ----------interral API ------------------------ */
static s32 _pwrap_init_dio(u32 dio_en);
static s32 _pwrap_init_cipher(void);
static s32 _pwrap_init_reg_clock(u32 regck_sel);

static s32 _pwrap_wacs2_nochk(u32 write, u32 adr, u32 wdata, u32 *rdata);
static s32 pwrap_write_nochk(u32 adr, u32 wdata);
static s32 pwrap_read_nochk(u32 adr, u32 *rdata);

struct pmic_wrapper {
	struct device *dev;
	struct regmap *regmap;
	struct clk *clk_wrap;
	struct clk *clk_spi;
};

#ifdef CONFIG_OF
static int pwrap_of_iomap(void);
static void pwrap_of_iounmap(void);
#endif

static inline void pwrap_dump_ap_register(void)
{
	u32 i = 0;
	u32 reg_value = 0;

	PWRAPREG("dump pwrap register, base=0x%p\n", PMIC_WRAP_BASE);
	PWRAPREG("address     :   3 2 1 0    7 6 5 4    B A 9 8    F E D C\n");
	for (i = 0; i <= PMIC_WRAP_REG_RANGE; i += 4) {
		PWRAPREG("offset 0x%.3x:0x%.8x 0x%.8x 0x%.8x 0x%.8x\n", i * 4,
			 WRAP_RD32(PMIC_WRAP_BASE + (i * 4) + 0),
			 WRAP_RD32(PMIC_WRAP_BASE + (i * 4) + 0x4),
			 WRAP_RD32(PMIC_WRAP_BASE + (i * 4) + 0x8),
			 WRAP_RD32(PMIC_WRAP_BASE + (i * 4) + 0xC));
	}
	/* PWRAPREG("elapse_time=%llx(ns)\n",elapse_time); */
	/* PWRAPREG("dump pwrap module clock register, check [3:0]\n"); */
	/* PWRAPREG("Clock Gating Register 0xF0001090 = 0x%.8x\n", WRAP_RD32(0xF0001090)); */
	/* PWRAPREG("dump pwrap bus dcm register, check [22]\n"); */
	/* PWRAPREG("Bus DCM Register 0xF0001074 = 0x%.8x\n", WRAP_RD32(0xF0001074)); */
	pwrap_read_nochk(0x200, &reg_value);
	PWRAPREG("PMIC HW CID = 0x%x\n", reg_value);
}

static inline void pwrap_dump_all_register(void)
{
	pwrap_dump_ap_register();
}

/******************************************************************************
  wrapper timeout
 ******************************************************************************/
#define PWRAP_TIMEOUT
#ifdef PWRAP_TIMEOUT
static u64 _pwrap_get_current_time(void)
{
	return sched_clock();	/* /TODO: fix me */
}

/* u64 elapse_time=0; */

static bool _pwrap_timeout_ns(u64 start_time_ns, u64 timeout_time_ns)
{
	u64 cur_time = 0;
	u64 elapse_time = 0;

	/* get current tick */
	cur_time = _pwrap_get_current_time();	/* ns */

	/* avoid timer over flow exiting in FPGA env */
	if (cur_time < start_time_ns) {
		PWRAPERR("@@@@Timer overflow! start%lld cur timer%lld\n", start_time_ns, cur_time);
		start_time_ns = cur_time;
		timeout_time_ns = 10000 * 1000;	/* 10000us */
		PWRAPERR("@@@@reset timer! start%lld setting%lld\n", start_time_ns,
			 timeout_time_ns);
	}

	elapse_time = cur_time - start_time_ns;

	/* check if timeout */
	if (timeout_time_ns <= elapse_time) {
		/* timeout */
		PWRAPERR("@@@@Timeout: elapse time%lld,start%lld setting timer%lld\n",
			 elapse_time, start_time_ns, timeout_time_ns);
		return true;
	}
	return false;
}

static u64 _pwrap_time2ns(u64 time_us)
{
	return time_us * 1000;
}

#else
static u64 _pwrap_get_current_time(void)
{
	return 0;
}

static bool _pwrap_timeout_ns(u64 start_time_ns, u64 elapse_time)	/* ,u64 timeout_ns) */
{
	return false;
}

static u64 _pwrap_time2ns(u64 time_us)
{
	return 0;
}

#endif
/* ##################################################################### */
/* define macro and inline function (for do while loop) */
/* ##################################################################### */
typedef u32(*loop_condition_fp) (u32);	/* define a function pointer */

static inline u32 wait_for_fsm_idle(u32 x)
{
	return (GET_WACS0_FSM(x) != WACS_FSM_IDLE);
}

static inline u32 wait_for_fsm_vldclr(u32 x)
{
	return (GET_WACS0_FSM(x) != WACS_FSM_WFVLDCLR);
}

static inline u32 wait_for_sync(u32 x)
{
	return (GET_SYNC_IDLE0(x) != WACS_SYNC_IDLE);
}

static inline u32 wait_for_idle_and_sync(u32 x)
{
	return ((GET_WACS2_FSM(x) != WACS_FSM_IDLE) || (GET_SYNC_IDLE2(x) != WACS_SYNC_IDLE));
}

static inline u32 wait_for_wrap_idle(u32 x)
{
	return ((GET_WRAP_FSM(x) != 0x0) || (GET_WRAP_CH_DLE_RESTCNT(x) != 0x0));
}

static inline u32 wait_for_wrap_state_idle(u32 x)
{
	return (GET_WRAP_AG_DLE_RESTCNT(x) != 0);
}

static inline u32 wait_for_man_idle_and_noreq(u32 x)
{
	return ((GET_MAN_REQ(x) != MAN_FSM_NO_REQ) || (GET_MAN_FSM(x) != MAN_FSM_IDLE));
}

static inline u32 wait_for_man_vldclr(u32 x)
{
	return (GET_MAN_FSM(x) != MAN_FSM_WFVLDCLR);
}

static inline u32 wait_for_cipher_ready(u32 x)
{
	return (x != 3);
}

static inline u32 wait_for_stdupd_idle(u32 x)
{
	return (GET_STAUPD_FSM(x) != 0x0);
}

static inline u32 wait_for_state_ready_init(loop_condition_fp fp, u32 timeout_us,
					    void *wacs_register, u32 *read_reg)
{

	u64 start_time_ns = 0, timeout_ns = 0;
	u32 reg_rdata = 0x0;

	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("wait_for_state_ready_init timeout when waiting for idle\n");
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
	} while (fp(reg_rdata));	/* IDLE State */
	if (read_reg)
		*read_reg = reg_rdata;
	return 0;
}

static inline u32 wait_for_state_idle_init(loop_condition_fp fp, u32 timeout_us,
					   void *wacs_register, void *wacs_vldclr_register,
					   u32 *read_reg)
{

	u64 start_time_ns = 0, timeout_ns = 0;
	u32 reg_rdata;

	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("wait_for_state_idle_init timeout when waiting for idle\n");
			pwrap_dump_ap_register();
			/* pwrap_trace_wacs2(); */
			/* BUG_ON(1); */
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
		/* if last read command timeout,clear vldclr bit */
		/* read command state machine:FSM_REQ-->wfdle-->WFVLDCLR;write:FSM_REQ-->idle */
		switch (GET_WACS0_FSM(reg_rdata)) {
		case WACS_FSM_WFVLDCLR:
			WRAP_WR32(wacs_vldclr_register, 1);
			PWRAPERR("WACS_FSM = PMIC_WRAP_WACS_VLDCLR\n");
			break;
		case WACS_FSM_WFDLE:
			PWRAPERR("WACS_FSM = WACS_FSM_WFDLE\n");
			break;
		case WACS_FSM_REQ:
			PWRAPERR("WACS_FSM = WACS_FSM_REQ\n");
			break;
		default:
			break;
		}
	} while (fp(reg_rdata));	/* IDLE State */
	if (read_reg)
		*read_reg = reg_rdata;
	return 0;
}

static inline u32 wait_for_state_idle(loop_condition_fp fp, u32 timeout_us, void *wacs_register,
				      void *wacs_vldclr_register, u32 *read_reg)
{

	u64 start_time_ns = 0, timeout_ns = 0;
	u32 reg_rdata;

	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("wait_for_state_idle timeout when waiting for idle\n");
			pwrap_dump_ap_register();
			/* pwrap_trace_wacs2(); */
			/* BUG_ON(1); */
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
		if (GET_INIT_DONE0(reg_rdata) != WACS_INIT_DONE) {
			PWRAPERR("initialization isn't finished\n");
			return E_PWR_NOT_INIT_DONE;
		}
		/* if last read command timeout,clear vldclr bit */
		/* read command state machine:FSM_REQ-->wfdle-->WFVLDCLR;write:FSM_REQ-->idle */
		switch (GET_WACS0_FSM(reg_rdata)) {
		case WACS_FSM_WFVLDCLR:
			WRAP_WR32(wacs_vldclr_register, 1);
			PWRAPERR("WACS_FSM = PMIC_WRAP_WACS_VLDCLR\n");
			break;
		case WACS_FSM_WFDLE:
			PWRAPERR("WACS_FSM = WACS_FSM_WFDLE\n");
			break;
		case WACS_FSM_REQ:
			PWRAPERR("WACS_FSM = WACS_FSM_REQ\n");
			break;
		default:
			break;
		}
	} while (fp(reg_rdata));	/* IDLE State */
	if (read_reg)
		*read_reg = reg_rdata;
	return 0;
}

/********************************************************************************************/
/* extern API for PMIC driver, INT related control, this INT is for PMIC chip to AP */
/********************************************************************************************/
u32 mt_pmic_wrap_eint_status(void)
{
	return WRAP_RD32(PMIC_WRAP_EINT_STA);
}

void mt_pmic_wrap_eint_clr(int offset)
{
	if ((offset < 0) || (offset > 3))
		PWRAPERR("clear EINT flag error, only 0-3 bit\n");
	else
		WRAP_WR32(PMIC_WRAP_EINT_CLR, (1 << offset));

	PWRAPREG("clear EINT flag mt_pmic_wrap_eint_status=0x%x\n", WRAP_RD32(PMIC_WRAP_EINT_STA));
}

static inline u32 wait_for_state_ready(loop_condition_fp fp, u32 timeout_us, void *wacs_register,
				       u32 *read_reg)
{

	u64 start_time_ns = 0, timeout_ns = 0;
	u32 reg_rdata;

	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("timeout when waiting for idle\n");
			pwrap_dump_ap_register();
			/* pwrap_trace_wacs2(); */
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);

		if (GET_INIT_DONE0(reg_rdata) != WACS_INIT_DONE) {
			PWRAPERR("initialization isn't finished\n");
			return E_PWR_NOT_INIT_DONE;
		}
	} while (fp(reg_rdata));	/* IDLE State */
	if (read_reg)
		*read_reg = reg_rdata;
	return 0;
}

/* -------------------------------------------------------- */
/* Function : pwrap_wacs2_hal() */
/* Description : */
/* Parameter : */
/* Return : */
/* -------------------------------------------------------- */
s32 pwrap_wacs2_hal(u32 write, u32 adr, u32 wdata, u32 *rdata)
{
	/* u64 wrap_access_time=0x0; */
	u32 reg_rdata = 0;
	u32 wacs_write = 0;
	u32 wacs_adr = 0;
	u32 wacs_cmd = 0;
	u32 return_value = 0;
	unsigned long flags = 0;
	/* PWRAPFUC(); */
	/* #ifndef CONFIG_MTK_LDVT_PMIC_WRAP */
	/* PWRAPLOG("wrapper access,write=%x,add=%x,wdata=%x,rdata=%x\n",write,adr,wdata,rdata); */
	/* #endif */
	/* Check argument validation */
	if ((write & ~(0x1)) != 0)
		return E_PWR_INVALID_RW;
	if ((adr & ~(0xffff)) != 0)
		return E_PWR_INVALID_ADDR;
	if ((wdata & ~(0xffff)) != 0)
		return E_PWR_INVALID_WDAT;

	spin_lock_irqsave(&wrp_lock, flags);
	/* Check IDLE & INIT_DONE in advance */
	return_value =
	    wait_for_state_idle(wait_for_fsm_idle, TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS2_RDATA,
				PMIC_WRAP_WACS2_VLDCLR, 0);
	if (return_value != 0) {
		PWRAPERR("wait_for_fsm_idle fail,return_value=%d\n", return_value);
		goto FAIL;
	}
	wacs_write = write << 31;
	wacs_adr = (adr >> 1) << 16;
	wacs_cmd = wacs_write | wacs_adr | wdata;

	WRAP_WR32(PMIC_WRAP_WACS2_CMD, wacs_cmd);
	if (write == 0) {
		if (NULL == rdata) {
			PWRAPERR("rdata is a NULL pointer\n");
			return_value = E_PWR_INVALID_ARG;
			goto FAIL;
		}
		return_value =
		    wait_for_state_ready(wait_for_fsm_vldclr, TIMEOUT_READ, PMIC_WRAP_WACS2_RDATA,
					 &reg_rdata);
		if (return_value != 0) {
			PWRAPERR("wait_for_fsm_vldclr fail,return_value=%d\n", return_value);
			return_value += 1;	/* E_PWR_NOT_INIT_DONE_READ or E_PWR_WAIT_IDLE_TIMEOUT_READ */
			goto FAIL;
		}
		*rdata = GET_WACS0_RDATA(reg_rdata);
		WRAP_WR32(PMIC_WRAP_WACS2_VLDCLR, 1);
	}
	/* spin_unlock_irqrestore(&wrp_lock,flags); */
FAIL:
	spin_unlock_irqrestore(&wrp_lock, flags);
	if (return_value != 0) {
		PWRAPERR("pwrap_wacs2_hal fail,return_value=%d\n", return_value);
		PWRAPERR("timeout:BUG_ON here\n");
		/* BUG_ON(1); */
	}
	/* wrap_access_time=sched_clock(); */
	/* pwrap_trace(wrap_access_time,return_value,write, adr, wdata,(u32)rdata); */
	return return_value;
}

/* s32 pwrap_wacs2( u32  write, u32  adr, u32  wdata, u32 *rdata ) */
/* { */
/* return pwrap_wacs2_hal(write, adr,wdata,rdata ); */
/* } */
/* EXPORT_SYMBOL(pwrap_wacs2); */
/* s32 pwrap_read( u32  adr, u32 *rdata ) */
/* { */
/* return pwrap_wacs2( PWRAP_READ, adr,0,rdata ); */
/* } */
/* EXPORT_SYMBOL(pwrap_read); */
/*  */
/* s32 pwrap_write( u32  adr, u32  wdata ) */
/* { */
/* return pwrap_wacs2( PWRAP_WRITE, adr,wdata,0 ); */
/* } */
/* EXPORT_SYMBOL(pwrap_write); */
/* ****************************************************************************** */
/* --internal API for pwrap_init------------------------------------------------- */
/* ****************************************************************************** */
/* -------------------------------------------------------- */
/* Function : _pwrap_wacs2_nochk() */
/* Description : */
/* Parameter : */
/* Return : */
/* -------------------------------------------------------- */

/* static s32 pwrap_read_nochk( u32  adr, u32 *rdata ) */
static s32 pwrap_read_nochk(u32 adr, u32 *rdata)
{
	return _pwrap_wacs2_nochk(0, adr, 0, rdata);
}

static s32 pwrap_write_nochk(u32 adr, u32 wdata)
{
	return _pwrap_wacs2_nochk(1, adr, wdata, 0);
}

static s32 _pwrap_wacs2_nochk(u32 write, u32 adr, u32 wdata, u32 *rdata)
{
	u32 reg_rdata = 0x0;
	u32 wacs_write = 0x0;
	u32 wacs_adr = 0x0;
	u32 wacs_cmd = 0x0;
	u32 return_value = 0x0;
	/* PWRAPFUC(); */
	/* Check argument validation */
	if ((write & ~(0x1)) != 0)
		return E_PWR_INVALID_RW;
	if ((adr & ~(0xffff)) != 0)
		return E_PWR_INVALID_ADDR;
	if ((wdata & ~(0xffff)) != 0)
		return E_PWR_INVALID_WDAT;

	/* Check IDLE */
	return_value =
	    wait_for_state_ready_init(wait_for_fsm_idle, TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS2_RDATA,
				      0);
	if (return_value != 0) {
		PWRAPERR("_pwrap_wacs2_nochk write command fail,return_value=%x\n", return_value);
		return return_value;
	}

	wacs_write = write << 31;
	wacs_adr = (adr >> 1) << 16;
	wacs_cmd = wacs_write | wacs_adr | wdata;
	WRAP_WR32(PMIC_WRAP_WACS2_CMD, wacs_cmd);

	if (write == 0) {
		if (NULL == rdata)
			return E_PWR_INVALID_ARG;
		/* wait for read data ready */
		return_value =
		    wait_for_state_ready_init(wait_for_fsm_vldclr, TIMEOUT_WAIT_IDLE,
					      PMIC_WRAP_WACS2_RDATA, &reg_rdata);
		if (return_value != 0) {
			PWRAPERR("_pwrap_wacs2_nochk read fail,return_value=%x\n", return_value);
			return return_value;
		}
		*rdata = GET_WACS0_RDATA(reg_rdata);
		WRAP_WR32(PMIC_WRAP_WACS2_VLDCLR, 1);
	}
	return 0;
}

/* -------------------------------------------------------- */
/* Function : _pwrap_init_dio() */
/* Description :call it in pwrap_init,mustn't check init done */
/* Parameter : */
/* Return : */
/* -------------------------------------------------------- */
static s32 _pwrap_init_dio(u32 dio_en)
{
	u32 arb_en_backup = 0x0;
	u32 rdata = 0x0;
	u32 return_value = 0;

	/* PWRAPFUC(); */
	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2);	/* only WACS2 */

#ifdef SLV_6325
	pwrap_write_nochk(MT6325_DEW_DIO_EN, (dio_en >> 1));
#endif
#ifdef SLV_6332
	pwrap_write_nochk(MT6332_DEW_DIO_EN, (dio_en >> 1));
#endif
#ifdef SLV_6323
	pwrap_write_nochk(MT6323_DEW_DIO_EN, dio_en);
#endif
	/* Check IDLE & INIT_DONE in advance */
	return_value =
	    wait_for_state_ready_init(wait_for_idle_and_sync, TIMEOUT_WAIT_IDLE,
				      PMIC_WRAP_WACS2_RDATA, 0);
	if (return_value != 0) {
		PWRAPERR("_pwrap_init_dio fail,return_value=%x\n", return_value);
		return return_value;
	}
	WRAP_WR32(PMIC_WRAP_DIO_EN, dio_en);
	/* Read Test */
#ifdef SLV_6325
	pwrap_read_nochk(MT6325_DEW_READ_TEST, &rdata);
	if (rdata != MT6325_DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("[Dio_mode][Read Test] fail,dio_en = %x, READ_TEST rdata=%x, exp=0x5aa5\n",
			 dio_en, rdata);
		return E_PWR_READ_TEST_FAIL;
	}
#endif
#ifdef SLV_6332
	pwrap_read_nochk(MT6332_DEW_READ_TEST, &rdata);
	if (rdata != MT6332_DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("[Dio_mode][Read Test] fail,dio_en = %x, READ_TEST rdata=%x, exp=0xa55a\n",
			 dio_en, rdata);
		return E_PWR_READ_TEST_FAIL;
	}
#endif
#ifdef SLV_6323
	pwrap_read_nochk(MT6323_DEW_READ_TEST, &rdata);
	if (rdata != MT6323_DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("[Dio_mode][Read Test] fail,dio_en = %x, READ_TEST rdata=%x, exp=0xa55a\n",
			 dio_en, rdata);
		return E_PWR_READ_TEST_FAIL;
	}
#endif
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);
	return 0;
}

/* -------------------------------------------------------- */
/* Function : _pwrap_init_cipher() */
/* Description : */
/* Parameter : */
/* Return : */
/* -------------------------------------------------------- */
static s32 _pwrap_init_cipher(void)
{
	u32 arb_en_backup = 0;
	u32 rdata = 0;
	u32 return_value = 0;
	u32 start_time_ns = 0, timeout_ns = 0;
	/* PWRAPFUC(); */
	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2);	/* only WACS0 */

	WRAP_WR32(PMIC_WRAP_CIPHER_SWRST, 1);
	WRAP_WR32(PMIC_WRAP_CIPHER_SWRST, 0);
	WRAP_WR32(PMIC_WRAP_CIPHER_KEY_SEL, 1);
	WRAP_WR32(PMIC_WRAP_CIPHER_IV_SEL, 2);
	WRAP_WR32(PMIC_WRAP_CIPHER_EN, 1);

	/* Config CIPHER @ PMIC */
#ifdef SLV_6325
	pwrap_write_nochk(MT6325_DEW_CIPHER_SWRST, 0x1);
	pwrap_write_nochk(MT6325_DEW_CIPHER_SWRST, 0x0);
	pwrap_write_nochk(MT6325_DEW_CIPHER_KEY_SEL, 0x1);
	pwrap_write_nochk(MT6325_DEW_CIPHER_IV_SEL, 0x2);
	pwrap_write_nochk(MT6325_DEW_CIPHER_EN, 0x1);
#endif
#ifdef SLV_6332
	pwrap_write_nochk(MT6332_DEW_CIPHER_SWRST, 0x1);
	pwrap_write_nochk(MT6332_DEW_CIPHER_SWRST, 0x0);
	pwrap_write_nochk(MT6332_DEW_CIPHER_KEY_SEL, 0x1);
	pwrap_write_nochk(MT6332_DEW_CIPHER_IV_SEL, 0x2);
	pwrap_write_nochk(MT6332_DEW_CIPHER_EN, 0x1);
#endif
#ifdef SLV_6323
	pwrap_write_nochk(MT6323_DEW_CIPHER_SWRST, 0x1);
	pwrap_write_nochk(MT6323_DEW_CIPHER_SWRST, 0x0);
	pwrap_write_nochk(MT6323_DEW_CIPHER_KEY_SEL, 0x1);
	pwrap_write_nochk(MT6323_DEW_CIPHER_IV_SEL, 0x2);
	pwrap_write_nochk(MT6323_DEW_CIPHER_EN, 0x1);
#endif
	/* wait for cipher data ready@AP */
	return_value =
	    wait_for_state_ready_init(wait_for_cipher_ready, TIMEOUT_WAIT_IDLE,
				      PMIC_WRAP_CIPHER_RDY, 0);
	if (return_value != 0) {
		PWRAPERR("wait for cipher data ready@AP fail,return_value=%x\n", return_value);
		return return_value;
	}
	/* wait for cipher data ready@PMIC */
#ifdef SLV_6325
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(0xFFFFFF);
	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("wait for cipher data ready@PMIC\n");
			/* pwrap_dump_all_register(); */
			/* return E_PWR_WAIT_IDLE_TIMEOUT; */
		}
		pwrap_read_nochk(MT6325_DEW_CIPHER_RDY, &rdata);
	} while (rdata != 0x1);	/* cipher_ready */

	pwrap_write_nochk(MT6325_DEW_CIPHER_MODE, 0x1);
#endif
#ifdef SLV_6332
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(0xFFFFFF);
	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("wait for cipher data ready@PMIC\n");
			/* pwrap_dump_all_register(); */
			/* return E_PWR_WAIT_IDLE_TIMEOUT; */
		}
		pwrap_read_nochk(MT6332_DEW_CIPHER_RDY, &rdata);
	} while (rdata != 0x1);	/* cipher_ready */

	pwrap_write_nochk(MT6332_DEW_CIPHER_MODE, 0x1);
#endif
#ifdef SLV_6323
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(0xFFFFFF);
	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("wait for cipher data ready@PMIC\n");
			/* pwrap_dump_all_register(); */
			/* return E_PWR_WAIT_IDLE_TIMEOUT; */
		}
		pwrap_read_nochk(MT6323_DEW_CIPHER_RDY, &rdata);
	} while (rdata != 0x1);	/* cipher_ready */

	pwrap_write_nochk(MT6323_DEW_CIPHER_MODE, 0x1);
#endif
	/* wait for cipher mode idle */
	return_value =
	    wait_for_state_ready_init(wait_for_idle_and_sync, TIMEOUT_WAIT_IDLE,
				      PMIC_WRAP_WACS2_RDATA, 0);
	if (return_value != 0) {
		PWRAPERR("wait for cipher mode idle fail,return_value=%x\n", return_value);
		return return_value;
	}
	WRAP_WR32(PMIC_WRAP_CIPHER_MODE, 1);

	/* Read Test */
#ifdef SLV_6325
	pwrap_read_nochk(MT6325_DEW_READ_TEST, &rdata);
	if (rdata != MT6325_DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("_pwrap_init_cipher,read test error,error code=%x, rdata=%x\n", 1, rdata);
		return E_PWR_READ_TEST_FAIL;
	}
#endif
#ifdef SLV_6332
	pwrap_read_nochk(MT6332_DEW_READ_TEST, &rdata);
	if (rdata != MT6332_DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("_pwrap_init_cipher,read test error,error code=%x, rdata=%x\n", 1, rdata);
		return E_PWR_READ_TEST_FAIL;
	}
#endif
#ifdef SLV_6323
	pwrap_read_nochk(MT6323_DEW_READ_TEST, &rdata);
	if (rdata != MT6323_DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("_pwrap_init_cipher,read test error,error code=%x, rdata=%x\n", 1, rdata);
		return E_PWR_READ_TEST_FAIL;
	}
#endif
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);
	return 0;
}

/* -------------------------------------------------------- */
/* Function : _pwrap_init_sistrobe() */
/* Description : Initialize SI_CK_CON and SIDLY */
/* Parameter : */
/* Return : */
/* -------------------------------------------------------- */
static s32 _pwrap_init_sistrobe(void)
{
	u32 arb_en_backup = 0;
	u32 rdata = 0;
	u32 i = 0;
	s32 ind = 0;
	u32 tmp1 = 0;
	u32 tmp2 = 0;
	u32 result_faulty = 0;
	u32 result[2] = { 0, 0 };
	s32 leading_one[2] = { -1, -1 };
	s32 tailing_one[2] = { -1, -1 };

	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2);	/* only WACS2 */

	/* --------------------------------------------------------------------- */
	/* Scan all possible input strobe by READ_TEST */
	/* --------------------------------------------------------------------- */
	/* 24 sampling clock edge */
	for (ind = 0; ind < 24; ind++) {
		WRAP_WR32(PMIC_WRAP_SI_CK_CON, (ind >> 2) & 0x7);
		WRAP_WR32(PMIC_WRAP_SIDLY, 0x3 - (ind & 0x3));
#ifdef SLV_6325
		_pwrap_wacs2_nochk(0, MT6325_DEW_READ_TEST, 0, &rdata);
		if (rdata == MT6325_DEFAULT_VALUE_READ_TEST) {
			PWRAPLOG
			    ("_pwrap_init_sistrobe [Read Test of MT6325] pass,index=%d rdata=%x\n",
			     ind, rdata);
			result[0] |= (0x1 << ind);
		} else {
			PWRAPLOG
			    ("_pwrap_init_sistrobe [Read Test of MT6325] tuning,index=%d rdata=%x\n",
			     ind, rdata);
		}
#endif
#ifdef SLV_6332
		_pwrap_wacs2_nochk(0, MT6332_DEW_READ_TEST, 0, &rdata);
		if (rdata == MT6332_DEFAULT_VALUE_READ_TEST) {
			PWRAPLOG
			    ("_pwrap_init_sistrobe [Read Test of MT6332] pass,index=%d rdata=%x\n",
			     ind, rdata);
			result[1] |= (0x1 << ind);
		} else {
			PWRAPLOG
			    ("_pwrap_init_sistrobe [Read Test of MT6332] tuning,index=%d rdata=%x\n",
			     ind, rdata);
		}
#endif
#ifdef SLV_6323
		_pwrap_wacs2_nochk(0, MT6323_DEW_READ_TEST, 0, &rdata);
		if (rdata == MT6323_DEFAULT_VALUE_READ_TEST) {
			PWRAPLOG
			    ("_pwrap_init_sistrobe [Read Test of MT6323] pass,index=%d rdata=%x\n",
			     ind, rdata);
			result[0] |= (0x1 << ind);
		} else {
			PWRAPLOG
			    ("_pwrap_init_sistrobe [Read Test of MT6323] tuning,index=%d rdata=%x\n",
			     ind, rdata);
		}
#endif
	}
#ifdef SLV_6323
	result[1] = result[0];
#else
#ifndef SLV_6325
	result[0] = result[1];
#endif
#ifndef SLV_6332
	result[1] = result[0];
#endif
#endif
	/* --------------------------------------------------------------------- */
	/* Locate the leading one and trailing one of PMIC 1/2 */
	/* --------------------------------------------------------------------- */
	for (ind = 23; ind >= 0; ind--) {
		if ((result[0] & (0x1 << ind)) && leading_one[0] == -1)
			leading_one[0] = ind;
		if (leading_one[0] > 0)
			break;
	}
	for (ind = 23; ind >= 0; ind--) {
		if ((result[1] & (0x1 << ind)) && leading_one[1] == -1)
			leading_one[1] = ind;
		if (leading_one[1] > 0)
			break;
	}

	for (ind = 0; ind < 24; ind++) {
		if ((result[0] & (0x1 << ind)) && tailing_one[0] == -1)
			tailing_one[0] = ind;
		if (tailing_one[0] > 0)
			break;
	}
	for (ind = 0; ind < 24; ind++) {
		if ((result[1] & (0x1 << ind)) && tailing_one[1] == -1)
			tailing_one[1] = ind;
		if (tailing_one[1] > 0)
			break;
	}

	/* --------------------------------------------------------------------- */
	/* Check the continuity of pass range */
	/* --------------------------------------------------------------------- */
	for (i = 0; i < 2; i++) {
		tmp1 = (0x1 << (leading_one[i] + 1)) - 1;
		tmp2 = (0x1 << tailing_one[i]) - 1;
		if ((tmp1 - tmp2) != result[i]) {
			/*TERR = "[DrvPWRAP_InitSiStrobe] Fail at PMIC %d, result = %x, leading_one:%d, tailing_one:%d"
			   , i+1, result[i], leading_one[i], tailing_one[i] */
			PWRAPERR
			    ("_pwrap_init_sistrobe Fail at PMIC %d, result = %x, leading_one:%d, tailing_one:%d\n",
			     i + 1, result[i], leading_one[i], tailing_one[i]);
			result_faulty = 0x1;
		}
	}

	/* --------------------------------------------------------------------- */
	/* Config SICK and SIDLY to the middle point of pass range */
	/* --------------------------------------------------------------------- */
	if (result_faulty == 0) {
		/* choose the best point in the interaction of PMIC1's pass range and PMIC2's pass range */
		ind =
		    ((leading_one[0] + tailing_one[0]) / 2 +
		     (leading_one[1] + tailing_one[1]) / 2) / 2;
		/*TINFO = "The best point in the interaction area is %d, ind" */
		WRAP_WR32(PMIC_WRAP_SI_CK_CON, (ind >> 2) & 0x7);
		WRAP_WR32(PMIC_WRAP_SIDLY, 0x3 - (ind & 0x3));
		/* --------------------------------------------------------------------- */
		/* Restore */
		/* --------------------------------------------------------------------- */
		WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);
	} else {
		PWRAPERR("_pwrap_init_sistrobe Fail,result_faulty=%x\n", result_faulty);
	}
	return result_faulty;
}

/* -------------------------------------------------------- */
/* Function : _pwrap_reset_spislv() */
/* Description : */
/* Parameter : */
/* Return : */
/* -------------------------------------------------------- */

s32 _pwrap_reset_spislv(void)
{
	u32 ret = 0;
	u32 return_value = 0;
	/* PWRAPFUC(); */
	/* This driver does not using _pwrap_switch_mux */
	/* because the remaining requests are expected to fail anyway */

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, DISABLE_ALL);
	WRAP_WR32(PMIC_WRAP_WRAP_EN, DISABLE);
	WRAP_WR32(PMIC_WRAP_MUX_SEL, MANUAL_MODE);
	WRAP_WR32(PMIC_WRAP_MAN_EN, ENABLE);
	WRAP_WR32(PMIC_WRAP_DIO_EN, DISABLE);

	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_CSL << 8));	/* 0x2100 */
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));	/* 0x2800//to reset counter */
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_CSH << 8));	/* 0x2000 */
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));

	return_value =
	    wait_for_state_ready_init(wait_for_sync, TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS2_RDATA, 0);
	if (return_value != 0) {
		PWRAPERR("_pwrap_reset_spislv fail,return_value=%x\n", return_value);
		ret = E_PWR_TIMEOUT;
		goto timeout;
	}

	WRAP_WR32(PMIC_WRAP_MAN_EN, DISABLE);
	WRAP_WR32(PMIC_WRAP_MUX_SEL, WRAPPER_MODE);

timeout:
	WRAP_WR32(PMIC_WRAP_MAN_EN, DISABLE);
	WRAP_WR32(PMIC_WRAP_MUX_SEL, WRAPPER_MODE);
	return ret;
}

static s32 _pwrap_init_reg_clock(u32 regck_sel)
{
	u32 wdata = 0;
	u32 rdata = 0;

	PWRAPFUC();

	/* Set Dummy cycle 6325 (assume 18MHz) */
#ifdef SLV_6325
	pwrap_write_nochk(MT6325_DEW_RDDMY_NO, 0x8);
#endif
#ifdef SLV_6332
	pwrap_write_nochk(MT6332_DEW_RDDMY_NO, 0x8);
#endif
#ifdef SLV_6323
	wdata = 0x3;
	pwrap_write_nochk(MT6323_TOP_CKCON1_CLR, wdata);
	pwrap_read_nochk(MT6323_TOP_CKCON1, &rdata);

	if ((rdata & 0x3) != 0) {
		PWRAPERR("_pwrap_init_reg_clock,PMIC_TOP_CKCON2 Write [15]=1 Fail, rdata=%x\n",
			 rdata);
		return E_PWR_WRITE_TEST_FAIL;
	}
	pwrap_write_nochk(MT6323_DEW_RDDMY_NO, 0x8);
#endif
	WRAP_WR32(PMIC_WRAP_RDDMY, 0x88);

	/* Config SPI Waveform according to reg clk */
	if (regck_sel == 1) {	/* 16.2MHz in 6325, 33MHz in BBChip */
#ifdef SLV_6323
		/* 12MHz in 6323 */
		WRAP_WR32(PMIC_WRAP_CSHEXT_WRITE, 0x5);
		WRAP_WR32(PMIC_WRAP_CSHEXT_READ, 0x0);
		WRAP_WR32(PMIC_WRAP_CSLEXT_START, 0x2);
		WRAP_WR32(PMIC_WRAP_CSLEXT_END, 0x2);
#else
		WRAP_WR32(PMIC_WRAP_CSHEXT_WRITE, 0x0);
		WRAP_WR32(PMIC_WRAP_CSHEXT_READ, 0x6);
		WRAP_WR32(PMIC_WRAP_CSLEXT_START, 0x0);
		WRAP_WR32(PMIC_WRAP_CSLEXT_END, 0x0);
#endif
	} else {		/* Safe mode */
		WRAP_WR32(PMIC_WRAP_CSHEXT_WRITE, 0xf);
		WRAP_WR32(PMIC_WRAP_CSHEXT_READ, 0xf);
		WRAP_WR32(PMIC_WRAP_CSLEXT_START, 0xf);
		WRAP_WR32(PMIC_WRAP_CSLEXT_END, 0xf);
	}

	return 0;
}

static s32 _pwrap_init_signature(u8 path)
{
	int ret = 0;
#ifdef SLV_6325
	u32 rdata = 0x0;
#endif
	PWRAPFUC();

	if (path == 1) {
		/* ############################### */
		/* Signature Checking - Using Write Test Register */
		/* should be the last to modify WRITE_TEST */
		/* ############################### */
#ifdef SLV_6325
		_pwrap_wacs2_nochk(1, MT6325_DEW_WRITE_TEST, 0x5678, &rdata);
		WRAP_WR32(PMIC_WRAP_SIG_ADR, MT6325_DEW_WRITE_TEST);
#endif
		WRAP_WR32(PMIC_WRAP_SIG_VALUE, 0x5678);
		WRAP_WR32(PMIC_WRAP_SIG_MODE, 0x1);
	} else {
		/* ############################### */
		/* Signature Checking using CRC and EINT update */
		/* should be the last to modify WRITE_TEST */
		/* ############################### */
		WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, 0);
#ifdef SLV_6325
		WRAP_WR32(PMIC_WRAP_SIG_ADR, WRAP_RD32(PMIC_WRAP_SIG_ADR) | MT6325_DEW_CRC_VAL);
		WRAP_WR32(PMIC_WRAP_EINT_STA0_ADR, MT6325_INT_STA);
		WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, WRAP_RD32(PMIC_WRAP_STAUPD_GRPEN) | 0x5);
#endif
#ifdef SLV_6332
		ret = pwrap_write_nochk(MT6332_DEW_CRC_EN, ENABLE);
		if (ret != 0) {
			PWRAPERR("MT6332 enable CRC fail,ret=%x\n", ret);
			return E_PWR_INIT_ENABLE_CRC;
		}
		WRAP_WR32(PMIC_WRAP_SIG_ADR,
			  WRAP_RD32(PMIC_WRAP_SIG_ADR) | (MT6332_DEW_CRC_VAL << 16));
		WRAP_WR32(PMIC_WRAP_EINT_STA1_ADR, MT6332_INT_STA);
		WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, WRAP_RD32(PMIC_WRAP_STAUPD_GRPEN) | 0xa);
#endif
#ifdef SLV_6323
		ret = pwrap_write_nochk(MT6323_DEW_CRC_EN, ENABLE);
		if (ret != 0) {
			PWRAPERR("enable CRC fail,ret=%x\n", ret);
			return E_PWR_INIT_ENABLE_CRC;
		}
		WRAP_WR32(PMIC_WRAP_SIG_ADR, WRAP_RD32(PMIC_WRAP_SIG_ADR) | MT6323_DEW_CRC_VAL);
		/* WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN,WRAP_RD32(PMIC_WRAP_STAUPD_GRPEN)|0x5); */
#endif
	}
#ifdef SLV_6325			/* GPSINF */
	WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, WRAP_RD32(PMIC_WRAP_STAUPD_GRPEN) | 0x70);
#endif
	/* Disable CRC check */
	/* WRAP_WR32(PMIC_WRAP_CRC_EN, ENABLE); */
	return ret;
}

/*
 * pmic_wrap init,init wrap interface
 */
s32 pwrap_init(void)
{
	s32 sub_return = 0;
	s32 sub_return1 = 0;
	/* s32 ret=0; */
	u32 rdata = 0x0;
	/* u32 timeout=0; */
	u32 cg_mask = 0;
	u32 backup = 0;

	PWRAPFUC();
#ifdef CONFIG_OF
	sub_return = pwrap_of_iomap();
	if (sub_return)
		return sub_return;
#endif
	/* ############################### */
	/* toggle PMIC_WRAP and pwrap_spictl reset */
	/* ############################### */
	/* WRAP_SET_BIT(0x80,INFRA_GLOBALCON_RST0); */
	/* WRAP_CLR_BIT(0x80,INFRA_GLOBALCON_RST0); */
	/* __pwrap_soft_reset(); */

	/* Turn off module clock */
	cg_mask = PMIC_CG_TMR | PMIC_CG_AP | PMIC_CG_MD | PMIC_CG_CONN;
	backup = (~WRAP_RD32(MODULE_SW_CG_0_STA)) & cg_mask;
	WRAP_WR32(MODULE_SW_CG_0_SET, cg_mask);
	/* dummy read to add latency (to wait clock turning off) */
	rdata = WRAP_RD32(PMIC_WRAP_SWRST);

	/* Toggle module reset */
	WRAP_WR32(PMIC_WRAP_SWRST, ENABLE);

	WRAP_WR32(PMIC_WRAP_SWRST, DISABLE);

	/* Turn on module clock */
	WRAP_WR32(MODULE_SW_CG_0_CLR, backup | PMIC_CG_AP);

	/* Turn on module clock dcm (in global_con) */
	rdata = WRAP_RD32(PERI_BUS_DCM_CTRL);
	WRAP_WR32(PERI_BUS_DCM_CTRL, rdata | PMIC_DCM_EN);

	/* ############################### */
	/* Set SPI_CK_freq = 26MHz */
	/* ############################### */
#ifndef CONFIG_MTK_FPGA
	WRAP_WR32(CLK_CFG_5_CLR, CLK_SPI_CK_26M);
#endif

	/* ############################### */
	/* toggle PERI_PWRAP_BRIDGE reset */
	/* ############################### */
	/* WRAP_SET_BIT(0x04,PERI_GLOBALCON_RST1); */
	/* WRAP_CLR_BIT(0x04,PERI_GLOBALCON_RST1); */

	/* ############################### */
	/* Enable DCM */
	/* ############################### */
	WRAP_WR32(PMIC_WRAP_DCM_EN, 3);	/* enable CRC DCM and PMIC_WRAP DCM */
	WRAP_WR32(PMIC_WRAP_DCM_DBC_PRD, DISABLE);	/* no debounce */
#ifdef SLV_6323
	WRAP_WR32(PMIC_WRAP_OP_TYPE, 0);
	WRAP_WR32(PMIC_WRAP_MSB_FIRST, 1);
#endif
	/* ############################### */
	/* Reset SPISLV */
	/* ############################### */
	sub_return = _pwrap_reset_spislv();
	if (sub_return != 0) {
		PWRAPERR("error,_pwrap_reset_spislv fail,sub_return=%x\n", sub_return);
		return E_PWR_INIT_RESET_SPI;
	}
	/* ############################### */
	/* Enable WACS2 */
	/* ############################### */
	WRAP_WR32(PMIC_WRAP_WRAP_EN, ENABLE);	/* enable wrap */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2);	/* Only WACS2 */
	WRAP_WR32(PMIC_WRAP_WACS2_EN, ENABLE);

#ifdef SLV_6323
	WRAP_WR32(PMIC_WRAP_RDDMY, 0xF);
#endif
	/* ############################### */
	/* Input data calibration flow; */
	/* ############################### */
	sub_return = _pwrap_init_sistrobe();
	if (sub_return != 0) {
		PWRAPERR("error,DrvPWRAP_InitSiStrobe fail,sub_return=%x\n", sub_return);
		return E_PWR_INIT_SIDLY;
	}
	/* ############################### */
	/* SPI Waveform Configuration */
	/* ############################### */
	/* 0:safe mode, 1:18MHz */
	sub_return = _pwrap_init_reg_clock(1);
	if (sub_return != 0) {
		PWRAPERR("error,_pwrap_init_reg_clock fail,sub_return=%x\n", sub_return);
		return E_PWR_INIT_REG_CLOCK;
	}
	/* ############################### */
	/* Enable DIO mode */
	/* ############################### */
	/* PMIC2 dual io not ready */
#ifdef SLV_6323
	sub_return = _pwrap_init_dio(1);
#else
	sub_return = _pwrap_init_dio(3);
#endif
	if (sub_return != 0) {
		PWRAPERR("_pwrap_init_dio test error,error code=%x, sub_return=%x\n", 0x11,
			 sub_return);
		return E_PWR_INIT_DIO;
	}
	/* ############################### */
	/* Enable Encryption */
	/* ############################### */

	sub_return = _pwrap_init_cipher();
	if (sub_return != 0) {
		PWRAPERR("Enable Encryption fail, return=%x\n", sub_return);
		return E_PWR_INIT_CIPHER;
	}
	/* ############################### */
	/* Write test using WACS2 */
	/* ############################### */
	/* check Wtiet test default value */

#ifdef SLV_6325
	sub_return = pwrap_write_nochk(MT6325_DEW_WRITE_TEST, MT6325_WRITE_TEST_VALUE);
	sub_return1 = pwrap_read_nochk(MT6325_DEW_WRITE_TEST, &rdata);
	if (rdata != MT6325_WRITE_TEST_VALUE) {
		PWRAPERR
		    ("write test error,rdata=0x%x,exp=0xa55a,sub_return=0x%x,sub_return1=0x%x\n",
		     rdata, sub_return, sub_return1);
		return E_PWR_INIT_WRITE_TEST;
	}
#endif
#ifdef SLV_6332
	sub_return = pwrap_write_nochk(MT6332_DEW_WRITE_TEST, MT6332_WRITE_TEST_VALUE);
	sub_return1 = pwrap_read_nochk(MT6332_DEW_WRITE_TEST, &rdata);
	if (rdata != MT6332_WRITE_TEST_VALUE) {
		PWRAPERR
		    ("write test error,rdata=0x%x,exp=0xa55a,sub_return=0x%x,sub_return1=0x%x\n",
		     rdata, sub_return, sub_return1);
		return E_PWR_INIT_WRITE_TEST;
	}
#endif
#ifdef SLV_6323
	sub_return1 = pwrap_read_nochk(MT6323_DEW_WRITE_TEST, &rdata);
	if (rdata != 0) {
		PWRAPERR("Default should be 0,rdata=0x%x,sub_return=0x%x\n", rdata, sub_return1);
		return E_PWR_INIT_WRITE_TEST;
	}
	sub_return = pwrap_write_nochk(MT6323_DEW_WRITE_TEST, MT6323_WRITE_TEST_VALUE);
	sub_return1 = pwrap_read_nochk(MT6323_DEW_WRITE_TEST, &rdata);
	if (rdata != MT6323_WRITE_TEST_VALUE) {
		PWRAPERR
		    ("write test error,rdata=0x%x,exp=0xa55a,sub_return=0x%x,sub_return1=0x%x\n",
		     rdata, sub_return, sub_return1);
		return E_PWR_INIT_WRITE_TEST;
	}
#endif
	/* ############################### */
	/* Signature Checking - Using CRC */
	/* should be the last to modify WRITE_TEST */
	/* ############################### */
	sub_return = _pwrap_init_signature(0);
	if (sub_return != 0) {
		PWRAPERR("Enable CRC fail, return=%x\n", sub_return);
		return E_PWR_INIT_ENABLE_CRC;
	}
	/* ############################### */
	/* PMIC_WRAP enables */
	/* ############################### */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0xff);
	WRAP_WR32(PMIC_WRAP_WACS0_EN, ENABLE);
	WRAP_WR32(PMIC_WRAP_WACS1_EN, ENABLE);
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x5);	/* 0x1:20us,for concurrence test,MP:0x5;  //100us */
	WRAP_WR32(PMIC_WRAP_WDT_UNIT, 0xf);
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0xffffffff);
	WRAP_WR32(PMIC_WRAP_TIMER_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_INT_EN, 0x7ffffff9);

	/* Priority adjustment for K2 */
	WRAP_WR32(PMIC_WRAP_HARB_HPRIO, WACS0 | DVFSINF | WACS2 | WACS_P2P);

	/* ############################### */
	/* GPS Initialization */
	/* set address of ready bit register and thermal data register */
	/* ############################### */
#ifdef SLV_6323
	WRAP_WR32(PMIC_WRAP_ADC_CMD_ADDR, MT6323_AUXADC_CON21);
	WRAP_WR32(PMIC_WRAP_PWRAP_ADC_CMD, 0x8000);
	WRAP_WR32(PMIC_WRAP_ADC_RDY_ADDR, MT6323_AUXADC_ADC12);
	WRAP_WR32(PMIC_WRAP_ADC_RDATA_ADDR1, MT6323_AUXADC_ADC13);
	WRAP_WR32(PMIC_WRAP_ADC_RDATA_ADDR2, MT6323_AUXADC_ADC14);
#else
	WRAP_WR32(PMIC_WRAP_ADC_CMD_ADDR, MT6325_AUXADC_CON21);
	WRAP_WR32(PMIC_WRAP_PWRAP_ADC_CMD, 0x8000);
	WRAP_WR32(PMIC_WRAP_ADC_RDY_ADDR, MT6325_AUXADC_ADC12);
	WRAP_WR32(PMIC_WRAP_ADC_RDATA_ADDR1, MT6325_AUXADC_ADC13);
	WRAP_WR32(PMIC_WRAP_ADC_RDATA_ADDR2, MT6325_AUXADC_ADC14);
#endif
	/* ############################### */
	/* Initialization Done */
	/* ############################### */
	WRAP_WR32(PMIC_WRAP_INIT_DONE2, ENABLE);

	/* ############################### */
	/* P2P Initialization Done */
	/* ############################### */
#ifndef CONFIG_MTK_FPGA
	WRAP_WR32(SCP_CG_CLK_CTRL, WRAP_RD32(SCP_CG_CLK_CTRL) | AP2P_MCLK_CG);
	WRAP_WR32(PMIC_WRAP_P2P_INIT_DONE_P2P, ENABLE);
	WRAP_WR32(PMIC_WRAP_P2P_WACS_P2P_EN, ENABLE);
#endif
	/* ############################### */
	/* TBD: Should be configured by MD MCU */
	/* ############################### */
	WRAP_WR32(PMIC_WRAP_INIT_DONE0, ENABLE);
	WRAP_WR32(PMIC_WRAP_INIT_DONE1, ENABLE);

#ifdef CONFIG_OF
	pwrap_of_iounmap();
#endif

	return 0;
}

/* EXPORT_SYMBOL(pwrap_init); */
/*Interrupt handler function*/
static irqreturn_t mt_pmic_wrap_irq(int irqno, void *dev_id)
{
	unsigned long flags = 0;

	PWRAPFUC();
	PWRAPREG("dump pwrap register\n");
	spin_lock_irqsave(&wrp_lock_isr, flags);
	/* *----------------------------------------------------------------------- */
	pwrap_dump_all_register();
	/* raise the priority of WACS2 for AP */
	WRAP_WR32(PMIC_WRAP_HARB_HPRIO, 1 << 3);

	/* *----------------------------------------------------------------------- */
	/* clear interrupt flag */
	WRAP_WR32(PMIC_WRAP_INT_CLR, 0xffffffff);
	PWRAPREG("INT flag 0x%x\n", WRAP_RD32(PMIC_WRAP_INT_EN));
	BUG_ON(1);
	spin_unlock_irqrestore(&wrp_lock_isr, flags);
	return IRQ_HANDLED;
}

static u32 pwrap_read_test(void)
{
	u32 rdata = 0;
	u32 return_value = 0;
	/* Read Test */
#ifdef SLV_6325
	return_value = pwrap_read(MT6325_DEW_READ_TEST, &rdata);
	if (rdata != MT6325_DEFAULT_VALUE_READ_TEST) {
		PWRAPREG("Read Test fail,rdata=0x%x, exp=0x5aa5,return_value=0x%x\n", rdata,
			 return_value);
		return_value = E_PWR_READ_TEST_FAIL;
	} else {
		PWRAPREG("Read Test pass,return_value=%d\n", return_value);
		return_value = 0;
	}
#endif
#ifdef SLV_6332
	return_value = pwrap_read(MT6332_DEW_READ_TEST, &rdata);
	if (rdata != MT6332_DEFAULT_VALUE_READ_TEST) {
		PWRAPREG("Read Test fail,rdata=0x%x, exp=0x5aa5,return_value=0x%x\n", rdata,
			 return_value);
		return_value = E_PWR_READ_TEST_FAIL;
	} else {
		PWRAPREG("Read Test pass,return_value=%d\n", return_value);
		return_value = 0;
	}
#endif
#ifdef SLV_6323
	return_value = pwrap_read(MT6323_DEW_READ_TEST, &rdata);
	if (rdata != MT6323_DEFAULT_VALUE_READ_TEST) {
		PWRAPREG("Read Test fail,rdata=0x%x, exp=0x5aa5,return_value=0x%x\n", rdata,
			 return_value);
		return_value = E_PWR_READ_TEST_FAIL;
	} else {
		PWRAPREG("Read Test pass,return_value=%d\n", return_value);
		return_value = 0;
	}
#endif
	return return_value;
}

static u32 pwrap_write_test(void)
{
	u32 rdata = 0;
	u32 sub_return = 0;
	u32 sub_return1 = 0;
	/* ############################### */
	/* Write test using WACS2 */
	/* ############################### */
#ifdef SLV_6325
	sub_return = pwrap_write(MT6325_DEW_WRITE_TEST, MT6325_WRITE_TEST_VALUE);
	PWRAPREG("after MT6325 pwrap_write\n");
	sub_return1 = pwrap_read(MT6325_DEW_WRITE_TEST, &rdata);
	if ((rdata != MT6325_WRITE_TEST_VALUE) || (sub_return != 0) || (sub_return1 != 0)) {
		PWRAPREG
		    ("write test error,rdata=0x%x,exp=0xa55a,sub_return=0x%x,sub_return1=0x%x\n",
		     rdata, sub_return, sub_return1);
		sub_return = E_PWR_INIT_WRITE_TEST;
	} else {
		PWRAPREG("write MT6325 Test pass\n");
		sub_return = 0;
	}
#endif
#ifdef SLV_6332
	sub_return = pwrap_write(MT6332_DEW_WRITE_TEST, MT6332_WRITE_TEST_VALUE);
	PWRAPREG("after MT6332 pwrap_write\n");
	sub_return1 = pwrap_read(MT6332_DEW_WRITE_TEST, &rdata);
	if ((rdata != MT6332_WRITE_TEST_VALUE) || (sub_return != 0) || (sub_return1 != 0)) {
		PWRAPREG
		    ("write test error,rdata=0x%x,exp=0xa55a,sub_return=0x%x,sub_return1=0x%x\n",
		     rdata, sub_return, sub_return1);
		sub_return = E_PWR_INIT_WRITE_TEST;
	} else {
		PWRAPREG("write MT6332 Test pass\n");
		sub_return = 0;
	}
#endif
#ifdef SLV_6323
	sub_return = pwrap_write(MT6323_DEW_WRITE_TEST, MT6323_WRITE_TEST_VALUE);
	PWRAPREG("after MT6323 pwrap_write\n");
	sub_return1 = pwrap_read(MT6323_DEW_WRITE_TEST, &rdata);
	if ((rdata != MT6323_WRITE_TEST_VALUE) || (sub_return != 0) || (sub_return1 != 0)) {
		PWRAPREG
		    ("write test error,rdata=0x%x,exp=0xa55a,sub_return=0x%x,sub_return1=0x%x\n",
		     rdata, sub_return, sub_return1);
		sub_return = E_PWR_INIT_WRITE_TEST;
	} else {
		PWRAPREG("write MT6323 Test pass\n");
		sub_return = 0;
	}
#endif
	return sub_return;
}

static void pwrap_int_test(void)
{
	u32 rdata1 = 0;
	u32 rdata2 = 0;

	while (1) {
#ifdef SLV_6325
		rdata1 = WRAP_RD32(PMIC_WRAP_EINT_STA);
		pwrap_read(MT6325_INT_STA, &rdata2);
		PWRAPREG
		    ("Pwrap INT status check,PMIC_WRAP_EINT_STA=0x%x,MT6325_INT_STA[0x01B4]=0x%x\n",
		     rdata1, rdata2);
#endif
#ifdef SLV_6332
		rdata1 = WRAP_RD32(PMIC_WRAP_EINT_STA);
		pwrap_read(MT6332_INT_STA, &rdata2);
		PWRAPREG
		    ("Pwrap INT status check,PMIC_WRAP_EINT_STA=0x%x,MT6332_INT_STA[0x8112]=0x%x\n",
		     rdata1, rdata2);
#endif
#ifdef SLV_6323
		pwrap_read(MT6323_INT_STA, &rdata2);
		PWRAPREG
		    ("Pwrap INT status check,PMIC_WRAP_EINT_STA=0x%x,MT6332_INT_STA[0x8112]=0x%x\n",
		     rdata1, rdata2);
#endif
		msleep(500);
	}
}

#ifndef USER_BUILD_KERNEL
static void pwrap_read_reg_on_pmic(u32 reg_addr)
{
	u32 reg_value = 0;
	u32 return_value = 0;
	/* PWRAPFUC(); */
	return_value = pwrap_read(reg_addr, &reg_value);
	PWRAPREG("0x%x=0x%x,return_value=%x\n", reg_addr, reg_value, return_value);
}

static void pwrap_write_reg_on_pmic(u32 reg_addr, u32 reg_value)
{
	u32 return_value = 0;

	PWRAPREG("write 0x%x to register 0x%x\n", reg_value, reg_addr);
	return_value = pwrap_write(reg_addr, reg_value);
	return_value = pwrap_read(reg_addr, &reg_value);
	/* PWRAPFUC(); */
	PWRAPREG("the result:0x%x=0x%x,return_value=%x\n", reg_addr, reg_value, return_value);
}
#endif

static void pwrap_ut(u32 ut_test)
{
	switch (ut_test) {
	case 1:
		/* pwrap_wacs2_para_test(); */
		pwrap_write_test();
		break;
	case 2:
		/* pwrap_wacs2_para_test(); */
		pwrap_read_test();
		break;

	default:
		PWRAPREG("default test.\n");
		break;
	}
}

/*---------------------------------------------------------------------------*/
static s32 mt_pwrap_show_hal(char *buf)
{
	PWRAPFUC();
	return snprintf(buf, PAGE_SIZE, "%s\n", "no implement");
}

/*---------------------------------------------------------------------------*/
static s32 mt_pwrap_store_hal(const char *buf, size_t count)
{
#ifndef USER_BUILD_KERNEL
	u32 reg_value = 0;
	u32 reg_addr = 0;
#endif
	u32 return_value = 0;
	u32 ut_test = 0;

	if (!strncmp(buf, "-h", 2)) {
		PWRAPREG
		    ("PWRAP debug: [-dump_reg][-trace_wacs2][-init][-rdpmic][-wrpmic][-readtest][-writetest]\n");
		PWRAPREG("PWRAP UT: [1][2]\n");
	} else if (!strncmp(buf, "-dump_reg", 9)) {
		pwrap_dump_all_register();
	} else if (!strncmp(buf, "-trace_wacs2", 12)) {
		/* pwrap_trace_wacs2(); */
	} else if (!strncmp(buf, "-init", 5)) {
		return_value = pwrap_init();
		if (return_value == 0)
			PWRAPREG("pwrap_init pass,return_value=%d\n", return_value);
		else
			PWRAPREG("pwrap_init fail,return_value=%d\n", return_value);
	}
#ifndef USER_BUILD_KERNEL
	else if (!strncmp(buf, "-rdpmic", 7) && (1 == sscanf(buf + 7, "%x", &reg_addr))) {
		pwrap_read_reg_on_pmic(reg_addr);
	} else if (!strncmp(buf, "-wrpmic", 7)
		   && (2 == sscanf(buf + 7, "%x %x", &reg_addr, &reg_value))) {
		pwrap_write_reg_on_pmic(reg_addr, reg_value);
	}
#endif
	else if (!strncmp(buf, "-readtest", 9)) {
		pwrap_read_test();
	} else if (!strncmp(buf, "-writetest", 10)) {
		pwrap_write_test();
	} else if (!strncmp(buf, "-int", 4)) {
		pwrap_int_test();
	} else if (!strncmp(buf, "-ut", 3) && (1 == sscanf(buf + 3, "%d", &ut_test))) {
		pwrap_ut(ut_test);
	} else {
		PWRAPREG("wrong parameter\n");
	}
	return count;
}

#ifdef CONFIG_OF
static int pwrap_of_iomap(void)
{
	/*
	 * Map the address of the following register base:
	 * INFRACFG_AO, TOPCKGEN, SCP_CLK_CTRL, SCP_PMICWP2P
	 */

	struct device_node *infracfg_ao_node;
	struct device_node *topckgen_node;
	struct device_node *scp_clk_ctrl_node;
	struct device_node *pwrap_p2p_node;

	infracfg_ao_node = of_find_compatible_node(NULL, NULL, "mediatek,INFRACFG_AO");
	if (!infracfg_ao_node) {
		pr_info("get INFRACFG_AO failed\n");
		return -ENODEV;
	}

	infracfg_ao_base = of_iomap(infracfg_ao_node, 0);
	if (!infracfg_ao_base) {
		pr_info("INFRACFG_AO iomap failed\n");
		return -ENOMEM;
	}

	topckgen_node = of_find_compatible_node(NULL, NULL, "mediatek,TOPCKGEN");
	if (!topckgen_node) {
		pr_info("get TOPCKGEN failed\n");
		return -ENODEV;
	}

	topckgen_base = of_iomap(topckgen_node, 0);
	if (!topckgen_base) {
		pr_info("TOPCKGEN iomap failed\n");
		return -ENOMEM;
	}

	scp_clk_ctrl_node = of_find_compatible_node(NULL, NULL, "mediatek,SCP_CLK_CTRL");
	if (!scp_clk_ctrl_node) {
		pr_info("get SCP_CLK_CTRL failed\n");
		return -ENODEV;
	}

	scp_clk_ctrl_base = of_iomap(scp_clk_ctrl_node, 0);
	if (!scp_clk_ctrl_base) {
		pr_info("SCP_CLK_CTRL iomap failed\n");
		return -ENOMEM;
	}

	pwrap_p2p_node = of_find_compatible_node(NULL, NULL, "mediatek,SCP_PMICWP2P");
	if (!pwrap_p2p_node) {
		pr_info("get PWRAP_P2P failed\n");
		return -ENODEV;
	}

	pwrap_p2p_base = of_iomap(pwrap_p2p_node, 0);
	if (!pwrap_p2p_base) {
		pr_info("PWRAP_P2P iomap failed\n");
		return -ENOMEM;
	}

	pr_info("INFRACFG_AO reg: 0x%p\n", infracfg_ao_base);
	pr_info("TOPCKGEN reg: 0x%p\n", topckgen_base);
	pr_info("SCP_CLK_CTRL reg: 0x%p\n", scp_clk_ctrl_base);
	pr_info("PWRAP_P2P reg: 0x%p\n", pwrap_p2p_base);

	return 0;
}

static void pwrap_of_iounmap(void)
{
	iounmap(infracfg_ao_base);
	iounmap(topckgen_base);
	iounmap(scp_clk_ctrl_base);
	iounmap(pwrap_p2p_base);
}
#endif

static int is_pwrap_init_done(void)
{
	int ret = 0;

	ret = WRAP_RD32(PMIC_WRAP_INIT_DONE2);
	if (ret != 0)
		return 0;

	ret = pwrap_init();
	if (ret != 0) {
		PWRAPERR("init error (%d)\n", ret);
		pwrap_dump_all_register();
		return ret;
	}
	PWRAPLOG("init successfully done (%d)\n\n", ret);
	return ret;
}

static int pwrap_reg_read(void *context, unsigned int reg, unsigned int *val)
{
	return pwrap_read(reg, val);
}

static int pwrap_reg_write(void *context, unsigned int reg, unsigned int val)
{
	return pwrap_write(reg, val);
}

const struct regmap_config pwrap_regmap_config = {
	.reg_bits = 16,
	.val_bits = 16,
	.reg_read = pwrap_reg_read,
	.reg_write = pwrap_reg_write
};

static struct of_device_id of_pwrap_match_tbl[] = {
	{.compatible = "mediatek,mt8163-pwrap"},
	{}
};

MODULE_DEVICE_TABLE(of, of_pwrap_match_tbl);

static int pwrap_probe(struct platform_device *pdev)
{
	s32 ret = 0;
#ifdef CONFIG_OF
	struct pmic_wrapper *wrp;
	struct device_node *pwrap_node = pdev->dev.of_node;
	const struct of_device_id *of_id = of_match_device(of_pwrap_match_tbl, &pdev->dev);
	struct resource *res;

	if (!of_id) {
		/* We do not expect this to happen */
		dev_err(&pdev->dev, "%s: Unable to match device\n", __func__);
		return -ENODEV;
	}

	wrp = devm_kzalloc(&pdev->dev, sizeof(*wrp), GFP_KERNEL);
	if (!wrp)
		return -ENOMEM;

	wrp->dev = &pdev->dev;

	platform_set_drvdata(pdev, wrp);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pwrap-base");
	if (!res) {
		pr_info("could not get pwrap-base resource\n");
		return -ENODEV;
	}

	pwrap_base = devm_ioremap_resource(wrp->dev, res);
	if (IS_ERR(pwrap_base)) {
		pr_info("PWRAP iomap failed\n");
		return PTR_ERR(pwrap_base);
	}

	wrp->regmap = devm_regmap_init(wrp->dev, NULL, wrp, &pwrap_regmap_config);
	if (IS_ERR(wrp->regmap))
		return PTR_ERR(wrp->regmap);

	pwrap_irq = platform_get_irq(pdev, 0);

	ret = of_platform_populate(pwrap_node, NULL, NULL, wrp->dev);
	if (ret) {
		dev_dbg(wrp->dev, "failed to create child devices at %s\n", pwrap_node->full_name);
		return ret;
	}

	wrp->clk_spi = devm_clk_get(wrp->dev, "spi");
	if (IS_ERR(wrp->clk_spi)) {
		pr_err("pwrap clk get clk_spi failed\n");
		return PTR_ERR(wrp->clk_spi);
	}
	ret = clk_prepare_enable(wrp->clk_spi);
	if (ret)
		pr_err("pwrap clk init failed.");

	wrp->clk_wrap = devm_clk_get(wrp->dev, "pwrap");
	if (IS_ERR(wrp->clk_wrap)) {
		pr_err("pwrap clk get failed\n");
		return PTR_ERR(wrp->clk_wrap);
	}
	ret = clk_prepare_enable(wrp->clk_wrap);
	if (ret)
		pr_err("pwrap clk init failed.");
#endif

	mt_wrp = get_mt_pmic_wrap_drv();
	mt_wrp->store_hal = mt_pwrap_store_hal;
	mt_wrp->show_hal = mt_pwrap_show_hal;
	mt_wrp->wacs2_hal = pwrap_wacs2_hal;

	if (is_pwrap_init_done() == 0) {
#ifdef PMIC_WRAP_NO_PMIC
#else

		ret =
		    request_irq(MT_PMIC_WRAP_IRQ_ID, mt_pmic_wrap_irq, IRQF_TRIGGER_HIGH,
				PMIC_WRAP_DEVICE, 0);
#endif
		if (ret) {
			PWRAPERR("register IRQ failed (%d)\n", ret);
			return ret;
		}
	} else {
		PWRAPERR("not init (%d)\n", ret);
	}

	/* PWRAPERR("not init (%x)\n", is_pwrap_init_done); */

	return ret;
}


static struct platform_driver pwrap_drv = {
	.driver = {
		   .name = "mediatek,mt8163-pwrap",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(of_pwrap_match_tbl),
		   },
	.probe = pwrap_probe,
};

static int __init pwrap_hal_init(void)
{
	pr_notice("pwrap_hal_init\n");
	return platform_driver_register(&pwrap_drv);
}
postcore_initcall(pwrap_hal_init);
