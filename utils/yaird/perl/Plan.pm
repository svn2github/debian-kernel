#!perl -w
#
# Plan -- high-level stuff
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
#
use strict;
use warnings;
use Base;
use FsTab;
use ActiveBlockDevTab;
use LvmTab;
use Hardware;
use ModProbe;
use RaidTab;
use Image;
use ActionList;
use CryptTab;
use NetDevTab;

package Plan;



#
# addHardware -- given hardware path, potentially undef,
# add required actions to the action list.
#
sub addHardware ($$) {
	my ($actions, $hardware) = @_;
	if (defined ($hardware)) {
		my $modList = Hardware::moduleList ($hardware);
		ModProbe::addModules ($actions, $modList);
		Base::info ("hardware: completed $hardware");
	}
}


#
# addDevicePlan -- given an active block device, add to actions
# a sequence to make it into a working device file, suitable for mounting.
# There is a possibility of loops here (consider an fstab
# where /a is a loopback mount of /a/disk.img); we want do
# detect these loops and produce an error.  Also, we want to
# avoid double work: if five lvm logical volumes use the same
# underlying raid device, there's not much point in running
# mdadm five times.
# There are some contraints on the name of the device file;
# see yspecial in ActiveBlockDev.
#
# The actual methods to make a device available are a bag of tricks
# that we try in order until we find one that works.  Since different
# methods may be needed for similar devices (device mapper in particular)
# we cannot use a switch to directly select the appropriate method.
#
# actions - add actions here
# device - the active block device to be made available.
# working - list of devices we're working on, excluding path.
#
sub addDevicePlan ($$$) {
	my ($actions, $device, $working) = @_;

	my $name = $device->name;
	for my $w (@{$working}) {
		if ($w eq $device) {
			Base::fatal ("loop detected: $name\n");
		}
	}

	my $ok = 0;
	$ok || ($ok = tryParent ($actions,$device,[$device,@{$working}]));
	$ok || ($ok = tryDmCrypt ($actions,$device,[$device,@{$working}]));
	$ok || ($ok = tryLvm ($actions,$device,[$device,@{$working}]));
	$ok || ($ok = tryRaid ($actions,$device,[$device,@{$working}]));
	$ok || ($ok = tryHardware ($actions,$device,[$device,@{$working}]));
	if (! $ok) {
		Base::fatal ("unsupported device required: $name");
	}
	Base::debug ("device: completed $name");
}

#
# tryParent -- If it's a partition, do the whole
# device, then make a special file for the partition.
#
sub tryParent ($$$) {
	my ($actions, $device, $working) = @_;

	my $parent = $device->parent;
	if (! defined ($parent)) {
		return 0;
	}

	addDevicePlan($actions,$parent,$working);
	my $name = $device->name;
	my $pname = $parent->name;
	$actions->add ("mkbdev", $device->yspecial,
			sysname => "$pname/$name");
	return 1;
}

