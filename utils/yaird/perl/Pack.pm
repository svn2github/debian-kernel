#!perl -w
#
# Pack -- create an image and pack in suitable format.
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
# knownFormat -- true if it's a known format
# package -- given format, image descriptor and destination,
#     put the image in the destination in requested format.
#
# Supported formats:
#  -- directory, for debugging
#  -- cramfs, as used in Debian initrd
#  -- cpio, actually cpio.gz, as used in Fedora initramfs.
#
# This module takes care not to leave a partially completed destination
# on error.
#

use strict;
use warnings;
use File::Temp;
use Base;
use Image;

package Pack;

sub packDirectory ($$);
sub packCramfs ($$);
sub packCpio ($$);

my $packagers = {
	directory	=> \&packDirectory,
	cramfs		=> \&packCramfs,
	cpio		=> \&packCpio,
};

sub knownFormat ($) {
	return exists ($packagers->{$_[0]});
}

sub package ($$$) {
	my ($image, $format, $destination) = @_;
	Base::assert (knownFormat ($format));

	#
	# Temp based on destination,to increase likelyhood
	# of rename being atomic.
	#
	my $template = "$destination.XXXXXXXXXXXXXXXX";
	my $tempdir = File::Temp::tempdir ($template, CLEANUP => 1);
	$image->buildHere ($tempdir);

	$packagers->{$format}($tempdir, $destination);
}



sub packDirectory ($$) {
	my ($tempdir, $destination) = @_;

	if (! rename ($tempdir, $destination)) {
		Base::fatal ("could not move $tempdir to $destination");
	}
}



sub packCramfs ($$) {
	my ($tempdir, $destination) = @_;

	my $template = "$destination.XXXXXXXXXXXXXXXX";
	my ($fh, $tempfile) = File::Temp::tempfile ($template, UNLINK => 1);
	if (system ("/usr/sbin/mkcramfs -E '$tempdir' '$tempfile' > /dev/null") != 0) {
		Base::fatal ("mkcramfs $tempdir failed");
	}
	if (! rename ($tempfile, $destination)) {
		Base::fatal ("could not move $tempfile to $destination");
	}
}



sub packCpio ($$) {
	my ($tempdir, $destination) = @_;

	#
	# Note how the tempfile can be a relative path, and how cpio
	# depends on a chdir into tempdir.  Thus, cpio output tempfile
	# should *not* be based on a template.
	#

	my $template = "$destination.XXXXXXXXXXXXXXXX";
	my ($fh, $fromCpio) = File::Temp::tempfile (UNLINK => 1);
	my ($fh2, $fromGzip) = File::Temp::tempfile ($template, UNLINK => 1);

	#
	# There are multiple universal unique portable archive formats,
	# each with the same magic number.
	# cpio(1) mentions newc and odc; these differ in size of
	# fields of header.  Whether -oc generates newc or odc depends
	# on a define in cpio, with Fedora picking a different default
	# than Debian.  The kernel accepts only newc format (see initramfs.c).
	# Use the -H newc option to get the right format, regardless
	# of which default the distro chooses for -c.
	#
	if (system ("cd '$tempdir' && find . | cpio --quiet -o -H newc > $fromCpio")) {
		# $! is useless here: inappropriate ioctl for device ...
		Base::fatal ("cpio $tempdir failed");
	}
	if (system ("gzip -9 < $fromCpio > $fromGzip")) {
		Base::fatal ("gzip $tempdir failed");
	}

	if (! rename ($fromGzip, $destination)) {
		Base::fatal ("could not move $fromGzip to $destination");
	}
}

1;
