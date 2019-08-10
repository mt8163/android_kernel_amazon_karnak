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
typedef struct _BATTERY_PROFILE_STRUCT {
	signed int percentage;
	signed int voltage;
} BATTERY_PROFILE_STRUCT, *BATTERY_PROFILE_STRUCT_P;

typedef struct _R_PROFILE_STRUCT {
	signed int resistance;
	signed int voltage;
} R_PROFILE_STRUCT, *R_PROFILE_STRUCT_P;

typedef struct {
	signed int BatteryTemp;
	signed int TemperatureR;
} BATT_TEMPERATURE;

#if defined(CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT)
typedef struct {
	signed int Q_0_CYCLE;
	signed int Q_500_CYCLE;
} Q_MAX_AGING;
#endif
/* ============================================================*/
/* <DOD, Battery_Voltage> Table*/
/* ============================================================*/
BATT_TEMPERATURE Batt_Temperature_Table[] = {
		{-20 , 75022},
		{-15 , 57926},
		{-10 , 45168},
		{ -5 , 35548},
		{  0 , 28224},
		{  5 , 22595},
		{ 10 , 18231},
		{ 15 , 14820},
		{ 20 , 12133},
		{ 25 , 10000},
		{ 30 , 8295},
		{ 35 , 6922},
		{ 40 , 5810},
		{ 45 , 4903},
		{ 50 , 4160},
		{ 55 , 3547},
		{ 60 , 3039},
		{ 65 , 3038},
		{ 70 , 3037}
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
BATTERY_PROFILE_STRUCT_P fgauge_get_profile(unsigned int temperature);

int fgauge_get_saddles_r_table(void);
R_PROFILE_STRUCT_P fgauge_get_profile_r_table(unsigned int temperature);

#endif	/*#ifndef _BATTERY_METER_TABLE_H*/
