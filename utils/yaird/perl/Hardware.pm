#!perl -w
#
# Hardware -- find modules for a thingy in /devices.
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
# Method: walk up the path, and for everything you recognise,
# prepend the appropriate module.
# As an example, for a usb-stick connected via a hub to a pci card,
# you'l need usb-storage, a usb protocol driver, and some usb module.
#
use strict;
use warnings;
use Conf;
use IdeDev;
use ScsiDev;
use PciDev;
use PciTab;
use UsbDev;
use UsbTab;
package Hardware;


#
# moduleList -- given a path relativwe to /sys/devices,
# return a list of the modules needed to activate it.
#
sub moduleList ($) {
	my ($path) = @_;
	my $result = [];
	my @components = split (/\/+/, $path);
	my $devicesPath = Conf::get ('sysFs') . "/devices";
	for my $i (0 .. $#components) {
		my $abspath = $devicesPath
			. "/" . join ('/', @components[0 .. $i]);
		my $modules = undef;
		if ($abspath =~ m!/pci[0-9a-f]{4}:[0-9a-f]{2}$!) {
			# PCI bus; harmless
		}
		elsif (-f "$abspath/subsystem_vendor") {
			# PCI function on a slot.
			my $dev = PciDev->new (path => $abspath);
			$modules = PciTab::find ($dev);
			push @{$modules}, @{addPciSiblings ($abspath)};
		}

		elsif (-f "$abspath/bDeviceClass"
			|| -f "$abspath/bInterfaceClass")
		{
			#
			# USB.  Every component in the path
			# "usb1/1-4/1-4.4/1-4.4:1.0" has either
			# a deviceClass or interfaceClass attribute.
			#
			my $dev = UsbDev->new (path => $abspath);
			$modules = UsbTab::find ($dev);
		}

		elsif ($abspath =~ m!/ide\d+$!) {
			# IDE bus; harmless
		}
		elsif ($abspath =~ m!/ide\d+/\d+\.\d+$!) {
			# IDE device
			my $dev = IdeDev->new (path => $abspath);
			$modules = IdeDev::findModuleByIdeDev ($dev);
		}
		
		elsif ($abspath =~ m!/host\d+$!) {
			# grouping of SCSI devices; harmless.
		}
		elsif ($abspath =~ m!/target\d+:\d+:\d+$!) {
			# grouping of SCSI devices within a host
			# (2.6.10 and later); harmless.
		}
		elsif ($abspath =~ m!/\d+:\d+:\d+:\d+$!
			&& -f "$abspath/scsi_level")
		{
			my $dev = ScsiDev->new (path => $abspath);
			$modules = ScsiDev::findModuleByScsiDev ($dev);
		}


		elsif ($abspath =~ m!/platform$!) {
			# the platform (pseudo) bus itself does not need any modules.
		}
		elsif ($abspath =~ m!/i8042$!) {
			# Controller for the serio bus for AT mouse and keyboard.
			$modules = [ "i8042" ];
		}
		elsif ($abspath =~ m!/serio\d+$!) {
			#
			# The following is conceptually wrong: serio is a bus,
			# and an attached device such as a keyboard should be in
			# a subdirectory of that.  However, in 2.6.11, that subdirectory
			# does not exist.  We'll assume the most common case, an AT keyboard;
			# other keyboards will have to wait for a kernel patch.
			#
			# Adding psmouse is also wrong, but at least in FC4,
			# the distro kernel has psmouse compiled in, and nothing
			# probes for it.  This hack really belongs in a template;
			# we'll get to that when the overall config is more flexible.
			#
			$modules = [ "serio", "atkbd", "psmouse" ];
		}
		elsif ($abspath =~ m!/floppy.\d+$!) {
			$modules = [ "floppy" ];
		}

		else {
			# NOTE: We may want to avoid duplicate messages
			Base::error ("unrecognised device: $abspath");
		}

		if (defined ($modules)) {
			push @{$result}, @{$modules};
		}
	}
	return $result;
}


