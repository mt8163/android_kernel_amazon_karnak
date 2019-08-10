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

#define RTMP_MODULE_OS

#include "rt_config.h"
#include "rtmp_comm.h"
#include "rt_os_util.h"
#include "rt_os_net.h"

#ifdef WCX_SUPPORT
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#ifdef WCX_FT_SUPPORT
#include <mach/mt_boot.h>
#endif /* WCX_FT_SUPPORT */
#endif /* WCX_SUPPORT */

#ifdef WOW_INPUTDEV_SUPPORT
#include <linux/input.h>
#endif /* WOW_INPUTDEV_SUPPORT */

#ifdef DEBUGFS_SUPPORT
#include "debugfs.h"
#endif

#include <linux/gpio.h>

extern USB_DEVICE_ID rtusb_dev_id[];
extern INT const rtusb_usb_id_len;

static BOOLEAN USBDevConfigInit(struct usb_device *dev, struct usb_interface *intf, VOID *pAd);

#ifndef PF_NOFREEZE
#define PF_NOFREEZE  0
#endif /* endif */

#ifdef PROFILE_PATH_DYNAMIC
static char *profilePath = STA_PROFILE_PATH;
module_param(profilePath, charp, S_IRUGO);
#endif /* PROFILE_PATH_DYNAMIC */

#ifdef RESUME_WITH_USB_RESET_SUPPORT
static VOID *gpAd = (VOID *) NULL;
#endif /* endif */

static int isreload;
static VOID rtusb_vendor_specific_check(struct usb_device *dev, VOID *pAd)
{

	RT_CMD_USB_MORE_FLAG_CONFIG Config = { dev->descriptor.idVendor,
		dev->descriptor.idProduct
	};
	RTMP_DRIVER_USB_MORE_FLAG_SET(pAd, &Config);
}

#ifdef RESUME_WITH_USB_RESET_SUPPORT
int rtusb_fast_probe(VOID *handle, VOID **ppAd, struct usb_interface *intf)
{
	VOID *pAd = *ppAd;
	VOID *pCookie = NULL;

	struct net_device *net_dev = NULL;
	struct usb_device *usb_dev = NULL;
#if (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)
	UCHAR WOWEable, WOWRun;
#endif /* (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT) */

	pCookie = RTMPCheckOsCookie(handle, &pAd);
	if (pCookie == NULL)
		return NDIS_STATUS_FAILURE;

	usb_dev = ((POS_COOKIE) pCookie)->pUsb_Dev;
	if (USBDevConfigInit(usb_dev, intf, pAd) == FALSE) {
		RTMPFreeAdapter(pAd);
		return NDIS_STATUS_FAILURE;
	}

	RTMP_DRIVER_USB_INIT(pAd, usb_dev, 0);

	RTMP_DRIVER_NET_DEV_GET(pAd, &net_dev);
	SET_NETDEV_DEV(net_dev, &(usb_dev->dev));
	set_wiphy_dev(net_dev->ieee80211_ptr->wiphy, &(usb_dev->dev));

/* RESUME */

#ifdef USB_SUPPORT_SELECTIVE_SUSPEND
	INT pm_usage_cnt;
	UCHAR Flag;
#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */

#if (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)
	RTMP_DRIVER_ADAPTER_RT28XX_WOW_STATUS(pAd, &WOWEable);
#endif /* (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT) */

#ifdef USB_SUPPORT_SELECTIVE_SUSPEND
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
	pm_usage_cnt = atomic_read(&intf->pm_usage_cnt);
#else
	pm_usage_cnt = intf->pm_usage_cnt;
#endif /* endif */

	if (pm_usage_cnt <= 0)
		usb_autopm_get_interface(intf);

	DBGPRINT(RT_DEBUG_ERROR, ("%s():=>autosuspend\n", __func__));

	RTMP_DRIVER_ADAPTER_SUSPEND_CLEAR(pAd);

#if (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)
	if (WOWEable == FALSE)
#endif /* (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT) */
		RTMP_DRIVER_ADAPTER_RT28XX_USB_ASICRADIO_ON(pAd);

	DBGPRINT(RT_DEBUG_ERROR, ("%s(): <=autosuspend\n", __func__));

	return NDIS_STATUS_SUCCESS;
#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */

#if (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)
	RTMP_DRIVER_ADAPTER_RT28XX_WOW_RUNSTATUS(pAd, &WOWRun);

	if ((WOWEable == TRUE) && (WOWRun == TRUE))
			RTMP_DRIVER_ADAPTER_RT28XX_WOW_DISABLE(pAd);

	if (WOWRun == TRUE)
		;
	else
#endif /* (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT) */
	{

		DBGPRINT(RT_DEBUG_ERROR, ("%s :radio_on\n", __func__));
		RTMP_DRIVER_ADAPTER_SUSPEND_CLEAR(pAd);
		RTMP_DRIVER_ADAPTER_RT28XX_CMD_RADIO_ON(pAd);

		RTMP_DRIVER_USB_RESUME(pAd);
	}

	DBGPRINT(RT_DEBUG_TRACE, ("<=%s()\n", __func__));
	return NDIS_STATUS_SUCCESS;
}
#endif /* RESUME_WITH_USB_RESET_SUPPORT */

