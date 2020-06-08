#!/bin/sh

#---------------------------------------------------------------------------------------
#  modbus_message_priority
#
#  test that only modbus machine hour can be transmitted over Iridium
#
#  Procedure:
#	1) write confirmation that test worked if only modbus machine hours can be repored to CAMS.
#
# Failure modes:
#	2) No any data from this unit reported to CAMS
#	2) Not only machine hours be transmitted to CAMS
#
#---------------------------------------------------------------------------------------


# TTlog will log to the screen and the TTresult.txt file
source TTlog.sh

#TThelper.sh include the get_val, set_val, clear_val functions.
source TThelper.sh

# must define TESTNAME for TTlog.sh to output the script name
#---------------------------------------------------------------------------------------
TESTNAME="MODBUS_MESSAGE_PRIORITY"

stage=`get_val stage`

write_log "Current Stage $stage"

# startup stage
if [ -z "$stage" ]; then

  set_val stage 'reboot'

  feature unset packetizer-calamps
  feature unset packetizer-iridium
  db-config set feature packetizer-cams 1
  db-config set feature modbus-monitor 1

  db-config set feature iridium-monitor 1
  db-config set modbus protocol rtu
  db-config set modbus baudrate 38400
  db-config set modbus periodic_seconds 60
  db-config set modbus periodic_overiridium_seconds 300
  db-config set modbus q_delay_seconds 60
  db-config set modbus parity N
  db-config set modbus data_bits 8
  db-config set modbus stop_bits 1

  db-config set Iridium byteLimit 20480
  db-config set Iridium LimitiTimePeriod 86500
  db-config set PositionUpdate IridiumReportTime 60
  db-config set packetizer-cams ForceIridium On
  db-config set packetizer-cams host testcams3.gpscams.com
  db-config set packetizer-cams port 51001
  db-config set packetizer-cams UseCompression 0
  db-config set packetizer-cams IridiumEnable 1
  db-config set packetizer-cams IridiumPriorityLevel 9
  db-config set packetizer-cams retry_limit 1
  db-config set packetizer-cams CellFailModeEnable 1
  db-config set packetizer-cams iridium_timeout 60
  db-config set packetizer-cams IridiumDataLimitPriority 1
  db-config set packetizer-cams m_keepalive 60
  db-config set packetizer-cams timeout 60

  db-config set modbus-db template_Murphy --file=/etc/redstone/defaultMurphy.xml
  db-config set modbus-db slave1 Murphy
  db-config set modbus-db slave2 Murphy

  write_log  "$TESTNAME: going to reboot now."

  reboot
  sleep 3
  exit

elif [ "$stage" == "reboot" ]; then
  sleep 120
  GetYNResponse "Modbus Message Priority Test" "Do you see the machine hours showing on CAMS?" "Test Failed"
fi

