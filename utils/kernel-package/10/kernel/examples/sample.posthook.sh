#!/bin/sh -
#                               -*- Mode: Sh -*- 
# example.posthook.sh --- 
# Author           : Manoj Srivastava ( srivasta@glaurung.green-gryphon.com ) 
# Created On       : Mon Aug 11 14:00:58 2003
# Created On Node  : glaurung.green-gryphon.com
# Last Modified By : Manoj Srivastava
# Last Modified On : Mon Aug 11 14:29:49 2003
# Last Machine Used: glaurung.green-gryphon.com
# Update Count     : 9
# Status           : Unknown, Use with caution!
# HISTORY          : 
# Description      : 
# 
# 
# This is an example of a script that can be run as a postinst hook,
# and manages the symbolic links in a manner similar to the kernel
# image defaul behaviour, except that the latest ttwo version (as
# determined by ls -lt) are kept. You can modify this script 
# 

set -e

# The dir where symlinks are managed
SYMLINKDIR=/

if [ $# -ne 2 ]; then
    echo Usage: $0 version location
    exit 2
fi

version="$1"
vmlinuz_location="$2"


cd $SYMLINKDIR || exit 1

rm -f vmlinuz vmlinuz.old vmlinuz-rd vmlinuz-rd.old initrd.img initrd.img.old 

# Create a temporary file safely
if [ -x /bin/tempfile ]; then
    outfile=$(tempfile -p outp -m 0600);
else
    set -e
    mkdir /tmp/kernel-image-$version-$$
    outfile=/tmp/kernel-image-$version-$$/output
fi

ls -t vmlinuz-*   > $outfile

STD="$(head -n 1 $outfile |             sed 's/vmlinuz-//')" 
OLD="$(head -n 2 $outfile | tail -n 1 | sed 's/vmlinuz-//')" 

if [ "X$STD" = "X" ]; then
    exit 0;
fi

# If you want version specific links, here's how to start
STD24="$(grep vmlinuz-2.4 $outfile | head -n 1 | sed 's/vmlinuz-//')" || true
OLD24="$(grep vmlinuz-2.4 $outfile | head -n 1 | tail -n 1 | sed 's/vmlinuz-//')" || true

STD25="$(grep vmlinuz-2.5 $outfile | head -n 1 | sed 's/vmlinuz-//')" || true
OLD25="$(grep vmlinuz-2.5 $outfile | head -n 1 | tail -n 1 | sed 's/vmlinuz-//')" || true

echo Booting $STD, old is $OLD

if [ -f initrd.img-$STD ] ; then 
   ln -s initrd.img-$STD initrd.img
   ln -s vmlinuz-$STD vmlinuz-rd
else
   ln -s vmlinuz-$STD vmlinuz
fi

if [ "X$OLD" != "X" ]; then
    if [ -f initrd.img-$OLD ] ; then
	ln -s initrd.img-$OLD initrd.img.old
	ln -s vmlinuz-$OLD vmlinuz-rd.old
    else
	ln -s vmlinuz-$OLD vmlinuz.old
    fi
fi

# if [ "X$STD24" != "X" ]; then
#     if [ -f initrd.img-$STD24 ] ; then 
# 	ln -s initrd.img-$STD24 initrd24.img
# 	ln -s vmlinuz-$STD24 vmlinuz24-rd
#     else
# 	ln -s vmlinuz-$STD24 vmlinuz24
#     fi
# fi
# if [ "X$OLD24" != "X" ]; then
#     if [ -f initrd.img-$OLD24 ] ; then
# 	ln -s initrd.img-$OLD24 initrd24.img.old
# 	ln -s vmlinuz-$OLD vmlinuz24-rd.old
#     else
# 	ln -s vmlinuz-$OLD vmlinuz24.old
#     fi
# fi

lilo

rm -f $outfile 
if [ -d /tmp/kernel-image-$version-$$ ]; then
    rmdir /tmp/kernel-image-$version-$$
fi

exit 0
