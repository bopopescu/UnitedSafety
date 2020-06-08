#!/bin/bash
	WORKDIR="/tmp/ppni-build/$$"
	rm -rf "$WORKDIR"
	mkdir -p "$WORKDIR"

	. /usr/share/cross-compile/arm-sourcery-glibc/SOURCE_ME
	export KERNEL_DIR=/home/user/ltib/ltib/rpm/BUILD/linux-2.6.35.3
	cd set-gpio
		make clean
		make
		cp -v set-gpio.ko $WORKDIR
	cd ..
	cd set-PD9
		make clean
		make
		cp -v set-PD9.ko $WORKDIR
	cd ..
	cd reset-PD9
		make clean
		make
		cp -v reset-PD9.ko $WORKDIR
	cd ..
	cd radio-gpio
		make clean
		make
		cp -v radio-gpio.ko $WORKDIR
	cd ..
	cd cell-enable
		make clean
		make
		cp -v cell-enable.ko $WORKDIR
	cd ..

	sudo mv -v "$WORKDIR/set-gpio.ko" /srv/rootfs-ppni-def/home/root/Cell/gpios/
	sudo mv -v "$WORKDIR/set-PD9.ko" /srv/rootfs-ppni-def/home/root/Cell/gpios/
	sudo mv -v "$WORKDIR/reset-PD9.ko" /srv/rootfs-ppni-def/home/root/Cell/gpios/
	sudo mv -v "$WORKDIR/radio-gpio.ko" /srv/rootfs-ppni-def/home/root/
	sudo mv -v "$WORKDIR/cell-enable.ko" /srv/rootfs-ppni-def/home/root/

	moddrvpath="modules/"`ls -1 /home/user/ltib/ltib/rootfs/lib/modules/`/kernel/drivers
	drvpath="/home/user/ltib/ltib/rootfs/lib/$moddrvpath"
	sudo cp -av "$drvpath/net/wireless/wl12xx" "/srv/rootfs-ppni-def/lib/$moddrvpath/net/wireless/"
	sudo cp -av "$drvpath/char" "/srv/rootfs-ppni-def/lib/$moddrvpath/"
	sudo cp -av "$drvpath/usb" "/srv/rootfs-ppni-def/lib/$moddrvpath/"

