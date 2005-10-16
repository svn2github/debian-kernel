#!perl -w
#
# ActiveBlockDev -- a single blockdev, as found in /sys
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
# The blockdev can be a whole device or a partition.
# descriptors contain:
# - name, path as found in /sys (sda, sda/sda1)
# - devno, as found in /sys (8:1)
# - parent, undef for whole device, maj:minor of parent otherwise
# - hw, path to underlying hardware device,
# 		eg an ide controller somewhere on a PCI bus.
#		this is a path relative to /sys/devices,
#		and can be undef (eg for ram disk)
# - yspecial, a block special file giving access to the device
# 		in the generated initrd image.  This often needs
# 		a mknod on the generated image.  This must be aware
# 		of the fact that device-mapper based devices
# 		(LVM, cryptsetup) must be in /dev/mapper.
# - partitions, list of partitions contained in the device.
#		these partitions are also ActiveBlockDevs.
#
# NOTE: the partition list relies on ActiveBlockDevTab making
# a complete scan of all block devices, each partition registering
# itself as partition with the parent device.
#
use strict;
use warnings;
use BlockSpecialFileTab;
package ActiveBlockDev;
use base 'Obj';


sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->takeArgs ('name', 'devno', 'parent', 'hw');
	$self->{partitions} = [];
	if (defined ($self->parent)) {
		push @{$self->parent->partitions}, $self;
	}
}

sub name	{ return $_[0]->{name}; }
sub devno	{ return $_[0]->{devno}; }
sub parent	{ return $_[0]->{parent}; }
sub hw		{ return $_[0]->{hw}; }
sub partitions	{ return $_[0]->{partitions}; }

sub string {
	my $self = shift;
	my $name = $self->name;
	my $devno = $self->devno;
	my $parent = (defined($self->parent) ? $self->parent->name : "--");
	my $hw = ($self->hw or "--");
	my $yspecial = ($self->yspecial or "--");
	return "$name($devno) in $parent at $hw becomes $yspecial";
}

sub yspecial {
	my $self = shift;
	my $name = $self->name;
	my $devno = $self->devno;
	my $yspecial = "/dev/$name";
	if ($name =~ /^dm-\d+$/) {
		my $paths = BlockSpecialFileTab::pathsByDevno ($devno);
		if (! defined ($paths)) {
			Base::fatal ("No block special file for $name in /dev");
		}
		my $match = undef;
		for my $p (@{$paths}) {
			if ($p =~ m!^/dev/mapper/([^/]*)$!) {
				if (defined ($match)) {
					Base::fatal ("Don't know how to choose between $match and $p for $name");
				}
				$match = $p;
			}
		}
		if (! defined ($match)) {
			Base::fatal ("No block special file for $name in /dev/mapper");
		}
		$yspecial = $match;
	}
	return $yspecial;
}

sub hasPartitions {
	my $self = shift;
	return ($#{$self->partitions} >= 0);
}

1;
