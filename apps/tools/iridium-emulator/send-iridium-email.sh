#!/bin/sh

WORKDIR="/tmp/email/$$"
mkdir -p "$WORKDIR"
MSG="$WORKDIR/msg"

if [ '' = "$RECIPIENT" ];then
	echo "RECIPIENT is not set" >&2
	exit 1
fi

SUBJECT=$1
MSG_BODY=$2
ATTACH=$3

	mkdir -p /tmp/.maildir

	if [ -s /mnt/nvram/rom/UnitID.txt ];then
		UNIT_ID=`cat /mnt/nvram/rom/UnitID.txt|tr -d '\r'|tr -d '\n'`
	else
		UNIT_ID='None'
	fi

	if [ -s /mnt/nvram/rom/imei.txt ];then
		IMEI=`head -n1 /mnt/nvram/rom/imei.txt|tr -d '\r'|tr -d '\n'`
	else

		if [ -s /tmp/config/imei.txt ];then
			IMEI=`head -n1 /tmp/config/imei.txt|tr -d '\r'|tr -d '\n'`
		else
			IMEI='NO_IMEI'
		fi

	fi

	if [ '' = "$SUBJECT" ];then
		echo "Missing Email subject" >&2
		rm -rf "$WORKDIR"
		exit 1
	fi

	if [ '' = "$MSG_BODY" ];then
		"Missing Email body" >&2
		rm -rf "$WORKDIR"
		exit 1
	fi

	echo "From: sbdservicer@sbd.iridium.com" > "$MSG"

	sent_date=`date +"%B %d, %Y %r"|sed 's/ 0/ /g'|sed 's/:[0-9][0-9] / /'`
	echo "Sent: $sent_date" >> "$MSG"
	echo "To: iridium" >> "$MSG"
	echo "Subject: $SUBJECT" >> "$MSG"
	echo >> "$MSG"
	cat "$MSG_BODY" >> "$MSG"
	echo >> "$MSG"

	if [ '' != "$ATTACH" ];then
		mutt -s "$SUBJECT" -a "$ATTACH" -- "$RECIPIENT" < "$MSG"
		rm -rf "$WORKDIR"
		exit 0
	fi

	cat "$MSG" | msmtp --file=/etc/msmtp.conf "$RECIPIENT"

rm -rf "$WORKDIR"
exit 0
