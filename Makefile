ifeq ($(KERNELRELEASE),)
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build

module:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) C=1 modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) C=1 clean

.PHONY: module clean

else
	obj-m := mioc.o
endif
