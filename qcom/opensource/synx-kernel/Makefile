CUR_MKFILE=$(abspath $(lastword $(MAKEFILE_LIST)))
KBUILD_OPTIONS+= SYNX_ROOT=$(dir $(CUR_MKFILE))

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules $(KBUILD_OPTIONS)
modules_install:
	$(MAKE) M=$(M) -C $(KERNEL_SRC) modules_install
%:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $@ $(KBUILD_OPTIONS)
clean:
	rm -f *.o *.ko *.mod.c *.mod.o *~ .*.cmd Module.symvers
	rm -rf .tmp_versions
