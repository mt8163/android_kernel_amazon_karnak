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
 */
#ifndef	__ED_MONITOR_COMM_H__
#define __ED_MONITOR_COMM_H__

#ifdef ED_MONITOR

#include "rtmp.h"

extern INT ed_status_read(RTMP_ADAPTER *pAd);
extern VOID ed_monitor_init(RTMP_ADAPTER *pAd);
extern INT ed_monitor_exit(RTMP_ADAPTER *pAd);

/* let run-time turn on/off */
extern INT set_ed_chk_proc(RTMP_ADAPTER *pAd, PSTRING arg);

#ifdef CONFIG_AP_SUPPORT
extern INT set_ed_sta_count_proc(RTMP_ADAPTER *pAd, PSTRING arg);
extern INT set_ed_ap_count_proc(RTMP_ADAPTER *pAd, PSTRING arg);
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
extern INT set_ed_ap_scanned_count_proc(RTMP_ADAPTER *pAd, PSTRING arg);
extern INT set_ed_current_ch_ap_proc(RTMP_ADAPTER *pAd, PSTRING arg);
extern INT set_ed_current_rssi_threhold_proc(RTMP_ADAPTER *pAd, PSTRING arg);
#endif /* CONFIG_STA_SUPPORT */

extern INT set_ed_block_tx_thresh(RTMP_ADAPTER *pAd, PSTRING arg);
extern INT set_ed_false_cca_threshold(RTMP_ADAPTER *pAd, PSTRING arg);
extern INT set_ed_threshold(RTMP_ADAPTER *pAd, PSTRING arg);
extern INT show_ed_stat_proc(RTMP_ADAPTER *pAd, PSTRING arg);
#endif /* ED_MONITOR */

#endif /* __ED_MONITOR_COMM_H__ */
