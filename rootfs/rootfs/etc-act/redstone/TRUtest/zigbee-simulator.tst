#!/bin/sh

#---------------------------------------------------------------------------------------
#  zigbee_simulator_test
#
#  Inject each message into the system, 
#
#  Procedure:
#	1) run /usr/bin/fob-sim to build connection with zigbee-monitor
#	2) send related command to control zigbee-monitor via unix socket.
#	3) write confirmation that test worked if the message came up to CAMS on time
#
# Failure modes:
#	1) Message does not show in CAMS.
#
#---------------------------------------------------------------------------------------

# TTlog will log to the screen and the TTresult.txt file
source TTlog.sh

#TThelper.sh include the get_val, set_val, clear_val functions.
source TThelper.sh

# must define TESTNAME for TTlog.sh to output the script name
#---------------------------------------------------------------------------------------
TESTNAME="ZIGBEE_SIM_TEST"

stage=`get_val stage`

write_log "Current Stage $stage"

# startup stage
if [ -z "$stage" ]; then
  set_val stage 'sim'
  db-config set feature zigbee-monitor 1
  write_log  "$TESTNAME: going to reboot now."
  reboot
  sleep 3
  exit

elif [ "$stage" == "sim" ]; then
  sleep 30 
  /usr/bin/fob-sim &
  sleep 30
  echo "checkin" | socat - unix-connect:/var/run/redstone/fob-sim
  echo "send checkin message"
  sleep 5
  GetYNResponse "zigbee test" "Do you see the checkin message on TESTCAMS3?" "Send checkin message Fail"
  
  echo "checkout" | socat - unix-connect:/var/run/redstone/fob-sim
  echo "send check out message"
  sleep 5
  GetYNResponse "zigbee test" "Do you see the checkout message on TESTCAMS3?" "Send checkout message Fail"

  echo "sos" | socat - unix-connect:/var/run/redstone/fob-sim
  echo "send SOS message"
  sleep 5
  GetYNResponse "zigbee test" "Do you see the SOS message on TESTCAMS3?" "Send SOS message Fail"

  echo "sos cancel" | socat - unix-connect:/var/run/redstone/fob-sim
  echo "send SOS CANCEL message"
  sleep 5
  GetYNResponse "zigbee test" "Do you see the SOS Cancel message on TESTCAMS3?" "Send SOS Cancel message Fail"

fi


