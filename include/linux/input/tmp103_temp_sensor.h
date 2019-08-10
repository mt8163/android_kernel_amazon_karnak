/*
* TMP103 Temperature sensor driver file
*
* Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
* Author: Mandrenko, Ievgen" <ievgen.mandrenko@ti.com>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
* 02110-1301 USA
*
*/


#ifndef __ARCH_ARM_PLAT_OMAP_INCLUDE_PLAT_TMP103_TEMPERATURE_SENSOR_H
#define __ARCH_ARM_PLAT_OMAP_INCLUDE_PLAT_TMP103_TEMPERATURE_SENSOR_H

#define TMP103_SENSOR_NAME "tmp103_sensor"

/*
 * omap_temp_sensor structure
 * @iclient - I2c client pointer
 * @dev - device pointer
 * @sensor_mutex - Mutex for sysfs, irq and PM
 * @therm_fw - thermal device
 */
struct tmp103_temp_sensor {
        struct i2c_client *iclient;
        struct device *dev;
        struct mutex sensor_mutex;
        struct thermal_dev *therm_fw;
        u16 config_orig;
        u16 config_current;
        unsigned long last_update;
        int temp[3];
        int debug_temp;
};

#endif
