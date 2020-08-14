/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/i2c.h>
#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <asm/setup.h>

#include <mt-plat/aee.h>
#include <mt-plat/mtk_cirq.h>

#include <mach/wd_api.h>
#ifdef CONFIG_MD32_SUPPORT
#include <mach/md32_helper.h>
#endif
#include "mt_spm_internal.h"
#include "mt_spm_sleep.h"
#include "mt_cpuidle.h"

/**************************************
 * only for internal debug
 **************************************/
#ifdef CONFIG_MTK_LDVT
#define SPM_PWAKE_EN            0
#define SPM_PCMWDT_EN           0
#define SPM_BYPASS_SYSPWREQ     1
#else
#define SPM_PWAKE_EN            1
#define SPM_PCMWDT_EN           1
#define SPM_BYPASS_SYSPWREQ     0
#endif

#ifdef CONFIG_OF
#define MCUCFG_BASE          spm_mcucfg
#else
#define MCUCFG_BASE          (0xF0200000)	/* 0x1020_0000 */
#endif
#define MP0_AXI_CONFIG          (MCUCFG_BASE + 0x2C)
#define MP1_AXI_CONFIG          (MCUCFG_BASE + 0x22C)
#define ACINACTM                (1<<4)

#define I2C_CHANNEL 1

int spm_dormant_sta = MT_CPU_DORMANT_RESET;
int spm_ap_mdsrc_req_cnt;
u32 spm_suspend_flag;

struct wake_status suspend_info[20];
u32 log_wakesta_cnt;
u32 log_wakesta_index;
u8 spm_snapshot_golden_setting;

/**********************************************************
 * PCM code for suspend
 **********************************************************/
