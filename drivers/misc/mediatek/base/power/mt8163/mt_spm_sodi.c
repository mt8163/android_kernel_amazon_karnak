#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/irqchip/mt-gic.h>
#include <linux/kernel.h>
#include <linux/lockdep.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <mt-plat/mt_cirq.h>

#include "mt_spm_internal.h"
#include "mt_spm_idle.h"
#include "mt_cpuidle.h"
#include "mt_cpufreq.h"

/**************************************
 * only for internal debug
 **************************************/
/* FIXME: for FPGA early porting */
#define  CONFIG_MTK_LDVT

#define SODI_DVT_APxGPT 0	/*0:disable, 1: enable : use in android load: mt_idle.c and mt_spm_sodi.c */
#define SODI_DVT_BLOCK_BF_WFI 0	/*blcok code before while issue WFI for step by step current measurement */
#define SODI_DVT_SPM_DBG_MODE 0	/*1: using debug mode fw, 0: normal fw */
#define SODI_DVT_SPM_DBG_MODE_1PLL 0	/*set pcm_reserve bit 10 */
#define SODI_DVT_SPM_MEM_RW_TEST 0
#define SODI_DVT_MAGIC_NUM 0xa5a5a5a5
#define SODI_MEMPLL_RESETMODE_FW 1	/*may usie reset mode fw+SODI_DVT_SPM_DBG_MODE_1PLL should use together */
#define SODI_SPM_DBG_MEMPLL_RESETMODE_FW 0	/*DBG Mode FW + MEMPLL Reset mode */

#if SODI_DVT_SPM_MEM_RW_TEST
static u32 magicArray[16] = {
	SODI_DVT_MAGIC_NUM, SODI_DVT_MAGIC_NUM, SODI_DVT_MAGIC_NUM, SODI_DVT_MAGIC_NUM,
	SODI_DVT_MAGIC_NUM, SODI_DVT_MAGIC_NUM, SODI_DVT_MAGIC_NUM, SODI_DVT_MAGIC_NUM,
	SODI_DVT_MAGIC_NUM, SODI_DVT_MAGIC_NUM, SODI_DVT_MAGIC_NUM, SODI_DVT_MAGIC_NUM,
	SODI_DVT_MAGIC_NUM, SODI_DVT_MAGIC_NUM, SODI_DVT_MAGIC_NUM, SODI_DVT_MAGIC_NUM,
};
#endif

#ifdef CONFIG_MTK_LDVT
#define SPM_BYPASS_SYSPWREQ     1
#else
#define SPM_BYPASS_SYSPWREQ     0
#endif


#if (SODI_DVT_APxGPT)		/* APxGPT test only */
#if (SODI_DVT_BLOCK_BF_WFI)
#define WAKE_SRC_FOR_SODI WAKE_SRC_EINT

#else
#define WAKE_SRC_FOR_SODI WAKE_SRC_GPT

#endif
#else
#define WAKE_SRC_FOR_SODI \
	(WAKE_SRC_MD32_WDT | WAKE_SRC_KP | WAKE_SRC_GPT | WAKE_SRC_CONN2AP | WAKE_SRC_EINT |  \
	 WAKE_SRC_CONN_WDT | WAKE_SRC_MD32_SPM | WAKE_SRC_USB_CD | WAKE_SRC_USB_PDN | WAKE_SRC_AFE |  \
	 WAKE_SRC_CIRQ | WAKE_SRC_SYSPWREQ)
#endif

#define WAKE_SRC_FOR_MD32  0                                          \
				/* (WAKE_SRC_AUD_MD32) */

#define I2C_CHANNEL 1

#ifdef CONFIG_OF
#define MCUCFG_BASE          spm_mcucfg
#else
#define MCUCFG_BASE          (0xF0200000)	/* 0x1020_0000 */
#endif
#define MP0_AXI_CONFIG          (MCUCFG_BASE + 0x2C)
#define ACINACTM                (1<<4)

