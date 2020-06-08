<?php

require_once $_SERVER['DOCUMENT_ROOT'].'inc/dbconfig_controller.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'inc/gps_controller.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'inc/util.inc';				//contains functions for input validation, socket interaction, error message display, and logging.

if(!empty($_GET) && !empty($_GET['codes']))
{
	foreach(explode(",",$_GET['codes']) as $key)
	{
		translateStatusCode($key);
	}

	//highlight the errored fields
	if(!empty($_GET['fields']))
	{
		foreach(explode(",",$_GET['fields']) as $field)
		{
			highlightField($field);
		}
	}
}


//OBJECT INSTANTIATION
$dbconfig = new dbconfigController();
$gps = new gpscontroller();

/* ===== POSITION UPDATE TAB ====== */
$posup_time = $posup_distance = $posup_heading =  $posup_min_send_time =
$posup_pinning = $posup_report_start_stop = $posup_report_when_stopped = $posup_stop_velocity = $posup_stop_time = $IridiumUpdateInterval = '';

if(isset($_GET['positionUpdateTime']) && $_GET['positionUpdateTime'] !== false)
{
	$posup_time = $_GET['positionUpdateTime'];
}
else
{
	$posup_time_raw = $dbconfig->getDbconfigData('PositionUpdate','Time');
	$posup_time = (isValidNumber($posup_time_raw) ? $posup_time_raw : '');
}

if(isset($_GET['positionUpdateDistance']) && $_GET['positionUpdateDistance'] !== false)
{
	$posup_distance = $_GET['positionUpdateDistance'];
}
else
{
	$posup_distance_raw = $dbconfig->getDbconfigData('PositionUpdate','Distance');
	$posup_distance = (isValidNumber($posup_distance_raw) ? $posup_distance_raw : '');
}


if(isset($_GET['positionHeading']) && $_GET['positionHeading'] !== false)
{
	$posup_heading = $_GET['positionHeading'];
}
else
{
	$posup_heading_raw = $dbconfig->getDbconfigData('PositionUpdate','Heading');
	$posup_heading = (isValidNumber($posup_heading_raw) ? $posup_heading_raw : '');
}

if(isset($_GET['positionPinning']) && $_GET['positionPinning'] !== false)
{
	$posup_pinning = $_GET['positionPinning'];
}
else
{
	$posup_pinning_raw = $dbconfig->getDbconfigData('PositionUpdate','Pinning');
	$posup_pinning = (isValidOnOff($posup_pinning_raw) ? $posup_pinning_raw : '');
}

if(isset($_GET['positionStopVelocity']) && $_GET['positionStopVelocity'] !== false)
{
	$posup_stop_velocity = $_GET['positionStopVelocity'];
}
else
{
	$posup_stop_velocity_raw = $dbconfig->getDbconfigData('PositionUpdate','StopVel');
	$posup_stop_velocity = (isValidNumber($posup_stop_velocity_raw) ? $posup_stop_velocity_raw : '');
}

if(isset($_GET['positionStopTime']) && $_GET['positionStopTime'] !== false)
{
	$posup_stop_time = $_GET['positionStopTime'];
}
else
{
	$posup_stop_time_raw = $dbconfig->getDbconfigData('PositionUpdate','StopTime');
	$posup_stop_time = (isValidNumber($posup_stop_time_raw) ? $posup_stop_time_raw : '');
}

if(isset($_GET['positionReportStopStart']) && $_GET['positionReportStopStart'] !== false)
{
	$posup_report_start_stop = $_GET['positionReportStopStart'];
}
else
{
	$posup_report_start_stop_raw = $dbconfig->getDbconfigData('PositionUpdate','ReportStopStart');
	$posup_report_start_stop = (isValidOnOff($posup_report_start_stop_raw) ? $posup_report_start_stop_raw : '');
}

if(isset($_GET['positionReportWhenStopped']) && $_GET['positionReportWhenStopped'] !== false)
{
	$posup_report_when_stopped = $_GET['positionReportWhenStopped'];
}
else
{
	$posup_report_when_stopped_raw = $dbconfig->getDbconfigData('PositionUpdate','ReportWhenStopped');
	$posup_report_when_stopped = (isValidOnOff($posup_report_when_stopped_raw) ? $posup_report_when_stopped_raw : '');
}

