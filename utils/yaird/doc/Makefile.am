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
# Quick hack of a makefile, based on dia xml documentation.
# using the XSL sheets with xsltproc is a lot quicker than
# using jade via db2html; the generated html also is a lot more
# readable.  On the other hand, jade's validation looks more robust.
#

noinst_DATA	= yaird.html
CLEANFILES	= $(noinst_DATA)

yaird.html: $(EXTRA_DIST) yaird.xml
	$(XMLTO) xhtml-nochunks yaird.xml



# not compiled, but must go in the tarball.
EXTRA_DIST = \
	authors.xml \
	concepts.xml \
	crypto.xml \
	figures/console.dia \
	figures/console.png \
	input.xml \
	intro.xml \
	kernel.xml \
	license.xml \
	nfs.xml \
	raid.xml \
	security.xml \
	shlibs.xml \
	spec.xml \
	tools.xml
