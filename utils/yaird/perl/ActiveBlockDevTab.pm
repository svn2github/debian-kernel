#!perl -w
#
# ActiveBlockDevTab -- descriptors of all block devices in /sys
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
use ActiveBlockDev;
package ActiveBlockDevTab;


my $abdTab = undef;
my $abdList = undef;


sub init () {
	if (defined ($abdTab)) {
		return;
	}
	$abdTab = {};

	my $blockdir;
	my $blockPath = Conf::get('sysFs') . "/block";
	if (! opendir ($blockdir, "$blockPath")) {
		Base::fatal ("can't open directory $blockPath");
	}
	while (defined (my $entry = readdir ($blockdir))) {
		next if $entry eq ".";
		next if $entry eq "..";
		oneBlockDev($blockPath, $entry);
	}
	if (! closedir ($blockdir)) {
		Base::fatal ("could not read directory $blockPath");
	}

	$abdList = [values %{$abdTab}];
}

sub oneBlockDev ($$) {
	my ($blockPath, $entry) = @_;
	my $devno = Base::getStringFile("$blockPath/$entry/dev");
	if (exists ($abdTab->{$devno})) {
		Base::fatal ("found duplicate devno in $blockPath/$entry");
	}
	my $hw = readlink("$blockPath/$entry/device");
	# failure results in undef, which is exactly how
	# we want to represent absence of a hw device.
	if (defined ($hw)) {
		unless ($hw =~ s!^(\.\./)+devices/!!) {
			# imagine localised linux (/sys/geraete ...)
			Base::fatal ("bad device link in $blockPath/$entry");
		}
	}
	my $descr = ActiveBlockDev->new (
		name => $entry,
		devno => $devno,
		parent => undef,
		hw => $hw,
		);
	$abdTab->{$devno} = $descr;

	#
	# Scan partitions.
	# Note that these are only visible after someone
	# read the a block from the device.
	#
	my $devdir;
	if (! opendir ($devdir, "$blockPath/$entry")) {
		Base::fatal ("can't open directory $blockPath/$entry");
	}
	while (defined (my $partition = readdir ($devdir))) {
		next if $partition eq ".";
		next if $partition eq "..";
		next unless -d "$blockPath/$entry/$partition";

		# there can be subdirectories in a blockdev
		# other than partitions; eg /sys/block/sda/queue.
		# a partition has major:minor, so has a dev file.
		next unless -f "$blockPath/$entry/$partition/dev";

		onePartition($blockPath, $entry, $partition, $descr);
	}
	if (! closedir ($devdir)) {
		Base::fatal ("could not read directory $blockPath/$entry");
	}
}

sub onePartition ($$$$) {
	my ($blockPath, $entry, $partition, $parent) = @_;
	my $dev = Base::getStringFile("$blockPath/$entry/$partition/dev");
	if (exists ($abdTab->{$dev})) {
		Base::fatal ("found duplicate devno in $blockPath/$entry/$partition");
	}
	$abdTab->{$dev} = ActiveBlockDev->new (
		name => $partition,
		devno => $dev,
		parent => $parent,
		hw => undef,
		);
}

#
# all -- return list of all known active block devices.
#
sub all	() {
	init;
	return $abdList;
}

#
# findByDevno -- given devno in format maj:min,
# return corresponding descriptor or undef.
#
sub findByDevno ($) {
	my ($devno) = @_;
	init;
	return $abdTab->{$devno};
}

#
# findByPath -- given path to block device file in /dev,
# return corresponding descriptor or undef.
#
sub findByPath ($) {
	my ($path) = @_;
	my $devno = Base::devno ($path);
	if (! defined ($devno)) {
		return undef;
	}
	return findByDevno ($devno);
}

1;