#ifdef WOW_INPUTDEV_SUPPORT
#define WOWLAN_NAME "WOWLAN"
#ifdef WCX_WOW_SUPPORT
static struct input_dev *g_wlan_input_dev;
#endif /* WCX_WOW_SUPPORT */
static int WowRegisterInputDevice(RTMP_ADAPTER *pAd)
{
	int ret = 0;
	struct input_dev *input;
	DBGPRINT(RT_DEBUG_TRACE, ("%s\n", __func__));
	pAd->input_key = NULL;
	input = input_allocate_device();
	if (!input) {
		ret = -ENOMEM;
		goto err1;
	}

	/* Indicate that we generate key events */
	__set_bit(EV_KEY, input->evbit);

	/* Indicate that we generate *any* key event */
	__set_bit(KEY_POWER, input->keybit);
	input->name = WOWLAN_NAME;
	ret = input_register_device(input);
	if (ret) {
		DBGPRINT(RT_DEBUG_ERROR, ("register fail(%d)\n", ret));
		goto err2;
	}
	pAd->input_key = input;
#ifdef WCX_WOW_SUPPORT
	g_wlan_input_dev = input;
#endif /* WCX_WOW_SUPPORT */
	DBGPRINT(RT_DEBUG_TRACE, ("%s, register input device success!\n", __func__));

	return ret;

err2:
	input_free_device(input);
err1:
	return ret;
}

static int WowUnRegisterInputDevice(RTMP_ADAPTER *pAd)
{
	if (pAd->input_key)
		input_unregister_device(pAd->input_key);
	pAd->input_key = NULL;

	return 0;
}

static int WowInputReportKey(RTMP_ADAPTER *pAd)
{
	DBGPRINT(RT_DEBUG_ERROR, ("%s, send KEY_POWER to input device!\n", __func__));

	input_report_key(pAd->input_key, KEY_POWER, 1);
	input_sync(pAd->input_key);

	input_report_key(pAd->input_key, KEY_POWER, 0);
	input_sync(pAd->input_key);

	return 0;
}
#endif /* WOW_INPUTDEV_SUPPORT */

#ifdef WCX_WOW_SUPPORT
static irqreturn_t mt76xx_wow_isr(int irq, void *dev)
{
	struct _RTMP_ADAPTER *pAd = (struct _RTMP_ADAPTER *)dev;
	DBGPRINT(RT_DEBUG_ERROR, ("%s()\n", __func__));
	pAd->wow_trigger = 1;
	WowInputReportKey(pAd);
	disable_irq_nosync(pAd->wow_irq);
	return IRQ_HANDLED;
}

static int mt76xxRegisterWowIrq(RTMP_ADAPTER *pAd)
{
	struct device_node *eint_node = NULL;
	eint_node = of_find_compatible_node(NULL, NULL, "mediatek, mt76xx_wifi_ctrl");
	if (eint_node) {
		DBGPRINT(RT_DEBUG_TRACE, ("Get mt76xx_ctrl compatible node\n"));
		pAd->wow_irq = irq_of_parse_and_map(eint_node, 0);
		if (pAd->wow_irq) {
			int interrupts[2];
			of_property_read_u32_array(eint_node, "interrupts",
						   interrupts, ARRAY_SIZE(interrupts));
			pAd->wow_irqlevel = interrupts[1];
			if (request_irq(pAd->wow_irq, mt76xx_wow_isr,
					pAd->wow_irqlevel, INF_MAIN_DEV_NAME, pAd))
				DBGPRINT(RT_DEBUG_ERROR, ("MT76xx WOWIRQ LINE NOT AVAILABLE!!\n"));
			else {
				irq_set_irq_wake(((struct _RTMP_ADAPTER *)pAd)->wow_irq, TRUE);
				disable_irq_nosync(pAd->wow_irq);
				pAd->wow_trigger = 0;
			}

		} else
			DBGPRINT(RT_DEBUG_ERROR, ("can't find mt76xx_ctrl irq\n"));

	} else {
		pAd->wow_irq = 0;
		DBGPRINT(RT_DEBUG_ERROR, ("can't find mt76xx_ctrl compatible node\n"));
	}
	return 0;
}

static int mt76xxUnRegisterWowIrq(RTMP_ADAPTER *pAd)
{
	if (pAd->wow_irq)
		free_irq(pAd->wow_irq, pAd);
	return 0;
}
#endif /* WCX_WOW_SUPPORT */

#ifdef CONFIG_IDME
#define IDME_OF_MAC_ADDR        "/idme/mac_addr"
#define IDME_OF_WIFI_MFG        "/idme/wifi_mfg"

