#!perl -w
#
# Opts -- encapsulate options as in fstab or crypttab
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
package Opts;
use base 'Obj';


sub fill {
	my $self = shift;
	$self->SUPER::fill();

	$self->takeArgs ('string');
	my @optlist = split (/,/, $self->{string});
	my $opthash = {};
	for my $opt (@optlist) {
		if ($opt =~ /^(.*)=(.*)$/) {
			$opthash->{$1} = $2;
		}
		else {
			$opthash->{$opt} = undef;
		}
	}
	$self->{optsHash} = $opthash;
}

#
# exists -- the option occurs, possibly with undef value.
#
sub exists {
	my ($self, $optnam) = @_;
	return exists ($self->{optsHash}{$optnam});
}

sub get {
	my ($self, $optnam) = @_;
	return $self->{optsHash}{$optnam};
}

sub string {
	my $self = shift;
	my $opts = $self->{optsHash};
	my @optlist = ();
	for my $key (sort keys %{$opts}) {
		push @optlist, ("$key=" . ($opts->{$key} or "-"));
	}
	return join (',', @optlist);
}

1;

