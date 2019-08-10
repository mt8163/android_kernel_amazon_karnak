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
    sta.h

    Abstract:
    Miniport generic portion header file

    Revision History:
    Who         When          What
    --------    ----------    ----------------------------------------------
*/

#ifndef __STA_H__
#define __STA_H__

BOOLEAN RTMPCheckChannel(IN PRTMP_ADAPTER pAd, IN UCHAR CentralChannel, IN UCHAR Channel);

VOID InitChannelRelatedValue(IN PRTMP_ADAPTER pAd);

VOID AdjustChannelRelatedValue(IN PRTMP_ADAPTER pAd,
			       OUT UCHAR *pBwFallBack,
			       IN USHORT ifIndex,
			       IN BOOLEAN BandWidth, IN UCHAR PriCh, IN UCHAR ExtraCh);

VOID RTMPReportMicError(IN PRTMP_ADAPTER pAd, IN PCIPHER_KEY pWpaKey);

INT RTMPCheckRxError(IN RTMP_ADAPTER *pAd,
		     IN HEADER_802_11 *pHeader, IN RX_BLK *pRxBlk, IN RXINFO_STRUCT *pRxInfo);

VOID WpaMicFailureReportFrame(IN PRTMP_ADAPTER pAd, IN MLME_QUEUE_ELEM *Elem);

VOID WpaDisassocApAndBlockAssoc(IN PVOID SystemSpecific1,
				IN PVOID FunctionContext,
				IN PVOID SystemSpecific2, IN PVOID SystemSpecific3);

VOID WpaStaPairwiseKeySetting(IN PRTMP_ADAPTER pAd);

VOID WpaStaGroupKeySetting(IN PRTMP_ADAPTER pAd);

VOID WpaSendEapolStart(IN PRTMP_ADAPTER pAdapter, IN PUCHAR pBssid);

BOOLEAN STACheckTkipMICValue(IN PRTMP_ADAPTER pAd, IN MAC_TABLE_ENTRY *pEntry, IN RX_BLK *pRxBlk);

VOID STAHandleRxDataFrame(RTMP_ADAPTER *pAd, RX_BLK *pRxBlk);

VOID STARxDataFrameAnnounce(IN PRTMP_ADAPTER pAd,
			    IN MAC_TABLE_ENTRY *pEntry,
			    IN RX_BLK *pRxBlk, IN UCHAR FromWhichBSSID);

VOID STARxEAPOLFrameIndicate(IN PRTMP_ADAPTER pAd,
			     IN MAC_TABLE_ENTRY *pEntry,
			     IN RX_BLK *pRxBlk, IN UCHAR FromWhichBSSID);

NDIS_STATUS STAHardTransmit(IN PRTMP_ADAPTER pAd, IN TX_BLK *pTxBlk, IN UCHAR QueIdx);

INT STASendPacket(IN RTMP_ADAPTER *pAd, IN PNDIS_PACKET pPacket);

INT STAInitialize(RTMP_ADAPTER *pAd);

VOID RTMPHandleTwakeupInterrupt(IN PRTMP_ADAPTER pAd);

#endif /* __STA_H__ */