#
# tryDmCrypt -- if the device is encrypted with dm-crypt,
# and /etc/crypttab tells us the appropriate hash etc,
# use cryptsetup to make it available.
# Supports both plain cryptsetup and cryptsetup-luks.
#
sub tryDmCrypt ($$$) {
	my ($actions, $device, $working) = @_;

	my $name = $device->name;
	my $devno = $device->devno;

	if ($name !~ /^dm-\d+$/) {
		return 0;
	}

	# cryptsetup builds on libdevmapper, which only
	# works with files in /dev/mapper/.
	my $target = undef;
	for my $p (@{BlockSpecialFileTab::pathsByDevno($devno)}) {
		if ($p =~ m!^/dev/mapper/([^/]*)$!) {
			$target = $1;
		}
	}
	if (! defined ($target)) {
		#
		# We have a dm device that is not in /dev/mapper.
		# This is sufficiently odd to merit a fatal().
		# The ugly part is that this errror really does not
		# relate to dm-crypt; OTOH we can generate a better
		# error message here than by simply failing all matches.
		#
		Base::fatal ("device '$name' relies on device mapper, but no corresponding block special file is in /dev/mapper.");
	}

	# Check this device is in crypttab
	my $cryptEntry = CryptTab::findByDst ($target);
	if (!defined ($cryptEntry)) {
		return 0;
	}

	#
	# Device is in crypttab, so cryptsetup should work.
	# From this point on, errors are fatal.
	#

	# Check consistency of configuration

	if ($cryptEntry->keyFile) {
		my $origin = $cryptEntry->origin;
		Base::fatal ("encrypted device '$target' has keyfile specified in $origin.  This is not supported.");
	}

	if ($cryptEntry->opts->exists('swap')) {
		# not thought about what this needs yet.
		my $origin = $cryptEntry->origin;
		Base::fatal ("Can't handle 'swap' option for $target at $origin");
	}

	#
	# Hmm, what if the encrypted device is readonly? 
	# - dont generate initrd with readonly device:
	#   it's not likely to boot
	# - dont ignore configuration directives:
	#   if crypttab says its readonly, we're not
	#   going to set it up r/w.
	# - so if config says readonly, fatal error:
	#   we can't reliably produce a safe image.
	# - as a separate issue, if the device is currently
	#   setup readonly, the initrd can make it r/w as
	#   the config file says it must.  This situation
	#   is not worth a warning.
	#
	if ($cryptEntry->opts->exists('readonly')) {
		my $origin = $cryptEntry->origin;
		Base::fatal ("encrypted device '$target' is marked readonly in $origin.  This is not supported.");
	}

	#
	# get info about dmcrypt device
	#
	if (! open (IN, "-|", "/sbin/cryptsetup status $target")) {
		Base::fatal ("can't run cryptsetup status $target");
	}
	my %dmstatus;
	while (defined (my $line = <IN>)) {
		chomp $line;
		if ($line =~ m!^/dev/mapper/.* is active:!) {
			next;
		}
		if ($line =~ /^\s*([^:]+):\s+(.+)$/) {
			$dmstatus{$1} = $2;
		}
		# ignore garbage.
	}
	if (! close (IN)) {
		Base::fatal ("error running cryptsetup status $target");
	}

	for my $key ('cipher','keysize','device','offset','size','mode') {
		if (! defined ($dmstatus{$key})) {
			Base::fatal ("missing '$key' record in output for cryptsetup status '$target'");
		}
	}

	#
	# Verify that active device matches configuration file.
	#

	my $cipher = $dmstatus{cipher};
	my $confCipher = $cryptEntry->opts->get('cipher');
	if (defined ($confCipher) && $cipher !~ /$confCipher(-.*)?/) {
		# NOTE: this means 'aes' in /etc/crypttab
		# matches aes-cbc-plain, but also with other
		# minor modes such as aes-cbc-essiv:sha256.
		# For now, keeping track of all default modes
		# in kernel crypt modules is too complicated.
		my $origin = $cryptEntry->origin;
		Base::fatal ("cryptsetup shows active cipher ($cipher) for '$target' conflicts with $origin ($confCipher)");
	}

	my $keySize = $dmstatus{keysize};
	$keySize =~ s/ bits$//;
	my $confSize = $cryptEntry->opts->get('size');
	if (defined ($confSize) && ($keySize ne $confSize)) {
		my $origin = $cryptEntry->origin;
		Base::fatal ("cryptsetup shows active cipher size ($keySize) for '$target' conflicts with $origin ($confSize)");
	}

	my $src = Base::canon($dmstatus{device});
	my $confSrc = Base::canon($cryptEntry->src);
	if (Base::devno($src) ne Base::devno($confSrc)) {
		# NOTE: there may be different block special files
		# with the same devno, and cryptsetup an crypttab
		# need not use the same one.
		my $origin = $cryptEntry->origin;
		Base::fatal ("cryptsetup shows active backing device ($src) for '$target' conflicts with $origin ($confSrc)");
	}


	#
	# See if it is a luks device and we have a luks-enabled
	# cryptsetup installed.
	#
	my $ignoreThis = `/sbin/cryptsetup isLuks $confSrc 2> /dev/null`;
	my $isLuks = ($? == 0);

	if (! $isLuks) {
		# Non-zero offset is supported by cryptsetup,
		# but not crypttab.
		my $offset = $dmstatus{offset};
		if ($offset ne '0 sectors') {
			Base::fatal ("cryptsetup shows offset ($offset) for '$target' is non-zero.  This is unsupported.  Perhaps you need a cryptsetup with luks support?");
		}
	}

	my $abd = ActiveBlockDevTab::findByPath($src);
	if (!defined ($abd)) {
		my $origin = $cryptEntry->origin;
		Base::fatal ("Block device '$src' not found for $origin");
	}


	#
	# Time to start image generation.  At this point we have:
	# - target: the encrypted device to be generated
	# - isLuks: it's a luks device
	# - abd: descritor for the underlying device
	# - cipher: full name of the cipher
	# - keySize: size in bits of the cipher
	# - hash: the passphrase hash function
	# - verify: whether to ask passphrase twice
	#   doing verify for luks is silly but supported.
	#

	addDevicePlan($actions,$abd,$working);

	ModProbe::addModules ($actions, [ "dm-crypt" ]);

	$cipher =~ /^([^-]+)-/;
	ModProbe::addModules ($actions, [ $1 ]);
	if ($cipher =~ /:(.*)$/) {
		#
		# Some block cipher modes, notably aes-cbc-essiv:sha256,
		# need a hash in the kernel.
		# This differs from the hash applied to the passphrase.
		#
		ModProbe::addModules ($actions, [ $1 ]);
	}

	if ($isLuks) {
		$actions->add ("cryptsetup_luks", $target,
			src => $abd->yspecial,
			verify => ($cryptEntry->opts->exists('verify') ? 1 : 0),
			);
	}
	else {
		$actions->add ("cryptsetup", $target,
			src => $abd->yspecial,
			cipher => $cipher,
			keySize => $keySize,
			hash => $cryptEntry->opts->get('hash'),
			verify => ($cryptEntry->opts->exists('verify') ? 1 : 0),
			);
	}

	return 1;
}


