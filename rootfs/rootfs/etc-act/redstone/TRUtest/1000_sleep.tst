#!/bin/sh

#---------------------------------------------------------------------------------------
# sleep
#
#  test that a unit can be shut off with the go-to-sleep command via power-monitor
#  Critical for forcing units to sleep for other tests
#
#  Procedure:
#	1) set wakeups so that only RTC is a wakeup
#	2) reboot
#	3) trigger sleep
#	4) write confirmation that test worked if the unit came up on time
#
# Failure modes:
#	1) Unit does not go to sleep
#	2) Unit does not wake up at specified time (failure of RTC wakeup)
#
#---------------------------------------------------------------------------------------


# TTlog will log to the screen and the TTresult.txt file
source TTlog.sh

#TThelper.sh include the get_val, set_val, clear_val functions.
source TThelper.sh

# must define TESTNAME for TTlog.sh to output the script name
#---------------------------------------------------------------------------------------
TESTNAME="SLEEP"

stage=`get_val stage`

write_log "Current Stage $stage"

# startup stage
if [ -z "$stage" ]; then
  db-config set wakeup mask rtc,~accel,~inp1,~inp2,~inp3,~can,~batt_volt,low_batt
  set-wakeup-mask
  db-config set heartbeat m_state_1_timeout_seconds 120
  killall heartbeat
  sleep 5
  set_val stage awake
  count=`ps | grep power-monitor | grep -v grep | wc -l`
  
  while [ "$count" -lt "1" ]; do
    sleep 2
    count=`ps | grep power-monitor | grep -v grep | wc -l`
  done

  set_val stage awake
  write_log  "$TESTNAME: going to sleep now."
  echo -e "PowerMonitorStateMachine shutdown force=true" | nc -w 1 localhost 41009
  sleep 30
  write_error "forced shutdown did not work!"
  write_error "system is still awake after 30 seconds - check power-monitor (is it running?)"
  exit
elif [ "$stage" == "awake" ]; then
  write_log "test completed succesfully."
fi


