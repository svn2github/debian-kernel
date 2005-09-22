#! /bin/sh
#
# (C) 1998 Manoj Srivastava & Eric Delaunay.

set -e

PATCHNAME=M4SUBARCH
PATCHFILE=debian-M4SUBARCH.diff.gz
ARCHITECTURE=`dpkg --print-installation-architecture`
if [ -n "$PATCH_DIR" -a -f "$PATCH_DIR/$PATCHFILE" ]; then
  PATCHDIR="$PATCH_DIR"
else
  PATCHDIR=/usr/src/kernel-patches/$ARCHITECTURE
fi

if ! test -d kernel -a -d Documentation ; then
    echo "Not in kernel top level directory. Exiting" >&2
    exit 1
fi

if test -f debian/APPLIED_${ARCHITECTURE}_$PATCHNAME ; then
   exit 0		# patch already applied
fi

VERSION=$(grep ^VERSION Makefile 2>/dev/null | \
                 sed -e 's/[^0-9]*\([0-9]*\)/\1/')
PATCHLEVEL=$(grep ^PATCHLEVEL Makefile 2>/dev/null | \
                    sed -e 's/[^0-9]*\([0-9]*\)/\1/')
SUBLEVEL=$(grep ^SUBLEVEL Makefile 2>/dev/null | \
                  sed -e 's/[^0-9]*\([0-9B]*\)/\1/')

PATCH_VERSION=$(patch -v | head -1 | sed -e 's/[^0-9\.]//g')
PATCH_OPTIONS="-l -s -p1"

rm -f include/asm
zcat $PATCHDIR/$PATCHFILE | patch $PATCH_OPTIONS

# Work around bug in kernel-package
echo stamp > stamp-patch

mkdir -p debian && echo stamp >> debian/APPLIED_${ARCHITECTURE}_$PATCHNAME
exit 0
