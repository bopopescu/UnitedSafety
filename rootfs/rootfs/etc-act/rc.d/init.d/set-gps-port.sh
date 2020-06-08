#!/bin/sh

rm /dev/ttySER1

var1=$(db-config get RedStone GPS | grep v_Value | grep Telit)

if [ ${#var1} -gt 5 ]; then
  echo "Using Telit modem for GPS"
  ln -s /dev/ttyGPS /dev/ttySER1
fi


var2=$(db-config get RedStone GPS | grep v_Value | grep Ublox)

if [ ${#var2} -gt 5 ]; then
  echo "Using Ublox for GPS"
  ln -s /dev/ttySP3 /dev/ttySER1
fi
