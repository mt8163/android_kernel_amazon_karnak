/*
** Id:
*/

/*! \file   "antenna_select.c"
    \brief  This file defines the FSM for Antenna selection MODULE.

    This file defines the FSM for Antenna selection MODULE.
*/

/*
** Log: antenna_select.c
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"

#ifdef CONFIG_MTK_WIFI_ANTENNA_SELECT
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
static int moving_mode_to_alpha_base[] = {4, 3, 2, 0}; /* 1/16, 1/8, 1/4, 1 */

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

VOID antenna_select_fsm_init(IN P_ADAPTER_T prAdapter)
{
	struct antenna_select_info *ant_sel;
	struct antenna_select_param *params;
	P_CONNECTION_SETTINGS_T prConnSettings;

	ant_sel = &(prAdapter->rWifiVar.ant_select_info);
	params = &ant_sel->params;

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	ant_sel->curr_state = ANT_SEL_STATE_IDLE;
	ant_sel->ch_dwell_time = DEFAULT_ANTENNA_SELECT_DWELL_TIME;
	params->query_time = DEFAULT_ANTENNA_SELECT_QUERY_TIME;
}

VOID antenna_select_fsm_uninit(IN P_ADAPTER_T prAdapter)
{
	struct antenna_select_info *ant_sel;
	struct antenna_select_param *params;

	ant_sel = &(prAdapter->rWifiVar.ant_select_info);
	params = &ant_sel->params;

	ant_sel->curr_state = ANT_SEL_STATE_IDLE;
	ant_sel->ch_dwell_time = DEFAULT_ANTENNA_SELECT_DWELL_TIME;
	params->query_time = DEFAULT_ANTENNA_SELECT_QUERY_TIME;
}

VOID antenna_select_fsm(IN P_ADAPTER_T prAdapter,
			IN enum antenna_select_state next_state)
{
	enum antenna_select_state ePreviousState;
	BOOLEAN fgIsTransition = (BOOLEAN) FALSE;
	struct antenna_select_info *ant_sel;
	struct antenna_select_param *params;
	OS_SYSTIME *ant_query_start_time;
	P_BSS_INFO_T prAisBssInfo;
	P_BSS_DESC_T prBssDesc = NULL;

	ant_sel = &(prAdapter->rWifiVar.ant_select_info);
	params = &ant_sel->params;
	ant_query_start_time = &ant_sel->ant_query_start_time;

	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prBssDesc = scanSearchBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);

	do {
		DBGLOG(ANT, STATE, "[%d] TRANSITION: [%d] -> [%d]\n",
		       DBG_ANT_IDX, ant_sel->curr_state, next_state);

		ePreviousState = ant_sel->curr_state;
		ant_sel->curr_state = next_state;

		fgIsTransition = (BOOLEAN) FALSE;

		/* Do tasks of the State that we just entered */
		switch (ant_sel->curr_state) {
		case ANT_SEL_STATE_IDLE:
			break;
		case ANT_SEL_STATE_START:
			aisFsmRunEventAntSelectQuery(prAdapter);
			break;
		case ANT_SEL_STATE_QUERY:
			if (!(*ant_query_start_time)) {
				if (prBssDesc)
					prBssDesc->ucAvgRCPI = 0;
				GET_CURRENT_SYSTIME(ant_query_start_time);

				antennaSwitch(prAdapter,
					      params->ant_num,
					      FALSE);

				DBGLOG(ANT, INFO,
				       "Antenna query start time = %u\n",
				       *ant_query_start_time);
			}
			break;
		default:
			ASSERT(0);
		}
	} while (fgIsTransition);
}

VOID antenna_select_query_antenna_event(IN P_ADAPTER_T prAdapter,
					IN P_CMD_INFO_T prCmdInfo,
					IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	struct antenna_select_config *config;
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_SW_DBG_CTRL_T prCmdSwCtrl;
	P_BSS_INFO_T prAisBssInfo;
	P_BSS_DESC_T prBssDesc = NULL;
	void *info_buffer = prCmdInfo->pvInformationBuffer;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prBssDesc = scanSearchBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);
	prGlueInfo = prAdapter->prGlueInfo;

	if (info_buffer) {
		prCmdSwCtrl = (P_CMD_SW_DBG_CTRL_T) (pucEventBuf);

		u4QueryInfoLen = sizeof(PARAM_CUSTOM_SW_CTRL_STRUCT_T);

		config = (struct antenna_select_config *)info_buffer;

		config->ant_num = prCmdSwCtrl->u4Data;

		if (prBssDesc)
			config->rssi = RCPI_TO_dBm(prBssDesc->ucRCPI);

		DBGLOG(ANT, INFO, "Main Antenna = Ant_%u: Connect Rssi = %d\n",
		       config->ant_num, config->rssi);
	}

	if (prCmdInfo->fgIsOid)
		kalOidComplete(prGlueInfo,
			       prCmdInfo->fgSetQuery,
			       u4QueryInfoLen,
			       WLAN_STATUS_SUCCESS);
}