static const u32 suspend_binary[] = {
	0xa1d30407, 0xa1d58407, 0x81f68407, 0x1800001f, 0xf7cf7f3f, 0x1b80001f,
	0x20000000, 0x80300400, 0x80328400, 0xa1d28407, 0x81f20407, 0x81f30407,
	0x80318400, 0x81409801, 0xd8000305, 0x17c07c1f, 0x18c0001f, 0x10006234,
	0xc0c023a0, 0x1200041f, 0x80310400, 0x1b80001f, 0x2000000e, 0xa0110400,
	0xe8208000, 0x10006354, 0xffff1fff, 0x18c0001f, 0x10059034, 0x1910001f,
	0x10059034, 0x81340404, 0xe0c00004, 0x1910001f, 0x10059034, 0x81f00407,
	0xa1dd0407, 0x81fd0407, 0xc2802a40, 0x1290041f, 0x1b80001f, 0x20000100,
	0x1b00001f, 0x7ffcf7ff, 0x18c0001f, 0x10007024, 0x1910001f, 0x10007024,
	0xa11e8404, 0xe0c00004, 0xf0000000, 0x17c07c1f, 0x1b00001f, 0x3ffce7ff,
	0x1b80001f, 0x20000004, 0xd820078c, 0x17c07c1f, 0xd0000e00, 0x17c07c1f,
	0x81459801, 0xd8200925, 0x17c07c1f, 0x1880001f, 0x10006320, 0xc0c02920,
	0xe080000f, 0xd8200923, 0x17c07c1f, 0x1b00001f, 0x7ffcf7ff, 0xd0000e00,
	0x17c07c1f, 0xe080001f, 0x18c0001f, 0x10059034, 0x1910001f, 0x10059034,
	0xa1140404, 0xe0c00004, 0x1910001f, 0x10059034, 0xe8208000, 0x10006354,
	0xffffffff, 0x81409801, 0xd8000b85, 0x17c07c1f, 0x18c0001f, 0x10006234,
	0xc0c02580, 0x17c07c1f, 0xc2802a40, 0x1290841f, 0xa0118400, 0xa0160400,
	0xa0168400, 0xa0170400, 0x1b80001f, 0x20000104, 0xa1d30407, 0xa1d20407,
	0x81f28407, 0xa1d68407, 0x1800001f, 0xf7cf7f3f, 0x1800001f, 0xf7ff7f3f,
	0x81f58407, 0x81f30407, 0x1b00001f, 0x3ffcefff, 0xf0000000, 0x17c07c1f,
	0x81411801, 0xd8000f65, 0x17c07c1f, 0x18c0001f, 0x10006240, 0xe0e00016,
	0xe0e0001e, 0xe0e0000e, 0xe0e0000f, 0x803e0400, 0x1b80001f, 0x20000050,
	0x80380400, 0x803b0400, 0x1b80001f, 0x20000300, 0x803d0400, 0x1b80001f,
	0x20000300, 0x80340400, 0x80310400, 0xe8208000, 0x10000044, 0x00000100,
	0xe8208000, 0x10000004, 0x00000002, 0x1b80001f, 0x20000068, 0x1b80001f,
	0x2000000a, 0x18c0001f, 0x10006240, 0xe0e0000d, 0x81411801, 0xd8001425,
	0x17c07c1f, 0x18c0001f, 0x100040f4, 0x1910001f, 0x100040f4, 0xa11c8404,
	0xe0c00004, 0x1b80001f, 0x2000000a, 0x813c8404, 0xe0c00004, 0x1b80001f,
	0x20000100, 0x81fa0407, 0x1b80001f, 0x20000100, 0x81f08407, 0xe8208000,
	0x10006354, 0xfff01b47, 0xa1d80407, 0xa1dc0407, 0xa1de8407, 0xa1df0407,
	0xc2802a40, 0x1291041f, 0x1b00001f, 0xbffce7ff, 0x18c0001f, 0x10007024,
	0x1910001f, 0x10007024, 0xa11f0404, 0xe0c00004, 0xf0000000, 0x17c07c1f,
	0x18c0001f, 0x10007024, 0x1910001f, 0x10007024, 0x89000004, 0x1fffffff,
	0xe0c00004, 0x1b80001f, 0x20000fdf, 0x1a50001f, 0x10006608, 0x80c9a401,
	0x810aa401, 0x10918c1f, 0xa0939002, 0x80ca2401, 0x810ba401, 0xa09c0c02,
	0xa0979002, 0x8080080d, 0xd8201b42, 0x17c07c1f, 0x1b00001f, 0x3ffce7ff,
	0x1b80001f, 0x20000004, 0xd800202c, 0x17c07c1f, 0x1b00001f, 0xbffce7ff,
	0xd0002020, 0x17c07c1f, 0x81f80407, 0x81fc0407, 0x81fe8407, 0x81ff0407,
	0x1880001f, 0x10006320, 0xc0c02640, 0xe080000f, 0xd8001a03, 0x17c07c1f,
	0xe080001f, 0xa1da0407, 0xe8208000, 0x10000048, 0x00000100, 0xe8208000,
	0x10000004, 0x00000002, 0x1b80001f, 0x20000068, 0xa0110400, 0xa0140400,
	0xa01b0400, 0xa01d0400, 0xa0180400, 0xa01e0400, 0x1b80001f, 0x20000104,
	0x81411801, 0xd8001fa5, 0x17c07c1f, 0x18c0001f, 0x10006240, 0xc0c02580,
	0x17c07c1f, 0xc2802a40, 0x1291841f, 0x1b00001f, 0x7ffcf7ff, 0xf0000000,
	0x17c07c1f, 0x1900001f, 0x10006830, 0xe1000003, 0xa1d30407, 0x81459801,
	0xd8002345, 0x17c07c1f, 0x11407c1f, 0x01400405, 0x1108141f, 0xd80022e4,
	0x17c07c1f, 0x18d0001f, 0x10006830, 0x68e00003, 0x0000beef, 0xd8202163,
	0x17c07c1f, 0xd0002340, 0x17c07c1f, 0xa1d78407, 0xd00022e0, 0x17c07c1f,
	0x81f30407, 0xf0000000, 0x17c07c1f, 0xe0f07f16, 0x1380201f, 0xe0f07f1e,
	0x1380201f, 0xe0f07f0e, 0x1b80001f, 0x20000104, 0xe0f07f0c, 0xe0f07f0d,
	0xe0f07e0d, 0xe0f07c0d, 0xe0f0780d, 0xe0f0700d, 0xf0000000, 0x17c07c1f,
	0xe0f07f0d, 0xe0f07f0f, 0xe0f07f1e, 0xe0f07f12, 0xf0000000, 0x17c07c1f,
	0xd00026e0, 0x11407c1f, 0x81f08407, 0x1b80001f, 0x20000001, 0xa1d08407,
	0x1b80001f, 0x20000104, 0x80eab401, 0xd8202803, 0x17c07c1f, 0x80c01403,
	0xd8202683, 0x01400405, 0x1900001f, 0x10006814, 0xf0000000, 0xe1000003,
	0xa1d10407, 0x1b80001f, 0x20000020, 0xf0000000, 0x17c07c1f, 0xa1d00407,
	0x1b80001f, 0x20000208, 0x80ea3401, 0x1a00001f, 0x10006814, 0xe2000003,
	0xf0000000, 0x17c07c1f, 0x18c0001f, 0x10006b6c, 0x1910001f, 0x10006b6c,
	0xa1002804, 0xe0c00004, 0xf0000000, 0x17c07c1f, 0x17c07c1f, 0x17c07c1f,
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
	0x17c07c1f, 0x17c07c1f, 0x1840001f, 0x00000001, 0x18c0001f, 0x10007024,
	0x1910001f, 0x10007024, 0x89000004, 0x1fffffff, 0xe0c00004, 0x1b80001f,
	0x20000100, 0xa1d48407, 0x1990001f, 0x10006b08, 0xe8208000, 0x10006b6c,
	0x00000000, 0x1b00001f, 0x2ffce7ff, 0x1b80001f, 0x500f0000, 0xe8208000,
	0x10006354, 0xfff01b47, 0xc0c02880, 0x81401801, 0xd8004865, 0x17c07c1f,
	0x81f60407, 0x18c0001f, 0x10006200, 0xc0c06540, 0x12807c1f, 0xe8208000,
	0x1000625c, 0x00000001, 0x1890001f, 0x1000625c, 0x81040801, 0xd8204484,
	0x17c07c1f, 0xc0c06540, 0x1280041f, 0x18c0001f, 0x10006208, 0xc0c06540,
	0x12807c1f, 0xe8208000, 0x10006244, 0x00000001, 0x1890001f, 0x10006244,
	0x81040801, 0xd8204644, 0x17c07c1f, 0xc0c06540, 0x1280041f, 0x18c0001f,
	0x10006290, 0xe0e0004f, 0xc0c06540, 0x1280041f, 0xe8208000, 0x10006404,
	0x00003101, 0xc2802a40, 0x1292041f, 0x1b00001f, 0x2ffce7ff, 0x1b80001f,
	0x30000004, 0x8880000c, 0x2ffce7ff, 0xd8005d42, 0x17c07c1f, 0xe8208000,
	0x10006294, 0x0003ffff, 0x18c0001f, 0x10006294, 0xe0e03fff, 0xe0e003ff,
	0x81449801, 0xd8004cc5, 0x17c07c1f, 0x1a00001f, 0x10006604, 0x81491801,
	0xd8004bc5, 0x17c07c1f, 0xc0c06c80, 0x12807c1f, 0xd0004cc0, 0x17c07c1f,
	0xe2200001, 0xc0c06a00, 0x17c07c1f, 0x1b80001f, 0x2000cd96, 0xe2200003,
	0xc0c06a00, 0x17c07c1f, 0x81419801, 0xd8004f45, 0x17c07c1f, 0x1a00001f,
	0x10006604, 0xe2200006, 0xc0c06a00, 0x17c07c1f, 0xc0c073e0, 0x17c07c1f,
	0xe2200007, 0xc0c06a00, 0x17c07c1f, 0xc0c073e0, 0x17c07c1f, 0xe2200005,
	0xc0c06a00, 0x17c07c1f, 0xc0c073e0, 0x17c07c1f, 0xc0c06940, 0x17c07c1f,
	0xa1d38407, 0xa1d98407, 0xa0108400, 0xa0120400, 0xa0148400, 0xa0150400,
	0xa0158400, 0xa01b8400, 0xa01c0400, 0xa01c8400, 0xa0188400, 0xa0190400,
	0xa0198400, 0xe8208000, 0x10006310, 0x0b1600f8, 0x1b00001f, 0xbffce7ff,
	0x1b80001f, 0x90100000, 0x80c28001, 0xc8c00003, 0x17c07c1f, 0x80c10001,
	0xc8c00e43, 0x17c07c1f, 0x1b00001f, 0x3ffce7ff, 0x18c0001f, 0x10006294,
	0xe0e007fe, 0xe0e00ffc, 0xe0e01ff8, 0xe0e03ff0, 0xe0e03fe0, 0xe0e03fc0,
	0x1b80001f, 0x20000020, 0xe8208000, 0x10006294, 0x0003ffc0, 0xe8208000,
	0x10006294, 0x0003fc00, 0x80388400, 0x80390400, 0x80398400, 0x1b80001f,
	0x20000300, 0x803b8400, 0x803c0400, 0x803c8400, 0x1b80001f, 0x20000300,
	0x80348400, 0x80350400, 0x80358400, 0x1b80001f, 0x20000104, 0x80308400,
	0x80320400, 0x81f38407, 0x81f98407, 0x81f90407, 0x81f40407, 0x81449801,
	0xd8005ac5, 0x17c07c1f, 0x1a00001f, 0x10006604, 0x81491801, 0xd80059c5,
	0x17c07c1f, 0xc2802a40, 0x1294041f, 0xe2200002, 0xc0c06c80, 0x1280041f,
	0xc0c06a00, 0x17c07c1f, 0xd0005a80, 0x17c07c1f, 0xe2200000, 0xc0c06a00,
	0x17c07c1f, 0xe2200002, 0xc0c06a00, 0x17c07c1f, 0x1b80001f, 0x200016a8,
	0x81419801, 0xd8005d45, 0x17c07c1f, 0x1a00001f, 0x10006604, 0xe2200007,
	0xc0c06a00, 0x17c07c1f, 0xc0c073e0, 0x17c07c1f, 0xe2200006, 0xc0c06a00,
	0x17c07c1f, 0xc0c073e0, 0x17c07c1f, 0xe2200004, 0xc0c06a00, 0x17c07c1f,
	0xc0c073e0, 0x17c07c1f, 0x81401801, 0xd80062a5, 0x17c07c1f, 0xe8208000,
	0x10006404, 0x00000101, 0x18c0001f, 0x10006290, 0x1212841f, 0xc0c066c0,
	0x12807c1f, 0xc0c066c0, 0x1280041f, 0x18c0001f, 0x10006208, 0x1212841f,
	0xc0c066c0, 0x12807c1f, 0xe8208000, 0x10006244, 0x00000000, 0x1890001f,
	0x10006244, 0x81040801, 0xd8005fe4, 0x17c07c1f, 0xc0c066c0, 0x1280041f,
	0x18c0001f, 0x10006200, 0x1212841f, 0xc0c066c0, 0x12807c1f, 0xe8208000,
	0x1000625c, 0x00000000, 0x1890001f, 0x1000625c, 0x81040801, 0xd80061c4,
	0x17c07c1f, 0xc0c066c0, 0x1280041f, 0x1910001f, 0x10006360, 0x1940001f,
	0x10006b6c, 0xe1400004, 0x11407c1f, 0xe8208000, 0x10006824, 0x000f0000,
	0x19c0001f, 0x61415820, 0x1ac0001f, 0x55aa55aa, 0x18c0001f, 0x10007024,
	0x1910001f, 0x10007024, 0xa9000004, 0xe0000000, 0xe0c00004, 0xf0000000,
	0xd80065ea, 0x17c07c1f, 0xe2e0004f, 0xe2e0006f, 0xe2e0002f, 0xd820668a,
	0x17c07c1f, 0xe2e0002e, 0xe2e0003e, 0xe2e00032, 0xf0000000, 0x17c07c1f,
	0xd80067ca, 0x17c07c1f, 0xe2e00036, 0x17c07c1f, 0x17c07c1f, 0xe2e0003e,
	0x1380201f, 0xe2e0003c, 0xd820690a, 0x17c07c1f, 0x1b80001f, 0x20000018,
	0xe2e0007c, 0x1b80001f, 0x20000003, 0xe2e0005c, 0xe2e0004c, 0xe2e0004d,
	0xf0000000, 0x17c07c1f, 0xa1d40407, 0x1391841f, 0xa1d90407, 0x1392841f,
	0xf0000000, 0x17c07c1f, 0x11407c1f, 0x01400405, 0x1108141f, 0xd8006b84,
	0x17c07c1f, 0x18d0001f, 0x10006604, 0x10cf8c1f, 0xd8206a23, 0x17c07c1f,
	0xd0006c40, 0x17c07c1f, 0x1a00001f, 0x10006814, 0xe2000004, 0x1880001f,
	0x10006320, 0xe080000f, 0xf0000000, 0x17c07c1f, 0xe8208000, 0x10059850,
	0x00000001, 0xe8208000, 0x10059840, 0x00000003, 0xe8208000, 0x10059854,
	0x00000000, 0xe8208000, 0x10059838, 0x00000001, 0xe8208000, 0x1005980c,
	0x0000001f, 0xe8208000, 0x10059814, 0x00000002, 0xe8208000, 0x10059820,
	0x00000011, 0xe8208000, 0x10059848, 0x00000103, 0xe8208000, 0x10059810,
	0x0000002a, 0xe8208000, 0x10059804, 0x000000c0, 0x1a00001f, 0x10059800,
	0xd82072ca, 0x17c07c1f, 0xe2200001, 0xe22000ac, 0xe8208000, 0x10059824,
	0x00000001, 0x814c1801, 0xd8007245, 0x17c07c1f, 0x1b80001f, 0x20000524,
	0xd0007280, 0x17c07c1f, 0x1b80001f, 0x200066cb, 0xd00073a0, 0x17c07c1f,
	0xe2200001, 0xe220002c, 0xe8208000, 0x10059824, 0x00000001, 0x1b80001f,
	0x20000524, 0xf0000000, 0x17c07c1f, 0x1880001f, 0x0000001d, 0x814a1801,
	0xd8007645, 0x17c07c1f, 0x81499801, 0xd8007745, 0x17c07c1f, 0x814a9801,
	0xd8007845, 0x17c07c1f, 0x18d0001f, 0x40000000, 0x18d0001f, 0x40000000,
	0xd8007542, 0x00a00402, 0xd0007940, 0x17c07c1f, 0x18d0001f, 0x40000000,
	0x18d0001f, 0x80000000, 0xd8007642, 0x00a00402, 0xd0007940, 0x17c07c1f,
	0x18d0001f, 0x40000000, 0x18d0001f, 0x60000000, 0xd8007742, 0x00a00402,
	0xd0007940, 0x17c07c1f, 0x18d0001f, 0x40000000, 0x18d0001f, 0xc0000000,
	0xd8007842, 0x00a00402, 0xd0007940, 0x17c07c1f, 0xf0000000, 0x17c07c1f
};

