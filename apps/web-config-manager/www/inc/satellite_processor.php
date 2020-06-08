<?php
require_once $_SERVER['DOCUMENT_ROOT'].'inc/session_validator.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'inc/dbconfig_controller.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';				//contains functions for socket interaction, error message display, and logging.

//OBJECT INSTANTIATION
$dbconfig = new dbconfigController();


//Check form submission
if(!empty($_REQUEST) && !empty($_REQUEST['csrfToken']) && $_REQUEST['csrfToken'] == $_SESSION['csrfToken'] && isset($_SESSION['M2M_SESH_USERAL']) && $_SESSION['M2M_SESH_USERAL'] == 200)
{
	debug('=========_REQUEST=============', $_REQUEST);	//DEBUG

	$result = submitSatellite($dbconfig, trimRequest($_REQUEST));
	header("location:http://".$_SERVER['HTTP_HOST']."/network/iridium/index.php?success=".$result['success']."&module=".$result['module']."&codes=".implode(",",$result['codes'])."&fields=".$result['fields'].$result['getParams']);
}
else
{
	header("location:http://".$_SERVER['HTTP_HOST']."/network/iridium/index.php");
}


/**
 * submitSatellite
 * Saves the settings to dbconfig using the dbconfig wrapper
 * @param object $dbconfig - wrapper class for dbconfig interaction
 * @param array $request - form REQUEST array
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function submitSatellite($dbconfig, $request)
{
	debug('submitSatellite', $request);	//DEBUG

	$result = array("success" => 'false', "module" => "Satellite", "codes" => array(), "fields" => null, "getParams" => null);	//array for capturing result status, status code, and field names.

	$Satellite_result = array();	//store the success/failure state for each setting

	if(isValidOnOff($request['IridiumEnableCtl']))
	{
		$Satellite_result['IridiumEnableCtl'] = $dbconfig->setDbconfigData('feature', 'iridium-monitor', isOn($request['IridiumEnableCtl']) ? 1 : 0);
		$dbconfig->setDbconfigData('packetizer-cams', 'IridiumEnable', isOn($request['IridiumEnableCtl']) ? "On" : "Off");
	}

	if(isOn($request['IridiumEnableCtl']))
	{
		$Satellite_result['IridiumDataLimit'] = (isValidNumber($request['IridiumDataLimit'])? $dbconfig->setDbconfigData('Iridium', 'byteLimit', $request['IridiumDataLimit']): false);
		$Satellite_result['IridiumDataLimitTimeout'] = (isValidNumber($request['IridiumDataLimitTimeout'])? $dbconfig->setDbconfigData('Iridium', 'LimitTimePeriod', $request['IridiumDataLimitTimeout']): false);
	}

	if (isValidNumber($request['IridiumUpdateIntervalCtl']))
		$Satellite_result['IridiumUpdateIntervalCtl'] = $dbconfig->setDbconfigData('PositionUpdate', 'IridiumReportTime',$request['IridiumUpdateIntervalCtl']);

	if (isValidNumber($request['ModbusReportingIntervalCtl']) )
	{
		$timeout = $request['ModbusReportingIntervalCtl'] * 60;
		$Satellite_result['ModbusReportingIntervalCtl'] = $dbconfig->setDbconfigData('modbus', 'periodic_overiridium_seconds', $timeout);
	}
	if(!isset($request['IridiumEnable']))
	{
		$request['IridiumEnable'] = 'Off';
	}

	if(isOn($request['IridiumEnable']))
	{
		//CAMS Irdium Settings
		$Satellite_result['IridiumPri'] = (isValidNumber($request['IridiumPri'])? $dbconfig->setDbconfigData('packetizer-cams', 'IridiumPriorityLevel', $request['IridiumPri']): false);
		$Satellite_result['camsRetries'] = (isValidNumber($request['camsRetries'])? $dbconfig->setDbconfigData('packetizer-cams', 'retry_limit', $request['camsRetries']) : false);
		$Satellite_result['CellFailMode'] = (isValidNumber($request['CellFailMode'])? $dbconfig->setDbconfigData('packetizer-cams', 'CellFailModeEnable', $request['CellFailMode']): false);
		$Satellite_result['camsIridiumTimeout'] = (isValidNumber($request['camsIridiumTimeout'])? $dbconfig->setDbconfigData('packetizer-cams', 'iridium_timeout', $request['camsIridiumTimeout']): false);
		$Satellite_result['camsIridiumDataLimitPriority'] =
		(isValidNumber($request['camsIridiumDataLimitPriority'])? $dbconfig->setDbconfigData('packetizer-cams', 'IridiumDataLimitPriority', $request['camsIridiumDataLimitPriority']): false);
	}

	debug('(Satellite_processor.php|submitSatellite()) $Satellite_result: ', $Satellite_result); 	//DEBUG

	// 1) find all the keys in the $Satellite_result array that have a value of false (these are the API calls that failed)
	// 2) build a string with the keys. (The key names are the same as the html element (input/radio/select) names and will be used to highlight the fields with jquery)
	$failed_results = array_keys($Satellite_result, false, true);
	$result['fields'] = implode(',', $failed_results);
	debug('(Satellite_processor.php|submitSatellite()) $result[\'fields\']: ', $result['fields']); 	//DEBUG

	foreach($failed_results as $field)
	{
		$result['getParams'] .= '&'.$field.'='.$request[$field];
	}

	if(empty($result['fields']))
	{
		debug('(Satellite_processor.php) setting Satellite signals'); 	//DEBUG

		$result['success'] = 'true';
		$result['codes'][] = 10;
		$result['codes'][] = 14;
	}
	else
	{
		$result['success'] = 'false';
		$result['codes'][] = 12;
	}

	debug('(Satellite_processor.php|submitSatellite()) $result: ', $result); 	//DEBUG
	return $result;

} //END submitSatellite

?>