// Iridium update interval (how often the iridium sends a position update
if(isset($_GET['IridiumUpdateIntervalCtl']) && $_GET['IridiumUpdateIntervalCtl'] !== false)
{
	$IridiumUpdateInterval = $_GET['IridiumUpdateIntervalCtl'];
}
else
{
	$IridiumUpdateIntervalraw = $dbconfig->getDbconfigData('PositionUpdate', 'IridiumReportTime');
	$IridiumUpdateInterval = (isValidNumber($IridiumUpdateIntervalraw) ? $IridiumUpdateIntervalraw : '720');
}

/* ===== HARDWARE TAB ====== */
$hw_gps_source = $hw_gps_type = $hw_disable_sleep = $hw_speed_src = $hw_keep_awake = $hw_reboot_gps ='';

// gps_source is always internal
$hw_gps_source = 'Internal';

// gps_type is non edittable - so just assign it from db-config
$hw_gps_type_raw = $gps->getGpsType();
$hw_gps_type = (isValidString($hw_gps_type_raw) ? $hw_gps_type_raw : '');


if(isset($_GET['hardwareDisableSleep']) && $_GET['hardwareDisableSleep'] !== false)
{
	$hw_disable_sleep = $_GET['hardwareDisableSleep'];
}
else
{
	$hw_disable_sleep_raw = $dbconfig->getDbconfigData('RedStone','DisableSleep');
	$hw_disable_sleep = (isValidOnOff($hw_disable_sleep_raw) ? $hw_disable_sleep_raw : '');
}


if(isset($_GET['hardwareKeepAwake']) && $_GET['hardwareKeepAwake'] !== false)
{
	$hw_keep_awake = $_GET['hardwareKeepAwake'];
}
else
{
	$hw_keep_awake_raw = $dbconfig->getDbconfigData('RedStone','KeepAwakeMinutes');
	$hw_keep_awake = (isValidNumber($hw_keep_awake_raw) ? $hw_keep_awake_raw : '');
}



/* ===== PROTOCOLS (formerly OUTPUT) TAB ====== */
$cams_status = $cams_host = $cams_port = $cams_compression = '';
$cams_oMaxMessagesPerCycle = '';

if(isset($_GET['cams']) && $_GET['cams'] !== false)
{
	$cams_status = $_GET['cams'];
}
else
{
	$cams_status_raw = $dbconfig->getDbconfigData('feature','packetizer-cams');
	$cams_status = (isValidOnOff($cams_status_raw) ? $cams_status_raw : '');
}


$iridium_status_raw = $dbconfig->getDbconfigData('feature','iridium-monitor');
$iridium_status = (isValidOnOff($iridium_status_raw) ? $iridium_status_raw : '');

if(!empty($_GET['camsHost']))
{
	$cams_host = $_GET['camsHost'];
}
else
{
	$cams_host_raw = $dbconfig->getDbconfigData('packetizer-cams', 'host');
	$cams_host = ((isValidIP($cams_host_raw) || isValidString($cams_host_raw)) ? $cams_host_raw : '');
}
if(isset($_GET['camsPort']) && $_GET['camsPort'] !== false)
{
	$cams_port = $_GET['camsPort'];
}
else
{
	$cams_port_raw = $dbconfig->getDbconfigData('packetizer-cams', 'port');
	$cams_port = (isValidNumber($cams_port_raw) ? $cams_port_raw : '');
}

if(isset($_GET['camsCompress']) && $_GET['camsCompress'] !== false)
{
	$cams_compression = $_GET['camsCompress'];
}
else
{
	$cams_compression_raw = $dbconfig->getDbconfigData('packetizer-cams', 'UseCompression');
	$cams_compression = (isValidOnOff($cams_compression_raw) ? $cams_compression_raw : '');
}