VOID antenna_select_query_antenna_timeout(IN P_ADAPTER_T prAdapter,
					  IN P_CMD_INFO_T prCmdInfo)
{
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	if (prCmdInfo->fgIsOid)
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery,
			       0,
			       WLAN_STATUS_FAILURE);
}

WLAN_STATUS antenna_select_update_config(IN P_ADAPTER_T prAdapter,
					 bool is_oid)
{
	struct antenna_select_info *ant_sel;
	P_GLUE_INFO_T prGlueInfo;
	CMD_SW_DBG_CTRL_T rCmdSwCtrl;

	ant_sel = &(prAdapter->rWifiVar.ant_select_info);
	prGlueInfo = prAdapter->prGlueInfo;

	rCmdSwCtrl.u4Id = 0xa0340000; /* Get current antenna number */
	rCmdSwCtrl.u4Data = 0;

	return wlanSendSetQueryCmd(prAdapter,
				   CMD_ID_SW_DBG_CTRL,
				   FALSE,
				   TRUE,
				   is_oid,
				   antenna_select_query_antenna_event,
				   antenna_select_query_antenna_timeout,
				   sizeof(CMD_SW_DBG_CTRL_T),
				   (PUINT_8)&rCmdSwCtrl,
				   (void *)&ant_sel->config,
				   sizeof(ant_sel->config));
}

int antenna_select_query_probe_resp_avg_rssi(IN P_ADAPTER_T prAdapter,
					     IN P_BSS_DESC_T prBssDesc)
{
	struct antenna_select_info *ant_sel;
	struct antenna_select_param *params;
	unsigned int alpha_base;

	ant_sel = &(prAdapter->rWifiVar.ant_select_info);
	params = &ant_sel->params;

	/* Get moving average alpha_base value by moving average mode */
	if (ant_sel->moving_avg_mode < ARRAY_SIZE(moving_mode_to_alpha_base))
		alpha_base =
			moving_mode_to_alpha_base[ant_sel->moving_avg_mode];
	else
		return -EINVAL;

	/*
	 * fomula: y(n)=(1-alpha)*y(n-1)+alpha*x(n), alpha<=1
	 *
	 *         y(n): New moving average RCPI.
	 *	   y(n-1): Last moving average RCPI.
	 *	   x(n): RCPI of probe response.
	 *	   alpha: 1/alpha_base.
	 */
	if (prBssDesc->ucAvgRCPI)
		prBssDesc->ucAvgRCPI = ((1 << alpha_base) - 1) *
			(prBssDesc->ucAvgRCPI >> alpha_base) +
			(prBssDesc->ucRCPI >> alpha_base);
	/* prBssDesc->ucAvgRCPI =
	   (((1 - 1 / ant_sel->alpha_base) * prBssDesc->ucAvgRCPI) +
	   (1 / ant_sel->alpha_base) * prBssDesc->ucRCPI); */
	else
		prBssDesc->ucAvgRCPI = prBssDesc->ucRCPI;

	if (ant_sel->curr_state == ANT_SEL_STATE_QUERY) {
		DBGLOG(ANT, INFO,
		       "Receive probe response : (%pM) ucAvgRCPI=%u at %u",
			prBssDesc->aucBSSID, prBssDesc->ucAvgRCPI,
			kalGetTimeTick());
	}

	return 0;
}

/* Scan request with current SSID and channel */
VOID antenna_select_scan_request(IN P_ADAPTER_T prAdapter,
				 IN P_MSG_SCN_SCAN_REQ_V2 prScanReqMsg)
{
	struct antenna_select_info *ant_sel;
	struct antenna_select_param *params;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prBssInfo;
	int i;

	ant_sel = &(prAdapter->rWifiVar.ant_select_info);

	if (ant_sel->curr_state != ANT_SEL_STATE_QUERY)
		return;

	params = &ant_sel->params;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	prBssInfo = &prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX];

	prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;
	prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_SPECIFIED;
	prScanReqMsg->ucSSIDNum = 4;

	/* Filled four ssid to send four probe request */
	for (i = 0; i < 4; i++)
		COPY_SSID(ant_sel->current_ssid[i].aucSsid,
			  ant_sel->current_ssid[i].u4SsidLen,
			  prConnSettings->aucSSID,
			  prConnSettings->ucSSIDLen);

	prScanReqMsg->prSsid = ant_sel->current_ssid;
	prScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
	prScanReqMsg->ucChannelListNum = 1;
	prScanReqMsg->arChnlInfoList[0].eBand = prBssInfo->eBand;
	prScanReqMsg->arChnlInfoList[0].ucChannelNum =
		prBssInfo->ucPrimaryChannel;
	prScanReqMsg->u2ProbeDelay = 0;
	prScanReqMsg->u2ChannelDwellTime = ant_sel->ch_dwell_time;

	DBGLOG(ANT, INFO,
	       "Antenna scan request: Band = %d, Ch = %d, Dwell = %u\n",
	       prBssInfo->eBand,
	       prBssInfo->ucPrimaryChannel,
	       ant_sel->ch_dwell_time);

}

