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

#ifndef __AMSI2C_H
#define __AMSI2C_H

extern int ams_i2c_blk_read(struct i2c_client *client, u8 reg, u8 *val,
		int size);
extern int ams_i2c_read(struct i2c_client *client, u8 reg, u8 *val);
extern int ams_i2c_blk_read_direct(struct i2c_client *client, u8 reg, u8 *val,
		int size);
extern int ams_i2c_write_direct(struct i2c_client *client, u8 reg, u8 val);
extern int ams_i2c_write(struct i2c_client *client, u8 *shadow, u8 reg, u8 val);
extern int ams_i2c_reg_blk_write(struct i2c_client *client,	u8 reg, u8 *val,
		int size);
extern int ams_i2c_ram_blk_write(struct i2c_client *client,	u8 reg, u8 *val,
		int size);
extern int ams_i2c_ram_blk_read(struct i2c_client *client, u8 reg, u8 *val,
		int size);
extern int ams_i2c_modify(struct i2c_client *client, u8 *shadow, u8 reg,
		u8 mask, u8 val);
extern void ams_i2c_set_field(struct i2c_client *client, u8 *shadow, u8 reg,
		u8 pos, u8 nbits, u8 val);
extern void ams_i2c_get_field(struct i2c_client *client, u8 reg, u8 pos,
		u8 nbits, u8 *val);


#endif /* __AMSI2C_H */

