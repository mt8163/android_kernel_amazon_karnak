# ---------------------------------------------------
# Compile Options
# ---------------------------------------------------
#include $(MTK_PATH_BUILD)/Makefile

CHIPSET = MT76x2

MODULE_NAME := wlan_$(shell echo $(strip $(CHIPSET)) | tr A-Z a-z)

ccflags-y += -DAGGREGATION_SUPPORT
ccflags-y += -DPIGGYBACK_SUPPORT
ccflags-y += -DWMM_SUPPORT
ccflags-y += -DLINUX
ccflags-y += -DSYSTEM_LOG_SUPPORT
ccflags-y += -DRT28xx_MODE=STA
ccflags-y += -DCHIPSET=MT7662u
ccflags-y += -DDBG_DIAGNOSE
ccflags-y += -DDBG_RX_MCS
ccflags-y += -DDBG_TX_MCS
ccflags-y += -DCONFIG_RA_NAT_NONE
ccflags-y += -DDOT11_VHT_AC
ccflags-y += -DCAL_FREE_IC_SUPPORT
ccflags-y += -DCONFIG_STA_SUPPORT
ccflags-y += -DSCAN_SUPPORT
ccflags-y += -DDBG
ccflags-y += -DMT76XX_BTCOEX_SUPPORT
ccflags-y += -DADHOC_WPA2PSK_SUPPORT
ccflags-y += -DWPA_SUPPLICANT_SUPPORT
ccflags-y += -DSTATS_COUNT_SUPPORT
ccflags-y += -DRT_CFG80211_SUPPORT
ccflags-y += -DWPA_SUPPLICANT_SUPPORT
ccflags-y += -DRT_CFG80211_P2P_SUPPORT
ccflags-y += -DUAPSD_SUPPORT
ccflags-y += -DMBSS_SUPPORT
ccflags-y += -DAP_SCAN_SUPPORT
ccflags-y += -DCONFIG_AP_SUPPORT
ccflags-y += -DAPCLI_SUPPORT
ccflags-y += -DRT_CFG80211_P2P_CONCURRENT_DEVICE
ccflags-y += -DPROFILE_PATH_DYNAMIC
ccflags-y += -DACTIVE_CHANNEL_SCAN_SUPPORT
ccflags-y += -DRELEASE_EXCLUDE
ccflags-y += -DCONFIG_FPGA_MODE
ccflags-y += -DCONFIG_WIFI_TEST
#ccflags-y += -DCONFIG_CALIBRATION_COLLECTION
ccflags-y += -DCONFIG_FW_DEBUG
ccflags-y += -DMT76x2
ccflags-y += -DRT65xx
ccflags-y += -DRLT_MAC
ccflags-y += -DRLT_BBP
ccflags-y += -DMT_RF
ccflags-y += -DRTMP_MAC_USB
ccflags-y += -DRTMP_USB_SUPPORT
ccflags-y += -DRTMP_TIMER_TASK_SUPPORT
ccflags-y += -DA_BAND_SUPPORT
ccflags-y += -DRTMP_EFUSE_SUPPORT
ccflags-y += -DNEW_MBSSID_MODE
ccflags-y += -DCONFIG_ANDES_SUPPORT
ccflags-y += -DRTMP_RF_RW_SUPPORT
ccflags-y += -DDYNAMIC_VGA_SUPPORT
ccflags-y += -DNEW_RATE_ADAPT_SUPPORT
ccflags-y += -DFIFO_EXT_SUPPORT
ccflags-y += -DMT7662
ccflags-y += -DMT7612
ccflags-y += -DRALINK_ATE -DCONFIG_RT2880_ATE_CMD_NEW -DRALINK_QA
ccflags-y += -DFW_LOAD_FROM_BIN
ccflags-y += -DTXBF_SUPPORT -DVHT_TXBF_SUPPORT
ccflags-y += -DCFG80211_SCAN_SIGNAL_AVG
ccflags-y += -DHOST_DYNAMIC_VGA_SUPPORT
ccflags-y += -DCERTIFICATION_SIGMA_SUPPORT
ccflags-y += -DNEW_WOW_SUPPORT -DWOW_STA_MODE
ccflags-y += -DWOW_BUF_PKT_SUPPORT
ccflags-y += -DCONFIG_FW_DEBUG -DCONFIG_FW_CORE_DUMP
ccflags-y += -DED_MONITOR
ccflags-y += -DSINGLE_SKU_V2
ccflags-y += -DSTA_P2P_CONNCURRENT
# PLATFORM USAGE
ccflags-y += -DWCX_SUPPORT
#ccflags-y += -DWCX_FT_SUPPORT
ccflags-y += -DWCX_WOW_SUPPORT -DWOW_INPUTDEV_SUPPORT
ccflags-y += -DDEBUGFS_SUPPORT
#ccflags-y += -DMULTI_URB
ccflags-y += -g -Wall -Werror -Werror=strict-prototypes -Wno-trigraphs

