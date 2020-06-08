#!/bin/sh

DEV_LOC=/sys/devices/platform/FlexCAN.0

echo 45 > $DEV_LOC/br_presdiv
echo 6  > $DEV_LOC/br_propseg
echo 7  > $DEV_LOC/br_pseg1
echo 2  > $DEV_LOC/br_pseg2

echo "Bitrate set:"`cat $DEV_LOC/bitrate`

ifconfig can0 up