/* ========================================== */
/* PCM code for SODI (Screen On Deep Idle) */
/*  */
/* core 0 : GPT 4 */
/* ========================================== */
static const u32 sodi_binary[] = {
	0x81411801, 0xd8000125, 0x17c07c1f, 0x18c0001f, 0x10006240, 0xe0e00016,
	0xe0e0001e, 0xe0e0000e, 0xe0e0000f, 0x803e0400, 0x1b80001f, 0x20000050,
	0x803e8400, 0x803f0400, 0x803f8400, 0x1b80001f, 0x20000208, 0x803d0400,
	0x1b80001f, 0x20000034, 0x80380400, 0xa01d8400, 0x1b80001f, 0x20000034,
	0x803d8400, 0x803b0400, 0x1b80001f, 0x20000158, 0x80340400, 0x80310400,
	0x1b80001f, 0x2000000a, 0x18c0001f, 0x10006240, 0xe0e0000d, 0x81fa0407,
	0x18c0001f, 0x100040f4, 0x1910001f, 0x100040f4, 0xa11c8404, 0xe0c00004,
	0x813c8404, 0xe0c00004, 0x1b80001f, 0x20000100, 0x81f08407, 0xe8208000,
	0x10006354, 0xfff01b01, 0xa1d80407, 0xa1dc0407, 0xa1de8407, 0xa1df0407,
	0x1b00001f, 0xbffce7ff, 0xf0000000, 0x17c07c1f, 0x1b80001f, 0x20000fdf,
	0x1a50001f, 0x10006608, 0x80c9a401, 0x810aa401, 0x10918c1f, 0xa0939002,
	0x80ca2401, 0x810ba401, 0xa09c0c02, 0xa0979002, 0xa0950402, 0x8080080d,
	0xd8200a82, 0x17c07c1f, 0x1b00001f, 0x3ffce7ff, 0x1b80001f, 0x20000004,
	0xd80011cc, 0x17c07c1f, 0x1b00001f, 0xbffce7ff, 0xd00011c0, 0x17c07c1f,
	0x81f80407, 0x81fc0407, 0x81fe8407, 0x81ff0407, 0xe8208000, 0x10006354,
	0xfff01b47, 0x1b80001f, 0x20000020, 0x02000408, 0x1880001f, 0x10006320,
	0xc0c01f40, 0xe080000f, 0xd8000943, 0x17c07c1f, 0xe080001f, 0x1950001f,
	0x10006b00, 0x81429401, 0xd8000ca5, 0x17c07c1f, 0xa1da0407, 0xa0110400,
	0xa0140400, 0xa01b0400, 0xa0180400, 0xa01d0400, 0x1950001f, 0x10006b00,
	0x81431401, 0xd8000e05, 0x17c07c1f, 0xa01f8400, 0xa01f0400, 0xa01e8400,
	0xa01e0400, 0x1b80001f, 0x20000104, 0x1950001f, 0x10006b00, 0x81439401,
	0xd8000f65, 0x17c07c1f, 0x81411801, 0xd80010e5, 0x17c07c1f, 0x18c0001f,
	0x10006240, 0xc0c01d60, 0x17c07c1f, 0x1950001f, 0x10006b00, 0x81441401,
	0xd80010e5, 0x17c07c1f, 0x1b00001f, 0x7ffce7ff, 0xf0000000, 0x17c07c1f,
	0x803d8400, 0x1b80001f, 0x2000001a, 0x80340400, 0x17c07c1f, 0x17c07c1f,
	0x80310400, 0x89c00007, 0xffeffffd, 0xa9c00007, 0x61010000, 0xe8208000,
	0x10006354, 0xfff01b01, 0x1b00001f, 0xbffce7ff, 0x1b00001f, 0xfffcffff,
	0x1b80001f, 0x20000004, 0x8a80000c, 0x2ffce7ff, 0xd820154a, 0x17c07c1f,
	0x1b00001f, 0x3ffce7ff, 0xf0000000, 0x17c07c1f, 0x1990001f, 0x10006b08,
	0x81461801, 0xd8001685, 0x10807c1f, 0x1890001f, 0x10212018, 0x808f8801,
	0x1a50001f, 0x10006608, 0x80c9a401, 0x810aa401, 0xa0918c02, 0xa0939002,
	0x80ca2401, 0x810ba401, 0xa09c0c02, 0xa0979002, 0xa0950402, 0x8080080d,
	0xd80013c2, 0x17c07c1f, 0x1b00001f, 0xfffcffff, 0x1b80001f, 0x20000004,
	0x8a80000c, 0x2ffce7ff, 0xd800150a, 0x17c07c1f, 0xe8208000, 0x10006354,
	0xfff01b47, 0x1b80001f, 0x20000020, 0x02000408, 0x89c00007, 0x9efeffff,
	0xa9c00007, 0x00100002, 0x1b80001f, 0x20000020, 0x1950001f, 0x10006b00,
	0x81449401, 0xd8001ac5, 0x17c07c1f, 0x1940001f, 0x001c0bae, 0xa0110400,
	0xa0140400, 0xa01d8400, 0x1b00001f, 0x7ffce7ff, 0x1950001f, 0x10006b00,
	0x81451401, 0xd8001c45, 0x17c07c1f, 0x1940001f, 0x001c0bae, 0xf0000000,
	0x17c07c1f, 0xe0f07f0d, 0xe0f07f0f, 0xe0f07f1e, 0xe0f07f12, 0xf0000000,
	0x17c07c1f, 0xa1d08407, 0x1b80001f, 0x20000080, 0x812ab401, 0x1900001f,
	0x10006814, 0xe1000003, 0xf0000000, 0x17c07c1f, 0xd0001fe0, 0x11407c1f,
	0x81f08407, 0x1b80001f, 0x20000001, 0xa1d08407, 0x1b80001f, 0x20000020,
	0x80eab401, 0xd8202103, 0x17c07c1f, 0x80c01403, 0xd8201f83, 0x01400405,
	0x1900001f, 0x10006814, 0xf0000000, 0xe1000003, 0xa1d10407, 0x1b80001f,
	0x20000020, 0xf0000000, 0x17c07c1f, 0x18c0001f, 0x10006b6c, 0x1910001f,
	0x10006b6c, 0xa1002804, 0xe0c00004, 0xf0000000, 0x17c07c1f, 0xd8202429,
	0x17c07c1f, 0xe2e0000d, 0xe2e0000c, 0xe2e0001c, 0xe2e0001e, 0xe2e00016,
	0xe2e00012, 0xf0000000, 0x17c07c1f, 0xd82025a9, 0x17c07c1f, 0xe2e00016,
	0x1380201f, 0xe2e0001e, 0x1380201f, 0xe2e0001c, 0x1380201f, 0xe2e0000c,
	0xe2e0000d, 0xf0000000, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
	0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001, 0xa1d48407, 0x1990001f,
	0x10006b08, 0xe8208000, 0x10006310, 0x0b160008, 0xe8208000, 0x10006b6c,
	0x00000000, 0x1b00001f, 0x2ffce7ff, 0x81469801, 0xd8004305, 0x17c07c1f,
	0x1b80001f, 0xd00f0000, 0x8880000c, 0x2ffce7ff, 0xd8005fa2, 0x17c07c1f,
	0xd0004340, 0x17c07c1f, 0x1b80001f, 0x500f0000, 0xe8208000, 0x10006354,
	0xfff01b01, 0x17c07c1f, 0x81401801, 0xd8004a25, 0x17c07c1f, 0x81f60407,
	0x1950001f, 0x10006b00, 0x81401401, 0xd8004445, 0x17c07c1f, 0x18c0001f,
	0x10006200, 0xc0c06100, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000001,
	0x1b80001f, 0x20000080, 0xc0c06100, 0x1280041f, 0x1950001f, 0x10006b00,
	0x81409401, 0xd8004645, 0x17c07c1f, 0x18c0001f, 0x10006208, 0xc0c06100,
	0x12807c1f, 0x1b80001f, 0x20000003, 0xe8208000, 0x10006248, 0x00000000,
	0x1b80001f, 0x20000080, 0xc0c06100, 0x1280041f, 0x1950001f, 0x10006b00,
	0x81411401, 0xd8004885, 0x17c07c1f, 0x18c0001f, 0x10006290, 0xe0e0004f,
	0xc0c06100, 0x1280041f, 0xe8208000, 0x10006404, 0x00003101, 0x81469801,
	0xd8204b85, 0x17c07c1f, 0x1b00001f, 0x2ffce7ff, 0x1b80001f, 0x30000004,
	0x8880000c, 0x2ffce7ff, 0xd8005b02, 0x17c07c1f, 0xe8208000, 0x10006294,
	0x0003ffff, 0x18c0001f, 0x10006294, 0xe0e03fff, 0xe0e003ff, 0x81449801,
	0xd8004d85, 0x17c07c1f, 0x1a00001f, 0x10006604, 0xc0c06c20, 0x17c07c1f,
	0xc0c06580, 0x12807c1f, 0x1950001f, 0x10006b00, 0x81419401, 0xd8004d85,
	0x17c07c1f, 0x81401801, 0xd8004f25, 0x17c07c1f, 0xa1d38407, 0xa0108400,
	0xa0148400, 0xa01b8400, 0xa0188400, 0x12007c1f, 0xe8208000, 0x10006310,
	0x0b160008, 0x81431801, 0xd8005125, 0x17c07c1f, 0xe8208000, 0x10006310,
	0x0b1600c8, 0x81439801, 0xd8205125, 0x17c07c1f, 0xe8208000, 0x10006310,
	0x0b160038, 0x1b00001f, 0xbffce7ff, 0x1950001f, 0x10006b00, 0x81421401,
	0xd8005165, 0x17c07c1f, 0x1940001f, 0x001c0bae, 0x1950001f, 0x10006b00,
	0x814f9401, 0xd8205325, 0x17c07c1f, 0x1b80001f, 0x00001000, 0x1b80001f,
	0x90100000, 0x1240301f, 0x1ac0001f, 0x10006b6c, 0xe2c00008, 0xe8208000,
	0x10006310, 0x0b160008, 0x82809c01, 0x1990001f, 0x10006b08, 0x81439801,
	0xd82055a5, 0x17c07c1f, 0x828d8001, 0xca80120a, 0x17c07c1f, 0xd00055e0,
	0x17c07c1f, 0xca80000a, 0x17c07c1f, 0x1990001f, 0x10006b08, 0x1b00001f,
	0x3ffce7ff, 0x18c0001f, 0x10006294, 0xe0e007fe, 0xe0e00ffc, 0xe0e01ff8,
	0xe0e03ff0, 0xe0e03fe0, 0xe0e03fc0, 0x1b80001f, 0x20000020, 0xe8208000,
	0x10006294, 0x0003ffc0, 0xe8208000, 0x10006294, 0x0003fc00, 0x81401801,
	0xd8005b05, 0x17c07c1f, 0x81449801, 0xd80059a5, 0x17c07c1f, 0xc0c06580,
	0x1280041f, 0x1b80001f, 0x200016a8, 0x80388400, 0x1b80001f, 0x20000300,
	0x803b8400, 0x1b80001f, 0x20000300, 0x80348400, 0x1b80001f, 0x20000104,
	0x80308400, 0x81f38407, 0x81401801, 0xd8005fa5, 0x17c07c1f, 0xe8208000,
	0x10006404, 0x00000101, 0x18c0001f, 0x10006290, 0x1212841f, 0xc0c06280,
	0x12807c1f, 0xc0c06280, 0x1280041f, 0x18c0001f, 0x10006208, 0x1212841f,
	0xc0c06280, 0x12807c1f, 0xe8208000, 0x10006248, 0x00000001, 0x1b80001f,
	0x20000080, 0xc0c06280, 0x1280041f, 0x18c0001f, 0x10006200, 0x1212841f,
	0xc0c06280, 0x12807c1f, 0xe8208000, 0x1000625c, 0x00000000, 0x1b80001f,
	0x20000080, 0xc0c06280, 0x1280041f, 0x19c0001f, 0x61415820, 0x10007c1f,
	0xe8208000, 0x10006b30, 0x00000000, 0xe8208000, 0x100063e0, 0x00000001,
	0xf0000000, 0x17c07c1f, 0xd80061aa, 0x17c07c1f, 0xe2e0004f, 0xe2e0006f,
	0xe2e0002f, 0xd820624a, 0x17c07c1f, 0xe2e0002e, 0xe2e0003e, 0xe2e00032,
	0xf0000000, 0x17c07c1f, 0xd800638a, 0x17c07c1f, 0xe2e00036, 0x17c07c1f,
	0x17c07c1f, 0xe2e0003e, 0x1380201f, 0xe2e0003c, 0xd82064ca, 0x17c07c1f,
	0x1b80001f, 0x20000018, 0xe2e0007c, 0x1b80001f, 0x20000003, 0xe2e0005c,
	0xe2e0004c, 0xe2e0004d, 0xf0000000, 0x17c07c1f, 0x1b80001f, 0x20000300,
	0xf0000000, 0x17c07c1f, 0x1a10001f, 0x10059824, 0xd8006588, 0x17c07c1f,
	0x1a00001f, 0x10059838, 0xe2200001, 0xe8208000, 0x10059814, 0x00000002,
	0xe8208000, 0x10059820, 0x00000011, 0xe8208000, 0x10059848, 0x00000000,
	0xe8208000, 0x10059810, 0x00000028, 0xe8208000, 0x10059804, 0x000000c0,
	0x1a00001f, 0x10059800, 0xd8206b0a, 0x17c07c1f, 0xe2200001, 0x1950001f,
	0x10006b04, 0xe2000005, 0xe8208000, 0x10059824, 0x00000001, 0x814c1801,
	0xd8006a85, 0x17c07c1f, 0x1b80001f, 0x20000524, 0xd0006ac0, 0x17c07c1f,
	0x1b80001f, 0x200066cb, 0xd0006be0, 0x17c07c1f, 0xe2200001, 0xe2200094,
	0xe8208000, 0x10059824, 0x00000001, 0x1b80001f, 0x20000524, 0xf0000000,
	0x17c07c1f, 0xe8208000, 0x10059838, 0x00000001, 0xe8208000, 0x1005980c,
	0x0000001f, 0xe8208000, 0x10059820, 0x00000011, 0xe8208000, 0x10059848,
	0x00000000, 0xe8208000, 0x10059810, 0x0000003a, 0xe8208000, 0x10059804,
	0x000000c0, 0xe8208000, 0x10059814, 0x00000001, 0xe8208000, 0x1005986c,
	0x00000001, 0xe8208000, 0x10059818, 0x00000002, 0x1a00001f, 0x10059800,
	0xe2000001, 0xe8208000, 0x10059824, 0x00000001, 0x8a100001, 0x1005980c,
	0xd8207048, 0x17c07c1f, 0x1950001f, 0x10059800, 0x1a00001f, 0x10006b04,
	0xe2000005, 0xe8208000, 0x10059810, 0x00000000, 0xe8208000, 0x10059818,
	0x00000001, 0xf0000000, 0x17c07c1f
};

