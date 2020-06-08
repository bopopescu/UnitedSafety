#!/bin/bash

function determine_tar_file
{
	TAR_FILE=$PATH_TO_TAR

	echo "$TAR_FILE" | grep "\.tar$" &> /dev/null

	if [ 0 != "$?" ];then
		TAR_FILE="$PATH_TO_TAR/rootfs-def.tar"
	fi
}

PATH_TO_TAR="$1"
if [ '' = "$PATH_TO_TAR" -a -e ./build.pl -a -e ../embedded-applications/mx28/rootfs/rootfs-def.tar ];then
	PATH_TO_TAR="../embedded-applications/mx28/rootfs/"
	echo "No path to rootfs.tar specified, unpacking rootfs-def.tar in \"$PATH_TO_TAR\"..."
fi

if [ '' = "$PATH_TO_TAR" ];then
	logger -s "Usage: $0 <path to rootfs in SVN or GIT> [unpack destination path]"
	exit 1
fi

if [ ! -e "$PATH_TO_TAR" ];then
	logger -s "\"$PATH_TO_TAR\" does not exist"
	exit 1
fi

if [ '' = "$M2M_BUILD_USER" ];then
	logger -s "M2M_BUILD_USER is not set, trying SUDO_USER..."
	M2M_BUILD_USER="$SUDO_USER"
	if [ '' = "$M2M_BUILD_USER" ];then
		logger -s "M2M_BUILD_USER is not set, trying USER..."
		M2M_BUILD_USER="$USER"
		if [ '' = "$M2M_BUILD_USER" ];then
			logger -s "M2M_BUILD_USER is not set (SUDO_USER and USER are not set)"
			exit 1
		fi
	fi
fi
logger -s "Running as \"$M2M_BUILD_USER\"..."

pid=$$
WORKDIR="/tmp/redstone-${M2M_BUILD_USER}/unpack-rootfs/$pid"
mkdir -p "$WORKDIR"

DESDIR="/srv/rootfs-redstone-def-$M2M_BUILD_USER"
if [ '' != "$2" ];then
	DESDIR="$2"
fi

if [ "/srv/rootfs-redstone-def-$M2M_BUILD_USER" = "$DESDIR" ];then
	logger -s "Removing existing \"$DESDIR\"..."
		sudo rm -rf "$DESDIR"
else
	if [ -e "$DESDIR" ];then
		logger -s "Directory \"$DESDIR\" already exists"
		exit 1
	fi
fi

determine_tar_file

logger -s "Unpacking into \"$DESDIR\"..."
	cp -v "$TAR_FILE" "$WORKDIR/rootfs.tar"
	sudo mkdir -p "$DESDIR"
	time sudo tar -C "$DESDIR/" -xf "$WORKDIR/rootfs.tar"
	if [ 0 != "$?" ];then
		logger -s "Failed to unpack \"$TAR_FILE\""
		exit 1
	fi

	rm -rf "$WORKDIR"

exit 0
