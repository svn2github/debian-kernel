#!perl -w
#
# Image -- what should go on the initrd image
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
# Note: consider keeping origin information with files on the image,
# so that we can give more precise error messages if some file cannot
# be copied to the image.
#
use strict;
use warnings;
use SharedLibraries;
use File::Copy;
use Base;
package Image;
use base 'Obj';

sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->{directories} = {};
	$self->{files} = {};
	$self->{devices} = {};
	$self->{symlinks} = {};
	$self->{scripts} = {};
}

#
# addFile -- mark listed file, it should be copied to image.
# If we add a symlink, make sure it stays a symlink in the target
# image, and that both link and target are included.  This matters
# eg for LVM, that has lots of symlinks to a single executable,
# with behaviour determined by the name of the link.
#
sub addFile {
	my ($self, $file, $origin) = @_;
	Base::assert (Base::isAbsolute ($file));

	# avoid problems with adding /lib/modules/. or
	# distinguishing between /bin and //bin.
	$file = Base::canon($file);

	$self->addParents ($file);
	if (-l $file) {
		if (! exists ($self->{symlinks}{$file})) {
			# careful: in order to break loops, we must
			# first mark the symlink, *then* follow it.
			# We cannot warn, since we do not know whether
			# the symlink was found in the course of symlink
			# resolution, or whether multiple executables
			# have the same symlink as a shared library.
			#
			# NOTE: Perhaps maintain an explicit stack of
			# symlinks being resolved in order to be able
			# to warn about loops?
			#
			my $link = readlink ($file);
			if (! defined ($link)) {
				Base::fatal ("could not follow symlink $file ($origin)");
			}
			if ($link !~ m!^([\w./-]+)$!) {
				# taint check.
				Base::fatal ("Strange character in symlink target for $file ($origin)");
			}
			$link = $1;

			Base::debug ("to image: $file (symlink) ($origin)");
			$self->{symlinks}{$file} = $link;
			my $target = Base::expandLink ($file);
			$self->addFile ($target, $origin);
		}
	}
	elsif (-b $file) {
		my $devno = Base::devno ($file);
		Base::debug ("to image: $file (blockdev) ($origin)");
		$self->{devices}{$file} = "b:$devno";
	}
	elsif (-c $file) {
		my $devno = Base::devno ($file);
		Base::debug ("to image: $file (chardev) ($origin)");
		$self->{devices}{$file} = "c:$devno";
	}
	elsif (-d $file) {
		$self->addDirectory ($file, $origin);
	}
	elsif (-f $file) {
		Base::debug ("to image: $file ($origin)");
		$self->{files}{$file}++;
		if (-x $file) {
			$self->addLibraries ($file, $origin);
		}
	}
	elsif (! -e $file) {
		Base::fatal ("missing file requested for image: $file ($origin)");
	}
	else {
		Base::fatal ("odd file mode: $file ($origin)");
	}
}


#
# addDirectory -- add a directory to the image
# the difference with addFile ($dir) is that the directory
# does not have to exist on the mother system.
#
sub addDirectory {
	my ($self, $directory, $origin) = @_;
	Base::assert (Base::isAbsolute ($directory));

	$self->addParents ($directory);
	Base::debug ("to image: $directory (directory) ($origin)");
	$self->{directories}{$directory}++;
}


