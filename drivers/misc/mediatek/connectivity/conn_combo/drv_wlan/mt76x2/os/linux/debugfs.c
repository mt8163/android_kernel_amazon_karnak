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
    debugfs.c

Abstract:
    debugfs related subroutines

    Revision History:
    Who         When          What
    --------    ----------    ----------------------------------------------
    Conard Chou   03-10-2015    created
*/

#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "rt_config.h"

struct dentry *mt_dfs_dir = NULL;

#define DFS_FILE_OPS(name)						\
static const struct file_operations mt_dfs_##name##_fops = {		\
	.read = mt_##name##_read,					\
	.write = mt_##name##_write,					\
	.open = mt_open_generic,					\
};

#define DFS_FILE_READ_OPS(name)						\
static const struct file_operations mt_dfs_##name##_fops = {		\
	.read = mt_##name##_read,					\
	.open = mt_open_generic,					\
};

#define DFS_FILE_WRITE_OPS(name)					\
static const struct file_operations mt_dfs_##name##_fops = {		\
	.write = mt_##name##_write,					\
	.open = mt_open_generic,					\
};



static int mt_open_generic(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}


static ssize_t mt_info_read(struct file *file, char __user *user_buf,
						size_t count, loff_t *ppos)
{
	ssize_t retval;
	char *buf, *p;
	size_t size = 1024;
	PRTMP_ADAPTER pad = (PRTMP_ADAPTER) file->private_data;
	unsigned int fw_ver = pad->FirmwareVersion;

	buf = kzalloc(size, GFP_KERNEL);
	p = buf;
	if (!p)
		return -ENOMEM;

	p += sprintf(p, "CHIP Ver:Rev=0x%08x\n", pad->MACVersion);
	p += sprintf(p, "DRI VER-%s  FW VER-%X.%X.%X-%s\n", STA_DRIVER_VERSION,
		(fw_ver & 0xff000000u) >> 24, (fw_ver & 0x00ff0000u) >> 16,
		(fw_ver & 0x0000ffffu), pad->FirmwareSubVer);
	retval = simple_read_from_buffer(user_buf, count, ppos, buf,
					(size_t) p - (size_t)buf);


	kfree(buf);
	return retval;
}


static ssize_t mt_debug_read(struct file *file, char __user *user_buf,
						size_t count, loff_t *ppos)
{
	ssize_t retval;
	char *buf;
	size_t size = 1024;
	size_t len = 0;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len = sprintf(buf, "debug read\n");
	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);

	kfree(buf);
	return retval;
}

static ssize_t mt_statlog_read(struct file *file, char __user *user_buf,
						size_t count, loff_t *ppos)
{
	ssize_t retval;
	char *buf, *p;
	unsigned long size = 2048;
	PRTMP_ADAPTER pad = (PRTMP_ADAPTER) file->private_data;

	buf = kzalloc(size, GFP_KERNEL);

	p = buf;
	if (!p)
		return -ENOMEM;

	memset(p, 0x00, size);
	RtmpIoctl_rt_private_get_statistics(pad, p, size);
	p = (char *)((size_t) p + strlen(p) + 1);   /* 1: size of '\0' */

	retval = simple_read_from_buffer(user_buf, count, ppos, buf,
					(size_t) p - (size_t)buf);

	kfree(buf);
	return retval;
}

static ssize_t mt_fwpath_write(struct file *file, const char __user *user_buf,
						size_t count, loff_t *ppos)
{
	char buf[16] = {0};
	PRTMP_ADAPTER pad = (PRTMP_ADAPTER) file->private_data;

	if (0 == copy_from_user(buf, user_buf, min(count, sizeof(buf))))
		DBGPRINT(RT_DEBUG_TRACE, ("%s:%s\n", __func__, buf));
	if (strncmp("r", buf, 1) == 0) {
		DBGPRINT(RT_DEBUG_OFF, ("%s:recorvey for ART reboot!\n", __func__));
		rtusb_unload(pad);
		rtusb_load();
	}
	return count;
}

