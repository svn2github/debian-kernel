#!perl -w
#
# Base -- support stuff
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
# setProgName -- what label to use in further messages.
# setVerbose -- if true, print info
# setDebug -- if true, print info and debug
# setQuiet -- if true, don't print warnings
#
# debug -- irritating noise
# info -- reassuring noise
# warning -- not good, but processing continues, exit status unaffected
# error -- not good, processing continues, exit status non-zero,
# 		program should not make changes to environment
# 		such as writing output files.
# fatal -- not good, exit immediately with non-zero status.
# bug -- fatal error in the program, not environment.
# assert -- if condition not met, it's a bug.
# bye -- exit now.
#
# Also routines to
# - read content of one-line files
# - get device number from block or character device files
# - interpret symbolic links
# - interpret pathnames
#
use strict;
use warnings;
package Base;


my $progName = "[app]";
sub setProgName ($) { $progName = $_[0]; }

my $verbose = 0;
sub setVerbose ($) { $verbose = $_[0]; }

my $debug = 0;
sub setDebug ($) { $debug = $_[0]; }

my $quiet = 0;
sub setQuiet ($) { $quiet = $_[0]; }


my $exitCode = 0;


sub debug ($) {
	my $msg = shift;
	if ($debug) {
		print "$progName:D: $msg\n";
	}
}

sub info ($) {
	my $msg = shift;
	if ($verbose || $debug) {
		print "$progName: $msg\n";
	}
}

sub warning ($) {
	my $msg = shift;
	if (! $quiet) {
		print STDERR "$progName warning: $msg\n";
	}
}

sub error ($) {
	my $msg = shift;
	print STDERR "$progName error: $msg\n";
	$exitCode = 1;

}

sub fatal ($) {
	my $msg = shift;
	error ($msg . " (fatal)");
	bye();
}

sub bug ($) {
	my $msg = shift;
	my ($package, $file, $line) = caller;
	fatal ($msg . " (internal $file:$line)");
}

sub assert ($) {
	my $cond = shift;
	my ($package, $file, $line) = caller;
	if (! $cond) {
		fatal ("assert failed (internal $file:$line)");
	}
}

sub bye () {
	exit ($exitCode);
}


#
# getStringFile -- return contents of a file that must contain
# a single line, dropping optional trailing space, or die.
#
sub getStringFile ($) {
	my ($filename) = @_;
	if (! open (F, "<", "$filename")) {
		Base::fatal ("can't open $filename");
	}
	my $line = <F>;
	if (! defined ($line)) {
		Base::fatal ("empty file $filename");
	}
	chomp $line;
	if (defined (<F>)) {
		Base::fatal ("extra lines in $filename");
	}
	if (! close (F)) {
		Base::fatal ("could not read $filename");
	}
	return $line;
}


#
# getHexFile -- given path to file, return content interpreted as hex number.
#
sub getHexFile ($) {
	my ($filename) = @_;
	my $content = getStringFile ($filename);
	if ($content !~ /^(0x)?[0-9a-fA-F]+$/) {
		fatal ("not a hex file: $filename");
	}
	return hex($content);
}


#
# devno -- given pathname to a device, return "maj:min" or undef.
# symlinks are resolved implicitly.
#
sub devno ($) {
	my ($path) = @_;
	if ( ! (-b $path || -c _)) {
		return undef;
	}

	my @fields = stat _;
	if ($#fields != 12) {
		Base::fatal ("stat failed on device $path");
	}
	# from 2.6.10-rc2, kdev.h, backward compatible.
	my $devno = $fields[6];
	my $major = ($devno & 0xfff00) >> 8;
	my $minor = ($devno & 0xff) | (($devno >> 12) & 0xfff00);
	return "$major:$minor";
}


#
# expandLink -- given a path to a symlink file,
# return a path to what it points to.
#
sub expandLink ($) {
	my ($path) = @_;
	Base::assert (-l $path);
	my $target = readlink("$path");

	if (isAbsolute ($target)) {
		return canon ($target);
	}
	my $base = dirname ($path);
	return canon ("$base/$target");
}


#
# isAbsolute -- given a path, return true iff it starts at root.
#
sub isAbsolute ($) {
	my ($path) = @_;
	return $path =~ /^\//;
}


#
# canon -- given path, return copy with redundant stuff removed.
#
sub canon ($) {
	my ($path) = @_;

	if ($path eq '') {
		$path = '.';
	}
	if ($path =~ m![^/]/+$!) {
		# drop trailing slashes, except in path like ////
		$path =~ s!/+$!!;
	}

	# iterate over components (ignoring duplicate slashes)
	# interpret a null list as 'current directory'
	# interpret a leading '' in list as absolute path
	# . can be dropped
	# .. causes dropping of last element of list, but:
	# (1) /.. => / 
	# (2) ../.. is not dropped.
	# (3) ./.. => ..
	my @result = ();
	for my $component (split (/\/+/, $path)) {
		if ($component eq '.') {
			next;
		}
		elsif ($component eq '..') {
			if ($#result == -1) {
				push @result, '..';
			}
			elsif ($#result == 0 && $result[0] eq '') {
				next;
			}
			elsif ($result[$#result] eq '..') {
				push @result, '..';
			}
			else {
				pop @result;
			}
		}
		else {
			push @result, $component;
		}
	}
	if ($#result == -1) {
		return ".";
	}
	elsif ($#result == 0 && $result[0] eq '') {
		return '/';
	}
	else {
		return join ('/', @result);
	}
}


#
# basename -- given path, return final component of canonified path
#
sub basename ($) {
	my ($path) = @_;
	my $t = canon ($path);
	if ($t =~ m!.*/([^/]+)$!) {
		# something is after the slash,
		# the simple case
		return $1;
	}
	elsif ($t !~ m!/!) {
		# basename aap => aap
		return $t;
	}
	else {
		return '/';
	}
}


#
# dirname -- given path, return all but final component of canonified path
#
sub dirname ($) {
	my ($path) = @_;
	my $t = canon ($path);
	if ($t =~ m!(.+)/[^/]+$!) {
		# something before and after the slash
		return $1;
	}
	elsif ($t !~ m!/!) {
		# dirname aap => .
		# but also dirname .. => .
		return '.';
	}
	else {
		return '/';
	}
}

1;
