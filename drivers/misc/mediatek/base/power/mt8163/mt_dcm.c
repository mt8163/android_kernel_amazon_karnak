#include <linux/init.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <linux/bug.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "mt_dcm.h"

/* #define DCM_DEFAULT_ALL_OFF */

#define DCM_HAVE_SECURE		0

#if DCM_HAVE_SECURE
#include <mach/mt_secure_api.h>
#endif

#if defined(__KERNEL__)

#if defined(CONFIG_OF)

static void __iomem *topckgen_base;
static void __iomem *dramc_base;
static void __iomem *infracfg_ao_base;
static void __iomem *mcucfg_base;
static phys_addr_t mcucfg_phys_base;

#define MCUCFG_NODE		"mediatek,mt8163-mcucfg"
#define INFRACFG_AO_NODE	"mediatek,mt8163-infracfg"
#define TOPCKGEN_NODE		"mediatek,mt8163-topckgen"
#define DRAMC_NODE		"mediatek,mt8163-dramco"

#undef INFRACFG_AO_BASE
#undef MCUCFG_BASE
#undef TOPCKGEN_BASE
#define INFRACFG_AO_BASE	(infracfg_ao_base)
#define MCUCFG_BASE		(mcucfg_base)
#define TOPCKGEN_BASE		(topckgen_base)
#define DRAMC_BASE		(dramc_base)

#else /* #if defined (CONFIG_OF) */

#undef INFRACFG_AO_BASE
#undef MCUCFG_BASE
#undef TOPCKGEN_BASE
#define INFRACFG_AO_BASE	0xF0001000
#define MCUCFG_BASE		0xF0200000
#define TOPCKGEN_BASE		0xF0000000
#define DRAMC_BASE		0xF0004000

#endif /* #if defined (CONFIG_OF) */

#else /* #if defined(__KERNEL__) */

#define INFRACFG_AO_BASE	0x10001000
#define MCUCFG_BASE		0x10200000
#define TOPCKGEN_BASE		0x10000000
#define DRAMC_BASE		0x10004000

#endif /* #if defined(__KERNEL__) */

#define DRAMC_PD_CTRL			(DRAMC_BASE + 0x1DC)
#define DFS_DRAMC_PD_CTRL		(DRAMC_BASE + 0x828)
#define INFRACFG_TOP_CKMUXSEL		(INFRACFG_AO_BASE + 0x0)
#define INFRACFG_TOP_CKDIV1		(INFRACFG_AO_BASE + 0x8)
#define INFRACFG_TOP_DCMCTL		(INFRACFG_AO_BASE + 0x10)
#define INFRACFG_TOP_DCMDBC		(INFRACFG_AO_BASE + 0x14)
#define INFRACFG_DCMCTL			(INFRACFG_AO_BASE + 0x50)
#define INFRACFG_INFRA_BUS_DCM_CTRL	(INFRACFG_AO_BASE + 0x70)
#define INFRACFG_PERI_BUS_DCM_CTRL	(INFRACFG_AO_BASE + 0x74)
#define INFRACFG_MEM_DCM_CTRL		(INFRACFG_AO_BASE + 0x78)
#define INFRACFG_DFS_MEM_DCM_CTRL	(INFRACFG_AO_BASE + 0x7C)

#define CLK_CFG_0			(TOPCKGEN_BASE + 0x0040)
#define	TOPCKGEN_CLK_MISC_CFG_0		(TOPCKGEN_BASE + 0x0104)

#define MCUCFG_L2C_SRAM_CTRL		(MCUCFG_BASE + 0x648)
#define MCUCFG_CCI_CLK_CTRL		(MCUCFG_BASE + 0x660)
#define MCUCFG_BUS_FABRIC_DCM_CTRL	(MCUCFG_BASE + 0x668)
#define MP0_AXI_CONFIG			(MCUCFG_BASE + 0x2C)
#define MP1_AXI_CONFIG			(MCUCFG_BASE + 0x22C)
#define ACINACTM			(1 << 4)

#define MCUCFG_L2C_SRAM_CTRL_PHY	(mcucfg_phys_base + 0x648)
#define MCUCFG_CCI_CLK_CTRL_PHY		(mcucfg_phys_base + 0x660)
#define MCUCFG_BUS_FABRIC_DCM_CTRL_PHY	(mcucfg_phys_base + 0x668)
#define MP0_AXI_CONFIG_PHY		(mcucfg_phys_base + 0x2C)
#define MP1_AXI_CONFIG_PHY		(mcucfg_phys_base + 0x22C)

#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
#define MCUSYS_SMC_WRITE(addr, val)  mcusys_smc_write_phy(addr##_PHY, val)
#else
#define MCUSYS_SMC_WRITE(addr, val)  mcusys_smc_write(addr, val)
#endif

#define TAG	"[Power/dcm] "

#define dcm_err(fmt, args...)	pr_err(TAG fmt, ##args)
#define dcm_warn(fmt, args...)	pr_warn(TAG fmt, ##args)
#define dcm_info(fmt, args...)	pr_debug(TAG fmt, ##args)
#define dcm_dbg(fmt, args...)	pr_debug(TAG fmt, ##args)
#define dcm_ver(fmt, args...)	pr_debug(TAG fmt, ##args)

/** macro **/
#define and(v, a)	((v) & (a))
#define or(v, o)	((v) | (o))
#define aor(v, a, o)	(((v) & (a)) | (o))

