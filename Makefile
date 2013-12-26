ifeq ($(KERNELRELEASE),)
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build

default: module app

module:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) C=1 modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) C=1 clean
	rm -f mioc-app

CC = cgcc
CFLAGS = -O2 -s -Wall -Werror -Wextra -pedantic -ansi

app:
	$(CC) $(CFLAGS) mioc-app.c -o mioc-app

.PHONY: module clean app

else
	obj-m := mioc.o
endif
