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
aux_PROGRAMS = ipconfig
noinst_LIBRARIES = libipconfig.a

include ../../Makefile.incl

ipconfig_SOURCES = bootp_proto.c dhcp_proto.c main.c netdev.c packet.c \
	bootp_packet.h bootp_proto.h dhcp_proto.h ipconfig.h netdev.h packet.h
libipconfig_a_SOURCES = bootp_proto.c dhcp_proto.c main.c netdev.c packet.c \
	bootp_packet.h bootp_proto.h dhcp_proto.h ipconfig.h netdev.h packet.h