if(isset($_GET['IridiumEnable']) && $_GET['IridiumEnable'] !== false)
{
	$iridiumEnable = $_GET['IridiumEnable'];
}
else
{
	$iridiumEnable_raw = $dbconfig->getDbconfigData('packetizer-cams', 'IridiumEnable');
	$iridiumEnable = (isValidOnOff($iridiumEnable_raw) ? $iridiumEnable_raw : '');
}

if(isset($_GET['CellFailMode']) && $_GET['CellFailMode'] !== false)
{
	$cellFailMode = $_GET['CellFailMode'];
}
else
{
	$cellFailMode_raw = $dbconfig->getDbconfigData('packetizer-cams', 'CellFailModeEnable');
	$cellFailMode = (isValidOnOff($cellFailMode_raw) ? $cellFailMode_raw : '');
}

if(isset($_GET['camsIridiumTimeout']) && $_GET['camsIridiumTimeout'] !== false)
{
	$camsIridiumTimeout = $_GET['camsIridiumTimeout'];
}
else
{
	$camsIridiumTimeout = $dbconfig->getDbconfigData('packetizer-cams', 'iridium_timeout');
}

if(isset($_GET['camsIridiumDataLimitPriority']) && $_GET['camsIridiumDataLimitPriority'] !== false)
{
	$camsIridiumDataLimitPriority = $_GET['camsIridiumDataLimitPriority'];
}
else
{
	$camsIridiumDataLimitPriority = $dbconfig->getDbconfigData('packetizer-cams', 'IridiumDataLimitPriority');
}

if(isset($_GET['camsKeepAlive']) && $_GET['camsKeepAlive'] !== false)
{
	$camsKeepAlive = $_GET['camsKeepAlive'];
}
else
{
	$camsKeepAlive = $dbconfig->getDbconfigData('packetizer-cams', 'm_keepalive');
}

if(isset($_GET['camsTimeout']) && $_GET['camsTimeout'] !== false)
{
	$camsTimeout = $_GET['camsTimeout'];
}
else
{
	$camsTimeout = $dbconfig->getDbconfigData('packetizer-cams', 'timeout');
}

if(isset($_GET['oMaxMessagesPerCycle']))
{
	$cams_oMaxMessagesPerCycle = $_GET['oMaxMessagesPerCycle'];
}
else
{
	$cams_oMaxMessagesPerCycle = $dbconfig->getDbconfigData('packetizer-cams', 'm_max_backlog');
}

if(isset($_GET['IridiumPri']))
{
	$cams_IridiumPri = $_GET['IridiumPri'];
}
else
{
	$cams_IridiumPri = $dbconfig->getDbconfigData('packetizer-cams', 'IridiumPriorityLevel');
}

if(isset($_GET['camsRetries']))
{
	$camsRetryLimit = $_GET['camsRetries'];
}
else
{
	$camsRetryLimit = $dbconfig->getDbconfigData('packetizer-cams', 'retry_limit');
}
/*
if(isset($_GET['Trak']) && $_GET['Trak'] !== false)
{
	$trakopolis_status = $_GET['Trak'];
}
else
{
	$trakopolis_status_raw = $dbconfig->getDbconfigData('feature','packetizer');
	$trakopolis_status = (isValidOnOff($trakopolis_status_raw) ? $trakopolis_status_raw : '');
}

if(!empty($_GET['trakHost']))
{
	$trakopolis_host = $_GET['trakHost'];
}
else
{
	$trakopolis_host_raw = $dbconfig->getDbconfigData('packetizer', 'host');
	$trakopolis_host = ((isValidIP($trakopolis_host_raw) || isValidString($trakopolis_host_raw)) ? $trakopolis_host_raw : '');
}
*/
/*if(isValidIP($trakopolis_host_raw))
{
	$trakopolis_host_type = "IP";
	$trakopolis_host = explode(".", $trakopolis_host_raw);
}
else if(isValidString($trakopolis_host_raw))
{
	$trakopolis_host_type = "DNS";
	$trakopolis_host = $trakopolis_host_raw;
}*/
/*
if(isset($_GET['trakPort']) && $_GET['trakPort'] !== false)
{
	$trakopolis_port = $_GET['trakPort'];
}
else
{
	$trakopolis_port_raw = $dbconfig->getDbconfigData('packetizer', 'port');
	$trakopolis_port = (isValidNumber($trakopolis_port_raw) ? $trakopolis_port_raw : '');
}

if(isset($_GET['RDS']) && $_GET['RDS'] !== false)
{
	$rds_status = $_GET['RDS'];
}
else
{
	$rds_status_raw = $dbconfig->getDbconfigData('feature','packetizer-dash');
	$rds_status = (isValidOnOff($rds_status_raw) ? $rds_status_raw : '');
}

if(!empty($_GET['rdsHost']))
{
	$rds_host = $_GET['rdsHost'];
}
else
{
	$rds_host_raw = $dbconfig->getDbconfigData('packetizer-dash', 'host');
	$rds_host = ((isValidIP($rds_host_raw) || isValidString($rds_host_raw)) ? $rds_host_raw : '');
}
if(isValidIP($rds_host_raw))
{
	$rds_host_type = "IP";
	$rds_host = explode(".", $rds_host_raw);
}
else if(isValidString($rds_host_raw))
{
	$rds_host_type = "DNS";
	$rds_host = $rds_host_raw;
}

if(isset($_GET['rdsPort']) && $_GET['rdsPort'] !== false)
{
	$rds_port = $_GET['rdsPort'];
}
else
{
	$rds_port_raw = $dbconfig->getDbconfigData('packetizer-dash', 'port');
	$rds_port = (isValidNumber($rds_port_raw) ? $rds_port_raw : '');
}
*/


