#!/bin/sh
. common.sh
	singleton_flag='/tmp/flags/report-mfg-stat'
	mkdir "$singleton_flag"

	if [ 0 != $? ];then
		exit 0
	fi

	wifi_err=/tmp/wifi_err.txt
	station_id=`head -n1 "$station_number_file"|tr -d '\n'|tr -d '\r'`
	i=0

	while true
	do
		des='/tmp/mfg-stat.txt'
		
		# Station
		printf "mfg_station=$station_id\n" > $des

		printf "i=$i\n" >> $des
		i=`expr $i + 1`

		# Report WiFi errors (if there are any).
		if [ -e "$wifi_err" ];then
			printf "report=" >> $des
			cat "$wifi_err"|./url_encode >> $des
			echo >> $des
		fi

		# Serial Number
		printf "sn=" >> $des
		head -n1 /mnt/nvram/rom/sn.txt|tr -d '\n'|tr -d '\r'|./url_encode >> $des
		echo >> $des

		# IMEI
		printf "imei=" >> $des
		head -n1 /tmp/config/imei|tr -d '\n'|tr -d '\r'|./url_encode >> $des
		echo >> $des

		# Uptime
		printf "uptime=" >> $des
		cat /proc/uptime|./url_encode >> $des
		echo >> $des

		# I/O Expander 0x20 >> $des
		printf "io_0x20=" >> $des
		i2cdump -y -r 0-7 0 0x20 2>/dev/null|tail -n1|./url_encode >> $des
		echo >> $des

		# I/O Expander 0x21
		printf "io_0x21=" >> $des
		i2cdump -y -r 0-7 0 0x21 2>/dev/null|tail -n1|./url_encode >> $des
		echo >> $des
		
		# GPS
		printf "gps=" >> $des
		NMEA show=NMEA_Client|./url_encode >> $des
		echo >> $des
		
		# Date
		printf "date=" >> $des
		date|tr -d '\n'|tr -d '\r'|./url_encode >> $des
		echo >> $des

		# HW Info
		printf "hw_info=" >> $des
		cat /dev/redstone-hw-config|./url_encode >> $des
		echo >> $des

		# Version
		printf "version=" >> $des
		cat /version|./url_encode >> $des
		echo >> $des

		info=`cat "$des"|./b64_encode`
		base_url='https://trulink.myabsolutetrac.com/index.php/api/1/trulink/stat?X-API-KEY=287ec790e1ce0861866c05ba918f83b36e83842f'
		./timeout 60 wget -O /tmp/resp.txt -T 3 -t 3 --no-check-certificate "$base_url&info=$info" &>/dev/null
		wget_ret=$?
		cat /tmp/resp.txt | ./proc_resp
		rm -f /tmp/resp.txt

#		if [ $wget_ret != 124 -a $wget_ret != 4 ];then
#			rm -f "$wifi_err"
#			sleep 10
#		else
#
#			if [ ! -e "$wifi_err" ];then
#				echo "i=$i" > "$wifi_err"
#				echo "date=`date|tr -d '\n'`" >> "$wifi_err"
#				echo "route:" >> "$wifi_err"
#				route -n 2>&1 >> "$wifi_err"
#				echo >> "$wifi_err"
#				echo "resolv.conf:" >> "$wifi_err"
#				cat /etc/resolv.conf >> "$wifi_err"
#				echo >> "$wifi_err"
#				printf "err:\nwget timeout, restarting wifi-monitor (work-around)\n" >> "$wifi_err"
#			fi
#
#			killall wifi-monitor
#			sleep 60
#		fi

	done

exit 0
