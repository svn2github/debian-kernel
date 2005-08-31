Source: kernel-patch-powerpc-M4KERNEL
Section: devel
Priority: optional
Maintainer: Jens Schmalzing <jensen@debian.org>
Uploaders: Jens Schmalzing <jensen@debian.org>, Sven Luther <luther@debian.org>
Standards-Version: 3.6.1

Package: kernel-image-M4KERNEL-M4FLAVOUR
Architecture: powerpc
Section: base
Provides: kernel-image, kernel-image-2.6
Depends: initrd-tools, coreutils | fileutils (>=4.0)
Description: intermediate Linux kernel binary image for M4KERNEL-M4FLAVOUR
 .
 This package contains the Linux kernel for version M4KERNEL-M4FLAVOUR,
 the corresponding System.map file, and the modules built by the packager.
 .
 It is created during the build of other PowerPC kernel-image packages
 for the sole purpose of being unpacked immediately afterwars and its
 contents redistributed.  Therefore, you should never encounter this
 package in the wild.