#define reg_read(addr)		readl(addr)
#define reg_write(addr, val)	writel(val, addr)

#define DCM_OFF		0
#define DCM_ON		1

/** global **/
static DEFINE_MUTEX(dcm_lock);
static unsigned int dcm_initiated;
static int armcore_dcm_fix_en;

/*****************************************
 * following is implementation per DCM module.
 * 1. per-DCM function is 1-argu with ON/OFF/MODE option.
 *****************************************/
typedef int (*DCM_FUNC)(int);

/** TOP_DCMCTL
 * 0	0	infra_dcm_enable
 * 4	4	ca7_dcm_enable
 * 5	5	ca7_dcm_wfi_enable
 * 6	6	ca7_dcm_wfe_enable
 * 8	8	ca15_dcm_enable (X)
 * 9	9	ca15_dcm_wfi_enable (X)
 * 10	10	ca15_dcm_wfe_enable (X)
 **/

enum ENUM_ARMCORE_DCM {
	ARMCORE_DCM_OFF = DCM_OFF,
	ARMCORE_DCM_MODE1 = DCM_ON,
	ARMCORE_DCM_MODE2 = DCM_ON + 1,
};

int dcm_armcore(enum ENUM_ARMCORE_DCM mode)
{

	if (mode == ARMCORE_DCM_OFF) {
		/* swithc to mode 2, and turn wfi/wfe-enable off */
		reg_write(INFRACFG_TOP_DCMCTL,
			  aor(reg_read(INFRACFG_TOP_DCMCTL),
			      ~(0x7 << 4), (0 << 4)));
		return 0;
	}

	if (mode == ARMCORE_DCM_MODE2) {
		/* switch to mode 2 */
		reg_write(INFRACFG_TOP_DCMCTL,
			  aor(reg_read(INFRACFG_TOP_DCMCTL),
			      ~(7 << 4), (6 << 4)));
	}

	else if (mode == ARMCORE_DCM_MODE1) {
		/* switch to mode 1, and mode 2 off */
		reg_write(INFRACFG_TOP_DCMCTL,
			  aor(reg_read(INFRACFG_TOP_DCMCTL),
			      ~(0x7 << 4), (7 << 4)));
	}

	return 0;
}

int dcm_armcore_pll_clkdiv(int pll, int div)
{
	int mux;

	if (pll == 2 || pll == 3)
		mux = 1 << (pll - 2);
	else
		mux = 0;

	reg_write(INFRACFG_TOP_CKMUXSEL,
		  aor(reg_read(INFRACFG_TOP_CKMUXSEL), ~(3 << 0), 0 << 0));

	reg_write(TOPCKGEN_CLK_MISC_CFG_0,
		  aor(reg_read(TOPCKGEN_CLK_MISC_CFG_0),
		      ~(0x03 << 4), mux << 4));

	reg_write(INFRACFG_TOP_CKMUXSEL,
		  aor(reg_read(INFRACFG_TOP_CKMUXSEL), ~(3 << 0), pll << 0));
	reg_write(INFRACFG_TOP_CKDIV1,
		  aor(reg_read(INFRACFG_TOP_CKDIV1), ~(0x1f << 0), div << 0));

	return 0;
}

enum ENUM_MCUSYS_DCM {
	MCUSYS_DCM_OFF = DCM_OFF,
	MCUSYS_DCM_ON = DCM_ON,
};

int dcm_mcusys(enum ENUM_MCUSYS_DCM on)
{
#if DCM_HAVE_SECURE
	if (on == MCUSYS_DCM_OFF) {
		/* MCUSYS bus DCM */
		MCUSYS_SMC_WRITE(MCUCFG_CCI_CLK_CTRL,
				 aor(reg_read(MCUCFG_CCI_CLK_CTRL),
				     ~(0x1 << 8), 0));
		/* L2C SRAM DCM */
		MCUSYS_SMC_WRITE(MCUCFG_L2C_SRAM_CTRL,
				 aor(reg_read(MCUCFG_L2C_SRAM_CTRL),
				     ~(1 << 0), 0));
		/* bus_fabric_dcm_ctrl */
		MCUSYS_SMC_WRITE(MCUCFG_BUS_FABRIC_DCM_CTRL,
				 aor(reg_read(MCUCFG_BUS_FABRIC_DCM_CTRL),
				     ~(0x0f0f), 0));
	} else {
		/* MCUSYS bus DCM */
		MCUSYS_SMC_WRITE(MCUCFG_CCI_CLK_CTRL,
				 aor(reg_read(MCUCFG_CCI_CLK_CTRL),
				     ~(0x1 << 8), (1 << 8)));
		/* L2C SRAM DCM */
		MCUSYS_SMC_WRITE(MCUCFG_L2C_SRAM_CTRL,
				 aor(reg_read(MCUCFG_L2C_SRAM_CTRL),
				     ~(1 << 0), (1 << 0)));
		/* bus_fabric_dcm_ctrl */
		MCUSYS_SMC_WRITE(MCUCFG_BUS_FABRIC_DCM_CTRL,
				 aor(reg_read(MCUCFG_BUS_FABRIC_DCM_CTRL),
				     ~(0x0f0f), (0x0f0f)));
	}
#endif /* DCM_HAVE_SECURE */

	return 0;
}

