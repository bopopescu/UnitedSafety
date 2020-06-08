#!/bin/bash

PATH_TO_TAR="$1"
if [ '' = "$PATH_TO_TAR" -a -e ./build.pl -a -e ../embedded-applications/mx28/rootfs-install/rootfs-install.tar ];then
	PATH_TO_TAR="../embedded-applications/mx28/rootfs-install/"
	echo "No path to rootfs-install.tar specified, unpacking rootfs-install.tar in \"$PATH_TO_TAR\"..."
fi

if [ '' = "$PATH_TO_TAR" ];then
	logger -s "Usage: $0 <path to rootfs-install in SVN or GIT> [unpack destination path]"
	exit 1
fi

if [ ! -e "$PATH_TO_TAR" ];then
	logger -s "\"$PATH_TO_TAR\" does not exist"
	exit 1
fi

REDSTONE_BUILD_USER="$SUDO_USER"
if [ '' = "$REDSTONE_BUILD_USER" ];then
	REDSTONE_BUILD_USER="$USER"
	if [ '' = "$USER" ];then
		logger -s "REDSTONE_BUILD_USER is not set (SUDO_USER and USER are not set)"
		exit 1
	fi
fi

pid=$$
WORKDIR="/tmp/redstone-$REDSTONE_BUILD_USER/unpack-rootfs/$pid"
mkdir -p "$WORKDIR"

SRCDIR="/srv/rootfs-redstone-install-$REDSTONE_BUILD_USER"
if [ '' != "$2" ];then
	SRCDIR="$2"
fi

if [ "/srv/rootfs-redstone-install-$REDSTONE_BUILD_USER" = "$SRCDIR" ];then
	logger -s "Removing existing \"$SRCDIR\"..."
	sudo rm -rf "$SRCDIR"
else
	if [ -e "$SRCDIR" ];then
		logger -s "Directory \"$SRCDIR\" already exists"
		exit 1
	fi
fi

logger -s "Unpacking \"$SRCDIR\"..."
	cp -v "$PATH_TO_TAR/rootfs-install.tar" "$WORKDIR/"
	sudo mkdir -p "$SRCDIR"
	time sudo tar -C "$SRCDIR/" -xf "$WORKDIR/rootfs-install.tar"
	if [ 0 != "$?" ];then
		logger -s "Failed to unpack rootfs-install.tar"
		exit 1
	fi

	rm -rf "$WORKDIR"

exit 0
