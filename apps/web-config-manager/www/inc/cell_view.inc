<?php 

require_once $_SERVER['DOCUMENT_ROOT'].'inc/dbconfig_controller.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'inc/cell_controller.inc';	//cellular controller
require_once $_SERVER['DOCUMENT_ROOT'].'inc/util.inc';				//contains functions for socket interaction, error message display, and logging.

//OBJECT INSTANTIATION
$dbconfig = new dbconfigController();
$cell_ctrl = new cellcontroller();

//VARIABLE INSTANTIATION
//cellular
$cell_imei = $cell_sim = $cell_phone = $cell_rssi = $cell_network = $cell_dns = $cell_apn = $cell_carrier = '';
$newSettings = false;

// set the cell rssi value
$cell_rssi_raw = $cell_ctrl->getRssi();
$cell_rssi = (is_numeric($cell_rssi_raw) ? $cell_rssi_raw.' dBm' : '');

if(!empty($_GET) && $_GET['op'] == 'RSSIupdate')
{
	echo trim($cell_rssi);
}
else if(!empty($_GET))
{
	if(!empty($_GET['codes']))
	{
		foreach(explode(",",$_GET['codes']) as $key)
		{
			if($key == "10") //indicates settings have been saved
				$newSettings = true;
			
			translateStatusCode($key);
		}
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



//READ CELL
$cell_imei_raw = $cell_ctrl->getImei();
$cell_imei = (isValidNumber($cell_imei_raw) ? $cell_imei_raw : '');

$cell_rssi_raw = $cell_ctrl->getRssi();
$cell_rssi = (is_numeric($cell_rssi_raw) ? $cell_rssi_raw.' dBm' : '');

$cell_sim_raw = $cell_ctrl->getSim();
$cell_sim = (isValidNumber($cell_sim_raw) ? $cell_sim_raw : '');

$cell_phone_raw = $cell_ctrl->getPhoneNumber();
$cell_phone = (isValidNumber($cell_phone_raw) ? $cell_phone_raw : '');

$cell_carrier_raw = $cell_ctrl->getCarrier();
$cell_carrier = (isValidString($cell_carrier_raw) ? $cell_carrier_raw : '');

if(!empty($_GET['apn']))
{
	$cell_apn = $_GET['apn'];
}
else
{
	//$cell_apn_raw = $cell_ctrl->getApn();
	$cell_apn_raw = $dbconfig->getDbconfigData('Cellular','carrier');
	$cell_apn = (isValidString($cell_apn_raw) ? $cell_apn_raw : '');
}
$cell_user = $dbconfig->getDbconfigData('Cellular','UserName');
$cell_pwd = $dbconfig->getDbconfigData('Cellular','Password');

$cell_network_raw = $cell_ctrl->getCellNetwork('ppp0');
$cell_network_ip = (!empty($cell_network_raw['ip']) && isValidIP($cell_network_raw['ip']) ? $cell_network_raw['ip'] : '');
$cell_network_mask = (!empty($cell_network_raw['mask']) && isValidIP($cell_network_raw['mask']) ? $cell_network_raw['mask'] : '');


$cell_dns_raw = $cell_ctrl->getCellDns();
$cell_dns_pri = (!empty($cell_dns_raw[0]) && isValidIP($cell_dns_raw[0]) ? $cell_dns_raw[0] : '');
$cell_dns_sec = (!empty($cell_dns_raw[1]) && isValidIP($cell_dns_raw[1]) ? $cell_dns_raw[1] : '');


//display warning to check sim only if the APN or WAN IP are missing
//do not display the warning if settings have been changed on the page but the system hasn't been rebooted yet. 
if((empty($cell_apn) || empty($cell_network_ip)) && !$newSettings)
{
	translateStatusCode('302', 'cell');
}

?>