static struct pcm_desc suspend_pcm = {
	.version	= "pcm_suspend_v00.04_mt8163_20171227_v1",
	.base		= suspend_binary,
	.size		= 972,
	.sess		= 2,
	.replace	= 0,
	.vec0		= EVENT_VEC(11, 1, 0, 0),	/* FUNC_26M_WAKEUP */
	.vec1		= EVENT_VEC(12, 1, 0, 52),	/* FUNC_26M_SLEEP */
	.vec2		= EVENT_VEC(30, 1, 0, 114),	/* FUNC_APSRC_WAKEUP */
	.vec3		= EVENT_VEC(31, 1, 0, 186),	/* FUNC_APSRC_SLEEP */
};

/**************************************
 * SW code for suspend
 **************************************/
#define SPM_SYSCLK_SETTLE       128	/* 3.9ms */

#define WAIT_UART_ACK_TIMES     10	/* 10 * 10us */

#define SPM_WAKE_PERIOD         600	/* sec */

#define WAKE_SRC_FOR_SUSPEND \
	    (WAKE_SRC_MD32_WDT | WAKE_SRC_KP | WAKE_SRC_CONN2AP |\
	    WAKE_SRC_EINT | WAKE_SRC_CONN_WDT | WAKE_SRC_MD32_SPM |\
	    WAKE_SRC_USB_CD | WAKE_SRC_USB_PDN | WAKE_SRC_ALL_MD32)