static ssize_t mt_fwpath_read(struct file *file, char __user *user_buf,
						size_t count, loff_t *ppos)
{
	ssize_t retval;
	char *buf;
	size_t size = 1024;
	size_t len = 0;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	/* init ready = 1, unload progress = 2, unload ready = 3 */
	len = sprintf(buf, "%d", rtusb_reloadcheck());
	retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);

	kfree(buf);
	return retval;

}
#ifdef WCX_SUPPORT
#define NVRAM_QUERY_PERIOD 100
#define NVRAM_QUERY_TIMES 5
static ssize_t mt_nvram_write(struct file *file, const char __user *user_buf,
						size_t count, loff_t *ppos)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) file->private_data;
	char buf[16] = {0};
	UINT32 wait_cnt = 0;

	if (count <= 0)
		DBGPRINT(RT_DEBUG_ERROR, ("%s:invalid param\n", __func__));

	if (0 == copy_from_user(buf, user_buf, min(count, sizeof(buf)))) {
		DBGPRINT(RT_DEBUG_TRACE, ("mt_nvram_write:%s\n", buf));
		while (TRUE != pAd->chipOps.eeinit(pAd) && wait_cnt < NVRAM_QUERY_TIMES) {
			msleep(NVRAM_QUERY_PERIOD);
			wait_cnt++;
			DBGPRINT(RT_DEBUG_TRACE, ("Get NVRAM times(%d)\n", wait_cnt));
		}
		if (wait_cnt >= NVRAM_QUERY_TIMES) {
			DBGPRINT(RT_DEBUG_ERROR, ("Get NVRAM timeout\n"));
			pAd->nvramValid = FALSE;
		} else
			pAd->nvramValid = TRUE;
	} else
		return -EFAULT;

	return count;
}
#endif /* WCX_SUPPORT */

#ifdef SINGLE_SKU_V2
#define pwrtbl_op_size (32)
#define pwrtbl_reg_len (2)
static char pwrtbl_op[pwrtbl_op_size];
static char pwrtbl_reg[pwrtbl_reg_len];
static ssize_t mt_pwrtbl_write(struct file *file, const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	char buf[pwrtbl_op_size];
	PSTRING op = NULL;
	PSTRING country = NULL;

	memset(buf, 0x00, pwrtbl_op_size);
	if (count > pwrtbl_op_size)
		DBGPRINT(RT_DEBUG_ERROR, ("%s:invalid param\n", __func__));

	if (copy_from_user(buf, user_buf, min(count, sizeof(buf))))
		return -EFAULT;
	buf[strlen(buf)-1] = '\0';
	op = rstrtok(buf, " ");
	country = rstrtok(NULL, " ");
	if (!op)
		goto err0;

	if (!strcmp(op, "country") && !country)
		goto err0;

	memset(pwrtbl_op, 0x00, pwrtbl_op_size);
	memset(pwrtbl_reg, 0x00, pwrtbl_reg_len);
	NdisMoveMemory(pwrtbl_op, op, strlen(op));
	if (country)
		NdisMoveMemory(pwrtbl_reg, country, pwrtbl_reg_len);
err0:
	return count;
}

