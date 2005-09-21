Package: kernel-patch-M4KERNEL-M4SUBARCH
Architecture: powerpc
Depends: kernel-source-M4KERNEL
Description: Diffs to the kernel source for APUS
 The PowerPC kernel is maintained in a multitude of source trees
 separate from Linus Torvalds' official copy.  This patch is based on
 the Linux/APUS tree from http://linux-apus.sourceforge.net, which in turn
 is based on the BitKeeper tree used by most active PowerPC developers (See
 http://penguinppc.org/dev/kernel.shtml for info).

Package: kernel-headers-M4KERNEL-M4SUBARCH
Section: devel
Architecture: powerpc
Provides: kernel-headers, kernel-headers-2.4
Description: Linux/APUS kernel headers.
 Read /usr/share/doc/kernel-headers-M4KERNEL-M4SUBARCH/debian.README.gz for details.

Package: kernel-image-M4KERNEL-M4SUBARCH
Section: base
Architecture: powerpc
Recommends: kernel-headers-M4KERNEL-M4SUBARCH
Suggests: kernel-doc-M4KERNEL
Provides: kernel-image
Description: Linux/APUS kernel binary image.
 This package contains the Linux/APUS kernel image, the
 System.map file, and the modules built by the package.
 .
 This version is for Amiga PowerUP System (APUS) computers.
 .
 Kernel image packages are generally produced using kernel-package,
 and it is suggested that you install that package if you wish to
 create a custom kernel from the sources.  You will also need
 kernel-patch-M4KERNEL-M4SUBARCH

Package: kernel-build-M4KERNEL-M4SUBARCH
Section: devel
Architecture: powerpc
Depends: kernel-headers-M4KERNEL-M4SUBARCH
Description: build infrastructure for kernel version M4KERNEL-M4SUBARCH
 .
 Together with kernel-headers-M4KERNEL-M4SUBARCH, this package provides the
 infrastructure for building additional modules for M4KERNEL-M4SUBARCH

