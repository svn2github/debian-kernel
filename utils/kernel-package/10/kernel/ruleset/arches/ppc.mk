######################### -*- Mode: Makefile-Gmake -*- ########################
## ppc.mk --- 
## Author           : Manoj Srivastava ( srivasta@glaurung.internal.golden-gryphon.com ) 
## Created On       : Mon Oct 31 18:31:06 2005
## Created On Node  : glaurung.internal.golden-gryphon.com
## Last Modified By : Manoj Srivastava
## Last Modified On : Mon Oct 31 18:31:06 2005
## Last Machine Used: glaurung.internal.golden-gryphon.com
## Update Count     : 0
## Status           : Unknown, Use with caution!
## HISTORY          : 
## Description      : handle the architecture specific variables.
## 
## arch-tag: 5f56e1be-14d8-4843-bf39-423460c2ab1a
## 
## 
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
##
###############################################################################

ifeq ($(DEB_BUILD_ARCH),powerpc)
# This is only meaningful when building on a PowerPC
  ifeq ($(GUESS_SUBARCH),)
    GUESS_SUBARCH:=$(shell awk '/machine/ { print $$3}' /proc/cpuinfo)
    ifneq (,$(findstring Power,$(GUESS_SUBARCH)))
      GUESS_SUBARCH:=pmac
    else
      # At the request of Colin Watson, changed from find string iMac.
      # Any powerpc system that would contain  Mac in /proc/cpuinfo is a
      # PowerMac system, according to arch/ppc/platforms/* in the kernel source
      ifneq (,$(findstring Mac,$(GUESS_SUBARCH)))
        GUESS_SUBARCH:=pmac
      endif
    endif
  else
    GUESS_SUBARCH:=pmac
  endif
  # Well NuBus powermacs are not pmac subarchs, but nubus ones.
  #ifeq (,$(shell grep NuBus /proc/cpuinfo))
  #  GUESS_SUBARCH:=nubus
  #endif
endif

ifeq (,$(findstring $(KPKG_SUBARCH),apus prpmc chrp mbx pmac prep Amiga APUs CHRP MBX PReP chrp-rs6k nubus powerpc powerpc64 ))
  KPKG_SUBARCH:=$(GUESS_SUBARCH)
endif

KERNEL_ARCH:=ppc

ifneq (,$(findstring $(KPKG_SUBARCH), powerpc powerpc64))
  ifneq (,$(findstring $(KPKG_SUBARCH), powerpc64))
    KERNEL_ARCH:=ppc64
    target := vmlinux
  endif
  ifneq (,$(findstring $(KPKG_SUBARCH), powerpc))
    KERNEL_ARCH:=ppc
    NEED_IMAGE_POST_PROCESSING = YES
    IMAGE_POST_PROCESS_TARGET := mkvmlinuz_support_install
    IMAGE_POST_PROCESS_DIR    := arch/ppc/boot
    INSTALL_MKVMLINUZ_PATH = $(SRCTOP)/$(IMAGE_TOP)/usr/lib/kernel-image-${version}
    target := zImage
    loaderdep=mkvmlinuz
  endif
  kimagesrc = vmlinux
  kimage := vmlinux
  kimagedest = $(INT_IMAGE_DESTDIR)/vmlinux-$(version)
  DEBCONFIG= $(CONFDIR)/config.$(KPKG_SUBARCH)
endif

ifneq (,$(findstring $(KPKG_SUBARCH),APUs apus Amiga))
  KPKG_SUBARCH:=apus
  loader := NoLoader
  kimage := vmapus.gz
  target = zImage
  kimagesrc = $(shell if [ -d arch/$(KERNEL_ARCH)/boot/images ]; then \
	echo arch/$(KERNEL_ARCH)/boot/images/vmapus.gz ; else \
	echo arch/$(KERNEL_ARCH)/boot/$(kimage) ; fi)
  kimagedest = $(INT_IMAGE_DESTDIR)/vmlinuz-$(version)
  kelfimagesrc = vmlinux
  kelfimagedest = $(INT_IMAGE_DESTDIR)/vmlinux-$(version)
  DEBCONFIG = $(CONFDIR)/config.apus
endif

ifneq (,$(findstring $(KPKG_SUBARCH),chrp-rs6k))
  KPKG_SUBARCH:=chrp-rs6k
  loaderdep=quik
  loader=quik
  loaderdoc=QuikDefault
  kimage := zImage
  target = $(kimage)
  kimagesrc = $(shell if [ -d arch/$(KERNEL_ARCH)/chrpboot ]; then \
	echo arch/$(KERNEL_ARCH)/chrpboot/$(kimage) ; else \
	echo arch/$(KERNEL_ARCH)/boot/images/$(kimage).chrp-rs6k ; fi)
  kimagedest = $(INT_IMAGE_DESTDIR)/vmlinuz-$(version)
  kelfimagesrc = vmlinux
  kelfimagedest = $(INT_IMAGE_DESTDIR)/vmlinux-$(version)
  DEBCONFIG = $(CONFDIR)/config.chrp