#define WAKE_SRC_FOR_MD32  0                                          \
				/* (WAKE_SRC_AUD_MD32) */

#define spm_is_wakesrc_invalid(wakesrc)     (!!((u32)(wakesrc) & 0xc0003803))

static struct pwr_ctrl suspend_ctrl = {
	.wake_src = WAKE_SRC_FOR_SUSPEND,
	.wake_src_md32 = WAKE_SRC_FOR_MD32,
	.r0_ctrl_en = 1,
	.r7_ctrl_en = 1,
	.infra_dcm_lock = 1,
	.wfi_op = WFI_OP_AND,

	.ca7top_idle_mask = 0,
	.ca15top_idle_mask = 0,
	.mcusys_idle_mask = 0,
	.disp_req_mask = 0,
	.mfg_req_mask = 0,
	.md32_req_mask = 0,
	.conn_mask = 0,

	/* .pcm_apsrc_req = 1, */
	/* .pcm_f26m_req = 1, */

	.ca7_wfi0_en = 1,
	.ca7_wfi1_en = 1,
	.ca7_wfi2_en = 1,
	.ca7_wfi3_en = 1,
	.ca15_wfi0_en = 1,
	.ca15_wfi1_en = 1,
	.ca15_wfi2_en = 1,
	.ca15_wfi3_en = 1,

#if SPM_BYPASS_SYSPWREQ
	.syspwreq_mask = 1,
#endif
};

