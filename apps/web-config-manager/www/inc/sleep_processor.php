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

	$result = submitSleep($dbconfig, trimRequest($_REQUEST));
	header("location:http://".$_SERVER['HTTP_HOST']."/device/settings/index.php?success=".$result['success']."&module=".$result['module']."&codes=".implode(",",$result['codes'])."&fields=".$result['fields'].$result['getParams']);
}
else
{
	header("location:http://".$_SERVER['HTTP_HOST']."/device/settings/index.php");
}


/**
 * submitsleep
 * Saves the settings to dbconfig using the dbconfig wrapper
 * @param object $dbconfig - wrapper class for dbconfig interaction
 * @param array $request - form REQUEST array
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function submitSleep($dbconfig, $request)
{
	debug('', $request);	//DEBUG
	$result = array("success" => 'false', "module" => "Sleep", "codes" => array(), "fields" => null, "getParams" => null);	//array for capturing result status, status code, and field names.

	$sleep_result = array();	//store the success/failure state for each setting

	$sleep_result['SleepKeepAwake'] = (isValidNumber($request['SleepKeepAwake']) ? $dbconfig->setDbconfigData('RedStone', 'KeepAwakeMinutes', $request['SleepKeepAwake']) : false);
	//$sleep_result['LowBatt'] = (isValidNumber($request['LowBatt']) ? $dbconfig->setDbconfigData('wakeup', 'ShutdownVoltage', $request['LowBatt'] * 1000) : false);
	//$sleep_result['wakeupLowBattV'] = (isValidNumber($request['wakeupLowBattV']) ? $dbconfig->setDbconfigData('wakeup', 'CriticalVoltage', $request['wakeupLowBattV'] * 1000) : false);
	if(isValidNumber($request['wakeupLowBattV']) && isValidNumber($request['maxCriticalVoltage']) && isValidNumber($request['minCriticalVoltage']) && $request['wakeupLowBattV'] >= $request['minCriticalVoltage'] && $request['wakeupLowBattV'] <= $request['maxCriticalVoltage']){
		$sleep_result['wakeupLowBattV'] = $dbconfig->setDbconfigData('wakeup', 'CriticalVoltage', $request['wakeupLowBattV'] * 1000);
	} else {
		$sleep_result['wakeupLowBattV'] = false;
	}

	debug('(sleep_processor.php|submitSleep()) $sleep_result: ', $sleep_result); 	//DEBUG

	// 1) find all the keys in the $sleep_result array that have a value of false (these are the API calls that failed)
	// 2) build a string with the keys. (The key names are the same as the html element (input/radio/select) names and will be used to highlight the fields with jquery)
	$failed_results = array_keys($sleep_result, false, true);
	$result['fields'] = implode(',', $failed_results);
	debug('(sleep_processor.php|submitSleep()) $result[\'fields\']: ', $result['fields']); 	//DEBUG

	foreach($failed_results as $field)
	{
		$result['getParams'] .= '&'.$field.'='.$request[$field];
	}

	if(empty($result['fields']))
	{
		$result['success'] = 'true';
		$result['codes'][] = 10;
		$result['codes'][] = 14;
	}
	else
	{
		$result['success'] = 'false';
		$result['codes'][] = 12;
	}

	debug('(sleep_processor.php|submitSleep()) $result: ', $result); 	//DEBUG
	return $result;

} //END submitsleep

?>
