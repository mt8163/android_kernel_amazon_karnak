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

    Abstract:

 */

#include "ap_autoChSel_cmm.h"

#ifndef __AUTOCHSELECT_H__
#define __AUTOCHSELECT_H__

#define AP_AUTO_CH_SEL(__P, __O)	APAutoSelectChannel((__P), (__O))

ULONG AutoChBssSearchWithSSID(IN PRTMP_ADAPTER pAd,
			      IN PUCHAR Bssid, IN PUCHAR pSsid, IN UCHAR SsidLen, IN UCHAR Channel);

VOID APAutoChannelInit(IN PRTMP_ADAPTER pAd);

VOID UpdateChannelInfo(IN PRTMP_ADAPTER pAd, IN int ch, IN ChannelSel_Alg Alg);

#ifdef SUPPORT_ACS_BY_SCAN
VOID AutoChBssTableUpdateByScanTab(IN PRTMP_ADAPTER pAd);
#endif /* endif */

ULONG AutoChBssInsertEntry(IN PRTMP_ADAPTER pAd,
			   IN PUCHAR pBssid,
			   IN CHAR Ssid[],
			   IN UCHAR SsidLen,
			   IN UCHAR ChannelNo, IN UCHAR ExtChOffset, IN CHAR Rssi);

VOID AutoChBssTableInit(IN PRTMP_ADAPTER pAd);

VOID ChannelInfoInit(IN PRTMP_ADAPTER pAd);

VOID AutoChBssTableDestroy(IN PRTMP_ADAPTER pAd);

VOID ChannelInfoDestroy(IN PRTMP_ADAPTER pAd);

VOID CheckPhyModeIsABand(IN PRTMP_ADAPTER pAd);

UCHAR SelectBestChannel(IN PRTMP_ADAPTER pAd, IN ChannelSel_Alg Alg);

UCHAR APAutoSelectChannel(IN PRTMP_ADAPTER pAd, IN ChannelSel_Alg Alg);

INT32 ACS_PerformAlgorithm(RTMP_ADAPTER *pAd, ChannelSel_Alg Alg);

#endif /* __AUTOCHSELECT_H__ */
