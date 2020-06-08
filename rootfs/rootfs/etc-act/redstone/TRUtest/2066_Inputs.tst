#!/bin/sh

#---------------------------------------------------------------------------------------
#  2066_Inputs
#
#  test that both the Input messages are sent out as defined in the LCM.  Note this is
#  simulating the inputs not using an actual input.
#
#  Procedure:
#  1) configure unit to send via packetizer-cams, no force iridium
#  2) configure the inputs to trigger a variety of messages
#  3) give the messages time to send
#  4) toggle to force iridium output 
#  5) send the messages again
#  6) give the messages time to send.
#
# Failure modes:
#  1) Messages fail to arrive at the CAMS end.  This cannot be tested internally.
#
#  Output should warn user to check for messages for the unit at the time the
#  test occurred.
#
#---------------------------------------------------------------------------------------

# TTlog will log to the screen and the TTresult.txt file  
source TTlog.sh   

#TThelper.sh include the get_val, set_val, clear_val functions.
source TThelper.sh

# must define TESTNAME for TTlog.sh to output the script name  
#---------------------------------------------------------------------------------------
TESTNAME="2066_Inputs"
wait_for_power_monitor


stage=`get_val stage` 
write_debug "Running test $TESTNAME, current stage $stage"

# startup stage 
if [ -z "$stage" ]; then  
  set_val stage "SendCell"
  db-config set feature packetizer-cams 1
  db-config set packetizer-cams host testcams3.gpscams.com
  db-config set packetizer-cams port 51001
  db-config set packetizer-cams ForceIridium	Off
  db-config set packetizer-cams IridiumEnable 1
  db-config set packetizer-cams UseCompression 0
  db-config set i2c-gpio-monitor UseDebug On
  db-config set i2c-gpio-monitor ForceVal 63
  db-config set RedStone KeepAwakeMinutes 0             # no delay in shutting down
  db-config set IgnitionMonitor UseIgnitionDebug	1     #allow artificial 'no ignition' condition
  db-config set feature wifi-monitor 0
  db-config set PositionUpdate Time 0                   # Have PositionUpdate generate no messages
  db-config set PositionUpdate Distance 0
  db-config set PositionUpdate Heading 0
  db-config set PositionUpdate IridiumReportTime 0
  db-config set PositionUpdate RealTimeInterval 0
  db-config set MSGPriority	ignition_off	9
  db-config set MSGPriority	ignition_on	9
  db-config set i2c-gpio-monitor input1	1
  db-config set i2c-gpio-monitor input2	1
  db-config set i2c-gpio-monitor input3	1
  db-config set i2c-gpio-monitor input4	1
  db-config set i2c-gpio-monitor input5	1
  db-config set i2c-gpio-monitor input6	1
  db-config set i2c-gpio-monitor inputOffPriority1	9
  db-config set i2c-gpio-monitor inputOffPriority2	9
  db-config set i2c-gpio-monitor inputOffPriority3	9
  db-config set i2c-gpio-monitor inputOffPriority4	9
  db-config set i2c-gpio-monitor inputOffPriority5	9
  db-config set i2c-gpio-monitor inputOffPriority6	9
  db-config set i2c-gpio-monitor inputOffType1	6
  db-config set i2c-gpio-monitor inputOffType2	4
  db-config set i2c-gpio-monitor inputOffType3	8
  db-config set i2c-gpio-monitor inputOffType4	2
  db-config set i2c-gpio-monitor inputOffType5	25
  db-config set i2c-gpio-monitor inputOffType6	42
  db-config set i2c-gpio-monitor inputOnPriority1	20
  db-config set i2c-gpio-monitor inputOnPriority2	20
  db-config set i2c-gpio-monitor inputOnPriority3	20
  db-config set i2c-gpio-monitor inputOnPriority4	20
  db-config set i2c-gpio-monitor inputOnPriority5	20
  db-config set i2c-gpio-monitor inputOnPriority6	20
  db-config set i2c-gpio-monitor inputOnType1	7
  db-config set i2c-gpio-monitor inputOnType2	5
  db-config set i2c-gpio-monitor inputOnType3	10
  db-config set i2c-gpio-monitor inputOnType4	12
  db-config set i2c-gpio-monitor inputOnType5	27
  db-config set i2c-gpio-monitor inputOnType6	43
  rm /mnt/update/database/messages.db
  rm /mnt/update/database/cams.db
  
  reboot
  sleep 10
  db-config set i2c-gpio-monitor ForceVal 62
