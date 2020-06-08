#!/bin/sh

#---------------------------------------------------------------------------------------
#  1853_modbus
#
#  test that setting modbus disables can-odb2-monitor and disabling modbus reenables
#  can-odb2-monitor
#
#  Procedure:
#	1) determine current config - flag if features not opposite (one on, one off)
#	2) instruct user to change to toggle the modbus setting
#	3) determine current config - flag if features not opposite (one on, one off)
#	4) instruct user to change to toggle the modbus setting
#	5) determine current config - flag if features not opposite (one on, one off)
#
# Failure modes:
#	1) features do not start out opposite
#	2) features do not remain opposite
#       3) user toggle at step 2 or 4 did not register
#---------------------------------------------------------------------------------------


# TTlog will log to the screen and the TTresult.txt file
source TTlog.sh

#TThelper.sh include the get_val, set_val, clear_val functions.
source TThelper.sh

# must define TESTNAME for TTlog.sh to output the script name
#---------------------------------------------------------------------------------------
TESTNAME="1853_MODBUS"
#Procedure step 1: determine current config - flag if features not opposite (one on, one off)

obd=`db-config get -v feature can-odb2-monitor`
mod=`db-config get -v feature modbus-monitor`
  
if [ "$obd" == "$mod" ]; then
  write_error "Features are the same: can-odb2-monitor = modbus-monitor=$mod"
  exit
fi
  
#Procedure step 2: instruct user to change to toggle the modbus setting
echo "Please toggle and save the 'Enable Modbus' setting in the LCM Device Status -> Modbus page"
read -p "Press [Enter] when done"

#Procedure step 3: determine current config - flag if features not opposite (one on, one off) 
obd2=`db-config get -v feature can-odb2-monitor`
mod2=`db-config get -v feature modbus-monitor`

if [ "$obd2" == "$mod2" ]; then
  write_error "Features are the same: can-odb2-monitor = modbus-monitor=$mod"
  exit
fi

if [ "$obd" == "$obd2" ]; then
  write_error "can-odb2-monitor remained the same after toggle (Did you toggle the modbus setting?)"
  exit
fi

if [ "$mod" == "$mod2" ]; then
  write_error "modbus-monitor remained the same after toggle (Did you toggle the modbus setting?)"
  exit
fi

#Procedure step 4: instruct user to change to toggle the modbus setting
echo "Please toggle and save the 'Enable Modbus' setting in the LCM Device Status -> Modbus page"
read -p "Press [Enter] when done"

obd3=`db-config get -v feature can-odb2-monitor`
mod3=`db-config get -v feature modbus-monitor`

#Procedure step 5: determine current config - flag if features not opposite (one on, one off)    

if [ "$obd3" == "$mod3" ]; then
  write_error "Features are the same: can-odb2-monitor = modbus-monitor=$mod"
  exit
fi

if [ "$obd3" == "$obd2" ]; then
  write_error "can-odb2-monitor remained the same after toggle (Did you toggle the modbus setting?)"
  exit
fi

if [ "$mod3" == "$mod2" ]; then
  write_error "modbus-monitor remained the same after toggle (Did you toggle the modbus setting?)"
  exit
fi

write_log "test successfully completed."
