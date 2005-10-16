#!perl -w
#
# VolumeGroup -- Volume Group Descriptor
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
# vgnam - volume group name
# uuid - uuid
#
use strict;
use warnings;
package VolumeGroup;
use base 'Obj';

sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->takeArgs ('vgnam', 'uuid');
}

sub vgnam	{ return $_[0]->{vgnam}; }
sub uuid	{ return $_[0]->{uuid}; }

sub string {
	my $self = shift;
	my $vgnam = $self->vgnam;
	my $uuid = $self->uuid;
	my $str = "$vgnam, $uuid";
	return $str;
}

1;
