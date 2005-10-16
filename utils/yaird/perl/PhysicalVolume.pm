#!perl -w
#
# PhysicalVolume -- Physical Volume Descriptor
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
# pvnam - physical volume name
# vgnam - volume group name
# devno - "major:minor"
#

use strict;
use warnings;
package PhysicalVolume;
use base 'Obj';

sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->takeArgs ('pvnam', 'vgnam', 'uuid');
}

sub pvnam	{ return $_[0]->{pvnam}; }
sub vgnam	{ return $_[0]->{vgnam}; }
sub uuid	{ return $_[0]->{uuid}; }

sub string {
	my $self = shift;
	my $pvnam = $self->pvnam;
	my $vgnam = $self->vgnam;
	my $uuid = $self->uuid;
	my $str = "$pvnam in $vgnam, $uuid";
	return $str;
}

1;