static ssize_t mt_pwrtbl_stat(PRTMP_ADAPTER ad, char *p)
{
	UINT32 data = 0;
	mt76x2_make_up_rate_pwr_table(ad);
	sprintf(p + strlen(p),
		"Formula: DefaultTargetPwr + rate_pwr = out_pwr\n");
	sprintf(p + strlen(p),
		"DefaultTargetPwr = %d\n",
		ad->DefaultTargetPwr);
	sprintf(p + strlen(p), "\n");

	/*
	   Bit 29:24 -> OFDM 12M/18M
	   Bit 21:16 -> OFDM 6M/9M
	   Bit 13:8 -> CCK 5.5M/11M
	   Bit 5:0 -> CCK 1M/2M
	 */
	sprintf(p + strlen(p),
		 "OFDM 12M/18M = %d\n",
		  ad->chipCap.rate_pwr_table.OFDM[2].mcs_pwr);
	sprintf(p + strlen(p),
		 "OFDM 6M/9M = %d\n",
		  ad->chipCap.rate_pwr_table.OFDM[0].mcs_pwr);
	sprintf(p + strlen(p),
		"CCK 5.5M/11M = %d\n",
		  ad->chipCap.rate_pwr_table.CCK[2].mcs_pwr);
	sprintf(p + strlen(p),
		"CCK[0] 1M/2M = %d\n",
		ad->chipCap.rate_pwr_table.CCK[0].mcs_pwr);
	RTMP_IO_READ32(ad, TX_PWR_CFG_0, &data);
	sprintf(p + strlen(p), "0x%x: 0x%08X\n", TX_PWR_CFG_0, data);
	sprintf(p + strlen(p), "\n");
	/*
	   Bit 29:24 -> HT/VHT1SS MCS 2/3
	   Bit 21:16 -> HT/VHT1SS MCS 0/1
	   Bit 13:8 -> OFDM 48M
	   Bit 5:0 -> OFDM 24M/36M
	 */
	sprintf(p + strlen(p),
		 "HT/VHT1SS MCS 2/3 = %d\n",
		  ad->chipCap.rate_pwr_table.HT[2].mcs_pwr);
	sprintf(p + strlen(p),
		 "HT/VHT1SS MCS 0/1 = %d\n",
		  ad->chipCap.rate_pwr_table.HT[0].mcs_pwr);
	sprintf(p + strlen(p),
		 "OFDM 48M = %d\n",
		  ad->chipCap.rate_pwr_table.OFDM[6].mcs_pwr);
	sprintf(p + strlen(p),
		 "OFDM 24M/36M = %d\n",
		  ad->chipCap.rate_pwr_table.OFDM[4].mcs_pwr);
	RTMP_IO_READ32(ad, TX_PWR_CFG_1, &data);
	sprintf(p + strlen(p), "0x%x: 0x%08X\n", TX_PWR_CFG_1, data);
	sprintf(p + strlen(p), "\n");
	/*
	   Bit 29:24 -> HT MCS 10/11 / VHT2SS MCS 2/3
	   Bit 21:16 -> HT MCS 8/9 / VHT2SS MCS 0/1
	   Bit 13:8 -> HT/VHT1SS MCS 6
	   Bit 5:0 -> HT/VHT1SS MCS 4/5
	 */
	sprintf(p + strlen(p),
		 "HT MCS 10/11 / VHT2SS MCS 2/3 = %d\n",
		  ad->chipCap.rate_pwr_table.HT[10].mcs_pwr);
	sprintf(p + strlen(p),
		 "HT MCS 8/9 / VHT2SS MCS 0/1 = %d\n",
		  ad->chipCap.rate_pwr_table.HT[8].mcs_pwr);
	sprintf(p + strlen(p),
		 "HT/VHT1SS MCS 6 = %d\n",
		  ad->chipCap.rate_pwr_table.HT[6].mcs_pwr);
	sprintf(p + strlen(p),
		 "HT/VHT1SS MCS 4/5 = %d\n",
		  ad->chipCap.rate_pwr_table.HT[4].mcs_pwr);

	RTMP_IO_READ32(ad, TX_PWR_CFG_2, &data);
	sprintf(p + strlen(p), "0x%x: 0x%08X\n", TX_PWR_CFG_2, data);
	sprintf(p + strlen(p), "\n");

	/*
	   Bit 29:24 -> HT/VHT STBC MCS 2/3
	   Bit 21:16 -> HT/VHT STBC MCS 0/1
	   Bit 13:8 -> HT MCS 14 / VHT2SS MCS 6
	   Bit 5:0 -> HT MCS 12/13 / VHT2SS MCS 4/5
	 */
	sprintf(p + strlen(p),
		 "HT/VHT STBC MCS 2/3 = %d\n",
		  ad->chipCap.rate_pwr_table.STBC[2].mcs_pwr);
	sprintf(p + strlen(p),
		 "HT/VHT STBC MCS 0/1 = %d\n",
		  ad->chipCap.rate_pwr_table.STBC[0].mcs_pwr);
	sprintf(p + strlen(p),
		 "HT MCS 14 / VHT2SS MCS 6 = %d\n",
		  ad->chipCap.rate_pwr_table.HT[14].mcs_pwr);
	sprintf(p + strlen(p),
		 "HT MCS 12/13 / VHT2SS MCS 4/5 = %d\n",
		  ad->chipCap.rate_pwr_table.HT[12].mcs_pwr);
	RTMP_IO_READ32(ad, TX_PWR_CFG_3, &data);
	sprintf(p + strlen(p), "0x%x: 0x%08X\n", TX_PWR_CFG_3, data);
	sprintf(p + strlen(p), "\n");

	/*
	   Bit 13:8 -> HT/VHT STBC MCS 6
	   Bit 5:0 -> HT/VHT STBC MCS 4/5
	 */
	sprintf(p + strlen(p),
		 "HT/VHT STBC MCS 6 = %d\n",
		  ad->chipCap.rate_pwr_table.STBC[6].mcs_pwr);
	sprintf(p + strlen(p),
		 "HT/VHT STBC MCS 4/5 = %d\n",
		  ad->chipCap.rate_pwr_table.STBC[4].mcs_pwr);
	RTMP_IO_READ32(ad, TX_PWR_CFG_4, &data);
	sprintf(p + strlen(p), "0x%x: 0x%08X\n", TX_PWR_CFG_4, data);
	sprintf(p + strlen(p), "\n");

	/*
	   Bit 29:24 -> VHT2SS MCS 9
	   Bit 21:16 -> HT/VHT1SS MCS 7
	   Bit 13:8 -> VHT2SS MCS 8
	   Bit 5:0 -> OFDM 54M
	 */
	sprintf(p + strlen(p),
		 "VHT2SS MCS 9 = %d\n",
		  ad->chipCap.rate_pwr_table.VHT2SS[9].mcs_pwr);
	sprintf(p + strlen(p),
		 "HT/VHT1SS MCS 7 = %d\n",
		  ad->chipCap.rate_pwr_table.HT[7].mcs_pwr);
	sprintf(p + strlen(p),
		 "VHT2SS MCS 8 = %d\n",
		  ad->chipCap.rate_pwr_table.VHT2SS[8].mcs_pwr);
	sprintf(p + strlen(p),
		 "OFDM 54M = %d\n",
		  ad->chipCap.rate_pwr_table.OFDM[7].mcs_pwr);
	RTMP_IO_READ32(ad, TX_PWR_CFG_7, &data);
	sprintf(p + strlen(p), "0x%x: 0x%08X\n", TX_PWR_CFG_7, data);
	sprintf(p + strlen(p), "\n");

	/*
	   Bit 29:24 -> VHT1SS MCS 9
	   Bit 21:16 -> VHT1SS MCS 8
	   Bit 5:0 -> HT MCS 15 / VHT2SS MCS 7
	 */
	sprintf(p + strlen(p),
		 "VHT1SS MCS 9 = %d\n",
		  ad->chipCap.rate_pwr_table.VHT1SS[9].mcs_pwr);
	sprintf(p + strlen(p),
		 "VHT1SS MCS 8 = %d\n",
		  ad->chipCap.rate_pwr_table.VHT1SS[8].mcs_pwr);
	sprintf(p + strlen(p),
		 "HT MCS 15 / VHT2SS MCS 7 = %d\n",
		  ad->chipCap.rate_pwr_table.HT[15].mcs_pwr);
	RTMP_IO_READ32(ad, TX_PWR_CFG_8, &data);
	sprintf(p + strlen(p), "0x%x: 0x%08X\n", TX_PWR_CFG_8, data);
	sprintf(p + strlen(p), "\n");

	/*
	   Bit 29:24 -> VHT STBC MCS 9
	   Bit 21:16 -> VHT STBC MCS 8
	   Bit 5:0 -> HT/VHT STBC MCS 7
	 */
	sprintf(p + strlen(p),
		"VHT STBC MCS 9 = %d\n",
		ad->chipCap.rate_pwr_table.STBC[9].mcs_pwr);
	sprintf(p + strlen(p),
		"VHT STBC MCS 8 = %d\n",
		ad->chipCap.rate_pwr_table.STBC[8].mcs_pwr);
	sprintf(p + strlen(p),
		"HT/VHT STBC MCS 7 = %d\n",
		ad->chipCap.rate_pwr_table.STBC[7].mcs_pwr);
	RTMP_IO_READ32(ad, TX_PWR_CFG_9, &data);
	sprintf(p + strlen(p), "0x%x: 0x%08X\n", TX_PWR_CFG_9, data);
	return strlen(p);
}