void idme_get_mac_addr(IN PRTMP_ADAPTER pAd)
{
	struct device_node *ap;
	int len, i;
	int ret = 0;
	char buf[3] = {0};

	ap = of_find_node_by_path(IDME_OF_MAC_ADDR);
	if (likely(ap)) {
		const char *mac_addr = of_get_property(ap, "value", &len);
		if (likely(len >= 12)) {
			for (i = 0; i < 12; i += 2) {
				buf[0] = mac_addr[i];
				buf[1] = mac_addr[i + 1];
				ret = kstrtou8(buf, 16, &pAd->CurrentAddress[i/2]);
				if (ret)
					DBGPRINT(RT_DEBUG_ERROR, ("[%d]kstrtou8 failed\n", i));
			}
			DBGPRINT(RT_DEBUG_TRACE, ("(IDME)MAC:=%02x:%02x:%02x:%02x:%02x:%02x\n",
				PRINT_MAC(pAd->CurrentAddress)));
		} else
			DBGPRINT(RT_DEBUG_ERROR, ("idme mac len err=%d\n", len));
	} else
		DBGPRINT(RT_DEBUG_ERROR, ("no idme mac\n"));
}

int idme_get_wifi_mfg(IN PRTMP_ADAPTER pAd)
{
	struct device_node *ap;
	int len, i;
	int ret = 0;
	char buf[3] = {0};

	ap = of_find_node_by_path(IDME_OF_WIFI_MFG);
	if (likely(ap)) {
		const char *wifi_mfg = of_get_property(ap, "value", &len);
		if (likely(len >= 1024)) {
			for (i = 0; i < 1024;  i += 2) {
				buf[0] = wifi_mfg[i];
				buf[1] = wifi_mfg[i+1];
				ret = kstrtou8(buf, 16, &pAd->EEPROMImage[i/2]);
				if (ret)
					DBGPRINT(RT_DEBUG_ERROR, ("[%d]kstrtou8 failed\n", i));
			}
			hex_dump("IDME_MFG", pAd->EEPROMImage, MAX_EEPROM_BIN_FILE_SIZE);
			DBGPRINT(RT_DEBUG_OFF, ("Load idme mac successful!\n"));
		} else {
			DBGPRINT(RT_DEBUG_ERROR, ("idme wifi_mfg len err=%d\n", len));
			hex_dump("IDME_MFG_ERR", wifi_mfg, len);
			ret = -1;
		}
	} else {
		DBGPRINT(RT_DEBUG_ERROR, ("no idme wifi_mfg(%s)\n", IDME_OF_WIFI_MFG));
		ret = -1;
	}

	return ret;
}
#endif /* CONFIG_IDME */

static int rt2870_probe(struct usb_interface *intf,
			struct usb_device *usb_dev, const USB_DEVICE_ID *dev_id, VOID **ppAd)
{
	struct net_device *net_dev = NULL;
#ifdef RESUME_WITH_USB_RESET_SUPPORT
	VOID *pAd = (VOID *) gpAd;
#else
	VOID *pAd = (VOID *) NULL;
#endif /* RESUME_WITH_USB_RESET_SUPPORT */

	INT status, rv;
	PVOID handle;
	RTMP_OS_NETDEV_OP_HOOK netDevHook;
	ULONG OpMode;
#ifdef CONFIG_PM
#ifdef USB_SUPPORT_SELECTIVE_SUSPEND
/*	INT pm_usage_cnt; */
	INT res = 1;
#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */
#endif /* CONFIG_PM */

