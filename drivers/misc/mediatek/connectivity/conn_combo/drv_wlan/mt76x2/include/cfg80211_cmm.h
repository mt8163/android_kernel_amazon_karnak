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
 ***************************************************************************/

/****************************************************************************

	Abstract:

	All CFG80211 Function Prototype.

***************************************************************************/

#ifndef __CFG80211CMM_H__
#define __CFG80211CMM_H__

#ifdef RT_CFG80211_SUPPORT

#define RTMP_CFG80211_HOSTAPD_ON(__pAd) (__pAd->net_dev->ieee80211_ptr->iftype == RT_CMD_80211_IFTYPE_AP)

#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
#define CFG80211_GetEventDevice(__pAd) __pAd->cfg80211_ctrl.dummy_p2p_net_dev
#else
#define CFG80211_GetEventDevice(__pAd) __pAd->net_dev
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */


#define RESTORE_CH_TIME 200
#define MAX_ROC_TIME 500

#ifdef RT_CFG80211_P2P_SUPPORT
typedef struct _P2PCLIENT_NOA_SCHEDULE {
	BOOLEAN bValid;
	BOOLEAN bInAwake;
	BOOLEAN bNeedResumeNoA;	/* Set to TRUE if need to restart infinite NoA */
	BOOLEAN bWMMPSInAbsent;	/* Set to TRUE if enter GO absent period by supported UAPSD GO */
	UCHAR Token;
	ULONG SwTimerTickCounter;	/* this Counter os used for sw-base NoA implementation tick counter */
	ULONG CurrentTargetTimePoint;	/* For sw-base method NoA usage */
	ULONG NextTargetTimePoint;
	ULONG NextTimePointForWMMPSCounting;	/* fro counting WMM PS EOSP bit. Not used for NoA implementation. */
	UCHAR Count;
	ULONG Duration;
	ULONG Interval;
	ULONG StartTime;
	ULONG OngoingAwakeTime;	/* this time will keep increasing as time go by. indecate the current awake time point */
	ULONG TsfHighByte;
	ULONG ThreToWrapAround;
	ULONG LastBeaconTimeStamp;
} P2PCLIENT_NOA_SCHEDULE, *PP2PCLIENT_NOA_SCHEDULE;

typedef struct {

	UCHAR Eid;

	UCHAR Len[2];

	CHAR Octet[1];

} P2PEID_STRUCT, *PP2PEID_STRUCT;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0))
struct ieee80211_vendor_ie {
	u8 element_id;
	u8 len;
	u8 oui[3];
	u8 oui_type;
} __packed;
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0) */

VOID CFG80211_P2PCTWindowTimer(IN PVOID SystemSpecific1,
			       IN PVOID FunctionContext,
			       IN PVOID SystemSpecific2, IN PVOID SystemSpecific3);

VOID CFG80211_P2pSwNoATimeOut(IN PVOID SystemSpecific1,
			      IN PVOID FunctionContext,
			      IN PVOID SystemSpecific2, IN PVOID SystemSpecific3);

VOID CFG80211_P2pPreAbsenTimeOut(IN PVOID SystemSpecific1,
				 IN PVOID FunctionContext,
				 IN PVOID SystemSpecific2, IN PVOID SystemSpecific3);

#endif /* RT_CFG80211_P2P_SUPPORT */

/* yiwei debug for P2P 7.1.3 */
#ifdef RT_CFG80211_P2P_SUPPORT

/* yiwei tmp hard code */
#define IS_SW_NOA_TIMER(_A) (1)

#define P2P_OPPS_BIT		0x80

/*
 *  Macros for bit check
*/

#define CFG80211_P2P_TEST_BIT(_M, _F)      (((_M) & (_F)) != 0)

#define CFG_P2P_DISABLE	0x00000000
#define CFG_P2P_GO_UP		0x00000001
#define CFG_P2P_CLI_UP		0x00000002

#define IS_CFG80211_P2P_ABSENCE(_A)	(((_A)->cfg80211_ctrl.bPreKeepSlient) || ((_A)->cfg80211_ctrl.bKeepSlient))

typedef struct __CFG_P2P_ENTRY_PARM {
	UCHAR CTWindow;		/* As GO, Store client's Presence request NoA.  As Client, Store GO's NoA In beacon or P2P Action frame */
	P2PCLIENT_NOA_SCHEDULE NoADesc[1];	/* As GO, Store client's Presence request NoA.  As Client, Store GO's NoA In beacon or P2P Action frame */
} CFG_P2P_ENTRY_PARM, *PCFG_P2P_ENTRY_PARM;

VOID CFG80211_P2PCTWindowTimer(IN PVOID SystemSpecific1,
			       IN PVOID FunctionContext,
			       IN PVOID SystemSpecific2, IN PVOID SystemSpecific3);

VOID CFG80211_P2pSwNoATimeOut(IN PVOID SystemSpecific1,
			      IN PVOID FunctionContext,
			      IN PVOID SystemSpecific2, IN PVOID SystemSpecific3);

VOID CFG80211_P2pPreAbsenTimeOut(IN PVOID SystemSpecific1,
				 IN PVOID FunctionContext,
				 IN PVOID SystemSpecific2, IN PVOID SystemSpecific3);

#endif /* RT_CFG80211_P2P_SUPPORT */

#endif /* RT_CFG80211_SUPPORT */

#endif /* __CFG80211CMM_H__ */
