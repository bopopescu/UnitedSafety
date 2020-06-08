#!/bin/sh

#---------------------------------------------------------------------------------------
#  2057_IridiumOnOff
#
#  test that both the On and the Off messages go out the Iridium.
#
#  Procedure:
#  1) configure unit to send via packetizer-cams, force iridium
#  2) Override Ignition On/Off in Ignition monitor
#  3) reboot
#  4) wait 1 minute
#  5) trigger ignition off
#  6) on reboot on and off messages should be in CAMS
#
# Failure modes:
#  1) None
#
#  Output should warn user to check for on and off messages for the unit at the time the
#  test occurred.
#
#---------------------------------------------------------------------------------------

# TTlog will log to the screen and the TTresult.txt file  
source TTlog.sh   

#TThelper.sh include the get_val, set_val, clear_val functions.
source TThelper.sh

# must define TESTNAME for TTlog.sh to output the script name  
#---------------------------------------------------------------------------------------
TESTNAME="2057_IridiumOnOff"
wait_for_power_monitor


stage=`get_val stage` 

if [ -z "$stage" ]; then
	write_log "Starting"
else
	write_log "Running Stage $stage"
fi

# startup stage 
if [ -z "$stage" ]; then  
  db-config set feature packetizer-cams 1
  db-config set packetizer-cams host testcams3.gpscams.com
  db-config set packetizer-cams port 51001
  db-config set packetizer-cams ForceIridium	On
  db-config set packetizer-cams IridiumEnable 1
  db-config set RedStone KeepAwakeMinutes 0             # no delay in shutting down
  db-config set IgnitionMonitor UseIgnitionDebug	1     #allow artificial 'no ignition' condition
  db-config set feature wifi-monitor 0
  db-config set PositionUpdate Time 0                   # Have PositionUpdate generate no messages
  db-config set PositionUpdate Distance 0
  db-config set PositionUpdate Heading 0
  db-config set PositionUpdate IridiumReportTime 0
  db-config set PositionUpdate RealTimeInterval 0
  db-config set MSGPriority	ignition_off	2
  db-config set MSGPriority	ignition_on	2
  set_val stage SendViaIridium
  rm /mnt/update/database/messages.db       # start with a clean message database
  reboot                         #reboot to have changes take affect
  sleep 10
elif [ "$stage" == "SendViaIridium" ]; then
  sleep 30   # time for things to settle down
  set_val stage CheckForMessages
  mid=`sqlite3 /mnt/update/database/messages.db "select mid from message_table where event_type = 6;"`
  isSending=1
  count=0
  
  while [ "$isSending" == "1" ]; do
    write_command pstate
    write_debug "In sending ON loop - count is $count" 

    pstate | grep -e "Message_$mid," > /dev/null

    if [ "$?" -ne "0" ]; then
      isSending=0
    else
      count=`expr $count + 1`
	    
      if [ "$count" -gt "5" ]; then
        write_debug "On message was in queue - it is now being sent"
        db-config set packetizer-cams ForceIridium	Off    # allow the message to be sent
      fi
      sleep 10
      if [ "$count" -gt "10" ]; then
        write_error "Unable to send message check log files in /mnt/update/TRUtest/logs"
        mkdir /mnt/update/TRUtest/logs
        cp -r /var/log/* /mnt/update/TRUtest/logs 
        exit
      fi
    fi
  done
  
  db-config set packetizer-cams ForceIridium	On
  db-config set IgnitionMonitor ForceIgnition 0  # put the unit to sleep
  write_debug "Unit has been forced to Ignition Off"
  # now make sure that an Ignition Off message was generated
  mid=`sqlite3 /mnt/update/database/messages.db "select mid from message_table where event_type = 7;"`

  while [ -z "$mid"]; do
    mid=`sqlite3 /mnt/update/database/messages.db "select mid from message_table where event_type = 7;"`
  done

  isSending=1
  count=0
  
  while [ "$isSending" == "1" ]; do
    write_debug "In sending OFF loop - count is $count" 

    pstate | grep "Message_$mid," > /dev/null
    
    if [ "$?" -ne "0" ]; then
      isSending=0
    else
      count=`expr $count + 1`
	    
      if [ "$count" -gt "5" ]; then
        write_debug "Off message was in queue - it is now being sent"
        db-config set packetizer-cams ForceIridium	Off    # allow the message to be sent
      fi
      if [ "$count" -gt "10" ]; then
        write_error "Unable to send message check log files in /mnt/update/TRUtest/logs"
        mkdir /mnt/update/TRUtest/logs
        cp -r /var/log/* /mnt/update/TRUtest/logs 
        exit
      fi
      sleep 10
    fi
  done

  write_log "Check for ignition on and ignition off messages in CAMS"
  write_log "test completed successfully"
  exit

fi

