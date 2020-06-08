<?php
require_once $_SERVER['DOCUMENT_ROOT'].'inc/session_validator.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'inc/dbconfig_controller.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/network_controller.inc'; //network (ethernet, wireless) controller
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/dhcp_controller.inc';	//dhcp controller
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';				//contains functions for socket interaction, error message display, and logging.
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/db_sqlite3.inc';		//contains functions for db interaction

//OBJECT INSTANTIATION
$nt_ctrl = new networkcontroller();
$dhcp_ctrl = new dhcpcontroller();
$dbconfig = new dbconfigController();


//Check form submission
if(!empty($_REQUEST) && !empty($_REQUEST['csrfToken']) && $_REQUEST['csrfToken'] == $_SESSION['csrfToken'] && isset($_SESSION['M2M_SESH_USERAL']) && $_SESSION['M2M_SESH_USERAL'] == 200)
{
	debug('=========_REQUEST=============', $_REQUEST);	//DEBUG
	//Ethernet form submission
	$result = submitEthernet($nt_ctrl, $dhcp_ctrl, $dbconfig, trimRequest($_REQUEST));
	header("location:http://".$_SERVER['HTTP_HOST']."/network/ethernet/index.php?success=".$result['success']."&module=".$result['module']."&codes=".implode(",",$result['codes'])."&fields=".$result['fields'].$result['getParams']);

}
else
{
	header("location:http://".$_SERVER['HTTP_HOST']."/network/ethernet/index.php");
}


/**
 * submitEthernet
 *
 * Submits the ethernet form content to the ethernet controller for processing.
 * If Ethernet settings are successfully saved, then proceeds to save DHCP settings.
 * @param object $nt_ctrl network controller object
 * @param object $dhcp_ctrl dhcp controller object
 * @param array - the _REQUEST variable that contains the form submission data
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function submitEthernet($nt_ctrl, $dhcp_ctrl, $dbconfig, $request)
{
	//debug('(ethernet_processor.php|submitEthernet()) $_REQUEST param',$request);	//DEBUG
	$result = array("success" => 'false', "module" => "ethernet", "codes" => array(), "fields" => null, "getParams" => null);	//array for capturing result status, status code, and field names.

	$interface = array();
	$interface['name'] = ethernet;
	$interface['type'] = 'eth';

	$ethernet_result = array();		//store the success/failure state for each setting

	// Parse IP
	$ip = '';		//IP
	if( (isset($request['eipoct1']) && $request['eipoct1'] != '') &&
			(isset($request['eipoct2']) && $request['eipoct2'] != '') &&
			(isset($request['eipoct3']) && $request['eipoct3'] != '') &&
			(isset($request['eipoct4']) && $request['eipoct4'] != ''))
	{
		$ip = $request['eipoct1'].'.'.$request['eipoct2'].'.'.$request['eipoct3'].'.'.$request['eipoct4'];
	}

	// Save IP
	$ethernet_result['eipoct1'] = (isValidIP($ip) ? $dbconfig->setDbconfigData('system', 'eth0addr', $ip) : false);


	// Parse Subnet Mask
	$mask = '';	//Subnet Mask
	if( (isset($request['esoct1']) && $request['esoct1'] != '') &&
			(isset($request['esoct2']) && $request['esoct2'] != '') &&
			(isset($request['esoct3']) && $request['esoct3'] != '') &&
			(isset($request['esoct4']) && $request['esoct4'] != ''))
	{
		$mask = $request['esoct1'].'.'.$request['esoct2'].'.'.$request['esoct3'].'.'.$request['esoct4'];
	}

	// Save Subnet Mask
	$ethernet_result['esoct1'] = (isValidIP($mask) ? $dbconfig->setDbconfigData('system', 'eth0mask', $mask) : false);


	// Parse MAC Address
	$mac = '';		//MAC
	session_start();



	if($ethernet_result['eipoct1'] === true && $ethernet_result['esoct1'] === true)		//ethernet settings successfully saved
	{
		debug('(ethernet_processor.php|submitEthernet()) Ethernet settings saved');	//DEBUG

		// Proceed to save dhcp settings
		if(!empty($request['dhcpserver']))
		{
			$ethernet_result['dhcpserver'] = $ethernet_result['sdhcpoct1'] = $ethernet_result['edhcpoct1'] = true;

			// Set DHCP State On | Off
			if(isOn($request['dhcpserver']))			//DHCP Server Enabled
			{
				$ethernet_result['dhcpserver']  = $dbconfig->setDbconfigData('system', 'eth0dhcp', 'On');
//				$ethernet_result['dhcpserver'] = $dbconfig->unsetDbconfigData('system', 'eth0dhcp');
			}
			else if(isOff($request['dhcpserver']))		//DHCP Server Disabled
			{
				$ethernet_result['dhcpserver']  = $dbconfig->setDbconfigData('system', 'eth0dhcp', 'Off');
			}
			else
			{
				$ethernet_result['dhcpserver']  = $dbconfig->setDbconfigData('system', 'eth0dhcp', 'auto');
//				$ethernet_result['dhcpserver'] = false;
			}


			if(isOn($request['dhcpserver']) /*&& $ethernet_result['dhcpserver'] === true*/)
			{
				// SET DHCP START IP RANGE
				$dhcp_sip = '';
				if( (isset($request['sdhcpoct1']) && $request['sdhcpoct1'] != '') &&
						(isset($request['sdhcpoct2']) && $request['sdhcpoct2'] != '') &&
						(isset($request['sdhcpoct3']) && $request['sdhcpoct3'] != '') &&
						(isset($request['sdhcpoct4']) && $request['sdhcpoct4'] != ''))
				{
					$dhcp_sip = $request['sdhcpoct1'].'.'.$request['sdhcpoct2'].'.'.$request['sdhcpoct3'].'.'.$request['sdhcpoct4'];
				}

				$ethernet_result['sdhcpoct1'] = (isValidIP($dhcp_sip) ? $dbconfig->setDbconfigData('system', 'eth0startip', $dhcp_sip) : false);

				// SET DHCP END IP RANGE
				$dhcp_eip = '';
				if( (isset($request['edhcpoct1']) && $request['edhcpoct1'] != '') &&
						(isset($request['edhcpoct2']) && $request['edhcpoct2'] != '') &&
						(isset($request['edhcpoct3']) && $request['edhcpoct3'] != '') &&
						(isset($request['edhcpoct4']) && $request['edhcpoct4'] != ''))
				{
					$dhcp_eip = $request['edhcpoct1'].'.'.$request['edhcpoct2'].'.'.$request['edhcpoct3'].'.'.$request['edhcpoct4'];
				}

				$ethernet_result['edhcpoct1'] = (isValidIP($dhcp_eip) ? $dbconfig->setDbconfigData('system', 'eth0endip', $dhcp_eip) : false);

			}

			if($ethernet_result['dhcpserver'] === true && $ethernet_result['sdhcpoct1'] === true && $ethernet_result['edhcpoct1'] === true)
			{
				// Unset the dhcpd.conf file in dbconfig
//				if($dbconfig->unsetDbconfigData('system', 'dhcpd.conf'))
				{
					$result['success'] = 'true';
					$result['codes'][] = 10;
					$result['codes'][] = 14;
					debug('(ethernet_processor.php|submitEthernet()) DHCP settings saved');	//DEBUG
				}
//				else
//				{
//					$result['success'] = 'false';
//					$result['codes'][] = 11;
//					debug('(ethernet_processor.php|submitEthernet()) Failed to unset dhcpd.conf in db-config');	//DEBUG
//				}
			}
