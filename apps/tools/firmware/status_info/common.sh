#!/bin/sh
CONSOLE='/dev/ttyAM0'

INFO_MSG='\x1b[1;44;33m'
LOG_MSG='\x1b[1;44;37m'
WARN_MSG='\x1b[0;43;30m'
PASS_MSG='\x1b[1;42;37m'
ERR_MSG='\x1b[1;41;37m'
END_MSG='\x1b[0m'

pass()
{
	logger "$0: [PASS] $@"
	printf "${PASS_MSG}$@${END_MSG}\n" > $CONSOLE
}

info()
{
	logger "$0: [INFO] $@"
	printf "${INFO_MSG}$@${END_MSG}\n" > $CONSOLE
}

warn()
{
	logger "$0: [WARN] $@"
	printf "${WARN_MSG}$@${END_MSG}\n" > $CONSOLE
}

msg()
{
	logger "$0: [MSG] $@"
	printf "${LOG_MSG}$@${END_MSG}\n" > $CONSOLE
}

err()
{
	logger "$0: [ERR] $@"
	printf "${ERR_MSG}$@${END_MSG}\n" > $CONSOLE
}

prepare_partition()
{
	part_desc=$1
	part_number=$2
	part_label=$3
	mount_point=$4

	if [ '' == "$mount_point" ];then
		err "Mount point not specified"
		return 1
	fi

	if [ ! "$part_number" -gt "3" ];then
		err "Partition number ($part_number) is not greater than 3"
		return 1
	fi

	part_name="ubi$part_number"
	part_ubi_node="ubi${part_number}_0"
	msg "Checking if $part_desc partition ($part_name) is mounted..."
	m=`mount | grep "^$part_ubi_node "`

	if [ 0 == $? ];then
		mount_dir=`printf '%s' "$m" | sed "s/^$part_ubi_node on //" | sed 's/ type ubifs .*//'`
		msg "$part_desc partition ($part_name) is mounted on \"$mount_dir\", unmounting..."
		umount "$mount_dir"

		if [ 0 != $? ];then
			err "Failed to unmount $part_desc partition mounted on \"$mount_dir\""
			return 1
		fi

	fi

	if [ ! -e '/usr/bin/ubinfo' ];then
		warn "No \"ubinfo\" application (cannot check ubi partition attach state), assuming $part_desc partition ($part_name) is not attached"
	else
		msg "Checking if $part_desc partition ($part_name) is attached..."
		ubinfo | grep "^Present UBI devices:.*$part_name\([^0-9]\|\$\)" &> /dev/null

		if [ 0 == $? ];then
			msg "$part_desc partition ($part_name) is attached, detaching..."
			ubidetach /dev/ubi_ctrl -d $part_number

			if [ 0 != $? ];then
				err "Failed to detach $part_desc partition $part_name"
				return 1
			fi

		fi

	fi

	msg "Erasing $part_desc partition (mtd$part_number)..."
	flash_eraseall /dev/mtd$part_number

	msg "Attaching $part_desc partition ($part_name)..."
	ubiattach /dev/ubi_ctrl -m $part_number -d $part_number

	if [ 0 != $? ];then
		err "Failed to attach $part_desc partition ($part_name)"
		return 1
	fi

	msg "Formatting update partition ($part_name)..."
	ubimkvol /dev/$part_name -n 0 -N "$part_label" -m

	if [ 0 != $? ];then
		err "Failed to format $part_desc partition ($part_name)"
		return 1
	fi

	msg "Mounting update partition ($part_name)..."
	mkdir -p "$mount_point"
	mount -t ubifs "$part_ubi_node" "$mount_point"

	if [ 0 != $? ];then
		err "Failed to mount $part_desc partition ($part_name)"
		return 1
	fi

	return 0
}
