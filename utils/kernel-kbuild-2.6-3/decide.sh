#!/bin/sh

if [ -n "$srctree" ]; then
	if [ -f "$srctree/.config" ]; then
		case "$(grep 'CONFIG_SPARC..=y' .config | cut -d= -f1)" in
			CONFIG_SPARC64) arch=sparc64 ;;
			CONFIG_SPARC32) arch=sparc ;;
		esac
	fi
fi

# Bah
if [ -z "$arch" ]; then	
	arch="$(uname -m)"
fi

case "$arch" in
	sparc | sparc64) ;;
	*)
		echo "You shouldn't be using this program!" >&2
		exit 1
	;;
esac

path=/usr/src/@kbuild@/scripts/mod

program=$(basename $0)

exec "$path/$arch/$program" "$@"
