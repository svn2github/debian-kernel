#
## Makefile for UNICORN PCI ADSL Modem
#

src ?= .
obj ?= .

ifndef KERNEL_SOURCES 
KERNEL_SOURCES=/usr/src/linux
endif
ifndef HPATH
HPATH=$(KERNEL_SOURCES)/include
endif

ifndef VERS 
VERS=0
endif

ifndef KVERS
KVERS = `uname -r`
endif

ifndef PATCHLEVEL
PATCHLEVEL=$(shell grep -s PATCHLEVEL ${KERNEL_SOURCES}/Makefile |  head -n 1 | sed s/'PATCHLEVEL = '//)
endif

ifndef MODDIR
MODDIR=$(PWD)
endif

include $(KERNEL_SOURCES)/.config

#
# -DUSE_HW_TIMER - set this to use 2ms PCI hardware timer
# -DPCI_BRIDGE_WORKAROUND - set this to work with low performance PCI bridges
# -DKT400 - set this if you have a MSI KT4 motherboard or similar
COPTIONS =

# make parameters
CC=gcc
LD=ld

INCLUDES :=  -I$(src)/../include -I$(src)/../
ifeq ($(PATCHLEVEL),4)
# kernel 2.4, add path to kernel includes
INCLUDES += -I$(HPATH)
endif
EXTRA_CFLAGS := -DVERS=$(VERS) -D_PCI_DRIVER -DDEBUG=1 $(INCLUDES) $(COPTIONS)
ifeq ($(PATCHLEVEL),4)
# kernel 2.4, need to specify CFLAGS
CFLAGS :=  -O2 -fomit-frame-pointer -Wall -Wno-trigraphs -fno-strict-aliasing -fno-common
EXTRA_CFLAGS += -D__KERNEL__ -DMODULE
CFLAGS += $(EXTRA_CFLAGS)
endif
CXXFLAGS = -fno-rtti -fno-exceptions $(CFLAGS) $(EXTRA_CFLAGS)

OBJS = unicorn_pcidrv.o ../msw/linrapi.o ../msw/msw.o ../msw/crc.o ../amu/amas.o ../amu/amu.o ../amu/bsp_pci.o
OBJS_ATM = ../unicorn_atm/unicorn_atmdrv.o
OBJS_ETH = ../unicorn_eth/unicorn_ethdrv.o
ifdef CONFIG_REGPARM
MODEM_LIB = $(src)/../arch/i386/modem_ant_PCI_LINUX.o.regparm3
else
MODEM_LIB = $(src)/../arch/i386/modem_ant_PCI_LINUX.o
endif
LIBM = $(src)/../libm/libm.a

ifeq ($(PATCHLEVEL),4)
# kernel 2.4, use old build process
modules: unicorn_pci_atm.o unicorn_pci_eth.o
modules_install: install_atm install_eth
else
# kernel 2.6, use kernel build process
modules:
	make CC=$(CC) -C $(KERNEL_SOURCES) SUBDIRS=$(MODDIR) modules
modules_install: 
	make CC=$(CC) -C $(KERNEL_SOURCES) SUBDIRS=$(MODDIR) modules_install
endif

obj-m := unicorn_pci_atm.o unicorn_pci_eth.o

unicorn_pci_atm-objs := $(OBJS) $(OBJS_ATM)

unicorn_pci_eth-objs := $(OBJS) $(OBJS_ETH)

EXTRA_LDFLAGS := $(MODEM_LIB) $(LIBM)

unicorn_pci_atm.o: $(unicorn_pci_atm-objs)
	$(LD) -r $(EXTRA_LDFLAGS) -o $@ $(unicorn_pci_atm-objs)

unicorn_pci_eth.o: $(unicorn_pci_eth-objs)
	$(LD) -r $(EXTRA_LDFLAGS) -o $@  $(unicorn_pci_eth-objs)

install_atm: unicorn_pci_atm.o
# remove old versions
	/bin/rm -f $(DESTDIR)/lib/modules/$(KVERS)/kernel/drivers/atm/unicorn_*.o

	mkdir -p $(DESTDIR)/lib/modules/$(KVERS)/extra
	cp $^ $(DESTDIR)/lib/modules/$(KVERS)/extra/

install_eth: unicorn_pci_eth.o
# remove old versions
	/bin/rm -f $(DESTDIR)/lib/modules/$(KVERS)/kernel/drivers/atm/unicorn_*.o

	mkdir -p $(DESTDIR)/lib/modules/$(KVERS)/extra
	cp $^ $(DESTDIR)/lib/modules/$(KVERS)/extra/

clean:
	/bin/rm -f $(unicorn_pci_atm-objs)
	/bin/rm -f $(unicorn_pci_eth-objs)
	/bin/rm -f unicorn_pci_atm.* unicorn_pci_eth.*
	/bin/rm -f .*.cmd
	/bin/rm -rf .tmp_versions