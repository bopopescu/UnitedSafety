#!/bin/sh

modprobe g_ether
ifconfig usb0 192.168.111.1
/etc/rc.d/init.d/sshd start

