#!perl -w
#
# CryptEntry -- encapsulate a single entry in /etc/crypttab
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
use LabeledPartitionTab;
package CryptEntry;
use base 'Obj';

sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->takeArgs ('dst', 'src', 'keyFile', 'opts', 'origin');
}

sub dst		{ return $_[0]->{dst}; }
sub src		{ return $_[0]->{src}; }
sub keyFile	{ return $_[0]->{keyFile}; }
sub opts	{ return $_[0]->{opts}; }
sub origin	{ return $_[0]->{origin}; }

sub string {
	my $self = shift;
	my $dst = $self->dst();
	my $src = $self->src();
	my $opts = $self->opts()->string();
	return "$dst in $src with $opts";
}


1;
