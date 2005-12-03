######################### -*- Mode: Makefile-Gmake -*- ########################
## ppc64.mk --- 
## Author           : Manoj Srivastava ( srivasta@glaurung.internal.golden-gryphon.com ) 
## Created On       : Mon Oct 31 18:31:05 2005
## Created On Node  : glaurung.internal.golden-gryphon.com
## Last Modified By : Manoj Srivastava
## Last Modified On : Mon Oct 31 18:31:05 2005
## Last Machine Used: glaurung.internal.golden-gryphon.com
## Update Count     : 0
## Status           : Unknown, Use with caution!
## HISTORY          : 
## Description      : handle the architecture specific variables.
## 
## arch-tag: 559327f7-3df3-4855-8b90-a9447063bae0
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

kimage := vmlinux
kimagesrc = vmlinux
kimagedest = $(INT_IMAGE_DESTDIR)/vmlinux-$(version)
DEBCONFIG= $(CONFDIR)/config.$(KPKG_SUBARCH)
loader=NoLoader
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
else
  KERNEL_ARCH=ppc64
  target = $(kimage)
  kelfimagesrc  = vmlinux
  kelfimagedest = $(INT_IMAGE_DESTDIR)/vmlinux-$(version)
endif

#Local variables:
#mode: makefile
#End:
