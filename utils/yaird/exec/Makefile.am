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
#
# The interesting part here is integration with klibc: it's absent.
# The hope is that by specifying -I and -l flags to configure, an external
# klibc build can be used.  This is untested.
#

# place package-specific executables that may go on the image in
# this directory.

SUBDIRS = ipconfig nfsmount
aux_PROGRAMS = run_init findlibs trynfs

include ../Makefile.incl

run_init_SOURCES = run_init.c
findlibs_SOURCES = findlibs.c
trynfs_SOURCES = trynfs.c
trynfs_LDADD = nfsmount/libnfsmount.a ipconfig/libipconfig.a
