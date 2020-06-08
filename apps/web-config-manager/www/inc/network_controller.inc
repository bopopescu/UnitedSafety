<?php

require_once $_SERVER['DOCUMENT_ROOT'].'/inc/db_sqlite3.inc';	//contains functions for db interaction
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';			//contains functions for socket interaction, error message display, and logging.
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/dbconfig_controller.inc';			//contains functions for getting, settings dbconfig parameters.

class networkcontroller
{
	private $dbconfig = '';

	function __construct()
	{
		$this->dbconfig = new dbconfigController();
	}

	/**
	 * Obtain network status of specified network interface, including ip address, subnet, mac address, broadcast, type.
	 * this function is a wrapper of ifconfig.
	 * @param interface name such as eth0, apcli0, ra0 etc.
	 * @return array that contains ip address, subnet, mac address and gateway with 'name', 'type', 'ip','broadcast' ,'netmask' and 'mac' keys .
	 * @return empty array if interface name is wroing.
	 * @access public
	 * @author Lee Wei (lee@absolutetrac.com)
	 */
	function getNetworkStatus($if)
	{
		//Legacy code for reading network interface via ifconfig
		$arlines = array();
		exec('ifconfig '.escapeshellarg($if).' '.'2>&1 ', $arlines);

		$str='';
		foreach($arlines as $line)
		{
			$str=$str.$line;
		}

		$regex=array();
		preg_match("/^([A-z]*\d)\s+Link\s+encap:([A-z]*)\s+HWaddr\s+([A-z0-9:]*).*".
			"inet addr:([0-9.]+).*Bcast:([0-9.]+).*Mask:([0-9.]+).*".
			"/ims", $str, $regex);

		$interfaces = array();
		if( !empty($regex) ){
			$interface = array();
			$interface['name'] = $regex[1];
			$interface['type'] = $regex[2];
			$interface['mac'] = $regex[3];
			$interface['ip'] = $regex[4];
			$interface['broadcast'] = $regex[5];
			$interface['netmask'] = $regex[6];
			$interfaces[] = $interface;
		}

		return $interfaces;
	} //END getNetworkStatus

	/**
	 * setNetwork
	 *
	 * Sets the network configuration (ip, subnet mask) for a given network interface
	 * 1) Saves network data to device via API
	 * 2) If successfull,
	 * 	  2a) updates network data to dbconfig
	 * 	  2b) Updates MAC address in dbconfig
	 * @param array $interface - contains the network configuration info parsed from the form
	 * @return array $result - contains success/fail flag + error codes indicating at which step the failure occured.
	 * @author Sean Toscano (sean@absolutetrac.com)
	 */
	function setNetwork($interface)
	{
		$result = array("success" => "false", "codes" => array());	//array for capturing success status, error codes
		/*
		//First, save IP and Mask to device via admin client API
		$sh_args = 'setip '.$interface['type'].' '.$interface['ip'].' '.$interface['mask'];
		$sh_out = '';											//socket output
		$sh_out = atsexec(escapeshellcmd($sh_args));			//socket call

		debug('(networkcontroller.inc|setNetwork()) admin client api command: setip '.$interface['type'].' '.$interface['ip'].' '.$interface['mask']); 	//DEBUG
		debug('(networkcontroller.inc|setNetwork()) admin client api command output: sh_out', $sh_out); 	//DEBUG
		*/
		//if(commandSuccessful($sh_out))		//API successfully set network interface config
		//{
			//debug('(networkcontroller.inc|setNetwork()): setip command completed.'); 	//DEBUG

			//Then, save Subnet Mask to db-config
			$result_ip_db = $this->setIpAddress($interface['name'], $interface['ip']);	//this is already being saved as part of the setip command
			$result_mask_db = $this->setSubnetMask($interface['name'], $interface['mask']);

			//Save MAC address to db-config
			$result_mac_db = $this->setMacAddress($interface['name'], $interface['mac']);

			if($result_mask_db === true && $result_mac_db === true)
			{
				debug('(networkcontroller.inc|setNetwork()): Saved subnet mask to db-config.'); 	//DEBUG
				debug('(networkcontroller.inc|setNetwork()): Saved MAC Address to db-config.'); 	//DEBUG

				$result['success'] = 'true';
			}
			else
			{
				if($result_mask_db === false)
				{
					debug('(networkcontroller.inc|setNetwork()) Failed to save subnet mask to db-config.'); 	//DEBUG
					$result['codes'][] = 103;
				}
				if($result_mac_db === false)
				{
					debug('(networkcontroller.inc|setNetwork()) Failed to save MAC address to db-config.'); 	//DEBUG
					$result['codes'][] = 104;
				}
			}
		//}
		/*else
		{
			debug('(networkcontroller.inc|setNetwork()) setip command failed.'); 	//DEBUG
			$result['codes'][] = 102;
		}*/

		return $result;
	} //END setNetwork


