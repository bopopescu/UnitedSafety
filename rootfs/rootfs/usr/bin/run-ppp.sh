#!/bin/sh

while [ ! -e /dev/ttyModem ]
do
	sleep 1
done

count=`ps | grep connect-ppp | grep -v grep | wc -l`

if [ "$count" -eq "0" ]; then
	/usr/bin/connect-ppp
fi

