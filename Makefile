obj-m := legion-go-wmi.o

KVER := $(shell uname -r)
KDIR := /lib/modules/$(KVER)/build
PWD := $(shell pwd)
INSTALL	:= install

SYMBOL_FILE := Module.symvers

EXTRA_CFLAGS += -O2

ccflags-y += -D__CHECK_ENDIAN__

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f /lib/modules/$(KVER)/drivers/platform/x86/legion-go-wmi.ko
	modprobe -r legion-go-wmi

modules_install:
	$(INSTALL) -d /lib/modules/$(KVER)/drivers/platform/x86/
	$(INSTALL) -m 644 legion-go-wmi.ko /lib/modules/$(KVER)/drivers/platform/x86/

install: modules_install
	depmod -a $(KVER)
	modprobe legion-go-wmi
