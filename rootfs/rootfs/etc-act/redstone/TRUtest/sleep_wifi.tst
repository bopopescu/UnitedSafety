#!/bin/sh

#---------------------------------------------------------------------------------------
#  sleep_wifi
#
#  test that a unit will stay awake when the LCM accesses the system
#
#  Procedure:
#	1) Set the wakeup expiry time
#	2) restart admin-client
#	3) trigger a simulated access
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
TESTNAME="SLEEP_WIFI"

stage=`get_val stage`

if [ -z "$stage" ]; then
	write_log "Starting"
else
	write_log "Running Stage $stage"
fi

# startup stage
if [ -z "$stage" ]; then
  wait_for_power_monitor
  db-config set admin-client set_work_expire_seconds 30
  set_val stage RunWifi_$TESTNAME
  sleep 5
  ForceReboot
elif [ "$stage" == "RunWifi_$TESTNAME" ]; then
  wait_for_power_monitor
  wait_for_admin_client
  echo -e "phpcmd dbconfigget RedStone Owner" | nc -w 1 localhost 39000
  sleep 5
  write_debug "Looking for webAccess work"

  write_command pstate
  pstate | grep -e "webAccess" > /dev/null

  if [ "$?" -ne "0" ]; then
    write_error "No webAccess work generated in power-monitor.  WiFi access is not keeping the system awake!"
    exit
  fi
  write_debug "webAccess work found - waiting for it to expire"

  sleep 30   # wait for message to expire

  pstate | grep -e "webAccess" > /dev/null

  if [ "$?" -eq "0" ]; then
    write_error "webAccess work still exists in power-monitor.  It should have expired by now.Check power monitor state"
    exit
  fi

  write_debug "webAccess work has expired - now shutting down for wakeup via rtc in 40 seconds"
  
  set_val stage restore_$TESTNAME
  write_log "test completed succesfully."
fi


