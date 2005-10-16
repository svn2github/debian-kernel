#!perl -w
#
# Input - a single device from the input layer
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
# info - info provided by low level driver for module matching
# name - provided by driver
# phys - provided by driver, no relation to sysfs location
# handlers - input layer handlers
# capabilities - kind of events that can be generated
# hw - ...
#
# Handler 'kbd' indicates the device is used for console;
# there is also a generic handler for each device, event\d+,
# that may have a link to underlying sysfs device.
# Capability KEY indicates there are buttons, this includes
# both qwerty keys and the fire button on a joystick.
#

use strict;
use warnings;
use Base;
use Conf;
package Input;
use base 'Obj';

sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->takeArgs ('info', 'name', 'phys', 'handlers', 'capabilities');

	#
	# Find hardware link, if any.
	#
	$self->{hw} = undef;
	for my $handler (keys %{$self->handlers}) {
		if ($handler !~ /^event\d+$/) {
			next;
		}
		my $devLink = Conf::get('sysFs')
			. "/class/input/$handler/device";
		my $hw = readlink ($devLink);
		if (defined ($hw)) {
			unless ($hw =~ s!^(\.\./)+devices/!!) {
				# imagine localised linux (/sys/geraete ...)
				Base::fatal ("bad device link in $devLink");
			}
			$self->{hw} = $hw;
		}
	}
}

sub info	{ return $_[0]->{info}; }
sub name	{ return $_[0]->{name}; }
sub phys	{ return $_[0]->{phys}; }
sub handlers	{ return $_[0]->{handlers}; }
sub capabilities	{ return $_[0]->{capabilities}; }
sub hw		{ return $_[0]->{hw}; }

sub string {
	my $self = shift;
	my $name = $self->name;
	my $phys = $self->phys;
	my $hw = $self->hw;
	$hw = "--" unless defined $hw;
	my $h = join (",", keys %{$self->handlers});
	my $c = join (",", keys %{$self->capabilities});
	my $kbd = $self->isKbd ? " (KBD)" : "";
	my $str = "$name is $phys at $hw [$h] [$c]$kbd";
	return $str;
}

#
# isKbd -- device may be useful to get an operational keyboard.
# This is conservative: there are two input devices for a DELL USB
# keyboard for example, and we make no effort to determine if
# we can leave one of them out.
#
sub isKbd {
	my $self = shift;
	if (! exists ($self->capabilities->{KEY})) {
		return 0;
	}
	if (! exists ($self->handlers->{kbd})) {
		return 0;
	}
	return 1;
}

1;
