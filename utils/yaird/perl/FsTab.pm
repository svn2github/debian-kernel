#!perl -w
#
# FsTab -- encapsulate /etc/fstab.
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
use FsOpts;
use FsEntry;
package FsTab;


my $fsTab = undef;


sub init () {
	if (defined ($fsTab)) {
		return;
	}
	$fsTab = [];
	my $name = Conf::get('fstab');
	if (! open (IN, "<", "$name")) {
		Base::fatal ("can't read $name");
	}
	my $lineNo = 0;
	while (defined (my $line = <IN>)) {
		$lineNo++;
		$line =~ s/^\s+//;
		next if $line =~ /^#/;	# comment line
		next if $line eq "";	# empty line
		my @fields = split (/\s+/, $line, 999);
		if ($#fields < 6) {
			# no test for extra fields;
			# the mount command allows that.
			Base::fatal ("malformed line in $name:$lineNo");
		}
		if ($fields[2] eq "swap") {
			if (($fields[1] ne "none") &&($fields[1] ne "swap")) {
				Base::fatal ("bad swap entry in $name:$lineNo");
			}
		}
		
		my $opts = FsOpts->new (
			string => unmangle($fields[3]));

		#
		# Canon makes sure that mountpoint /usr is same as /usr/.
		# For UUID= devices, it should be harmless.
		#
		my $descr = FsEntry->new(
			dev => Base::canon (unmangle($fields[0])),
			mnt => Base::canon (unmangle($fields[1])),
			type => unmangle($fields[2]),
			opts => $opts,
			origin => "$name:$lineNo",
			);
		push @{$fsTab}, $descr;
	}
	if (! close (IN)) {
		Base::fatal ("could not read $name");
	}
}

sub all	() {
	init;
	return $fsTab;
}

sub findByMountPoint ($) {
	my ($mnt) = @_;
	my $result;

	$mnt = Base::canon ($mnt);
	for my $entry (@{FsTab::all()}) {
		if ($entry->mnt() eq $mnt) {
			if (defined ($result)) {
				Base::fatal ("duplicate mount point in fstab $mnt");
			}
			$result = $entry;
		}
	}
	if (! defined ($result)) {
		Base::fatal ("mount point not in fstab: $mnt");
	}
	return $result;
}

#
# findByDevName -- given name of a block device, find fstab entry.
# Note:
# - some devices may not have blockdev, eg nfs devices
# - some may be specified as LABEL=/usr
# - aliases can occur: eg /dev/root and /dev/hda1 have same device no.
#
sub findByDevName ($) {
	my ($dev) = @_;
	my $result;
	my $devno = Base::devno ($dev);
	if (! defined ($devno)) {
		Base::fatal ("cannot find device number for: $dev");
	}

	for my $entry (@{FsTab::all()}) {
		my ($b2, $msg) = $entry->blockDevPath();
		if (! defined ($b2)) {
			next;
		}
		my $n2 = Base::devno ($b2);
		if (! defined ($n2)) {
			next;
		}

		if ($n2 eq $devno) {
			if (defined ($result)) {
				Base::fatal ("duplicate device name in fstab: $dev");
			}
			$result = $entry;
		}
	}
	if (! defined ($result)) {
		Base::fatal ("device name not in fstab: $dev");
	}
	return $result;
}

# replace octal escapes
sub unmangle ($) {
	my ($string) = @_;
	while ($string =~ /\\([0-7]{3})/) {
		$string = $` . chr (oct ($1)) . $';
	}
	return $string;
}

1;
