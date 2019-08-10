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

	All MAC80211/CFG80211 Related Structure & Definition.

***************************************************************************/
#ifndef __CFG80211_H__
#define __CFG80211_H__

#ifdef RT_CFG80211_SUPPORT

#include <linux/ieee80211.h>

extern UCHAR Cfg80211_Chan[];
extern const int Num_Cfg80211_Chan;

#define RT_REG_RULE(regr, start, end, bw, gain, eirp, reg_flags) \
	regr.freq_range.start_freq_khz = MHZ_TO_KHZ(start);	\
	regr.freq_range.end_freq_khz = MHZ_TO_KHZ(end);	\
	regr.freq_range.max_bandwidth_khz = MHZ_TO_KHZ(bw);	\
	regr.power_rule.max_antenna_gain = DBI_TO_MBI(gain);\
	regr.power_rule.max_eirp = DBM_TO_MBM(eirp);	\
	regr.flags = reg_flags;

typedef enum _NDIS_HOSTAPD_STATUS {
	Hostapd_Disable = 0,
	Hostapd_EXT,
	Hostapd_CFG
} NDIS_HOSTAPD_STATUS, *PNDIS_HOSTAPD_STATUS;

struct _CH_FLAGS_BEACON {
	UCHAR ch;
	UINT32 flags;
};

typedef struct __CFG80211_CB {

	/* we can change channel/rate information on the fly so we backup them */
	struct ieee80211_supported_band Cfg80211_bands[IEEE80211_NUM_BANDS];
	struct ieee80211_channel *pCfg80211_Channels;
	struct ieee80211_rate *pCfg80211_Rates;

	/* used in wiphy_unregister */
	struct wireless_dev *pCfg80211_Wdev;

	/* used in scan end */
	struct cfg80211_scan_request *pCfg80211_ScanReq;

	/* monitor filter */
	UINT32 MonFilterFlag;

	struct _CH_FLAGS_BEACON *ch_flags_by_beacon;
	/* channel information */
	struct ieee80211_channel ChanInfo[MAX_NUM_OF_CHANNELS];

	/* to protect scan status */
	spinlock_t scan_notify_lock;

} CFG80211_CB;

/*
========================================================================
Routine Description:
	Register MAC80211 Module.

Arguments:
	pAd				- WLAN control block pointer
	pDev			- Generic device interface
	pNetDev			- Network device

Return Value:
	NONE

Note:
	pDev != pNetDev
	#define SET_NETDEV_DEV(net, pdev)	((net)->dev.parent = (pdev))

	Can not use pNetDev to replace pDev; Or kernel panic.
========================================================================
*/
BOOLEAN CFG80211_Register(VOID *pAd, struct device *pDev, struct net_device *pNetDev);

#endif /* RT_CFG80211_SUPPORT */

#endif /* __CFG80211_H__ */

/* End of cfg80211.h */
