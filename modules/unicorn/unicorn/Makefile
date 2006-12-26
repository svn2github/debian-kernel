#
## Makefile for UNICORN ADSL Modem
#

# Version
VERS=0x093

# path to kernel sources and headers (must correspond to running kernel)
KERNEL_SOURCES=/usr/src/linux
HPATH=$(KERNEL_SOURCES)/include
MAKE=make  

.EXPORT_ALL_VARIABLES:

DIRS = adsl_status unicorntest tools
DRVDIRS = libm unicorn_pci unicorn_usb

all: applis modules
 
applis: 
	for i in $(DIRS) ; do make -C $$i all ; done

modules: 
	for i in $(DRVDIRS) ; do make -C $$i modules MODDIR=$(PWD)/$$i ; done

clean: 
	for i in $(DIRS) $(DRVDIRS); do (if [ -d $$i ]; then make -C $$i clean; fi); done

install: applis_install modules_install

applis_install: 
	for i in $(DIRS) ; do make -C $$i install prefix=$(prefix); done

modules_install: 
	for i in $(DRVDIRS) ; do  make -C $$i modules_install MODDIR=$(PWD)/$$i ; done
	/sbin/depmod -ae

only_modules_install: 
	for i in $(DRVDIRS) ; do make -C $$i modules_install ; done
