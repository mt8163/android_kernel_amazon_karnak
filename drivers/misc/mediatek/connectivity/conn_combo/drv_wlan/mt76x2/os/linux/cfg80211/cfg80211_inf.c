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
	cfg80211_inf.c

	Abstract:

	Revision History:
	Who		When			What
	--------	----------		----------------------------------------------
	YF Luo		06-28-2012		Init version
			12-26-2013		Integration of NXTC
*/
#define RTMP_MODULE_OS

#include "rt_config.h"
#include "rtmp_comm.h"
#include "rt_os_util.h"
#include "rt_os_net.h"

#ifdef RT_CFG80211_SUPPORT

extern INT ApCliAllowToSendPacket(RTMP_ADAPTER *pAd, struct wifi_dev *wdev,
				  PNDIS_PACKET pPacket, UCHAR *pWcid);

BOOLEAN CFG80211DRV_OpsChgVirtualInf(RTMP_ADAPTER *pAd, VOID *pData)
{
	CMD_RTPRIV_IOCTL_80211_VIF_PARM *pVifParm = (CMD_RTPRIV_IOCTL_80211_VIF_PARM *) pData;
	UINT newType = pVifParm->newIfType;
#ifdef RT_CFG80211_P2P_SUPPORT
	struct wifi_dev *wdev = NULL;
	PCFG80211_CTRL pCfg80211_ctrl = &pAd->cfg80211_ctrl;
	UINT oldType = pVifParm->oldIfType;
#endif /* RT_CFG80211_P2P_SUPPORT */

#ifdef RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE
	APCLI_STRUCT *pApCliEntry;
	struct wifi_dev *wdev;
	PCFG80211_CTRL cfg80211_ctrl = &pAd->cfg80211_ctrl;

	CFG80211DBG(RT_DEBUG_TRACE,
		    ("80211> CFG80211DRV_OpsChgVirtualInf  newType %d  oldType %d\n", oldType,
		     newType));
	if (strcmp(pVifParm->net_dev->name, "p2p0") == 0) {
		switch (newType) {
		case RT_CMD_80211_IFTYPE_MONITOR:
			DBGPRINT(RT_DEBUG_TRACE, ("CFG80211 I/F Monitor Type\n"));
			/* RTMP_OS_NETDEV_SET_TYPE_MONITOR(new_dev_p); */
			break;

		case RT_CMD_80211_IFTYPE_STATION:
			RTMP_CFG80211_RemoveVifEntry(pAd, cfg80211_ctrl->dummy_p2p_net_dev);
			RTMP_CFG80211_AddVifEntry(pAd, cfg80211_ctrl->dummy_p2p_net_dev, newType);
			break;

		case RT_CMD_80211_IFTYPE_P2P_CLIENT:
			pApCliEntry = &pAd->ApCfg.ApCliTab[MAIN_MBSSID];
			wdev = &pApCliEntry->wdev;
			wdev->wdev_type = WDEV_TYPE_STA;
			wdev->func_dev = pApCliEntry;
			wdev->sys_handle = (void *)pAd;
			wdev->if_dev = cfg80211_ctrl->dummy_p2p_net_dev;
			wdev->tx_pkt_allowed = ApCliAllowToSendPacket;

			RTMP_CFG80211_RemoveVifEntry(pAd, cfg80211_ctrl->dummy_p2p_net_dev);
			RTMP_CFG80211_AddVifEntry(pAd, cfg80211_ctrl->dummy_p2p_net_dev, newType);

			RTMP_OS_NETDEV_SET_PRIV(cfg80211_ctrl->dummy_p2p_net_dev, pAd);
			RTMP_OS_NETDEV_SET_WDEV(cfg80211_ctrl->dummy_p2p_net_dev, wdev);
			if (rtmp_wdev_idx_reg(pAd, wdev) < 0) {
				DBGPRINT(RT_DEBUG_ERROR,
					 ("%s: Assign wdev idx for %s failed, free net device!\n",
					  __func__,
					  RTMP_OS_NETDEV_GET_DEVNAME
					  (cfg80211_ctrl->dummy_p2p_net_dev)));
				RtmpOSNetDevFree(cfg80211_ctrl->dummy_p2p_net_dev);
				cfg80211_ctrl->dummy_p2p_net_dev = NULL;
				break;
			}

			/* init MAC address of virtual network interface */
			COPY_MAC_ADDR(wdev->if_addr, pAd->cfg80211_ctrl.P2PCurrentAddress);

			pAd->flg_apcli_init = TRUE;
			ApCli_Open(pAd, cfg80211_ctrl->dummy_p2p_net_dev);
			break;

		case RT_CMD_80211_IFTYPE_P2P_GO:
			/* pNetDevOps->priv_flags = INT_P2P; */
			pAd->ApCfg.MBSSID[MAIN_MBSSID].MSSIDDev = NULL;
			/* The Behivaor in SetBeacon Ops        */
			/* pAd->ApCfg.MBSSID[MAIN_MBSSID].MSSIDDev = new_dev_p; */
			RTMP_CFG80211_RemoveVifEntry(pAd, cfg80211_ctrl->dummy_p2p_net_dev);
			RTMP_CFG80211_AddVifEntry(pAd, cfg80211_ctrl->dummy_p2p_net_dev, newType);
#ifdef CONFIG_CUSTOMIZED_DFS
			wdev = &pAd->ApCfg.MBSSID[MAIN_MBSSID].wdev;
			wdev->wdev_type = WDEV_TYPE_AP;
			wdev->func_dev = &pAd->ApCfg.MBSSID[MAIN_MBSSID];
			wdev->func_idx = MAIN_MBSSID;
			wdev->sys_handle = (void *)pAd;
			wdev->if_dev = cfg80211_ctrl->dummy_p2p_net_dev;

			RTMP_OS_NETDEV_SET_PRIV(cfg80211_ctrl->dummy_p2p_net_dev, pAd);
			RTMP_OS_NETDEV_SET_WDEV(cfg80211_ctrl->dummy_p2p_net_dev, wdev);

			RTMPSetPhyMode(pAd, pAd->CommonCfg.PhyMode);
#endif /* endif */

			pAd->cfg80211_ctrl.isCfgInApMode = RT_CMD_80211_IFTYPE_AP;
			COPY_MAC_ADDR(pAd->ApCfg.MBSSID[MAIN_MBSSID].wdev.if_addr,
				      pAd->cfg80211_ctrl.P2PCurrentAddress);
			COPY_MAC_ADDR(pAd->ApCfg.MBSSID[MAIN_MBSSID].wdev.bssid,
				      pAd->cfg80211_ctrl.P2PCurrentAddress);
			break;

		default:
			DBGPRINT(RT_DEBUG_ERROR, ("Unknown CFG80211 I/F Type (%d)\n", newType));
		}

		if ((newType == RT_CMD_80211_IFTYPE_STATION) &&
		    (oldType == RT_CMD_80211_IFTYPE_P2P_CLIENT) && (pAd->flg_apcli_init == TRUE)) {
			DBGPRINT(RT_DEBUG_TRACE, ("ApCli_Close\n"));
			CFG80211OS_ScanEnd(pAd->pCfg80211_CB, TRUE);
			if (pAd->cfg80211_ctrl.FlgCfg80211Scanning)
				pAd->cfg80211_ctrl.FlgCfg80211Scanning = FALSE;
			pAd->flg_apcli_init = FALSE;
			RT_MOD_INC_USE_COUNT();
			pAd->cfg80211_ctrl.isCfgInApMode = RT_CMD_80211_IFTYPE_STATION;

			return ApCli_Close(pAd, cfg80211_ctrl->dummy_p2p_net_dev);
		} else if ((newType == RT_CMD_80211_IFTYPE_STATION) &&
			   (oldType == RT_CMD_80211_IFTYPE_P2P_GO)) {
			DBGPRINT(RT_DEBUG_TRACE, ("GOi_Close\n"));
		}
	} else
		DBGPRINT(RT_DEBUG_TRACE, ("%s bypass setting\n", pVifParm->net_dev->name));
#endif /* RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE */

#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
#ifndef RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE
	/* After P2P NEGO phase, the device type may be change from GC to GO
	   or no change. We remove the GC in VIF list if nego as GO case.
	 */
	if ((newType == RT_CMD_80211_IFTYPE_P2P_GO) && (oldType == RT_CMD_80211_IFTYPE_P2P_CLIENT)) {

		/* Unregister GC's wdev from list */
		wdev = RtmpOsGetNetDevWdev(pVifParm->net_dev);
		if (rtmp_wdev_idx_unreg(pAd, wdev) < 0) {
			DBGPRINT(RT_DEBUG_ERROR, ("Assign %s wdev failed\n",
						RTMP_OS_NETDEV_GET_DEVNAME(pVifParm->net_dev)));
		}

		/* Register GO's wdev and reassign VIF net device's wdev */
		wdev = &pAd->ApCfg.MBSSID[MAIN_MBSSID].wdev;
		RTMP_OS_NETDEV_SET_PRIV(pVifParm->net_dev, pAd);
		RTMP_OS_NETDEV_SET_WDEV(pVifParm->net_dev, wdev);
		if (rtmp_wdev_idx_reg(pAd, wdev) < 0) {
			DBGPRINT(RT_DEBUG_ERROR, ("Assign %s wdev failed\n",
						RTMP_OS_NETDEV_GET_DEVNAME(pVifParm->net_dev)));
		}

		RTMP_CFG80211_VirtualIF_CancelP2pClient(pAd);
	}
#endif /* RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE */
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */

#ifdef RT_CFG80211_P2P_SINGLE_DEVICE

	CFG80211DBG(RT_DEBUG_TRACE, ("80211> @@@ Change from %u  to %u Mode\n", oldType, newType));

	pCfg80211_ctrl->P2POpStatusFlags = CFG_P2P_DISABLE;
	if (newType == RT_CMD_80211_IFTYPE_P2P_CLIENT) {
		pCfg80211_ctrl->P2POpStatusFlags = CFG_P2P_CLI_UP;

	} else if (newType == RT_CMD_80211_IFTYPE_P2P_GO) {
		pCfg80211_ctrl->P2POpStatusFlags = CFG_P2P_GO_UP;
	}
#endif /* RT_CFG80211_P2P_SINGLE_DEVICE */

#ifdef CONFIG_STA_SUPPORT
	/* Change Device Type */
	if (newType == RT_CMD_80211_IFTYPE_ADHOC) {
		Set_NetworkType_Proc(pAd, "Adhoc");
	} else if ((newType == RT_CMD_80211_IFTYPE_STATION) ||
		   (newType == RT_CMD_80211_IFTYPE_P2P_CLIENT)) {
		CFG80211DBG(RT_DEBUG_TRACE, ("80211> Change the Interface to STA Mode\n"));

#ifdef CONFIG_AP_SUPPORT
		if (pAd->cfg80211_ctrl.isCfgInApMode == RT_CMD_80211_IFTYPE_AP
		    && RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_START_UP))
			CFG80211DRV_DisableApInterface(pAd);
#endif /* CONFIG_AP_SUPPORT */

		pAd->cfg80211_ctrl.isCfgInApMode = RT_CMD_80211_IFTYPE_STATION;
	} else
