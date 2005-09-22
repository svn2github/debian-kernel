Package: kernel-image-M4KERNEL-M4FLAVOUR
Section: base
Architecture: powerpc
Recommends: kernel-headers-M4KERNEL-M4SUBARCH
Depends: mkvmlinuz
Suggests: kernel-doc-M4KERNEL
Provides: kernel-image, kernel-image-M4KERNEL-M4FLAVOUR-chrp, kernel-image-M4KERNEL-M4FLAVOUR-chrp-rs6k, kernel-image-M4KERNEL-M4FLAVOUR-pmac, kernel-image-M4KERNEL-M4FLAVOUR-prep, kernel-modules-M4KERNEL-M4FLAVOUR
Replaces: kernel-image-M4KERNEL-M4FLAVOUR-chrp, kernel-image-M4KERNEL-M4FLAVOUR-chrp-rs6k, kernel-image-M4KERNEL-M4FLAVOUR-pmac, kernel-image-M4KERNEL-M4FLAVOUR-prep, kernel-modules-M4KERNEL-M4FLAVOUR
Conflicts: kernel-image-M4KERNEL-M4FLAVOUR-chrp, kernel-image-M4KERNEL-M4FLAVOUR-chrp-rs6k, kernel-image-M4KERNEL-M4FLAVOUR-pmac, kernel-image-M4KERNEL-M4FLAVOUR-prep, kernel-modules-M4KERNEL-M4FLAVOUR
Description: Linux/PowerPC kernel binary image for the M4FLAVOUR flavour
 This package contains the Linux/PowerPC kernel image, the
 System.map file, and the modules built by the package.
 .
 This version contains the kernel images for multiprocessor PowerPC machines,
 Using from the ppc601 upto the latest G4 class processors.
 .
 Kernel image packages are generally produced using kernel-package,
 and it is suggested that you install that package if you wish to
 create a custom kernel from the sources.  You will also need
 kernel-patch-M4KERNEL-M4SUBARCH

Package: kernel-build-M4KERNEL-M4FLAVOUR
Section: devel
Architecture: powerpc
Depends: kernel-headers-M4KERNEL-M4SUBARCH
Description: build infrastructure for kernel version M4KERNEL-M4FLAVOUR
 .
 Together with kernel-headers-M4KERNEL-M4FLAVOUR, this package provides the
 infrastructure for building additional modules for M4KERNEL-M4FLAVOUR

