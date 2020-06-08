#!/bin/bash

	if [ '' = "$M2M_BUILD_DIR" ];then
		echo "M2M_BUILD_DIR is not set"
		exit 1
	fi
	if [ ! -e "$M2M_BUILD_DIR" ];then
		echo "Build directory \"$M2M_BUILD_DIR\" does not exist"
		exit 1
	fi

	. /usr/share/cross-compile/arm-sourcery-glibc/SOURCE_ME

	#=================================================================
	#= Build BSP_WiFi shared object
	#=================================================================
		cd "$M2M_BUILD_DIR/bsp/arm/BSP_WiFi"
		/home/siconix/qt-arm/bin/qmake
		ret=$?
		if [ 0 != "$ret" ];then
			echo "[$ret]: qmake for Arm failed"
			exit 1
		fi

		make clean
		make
		ret=$?
		if [ 0 != "$ret" ];then
			echo "[$ret]: Failed to \"make\" BSP_WiFi"
			exit 1
		fi

		sudo cp -Lv "./bin/libBSP_WiFi.so.1" "$M2M_BUILD_DIR/rootfs-def/usr/lib/"
		sudo cp -Lv "./bin/libBSP_WiFi.so.1" "$M2M_BUILD_DIR/rootfs-def-287/usr/lib/"

	#=================================================================
	#= Build BSP_WiFi application
	#=================================================================
		cd "$M2M_BUILD_DIR/bsp/arm/BSP_WiFi/target"
		/home/siconix/qt-arm/bin/qmake
		ret=$?
		if [ 0 != "$ret" ];then
			echo "[$ret]: qmake for Arm failed"
			exit 1
		fi

		make clean
		make
		ret=$?
		if [ 0 != "$ret" ];then
			echo "[$ret]: Failed to \"make\" BSP_WiFi_app"
			exit 1
		fi

		${CROSS_COMPILE}strip "./bin/BSP_WiFi"
		sudo cp -av "./bin/BSP_WiFi" "$M2M_BUILD_DIR/rootfs-def/usr/bin/"
		sudo cp -av "./bin/BSP_WiFi" "$M2M_BUILD_DIR/rootfs-def-287/usr/bin/"

exit 0
