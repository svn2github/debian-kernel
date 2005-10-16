#!perl -w
#
# Blacklist -- encapsulate /etc/hotplug/blacklist
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
# isBlacklisted -- module occurs in one of the hotplug blacklists.
# normalise underscore to hyhpen before comparing.
#

use strict;
use warnings;
use Base;
use Conf;
package Blacklist;

my $blackMap = undef;
my $blackList = undef;

sub init () {
	if (defined ($blackList)) {
		return;
	}
	my $blackMap = {};
	my $hotplugDir = Conf::get ('hotplug');
	my $blackListName = "hotplugDir/blacklist";
	if (-e $blackListName) {
		# The blacklist does not have to exist:
		# there are machines without hotplug,
		# and blacklist support in hotplug is
		# likely to be replaced with a keyword in
		# the module-init-tools config file.
		# If it does exist, but is unreadable,
		# that's an error.
		parseBlackList ($blackListName);
	}

	my $dir;
	my $dirName = "$hotplugDir/blacklist.d";
	if (-d $dirName) {
		# blacklist.d does not have to exist (Debian
		# has it, Fedora not), but if it exists,
		# it must be readable.
		if (! opendir ($dir, $dirName)) {
			Base::fatal ("can't open directory $dirName");
		}
		while (defined(my $entry = readdir($dir))) {
			if ( -f "$dirName/$entry" ) {
				parseBlackList ("$dirName/$entry");
			}
		}
		if (! closedir ($dir)) {
			Base::fatal ("could not read directory $dirName");
		}
	}

	my $blackList = [ keys %{$blackMap} ];
}

sub parseBlackList ($) {
	my ($fileName) = @_;
	if (! open (IN, "<", "$fileName")) {
		Base::fatal ("can't open blacklist $fileName");
	}
	while (defined (my $line = <IN>)) {
		chomp $line;
		$line =~ s/#.*//;
		$line =~ s/^\s+//;
		$line =~ s/\s+$//;
		$line =~ s/_/-/g;
		if ($line eq "") {
			next;
		}
		if ($line !~ /^[a-zA-Z0-9-]+$/) {
			Base::fatal ("bad line in blacklist $fileName: $line");
		}
		$blackMap->{$line}++;
	}
	if (! close (IN)) {
		Base::fatal ("could not read blacklist $fileName");
	}
}

sub all	() {
	init;
	return $blackList;
}

sub isBlacklisted ($) {
	my ($module) = @_;
	init;
	$module =~ s/_/-/g;
	return exists ($blackMap->{$module});
}


1;
