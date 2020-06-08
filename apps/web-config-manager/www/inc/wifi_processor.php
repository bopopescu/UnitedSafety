<?php
require_once $_SERVER['DOCUMENT_ROOT'].'inc/session_validator.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'inc/dbconfig_controller.inc';
//require_once $_SERVER['DOCUMENT_ROOT'].'/inc/network_controller.inc'; //network (ethernet, wireless) controller
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/wifi_controller.inc';	//wifi (AP + client) controller
//require_once $_SERVER['DOCUMENT_ROOT'].'/inc/dhcp_controller.inc';	//dhcp controller
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';				//contains functions for socket interaction, error message display, and logging.
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/db_sqlite3.inc';		//contains functions for db interaction

//OBJECT INSTANTIATION
//$nt_ctrl = new networkcontroller();
$wifi_ctrl = new wificontroller();
//$dhcp_ctrl = new dhcpcontroller();
$dbconfig = new dbconfigController();

//Check form submission
if(!empty($_REQUEST) && !empty($_REQUEST['csrfToken']) && $_REQUEST['csrfToken'] == $_SESSION['csrfToken'] && isset($_SESSION['M2M_SESH_USERAL']) && $_SESSION['M2M_SESH_USERAL'] == 200)
{
	debug('=========_REQUEST=============', $_REQUEST);	//DEBUG

	//WiFi form submission
	$result = submitWifi($wifi_ctrl, $dbconfig, trimRequest($_REQUEST));
	header("location:http://".$_SERVER['HTTP_HOST']."/network/wifi/index.php?success=".$result['success']."&module=".$result['module']."&codes=".implode(",",$result['codes'])."&fields=".$result['fields'].$result['getParams']);
}
else
{
	header("location:http://".$_SERVER['HTTP_HOST']."/network/wifi/index.php");
}


