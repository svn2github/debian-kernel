############################ -*- Mode: Makefile -*- ###########################
## targets.mk --- 
## Author           : Manoj Srivastava ( srivasta@glaurung.green-gryphon.com ) 
## Created On       : Sat Nov 15 01:10:05 2003
## Created On Node  : glaurung.green-gryphon.com
## Last Modified By : Manoj Srivastava
## Last Modified On : Mon Apr 11 13:11:54 2005
## Last Machine Used: glaurung.internal.golden-gryphon.com
## Update Count     : 46
## Status           : Unknown, Use with caution!
## HISTORY          : 
## Description      : 
## 
## arch-tag: a81086a7-00f7-4355-ac56-8f38396935f4
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

#######################################################################
#######################################################################
###############             Miscellaneous               ###############
#######################################################################
#######################################################################
source diff:
	@echo >&2 'source and diff are obsolete - use dpkg-source -b'; false

testroot:
	@test $$(id -u) = 0 || (echo need root priviledges; exit 1)

checkpo:
	$(CHECKPO)

# arch-buildpackage likes to call this
prebuild: 

# OK. We have two sets of rules here, one for arch dependent packages,
# and one for arch independent packages. We have already calculated a
# list of each of these packages.

# In each set, we may need to do things in five steps: configure,
# build, install, package, and clean. Now, there can be a common
# actions to be taken for all the packages, all arch dependent
# packages, all all independent packages, and each package
# individually at each stage.

#######################################################################
#######################################################################
###############             Configuration               ###############
#######################################################################
#######################################################################

# Work here
CONFIG-common:: testdir

stamp-arch-conf:  testdir CONFIG-common
	@touch $@
stamp-indep-conf: testdir CONFIG-common
	@touch $@

# Work here
CONFIG-arch::  stamp-arch-conf
CONFIG-indep:: stamp-indep-conf

STAMPS_TO_CLEAN += stamp-arch-conf stamp-indep-conf
# Work here
$(patsubst %,CONFIG/%,$(DEB_ARCH_PACKAGES))  :: CONFIG/% : testdir CONFIG-arch  
$(patsubst %,CONFIG/%,$(DEB_INDEP_PACKAGES)) :: CONFIG/% : testdir CONFIG-indep 

stamp-configure-arch:  $(patsubst %,CONFIG/%,$(DEB_ARCH_PACKAGES))
	@touch $@
stamp-configure-indep: $(patsubst %,CONFIG/%,$(DEB_INDEP_PACKAGES))
	@touch $@

configure-arch:  stamp-configure-arch
configure-indep: stamp-configure-indep

stamp-configure: configure-arch configure-indep
	@touch $@

configure: stamp-configure

STAMPS_TO_CLEAN += stamp-configure-arch stamp-configure-indep stamp-configure
#######################################################################
#######################################################################
###############                 Build                   ###############
#######################################################################
#######################################################################

# Work here
BUILD-common:: testdir

stamp-arch-build:  testdir BUILD-common $(patsubst %,CONFIG/%,$(DEB_ARCH_PACKAGES))  
	@touch $@
stamp-indep-build: testdir BUILD-common $(patsubst %,CONFIG/%,$(DEB_INDEP_PACKAGES)) 
	@touch $@

STAMPS_TO_CLEAN += stamp-arch-build stamp-indep-build
# sync. Work here
BUILD-arch::  testdir stamp-arch-build
BUILD-indep:: testdir stamp-indep-build

# Work here
$(patsubst %,BUILD/%,$(DEB_ARCH_PACKAGES))  :: BUILD/% : testdir BUILD-arch  
$(patsubst %,BUILD/%,$(DEB_INDEP_PACKAGES)) :: BUILD/% : testdir BUILD-indep 

stamp-build-arch:  $(patsubst %,BUILD/%,$(DEB_ARCH_PACKAGES))
	@touch $@
stamp-build-indep: $(patsubst %,BUILD/%,$(DEB_INDEP_PACKAGES))
	@touch $@

build-arch:  stamp-build-arch
build-indep: stamp-build-indep

stamp-build: build-arch build-indep 
	@touch $@

build: stamp-build

# Work here
POST-BUILD-arch-stamp::
POST-BUILD-indep-stamp::

STAMPS_TO_CLEAN += stamp-build-arch stamp-build-indep stamp-build
#######################################################################
#######################################################################
###############                Install                  ###############
#######################################################################
#######################################################################
# Work here
INST-common:: testdir

stamp-arch-inst:  testdir POST-BUILD-arch-stamp  INST-common $(patsubst %,BUILD/%,$(DEB_ARCH_PACKAGES))    
	@touch $@
stamp-indep-inst: testdir POST-BUILD-indep-stamp INST-common $(patsubst %,BUILD/%,$(DEB_INDEP_PACKAGES)) 
	@touch $@

STAMPS_TO_CLEAN += stamp-arch-inst stamp-indep-inst
# sync. Work here
INST-arch::  testdir stamp-arch-inst
INST-indep:: testdir stamp-indep-inst

