# -*- mode: makefile; coding: utf-8 -*-
# Copyright © 2004 Jonas Smedegaard <dr@jones.dk>
# Description: Generate and include build information
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2, or (at
# your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA


ifndef _cdbs_bootstrap
_cdbs_scripts_path ?= /usr/lib/cdbs
_cdbs_rules_path ?= /usr/share/cdbs/1/rules
_cdbs_class_path ?= /usr/share/cdbs/1/class
endif

ifndef _cdbs_class_buildinfo
_cdbs_class_buildinfo := 1

include $(_cdbs_rules_path)/buildcore.mk$(_cdbs_makefile_suffix)

CDBS_BUILD_DEPENDS := $(CDBS_BUILD_DEPENDS), dh-buildinfo

common-install-arch common-install-indep:: debian/stamp-buildinfo

debian/stamp-buildinfo:
	dh_buildinfo

clean::
	rm -f debian/stamp-buildinfo

endif