static struct pcm_desc sodi_pcm = {
	.version = "pcm_sodi_v00.04_mt8163_20171227",
	.base = sodi_binary,
	.size = 915,
	.sess = 2,
	.replace = 0,
	.vec2 = EVENT_VEC(30, 1, 0, 0),	/* FUNC_SUSPEND_APSRC_WAKEUP */
	.vec3 = EVENT_VEC(31, 1, 0, 58),	/* FUNC_SUSPEND_APSRC_SLEEP */
	.vec0 = EVENT_VEC(30, 1, 0, 144),	/* FUNC_APSRC_WAKEUP */
	.vec1 = EVENT_VEC(31, 1, 0, 172),	/* FUNC_APSRC_SLEEP */
};

static struct pwr_ctrl sodi_ctrl = {
	.wake_src = WAKE_SRC_FOR_SODI,
	.wake_src_md32 = WAKE_SRC_FOR_MD32,
	.r0_ctrl_en = 1,
	.r7_ctrl_en = 1,
	.wfi_op = WFI_OP_AND,
#if (!SODI_DVT_APxGPT)
	.ca15_wfi0_en = 1,
	.ca15_wfi1_en = 1,
	.ca15_wfi2_en = 1,
	.ca15_wfi3_en = 1,
	.ca7_wfi0_en = 1,
	.ca7_wfi1_en = 1,
	.ca7_wfi2_en = 1,
	.ca7_wfi3_en = 1,
	.mfg_req_mask = 1,
#if SPM_BYPASS_SYSPWREQ
	.syspwreq_mask = 1,
#endif
#else
	.ca15_wfi0_en = 0,
	.ca15_wfi1_en = 0,
	.ca15_wfi2_en = 0,
	.ca15_wfi3_en = 0,
	.ca7_wfi0_en = 1,
	.ca7_wfi1_en = 0,
	.ca7_wfi2_en = 0,
	.ca7_wfi3_en = 0,
	.conn_mask = 1,
	.disp_req_mask = 1,
	.mfg_req_mask = 1,
	.md32_req_mask = 1,
	.mcusys_idle_mask = 1,
	.ca7top_idle_mask = 1,
	.ca15top_idle_mask = 1,
	.syspwreq_mask = 1,
#if SPM_BYPASS_SYSPWREQ
	.syspwreq_mask = 1,
#endif

#endif
};