	DBGPRINT(RT_DEBUG_TRACE, ("===>rt2870_probe()!\n"));

#ifdef CONFIG_PM
#ifdef USB_SUPPORT_SELECTIVE_SUSPEND
	res = usb_autopm_get_interface(intf);
	if (res) {
		DBGPRINT(RT_DEBUG_ERROR, ("rt2870_probe autopm_resume fail ------\n"));
		return -EIO;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
	atomic_set(&intf->pm_usage_cnt, 1);
	printk(" rt2870_probe ====> pm_usage_cnt %d\n", atomic_read(&intf->pm_usage_cnt));
#else
	intf->pm_usage_cnt = 1;
	printk(" rt2870_probe ====> pm_usage_cnt %d\n", intf->pm_usage_cnt);
#endif /* endif */

#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */
#endif /* CONFIG_PM */

	os_alloc_mem(NULL, (UCHAR **) &handle, sizeof(struct os_cookie));
	if (handle == NULL) {
		printk("rt2870_probe(): Allocate memory for os handle failed!\n");
		return -ENOMEM;
	}
	memset(handle, 0, sizeof(struct os_cookie));

	((POS_COOKIE) handle)->pUsb_Dev = usb_dev;

#ifdef CONFIG_PM
#ifdef USB_SUPPORT_SELECTIVE_SUSPEND
	((POS_COOKIE) handle)->intf = intf;
#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */
#endif /* CONFIG_PM */

	/* set/get operators to/from DRIVER module */

#ifdef RESUME_WITH_USB_RESET_SUPPORT
	if (rtusb_fast_probe(handle, &pAd, intf) == NDIS_STATUS_SUCCESS) {
		*ppAd = pAd;
		goto fast_probe_done;
	}
#endif /* RESUME_WITH_USB_RESET_SUPPORT */

	rv = RTMPAllocAdapterBlock(handle, &pAd);
	if (rv != NDIS_STATUS_SUCCESS) {
		os_free_mem(NULL, handle);
		goto err_out;
	}

	if (USBDevConfigInit(usb_dev, intf, pAd) == FALSE)
		goto err_out_free_radev;

	RTMP_DRIVER_USB_INIT(pAd, usb_dev, dev_id->driver_info);

	net_dev = RtmpPhyNetDevInit(pAd, &netDevHook);
	if (net_dev == NULL)
		goto err_out_free_radev;

	/* Here are the net_device structure with usb specific parameters. */
#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
	/* for supporting Network Manager.
	 * Set the sysfs physical device reference for the network logical device if set prior to registration will
	 * cause a symlink during initialization.
	 */
	SET_NETDEV_DEV(net_dev, &(usb_dev->dev));
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
/*    pAd->StaCfg.OriDevType = net_dev->type; */
	RTMP_DRIVER_STA_DEV_TYPE_SET(pAd, net_dev->type);
#ifdef PROFILE_PATH_DYNAMIC
	RTMP_DRIVER_STA_PROFILEPATH_SET(pAd, (ULONG) profilePath);
#endif /* PROFILE_PATH_DYNAMIC */
#endif /* CONFIG_STA_SUPPORT */

/*All done, it's time to register the net device to linux kernel. */
	/* Register this device */
#ifdef RT_CFG80211_SUPPORT
	{
/*	pAd->pCfgDev = &(usb_dev->dev); */
/*	pAd->CFG80211_Register = CFG80211_Register; */
/*	RTMP_DRIVER_CFG80211_INIT(pAd, usb_dev); */

		/*
		   In 2.6.32, cfg80211 register must be before register_netdevice();
		   We can not put the register in rt28xx_open();
		   Or you will suffer NULL pointer in list_add of
		   cfg80211_netdev_notifier_call().
		 */
		CFG80211_Register(pAd, &(usb_dev->dev), net_dev);
	}
#endif /* RT_CFG80211_SUPPORT */

	RTMP_DRIVER_OP_MODE_GET(pAd, &OpMode);
	status = RtmpOSNetDevAttach(OpMode, net_dev, &netDevHook);
	if (status != 0)
		goto err_out_free_netdev;

#ifdef DEBUGFS_SUPPORT
	mt_dev_debugfs_init(pAd);
#endif /* DEBUGFS_SUPPORT */

/*#ifdef KTHREAD_SUPPORT */

	*ppAd = pAd;
#ifdef RESUME_WITH_USB_RESET_SUPPORT
	gpAd = pAd;
#endif /* RESUME_WITH_USB_RESET_SUPPORT */

#ifdef INF_PPA_SUPPORT
	RTMP_DRIVER_INF_PPA_INIT(pAd);
#endif /* INF_PPA_SUPPORT */

#ifdef PRE_ASSIGN_MAC_ADDR
	{
		UCHAR PermanentAddress[MAC_ADDR_LEN];
		RTMP_DRIVER_MAC_ADDR_GET(pAd, &PermanentAddress[0]);
		DBGPRINT(RT_DEBUG_TRACE, ("%s():MAC Addr - %02x:%02x:%02x:%02x:%02x:%02x\n",
					  __func__, PermanentAddress[0], PermanentAddress[1],
					  PermanentAddress[2], PermanentAddress[3],
					  PermanentAddress[4], PermanentAddress[5]));

		/* Set up the Mac address */
		RtmpOSNetDevAddrSet(OpMode, net_dev, &PermanentAddress[0], NULL);
	}
#endif /* PRE_ASSIGN_MAC_ADDR */

#ifdef EXT_BUILD_CHANNEL_LIST
	RTMP_DRIVER_SET_PRECONFIG_VALUE(pAd);
#endif /* EXT_BUILD_CHANNEL_LIST */
#ifdef WCX_FT_SUPPORT
	{
		BOOTMODE boot_mode;
		boot_mode = get_boot_mode();
		if (boot_mode == FACTORY_BOOT) {
			DBGPRINT(RT_DEBUG_TRACE,
				 ("%s, open in Factory boot, ifc_up itself\n", __func__));
			rt28xx_open(net_dev);
		}
	}
#endif /* WCX_FT_SUPPORT */
#ifdef WCX_WOW_SUPPORT
	mt76xxRegisterWowIrq(pAd);
#endif /* WCX_WOW_SUPPORT */
#ifdef WOW_INPUTDEV_SUPPORT
	WowRegisterInputDevice(pAd);
#endif /* WOW_INPUTDEV_SUPPORT */
	if (!rtusb_reloadcheck())
		rtusb_reloadset(1); /* probe completed */
	else
		rtusb_reloadset(3); /* reload ready */
	DBGPRINT(RT_DEBUG_TRACE, ("<===rt2870_probe()!\n"));

	return 0;

	/* --------------------------- ERROR HANDLE --------------------------- */
err_out_free_netdev:
	RtmpOSNetDevFree(net_dev);
	net_dev = NULL;

err_out_free_radev:
	RTMPFreeAdapter(pAd);

err_out:
	*ppAd = NULL;

	return -1;

#ifdef RESUME_WITH_USB_RESET_SUPPORT
fast_probe_done:
	printk("fast probe done\n");
	return 0;
#endif /* RESUME_WITH_USB_RESET_SUPPORT */
}

/*
========================================================================
Routine Description:
    Release allocated resources.

Arguments:
    *dev				Point to the PCI or USB device
	pAd					driver control block pointer

Return Value:
    None

Note:
========================================================================
*/
static void rt2870_disconnect(struct usb_device *dev, VOID *pAd)
{
	struct net_device *net_dev;

	DBGPRINT(RT_DEBUG_ERROR, ("rtusb_disconnect: unregister usbnet usb-%s-%s\n",
				  dev->bus->bus_name, dev->devpath));
	if (!pAd) {
		usb_put_dev(dev);

		printk("rtusb_disconnect: pAd == NULL!\n");
		return;
	}
/*	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST); */
	RTMP_DRIVER_NIC_NOT_EXIST_SET(pAd);

	/* for debug, wait to show some messages to /proc system */
	udelay(1);

	RTMP_DRIVER_NET_DEV_GET(pAd, &net_dev);

	RtmpPhyNetDevExit(pAd, net_dev);

	/* FIXME: Shall we need following delay and flush the schedule?? */
	udelay(1);
	flush_scheduled_work();
	udelay(1);

#ifdef RT_CFG80211_SUPPORT
	RTMP_DRIVER_80211_UNREGISTER(pAd, net_dev);
#endif /* RT_CFG80211_SUPPORT */

	/* free the root net_device */
/* RtmpOSNetDevFree(net_dev); */
#ifdef WCX_WOW_SUPPORT
	mt76xxUnRegisterWowIrq(pAd);
#endif /* WCX_WOW_SUPPORT */
#ifdef WOW_INPUTDEV_SUPPORT
	DBGPRINT(RT_DEBUG_TRACE, ("WowRegisterInputDevice...\n"));
	WowUnRegisterInputDevice(pAd);
#endif /* WOW_INPUTDEV_SUPPORT */

#ifdef DEBUGFS_SUPPORT
	mt_dev_debugfs_remove(pAd);
#endif /* DEBUGFS_SUPPORT */

	RtmpRaDevCtrlExit(pAd);

	/* free the root net_device */
	RtmpOSNetDevFree(net_dev);
	net_dev = NULL;

	/* release a use of the usb device structure */
	usb_put_dev(dev);
	udelay(1);



#ifdef RESUME_WITH_USB_RESET_SUPPORT
	gpAd = NULL;
#endif /* RESUME_WITH_USB_RESET_SUPPORT */

	DBGPRINT(RT_DEBUG_ERROR, (" RTUSB disconnect successfully\n"));
}

/**************************************************************************/
/**************************************************************************/
/*tested for kernel 2.6series */
/**************************************************************************/
/**************************************************************************/

#ifdef CONFIG_PM
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 10)
#define pm_message_t u32
#endif /* endif */
static int rtusb_suspend(struct usb_interface *intf, pm_message_t state)
{
	struct net_device *net_dev;
#if (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)
	UCHAR WOWEnable, INFRA_On, WOWRun;
#endif /* endif */
	VOID *pAd = usb_get_intfdata(intf);

	DBGPRINT(RT_DEBUG_ERROR, ("===> rtusb_suspend()\n"));
	if (!RTMP_TEST_FLAG((PRTMP_ADAPTER) pAd, fRTMP_ADAPTER_START_UP))
		goto out;
#if (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)
	RTMP_DRIVER_ADAPTER_RT28XX_WOW_STATUS(pAd, &WOWEnable);
	RTMP_DRIVER_ADAPTER_RT28XX_INFRA_STATUS(pAd, &INFRA_On);
#endif /* (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT) */

#ifdef USB_SUPPORT_SELECTIVE_SUSPEND
	UCHAR Flag;
	DBGPRINT(RT_DEBUG_ERROR, ("%s():=>autosuspend\n", __func__));

#if (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)
	if (WOWEnable == TRUE)
		;
	else
#endif /* (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT) */
	{
/*	if(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_IDLE_RADIO_OFF)) */
		RTMP_DRIVER_ADAPTER_END_DISSASSOCIATE(pAd);
		RTMP_DRIVER_ADAPTER_IDLE_RADIO_OFF_TEST(pAd, &Flag);
		if (!Flag) {
			/*RT28xxUsbAsicRadioOff(pAd); */
			RTMP_DRIVER_ADAPTER_RT28XX_USB_ASICRADIO_OFF(pAd);
		}
	}
	/*RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_SUSPEND); */
	RTMP_DRIVER_ADAPTER_SUSPEND_SET(pAd);
	return 0;
#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */

#if (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)
	/* Mask to prevent the case !INFRA_ON, !GO_ON, which will not enable wow for RC */
	/* if ((WOWEnable == TRUE) && ((INFRA_On == TRUE) || RTMP_CFG80211_VIF_P2P_GO_ON(pAd))) { */
	if (WOWEnable == TRUE) {
		if (RTMP_CFG80211_VIF_P2P_GO_ON(pAd) && (!IsRemotePassiveCh(pAd)))
			AsicDisableSync(pAd);

		RTMP_DRIVER_ADAPTER_RT28XX_WOW_ENABLE(pAd);
#ifdef WCX_WOW_SUPPORT
		enable_irq(((struct _RTMP_ADAPTER *)pAd)->wow_irq);
#endif /* WCX_WOW_SUPPORT */
	}
	RTMP_DRIVER_ADAPTER_RT28XX_WOW_RUNSTATUS(pAd, &WOWRun);

	if (WOWRun == FALSE) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s :radio_off\n",  __func__));
		RTMP_DRIVER_ADAPTER_RT28XX_CMD_RADIO_OFF(pAd);
	}

	if (WOWRun == TRUE)
		;
	else
