/*
 ***************************************************************************
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 ***************************************************************************

    Module Name:
    debugfs.h

    Abstract:

    Revision History:
    Who         When            What
    --------    ----------      ----------------------------------------------
    Name        Date            Modification logs
    Conard    03-16-2015
*/



#ifndef __DEBUGFS_H__
#define __DEBUGFS_H__
void mt_debugfs_init(void);
void mt_dev_debugfs_init(PRTMP_ADAPTER);
void mt_dev_debugfs_remove(PRTMP_ADAPTER);
void mt_debugfs_remove(void);
#endif

