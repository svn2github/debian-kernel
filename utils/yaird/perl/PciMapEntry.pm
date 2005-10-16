#!perl -w
#
# PciMapEntry - iencapsulate a line form modules.pcimap.
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
# Knows how to match, can return module name.
#

use strict;
use warnings;
use Base;
package PciMapEntry;
use base 'Obj';

sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->takeArgs ('module', 'vendor', 'device', 'subvendor',
			'subdevice', 'class', 'classmask', 'driver_data');
}

sub module	{ return $_[0]->{module}; }

# we could do the PCI_ANY processing in init,
# if it turns out to be time critical.
my $PCI_ANY = 0xffffffff;
sub matches {
	my ($self, $dev) = @_;
	if ($self->{vendor} != $PCI_ANY  && $self->{vendor} != $dev->vendor) {
		return 0;
	}
	if ($self->{device} != $PCI_ANY  && $self->{device} != $dev->device) {
		return 0;
	}
	if ($self->{subvendor} != $PCI_ANY 
		&& $self->{subvendor} != $dev->subvendor) {
		return 0;
	}
	if ($self->{subdevice} != $PCI_ANY
		&& $self->{subdevice} != $dev->subdevice) {
		return 0;
	}
	if ($self->{class} == ($dev->class & $self->{classmask})) {
		return 1;
	}
	return 0;
}

1;