/* ===== SOCKETS TAB ====== */
$sock_nmea_status = $sock_ser_gps = $sock_nmea_port = '';


if(isset($_GET['gpsSocketServer']) && $_GET['gpsSocketServer'] !== false)
{
	$sock_nmea_status = $_GET['gpsSocketServer'];
}
else
{
	$sock_nmea_status_raw = $dbconfig->getDbconfigData('feature','gps-socket-server');
	$sock_nmea_status = (isValidOnOff($sock_nmea_status_raw) ? $sock_nmea_status_raw : '');
}


$sock_ser_gps_raw = $dbconfig->getDbconfigData('SER_GPS','sendGPS');
$sock_ser_gps = (isValidOnOff($sock_ser_gps_raw) ? $sock_ser_gps_raw : '');

if(isset($_GET['gpsSocketServerPort']) && $_GET['gpsSocketServerPort'] !== false)
{
	$sock_nmea_port = $_GET['gpsSocketServerPort'];
}
else
{
	$sock_nmea_port_raw = $dbconfig->getDbconfigData('gps-socket-server', 'listen_server_port');
	$sock_nmea_port = (isValidNumber($sock_nmea_port_raw) ? $sock_nmea_port_raw : '');
}




/* ===== SLEEP CONDITIONS TAB ====== */
$sleep_low_batt = $sleep_wifi_activity = $sleep_wifi_activity_max = $sleep_keep_awake = '';

if(isset($_GET['LowBatt']) && $_GET['LowBatt'] !== false)
{
	$sleep_low_batt = $_GET['LowBatt'];
}
else
{
	$sleep_low_batt_raw = $dbconfig->getDbconfigData('wakeup','ShutdownVoltage') / 1000;
	$sleep_low_batt = (isValidNumber($sleep_low_batt_raw) ? $sleep_low_batt_raw : '');
}

if(isset($_GET['WiFiActivity']) && $_GET['WiFiActivity'] !== false)
{
	$sleep_wifi_activity = $_GET['WiFiActivity'];
	$sleep_low_batt_raw = $dbconfig->getDbconfigData('wakeup','ShutdownVoltage') / 1000;}
else
{
	$sleep_wifi_activity_raw = $dbconfig->getDbconfigData('system','WiFiClientAliveTimeoutMinutes');
	$sleep_wifi_activity = (isValidNumber($sleep_wifi_activity_raw) ? $sleep_wifi_activity_raw : '0');
}

