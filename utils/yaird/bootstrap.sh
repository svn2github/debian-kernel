#
# Bootstrap.sh - generate files needed to configure the package
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
# Use this if you got yaird from version control and need
# to generate the configure scripts that come with the tarball.
#
# See http://sources.redhat.com/autobook/.
# See http://www.gnu.org/software/autoconf/manual/autoconf-2.57/
#
# V=--verbose
# Q=--quiet
# set -x

# to build makefiles for shared libraries
# libtoolize --force  || exit 1

# copy definitions from /usr/share to ./aclocal.m4
# for macros that are used in configure.in
aclocal $V  || exit 1

# use aclocal.m4 to create a configure file from configure.in
autoconf $V  || exit 1

# Build config.h.in, needed if you have C code.
autoheader  || exit 1

# transform makefile.am to makefile.in;
# copy helpful scripts such as install-sh from /usr/share to here.
automake $V --add-missing  || exit 1

# At this point, the tree can be tarred, transplanted to the target
# system and installed with:
# configure --prefix=/home/santaclaus/testing --enable-template=Debian
# make install
