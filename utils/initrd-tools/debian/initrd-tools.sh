#!/bin/sh
#
# initrd-tools.sh	Unmount and free the initrd.
#
# Version:		$Id: initrd-tools.sh,v 1.5 2002/08/18 00:10:30 herbert Exp $
#

. /etc/default/initrd-tools.sh

[ "$KEEPINITRD" = yes ] && exit
command -v blockdev > /dev/null 2>&1 || exit 0

[ -f /proc/mounts ] || {
	mount -n /proc || exit
	trap 'umount -n /proc' EXIT
}

grep -q '^[^ ]* /initrd ' /proc/mounts || exit 0

if [ -c /initrd/dev/.devfsd ]; then
	umount /initrd/dev || exit
fi
umount /initrd || exit

if [ -b /dev/ram0 ]; then
	blockdev --flushbufs /dev/ram0
elif [ -b /dev/rd/0 ]; then
	blockdev --flushbufs /dev/rd/0
else
	echo "freeinitrd.sh: Cannot find initrd device" >&2
	exit 1
fi
