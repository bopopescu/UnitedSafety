#!/bin/sh

#---------------------------------------------------------------------------------------
#  PROXY
#
#  Send a bunch of messages to the proxy server via cams to make sure that all messages
#  are being handled properly.  Do it both via cell/ethernet and via Iridium.  
#
#  Procedure:
#  1) configure unit for debug shutdown (wakeup, IgnitionMonitor, etc)
#  2) inject a message into the system at priority 20
#  3) trigger IgnitionOff
#  4) Watch for sleep
#  5) repeat for priority 9 message
#  6) repeat for priority 1 message
#
# Failure modes:
#  1) Unit does not go to sleep on priority 20 message
#  2) Unit goes to sleep too fast on priority 9 message
#  3) Unit does not go to sleep after 15 minutes on priority 9 message 
#  4) Unit goes to sleep on priority 1 message
#  
#---------------------------------------------------------------------------------------

# TTlog will log to the screen and the TTresult.txt file  
source TTlog.sh   

#TThelper.sh include the get_val, set_val, clear_val functions.
source TThelper.sh

# must define TESTNAME for TTlog.sh to output the script name  
#---------------------------------------------------------------------------------------
TESTNAME="PROXY"
wait_for_power_monitor


stage=`get_val stage` 
write_debug "Running test $TESTNAME, current stage $stage"

# startup stage 
if [ -z "$stage" ]; then  
  db-config set RedStone KeepAwakeMinutes 0             # no delay in shutting down
  db-config set IgnitionMonitor UseIgnitionDebug	1     #allow artificial 'no ignition' condition
  db-config set feature wifi-monitor 0
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
  db-config set packetizer-cams host proxy.gpscams.com
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
  db-config set MSGPriority	ping	2o
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
  # set up i2c-gpio-monitor 
  db-config set i2c-gpio-monitor UseDebug On
  db-config set i2c-gpio-monitor ForceVal 63
  db-config set i2c-gpio-monitor inputOffPriority1	9
  db-config set i2c-gpio-monitor inputOffPriority2	9
  db-config set i2c-gpio-monitor inputOffPriority3	9
  db-config set i2c-gpio-monitor inputOffPriority4	9
  db-config set i2c-gpio-monitor inputOffPriority5	9
  db-config set i2c-gpio-monitor inputOffPriority6	9
  db-config set i2c-gpio-monitor inputOffType1	10
  db-config set i2c-gpio-monitor inputOffType2	10
  db-config set i2c-gpio-monitor inputOffType3	10
  db-config set i2c-gpio-monitor inputOffType4	10
  db-config set i2c-gpio-monitor inputOffType5	10
  db-config set i2c-gpio-monitor inputOffType6	10
  db-config set i2c-gpio-monitor inputOnPriority1	20
  db-config set i2c-gpio-monitor inputOnPriority2	20
  db-config set i2c-gpio-monitor inputOnPriority3	20
  db-config set i2c-gpio-monitor inputOnPriority4	20
  db-config set i2c-gpio-monitor inputOnPriority5	20
  db-config set i2c-gpio-monitor inputOnPriority6	20
  db-config set i2c-gpio-monitor inputOnType1	10
  db-config set i2c-gpio-monitor inputOnType2	10
  db-config set i2c-gpio-monitor inputOnType3	10
  db-config set i2c-gpio-monitor inputOnType4	110
  db-config set i2c-gpio-monitor inputOnType5	10
  db-config set i2c-gpio-monitor inputOnType6	10
  
  db-config set heartbeat m_state_1_timeout_seconds 86400
  killall heartbeat
  rm /mnt/update/database/cams.db
  rm /mnt/update/database/messages.db
  sleep 5

  set_val stage TEST1_$TESTNAME
  reboot                         #reboot to have changes take affect
  sleep 10
elif [ "$stage" == "TEST1_$TESTNAME" ]; then
  #  Send Ignition On, Ignition Off then Ignition On messages (Leaves CAMS with an ON state for further messages)
  #  Generate the messages 5 seconds apart
  sleep 10   # time for things to settle down
