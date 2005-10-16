#!perl -w
#
# Obj -- basic object stuff
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
# The new() function takes a hash as argument, used to do object
# initialisation.  Initialisation is delegated to fill() in derived
# classes.  Fill() is expected to use takeArgs() to claim those
# initialisation args it's interested in; other args are left for
# fill() functions further down the inheritance chain.  Arguments
# may have undef as value.
#

use strict;
use warnings;
use Base;
package Obj;

sub new {
	my $class = shift;
	my $self = {};
	$self->{'_args'} = { @_ };
	bless ($self, $class);
	$self->fill();
        my $bad = join (', ', keys %{$self->{'_args'}});
	if ($bad ne '') {
		Base::bug "Unknown constructor args: $bad";
	}
	return $self;
}

sub fill {
	my $self = shift;
	# Derived classes can claim arguments like so:
	# self->SUPER::fill();
	# $self->takeArgs ('a', 'b', 'c');
}

sub takeArgs {
	my ($self, @fields) = @_;
	for my $field (@fields) {
		Base::assert (exists ($self->{'_args'}{$field}));
		$self->{$field} = delete $self->{'_args'}{$field};
	}
}

1;
