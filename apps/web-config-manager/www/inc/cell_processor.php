<?php
require_once $_SERVER['DOCUMENT_ROOT'].'inc/session_validator.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'inc/dbconfig_controller.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'inc/cell_controller.inc';	//cellular controller
require_once $_SERVER['DOCUMENT_ROOT'].'inc/ipsec_controller.inc';	//ipsec controller
require_once $_SERVER['DOCUMENT_ROOT'].'inc/util.inc';				//contains functions for socket interaction, error message display, and logging.
require_once $_SERVER['DOCUMENT_ROOT'].'inc/db_sqlite3.inc';		//contains functions for db interaction


//OBJECT INSTANTIATION
$cell_ctrl = new cellcontroller();
$ipsec_ctrl = new ipseccontroller();
$dbconfig = new dbconfigController();

//Check form submission
if(!empty($_REQUEST) && !empty($_REQUEST['csrfToken']) && $_REQUEST['csrfToken'] == $_SESSION['csrfToken']  && isset($_SESSION['M2M_SESH_USERAL']) && $_SESSION['M2M_SESH_USERAL'] == 200 && $_REQUEST['network'] == 'cellular')
{
	debug('=========_REQUEST=============', $_REQUEST);	//DEBUG

	$result = submitCellular($cell_ctrl, $ipsec_ctrl, $dbconfig, trimRequest($_REQUEST));

	header("location:http://".$_SERVER['HTTP_HOST']."/network/cellular/index.php?success=".$result['success']."&module=".$result['module']."&codes=".implode(",",$result['codes'])."&fields=".$result['fields'].$result['getParams']);

}
else
{
	header("location:http://".$_SERVER['HTTP_HOST']."/network/cellular/index.php");
}


/**
 * submitCellular
 * Submits the cellular form content to the cellular controller for processing
 * @param object $cell_ctrl
 * @param object $ipsec_ctrl
 * @param array $request
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function submitCellular($cell_ctrl, $ipsec_ctrl, $dbconfig, $request)
{
	$result = array("success" => 'false', "module" => "cell", "codes" => array(),  "fields" => null, "getParams" => null);	//array for capturing result status, status code, and field names.

	//check if an ipsec policy is active
	$ipsecstatus = false;
	$ipsecPolicy = $ipsec_ctrl->getActivePolicyName();	//get the policy name of the latest ipsec policy
	debug('(cell_processor.php|submitCellular()) latest vpn policy on device', $ipsecPolicy); 	//DEBUG

	if($ipsecPolicy)
	{
		$ipsecstatus = $ipsec_ctrl->getIpsecStatus($ipsecPolicy);	//check whether it's active
		debug('(cell_processor.php|submitCellular()) status of vpn policy', $ipsecstatus); 	//DEBUG
	}

	$dbconfig->setDbconfigData('Cellular','UserName', htmlspecialchars(strip_tags($request['sim_usr'])));
	$dbconfig->setDbconfigData('Cellular','Password', htmlspecialchars(strip_tags($request['sim_pwd'])));
	//set apn
	$result_apn = (isValidString($request['apn']) ? $dbconfig->setDbconfigData('Cellular','carrier', htmlspecialchars(strip_tags($request['apn']))) : false);

	if($result_apn === true)	//if setapn call was successfull
	{
		$result['success'] = 'true';
		$result['codes'][] = 10;
		$result['codes'][] = 14;

		debug('(cell_processor.php|submitCellular()) Cellular apn saved');					//DEBUG

		//if there was an active ipsec policy, restart the ipsec service
		if($ipsecstatus)
		{
			$result['codes'][] = 402;
			debug('(cell_processor.php|submitCellular()) Restarting ipsec service...');		//DEBUG
			if($ipsec_ctrl->ipsecRestart())
				$result['codes'][] = 400;
			else
				$result['codes'][] = 401;
		}

	}
	else					//if setapn call was unsuccessfull
	{
		debug('(cell_processor.php|submitCellular()) Failed to save cellular apn.');	//DEBUG
		$result['success'] = 'false';
		$result['codes'][] = 12;

		$result['fields'] .= 'apn';
	}

	//$result['getParams'] .= '&apn='.$request['apn'];

	return $result;

} //END submitCellular

?>
