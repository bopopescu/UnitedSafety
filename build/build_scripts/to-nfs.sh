#!/bin/bash
# Description: Converts a default rootfs to a Network File System.
#	The RedStone device is then able to use the file system via NFS.

FS=$1
if [ '' = "$FS" ];then
	echo "Usage: $0 <path to default rootfs>"
	exit 1
fi

if [ ! -e "$FS" ];then
	echo "\"$FS\" does not exist"
	exit 1
fi

if [ -e "$FS/etc-act" -a -e "$FS/mnt-act" -a -e "$FS/etc" -a -e "$FS/mnt" ];then

	echo "$FS" | grep "^/srv" > /dev/null
	if [ 0 != "$?" ];then
		echo "Default rootfs is not in the expected directory (should be in \"/srv\", but is in \"$FS\")"
		exit 1
	fi

	sudo mv "$FS/etc" "$FS/etc-def"
	sudo mv "$FS/mnt" "$FS/mnt-def"
	sudo mv "$FS/etc-act" "$FS/etc"
	sudo mv "$FS/mnt-act" "$FS/mnt"

else
	echo "Default rootfs \"$FS\" is not in the expected format, can not convert to NFS"
	exit 1
fi

exit 0
