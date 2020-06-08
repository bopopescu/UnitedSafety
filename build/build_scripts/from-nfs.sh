#!/bin/bash
# Description: Converts an NFS back into the default rootfs format.

FS=$1
if [ '' = "$FS" ];then
	echo "Usage: $0 <path to default NFS>"
	exit 1
fi

if [ ! -e "$FS" ];then
	echo "\"$FS\" does not exist"
	exit 1
fi

if [ -e "$FS/etc-def" -a -e "$FS/mnt-def" -a -e "$FS/etc" -a -e "$FS/mnt" ];then

	echo "$FS" | grep "^/srv" > /dev/null
	if [ 0 != "$?" ];then
		echo "NFS is not in the expected directory (should be in \"/srv\", but is in \"$FS\")"
		exit 1
	fi

	sudo mv "$FS/etc" "$FS/etc-act"
	sudo mv "$FS/mnt" "$FS/mnt-act"
	sudo mv "$FS/etc-def" "$FS/etc"
	sudo mv "$FS/mnt-def" "$FS/mnt"

else
	echo "NFS \"$FS\" is not in the expected format, can not convert to default rootfs"
	exit 1
fi

exit 0
