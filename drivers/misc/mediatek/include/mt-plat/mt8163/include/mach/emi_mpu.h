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

#ifndef __MT_EMI_MPU_H
#define __MT_EMI_MPU_H

#define EMI_MPUA		(EMI_BASE_ADDR+0x0160)
#define EMI_MPUB		(EMI_BASE_ADDR+0x0168)
#define EMI_MPUC		(EMI_BASE_ADDR+0x0170)
#define EMI_MPUD		(EMI_BASE_ADDR+0x0178)
#define EMI_MPUE		(EMI_BASE_ADDR+0x0180)
#define EMI_MPUF		(EMI_BASE_ADDR+0x0188)
#define EMI_MPUG		(EMI_BASE_ADDR+0x0190)
#define EMI_MPUH		(EMI_BASE_ADDR+0x0198)
#define EMI_MPUI		(EMI_BASE_ADDR+0x01A0)
#define EMI_MPUI_2ND	(EMI_BASE_ADDR+0x01A4)
#define EMI_MPUJ		(EMI_BASE_ADDR+0x01A8)
#define EMI_MPUJ_2ND	(EMI_BASE_ADDR+0x01AC)
#define EMI_MPUK		(EMI_BASE_ADDR+0x01B0)
#define EMI_MPUK_2ND	(EMI_BASE_ADDR+0x01B4)
#define EMI_MPUL		(EMI_BASE_ADDR+0x01B8)
#define EMI_MPUL_2ND	(EMI_BASE_ADDR+0x01BC)
#define EMI_MPUM		(EMI_BASE_ADDR+0x01C0)
#define EMI_MPUN		(EMI_BASE_ADDR+0x01C8)
#define EMI_MPUO		(EMI_BASE_ADDR+0x01D0)
#define EMI_MPUP		(EMI_BASE_ADDR+0x01D8)
#define EMI_MPUQ		(EMI_BASE_ADDR+0x01E0)
#define EMI_MPUR		(EMI_BASE_ADDR+0x01E8)
#define EMI_MPUS		(EMI_BASE_ADDR+0x01F0)
#define EMI_MPUT		(EMI_BASE_ADDR+0x01F8)
#define EMI_MPUU		(EMI_BASE_ADDR+0x0200)
#define EMI_MPUY		(EMI_BASE_ADDR+0x0220)
#define EMI_MPUA2		(EMI_BASE_ADDR+0x0260)
#define EMI_MPUB2		(EMI_BASE_ADDR+0x0268)
#define EMI_MPUC2		(EMI_BASE_ADDR+0x0270)
#define EMI_MPUD2		(EMI_BASE_ADDR+0x0278)
#define EMI_MPUE2		(EMI_BASE_ADDR+0x0280)
#define EMI_MPUF2		(EMI_BASE_ADDR+0x0288)
#define EMI_MPUG2		(EMI_BASE_ADDR+0x0290)
#define EMI_MPUH2		(EMI_BASE_ADDR+0x0298)
#define EMI_MPUI2		(EMI_BASE_ADDR+0x02A0)
#define EMI_MPUI2_2ND	(EMI_BASE_ADDR+0x02A4)
#define EMI_MPUJ2		(EMI_BASE_ADDR+0x02A8)
#define EMI_MPUJ2_2ND	(EMI_BASE_ADDR+0x02AC)
#define EMI_MPUK2		(EMI_BASE_ADDR+0x02B0)
#define EMI_MPUK2_2ND	(EMI_BASE_ADDR+0x02B4)
#define EMI_MPUL2		(EMI_BASE_ADDR+0x02B8)
#define EMI_MPUL2_2ND	(EMI_BASE_ADDR+0x02BC)
#define EMI_MPUM2		(EMI_BASE_ADDR+0x02C0)
#define EMI_MPUN2		(EMI_BASE_ADDR+0x02C8)
#define EMI_MPUO2		(EMI_BASE_ADDR+0x02D0)
#define EMI_MPUP2		(EMI_BASE_ADDR+0x02D8)
#define EMI_MPUQ2		(EMI_BASE_ADDR+0x02E0)
#define EMI_MPUR2		(EMI_BASE_ADDR+0x02E8)
#define EMI_MPUU2		(EMI_BASE_ADDR+0x0300)
#define EMI_MPUY2		(EMI_BASE_ADDR+0x0320)

