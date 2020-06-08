<?php
debug('../inc/status_view.php entry');
require_once $_SERVER['DOCUMENT_ROOT'].'inc/dbconfig_controller.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'inc/util.inc';				//contains functions for input validation, socket interaction, error message display, and logging.
debug('../inc/status_view.php 1');
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

debug('../inc/status_view.php 1');
//OBJECT INSTANTIATION
$dbconfig = new dbconfigController();

$cams_status = $cams_host = $cams_host_type = $cams_port =  $cams_compression = $iridium_status = $last_heartbeat ='';

debug('../inc/status_view.php 1');
$cams_status_raw = $dbconfig->getDbconfigData('feature','packetizer-cams');
$cams_status = (isValidOnOff($cams_status_raw) ? $cams_status_raw : '');

$iridium_status_raw = $dbconfig->getDbconfigData('feature','iridium-monitor');
$iridium_status = (isValidOnOff($iridium_status_raw) ? $iridium_status_raw : '');

$cams_host_raw = $dbconfig->getDbconfigData('packetizer-cams', 'host');
$cams_host = ((isValidIP($cams_host_raw) || isValidString($cams_host_raw)) ? $cams_host_raw : '');

$cams_port_raw = $dbconfig->getDbconfigData('packetizer-cams', 'port');
$cams_port = (isValidNumber($cams_port_raw) ? $cams_port_raw : '');

$cams_compression_raw = $dbconfig->getDbconfigData('packetizer-cams', 'UseCompression');
$cams_compression = (isValidOnOff($cams_compression_raw) ? $cams_compression_raw : '');

$iridiumEnable_raw = $dbconfig->getDbconfigData('packetizer-cams', 'IridiumEnable');
$iridiumEnable = (isValidOnOff($iridiumEnable_raw) ? $iridiumEnable_raw : '');

$last_heartbeat_raw = $dbconfig->getDbconfigData('heartbeat', 'LastHeartbeat');
$last_heartbeat = $last_heartbeat_raw;

$wakeup_reason = file_get_contents("/tmp/logdir/wakeup.txt");
$wakeup_time = date ("H:i:s  F d Y", filemtime('/tmp/logdir/wakeup.txt'));
debug('../inc/status_view.php end');
?>