ccflags-y += -DRTMP_FREQ_CALIBRATION_SUPPORT
ccflags-y += -DMULTI_CORE_SUPPORT
ccflags-y += -DKMALLOC_MEM_SUPPORT
ccflags-y += -DCMD_TIMEOUT_RECOVER_SUPPORT
#ccflags-y += -DFORCE_CUSTOMIZED_COUNTRY_REGION
ccflags-y += -DOLD_PRE_ALLOC
# ---------------------------------------------------
# Include Path
# ---------------------------------------------------
ccflags-y += -I$(src)/include -I$(src)/include/chip -I$(src)/include/eeprom
ccflags-y += -I$(src)/include/fpga -I$(src)/include/iface -I$(src)/include/mac_ral
ccflags-y += -I$(src)/include/mcu -I$(src)/include/os -I$(src)/include/phy
ccflags-y += -I$(src)/include/sniffer
ccflags-y += -I$(src)/ate/include

#include $(srctree)/drivers/misc/mediatek/Makefile.custom
ccflags-y += -I$(src)/linux/include

# ---------------------------------------------------
# Object List
# ---------------------------------------------------
obj-$(CONFIG_MTK_COMBO_CHIP_MT76x2) += $(MODULE_NAME).o

# RST CTL DEVICE
#obj-y +=mt_wifi_chrdev.o
#mt_wifi_chrdev-objs := os/linux/mt_wifi_chrdev.o

# ---------------------------------------------------
# Directory List
# ---------------------------------------------------
AP_DIR       := ap/
CHIP_DIR     := chips/
COMMON_DIR   := common/
MAC_DIR      := mac/
MCU_DIR      := mcu/
MGMT_DIR     := mgmt/
OS_DIR       := os/linux/
CFG80211_DIR := os/linux/cfg80211/
PHY_DIR      := phy/
RATECTRL_DIR := rate_ctrl/
STA_DIR      := sta/
TXRX_DIR     := tx_rx/
ATE_DIR      := ate/


# ---------------------------------------------------
# Objects List
# ---------------------------------------------------
AP_OBJS := $(AP_DIR)ap_apcli.o \
           $(AP_DIR)apcli_ctrl.o \
           $(AP_DIR)apcli_sync.o \
           $(AP_DIR)apcli_auth.o \
           $(AP_DIR)apcli_assoc.o \
           $(AP_DIR)ap_apcli_inf.o \
           $(AP_DIR)ap_mbss.o \
           $(AP_DIR)ap.o \
           $(AP_DIR)ap_assoc.o \
           $(AP_DIR)ap_auth.o \
           $(AP_DIR)ap_connect.o \
           $(AP_DIR)ap_mlme.o \
           $(AP_DIR)ap_sanity.o \
           $(AP_DIR)ap_sync.o \
           $(AP_DIR)ap_wpa.o \
           $(AP_DIR)ap_data.o \
           $(AP_DIR)ap_autoChSel.o \
           $(AP_DIR)ap_qload.o \
           $(AP_DIR)ap_cfg.o

CHIP_OBJS := $(CHIP_DIR)rtmp_chip.o \
             $(CHIP_DIR)mt76x2.o \
	     $(CHIP_DIR)rt65xx.o

COMMON_OBJS :=	$(COMMON_DIR)vht.o \
                $(COMMON_DIR)crypt_md5.o \
                $(COMMON_DIR)crypt_sha2.o \
                $(COMMON_DIR)crypt_hmac.o \
                $(COMMON_DIR)crypt_aes.o \
                $(COMMON_DIR)crypt_arc4.o \
                $(COMMON_DIR)mlme.o \
                $(COMMON_DIR)cmm_wep.o \
                $(COMMON_DIR)action.o \
                $(COMMON_DIR)cmm_data.o \
                $(COMMON_DIR)rtmp_init.o \
                $(COMMON_DIR)rtmp_init_inf.o \
                $(COMMON_DIR)cmm_tkip.o \
                $(COMMON_DIR)cmm_aes.o \
                $(COMMON_DIR)cmm_sync.o \
                $(COMMON_DIR)eeprom.o \
                $(COMMON_DIR)cmm_sanity.o \
                $(COMMON_DIR)cmm_info.o \
                $(COMMON_DIR)cmm_cfg.o \
                $(COMMON_DIR)cmm_wpa.o \
                $(COMMON_DIR)cmm_radar.o \
                $(COMMON_DIR)spectrum.o \
                $(COMMON_DIR)rtmp_timer.o \
                $(COMMON_DIR)rt_channel.o \
                $(COMMON_DIR)cmm_profile.o \
                $(COMMON_DIR)cmm_asic.o \
                $(COMMON_DIR)scan.o \
                $(COMMON_DIR)cmm_cmd.o \
                $(COMMON_DIR)uapsd.o \
                $(COMMON_DIR)ps.o \
                $(COMMON_DIR)sys_log.o \
                $(COMMON_DIR)txpower.o \
                $(COMMON_DIR)ba_action.o \
                $(COMMON_DIR)rt_os_util.o \
                $(COMMON_DIR)cmm_wpa_adhoc.o \
                $(COMMON_DIR)cmm_mac_usb.o \
                $(COMMON_DIR)cmm_data_usb.o \
                $(COMMON_DIR)rtusb_io.o \
                $(COMMON_DIR)rtusb_data.o \
                $(COMMON_DIR)rtusb_bulk.o \
                $(COMMON_DIR)ee_prom.o \
                $(COMMON_DIR)ee_efuse.o \
                $(COMMON_DIR)rtusb_dev_id.o \
                $(COMMON_DIR)frq_cal.o \
		$(COMMON_DIR)cmm_txbf.o \
		$(COMMON_DIR)cmm_txbf_cal.o \
		$(COMMON_DIR)ed_monitor.o \
		$(COMMON_DIR)frq_cal.o \
		$(COMMON_DIR)prealloc.o