/** INFRACFG_INFRA_BUS_DCM_CTRL
 * 0	0	infra_dcm_rg_clkoff_en
 * 1	1	infra_dcm_rg_clkslow_en
 * 4	4	infra_dcm_rg_force_on
 * 9	5	infra_dcm_rg_fsel
 * 14	10	infra_dcm_rg_sfsel
 * 19	15	infra_dcm_dbc_rg_dbc_num
 * 20	20	infra_dcm_dbc_rg_dbc_en
 **/

#define INFRA_DCM_DEFAULT_MASK	((1 << 20) | (0x1f << 15) |	\
				 (0x1f << 10) | (0x1f << 5) | (3 << 0))
#define INFRA_DCM_DEFAULT (0x1f0603)

enum ENUM_INFRA_DCM {
	INFRA_DCM_OFF = DCM_OFF,
	INFRA_DCM_ON = DCM_ON,
};

int dcm_infra(enum ENUM_INFRA_DCM on)
{
	unsigned int val;

	val = (on == INFRA_DCM_ON) ? 3 : 0;
	reg_write(INFRACFG_INFRA_BUS_DCM_CTRL,
		  aor(reg_read(INFRACFG_INFRA_BUS_DCM_CTRL),
		      ~(0x03 << 0), val));

	return 0;
}

int dcm_infra_dbc(int cnt)
{
	reg_write(INFRACFG_INFRA_BUS_DCM_CTRL,
		  aor(reg_read(INFRACFG_INFRA_BUS_DCM_CTRL),
		      ~(0x1f << 15), (cnt << 15)));

	return 0;
}

int dcm_infra_rate(unsigned int fsel, unsigned int sfsel)
{
	int val;

	if (fsel > 5 || sfsel > fsel)
		BUG();

	val = (sfsel == 5) ? 0 : (1 << (14 - sfsel));
	val |= (fsel == 5) ? 0 : (1 << (9 - fsel));
	reg_write(INFRACFG_INFRA_BUS_DCM_CTRL,
		  aor(reg_read(INFRACFG_INFRA_BUS_DCM_CTRL),
		      ~((0x1f << 10) | (0x1f << 5)), val));

	return 0;
}

/** INFRACFG_PERI_BUS_DCM_CTRL
 * 0	0	peri_dcm_rg_clkoff_en
 * 1	1	peri_dcm_rg_clkslow_en
 * 2	2	peri_dcm_rg_force_clkoff
 * 3	3	peri_dcm_rg_force_clkslow
 * 4	4	peri_dcm_rg_force_on
 * 9	5	peri_dcm_rg_fsel
 * 14	10	peri_dcm_rg_sfsel
 * 19	15	peri_dcm_dbc_rg_dbc_num
 * 20	20	peri_dcm_dbc_rg_dbc_en
 * 21	21	re_usb_dcm_en
 * 22	22	re_pmic_dcm_en
 * 27	23	pmic_cnt_mst_rg_sfsel
 * 28	28	re_icusb_dcm_en
 * 29	29	rg_audio_dcm_en
 **/
#define PERI_DCM_DEFAULT_MASK	((1 << 20) | (0x1f << 15) |	\
				 (0x1f << 10) | (0x1f << 5) | (3 << 0))
#define PERI_DCM_DEFAULT (0x001f0603)

#define MISC_DCM_DEFAULT_MASK  ((1 << 29) | (1 << 28) | (1 << 22) | (1 << 21))
#define MISC_DCM_DEFAULT (0xb0000000)

#define PMIC_DCM_DEFAULT (1 << 22)
#define USB_DCM_DEFAULT (1 << 21)
#define ICUSB_DCM_DEFAULT (1 << 28)
#define AUDIO_DCM_DEFAULT (1 << 29)

enum ENUM_MISC_DCM {
	MISC_DCM_OFF = DCM_OFF,
	PMIC_DCM_OFF = DCM_OFF,
	USB_DCM_OFF = DCM_OFF,
	ICUSB_DCM_OFF = DCM_OFF,
	AUDIO_DCM_OFF = DCM_OFF,

	MISC_DCM_ON = DCM_ON,
	PMIC_DCM_ON = DCM_ON,
	USB_DCM_ON = DCM_ON,
	ICUSB_DCM_ON = DCM_ON,
	AUDIO_DCM_ON = DCM_ON,
};

/** argu REG, is 1-bit hot value **/
int _dcm_peri_misc(unsigned int reg, int on)
{
	reg_write(INFRACFG_PERI_BUS_DCM_CTRL,
		  aor(reg_read(INFRACFG_PERI_BUS_DCM_CTRL),
		      ~reg, (on) ? reg : 0));

	return 0;
}

int dcm_pmic(enum ENUM_MISC_DCM on)
{
	_dcm_peri_misc(PMIC_DCM_DEFAULT, on);

	return 0;
}

int dcm_usb(enum ENUM_MISC_DCM on)
{
	_dcm_peri_misc(USB_DCM_DEFAULT, on);

	return 0;
}

int dcm_icusb(enum ENUM_MISC_DCM on)
{
	_dcm_peri_misc(ICUSB_DCM_DEFAULT, on);

	return 0;
}

int dcm_audio(enum ENUM_MISC_DCM on)
{
	_dcm_peri_misc(AUDIO_DCM_DEFAULT, on);

	return 0;
}

enum ENUM_PERI_DCM {
	PERI_DCM_OFF = DCM_OFF,
	PERI_DCM_ON = DCM_ON,
};

