#!/bin/sh

WORKDIR="/tmp/email/$$"
mkdir -p "$WORKDIR"
MSG="$WORKDIR/msg"

SUBJECT=$1
MSG_BODY=$2
ATTACH=$3

	mkdir -p /tmp/.maildir

	if [ -s /mnt/nvram/rom/UnitID.txt ];then
		UNIT_ID=`cat /mnt/nvram/rom/UnitID.txt|tr -d '\r'|tr -d '\n'`
	else
		UNIT_ID='None'
	fi

	if [ -s /mnt/nvram/rom/imei ];then
		IMEI=`head -n1 /mnt/nvram/rom/imei|tr -d '\r'|tr -d '\n'`
	else

		if [ -s /tmp/config/imei ];then
			IMEI=`head -n1 /tmp/config/imei|tr -d '\r'|tr -d '\n'`
		else
			IMEI='NO_IMEI'
		fi

	fi

	if [ '' = "$SUBJECT" ];then
		logger -s "Missing Email subject"
		rm -rf "$WORKDIR"
		exit 1
	fi

	if [ '' = "$MSG_BODY" ];then
		logger -s "Missing Email body"
		rm -rf "$WORKDIR"
		exit 1
	fi

	echo "From: TRULink <trulink@gps1.com>" > "$MSG"
	echo "MIME-Version: 1.0" >> "$MSG"
	echo "To: admin <trulink@gps1.com>" >> "$MSG"
	echo "Date: "`date` >> "$MSG"
	echo "Subject: [$UNIT_ID,$IMEI] $SUBJECT" >> "$MSG"
	echo >> "$MSG"
	cat "$MSG_BODY" >> "$MSG"
	echo >> "$MSG"

	if [ '' != "$ATTACH" ];then
		mutt -s "[$UNIT_ID,$IMEI] $SUBJECT" -a "$ATTACH" -- "trulink@gps1.com" < "$MSG"
		rm -rf "$WORKDIR"
		exit 0
	fi

	cat "$MSG" | msmtp --file=/etc/msmtp.conf "trulink@gps1.com"

rm -rf "$WORKDIR"
exit 0