if(isset($_GET['WiFiActivityMax']) && $_GET['WiFiActivityMax'] !== false)
{
	$sleep_wifi_activity = $_GET['WiFiActivityMax'];
}
else
{
	$sleep_wifi_activity_max_raw = $dbconfig->getDbconfigData('system','WiFiClientMaxKeepAliveMinutes');
	$sleep_wifi_activity_max = (isValidNumber($sleep_wifi_activity_max_raw) ? $sleep_wifi_activity_max_raw : '0');
}

if(isset($_GET['ZigBeeActivity']) && $_GET['ZigBeeActivity'] !== false)
{
	$sleep_zigbee_activity = $_GET['ZigBeeActivity'];
}
else
{
	$sleep_zigbee_activity_raw = $dbconfig->getDbconfigData('zigbee','AliveTimeoutMinutes');
	$sleep_zigbee_activity = (isValidNumber($sleep_zigbee_activity_raw) ? $sleep_zigbee_activity_raw : '0');
}

if(isset($_GET['ZigBeeActivityMax']) && $_GET['ZigBeeActivityMax'] !== false)
{
	$sleep_zigbee_activity = $_GET['ZigBeeActivityMax'];
}
else
{
	$sleep_zigbee_activity_max_raw = $dbconfig->getDbconfigData('zigbee','MaxKeepAliveMinutes');
	$sleep_zigbee_activity_max = (isValidNumber($sleep_zigbee_activity_max_raw) ? $sleep_zigbee_activity_max_raw : '0');
}

if(isset($_GET['SleepKeepAwake']) && $_GET['SleepKeepAwake'] !== false)
{
	$sleep_keep_awake = $_GET['SleepKeepAwake'];
}
else
{
	$sleep_keep_awake_raw = $dbconfig->getDbconfigData('RedStone','KeepAwakeMinutes');
	$sleep_keep_awake = (isValidNumber($sleep_keep_awake_raw) ? $sleep_keep_awake_raw : '0');
}

/* ===== WAKEUP TRIGGERS TAB ====== */

$wakeup_reason = file_get_contents("/tmp/logdir/wakeup.txt");
$wakeup_time = date ("H:i:s  F d Y", filemtime('/tmp/logdir/wakeup.txt'));
$wakeup_reason2 = file_get_contents("/tmp/logprev/wakeup.txt");
$wakeup_time2 = date ("H:i:s  F d Y", filemtime('/tmp/logprev/wakeup.txt'));
// get the flags for the wakeup triggers.
$wakeup_low_battery_voltage = $dbconfig->getDbconfigData('wakeup', 'CriticalVoltage') / 1000;
$wakeup_g_force = $dbconfig->getDbconfigData('wakeup', 'AccelTriggerG') / 10;
$wakeup_battery_voltage = $dbconfig->getDbconfigData('wakeup', 'WakeupVoltage') / 1000;
$wakeup_mask = array_flip(explode(",", $dbconfig->getDbconfigData('wakeup', 'mask')));

// UseXXX is only true if it is explicitly set
$UseRTC = isset($wakeup_mask['rtc']);
$UseIgnition = isset($wakeup_mask['inp1']);
$UseAccel = isset($wakeup_mask['accel']);
$UseInp2 = isset($wakeup_mask['inp2']);
$UseIridium = isset($wakeup_mask['inp3']);
$UseCAN = isset($wakeup_mask['can']);
$UseVoltage = isset($wakeup_mask['batt_volt']);
$UseLowBatt = isset($wakeup_mask['low_batt']);




/* ===== Message Priority TAB ====== */
$mp_data = $dbconfig->getDbconfig('MSGPriority', NULL);
$mp_data_array = split("\n", $mp_data);
$mp_data_map = array();
foreach ($mp_data_array as $mp_data_row)
{
	$row = split(" ", $mp_data_row);
	$mp_data_map[hex2bin($row[0])] = trim($row[1]);
}
debug('MessagePriority|$mp_data_map', $mp_data_map);
if(isset($_GET['MP_acceleration_pri']) && $_GET['MP_acceleraion_pri'] !== false)
{
	$mp_acceleration_pri = $_GET['MP_acceleration_pri'];
}
else
{
	$mp_acceleration_pri = getSliderValue($mp_data_map['acceleration']);
}

