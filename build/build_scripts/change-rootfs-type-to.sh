#!/bin/bash

if [ '' = "$M2M_BUILD_USER" ];then
	M2M_BUILD_USER="$SUDO_USER"
	if [ '' = "$M2M_BUILD_USER" ];then
		M2M_BUILD_USER="$USER"
		if [ '' = "$M2M_BUILD_USER" ];then
			echo "M2M_BUILD_USER is not set (SUDO_USER and USER are not set)"
			exit 1
		fi
	fi
fi
logger -s "Running as \"$M2M_BUILD_USER\"..."

ACTDIR="/srv/rootfs-m2m-act-$M2M_BUILD_USER"
DEFDIR="/srv/rootfs-m2m-def-$M2M_BUILD_USER"

# Change to active rootfs
	if [ "act" = "$1" ];then
		if [ ! -e "$DEFDIR" ];then
			echo "Error: Default rootfs \"$DEFDIR\" does not exist"
			exit 1
		fi
		if [ -e "$ACTDIR" ];then
			echo "Error: Active rootfs \"$ACTDIR\" already exists"
			exit 1
		fi
		sudo cp -av "$DEFDIR" "$ACTDIR"
		sudo mv "$ACTDIR/etc" "$ACTDIR/etc-def"
		sudo mv "$ACTDIR/mnt" "$ACTDIR/mnt-def"

		sudo mv "$ACTDIR/etc-act" "$ACTDIR/etc"
		sudo mv "$ACTDIR/mnt-act" "$ACTDIR/mnt"
		exit 0
	fi

# Change to default rootfs
	if [ "def" = "$1" ];then
		if [ ! -e "$ACTDIR" ];then
			echo "Active rootfs \"$ACTDIR\" does not exist"
			exit 1
		fi
		if [ -e "$DEFDIR" ];then
			echo "Error: Default rootfs \"$DEFDIR\" already exists"
			exit 1
		fi
		sudo cp -av "$ACTDIR" "$DEFDIR"
		sudo mv "$DEFDIR/etc" "$DEFDIR/etc-act"
		sudo mv "$DEFDIR/mnt" "$DEFDIR/mnt-act"

		sudo mv "$DEFDIR/etc-def" "$DEFDIR/etc"
		sudo mv "$DEFDIR/mnt-def" "$DEFDIR/mnt"
		exit 0
	fi

echo "Usage: $0 <act/def>"
exit 1