#
# tryLvm -- to start an LVM device, make available the underlying
# physical volumes, then start the volume group.  Creating the block
# special file is a side effect of starting the volume group.
# The activeBlockDev->yspecial() method is choosen such that the name
# returned matches the name of the block special file generated by vgchange.
# Physical volumes are named by the block special file giving access to it.
#
sub tryLvm ($$$) {
	my ($actions, $device, $working) = @_;

	my $name = $device->name;
	my $devno = $device->devno;
	if ($name !~ /^dm-\d+$/) {
		return 0;
	}

	my $lv = LvmTab::findLVByDevno ($devno);
	if (! defined ($lv)) {
		# Base::fatal ("Can't find LVM info for $name");
		return 0;
	}
	my $vgnam = $lv->vgnam;
	for my $physVol (@{LvmTab::findPVsByVgnam($vgnam)}) 
	{
		my $pdev = ActiveBlockDevTab::findByPath ($physVol->pvnam);
		addDevicePlan ($actions, $pdev, $working);
	}
	ModProbe::addModules ($actions, [ "dm-mod" ]);
	$actions->add ("vgchange", $vgnam);
	return 1;
}


#
# tryRaid -- To start an md raid device, start the underlying hardware,
# load raid module, then do mdadm --assemble.
#
sub tryRaid ($$$) {
	my ($actions, $device, $working) = @_;

	my $name = $device->name;
	my $devno = $device->devno;
	if ($name !~ /^md\d+$/) {
		return 0;
	}

	my $rd = RaidTab::findByDevno ($devno);
	if (! defined ($rd)) {
		Base::fatal ("Can't find Raid info for $name");
	}
	my $components = [];
	for my $subDiskPath (@{$rd->devices()}) {
		my $subDisk = ActiveBlockDevTab::findByPath ($subDiskPath);
		addDevicePlan ($actions, $subDisk, $working);
		my $subName = $subDisk->yspecial;
		push @{$components}, {dev => $subName};
	}
	ModProbe::addModules ($actions, [ $rd->level ]);

	my $uuid = $rd->uuid;
	my ($major, $minor) = ($devno =~ /(\d+):(\d+)/);

	$actions->add ("mdadm", $device->yspecial,
		major => $major,
		minor => $minor,
		uuid => $uuid,
		components => $components,
		);
	return 1;
}


#
# tryHardware -- for devices that just want some modules loaded.
#
sub tryHardware ($$$) {
	my ($actions, $device, $working) = @_;

	my $name = $device->name;
	if (
		$name =~ /^hd[a-z]$/
             || $name =~ /^sd[a-z]$/
             || $name =~ /^fd\d+$/)
	{
		# IDE or SCSI.
		my $hardware = $device->hw;
		addHardware ($actions, $hardware);
		$actions->add("mkbdev", $device->yspecial, sysname => $name);
		return 1;
	}

	#
	# floppies before 2.6.12 or so did not have hardware link.
	# plan B: assume that a floppy is a floppy.
	#
	if ($name =~ /^fd\d+$/) {
		ModProbe::addModules ($actions, [ "floppy" ]);
		$actions->add("mkbdev", $device->yspecial, sysname => $name);
		return 1;
	}

	return 0;
}