#
# addParents -- given a pathname to be included on
# initrd image, ensure that parent directories are also created.
#
sub addParents {
	my ($self, $path) = @_;
	Base::assert (Base::isAbsolute ($path));

	my @components = split (/\/+/, $path);
	for my $idx (1 .. ($#components -1)) {
		my $dir = join ('/', @components[0 .. $idx]);
		$self->{directories}{$dir}++;
	}
}

#
# addTree -- add a tree to the image
# this is an existing file or directory with everything in it.
# NOTE: There's a trade-off here: it would be faster to defer walking
# the tree to the actual writing of the tree, but that would require
# duplicating the addFile checks on file type.  2.6.10 has 1700 files
# in /lib/modules; I expect the overhead to be acceptable.
#
sub addTree {
	my ($self, $treeName, $origin) = @_;

	$self->addFile ($treeName);
	if (-d $treeName && ! -l $treeName) {
		my $dir;
		if (! opendir ($dir, $treeName)) {
			Base::fatal ("can't open directory $treeName ($origin)");
		}
		while (defined(my $entry = readdir($dir))) {
			next if $entry eq ".";
			next if $entry eq "..";
			my $path = "$treeName/$entry";
			$self->addTree ($path, $origin);
		}
		if (! closedir ($dir)) {
			Base::fatal ("could not read directory $treeName ($origin)");
		}
	}
}


#
# addLibraries -- given an executable to be included on
# initrd image, ensure that shared libraries are also included.
# If the executable is a shell script or statically linked,
# nothing needs to be done.
#
sub addLibraries {
	my ($self, $executable, $origin) = @_;
	my $in;

	for my $lib (@{SharedLibraries::find ($executable)}) {
		$self->addFile ($lib, $origin);
	}
}

#
# addScriptLine -- given scriptname and text, append text to the
# text to be included in script.
#
sub addScriptLine {
	my ($self, $scriptname, $text, $origin) = @_;
	Base::assert (Base::isAbsolute ($scriptname));

	$self->addParents ($scriptname);
	$self->{scripts}{$scriptname} .= "$text";
}


#
# buildHere -- given pathname of a destination directory,
# create that directory, with all files and scripts prescribed
# by self in it.
# The top directory is expected to be created by File::Temp::tempfile.
#
sub buildHere {
	my ($self, $dest) = @_;

	if (! -d $dest) {
		Base::fatal ("Build directory not found: $dest");
	}
	if (! chmod (0755, $dest)) {
		Base::fatal ("Could not chmod $dest");
	}

	for my $d (sort keys %{$self->{directories}}) {
		if (! mkdir ("$dest$d")) {
			Base::fatal ("Could not mkdir $dest$d");
		}
		if (! chmod (0755, "$dest$d")) {
			Base::fatal ("Could not chmod $dest$d");
		}
	}

	for my $f (sort keys %{$self->{files}}) {
		if (! File::Copy::copy ($f, "$dest$f")) {
			Base::fatal ("Could not copy $f to $dest$f");
		}
		if (-x $f) {
			if (! chmod (0555, "$dest$f")) {
			    Base::fatal ("Could not chmod 0555 $dest$f");
			}
		}
		else {
			if (! chmod (0444, "$dest$f")) {
			    Base::fatal ("Could not chmod 0444 $dest$f");
			}
		}
	}

	for my $d (sort keys %{$self->{devices}}) {
		my $typDevno = $self->{devices}{$d};
		if ($typDevno =~ /^(b|c):(\d+):(\d+)$/) {
			#
			# permission 0600 is restrictive but sufficient:
			# the devices created here are used only by root
			# on a temporary root, so we can get away with
			# /dev/null mode 0600.  More permissive modes
			# risk ending up with a /dev/hda1 0666 somewhere
			# in a temporary directory.
			#
			if (system ("/bin/mknod -m 0600 '$dest$d' $1 $2 $3") != 0) {
				Base::fatal ("Could not mknod $dest$d $1 $2 $3");
			}
		}
		else {
			Base::bug ("bad devno $typDevno for $d");
		}
	}

	for my $l (sort keys %{$self->{symlinks}}) {
		my $target = $self->{symlinks}{$l};
		if (! symlink ($target, "$dest/$l")) {
			Base::fatal ("Could symlink $target $dest$l");
		}
	}

	for my $scriptname (sort keys %{$self->{scripts}}) {
		my $script = $self->{scripts}{$scriptname};
		if (!open (OUT, ">", "$dest$scriptname")) {
			Base::fatal ("Can't create $dest$scriptname");
		}
		print OUT "$script";
		if (!close (OUT)) {
			Base::fatal ("Could not write $dest$scriptname");
		}
		if (! chmod (0555, "$dest$scriptname")) {
			Base::fatal ("Couldn't chmod 0555 $dest$scriptname");
		}
	}
}

1;