//			else
//			{
//				$result['success'] = 'false';
//				$result['codes'][] = 12;
//				debug('(ethernet_processor.php|submitEthernet()) Failed to save DHCP settings');	//DEBUG
//			}
		}

		if(!empty($request['ctlRouteOverride']))
		{
			$ethernet_result['routeOverride']  = $dbconfig->setDbconfigData('system', 'RouteOverride', $request['ctlRouteOverride']);
			if ($request['ctlRouteOverride'] == 'secondary' && isOn($request['dhcpserver']))
			{
				$ethernet_result['routeIP']  = $dbconfig->setDbconfigData('system', 'RouteIP', $request['ctlRouteIP']);
				$dbconfig->setDbconfigData('system', 'eth0dhcp', 'client');
				$dbconfig->setDbconfigData('feature', 'packetizer-secondary', '1');
				$dbconfig->setDbconfigData('feature', 'secondary-monitor', '1');
				$dbconfig->setDbconfigData('feature', 'primary-monitor', '0');
			}
			else if ($request['ctlRouteOverride'] == 'primary' && isOn($request['dhcpserver']))
			{
				$dbconfig->setDbconfigData('system', 'eth0dhcp', 'client');
				$dbconfig->setDbconfigData('feature', 'packetizer-secondary', '0');
				$dbconfig->setDbconfigData('feature', 'secondary-monitor', '0');
				$dbconfig->setDbconfigData('feature', 'primary-monitor', '1');
			}
			else
			{
				$dbconfig->setDbconfigData('feature', 'packetizer-secondary', '0');
				$dbconfig->setDbconfigData('feature', 'secondary-monitor', '0');
				$dbconfig->setDbconfigData('feature', 'primary-monitor', '0');
			}
		}
		else // need to turn off primary and secondary monitors,etc
		{
		}
	}
	else
	{
		$result['success'] = 'false';
		$result['codes'][] = 12;
		//$result['codes'] = array_merge($result['codes'], $result_eth['codes']);
		debug('(ethernet_processor.php|submitEthernet()) Failed to save Ethernet settings');	//DEBUG
	}



	// 1) find all the keys in the $ethernet_result array that have a value of false (these are the API calls that failed)
	// 2) build a string with the keys. (The key names are the same as the html element (input/radio/select) names and will be used to highlight the fields with jquery)
	$failed_results = array_keys($ethernet_result, false, true);
	$result['fields'] = implode(',', $failed_results);

	foreach($failed_results as $field)
	{
		if($field == 'eipoct1')			// IP
		{
			$result['getParams'] .= '&ip='.$request['eipoct1'].'.'.$request['eipoct2'].'.'.$request['eipoct3'].'.'.$request['eipoct4'];
		}
		else if($field == 'esoct1')		// Subnet Mask
		{
			$result['getParams'] .= '&mask='.$request['esoct1'].'.'.$request['esoct2'].'.'.$request['esoct3'].'.'.$request['esoct4'];
		}
		else if($field == 'sdhcpoct1')	// DHCP Start IP
		{
			$result['getParams'] .= '&dhcpsip='.$request['sdhcpoct1'].'.'.$request['sdhcpoct2'].'.'.$request['sdhcpoct3'].'.'.$request['sdhcpoct4'];
		}
		else if($field == 'edhcpoct1')	// DHCP End IP
		{
			$result['getParams'] .= '&dhcpeip='.$request['edhcpoct1'].'.'.$request['edhcpoct2'].'.'.$request['edhcpoct3'].'.'.$request['edhcpoct4'];
		}
		else if($field == 'emac1')		// MAC
		{
			$result['getParams'] .= '&mac='.$request['emac1'].':'.$request['emac2'].':'.$request['emac3'].':'.$request['emac4'].':'.$request['emac5'].':'.$request['emac6'];
		}
		else	// Other fields
		{
			$result['getParams'] .= '&'.$field.'='.$request[$field];
		}

		$result['success'] = 'false';
		$result['codes'] = array(12);
	}

	debug('(ethernet_processor.php|submitEthernet()) $result: ', $result); 	//DEBUG
	return $result;

} //END submitEthernet