MAC_OBJS := $(MAC_DIR)ral_nmac.o \
            $(MAC_DIR)rtmp_mac.o

MCU_OBJS := $(MCU_DIR)mcu.o \
            $(MCU_DIR)mcu_and.o \
	        $(MCU_DIR)bt_coex.o

MGMT_OBJS := $(MGMT_DIR)mgmt_entrytb.o \
	         $(MGMT_DIR)mgmt_ht.o \
	         $(MGMT_DIR)mgmt_vht.o
#$(MGMT_DIR)mgmt_hw.o \

CFG80211_OBJS := $(CFG80211_DIR)cfg80211.o \
                 $(CFG80211_DIR)cfg80211_util.o \
                 $(CFG80211_DIR)cfg80211_scan.o \
                 $(CFG80211_DIR)cfg80211_rx.o \
                 $(CFG80211_DIR)cfg80211_tx.o \
                 $(CFG80211_DIR)cfg80211_inf.o \
                 $(CFG80211_DIR)cfg80211_p2p.o \
                 $(CFG80211_DIR)cfg80211_ap.o \
                 $(CFG80211_DIR)cfg80211drv.o

OS_OBJS := $(OS_DIR)sta_ioctl.o \
           $(OS_DIR)ap_ioctl.o \
           $(OS_DIR)rt_profile.o \
           $(OS_DIR)rt_usb.o \
           $(OS_DIR)usb_main_dev.o \
           $(OS_DIR)rt_linux.o \
           $(OS_DIR)rt_usb_util.o \
           $(OS_DIR)rt_main_dev.o \
           $(OS_DIR)debugfs.o

PHY_OBJS := $(PHY_DIR)phy.o \
            $(PHY_DIR)rf.o \
            $(PHY_DIR)rlt_phy.o \
	    $(PHY_DIR)rt_rf.o \
            $(PHY_DIR)mt_rf.o

RATECTTL_OBJS := $(RATECTRL_DIR)ra_ctrl.o \
                 $(RATECTRL_DIR)alg_legacy.o \
                 $(RATECTRL_DIR)alg_ags.o \
                 $(RATECTRL_DIR)alg_grp.o

STA_OBJS := $(STA_DIR)assoc.o \
            $(STA_DIR)auth.o \
            $(STA_DIR)auth_rsp.o \
            $(STA_DIR)sync.o \
            $(STA_DIR)sanity.o \
            $(STA_DIR)rtmp_data.o \
            $(STA_DIR)connect.o \
            $(STA_DIR)wpa.o \
            $(STA_DIR)sta_cfg.o \
            $(STA_DIR)sta.o

TXRX_OBJS := $(TXRX_DIR)wdev_tx.o

ATE_OBJS := $(ATE_DIR)/chips/mt76x2_ate.o \
            $(ATE_DIR)/common/ate_usb.o \
			$(ATE_DIR)/common/rt_ate.o \
			$(ATE_DIR)/common/rt_qa.o \
			$(ATE_DIR)/common/meta_agent.o

$(MODULE_NAME)-objs  += $(AP_OBJS)
$(MODULE_NAME)-objs  += $(CHIP_OBJS)
$(MODULE_NAME)-objs  += $(COMMON_OBJS)
$(MODULE_NAME)-objs  += $(MAC_OBJS)
$(MODULE_NAME)-objs  += $(MCU_OBJS)
$(MODULE_NAME)-objs  += $(MGMT_OBJS)
$(MODULE_NAME)-objs  += $(CFG80211_OBJS)
$(MODULE_NAME)-objs  += $(OS_OBJS)
$(MODULE_NAME)-objs  += $(PHY_OBJS)
$(MODULE_NAME)-objs  += $(RATECTTL_OBJS)
$(MODULE_NAME)-objs  += $(STA_OBJS)
$(MODULE_NAME)-objs  += $(TXRX_OBJS)
$(MODULE_NAME)-objs  += $(ATE_OBJS)
