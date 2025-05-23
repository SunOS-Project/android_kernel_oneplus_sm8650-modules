# Android makefile for BT kernel modules

LOCAL_PATH := $(call my-dir)

# Build/Package only in case of supported target
ifeq ($(call is-board-platform-in-list,taro kalama pineapple blair pitti volcano niobe anorak61 neo61 seraph), true)

BT_SELECT := CONFIG_MSM_BT_POWER=m
ifneq ($(call is-board-platform-in-list, niobe anorak61 neo61 pitti taro blair kalama), true)
BT_SELECT += CONFIG_BTFM_CODEC=m
BT_SELECT += CONFIG_BTFM_SWR=m
endif

ifneq ($(call is-board-platform-in-list, niobe anorak61 neo61 pitti seraph blair), true)
BT_SELECT += CONFIG_I2C_RTC6226_QCA=m
endif

BT_SELECT += CONFIG_BTFM_SLIM=m

ifeq ($(TARGET_KERNEL_DLKM_SECURE_MSM_OVERRIDE), true)
ifeq ($(ENABLE_PERIPHERAL_STATE_UTILS), true)
BT_SELECT += CONFIG_BT_HW_SECURE_DISABLE=y
endif
endif

LOCAL_PATH := $(call my-dir)
LOCAL_MODULE_DDK_BUILD := true
LOCAL_MODULE_KO_DIRS := pwr/btpower.ko
ifneq ($(call is-board-platform-in-list, niobe anorak61 neo61 pitti taro blair kalama), true)
LOCAL_MODULE_KO_DIRS += btfmcodec/btfmcodec.ko
LOCAL_MODULE_KO_DIRS += soundwire/bt_fm_swr.ko
endif
LOCAL_MODULE_KO_DIRS += slimbus/bt_fm_slim.ko
ifneq ($(call is-board-platform-in-list, niobe anorak61 neo61 pitti seraph tlair), true)
LOCAL_MODULE_KO_DIRS += rtc6226/radio-i2c-rtc6226-qca.ko
endif


# This makefile is only for DLKM
ifneq ($(findstring vendor,$(LOCAL_PATH)),)

ifneq ($(findstring opensource,$(LOCAL_PATH)),)
	BT_BLD_DIR := $(abspath .)/vendor/qcom/opensource/bt-kernel
endif # opensource

DLKM_DIR := $(TOP)/device/qcom/common/dlkm


###########################################################
# This is set once per LOCAL_PATH, not per (kernel) module
KBUILD_OPTIONS := BT_KERNEL_ROOT=$(BT_BLD_DIR)
KBUILD_OPTIONS += $(foreach bt_select, \
       $(BT_SELECT), \
       $(bt_select))
BT_SRC_FILES := \
	$(wildcard $(LOCAL_PATH)/*) \
	$(wildcard $(LOCAL_PATH)/*/*) \

ifeq ($(TARGET_KERNEL_DLKM_SECURE_MSM_OVERRIDE), true)
ifeq ($(ENABLE_PERIPHERAL_STATE_UTILS), true)
KBUILD_REQUIRED_KOS := smcinvoke_dlkm.ko
endif
endif

KBUILD_REQUIRED_KOS += swr_dlkm.ko

# Module.symvers needs to be generated as a intermediate module so that
# other modules which depend on BT platform modules can set local
# dependencies to it.

########################### Module.symvers ############################
include $(CLEAR_VARS)
LOCAL_SRC_FILES           := $(BT_SRC_FILES)
LOCAL_MODULE              := bt-kernel-module-symvers
LOCAL_MODULE_STEM         := Module.symvers
LOCAL_MODULE_KBUILD_NAME  := Module.symvers
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk

# Below are for Android build system to recognize each module name, so
# they can be installed properly. Since Kbuild is used to compile these
# modules, invoking any of them will cause other modules to be compiled
# as well if corresponding flags are added in KBUILD_OPTIONS from upper
# level Makefiles.

################################ pwr ################################
include $(CLEAR_VARS)
LOCAL_SRC_FILES           := $(BT_SRC_FILES)
LOCAL_MODULE              := btpower.ko
LOCAL_MODULE_KBUILD_NAME  := pwr/btpower.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk

ifneq ($(call is-board-platform-in-list, niobe anorak61 neo61 pitti), true)
################################ BTFM CODEC Driver #########################
include $(CLEAR_VARS)
LOCAL_SRC_FILES           := $(BT_SRC_FILES)
LOCAL_MODULE              := btfmcodec.ko
LOCAL_MODULE_KBUILD_NAME  := btfmcodec/btfmcodec.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk
################################ soundwire ################################
include $(CLEAR_VARS)
LOCAL_SRC_FILES           := $(BT_SRC_FILES)
LOCAL_MODULE              := bt_fm_swr.ko
LOCAL_MODULE_KBUILD_NAME  := soundwire/bt_fm_swr.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk
endif
################################ slimbus ################################
include $(CLEAR_VARS)
LOCAL_SRC_FILES           := $(BT_SRC_FILES)
LOCAL_MODULE              := bt_fm_slim.ko
LOCAL_MODULE_KBUILD_NAME  := slimbus/bt_fm_slim.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk
################################ rtc6226 ################################
include $(CLEAR_VARS)
LOCAL_SRC_FILES           := $(BT_SRC_FILES)
LOCAL_MODULE              := radio-i2c-rtc6226-qca.ko
LOCAL_MODULE_KBUILD_NAME  := rtc6226/radio-i2c-rtc6226-qca.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk
###########################################################

endif # DLKM check
endif # supported target check
