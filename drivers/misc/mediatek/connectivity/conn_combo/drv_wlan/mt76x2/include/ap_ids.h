/****************************************************************************
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
 ****************************************************************************

     Module Name:
     ap_ids.c

     Abstract:
     IDS definition

     Revision History:
     Who         When          What
     --------    ----------    ----------------------------------------------
 */

VOID RTMPIdsPeriodicExec(IN PVOID SystemSpecific1,
			 IN PVOID FunctionContext,
			 IN PVOID SystemSpecific2, IN PVOID SystemSpecific3);

BOOLEAN RTMPSpoofedMgmtDetection(IN RTMP_ADAPTER *pAd,
				 IN HEADER_802_11 *pHeader, IN RX_BLK *rxblk);

VOID RTMPConflictSsidDetection(IN PRTMP_ADAPTER pAd,
			       IN PUCHAR pSsid,
			       IN UCHAR SsidLen, IN CHAR Rssi0, IN CHAR Rssi1, IN CHAR Rssi2);

BOOLEAN RTMPReplayAttackDetection(IN RTMP_ADAPTER *pAd, IN UCHAR *pAddr2, IN RX_BLK *rxblk);

VOID RTMPUpdateStaMgmtCounter(IN PRTMP_ADAPTER pAd, IN USHORT type);

VOID RTMPClearAllIdsCounter(IN PRTMP_ADAPTER pAd);

VOID RTMPIdsStart(IN PRTMP_ADAPTER pAd);

VOID RTMPIdsStop(IN PRTMP_ADAPTER pAd);

VOID rtmp_read_ids_from_file(IN PRTMP_ADAPTER pAd, char *tmpbuf, char *buffer);
