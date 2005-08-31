
Package: kernel-build-M4KERNEL-M4FLAVOUR
Architecture: powerpc
Depends: kernel-headers-M4KERNEL-powerpc
Description: build infrastructure for kernel version M4KERNEL-M4FLAVOUR
 .
 Together with kernel-headers-M4KERNEL-powerpc, this package provides the
 infrastructure for building additional modules for M4KERNEL-M4FLAVOUR.

Package: kernel-image-M4KERNEL-M4FLAVOUR
Section: base
Architecture: powerpc
Depends: initrd-tools (>= 0.1.65), module-init-tools, mkvmlinuz
Recommends: hotplug
Provides: kernel-image, kernel-image-2.6
Description: Linux kernel image for M4KERNEL-M4FLAVOUR
 .
 This package contains the Linux kernel for version M4KERNEL-M4FLAVOUR.