/**
 * submitWifi
 *
 * Submits the wifi form content to the wifi controller for processing.
 * If Wifi settings are successfully saved, then proceeds to save DHCP settings.
 * @param object $wifi_ctrl wifi controller object
 * @param object $dbconfig db controller object
 * @param array - the _REQUEST variable that contains the form submission data
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function submitWifi($wifi_ctrl, $dbconfig, $request)
{
	$result = array("success" => "false", "module" => "wifi", "codes" => array(), "fields" => null, "getParams" => null);	//array for capturing result status, status code, and field names.

	//assign wifi ap data from
	$interface = array();
	$interface['name'] = wireless;
	$interface['type'] = 'wifiap';

	$wifi_result = array();		//store the success/failure state for each setting
	$wifi_net_result = false;
	$wifi_ap_result = false;
	$result_ap_fail_codes = array();
	$error_fields_highlighted = false;
	$wifi_reset = array(); //array for capturing each settings to restore on failure

	// Get current Wifi Enable setting
	$wifi_reset['WiFi'] = ['WiFi', 'ap-enabled', $dbconfig->getDbconfigData('WiFi', 'ap-enabled')];

	if ($wifi_reset['WiFi'][2] != 1) {
    // Set WiFi to enable 
		$dbconfig->setDbconfigData('WiFi', 'ap-enabled', 1);
	}

	// Get current Wifi SSH Enable value
	$wifi_reset['EnableSSH'] = ['WiFi', 'EnableSSH', $dbconfig->getDbconfigData('WiFi', 'EnableSSH')];
	if ($wifi_reset['EnableSSH'][2] != $request['wifi-ssh-enable']) {
		if(isValidOnOff(isset($request['wifi-ssh-enable']) ? $request['wifi-ssh-enable'] : true)) {
			// Set Wifi SSH Enable value
			$dbconfig->setDbconfigData('WiFi', 'EnableSSH', isOn($request['wifi-ssh-enable']) ? 1 : 0);
		}
	}
	// Save network settings

	// Parse IP
	$ip = '';		//IP
	if ((isset($request['wipoct1']) && $request['wipoct1'] != '') &&
		(isset($request['wipoct2']) && $request['wipoct2'] != '') &&
		(isset($request['wipoct3']) && $request['wipoct3'] != '') &&
		(isset($request['wipoct4']) && $request['wipoct4'] != ''))
	{
		$ip = $request['wipoct1'].'.'.$request['wipoct2'].'.'.$request['wipoct3'].'.'.$request['wipoct4'];
	}
	// Get current IP
	$wifi_reset['IP'] = ['system', 'ra0addr', $dbconfig->getDbconfigData('system', 'ra0addr')];

	if ($wifi_reset['IP'][2] != $ip && isValidIP($ip)) {
		// Save IP
		$wifi_net_result = $wifi_result['wipoct1'] = (isValidIP($ip) ? $dbconfig->setDbconfigData('system', 'ra0addr', $ip) : false);
		debug('(wifi_processor.php|submitWifi()) WiFi network settings|Ip Address saved');	//DEBUG
	}
	// Parse Subnet Mask
	$mask = '';	//Subnet Mask
	if ((isset($request['wsoct1']) && $request['wsoct1'] != '') &&
		(isset($request['wsoct2']) && $request['wsoct2'] != '') &&
		(isset($request['wsoct3']) && $request['wsoct3'] != '') &&
		(isset($request['wsoct4']) && $request['wsoct4'] != ''))
	{
		$mask = $request['wsoct1'].'.'.$request['wsoct2'].'.'.$request['wsoct3'].'.'.$request['wsoct4'];
	}
	// Get current Subnet Mask
	$wifi_reset['Mask'] = ['system', 'ra0mask', $dbconfig->getDbconfigData('system', 'ra0mask')];

	if ($wifi_reset['Mask'][2] != $mask && isValidIP($mask)) {
		// Save Subnet Mask
		$wifi_net_result = $wifi_result['wsoct1'] = (isValidIP($mask) ? $dbconfig->setDbconfigData('system', 'ra0mask', $mask) : false);
		debug('(wifi_processor.php|submitWifi()) WiFi network settings|Subnet Mask saved');	//DEBUG
	}

	// Proceed to save dhcp settings
	if (isset($request['dhcpserver']))
	{
		$wifi_result['dhcpserver'] = $wifi_result['sdhcpoct1'] = $wifi_result['edhcpoct1'] = true;

		// Get current DHCP settings
		$wifi_reset['DHCP'] = ['system', 'ra0dhcp', $dbconfig->getDbconfigData('system', 'ra0dhcp')];

		// Set DHCP State On | Off
		if (isOn($request['dhcpserver']))			//DHCP Server Enabled
		{
			// $wifi_result['dhcpserver'] = $dbconfig->unsetDbconfigData('system', 'ra0dhcp');
			$wifi_result['dhcpserver']  = $dbconfig->setDbconfigData('system', 'ra0dhcp', 'on');

			// SET DHCP START IP RANGE
			$dhcp_sip = '';
			if ((isset($request['sdhcpoct1']) && $request['sdhcpoct1'] != '') &&
				(isset($request['sdhcpoct2']) && $request['sdhcpoct2'] != '') &&
				(isset($request['sdhcpoct3']) && $request['sdhcpoct3'] != '') &&
				(isset($request['sdhcpoct4']) && $request['sdhcpoct4'] != ''))
			{
				$dhcp_sip = $request['sdhcpoct1'].'.'.$request['sdhcpoct2'].'.'.$request['sdhcpoct3'].'.'.$request['sdhcpoct4'];
			}

			// Get current DHCP Start IP
			$wifi_reset['DHCPstartIP'] = ['system', 'ra0startip', $dbconfig->getDbconfigData('system', 'ra0startip')];

			if ($wifi_reset['DHCPstartIP'][2] != $dhcp_sip && isValidIP($dhcp_sip)) {
				$wifi_net_result = $wifi_result['sdhcpoct1'] = (isValidIP($dhcp_sip) ? $dbconfig->setDbconfigData('system', 'ra0startip', $dhcp_sip) : false);
			}

			// SET DHCP END IP RANGE
			$dhcp_eip = '';
			if ((isset($request['edhcpoct1']) && $request['edhcpoct1'] != '') &&
				(isset($request['edhcpoct2']) && $request['edhcpoct2'] != '') &&
				(isset($request['edhcpoct3']) && $request['edhcpoct3'] != '') &&
				(isset($request['edhcpoct4']) && $request['edhcpoct4'] != ''))
			{
				$dhcp_eip = $request['edhcpoct1'].'.'.$request['edhcpoct2'].'.'.$request['edhcpoct3'].'.'.$request['edhcpoct4'];
			}
			// Get current DHCP Start IP
			$wifi_reset['DHCPendIP'] = ['system', 'ra0endip', $dbconfig->getDbconfigData('system', 'ra0endip')];

			if ($wifi_reset['edhcpoct1'][2] != $dhcp_eip && isValidIP($dhcp_eip)) {
				$wifi_net_result = $wifi_result['edhcpoct1'] = (isValidIP($dhcp_eip) ? $dbconfig->setDbconfigData('system', 'ra0endip', $dhcp_eip) : false);
			}	
			
		}
		else if (isOff($request['dhcpserver']))		//DHCP Server Disabled
		{
			$wifi_result['dhcpserver']  = $dbconfig->setDbconfigData('system', 'ra0dhcp', 'off');
		}
		else
		{
			$wifi_result['dhcpserver'] = false;
		}

		if ($wifi_result['dhcpserver'] === true && $wifi_result['sdhcpoct1'] === true && $wifi_result['edhcpoct1'] === true)
		{
			// Unset the dhcpd.conf file in dbconfig
			if($dbconfig->unsetDbconfigData('system', 'dhcpd.conf'))
			{
				$wifi_net_result = true;
				debug('(wifi_processor.php|submitWifi()) DHCP settings saved');	//DEBUG
			}
			else
			{
				$wifi_net_result = false;
				debug('(wifi_processor.php|submitWifi()) Failed to unset dhcpd.conf in db-config');	//DEBUG
			}
		}
		else
		{
			$wifi_net_result = false;
		//$result['success'] = 'false';
		//$result['codes'][] = 221;
		debug('(wifi_processor.php|submitWifi()) Failed to save DHCP settings');	//DEBUG
		}
	}

	// Save the WiFi AP settings
	if (!empty($request['ssid']) && !empty($request['authtype']) && !empty($request['encryptype']))
	{
		
		// Get current Wifi SSID, Password, Authentication Mode and Encryption Type
		$wif_ap_info = $wifi_ctrl->getAPinfo();
		$pass = ($request['encryptype'] == "WEP" ? "0" . getSerialToSetWiFiPass() : 'ISC0' . getSerialToSetWiFiPass());
		$currentPass = ($wif_ap_info['encryp'] == "WEP" ? "0" . getSerialToSetWiFiPass() : 'ISC0' . getSerialToSetWiFiPass());
		$wifi_reset['AP'] = ['interface' => $interface['name'], 'essid'=> $wif_ap_info['ssid'], 'authmode'=>$wif_ap_info['auth'], 'encryptype'=>$wif_ap_info['encryp'], 'password'=>$currentPass, 'mac'=>''];

		// Update/Save Wifi ap settings
		$result_ap = $wifi_ctrl->APupdate(array('interface' => $interface['name'], 'essid'=> $request['ssid'], 'authmode'=>$request['authtype'], 'encryptype'=>$request['encryptype'], 'password'=>$pass, 'mac'=>$interface['mac']));

		if ($result_ap['success'] == 'true')		//wifi ap settings successfully saved
		{
			$wifi_ap_result = true;
			debug('(wifi_processor.php|submitWifi()) WiFi AP settings saved');	//DEBUG
			//$result['success'] = 'true';
			//$result['codes'][] = 210;
		}
		else
		{
			$wifi_ap_result = false;
			$result_ap_fail_codes[] = 211;
			$result_ap_fail_codes = array_merge($result_ap_fail_codes, $result_ap['codes']);

			debug('(wifi_processor.php|submitWifi()) Failed to save WiFi AP settings');	//DEBUG
		}
	}
	else
	{
		if (empty($request['ssid']))
			$wifi_result['ssid'] = false;

		if (empty($request['authtype']))
			$wifi_result['authtype'] = false;

		if (empty($request['encryptype']))
			$wifi_result['encryptype'] = false;

		$wifi_ap_result = false;
		$error_fields_highlighted = true;
	}

	if ($wifi_net_result === true && $wifi_ap_result === true)
	{
		$result['success'] = 'true';
		$result['codes'][] = 10;
		$result['codes'][] = 14;
	}
	else
	{
		$result['success'] = 'false';
		$result['codes'] = array_merge($result['codes'], $result_ap_fail_codes);

		if ($wifi_net_result === false || $error_fields_highlighted === true)
		{
			$result['codes'][] = 12;
		}
		else
		{
			$result['codes'][] = 11;
		}
	}

	// 1) find all the keys in the $wifi_result array that have a value of false (these are the API calls that failed)
	// 2) build a string with the keys. (The key names are the same as the html element (input/radio/select) names and will be used to highlight the fields with jquery)
	$failed_results = array_keys($wifi_result, false, true);
	$result['fields'] = implode(',', $failed_results);

	foreach($failed_results as $field)
	{
		if ($field == 'wipoct1')			// IP
		{
			$result['getParams'] .= '&ip='.$request['wipoct1'].'.'.$request['wipoct2'].'.'.$request['wipoct3'].'.'.$request['wipoct4'];
		}
		else if ($field == 'wsoct1')		// Subnet Mask
		{
			$result['getParams'] .= '&mask='.$request['wsoct1'].'.'.$request['wsoct2'].'.'.$request['wsoct3'].'.'.$request['wsoct4'];
		}
		else if ($field == 'sdhcpoct1')	// DHCP Start IP
		{
			$result['getParams'] .= '&dhcpsip='.$request['sdhcpoct1'].'.'.$request['sdhcpoct2'].'.'.$request['sdhcpoct3'].'.'.$request['sdhcpoct4'];
		}
		else if ($field == 'edhcpoct1')	// DHCP End IP
		{
			$result['getParams'] .= '&dhcpeip='.$request['edhcpoct1'].'.'.$request['edhcpoct2'].'.'.$request['edhcpoct3'].'.'.$request['edhcpoct4'];
		}
		else	// Other fields
		{
			$result['getParams'] .= '&'.$field.'='.$request[$field];
		}
	}

	debug('(wifi_processor.php|submitWifi()) $result: ', $result); 	//DEBUG

	// Reset Wifi Settings
	if ($result['success'] == 'false') {
		foreach($wifi_reset as $key=>$value) {
			if ($key == 'EnableSSH' || 'IP' || 'Mask' || 'DHCP' || 'DHCPstartIP' || 'DHCPendIP') {
				if ($key == "DHCP" && isOn($value)) {
					$dbconfig->setDbconfigData('system', 'ra0dhcp', 'on');
				} else {
					$dbconfig->setDbconfigData($value[0], $value[1], $value[2]);
				}
			} elseif ($key == 'AP') { 
				$wifi_ctrl->APupdate($value);
			}
		}
	}

	return $result;

} //END submitWifi

function getSerialToSetWiFiPass()
{
	$serialConf = '/mnt/nvram/rom/sn.txt';
	$serial = '1234';
	$readfile = file($serialConf,FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
	if($readfile)
		$serial = $readfile[0];
	return $serial;
}

?>