if(isset($_GET['MP_accel_ok_pri']) && $_GET['MP_accel_ok_pri'] !== false)
{
	$mp_accel_ok_pri = $_GET['MP_accel_ok_pri'];
}
else
{
	$mp_accel_ok_pri = getSliderValue($mp_data_map['accept_accel_resumed']);
}

/* mp_accept_deccel_resumed */
if(isset($_GET['MP_decel_ok_pri']) && $_GET['MP_decel_ok_pri'] !== false)
{
	$mp_decel_ok_pri = $_GET['MP_decel_ok_pri'];
}
else
{
	$mp_decel_ok_pri = getSliderValue($mp_data_map['accept_deccel_resumed']);
}

/* calamp_user_msg */
if(isset($_GET['MP_calamp_user_pri']) && $_GET['MP_calamp_user_pri'] !== false)
{
	$mp_calamp_user_pri = $_GET['MPcalamp_user_pri'];
}
else
{
	$mp_calamp_user_pri = getSliderValue($mp_data_map['calamp_user_msg']);
}
/* check_in */
if(isset($_GET['MP_ci_pri']) && $_GET['MP_ci_pri'] !== false)
{
	$mp_ci_pri = $_GET['MP_ci_pri'];
}
else
{
	$mp_ci_pri = getSliderValue($mp_data_map['check_in']);
}

/* check_out */
if(isset($_GET['MP_co_pri']) && $_GET['MP_co_pri'] !== false)
{
	$mp_co_pri = $_GET['MP_co_pri'];
}
else
{
	$mp_co_pri = getSliderValue($mp_data_map['check_out']);
}
/* crit_batt */
if(isset($_GET['MP_crit_batt_pri']) && $_GET['MP_crit_batt_pri'] !== false)
{
	$mp_crit_batt_pri = $_GET['MP_crit_batt_pri'];
}
else
{
	$mp_crit_batt_pri = getSliderValue($mp_data_map['crit_batt']);
}
/* driver_status */
if(isset($_GET['MP_driver_status_pri']) && $_GET['MP_driver_status_pri'] !== false)
{
	$mp_driver_status_pri = $_GET['MP_driver_status_pr'];
}
else
{
	$mp_driver_status_pri = getSliderValue($mp_data_map['driver_status']);
}
if(isset($_GET['MP_engine_off_pri']) && $_GET['MP_engine_off_pri'] !== false)
{
	$mp_engine_off_pri = $_GET['MP_engine_off_pri'];
}
else
{
	$mp_engine_off_pri = getSliderValue($mp_data_map['engine_off']);
}
/* engine_on */
if(isset($_GET['MP_engine_on_pri']) && $_GET['MP_engine_on_pri'] !== false)
{
	$mp_engine_on_pri = $_GET['MP_engine_on_pri'];
}
else
{
	$mp_engine_on_pri = getSliderValue($mp_data_map['engine_on']);
}

/* heartbeat */
if(isset($_GET['MP_heartbeat_pri']) && $_GET['MP_heartbeat_pri'] !== false)
{
	$mp_heartbeat_pri = $_GET['MP_heartbeat_pri'];
}
else
{
	$mp_heartbeat_pri = getSliderValue($mp_data_map['heartbeat']);
}
/* ignition_off */
if(isset($_GET['MP_ignition_off_pri']) && $_GET['MP_ignition_off_pri'] !== false)
{
	$mp_ignition_off_pri = $_GET['MP_ignition_off_pri'];
}
else
{
	$mp_ignition_off_pri = getSliderValue($mp_data_map['ignition_off']);
}
/* ignition_on */
if(isset($_GET['MPignition_on']) && $_GET['MPignition_on'] !== false)
{
	$mp_ignition_on_pri = $_GET['MP_ignition_on'];
}
else
{
	$mp_ignition_on_pri = getSliderValue($mp_data_map['ignition_on']);
}
/* j1939 */
if(isset($_GET['MP_j1939_periodic_pri']) && $_GET['MP_j1939_periodic_pri'] !== false)
{
	$mp_j1939_periodic_pri = $_GET['MP_j1939_periodic_pri'];
}
else
{
	$mp_j1939_periodic_pri = getSliderValue(trim($dbconfig->getDbconfig('CanJ1939Monitor','overiridium_priority_periodic')));
}
/* j1939_fault */
if(isset($_GET['MP_j1939_fault_pri']) && $_GET['MP_j1939_fault_pri'] !== false)
{
	$mp_j1939_fault_pri = $_GET['MP_j1939_fault_pri'];
}
else
{
	$mp_j1939_fault_pri = getSliderValue(trim($dbconfig->getDbconfig('CanJ1939Monitor','overiridium_priority_fault')));
}
/* j1939_status2 */
if(isset($_GET['MP_j1939_exceed_pri']) && $_GET['MP_j1939_exceed_pri'] !== false)
{
	$mp_j1939_exceed_pri = $_GET['MP_j1939_exceed_pri'];
}
else
{
	$mp_j1939_exceed_pri = getSliderValue(trim($dbconfig->getDbconfig('CanJ1939Monitor','overiridium_priority_exceedance')));
}

