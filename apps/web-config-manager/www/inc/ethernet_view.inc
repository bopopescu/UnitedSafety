<?php

require_once $_SERVER['DOCUMENT_ROOT'].'/inc/dbconfig_controller.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/network_controller.inc'; //network (ethernet, wireless) controller
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/dhcp_controller.inc';	//dhcp controller
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';				//contains functions for socket interaction, error message display, and logging.
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/db_sqlite3.inc';		//contains functions for db interaction


if(!empty($_GET))
{
	if(!empty($_GET['codes']))
	{
		foreach(explode(",",$_GET['codes']) as $key)
		{
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


//OBJECT INSTANTIATION
$nt_ctrl = new networkcontroller();
$dhcp_ctrl = new dhcpcontroller();
$dbconfig = new dbconfigController();

//VARIABLE INSTANTIATION
$interface= ethernet;

$eip = $cip = $emask = $emac = '';
$edhcp_status = $edhcp_startip = $edhcp_endip = $edhcp_leases = '';
$dns = $dns1 = $dns2 = '';

$useDefaultRoute = $overrideDefaultRoute = $RouteIP = '';

// MAC
if(isset($_GET['mac']) && $_GET['mac'] !== false)
{
	$emac_raw = $_GET['mac'];
	$emac = explode(":", $emac_raw);
}
else
{
	$emac_raw = $dbconfig->getDbconfigData('system', 'ethmacaddr');
	$emac = (isValidMAC($emac_raw) ? explode(":", $emac_raw) : array(" ", " ", " ", " ", " ", " "));
}

// IP
if(isset($_GET['ip']) && $_GET['ip'] !== false)
{
	$eip_raw = $_GET['ip'];
	$eip = explode(".", $eip_raw);
}
else
{
	$eip_raw = $dbconfig->getDbconfigData('system', 'eth0addr');
	$eip = (isValidIP($eip_raw) ? explode(".", $eip_raw) : array(" ", " ", " ", " "));
}

{
	$cip_raw = exec('ifconfig eth0 | grep '.escapeshellarg('inet addr:').' | cut -d '.escapeshellarg(':').' -f 2 | awk '.escapeshellarg('{print $1}'));
	$cip = (isValidIP($cip_raw) ? explode(".", $cip_raw) : array(" ", " ", " ", " "));
}

// Subnet Mask
if(isset($_GET['mask']) && $_GET['mask'] !== false)
{
	$emask_raw = $_GET['mask'];
	$emask = explode(".", $emask_raw);
}
else
{
	$emask_raw = $dbconfig->getDbconfigData('system', 'eth0mask');
	$emask = (isValidIP($emask_raw) ? explode(".", $emask_raw) : array("255", "255", "255", "0"));
}


// DHCP Status
if(isset($_GET['dhcpserver']) && $_GET['dhcpserver'] !== false)
{
	$edhcp_status_raw = $_GET['dhcpserver'];

	if (empty($edhcp_status_raw) || $edhcp_status_raw == "auto")
	  $edhcp_status = "auto";
	else
		$edhcp_status = ((!empty($edhcp_status_raw) && $edhcp_status_raw == "Enabled") ? "on" : "off");
}
else
{
	$edhcp_status_raw = $dbconfig->getDbconfigData('system', 'eth0dhcp');

	if (empty($edhcp_status_raw) || $edhcp_status_raw == "auto")
		$edhcp_status = "auto";		
	elseif ($edhcp_status_raw == "client")
		$edhcp_status = "on";
	else
		$edhcp_status = ((!empty($edhcp_status_raw) && $edhcp_status_raw == "On") ? "on" : "off");
}

// DHCP START IP
if(isset($_GET['dhcpsip']) && $_GET['dhcpsip'] !== false)
{
	$edhcp_startip_raw = $_GET['dhcpsip'];
	$edhcp_startip = explode(".", $edhcp_startip_raw);
}
else
{
	$edhcp_startip_raw = $dbconfig->getDbconfigData('system', 'eth0startip');
	$edhcp_startip = (isValidIP($edhcp_startip_raw) ? explode(".", $edhcp_startip_raw) : array(" ", " ", " ", " "));
}

// DHCP END IP
if(isset($_GET['dhcpeip']) && $_GET['dhcpeip'] !== false)
{
	$edhcp_endip_raw = $_GET['dhcpeip'];
	$edhcp_endip = explode(".", $edhcp_endip_raw);
}
else
{
	$edhcp_endip_raw = $dbconfig->getDbconfigData('system', 'eth0endip');
	$edhcp_endip = (isValidIP($edhcp_endip_raw) ? explode(".", $edhcp_endip_raw) : array(" ", " ", " ", " "));
}

// DHCP LEASES
$edhcp_leases = $dhcp_ctrl->getActiveDhcpLeases($eip);	//get active dhcp leases

// DHCP Route Override

// Use as default route - if on others should be disabled
$useDefaultRoute_raw = $dbconfig->getDbconfigData('system', 'RouteOverride');

if ($useDefaultRoute_raw == "primary" || $useDefaultRoute_raw == "secondary")
	$useDefaultRoute = $useDefaultRoute_raw;
else
	$useDefaultRoute = "none";

//Route IP address - last octet of IP address - rest will be the eth0 supplied ip address
$RouteIP = $dbconfig->getDbconfigData('system', 'RouteIP');

//READ DNS SETTINGS
$dns = $nt_ctrl->getDNS();

//parse DNS info
$dns1 = explode(".", $dns['0']);
if(count($dns) > 1)
{
	$dns2 = explode(".", $dns['1']);
}



?>