endif

ifneq (,$(findstring $(KPKG_SUBARCH),CHRP chrp))
  KPKG_SUBARCH:=chrp
  loaderdep=quik
  loader=quik
  loaderdoc=QuikDefault
  kimage := zImage
  target = $(kimage)
  kimagesrc = $(shell if [ -d arch/$(KERNEL_ARCH)/chrpboot ]; then \
       echo arch/$(KERNEL_ARCH)/chrpboot/$(kimage) ; else \
       echo arch/$(KERNEL_ARCH)/boot/images/$(kimage).chrp ; fi)
  kimagedest = $(INT_IMAGE_DESTDIR)/vmlinuz-$(version)
  kelfimagesrc = vmlinux
  kelfimagedest = $(INT_IMAGE_DESTDIR)/vmlinux-$(version)
  DEBCONFIG = $(CONFDIR)/config.chrp
endif

ifneq (,$(findstring $(KPKG_SUBARCH),PRPMC prpmc))
  KPKG_SUBARCH:=prpmc
  loader := NoLoader
  kimage := zImage
  target = $(kimage)
  kimagesrc = arch/$(KERNEL_ARCH)/boot/images/zImage.pplus
  kimagedest = $(INT_IMAGE_DESTDIR)/vmlinuz-$(version)
  kelfimagesrc = vmlinux
  kelfimagedest = $(INT_IMAGE_DESTDIR)/vmlinux-$(version)
endif

ifneq (,$(findstring $(KPKG_SUBARCH),MBX mbx))
  KPKG_SUBARCH:=mbx
  loader := NoLoader
  kimage := zImage
  target = $(kimage)
  kimagesrc = $(shell if [ -d arch/$(KERNEL_ARCH)/mbxboot ]; then \
	echo arch/$(KERNEL_ARCH)/mbxboot/$(kimage) ; else \
	echo arch/$(KERNEL_ARCH)/boot/images/zvmlinux.embedded; fi)
  kimagedest = $(INT_IMAGE_DESTDIR)/vmlinuz-$(version)
  kelfimagesrc = vmlinux
  kelfimagedest = $(INT_IMAGE_DESTDIR)/vmlinux-$(version)
  DEBCONFIG = $(CONFDIR)/config.mbx
endif

ifneq (,$(findstring $(KPKG_SUBARCH),pmac))
  KPKG_SUBARCH:=pmac
  target := zImage
  ifeq ($(DEB_BUILD_ARCH),powerpc)
    # This is only meaningful when building on a PowerPC
    ifneq (,$(shell grep NewWorld /proc/cpuinfo))
      loaderdep=yaboot
      loader=yaboot
      #loaderdoc=
    else
      loaderdep=quik
      loader=quik
      loaderdoc=QuikDefault
    endif
  else
    loaderdep=yaboot
    loader=yaboot
  endif
  kimagesrc = vmlinux
  kimage := vmlinux
  kimagedest = $(INT_IMAGE_DESTDIR)/vmlinux-$(version)
  HAVE_COFF_IMAGE = YES
  coffsrc = $(shell if [ -d arch/$(KERNEL_ARCH)/coffboot ]; then \
       echo arch/$(KERNEL_ARCH)/coffboot/$(kimage).coff ; else \
       echo arch/$(KERNEL_ARCH)/boot/images/$(kimage).coff ; fi)
  coffdest=$(INT_IMAGE_DESTDIR)/vmlinux.coff-$(version)
  DEBCONFIG = $(CONFDIR)/config.pmac
endif

ifneq (,$(findstring $(KPKG_SUBARCH),PReP prep))
  KPKG_SUBARCH:=prep
  loader := NoLoader
  kimage := zImage
  target = $(kimage)
  kimagesrc = $(shell if [ -d arch/$(KERNEL_ARCH)/boot/images ]; then \
       echo arch/$(KERNEL_ARCH)/boot/images/$(kimage).prep ; else \
       echo arch/$(KERNEL_ARCH)/boot/$(kimage) ; fi)
  kimagedest = $(INT_IMAGE_DESTDIR)/vmlinuz-$(version)
  kelfimagesrc = vmlinux
  kelfimagedest = $(INT_IMAGE_DESTDIR)/vmlinux-$(version)
  DEBCONFIG = $(CONFDIR)/config.prep
endif

ifneq (,$(findstring $(KPKG_SUBARCH), NuBuS nubus))
  KPKG_SUBARCH := nubus
  target := zImage
  loader= NoLoader
  kimagesrc = arch/$(KERNEL_ARCH)/appleboot/Mach\ Kernel
  kimage := vmlinux
  kimagedest = $(INT_IMAGE_DESTDIR)/vmlinuz-$(version)
endif

#Local variables:
#mode: makefile
#End:
