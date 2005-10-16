#!perl -w
#
# KConfig -- encapsulate kernel configuration file and builtin modules
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
#
# KConfig is a mapping from kernel define symbol to value.
# Common values are 'y' (compiled in), 'm' modular.
# The "no" choice in kernel configuration is represented by absence of the
# symbol.  Other values (for example decimal or hex numbers) also occur.
#
# We also maintain a list of known modules and corresponding kernel define.
# If the define is 'y', we need not load the corresponding module.
# This is not relevant to hardware (if a PCI controller is builtin,
# it will not occur in modules.pcimap, so no loading is attempted),
# but it does help with filesystems, where an 'ext3' line in /etc/fstab
# means the ext3 module needs to be loaded unless its compiled in.
#

use strict;
use warnings;
use Base;
use Conf;
package KConfig;

my $kConfMap = undef;
my $kConfList = undef;

sub init () {
	if (defined ($kConfMap)) {
		return;
	}
	$kConfMap = {};
	my $name = Conf::get('kernConf');
	if (! open (IN, "<", "$name")) {
		Base::fatal ("can't open kernel config file $name");
	}
	while (defined (my $line = <IN>)) {
		chomp $line;
		$line =~ s/^\s+//;
		$line =~ s/\s+$//;
		$line =~ s/#.*//;
		next if ($line eq "");

		if ($line !~ /^([A-Z][A-Za-z0-9_]+)=(.+)$/) {
			# lowercase is uncommon, but: CONFIG_SCx200=m
			Base::fatal "bad line in $name: $line";
		}
		my $key = $1;
		my $value = $2;
		if ($value eq 'y'
			|| $value eq 'm'
			|| $value =~ /^-?\d+$/
			|| $value =~ /^0x[0-9a-f]+$/
			|| $value =~ /^"[-a-zA-Z0-9,._\/]*"$/
		) {
			$kConfMap->{$key} = $value;
		}
		else {
			Base::fatal "bad value in $name: $line";
		}
	}
	if (! close (IN)) {
		Base::fatal "could not read kernel config file $name";
	}
	$kConfList = [ sort keys %{$kConfMap} ];
}