#endif /*CONFIG_STA_SUPPORT */
	if ((newType == RT_CMD_80211_IFTYPE_AP) || (newType == RT_CMD_80211_IFTYPE_P2P_GO)) {
		CFG80211DBG(RT_DEBUG_TRACE, ("80211> Change the Interface to AP Mode\n"));
		pAd->cfg80211_ctrl.isCfgInApMode = RT_CMD_80211_IFTYPE_AP;
	}
#ifdef CONFIG_STA_SUPPORT
	else if (newType == RT_CMD_80211_IFTYPE_MONITOR) {
		/* set packet filter */
		Set_NetworkType_Proc(pAd, "Monitor");

		if (pVifParm->MonFilterFlag != 0) {
			UINT32 Filter;

			RTMP_IO_READ32(pAd, RX_FILTR_CFG, &Filter);

			if ((pVifParm->MonFilterFlag & RT_CMD_80211_FILTER_FCSFAIL) ==
			    RT_CMD_80211_FILTER_FCSFAIL) {
				Filter = Filter & (~0x01);
			} else {
				Filter = Filter | 0x01;
			}

			if ((pVifParm->MonFilterFlag & RT_CMD_80211_FILTER_PLCPFAIL) ==
			    RT_CMD_80211_FILTER_PLCPFAIL) {
				Filter = Filter & (~0x02);
			} else {
				Filter = Filter | 0x02;
			}

			if ((pVifParm->MonFilterFlag & RT_CMD_80211_FILTER_CONTROL) ==
			    RT_CMD_80211_FILTER_CONTROL) {
				Filter = Filter & (~0xFF00);
			} else {
				Filter = Filter | 0xFF00;
			}

			if ((pVifParm->MonFilterFlag & RT_CMD_80211_FILTER_OTHER_BSS) ==
			    RT_CMD_80211_FILTER_OTHER_BSS) {
				Filter = Filter & (~0x08);
			} else {
				Filter = Filter | 0x08;
			}

			RTMP_IO_WRITE32(pAd, RX_FILTR_CFG, Filter);
			pVifParm->MonFilterFlag = Filter;
		}
	}
#endif /*CONFIG_STA_SUPPORT */

	if ((newType == RT_CMD_80211_IFTYPE_P2P_CLIENT) || (newType == RT_CMD_80211_IFTYPE_P2P_GO)) {
		COPY_MAC_ADDR(pAd->cfg80211_ctrl.P2PCurrentAddress, pVifParm->net_dev->dev_addr);
	} else {
#ifdef RT_CFG80211_P2P_SUPPORT
		pCfg80211_ctrl->bP2pCliPmEnable = FALSE;
		pCfg80211_ctrl->bPreKeepSlient = FALSE;
		pCfg80211_ctrl->bKeepSlient = FALSE;
		pCfg80211_ctrl->NoAIndex = MAX_LEN_OF_MAC_TABLE;
		pCfg80211_ctrl->MyGOwcid = MAX_LEN_OF_MAC_TABLE;
		pCfg80211_ctrl->CTWindows = 0;	/* CTWindows and OppPS parameter field */
#endif /* RT_CFG80211_P2P_SUPPORT */
	}

	return TRUE;
}

#ifdef RT_CFG80211_P2P_SUPPORT
BOOLEAN RTMP_CFG80211_VIF_P2P_GO_ON(VOID *pAdSrc)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
	PNET_DEV pNetDev = NULL;
	pNetDev = RTMP_CFG80211_FindVifEntry_ByType(pAd, RT_CMD_80211_IFTYPE_P2P_GO);
	if ((pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList.size > 0) &&
	    (pNetDev != NULL))
		return TRUE;
	else
		return FALSE;
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */

#ifdef RT_CFG80211_P2P_SINGLE_DEVICE
	if (CFG80211_P2P_TEST_BIT(pAd->cfg80211_ctrl.P2POpStatusFlags, CFG_P2P_GO_UP))
		return TRUE;
	else
		return FALSE;
#endif /* RT_CFG80211_P2P_SINGLE_DEVICE */

	return FALSE;
}

BOOLEAN RTMP_CFG80211_VIF_P2P_CLI_ON(VOID *pAdSrc)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
	PNET_DEV pNetDev = NULL;
	pNetDev = RTMP_CFG80211_FindVifEntry_ByType(pAd, RT_CMD_80211_IFTYPE_P2P_CLIENT);
	if ((pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList.size > 0) &&
	    (pNetDev != NULL))
		return TRUE;
	else
		return FALSE;
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */

#ifdef RT_CFG80211_P2P_SINGLE_DEVICE
	if (CFG80211_P2P_TEST_BIT(pAd->cfg80211_ctrl.P2POpStatusFlags, CFG_P2P_CLI_UP))
		return TRUE;
	else
		return FALSE;
#endif /* RT_CFG80211_P2P_SINGLE_DEVICE */

	return FALSE;
}

BOOLEAN RTMP_CFG80211_VIF_P2P_CLI_CONNECTED(VOID *pAdSrc)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
	return pAd->ApCfg.ApCliTab[MAIN_MBSSID].CtrlCurrState == APCLI_CTRL_CONNECTED;
}

#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
BOOLEAN CFG80211DRV_OpsVifAdd(VOID *pAdOrg, VOID *pData)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdOrg;
	CMD_RTPRIV_IOCTL_80211_VIF_SET *pVifInfo;
	pVifInfo = (CMD_RTPRIV_IOCTL_80211_VIF_SET *) pData;

	/* Already one VIF in list */
	if (pAd->cfg80211_ctrl.Cfg80211VifDevSet.isGoingOn)
		return FALSE;

	pAd->cfg80211_ctrl.Cfg80211VifDevSet.isGoingOn = TRUE;
	return RTMP_CFG80211_VirtualIF_Init(pAd, pVifInfo->vifName, pVifInfo->vifType);
}
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */
#endif /* RT_CFG80211_P2P_SUPPORT */

BOOLEAN RTMP_CFG80211_VIF_ON(VOID *pAdSrc)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
	return pAd->cfg80211_ctrl.Cfg80211VifDevSet.isGoingOn;
}

BOOLEAN RTMP_CFG80211_ROC_ON(VOID *pAdSrc)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
	return pAd->cfg80211_ctrl.Cfg80211RocTimerRunning;
}

static
PCFG80211_VIF_DEV RTMP_CFG80211_FindVifEntry_ByMac(VOID *pAdSrc, PNET_DEV pNewNetDev)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
	PLIST_HEADER pCacheList = &pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList;
	PCFG80211_VIF_DEV pDevEntry = NULL;
	PLIST_ENTRY pListEntry = NULL;

	pListEntry = pCacheList->pHead;
	pDevEntry = (PCFG80211_VIF_DEV) pListEntry;
	while (pDevEntry != NULL) {
		if (RTMPEqualMemory
		    (pDevEntry->net_dev->dev_addr, pNewNetDev->dev_addr, MAC_ADDR_LEN))
			return pDevEntry;

		pListEntry = pListEntry->pNext;
		pDevEntry = (PCFG80211_VIF_DEV) pListEntry;
	}

	return NULL;
}

PNET_DEV RTMP_CFG80211_FindVifEntry_ByType(VOID *pAdSrc, UINT32 devType)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
	PLIST_HEADER pCacheList = &pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList;
	PCFG80211_VIF_DEV pDevEntry = NULL;
	PLIST_ENTRY pListEntry = NULL;

	pListEntry = pCacheList->pHead;
	pDevEntry = (PCFG80211_VIF_DEV) pListEntry;
	while (pDevEntry != NULL) {
		if (pDevEntry->devType == devType)
			return pDevEntry->net_dev;

		pListEntry = pListEntry->pNext;
		pDevEntry = (PCFG80211_VIF_DEV) pListEntry;
	}

	return NULL;
}

PWIRELESS_DEV RTMP_CFG80211_FindVifEntryWdev_ByType(IN VOID *pAdSrc, UINT32 devType)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
	PLIST_HEADER pCacheList = &pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList;
	PCFG80211_VIF_DEV pDevEntry = NULL;
	PLIST_ENTRY pListEntry = NULL;

	pListEntry = pCacheList->pHead;
	pDevEntry = (PCFG80211_VIF_DEV) pListEntry;
	while (pDevEntry != NULL) {
		if (pDevEntry->devType == devType)
			return pDevEntry->net_dev->ieee80211_ptr;

		pListEntry = pListEntry->pNext;
		pDevEntry = (PCFG80211_VIF_DEV) pListEntry;
	}

	return NULL;
}

VOID RTMP_CFG80211_AddVifEntry(VOID *pAdSrc, PNET_DEV pNewNetDev, UINT32 DevType)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
	PCFG80211_VIF_DEV pNewVifDev = NULL;

	os_alloc_mem(NULL, (UCHAR **) &pNewVifDev, sizeof(CFG80211_VIF_DEV));
	if (pNewVifDev) {
		NdisZeroMemory(pNewVifDev, sizeof(CFG80211_VIF_DEV));

		pNewVifDev->pNext = NULL;
		pNewVifDev->net_dev = pNewNetDev;
		pNewVifDev->devType = DevType;
		NdisZeroMemory(pNewVifDev->CUR_MAC, MAC_ADDR_LEN);
		NdisCopyMemory(pNewVifDev->CUR_MAC, pNewNetDev->dev_addr, MAC_ADDR_LEN);

		insertTailList(&pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList,
			       (PLIST_ENTRY) pNewVifDev);
		DBGPRINT(RT_DEBUG_TRACE,
			 ("Add CFG80211 VIF Device, Type: %d.\n", pNewVifDev->devType));
	} else {
		DBGPRINT(RT_DEBUG_ERROR, ("Error in alloc mem in New CFG80211 VIF Function.\n"));
	}
}

VOID RTMP_CFG80211_RemoveVifEntry(VOID *pAdSrc, PNET_DEV pNewNetDev)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
	PLIST_ENTRY pListEntry = NULL;

	pListEntry = (PLIST_ENTRY) RTMP_CFG80211_FindVifEntry_ByMac(pAd, pNewNetDev);

	if (pListEntry) {
		delEntryList(&pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList, pListEntry);
		os_free_mem(NULL, pListEntry);
	} else {
		DBGPRINT(RT_DEBUG_ERROR, ("Error in RTMP_CFG80211_RemoveVifEntry.\n"));
	}
}

PNET_DEV RTMP_CFG80211_VirtualIF_Get(IN VOID *pAdSrc)
{
	/* PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdSrc; */
	/* return pAd->Cfg80211VifDevSet.Cfg80211VifDev[0].net_dev; */
	return NULL;
}

#ifdef RT_CFG80211_P2P_SUPPORT
VOID RTMP_CFG80211_VirtualIF_CancelP2pClient(VOID *pAdSrc)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
	PLIST_HEADER pCacheList = &pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList;
	PCFG80211_VIF_DEV pDevEntry = NULL;
	PLIST_ENTRY pListEntry = NULL;

	DBGPRINT(RT_DEBUG_TRACE, ("==> %s\n", __func__));

	pListEntry = pCacheList->pHead;
	pDevEntry = (PCFG80211_VIF_DEV) pListEntry;
	while (pDevEntry != NULL) {
		if (pDevEntry->devType == RT_CMD_80211_IFTYPE_P2P_CLIENT) {
			DBGPRINT(RT_DEBUG_ERROR,
				 ("==> RTMP_CFG80211_VirtualIF_CancelP2pClient HIT.\n"));
			pDevEntry->devType = RT_CMD_80211_IFTYPE_P2P_GO;
			break;
		}

		pListEntry = pListEntry->pNext;
		pDevEntry = (PCFG80211_VIF_DEV) pListEntry;
	}

	pAd->flg_apcli_init = FALSE;
	pAd->ApCfg.ApCliTab[MAIN_MBSSID].wdev.if_dev = NULL;

	DBGPRINT(RT_DEBUG_TRACE, ("<== %s\n", __func__));
}
#endif /* RT_CFG80211_P2P_SUPPORT */

