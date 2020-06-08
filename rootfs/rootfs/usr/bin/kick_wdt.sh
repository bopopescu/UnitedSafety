#!/bin/sh
while [ 1 ]
do
	echo "1" > /dev/watchdog
	sleep 10
done
