#!/bin/sh

#---------------------------------------------------------------------------------------
#  2071_MsgTimeouts
#
#  test that confirms that message priority affects shutdown properly.  
#  Unit should not shut down if a priority 1 message is in the queue.
#  Units should try priority 2-9 messages for 15 minutes beofre shutting down.
#  Units should shut down if there are unsent messages > priority 11
#
#  Procedure:
#  1) configure unit for debug shutdown (wakeup, IgnitionMonitor, etc)
#  2) inject a message into the system at priority 20
#  3) trigger IgnitionOff
#  4) Watch for sleep
#  5) repeat for priority 9 message
#  6) repeat for priority 1 message
#
# Failure modes:
#  1) Unit does not go to sleep on priority 20 message
#  2) Unit goes to sleep too fast on priority 9 message
#  3) Unit does not go to sleep after 15 minutes on priority 9 message 
#  4) Unit goes to sleep on priority 1 message
#  
#---------------------------------------------------------------------------------------

# TTlog will log to the screen and the TTresult.txt file  
source TTlog.sh   

#TThelper.sh include the get_val, set_val, clear_val functions.
source TThelper.sh

# must define TESTNAME for TTlog.sh to output the script name  
#---------------------------------------------------------------------------------------
TESTNAME="2071_MsgTimeouts"
wait_for_power_monitor


stage=`get_val stage` 
write_debug "Running test $TESTNAME, current stage $stage"

# startup stage 
if [ -z "$stage" ]; then  
  db-config set RedStone KeepAwakeMinutes 0             # no delay in shutting down
  db-config set IgnitionMonitor UseIgnitionDebug	1     #allow artificial 'no ignition' condition
  db-config set feature wifi-monitor 0
  db-config set PositionUpdate Time 0                   # Have PositionUpdate generate no messages
  db-config set PositionUpdate Distance 0
  db-config set PositionUpdate Heading 0
  db-config set PositionUpdate IridiumReportTime 0
  db-config set PositionUpdate RealTimeInterval 0
  db-config set feature packetizer-cams 0
  db-config set feature packetizer-calamp 0
  db-config set feature packetizer 0
  db-config set feature packetizer_dash 0
  db-config set feature packetizer-dash 0
  db-config set MSGPriority	acceleration	20
  db-config set MSGPriority	accept_accel_resumed	20
  db-config set MSGPriority	accept_deccel_resumed	20
  db-config set MSGPriority	calamp_user_msg	20
  db-config set MSGPriority	check_in	20
  db-config set MSGPriority	check_out	20
  db-config set MSGPriority	crit_batt	20
  db-config set MSGPriority	direction_change	20
  db-config set MSGPriority	driver_status	20
  db-config set MSGPriority	engine_off	20
  db-config set MSGPriority	engine_on	20
  db-config set MSGPriority	engine_param_exceed	20
  db-config set MSGPriority	engine_param_normal	20
  db-config set MSGPriority	engine_period_report	20
  db-config set MSGPriority	engine_trouble_code	20
  db-config set MSGPriority	fall_detected	20
  db-config set MSGPriority	fuel_log	20
  db-config set MSGPriority	gpsfix_invalid	20
  db-config set MSGPriority	hard_brake	20
  db-config set MSGPriority	heartbeat	20
  db-config set MSGPriority	help	20
  db-config set MSGPriority	ignition_off	20
  db-config set MSGPriority	ignition_on	20
  db-config set MSGPriority	j1939	20
  db-config set MSGPriority	j1939_fault	20
  db-config set MSGPriority	j1939_status2	20
  db-config set MSGPriority	low_batt	20
  db-config set MSGPriority	not_check_in	2o
  db-config set MSGPriority	odometer_update	20
  db-config set MSGPriority	ok	20
  db-config set MSGPriority	other	20
  db-config set MSGPriority	ping	2o
  db-config set MSGPriority	power_off	20
  db-config set MSGPriority	power_on	20
  db-config set MSGPriority	scheduled_message	20
  db-config set MSGPriority	sensor	20
  db-config set MSGPriority	sos	1
  db-config set MSGPriority	speed_acceptable	20
  db-config set MSGPriority	speed_exceeded	20
  db-config set MSGPriority	start_condition	20
  db-config set MSGPriority	stop_condition	20
  db-config set MSGPriority	switch_int_power	20
  db-config set MSGPriority	switch_wired_power	20
  db-config set MSGPriority	text	20

  set_val stage pri20
  reboot                         #reboot to have changes take affect
  sleep 10
elif [ "$stage" == "pri20" ]; then
  sleep 30   # time for things to settle down
  set_val stage pri9
  echo "msg scheduled_message msg_priority=20\r" | socat - unix-connect:/var/run/redstone/message-assembler
  db-config set IgnitionMonitor ForceIgnition 0  # put the unit to sleep
  sleep 60   # this should allow enough time for a reboot
  write_error "Priority 20 message added to queue but system is still awake after 60 seconds"
  write_command pstate
  exit

elif [ "$stage" == "pri9" ]; then
  sleep 30   # time for things to settle down
  set_val stage pri1
  echo "msg scheduled_message msg_priority=9\r" | socat - unix-connect:/var/run/redstone/message-assembler
  db-config set IgnitionMonitor ForceIgnition 0  # put the unit to sleep
  sleep 960   # this should allow enough time for a reboot
  write_error "System is still awake 16 minutes after Iridium priority message sent (should have shut down by now)"
  write_command pstate
  exit
elif [ "$stage" == "pri1" ]; then
  sleep 30   # time for things to settle down
  set_val stage postpri1
  echo "msg scheduled_message msg_priority=1\r" | socat - unix-connect:/var/run/redstone/message-assembler
  db-config set IgnitionMonitor ForceIgnition 0  # put the unit to sleep
  sleep 960   # this should allow enough time for a reboot
  write_log "test completed successfully"
  exit
elif [ "$stage" == "postpri1" ]; then
  write_error "System shut down while priority 1 message in queue."
  exit
fi

