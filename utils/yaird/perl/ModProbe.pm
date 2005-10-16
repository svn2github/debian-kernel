#!perl -w
#
# ModProbe -- insert modules plus dependencies
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
# This uses modprobe to determine which files to load to get a certain
# module.  It adds two extra features:
#  - do not load blacklisted modules
#  - if we know a module is compiled into the kernel,
#    don't attempt to load it, and don't complain.
#
# Notes on the behaviour of modprobe:
#
#   modprobe --verbose --dry-run --show-depends --set-version 2.6.11 aes
#
# shows the files that are needed, regardless of whether they are
# already loaded.  Lines start with 'insmod ' for files to be loaded,
# or 'install ' for commands to be executed.
#
# Modprobe file format:
#  - characters are bytes, no messing with utf-8.
#  - backslash at end of line merges lines
#  - other \x get replaced by x
#  - ^\s*# lines are ignored
#  - lines without : are ignored.
# This means "aap: noot.#7" is a valid dependency.
# The backslash interpretation is mostly for modprobe.conf;
# depmod does not generate it.
#
# Modprobe determines module name by dropping everything after dot:
# "/lib/noot.#7" is module "noot".  We'll adopt the same policy.
#
# Depmod only grabs modules ending in .ko or .ko.gz.
#
# Note that modprobe does not discriminate against modules outside
# /lib/modules: if it's listed in modules.dep, it's a valid module.
#
# Note that redhat has a convention that modules in .../update take
# precedence over other modules with the same name.  Depmod implements
# this by not putting modules that are overridden in modules.dep.
# Thus modprobe needs no special action to support that convention.
#
# The logic modprobe uses to determine which modules to load:
# - replace hyphens with underscore
# - read config specified on command line,
#   OR modprobe.conf OR modprobe.d, only first one found.
#   Remember all "install" and "option" directives,
#   rewrite module name if an alias matches
# - if no alias found and the name is of the form 'symbol:xxx':
# 	look in modules.symbols to resolve to a module name
# - if alias found:
#	make a list of modules to load, based on modules.dep
# - else:
#	make a list of modules to load, based on modules.dep
#	if that turned up no modules AND there was no install cmd:
#		- look in modules.aliases to resolve to modulename
#		- make a list of modules to load, based on modules.dep
# - if the list to load is empty AND there was no install command:
# 	- complain.
# # in insmod():
# - recurse over the module list, most basic stuff first, doing:
# 	- if there is a command for this module name:
# 		execute it
# 	- else:
#		load the module
#
# Loading of the module is done with a system call.  The option string
# is passed as third argument; splitting the option string in separate
# module options is done in the kernel.  Double quote escapes spaces,
# double quotes themselves cannot be escaped.
#
use strict;
use warnings;
use Base;
use Conf;
use ActionList;
use Blacklist;
use KConfig;
package ModProbe;


#
# addModules -- given an actionList and list of modules,
# add actions to load all modules, plus their dependencies,
# unless blacklisted or compiled in.
#
sub addModules ($$) {
	my ($actionList, $modList) = @_;

	for my $moduleName (@{$modList}) {
		if (Blacklist::isBlacklisted ($moduleName)) {
			next;
		}
		if (KConfig::isBuiltIn ($moduleName)) {
			next;
		}
		addOneModule ($actionList, $moduleName);
	}
}


#
# addOneModule -- for one module, that is not blacklisted or compiled in,
# add actions to load it plus all its dependencies.
#
sub addOneModule ($$) {
	my ($actionList, $m) = @_;
	my $v = Conf::get('version');
	my $cmd = "/sbin/modprobe -v -n --show-depends --set-version $v $m";
	if (! open (IN, "-|", "$cmd 2> /dev/null")) {
		Base::fatal ("can't run modprobe");
		return;
	}
	while (defined (my $line = <IN>)) {
		chomp $line;
		$line =~ s/\s+$//;
		if ($line =~ /^install (.*)/) {
			Base::fatal ("modprobe $m requires install $1");
		}
		elsif ($line =~ /^insmod (\S+)$/) {
			$actionList->add ("insmod", $1,
				optionList => '');
		}
		elsif ($line =~ /^insmod (\S+)\s+(.*)$/) {
			my $file = $1;
			my $option = $2;
			
			# This should allow options in modprobe.conf
			# to contain both ' and " characters and spaces.
			$option =~ s![^a-zA-Z0-9,_./=-]!\\$&!g;

			$actionList->add ("insmod", $file,
				optionList => $option);
		}
		else {
			Base::fatal ("modprobe $m - unsupported output $line");
		}
	}
	if (! close (IN)) {
		Base::fatal ("error running modprobe $m");
	}
}

1;