#endif /* (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT) */
	{
		DBGPRINT(RT_DEBUG_TRACE, ("%s()=>\n", __func__));

		RTMP_DRIVER_NET_DEV_GET(pAd, &net_dev);
		netif_device_detach(net_dev);

		RTMP_DRIVER_USB_SUSPEND(pAd, netif_running(net_dev));
	}
out:
	DBGPRINT(RT_DEBUG_ERROR, ("<=%s()\n", __func__));
	return 0;
}

#ifdef RESUME_WITH_USB_RESET_SUPPORT
static int rtusb_resume(struct usb_interface *intf)
{
	/* host usb driver is not available yet when usb_device's resume function is callback */
	DBGPRINT(RT_DEBUG_OFF, ("<= Dummy %s()\n", __func__));
	return 0;
}
#else
static int rtusb_resume(struct usb_interface *intf)
{
	struct net_device *net_dev;
	VOID *pAd = usb_get_intfdata(intf);
#if (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)
	UCHAR WOWEnable, WOWRun;
#endif /* (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT) */

#ifdef USB_SUPPORT_SELECTIVE_SUSPEND
	INT pm_usage_cnt;
	UCHAR Flag;
#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */

	DBGPRINT(RT_DEBUG_ERROR, ("%s()=>\n", __func__));
	if (!RTMP_TEST_FLAG((PRTMP_ADAPTER) pAd, fRTMP_ADAPTER_START_UP))
		goto out;

#if (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)
	RTMP_DRIVER_ADAPTER_RT28XX_WOW_STATUS(pAd, &WOWEnable);
#endif /* (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT) */

#ifdef USB_SUPPORT_SELECTIVE_SUSPEND
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
	pm_usage_cnt = atomic_read(&intf->pm_usage_cnt);
#else
	pm_usage_cnt = intf->pm_usage_cnt;
#endif /* endif */

	if (pm_usage_cnt <= 0)
		usb_autopm_get_interface(intf);

	DBGPRINT(RT_DEBUG_ERROR, ("%s():=>autosuspend\n", __func__));

	/*RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_SUSPEND); */
	RTMP_DRIVER_ADAPTER_SUSPEND_CLEAR(pAd);

#if (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)
	if (WOWEnable == FALSE)
#endif /* (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT) */
		/*RT28xxUsbAsicRadioOn(pAd); */
		RTMP_DRIVER_ADAPTER_RT28XX_USB_ASICRADIO_ON(pAd);

	DBGPRINT(RT_DEBUG_ERROR, ("%s(): <=autosuspend\n", __func__));

	return 0;
#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */

#if (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT)
	RTMP_DRIVER_ADAPTER_RT28XX_WOW_RUNSTATUS(pAd, &WOWRun);

	if ((WOWEnable == TRUE) && (WOWRun == TRUE)) {
		RTMP_DRIVER_ADAPTER_RT28XX_WOW_DISABLE(pAd);

		if (RTMP_CFG80211_VIF_P2P_GO_ON(pAd) && (!IsRemotePassiveCh(pAd)))
			AsicEnableApBssSync(pAd);


#ifdef WCX_WOW_SUPPORT
		if (((struct _RTMP_ADAPTER *)pAd)->wow_trigger == 0)
			disable_irq_nosync(((struct _RTMP_ADAPTER *)pAd)->wow_irq);
		else
			((struct _RTMP_ADAPTER *)pAd)->wow_trigger = 0;
#endif /* WCX_WOW_SUPPORT */
	}

	if (WOWEnable == TRUE)
		;
	else
#endif /* (defined(WOW_SUPPORT) && defined(RTMP_MAC_USB)) || defined(NEW_WOW_SUPPORT) */
	{
		RTMP_DRIVER_ADAPTER_SUSPEND_CLEAR(pAd);
		DBGPRINT(RT_DEBUG_ERROR, ("%s :radio_on\n", __func__));
		RTMP_DRIVER_ADAPTER_RT28XX_CMD_RADIO_ON(pAd);

		RTMP_DRIVER_USB_RESUME(pAd);

		RTMP_DRIVER_NET_DEV_GET(pAd, &net_dev);
		netif_device_attach(net_dev);
		netif_start_queue(net_dev);
		netif_carrier_on(net_dev);
		netif_wake_queue(net_dev);
	}
out:
	DBGPRINT(RT_DEBUG_ERROR, ("<=%s()\n", __func__));
	return 0;
}
#endif /* RESUME_WITH_USB_RESET_SUPPORT */
#endif /* CONFIG_PM */

