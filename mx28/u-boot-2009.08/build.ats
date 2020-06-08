#!/bin/bash

# XXX: BUILD_DIR is special to U-Boot, so don't use it.
unset BUILD_DIR

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
	EXTRAVERSION=$SVN_VER
	#=================================================================
	#= i.MX28
	#=================================================================
		cd "$APP_BUILD_DIR"

		make mx28_evk_config
		ret=$?
		if [ 0 != "$ret" ];then
			echo "[$ret]: Failed to configure U-Boot build"
			exit 1
		fi

		make -j$TRULINK_JOBS
		ret=$?
		if [ 0 != "$ret" ];then
			echo "[$ret]: Failed to build U-Boot image"
			exit 1
		fi

		mkdir -p "$REDSTONE_BUILD_DIR/firmware"
		cp -v u-boot "$REDSTONE_BUILD_DIR/firmware/"

exit 0