#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
static INT CFG80211_VirtualIF_Open(PNET_DEV dev_p)
{
	VOID *pAdSrc = RTMP_OS_NETDEV_GET_PRIV(dev_p);
	ASSERT(pAdSrc);

	DBGPRINT(RT_DEBUG_OFF, ("%s: ===> %d,%s\n", __func__, dev_p->ifindex,
				  RTMP_OS_NETDEV_GET_DEVNAME(dev_p)));

	/* if (VIRTUAL_IF_UP(pAd) != 0) */
	/* return -1; */

	/* increase MODULE use count */
	RT_MOD_INC_USE_COUNT();

#ifdef RT_CFG80211_P2P_SUPPORT
	if (dev_p->ieee80211_ptr->iftype == RT_CMD_80211_IFTYPE_P2P_CLIENT) {
		PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
		DBGPRINT(RT_DEBUG_TRACE, ("CFG80211_VirtualIF_Open\n"));
		pAd->flg_apcli_init = TRUE;
		ApCli_Open(pAd, dev_p);
		return 0;
	}
#endif /* RT_CFG80211_P2P_SUPPORT */

#ifdef CONFIG_SNIFFER_SUPPORT
#ifdef CONFIG_AP_SUPPORT
	if (dev_p->ieee80211_ptr->iftype == RT_CMD_80211_IFTYPE_MONITOR) {
		PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
		pAd->ApCfg.BssType = BSS_MONITOR;
		pAd->sniffer_ctl.sniffer_type = RADIOTAP_TYPE;
		AsicSetRxFilter(pAd);
	}
#endif /*CONFIG_AP_SUPPORT */
#endif /* CONFIG_SNIFFER_SUPPORT */
	RTMP_OS_NETDEV_START_QUEUE(dev_p);
	DBGPRINT(RT_DEBUG_TRACE, ("%s: <=== %s\n", __func__, RTMP_OS_NETDEV_GET_DEVNAME(dev_p)));

	return 0;
}

static INT CFG80211_VirtualIF_Close(PNET_DEV dev_p)
{
	VOID *pAdSrc = RTMP_OS_NETDEV_GET_PRIV(dev_p);
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
	ASSERT(pAdSrc);

	DBGPRINT(RT_DEBUG_OFF,
		 ("%s: ===> %s (iftype=0x%x)\n", __func__, RTMP_OS_NETDEV_GET_DEVNAME(dev_p),
		  dev_p->ieee80211_ptr->iftype));

#ifdef RT_CFG80211_P2P_SUPPORT
	if (dev_p->ieee80211_ptr->iftype == RT_CMD_80211_IFTYPE_P2P_CLIENT) {
		DBGPRINT(RT_DEBUG_TRACE, ("CFG80211_VirtualIF_Close\n"));
		CFG80211DRV_OpsScanInLinkDownAction(pAd);
		RT_MOD_DEC_USE_COUNT();
		return ApCli_Close(pAd, dev_p);
	}
#endif /* RT_CFG80211_P2P_SUPPORT */
#ifdef CONFIG_SNIFFER_SUPPORT
#ifdef CONFIG_AP_SUPPORT
	if (dev_p->ieee80211_ptr->iftype == RT_CMD_80211_IFTYPE_MONITOR) {
		pAd->ApCfg.BssType = BSS_INFRA;
		AsicSetRxFilter(pAd);
	}
#endif /*CONFIG_AP_SUPPORT */
#endif /*CONFIG_SNIFFER_SUPPORT */
	DBGPRINT(RT_DEBUG_TRACE, ("%s: ===> %s\n", __func__, RTMP_OS_NETDEV_GET_DEVNAME(dev_p)));

	RTMP_OS_NETDEV_STOP_QUEUE(dev_p);

	if (netif_carrier_ok(dev_p))
		netif_carrier_off(dev_p);
#ifdef CONFIG_STA_SUPPORT
	if (INFRA_ON(pAd))
		AsicEnableBssSync(pAd);

	else if (ADHOC_ON(pAd))
		AsicEnableIbssSync(pAd);
#else
	else
		AsicDisableSync(pAd);
#endif /* endif */

	/* VIRTUAL_IF_DOWN(pAd); */

	RT_MOD_DEC_USE_COUNT();
	return 0;
}

static INT CFG80211_PacketSend(PNDIS_PACKET pPktSrc, PNET_DEV pDev, RTMP_NET_PACKET_TRANSMIT Func)
{
	PRTMP_ADAPTER pAd;
	pAd = RTMP_OS_NETDEV_GET_PRIV(pDev);
	ASSERT(pAd);

	/* To Indicate from Which VIF */
	switch (pDev->ieee80211_ptr->iftype) {
	case RT_CMD_80211_IFTYPE_AP:
		/* minIdx = MIN_NET_DEVICE_FOR_CFG80211_VIF_AP; */
		RTMP_SET_PACKET_OPMODE(pPktSrc, OPMODE_AP);
		break;

	case RT_CMD_80211_IFTYPE_P2P_GO:
		/* minIdx = MIN_NET_DEVICE_FOR_CFG80211_VIF_P2P_GO; */
		if (!OPSTATUS_TEST_FLAG(pAd, fOP_AP_STATUS_MEDIA_STATE_CONNECTED)) {
			DBGPRINT(RT_DEBUG_TRACE,
				 ("Drop the Packet due P2P GO not in ready state\n"));
			RELEASE_NDIS_PACKET(pAd, pPktSrc, NDIS_STATUS_FAILURE);
			return 0;
		}
		RTMP_SET_PACKET_OPMODE(pPktSrc, OPMODE_AP);
		break;

	case RT_CMD_80211_IFTYPE_P2P_CLIENT:
		/* minIdx = MIN_NET_DEVICE_FOR_CFG80211_VIF_P2P_CLI; */
		RTMP_SET_PACKET_OPMODE(pPktSrc, OPMODE_AP);
		break;

	case RT_CMD_80211_IFTYPE_STATION:
	default:
		DBGPRINT(RT_DEBUG_TRACE,
			 ("Unknown CFG80211 I/F Type(%d)\n", pDev->ieee80211_ptr->iftype));
		RELEASE_NDIS_PACKET(pAd, pPktSrc, NDIS_STATUS_FAILURE);
		return 0;
	}

	DBGPRINT(RT_DEBUG_INFO, ("CFG80211 Packet Type  [%s](%d)\n",
				 pDev->name, pDev->ieee80211_ptr->iftype));

	RTMP_SET_PACKET_NET_DEVICE_MBSSID(pPktSrc, MAIN_MBSSID);

	return Func(RTPKT_TO_OSPKT(pPktSrc));
}

static INT CFG80211_VirtualIF_PacketSend(struct sk_buff *skb, PNET_DEV dev_p)
{
	struct wifi_dev *wdev;

	DBGPRINT(RT_DEBUG_INFO, ("%s ---> %d\n", __func__, dev_p->ieee80211_ptr->iftype));

	if (!(RTMP_OS_NETDEV_STATE_RUNNING(dev_p))) {
		/* the interface is down */
		RELEASE_NDIS_PACKET(NULL, skb, NDIS_STATUS_FAILURE);
		return 0;
	}

	/* The device not ready to send packt. */
	wdev = RTMP_OS_NETDEV_GET_WDEV(dev_p);
	ASSERT(wdev);
	if (!wdev)
		return -1;

	return CFG80211_PacketSend(skb, dev_p, rt28xx_packet_xmit);
}

static INT CFG80211_VirtualIF_Ioctl(IN PNET_DEV dev_p, IN OUT VOID *rq_p, IN INT cmd)
{

	RTMP_ADAPTER *pAd;

	pAd = RTMP_OS_NETDEV_GET_PRIV(dev_p);
	ASSERT(pAd);

	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
		return -ENETDOWN;

	DBGPRINT(RT_DEBUG_TRACE, ("%s --->\n", __func__));

	return rt28xx_ioctl(dev_p, rq_p, cmd);

}

BOOLEAN RTMP_CFG80211_VirtualIF_Init(IN VOID *pAdSrc, IN CHAR *pDevName, IN UINT32 DevType)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
	RTMP_OS_NETDEV_OP_HOOK netDevHook, *pNetDevOps;
	PNET_DEV new_dev_p;