else
  now=`date`
  
  sleep 1
  db-config set i2c-gpio-monitor ForceVal 63
  sleep 1
  db-config set i2c-gpio-monitor ForceVal 61
  sleep 1
  db-config set i2c-gpio-monitor ForceVal 63
  sleep 1
  db-config set i2c-gpio-monitor ForceVal 59
  sleep 1
  db-config set i2c-gpio-monitor ForceVal 63
  sleep 1
  db-config set i2c-gpio-monitor ForceVal 55
  sleep 1
  db-config set i2c-gpio-monitor ForceVal 63
  sleep 1
  db-config set i2c-gpio-monitor ForceVal 47
  sleep 1
  db-config set i2c-gpio-monitor ForceVal 63
  sleep 1
  db-config set i2c-gpio-monitor ForceVal 31
  sleep 1
  db-config set i2c-gpio-monitor ForceVal 63
  sleep 1
  
  val=`sqlite3 /mnt/update/database/cams.db "Select count(mid) from message_table;"`
  count=0
  
  while [ "$val" != "0" ] && [ "$count" -le "12" ]; 
  do
    count=`expr $count + 1`
    sleep 5
    val=`sqlite3 /mnt/update/database/cams.db "Select count(mid) from message_table;"`
  done
  
  if [ "$count" -gt "12" ]; then
    write_error "Unable to deliver all the messages via cell within 1 minute"
  else
    write_log "Check for the following cell delivered messages - starting after $now"
    write_log "ignition_on\\nignition_off\\nstop_condition\\nstart_condition\\nheartbeat\\nsensor\\nspeed_exceeded"
    write_log "speed_acceptable\\nfall_detected\\nnot_check_in\\naccel_acceptable\\ndeccel_acceptable\\n"
  fi
  write_debug "Sending messages via Iridium"  
  db-config set packetizer-cams ForceIridium	On  # repeat with just iridium messages
  now=`date`

  sleep 10   # time for things to settle down
  db-config set i2c-gpio-monitor ForceVal 62
  sleep 5
  db-config set i2c-gpio-monitor ForceVal 63
  sleep 5
  db-config set i2c-gpio-monitor ForceVal 61
  sleep 5
  db-config set i2c-gpio-monitor ForceVal 63
  sleep 5
  db-config set i2c-gpio-monitor ForceVal 59
  sleep 5
  db-config set i2c-gpio-monitor ForceVal 63
  sleep 5
  db-config set i2c-gpio-monitor ForceVal 55
  sleep 5
  db-config set i2c-gpio-monitor ForceVal 63
  sleep 5
  db-config set i2c-gpio-monitor ForceVal 47
  sleep 5
  db-config set i2c-gpio-monitor ForceVal 63
  sleep 5
  db-config set i2c-gpio-monitor ForceVal 31
  sleep 5
  db-config set i2c-gpio-monitor ForceVal 63
  sleep 10

  # when iridium delivers all the messages there will be 6 left over from the lower priority messages
  
  val=`sqlite3 /mnt/update/database/cams.db "Select count(mid) from message_table;"`
  count=0
  
  while [ "$val" -gt "6" ] && [ "$count" -le "60" ]; 
  do
    count=`expr $count + 1`
    sleep 5
    val=`sqlite3 /mnt/update/database/cams.db "Select count(mid) from message_table;"`
  done
  
  if [ "$count" -gt "60" ]; then
    write_error "Unable to deliver all the messages via Iridium within 5 minutes"
  else
    write_log "Check for the following Iridium delivered messages - starting after $now"
    write_log "ignition_on\\nnstop_condition\\nheartbeat\\nspeed_exceeded\\nfall_detected\\naccel_acceptable\\n"
  fi

  write_log "test completed successfully"
  exit

fi