/** DEPRECATED - January 2014
 * submitDhcp
 * Submits the dhcp form content to the dhcp controller for processing
 *
 * @param object $dhcp_ctrl - dhcp controller object
 * @param array $request -  the _REQUEST variable that contains the form submission data
 * @param array $interface	- network interface data( Start/End IP, Mask, Gateway)
 * @return array $result - contains success/fail flag + error codes indicating at which step the failure occured.
 * @author Sean Toscano (sean@absolutetrac.com)
 */
/*function submitDhcp($dhcp_ctrl, $request, $interface)
{
	//debug('(ethernet_processor.php|submitDhcp()) $_REQUEST param',$request);	//DEBUG

	$dhcp['name'] = $interface['name']; //eth0/ra0

	if(strcasecmp(trim($request['dhcpserver']),'Enabled') == 0)			//DHCP Server Enabled
	{
		$dhcp['dhcp_status'] = "on";
	}
	else if(strcasecmp(trim($request['dhcpserver']),'Disabled') == 0)	//DHCP Server Disabled
	{
		$dhcp['dhcp_status'] = "off";
	}

	if($dhcp['dhcp_status'] == "on")
	{

		$dhcp['dhcp_sip'] = '';			//DHCP Range Start Ip
		if( (isset($request['sdhcpoct1']) && $request['sdhcpoct1'] != '') &&
				(isset($request['sdhcpoct2']) && $request['sdhcpoct2'] != '') &&
				(isset($request['sdhcpoct3']) && $request['sdhcpoct3'] != '') &&
				(isset($request['sdhcpoct4']) && $request['sdhcpoct4'] != ''))
		{
			$dhcp['dhcp_sip'] = $request['sdhcpoct1'].'.'.$request['sdhcpoct2'].'.'.$request['sdhcpoct3'].'.'.$request['sdhcpoct4'];
		}

		$dhcp['dhcp_eip'] = '';			//DHCP Range End Ip
		if( (isset($request['edhcpoct1']) && $request['edhcpoct1'] != '') &&
				(isset($request['edhcpoct2']) && $request['edhcpoct2'] != '') &&
				(isset($request['edhcpoct3']) && $request['edhcpoct3'] != '') &&
				(isset($request['edhcpoct4']) && $request['edhcpoct4'] != ''))
		{
			$dhcp['dhcp_eip'] = $request['edhcpoct1'].'.'.$request['edhcpoct2'].'.'.$request['edhcpoct3'].'.'.$request['edhcpoct4'];
		}

		$dhcp['dhcp_gateway'] = $interface['ip'];		//DHCP Gateway
	}
	/* #1164 - dhcp conf generation is now being handled by admin client
	$dhcp['dhcp_mask'] = $interface['mask'];	//DHCP Subnet Mask

	//formulate DHCP subnet from the IP
	$dhcp['dhcp_network'] = '';
	$network = explode('.', $interface['ip']);
	$network[3] = 0;	//the last octet defines the network and needs to be 0
	$dhcp['dhcp_network'] = implode('.', $network);
	*/

	/*debug('(ethernet_processor.php|submitDhcp()) dhcp array', $dhcp);	//DEBUG
	return $dhcp_ctrl->setDhcp($dhcp);*/

//} //END submitDhcp

?>