struct spm_lp_scen __spm_sodi = {
	.pcmdesc = &sodi_pcm,
	.pwrctrl = &sodi_ctrl,
};

static bool gSpm_SODI_mempll_pwr_mode = 1;
static bool gSpm_sodi_en;
static bool gSpm_SODI_cpu_dvs_en = 1;
struct clk *c;

/*
extern int mt_irq_mask_all(struct mtk_irq_mask *mask);
extern int mt_irq_mask_restore(struct mtk_irq_mask *mask);
extern void mt_irq_unmask_for_sleep(unsigned int irq);
*/

#ifdef SPM_SODI_DEBUG
static void spm_sodi_dump_regs(void)
{
	/* SPM register */
	spm_idle_ver("SPM_MP0_CPU0_IRQ_MASK   0x%x = 0x%x\n", SPM_CA7_CPU0_IRQ_MASK,
		     spm_read(SPM_CA7_CPU0_IRQ_MASK));
	spm_idle_ver("SPM_MP0_CPU1_IRQ_MASK   0x%x = 0x%x\n", SPM_CA7_CPU1_IRQ_MASK,
		     spm_read(SPM_CA7_CPU1_IRQ_MASK));
	spm_idle_ver("SPM_MP0_CPU2_IRQ_MASK   0x%x = 0x%x\n", SPM_CA7_CPU2_IRQ_MASK,
		     spm_read(SPM_CA7_CPU2_IRQ_MASK));
	spm_idle_ver("SPM_MP0_CPU3_IRQ_MASK   0x%x = 0x%x\n", SPM_CA7_CPU3_IRQ_MASK,
		     spm_read(SPM_CA7_CPU3_IRQ_MASK));
	spm_idle_ver("SPM_MP1_CPU0_IRQ_MASK   0x%x = 0x%x\n", SPM_MP1_CPU0_IRQ_MASK,
		     spm_read(SPM_MP1_CPU0_IRQ_MASK));
	spm_idle_ver("SPM_MP1_CPU1_IRQ_MASK   0x%x = 0x%x\n", SPM_MP1_CPU1_IRQ_MASK,
		     spm_read(SPM_MP1_CPU1_IRQ_MASK));
	spm_idle_ver("SPM_MP1_CPU2_IRQ_MASK   0x%x = 0x%x\n", SPM_MP1_CPU2_IRQ_MASK,
		     spm_read(SPM_MP1_CPU2_IRQ_MASK));
	spm_idle_ver("SPM_MP1_CPU3_IRQ_MASK   0x%x = 0x%x\n", SPM_MP1_CPU3_IRQ_MASK,
		     spm_read(SPM_MP1_CPU3_IRQ_MASK));
#if 0
	spm_idle_ver("POWER_ON_VAL0   0x%x = 0x%x\n", SPM_POWER_ON_VAL0,
		     spm_read(SPM_POWER_ON_VAL0));
	spm_idle_ver("POWER_ON_VAL1   0x%x = 0x%x\n", SPM_POWER_ON_VAL1,
		     spm_read(SPM_POWER_ON_VAL1));
	spm_idle_ver("PCM_PWR_IO_EN   0x%x = 0x%x\n", SPM_PCM_PWR_IO_EN,
		     spm_read(SPM_PCM_PWR_IO_EN));
	spm_idle_ver("CLK_CON         0x%x = 0x%x\n", SPM_CLK_CON, spm_read(SPM_CLK_CON));
	spm_idle_ver("AP_DVFS_CON     0x%x = 0x%x\n", SPM_AP_DVFS_CON_SET,
		     spm_read(SPM_AP_DVFS_CON_SET));
	spm_idle_ver("PWR_STATUS      0x%x = 0x%x\n", SPM_PWR_STATUS, spm_read(SPM_PWR_STATUS));
	spm_idle_ver("PWR_STATUS_S    0x%x = 0x%x\n", SPM_PWR_STATUS_S, spm_read(SPM_PWR_STATUS_S));
	spm_idle_ver("SLEEP_TIMER_STA 0x%x = 0x%x\n", SPM_SLEEP_TIMER_STA,
		     spm_read(SPM_SLEEP_TIMER_STA));
	spm_idle_ver("WAKE_EVENT_MASK 0x%x = 0x%x\n", SPM_SLEEP_WAKEUP_EVENT_MASK,
		     spm_read(SPM_SLEEP_WAKEUP_EVENT_MASK));
	spm_idle_ver("SPM_SLEEP_CPU_WAKEUP_EVENT 0x%x = 0x%x\n", SPM_SLEEP_CPU_WAKEUP_EVENT,
		     spm_read(SPM_SLEEP_CPU_WAKEUP_EVENT));
	spm_idle_ver("SPM_PCM_RESERVE   0x%x = 0x%x\n", SPM_PCM_RESERVE, spm_read(SPM_PCM_RESERVE));
	spm_idle_ver("SPM_AP_STANBY_CON   0x%x = 0x%x\n", SPM_AP_STANBY_CON,
		     spm_read(SPM_AP_STANBY_CON));
	spm_idle_ver("SPM_PCM_TIMER_OUT   0x%x = 0x%x\n", SPM_PCM_TIMER_OUT,
		     spm_read(SPM_PCM_TIMER_OUT));
	spm_idle_ver("SPM_PCM_CON1   0x%x = 0x%x\n", SPM_PCM_CON1, spm_read(SPM_PCM_CON1));
#endif

	/* PCM register */
	spm_idle_ver("PCM_REG0_DATA   0x%x = 0x%x\n", SPM_PCM_REG0_DATA,
		     spm_read(SPM_PCM_REG0_DATA));
	spm_idle_ver("PCM_REG1_DATA   0x%x = 0x%x\n", SPM_PCM_REG1_DATA,
		     spm_read(SPM_PCM_REG1_DATA));
	spm_idle_ver("PCM_REG2_DATA   0x%x = 0x%x\n", SPM_PCM_REG2_DATA,
		     spm_read(SPM_PCM_REG2_DATA));
	spm_idle_ver("PCM_REG3_DATA   0x%x = 0x%x\n", SPM_PCM_REG3_DATA,
		     spm_read(SPM_PCM_REG3_DATA));
	spm_idle_ver("PCM_REG4_DATA   0x%x = 0x%x\n", SPM_PCM_REG4_DATA,
		     spm_read(SPM_PCM_REG4_DATA));
	spm_idle_ver("PCM_REG5_DATA   0x%x = 0x%x\n", SPM_PCM_REG5_DATA,
		     spm_read(SPM_PCM_REG5_DATA));
	spm_idle_ver("PCM_REG6_DATA   0x%x = 0x%x\n", SPM_PCM_REG6_DATA,
		     spm_read(SPM_PCM_REG6_DATA));
	spm_idle_ver("PCM_REG7_DATA   0x%x = 0x%x\n", SPM_PCM_REG7_DATA,
		     spm_read(SPM_PCM_REG7_DATA));
	spm_idle_ver("PCM_REG8_DATA   0x%x = 0x%x\n", SPM_PCM_REG8_DATA,
		     spm_read(SPM_PCM_REG8_DATA));
	spm_idle_ver("PCM_REG9_DATA   0x%x = 0x%x\n", SPM_PCM_REG9_DATA,
		     spm_read(SPM_PCM_REG9_DATA));
	spm_idle_ver("PCM_REG10_DATA   0x%x = 0x%x\n", SPM_PCM_REG10_DATA,
		     spm_read(SPM_PCM_REG10_DATA));
	spm_idle_ver("PCM_REG11_DATA   0x%x = 0x%x\n", SPM_PCM_REG11_DATA,
		     spm_read(SPM_PCM_REG11_DATA));
	spm_idle_ver("PCM_REG12_DATA   0x%x = 0x%x\n", SPM_PCM_REG12_DATA,
		     spm_read(SPM_PCM_REG12_DATA));
	spm_idle_ver("PCM_REG13_DATA   0x%x = 0x%x\n", SPM_PCM_REG13_DATA,
		     spm_read(SPM_PCM_REG13_DATA));
	spm_idle_ver("PCM_REG14_DATA   0x%x = 0x%x\n", SPM_PCM_REG14_DATA,
		     spm_read(SPM_PCM_REG14_DATA));
	spm_idle_ver("PCM_REG15_DATA   0x%x = 0x%x\n", SPM_PCM_REG15_DATA,
		     spm_read(SPM_PCM_REG15_DATA));

	spm_idle_ver("SPM_MP0_FC0_PWR_CON   0x%x = 0x%x\n", SPM_MP0_FC0_PWR_CON,
		     spm_read(SPM_MP0_FC0_PWR_CON));
	spm_idle_ver("SPM_MP0_FC1_PWR_CON   0x%x = 0x%x\n", SPM_MP0_FC1_PWR_CON,
		     spm_read(SPM_MP0_FC1_PWR_CON));
	spm_idle_ver("SPM_MP0_FC2_PWR_CON   0x%x = 0x%x\n", SPM_MP0_FC2_PWR_CON,
		     spm_read(SPM_MP0_FC2_PWR_CON));
	spm_idle_ver("SPM_MP0_FC3_PWR_CON   0x%x = 0x%x\n", SPM_MP0_FC3_PWR_CON,
		     spm_read(SPM_MP0_FC3_PWR_CON));
	spm_idle_ver("SPM_MP1_FC0_PWR_CON   0x%x = 0x%x\n", SPM_MP1_FC0_PWR_CON,
		     spm_read(SPM_MP1_FC0_PWR_CON));
	spm_idle_ver("SPM_MP1_FC1_PWR_CON   0x%x = 0x%x\n", SPM_MP1_FC1_PWR_CON,
		     spm_read(SPM_MP1_FC1_PWR_CON));
	spm_idle_ver("SPM_MP1_FC2_PWR_CON   0x%x = 0x%x\n", SPM_MP1_FC2_PWR_CON,
		     spm_read(SPM_MP1_FC2_PWR_CON));
	spm_idle_ver("SPM_MP1_FC3_PWR_CON   0x%x = 0x%x\n", SPM_MP1_FC3_PWR_CON,
		     spm_read(SPM_MP1_FC3_PWR_CON));

	spm_idle_ver("CLK_CON         0x%x = 0x%x\n", SPM_CLK_CON, spm_read(SPM_CLK_CON));
	spm_idle_ver("SPM_PCM_CON0   0x%x = 0x%x\n", SPM_PCM_CON0, spm_read(SPM_PCM_CON0));
	spm_idle_ver("SPM_PCM_CON1   0x%x = 0x%x\n", SPM_PCM_CON1, spm_read(SPM_PCM_CON1));

	spm_idle_ver("SPM_PCM_MP_CORE0_AUX   0x%x = 0x%x\n", SPM_PCM_EVENT_VECTOR2,
		     spm_read(SPM_PCM_EVENT_VECTOR2));
	spm_idle_ver("SPM_PCM_MP_CORE1_AUX   0x%x = 0x%x\n", SPM_PCM_EVENT_VECTOR3,
		     spm_read(SPM_PCM_EVENT_VECTOR3));
	spm_idle_ver("SPM_PCM_MP_CORE2_AUX   0x%x = 0x%x\n", SPM_PCM_EVENT_VECTOR4,
		     spm_read(SPM_PCM_EVENT_VECTOR4));
	spm_idle_ver("SPM_PCM_MP_CORE3_AUX   0x%x = 0x%x\n", SPM_PCM_EVENT_VECTOR5,
		     spm_read(SPM_PCM_EVENT_VECTOR5));
	spm_idle_ver("SPM_PCM_MP_CORE4_AUX   0x%x = 0x%x\n", SPM_PCM_EVENT_VECTOR6,
		     spm_read(SPM_PCM_EVENT_VECTOR6));
	spm_idle_ver("SPM_PCM_MP_CORE5_AUX   0x%x = 0x%x\n", SPM_PCM_EVENT_VECTOR7,
		     spm_read(SPM_PCM_EVENT_VECTOR7));
	spm_idle_ver("SPM_PCM_MP_CORE6_AUX   0x%x = 0x%x\n", SPM_PCM_RESERVE,
		     spm_read(SPM_PCM_RESERVE));
	spm_idle_ver("SPM_PCM_MP_CORE7_AUX   0x%x = 0x%x\n", SPM_PCM_WDT_TIMER_VAL,
		     spm_read(SPM_PCM_WDT_TIMER_VAL));

	spm_idle_ver("SPM_MP0_CORE0_WFI_SEL   0x%x = 0x%x\n", SPM_SLEEP_CA7_WFI0_EN,
		     spm_read(SPM_SLEEP_CA7_WFI0_EN));
	spm_idle_ver("SPM_MP0_CORE1_WFI_SEL   0x%x = 0x%x\n", SPM_SLEEP_CA7_WFI1_EN,
		     spm_read(SPM_SLEEP_CA7_WFI1_EN));
	spm_idle_ver("SPM_MP0_CORE2_WFI_SEL   0x%x = 0x%x\n", SPM_SLEEP_CA7_WFI2_EN,
		     spm_read(SPM_SLEEP_CA7_WFI2_EN));
	spm_idle_ver("SPM_MP0_CORE3_WFI_SEL   0x%x = 0x%x\n", SPM_SLEEP_CA7_WFI3_EN,
		     spm_read(SPM_SLEEP_CA7_WFI3_EN));
	spm_idle_ver("SPM_MP1_CORE0_WFI_SEL   0x%x = 0x%x\n", SPM_SLEEP_CA15_WFI0_EN,
		     spm_read(SPM_SLEEP_CA15_WFI0_EN));
	spm_idle_ver("SPM_MP1_CORE1_WFI_SEL   0x%x = 0x%x\n", SPM_SLEEP_CA15_WFI1_EN,
		     spm_read(SPM_SLEEP_CA15_WFI1_EN));
	spm_idle_ver("SPM_MP1_CORE2_WFI_SEL   0x%x = 0x%x\n", SPM_SLEEP_CA15_WFI2_EN,
		     spm_read(SPM_SLEEP_CA15_WFI2_EN));
	spm_idle_ver("SPM_MP1_CORE3_WFI_SEL   0x%x = 0x%x\n", SPM_SLEEP_CA15_WFI3_EN,
		     spm_read(SPM_SLEEP_CA15_WFI3_EN));

	spm_idle_ver("SPM_SLEEP_TIMER_STA   0x%x = 0x%x\n", SPM_SLEEP_TIMER_STA,
		     spm_read(SPM_SLEEP_TIMER_STA));
	spm_idle_ver("SPM_PWR_STATUS   0x%x = 0x%x\n", SPM_PWR_STATUS, spm_read(SPM_PWR_STATUS));
	spm_idle_ver("SPM_PWR_STATUS_S   0x%x = 0x%x\n", SPM_PWR_STATUS_S,
		     spm_read(SPM_PWR_STATUS_S));

	spm_idle_ver("SPM_MP0_FC0_PWR_CON   0x%x = 0x%x\n", SPM_MP0_FC0_PWR_CON,
		     spm_read(SPM_MP0_FC0_PWR_CON));
	spm_idle_ver("SPM_MP0_DBG_PWR_CON   0x%x = 0x%x\n", SPM_MP0_DBG_PWR_CON,
		     spm_read(SPM_MP0_DBG_PWR_CON));
	spm_idle_ver("SPM_MP0_CPU_PWR_CON   0x%x = 0x%x\n", SPM_MP0_CPU_PWR_CON,
		     spm_read(SPM_MP0_CPU_PWR_CON));

}
#endif