static ssize_t mt_pwrtbl_print(PRTMP_ADAPTER pAd, char *p, char *country, BOOLEAN in_use)
{
	struct _PRELOAD_SKU_LIST *list, *list_temp;
	CH_POWER *ch, *ch_temp;
	DL_LIST *tbl = NULL;

	DlListForEachSafe(list, list_temp, &pAd->PreloadSkuPwrList, struct _PRELOAD_SKU_LIST, List) {
		if (!(in_use && list->in_use) && strcmp(list->Country, country))
			continue;
		if (list->in_use)
			tbl = &pAd->SingleSkuPwrList;
		else
			tbl = &list->sku_pwr_list;

		sprintf(p + strlen(p), "Power Table of Country(%s):%c%c\n"
			, (list->in_use)?"inuse":"preload", list->Country[0], list->Country[1]);
		break;
	}

	if (!tbl) {
		sprintf(p + strlen(p), "Power table of %s is not found\n", country);
		goto err0;
	}

	DlListForEachSafe(ch, ch_temp, tbl, CH_POWER, List) {
		int i;
		sprintf(p + strlen(p), "start@ch %d, num %d\n", ch->StartChannel, ch->num);
		sprintf(p + strlen(p), "Channel: ");
		for (i = 0; i < ch->num; i++)
			sprintf(p + strlen(p), "%d ", ch->Channel[i]);
		sprintf(p + strlen(p), "\n");

		sprintf(p + strlen(p), "CCK: ");
		for (i = 0; i < SINGLE_SKU_TABLE_CCK_LENGTH; i++)
			sprintf(p + strlen(p), "%d ", ch->PwrCCK[i]);
		sprintf(p + strlen(p), "\n");

		sprintf(p + strlen(p), "OFDM: ");
		for (i = 0; i < SINGLE_SKU_TABLE_OFDM_LENGTH; i++)
			sprintf(p + strlen(p), "%d ", ch->PwrOFDM[i]);
		sprintf(p + strlen(p), "\n");

		sprintf(p + strlen(p), "HT/VHT20: ");
		for (i = 0; i < SINGLE_SKU_TABLE_HT_LENGTH; i++)
			sprintf(p + strlen(p), "%d ", ch->PwrHT20[i]);
		sprintf(p + strlen(p), "\n");

		sprintf(p + strlen(p), "HT/VHT40: ");
		for (i = 0; i < SINGLE_SKU_TABLE_HT_LENGTH; i++)
			sprintf(p + strlen(p), "%d ", ch->PwrHT40[i]);
		sprintf(p + strlen(p), "\n");

		sprintf(p + strlen(p), "VHT80: ");
		for (i = 0; i < SINGLE_SKU_TABLE_VHT_LENGTH; i++)
			sprintf(p + strlen(p), "%d ", ch->PwrVHT80[i]);
		sprintf(p + strlen(p), "\n");
	}

	sprintf(p + strlen(p), "\n");
err0:
	return strlen(p);
}

