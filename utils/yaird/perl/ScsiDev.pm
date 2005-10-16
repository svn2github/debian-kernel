#!perl -w
#
# ScsiDev -- the probed values for a SCSI device, as found in /sys.
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
# Like IDE devices, SCSI devices need a protocol driver, based
# on device type, without hardware probing and matching against
# a kernel generated table.
#
# The type and module knowledge is based in /etc/hotplug/scsi.agent,
# that attributes scsi/scsi.h.
#
use strict;
use warnings;
use Base;
package ScsiDev;
use base 'Obj';

sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->takeArgs ('path');
	my $path = $self->path;
	my $name = readlink ("$path/block");
	if (defined ($name)) {
		$name = Base::basename ($name);
		$self->{name} = $name;
	}
	else {
		$self->{name} = undef;
	}
	$self->{vendor} = Base::getStringFile ("$path/vendor");
	$self->{model} = Base::getStringFile ("$path/model");

	my $type = Base::getStringFile ("$path/type");
	$self->{type} = "disk" if $type eq '0';
	$self->{type} = "tape" if $type eq '1';
	$self->{type} = "printer" if $type eq '2';
	$self->{type} = "processor" if $type eq '3';
	$self->{type} = "worm" if $type eq '4';
	$self->{type} = "cdrom" if $type eq '5';
	$self->{type} = "scanner" if $type eq '6';
	$self->{type} = "mod" if $type eq '7';
	$self->{type} = "changer" if $type eq '8';
	$self->{type} = "comm" if $type eq '9';
	$self->{type} = "enclosure" if $type eq '14';
	$self->{type} = "type$type" if (!defined ($self->{type}));
}

sub path	{ return $_[0]->{path}; }
sub name	{ return $_[0]->{name}; }
sub vendor	{ return $_[0]->{vendor}; }
sub model	{ return $_[0]->{model}; }
sub type	{ return $_[0]->{type}; }

sub string {
	my $self = shift;
	my $path = $self->path();
	my $name = $self->name();
	my $vendor = $self->vendor();
	my $model = $self->model();
	my $type = $self->type();
	$name = "---" if (! defined ($name));
	return "$name ($type) = $vendor, $model at $path";
}


#
# findModuleByScsiDev -- list of required SCSI drivers
#
sub findModuleByScsiDev ($) {
	my ($scsiDev) = @_;
	my $type = $scsiDev->type();
	my $driver;
	$driver = "sd-mod" if $type eq "disk";
	# "FIXME some tapes use 'osst'"
	$driver = "st" if $type eq "tape";
	# if $type eq "printer";
	# if $type eq "processor";
	$driver = "sr-mod" if $type eq "worm";
	$driver = "sr-mod" if $type eq "cdrom";
	# if $type eq "scanner";
	$driver = "sd-mod" if $type eq "mod";
	# if $type eq "changer";
	# if $type eq "comm";
	# if $type eq "enclosure";
	$driver = "sg" if (! defined ($driver));
	return [ $driver ];
}

1;


