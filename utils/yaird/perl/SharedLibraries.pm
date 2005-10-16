#!perl -w
#
# SharedLibraries -- what shared libraries an object needs.
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
use strict;
use warnings;
use Base;
use Conf;
package SharedLibraries;


#
# find -- find the shared libraries that are needed by an executable.
# Try to find libraries with ldd first.  That works only with executables
# linked with glibc, so if ldd finds nothing, we look for traces of klibc.
#
sub find ($) {
	my ($executable) = @_;
	my $in;
	my $result = [];

	if ($executable !~ m!^([.\w/-]+)$!) {
		# taint check.
		Base::fatal ("odd characters in executable name: $executable");
	}
	$executable = $1;

	Base::debug ("looking for shared libs in $executable");
	#
	# <blech>
	# on FC4, modules are excutable (silly), so we need to check for shared
	# libs.  However, with ldd from glibc 2.3.5 this results in:
	# ldd: /lib/ld-linux.so.2 exited with unknown exit code (139)
	# We make an attempt to skip them.
	#
	my $modDir = Conf::get ('modDir');
	if ($executable =~ /^$modDir\/.*\.ko$/) {
		Base::debug ("ignore module $executable");
		return $result;
	}
	# </blech>

	if (! open ($in, "-|", "/usr/bin/ldd $executable")) {
		Base::fatal ("can't read ldd for $executable");
	}
	while (defined (my $line = <$in>)) {
		chomp $line;
		last if ($line =~ /statically linked/);
		if ($line =~ /not a dynamic executable/) {
			#
			# Happens when doing ldd on a shell script
			# or kernel module (these have x-bit on in FC3,
			# no idea why they would do that).  Ldd exit status
			# will be non-zero.
			# Ldd only understands executables compiled with
			# glibc; try to find use of other libraries.
			#
			return findLibr ($executable);
		}
		elsif ($line =~ m!.* => (/.+) \(0x[0-9a-fA-F]+\)$!) {
			#
			# in ldd 2.3.2 (Debian) output looks like this:
			# /lib/ld-linux.so.2 => /lib/ld-linux.so.2 (0xb7fea000)
			#
			push @{$result}, $1;
		}
		elsif ($line =~ m!^\s*(/[^\s]+) \(0x[0-9a-fA-F]+\)$!) {
			#
			# But ldd 2.3.3 (FC3) has this:
			# /lib/ld-linux.so.2 (0x0056f000)
			#
			push @{$result}, $1;
		}
		elsif ($line =~ m!.* =>  \(0x[0-9a-fA-F]+\)$!) {
			#
			# And eg amd64 also has this:
			#	linux-gate.so.1 =>  (0x00000000)
			#
			# note the double space.  This is a section not
			# loaded from a file but mapped in by the kernel.
			# The kernel puts some code with proper syscall
			# linkage here.
			# See http://lkml.org/lkml/2003/6/18/156
			#
			next;
		}
		else {
			Base::fatal ("unexpected ldd output for $executable: $line");
		}
	}
	if (! close ($in)) {
		Base::fatal ("could not read ldd for $executable");
	}
	return $result;
}


#
# findLibr -- given a filename, return name of dynamic loader.
# non-ELF files or non-executable ELF files return empty list,
# corrupt ELF files are fatal error.
# If there are shared libraries without absolute path,
# fatal error, since we don't know how the path the library uses
# to look up the shared library.
#
sub findLibr ($) {
	my ($executable) = @_;
	my $in;
	my $result = [];
	my $auxDir = Conf::get ('auxDir');

	if (! open ($in, "-|", "$auxDir/findlibs -q $executable")) {
		Base::fatal ("can't run findlibr for $executable");
	}

	while (defined (my $line = <$in>)) {
		chomp $line;
		if ($line =~ /^interpreter: ([\w.\/-]+)$/) {
			push @{$result}, $1;
		}
		elsif ($line =~ /^needed: ([\w.\/-]+)$/) {
			if (Base::isAbsolute ($1)) {
				push @{$result}, $1;
			}
			else {
				fatal ("shared library without glibc: $1 in $executable");
			}
		}
		else {
			bug ("odd output for findlibs");
		}
	}

	if (! close ($in)) {
		Base::fatal ("could not run findlibr for $executable");
	}
	return $result;
}

1;
