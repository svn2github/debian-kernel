#!perl -w
#
# LogicalVolume -- Logical Volume Descriptor
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
# lvnam - logical volume name
# vgnam - volume group name
# devno - "major:minor"
#

use strict;
use warnings;
package LogicalVolume;
use base 'Obj';

sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->takeArgs ('lvnam', 'vgnam', 'devno');
}

sub lvnam	{ return $_[0]->{lvnam}; }
sub vgnam	{ return $_[0]->{vgnam}; }
sub devno	{ return $_[0]->{devno}; }

sub string {
	my $self = shift;
	my $lvnam = $self->lvnam;
	my $vgnam = $self->vgnam;
	my $devno = $self->devno;
	my $str = "$lvnam in $vgnam at $devno";
	return $str;
}

1;
