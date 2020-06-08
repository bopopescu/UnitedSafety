#!/bin/sh

# Put device into Test-Mode (this disables the DHCP server on the device, and allows
# connection to the MFG Test Server).
	touch /mnt/nvram/config/testmode
	touch /mnt/nvram/config/testmode-mtu
	feature unset can-odb2-monitor

# Disable NMEA to prevent rebooting every 15 minutes
	feature unset NMEA

# Prevent Sleep
	db-config set RedStone IgnitionSource POWER

# Setup Zigbee
	db-config set feature zigbee-monitor 1
	zigbee-monitor&
	sleep 3
	echo  -e "linkkey key=11111111111111111111111111111111" | socat  -t 2 - unix-connect:/var/run/redstone/zigbee-monitor
	sleep 1

# Reboot the device
	sync
	reboot

exit 0
