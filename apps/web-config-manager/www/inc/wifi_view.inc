<?php
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/dbconfig_controller.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/network_controller.inc'; //network (ethernet, wireless) controller
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/wifi_controller.inc';	//wifi (AP + client) controller
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/dhcp_controller.inc';	//dhcp controller
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';				//contains functions for socket interaction, error message display, and logging.
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/db_sqlite3.inc';		//contains functions for db interaction

if(!empty($_GET))
{
	if(!empty($_GET['codes']))
	{
		foreach(explode(",",$_GET['codes']) as $key)
		{
			translateStatusCode($key, $_GET['module']);
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

//OBJECT INSTANTIATION
$nt_ctrl = new networkcontroller();
$wifi_ctrl = new wificontroller();
$dhcp_ctrl = new dhcpcontroller();
$dbconfig = new dbconfigController();

//VARIABLE INSTANTIATION
//wifi
$interface = wireless;	//see config.inc

$wip = $wmask = $wgateway = $wmac = '';
$wdhcp_status = $wdhcp_startip = $wdhcp_endip = $wdhcp_leases = '';
$dns = $dns1 = $dns2 = '';
//wifi ap
$wssid = $wauth = $wencrypt = '';

//READ WIFI AP
$wifi_ap = $wifi_ctrl->getAPinfo();

// SSID
if(isset($_GET['ssid']) && $_GET['ssid'] !== false)
{
	$wssid = $_GET['ssid'];
}
else
{
	$wssid = (!empty($wifi_ap['ssid']) ? $wifi_ap['ssid'] : "");
}

// AUTH
if(isset($_GET['authtype']) && $_GET['authtype'] !== false)
{
	$wauth = $_GET['authtype'];
}
else
{
	$wauth = (!empty($wifi_ap['auth']) ? $wifi_ap['auth'] : "");
}

// ENCRYP TYPE
if(isset($_GET['encryptype']) && $_GET['encryptype'] !== false)
{
	$wencrypt = $_GET['encryptype'];
}
else
{
	$wencrypt = (!empty($wifi_ap['encryp']) ? $wifi_ap['encryp'] : "");
}

$f = fopen('/mnt/nvram/rom/wifimacaddr.txt', 'r');
$wmac_raw = fgets($f);
fclose($f);
$wmac = (isValidMAC($wmac_raw) ? explode(":", $wmac_raw) : array(" ", " ", " ", " ", " ", " "));

// IP
if(isset($_GET['ip']) && $_GET['ip'] !== false)
{
	$wip_raw = $_GET['ip'];
	$wip = explode(".", $wip_raw);
}
else
{
	$wip_raw = $dbconfig->getDbconfigData('system', 'ra0addr');
	$wip = (isValidIP($wip_raw) ? explode(".", $wip_raw) : array(" ", " ", " ", " "));
}

// Subnet Mask
if(isset($_GET['mask']) && $_GET['mask'] !== false)
{
	$wmask_raw = $_GET['mask'];
	$wmask = explode(".", $wmask_raw);
}
else
{
	$wmask_raw = $dbconfig->getDbconfigData('system', 'ra0mask');
	$wmask = (isValidIP($wmask_raw) ? explode(".", $wmask_raw) : array("255", "255", "255", "0"));
}

// DHCP Status
if(isset($_GET['dhcpserver']) && $_GET['dhcpserver'] !== false)
{
	$wdhcp_status_raw = $_GET['dhcpserver'];
	$wdhcp_status = ((!empty($wdhcp_status_raw) && $wdhcp_status_raw == "Enabled") ? "on" : "off");
}
else
{
	$wdhcp_status_raw = $dbconfig->getDbconfigData('system', 'ra0dhcp');
	$wdhcp_status = $wdhcp_status_raw;//((isset($wdhcp_status_raw) && ($wdhcp_status_raw === "on" || $wdhcp_status_raw === 1)) ? "off" : "on");
}

// DHCP START IP
if(isset($_GET['dhcpsip']) && $_GET['dhcpsip'] !== false)
{
	$wdhcp_startip_raw = $_GET['dhcpsip'];
	$wdhcp_startip = explode(".", $wdhcp_startip_raw);
}
else
{
	$wdhcp_startip_raw = $dbconfig->getDbconfigData('system', 'ra0startip');
	$wdhcp_startip = (isValidIP($wdhcp_startip_raw) ? explode(".", $wdhcp_startip_raw) : array(" ", " ", " ", " "));
}

// DHCP END IP
if(isset($_GET['dhcpeip']) && $_GET['dhcpeip'] !== false)
{
	$wdhcp_endip_raw = $_GET['dhcpeip'];
	$wdhcp_endip = explode(".", $wdhcp_endip_raw);
}
else
{
	$wdhcp_endip_raw = $dbconfig->getDbconfigData('system', 'ra0endip');
	$wdhcp_endip = (isValidIP($wdhcp_endip_raw) ? explode(".", $wdhcp_endip_raw) : array(" ", " ", " ", " "));
}

// DHCP LEASES
$wdhcp_leases = $dhcp_ctrl->getActiveDhcpLeases($wip);	//get active dhcp leases

//READ DNS
$dns = $nt_ctrl->getDNS();

//parse DNS info
$dns1 = explode(".", $dns['0']);
if(count($dns) > 1)
{
	$dns2 = explode(".", $dns['1']);
}

$wifi_ap_enable_status = (isset($_GET['wifi-ap-enable']) && $_GET['wifi-ap-enable'] !== false) ?
	$wifi_ap_enable_status = $_GET['wifi-ap-enable']
	:
	$wifi_ap_enable_status = hex2bin(trim($dbconfig->getDbconfig('WiFi','ap-enabled')));
debug("wifi_view.inc ".$wifi_ap_enable_status);

if(!isValidOnOff($wifi_ap_enable_status))
{
	$wifi_ap_enable_status = '1';
}

$wifi_client_enable_status = (isset($_GET['wifi-client-enable']) && $_GET['wifi-client-enable'] !== false) ?
	$wifi_client_enable_status = $_GET['wifi-client-enable']
	:
	$wifi_client_enable_status = hex2bin(trim($dbconfig->getDbconfig('feature','wifi-monitor')));

if(!isValidOnOff($wifi_client_enable_status))
{
	$wifi_client_enable_status = '';
}

$wifi_ssh_enable_status = (isset($_GET['wifi-ssh-enable']) && $_GET['wifi-ssh-enable'] !== false) ?
	$wifi_ssh_enable_status = $_GET['wifi-ssh-enable']
	:
	$wifi_ssh_enable_status = hex2bin(trim($dbconfig->getDbconfig('WiFi','EnableSSH')));
	debug("wifi_view.inc EnableSSH".$wifi_ssh_enable_status);

if(!isValidOnOff($wifi_ssh_enable_status))
{
	$wifi_ssh_enable_status = '0';  // default to off since that is the default in the TRULink
}
?>
