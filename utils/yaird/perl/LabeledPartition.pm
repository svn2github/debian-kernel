#!perl -w
#
# LabeledPartition -- a partition with ext2 or other label.
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
# descriptors contain:
# - label - string such as volume name for matching in /etc/fstab
# - uuid - similar but hex
# - path - the block special file where the label was found
# - type - what kind of superblock the label was found in.
#
# NOTE: consider delegating this whole mess to findfs from the
# e2fsprogs package.  That package can probe for an extensive
# set of superblocks, knows about LVM, but does not make fs type
# information available via the command line.
#
use strict;
use warnings;
package LabeledPartition;
use base 'Obj';


sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->takeArgs ('label', 'uuid', 'path', 'type');
}

sub label	{ return $_[0]->{label}; }
sub uuid	{ return $_[0]->{uuid}; }
sub type	{ return $_[0]->{type}; }
sub path	{ return $_[0]->{path}; }

sub string {
	my $self = shift;
	my $label = ($self->label or '-');
	my $uuid = ($self->uuid or '-');
	my $type = $self->type;
	my $path = $self->path;
	return "labeled($type) $label, $uuid at $path";
}


#
# try -- return descriptor for labeled fs, or undef,
# trying a number of formats for superblock.
#
sub try ($) {
	my ($path) = @_;
	my $result = undef;
	my $fh;
	if (! open ($fh, "<", "$path")) {
		return $result;
	}
	binmode ($fh);
	$result = tryExt2 ($fh, $path) unless defined $result;
	$result = tryReiser ($fh, $path) unless defined $result;

	# ignore errors, could not have been very wrong
	# if a working superblock came out of it.
	close ($fh);
	return $result;
}


#
# lets hope the sysadm labeled it in ascii not utf8.
# based on util-linux-2.12h mount; they have lots of other
# superblocks to match.  Later.
#
sub tryExt2 ($$) {
	my ($fh, $path) = @_;
	my $result = undef;

	if (! seek ($fh, 1024, 0)) {
		return $result;
	}
	# undef on read error - is this worth a warning?
	my $superBlock;
	my $rc = read ($fh, $superBlock, 1024);
	if (! defined ($rc) || $rc != 1024) {
		return $result;
	}
	my ($magic, $uuid, $label) =
		unpack
		"x[56] v x[34] x[VVV] H[32] Z[16]",
		$superBlock;

	if ($magic == 0xEF53) {
		$uuid = fancyUuid ($uuid);
		$result = LabeledPartition->new (
			type => "ext2",
			label => $label,
			uuid => $uuid,
			path => $path,
			);
	}
	return $result;
}

# fancyUuid - given a hex uuid, add hyphens at conventional place.
sub fancyUuid ($) {
	my ($uuid) = @_;
	$uuid =~ s/^(.{8})(.{4})(.{4})(.{4})(.{12})$/$1-$2-$3-$4-$5/;
	return lc ($uuid);
}

sub tryReiser ($$) {
	my ($fh, $path) = @_;
	my $result = undef;
	# offset in reisser 3.6 (3.5-3.5.10 had 8k)
	if (! seek ($fh, (64*1024), 0)) {
		return $result;
	}
	# undef on read error - is this worth a warning?
	my $superBlock;
	my $rc = read ($fh, $superBlock, 1024);
	if (! defined ($rc) || $rc != 1024) {
		return $result;
	}
	my ($magic, $uuid, $label) =
		unpack
		"x[52] Z[10] x[22] a[16] Z[16]",
		$superBlock;
	my $t = join ("", map { sprintf "%02x", ord($_) } split(//, $uuid));
	if ($magic eq "ReIsEr2Fs" || $magic eq "ReIsEr3Fs") {
		$uuid = fancyUuid ($uuid);
		$result = LabeledPartition->new (
			type => "reiserfs",
			label => $label,
			uuid => fancyUuid($t),
			path => $path,
			);
	}
	return $result;
}

1;
