#!/bin/bash

	if [ '' = "$REDSTONE_BUILD_DIR" ];then
		logger -s "REDSTONE_BUILD_DIR is not set"
		exit 1
	fi
	if [ ! -e "$REDSTONE_BUILD_DIR" ];then
		logger -s "Build directory \"$REDSTONE_BUILD_DIR\" does not exist"
		exit 1
	fi

	if [ '' = "$REDSTONE_BUILD_USER" ];then
		REDSTONE_BUILD_USER="$SUDO_USER"
		if [ '' = "$REDSTONE_BUILD_USER" ];then
			REDSTONE_BUILD_USER="$USER"
			if [ '' = "$REDSTONE_BUILD_USER" ];then
				logger -s "REDSTONE_BUILD_USER is not set (SUDO_USER and USER are not set)"
				exit 1
			fi
		fi
	fi

	. /usr/share/cross-compile/arm-sourcery-glibc/SOURCE_ME

	#=================================================================
	#= i.MX28
	#=================================================================
		cd "$REDSTONE_BUILD_DIR/bootstream"

		cp -v "$REDSTONE_BUILD_DIR/firmware/u-boot" .
		if [ 0 != "$?" ];then
			logger -s "Could not copy u-boot ELF image into bootstream build directory"
			exit 1
		fi

		make -j1 MEM_TYPE=MEM_DDR1 BOARD=iMX28_EVK
		ret=$?
		if [ 0 != "$ret" ];then
			echo "[$ret]: \"make\" failed for bootstream"
			exit 1
		fi

		mkdir -p "$REDSTONE_BUILD_DIR/firmware"
		cp -v imx28_ivt_uboot.sb "$REDSTONE_BUILD_DIR/firmware/"
		if [ 0 != "$?" ];then
			logger -s "Could not copy final Safe-Boot U-Boot image into firmware directory"
			exit 1
		fi

exit 0
