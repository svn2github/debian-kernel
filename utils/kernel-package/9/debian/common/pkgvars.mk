############################ -*- Mode: Makefile -*- ###########################
## pkgvars.mk --- 
## Author           : Manoj Srivastava ( srivasta@glaurung.green-gryphon.com ) 
## Created On       : Sat Nov 15 02:56:30 2003
## Created On Node  : glaurung.green-gryphon.com
## Last Modified By : Manoj Srivastava
## Last Modified On : Tue Nov 18 01:06:00 2003
## Last Machine Used: glaurung.green-gryphon.com
## Update Count     : 5
## Status           : Unknown, Use with caution!
## HISTORY          : 
## Description      : 
## 
## arch-tag: 75fcc720-7389-4eaa-a7ac-c556d3eac331
## 
###############################################################################

# The maintainer information.
maintainer := $(shell LC_ALL=C dpkg-parsechangelog | grep ^Maintainer: | \
                sed 's/^Maintainer: *//')
email := srivasta@debian.org

# Priority of this version (or urgency, as dchanges would call it)
urgency := $(shell LC_ALL=C dpkg-parsechangelog | grep ^Urgency: | \
             sed 's/^Urgency: *//')

# Common useful variables
DEB_SOURCE_PACKAGE := $(strip $(shell egrep '^Source: ' debian/control      |       \
                                      cut -f 2 -d ':'))
DEB_VERSION        := $(strip $(shell LC_ALL=C dpkg-parsechangelog          |       \
                                      egrep '^Version:' | cut -f 2 -d ' '))
DEB_ISNATIVE       := $(strip $(shell LC_ALL=C dpkg-parsechangelog          |       \
                       perl -ne 'print if (m/^Version:/g && ! m/^Version:.*\-/);'))

DEB_PACKAGES := $(shell perl -e '                                                    \
                  $$/="";                                                            \
                  while(<>){                                                         \
                     $$p=$$1 if m/^Package:\s*(\S+)/;                                \
                     die "duplicate package $$p" if $$seen{$$p};                     \
                     $$seen{$$p}++; print "$$p " if $$p;                             \
                  }' debian/control )

DEB_INDEP_PACKAGES := $(shell perl -e '                                              \
                         $$/="";                                                     \
                         while(<>){                                                  \
                            $$p=$$1 if m/^Package:\s*(\S+)/;                         \
                            die "duplicate package $$p" if $$seen{$$p};              \
                            $$seen{$$p}++;                                           \
                            $$a=$$1 if m/^Architecture:\s*(\S+)/m;                   \
                            next unless ($$a eq "all");                              \
                            print "$$p " if $$p;                                     \
                         }' debian/control )

DEB_ARCH_PACKAGES := $(shell perl -e '                                               \
                         $$/="";                                                     \
                         while(<>){                                                  \
                            $$p=$$1 if m/^Package:\s*(\S+)/;                         \
                            die "duplicate package $$p" if $$seen{$$p};              \
                            $$seen{$$p}++;                                           \
                            $$a=$$1 if m/^Architecture:\s*(\S+)/m;                   \
                            next unless ($$a eq "$(DEB_HOST_ARCH)" || $$a eq "any"); \
                            print "$$p " if $$p;                                     \
                         }' debian/control )

# This package is what we get after removing the psuedo dirs we use in rules
package = $(notdir $@)