#
# Map module name to kernel define.  Module names here
# must use hyphen, not underscore.
#
my $moduleMap = {
	# user interface devices
	fbcon => 'FRAMEBUFFER_CONSOLE',
	vesafb => 'FB_VESA',
	serio => 'SERIO',
	i8042 => 'SERIO_I8042',
	usbhid => 'USB_HID',
	atkbd => 'KEYBOARD_ATKBD',
	mousedev => 'INPUT_MOUSEDEV',
	evdev => 'INPUT_EVDEV',
	psmouse => 'MOUSE_PS2',

	# file systems
	ext2 => 'EXT2_FS',
	ext3 => 'EXT3_FS',
	jbd => 'JBD',
	reiserfs => 'REISERFS_FS',
	jfs => 'JFS_FS',
	xfs => 'XFS_FS',
	minix => 'MINIX_FS',
	romfs => 'ROMFS_FS',
	isofs => 'ISO9660_FS',
	udf => 'UDF_FS',
	fat => 'FAT_FS',
	msdos => 'MSDOS_FS',
	vfat => 'VFAT_FS',
	# broken, and nonmodular: umsdos => 'UMSDOS_FS',
	ntfs => 'NTFS_FS',
	adfs => 'ADFS_FS',
	affs => 'AFFS_FS',
	hfs => 'HFS_FS',
	hfsplus => 'HFSPLUS_FS',
	befs => 'BEFS_FS',
	bfs => 'BFS_FS',
	efs => 'EFS_FS',
	jffs => 'JFFS_FS',
	jffs2 => 'JFFS2_FS',
	cramfs => 'CRAMFS',
	freevxfs => 'VXFS_FS',
	hpfs => 'HPFS_FS',
	qnx4 => 'QNX4FS_FS',
	sysv => 'SYSV_FS',
	ufs => 'UFS_FS',
	nfs => 'NFS_FS',
	smbfs => 'SMB_FS',
	cifs => 'CIFS',
	ncpfs => 'NCP_FS',
	coda => 'CODA_FS',
	kafs => 'AFS_FS',

	# network
	'af-packet' => 'PACKET',

	# device mapper: raid and lvm.
	linear => 'MD_LINEAR',
	raid0 => 'MD_RAID0',
	raid1 => 'MD_RAID1',
	raid10 => 'MD_RAID10',
	raid5 => 'MD_RAID5',
	raid6 => 'MD_RAID6',
	multipath => 'MD_MULTIPATH',
	faulty => 'MD_FAULTY',
	md => 'BLK_DEV_MD',
	'dm-mod' => 'BLK_DEV_DM',
	'dm-crypt' => 'DM_CRYPT',
	'dm-snapshot' => 'DM_SNAPSHOT',
	'dm-mirror' => 'DM_MIRROR',
	'dm-zero' => 'DM_ZERO',

	# crypto
	hmac => 'CRYPTO_HMAC',
	'crypto-null' => 'CRYPTO_NULL',
	md4 => 'CRYPTO_MD4',
	md5 => 'CRYPTO_MD5',
	sha1 => 'CRYPTO_SHA1',
	sha256 => 'CRYPTO_SHA256',
	sha512 => 'CRYPTO_SHA512',
	wp512 => 'CRYPTO_WP512',
	des => 'CRYPTO_DES',
	blowfish => 'CRYPTO_BLOWFISH',
	twofish => 'CRYPTO_TWOFISH',
	serpent => 'CRYPTO_SERPENT',
	aes => 'CRYPTO_AES',
	cast5 => 'CRYPTO_CAST5',
	cast6 => 'CRYPTO_CAST6',
	arc4 => 'CRYPTO_ARC4',
	tea => 'CRYPTO_TEA',
	khazad => 'CRYPTO_KHAZAD',
	anubis => 'CRYPTO_ANUBIS',
	deflate => 'CRYPTO_DEFLATE',
	'michael-mic' => 'CRYPTO_MICHAEL_MIC',
	crc32c => 'CRYPTO_CRC32C',
	tcrypt => 'CRYPTO_TEST',

	# IDE
	'ide-generic' => 'IDE_GENERIC',
	'ide-disk' => 'BLK_DEV_IDEDISK',
	'ide-cd' => 'BLK_DEV_IDECD',
	'ide-tape' => 'BLK_DEV_IDETAPE',
	'ide-floppy' => 'BLK_DEV_IDEFLOPPY',

	# SCSI
	'sd-mod' => 'BLK_DEV_SD',
	'st' => 'CHR_DEV_ST',
	'sr-mod' => 'BLK_DEV_SR',
	'sg' => 'CHR_DEV_SG',
};


#
# all -- return a list of all known configuration defines
#
sub all	() {
	init;
	return $kConfList;
}

#
# allKnownModules -- return list of all module names for
# which a corresponding kernel define is known.
#
sub allKnownModules () {
	init;
	return [ sort keys %{$moduleMap} ];
}

#
# isBuiltIn -- true if the module is known to be compiled
# into the kernel.
#
sub isBuiltIn ($) {
	my ($module) = @_;
	init;
	$module =~ s!_!-!g;
	my $confKey = $moduleMap->{$module};
	if (! defined ($confKey)) {
		return 0;
	}
	my $confVal = $kConfMap->{"CONFIG_$confKey"};
	if (! defined ($confVal)) {
		return 0;
	}
	return ($confVal eq 'y');
}

#
# isOmitted -- true if the module is not part of the kernel:
# neither compiled in nor module.
#
sub isOmitted ($) {
	my ($module) = @_;
	init;
	$module =~ s!_!-!g;
	my $confKey = $moduleMap->{$module};
	if (! defined ($confKey)) {
		# if we don't know what kernel define corresponds
		# to the module name, we can't know whether it's
		# compiled in.  Punt.
		Base::bug ("broken isOmitted check for $module");
	}
	my $confVal = $kConfMap->{"CONFIG_$confKey"};
	return (! defined ($confVal));
}

1;