int dcm_peri(enum ENUM_PERI_DCM on)
{
	unsigned int val = (on == PERI_DCM_ON) ? 3 : 0;

	reg_write(INFRACFG_PERI_BUS_DCM_CTRL,
		  aor(reg_read(INFRACFG_PERI_BUS_DCM_CTRL), ~(3 << 0), val));

	return 0;
}

int dcm_peri_dbc(int cnt)
{
	reg_write(INFRACFG_PERI_BUS_DCM_CTRL,
		  aor(reg_read(INFRACFG_PERI_BUS_DCM_CTRL),
		      ~(0x1f << 15), (cnt << 15)));

	return 0;
}

/** order is 0~5, respectly 1/1 ~ 1/32 **/
int dcm_peri_rate(unsigned int fsel, unsigned int sfsel)
{
	int val;

	if (fsel > 5 || sfsel > fsel)
		BUG();

	val = (sfsel == 5) ? 0 : (1 << (14 - sfsel));
	val |= (fsel == 5) ? 0 : (1 << (9 - fsel));
	reg_write(INFRACFG_PERI_BUS_DCM_CTRL,
		  aor(reg_read(INFRACFG_PERI_BUS_DCM_CTRL),
		      ~((0x1f << 10) | (0x1f << 5)), val));

	return 0;
}

/** MEM_DCM_CTRL
 *    0		0	mem_dcm_apb_toggle
 *    5		1	mem_dcm_apb_sel
 *    6		6	mem_dcm_force_on
 *    7		7	mem_dcm_dcm_en
 *    8		8	mem_dcm_dbc_en
 *    15	9	mem_dcm_dbc_cnt
 *    20	16	mem_dcm_fsel
 *    25	21	mem_dcm_idle_fsel
 *    26	26	mem_dcm_force_off
 *    28	28	phy_cg_off_disable
 *    29	29	pipe_0_cg_off_disable
 *    31	31	mem_dcm_hwcg_off_disable
 **/
#define EMI_3PLL_MODE_DEFAULT_MASK	((0x1f << 21) | (0x1f << 16) |	\
					 (0x3f << 9) | (1 << 8) |	\
					 (1 << 7) | (1 << 6) | (0x1f << 1))
#define EMI_3PLL_MODE_DEFAULT	((0x1f << 21) | (0 << 16) | (1 << 9) |	\
				 (1 << 8) | (1 << 7) | (0 << 6) | (0x1f << 1))
#define EMI_3PLL_MODE_DCM_OFF()                                         \
	do {                                                            \
		reg_write(INFRACFG_MEM_DCM_CTRL,                        \
			  aor(reg_read(INFRACFG_MEM_DCM_CTRL),          \
			      ~(EMI_3PLL_MODE_DEFAULT_MASK),            \
			      (EMI_3PLL_MODE_DEFAULT & ~(1 << 7))));    \
									\
		/* toggle */                                            \
		reg_write(INFRACFG_MEM_DCM_CTRL,                        \
			  or(reg_read(INFRACFG_MEM_DCM_CTRL), 1));      \
		reg_write(INFRACFG_MEM_DCM_CTRL,                        \
			  and(reg_read(INFRACFG_MEM_DCM_CTRL), ~1));    \
	} while (0)

#define EMI_3PLL_MODE_DCM_ON()                                          \
	do {                                                            \
		reg_write(INFRACFG_MEM_DCM_CTRL,                        \
			  aor(reg_read(INFRACFG_MEM_DCM_CTRL),          \
			      ~(EMI_3PLL_MODE_DEFAULT_MASK),            \
			      (EMI_3PLL_MODE_DEFAULT)));                \
									\
		/* toggle */                                            \
		reg_write(INFRACFG_MEM_DCM_CTRL,                        \
			  or(reg_read(INFRACFG_MEM_DCM_CTRL), 1));      \
		reg_write(INFRACFG_MEM_DCM_CTRL,                        \
			  and(reg_read(INFRACFG_MEM_DCM_CTRL), ~1));    \
	} while (0)

/*
 * Return value definitions
 *  0: 26M,
 *  1: 3 PLL,
 *  2: 1 PLL
 */
unsigned int __attribute__((weak)) mt_get_clk_mem_sel(void)
{
	unsigned int val;

	/* CLK_CFG_0(0x10000040)[9:8] */
	/* clk_mem_sel */
	/* 2'b00:clk26m */
	/* 2'b01:dmpll_ck->3PLL */
	/* 2'b10:ddr_x1_ck->1PLL */
	val = reg_read(CLK_CFG_0);
	val = (val >> 8) & 0x3;

	return val;
}

#define IS_EMI_1PLL_MODE() (mt_get_clk_mem_sel() == 0x2)

/** DFS_MEM_DCM_CTRL
 * 0	0	mem_dcm_apb_toggle
 * 5	1	mem_dcm_apb_sel
 * 6	6	mem_dcm_force_on
 * 7	7	mem_dcm_dcm_en
 * 8	8	mem_dcm_dbc_en
 * 15	9	mem_dcm_dbc_cnt
 * 20	16	mem_dcm_fsel
 * 25	21	mem_dcm_idle_fsel
 * 26	26	mem_dcm_force_off
 * 28	28	phy_cg_off_disable
 * 29	29	pipe_0_cg_off_disable
 * 31	31	mem_dcm_hwcg_off_disable
 **/
#define EMI_1PLL_MODE_DEFAULT_MASK	((1 << 28) | (0x1f < 21) |	\
					 (0x1f << 16) | (0x3f << 9) |	\
					 (1 << 8) | (1 << 7) |		\
					 (1 << 6) | (0x1f << 1))
