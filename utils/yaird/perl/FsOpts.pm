#!perl -w
#
# FsOpts -- encapsulate the options in an FsEntry
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
# This just adds a fstab specific access function to basic opts.
#
use strict;
use warnings;
package FsOpts;
use base 'Opts';

#
# cmdLineVersion -- return a version of all options suitable for
# a mount command line.
# Note that some options in fstab can only be used in fstab,
# not in scripts.
#
sub cmdLineVersion {
	my $self = shift;
	my $opts = $self->{optsHash};

	my @cmdLine = ();
	for my $key (sort keys %{$opts}) {
		next if $key eq 'auto';
		next if $key eq 'noauto';
		next if $key eq 'nouser';
		next if $key eq 'owner';
		next if $key eq 'user';
		next if $key eq 'users';
		next if $key eq 'defaults';
		my $val = $opts->{$key};
		if (defined ($val)) {
			push @cmdLine, "$key=$val";
		}
		else {
			push @cmdLine, "$key";
		}
	}
	if ($#cmdLine == -1) {
		return "";
	}
	else {
		# protect against logdev=John's disk.
		my $cmdLine = join (',', @cmdLine);
		$cmdLine =~ s/(['\\])/\\$1/g; 
		return "-o '$cmdLine'";
	}
}

1;

