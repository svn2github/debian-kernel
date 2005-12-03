############################ -*- Mode: Makefile -*- ###########################
## archvars.mk --- 
## Author           : Manoj Srivastava ( srivasta@glaurung.green-gryphon.com ) 
## Created On       : Sat Nov 15 02:40:56 2003
## Created On Node  : glaurung.green-gryphon.com
## Last Modified By : Manoj Srivastava
## Last Modified On : Tue Nov 16 23:36:15 2004
## Last Machine Used: glaurung.internal.golden-gryphon.com
## Update Count     : 5
## Status           : Unknown, Use with caution!
## HISTORY          : 
## Description      : 
## 
## arch-tag: e16dd848-0fd6-4c0e-ae66-bef20d1f7c63
## 
###############################################################################

DPKG_ARCH := dpkg-architecture
ifdef ARCH
  ifeq ($(strip $(MAKING_VIRTUAL_IMAGE)),)
    ha:=-a$(ARCH)
  endif
endif

# set the dpkg-architecture vars

# set the dpkg-architecture vars
export DEB_BUILD_ARCH      := $(shell $(DPKG_ARCH)       -qDEB_BUILD_ARCH)
export DEB_BUILD_GNU_CPU   := $(shell $(DPKG_ARCH)       -qDEB_BUILD_GNU_CPU)
export DEB_BUILD_GNU_TYPE  := $(shell $(DPKG_ARCH)       -qDEB_BUILD_GNU_TYPE)
export DEB_HOST_ARCH       := $(shell $(DPKG_ARCH) $(ha) -qDEB_HOST_ARCH)
export DEB_HOST_GNU_CPU    := $(shell $(DPKG_ARCH) $(ha) -qDEB_HOST_GNU_CPU)
export DEB_HOST_GNU_SYSTEM := $(shell $(DPKG_ARCH) $(ha) -qDEB_HOST_GNU_SYSTEM)
export DEB_HOST_GNU_TYPE   := $(shell $(DPKG_ARCH) $(ha) -qDEB_HOST_GNU_TYPE)
export DEB_BUILD_GNU_SYSTEM:= $(shell $(DPKG_ARCH)       -qDEB_BUILD_GNU_SYSTEM)


REASON = @if [ -f $@ ]; then \
 echo "====== making $(notdir $@) because of $(notdir $?) ======";\
 else \
   echo "====== making (creating) $@ ======"; \
 fi

OLDREASON = @if [ -f $@ ]; then \
 echo "====== making $(notdir $@) because of $(notdir $?) ======";\
 else \
   echo "====== making (creating) $(notdir $@) ======"; \
 fi

LIBREASON = @echo "====== making $(notdir $@)($(notdir $%))because of $(notdir $?)======"