#define EMI_WP_ADR    (EMI_BASE_ADDR + 0x5E0)
#define EMI_WP_CTRL   (EMI_BASE_ADDR + 0x5E8)
#define EMI_CHKER        (EMI_BASE_ADDR + 0x5F0)
#define EMI_CHKER_TYPE    (EMI_BASE_ADDR + 0x5F4)
#define EMI_CHKER_ADR   (EMI_BASE_ADDR + 0x5F8)

#define EMI_CONA    (EMI_BASE_ADDR + 0x00)
#define EMI_CONH    (EMI_BASE_ADDR + 0x38)

#define EMI_MPU_START   (0x0160)
#define EMI_MPU_END     (0x03B8)

/*#define DISABLE_IRQ*/

#define NO_PROTECTION 0
#define SEC_RW 1
#define SEC_RW_NSEC_R 2
#define SEC_RW_NSEC_W 3
#define SEC_R_NSEC_R 4
#define FORBIDDEN 5
#define SEC_R_NSEC_RW 6

#define EN_MPU_STR "ON"
#define DIS_MPU_STR "OFF"

/*EMI memory protection align 64K*/
#define EMI_MPU_ALIGNMENT 0x10000
#define OOR_VIO 0x00000200
#define AP_REGION_ID 15

enum {
	/* apmcu */
	MST_ID_APMCU_0, MST_ID_APMCU_1, MST_ID_APMCU_2,
	MST_ID_APMCU_3, MST_ID_APMCU_4, MST_ID_APMCU_5,
	MST_ID_APMCU_6, MST_ID_APMCU_7, MST_ID_APMCU_8,
	MST_ID_APMCU_9, MST_ID_APMCU_10, MST_ID_APMCU_11,
	MST_ID_APMCU_12, MST_ID_APMCU_13, MST_ID_APMCU_14,
	MST_ID_APMCU_15, MST_ID_APMCU_16, MST_ID_APMCU_17,
	MST_ID_APMCU_18, MST_ID_APMCU_19, MST_ID_APMCU_20,
	MST_ID_APMCU_21, MST_ID_APMCU_22,

	/* Modem */
	MST_ID_MDMCU_0,
	MST_ID_MD_LITE,
	/* Modem HW (2G/3G) */
	MST_ID_MDHW_0,

	/* MM */
	MST_ID_MM_0, MST_ID_MM_1, MST_ID_MM_2,
	MST_ID_MM_3, MST_ID_MM_4, MST_ID_MM_5,

	/* Periperal */
	MST_ID_PERI_0, MST_ID_PERI_1, MST_ID_PERI_2,
	MST_ID_PERI_3, MST_ID_PERI_4, MST_ID_PERI_5,
	MST_ID_PERI_6, MST_ID_PERI_7, MST_ID_PERI_8,
	MST_ID_PERI_9, MST_ID_PERI_10, MST_ID_PERI_11,
	MST_ID_PERI_12, MST_ID_PERI_13, MST_ID_PERI_14,
	MST_ID_PERI_15, MST_ID_PERI_16, MST_ID_PERI_17,
	MST_ID_PERI_18, MST_ID_PERI_20,

	/*Conn*/
	MST_ID_CONN_0,

	/* GPU */
	MST_ID_GPU_0,

	MST_INVALID,
	NR_MST,
};

enum {
	MASTER_APMCU = 0,
	MASTER_APMCU_2 = 1,
	MASTER_MM,
	MASTER_MDMCU,
	MASTER_MDHW,
	MASTER_MM_2,
	MASTER_PERI,
	MASTER_GPU,
	MASTER_ALL = 8
};

#define SET_ACCESS_PERMISSON(d7, d6, d5, d4, d3, d2, d1, d0)\
	((((d3) << 9) | ((d2) << 6) | ((d1) << 3) | (d0)) | \
	((((d7) << 9) | ((d6) << 6) | ((d5) << 3) | (d4)) << 16))

extern int emi_mpu_set_region_protection(unsigned int start_addr,
			unsigned int end_addr,
			int region, unsigned int access_permission);
extern phys_addr_t get_max_DRAM_size(void);
extern void __iomem *EMI_BASE_ADDR;

#endif  /* !__MT_EMI_MPU_H */