#
# addPciSiblings -- probably a bug.
#
# Here's something odd: my test machine has an USB keyboard, connected
# via PCI.  The same 8 ports are visible both as one 8-port EHCI
# controller and four 2-port UHCI controllers.  The difference is not
# in the hardware, only in the protocol used to talk to the remote device.
# These are different PCI functions (0..7) in the same PCI slot (1d):
#	0000:00:1d.0 USB Controller: Intel ... USB UHCI #1 (rev 03)
#	0000:00:1d.1 USB Controller: Intel ... USB UHCI #2 (rev 03)
#	0000:00:1d.2 USB Controller: Intel ... USB UHCI #3 (rev 03)
#	0000:00:1d.3 USB Controller: Intel ... USB UHCI #4 (rev 03)
#	0000:00:1d.7 USB Controller: Intel ... USB2 EHCI Controller (rev 03)
#
# The keyboard shows up under the EHCI controller, a printer shows up
# under one of the UHCI controllers.
# If we load only the EHCI module, the UHCI-only printer causes some
# complaints, and the keyboard is not detected (unless you try to
# debug this via a serial line ...)
# If you load UHCI as well, the keyboard is detected flawlessly.
#
# We could interpret this as a bug in EHCI, and claim that a non-EHCI
# device on one of the ports should not interfere with detecting devices
# on other ports, but it's more productive to see this as an example of
# how some PCI devices work better if there's a driver for every
# function in the slot.
#
# (Or you could just add a special case to always add an UHCI driver
# after EHCI, but then you would have to consider OHCI as well, plus
# think about blacklisting and competing USB driver implementations.
# I'd rather not go there.)
#
# The kernel function pci_setup_device() uses the following format
# for PCI function directories in sysfs:
#	"%04x:%02x:%02x.%d", domain, bus, slot, function.
#
# Given an absolute path to a PCI function directory, return a list
# of modules needed for all USB functions *except* the one specified by
# the path, but only if the path refers to a USB function.
#
# Limiting this behaviour to USB functions alone is desirable, given
# the existence of chips such as VT8366/A/7, that combine ISA, IDE,
# USB and multimedia in a single PCI slot.
#
sub addPciSiblings ($) {
	my ($abspath) = @_;
	my $modules = [];
	if (! isUsb ($abspath)) {
		return $modules;
	}

	my $dirName = Base::dirname ($abspath);
	my $cur = Base::basename ($abspath);

	if ($cur !~ /^([0-9a-f]{4}):([0-9a-f]{2}):([0-9a-f]{2})\.(\d)$/) {
		Base::fatal ("Odd PCI directory in sysfs: $abspath");
	}
	my $domain = $1;
	my $bus = $2;
	my $slot = $3;
	my $function = $4;

	#print "D $dirName, B $cur, d $domain b $bus s $slot f $function\n";
	my $dir;
	if (! opendir ($dir, $dirName)) {
		Base::fatal ("can't open directory $dirName");
	}
	while (defined(my $entry = readdir($dir))) {
		if ($entry !~ /^([0-9a-f]{4}):([0-9a-f]{2}):([0-9a-f]{2})\.(\d+)$/) {
			next;
		}
		if (! ($1 eq $domain && $2 eq $bus && $3 eq $slot
			&& $4 ne $function))
		{
			next;
		}

		# OK, it's in my slot, and it isn't me.
		# Add required modules if it's USB.
		my $sibling = "$dirName/$entry";
		if (isUsb ($sibling)) {
			my $dev = PciDev->new (path => $sibling);
			push @{$modules}, @{PciTab::find ($dev)};
		}
	}
        if (! closedir ($dir)) {
		Base::fatal ("could not read directory $dirName");
	}
	return $modules;
}

#
# isUsb -- given an absolute path into sysfs, true iff it controls a usb port.
#
sub isUsb ($) {
	my ($abspath) = @_;
	my $dir;
	my $result = 0;
	if (! opendir ($dir, $abspath)) {
		Base::fatal ("can't open directory $abspath");
	}
	while (defined(my $entry = readdir($dir))) {
		if ($entry =~ /^usb\d+$/) {
			$result = 1;
		}
	}
        if (! closedir ($dir)) {
		Base::fatal ("could not read directory $abspath");
	}
	return $result;
}

1;

