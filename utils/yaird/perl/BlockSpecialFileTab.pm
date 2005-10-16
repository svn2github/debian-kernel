#!perl -w
#
# BlockSpecialFileTab -- block special files found in /dev
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
# pathsByDevno -- list of all matching block special files in /dev for a devno
# allDevnos -- list of all devno's present in /dev.
#
use strict;
use warnings;
use Conf;
package BlockSpecialFileTab;


my $bsfTab = undef;
my $bsfList = undef;


sub walkBlockSpecialFiles ($);

sub init () {
	if (defined ($bsfTab)) {
		return;
	}
	$bsfTab = {};
	my $devDir = Conf::get('dev');
	walkBlockSpecialFiles ($devDir);
	$bsfList = [ sort keys %{$bsfTab} ];
}

sub walkBlockSpecialFiles ($) {
	my ($dirName) = @_;
	my $dir;
	if (! opendir ($dir, $dirName)) {
		Base::fatal ("can't open directory $dirName");
	}
	while (defined(my $entry = readdir($dir))) {
		next if $entry eq ".";
		next if $entry eq "..";
		my $path = "$dirName/$entry";
		if (-l $path) {
			next;
		}
		elsif (-b _) {
			my $devno = Base::devno ($path);
			push @{$bsfTab->{$devno}}, $path;
		}
		elsif (-d _) {
			walkBlockSpecialFiles ($path);
		}
	}
	if (! closedir ($dir)) {
		Base::fatal ("could not read directory $dirName");
	}
}


#
# pathsByDevno -- list of all matching block special files in /dev for devno,
# or undef.
#
sub pathsByDevno ($) {
	my ($devno) = @_;
	init;
	return $bsfTab->{$devno};
}

#
# all -- return list of all devno's with  a path in /dev.
#
sub allDevnos () {
	init;
	return $bsfList;
}


1;
