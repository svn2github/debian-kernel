#!perl -w
#
# ActionList -- record and expand actions to be performed by the initrd image
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
# Actions are represented by a hash, containing at least:
#  - action
#  - target
#
# Actions are added to the end of the list, but if an action
# with same name and target already exists, the later action
# is considered redundant and silently omited.
#
# The point of an action list is that it can be expanded to an Image,
# an exact description of every file, directory and script that should
# go on the initrd image.
#
# This expansion is driven by templates, where attributes from the
# hash may be inserted in file names or script fragments.
# There are attributes that are added automatically to every action:
#  - version, the required kernel version.
#

use strict;
use warnings;
use HTML::Template;

use Base;
use Conf;

package ActionList;
use base 'Obj';


sub fill {
	my $self = shift;
	$self->SUPER::fill();
	$self->{actions} = [];
}


#
# add -- add an action to the plan.
#
sub add {
	my ($self, $action, $target, %hash ) = @_;

	for my $a (@{$self->{actions}}) {
		if ($a->{action} eq $action && $a->{target} eq $target) {
			Base::debug ("redundant action: $action, $target");
			return;
		}
	}

	my $pairs = "";
	for my $key (sort keys %hash) {
		my $val = ($hash{$key} or '--');
		$pairs .= "$key=$val ";
	}
	Base::info ("action: $action, $target {$pairs}");

	push @{$self->{actions}}, {
		action => $action,
		target => $target,
		version => Conf::get('version'),
		appVersion => Conf::get('appVersion'),
		auxDir => Conf::get('auxDir'),
		%hash
		};
}


#
# string -- render an action list.
#
sub string {
	my $self = shift;
	my $result = "";
	for my $a (@{$self->{actions}}) {
		my $action = $a->{action};
		my $target = $a->{target};
		my $pairs = "";
		for my $key (sort keys %{$a}) {
			next if $key eq "action";
			next if $key eq "target";
			next if $key eq "version";
			next if $key eq "appVersion";
			next if $key eq "auxDir";
			my $val = $a->{$key};
			my $pairs .= "$key=$val ";
		}
		$result .= "\t\t$action $target {$pairs}\n";
	}
	return $result;
}


#
# expandFragment -- given a fragment of text, and a hash
# of parameter settings, return the fragment with HTML::Template
# expansion applied.
# Don't die on bad parameters: caller cannot know which parameters
# the template is willing to use.
#
sub expandFragment ($$) {
	my ($fragment, $params) = @_;
	my $htempl = HTML::Template->new (
		scalarref => \$fragment,
		die_on_bad_params => 0,
		);
	$htempl->param ($params);
	my $result = $htempl->output();
	return $result;
}


#
# expand -- given an overall plan, return an initrd image specification.
#
sub expand {
	my ($self, $templates) = @_;
	my $image = Image->new();

	for my $a (@{$self->{actions}}) {
		my $action = $a->{action};
		Base::debug ("expanding '$action'");
		my $template = $templates->{$action};
		if (! defined ($template)) {
			Base::fatal ("no template for $action defined");
		}

		for my $directive (@{$template->{directives}}) {
			my $type = $directive->{type};
			my $origin = $directive->{origin};
			Base::debug ("applying $type, $origin");

			if ($type eq "file") {
				my $fileTempl = $directive->{value};
				my $fileName = expandFragment ($fileTempl, $a);
				$image->addFile ($fileName, $origin);
			}
			elsif ($type eq "directory") {
				my $dirTempl = $directive->{value};
				my $dirName = expandFragment ($dirTempl, $a);
				$image->addDirectory ($dirName, $origin);
			}
			elsif ($type eq "tree") {
				my $treeTempl = $directive->{value};
				my $treeName = expandFragment ($treeTempl, $a);
				$image->addTree ($treeName, $origin);
			}
			elsif ($type eq "script") {
				my $scriptNTempl = $directive->{pathname};
				my $scriptName = expandFragment ($scriptNTempl, $a);
				my $contentTempl = $directive->{value};
				my $content = expandFragment ($contentTempl, $a);
				$image->addScriptLine ($scriptName, $content, $origin);
			}
			else {
				Base::bug ("cannot handle $type at $origin");
			}
		}

	}

	return $image;
}

1;