struct spm_lp_scen __spm_suspend = {
	.pcmdesc = &suspend_pcm,
	.pwrctrl = &suspend_ctrl,
	.wakestatus = &suspend_info[0],
};

static bool spm_set_suspend_pcm_ver(u32 *suspend_flags)
{
	u32 flag;

	flag = *suspend_flags;

	__spm_suspend.pcmdesc = &suspend_pcm;
	/* flag |= SPM_VCORE_DVS_DIS; */

	*suspend_flags = flag;
	return true;

}

static void spm_set_sysclk_settle(void)
{
	u32 settle;

	/* SYSCLK settle = MD SYSCLK settle but set it again for MD PDN */
	spm_write(SPM_CLK_SETTLE, SPM_SYSCLK_SETTLE);
	settle = spm_read(SPM_CLK_SETTLE);

	spm_crit2("settle = %u\n", settle);
}

static void spm_kick_pcm_to_run(struct pwr_ctrl *pwrctrl)
{
	/* enable PCM WDT (normal mode) to start count if needed */
#if SPM_PCMWDT_EN
	{
		u32 con1;

		con1 = spm_read(SPM_PCM_CON1) &
			~(CON1_PCM_WDT_WAKE_MODE | CON1_PCM_WDT_EN);
		spm_write(SPM_PCM_CON1, CON1_CFG_KEY | con1);

		if (spm_read(SPM_PCM_TIMER_VAL) > PCM_TIMER_MAX)
			spm_write(SPM_PCM_TIMER_VAL, PCM_TIMER_MAX);
		spm_write(SPM_PCM_WDT_TIMER_VAL,
			spm_read(SPM_PCM_TIMER_VAL) + PCM_WDT_TIMEOUT);
		spm_write(SPM_PCM_CON1, con1 | CON1_CFG_KEY | CON1_PCM_WDT_EN);
	}
#endif

	/* init PCM_PASR_DPD_0 for DPD */
	spm_write(SPM_PCM_PASR_DPD_0, 0);

#if 0
	/* make MD32 work in suspend: fscp_ck = CLK26M */
	clkmux_sel(MT_MUX_SCP, 0, "SPM-Sleep");
#endif

	__spm_kick_pcm_to_run(pwrctrl);
}