#ifdef RT_CFG80211_P2P_SUPPORT
	APCLI_STRUCT *pApCliEntry;
#endif /* RT_CFG80211_P2P_SUPPORT */
	struct wifi_dev *wdev;

	CHAR preIfName[IFNAMSIZ];
	UINT devNameLen = strlen(pDevName);
	UINT preIfIndex;
	CFG80211_CB *p80211CB = pAd->pCfg80211_CB;
	struct wireless_dev *pWdev;
	UINT32 MC_RowID = 0, IoctlIF = 0, Inf = INT_P2P;
#ifdef RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE
	return TRUE;		/* move to CFG80211DRV_OpsChgVirtualInf */
#endif /* RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE */

	if (devNameLen > IFNAMSIZ)
		devNameLen = IFNAMSIZ;
	preIfIndex = pDevName[devNameLen - 1] - 48;
	ASSERT(pDevName[devNameLen - 1] - 48 >= 0);
	NdisCopyMemory(preIfName, pDevName, devNameLen - 1);
	preIfName[devNameLen - 1] = '\0';

	pNetDevOps = &netDevHook;

	DBGPRINT(RT_DEBUG_OFF,
		 ("%s ---> (%s, %s, %d)\n", __func__, pDevName, preIfName, preIfIndex));

	/* init operation functions and flags */
	NdisZeroMemory(&netDevHook, sizeof(netDevHook));
	netDevHook.open = CFG80211_VirtualIF_Open;	/* device opem hook point */
	netDevHook.stop = CFG80211_VirtualIF_Close;	/* device close hook point */
	netDevHook.xmit = CFG80211_VirtualIF_PacketSend;	/* hard transmit hook point */
	netDevHook.ioctl = CFG80211_VirtualIF_Ioctl;	/* ioctl hook point */

#if WIRELESS_EXT >= 12
	/* netDevHook.iw_handler = (void *)&rt28xx_ap_iw_handler_def; */
#endif /* WIRELESS_EXT >= 12 */

	new_dev_p =
	    RtmpOSNetDevCreate(MC_RowID, &IoctlIF, Inf, preIfIndex, sizeof(PRTMP_ADAPTER),
			       preIfName);

	if (new_dev_p == NULL) {
		/* allocation fail, exit */
		DBGPRINT(RT_DEBUG_ERROR, ("Allocate network device fail (CFG80211)...\n"));
		return FALSE;
	} else {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("Register CFG80211 I/F (%s)\n", RTMP_OS_NETDEV_GET_DEVNAME(new_dev_p)));
	}

	new_dev_p->destructor = free_netdev;
	RTMP_OS_NETDEV_SET_PRIV(new_dev_p, pAd);
	pNetDevOps->needProtcted = TRUE;

	/* Shared mac address is not allow for wpa_supplicant */
	NdisMoveMemory(&pNetDevOps->devAddr[0], &pAd->CurrentAddress[0], MAC_ADDR_LEN);

	/* CFG_TODO */
	/*
	   Bit1 of MAC address Byte0 is local administration bit
	   and should be set to 1 in extended multiple BSSIDs'
	   Bit3~ of MAC address Byte0 is extended multiple BSSID index.
	 */
	if (pAd->chipCap.MBSSIDMode == MBSSID_MODE1)
		pNetDevOps->devAddr[0] += 2;	/* NEW BSSID */
	else {
#ifdef P2P_ODD_MAC_ADJUST
		if (pNetDevOps->devAddr[5] & 0x01 == 0x01)
			pNetDevOps->devAddr[5] -= 1;
		else
#endif /* P2P_ODD_MAC_ADJUST */
			pNetDevOps->devAddr[5] += FIRST_MBSSID;
	}

#ifdef CUSTOMIZED_FEATURE_SUPPORT
	/* Shared p2p mac address is not allow for original wpa_supplicant */
	if (CHECK_CUSTOMIZED_FEATURE(pAd, CUSTOMIZED_FEATURE_SHARED_VIF_MAC) == FALSE)
		pNetDevOps->devAddr[0] ^= 1 << 2;
#else
	pNetDevOps->devAddr[0] ^= 1 << 2;
#endif /* CUSTOMIZED_FEATURE_SUPPORT */

	switch (DevType) {
	case RT_CMD_80211_IFTYPE_MONITOR:
		DBGPRINT(RT_DEBUG_ERROR, ("CFG80211 I/F Monitor Type\n"));

		/* RTMP_OS_NETDEV_SET_TYPE_MONITOR(new_dev_p); */
		break;
#ifdef RT_CFG80211_P2P_SUPPORT
	case RT_CMD_80211_IFTYPE_P2P_CLIENT:
		pApCliEntry = &pAd->ApCfg.ApCliTab[MAIN_MBSSID];
		wdev = &pApCliEntry->wdev;
		wdev->wdev_type = WDEV_TYPE_STA;
		wdev->func_dev = pApCliEntry;
		wdev->sys_handle = (void *)pAd;
		wdev->if_dev = new_dev_p;
		wdev->tx_pkt_allowed = ApCliAllowToSendPacket;
		RTMP_OS_NETDEV_SET_PRIV(new_dev_p, pAd);
		RTMP_OS_NETDEV_SET_WDEV(new_dev_p, wdev);
		if (rtmp_wdev_idx_reg(pAd, wdev) < 0) {
			DBGPRINT(RT_DEBUG_ERROR,
				 ("%s: Assign wdev idx for %s failed, free net device!\n",
				  __func__, RTMP_OS_NETDEV_GET_DEVNAME(new_dev_p)));
			RtmpOSNetDevFree(new_dev_p);
			new_dev_p = NULL;
			wdev->if_dev = NULL;
			return FALSE;
		}

		/* init MAC address of virtual network interface */
		COPY_MAC_ADDR(wdev->if_addr, pNetDevOps->devAddr);
		break;
	case RT_CMD_80211_IFTYPE_P2P_GO:
		pNetDevOps->priv_flags = INT_P2P;
		pAd->ApCfg.MBSSID[MAIN_MBSSID].MSSIDDev = NULL;
		/* The Behivaor in SetBeacon Ops        */
		/* pAd->ApCfg.MBSSID[MAIN_MBSSID].MSSIDDev = new_dev_p; */

#ifdef CONFIG_CUSTOMIZED_DFS
		wdev = &pAd->ApCfg.MBSSID[MAIN_MBSSID].wdev;
		wdev->wdev_type = WDEV_TYPE_AP;
		wdev->func_dev = &pAd->ApCfg.MBSSID[MAIN_MBSSID];
		wdev->func_idx = MAIN_MBSSID;
		wdev->sys_handle = (void *)pAd;
		wdev->if_dev = new_dev_p;

		RTMP_OS_NETDEV_SET_PRIV(new_dev_p, pAd);
		RTMP_OS_NETDEV_SET_WDEV(new_dev_p, wdev);

		RTMPSetPhyMode(pAd, pAd->CommonCfg.PhyMode);
#endif /* endif */

		pAd->cfg80211_ctrl.isCfgInApMode = RT_CMD_80211_IFTYPE_AP;
		COPY_MAC_ADDR(pAd->ApCfg.MBSSID[MAIN_MBSSID].wdev.if_addr, pNetDevOps->devAddr);
		COPY_MAC_ADDR(pAd->ApCfg.MBSSID[MAIN_MBSSID].wdev.bssid, pNetDevOps->devAddr);
		break;
#endif /* RT_CFG80211_P2P_SUPPORT */
	default:
		DBGPRINT(RT_DEBUG_ERROR, ("Unknown CFG80211 I/F Type (%d)\n", DevType));
	}

	/* CFG_TODO : should be move to VIF_CHG */
	if ((DevType == RT_CMD_80211_IFTYPE_P2P_CLIENT) || (DevType == RT_CMD_80211_IFTYPE_P2P_GO)) {
		COPY_MAC_ADDR(pAd->cfg80211_ctrl.P2PCurrentAddress, pNetDevOps->devAddr);
	}

	pWdev = kzalloc(sizeof(*pWdev), GFP_KERNEL);

	new_dev_p->ieee80211_ptr = pWdev;
	pWdev->wiphy = p80211CB->pCfg80211_Wdev->wiphy;
	SET_NETDEV_DEV(new_dev_p, wiphy_dev(pWdev->wiphy));
	pWdev->netdev = new_dev_p;
	pWdev->iftype = DevType;

	RtmpOSNetDevAttach(pAd->OpMode, new_dev_p, pNetDevOps);

	/* Record the pNetDevice to Cfg80211VifDevList */
	RTMP_CFG80211_AddVifEntry(pAd, new_dev_p, DevType);

	DBGPRINT(RT_DEBUG_TRACE, ("%s <---\n", __func__));
	return TRUE;
}
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */

VOID RTMP_CFG80211_VirtualIF_Remove(IN VOID *pAdSrc, IN PNET_DEV dev_p, IN UINT32 DevType)
{

	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;

	if (dev_p) {
		RTMP_CFG80211_RemoveVifEntry(pAd, dev_p);
		RTMP_OS_NETDEV_STOP_QUEUE(dev_p);
#ifdef RT_CFG80211_P2P_SUPPORT
		pAd->cfg80211_ctrl.Cfg80211VifDevSet.isGoingOn = FALSE;

		if (RTMP_CFG80211_VIF_P2P_GO_ON(pAd)) {
#ifdef CONFIG_MULTI_CHANNEL
			PMULTISSID_STRUCT pMbss = &pAd->ApCfg.MBSSID[MAIN_MBSSID];
			struct wifi_dev *pwdev = &pMbss->wdev;

			if (pAd->Mlme.bStartMcc == TRUE) {
				DBGPRINT(RT_DEBUG_TRACE, ("Group remove stop mcc\n"));
				pAd->chipCap.tssi_enable = TRUE;	/* let host do tssi */
				Stop_MCC(pAd, 1);
				pAd->Mlme.bStartMcc = FALSE;
			}

			if (pAd->Mlme.bStartScc == TRUE) {
				DBGPRINT(RT_DEBUG_TRACE,
					 ("GO remove & switch to Infra BW = %d  pAd->StaCfg.wdev.CentralChannel %d\n",
					  pAd->StaCfg.wdev.bw, pAd->StaCfg.wdev.CentralChannel));
				pAd->Mlme.bStartScc = FALSE;
				AsicSwitchChannel(pAd, pAd->StaCfg.wdev.CentralChannel, FALSE);
				AsicLockChannel(pAd, pAd->StaCfg.wdev.CentralChannel);
				bbp_set_bw(pAd, pAd->StaCfg.wdev.bw);
			}

			pwdev->channel = 0;
			pwdev->CentralChannel = 0;
			pwdev->bw = 0;
			pwdev->extcha = EXTCHA_NONE;

/*after p2p cli connect , neet to change to default configure*/
			pAd->CommonCfg.RegTransmitSetting.field.EXTCHA = EXTCHA_BELOW;
			pAd->CommonCfg.RegTransmitSetting.field.BW = BW_40;
			pAd->CommonCfg.HT_Disable = 0;
			SetCommonHT(pAd);

#endif /* CONFI1G_MULTI_CHANNEL */

			/* Always restore to default iftype for avoiding case without iftype change */
			pAd->cfg80211_ctrl.isCfgInApMode = RT_CMD_80211_IFTYPE_STATION;
			RtmpOSNetDevDetach(dev_p);
			pAd->ApCfg.MBSSID[MAIN_MBSSID].MSSIDDev = NULL;
		} else if (pAd->flg_apcli_init) {
			struct wifi_dev *wdev;
			wdev = &pAd->ApCfg.ApCliTab[MAIN_MBSSID].wdev;

#ifdef CONFIG_MULTI_CHANNEL
			/* actually not mcc still need to check this! */

			if (pAd->Mlme.bStartMcc == TRUE) {
				DBGPRINT(RT_DEBUG_TRACE, ("@@@ GC remove stop mcc\n"));
				pAd->chipCap.tssi_enable = TRUE;	/* let host do tssi */
				Stop_MCC(pAd, 1);
				pAd->Mlme.bStartMcc = FALSE;
			} else
				/* if (pAd->Mlme.bStartScc == TRUE) */
			{
				DBGPRINT(RT_DEBUG_TRACE,
					 ("@@@ GC remove & switch to Infra BW = %d  pAd->StaCfg.wdev.CentralChannel %d\n",
					  pAd->StaCfg.wdev.bw, pAd->StaCfg.wdev.CentralChannel));
				pAd->Mlme.bStartScc = FALSE;
				AsicSwitchChannel(pAd, pAd->StaCfg.wdev.CentralChannel, FALSE);
				AsicLockChannel(pAd, pAd->StaCfg.wdev.CentralChannel);
				bbp_set_bw(pAd, pAd->StaCfg.wdev.bw);
			}

			wdev->CentralChannel = 0;
			wdev->channel = 0;
			wdev->bw = HT_BW_20;
			wdev->extcha = EXTCHA_NONE;
#endif /* CONFIG_MULTI_CHANNEL */
			OPSTATUS_CLEAR_FLAG(pAd, fOP_AP_STATUS_MEDIA_STATE_CONNECTED);
			cfg80211_disconnected(dev_p, WLAN_REASON_DEAUTH_LEAVING, NULL, 0,
						      GFP_KERNEL);
			NdisZeroMemory(pAd->ApCfg.ApCliTab[MAIN_MBSSID].CfgApCliBssid,
				       MAC_ADDR_LEN);
			RtmpOSNetDevDetach(dev_p);
			rtmp_wdev_idx_unreg(pAd, wdev);
			pAd->flg_apcli_init = FALSE;
			wdev->if_dev = NULL;
		} else
#endif /* RT_CFG80211_P2P_SUPPORT */
		{
			/* Never Opened When New Netdevice on */
			RtmpOSNetDevDetach(dev_p);
		}

		if (!INFRA_ON(pAd)) {
			if (pAd->CommonCfg.Channel != 1) {
				pAd->CommonCfg.Channel = 1;
				pAd->CommonCfg.CentralChannel = 1;
#ifdef DOT11_VHT_AC
				pAd->CommonCfg.vht_cent_ch = 1;
#endif /* DOT11_VHT_AC */
				CFG80211DBG(RT_DEBUG_ERROR,
					    ("80211> %s, Restore to channel %d\n", __func__,
					     pAd->CommonCfg.Channel));
				AsicSwitchChannel(pAd, pAd->CommonCfg.Channel, FALSE);
				AsicLockChannel(pAd, pAd->CommonCfg.Channel);
			}
			/* Restore to BW 20 */
			bbp_set_bw(pAd, BW_20);
		}
		if (!RTMP_CFG80211_VIF_P2P_GO_ON(pAd) && !RTMP_CFG80211_VIF_P2P_CLI_ON(pAd))	{
			PCFG80211_CTRL cfg80211_ctrl = &pAd->cfg80211_ctrl;
			PNET_DEV dummy_p2p_net_dev = (PNET_DEV) cfg80211_ctrl->dummy_p2p_net_dev;
			COPY_MAC_ADDR(pAd->cfg80211_ctrl.P2PCurrentAddress,
			  dummy_p2p_net_dev->dev_addr);
			DBGPRINT(RT_DEBUG_TRACE,
			 ("%s(): P2PCurrentAddress %X:%X:%X:%X:%X:%X\n",
			  __func__, PRINT_MAC(pAd->cfg80211_ctrl.P2PCurrentAddress)));
		}

	}
}

