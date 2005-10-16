#!perl -w
#
# UsbTab -- encapsulate modules.usbmap
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
use UsbMapEntry;
package UsbTab;

my $usbList = undef;

# Parse this:
# # usb module         match_flags idVendor idProduct bcdDevice_lo bcdDevice_hi bDeviceClass bDeviceSubClass bDeviceProtocol bInterfaceClass bInterfaceSubClass bInterfaceProtocol driver_info
# bfusb                0x0003      0x057c   0x2200    0x0000       0x0000       0x00         0x00            0x00            0x00            0x00               0x00               0x0
# bcm203x              0x0003      0x0a5c   0x2033    0x0000       0x0000       0x00         0x00            0x00            0x00            0x00               0x00               0x0

sub init () {
	if (defined ($usbList)) {
		return;
	}
	$usbList = [];
	my $name = Conf::get('usbMap');
	if (! open (IN, "<", "$name")) {
		Base::fatal ("can't open usb module list $name");
	}
	while (defined (my $line = <IN>)) {
		chomp $line;
		$line =~ s/#.*//;
		$line =~ s/^\s+//;
		$line =~ s/\s+$//;
		next if ($line eq "");
		my @fields = split (/\s+/, $line, 999);
		if ($#fields != 12) {
			Base::fatal ("malformed line in usb module list $name");
		}
		push @{$usbList}, UsbMapEntry->new (
			module => $fields[0],
			match_flags => hex ($fields[1]),
			idVendor => hex ($fields[2]),
			idProduct => hex ($fields[3]),
			bcdDevice_lo => hex ($fields[4]),
			bcdDevice_hi => hex ($fields[5]),
			bDeviceClass => hex ($fields[6]),
			bDeviceSubClass => hex ($fields[7]),
			bDeviceProtocol => hex ($fields[8]),
			bInterfaceClass => hex ($fields[9]),
			bInterfaceSubClass => hex ($fields[10]),
			bInterfaceProtocol => hex ($fields[11]),
			driver_info => hex ($fields[12]),
			);
	}
	if (! close (IN)) {
		Base::fatal ("could not read usb module list $name");
	}
}

sub all	() {
	init;
	return $usbList;
}

# given pathname in devices tree, return list of matching modules.
sub find ($) {
	my ($dev) = @_;
	my @result = ();
	for my $ume (@{UsbTab::all()}) {
		if ($ume->matches ($dev)) {
			push @result, $ume->module;
		}
	}
	return [@result];
}


1;
