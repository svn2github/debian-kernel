#!perl -w
#
# FsEntry -- encapsulate a single entry in /etc/fstab
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
use LabeledPartitionTab;
package FsEntry;
use base 'Obj';

sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->takeArgs ('dev', 'mnt', 'type', 'opts', 'origin');
}

sub dev		{ return $_[0]->{dev}; }
sub mnt		{ return $_[0]->{mnt}; }
sub type	{ return $_[0]->{type}; }
sub opts	{ return $_[0]->{opts}; }
sub origin	{ return $_[0]->{origin}; }

sub string {
	my $self = shift;
	my $dev = $self->dev();
	my $mnt = $self->mnt();
	my $type = $self->type();
	my $opts = $self->opts()->string();
	my $origin = $self->origin();
	return "$origin: $mnt at $dev ($type) with $opts";
}


#
# isRequiredAtBoot -- To be mounted at boot time with mount -a.
# This does not imply the initrd should already mount it.
#
sub isRequiredAtBoot {
	my $self = shift;
	return 0 if ($self->type eq "ignore");
	return 0 if ($self->opts->exists("noauto"));
	return 1;
}


#
# blockDevPath -- return (blockdev pathname, undef) for an fstab entry.
# must resolve LABEL= and UUID=.  Note that LVM and raid devices
# may be implemented on top of multiple other devices; this returns
# only the top device.
#
# Returns (undef, message) if first field of fstab entry does not point to
# an existing block device.  Possible reasons:
# - filesystem does not have underlying device, such as /proc.
# - filesystem has a non-file underlying resource, such as NFS
# - garbage line with missing blockdev
# - filesystem has a plain file as underlying resource;
#   this can be a loopback file system, or swap to plain file.
#
# NOTE: We should support the last case, but for now that's too difficult:
# imagine /a/b/c, where /a is reiserfs, b is a symlink to /d (an NFS
# filesystem), and /d/c is a symlink to /e/f, where /e is a ramdisk.)
#
sub blockDevPath {
	my $self = shift;
	my $dev = $self->dev();
	my $origin = $self->origin();
	my $msg = undef;

	if ($dev =~  /^\//) {
		if ($self->opts->exists('loop')) {
			$msg = "loopback mount for '$dev' not supported ($origin)";
			return (undef, $msg);
		}
		if (-f $dev && $self->type eq "swap") {
			$msg = "swap to regular file for '$dev' not supported ($origin)";
			return (undef, $msg);
		}

		if (! -e $dev) {
			$msg = "'$dev' not found ($origin)";
			return (undef, $msg);
		}

		if (! -b $dev) {
			$msg = "'$dev' must be a block device ($origin)";
			return (undef, $msg);
		}

		return ($dev, undef);
	}
	elsif ($dev =~ /^LABEL=(.*)/) {
		my $path = LabeledPartitionTab::findPathByLabel ($1);
		if (defined ($path)) {
			return ($path, undef);
		}
		$msg = "label '$1' not found ($origin)";
		return (undef, $msg);
	}
	elsif ($dev =~ /^UUID=(.*)/) {
		my $path = LabeledPartitionTab::findPathByUuid ($1);
		if (defined ($path)) {
			return ($path, undef);
		}
		$msg = "uuid '$1' not found ($origin)";
		return (undef, $msg);
	}
	$msg = "bad device name '$dev' ($origin)";
	return (undef, $msg);
}


1;