/* Modbus */
if(isset($_GET['MP_modbus_periodic_pri']) && $_GET['MP_modbus_periodic_pri'] !== false)
{
	$mp_modbus_periodic_pri = $_GET['MP_modbus_periodic_pri'];
}
else
{
	$mp_modbus_periodic_pri = getSliderValue(trim($dbconfig->getDbconfig('modbus','overiridium_priority_periodic')));
}
/* Modbus_fault */
if(isset($_GET['MP_modbus_fault_pri']) && $_GET['MP_modbus_fault_pri'] !== false)
{
	$mp_modbus_fault_pri = $_GET['MP_modbus_fault_pri'];
}
else
{
	$mp_modbus_fault_pri = getSliderValue(trim($dbconfig->getDbconfig('modbus','overiridium_priority_fault')));
}
/* Modbus_exceedance */
if(isset($_GET['MP_modbus_exceed_pri']) && $_GET['MP_modbus_exceed_pri'] !== false)
{
	$mp_modbus_exceed_pri = $_GET['MP_modbus_exceed_pri'];
}
else
{
	$mp_modbus_exceed_pri = getSliderValue(trim($dbconfig->getDbconfig('modbus','overiridium_priority_exceedance')));
}
/* low_batt */
if(isset($_GET['MP_low_batt_pri']) && $_GET['MP_low_batt_pri'] !== false)
{
	$mp_low_batt_pri = $_GET['MP_low_batt_pri'];
}
else
{
	$mp_low_batt_pri = getSliderValue($mp_data_map['low_batt']);
}
/* sensor*/
if(isset($_GET['MP_sensor_pri']) && $_GET['MP_sensor_pri'] !== false)
{
	$mp_sensor_pri = $_GET['MP_sensor_pri'];
}
else
{
	$mp_sensor_pri = getSliderValue($mp_data_map['sensor']);
}
/* ping */
if(isset($_GET['MP_ping_pri']) && $_GET['MP_ping_pri'] !== false)
{
	$mp_ping_pri = $_GET['MP_ping_pri'];
}
else
{
	$mp_ping_pri = getSliderValue($mp_data_map['ping']);
}
/* power_off */
if(isset($_GET['MP_power_off_pri']) && $_GET['MP_power_off_pri'] !== false)
{
	$mp_power_off_pri = $_GET['MP_power_off_pri'];
}
else
{
	$mp_power_off_pri = getSliderValue($mp_data_map['power_off']);
}
/* power_on */
if(isset($_GET['MP_power_on_pri']) && $_GET['MP_power_on_pri'] !== false)
{
	$mp_power_on_pri = $_GET['MP_power_on_pri'];
}
else
{
	$mp_power_on_pri = getSliderValue($mp_data_map['power_on']);
}
/* scheduled_message */
if(isset($_GET['MP_sched_msg_pri']) && $_GET['MP_sched_msg_pri'] !== false)
{
	$mp_sched_msg_pri = $_GET['MP_sched_msg_pri'];
}
else
{
	$mp_sched_msg_pri = getSliderValue($mp_data_map['scheduled_message']);
}

