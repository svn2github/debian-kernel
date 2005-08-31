Source: kernel-source-nonfree-M4UPSTREAM
Section: non-free/devel
Priority: optional
Maintainer: Debian Kernel Team <debian-kernel@lists.debian.org>
Uploaders: Andres Salomon <dilinger@debian.org>
Standards-Version: 3.6.1
Build-Depends: bzip2, cdbs, debhelper (>> 4.1.0), fakeroot, module-assistant M4BUILDDEP

Package: kernel-nonfree-source
Architecture: all
Depends: module-assistant, bzip2, coreutils | fileutils (>= 4.0), debhelper (>> 4.1.0), m4
Description: Linux kernel module source for non-free M4UPSTREAM drivers
 This package provides the source code for the Linux kernel modules
 contained in M4UPSTREAM that are considered non-free by Debian.  Some of
 the modules contained within this package include acenic, tg3, keyspan, and
 the various qla2xxx modules.