#define EMI_1PLL_MODE_DEFAULT	((1 << 28) | (0x7 << 21) | (0 << 16) |	\
				 (1 << 9) | (1 << 8) | (1 << 7) |	\
				 (0 << 6) | (0x1f << 1))

#define EMI_1PLL_MODE_DCM_OFF()                                         \
	do {                                                            \
		reg_write(INFRACFG_DFS_MEM_DCM_CTRL,                    \
			  aor(reg_read(INFRACFG_DFS_MEM_DCM_CTRL),      \
			      ~(EMI_1PLL_MODE_DEFAULT_MASK),            \
			      (EMI_1PLL_MODE_DEFAULT & ~(1 << 7)))); \
									\
		/* toggle */                                            \
		reg_write(INFRACFG_DFS_MEM_DCM_CTRL,                    \
			  or(reg_read(INFRACFG_DFS_MEM_DCM_CTRL), 1));  \
		reg_write(INFRACFG_DFS_MEM_DCM_CTRL,                    \
			  and(reg_read(INFRACFG_DFS_MEM_DCM_CTRL), ~1)); \
									\
		/* gate off the 1pll mode clock before mux sel */       \
		if (!IS_EMI_1PLL_MODE())                                \
			reg_write(INFRACFG_DFS_MEM_DCM_CTRL,            \
				or(reg_read(INFRACFG_DFS_MEM_DCM_CTRL), \
				   (1 << 31)));                         \
	} while (0)

#define EMI_1PLL_MODE_DCM_ON()                                          \
	do {                                                            \
		if (!IS_EMI_1PLL_MODE())                                \
			reg_write(INFRACFG_DFS_MEM_DCM_CTRL,            \
				or(reg_read(INFRACFG_DFS_MEM_DCM_CTRL), \
				   (1 << 31)));                         \
									\
		reg_write(INFRACFG_DFS_MEM_DCM_CTRL,                    \
			  aor(reg_read(INFRACFG_DFS_MEM_DCM_CTRL),      \
			      ~(EMI_1PLL_MODE_DEFAULT_MASK),            \
			      (EMI_1PLL_MODE_DEFAULT)));                \
		/* ungate the 1pll mode clock  */                       \
		reg_write(INFRACFG_DFS_MEM_DCM_CTRL,                    \
			  and(reg_read(INFRACFG_DFS_MEM_DCM_CTRL),      \
			      ~(1 << 31)));                             \
									\
		/* toggle */                                            \
		reg_write(INFRACFG_DFS_MEM_DCM_CTRL,                    \
			  or(reg_read(INFRACFG_DFS_MEM_DCM_CTRL), 1));  \
		reg_write(INFRACFG_DFS_MEM_DCM_CTRL,                    \
			  and(reg_read(INFRACFG_DFS_MEM_DCM_CTRL), ~1)); \
	} while (0)

enum ENUM_EMI_DCM {
	EMI_DCM_OFF = DCM_OFF,
	EMI_DCM_3PLL_MODE = DCM_ON,
	EMI_DCM_1PLL_MODE = (DCM_ON + 1),
};

int dcm_emi(enum ENUM_EMI_DCM mode)
{
	if (mode == EMI_DCM_OFF) {
		EMI_3PLL_MODE_DCM_OFF();
		EMI_1PLL_MODE_DCM_OFF();

		return 0;
	}

	/* 1PLL mode */
	if (mode == EMI_DCM_1PLL_MODE) {
		EMI_3PLL_MODE_DCM_OFF();
		EMI_1PLL_MODE_DCM_ON();
	} else if (mode == EMI_DCM_3PLL_MODE) {
		EMI_3PLL_MODE_DCM_ON();
		EMI_1PLL_MODE_DCM_OFF();
	}

	return 0;
}

int dcm_emi_dbc(enum ENUM_EMI_DCM mode, int cnt)
{
	if (mode == EMI_DCM_1PLL_MODE) {
		reg_write(INFRACFG_DFS_MEM_DCM_CTRL,
			  aor(reg_read(INFRACFG_DFS_MEM_DCM_CTRL),
			      ~(0x1f << 9), cnt));
	} else {
		reg_write(INFRACFG_MEM_DCM_CTRL,
			  aor(reg_read(INFRACFG_MEM_DCM_CTRL),
			      ~(0x1f << 9), cnt));
	}

	return 0;
}

enum {
	ARMCORE_DCM_TYPE = (1U << 0),
	MCUSYS_DCM_TYPE = (1U << 1),
	INFRA_DCM_TYPE = (1U << 2),
	PERI_DCM_TYPE = (1U << 3),
	EMI_DCM_TYPE = (1U << 4),
	PMIC_DCM_TYPE = (1U << 5),
	USB_DCM_TYPE = (1U << 6),
	ICUSB_DCM_TYPE = (1U << 7),
	AUDIO_DCM_TYPE = (1U << 8),

	NR_DCM_TYPE = 9,
};

#define ALL_DCM_TYPE	\
	(ARMCORE_DCM_TYPE | MCUSYS_DCM_TYPE | INFRA_DCM_TYPE | PERI_DCM_TYPE | \
	 EMI_DCM_TYPE | PMIC_DCM_TYPE | USB_DCM_TYPE | ICUSB_DCM_TYPE | \
	 AUDIO_DCM_TYPE)

struct _dcm {
	int current_state;
	int saved_state;
	int disable_refcnt;
	int default_state;
	DCM_FUNC func;
	int typeid;
	char *name;
};

