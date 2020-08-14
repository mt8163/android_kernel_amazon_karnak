/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _BATTERY_METER_TABLE_H
#define _BATTERY_METER_TABLE_H

/* ============================================================*/
/* Battery profile and r_profile table should be all the size */
/* Battery profile and r_profile in DTS should be equal or smaller */
/* ============================================================*/
#define PROFILE_TABLE_SIZE    100

/* ============================================================*/
/* typedef*/
/* ============================================================*/
#define BATTERY_PROFILE_STRUCT struct battery_profile_struct

struct battery_profile_struct {
	signed int percentage;
	signed int voltage;
};

#define R_PROFILE_STRUCT struct r_profile_struct

struct r_profile_struct {
	signed int resistance; /* Ohm*/
	signed int voltage;
};

#if defined(CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT)
#define Q_MAX_AGING struct q_max_aging
struct q_max_aging {
	signed int Q_0_CYCLE;
	signed int Q_500_CYCLE;
};
#endif
/* ============================================================*/
/* <DOD, Battery_Voltage> Table*/
/* ============================================================*/
BATT_TEMPERATURE Batt_Temperature_Table[] = {
		{-20, 75022},
		{-15, 57926},
		{-10, 45168},
		{ -5, 35548},
		{  0, 28224},
		{  5, 22595},
		{ 10, 18231},
		{ 15, 14820},
		{ 20, 12133},
		{ 25, 10000},
		{ 30, 8295},
		{ 35, 6922},
		{ 40, 5810},
		{ 45, 4903},
		{ 50, 4160},
		{ 55, 3547},
		{ 60, 3039},
		{ 65, 3038},
		{ 70, 3037}
};

/* T0 -10C*/
BATTERY_PROFILE_STRUCT battery_profile_t0[PROFILE_TABLE_SIZE];

/* T1 0C*/
BATTERY_PROFILE_STRUCT battery_profile_t1[PROFILE_TABLE_SIZE];

/* T1_5 10C*/
BATTERY_PROFILE_STRUCT battery_profile_t1_5[PROFILE_TABLE_SIZE];

/* T2 25C*/
BATTERY_PROFILE_STRUCT battery_profile_t2[PROFILE_TABLE_SIZE];

/* T3 50C*/
BATTERY_PROFILE_STRUCT battery_profile_t3[PROFILE_TABLE_SIZE];

BATTERY_PROFILE_STRUCT battery_profile_temperature[PROFILE_TABLE_SIZE];

/* ============================================================*/
/* <Rbat, Battery_Voltage> Table*/
/* ============================================================*/
/* T0 -10C*/
R_PROFILE_STRUCT r_profile_t0[PROFILE_TABLE_SIZE];

/* T1 0C*/
R_PROFILE_STRUCT r_profile_t1[PROFILE_TABLE_SIZE];

/* T1_5 10C*/
R_PROFILE_STRUCT r_profile_t1_5[PROFILE_TABLE_SIZE];

/* T2 25C*/
R_PROFILE_STRUCT r_profile_t2[PROFILE_TABLE_SIZE];

/* T3 50C*/
R_PROFILE_STRUCT r_profile_t3[PROFILE_TABLE_SIZE];

R_PROFILE_STRUCT r_profile_temperature[PROFILE_TABLE_SIZE];
#if defined(CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT)
/* ============================================================*/
/* Battery Aging Table*/
/* ============================================================*/
/* T0 -10C*/
BATTERY_PROFILE_STRUCT battery_profile_t0_0cycle[PROFILE_TABLE_SIZE];
BATTERY_PROFILE_STRUCT battery_profile_t0_500cycle[PROFILE_TABLE_SIZE];
R_PROFILE_STRUCT r_profile_t0_0cycle[PROFILE_TABLE_SIZE];
R_PROFILE_STRUCT r_profile_t0_500cycle[PROFILE_TABLE_SIZE];
/* (Q_max_0cycle, Q_max_500cycle) */
Q_MAX_AGING q_max_t0_aging;
Q_MAX_AGING q_max_t0_h_current_aging;

/* T1 0C*/
BATTERY_PROFILE_STRUCT battery_profile_t1_0cycle[PROFILE_TABLE_SIZE];
BATTERY_PROFILE_STRUCT battery_profile_t1_500cycle[PROFILE_TABLE_SIZE];
R_PROFILE_STRUCT r_profile_t1_0cycle[PROFILE_TABLE_SIZE];
R_PROFILE_STRUCT r_profile_t1_500cycle[PROFILE_TABLE_SIZE];
Q_MAX_AGING q_max_t1_aging;
Q_MAX_AGING q_max_t1_h_current_aging;

/* T1_5 10C*/
BATTERY_PROFILE_STRUCT battery_profile_t1_5_0cycle[PROFILE_TABLE_SIZE];
BATTERY_PROFILE_STRUCT battery_profile_t1_5_500cycle[PROFILE_TABLE_SIZE];
R_PROFILE_STRUCT r_profile_t1_5_0cycle[PROFILE_TABLE_SIZE];
R_PROFILE_STRUCT r_profile_t1_5_500cycle[PROFILE_TABLE_SIZE];
Q_MAX_AGING q_max_t1_5_aging;
Q_MAX_AGING q_max_t1_5_h_current_aging;

/* T2 25C*/
BATTERY_PROFILE_STRUCT battery_profile_t2_0cycle[PROFILE_TABLE_SIZE];
BATTERY_PROFILE_STRUCT battery_profile_t2_500cycle[PROFILE_TABLE_SIZE];
R_PROFILE_STRUCT r_profile_t2_0cycle[PROFILE_TABLE_SIZE];
R_PROFILE_STRUCT r_profile_t2_500cycle[PROFILE_TABLE_SIZE];
Q_MAX_AGING q_max_t2_aging;
Q_MAX_AGING q_max_t2_h_current_aging;

/* T3 50C*/
BATTERY_PROFILE_STRUCT battery_profile_t3_0cycle[PROFILE_TABLE_SIZE];
BATTERY_PROFILE_STRUCT battery_profile_t3_500cycle[PROFILE_TABLE_SIZE];
R_PROFILE_STRUCT r_profile_t3_0cycle[PROFILE_TABLE_SIZE];
R_PROFILE_STRUCT r_profile_t3_500cycle[PROFILE_TABLE_SIZE];
Q_MAX_AGING q_max_t3_aging;
Q_MAX_AGING q_max_t3_h_current_aging;
#endif

/* ============================================================*/
/* function prototype*/
/* ============================================================*/
int fgauge_get_saddles(void);
BATTERY_PROFILE_STRUCT *fgauge_get_profile(unsigned int temperature);

int fgauge_get_saddles_r_table(void);
R_PROFILE_STRUCT *fgauge_get_profile_r_table(unsigned int temperature);

#endif	/*#ifndef _BATTERY_METER_TABLE_H*/
