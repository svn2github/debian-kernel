#!perl -w
#
# CryptTab -- encapsulate /etc/crypttab.
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
# Hmm, what do we do about no crypttab?
#
use strict;
use warnings;
use Base;
use Conf;
use CryptEntry;
package CryptTab;


my $cryptTab = undef;


sub init () {
	if (defined ($cryptTab)) {
		return;
	}
	$cryptTab = [];
	my $name = Conf::get('crypttab');
	if (! -e $name) {
		#
		# It's OK if there's no /etc/crypttab, but if it
		# exists, it had better be readable.
		#
		return;
	}
	if (! open (IN, "<", "$name")) {
		Base::fatal ("can't read $name");
	}
	my $lineNo = 0;
	while (defined (my $line = <IN>)) {
		$lineNo++;
		chomp $line;

		$line =~ s/^\s*//;
		next if $line =~ /^#/;	# comment line
		next if $line eq "";

		my @fields = split (/\s+/, $line);
		if (@fields < 2) {
			Base::fatal ("no source device in $name:$lineNo");
		}
		my $dst = shift @fields;
		my $src = shift @fields;
		my $keyFile = shift @fields; # may be undef
		my $optString = (shift @fields or '');
		my $opts = Opts->new (string => $optString);

		if (defined ($keyFile) && $keyFile eq 'none') {
			$keyFile = undef;
		}

		my $descr = CryptEntry->new(
			dst => $dst,
			src => $src,
			keyFile => $keyFile,
			opts => $opts,
			origin => "$name:$lineNo",
			);
		push @{$cryptTab}, $descr;
	}
	if (! close (IN)) {
		Base::fatal ("could not read $name");
	}
}

sub all	() {
	init;
	return $cryptTab;
}

#
# findByDst -- return crypttab entry or undef
#
sub findByDst ($) {
	my ($dst) = @_;
	my $result;
	for my $entry (@{CryptTab::all()}) {
		if ($entry->dst() eq $dst) {
			if (defined ($result)) {
				my $o1 = $entry->origin;
				my $o2 = $result->origin;
				Base::fatal ("duplicate device '$dst' in $o1, $o2");
			}
			$result = $entry;
		}
	}
	return $result;
}

1;
