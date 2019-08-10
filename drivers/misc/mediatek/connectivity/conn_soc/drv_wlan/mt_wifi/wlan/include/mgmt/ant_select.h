/*
** Id:
*/

/*! \file   "antenna_select.h"
    \brief  This file defines the FSM for Antenna selection MODULE.

    This file defines the FSM for Antenna selection MODULE.
*/


#ifndef _ANT_SELECT_H
#define _ANT_SELECT_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#define DEFAULT_ANTENNA_SELECT_DWELL_TIME 5
#define DEFAULT_ANTENNA_SELECT_QUERY_TIME 100
#define MOVING_AVERAGE_ONE 0x3
#define MOVING_AVERAGE_FOURTH 0x2
#define MOVING_AVERAGE_EIGHTH 0x1
#define MOVING_AVERAGE_SIXTEENTH 0x0

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/**
  * enum antenna_select_state - Antenna selection state machine state
  *
  * @ANT_SEL_STATE_IDLE: Antenna query stop while initial, done or abort.
  * @ANT_SEL_STATE_START: Notify AIS module start antenna query process.
  * @ANT_SEL_STATE_QUERY: Under antenna query process.
  * @ANT_SEL_STATE_NUM: Number of state, not a real state.
  */
enum antenna_select_state {
	ANT_SEL_STATE_IDLE = 0,
	ANT_SEL_STATE_START,
	ANT_SEL_STATE_QUERY,
	ANT_SEL_STATE_NUM
};

/**
  * struct antenna_select_config - Antenna selection configuration
  *
  * Report active antenna number and Rssi
  *
  * @ant_num: Active antenna number (0/1)
  * @rssi: The Rssi value while connect to AP or switch antenna.
  */
struct antenna_select_config {
	unsigned int ant_num;
	PARAM_RSSI rssi;
};

/**
  * struct antenna_select_param - Antenna query parameter
  *
  * @ant_num: Query antenna.
  * @query_time: Query time (unit:ms)
  */
struct antenna_select_param {
	unsigned int ant_num;
	unsigned int query_time;
};

/**
  * struct antenna_select_info - Antenna selection info
  *
  * @curr_state: Currenct state machine state
  * @ant_query_start_time: Enter ANT_SEL_STATE_QUERY time
  * @current_ssid: Current SSID for scan request
  * @ant_avg_rssi: Moving average Rssi of probe response
  * @params: Antenna query parameter
  * @config: Antenna selection configuration
  * @ch_dwell_time: Scan channel dwell time (unit:ms)
  * @moving_avg_mode: Moving average mode (0=1/16, 1=1/4, 2=1/8, 3=1)
  */
struct antenna_select_info {
	enum antenna_select_state curr_state;
	OS_SYSTIME ant_query_start_time;
	PARAM_SSID_T current_ssid[4];
	PARAM_RSSI *ant_avg_rssi;
	struct antenna_select_param params;
	struct antenna_select_config config;
	unsigned short ch_dwell_time;
	unsigned char moving_avg_mode;
};

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
VOID antenna_select_fsm_init(IN P_ADAPTER_T prAdapter);
VOID antenna_select_fsm_uninit(IN P_ADAPTER_T prAdapter);
VOID antenna_select_fsm(IN P_ADAPTER_T prAdapter,
			IN enum antenna_select_state next_state);
VOID antenna_select_scan_request(IN P_ADAPTER_T prAdapter,
				 IN P_MSG_SCN_SCAN_REQ_V2 prScanReqMsg);
int antenna_select_query_probe_resp_avg_rssi(IN P_ADAPTER_T prAdapter,
					     IN P_BSS_DESC_T prBssDesc);
WLAN_STATUS antenna_select_update_config(IN P_ADAPTER_T prAdapter,
					 bool is_oid);
VOID antenna_select_query_done(IN P_ADAPTER_T prAdapter, bool abort);
VOID antenna_select_scan_done(IN P_ADAPTER_T prAdapter);
VOID antenna_select_abort(IN P_ADAPTER_T prAdapter);
VOID antenna_select_query_rssi(IN P_ADAPTER_T prAdapter);
int antenna_select_start(IN P_ADAPTER_T prAdapter, IN PARAM_RSSI *ant_avg_rssi);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _ANT_SELECT_H */
