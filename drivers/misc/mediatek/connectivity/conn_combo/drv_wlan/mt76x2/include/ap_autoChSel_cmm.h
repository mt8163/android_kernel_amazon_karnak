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

#ifndef __AUTOCHSELECT_CMM_H__
#define __AUTOCHSELECT_CMM_H__

#define RSSI_TO_DBM_OFFSET          120	/* RSSI-115 = dBm */
#define ACS_DIRTINESS_RSSI_STRONG   (-50)	/* dBm */
#define ACS_DIRTINESS_RSSI_WEAK     (-80)	/* dBm */
#define ACS_FALSECCA_THRESHOLD      (100)

typedef struct {
	ULONG dirtyness[MAX_NUM_OF_CHANNELS + 1];
	ULONG ApCnt[MAX_NUM_OF_CHANNELS + 1];
	UINT32 FalseCCA[MAX_NUM_OF_CHANNELS + 1];
	BOOLEAN SkipList[MAX_NUM_OF_CHANNELS + 1];
#ifdef AP_QLOAD_SUPPORT
	UINT32 chanbusytime[MAX_NUM_OF_CHANNELS + 1];	/* QLOAD ALARM */
#endif				/* AP_QLOAD_SUPPORT */
	BOOLEAN IsABand;
} CHANNELINFO, *PCHANNELINFO;

typedef struct {
	UCHAR Bssid[MAC_ADDR_LEN];
	UCHAR SsidLen;
	CHAR Ssid[MAX_LEN_OF_SSID];
	UCHAR Channel;
	UCHAR ExtChOffset;
	CHAR Rssi;
} BSSENTRY, *PBSSENTRY;

typedef struct {
	UCHAR BssNr;
	BSSENTRY BssEntry[MAX_LEN_OF_BSS_TABLE];
} BSSINFO, *PBSSINFO;

typedef struct {
	UCHAR ch;
	ULONG dirtyness;
	ULONG falseCCA;
} ACS_SORT_ENTRY;

typedef enum ChannelSelAlg {
	ChannelAlgCombined,	/* Default */
	ChannelAlgApCnt,
	ChannelAlgCCA,
	ChannelAlgRandom	/*use by Dfs */
} ChannelSel_Alg;

#endif /* __AUTOCHSELECT_CMM_H__ */
