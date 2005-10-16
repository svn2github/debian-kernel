#!perl -w
#
# Parser -- read the config file and template file
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
# This file contains two separate parsers: one for the overall config,
# one for the template specification.  The reason is that different
# configurations (eg disk or network based) can share the same templates.
#
use strict;
use warnings;
use Parse::RecDescent;

use Base;

package Parser;


		#
		# Comment processing: the part of
		# line after # is ignored.
		#
$Parse::RecDescent::skip = qr{(\s+|#.*\n)*};

		#
		# First grammar: the templates
		#
my $templateGrammar = <<'...';


		#
		# start_rule -- parse config file and
		# return errors.
		# It turns out that everything but the first
		# error is useless.
		#
start_rule :	config_file[fileName => $arg{fileName} ]
	      |	{
			my $fileName = $arg{fileName};
			my $errors = $thisparser->{errors};
			$thisparser->{errors} = undef;
			my $firstError = shift @{$errors};
			my $msg = $firstError->[0];
			my $line = $firstError->[1];
			Base::fatal ("$fileName:$line: $msg");
		}


config_file :	template_set[fileName => $arg{fileName}] end_of_file
		{ $return = $item{template_set}; }
	      |	<error>

end_of_file :	/\z/
	      |	<error: junk at end of file>


template_set :	<rulevar: $set = {} >
template_set :	'TEMPLATE' 'SET' <commit>
			template[fileName => $arg{fileName}, set => $set](s)
		'END' 'TEMPLATE' 'SET'
		{
			$return = $set;
		}
	    |   <error?>


template :	'TEMPLATE' <commit> identifier
		'BEGIN'
			template_directive[fileName => $arg{fileName}](s?)
		'END' 'TEMPLATE'
		{
			my $name = $item{identifier};

			if (exists ($arg{set}->{$name})) {
				my $old = $arg{set}->{$name}{origin};
 				Base::fatal ("$arg{fileName}:$prevline: redefinition of $name (earlier definition at $old)");
			}

			$arg{set}->{$name} = {
				origin => "$arg{fileName}:$prevline",
				identifier => $item{identifier},
				directives => $item{'template_directive(s?)'},
			};
			$return = 1;
		}
	    |	<error?>


template_directive :
		file_directive[fileName => $arg{fileName}]
	    |	dir_directive[fileName => $arg{fileName}]
	    |	tree_directive[fileName => $arg{fileName}]
	    |	script_directive[fileName => $arg{fileName}]
	    |	<error>


		#
		# Include this file from host on the image
		#
file_directive:	'FILE' <commit> pathname
		{
			$return = {
				type => 'file',
				value => $item{pathname},
				origin => "$arg{fileName}:$prevline",
			};
		}

		#
		# Create this directory on the image
		#
dir_directive :	'DIRECTORY' <commit> pathname
		{
			$return = {
				type => 'directory',
				value => $item{pathname},
				origin => "$arg{fileName}:$prevline",
			};
		}

		#
		# Copy this tree recursively from host to image
		#
tree_directive:	'TREE' <commit> pathname
		{
			$return = {
				type => 'tree',
				value => $item{pathname},
				origin => "$arg{fileName}:$prevline",
			};
		}

		#
		# Add this fragment to named script on the image
		#
script_directive: 'SCRIPT' <commit> pathname
		'BEGIN'
			inline_fragment
		'END' 'SCRIPT'
		{
			$return = {
				type => 'script',
				pathname => $item{pathname},
				value => $item{inline_fragment},
				origin => "$arg{fileName}:$prevline",
			};
		}
	    |	<error?>


		#
		# Identifier in config file; normally corresponds to
		# variable in calling perl code.  No reason to allow
		# these identifiers to have same name as reserved
		# words in config language, so no need to quote.
		#
identifier :	/[A-Za-z][A-Za-z0-9_]*/
	      |	<error>


		#
		# Pathname on generated image.  This will be subjected
		# to template substitution, so can contain stuff not
		# commonly found in pathnames
		# Let's not allow newline, to give error recovery some chance.
		#
pathname :	/"[^"\n]+"/
		{
			$return = $item{__PATTERN1__};
			$return =~ s/^"(.*)"$/\1/;
		}
	      |	<error: Invalid pathname: use double quoted string>


		#
		# A here document.  Twist: leading space plus '!'
		# is removed.  This means no terminator is needed,
		# and the here doc can be indented, and (unlike YAML)
		# it's insensitive to tabs vs space problems.
		#
inline_fragment: /!.*\n(\s*!.*\n)*/
		{
			$return = $item{__PATTERN1__};
			$return =~ s/^\s*!//mg;
		}
	      |	<error: Invalid inline fragment: start lines with exclamation mark (!)>

...


		#
		# Second grammar, this one for the overall config file.
		# This has some stuff in common with the template grammar.
		# Can we avoid repeating the code?  Options:
		# - merge config files at install time - too many variants
		# - concat the shared part to each grammar - ugly
		# - parameter to select real start_rule - hmm; possible
		# - recdescent include mechanism - ugly: call can be
		#   only where syntax allows it, but replacement text
		#   can straddle syntactic boundaries.
		# For now, just duplicate.
		#
my $configGrammar = <<'...';


		#
		# start_rule -- parse config file and return errors.
		# It turns out that everything but the first error is useless.
		#
start_rule :	config_file[fileName => $arg{fileName} ]
	      |	{
			my $fileName = $arg{fileName};
			my $errors = $thisparser->{errors};
			$thisparser->{errors} = undef;
			my $firstError = shift @{$errors};
			my $msg = $firstError->[0];
			my $line = $firstError->[1];
			Base::fatal ("$fileName:$line: $msg");
		}


config_file :	config_set[fileName => $arg{fileName}] end_of_file
		{ $return = $item{config_set}; }
	      |	<error>

end_of_file :	/\z/
	      |	<error: junk at end of file>


config_set :	<rulevar: $set = {} >
config_set :	'CONFIG'  <commit>
			config_item[fileName => $arg{fileName}, set => $set](s)
		'END' 'CONFIG'
		{
			if (! exists ($set->{templateFileName})) {
 				Base::fatal ("$arg{fileName}:$prevline: missing templatefile specification");
			}
			if (! exists ($set->{goals})) {
 				Base::fatal ("$arg{fileName}:$prevline: missing goal list");
			}
			$return = $set;
		}
	    |   <error?>


		# do these need origin?  seems a bit small for that.
config_item :	goal_list[fileName => $arg{fileName}, set => $arg{set}]
	    |	format_specification[fileName => $arg{fileName}, set => $arg{set}]
	    |	template_file_name[fileName => $arg{fileName}, set => $arg{set}]
	    |	<error>


format_specification :
		'FORMAT' <commit> identifier
		{
			if (exists ($arg{set}->{format})) {
 				Base::fatal ("$arg{fileName}:$prevline: duplicate format specification");
			}
			$arg{set}->{format} = $item{identifier};
			$return = 1;
		}
	    |	<error?>



template_file_name :
		'TEMPLATE' <commit> 'FILE' pathname
		{
			if (exists ($arg{set}->{templateFileName})) {
 				Base::fatal ("$arg{fileName}:$prevline: duplicate templatefile specification");
			}
			$arg{set}->{templateFileName} = $item{pathname};
			$return = 1;
		}
	    |	<error?>


		#
		# Goal_list - what the initial image should do.
		#
goal_list :	'GOALS' <commit>
			goal_directive[fileName => $arg{fileName}](s?)
		'END' 'GOALS'
		{
			if (exists ($arg{set}->{goals})) {
 				Base::fatal ("$arg{fileName}:$prevline: duplicate goal list");
			}
			$arg{set}->{goals} = $item{'goal_directive(s?)'};
			$return = 1;
		}
	    |	<error?>


goal_directive :
		template_directive[fileName => $arg{fileName}]
	    |	input_directive[fileName => $arg{fileName}]
	    |	network_directive[fileName => $arg{fileName}]
	    |	module_directive[fileName => $arg{fileName}]
	    |	mountdir_directive[fileName => $arg{fileName}]
	    |	mountdev_directive[fileName => $arg{fileName}]
	    |	<error>


		#
		# Expand this template.  No parameters supported.
		#
template_directive:	'TEMPLATE' <commit> identifier
		{
			$return = {
				type => 'template',
				value => $item{identifier},
				origin => "$arg{fileName}:$prevline",
			};
		}

		#
		# Load modules for all input devices
		#
input_directive :	'INPUT'
		{
			$return = {
				type => 'input',
				origin => "$arg{fileName}:$prevline",
			};
		}

		#
		# Load modules for all network devices
		#
network_directive :	'NETWORK'
		{
			$return = {
				type => 'network',
				origin => "$arg{fileName}:$prevline",
			};
		}

		#
		# Load this module
		#
module_directive:	'MODULE' <commit> identifier
		{
			$return = {
				type => 'module',
				value => $item{identifier},
				origin => "$arg{fileName}:$prevline",
			};
		}

		#
		# Mount the fs that fstab lists for pathname
		#
mountdir_directive:	'MOUNTDIR' <commit> pathname mount_point
		{
			$return = {
				type => 'mountdir',
				value => $item{pathname},
				mountPoint => $item{mount_point},
				origin => "$arg{fileName}:$prevline",
			};
		}

		#
		# Mount the fs in blockdev
		#
mountdev_directive:	'MOUNTDEV' <commit> pathname mount_point
		{
			$return = {
				type => 'mountdev',
				value => $item{pathname},
				mountPoint => $item{mount_point},
				origin => "$arg{fileName}:$prevline",
			};
		}

mount_point:	pathname

		#
		# Identifier in config file; normally corresponds to
		# variable in calling perl code.  No reason to allow
		# these identifiers to have same name as reserved
		# words in config language, so no need to quote.
		#
identifier :	/[A-Za-z][A-Za-z0-9_]*/
	      |	<error>


		#
		# Pathname.
		# Let's not allow newline, to give error recovery some chance.
		#
pathname :	/"[^"\n]+"/
		{
			$return = $item{__PATTERN1__};
			$return =~ s/^"(.*)"$/\1/;
		}
	      |	<error: Invalid pathname: use double quoted string>

...


#
# parse -- given grammar and filename, return parse tree or die.
# what the tree looks like is completely determined by the grammar.
#
sub parse ($$) {
	my ($grammar, $fileName) = @_;
	my $parser = new Parse::RecDescent ($grammar);

	if (! open (IN, "<", $fileName)) {
		Base::fatal ("could not open $fileName");
	}
	my $slurp = $/;
	$/ = undef;
	my $text = <IN>;
	$/ = $slurp;
	if (! close (IN)) {
		Base::fatal ("could not read $fileName");
	}

	# $::RD_TRACE = 1;
	my $tree = $parser->start_rule ($text, 0, 'fileName' => $fileName);
	return $tree;
}

#
# parseConfig -- parse config file and template file
#
sub parseConfig ($) {
	my ($fileName) = @_;
	my $config = parse ($configGrammar, $fileName);
	$config->{templates} = parse ($templateGrammar,
		$config->{templateFileName});
	return $config;
}


1;
