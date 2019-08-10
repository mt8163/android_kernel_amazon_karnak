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

#include "rt_config.h"

#ifdef DOT11_VHT_AC

/*
	========================================================================
	Routine Description:
		Caller ensures we has 802.11ac support.
		Calls at setting VHT from AP/STASetinformation

	Arguments:
		pAd - Pointer to our adapter
		phymode  -

	========================================================================
*/
static VOID RTMPSetVHT(IN RTMP_ADAPTER *pAd)
{
	VHT_CAP_INFO *vht_cap = &pAd->CommonCfg.vht_cap_ie.vht_cap;

#ifdef VHT_TXBF_SUPPORT
	if (pAd->chipCap.FlgHwTxBfCap) {
		/* Set ETxBF */
		setVHTETxBFCap(pAd, vht_cap);
	}
#endif /* VHT_TXBF_SUPPORT */
}

VOID rtmp_set_vht(RTMP_ADAPTER *pAd, RT_PHY_INFO *phy_info)
{
	if (!phy_info)
		return;

	if (phy_info->bVhtEnable && (pAd->CommonCfg.vht_bw == VHT_BW_80))
		phy_info->vht_bw = VHT_BW_80;
	else
		phy_info->vht_bw = VHT_BW_2040;

}

INT SetCommonVHT(RTMP_ADAPTER *pAd)
{
	UCHAR cent_ch = 0;

	if (!WMODE_CAP_AC(pAd->CommonCfg.PhyMode)) {
		/* Clear previous VHT information */
		return FALSE;
	}

	RTMPSetVHT(pAd);

	pAd->CommonCfg.vht_cent_ch = vht_cent_ch_freq(pAd, pAd->CommonCfg.Channel);

	DBGPRINT(RT_DEBUG_TRACE,
		 ("%s(): Config VHT parameters!cent_ch=%d!vht_cent_ch = %d, vht_cent_ch2 = %d\n",
		  __func__, cent_ch, pAd->CommonCfg.vht_cent_ch, pAd->CommonCfg.vht_cent_ch2));
	return TRUE;
}

#endif /* DOT11_VHT_AC */
