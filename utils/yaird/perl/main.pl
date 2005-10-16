#!@PERL@
#
# Main -- an exercise in boot image building
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
#
use strict;
use warnings;
use lib '@PERLDIR@';

use Getopt::Long;
use Plan;
use TestSet;
use Pack;
use Parser;

my $appName = "@PACKAGE@";
my $appVersion = "@VERSION@";


#
# go -- main program:
# write an image to destination in specified format.
# destination must not exist yet.
# templates define how actions in masterplan are expanded.
#
sub go ($$) {
	my ($config, $destination) = @_;
	my $masterPlan = Plan::makePlan ($config->{goals});
	my $image = $masterPlan->expand ($config->{templates});
	Pack::package ($image, $config->{format}, $destination);
}


#
# paranoia -- Random bits of -
#
sub paranoia {
	umask 077;

	#
	# Perlsec(1) identifies the following as risky:
	#
	#	delete @ENV{qw(IFS CDPATH ENV BASH_ENV)}; 
	#
	# but just scrapping the complete environment
	# seems to work nicely.
	#
	delete @ENV{keys %ENV};
	$ENV{'PATH'} = '/bin:/sbin:/usr/bin:/usr/sbin';
}


#
# usage -- print invocation patterns of the application
# For --nfs, use -N as short option rather than -n,
# since -n is often used as a 'just-pretend' option.
#
sub usage {
	print <<"//";

$appName generates initrd images.

$appName --test [ version ]
	print an overview of system data, without writing anything to disk.

$appName --output dest [ version ]
	generate an initrd image in 'dest'.

Options:
   -h, -?, --help	print this help text and exit
   -V, --version	print version and exit (don't bother: $appVersion)
   -v, --verbose	print progress messages
   -d, --debug		detailed output
   -q, --quiet		don't print warning messages
   -o, --output dest	place result here
   -c, --config file	use this configuration file
   -f, --format form	produce the image in this format,
			valid values: directory, cramfs, cpio
   -t, --test		print overview of system data,
			used as basis for debugging.

//
}


#
# main -- main program, argument processing
#
sub main {
	my ($opt_help, $opt_version,
		$opt_verbose, $opt_quiet, $opt_debug,
		$opt_config, $opt_format, $opt_output,
		$opt_test);

	paranoia ();
	Base::setProgName ($appName);

	my @warnings = ();
	{
		#
		# catch warnings during getopt, but not elsewhere.
		# The idea is to let -? produce help without complaint,
		# even if other args are broken.
		#
		local $SIG{__WARN__} = sub { push @warnings, $_[0]; };

		Getopt::Long::Configure ("bundling");
		my $result = GetOptions (
			"h|help|?"	=> \$opt_help,
			"V|version"	=> \$opt_version,
			"v|verbose"	=> \$opt_verbose,
			"q|quiet"	=> \$opt_quiet,
			"d|debug"	=> \$opt_debug,
			"c|config=s"	=> \$opt_config,
			"f|format=s"	=> \$opt_format,
			"o|output=s"	=> \$opt_output,
			"t|test"	=> \$opt_test,
			);
	}

	#
	# --help and --version processed without regard to other options,
	# exit without side effect.
	#
	if (defined ($opt_help)) {
		usage ();
		Base::bye();
	}
	if (defined ($opt_version)) {
		Base::setVerbose (1);
		Base::info ("version $appVersion");
		Base::bye();
	}

	#
	# Argument validation
	#
	for my $w (@warnings) {
		# lazy: perhaps we could provide more info here?
		Base::fatal ("$w");
	}
	if (defined ($opt_output) && defined ($opt_test)) {
		Base::fatal "conflicting options --output and --test";
	}
	if (! defined ($opt_output) && ! defined ($opt_test)) {
		Base::fatal "one of --output and --test must be given";
	}
	if (defined ($opt_output)) {
		#
		# Untaint -- is there a need for 'output' with an umlaut?
		# do this before existence test.
		#
		if ($opt_output !~ m!^([-_./[a-zA-Z0-9+]+)$!) {
			Base::fatal ("illegal character in output file name");
		}
		$opt_output = $1;

		if (-e $opt_output) {
			Base::fatal "destination $opt_output already exists";
		}

	}
	if (defined ($opt_test)) {
		if (defined ($opt_format)) {
			Base::fatal "conflicting options --test and --format";
		}
	}
	if ($#ARGV > 0) {
		my $valid = $ARGV[0];
		Base::fatal "extra arguments after $valid";
	}

	#
	# All arguments validated; start processing.
	#
	if ($#ARGV == 0) {
		Conf::set ("version", $ARGV[0]);
	}
	if ($opt_verbose) {
		Base::setVerbose (1);
	}
	if ($opt_debug) {
		Base::setDebug (1);
	}
	if ($opt_quiet) {
		Base::setQuiet (1);
	}
	if (! defined ($opt_config)) {
		$opt_config = Conf::get("cfgDir") . "/Default.cfg";
	}

	# avoid silly messages with an early test
	my $v = Conf::get("version");
	my $mdir = Conf::get("libModules") . "/$v";
	if (! -d $mdir) {
		Base::fatal ("unknown kernel version: $v");
	}

	my $config = Parser::parseConfig ($opt_config);

	#
	# output format on command line overrides config file;
	# output format must be defined in at least one of them.
	#
	if (defined ($opt_format)) {
		$config->{format} = $opt_format;
	}
	if (! defined ($config->{format})) {
		Base::fatal ("no output format defined in config file or command line");
	}
	if (! Pack::knownFormat ($config->{format})) {
		my $format = $config->{format};
		Base::fatal "unknown format: $format";
	}

	#
	# go for it.
	#
	if (defined ($opt_test)) {
		TestSet::testAll();
	}
	elsif (defined ($opt_output)) {
		go ($config, $opt_output);
	}
	Base::bye;
}

main ();