static BOOLEAN USBDevConfigInit(struct usb_device *dev, struct usb_interface *intf, VOID *pAd)
{
	struct usb_host_interface *iface_desc;
	ULONG BulkOutIdx;
	ULONG BulkInIdx;
	UINT32 i;
	RT_CMD_USB_DEV_CONFIG Config, *pConfig = &Config;

	/* get the active interface descriptor */
	iface_desc = intf->cur_altsetting;

	/* get # of enpoints  */
	pConfig->NumberOfPipes = iface_desc->desc.bNumEndpoints;
	DBGPRINT(RT_DEBUG_TRACE, ("NumEndpoints=%d\n", iface_desc->desc.bNumEndpoints));

	/* Configure Pipes */
	BulkOutIdx = 0;
	BulkInIdx = 0;

	for (i = 0; i < pConfig->NumberOfPipes; i++) {
		if ((iface_desc->endpoint[i].desc.bmAttributes == USB_ENDPOINT_XFER_BULK) &&
		    ((iface_desc->endpoint[i].desc.bEndpointAddress & USB_ENDPOINT_DIR_MASK) ==
		     USB_DIR_IN)) {
			if (BulkInIdx < 2) {
				pConfig->BulkInEpAddr[BulkInIdx++] =
				    iface_desc->endpoint[i].desc.bEndpointAddress;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
				pConfig->BulkInMaxPacketSize =
				    le2cpu16(iface_desc->endpoint[i].desc.wMaxPacketSize);
#else
				pConfig->BulkInMaxPacketSize =
				    iface_desc->endpoint[i].desc.wMaxPacketSize;
#endif /* LINUX_VERSION_CODE */

				DBGPRINT_RAW(RT_DEBUG_TRACE,
					     ("BULK IN MaxPacketSize = %d\n",
					      pConfig->BulkInMaxPacketSize));
				DBGPRINT_RAW(RT_DEBUG_TRACE,
					     ("EP address = 0x%2x\n",
					      iface_desc->endpoint[i].desc.bEndpointAddress));
			} else {
				DBGPRINT(RT_DEBUG_ERROR, ("Bulk IN endpoint nums large than 2\n"));
			}
		} else if ((iface_desc->endpoint[i].desc.bmAttributes == USB_ENDPOINT_XFER_BULK) &&
			   ((iface_desc->endpoint[i].desc.
			     bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT)) {
			if (BulkOutIdx < 6) {
				/* there are 6 bulk out EP. EP6 highest priority. */
				/* EP1-4 is EDCA.  EP5 is HCCA. */
				pConfig->BulkOutEpAddr[BulkOutIdx++] =
				    iface_desc->endpoint[i].desc.bEndpointAddress;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
				pConfig->BulkOutMaxPacketSize =
				    le2cpu16(iface_desc->endpoint[i].desc.wMaxPacketSize);
#else
				pConfig->BulkOutMaxPacketSize =
				    iface_desc->endpoint[i].desc.wMaxPacketSize;
#endif /* endif */

				DBGPRINT_RAW(RT_DEBUG_TRACE,
					     ("BULK OUT MaxPacketSize = %d\n",
					      pConfig->BulkOutMaxPacketSize));
				DBGPRINT_RAW(RT_DEBUG_TRACE,
					     ("EP address = 0x%2x\n",
					      iface_desc->endpoint[i].desc.bEndpointAddress));
			} else {
				DBGPRINT(RT_DEBUG_ERROR, ("Bulk Out endpoint nums large than 6\n"));
			}
		}
	}

	if (!(pConfig->BulkInEpAddr && pConfig->BulkOutEpAddr[0])) {
		printk("%s: Could not find both bulk-in and bulk-out endpoints\n", __func__);
		return FALSE;
	}

	pConfig->pConfig = &dev->config->desc;
	usb_set_intfdata(intf, pAd);
	RTMP_DRIVER_USB_CONFIG_INIT(pAd, pConfig);
	rtusb_vendor_specific_check(dev, pAd);

	return TRUE;

}

static int rtusb_probe(struct usb_interface *intf, const USB_DEVICE_ID *id)
{
	VOID *pAd;
	struct usb_device *dev;
	int rv;

	dev = interface_to_usbdev(intf);
	dev = usb_get_dev(dev);

	rv = rt2870_probe(intf, dev, id, &pAd);
	if (rv != 0)
		usb_put_dev(dev);
	return rv;
}

static void rtusb_disconnect(struct usb_interface *intf)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	VOID *pAd;

	pAd = usb_get_intfdata(intf);
	usb_set_intfdata(intf, NULL);

	rt2870_disconnect(dev, pAd);

#ifdef CONFIG_PM
#ifdef USB_SUPPORT_SELECTIVE_SUSPEND
	pr_info("rtusb_disconnect usb_autopm_put_interface\n");
	usb_autopm_put_interface(intf);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
	pr_info("%s() => pm_usage_cnt %d\n", __func__, atomic_read(&intf->pm_usage_cnt));
#else
	pr_info("%s() => pm_usage_cnt %d\n", __func__, intf->pm_usage_cnt);
#endif /* endif */
#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */
#endif /* CONFIG_PM */

}