static ssize_t mt_pwrtbl_read(struct file *file, char __user *user_buf,
						size_t count, loff_t *ppos)
{	ssize_t retval = 0;
	char *buf, *p;
	unsigned long size = 8192;
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) file->private_data;

	buf = kzalloc(size, GFP_KERNEL);

	p = buf;
	if (!p)
		return -ENOMEM;

	memset(p, 0x00, size);

	if (!strcmp(pwrtbl_op, "country"))
		mt_pwrtbl_print(pAd, p, pwrtbl_reg, false);
	else if (!strcmp(pwrtbl_op, "stat"))
		mt_pwrtbl_stat(pAd, p);
	else if (!strcmp(pwrtbl_op, "inuse"))
		mt_pwrtbl_print(pAd, p, pwrtbl_reg, true);
	else {
		sprintf(p + strlen(p), "%s unknown operation %s strlen %zd\n"
			, __func__, pwrtbl_op, strlen(pwrtbl_op));
	}

	p = (char *)((size_t) p + strlen(p) + 1);   /* 1: size of '\0' */
	retval = simple_read_from_buffer(user_buf, count, ppos, buf,
					(size_t) p - (size_t)buf);
	kfree(buf);

	return retval;
}
#endif
static ssize_t mt_chinfo_read(struct file *file, char __user *user_buf,
						size_t count, loff_t *ppos)
{
	ssize_t retval;
	char *buf, *p;
	unsigned long size = 8192;
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) file->private_data;
	/* char reg_domain[5][7] = {"CE", "FCC", "JAP", "JAP_W53", "JAP_W56"}; */
	PCFG80211_CTRL cfg80211_ctrl = NULL;
	struct _CH_FLAGS_BEACON *flags_updated;
	CFG80211_CB *cfg80211_cb = NULL;
	struct wiphy *pWiphy = NULL;
	struct ieee80211_supported_band *band2g = NULL;
	struct ieee80211_supported_band *band5g = NULL;
	UINT32 band_id;
	UINT32 num_ch, ch_id;
	INT sup_band = 0;
	INT start_band = 0;
	CFG80211_BAND band_info;
	int i = 0;

	buf = kzalloc(size, GFP_KERNEL);

	p = buf;
	if (!p)
		return -ENOMEM;

	memset(p, 0x00, size);
	pWiphy = pAd->net_dev->ieee80211_ptr->wiphy;
	/* Init */
	RTMP_DRIVER_80211_BANDINFO_GET(pAd, &band_info);
	if (band_info.RFICType == 0)
		band_info.RFICType = RFIC_24GHZ | RFIC_5GHZ;
	/* 1. Calcute the Channel Number */
	if (band_info.RFICType & RFIC_5GHZ)
		band5g = pWiphy->bands[IEEE80211_BAND_5GHZ];
	band2g = pWiphy->bands[IEEE80211_BAND_2GHZ];

	start_band = (band_info.RFICType & RFIC_24GHZ)?0:1;
	if (band_info.RFICType & RFIC_24GHZ)
		sup_band++;

	if (band_info.RFICType & RFIC_5GHZ)
		sup_band++;

	sprintf(p + strlen(p), "Wiphy ch info\n");
	for (band_id = start_band; band_id < sup_band; band_id++) {
		struct ieee80211_supported_band *band_tmp = NULL;
		struct ieee80211_channel *channels = NULL;

		if (band_id == 0)
			band_tmp = band2g;
		else
			band_tmp = band5g;

		if (!band_tmp)
			goto fail;
		num_ch = band_tmp->n_channels;
		channels = band_tmp->channels;

		if (!channels)
			goto fail;

		for (ch_id = 0; ch_id < num_ch; ch_id++) {
			struct ieee80211_channel *ch = &channels[ch_id];
			UINT32 chidx = 0;
			chidx = ieee80211_frequency_to_channel(ch->center_freq);

			sprintf(p + strlen(p), "channel %d,\t", chidx);
			sprintf(p + strlen(p), "flag %x: ", ch->flags);
			if (ch->flags & CHANNEL_DISABLED) {
				sprintf(p + strlen(p), "%s\n", "DISABLED");
				continue;
			}

			if (ch->flags & CHANNEL_PASSIVE_SCAN)
				sprintf(p + strlen(p), "%s ", "PASSIVE_SCAN");
			if (ch->flags & CHANNEL_RADAR)
				sprintf(p + strlen(p), "%s ", "RADAR");
			sprintf(p + strlen(p), "\n");
		}
	}
	sprintf(p + strlen(p), "\n");

	RTMP_DRIVER_80211_CB_GET(pAd, &cfg80211_cb);
	flags_updated = cfg80211_cb->ch_flags_by_beacon;

	sprintf(p + strlen(p), "Flags by beacon2\n");
	for (i = 0; i < Num_Cfg80211_Chan; i++) {
		if (!&flags_updated[i])
			break;
		sprintf(p + strlen(p), "channel %d\t", flags_updated[i].ch);
		sprintf(p + strlen(p), "flag %x: ", flags_updated[i].flags);
		if (flags_updated[i].flags & CHANNEL_PASSIVE_SCAN)
			sprintf(p + strlen(p), "%s ", "PASSIVE_SCAN");
		if (flags_updated[i].flags & CHANNEL_RADAR)
			sprintf(p + strlen(p), "%s ", "RADAR");
		sprintf(p + strlen(p), "\n");
	}
	sprintf(p + strlen(p), "\n");
	sprintf(p + strlen(p), "DFS channel skip: %s\n"
		, (pAd->CommonCfg.bIEEE80211H_PASSIVE_SCAN)?"NO":"YES");
	for (i = 0; i < pAd->ChannelListNum; i++) {
		sprintf(p + strlen(p), "channel %d,\t", pAd->ChannelList[i].Channel);
		sprintf(p + strlen(p), "DFS:%s\t", (pAd->ChannelList[i].DfsReq)?"YES,":"NO,\t");
		sprintf(p + strlen(p), "flag: ");
		if (pAd->ChannelList[i].Flags & CHANNEL_PASSIVE_SCAN)
			sprintf(p + strlen(p), "%s ", "PASSIVE_SCAN");
		if (pAd->ChannelList[i].Flags & CHANNEL_RADAR)
			sprintf(p + strlen(p), "%s ", "RADAR");
		sprintf(p + strlen(p), "\n");
	}

