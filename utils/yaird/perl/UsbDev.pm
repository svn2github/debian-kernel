#!perl -w
#
# UsbDev -- the probed values for a USB device, as found in /sys.
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
# The information passed to the matcher should correspond to what
# is passed by hotplug.
#
# For devices:
#	vendor, product, bcdDevice = 0, 0, 0
#	deviceClass, subClass, protocol = from /sys
#	interfaceClass, subClass, protocol = 1000,1000,1000
# For interfaces:
#	vendor, product, bcdDevice = from /sys in parent device
#	deviceClass, subClass, protocol = 1000,1000,1000
#	interfaceClass, subClass, protocol = from /sys
#
# Note that somewhere in 2.5, the hotplug events for USB
# changed: old kernels only had an event for the first interface,
# with no distinction between interface and device.
#
use strict;
use warnings;
use Base;
package UsbDev;
use base 'Obj';


sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->takeArgs ('path');
	my $path = $self->{path};
	if (-f "$path/bDeviceClass") {
		$self->{idVendor} = 0;
		$self->{idProduct} = 0;
		$self->{bcdDevice} = 0;
		$self->{bDeviceClass} = Base::getHexFile ("$path/bDeviceClass");
		$self->{bDeviceSubClass} = Base::getHexFile ("$path/bDeviceSubClass");
		$self->{bDeviceProtocol} = Base::getHexFile ("$path/bDeviceProtocol");
		$self->{bInterfaceClass} = 1000;
		$self->{bInterfaceSubClass} = 1000;
		$self->{bInterfaceProtocol} = 1000;
	}
	elsif (-f "$path/bInterfaceClass") {
		$self->{idVendor} = Base::getHexFile ("$path/../idVendor");
		$self->{idProduct} = Base::getHexFile ("$path/../idProduct");
		$self->{bcdDevice} = Base::getHexFile ("$path/../bcdDevice");
		$self->{bDeviceClass} = 1000;
		$self->{bDeviceSubClass} = 1000;
		$self->{bDeviceProtocol} = 1000;
		$self->{bInterfaceClass} = Base::getHexFile ("$path/bInterfaceClass");
		$self->{bInterfaceSubClass} = Base::getHexFile ("$path/bInterfaceSubClass");
		$self->{bInterfaceProtocol} = Base::getHexFile ("$path/bInterfaceProtocol");
	}
	else {
		Base::fatal "trying to interpret $path as /sys USB devive";
	}
}

sub idVendor		{ return $_[0]->{idVendor}; }
sub idProduct		{ return $_[0]->{idProduct}; }
sub bcdDevice		{ return $_[0]->{bcdDevice}; }
sub bDeviceClass	{ return $_[0]->{bDeviceClass}; }
sub bDeviceSubClass	{ return $_[0]->{bDeviceSubClass}; }
sub bDeviceProtocol	{ return $_[0]->{bDeviceProtocol}; }
sub bInterfaceClass	{ return $_[0]->{bInterfaceClass}; }
sub bInterfaceSubClass	{ return $_[0]->{bInterfaceSubClass}; }
sub bInterfaceProtocol	{ return $_[0]->{bInterfaceProtocol}; }



1;
