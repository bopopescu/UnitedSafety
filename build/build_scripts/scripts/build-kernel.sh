#!/bin/bash

	if [ '' = "$REDSTONE_BUILD_DIR" ];then
		echo "REDSTONE_BUILD_DIR is not set"
		exit 1
	fi
	if [ ! -e "$REDSTONE_BUILD_DIR" ];then
		echo "Build directory \"$REDSTONE_BUILD_DIR\" does not exist"
		exit 1
	fi

	if [ '' = "$REDSTONE_BUILD_USER" ];then
		echo "REDSTONE_BUILD_USER is not set"
		exit 1
	fi

	. /usr/share/cross-compile/arm-sourcery-glibc/SOURCE_ME

	#=================================================================
	#= i.MX28 Kernel
	#=================================================================
		PREV_DIR=`pwd`
		cd "$REDSTONE_BUILD_DIR/kernel"

		make redstone_defconfig
		ret=$?
		if [ 0 != "$ret" ];then
			echo "[$ret]: make redstone_defconfig failed for i.MX28 Kernel"
			exit 1
		fi

		make -j$TRULINK_JOBS uImage
		ret=$?
		if [ 0 != "$ret" ];then
			echo "[$ret]: \"make\" uImage failed for i.MX28 Kernel"
			exit 1
		fi

		make -j$TRULINK_JOBS modules
		ret=$?
		if [ 0 != "$ret" ];then
			echo "[$ret]: \"make\" modules failed for i.MX28 Kernel"
			exit 1
		fi

		sudo chown -R $REDSTONE_BUILD_USER:admin "$REDSTONE_BUILD_DIR/rootfs-def/lib"
		make modules_install INSTALL_MOD_PATH="$REDSTONE_BUILD_DIR/rootfs-def/"
		ret=$?
		sudo chown -R root:root "$REDSTONE_BUILD_DIR/rootfs-def/lib"
		if [ 0 != "$ret" ];then
			echo "[$ret]: make modules_install failed for i.MX28 Kernel"
			exit 1
		fi

		mkdir -p "$REDSTONE_BUILD_DIR/firmware"
		cp -v arch/arm/boot/uImage "$REDSTONE_BUILD_DIR/firmware/def-uImage-${PROJECT_NAME}"
		cp -v Module.symvers "$REDSTONE_BUILD_DIR/firmware/"
		cp -v System.map "$REDSTONE_BUILD_DIR/firmware/"
		cp -v .config "$REDSTONE_BUILD_DIR/firmware/kernel.config"

		kernel_version=`cat .config|grep "Linux kernel version"|sed 's/.*: //'`
		cd "$PREV_DIR"

		if [ '' != "$kernel_version" ];then
			echo "Correcting base paths in modules.dep..."
			base_dir="/lib/modules/$kernel_version"
			tmp_file="/tmp/modules.dep.tmp.${REDSTONE_BUILD_USER}.$$"
			cat "$REDSTONE_BUILD_DIR/rootfs-def${base_dir}/modules.dep" | ./scripts/depmod.pl "$base_dir" > "$tmp_file"
			sudo mv "$tmp_file" "$REDSTONE_BUILD_DIR/rootfs-def${base_dir}/modules.dep"
			echo "Remove unused module files..."
			sudo rm "$REDSTONE_BUILD_DIR/rootfs-def${base_dir}/build"
			sudo rm "$REDSTONE_BUILD_DIR/rootfs-def${base_dir}/source"
		fi

exit 0
