#!perl -w
#
# NetDevTab -- active network devices, based on /sys/class/net.
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
use NetDev;
package NetDevTab;


my $netTab = undef;		# indexed by name
my $netList = undef;


sub init () {
	if (defined ($netTab)) {
		return;
	}
	$netTab = {};

	my $netDir;
	my $netPath = Conf::get('sysFs') . "/class/net";
	if (! opendir ($netDir, "$netPath")) {
		Base::fatal ("can't open directory $netPath");
	}
	while (defined (my $entry = readdir ($netDir))) {
		next if $entry eq ".";
		next if $entry eq "..";
		oneNetDev($netPath, $entry);
	}
	if (! closedir ($netDir)) {
		Base::fatal ("could not read directory $netDir");
	}

	# the sort helps eth0 get loaded before eth1
	# no guarantee, since devices may have been renamed via udev.
	$netList = [ sort {$a->name cmp $b->name} values %{$netTab}];
}

sub oneNetDev ($$) {
	my ($netPath, $entry) = @_;
	my $hw = readlink("$netPath/$entry/device");
	# failure results in undef, which is exactly how
	# we want to represent absence of a hw device.
	if (defined ($hw)) {
		unless ($hw =~ s!^(\.\./\.\./\.\./)+devices/!!) {
			# imagine localised linux (/sys/geraete ...)
			Base::fatal ("bad device link in $netPath/$entry");
		}
	}
	my $descr = NetDev->new (
		name => $entry,
		hw => $hw,
		);
	$netTab->{$entry} = $descr;
}

#
# all -- return list of all known active block devices.
#
sub all	() {
	init;
	return $netList;
}

1;
