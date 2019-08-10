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

	Abstract:

	Revision History:
	Who		When			What
	--------	----------		----------------------------------------------
*/

#define RTMP_MODULE_OS

#ifdef RT_CFG80211_SUPPORT

#include "rt_config.h"

extern UCHAR CFG_P2POUIBYTE[];

VOID CFG80211_Convert802_3Packet(RTMP_ADAPTER *pAd, RX_BLK *pRxBlk, UCHAR *pHeader802_3)
{
#ifdef CONFIG_AP_SUPPORT
	if (IS_PKT_OPMODE_AP(pRxBlk)) {
		RTMP_AP_802_11_REMOVE_LLC_AND_CONVERT_TO_802_3(pRxBlk, pHeader802_3);
	} else
#endif /* CONFIG_AP_SUPPORT */
	{
#ifdef CONFIG_STA_SUPPORT
		RTMP_802_11_REMOVE_LLC_AND_CONVERT_TO_802_3(pRxBlk, pHeader802_3);
#endif /*CONFIG_STA_SUPPORT */
	}
}

VOID CFG80211_Announce802_3Packet(RTMP_ADAPTER *pAd, RX_BLK *pRxBlk, UCHAR FromWhichBSSID)
{
#ifdef CONFIG_AP_SUPPORT
	if (IS_PKT_OPMODE_AP(pRxBlk)) {
		AP_ANNOUNCE_OR_FORWARD_802_3_PACKET(pAd, pRxBlk->pRxPacket, FromWhichBSSID);
	} else
#endif /* CONFIG_AP_SUPPORT */
	{
#ifdef CONFIG_STA_SUPPORT
		ANNOUNCE_OR_FORWARD_802_3_PACKET(pAd, pRxBlk->pRxPacket, FromWhichBSSID);
#endif /*CONFIG_STA_SUPPORT */
	}

}