struct usb_driver rtusb_driver = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 15)
	.owner = THIS_MODULE,
#endif /* endif */
	.name = RTMP_DRV_NAME,
	.probe = rtusb_probe,
	.disconnect = rtusb_disconnect,
	.id_table = rtusb_dev_id,

#ifdef CONFIG_PM
#ifdef USB_SUPPORT_SELECTIVE_SUSPEND
	.supports_autosuspend = 1,
#endif /* USB_SUPPORT_SELECTIVE_SUSPEND */
	.suspend = rtusb_suspend,
	.resume = rtusb_resume,
#endif /* CONFIG_PM */
};

static INT __init rtusb_init(void)
{
#define WIFI_INIT_SLEEP_TIME_MS     4000
	INT res = 0;

	printk("%s : %s --->\n", __func__, RTMP_DRV_NAME);
#ifdef DEBUGFS_SUPPORT
	mt_debugfs_init();
#endif
	res = usb_register(&rtusb_driver);
#ifdef MODULE_INIT_DELAY_SLEEP
	printk("%s : sleeping for %d ms\n", __func__, WIFI_INIT_SLEEP_TIME_MS);
	msleep(WIFI_INIT_SLEEP_TIME_MS);
#endif /* MODULE_INIT_DELAY_SLEEP */

	printk("Exiting %s\n", __func__);
	return res;
}

