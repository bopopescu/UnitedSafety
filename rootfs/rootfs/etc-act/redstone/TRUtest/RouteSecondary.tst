#!/bin/sh

#---------------------------------------------------------------------------------------
#  RouteSecondary
#
#  Set up a unit as a Secondary unit (sends all data to another (Primary) trulink)
#
#  Procedure:
#  1) configure unit for Secondary, testcams3, RouteOverride, turn off message outputs
#  2) configure the link key to 0's
#  3) get RouteIP from primary
#  4) reboot
#  5) send messages
#  6) send SLP messages
#  7) confirm that SLP received acks and state
#
# Failure modes:
#  1) Unit does not fall over to network IP
#	 2) Unit fails to send startup message to testcams
#  3) Unit fails to pass along message from secondary unit
#  3) Unit fails to pass along message from SLP attached to secondary unit
#  3) Unit fails to pass along ack to SLP attached to secondary unit
#  
#---------------------------------------------------------------------------------------

# TTlog will log to the screen and the TTresult.txt file  
source TTlog.sh   

#TThelper.sh include the get_val, set_val, clear_val functions.
source TThelper.sh

# must define TESTNAME for TTlog.sh to output the script name  
#---------------------------------------------------------------------------------------
TESTNAME="RouteSecondary"
wait_for_power_monitor


stage=`get_val stage` 
if [ -z "$stage" ]; then  
	write_debug "Running test $TESTNAME"
else
	write_debug "Running test $TESTNAME: current stage $stage"
fi

# startup stage 
if [ -z "$stage" ]; then  
  set_val stage TEST1_$TESTNAME

  db-config set RedStone KeepAwakeMinutes 15             # no delay in shutting down
  db-config set PositionUpdate Time 0                   # Have PositionUpdate generate no messages
  db-config set PositionUpdate Distance 0
  db-config set PositionUpdate Heading 0
  db-config set PositionUpdate IridiumReportTime 0
  db-config set PositionUpdate RealTimeInterval 0
  db-config set feature packetizer-cams 1
  db-config set feature packetizer-calamp 0
  db-config set feature packetizer 0
  db-config set feature packetizer_dash 0
  db-config set feature packetizer-dash 0
  db-config set packetizer-cams ForceIridium Off
  db-config set packetizer-cams host testcams3.gpscams.com
  db-config set MSGPriority	acceleration	20
  db-config set MSGPriority	accept_accel_resumed	20
  db-config set MSGPriority	accept_deccel_resumed	20
  db-config set MSGPriority	calamp_user_msg	20
  db-config set MSGPriority	check_in	20
  db-config set MSGPriority	check_out	20
  db-config set MSGPriority	crit_batt	20
  db-config set MSGPriority	direction_change	20
  db-config set MSGPriority	driver_status	20
  db-config set MSGPriority	engine_off	20
  db-config set MSGPriority	engine_on	20
  db-config set MSGPriority	engine_param_exceed	20
  db-config set MSGPriority	engine_param_normal	20
  db-config set MSGPriority	engine_period_report	20
  db-config set MSGPriority	engine_trouble_code	20
  db-config set MSGPriority	fall_detected	20
  db-config set MSGPriority	fuel_log	20
  db-config set MSGPriority	gpsfix_invalid	20
  db-config set MSGPriority	hard_brake	20
  db-config set MSGPriority	heartbeat	20
  db-config set MSGPriority	help	20
  db-config set MSGPriority	ignition_off	20
  db-config set MSGPriority	ignition_on	20
  db-config set MSGPriority	j1939	20
  db-config set MSGPriority	j1939_fault	20
  db-config set MSGPriority	j1939_status2	20
  db-config set MSGPriority	low_batt	20
  db-config set MSGPriority	not_check_in	2o
  db-config set MSGPriority	odometer_update	20
  db-config set MSGPriority	ok	20
  db-config set MSGPriority	other	20
  db-config set MSGPriority	ping	20
  db-config set MSGPriority	power_off	20
  db-config set MSGPriority	power_on	20
  db-config set MSGPriority	scheduled_message	20
  db-config set MSGPriority	sensor	20
  db-config set MSGPriority	sos	1
  db-config set MSGPriority	speed_acceptable	20
  db-config set MSGPriority	speed_exceeded	20
  db-config set MSGPriority	start_condition	20
  db-config set MSGPriority	stop_condition	20
  db-config set MSGPriority	switch_int_power	20
  db-config set MSGPriority	switch_wired_power	20
  db-config set MSGPriority	text	20
  
  db-config set system PrimaryPort	39002
  db-config set system RouteOverride secondary
  db-config set system BroadcastPort	39003
  db-config set system RouteIP 21
  db-config set feature primary-monitor 0
  db-config set feature secondary-monitor 1
  db-config set feature packetizer-secondary 1
  
  db-config set zigbee link-key "00000000000000000000000044444444"
  
  rm /mnt/update/database/cams.db
  rm /mnt/update/database/messages.db

	prompt "Execute RoutePrimary.tst on the primary unit.  Wait for RouteID to be displayed."
	get_response "Enter the RouteID provided by the primary unit:"
  db-config set system RouteIP $response
 
  ForceReboot                         #reboot to have changes take affect
  sleep 10