ACTION_FRAME_TYPE CFG80211_CheckActionFrameType(IN RTMP_ADAPTER *pAd,
						IN PUCHAR preStr, IN PUCHAR pData, IN UINT32 length)
{
	ACTION_FRAME_TYPE actionFrame = NONE_ACTION_FRAME_TYPE;
	struct ieee80211_mgmt *mgmt;
	mgmt = (struct ieee80211_mgmt *)pData;
	if (ieee80211_is_mgmt(mgmt->frame_control)) {
		if (ieee80211_is_probe_resp(mgmt->frame_control)) {
			DBGPRINT(RT_DEBUG_INFO,
				 ("CFG80211_PKT: %s ProbeRsp Frame %d\n", preStr,
				  pAd->LatchRfRegs.Channel));
			if (!mgmt->u.probe_resp.timestamp) {
				struct timeval tv;
				do_gettimeofday(&tv);
				mgmt->u.probe_resp.timestamp =
				    ((UINT64) tv.tv_sec * 1000000) + tv.tv_usec;
			}
		} else if (ieee80211_is_disassoc(mgmt->frame_control)) {
			DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: %s DISASSOC Frame\n", preStr));
		} else if (ieee80211_is_deauth(mgmt->frame_control)) {
			DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: %s Deauth Frame\n", preStr));
		} else if (ieee80211_is_action(mgmt->frame_control)) {
			PP2P_PUBLIC_FRAME pFrame = (PP2P_PUBLIC_FRAME) pData;
			if ((pFrame->p80211Header.FC.SubType == SUBTYPE_ACTION) &&
			    (pFrame->Category == CATEGORY_PUBLIC) &&
			    (pFrame->Action == ACTION_WIFI_DIRECT)) {
				actionFrame = PUBLIC_ACTION_FRAME_TYPE;
				switch (pFrame->Subtype) {
				case GO_NEGOCIATION_REQ:
					DBGPRINT(RT_DEBUG_ERROR,
						 ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s GO_NEGOCIACTION_REQ %d\n",
						  PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da), preStr,
						  pAd->LatchRfRegs.Channel));
					break;

				case GO_NEGOCIATION_RSP:
					DBGPRINT(RT_DEBUG_ERROR,
						 ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s GO_NEGOCIACTION_RSP %d\n",
						  PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da), preStr,
						  pAd->LatchRfRegs.Channel));
					break;

				case GO_NEGOCIATION_CONFIRM:
					DBGPRINT(RT_DEBUG_ERROR,
						 ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s GO_NEGOCIACTION_CONFIRM %d\n",
						  PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da), preStr,
						  pAd->LatchRfRegs.Channel));
					break;

				case P2P_PROVISION_REQ:
					DBGPRINT(RT_DEBUG_ERROR,
						 ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2P_PROVISION_REQ %d\n",
						  PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da), preStr,
						  pAd->LatchRfRegs.Channel));
					break;

				case P2P_PROVISION_RSP:
					DBGPRINT(RT_DEBUG_ERROR,
						 ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2P_PROVISION_RSP %d\n",
						  PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da), preStr,
						  pAd->LatchRfRegs.Channel));
					break;

				case P2P_INVITE_REQ:
					DBGPRINT(RT_DEBUG_ERROR,
						 ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2P_INVITE_REQ %d\n",
						  PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da), preStr,
						  pAd->LatchRfRegs.Channel));
					break;

				case P2P_INVITE_RSP:
					DBGPRINT(RT_DEBUG_ERROR,
						 ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2P_INVITE_RSP %d\n",
						  PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da), preStr,
						  pAd->LatchRfRegs.Channel));
					break;
				case P2P_DEV_DIS_REQ:
					DBGPRINT(RT_DEBUG_ERROR,
						 ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2P_DEV_DIS_REQ %d\n",
						  PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da), preStr,
						  pAd->LatchRfRegs.Channel));
					break;
				case P2P_DEV_DIS_RSP:
					DBGPRINT(RT_DEBUG_ERROR,
						 ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2P_DEV_DIS_RSP %d\n",
						  PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da), preStr,
						  pAd->LatchRfRegs.Channel));
					break;
				}
			} else if ((pFrame->p80211Header.FC.SubType == SUBTYPE_ACTION) &&
				   (pFrame->Category == CATEGORY_PUBLIC) &&
				   ((pFrame->Action == ACTION_GAS_INITIAL_REQ) ||
				    (pFrame->Action == ACTION_GAS_INITIAL_RSP) ||
				    (pFrame->Action == ACTION_GAS_COMEBACK_REQ) ||
				    (pFrame->Action == ACTION_GAS_COMEBACK_RSP))) {
				actionFrame = PUBLIC_ACTION_FRAME_TYPE;
			} else if (pFrame->Category == CATEGORY_VENDOR_SPECIFIC_WFD) {
				PP2P_ACTION_FRAME pP2PActionFrame = (PP2P_ACTION_FRAME) pData;
				if (RTMPEqualMemory(&pP2PActionFrame->Octet[2], CFG_P2POUIBYTE, 4)) {
					actionFrame = P2P_ACTION_FRAME_TYPE;
					switch (pP2PActionFrame->Subtype) {
					case P2PACT_NOA:
						DBGPRINT(RT_DEBUG_TRACE,
							 ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2PACT_NOA %d\n",
							  PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da),
							  preStr, pAd->LatchRfRegs.Channel));
						break;
					case P2PACT_PERSENCE_REQ:
						DBGPRINT(RT_DEBUG_ERROR,
							 ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2PACT_PERSENCE_REQ %d\n",
							  PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da),
							  preStr, pAd->LatchRfRegs.Channel));
						break;
					case P2PACT_PERSENCE_RSP:
						DBGPRINT(RT_DEBUG_ERROR,
							 ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2PACT_PERSENCE_RSP %d\n",
							  PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da),
							  preStr, pAd->LatchRfRegs.Channel));
						break;
					case P2PACT_GO_DISCOVER_REQ:
						DBGPRINT(RT_DEBUG_ERROR,
							 ("CFG80211_PKT: (%02X:%02X:%02X:%02X:%02X:%02X) -> (%02X:%02X:%02X:%02X:%02X:%02X) %s P2PACT_GO_DISCOVER_REQ %d\n",
							  PRINT_MAC(mgmt->sa), PRINT_MAC(mgmt->da),
							  preStr, pAd->LatchRfRegs.Channel));
						break;
					}
				}
			} else {
				DBGPRINT(RT_DEBUG_INFO,
					 ("CFG80211_PKT: %s ACTION Frame with Channel%d\n", preStr,
					  pAd->LatchRfRegs.Channel));
			}
		} else {
			DBGPRINT(RT_DEBUG_ERROR,
				 ("CFG80211_PKT: %s UNKOWN MGMT FRAME TYPE\n", preStr));
		}
	} else {
		DBGPRINT(RT_DEBUG_ERROR, ("CFG80211_PKT: %s UNKOWN FRAME TYPE\n", preStr));
	}

	return actionFrame;
}

