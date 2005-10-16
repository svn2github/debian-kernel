#!perl -w
#
# LvmTab -- encapsulate LVM output
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
# The trick here is to find all block devices that back a physical
# volume that is part of a volume group, where in that volume group
# there is a logical volume with maj:min that contains a filesystem
# to be mounted at boot time.  In particular, if there are two
# volume groups, one for OS and one for database, you don't want
# the modules for the database drives on your initrd image.
#
# If no LVM executables are found on init, the internal maps are
# set to empty (as opposed to undefined).  No error is generated.
#
# NOTE: There seem to be newer reporting tools (vgs, lvs, pvs)
# that produce more flexible reports for easier grepping than with
# the commands used here.
#
use strict;
use warnings;
use Base;
use LogicalVolume;
use VolumeGroup;
use PhysicalVolume;
package LvmTab;


my $lvMap = undef;
my $vgMap = undef;
my $pvMap = undef;
my $lvList = undef;
my $vgList = undef;
my $pvList = undef;

sub init () {
	if (defined ($lvMap)) {
		return;
	}
	initLvMap ();
	initVgMap ();
	initPvMap ();
	$lvList = [ values %{$lvMap} ];
	$vgList = [ values %{$vgMap} ];
	$pvList = [ values %{$pvMap} ];
}

sub allLogicalVolumes () {
	init;
	return $lvList;
}

sub allVolumeGroups () {
	init;
	return $vgList;
}

sub allPhysicalVolumes () {
	init;
	return $pvList;
}

sub findLVByDevno ($) {
	init;
	return $lvMap->{$_[0]};
}

sub findVGByVgnam ($) {
	init;
	return $vgMap->{$_[0]};
}

sub findPVsByVgnam ($) {
	my ($vgnam) = @_;
	my $result = [];
	for my $pv (@{allPhysicalVolumes()}) {
		if ($pv->vgnam() eq $vgnam) {
			push @{$result}, $pv;
		}
	}
	return $result;
}

#
# findPhysicalVolumePathsByLVDevno -- return a list of special file names
# that make up the physical volumes underlying a logical volume
# identified by maj:min, or undef if the devno seems not to be
# implemented by lvm.
#
sub findPVPathsByLVDevno ($) {
	my ($devno) = @_;
	my $lv = LvmTab::findLVByDevno ($devno);
	if (! defined ($lv)) {
		return undef;
	}
	my $vgnam = $lv->vgnam();
	if (! defined (LvmTab::findVGByVgnam($vgnam))) {
		Base::fatal ("unknown LVM volume group $vgnam for Logical Volume $devno");
	}
	my $result = [];
	for my $pv (@{LvmTab::findPVsByVgnam ($vgnam)}) {
		push @{$result}, $pv->pvnam();
	}
	return $result;
}


# build map from devno to logical volume descriptor,
# built on lvdisplay.
#
# /dev/vg0/root:vg0:3:1:-1:1:2097152:256:-1:0:0:254:0
#
sub initLvMap () {
	$lvMap = {};
	if (! open (IN, "-|", "lvdisplay -c 2> /dev/null")) {
		Base::debug ("can't run lvdisplay");
		return;
	}
	while (defined (my $line = <IN>)) {
		chomp $line;
		$line =~ s/^\s*//;
		my @fields = split (/:/, $line, 500);
		if ($#fields != 12) {
			Base::fatal ("malformed output from lvdisplay");
		}
		my $lvnam = $fields[0];
		my $vgnam = $fields[1];
		# Hide this, since lvdisplay output is inconsistent with docs.
		my $lvsiz = $fields[6];
		my $major = $fields[11];
		my $minor = $fields[12];
		if ($major !~ /^\d+$/) {
			Base::fatal ("malformed output (major) from lvdisplay");
		}
		if ($minor !~ /^\d+$/) {
			Base::fatal ("malformed output (minor) from lvdisplay");
		}
		my $devno = "$major:$minor";
		if (exists ($lvMap->{$devno})) {
			Base::fatal ("duplicate output ($devno) from lvdisplay");
		}
		$lvMap->{$devno} = LogicalVolume->new (
			lvnam => $lvnam,
			vgnam => $vgnam,
			devno => $devno,
			);
	}
	if (! close (IN)) {
		Base::fatal ("error running lvdisplay");
	}
}

# build map from vgnam to volume group descriptor,
# built on vgdisplay.
#
#  vg0:r/w:772:-1:0:7:6:-1:0:1:1:77799424:4096: \
#  	18994:6912:12082:MO1svc-uvVC-TFCL-qvrB-29wD-bh9K-V2e3U2
#
sub initVgMap () {
	$vgMap = {};
	if (! open (IN, "-|", "vgdisplay -c 2> /dev/null")) {
		Base::debug ("can't run vgdisplay");
		return;
	}
	while (defined (my $line = <IN>)) {
		chomp $line;
		$line =~ s/^\s*//;
		my @fields = split (/:/, $line, 500);
		if ($#fields != 16) {
			Base::fatal ("malformed output from vgdisplay");
		}
		my ($vgnam, $vgacc, $vgstat,
			$vgnum, $maxlv, $curlv, $openlv,
			$maxlvsiz, $maxpvcnt, $curpvcnt, $actpvcnt,
			$vgsiz, $extsiz, $extcnt, $extalloc, $extfree,
			$uuid)
			= @fields;

		if (exists ($vgMap->{$vgnam})) {
			Base::fatal ("duplicate output ($vgnam) from vgdisplay");
		}
		$vgMap->{$vgnam} = VolumeGroup->new (
			vgnam => $vgnam,
			uuid => $uuid,
			);
	}
	if (! close (IN)) {
		Base::fatal ("error running vgdisplay");
	}
}

# build map from pvnam to physical volume descriptor,
# built on pvdisplay.
#
#  /dev/sda3:vg0:155598848:-1:8:8:-1:4096:18994: \
#  	12082:6912:X5hDer-dYpy-jpAB-IhXQ-44j4-kyj0-cOQkyE
#
sub initPvMap () {
	$pvMap = {};
	if (! open (IN, "-|", "pvdisplay -c 2> /dev/null")) {
		Base::debug ("can't run pvdisplay");
		return;
	}
	while (defined (my $line = <IN>)) {
		chomp $line;
		$line =~ s/^\s*//;
		my @fields = split (/:/, $line, 500);
		if ($#fields != 11) {
			Base::fatal ("malformed output from pvdisplay");
		}
		my ($pvnam,$vgnam,$pvsiz,$pvnum,$pvstat,
			$pvalloc,$lvcnt,$extsiz,$extcnt,
			$extfree,$extalloc,$uuid)
			= @fields;

		if (exists ($pvMap->{$pvnam})) {
			Base::fatal ("duplicate output ($pvnam) from vgdisplay");
		}
		$pvMap->{$pvnam} = PhysicalVolume->new (
			pvnam => $pvnam,
			vgnam => $vgnam,
			uuid => $uuid,
			);
	}
	if (! close (IN)) {
		Base::fatal ("error running pvdisplay");
	}
}

1;


