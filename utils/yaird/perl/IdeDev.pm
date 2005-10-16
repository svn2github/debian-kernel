#!perl -w
#
# IdeDev -- probed values for an IDE device as found in /proc/ide
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
# The probed values for an IDE device as found in /proc/ide,
# based on the name in /sys/devices/.../ideX/X.X/block.
#
# path - location in sysfs
# name - descriptive string, retrieved from device
# media - type of device: disk, tape, ...
# model - descriptive string, retrieved from device
#
# Background: hotplug is triggered when an IDE controller is
# found on for instance the PCI bus.  The loaded module should
# be good enough to talk to the hardware, but that does not
# mean you can actually use it: you will also need something
# to use the hardware driver to send IDE commands over the
# IDE cable to the controller on the other end of the cable.
# Those commands also have to make sense: a disk controller
# uses a different set of IDE commands than an IDE tape controller.
# The ide-disk, ide-cdrom etc modules are the protocol drivers
# that handle this.
#
# Ide-generic is special: it does not talk to the device at the far end
# of the cable, but to the chipset at the near end.  It manages a lot of
# different devices (in fact can function as a default for any PCI IDE
# chipset) but does not come with modalias to attest to that capability.
# When ide-disk is seen, load ide-generic as well.  The effect is that if
# there was no specific driver for the PCI chipset, ide-generic will
# function as a default.
#
# The following detection is based on an ide.rc script by Marco d'Itri
# that was not included in hotplug.  Note that some CDROMs may need
# ide-generic in addition to ide-cdrom to work; that can be considered
# a driver bug rather than a valid dependency.  Note that there is discussion
# over whether ide-generic should grab otherwise unhandled IDE devices.
# - http://thread.gmane.org/gmane.linux.hotplug.devel/6003
# - http://lists.debian.org/debian-kernel/2004/11/msg00218.html
# - http://www.ussg.iu.edu/hypermail/linux/kernel/0410.1/1452.html
#
# Hmm, via82cxxx (2.6.8) also needs ide-generic to load it seems.  That could
# be because ide-generic contains a call to ide_probe_init() which is in
# the ide-core module.  Or it could be because the IDE part of the chip
# is managed by ide-generic.
#
use strict;
use warnings;
use Base;
use Conf;
package IdeDev;
use base 'Obj';

sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->takeArgs ('path');
	my $path = $self->path;
	my $link = readlink ("$path/block");
	if (! defined ($link)) {
		Base::fatal ("no link to block device in $path");
	}
	if ($link !~ m!.*/([^/]+)!) {
		Base::fatal ("malformed link to block device in $path");
	}
	my $name = $1;
	my $ideDir = Conf::get('procFs') . "/ide";
	$self->{name} = $name;
	$self->{media} = Base::getStringFile ("$ideDir/$name/media");
	$self->{model} = Base::getStringFile ("$ideDir/$name/model");
}

sub path	{ return $_[0]->{path}; }
sub name	{ return $_[0]->{name}; }
sub media	{ return $_[0]->{media}; }
sub model	{ return $_[0]->{model}; }

sub string {
	my $self = shift;
	my $path = $self->path();
	my $name = $self->name();
	my $media = $self->media();
	my $model = $self->model();
	return "$name ($media) = $model at $path";
}


#
# findModuleByIdeDev -- list of suitable IDE drivers;
# you need all of them.
#
sub findModuleByIdeDev ($) {
	my ($ideDev) = @_;
	my $media = $ideDev->media();
	my $result = [];
	if (! KConfig::isOmitted ("ide-generic")) {
		# Supply ide-generic as default chipseet driver only if
		# it was compiled into the kernel.
		push @{$result}, "ide-generic";
	}
	
	my $driver;
	$driver = "ide-disk" if ($media eq "disk");
	$driver = "ide-tape" if ($media eq "tape");
	$driver = "ide-cd" if ($media eq "cdrom");
	$driver = "ide-floppy" if ($media eq "floppy");
	if (defined ($driver)) {
		push @{$result}, $driver;
	}

	return $result;
}


1;

