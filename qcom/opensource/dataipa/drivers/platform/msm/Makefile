ifeq ($(KP_MODULE_ROOT),)
KP_MODULE_ROOT=$(KERNEL_SRC)/$(M)
endif

KBUILD_OPTIONS+=KBUILD_DTC_INCLUDE=$(KP_MODULE_ROOT)
KBUILD_OPTIONS += KBUILD_EXTRA_SYMBOLS=$(OUT_DIR)/../sm8650-modules/qcom/opensource/datarmnet-ext/mem/Module.symvers

all: modules # dtbs

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean

%:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $@ $(KBUILD_OPTIONS)
