# Copyright (C) 2017 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.

LOCAL_PATH := $(call my-dir)
KERNEL_ROOT_DIR := $(PWD)

ROOTDIR := $(abspath $(TOP))

KERNEL_TARGET_ARCH ?= arm64
KERNEL_TARGET_TOOLCHAIN_ROOT ?= $(KERNEL_ROOT_DIR)/prebuilts/gcc/$(HOST_PREBUILT_TAG)/aarch64/aarch64-linux-gnu-6.3.1
KERNEL_CROSS_COMPILE ?= $(KERNEL_TARGET_TOOLCHAIN_ROOT)/bin/aarch64-linux-gnu-

define touch-kernel-image-timestamp
if [ -e $(1) ] && [ -e $(2) ] && cmp -s $(1) $(2); then \
 echo $(2) has no change;\
 mv -f $(1) $(2);\
else \
 rm -f $(1);\
fi
endef

define move-kernel-module-files
v=`cat $(2)/include/config/kernel.release`;\
for i in `grep -h '\.ko' /dev/null $(2)/.tmp_versions/*.mod`; do \
 o=`basename $$i`;\
 if [ -e $(1)/$$o ] && cmp -s $(1)/lib/modules/$$v/kernel/$$i $(1)/$$o; then \
  echo $(1)/$$o has no change;\
 else \
  echo Update $(1)/$$o;\
  mv -f $(1)/lib/modules/$$v/kernel/$$i $(1)/$$o;\
 fi;\
done
endef

define clean-kernel-module-dirs
rm -rf $(1)/lib/modules/$(if $(2),`cat $(2)/include/config/kernel.release`,*/)
endef

# '\\' in command is wrongly replaced to '\\\\' in kernel/out/arch/arm/boot/compressed/.piggy.xzkern.cmd
define fixup-kernel-cmd-file
if [ -e $(1) ]; then cp $(1) $(1).bak; sed -e 's/\\\\\\\\/\\\\/g' < $(1).bak > $(1); rm -f $(1).bak; fi
endef

ifneq ($(strip $(BOARD_AMAZON_SYSTEM_VERITY_PUBK)),)
ifeq ($(wildcard $(BOARD_AMAZON_SYSTEM_VERITY_PUBK)),)
$(error defined BOARD_AMAZON_SYSTEM_VERITY_PUBK: $(BOARD_AMAZON_SYSTEM_VERITY_PUBK), but file not exist)
endif
endif

ifneq ($(strip $(TARGET_NO_KERNEL)),true)
    KERNEL_DIR := $(LOCAL_PATH)
    BUILT_SYSTEMIMAGE := $(call intermediates-dir-for,PACKAGING,systemimage)/system.img

  ifeq ($(KERNEL_CROSS_COMPILE),)
  ifeq ($(KERNEL_TARGET_ARCH), arm64)
    KERNEL_CROSS_COMPILE := $(KERNEL_ROOT_DIR)/$(KERNEL_TARGET_TOOLS_PREFIX)
  else
    KERNEL_CROSS_COMPILE := $(KERNEL_ROOT_DIR)/prebuilts/gcc/$(HOST_PREBUILT_TAG)/arm/arm-eabi-$(TARGET_GCC_VERSION)/bin/arm-eabi-
  endif
  endif
  ifeq ($(wildcard $(TARGET_PREBUILT_KERNEL)),)
 #   KERNEL_OUT ?= $(if $(filter /% ~%,$(TARGET_OUT_INTERMEDIATES)),,$(KERNEL_ROOT_DIR)/)$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
    KERNEL_OUT = $(ROOTDIR)/$(PRODUCT_OUT)/obj/KERNEL_OBJ
    ifeq ($(KERNEL_TARGET_ARCH), arm64)
      ifeq ($(MTK_APPENDED_DTB_SUPPORT), yes)
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/Image.gz-dtb
      else
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/Image.gz
      endif
    else
      ifeq ($(MTK_APPENDED_DTB_SUPPORT), yes)
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/zImage-dtb
      else
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/zImage
      endif
    endif
    BUILT_KERNEL_TARGET := $(KERNEL_ZIMAGE_OUT).bin
    INSTALLED_KERNEL_TARGET := $(PRODUCT_OUT)/kernel
    TARGET_KERNEL_CONFIG := $(KERNEL_OUT)/.config
    KERNEL_HEADERS_INSTALL := $(PRODUCT_OUT)/obj/KERNEL_OBJ/usr
    KERNEL_CONFIG_FILE := $(KERNEL_DIR)/arch/$(KERNEL_TARGET_ARCH)/configs/$(KERNEL_DEFCONFIG)
    KERNEL_CONFIG_MODULES := $(shell grep ^CONFIG_MODULES=y $(KERNEL_CONFIG_FILE))
    KERNEL_MODULES_OUT := $(if $(filter /% ~%,$(TARGET_OUT)),,$(KERNEL_ROOT_DIR)/)$(TARGET_OUT)/lib/modules
    KERNEL_MODULES_DEPS := $(if $(wildcard $(KERNEL_MODULES_OUT)/*.ko),$(wildcard $(KERNEL_MODULES_OUT)/*.ko),$(KERNEL_MODULES_OUT))
    KERNEL_MODULES_SYMBOLS_OUT := $(if $(filter /% ~%,$(TARGET_OUT_UNSTRIPPED)),,$(KERNEL_ROOT_DIR)/)$(TARGET_OUT_UNSTRIPPED)/system/lib/modules
    KERNEL_MAKE_OPTION := O=$(KERNEL_OUT) ARCH=$(KERNEL_TARGET_ARCH) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) ROOTDIR=$(KERNEL_ROOT_DIR) $(if $(strip $(SHOW_COMMANDS)),V=1)

# .config cannot be PHONY due to config_data.gz
ifneq ($(wildcard $(BOARD_AMAZON_SYSTEM_VERITY_PUBK)),)
$(TARGET_KERNEL_CONFIG): $(BOARD_AMAZON_SYSTEM_VERITY_PUBK)
endif
$(TARGET_KERNEL_CONFIG): $(KERNEL_CONFIG_FILE)
ifneq ($(wildcard $(TARGET_KERNEL_CONFIG)),)
$(TARGET_KERNEL_CONFIG): $(shell find $(KERNEL_DIR) -name "Kconfig*")
endif
	$(hide) mkdir -p $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) $(KERNEL_DEFCONFIG)
ifneq ($(wildcard $(BOARD_AMAZON_SYSTEM_VERITY_PUBK)),)
	$(hide) cp $(BOARD_AMAZON_SYSTEM_VERITY_PUBK) $(KERNEL_DIR)/certs/amazon_verity.x509.pem
endif

$(KERNEL_MODULES_DEPS): $(KERNEL_ZIMAGE_OUT) ;

.PHONY: $(KERNEL_HEADERS_INSTALL)

KERNEL_HEADERS_INSTALL: $(TARGET_KERNEL_CONFIG)
	$(MAKE) $(KERNEL_MAKE_OPTION)) headers_install

$(KERNEL_ZIMAGE_OUT): $(TARGET_KERNEL_CONFIG) FORCE
	$(hide) mkdir -p $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION)
	$(hide) $(call fixup-kernel-cmd-file,$(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/compressed/.piggy.xzkern.cmd)
ifneq ($(KERNEL_CONFIG_MODULES),)
	$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) modules
	$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) INSTALL_MOD_PATH=$(KERNEL_MODULES_SYMBOLS_OUT) modules_install
	$(hide) $(call move-kernel-module-files,$(KERNEL_MODULES_SYMBOLS_OUT),$(KERNEL_OUT))
	$(hide) $(call clean-kernel-module-dirs,$(KERNEL_MODULES_SYMBOLS_OUT),$(KERNEL_OUT))
	$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) INSTALL_MOD_PATH=$(KERNEL_MODULES_OUT) modules_install
	$(hide) $(call move-kernel-module-files,$(KERNEL_MODULES_OUT),$(KERNEL_OUT))
	$(hide) $(call clean-kernel-module-dirs,$(KERNEL_MODULES_OUT),$(KERNEL_OUT))
endif

ifeq ($(strip $(MTK_HEADER_SUPPORT)), yes)
$(BUILT_KERNEL_TARGET): $(KERNEL_ZIMAGE_OUT) $(TARGET_KERNEL_CONFIG) | $(HOST_OUT_EXECUTABLES)/mkimage$(HOST_EXECUTABLE_SUFFIX)
	$(hide) $(HOST_OUT_EXECUTABLES)/mkimage$(HOST_EXECUTABLE_SUFFIX) $< KERNEL 0xffffffff > $@

else
$(BUILT_KERNEL_TARGET): $(KERNEL_ZIMAGE_OUT) $(TARGET_KERNEL_CONFIG) | $(ACP)
	$(copy-file-to-target)

endif

$(TARGET_PREBUILT_KERNEL): $(BUILT_KERNEL_TARGET) | $(ACP)
	$(copy-file-to-new-target)

  else
    BUILT_KERNEL_TARGET := $(TARGET_PREBUILT_KERNEL)
  endif#TARGET_PREBUILT_KERNEL

$(INSTALLED_KERNEL_TARGET): $(BUILT_KERNEL_TARGET) | $(ACP)
	$(copy-file-to-target)

ifneq ($(KERNEL_CONFIG_MODULES),)
.PHONY: external-modules
external-modules: $(KERNEL_MODULES_DEPS)

$(BUILT_SYSTEMIMAGE): external-modules
endif

.PHONY: kernel save-kernel kernel-savedefconfig %config-kernel clean-kernel
kernel: $(INSTALLED_KERNEL_TARGET)
save-kernel: $(TARGET_PREBUILT_KERNEL)

kernel-savedefconfig: $(TARGET_KERNEL_CONFIG)
	cp $(TARGET_KERNEL_CONFIG) $(KERNEL_CONFIG_FILE)

kernel-menuconfig:
	$(hide) mkdir -p $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) menuconfig

%config-kernel:
	$(hide) mkdir -p $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) $(patsubst %config-kernel,%config,$@)

clean-kernel:
	$(hide) rm -rf $(KERNEL_OUT) $(KERNEL_MODULES_OUT) $(INSTALLED_KERNEL_TARGET)

#.PHONY: check-kernel-config check-kernel-dotconfig
#droid: check-kernel-config check-kernel-dotconfig
#check-mtk-config: check-kernel-config check-kernel-dotconfig
#check-kernel-config:
#	-python device/mediatek/build/build_mt8163/tools/check_kernel_config.py -c $(MTK_TARGET_PROJECT_FOLDER)/ProjectConfig.mk -k $(KERNEL_CONFIG_FILE) -p $(MTK_PROJECT_NAME)

#ifneq ($(filter check-kernel-dotconfig,$(MAKECMDGOALS)),)
#.PHONY: $(TARGET_KERNEL_CONFIG)
#endif
#check-kernel-dotconfig: $(TARGET_KERNEL_CONFIG)
#	-python device/mediatek/build/build_mt8163/tools/check_kernel_config.py -c $(MTK_TARGET_PROJECT_FOLDER)/ProjectConfig.mk -k $(TARGET_KERNEL_CONFIG) -p $(MTK_PROJECT_NAME)

endif#TARGET_NO_KERNEL
