#!perl -w
#
# PciTab -- encapsulate modules.pcimap
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
use Conf;
use PciMapEntry;
package PciTab;

my $pciList = undef;

# Parse this:
# # pci module         vendor     device     subvendor  subdevice  class      class_mask driver_data
# parport_pc           0x00001106 0x00000686 0xffffffff 0xffffffff 0x00000000 0x00000000 0x0
# parport_pc           0x00001106 0x00008231 0xffffffff 0xffffffff 0x00000000 0x00000000 0x0

sub init () {
	if (defined ($pciList)) {
		return;
	}
	$pciList = [];
	my $name = Conf::get('pciMap');
	if (! open (IN, "<", "$name")) {
		Base::fatal ("can't open pci module list $name");
	}
	while (defined (my $line = <IN>)) {
		chomp $line;
		$line =~ s/#.*//;
		$line =~ s/^\s+//;
		$line =~ s/\s+$//;
		next if ($line eq "");
		my @fields = split (/\s+/, $line, 999);
		if ($#fields != 7) {
			Base::fatal "malformed line in pci module list $name";
		}
		push @{$pciList}, PciMapEntry->new (
			module => $fields[0],
			vendor => hex ($fields[1]),
			device => hex ($fields[2]),
			subvendor => hex ($fields[3]),
			subdevice => hex ($fields[4]),
			class => hex ($fields[5]),
			classmask => hex ($fields[6]),
			driver_data => hex ($fields[7]),
			);
	}
	if (! close (IN)) {
		Base::fatal "could not read pci module list $name";
	}
}

sub all	() {
	init;
	return $pciList;
}

# given pathname in devices tree, find module name in PCI map as a list.
sub find ($) {
	my ($dev) = @_;
	my @result = ();
	for my $pme (@{PciTab::all()}) {
		if ($pme->matches ($dev)) {
			push @result, $pme->module;
		}
	}
	return [@result];
}


1;

