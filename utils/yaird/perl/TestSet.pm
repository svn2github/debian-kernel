#!perl -w
#
# Test -- test routines, mostly printing hw inventory.
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
use Base;
use FsTab;
use ActiveBlockDevTab;
use BlockSpecialFileTab;
use LabeledPartitionTab;
use LvmTab;
use Hardware;
use RaidTab;
use InputTab;
use Image;
use Plan;
use KConfig;

package TestSet;


sub testGetFsTab () {
	print "Entries in fstab:\n";
	for my $entry (@{FsTab->all}) {
		my $str = $entry->string;
		my $cmd = $entry->opts->cmdLineVersion;
		print "\t$str\n";
		print "\t\t$cmd\n";
	}
}


sub testActiveBlockDevs () {
	print "Block devices in /sys:\n";
	for my $abd (@{ActiveBlockDevTab::all()}) {
		my $str = $abd->string;
		print "\t$str\n";
	}
}

sub testActiveBlockDevPartitions () {
	print "Partitions for block devices in /sys:\n";
	for my $abd (@{ActiveBlockDevTab::all()}) {
		my $str = $abd->string;
		my $hp = $abd->hasPartitions() ? "yes" : "no";
		my $partitions = $abd->partitions();
		my $partList = '['
			. join (',', (map {$_->name} @{$partitions})) . ']';
		print "\t$str\n";
		print "\t\tpartitioned:\t$hp\n";
		print "\t\tpartitions:\t$partList\n";
	}
}


sub testBlockSpecialFiles () {
	print "Block special files /dev:\n";
	for my $devno (@{BlockSpecialFileTab::allDevnos()}) {
		my $paths = BlockSpecialFileTab::pathsByDevno ($devno);
		my $list = join (", ", @{$paths});
		print "\t$devno: $list\n";
	}
}


sub testLabeledPartitions () {
	print "Labeled partitions detected:\n";
	for my $lp (@{LabeledPartitionTab::all()}) {
		my $str = $lp->string;
		print "\t$str\n";
	}
}


sub testLvm () {
	print "LVM Logical Volumes:\n";
	for my $lv (@{LvmTab::allLogicalVolumes()}) {
		my $str = $lv->string;
		print "\t$str\n";
	}

	print "LVM Volume Groups:\n";
	for my $vg (@{LvmTab::allVolumeGroups()}) {
		my $str = $vg->string;
		print "\t$str\n";
	}

	print "LVM Physical Volumes:\n";
	for my $pv (@{LvmTab::allPhysicalVolumes()}) {
		my $str = $pv->string;
		print "\t$str\n";
	}
}


sub testHardware () {
	print "Hardware modules needed for devices in /sys:\n";
	for my $abd (@{ActiveBlockDevTab::all()}) {
		my $str = $abd->string;
		my $hw = $abd->hw;
		next unless defined ($hw);
		print "\t$str\n";
		my $moduleList = Hardware::moduleList ($hw);
		print "\t\t[" . join(',', @{$moduleList}) . "]\n";
	}
}

sub testRaidDevices () {
	print "Raid devices:\n";
	for my $rd (@{RaidTab::all()}) {
		my $str = $rd->string;
		print "\t$str\n";
	}
}

sub testInput () {
	print "Input devices:\n";
	for my $inp (@{InputTab::all()}) {
		my $str = $inp->string;
		print "\t$str\n";
	}
}

sub testInterpretation () {
	print "Interpreted Fstab Entries:\n";
	for my $fse (@{FsTab->all}) {
		my $fseStr = $fse->string;
		my $active = $fse->isRequiredAtBoot ? "yes": "no";
		my ($path, $msg) = $fse->blockDevPath();
		my $lvmStr = "--";
		my $devno = "--";
		my $hardware;
		my $name = "--";
		my $modules;
		if (defined ($path) && -b $path) {
			$devno = Base::devno ($path);
			my $lvmFiles = LvmTab::findPVPathsByLVDevno ($devno);
			if (defined ($lvmFiles)) {
				$lvmStr = '[' . join (',', @{$lvmFiles}) . ']';
			}

			my $abd = ActiveBlockDevTab::findByDevno($devno);
			if (defined ($abd)) {
				$name = $abd->name();
				my $parent = $abd->parent();
				if (defined ($parent)) {
					$hardware = $parent->hw();
				}
				else {
					$hardware = $abd->hw();
				}
			}
		}
		if (defined ($hardware)) {
			my $moduleList = Hardware::moduleList ($hardware);
			$modules = "[" . join(',', @{$moduleList}) . "]";
		}

		$hardware = "--" unless defined ($hardware);
		$modules = "--" unless defined ($modules);
		$path = "--" unless defined ($path);
		print "\t$fseStr\n";
		print "\t\tactive:\t$active\n";
		print "\t\tdevno:\t$devno\n";
		print "\t\tpath:\t$path\n";
		print "\t\tname:\t$name\n";
		print "\t\thw:\t$hardware\n";
		print "\t\tmods:\t$modules\n";
		print "\t\tlvm:\t$lvmStr\n";

		if (defined ($path)) {
			my $abd = ActiveBlockDevTab::findByPath($path);
			if (defined ($abd)) {
				my $list = ActionList->new();
				Plan::addDevicePlan ($list, $abd, []);
				# the output provided by addDevicePlan
				# is enough.
				# print $list->string();
			}
		}
	}
}

sub testKconfig () {
	print "Kernel Builtin Modules:\n";
	for my $module (@{KConfig::allKnownModules()}) {
		my $builtIn = KConfig::isBuiltIn($module) ? "BUILTIN" : "--";
		print "\t$module:\t$builtIn\n";
	}
}

#
# testAll -- run various interesting subroutines
# that may trigger errors.
#
sub testAll ()
{
	testGetFsTab ();
	testBlockSpecialFiles ();
	testLabeledPartitions ();
	testActiveBlockDevs ();
	testActiveBlockDevPartitions ();
	testLvm ();
	testHardware ();
	testRaidDevices();
	testInterpretation ();
	testInput ();
	testKconfig ();
	Base::bye ();
}


1;
