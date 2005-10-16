#!perl -w
#
# LabeledPartitionTab -- all partitions with a label or uuid.
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
# Found by looking at partitions of block devices in /sys that
# - have hardware backing them
# - are accessible via a block special file in /dev.
# - like mount(8), scan in two passes: raid devices first,
#   ignore known uuids in second scan on underlying devices.
#
#

use strict;
use warnings;
use Base;
use BlockSpecialFileTab;
use LabeledPartition;
package LabeledPartitionTab;

my $lpTab = undef;
my $lpList = [];


sub init () {
	if (defined ($lpTab)) {
		return;
	}

	$lpTab = {};
	for my $pass (0, 1) {
		for my $abd (@{ActiveBlockDevTab::all()}) {
			my $name = $abd->name();
			my $devno = $abd->devno();
			my $paths = BlockSpecialFileTab::pathsByDevno($devno);
			if (! defined ($paths)) {
				# must be accessible via block special file
				next;
			}
			next if $abd->hasPartitions();

			# let's not look for labels on floppy,
			# tends to hang.
			# NOTE: perhaps not look for labels on any
			# removable device, as marked by /sys?
			next if ($name =~ /^fd\d+$/);

			if (($pass == 0) == ($name =~ /^md\d+$/)) {
				# all paths give access to the same device
				# pick first one to actually open it.
				my $special = $paths->[0];
				my $descr = LabeledPartition::try ($special);
				if (defined ($descr)) {
					#
					# Using a partially completed
					# data structure.  In first pass,
					# the list empty, so nothing found.
					# In second pass, we look at non-raid
					# devices, and only raid devices
					# are in the list.
					# The tricky part is that the list
					# used to find existing devices
					# is updated only at the end of the
					# pass, so that different devices
					# with same uuid are entered in the
					# table and will cause a proper error
					# message if the uuid is later used
					# to find a device.
					#
					if (! findByUuid ($descr->uuid)) {
						my $devno = $abd->devno;
						$lpTab->{$devno} = $descr;
					}
				}
			}
		}
		$lpList = [ values %{$lpTab} ];
	}

}

sub all	() {
	init;
	return $lpList;
}

sub findByLabel ($) {
	my ($label) = @_;
	my $result = undef;
	for my $lp (@{LabeledPartitionTab::all()}) {
		if ($lp->label() eq $label) {
			if (defined ($result)) {
				Base::fatal ("duplicate file system label $label");
			}
			$result = $lp;
		}
	}
	return $result;
}

sub findPathByLabel ($) {
	my ($label) = @_;
	my $lp = findByLabel ($label);
	return $lp->path() if $lp;
	return undef;
}

sub findByUuid ($) {
	my ($uuid) = @_;
	my $result = undef;
	$uuid = lc ($uuid);
	for my $lp (@{LabeledPartitionTab::all()}) {
		if ($lp->uuid() eq $uuid) {
			if (defined ($result)) {
				Base::fatal ("duplicate file system uuid $uuid");
			}
			$result = $lp;
		}
	}
	return $result;
}

sub findPathByUuid ($) {
	my ($uuid) = @_;
	my $lp = findByUuid ($uuid);
	return $lp->path() if $lp;
	return undef;
}

1;