static struct _dcm dcm_array[NR_DCM_TYPE] = {
	{
	 .typeid = ARMCORE_DCM_TYPE,
	 .name = "ARMCORE_DCM",
	 .func = (DCM_FUNC) dcm_armcore,
	 .current_state = ARMCORE_DCM_MODE1,
	 .default_state = ARMCORE_DCM_MODE1,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = MCUSYS_DCM_TYPE,
	 .name = "MCUSYS_DCM",
	 .func = (DCM_FUNC) dcm_mcusys,
	 .current_state = MCUSYS_DCM_ON,
	 .default_state = MCUSYS_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = INFRA_DCM_TYPE,
	 .name = "INFRA_DCM",
	 .func = (DCM_FUNC) dcm_infra,
	 .current_state = INFRA_DCM_ON,
	 .default_state = INFRA_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = PERI_DCM_TYPE,
	 .name = "PERI_DCM",
	 .func = (DCM_FUNC) dcm_peri,
	 .current_state = PERI_DCM_ON,
	 .default_state = PERI_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = EMI_DCM_TYPE,
	 .name = "EMI_DCM",
	 .func = (DCM_FUNC) dcm_emi,
	 .current_state = EMI_DCM_3PLL_MODE,
	 .default_state = EMI_DCM_3PLL_MODE,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = PMIC_DCM_TYPE,
	 .name = "PMIC_DCM",
	 .func = (DCM_FUNC) dcm_pmic,
	 .current_state = PMIC_DCM_ON,
	 .default_state = PMIC_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = USB_DCM_TYPE,
	 .name = "USB_DCM",
	 .func = (DCM_FUNC) dcm_usb,
	 .current_state = USB_DCM_ON,
	 .default_state = USB_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = ICUSB_DCM_TYPE,
	 .name = "ICUSB_DCM",
	 .func = (DCM_FUNC) dcm_icusb,
	 .current_state = ICUSB_DCM_ON,
	 .default_state = ICUSB_DCM_ON,
	 .disable_refcnt = 0,
	 },
	{
	 .typeid = AUDIO_DCM_TYPE,
	 .name = "AUDIO_DCM",
	 .func = (DCM_FUNC) dcm_audio,
	 .current_state = AUDIO_DCM_ON,
	 .default_state = AUDIO_DCM_ON,
	 .disable_refcnt = 0,
	 },
};

/*****************************************
 * DCM driver will provide regular APIs :
 * 1. dcm_restore(type) to recovery CURRENT_STATE before any power-off reset.
 * 2. dcm_set_default(type) to reset as cold-power-on init state.
 * 3. dcm_disable(type) to disable all dcm.
 * 4. dcm_set_state(type) to set dcm state.
 * 5. dcm_dump_state(type) to show CURRENT_STATE.
 * 6. /sys/power/dcm_state interface:  'restore', 'disable', 'dump', 'set'.
 *
 * spsecified APIs for workaround:
 * 1. (definitely no workaround now)
 *****************************************/

void dcm_set_default(unsigned int type)
{
	int i;
	struct _dcm *dcm;

	dcm_info("[%s]type:0x%08x\n", __func__, type);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
		if (type & dcm->typeid) {
			if (dcm->typeid == EMI_DCM_TYPE) {
				if (IS_EMI_1PLL_MODE())
					dcm->default_state = EMI_DCM_1PLL_MODE;
			}

			dcm->saved_state = dcm->current_state =
				dcm->default_state;
			dcm->disable_refcnt = 0;
			dcm->func(dcm->current_state);

			dcm_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state,
				 dcm->disable_refcnt);

		}
	}

	mutex_unlock(&dcm_lock);
}

void dcm_set_state(unsigned int type, int state)
{
	int i;
	struct _dcm *dcm;

	dcm_info("[%s]type:0x%08x, set:%d\n", __func__, type, state);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &dcm_array[0]; type && i < NR_DCM_TYPE; i++, dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			dcm->saved_state = state;
			if (dcm->disable_refcnt == 0) {
				dcm->current_state = state;
				dcm->func(dcm->current_state);
			}

			dcm_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state,
				 dcm->disable_refcnt);
		}
	}

	mutex_unlock(&dcm_lock);
}

void dcm_disable(unsigned int type)
{
	int i;
	struct _dcm *dcm;

	dcm_info("[%s]type:0x%08x\n", __func__, type);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &dcm_array[0]; type && i < NR_DCM_TYPE; i++, dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			dcm->current_state = DCM_OFF;
			dcm->disable_refcnt++;
			dcm->func(dcm->current_state);

			dcm_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state,
				 dcm->disable_refcnt);
		}
	}

	mutex_unlock(&dcm_lock);
}

void dcm_restore(unsigned int type)
{
	int i;
	struct _dcm *dcm;

	dcm_info("[%s]type:0x%08x\n", __func__, type);

	mutex_lock(&dcm_lock);

	for (i = 0, dcm = &dcm_array[0]; type && i < NR_DCM_TYPE; i++, dcm++) {
		if (type & dcm->typeid) {
			type &= ~(dcm->typeid);

			if (dcm->disable_refcnt > 0)
				dcm->disable_refcnt--;
			if (dcm->disable_refcnt == 0) {
				dcm->current_state = dcm->saved_state;
				dcm->func(dcm->current_state);
			}

			dcm_info("[%16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state,
				 dcm->disable_refcnt);

		}
	}

	mutex_unlock(&dcm_lock);
}