elif [ "$stage" == "TEST1_$TESTNAME" ]; then
  set_val stage TEST2_$TESTNAME
	write_debug "Waiting for dhcp reassignment"

  count=`ps | grep network-monitor | grep -v grep | wc -l`
    
  while [ "$count" -lt "1" ]; do
    sleep 2
    count=`ps | grep network-monitor | grep -v grep | wc -l`
  done

  sleep 5;
  
  # wait for eth0 to come up the first time
	val=`ifconfig eth0`
	
	while [ -z "$val" ]; do
		sleep 4
		val=`ifconfig eth0`
	done
	
  # wait for eth0 to switch to local network
	ip=`db-config get -v system eth0addr`
	val=`ifconfig eth0 | grep "$ip"`

	while [ -n "$val" ]; do
 		sleep 4
		val=`ifconfig eth0 | grep $ip`
	done
	
	link=`db-config get -b zigbee link-key`
	prompt "Turn on SLP with link-key $link.  Wait for connection"
	echo  -e "$BLUE""Check IN via the SLP.  Wait for ack$NORMAL"
	GetYNResponse $TESTNAME "Did you receive an ack?" "SLP failed to receive an ack on route 'WorkAlone->local network->primary->secondary->SLP'"
	echo -e "$BLUE""Check OUT via the SLP.  Wait for ack$NORMAL"
	GetYNResponse $TESTNAME "Did you receive an ack?" "SLP failed to receive an ack on route 'WorkAlone->local network->primary->secondary->SLP'"
  echo -e "$YELLOW Network communications are successful$NORMAL"
  
	prompt "Remove Network connection from switch"
	echo -e "$BLUE""This is $INVERT NETWORK SWITCHOVER. $NORMAL "
	prompt "Hit enter on Primary unit"
	echo  -e "$BLUE""Check IN via the SLP.  Wait for ack$YELLOW (This will take a while for the network to switch over)$NORMAL"
	GetYNResponse $TESTNAME "Did you receive an ack?" "SLP failed to receive an ack on route 'WorkAlone->primary->secondary->SLP'"

	prompt "Remove SIM card from primary unit"

	echo -e "$BLUE""This is $INVERT IRIDIUM SWITCHOVER. $NORMAL "
	prompt "Hit enter on Primary unit"
	echo  -e "$BLUE""Check OUT via the SLP.  Wait for ack$YELLOW (This will take a while for the network to switch over)$NORMAL"
	GetYNResponse $TESTNAME "Did you receive an ack?" "SLP failed to receive an ack on route 'WorkAlone->primary (Iridium)->secondary->SLP'"
  echo -e "$YELLOW Iridium communications are successful. $NORMAL"

	echo -e "$BLUE""This is $INVERT Test Completion. $NORMAL "
  prompt "Test complete. Hit enter on Primary unit."
fi


