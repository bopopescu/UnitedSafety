#!/bin/sh
TTfile="/mnt/update/TRUtest/TRUtest.txt"

# Use RED - errors
#			GREEN - Response required ("Hit Enter to continue")
#			CYAN - debugging information (progress)
#			BLUE - instructions/actions required ("Remove the sym",...)
#			YELLOW - regular log entries ("Test was successful"...)
RED="\033[31m"
GREEN="\033[32m"
YELLOW="\033[33m"
BLUE="\033[34m"
CYAN="\033[36m"
NORMAL="\033[39m\033[0m"
INVERT="\033[7m"

pRED=$'\033[31m'
pGREEN=$'\033[32m'
pYELLOW=$'\033[33m'
pNORMAL=$'\033[39m\033[0m'
pBLUE=$'\033[34m'
pCYAN=$'\033[36m'
pINVERT=$'\033[7m'



clear_val()
{
  grep $1 $TTfile > /dev/null 2>&1
				
  if [ $? = 0 ]; then
    lineno=`grep -n $1 $TTfile  |cut -f1 -d:`
    strv=$lineno
    xx="d"
    sed -i "$strv$xx" $TTfile
  fi
}

get_val ()
{
  if [ -e $TTfile ]; then
    cat $TTfile | grep $1 | awk 'BEGIN {FS="="}{print $2}'
  fi
}


set_val ()
{
  newentry="$1=$2"

  grep $1 $TTfile > /dev/null 2>&1
				
  if [ $? = 0 ]; then
    lineno=`grep -n $1 $TTfile  |cut -f1 -d:`
    xx="s"
    sed -i "$lineno$xx/.*/$newentry/" $TTfile
  else
    echo $newentry >> $TTfile
  fi
}

# issue a prompt and get the response (converted to uppercase)
get_response()
{
  read -p "${pGREEN} $1 ${pNORMAL}" response
  response=$(echo $response | tr 'a-z' 'A-Z')
}

prompt()
{
  echo -e "$GREEN $1 $NORMAL"
  read -p "${pGREEN} Press [Enter] when done ${pNORMAL}"
}

wait_for_power_monitor()
{
  count=`ps | grep power-monitor | grep -v grep | wc -l`
    
  while [ "$count" -lt "1" ]; do
    sleep 2
    count=`ps | grep power-monitor | grep -v grep | wc -l`
  done
}

wait_for_admin_client()
{
  count=`ps | grep admin-client | grep -v grep | wc -l`
    
  while [ "$count" -lt "1" ]; do
    sleep 2
    count=`ps | grep admin-client | grep -v grep | wc -l`
  done
}

# this will wait for up to 1 minute to see that the last message in the message table is gone from the cams database
wait_for_message_send()
{
	last_mid=$(sqlite3 /mnt/update/database/messages.db "select MAX(mid) from message_table;")
 	still_sending=$(sqlite3 /mnt/update/database/cams.db  "select count(mtid) from message_table where mtid = $last_mid;")
 	try_count=0
	while [ "$still_sending" == "1" ] && [ "$try_count" -lt "12" ]; do
	  sleep 5
  	still_sending=$(sqlite3 /mnt/update/database/cams.db  "select count(mtid) from message_table where mtid = $last_mid;")	
  	try_count=`expr $try_count + 1`
	done
}

#-----------------------------------------------------------------------------------------------
# GetYNResponse - asks user for a Y or N response.
#  Usage: GetYNResponse "TESTNAME" "Prompt" "ErrorString"
# Y indicates success
# N indicates an error and prints the error string then exits
#
GetYNResponse()
{
  get_response "$2 [Y/N]:"
  answered=0                                      
                                          
  while [ "$answered" == "0" ]; do        
    if [ "$response" == "Y" ]; then                       
      write_log "$1: success"                  
      answered=1                                  
    elif  [ "$response" == "N" ]; then                    
      write_error "$1: $3"
      exit                                                                                             
    else                                                  
      echo -e "$GREEN Please enter Y or N. $NORMAL"
      get_response "$1 [Y/N]:"
    fi                                                         
  done                                                    
}

#-----------------------------------------------------------------------------------------------
# ForceSleep - makes the unit go to sleep
ForceSleep ()
{
  write_debug "Forcing sleep"
  sleep 2 # let them see the message
  echo -e "PowerMonitorStateMachine shutdown force=true" | nc -w 1 localhost 41009
  sleep 30
  write_error "forced shutdown did not work!"
  write_error "system is still awake after 30 seconds - check power-monitor (is it running?)"
}

#-----------------------------------------------------------------------------------------------
# ForceReboot - reboots the system - generates error if it doesn't reboot in a timely fashion
ForceReboot ()
{
  write_debug "Forcing reboot"
  sleep 2;  # let them see the message
  reboot;
  sleep 10;
  write_error "Forced reboot failed!"
  write_error "system is still awake after 10 seconds - check power-monitor (is it running?)"
}

