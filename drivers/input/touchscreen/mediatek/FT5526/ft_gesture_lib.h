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
* File Name: Ft_gesture_lib.h

* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract: entrance for focaltech ts driver
*
* Version: V1.0
*
*******************************************************************************/

#ifndef __LINUX_FT_GESTURE_LIB_H__
#define __LINUX_FT_GESTURE_LIB_H__

/* int fetch_object_sample(unsigned short *datax,unsigned short *datay,unsigned char *dataxy,short pointnum,unsigned long time_stamp); */
int fetch_object_sample(unsigned char *buf, short pointnum);
void init_para(int x_pixel, int y_pixel, int time_slot, int cut_x_pixel, int cut_y_pixel);

#endif