static void spm_trigger_wfi_for_sleep(struct pwr_ctrl *pwrctrl)
{
#if 0
	sync_hw_gating_value();	/* for Vcore DVFS */
#endif

	if (is_cpu_pdn(pwrctrl->pcm_flags)) {
		spm_dormant_sta = mt_cpu_dormant(CPU_SHUTDOWN_MODE);
		switch (spm_dormant_sta) {
		case MT_CPU_DORMANT_RESET:
			break;
		case MT_CPU_DORMANT_ABORT:
			break;
		case MT_CPU_DORMANT_BREAK:
			break;
		case MT_CPU_DORMANT_BYPASS:
			break;
		}
	} else {
		spm_dormant_sta = -1;
		spm_write(MP0_AXI_CONFIG, spm_read(MP0_AXI_CONFIG) | ACINACTM);
		wfi_with_sync();
		spm_write(MP0_AXI_CONFIG, spm_read(MP0_AXI_CONFIG) & ~ACINACTM);
	}

	if (is_infra_pdn(pwrctrl->pcm_flags))
		mtk8250_restore_dev();
}

static void spm_clean_after_wakeup(void)
{
	/* disable PCM WDT to stop count if needed */
#if SPM_PCMWDT_EN
	spm_write(SPM_PCM_CON1, CON1_CFG_KEY |
		(spm_read(SPM_PCM_CON1) & ~CON1_PCM_WDT_EN));
#endif

	__spm_clean_after_wakeup();

#if 0
	/* restore clock mux: fscp_ck = SYSPLL1_D2 */
	clkmux_sel(MT_MUX_SCP, 1, "SPM-Sleep");
#endif
}

static int
	    spm_output_wake_reason(struct wake_status *wakesta,
			struct pcm_desc *pcmdesc)
{
	int wr;
	u32 md32_flag = 0;
	u32 md32_flag2 = 0;

