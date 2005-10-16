#!perl -w
#
# NetDev - a single network device, just name, hw
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
# The blockdev can be a whole device or a partition.
# descriptors contain:
# - name, path as found in /sys (sda, sda/sda1)
# - hw, path to underlying hardware device,
# 		eg an ide controller somewhere on a PCI bus.
#		this is a path relative to /sys/devices,
#		and can be undef (eg for ram disk)
#
use strict;
use warnings;
package NetDev;
use base 'Obj';


sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->takeArgs ('name', 'hw');
}

sub name	{ return $_[0]->{name}; }
sub hw		{ return $_[0]->{hw}; }

sub string {
	my $self = shift;
	my $name = $self->name;
	my $hw = ($self->hw or "--");
	return "$name at $hw";
}

1;
