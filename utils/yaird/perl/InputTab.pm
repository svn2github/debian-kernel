#!perl -w
#
# InputTab -- encapsulate /proc/bus/input/devices
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

use strict;
use warnings;
use Base;
use Conf;
use Input;

package InputTab;

my $inputList = undef;

sub init () {
	if (defined ($inputList)) {
		return;
	}
	$inputList = [];
	my $name = Conf::get('procFs') . '/bus/input/devices';
	if (! open (IN, "<", "$name")) {
		Base::fatal ("can't open $name");
	}
	my $work = {
		capabilities => {},
	};
	while (defined (my $line = <IN>)) {
		chomp $line;
		if ($line =~ /^I: (.*)$/) {
			$work->{info} = $1;
		}
		elsif ($line =~ /^N: Name="(.*)"$/) {
			$work->{name} = $1;
		}
		elsif ($line =~ /^P: Phys=(.*)$/) {
			$work->{phys} = $1;
		}
		elsif ($line =~ /^H: Handlers=(.*)$/) {
			my @handlers = split (/\s+/, $1);
			$work->{handlers} = {};
			for my $h (@handlers) {
				$work->{handlers}{$h}++;
			}
		}
		elsif ($line =~ /^B: ([A-Z]+)=(.*)$/) {
			$work->{capabilities}{$1} = $2;
		}
		elsif ($line =~ /^$/) {
			if (! exists ($work->{info})) {
				Base::fatal ("missing I: in $name");
			}
			if (! exists ($work->{name})) {
				Base::fatal ("missing N: in $name");
			}
			if (! exists ($work->{phys})) {
				Base::fatal ("missing P: in $name");
			}
			if (! exists ($work->{handlers})) {
				Base::fatal ("missing H: in $name");
			}
			push @{$inputList}, Input->new (%{$work});
			$work = {
				capabilities => {},
			};
		}
		else {
			Base::fatal ("unrecognised line in $name: $line");
		}

	}
	if (! close (IN)) {
		Base::fatal ("could not read $name");
	}
}

sub all	() {
	init;
	return $inputList;
}


1;
