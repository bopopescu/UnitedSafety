#!/bin/sh

#---------------------------------------------------------------------------------------
#  gps_sim
#
#  send simulated position message to CAMS.
#
#  Procedure:
#	1) set proper configuration so that send position message periodically.
#	2) reboot
#	4) write confirmation that test worked if the unit came up on time
#
# Failure modes:
#	1) No position message reported to CAMS
#	2) No correct Position GPS reported to CAMS
#
#---------------------------------------------------------------------------------------

# TTlog will log to the screen and the TTresult.txt file
source TTlog.sh

#TThelper.sh include the get_val, set_val, clear_val functions.
source TThelper.sh

# must define TESTNAME for TTlog.sh to output the script name
#---------------------------------------------------------------------------------------
TESTNAME="GPS_SIM"

stage=`get_val stage`

write_log "Current Stage $stage"

# startup stage
if [ -z "$stage" ]; then

  set_val stage 'reboot'

  db-config set feature packetizer-cams 1
  db-config set feature PositionUpdate 1

  db-config set PositionUpdate Time 5
  db-config set PositionUpdate ReportWHenStopped On

  db-config set packetizer-cams host testcams3.gpscams.com
  db-config set packetizer-cams port 51001

  write_log  "$TESTNAME: going to reboot now."

  reboot
  sleep 3
  exit

elif [ "$stage" == "reboot" ]; then
  sleep 20
	write_prompt "gps simulator is running, please wait patientlly until a messaeg prompt. meanwhile you can login test cams to check if proper position message arrived."
	gps-sim /etc/redstone/TRUtest/talisman.NMEA
  GetYNResponse "GPS SIM Test" "Did you see the position message with current generated time, position time, received time showing on CAMS?" "Test Failed"
fi

