#!/bin/bash

APP_NAME='ats-common'

	if [ '' = "$BUILD_DIR" ];then
		echo "BUILD_DIR is not set"
		exit 1
	fi
	if [ ! -e "$BUILD_DIR" ];then
		echo "Build directory \"$BUILD_DIR\" does not exist"
		exit 1
	fi

	if [ '' = "$APP_BUILD_DIR" ];then
		echo "APP_BUILD_DIR is not set"
		exit 1
	fi
	if [ ! -e "$APP_BUILD_DIR" ];then
		echo "Build directory \"$APP_BUILD_DIR\" does not exist"
		exit 1
	fi

	cd "$APP_BUILD_DIR"

	if [ ! -e "./build.ats" ];then
		echo "Standard \"build.ats\" script is missing"
		exit 1
	fi

	./build.ats "$BUILD_DIR/rootfs-def/usr/bin/"
	ret=$?
	if [ 0 != "$ret" ];then
		echo "[$ret]: Failed to run \"build.ats\" for $APP_NAME"
		exit 1
	fi

exit 0