#
# addInputPlan -- list of actions to activate console,
# for now only keyboard, no framebuffer.
#
# The legacy mouse is a bit, well, odd.
# you need mousedev to get the /dev/input/mice interface
# to the mouse, psmouse alone will only give you /dev/psaux,
# which is not good enough for X-Windows.
# FC4, 2.6.12: probing psmouse in multiuser *somehow* sucks
# in mousedev, but loading it in initramfs doesn't, probably
# because of some limitation in coldplugging.
# This hack belongs in a higher level config file, but we
# don't have that yet.
#
# The legacy kbd is odd in other ways.  The kbd handler
# does not show up in /sys/class/input/kbd (2.6.12),
# but if you load evdev, it will show up as /sys/class/unput/event\d,
# complete with hardware link.
#
# To cope with this, add 'mousedev' and 'evdev' after activating
# input devices.
#
sub addInputPlan ($) {
	my ($actions) = @_;
	for my $input (@{InputTab::all()}) {
		my $str = $input->string;
		Base::debug ("addInput: consider $str");

		if (! $input->isKbd()) {
			Base::debug ("addInput: skipping");
			next;
		}
		if (defined ($input->hw)) {
			addHardware ($actions, $input->hw);
			next;
		}

		#
		# fallback crap.  The hardware symlink is missing
		# for AT keyboards before 2.6.12 (11?) or if
		# module evdev is not loaded.
		#
		if ($input->name =~ /AT Translated Set 2 keyboard/) {
			ModProbe::addModules ($actions, [ "i8042", "atkbd" ]);
		}
	}
	Base::debug ("addInput: end");
}


#
# addNetworkPlan -- add list of actions to activate all
# network devices.  This is for nfs root mount,
# and since we can't predict routing at boot time,
# we just activate all devices.
#
sub addNetworkPlan ($) {
	my ($actions) = @_;
	for my $netDev (@{NetDevTab::all()}) {
		my $name = $netDev->name;
		Base::info ("network: starting $name");
		if (defined ($netDev->hw)) {
			addHardware ($actions, $netDev->hw);
		}
	}
}


#
# addBlockDevMount -- add list of actions to mount named device
# at mountPoint: activate device, activate fstype, do mount.
#
sub addBlockDevMount ($$$) {
	my ($actions, $rootDevName, $mountPoint) = @_;

	#
	# Device must be in fstab, to determine options
	#
	my $root = FsTab::findByDevName($rootDevName);
	if (! defined ($root)) {
		fatal ("requested root device ($rootDevName) not in fstab");
	}

	#
	# and device must be in /dev, to determine whether
	# it's raid, lvm, scsi or whatever.
	#
	my $abd = ActiveBlockDevTab::findByPath($rootDevName);
	if (! defined ($abd)) {
		my $origin = $root->origin;
		Base::fatal ("block device '$rootDevName' unavailable ($origin)");
	}

	addDevicePlan ($actions, $abd, []);

	my $fsType = $root->type;
	ModProbe::addModules ($actions, [ $fsType ]);

	my $yspecial = $abd->yspecial();
	my $opts = $root->opts->cmdLineVersion();

	# XXX - isRoot should be readOnly, and configurable.
	$actions->add ("mount", $mountPoint,
		options => $opts,
		fsType => $fsType,
		isRoot => 1,
		device => $yspecial);
}


#
# addFsTabMount -- add list of actions to mount the device
# at fsTabEntry (eg /, /usr, /var) at mountPoint (eg /mnt)
# on the initial file system.  the fsTabEntry must correspond
# to a block device: NFS not supported here.
#
sub addFsTabMount ($$$) {
	my ($actions, $fsTabEntry, $mountPoint) = @_;
	my $root = FsTab::findByMountPoint($fsTabEntry);
	if (! defined ($root)) {
		fatal ("can't find $fsTabEntry in fstab");
	}

	my ($blockDevName, $msg) = $root->blockDevPath();
	if (! defined ($blockDevName)) {
		Base::fatal ($msg);
	}
	addBlockDevMount ($actions, $blockDevName, $mountPoint);
}


#
# makePlan -- given list of goals read from config file,
# translate to list of actions that can be put on the initial image.
#
sub makePlan ($) {
	my ($goals) = @_;
	my $actions = ActionList->new();

	for my $goal (@{$goals}) {
		my $origin = $goal->{origin};
		my $type = $goal->{type};
		my $value = ($goal->{value} || "--");

		Base::info ("goal: $type, $value ($origin)");

		if ($type eq 'template') {
			$actions->add ($value, "");
		}
		elsif ($type eq 'module') {
			ModProbe::addModules ($actions, [ $value ]);
		}
		elsif ($type eq 'input') {
			addInputPlan ($actions);
		}
		elsif ($type eq 'network') {
			addNetworkPlan ($actions);
		}
		elsif ($type eq 'mountdir') {
			my $mountPoint = $goal->{mountPoint};
			Base::assert (defined ($mountPoint));
			addFsTabMount ($actions, $value, $mountPoint);
		}
		elsif ($type eq 'mountdev') {
			my $mountPoint = $goal->{mountPoint};
			Base::assert (defined ($mountPoint));
			addBlockDevMount ($actions, $value, $mountPoint);
		}
		else {
			Base::fatal ("Unknown goal");
		}
	}
	return $actions;
}

1;