	wr = __spm_output_wake_reason(wakesta, pcmdesc, true);

#if 1
	memcpy(&suspend_info[log_wakesta_cnt],
		wakesta, sizeof(struct wake_status));
	suspend_info[log_wakesta_cnt].log_index = log_wakesta_index;

	if (log_wakesta_cnt >= 10) {
		log_wakesta_cnt = 0;
		spm_snapshot_golden_setting = 0;
	} else {
		log_wakesta_cnt++;
		log_wakesta_index++;
	}

	if (log_wakesta_index >= 0xFFFFFFF0)
		log_wakesta_index = 0;
#endif

#ifdef CONFIG_MD32_SUPPORT
	md32_flag = read_md32_cfgreg(0x2C);
	md32_flag2 = read_md32_cfgreg(0x30);
#endif
	spm_crit2("suspend dormant state = %d, ",
		spm_dormant_sta);
	spm_crit2("md32_flag = 0x%x, md32_flag2 = %d\n",
		md32_flag, md32_flag2);
#if 0
	if (wakesta->r12 & WAKE_SRC_EINT)
		mt_eint_print_status();
#endif
#if 0
	if (wakesta->debug_flag & (1 << 18)) {
		spm_crit2("MD32 suspned pmic wrapper error");
		WARN_ON(1);
	}

	if (wakesta->debug_flag & (1 << 19)) {
		spm_crit2("MD32 resume pmic wrapper error");
		WARN_ON(1);
	}
#endif

	return wr;
}

#if SPM_PWAKE_EN
static u32 spm_get_wake_period(int pwake_time, int last_wr)
{
	int period = SPM_WAKE_PERIOD;

	if (pwake_time < 0) {
		/* use FG to get the period of 1% battery decrease */
		period = get_dynamic_period(
			last_wr != WR_PCM_TIMER ? 1 : 0, SPM_WAKE_PERIOD, 1);
		if (period <= 0) {
			spm_warn("CANNOT GET PERIOD FROM FUEL GAUGE\n");
			period = SPM_WAKE_PERIOD;
		}
	} else {
		period = pwake_time;
		spm_crit2("pwake = %d\n", pwake_time);
	}

	if (period > 36 * 3600)	/* max period is 36.4 hours */
		period = 36 * 3600;

	return period;
}
#endif

/*
 * wakesrc: WAKE_SRC_XXX
 * enable : enable or disable @wakesrc
 * replace: if true, will replace the default setting
 */
int spm_set_sleep_wakesrc(u32 wakesrc, bool enable, bool replace)
{
	unsigned long flags;

	if (spm_is_wakesrc_invalid(wakesrc))
		return -EINVAL;

	spin_lock_irqsave(&__spm_lock, flags);
	if (enable) {
		if (replace)
			__spm_suspend.pwrctrl->wake_src = wakesrc;
		else
			__spm_suspend.pwrctrl->wake_src |= wakesrc;
	} else {
		if (replace)
			__spm_suspend.pwrctrl->wake_src = 0;
		else
			__spm_suspend.pwrctrl->wake_src &= ~wakesrc;
	}
	spin_unlock_irqrestore(&__spm_lock, flags);

	return 0;
}

/*
 * wakesrc: WAKE_SRC_XXX
 */
u32 spm_get_sleep_wakesrc(void)
{
	return __spm_suspend.pwrctrl->wake_src;
}

static u32 last_wakesrc;

u32 spm_get_last_wakeup_src(void)
{
	return last_wakesrc;
}
EXPORT_SYMBOL(spm_get_last_wakeup_src);

int spm_go_to_sleep(u32 spm_flags, u32 spm_data)
{
	u32 sec = 2;
#if 0
	int wd_ret;
#endif
	struct wake_status wakesta;
	unsigned long flags;
#if 1
	struct mtk_irq_mask mask;
	struct irq_desc *desc = irq_to_desc(spm_irq_0);
#endif
#if 0
	struct wd_api *wd_api;
#endif
	static int last_wr = WR_NONE;
	struct pcm_desc *pcmdesc;
	struct pwr_ctrl *pwrctrl;

	if (spm_set_suspend_pcm_ver(&spm_flags) == false) {
		last_wr = WR_UNKNOWN;
		return last_wr;
	}

	pcmdesc = __spm_suspend.pcmdesc;
	pwrctrl = __spm_suspend.pwrctrl;

	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);
	set_pwrctrl_pcm_data(pwrctrl, spm_data);

