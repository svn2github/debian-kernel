# 
# Makefile.am -- installation and packaging instructions
#   Copyright (C) 2005  Erik van Konijnenburg
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
#
include ../Makefile.incl

edit =	sed \
	-e 's!@PERL\@!$(PERL)!' \
	-e 's!@PACKAGE\@!$(PACKAGE)!' \
	-e 's!@VERSION\@!$(VERSION)!' \
	-e 's!@AUXDIR\@!$(auxdir)!' \
	-e 's!@CFGDIR\@!$(configdir)!' \
	-e 's!@PERLDIR\@!$(perldir)!'

Default.cfg: Default.cfg.in
	$(edit) < $< > $@

# not installed, but must go in the tarball.
EXTRA_DIST = \
	Default.cfg.in

CLEANFILES	= Default.cfg
nodist_config_DATA = \
	Default.cfg

# installed, and must go in tarball
dist_config_DATA = \
	Fedora.cfg \
	Debian.cfg \
	Debian-initrd.cfg

