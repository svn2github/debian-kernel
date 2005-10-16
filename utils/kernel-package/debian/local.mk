############################ -*- Mode: Makefile -*- ###########################
## local.mk --- 
## Author           : Manoj Srivastava ( srivasta@glaurung.green-gryphon.com ) 
## Created On       : Sat Nov 15 10:42:10 2003
## Created On Node  : glaurung.green-gryphon.com
## Last Modified By : Manoj Srivastava
## Last Modified On : Tue Feb 17 08:31:06 2004
## Last Machine Used: glaurung.internal.golden-gryphon.com
## Update Count     : 17
## Status           : Unknown, Use with caution!
## HISTORY          : 
## Description      : 
## 
## arch-tag: b07b1015-30ba-4b46-915f-78c776a808f4
## 
###############################################################################

BUILD/kernel-package:: check/kernel-package
INST/kernel-package::  install/kernel-package
BIN/kernel-package::   binary/kernel-package

testdir:
	$(checkdir)

check/kernel-package:        
	$(REASON)
	$(checkdir)
	$(MAKE) build

install/kernel-package: testroot
	$(REASON)
	$(checkdir)
	rm -rf $(TMPTOP)  $(TMPTOP).deb
	$(make_directory)  $(TMPTOP)/etc
	$(make_directory)  $(TMPTOP)/usr/bin
	$(make_directory)  $(TMPTOP)/usr/sbin
	$(make_directory)  $(TMPTOP)/usr/share/$(package)
	$(make_directory)  $(LINTIANDIR)
	echo "$(package): description-synopsis-might-not-be-phrased-properly" \
                                >>          $(LINTIANDIR)/$(package)
	chmod 0644                         $(LINTIANDIR)/$(package)
	$(MAKE) version=$(DEB_VERSION)  prefix=$(TMPTOP) install

binary/kernel-package: testroot
	$(REASON)
	$(checkdir)
	$(make_directory)  $(TMPTOP)/DEBIAN
	$(install_file)    debian/conffiles             $(TMPTOP)/DEBIAN/conffiles
	dpkg-gencontrol -isp
	chown -R root:root $(TMPTOP)
	chmod -R u+w,go=rX $(TMPTOP)
	dpkg --build       $(TMPTOP) ..
	touch              stamp-binary