fail:
	cfg80211_ctrl = &pAd->cfg80211_ctrl;
	if (cfg80211_ctrl->pCfg80211ChanList) {
		sprintf(p + strlen(p), "CFG Scan Channel List@%u: ", cfg80211_ctrl->Cfg80211ChanListLen);
		for (i = 0; i < cfg80211_ctrl->Cfg80211ChanListLen; i++)
			sprintf(p + strlen(p), "%u ", cfg80211_ctrl->pCfg80211ChanList[i]);
	} else
		sprintf(p + strlen(p), "No Scan Channel list");
	sprintf(p + strlen(p), "\n");
	p = (char *)((size_t) p + strlen(p) + 1);   /* 1: size of '\0' */

	retval = simple_read_from_buffer(user_buf, count, ppos, buf,
					(size_t) p - (size_t)buf);
	kfree(buf);
	return retval;
}

static ssize_t mt_chinfo_write(struct file *file, const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER) file->private_data;
	char buf[16] = {0};
	PSTRING op = NULL;
	PSTRING val = NULL;
	int err;
	long lval = 0;

	if (count > 16)
		DBGPRINT(RT_DEBUG_ERROR, ("%s:invalid param\n", __func__));

	if (copy_from_user(buf, user_buf, min(count, sizeof(buf))))
		return -EFAULT;

	buf[strlen(buf)-1] = '\0';
	op = rstrtok(buf, " ");
	val = rstrtok(NULL, " ");

	if (!op || !val)
		goto err0;

	err = os_strtol(val, 10, &lval);

	if (err)
		goto err0;

	pAd->CommonCfg.bIEEE80211H_PASSIVE_SCAN = (lval)?TRUE:FALSE;
