/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MUSBFSH_MT65XX_H__
#define __MUSBFSH_MT65XX_H__
#ifndef CONFIG_OF
#include <mach/mt_reg_base.h>
#endif

#if defined(MTK_DT_SUPPORT) && !defined(EVDO_DT_SUPPORT)
#include <cust_eint.h>

typedef void (*EINT_FUNC_PTR)(void);

extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
extern void mt65xx_eint_set_polarity(uint8_t eintno,
					enum kal_bool ACT_Polarity);
extern void mt65xx_eint_set_hw_debounce(uint8_t eintno, unsigned int ms);
extern unsigned int mt65xx_eint_set_sens(uint8_t eintno, enum kal_bool sens);
extern void mt65xx_eint_registration(uint8_t eintno, enum kal_bool Dbounce_En,
		     enum kal_bool ACT_Polarity, EINT_FUNC_PTR func,
		     enum kal_bool auto_umask);
#endif
extern int usb11_init(void);
extern void usb11_exit(void);
#ifdef CONFIG_OF
extern struct musbfsh *musbfsh_Device;
#ifdef CONFIG_MTK_USB_RESET_WIFI_DONGLE
extern int wifi_reset_pin;
extern void wifi_set_low(void);
extern void wifi_set_high(void);
#endif
#endif
/*#define IC_USB*/


/*Level 1 interrupts:*/
#define USB11_L1INTS 0xA0
#define USB11_L1INTM 0xA4
#define USB11_L1INTP 0xA8
#define MUSBFSH_HSDMA_BASE		0x200
#define MUSBFSH_HSDMA_INTR		(MUSBFSH_HSDMA_BASE + 0)
#define MUSBFSH_DMA_INTR_UNMASK_CLR_OFFSET (16)
#define MUSBFSH_DMA_INTR_UNMASK_SET_OFFSET (24)
#define USB11_BASE USB1_BASE
#define USB_BASE USB11_BASE
/*USB11 PHY registers:*/
#define USB11_PHY_ADDR (USB_SIF_BASE + 0x900)

#define U1PHYCR0 0xC0
/* @U1PHYCR0+1, 1:power on or recovery; 0:save current*/
#define RG_USB11_FSLS_ENBGRI 0x08

#define U1PHYCR1 0xC4
#define force_usb11_en_fs_ls_rcv 0x04	/* @U1PHYCR1+2*/
#define force_usb11_en_fs_ls_tx 0x02	/* @U1PHYCR1+2*/
#define RG_USB11_EN_FS_LS_RCV 0x04	/* @U1PHYCR1+3*/
#define RG_USB11_EN_FS_LS_TX 0x02	/* @U1PHYCR1+3*/

#define U1PHTCR2 0xC8
#define force_usb11_dm_rpu 0x01
#define force_usb11_dp_rpu 0x02
#define force_usb11_dm_rpd 0x04
#define force_usb11_dp_rpd 0x08
#define RG_USB11_DM_RPU 0x10
#define RG_USB11_DP_RPU 0x20
#define RG_USB11_DM_RPD 0x40
#define RG_USB11_DP_RPD 0x80
#define RG_USB11_AVALID 0x04	/* @U1PHYCR2+2*/
#define RG_USB11_BVALID 0x08	/* @U1PHYCR2+2*/
#define RG_USB11_SESSEND 0x10	/* @U1PHYCR2+2*/
#define RG_USB11_VBUSVALID 0x20	/* @U1PHYCR2+2*/
#define force_usb11_avalid 0x04	/*@U1PHYCR2+3*/
#define force_usb11_bvalid 0x08	/* @U1PHYCR2+3*/
#define force_usb11_sessend 0x10	/* @U1PHYCR2+3*/
#define force_usb11_vbusvalid 0x20	/* @U1PHYCR2+3*/

#ifdef CONFIG_OF

/*USB11 PHY access macro: Need Modify Later*/
#define USB11PHY_READ32(offset) \
	__raw_readl((void __iomem *)(musbfsh_Device->phy_reg_base \
	+ 0x900 + (offset)))
#define USB11PHY_READ8(offset) \
	__raw_readb((void __iomem *)(musbfsh_Device->phy_reg_base \
	+ 0x900 + (offset)))
#define USB11PHY_WRITE8(offset, value)  \
	__raw_writeb(value, \
	(void __iomem *)(musbfsh_Device->phy_reg_base + 0x900 + (offset)))
#define USB11PHY_SET8(offset, mask) \
	USB11PHY_WRITE8((offset), USB11PHY_READ8(offset) | (mask))
#define USB11PHY_CLR8(offset, mask) \
	USB11PHY_WRITE8((offset), USB11PHY_READ8(offset) & (~(mask)))
#else

/*USB11 PHY access macro:*/
#define USB11PHY_READ32(offset) \
	__raw_readl((void __iomem *)(USB11_PHY_ADDR + (offset)))
#define USB11PHY_READ8(offset) \
	__raw_readb((void __iomem *)(USB11_PHY_ADDR + (offset)))
#define USB11PHY_WRITE8(offset, value) \
	__raw_writeb(value, (void __iomem *)(USB11_PHY_ADDR + (offset)))
#define USB11PHY_SET8(offset, mask) \
	USB11PHY_WRITE8((offset), USB11PHY_READ8(offset) | (mask))
#define USB11PHY_CLR8(offset, mask) \
	USB11PHY_WRITE8((offset), USB11PHY_READ8(offset) & (~(mask)))
#endif
#endif