#  set_val stage TEST2
  echo -e "msg ignition_on mobile_id=9999999999999999\r" | socat - unix-connect:/var/run/redstone/message-assembler
  sleep 5
  echo -e "msg ignition_off mobile_id=9999999999999999\r" | socat - unix-connect:/var/run/redstone/message-assembler
  sleep 5
  echo -e "msg ignition_on mobile_id=9999999999999999\r" | socat - unix-connect:/var/run/redstone/message-assembler
  
  # Send the messages and wait for them to be sent
  wait_for_message_send
  
  #GetYNResponse "TEST1:" "Were Ignition ON/off/ON records received in CAMS?" "Ignition messages were not received in CAMS"
 
 
  #Test3 Position report
  
  echo -e "msg scheduled_message mobile_id=9999999999999999\r" | socat - unix-connect:/var/run/redstone/message-assembler
  wait_for_message_send                                                              

  #GetYNResponse "TEST3:" "Was a POSITION record received in CAMS?" "Position messages were not received in CAMS"


  #Test5 SLP report
                                                                        
  # OK message (checkin)                                  
  echo -e "msg calamp_user_msg usr_msg_data=245047454D43492C2C2C2C2C534C502A3736 mobile_id=9999999999999998 mobile_id_type=4\r" | socat - unix-connect:/var/run/redstone/message-assembler
                                                                                        
  # SOS                                           
  echo -e "msg calamp_user_msg usr_msg_data=245047454D454D2C2C2C534C502A3734 mobile_id=9999999999999998 mobile_id_type=4\r" | socat - unix-connect:/var/run/redstone/message-assembler
                                                  
  # cancel emergency                      
  echo -e "msg calamp_user_msg usr_msg_data=245047454D45432C2C2C534C502A3741 mobile_id=9999999999999998 mobile_id_type=4\r" | socat - unix-connect:/var/run/redstone/message-assembler
                                                  
  # Stop monitoring                       
  echo -e "msg calamp_user_msg usr_msg_data=245047454D4D4F2C2C2C534C502A3745 mobile_id=9999999999999998 mobile_id_type=4\r" | socat - unix-connect:/var/run/redstone/message-assembler
  wait_for_message_send                                                              
                                                  
  #GetYNResponse "TEST5:" "Were 4 SLP records received in CAMS?" "SLP messages were not received in CAMS"
  
  #Test7 Heartbeat report
  echo -e "msg heartbeat\r" | socat - unix-connect:/var/run/redstone/message-assembler
  wait_for_message_send
  #GetYNResponse "TEST7:" "Was a HEARTBEAT record received in CAMS?" "HEARTBEAT message were not received in CAMS"
  
  #Test 9 INPUTS
  db-config set i2c-gpio-monitor ForceVal 61
  sleep 5
  db-config set i2c-gpio-monitor ForceVal 63
  wait_for_message_send

  #GetYNResponse "TEST9:" "Were 2 Sensor records received in CAMS?" "SENSOR messages were not received in CAMS"
  
  
  #Test 11 Modbus
  data1="2450415453504D2C322C31352C342C352C36313434342C3531332C302E302C31392E302C31352E302C36313434342C3139302C313739382E302C"
  data2="313830322E302C313830312E302C36353236322C3131302C3137362E302C3138302E302C3137382E302C36353236332C3130302C38332E302C38392E302C38352E302A3342"
  echo -e "msg calamp_user_msg 	mobile_id=9999999999999999 usr_msg_data=$data1$data2\r" | socat - unix-connect:/var/run/redstone/message-assembler
  wait_for_message_send
  #GetYNResponse "TEST11:" "Was a MODBUS update record received in CAMS?" "MODBUS messages were not received in CAMS"

  #Test 13 Modbus exceedence 
  data="245041545348582C322C36313434342C3139302C302C313835362E302C313938302E302C302A3139"
  echo -e "msg calamp_user_msg mobile_id=9999999999999999 	usr_msg_data=$data\r" | socat - unix-connect:/var/run/redstone/message-assembler
  wait_for_message_send
  #GetYNResponse "TEST15:" "Was a MODBUS Fault record received in CAMS?" "MODBUS fault were not received in CAMS"
  
  
  #Test 15 Modbus Fault
  data="245041545346542C322C313230382C332C31302A3046"
  echo -e "msg calamp_user_msg mobile_id=9999999999999999	usr_msg_data=$data\r" | socat - unix-connect:/var/run/redstone/message-assembler
  wait_for_message_send
  #GetYNResponse "TEST15:" "Was a MODBUS Fault record received in CAMS?" "MODBUS fault were not received in CAMS"

  #-------------------------------------------------------------------------------------------------------
  # Now repeat over Iridium only
  #
  db-config set packetizer-cams ForceIridium On
  #Test 4 On/Off report
  echo -e "msg ignition_on mobile_id=9999999999999999 msg_priority=9\r" | socat - unix-connect:/var/run/redstone/message-assembler
  sleep 5
  echo -e "msg ignition_off mobile_id=9999999999999999 msg_priority=9\r" | socat - unix-connect:/var/run/redstone/message-assembler
  sleep 5
  echo -e "msg ignition_on mobile_id=9999999999999999 msg_priority=9\r" | socat - unix-connect:/var/run/redstone/message-assembler
  
  # Send the messages and wait for them to be sent
  wait_for_message_send
  
  #GetYNResponse "TEST2:" "Were Ignition ON/off/ON records received in CAMS?" "Ignition messages were not received in CAMS"
 
 
  #Test 4 Position report
  
  echo -e "msg scheduled_message mobile_id=9999999999999999 msg_priority=9\r" | socat - unix-connect:/var/run/redstone/message-assembler
  wait_for_message_send                                                              

  #GetYNResponse "TEST4:" "Was a POSITION record received in CAMS?" "Position messages were not received in CAMS"


  #Test 6 SLP report
                                                                        
  # OK message (checkin)                                  
  echo -e "msg calamp_user_msg  mobile_id=9999999999999999 msg_priority=9 usr_msg_data=245047454D43492C2C2C2C2C534C502A3736 mobile_id=9999999999999998 mobile_id_type=4\r" | socat - unix-connect:/var/run/redstone/message-assembler
                                                                                        
  # SOS                                           
  echo -e "msg calamp_user_msg  mobile_id=9999999999999999 msg_priority=9 usr_msg_data=245047454D454D2C2C2C534C502A3734 mobile_id=9999999999999998 mobile_id_type=4\r" | socat - unix-connect:/var/run/redstone/message-assembler
                                                  
  # cancel emergency                      
  echo -e "msg calamp_user_msg  mobile_id=9999999999999999 msg_priority=9 usr_msg_data=245047454D45432C2C2C534C502A3741 mobile_id=9999999999999998 mobile_id_type=4\r" | socat - unix-connect:/var/run/redstone/message-assembler
                                                  
  # Stop monitoring                       
  echo -e "msg calamp_user_msg  mobile_id=9999999999999999 msg_priority=9 usr_msg_data=245047454D4D4F2C2C2C534C502A3745 mobile_id=9999999999999998 mobile_id_type=4\r" | socat - unix-connect:/var/run/redstone/message-assembler
  wait_for_message_send                                                              
                                                  
  #GetYNResponse "TEST6:" "Were 4 SLP records received in CAMS?" "SLP messages (Iridium) were not received in CAMS"
  
  #Test 8 Heartbeat report
  echo -e "msg heartbeat msg_priority=9 mobile_id=9999999999999999\r" | socat - unix-connect:/var/run/redstone/message-assembler
  wait_for_message_send
  #GetYNResponse "TEST8:" "Was a HEARTBEAT record received in CAMS?" "HEARTBEAT message (Iridium) were not received in CAMS"
  
  #Test 10 INPUTS
  echo -e "msg sensor msg_priority=9 mobile_id=9999999999999999\r" | socat - unix-connect:/var/run/redstone/message-assembler
  echo -e "msg sensor msg_priority=9 mobile_id=9999999999999999\r" | socat - unix-connect:/var/run/redstone/message-assembler
  wait_for_message_send

  #GetYNResponse "TEST10:" "Were 2 Sensor records received in CAMS?" "SENSOR messages (Iridium) were not received in CAMS"

  data="2450415453504D2C322C312C312C352C36313138342C3532303139322C313933302E362C313933302E362C313933302E362A3034"
  echo -e "msg calamp_user_msg 	msg_priority=9 mobile_id=9999999999999999 usr_msg_data=$data\r" | socat - unix-connect:/var/run/redstone/message-assembler
  wait_for_message_send
  
  #GetYNResponse "TEST12:" "Was a MODBUS Update record received in CAMS (Engine hours only)?" "MODBUS Engine hours (Iridium) were not received in CAMS"
  
  
  #Test 14 Modbus exceedence Iridium
  data="245041545348582C322C36313434342C3139302C302C313835362E302C313938302E302C302A3139"
  echo -e "msg calamp_user_msg 	msg_priority=9 mobile_id=9999999999999999 usr_msg_data=$data\r" | socat - unix-connect:/var/run/redstone/message-assembler
  wait_for_message_send
  #GetYNResponse "TEST15:" "Was a MODBUS Fault record received in CAMS?" "MODBUS exdeedence (Iridium) was not received in CAMS"
  
  #Test 16 Modbus Fault Iridium
  data="245041545346542C322C313230382C332C31302A3046"
  echo -e "msg calamp_user_msg 	msg_priority=9 mobile_id=9999999999999999 usr_msg_data=$data\r" | socat - unix-connect:/var/run/redstone/message-assembler
  wait_for_message_send
  #GetYNResponse "TEST15:" "Was a MODBUS Fault record received in CAMS?" "MODBUS fault (Iridium) was not received in CAMS"

fi