# Work here
$(patsubst %,INST/%,$(DEB_ARCH_PACKAGES))  :: INST/% : testdir testroot INST-arch  
$(patsubst %,INST/%,$(DEB_INDEP_PACKAGES)) :: INST/% : testdir testroot INST-indep 

stamp-install-arch:  $(patsubst %,INST/%,$(DEB_ARCH_PACKAGES))
	@touch $@
stamp-install-indep: $(patsubst %,INST/%,$(DEB_INDEP_PACKAGES))
	@touch $@

install-arch:  stamp-install-arch
install-indep: stamp-install-indep

stamp-install: install-indep install-arch
	@touch $@

install: stamp-install

STAMPS_TO_CLEAN += stamp-install stamp-install-arch stamp-install-indep
#######################################################################
#######################################################################
###############                Package                  ###############
#######################################################################
#######################################################################
# Work here
BIN-common:: testdir testroot 

stamp-arch-bin:  testdir testroot BIN-common  $(patsubst %,INST/%,$(DEB_ARCH_PACKAGES))
	@touch $@
stamp-indep-bin: testdir testroot BIN-common  $(patsubst %,INST/%,$(DEB_INDEP_PACKAGES))
	@touch $@

STAMPS_TO_CLEAN += stamp-arch-bin stamp-indep-bin
# sync Work here
BIN-arch::  testdir testroot  stamp-arch-bin
BIN-indep:: testdir testroot  stamp-indep-bin

# Work here
$(patsubst %,BIN/%,$(DEB_ARCH_PACKAGES))  :: BIN/% : testdir testroot BIN-arch  
$(patsubst %,BIN/%,$(DEB_INDEP_PACKAGES)) :: BIN/% : testdir testroot BIN-indep 


stamp-binary-arch:  $(patsubst %,BIN/%,$(DEB_ARCH_PACKAGES)) 
	@touch $@
stamp-binary-indep: $(patsubst %,BIN/%,$(DEB_INDEP_PACKAGES))
	@touch $@
# required
binary-arch:  stamp-binary-arch
binary-indep: stamp-binary-indep

stamp-binary: binary-indep binary-arch
	@touch $@

# required
binary: stamp-binary

STAMPS_TO_CLEAN += stamp-binary stamp-binary-arch stamp-binary-indep
#######################################################################
#######################################################################
###############                 Clean                   ###############
#######################################################################
#######################################################################
# Work here
CLN-common:: testdir 
# sync Work here
CLN-arch::  testdir CLN-common
CLN-indep:: testdir CLN-common
# Work here
$(patsubst %,CLEAN/%,$(DEB_ARCH_PACKAGES))  :: CLEAN/% : testdir CLN-arch
$(patsubst %,CLEAN/%,$(DEB_INDEP_PACKAGES)) :: CLEAN/% : testdir CLN-indep

clean-arch:  $(patsubst %,CLEAN/%,$(DEB_ARCH_PACKAGES))   
clean-indep: $(patsubst %,CLEAN/%,$(DEB_INDEP_PACKAGES))

stamp-clean: clean-indep clean-arch
	$(checkdir)
	-test -f Makefile && $(MAKE) distclean
	-rm -f  $(FILES_TO_CLEAN) $(STAMPS_TO_CLEAN)
	-rm -rf $(DIRS_TO_CLEAN)
	-rm -f core `find . \( -name '*.orig' -o -name '*.rej' -o -name '*~'     \
	         -o -name '*.bak' -o -name '#*#' -o -name '.*.orig'              \
		 -o -name '.*.rej' -o -name '.SUMS' -o -size 0 \) -print` TAGS

clean: stamp-clean


#######################################################################
#######################################################################
###############                                         ###############
#######################################################################
#######################################################################

.PHONY: CONFIG-common  CONFIG-indep  CONFIG-arch  configure-arch  configure-indep  configure     \
        BUILD-common   BUILD-indep   BUILD-arch   build-arch      build-indep      build         \
        INST-common    INST-indep    INST-arch    install-arch    install-indep    install       \
        BIN-common     BIN-indep     BIN-arch     binary-arch     binary-indep     binary        \
        CLN-common     CLN-indep     CLN-arch     clean-arch      clean-indep      clean         \
        $(patsubst %,CONFIG/%,$(DEB_INDEP_PACKAGES)) $(patsubst %,CONFIG/%,$(DEB_ARCH_PACKAGES)) \
        $(patsubst %,BUILD/%, $(DEB_INDEP_PACKAGES)) $(patsubst %,BUILD/%, $(DEB_ARCH_PACKAGES)) \
        $(patsubst %,INST/%,  $(DEB_INDEP_PACKAGES)) $(patsubst %,INST/%,  $(DEB_ARCH_PACKAGES)) \
        $(patsubst %,BIN/%,   $(DEB_INDEP_PACKAGES)) $(patsubst %,BIN/%,   $(DEB_ARCH_PACKAGES)) \
        $(patsubst %,CLEAN/%, $(DEB_INDEP_PACKAGES)) $(patsubst %,CLEAN/%, $(DEB_ARCH_PACKAGES)) \
        implode explode prebuild checkpo