static void spm_trigger_wfi_for_sodi(struct pwr_ctrl *pwrctrl)
{
	/* TODO:[fixme: early porting comment out] */
	/* sync_hw_gating_value();     // for Vcore DVFS */

	if (is_cpu_pdn(pwrctrl->pcm_flags)) {
		mt_cpu_dormant(CPU_SODI_MODE);
	} else {
		/*MP0 and with wfi */
		spm_write(MP0_AXI_CONFIG, spm_read(MP0_AXI_CONFIG) | ACINACTM);
		wfi_with_sync();
		spm_write(MP0_AXI_CONFIG, spm_read(MP0_AXI_CONFIG) & ~ACINACTM);
	}
}

#define CHA_DDRPHY_BASE  0xf000F000
#if SODI_DVT_SPM_MEM_RW_TEST
static u32 magic_init;
#endif

void spm_go_to_sodi(u32 spm_flags, u32 spm_data)
{
	struct wake_status wakesta;
	unsigned long flags;
	struct mtk_irq_mask mask;

	wake_reason_t wr = WR_NONE;
	struct pcm_desc *pcmdesc = __spm_sodi.pcmdesc;
	struct pwr_ctrl *pwrctrl = __spm_sodi.pwrctrl;

#if SODI_DVT_SPM_MEM_RW_TEST
	if (magic_init == 0) {
		magic_init++;
		pr_debug("magicNumArray:0x%x", magicArray);
	}
#endif

	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

	/* set PMIC WRAP table for deepidle power control */
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_DEEPIDLE);
	soidle_before_wfi(0);

	spin_lock_irqsave(&__spm_lock, flags);
	mt_irq_mask_all(&mask);
	mt_irq_unmask_for_sleep(SPM_IRQ0_ID);
	mt_cirq_clone_gic();
	mt_cirq_enable();

	__spm_reset_and_init_pcm(pcmdesc);

#if 0				/* (!SODI_DVT_APxGPT)  //workaroud for ca17 disp mempll power down switch crash */
	if (gSpm_SODI_mempll_pwr_mode == 1) {	/* disp req: reset or 1pll request */
		/* pr_debug("[SODI] MEMPLL RESET\n"); */
		/* MEMPLL 1PLL Mode */
		pwrctrl->pcm_flags |= SPM_MEMPLL_RESET;
#if (!SODI_MEMPLL_RESETMODE_FW)	/* spm fw : 1pll + pdn fw */
		/* pr_debug("[SODI] 1PLL\n"); */
		pwrctrl->md2_req_mask = 1;
		pwrctrl->md1_req_mask = 1;
		pwrctrl->conn_mask = 1;
		pwrctrl->md32_req_mask = 1;
#endif


	} else {			/* pdn */
		/* pr_debug("[SODI] MEMPLL PDN Mode\n"); */
		/* power down mode */
#if (!SODI_MEMPLL_RESETMODE_FW)
		pwrctrl->md2_req_mask = 0;
		pwrctrl->md1_req_mask = 0;
		pwrctrl->conn_mask = 0;
		pwrctrl->md32_req_mask = 0;
#endif
		pwrctrl->pcm_flags &= ~SPM_MEMPLL_RESET;
	}
#else				/* stress test for rest mode + pdn */

	pwrctrl->pcm_flags |= SPM_LOW_SPD_I2C;

#if 1
	/* pr_debug("[SODI] MEMPLL RESET!\n"); */
	/* MEMPLL RESET Mode */
	pwrctrl->pcm_flags |= SPM_MEMPLL_RESET;
#endif

#if 0
	/* pr_debug("[SODI] MEMPLL 1PLL\n"); */
	/* MEMPLL 1PLL Mode */
	pwrctrl->pcm_flags |= SPM_MEMPLL_RESET;
	pwrctrl->md2_req_mask = 1;
	pwrctrl->md1_req_mask = 1;
	pwrctrl->conn_mask = 1;
	pwrctrl->md32_req_mask = 1;
#endif

#if 0
	/* pr_debug("[SODI] MEMPLL PDN Mode!\n"); */
	/* power down mode */
	if (gSpm_SODI_mempll_pwr_mode == 1)
		pwrctrl->pcm_flags |= SPM_SODI_DIS;
	else
		pwrctrl->pcm_flags &= ~SPM_SODI_DIS;

	pwrctrl->pcm_flags &= ~SPM_MEMPLL_RESET;
#endif
#endif
	/* check GCE */
	if (__clk_is_enabled(c))
		pwrctrl->pcm_flags &= ~SPM_DDR_HIGH_SPEED;
	else
		pwrctrl->pcm_flags |= SPM_DDR_HIGH_SPEED;

	/* arm atf dormant abort */
#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
	pwrctrl->pcm_flags |= SPM_SCREEN_OFF;
#endif

	if (gSpm_SODI_cpu_dvs_en)
		pwrctrl->pcm_flags &= ~SPM_CPU_DVS_DIS;
	else
		pwrctrl->pcm_flags |= SPM_CPU_DVS_DIS;

	__spm_kick_im_to_fetch(pcmdesc);

	__spm_init_pcm_register();

	__spm_init_event_vector(pcmdesc);

	/* keep bit 1's value for video/cmd mode lcm */
	if ((spm_read(SPM_PCM_SRC_REQ) & 0x00000001))
		pwrctrl->pcm_apsrc_req = 1;
	else
		pwrctrl->pcm_apsrc_req = 0;

	__spm_set_power_control(pwrctrl);

	__spm_set_wakeup_event(pwrctrl);

#if (SODI_DVT_BLOCK_BF_WFI) && (SODI_DVT_APxGPT)
	spm_write(SPM_PCM_CON1, spm_read(SPM_PCM_CON1) & (~CON1_PCM_TIMER_EN));
	pwrctrl->pcm_reserve = 0x000003ff;
#endif
	__spm_kick_pcm_to_run(pwrctrl);

	/* test begin */
#if (SODI_DVT_SPM_DBG_MODE) && (SODI_DVT_SPM_DBG_MODE_1PLL) && (SODI_DVT_APxGPT)
	spm_write(SPM_PCM_RESERVE, 0x400);	/* keep in 1pll */
#endif
	/* end */

#ifdef SPM_SODI_DEBUG
	spm_idle_ver("============SODI Before============\n");
	spm_sodi_dump_regs();	/* dump debug info */
#endif

	spm_trigger_wfi_for_sodi(pwrctrl);

#ifdef SPM_SODI_DEBUG
	spm_idle_ver("============SODI After=============\n");
	spm_sodi_dump_regs();	/* dump debug info */
#endif

	__spm_get_wakeup_status(&wakesta);

	spm_idle_ver("SODI:dram-selfrefrsh cnt %d", spm_read(SPM_PCM_PASR_DPD_3));

	__spm_clean_after_wakeup();

	wr = __spm_output_wake_reason(&wakesta, pcmdesc, false);

	mt_cirq_flush();
	mt_cirq_disable();
	mt_irq_mask_restore(&mask);
	spin_unlock_irqrestore(&__spm_lock, flags);

	soidle_after_wfi(0);
	/* set PMIC WRAP table for normal power control */
	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);

#if SODI_DVT_SPM_MEM_RW_TEST
	{
		int i = 0;

		for (i = 0; i < 16; i++) {
			if (magicArray[i] != SODI_DVT_MAGIC_NUM) {
				pr_debug("Error: sodi magic number no match!!!");
				ASSERT(0);
			}
		}
	}
#endif
	/* return wr; */

}

void spm_sodi_mempll_pwr_mode(bool pwr_mode)
{
	/* pr_debug("[SODI]set pwr_mode = %d\n",pwr_mode); */
	gSpm_SODI_mempll_pwr_mode = pwr_mode;
}

void spm_enable_sodi(bool en)
{
	gSpm_sodi_en = en;
}

bool spm_get_sodi_en(void)
{
	return gSpm_sodi_en;
}

/* Temporarily workaround api for ext buck */
void spm_sodi_cpu_dvs_en(bool en)
{
	gSpm_SODI_cpu_dvs_en = en;
}

static void spm_set_sodi_pcm_ver(void)
{
}

void spm_sodi_init(void)
{
	c = __clk_lookup("infra_gce");
	spm_set_sodi_pcm_ver();
}


#if 0
void spm_sodi_lcm_video_mode(bool IsLcmVideoMode)
{
	gSpm_IsLcmVideoMode = IsLcmVideoMode;

	spm_idle_ver("spm_sodi_lcm_video_mode() : gSpm_IsLcmVideoMode = %x\n", gSpm_IsLcmVideoMode);

}
#endif
MODULE_DESCRIPTION("SPM-SODI Driver v0.1");
