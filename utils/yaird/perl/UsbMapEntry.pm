#!perl -w
#
# UsbMapEntry -- encapsulate single line from modules.usbmap
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
# The ultimate authority on matching is
# linux/drivers/usb/core/usb.c:usb_match_id(),
# the routine where the kernel determines whether to let a particular
# driver probe an interface.  Both the hotplug usb.agent and this
# script mimic the behaviour of that function.
# AFAICS, usb_match_id is only invoked on interfaces, not on the devices
# these interfaces are part of.
# However, driver/usb/core/hub.c:usb_new_device() invokes device_add()
# for both the device and (via usb_set_configuration()) for the interfaces;
# this in turn will produce hotplut events (in the case of interfaces with
# a callback to usb_hotplug(), which puts product information in the hotplug
# environment), and the hotplug scripts will do matching for both devices
# and interfaces.  The coldplug script usb.rc emulates this kernel behaviour.
#
# Thus, there seems to be a discrepancy between what gets matched in the
# kernel and what gets matched in hotplug; we follow hotplug behaviour.
#
# Note that hotplug uses this matching algorithm not only on the module
# map provided by the kernel, but also on /etc/hotplug/usb/*.usermap;
# the latter results not in loading of a module but in execution of a script.
# This can be used to start gphoto, or it can be used to push firmware
# to a device.  Question: what should mkinitrd know about this?
#
use strict;
use warnings;
package UsbMapEntry;
use base 'Obj';


sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->takeArgs ('module', 'match_flags', 'idVendor', 
			'idProduct', 'bcdDevice_lo', 'bcdDevice_hi',
			'bDeviceClass', 'bDeviceSubClass', 'bDeviceProtocol',
			'bInterfaceClass', 'bInterfaceSubClass',
			'bInterfaceProtocol', 'driver_info');
}

sub module		{ return $_[0]->{module}; }
sub match_flags		{ return $_[0]->{match_flags}; }
sub idVendor		{ return $_[0]->{idVendor}; }
sub idProduct		{ return $_[0]->{idProduct}; }
sub bcdDevice_lo	{ return $_[0]->{bcdDevice_lo}; }
sub bcdDevice_hi	{ return $_[0]->{bcdDevice_hi}; }
sub bDeviceClass	{ return $_[0]->{bDeviceClass}; }
sub bDeviceSubClass	{ return $_[0]->{bDeviceSubClass}; }
sub bDeviceProtocol	{ return $_[0]->{bDeviceProtocol}; }
sub bInterfaceClass	{ return $_[0]->{bInterfaceClass}; }
sub bInterfaceSubClass	{ return $_[0]->{bInterfaceSubClass}; }
sub bInterfaceProtocol	{ return $_[0]->{bInterfaceProtocol}; }
sub driver_info		{ return $_[0]->{driver_info}; }

# we could do the USB_MATCH_XXX processing in init,
# if it turns out to be time critical.
my $USB_MATCH_VENDOR		= 0x0001;
my $USB_MATCH_PRODUCT		= 0x0002;
my $USB_MATCH_DEV_LO		= 0x0004;
my $USB_MATCH_DEV_HI		= 0x0008;
my $USB_MATCH_DEV_CLASS		= 0x0010;
my $USB_MATCH_DEV_SUBCLASS	= 0x0020;
my $USB_MATCH_DEV_PROTOCOL	= 0x0040;
my $USB_MATCH_INT_CLASS		= 0x0080;
my $USB_MATCH_INT_SUBCLASS	= 0x0100;
my $USB_MATCH_INT_PROTOCOL	= 0x0200;


sub matches {
	my ($self, $dev) = @_;
	my $match = $self->match_flags;

	if ($match & $USB_MATCH_VENDOR && $self->idVendor != $dev->idVendor) {
		return 0;
	}
	if ($match & $USB_MATCH_PRODUCT && $self->idProduct != $dev->idProduct) {
		return 0;
	}
	if ($match & $USB_MATCH_DEV_LO && $self->bcdDevice_lo > $dev->bcdDevice) {
		return 0;
	}
	if ($match & $USB_MATCH_DEV_HI && $self->bcdDevice_hi < $dev->bcdDevice) {
		return 0;
	}
	if ($match & $USB_MATCH_DEV_CLASS && $self->bDeviceClass != $dev->bDeviceClass) {
		return 0;
	}
	if ($match & $USB_MATCH_DEV_SUBCLASS && $self->bDeviceSubClass != $dev->bDeviceSubClass) {
		return 0;
	}
	if ($match & $USB_MATCH_DEV_PROTOCOL && $self->bDeviceProtocol != $dev->bDeviceProtocol) {
		return 0;
	}

	# Quoting usb.agent:
	# for now, this only checks the first of perhaps
	# several interfaces for this device.
	if ($match & $USB_MATCH_INT_CLASS && $self->bInterfaceClass != $dev->bInterfaceClass) {
		return 0;
	}
	if ($match & $USB_MATCH_INT_SUBCLASS && $self->bInterfaceSubClass != $dev->bInterfaceSubClass) {
		return 0;
	}
	if ($match & $USB_MATCH_INT_PROTOCOL && $self->bInterfaceProtocol != $dev->bInterfaceProtocol) {
		return 0;
	}

	return 1;
}

1;

