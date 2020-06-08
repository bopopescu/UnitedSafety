#!/bin/sh

#---------------------------------------------------------------------------------------
#  wifi_test
#
#  Test message transmission over WiFi client
#
#  Procedure:
#	1) turn on wifi-monitor, packetizer-cams
# 2) unset wifi databaese
# 3) inject office wifi username and password into wifi database
# 4) reboot
#	5) turn off ppp0 and eth0 device via ifconfig command
#	6) check route table, ensure all traffic go through apcli0
# 7) send schedule message
# 8) check in testcams3
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
TESTNAME="WIFI_TEST"

stage=`get_val stage`

write_log "Current Stage $stage"

# startup stage
if [ -z "$stage" ]; then
  set_val stage 'wifi-stage2'
  db-config set feature wifi-monitor 1
  db-config set feature packetizer-cams 1
  echo "unsetdb all" | socat - unix-connect:/var/run/redstone/wifi-monitor
  echo "updatedb username=\"41465320576972656c657373\" password=\"61667361747333333735\" " | socat - unix-connect:/var/run/redstone/wifi-monitor
  write_log  "$TESTNAME: going to reboot now."
  reboot
  sleep 3
  exit

elif [ "$stage" == "wifi-stage2" ]; then
  sleep 120;
  ifconfig ppp0 down
  ifconfig eth0 down

  i=0;
  while true; do
    i=`expr $i + 1`
    essid=`iwconfig apcli0 | grep "ESSID" | awk 'BEGIN {FS=":"}{print $2}'`
    if [ "$essid"="AFS Wireless" ];then
      break;
    fi

    if [ $i -gt 30 ];then
      write_error "Failed to connect WiFi Host."
      exit;
    fi
    sleep 2
  done

  echo "msg scheduled_message" | socat - unix-connect:/var/run/redstone/message-assembler
  sleep 5
  read -p "Do you see the any updated message at current time on TESTCAMS3? (Y/n)" choice
  if [ $? != 0 ]; then
    write_error "send message failed over wifi client."
    exit
  else
    write_log "test completed succesfully."
  fi
fi