err0:
	return count;
}

/* Add debugfs file operation */
DFS_FILE_READ_OPS(info);
DFS_FILE_READ_OPS(debug);
DFS_FILE_READ_OPS(statlog);
DFS_FILE_OPS(fwpath);
#ifdef WCX_SUPPORT
DFS_FILE_WRITE_OPS(nvram);
#endif /* WCX_SUPPORT */
#ifdef SINGLE_SKU_V2
DFS_FILE_OPS(pwrtbl);
#endif
DFS_FILE_OPS(chinfo);
/*
 * create debugfs root dir
 */
void mt_debugfs_init(void)
{
	char *dfs_dir_name = "mtwifi";

	if (!mt_dfs_dir)
		mt_dfs_dir = debugfs_create_dir(dfs_dir_name, NULL);


	if (!mt_dfs_dir) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: create %s dir fail!\n", __func__, dfs_dir_name));
		return;
	}
}

/*
 * create debugfs directory & file for each device
 */
void mt_dev_debugfs_init(PRTMP_ADAPTER pad)
{
	POS_COOKIE pobj = (POS_COOKIE) pad->OS_Cookie;

	DBGPRINT(RT_DEBUG_INFO, ("device debugfs init start!\n"));

	/* create device dir */
	pobj->debugfs_dev = debugfs_create_dir(pad->net_dev->name, mt_dfs_dir);

	if (!pobj->debugfs_dev) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: create %s dir fail!\n",
					__func__, pad->net_dev->name));
		return;
	}
	/* Add debugfs file */
	if (!debugfs_create_file("info", 0644, pobj->debugfs_dev, pad, &mt_dfs_info_fops)) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: create info file fail!\n", __func__));
		goto LABEL_CREATE_FAIL;
	}

	if (!debugfs_create_file("debug", 0644, pobj->debugfs_dev, pad, &mt_dfs_debug_fops)) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: create debug file fail!\n", __func__));
		goto LABEL_CREATE_FAIL;
	}

	if (!debugfs_create_file("statlog", 0644, pobj->debugfs_dev, pad, &mt_dfs_statlog_fops)) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: create statistics file fail!\n", __func__));
		goto LABEL_CREATE_FAIL;
	}

	if (!debugfs_create_file("fwpath", 0644, pobj->debugfs_dev, pad, &mt_dfs_fwpath_fops)) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: create statistics file fail!\n", __func__));
		goto LABEL_CREATE_FAIL;
	}
