#!perl -w
#
# PciDev -- the probed values for a PCI device, as found in /sys.
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
use Base;
package PciDev;
use base 'Obj';

sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->takeArgs ('path');
	my $path = $self->path;
	$self->{vendor} = Base::getHexFile ("$path/vendor");
	$self->{device} = Base::getHexFile ("$path/device");
	$self->{subvendor} = Base::getHexFile ("$path/subsystem_vendor");
	$self->{subdevice} = Base::getHexFile ("$path/subsystem_device");
	$self->{class} = Base::getHexFile ("$path/class");
}

sub path	{ return $_[0]->{path}; }
sub vendor	{ return $_[0]->{vendor}; }
sub device	{ return $_[0]->{device}; }
sub subvendor	{ return $_[0]->{subvendor}; }
sub subdevice	{ return $_[0]->{subdevice}; }
sub class	{ return $_[0]->{class}; }


1;