VOID antenna_select_query_done(IN P_ADAPTER_T prAdapter, bool abort)
{
	struct antenna_select_info *ant_sel;
	P_BSS_INFO_T prAisBssInfo;
	P_BSS_DESC_T prBssDesc;
	OS_SYSTIME rCurrentTime;


	ant_sel = &(prAdapter->rWifiVar.ant_select_info);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);
	prBssDesc = scanSearchBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);

	GET_CURRENT_SYSTIME(&rCurrentTime);

	if (prBssDesc)
		*ant_sel->ant_avg_rssi = RCPI_TO_dBm(prBssDesc->ucAvgRCPI);
	else
		*ant_sel->ant_avg_rssi = 0;

	/* Restore main antenna */
	antennaSwitch(prAdapter, ant_sel->config.ant_num, FALSE);

	if (abort) {
		kalOidComplete(prAdapter->prGlueInfo,
			       0, 0, WLAN_STATUS_FAILURE);

		DBGLOG(ANT, ERROR, "Antennet Query Abort at %u\n",
		       rCurrentTime);
	} else {
		kalOidComplete(prAdapter->prGlueInfo,
			       0, 0, WLAN_STATUS_SUCCESS);

		DBGLOG(ANT, INFO, "Antenna Query done avg rssi =%d in %ums\n",
		       *ant_sel->ant_avg_rssi,
		       rCurrentTime - ant_sel->ant_query_start_time);
	}
}

VOID antenna_select_scan_done(IN P_ADAPTER_T prAdapter)
{
	struct antenna_select_info *ant_sel;
	struct antenna_select_param *params;
	enum antenna_select_state next_state;
	OS_SYSTIME rCurrentTime;

	ant_sel = &(prAdapter->rWifiVar.ant_select_info);
	params = &ant_sel->params;

	if (ant_sel->curr_state != ANT_SEL_STATE_QUERY)
		return;

	DBGLOG(ANT, INFO, "Antenna process scan done at %u\n",
	       kalGetTimeTick());

	/* If query doesn't timeout, toggle next scan request */
	GET_CURRENT_SYSTIME(&rCurrentTime);
	if (CHECK_FOR_TIMEOUT(rCurrentTime,
			      ant_sel->ant_query_start_time,
			      MSEC_TO_SYSTIME(params->query_time))) {
		next_state = ANT_SEL_STATE_IDLE;

		if (next_state != ant_sel->curr_state) {
			antenna_select_query_done(prAdapter, 0);
			antenna_select_fsm(prAdapter, next_state);
		}
	} else {
		DBGLOG(ANT, INFO, "Antenna next scan request\n");

		next_state = ANT_SEL_STATE_START;

		if (next_state != ant_sel->curr_state)
			antenna_select_fsm(prAdapter, next_state);
	}
	DBGLOG(ANT, INFO, "Antenna current query time %u\n",
	       rCurrentTime - ant_sel->ant_query_start_time);
}

VOID antenna_select_abort(IN P_ADAPTER_T prAdapter)
{
	struct antenna_select_info *ant_sel;
	enum antenna_select_state next_state;

	ant_sel = &(prAdapter->rWifiVar.ant_select_info);

	DBGLOG(ANT, TRACE, "Antenna Query Abort!\n");

	next_state = ANT_SEL_STATE_IDLE;

	if (next_state != ant_sel->curr_state) {
		antenna_select_query_done(prAdapter, 1);
		antenna_select_fsm(prAdapter, next_state);
	}
}

VOID antenna_select_query_rssi(IN P_ADAPTER_T prAdapter)
{
	struct antenna_select_info *ant_sel;
	enum antenna_select_state next_state;

	ant_sel = &(prAdapter->rWifiVar.ant_select_info);

	/* START to QUERY */
	if (ant_sel->curr_state != ANT_SEL_STATE_START)
		return;

	DBGLOG(ANT, INFO, "Antenna selection start query at %u\n",
	       kalGetTimeTick());

	next_state = ANT_SEL_STATE_QUERY;

	if (next_state != ant_sel->curr_state)
		antenna_select_fsm(prAdapter, next_state);
}

int antenna_select_start(IN P_ADAPTER_T prAdapter, IN PARAM_RSSI *ant_avg_rssi)
{
	struct antenna_select_info *ant_sel;
	enum antenna_select_state next_state;
	P_BSS_INFO_T prAisBssInfo;

	ant_sel = &(prAdapter->rWifiVar.ant_select_info);
	prAisBssInfo = &(prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX]);

	/* IDLE to START and must under connecting state */
	if (ant_sel->curr_state != ANT_SEL_STATE_IDLE ||
	   prAisBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED) {
		DBGLOG(ANT, ERROR, "Antenna selection start fail\n");
		return -EFAULT;
	}

	DBGLOG(ANT, INFO, "Antenna selection start at %u\n", kalGetTimeTick());

	next_state = ANT_SEL_STATE_START;

	if (next_state != ant_sel->curr_state) {
		ant_sel->ant_avg_rssi = ant_avg_rssi;
		*ant_sel->ant_avg_rssi = 0;
		ant_sel->ant_query_start_time = 0;
		antenna_select_fsm(prAdapter, next_state);
	}

	return 0;
}
#endif /* CONFIG_MTK_WIFI_ANTENNA_SELECT */