VOID RTMP_CFG80211_AllVirtualIF_Remove(IN VOID *pAdSrc)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
	PLIST_HEADER pCacheList = &pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList;
	PCFG80211_VIF_DEV pDevEntry = NULL;
	PLIST_ENTRY pListEntry = NULL;
	pListEntry = pCacheList->pHead;
	pDevEntry = (PCFG80211_VIF_DEV) pListEntry;

	while ((pDevEntry != NULL) && (pCacheList->size != 0)) {
		RtmpOSNetDevProtect(1);
		RTMP_CFG80211_VirtualIF_Remove(pAd, pDevEntry->net_dev,
					       pDevEntry->net_dev->ieee80211_ptr->iftype);
		RtmpOSNetDevProtect(0);

		pListEntry = pListEntry->pNext;
		pDevEntry = (PCFG80211_VIF_DEV) pListEntry;
	}
}

#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
static INT CFG80211_DummyP2pIf_Open(IN PNET_DEV dev_p)
{
	struct wireless_dev *wdev = dev_p->ieee80211_ptr;
#ifdef RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE
	VOID *pAdSrc;
	PRTMP_ADAPTER pAd;
	printk("CFG80211_DummyP2pIf_Open=======> Open\n");
	pAdSrc = RTMP_OS_NETDEV_GET_PRIV(dev_p);
	pAd = (PRTMP_ADAPTER) pAdSrc;
#endif /* RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE */

	if (!wdev)
		return -EINVAL;

	wdev->wiphy->interface_modes |= (BIT(NL80211_IFTYPE_P2P_CLIENT)
					 | BIT(NL80211_IFTYPE_P2P_GO));

#ifdef RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE
	RT_MOD_INC_USE_COUNT();
	/* ApCli_Open move to CFG80211DRV_OpsChgVirtualInf */
	RTMP_OS_NETDEV_START_QUEUE(dev_p);
	AsicSetBssid(pAd, pAd->cfg80211_ctrl.P2PCurrentAddress);
#endif /* RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE */
	return 0;
}

static INT CFG80211_DummyP2pIf_Close(IN PNET_DEV dev_p)
{
	struct wireless_dev *wdev = dev_p->ieee80211_ptr;
#ifdef RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE
	VOID *pAdSrc;
	PRTMP_ADAPTER pAd;
#endif /* RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE */
	if (!wdev)
		return -EINVAL;
#ifdef RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE
	pAdSrc = RTMP_OS_NETDEV_GET_PRIV(dev_p);
	ASSERT(pAdSrc);
	pAd = (PRTMP_ADAPTER) pAdSrc;

	DBGPRINT(RT_DEBUG_TRACE,
		 ("%s: ===> %s (iftype=0x%x)\n", __func__, RTMP_OS_NETDEV_GET_DEVNAME(dev_p),
		  dev_p->ieee80211_ptr->iftype));

	if (dev_p->ieee80211_ptr->iftype == RT_CMD_80211_IFTYPE_P2P_CLIENT) {
		DBGPRINT(RT_DEBUG_TRACE, ("CFG80211_VirtualIF_Close\n"));
		CFG80211OS_ScanEnd(pAd->pCfg80211_CB, TRUE);
		if (pAd->cfg80211_ctrl.FlgCfg80211Scanning)
			pAd->cfg80211_ctrl.FlgCfg80211Scanning = FALSE;
		RT_MOD_DEC_USE_COUNT();
		ApCli_Close(pAd, dev_p);
	}

	RTMP_OS_NETDEV_STOP_QUEUE(dev_p);

	if (INFRA_ON(pAd))
		AsicEnableBssSync(pAd);
	else if (ADHOC_ON(pAd))
		AsicEnableIbssSync(pAd);
	else
		AsicDisableSync(pAd);

	/* VIRTUAL_IF_DOWN(pAd); */

	RT_MOD_DEC_USE_COUNT();

#endif /* RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE */

	wdev->wiphy->interface_modes = (wdev->wiphy->interface_modes)
	    & (~(BIT(NL80211_IFTYPE_P2P_CLIENT) | BIT(NL80211_IFTYPE_P2P_GO)));

	{
		extern const struct ieee80211_iface_combination *p_ra_iface_combinations_ap_sta;
		extern const INT ra_iface_combinations_ap_sta_num;
		wdev->wiphy->iface_combinations = p_ra_iface_combinations_ap_sta;
		wdev->wiphy->n_iface_combinations = ra_iface_combinations_ap_sta_num;
	}

	wdev->iftype = NL80211_IFTYPE_STATION;
	wdev->wiphy->software_iftypes |= BIT(NL80211_IFTYPE_P2P_DEVICE);

	return 0;
}

static INT CFG80211_DummyP2pIf_Ioctl(IN PNET_DEV dev_p, IN OUT VOID *rq_p, IN INT cmd)
{
	RTMP_ADAPTER *pAd;

	pAd = RTMP_OS_NETDEV_GET_PRIV(dev_p);
	ASSERT(pAd);

	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
		return -ENETDOWN;

	DBGPRINT(RT_DEBUG_TRACE, ("%s --->\n", __func__));

	return rt28xx_ioctl(dev_p, rq_p, cmd);

}

static INT CFG80211_DummyP2pIf_PacketSend(
#ifdef RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE
						 struct sk_buff *skb, PNET_DEV dev_p)
#else
						 IN PNDIS_PACKET skb_p, IN PNET_DEV dev_p)
#endif				/* RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE */
{
#ifdef RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE
	struct wifi_dev *wdev;

	DBGPRINT(RT_DEBUG_INFO, ("%s ---> %d\n", __func__, dev_p->ieee80211_ptr->iftype));

	if (!(RTMP_OS_NETDEV_STATE_RUNNING(dev_p))) {
		/* the interface is down */
		RELEASE_NDIS_PACKET(NULL, skb, NDIS_STATUS_FAILURE);
		return 0;
	}

	/* The device not ready to send packt. */
	wdev = RTMP_OS_NETDEV_GET_WDEV(dev_p);
	ASSERT(wdev);
	if (!wdev)
		return -1;

	return CFG80211_PacketSend(skb, dev_p, rt28xx_packet_xmit);

#else
	return 0;
#endif /* RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE */
}

VOID RTMP_CFG80211_DummyP2pIf_Remove(IN VOID *pAdSrc)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
	PCFG80211_CTRL cfg80211_ctrl = &pAd->cfg80211_ctrl;
	PNET_DEV dummy_p2p_net_dev = (PNET_DEV) cfg80211_ctrl->dummy_p2p_net_dev;
	struct wifi_dev *wdev = &cfg80211_ctrl->dummy_p2p_wdev;

	DBGPRINT(RT_DEBUG_TRACE, (" %s =====>\n", __func__));
	RtmpOSNetDevProtect(1);
	if (dummy_p2p_net_dev) {
#ifdef RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE
		RTMP_CFG80211_RemoveVifEntry(pAd, dummy_p2p_net_dev);
#endif /* RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE */

		RTMP_OS_NETDEV_STOP_QUEUE(dummy_p2p_net_dev);
#ifdef RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE
		if (netif_carrier_ok(dummy_p2p_net_dev))
			netif_carrier_off(dummy_p2p_net_dev);
#endif /* RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE */
		RtmpOSNetDevDetach(dummy_p2p_net_dev);
		rtmp_wdev_idx_unreg(pAd, wdev);
		wdev->if_dev = NULL;

		if (dummy_p2p_net_dev->ieee80211_ptr) {
			kfree(dummy_p2p_net_dev->ieee80211_ptr);
			dummy_p2p_net_dev->ieee80211_ptr = NULL;
		}

		RtmpOSNetDevProtect(0);
		RtmpOSNetDevFree(dummy_p2p_net_dev);
		cfg80211_ctrl->dummy_p2p_net_dev = NULL;
		RtmpOSNetDevProtect(1);

		cfg80211_ctrl->flg_cfg_dummy_p2p_init = FALSE;
	}
	RtmpOSNetDevProtect(0);
	DBGPRINT(RT_DEBUG_TRACE, (" %s <=====\n", __func__));
}

