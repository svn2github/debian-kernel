# 
# Makefile.am -- installation and packaging instructions
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

include ../Makefile.incl

sbin_SCRIPTS	= yaird
CLEANFILES	= $(sbin_SCRIPTS) Conf.pm

edit =	sed \
	-e 's!@PERL\@!$(PERL)!' \
	-e 's!@PACKAGE\@!$(PACKAGE)!' \
	-e 's!@VERSION\@!$(VERSION)!' \
	-e 's!@AUXDIR\@!$(auxdir)!' \
	-e 's!@CFGDIR\@!$(configdir)!' \
	-e 's!@PERLDIR\@!$(perldir)!'

yaird:	main.pl
	$(edit) < $< > $@
	chmod ugo+x $@

Conf.pm: Conf.pm.in
	$(edit) < $< > $@

# not compiled, but must go in the tarball.
EXTRA_DIST = \
	main.pl \
	Conf.pm.in

nodist_perl_DATA = \
	Conf.pm

dist_perl_DATA = \
	ActionList.pm \
	ActiveBlockDev.pm \
	ActiveBlockDevTab.pm \
	Base.pm \
	Blacklist.pm \
	BlockSpecialFileTab.pm \
	CryptEntry.pm \
	CryptTab.pm \
	FsEntry.pm \
	FsOpts.pm \
	FsTab.pm \
	Hardware.pm \
	IdeDev.pm \
	Image.pm \
	Input.pm \
	InputTab.pm \
	KConfig.pm \
	LabeledPartition.pm \
	LabeledPartitionTab.pm \
	LogicalVolume.pm \
	LvmTab.pm \
	ModProbe.pm \
	NetDev.pm \
	NetDevTab.pm \
	Obj.pm \
	Opts.pm \
	Pack.pm \
	Parser.pm \
	PciDev.pm \
	PciMapEntry.pm \
	PciTab.pm \
	PhysicalVolume.pm \
	Plan.pm \
	RaidDev.pm \
	RaidTab.pm \
	ScsiDev.pm \
	SharedLibraries.pm \
	TestSet.pm \
	UsbDev.pm \
	UsbMapEntry.pm \
	UsbTab.pm \
	VolumeGroup.pm