#if SPM_PWAKE_EN
	sec = spm_get_wake_period(-1/* FIXME */, last_wr);
#endif
	pwrctrl->timer_val = sec * 32768;

#if 0
	wd_ret = get_wd_api(&wd_api);
	if (!wd_ret)
		wd_api->wd_suspend_notify();
#endif

	spin_lock_irqsave(&__spm_lock, flags);
	mt_irq_mask_all(&mask);
	if (desc)
		unmask_irq(desc);
	mt_cirq_clone_gic();
	mt_cirq_enable();

	spm_set_sysclk_settle();

	spm_crit2("sec = %u, wakesrc = 0x%x (%u)(%u)\n",
		  sec, pwrctrl->wake_src, is_cpu_pdn(pwrctrl->pcm_flags),
		  is_infra_pdn(pwrctrl->pcm_flags));

	if (mtk8250_request_to_sleep()) {
		last_wr = WR_UART_BUSY;
		goto RESTORE_IRQ;
	}

	__spm_reset_and_init_pcm(pcmdesc);

	__spm_kick_im_to_fetch(pcmdesc);

	__spm_init_pcm_register();

	__spm_init_event_vector(pcmdesc);

	__spm_set_power_control(pwrctrl);

	__spm_set_wakeup_event(pwrctrl);

	spm_kick_pcm_to_run(pwrctrl);

	spm_trigger_wfi_for_sleep(pwrctrl);

	__spm_get_wakeup_status(&wakesta);

	last_wakesrc = wakesta.r12;

	spm_clean_after_wakeup();

	mtk8250_request_to_wakeup();

	last_wr = spm_output_wake_reason(&wakesta, pcmdesc);

RESTORE_IRQ:
	mt_cirq_flush();
	mt_cirq_disable();
	mt_irq_mask_restore(&mask);
	spin_unlock_irqrestore(&__spm_lock, flags);

#if 0
	if (!wd_ret)
		wd_api->wd_resume_notify();
#endif
	return last_wr;
}

bool spm_is_conn_sleep(void)
{
	return !(spm_read(SPM_PCM_REG13_DATA) & R13_CONN_SRCLKENA);
}

void spm_set_wakeup_src_check(void)
{
	/* clean wakeup event raw status */
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, 0xFFFFFFFF);

	/* set wakeup event */
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, ~WAKE_SRC_FOR_SUSPEND);
}

bool spm_check_wakeup_src(void)
{
	u32 wakeup_src;

	/* check wanek event raw status */
	wakeup_src = spm_read(SPM_SLEEP_ISR_RAW_STA);

	if (wakeup_src) {
		spm_crit2("WARNING: spm_check_wakeup_src = 0x%x", wakeup_src);
		return 1;
	} else
		return 0;
}

void spm_poweron_config_set(void)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);
	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
	spin_unlock_irqrestore(&__spm_lock, flags);
}

void spm_md32_sram_con(u32 value)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);
	/* enable register control */
	spm_write(SPM_MD32_SRAM_CON, value);
	spin_unlock_irqrestore(&__spm_lock, flags);
}

#define hw_spin_lock_for_ddrdfs()
#define hw_spin_unlock_for_ddrdfs()

#if 0
bool spm_set_suspned_pcm_init_flag(u32 *suspend_flags)
{
#ifdef CONFIG_OF
	int node;

	spm_suspend_flag = *suspend_flags;

	node = of_scan_flat_dt(dt_scan_memory, NULL);
#else
	spm_err("dram rank1_info_error: no rank info\n");
	WARN_ON(1);
	/* return false; */
#endif

#if 0
	if (is_ext_buck_exist())
		flag &= ~SPM_BUCK_SEL;
	else
		flag |= SPM_BUCK_SEL;
#endif

	/* spm_suspend_flag would be set in dt_scan_memory */
	*suspend_flags = spm_suspend_flag;
	return true;
}
#endif

void spm_output_sleep_option(void)
{
	spm_notice("PWAKE_EN:%d, PCMWDT_EN:%d, ",
		   SPM_PWAKE_EN, SPM_PCMWDT_EN);
	spm_notice("BYPASS_SYSPWREQ:%d, I2C_CHANNEL:%d\n",
		   SPM_BYPASS_SYSPWREQ, I2C_CHANNEL);
}

MODULE_DESCRIPTION("SPM-Sleep Driver v0.1");