BOOLEAN CFG80211_HandleP2pMgmtFrame(RTMP_ADAPTER *pAd, RX_BLK *pRxBlk, UCHAR OpMode)
{
	RXWI_STRUCT *pRxWI = pRxBlk->pRxWI;
	PHEADER_802_11 pHeader = pRxBlk->pHeader;
	PCFG80211_CTRL pCfg80211_ctrl = &pAd->cfg80211_ctrl;
	UINT32 freq;

	if ((pHeader->FC.SubType == SUBTYPE_PROBE_REQ) ||
	    ((pHeader->FC.SubType == SUBTYPE_ACTION) &&
	     CFG80211_CheckActionFrameType(pAd, "RX", (PUCHAR) pHeader,
					   pRxWI->RXWI_N.MPDUtotalByteCnt))) {
		MAP_CHANNEL_ID_TO_KHZ(RTMP_GetPrimaryCh(pAd, pAd->LatchRfRegs.Channel), freq);
		freq /= 1000;

#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
		/* Check the P2P_GO exist in the VIF List */
		if (pCfg80211_ctrl->Cfg80211VifDevSet.vifDevList.size > 0) {
			PNET_DEV pNetDev = NULL;
			BOOLEAN res = FALSE;
			pNetDev = RTMP_CFG80211_FindVifEntry_ByType(pAd,
							       RT_CMD_80211_IFTYPE_P2P_GO);
			if (pNetDev != NULL) {
				DBGPRINT(RT_DEBUG_TRACE,
					 ("VIF STA GO RtmpOsCFG80211RxMgmt OK!!\n"));
				DBGPRINT(RT_DEBUG_TRACE,
					 ("TYPE = %d, freq = %d, %02x:%02x:%02x:%02x:%02x:%02x, seq (%d)\n",
						pHeader->FC.SubType, freq,
						PRINT_MAC(pHeader->Addr2), pHeader->Sequence));

				if ((RTMP_GetPrimaryCh(pAd, pAd->LatchRfRegs.Channel) == pAd->CommonCfg.Channel)
					|| (pAd->CommonCfg.Channel == 0)) {
					if (pHeader->FC.SubType == SUBTYPE_ACTION) {
						if (CFG80211_CheckActionFrameType
							(pAd, "CHECK", (PUCHAR)pHeader,
							pRxWI->RXWI_N.MPDUtotalByteCnt) ==
							P2P_ACTION_FRAME_TYPE)
							res =
							CFG80211OS_RxMgmt(pNetDev, freq,
							(PUCHAR)pHeader,
							pRxWI->RXWI_N.
							MPDUtotalByteCnt);
						else
							res =
							CFG80211OS_RxMgmt(CFG80211_GetEventDevice(pAd),
							freq, (PUCHAR)pHeader,
							pRxWI->RXWI_N.
							MPDUtotalByteCnt);

						if (res != TRUE)
							DBGPRINT(RT_DEBUG_ERROR,
							(" SUBTYPE_ACTION CFG80211OS_RxMgmt failed! (%02x:%02x:%02x:%02x:%02x:%02x)\n",
							PRINT_MAC(pHeader->Addr2)));
					} else if (pHeader->FC.SubType == SUBTYPE_PROBE_REQ)
						res =
						CFG80211OS_RxMgmt(pNetDev, freq, (PUCHAR)pHeader,
						pRxWI->RXWI_N.MPDUtotalByteCnt);

					if (res != TRUE)
						DBGPRINT(RT_DEBUG_TRACE,
						("CFG80211OS_RxMgmt failed! (%02x:%02x:%02x:%02x:%02x:%02x)\n",
						PRINT_MAC(pHeader->Addr2)));
				} else {
					DBGPRINT(RT_DEBUG_TRACE,
						("Receive frame not from the P2P operation channel, ignore!!\n"));
					DBGPRINT(RT_DEBUG_TRACE,
						("RX Frame Chan=%d, P2P Chan=%d\n",
						pAd->LatchRfRegs.Channel, pAd->CommonCfg.Channel));
				}
				if (OpMode == OPMODE_AP)
					return TRUE;
			}
		}
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */

		if (((pHeader->FC.SubType == SUBTYPE_PROBE_REQ) &&
		     (pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterProbeReqFrame == TRUE)) ||
		    ((pHeader->FC.SubType ==
		      SUBTYPE_ACTION) /*&& ( pAd->Cfg80211RegisterActionFrame == TRUE) */)) {
			DBGPRINT(RT_DEBUG_TRACE, ("MAIN STA RtmpOsCFG80211RxMgmt OK!!"));
			DBGPRINT(RT_DEBUG_TRACE,
				 ("TYPE = %d, freq = %d, %02x:%02x:%02x:%02x:%02x:%02x\n",
					pHeader->FC.SubType, freq, PRINT_MAC(pHeader->Addr2)));
			if (CFG80211_GetEventDevice(pAd) == NULL) {	/* Fix crash problem when hostapd boot up. */
				DBGPRINT(RT_DEBUG_ERROR, ("Not Ready for p2p0 netdevice\n"));
				return FALSE;
			} else {
				if (RTMP_CFG80211_HOSTAPD_ON(pAd))
					CFG80211OS_RxMgmt(pAd->net_dev, freq, (PUCHAR) pHeader,
							  pRxWI->RXWI_N.MPDUtotalByteCnt);
				else
					CFG80211OS_RxMgmt(CFG80211_GetEventDevice(pAd), freq,
							  (PUCHAR) pHeader,
							  pRxWI->RXWI_N.MPDUtotalByteCnt);
			}

			if (OpMode == OPMODE_AP)
				return TRUE;
		}
	}

	return FALSE;
}

#endif /* RT_CFG80211_SUPPORT */
