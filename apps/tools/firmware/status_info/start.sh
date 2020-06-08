#!/bin/sh
mkdir /tmp/MFG-Burn-In
ret=$?
. common.sh

#if [ 0 != $ret ];then
#	info "$$: ($ret) Another MFG-Burn-In process is already running, exiting..."
#	sleep 86400
#	exit 0
#fi

#./system-watchdog&
msg "$$: Starting..."

lock='/tmp/.check-update'
update_flag='/mnt/update/update-ready-to-run'

wait_for_update_lock()
{
	msg "Waiting for update lock..."

	while true
	do
		mkdir "$lock" &> /dev/null

		if [ 0 == $? ];then
			break
		fi

		sleep 1
	done

	msg "Got update lock"
}

release_update_lock()
{
	msg "Releasing update lock..."
	rmdir "$lock"

	if [ 0 == $? ];then
		msg "Released update lock"
	else
		err "Failed to release update lock"
	fi

}

# XXX: This procedure assumes certain things about the operating system and
#	file system. This means that it may not work on some versions of TRULink
#	software.
main()
{
#	msg "$$: starting test..."
#	udevadm settle --timeout=2
#	touch /tmp/.start-redstone
	export HOME='/home/root'
#	export D='/etc/rc.d/init.d'

#	msg 'Starting Syslog...'
#	/etc/rc.d/init.d/syslog start&

#	. '/etc/rc.d/init.d/common'
#	printf "nameserver 8.8.8.8\nnameserver 8.8.4.4\n" > /tmp/config/resolv.conf
#	ifconfig lo up
#	$D/setup-firewall 2>&1 > /dev/null
#	$D/run-db-monitor&

#	wait_for_app 'db-monitor' 41012 24 125000
#	init-wakeup-signals&

#	port-forward&

	# allow SSH over ra0
#	iptables -I INPUT -i ra0 -p tcp --dport 22 -j ACCEPT 

#	echo -n N > /dev/set-gpio # CAN_ON enabled

#	sys_monitor

#	msg 'Starting RTC-Monitor...'
#	printf 'start_logging\r' | socat - unix-connect:/var/run/redstone/rtc-monitor

#	msg 'Starting I2C-GPIO-Monitor...'
#	run-app 'I2C-GPIO-Monitor' i2c-gpio-monitor &> /dev/null &

#	msg 'Starting Modem-Monitor...'
#	run-app 'Modem-Monitor' modem-monitor &> /dev/null &

#	msg 'Starting Network-Monitor...'
#	run-app Ethernet run-ethernet &> /dev/null &

#	msg 'Starting GPS...'
#	run-app SER_GPS run-SER_GPS &> /dev/null &
#	run-app NMEA NMEA &> /dev/null &

	# Configure WiFi
#	msg "$$: `ls -l`"
#	diff ./wifi.db /mnt/update/database/wifi.db > /dev/null

#	if [ 0 != $? ];then
#		cp ./wifi.db /mnt/update/database/
#	fi

#	msg 'Starting WiFi systems (run-softap)...'
#	run-softap &> /dev/null

#	msg 'Starting WiFi-Monitor...'
#	run-app WiFi-Monitor wifi-monitor &> /dev/null &

	./report-mfg-stat.sh&
#	/sbin/getty -L ttyAM0 115200 vt100
}

#	msg "Checking if called by 'start-updated'..."
#	./called-by-start-updated.sh
#	update_mode=$?

#	if [ 1 == $update_mode ];then
#		msg "Running in 'update' mode..."
#		usb_dir=`df|grep -o '/tmp/autorun/dev/sda\([0-9]\+\)\?'`
		export station_number_file='/tmp/station_number.txt'
		expr "`head -n1 /mnt/nvram/rom/sn.txt`" + 51000 > "$station_number_file"

#		if [ ! -e "$station_number_file" ];then
#			msg "$$: Missing station number file: $station_number_file"
#			msg "$$: USB stick is probably not present, meaning this test ends..."
#			msg "$$: `df`"
#			sys_monitor
#			exit 0
#		fi

#		msg "$$: Preparing device to run this test software on next boot..."
#		touch "$update_flag"
		main
		err "$$: \"main\" returned..."
		exit 0
#	fi

#	wait_for_update_lock

#	if [ -e "$update_flag" ];then
#		warn "An update already exists. Will run that update first, then try again..."
#	else
#		pass "Installing test software..."
#		rm -rf /mnt/update/update-fw
#		mkdir /mnt/update/update-fw
#		cp -a * /mnt/update/update-fw
#		touch "$update_flag"
#		pass "Updated device to run test code on next reboot"
#	fi
#
#	release_update_lock

#	msg "Rebooting..."
#	reboot

exit 0
