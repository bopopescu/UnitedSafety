#!/bin/sh

#---------------------------------------------------------------------------------------
#  wakeup_can
#
#  test that a unit will wakeup on the can switch
#
#  Procedure:
#	1) set wakeups so that only can is a wakeup
#	2) reboot
#	3) trigger sleep
#	4) write confirmation that test worked if the unit came up on time
#
# Failure modes:
#	1) Unit does not go to sleep
#	2) Unit does not wake up with can
#
#---------------------------------------------------------------------------------------


# TTlog will log to the screen and the TTresult.txt file
source TTlog.sh

#TThelper.sh include the get_val, set_val, clear_val functions.
source TThelper.sh

# must define TESTNAME for TTlog.sh to output the script name
#---------------------------------------------------------------------------------------
TESTNAME="wakeup_can"

stage=`get_val stage`

# startup stage
if [ -z "$stage" ]; then
  wait_for_power_monitor

  db-config set wakeup mask rtc,~accel,~inp1,~inp2,~inp3,can,~batt_volt,low_batt
  set-wakeup-mask
  set_val stage "awake_$TESTNAME"
  sleep 5

  write_log  "$TESTNAME: going to sleep now."
  echo -e "PowerMonitorStateMachine shutdown force=true" | nc -w 1 localhost 41009
  sleep 30
  write_error "forced shutdown did not work!"
  write_error "system is still awake after 30 seconds - check power-monitor (is it running?)"
  exit
elif [ "$stage" == "awake_$TESTNAME" ]; then
  grep can /tmp/logdir/wakeup.txt> /dev/null
  
  if [ $? != 0 ]; then
    write_error "test failed."
    write_error " unit woke with `cat /tmp/logdir/wakeup.txt`"
  else
    write_log "Unit woke on can - test completed succesfully."
  fi
fi