	/**
	 * getDNS
	 *
	 * Obtain dns server ip by reading the /etc/resolv.conf file for the DNS server IP
	 * @return array - contains DNS server ip addresses.
	 * @return array - empty array if cannot read/find file or file is empty.
	 * @author Sean Toscano (sean@absolutetrac.com)
	 */
	function getDNS()
	{
		$sh_out = array();
		exec('cat /etc/resolv.conf 2>&1 ', $sh_out);

		debug('(networkcontroller.inc|getDNS()) admin client api output: sh_out', $sh_out); 	//DEBUG

		$sh_str = implode(',', $sh_out);
		preg_match_all("/(([0-9]+\.?)+)/", $sh_str, $sh_result); 	//regex to match for the ip addresses
		debug('(networkcontroller.inc|getDNS()) preg_match_all regex output: $sh_result', $sh_result); 	//DEBUG

		return $sh_result[0];
	} //END getDNS

	// do not use this function for eth0 interface currently as eth0 can't be wake up after turn it down, a kernel bug need be fixed?
	/*function restartNetwork($int, $ipaddress='')
	{
		exec('ifconfig ' . escapeshellarg($interface). ' down');
		exec('ifconfig ' . escapeshellarg($interface). escapeshellarg($ipaddress). ' up');
	}*/


	function getIpAddress($interface)
	{
		debug('network_controller.inc|getIpAddress: $interface = ', $interface);  	//DEBUG

		switch ($interface)
		{
			case ethernet:
				return $this->dbconfig->getDbconfigData('system', 'eth0addr');
				break;
			case wireless:
				return $this->dbconfig->getDbconfigData('system', 'ra0addr');
				break;
			default:
				return "000.000.000.000";
				break;
		}
	} //END getIpAddress


	private function setIpAddress($key, $value)
	{
		debug('network_controller.inc|setIpAddress: key ='.$key.' value='.$value);  	//DEBUG

		switch ($key)
		{
			case ethernet:
				return $this->dbconfig->setDbconfigData('system','eth0addr',$value);
			case wireless:
				return $this->dbconfig->setDbconfigData('system','ra0addr',$value);
			default:
				return false;
		}

	} //END setIpAddress

	function getSubnetMask($interface)
	{
		debug('network_controller.inc|getSubnetMask: $interface = ', $interface);  	//DEBUG

		switch ($interface)
		{
			case ethernet:
				return $this->dbconfig->getDbconfigData('system', 'eth0mask');
				break;
			case wireless:
				return $this->dbconfig->getDbconfigData('system', 'ra0mask');
				break;
			default:
				return "000.000.000.000";
				break;
		}
	} //END getSubnetMask


	private function setSubnetMask($key, $value)
	{
		debug('network_controller.inc|setSubnetMask: key ='.$key.' value='.$value);  	//DEBUG

		switch ($key)
		{
			case ethernet:
				return $this->dbconfig->setDbconfigData('system','eth0mask',$value);
			case wireless:
				return $this->dbconfig->setDbconfigData('system','ra0mask',$value);
			default:
				return false;
		}

	} //END setSubnetMask

	function getMacAddress($interface)
	{
		debug('(network_controller.inc|getMacAddress()): $interface', $interface);  	//DEBUG

		switch ($interface)
		{
			case ethernet:
				return $this->dbconfig->getDbconfigData('system', 'ethmacaddr');
				break;
			case wireless:
				return $this->dbconfig->getDbconfigData('system', 'wifimacaddr');
				break;
			default:
				return "00:00:00:00:00:00";
				break;
		}
	} //END getMacAddress


	function setMacAddress($key, $value)
	{
		debug('network_controller.inc|setMacAddress: key ='.$key.' value='.$value);  	//DEBUG

		switch ($key)
		{
			case ethernet:
				return $this->dbconfig->setDbconfigData('system','ethmacaddr',$value);
			case wireless:
				return $this->dbconfig->setDbconfigData('system','wifimacaddr',$value);
			default:
				return false;
		}
	} //END setMacAddress

} //END networkcontroller
?>