static VOID __exit rtusb_exit(void)
{
	usb_deregister(&rtusb_driver);
#ifdef DEBUGFS_SUPPORT
	mt_debugfs_remove();
#endif
	printk("<--- rtusb exit\n");
}

#if 0
#define WIFI_HW_RESET 104

static void wifi_hw_reset(void)
{
	int retval;

	retval = gpio_request(WIFI_HW_RESET, "wifi_hw_rst");
	if (retval) {
		DBGPRINT(RT_DEBUG_OFF, ("fail to requeset wifi_hw_rst :%d\n", WIFI_HW_RESET));
		return;
	}
	gpio_direction_output(WIFI_HW_RESET, 1);

	gpio_set_value(WIFI_HW_RESET, 0);
	DBGPRINT(RT_DEBUG_OFF, ("%s: gpio:%d (val: %d)\n", __func__, WIFI_HW_RESET, gpio_get_value(WIFI_HW_RESET)));
	mdelay(10);
	gpio_set_value(WIFI_HW_RESET, 1);
	DBGPRINT(RT_DEBUG_OFF, ("%s: gpio:%d (val: %d)\n", __func__, WIFI_HW_RESET, gpio_get_value(WIFI_HW_RESET)));
	gpio_free(WIFI_HW_RESET);
}
#endif

void rtusb_load(void)
{
	DBGPRINT(RT_DEBUG_OFF, ("<=%s()\n", __func__));
	usb_register(&rtusb_driver);
	DBGPRINT(RT_DEBUG_OFF, ("=>%s()\n", __func__));
}

void rtusb_unload(PRTMP_ADAPTER pAd)
{
	struct net_device *netdev = NULL;
	int wait_cnt = 0;

	DBGPRINT(RT_DEBUG_OFF, ("<=%s()\n", __func__));
	/* unload before HW reset */
	usb_deregister(&rtusb_driver);
	netdev = dev_get_by_name(&init_net, "wlan0");
	while (netdev != NULL && wait_cnt < 10) {
		DBGPRINT(RT_DEBUG_OFF, ("=>%s()(%d) wait wlan0 unload\n", __func__, wait_cnt));
		msleep(200);
		wait_cnt++;
		netdev = dev_get_by_name(&init_net, "wlan0");
	}
	rtusb_reloadset(2);
	/* wifi_hw_reset(); */
	msleep(1000); /* wait for reset */
	DBGPRINT(RT_DEBUG_OFF, ("=>%s()\n", __func__));
}

void rtusb_reloadset(int val)
{
	isreload = val;
}
int rtusb_reloadcheck(void)
{
	/* wlan up = 0 wlan ready = 1
	   wlan reload done not ready = 2 wlan reload ready = 3 */
	return isreload;
}
#ifndef MULTI_INF_SUPPORT

module_init(rtusb_init);
module_exit(rtusb_exit);

/* Following information will be show when you run 'modinfo' */
/* *** If you have a solution for the bug in current version of driver, please mail to me. */
/* Otherwise post to forum in ralinktech's web site(www.ralinktech.com) and let all users help you. *** */
MODULE_AUTHOR("Ralink");
MODULE_DESCRIPTION("Ralink Wireless Lan Linux Driver");
MODULE_LICENSE("GPL");

#ifdef CONFIG_STA_SUPPORT
#ifdef MODULE_VERSION
MODULE_VERSION(STA_DRIVER_VERSION);
#endif /* endif */
#endif /* CONFIG_STA_SUPPORT */

#endif /* MULTI_INF_SUPPORT */