VOID RTMP_CFG80211_DummyP2pIf_Init(IN VOID *pAdSrc)
{
#define INF_CFG80211_DUMMY_P2P_NAME "p2p"

	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) pAdSrc;
	CFG80211_CB *p80211CB = pAd->pCfg80211_CB;
	PCFG80211_CTRL cfg80211_ctrl = &pAd->cfg80211_ctrl;
	RTMP_OS_NETDEV_OP_HOOK netDevHook, *pNetDevOps;
	PNET_DEV new_dev_p;
	UINT32 MC_RowID = 0, IoctlIF = 0, Inf = INT_P2P;
	UINT preIfIndex = 0;
	struct wireless_dev *pWdev;
	struct wifi_dev *wdev = NULL;

	DBGPRINT(RT_DEBUG_TRACE, (" %s =====>\n", __func__));
	if (cfg80211_ctrl->flg_cfg_dummy_p2p_init != FALSE) {
		new_dev_p = cfg80211_ctrl->dummy_p2p_net_dev;
		pWdev = new_dev_p->ieee80211_ptr;
		pWdev->iftype = RT_CMD_80211_IFTYPE_P2P_DEVICE;
		/* interface_modes move from IF open to init */
		pWdev->wiphy->interface_modes |= (BIT(NL80211_IFTYPE_P2P_CLIENT)
						  | BIT(NL80211_IFTYPE_P2P_GO));
		pWdev->wiphy->software_iftypes |= BIT(NL80211_IFTYPE_P2P_DEVICE);
		{
			extern const struct ieee80211_iface_combination
			*p_ra_iface_combinations_p2p;
			extern const INT ra_iface_combinations_p2p_num;
			pWdev->wiphy->iface_combinations = p_ra_iface_combinations_p2p;
			pWdev->wiphy->n_iface_combinations = ra_iface_combinations_p2p_num;
		}
		DBGPRINT(RT_DEBUG_TRACE, (" %s <======== dummy p2p existed\n", __func__));
		return;
	}
#ifdef RT_CFG80211_P2P_SINGLE_DEVICE
	cfg80211_ctrl->P2POpStatusFlags = CFG_P2P_DISABLE;
#endif /* RT_CFG80211_P2P_SINGLE_DEVICE */

#if RT_CFG80211_P2P_SUPPORT
	cfg80211_ctrl->bP2pCliPmEnable = FALSE;
	cfg80211_ctrl->bPreKeepSlient = FALSE;
	cfg80211_ctrl->bKeepSlient = FALSE;
	cfg80211_ctrl->NoAIndex = MAX_LEN_OF_MAC_TABLE;
	cfg80211_ctrl->MyGOwcid = MAX_LEN_OF_MAC_TABLE;
	cfg80211_ctrl->CTWindows = 0;	/* CTWindows and OppPS parameter field */
#endif /* RT_CFG80211_P2P_SUPPORT */

	pNetDevOps = &netDevHook;

	/* init operation functions and flags */
	NdisZeroMemory(&netDevHook, sizeof(netDevHook));
	netDevHook.open = CFG80211_DummyP2pIf_Open;	/* device opem hook point */
	netDevHook.stop = CFG80211_DummyP2pIf_Close;	/* device close hook point */
	netDevHook.xmit = CFG80211_DummyP2pIf_PacketSend;	/* hard transmit hook point */
	netDevHook.ioctl = CFG80211_DummyP2pIf_Ioctl;	/* ioctl hook point */

	new_dev_p =
	    RtmpOSNetDevCreate(MC_RowID, &IoctlIF, Inf, preIfIndex, sizeof(PRTMP_ADAPTER),
			       INF_CFG80211_DUMMY_P2P_NAME);

	if (new_dev_p == NULL) {
		/* allocation fail, exit */
		DBGPRINT(RT_DEBUG_ERROR,
			 ("Allocate network device fail (CFG80211: Dummy P2P IF)...\n"));
		return;
	} else {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("Register CFG80211 I/F (%s)\n", RTMP_OS_NETDEV_GET_DEVNAME(new_dev_p)));
	}

	RTMP_OS_NETDEV_SET_PRIV(new_dev_p, pAd);

	/* Set local administration bit for unique mac address of p2p0 */
	NdisMoveMemory(&pNetDevOps->devAddr[0], &pAd->CurrentAddress[0], MAC_ADDR_LEN);
	pNetDevOps->devAddr[0] += 2;
	COPY_MAC_ADDR(pAd->cfg80211_ctrl.P2PCurrentAddress, pNetDevOps->devAddr);
	AsicSetBssid(pAd, pAd->cfg80211_ctrl.P2PCurrentAddress);

	pNetDevOps->needProtcted = TRUE;

	pWdev = kzalloc(sizeof(*pWdev), GFP_KERNEL);
	if (unlikely(!pWdev)) {
		DBGPRINT(RT_DEBUG_ERROR, ("Could not allocate wireless device\n"));
		return;
	}

	new_dev_p->ieee80211_ptr = pWdev;
	pWdev->wiphy = p80211CB->pCfg80211_Wdev->wiphy;
	SET_NETDEV_DEV(new_dev_p, wiphy_dev(pWdev->wiphy));
	pWdev->netdev = new_dev_p;
	/*
	   pWdev->iftype = RT_CMD_80211_IFTYPE_STATION;
	   pWdev->wiphy->interface_modes = (pWdev->wiphy->interface_modes)
	   & (~(BIT(NL80211_IFTYPE_P2P_CLIENT)|
	   BIT(NL80211_IFTYPE_P2P_GO)));
	 */
	pWdev->iftype = RT_CMD_80211_IFTYPE_P2P_DEVICE;
	/* interface_modes move from IF open to init */
	pWdev->wiphy->interface_modes |= (BIT(NL80211_IFTYPE_P2P_CLIENT)
					  | BIT(NL80211_IFTYPE_P2P_GO));
	pWdev->wiphy->software_iftypes |= BIT(NL80211_IFTYPE_P2P_DEVICE);
	{
		extern const struct ieee80211_iface_combination *p_ra_iface_combinations_p2p;
		extern const INT ra_iface_combinations_p2p_num;
		pWdev->wiphy->iface_combinations = p_ra_iface_combinations_p2p;
		pWdev->wiphy->n_iface_combinations = ra_iface_combinations_p2p_num;
	}
	wdev = &cfg80211_ctrl->dummy_p2p_wdev;
	wdev->wdev_type = WDEV_TYPE_STA;
	wdev->sys_handle = (void *)pAd;
	wdev->if_dev = new_dev_p;
	COPY_MAC_ADDR(wdev->if_addr, pNetDevOps->devAddr);
	RTMP_OS_NETDEV_SET_PRIV(new_dev_p, pAd);
	RTMP_OS_NETDEV_SET_WDEV(new_dev_p, wdev);
	if (rtmp_wdev_idx_reg(pAd, wdev) < 0) {
		DBGPRINT(RT_DEBUG_ERROR,
			 ("===============> fail register the wdev for dummy p2p\n"));
	}

	RtmpOSNetDevAttach(pAd->OpMode, new_dev_p, pNetDevOps);
	cfg80211_ctrl->dummy_p2p_net_dev = new_dev_p;
	cfg80211_ctrl->flg_cfg_dummy_p2p_init = TRUE;
#ifdef RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE
	/* Record the pNetDevice to Cfg80211VifDevList */
	RTMP_CFG80211_AddVifEntry(pAd, new_dev_p, pWdev->iftype);
#endif /* RT_CFG80211_P2P_STATIC_CONCURRENT_DEVICE */

	DBGPRINT(RT_DEBUG_TRACE, (" %s <=====\n", __func__));
}
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */
#endif /* RT_CFG80211_SUPPORT */
