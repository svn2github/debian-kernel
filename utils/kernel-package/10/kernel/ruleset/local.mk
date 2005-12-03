######################### -*- Mode: Makefile-Gmake -*- ########################
## local.mk<ruleset> --- 
## Author           : Manoj Srivastava ( srivasta@glaurung.internal.golden-gryphon.com ) 
## Created On       : Fri Oct 28 00:37:46 2005
## Created On Node  : glaurung.internal.golden-gryphon.com
## Last Modified By : Manoj Srivastava
## Last Modified On : Thu Dec  1 15:14:01 2005
## Last Machine Used: glaurung.internal.golden-gryphon.com
## Update Count     : 5
## Status           : Unknown, Use with caution!
## HISTORY          : 
## Description      : 
## 
## arch-tag: d047cfca-c918-4f47-b6e2-8c7df9778b26
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
## Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
##
###############################################################################
testdir:
	$(checkdir)

$(eval $(which_debdir))
include $(DEBDIR)/ruleset/targets/target.mk


CONFIG-common:: debian/stamp-conf
	$(REASON)
CONFIG-arch:: .config 
	$(REASON)
CONFIG-indep:: conf.vars debian/stamp-kernel-conf
	$(REASON)


BUILD-common:: sanity_check 
	$(REASON)
BUILD-arch:: debian/stamp-build-kernel
	$(REASON)

BIN/$(s_package):: binary/$(s_package)
	$(REASON)
BIN/$(i_package):: binary/$(i_package)
	$(REASON)
BIN/$(d_package):: binary/$(d_package)
	$(REASON)
BIN/$(m_package):: binary/$(m_package)
	$(REASON)
BIN/$(h_package):: binary/$(h_package)
	$(REASON)

INST/$(s_package):: install/$(s_package)
	$(REASON)
INST/$(i_package):: install/$(i_package)
	$(REASON)
INST/$(d_package):: install/$(d_package)
	$(REASON)
INST/$(m_package):: install/$(m_package)
	$(REASON)
INST/$(h_package):: install/$(h_package)
	$(REASON)

CLN-common::
	$(REASON)
	$(warn_root)
	$(eval $(deb_rule))
	$(root_run_command) real_stamp_clean

CLEAN/$(s_package)::
	-rm -rf $(TMPTOP)
CLEAN/$(i_package)::
	-rm -rf $(TMPTOP)
ifneq ($(strip $(KERNEL_ARCH)),um)
  ifneq ($(strip $(KERNEL_ARCH)),xen)
	-test -d ./debian &&                                         \
	sed -e 's/=V/$(version)/g'    -e 's/=B/$(link_in_boot)/g'    \
            -e 's/=ST/$(INT_STEM)/g'  -e 's/=R/$(reverse_symlink)/g' \
            -e 's/=K/$(kimage)/g'     -e 's/=L/$(loader)/g'          \
            -e 's@=MK@$(initrdcmd)@g' -e 's@=A@$(DEB_HOST_ARCH)@g'   \
            -e 's/=I/$(INITRD)/g'     -e 's,=D,$(IMAGEDIR),g'        \
            -e 's/=MD/$(initrddep)/g'                                \
            -e 's@=M@$(MKIMAGE)@g'    -e 's/=OF/$(AM_OFFICIAL)/g'    \
            -e 's/=S/$(no_symlink)/g' -e 's@=B@$(KERNEL_ARCH)@g'     \
          $(DEBDIR)/templates.in   > ./debian/templates.master
	-test -d ./debian &&          $(INSTALL_TEMPLATE)
  endif
endif

CLEAN/$(d_package)::
	-rm -rf $(TMPTOP)
CLEAN/$(m_package)::
	-rm -rf $(TMPTOP)
CLEAN/$(h_package)::
	-rm -rf $(TMPTOP)

buildpackage: clean CONFIG-common stamp-buildpackage
stamp-buildpackage: 
	$(REASON)
ifneq ($(strip $(HAVE_VERSION_MISMATCH)),)
	@echo "The changelog says we are creating $(saved_version)"
	@echo "However, I thought the version is $(version)"
	exit 1
endif
	echo 'Building Package' > stamp-building
	dpkg-buildpackage -nc $(strip $(int_root_cmd)) $(strip $(int_us))  \
               $(strip $(int_uc)) -m"$(maintainer) <$(email)>" -k"$(pgp)"
	rm -f stamp-building
	echo done >  $@
STAMPS_TO_CLEAN += stamp-buildpackage

# All of these are targets that insert themselves into the normal flow
# of policy specified targets, so they must hook themselves into the
# stream.            
debian:  stamp-indep-conf
	$(REASON)

# For the following, that means that we must make sure that the configure and 
# corresponding build targets are all done before the packages are built.
kernel-source  kernel_source:  stamp-configure stamp-build-indep stamp-kernel-source
	$(REASON)

stamp-kernel-source: install/$(s_package) binary/$(s_package) 
	$(REASON)
	echo done > $@
STAMPS_TO_CLEAN += stamp-kernel-source

kernel-manual  kernel_manual:  stamp-configure stamp-build-indep stamp-kernel-manual
	$(REASON)
stamp-kernel-manual: install/$(m_package) binary/$(m_package) 
	$(REASON)
	echo done > $@
STAMPS_TO_CLEAN += stamp-kernel-manual

kernel-doc     kernel_doc:     stamp-configure stamp-build-indep stamp-kernel-doc
	$(REASON)
stamp-kernel-doc: install/$(d_package) binary/$(d_package) 
	$(REASON)
	echo done > $@
STAMPS_TO_CLEAN += stamp-kernel-doc

kernel-headers kernel_headers: stamp-configure stamp-build-arch stamp-kernel-headers
	$(REASON)
stamp-kernel-headers: install/$(h_package) binary/$(h_package) 
	$(REASON)
	echo done > $@
STAMPS_TO_CLEAN += stamp-kernel-headers

kernel-image   kernel_image:   stamp-configure stamp-build-arch stamp-kernel-image
	$(REASON)
kernel-image-deb stamp-kernel-image: install/$(i_package) binary/$(i_package) 
	$(REASON)
	echo done > $@
STAMPS_TO_CLEAN += stamp-kernel-image

libc-kheaders libc_kheaders: 
	$(REASON)
	@echo This target is now obsolete.


$(eval $(which_debdir))
include $(DEBDIR)/ruleset/modules.mk

#Local variables:
#mode: makefile
#End:
