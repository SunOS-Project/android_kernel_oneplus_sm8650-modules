#Build ipa
TARGET_DATAIPA_DLKM_ENABLE := false
ifeq ($(TARGET_KERNEL_DLKM_DISABLE), true)
	ifeq ($(TARGET_KERNEL_DLKM_DATAIPA_OVERRIDE), true)
		TARGET_DATAIPA_DLKM_ENABLE := true
	endif
else
	TARGET_DATAIPA_DLKM_ENABLE := true
endif

ifeq ($(TARGET_DATAIPA_DLKM_ENABLE), true)
DATA_DLKM_BOARD_PLATFORMS_LIST := taro kalama bengal monaco pineapple blair holi cliffs pitti volcano niobe seraph
ifneq ($(TARGET_BOARD_AUTO),true)
ifeq ($(call is-board-platform-in-list,$(DATA_DLKM_BOARD_PLATFORMS_LIST)),true)
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/gsim.ko
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/ipam.ko
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/ipanetm.ko
ifeq ($(CONFIG_LOCALVERSION), "-gki-consolidate")
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/ipatestm.ko
endif
endif
endif
endif