/* speed_acceptable */
if(isset($_GET['MP_speed_ok_pri']) && $_GET['MP_speed_ok_pri'] !== false)
{
	$mp_speed_ok_pri = $_GET['MP_speed_ok_pri'];
}
else
{
	$mp_speed_ok_pri = getSliderValue($mp_data_map['speed_acceptable']);
}
/* speed_exceeded */
if(isset($_GET['MP_speed_exceed_pri']) && $_GET['MP_speed_exceed_pri'] !== false)
{
	$mp_speed_exceed_pri = $_GET['MP_speed_exceed_pri'];
}
else
{
	$mp_speed_exceed_pri = getSliderValue($mp_data_map['speed_exceeded']);
}
/* start_condition */
if(isset($_GET['MP_start_pri']) && $_GET['MP_start_pri'] !== false)
{
	$mp_start_pri = $_GET['MP_start_pri'];
}
else
{
	$mp_start_pri = getSliderValue($mp_data_map['start_condition']);
}
/* stop_condition */
if(isset($_GET['MP_stop_pri']) && $_GET['MP_stop_pri'] !== false)
{
	$mp_stop_pri = $_GET['MP_stop_pri'];
}
else
{
	$mp_stop_pri = getSliderValue($mp_data_map['stop_condition']);
}

/* ===== END OF Message Priority TAB ====== */

/* ===== COM Ports TAB ====== */
/* Enable COM 1 */
if(isset($_GET['CPCom1Enable']) && $_GET['CPCom1Enable'] !== false)
{
	$cp_Com1Enable = $_GET['CPCom1Enable'];
}
else
{
	$cp_Com1Enable = $dbconfig->getDbconfigData('ComPorts','Com1Enable');
}

/* Dest COM 1 */
if(isset($_GET['CPCom1Dest']) && $_GET['CPCom1Dest'] !== false)
{
	$cp_Com1Dest = $_GET['CPCom1Dest'];
}
else
{
	$cp_Com1Dest = $dbconfig->getDbconfigData('ComPorts','Com1Dest');
}

/* Port COM 1 */
if(isset($_GET['CPCom1Port']) && $_GET['CPCom1Port'] !== false)
{
	$cp_Com1Port = $_GET['CPCom1Port'];
}
else
{
	$cp_Com1Port = $dbconfig->getDbconfigData('ComPorts','Com1Port');
}

/* Baud COM 1 */
if(isset($_GET['CPCom1Baud']) && $_GET['CPCom1Baud'] !== false)
{
	$cp_Com1Baud = $_GET['CPCom1Baud'];
}
else
{
	$cp_Com1Baud = $dbconfig->getDbconfigData('ComPorts','Com1Baud');
}

if(isset($_GET['CPCom2Enable']) && $_GET['CPCom2Enable'] !== false)
{
	$cp_Com2Enable = $_GET['CPCom2Enable'];
}
else
{
	$cp_Com2Enable = $dbconfig->getDbconfigData('ComPorts','Com2Enable');
}

/* Dest COM 1 */
if(isset($_GET['CPCom2Dest']) && $_GET['CPCom2Dest'] !== false)
{
	$cp_Com2Dest = $_GET['CPCom2Dest'];
}
else
{
	$cp_Com2Dest = $dbconfig->getDbconfigData('ComPorts','Com2Dest');
}

/* Port COM 1 */
if(isset($_GET['CPCom2Port']) && $_GET['CPCom2Port'] !== false)
{
	$cp_Com2Port = $_GET['CPCom2Port'];
}
else
{
	$cp_Com2Port = $dbconfig->getDbconfigData('ComPorts','Com2Port');
}

/* Baud COM 1 */
if(isset($_GET['CPCom2Baud']) && $_GET['CPCom2Baud'] !== false)
{
	$cp_Com2Baud = $_GET['CPCom2Baud'];
}
else
{
	$cp_Com2Baud = $dbconfig->getDbconfigData('ComPorts','Com2Baud');
}

/* ===== END OF COM Ports TAB ====== */
?>

