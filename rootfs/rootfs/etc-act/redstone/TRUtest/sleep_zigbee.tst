#!/bin/sh

#---------------------------------------------------------------------------------------
#  sleep_zigbee
#
#  test that a unit will stay awake when a zigbee (SLP) connects to the system
#
#  Procedure:
#	1) run the zigbee simulator
#	2) trigger a simulated access
#	3) 
#	4) look for work in the power monitor output
# 5) wait for work to expire
# 6) wait for unit to go to sleep
#
# Failure modes:
#	1) Unit does not generate a webAccess work field when access is simulated
#	2) Unit does not go to sleep after the webAccess expires 
#
#---------------------------------------------------------------------------------------


# TTlog will log to the screen and the TTresult.txt file
source TTlog.sh

#TThelper.sh include the get_val, set_val, clear_val functions.
source TThelper.sh

# must define TESTNAME for TTlog.sh to output the script name
#---------------------------------------------------------------------------------------
TESTNAME="SLEEP_ZIGBEE"

stage=`get_val stage`

if [ -z "$stage" ]; then
	write_log "Starting"
else
	write_log "Running Stage $stage"
fi

# startup stage
if [ -z "$stage" ]; then
  wait_for_power_monitor
  db-config set feature zigbee-monitor 1
  db-config set zigbee AliveTimeoutMinutes 1 
  set_val stage Runzigbee_$TESTNAME
  sleep 5
  ForceReboot
elif [ "$stage" == "Runzigbee_$TESTNAME" ]; then
  wait_for_power_monitor

  /usr/bin/fob-sim &
  sleep 60;
  write_command pstate
  pstate | grep -e "SLP:" > /dev/null

  if [ "$?" -ne "0" ]; then
    write_error "No SLP work generated in power-monitor.  zigbee access is not keeping the system awake!"
    exit
  fi
  write_debug "SLP work found - waiting for it to expire"

  killall fob-sim
  
  sleep 65   # wait for message to expire

  pstate | grep -e "SLP:" > /dev/null

  if [ "$?" -eq "0" ]; then
    write_error "SLP work still exists in power-monitor.  It should have expired by now.Check power monitor state"
    exit
  fi
  write_debug "SLP work has expired"
  
  write_log "test completed succesfully."
fi


