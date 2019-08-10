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
	aironet.h

	Abstract:

	Revision History:
	Who			When			What
	--------	----------		----------------------------------------------
	Name		Date			Modification logs
*/

#ifndef	__ACTION_H__
#define	__ACTION_H__

struct _RTMP_ADAPTER;

VOID MlmeQOSAction(IN PRTMP_ADAPTER pAd, IN MLME_QUEUE_ELEM *Elem);

VOID MlmeDLSAction(IN PRTMP_ADAPTER pAd, IN MLME_QUEUE_ELEM *Elem);

VOID MlmeInvalidAction(IN PRTMP_ADAPTER pAd, IN MLME_QUEUE_ELEM *Elem);

VOID PeerRMAction(IN PRTMP_ADAPTER pAd, IN MLME_QUEUE_ELEM *Elem);

VOID PeerQOSAction(IN PRTMP_ADAPTER pAd, IN MLME_QUEUE_ELEM *Elem);

VOID PeerAddBAReqAction(IN PRTMP_ADAPTER pAd, IN MLME_QUEUE_ELEM *Elem);

VOID PeerAddBARspAction(IN PRTMP_ADAPTER pAd, IN MLME_QUEUE_ELEM *Elem);

VOID PeerDelBAAction(IN PRTMP_ADAPTER pAd, IN MLME_QUEUE_ELEM *Elem);

VOID PeerBAAction(IN PRTMP_ADAPTER pAd, IN MLME_QUEUE_ELEM *Elem);

VOID PeerHTAction(IN PRTMP_ADAPTER pAd, IN MLME_QUEUE_ELEM *Elem);

#ifdef DOT11_VHT_AC
VOID PeerVHTAction(IN PRTMP_ADAPTER pAd, IN MLME_QUEUE_ELEM *Elem);
#endif /* DOT11_VHT_AC */

VOID PeerPublicAction(IN PRTMP_ADAPTER pAd, IN MLME_QUEUE_ELEM *Elem);

#ifdef CONFIG_STA_SUPPORT
VOID StaPublicAction(IN PRTMP_ADAPTER pAd, IN BSS_2040_COEXIST_IE *pBss2040CoexIE);
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
VOID ApPublicAction(IN PRTMP_ADAPTER pAd, IN MLME_QUEUE_ELEM *Elem);
#endif /* CONFIG_AP_SUPPORT */

#ifdef QOS_DLS_SUPPORT
VOID PeerDLSAction(IN PRTMP_ADAPTER pAd, IN MLME_QUEUE_ELEM *Elem);
#endif /* QOS_DLS_SUPPORT */

#endif /* __ACTION_H__ */