#ifdef WCX_SUPPORT
	if (!debugfs_create_file("nvram", 0644, pobj->debugfs_dev, pad, &mt_dfs_nvram_fops)) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: create statistics file fail!\n", __func__));
		goto LABEL_CREATE_FAIL;
	}
#endif /* WCX_SUPPORT */

#ifdef SINGLE_SKU_V2
	if (!debugfs_create_file("pwrtbl", 0644, pobj->debugfs_dev, pad, &mt_dfs_pwrtbl_fops)) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: create channel pwr table file fail!\n", __func__));
		goto LABEL_CREATE_FAIL;
	}
#endif
	if (!debugfs_create_file("chinfo", 0644, pobj->debugfs_dev, pad, &mt_dfs_chinfo_fops)) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s: create channel info file fail!\n", __func__));
		goto LABEL_CREATE_FAIL;
	}

	DBGPRINT(RT_DEBUG_INFO, ("debugfs init finish!\n"));
	return;

LABEL_CREATE_FAIL:
	debugfs_remove_recursive(pobj->debugfs_dev);
	pobj->debugfs_dev = NULL;
}

/*
 * remove debugfs directory and files for each device
 */
void mt_dev_debugfs_remove(PRTMP_ADAPTER pad)
{
	POS_COOKIE pobj = (POS_COOKIE) pad->OS_Cookie;

	if (pobj->debugfs_dev != NULL) {
		debugfs_remove_recursive(pobj->debugfs_dev);
		pobj->debugfs_dev = NULL;
		DBGPRINT(RT_DEBUG_INFO, ("debugfs remove finsih!\n"));
	}
}

/*
 * remove debugfs root dir
 */
void mt_debugfs_remove(void)
{
	if (mt_dfs_dir != NULL) {
		debugfs_remove(mt_dfs_dir);
		mt_dfs_dir = NULL;
	}
}
