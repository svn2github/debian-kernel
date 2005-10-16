#
# Makefile.am -- compiling executables to be included in generated image
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
# NOTE: We compile the same sources twice,
# once as standalone program, once as library to be linked with trymount.
#

# place package-specific executables that may go on the image in
# this directory.
aux_PROGRAMS = nfsmount
noinst_LIBRARIES = libnfsmount.a

include ../../Makefile.incl

#
# nfs_mount.h includes linux/in.h, which breaks glibc,
# so we can't use it.
# We also cannot inline the code, since that would make the whole
# program GPL, and GPL does not allow extra restrictions
# on distribution, and HPA's licence has the restriction
# that his disclaimer be kept intact.
# The fun part is that the kernel compiles perfectly well
# without the offending linux/in.h.
#
CLEANFILES = ridiculous.h
BUILT_SOURCES = ridiculous.h
ridiculous.h:	/usr/include/linux/nfs_mount.h
	grep -v '<linux/in.h>' < $< > $@

nfsmount_SOURCES = dummypmap.c main.c mount.c portmap.c sunrpc.c \
	dummypmap.h nfsmount.h sunrpc.h
libnfsmount_a_SOURCES = dummypmap.c main.c mount.c portmap.c sunrpc.c \
	dummypmap.h nfsmount.h sunrpc.h