void dcm_dump_state(int type)
{
	int i;
	struct _dcm *dcm;

	dcm_info("\n******** dcm dump state *********\n");
	for (i = 0, dcm = &dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
		if (type & dcm->typeid) {
			dcm_info("[%-16s 0x%08x] current state:%d (%d)\n",
				 dcm->name, dcm->typeid, dcm->current_state,
				 dcm->disable_refcnt);
		}
	}
}

#define REG_DUMP(addr)	\
	dcm_info("%-30s(0x%p): 0x%08x\n", #addr, addr, reg_read(addr))

void dcm_dump_regs(void)
{
	dcm_info("\n******** dcm dump register *********\n");
	REG_DUMP(INFRACFG_TOP_CKMUXSEL);
	REG_DUMP(INFRACFG_TOP_CKDIV1);
	REG_DUMP(INFRACFG_TOP_DCMCTL);
	REG_DUMP(INFRACFG_TOP_DCMDBC);
	REG_DUMP(INFRACFG_DCMCTL);
	REG_DUMP(INFRACFG_INFRA_BUS_DCM_CTRL);
	REG_DUMP(INFRACFG_PERI_BUS_DCM_CTRL);
	REG_DUMP(INFRACFG_MEM_DCM_CTRL);
	REG_DUMP(INFRACFG_DFS_MEM_DCM_CTRL);

	REG_DUMP(MCUCFG_L2C_SRAM_CTRL);
	REG_DUMP(MCUCFG_CCI_CLK_CTRL);
	REG_DUMP(MCUCFG_BUS_FABRIC_DCM_CTRL);
}

#if defined(CONFIG_PM)
static ssize_t dcm_state_show(struct kobject *kobj,
			      struct kobj_attribute *attr, char *buf)
{
	const char *sn = "/sys/power/dcm_state";
	int len = 0;
	char *p = buf;
	int i;
	struct _dcm *dcm;

	p += sprintf(p, "\n******** dcm dump state *********\n");
	for (i = 0, dcm = &dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
		p += sprintf(p, "[%-16s 0x%08x] current state:%d (%d)\n",
			     dcm->name, dcm->typeid, dcm->current_state,
			     dcm->disable_refcnt);
	}

	p += sprintf(p, "\n********** dcm_state help *********\n");
	p += sprintf(p, "set:           echo set [mask] [mode] > %s\n", sn);
	p += sprintf(p, "disable:       echo disable [mask] > %s\n", sn);
	p += sprintf(p, "restore:       echo restore [mask] > %s\n", sn);
	p += sprintf(p, "dump:          echo dump [mask] > %s\n", sn);
	p += sprintf(p, "***** [mask] is hexl bit mask of dcm;\n");
	p += sprintf(p, "***** [mode] is type of DCM to set and retained\n");

	len = p - buf;
	return len;
}

static ssize_t dcm_state_store(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t n)
{
	char cmd[16];
	unsigned int mask;

	if (sscanf(buf, "%15s %x", cmd, &mask) == 2) {
		mask &= ALL_DCM_TYPE;

		if (!strcmp(cmd, "restore")) {
			dcm_restore(mask);
		} else if (!strcmp(cmd, "disable")) {
			dcm_disable(mask);
		} else if (!strcmp(cmd, "dump")) {
			dcm_dump_state(mask);
			dcm_dump_regs();
		} else if (!strcmp(cmd, "armcore_fix")) {
			if (mask == 0) {
				unsigned int val;

				val = reg_read(MP0_AXI_CONFIG);
				val = and(val, ~ACINACTM);
#if DCM_HAVE_SECURE
				MCUSYS_SMC_WRITE(MP0_AXI_CONFIG, val);
#endif
			}
			armcore_dcm_fix_en = mask;
		} else if (!strcmp(cmd, "set")) {
			int mode;

			if (sscanf(buf, "%15s %x %d", cmd, &mask, &mode) == 3) {
				mask &= ALL_DCM_TYPE;

				dcm_set_state(mask, mode);
			}
		} else {
			dcm_info("SORRY, do not support your command: %s\n",
				 cmd);
		}

		return n;
	}

	dcm_info("SORRY, do not support your command.\n");

	return -EINVAL;
}

static struct kobj_attribute dcm_state_attr = {
	.attr = {
		 .name = "dcm_state",
		 .mode = 0644,
		 },
	.show = dcm_state_show,
	.store = dcm_state_store,
};

static int dcm_cpu_notify(struct notifier_block *self, unsigned long action,
			  void *hcpu)
{
	cpumask_t mp1_cpu_online;
	int scpu = (long)hcpu;
	unsigned int val;

	if (armcore_dcm_fix_en == 0)
		return NOTIFY_OK;

	if (scpu < 4)
		return NOTIFY_OK;

	cpumask_andnot(&mp1_cpu_online, cpu_online_mask, cpumask_of(scpu));
	cpumask_shift_right(&mp1_cpu_online, &mp1_cpu_online, 4);
	if (!cpumask_empty(&mp1_cpu_online))
		return NOTIFY_OK;

	switch (action) {
	case CPU_DOWN_FAILED:
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		val = reg_read(MP0_AXI_CONFIG);
		val = and(val, ~ACINACTM);
#if DCM_HAVE_SECURE
		MCUSYS_SMC_WRITE(MP0_AXI_CONFIG, val);
#endif
		break;

	case CPU_ONLINE:
		if (and(reg_read(MP0_AXI_CONFIG), ACINACTM)) {
			dcm_info("axi_config:0x%0x\n",
				 reg_read(MP0_AXI_CONFIG));
			BUG();
		}
		break;

	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		val = reg_read(MP0_AXI_CONFIG);
		val = or(val, ACINACTM);
#if DCM_HAVE_SECURE
		MCUSYS_SMC_WRITE(MP0_AXI_CONFIG, val);
#endif
		break;

	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block dcm_cpu_nb = {
	.notifier_call = dcm_cpu_notify,
};

#endif /* #if defined(CONFIG_PM) */

#if defined(CONFIG_OF)

static int mt_dcm_dts_map(void)
{
	struct device_node *node;
	struct resource r;

	/* topckgen */
	node = of_find_compatible_node(NULL, NULL, TOPCKGEN_NODE);
	if (!node) {
		dcm_info("error: cannot find node " TOPCKGEN_NODE);
		BUG();
	}
	topckgen_base = of_iomap(node, 0);
	if (!topckgen_base) {
		dcm_info("error: cannot iomap " TOPCKGEN_NODE);
		BUG();
	}

	/* mcucfg */
	node = of_find_compatible_node(NULL, NULL, MCUCFG_NODE);
	if (!node) {
		dcm_info("error: cannot find node " MCUCFG_NODE);
		BUG();
	}
	if (of_address_to_resource(node, 0, &r)) {
		dcm_info("error: cannot get phys addr" MCUCFG_NODE);
		BUG();
	}
	mcucfg_phys_base = r.start;

	mcucfg_base = of_iomap(node, 0);
	if (!mcucfg_base) {
		dcm_info("error: cannot iomap " MCUCFG_NODE);
		BUG();
	}

	/* dramc */
	node = of_find_compatible_node(NULL, NULL, DRAMC_NODE);
	if (!node) {
		dcm_info("error: cannot find node " DRAMC_NODE);
		BUG();
	}
	dramc_base = of_iomap(node, 0);
	if (!dramc_base) {
		dcm_info("error: cannot iomap " DRAMC_NODE);
		BUG();
	}

	/* infracfg_ao */
	node = of_find_compatible_node(NULL, NULL, INFRACFG_AO_NODE);
	if (!node) {
		dcm_info("error: cannot find node " INFRACFG_AO_NODE);
		BUG();
	}
	infracfg_ao_base = of_iomap(node, 0);
	if (!infracfg_ao_base) {
		dcm_info("error: cannot iomap " INFRACFG_AO_NODE);
		BUG();
	}

	of_node_put(node);

	return 0;
}

#else /* #if defined(CONFIG_OF) */

static int mt_dcm_dts_map(void)
{
	return 0;
}

#endif /* #if defined(CONFIG_OF) */

int mt_dcm_init(void)
{
	if (dcm_initiated)
		return 0;

	dcm_initiated = 1;
	mt_dcm_dts_map();

	/** fixme, should we set this dramc register for each 3pll/1pll switch?
	 * to enable DRAMC_PD_CTRL[25]: DCMEN, otherwise EMI DCM will not work.
	 */
	reg_write(DRAMC_PD_CTRL, or(reg_read(DRAMC_PD_CTRL), 1 << 25));
	reg_write(DFS_DRAMC_PD_CTRL, or(reg_read(DFS_DRAMC_PD_CTRL), 1 << 25));

#if !defined(DCM_DEFAULT_ALL_OFF)
	dcm_set_default(ALL_DCM_TYPE);
#else /* #if !defined (DCM_DEFAULT_ALL_OFF) */
	dcm_set_state(ALL_DCM_TYPE, DCM_OFF);
#endif /* #if !defined (DCM_DEFAULT_ALL_OFF) */

#if defined(CONFIG_PM)

	do {
		int err = 0;

		err = sysfs_create_file(power_kobj, &dcm_state_attr.attr);

		if (err)
			dcm_err("[%s]: fail to create sysfs\n", __func__);
	} while (0);

#endif /* #if defined(CONFIG_PM) */

	register_cpu_notifier(&dcm_cpu_nb);

	return 0;
}

late_initcall(mt_dcm_init);

/**** public APIs *****/

void mt_dcm_emi_1pll_mode(void)
{
	mt_dcm_init();
	dcm_set_state(EMI_DCM_TYPE, EMI_DCM_1PLL_MODE);
}
EXPORT_SYMBOL(mt_dcm_emi_1pll_mode);

void mt_dcm_emi_off(void)
{
	mt_dcm_init();

	/* using dcm_set_state to keep 'off' instead of dcm_disable(EMI) */
	dcm_set_state(EMI_DCM_TYPE, EMI_DCM_OFF);
}
EXPORT_SYMBOL(mt_dcm_emi_off);

void mt_dcm_emi_3pll_mode(void)
{
	mt_dcm_init();
	dcm_set_state(EMI_DCM_TYPE, EMI_DCM_3PLL_MODE);
}
EXPORT_SYMBOL(mt_dcm_emi_3pll_mode);

void mt_dcm_disable(void)
{
	mt_dcm_init();
	dcm_disable(ALL_DCM_TYPE);
}
EXPORT_SYMBOL(mt_dcm_disable);

void mt_dcm_restore(void)
{
	mt_dcm_init();
	dcm_restore(ALL_DCM_TYPE);
}
EXPORT_SYMBOL(mt_dcm_restore);
