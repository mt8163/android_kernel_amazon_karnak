/*
 * Device driver for monitoring ambient light intensity (lux)
 * for device tsl2540.
 *
 * Copyright (c) 2017, ams AG
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __AMS_TSL2540_ALS_H
#define __AMS_TSL2540_ALS_H

extern struct attribute_group tsl2540_als_attr_group;

extern int tsl2540_configure_als_mode(struct tsl2540_chip *chip, u8 state);
extern int tsl2540_get_lux(struct tsl2540_chip *chip);
extern int tsl2540_read_als(struct tsl2540_chip *chip);
extern void tsl2540_report_als(struct tsl2540_chip *chip);

#endif /* __AMS_TSL2540_ALS_H */
