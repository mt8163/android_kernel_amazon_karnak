/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2016, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*******************************************************************************
*
* File Name: Tpd_custom_upgrade.h

* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract: entrance for focaltech ts driver
*
* Version: V1.0
*
*******************************************************************************/

#ifndef TOUCHPANEL_UPGRADE_H__
#define TOUCHPANEL_UPGRADE_H__



/*#define CTP_DETECT_SUPPLIER_THROUGH_GPIO  1*/

#ifdef TPD_ALWAYS_UPGRADE_FW
#undef TPD_ALWAYS_UPGRADE_FW
#endif

/*#define GPIO_CTP_COMPAT_PIN1  GPIO28
#define GPIO_CTP_COMPAT_PIN2    GPIO29*/

#define TPD_SUPPLIER_0      0
#define TPD_SUPPLIER_1      1
#define TPD_SUPPLIER_2      2
#define TPD_SUPPLIER_3      3

#define TPD_FW_VER_0        0x00
#define TPD_FW_VER_1        0x11  /* ht tp   0   1 (choice 0) */
#define TPD_FW_VER_2            0x00
#define TPD_FW_VER_3            0x10  /* zxv tp  0   0 */


static unsigned char TPD_FW[] = {

#include "5526i_TPV_Wally_MG_Edge_V0e_D01_20161025_app.i"
};

#ifdef CTP_DETECT_SUPPLIER_THROUGH_GPIO

static unsigned char TPD_FW0[] = {

#include "5526i_TPV_Wally_MG_Edge_V0e_D01_20161025_app.i"
};

static unsigned char TPD_FW1[] = {

#include "5526i_TPV_Wally_MG_Edge_V0e_D01_20161025_app.i"
};

static unsigned char TPD_FW2[] = {

#include "5526i_TPV_Wally_MG_Edge_V0e_D01_20161025_app.i"
};

static unsigned char TPD_FW3[] = {

#include "5526i_TPV_Wally_MG_Edge_V0e_D01_20161025_app.i"
};

#endif /* CTP_DETECT_SUPPLIER_THROUGH_GPIO */


#endif /* TOUCHPANEL_UPGRADE_H__ */
