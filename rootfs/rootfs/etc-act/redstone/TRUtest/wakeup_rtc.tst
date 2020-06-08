#!/bin/sh

#---------------------------------------------------------------------------------------
#  wakeup_rtc
#
#  test that a unit will wakeup on the RTC heartbeat
#
#  Procedure:
#	1) set wakeups so that only RTC is a wakeup
#	2) reboot
#	3) trigger sleep
#	4) write confirmation that test worked if the unit came up on time
#
# Failure modes:
#	1) Unit does not go to sleep
#	2) Unit does not wake up with RTC 
#
#---------------------------------------------------------------------------------------


# TTlog will log to the screen and the TTresult.txt file
source TTlog.sh

#TThelper.sh include the get_val, set_val, clear_val functions.
source TThelper.sh

# must define TESTNAME for TTlog.sh to output the script name
#---------------------------------------------------------------------------------------
TESTNAME="WAKEUP_RTC"

stage=`get_val stage`

write_log "Current Stage $stage"

# startup stage
if [ -z "$stage" ]; then
  wait_for_power_monitor

  db-config set wakeup mask rtc,~accel,~inp1,~inp2,~inp3,~can,~batt_volt,low_batt
  set-wakeup-mask
  db-config set heartbeat m_state_1_timeout_seconds 120
  killall heartbeat
  sleep 5

  set_val stage awake_$TESTNAME
  write_log  "$TESTNAME: going to sleep now."
  echo -e "PowerMonitorStateMachine shutdown force=true" | nc -w 1 localhost 41009
  sleep 30
  write_error "forced shutdown did not work!"
  write_error "system is still awake after 30 seconds - check power-monitor (is it running?)"
  exit
elif [ "$stage" == "awake_$TESTNAME" ]; then
  grep rtc /tmp/logdir/wakeup.txt> /dev/null
  
  if [ $? != 0 ]; then
    write_error "test failed."
    write_error " unit woke with `cat /tmp/logdir/wakeup.txt`"
  else
    write_log "test completed succesfully."
  fi
  db-config set heartbeat m_state_1_timeout_seconds 86400
fi


