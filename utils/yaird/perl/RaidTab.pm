#!perl -w
#
# RaidTab -- encapsulate mdadm output
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
# NOTE: The mdadm --detail shows only devices that are running,
# and does not distinguish between partitionable and non-partitionable
# devices.
#
use strict;
use warnings;
use Base;
use Conf;
use RaidDev;
package RaidTab;


my $raidTab = undef;


sub init () {
	if (defined ($raidTab)) {
		return;
	}

	# my $name = Conf::get('fstab');
	$raidTab = [];
	my $in;
	if (! open ($in, "-|", "/sbin/mdadm --detail --scan")) {
		Base::fatal ("Can't read mdadm output");
	}
	my @lines = <$in>;
	if (! close ($in)) {
		Base::fatal ("Could not read mdadm output");
	}
	my $buf;
	for my $line (@lines) {
		chomp $line;
		if ($line =~ /^\s+/) {
			$buf = $buf . $line;
		}
		else {
			if (defined ($buf)) {
				processLine ($buf);
			}
			$buf = $line;
		}
	}
	if (defined ($buf)) {
		processLine ($buf);
	}
}

sub processLine ($) {
	my ($line) = @_;
	my @fields = split (/\s+/, $line);
	if ($fields[0] ne "ARRAY") {
		Base::fatal ("Expected ARRAY keyword in mdadm output");
	}
	my $path = $fields[1];
	my $uuid;
	my $level;
	my $devices;
	for my $i (2 .. $#fields) {
		if ($fields[$i] =~ /^uuid=(.+)$/i) {
			if (defined ($uuid)) {
				Base::fatal ("duplicate uuid attribute in mdadm output");
			}
			$uuid = $1;
		}
		elsif ($fields[$i] =~ /^super-minor=(\d+)$/i) {
			# nothing
		}
		elsif ($fields[$i] =~ /^devices=(.+)$/i) {
			if (defined ($devices)) {
				Base::fatal ("duplicate devices attribute in mdadm output");
			}
			$devices = $1;
		}
		elsif ($fields[$i] =~ /^level=(.+)$/i) {
			if (defined ($level)) {
				Base::fatal ("duplicate level attribute in mdadm output");
			}
			$level = $1;
		}
		elsif ($fields[$i] =~ /^num-devices=(\d+)$/i) {
			# nothing
		}
		elsif ($fields[$i] =~ /^spare-group=(.+)$/i) {
			# nothing
		}
		elsif ($fields[$i] =~ /^auto=(.+)$/i) {
			# nothing
		}
		elsif ($fields[$i] =~ /^spares=(\d+)$/i) {
			# nothing
		}
		else {
			my $pair = $fields[$i];
			Base::fatal ("Unknown attribute $pair in mdadm output");
		}
	}
	if (! defined($path)) {
		Base::fatal ("Missing device field in mdadm output");
	}
	if (! defined($uuid)) {
		Base::fatal ("Missing uuid attribute in mdadm output");
	}
	if (! defined($level)) {
		Base::fatal ("Missing level attribute in mdadm output");
	}
	if (! defined($devices)) {
		Base::fatal ("Missing devices attribute in mdadm output");
	}

	my $devno = Base::devno ($path);
	if (! defined($devno)) {
		Base::fatal ("Device '$path' in mdadm output: cant find device major/minor number");
	}

	my $descr = RaidDev->new (
		path => $path,
		devno => $devno,
		uuid => $uuid,
		level => $level,
		devices => [split (/,/, $devices)],
		);
	push @{$raidTab}, $descr;
}


sub all	() {
	init;
	return $raidTab;
}

sub findByPath ($) {
	my ($path) = @_;
	for my $rd (@{all()}) {
		if ($rd->path() eq $path) {
			return $rd;
		}
	}
	return undef;
}

sub findByDevno ($) {
	my ($devno) = @_;
	for my $rd (@{all()}) {
		if ($rd->devno() eq $devno) {
			return $rd;
		}
	}
	return undef;
}

1;
