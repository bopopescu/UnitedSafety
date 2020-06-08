#!/bin/sh
#Write resolv.conf file
cp -f /etc/redstone/resolv.conf /tmp/config/resolv.conf

#Setup routes
route del default
route -n |grep ppp0
if [ $? -ne 0 ];then
	route add -net 0.0.0.0 netmask 0.0.0.0 dev ppp0
fi
