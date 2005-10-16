#!perl -w
#
# RaidDev -- the probed values for a raid device, as found by mdadm.
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
use strict;
use warnings;
package RaidDev;
use base 'Obj';

sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->takeArgs ('path', 'devno', 'uuid', 'level', 'devices');
}

sub path	{ return $_[0]->{path}; }
sub devno	{ return $_[0]->{devno}; }
sub uuid	{ return $_[0]->{uuid}; }
sub level	{ return $_[0]->{level}; }
sub devices	{ return $_[0]->{devices}; }

sub string {
	my $self = shift;
	my $path = $self->path;
	my $devno = $self->devno;
	my $uuid = $self->uuid;
	my $level = $self->level;
	my $devices = join (',', @{$self->devices});
	return "$path($devno, $level) = $uuid at $devices";
}


1;
