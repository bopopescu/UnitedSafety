<?php

require_once $_SERVER['DOCUMENT_ROOT'].'/inc/db_sqlite3.inc';	//contains functions for db interaction
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';			//contains functions for socket interaction, error message display, and logging.
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/dbconfig_controller.inc';			//contains functions for getting, settings dbconfig parameters.

class ParseClass
{
  //Create a 2-dimensional table for the dhcplease file
  public $dhcptable = array(array());
  //Number of entries to display after filtering
  public $filtered_number_display = 0;
  public function parser($openfile)
  {
	$line_number = 0;
	$row_array = array(array());
	while( !feof($openfile) )
	{
	  $read_line=fgets($openfile, 4096);
	  if (substr($read_line, 0, 1) != "#") //check for comment (skip)
	  {
		$tok = strtok($read_line, " ");
		if ($tok == "lease")
		{
		  $row_array[$line_number] = $this->initialize_array();
		  $row_array[$line_number][0] = strtok(" ")."\n";
		}
		else if ($tok == "starts")
		{
		  $day = $this->intToDay(strtok(" "));
		  $row_array[$line_number][1] = strtok(" ") . " ";
		  $time = strtok(" ");
		  $time = str_replace(";", "", $time);
		  $row_array[$line_number][1] = $row_array[$line_number][1].$time;
		  $row_array[$line_number][1] = $row_array[$line_number][1]."(".$day.")";
		}
		else if ($tok == "ends")
		{
		  $day = $this->intToDay(strtok(" "));
		  $row_array[$line_number][2] = strtok(" ") . " ";
		  $time = strtok(" ");
		  $time = str_replace(";", "", $time);
		  $row_array[$line_number][2] = $row_array[$line_number][2].$time;
		  $row_array[$line_number][2] = $row_array[$line_number][2]."(".$day.")";
		}
		else if ($tok == "tstp")
		{
		  $day = $this->intToDay(strtok(" "));
		  $row_array[$line_number][3] = strtok(" ") . " ";
		  $time = strtok(" ");
		  $time = str_replace(";", "", $time);
		  $row_array[$line_number][3] = $row_array[$line_number][3].$time;
		  $row_array[$line_number][3] = $row_array[$line_number][3]."(".$day.")";
		}
		else if ($tok == "hardware")
		{
		  $row_array[$line_number][4] = strtok(" ") . " - ";
		  $MAC = strtok(" ");
		  $MAC = strtoupper(str_replace(";", "", $MAC));
		  $MAC = strtoupper(str_replace("ethernet - ", "", $MAC));
		  $row_array[$line_number][4] = $MAC;
		}
		else if ($tok == "uid")
		{
		  $uid = strtok(" ");
		  $replace = array(".", "\"", ";");
		  $uid = str_replace($replace, "", $uid);
		  $row_array[$line_number][5] = $uid;
		}
		else if ($tok == "client-hostname")
		{
		  $hostname = strtok(" ");
		  $replace = array("\"", ";");
		  $hostname = str_replace($replace, "", $hostname);
		  $row_array[$line_number][6] = $hostname;
		}
		else if ($tok == "}\n")
		{
		  $row_array[$line_number][6] = $row_array[$line_number][6];
		  $line_number++;
		}
	  }
	}
	$this->dhcptable = $row_array;
  }

  public function filter()
  {
	$t = $this->dhcptable;
	$a = array();
	for( $i = 0 ; $i < count($t); $i++ )
	{
	  for( $j = 0 ; $j < count($t); $j++ )
	  {
		if( $i == $j )
		  continue;

		if( $this->compare_hostname($t[$i], $t[$j]) == 0 )
		{
		  if( $this->compare_end_time($t[$i], $t[$j]) < 0 )
		  {
			array_push($a,$i);
		  }
		}
	  }
	}
	$b = array_unique($a);
	foreach( $b as $i )
	{
	  unset($t[$i]);
	}
	$this->dhcptable=$t;
  }

  private function intToDay($integer)
  {
	if ($integer == 0)
	  return "Sunday";
	else if ($integer == 1)
	  return "Monday";
	else if ($integer == 2)
	  return "Tuesday";
	else if ($integer == 3)
	  return "Wednesday";
	else if ($integer == 4)
	  return "Thursday";
	else if ($integer == 5)
	  return "Friday";
	else
	  return "Saturday";
  }
  private function initialize_array()
  {
	$row_array = array();
	for ($i = 0; $i < 7; $i++) {
	  $row_array[$i] = "-";
	}
	return $row_array;
  }

  private function compare_ip($a, $b)
  {
	return strnatcmp($a[0], $b[0]);
  }
  private function compare_start_time($a, $b)
  {
	return strnatcmp($a[1], $b[1]);
  }
  private function compare_end_time($a, $b)
  {
	return strnatcmp($a[2], $b[2]);
  }
  private function compare_lease_expire($a, $b)
  {
	return strnatcmp($a[3], $b[3]);
  }
  private function compare_mac($a, $b)
  { 
	return strnatcmp($a[4], $b[4]);
  }
  private function compare_uid($a, $b)
  {
	return strnatcmp($a[5], $b[5]);
  }
  private function compare_hostname($a, $b)
  {
	return strnatcmp($a[6], $b[6]);
  }
  private function checkActiveLease($dhcp_line)
  {
	//Returns true or false depending on if the lease is currently active or not
	$leaseStart = strtotime(substr($dhcp_line[1], 0, strpos($dhcp_line[1], "(")));
	$leaseEnd = strtotime(substr($dhcp_line[2], 0, strpos($dhcp_line[2], "(")));
	if (time() >= $leaseStart && time() <= $leaseEnd) {
	  return true;
	} else {
	  return false;
	}
  }
}

class dhcpcontroller
{
	var $lease_file = '/var/state/dhcp/dhcpd.leases';	//location of dhcp lease file
	var $defaultconf = '/etc/dhcpd.conf';				//location of dhcp configuration file
	var $path = '/tmp/webconfig/';						//location for php script to store the file. admin client reads the file from this location  
	var $conffile = 'dhcpd.conf';						//name of dhcp config file
	var $dt = '';
	var $conn = '';
	private $dbconfig = '';
	
	/**
	 * Constructor
	 */
	function __construct()
	{
		$this->dt = new db_sqlite3();
		$this->conn = $this->dt->connect_sqlite($_SERVER['DOCUMENT_ROOT'].'/../www-app/config_mang.db');
		$this->dbconfig = new dbconfigController();
	}
	
	/**
	 * enableDhcp
	 * 
	 * Enable the DHCP server by calling the admin client function 'enabledhcpd'
	 * @return boolean - true if admin client command is successfully executed, false if failed to execute command
	 * @author Sean Toscano (sean@absolutetrac.com) 
	 */
	function enableDhcp()
	{
	
		$sh_args = 'enabledhcpd';	//admin client command for enabling dhcp server
		$sh_out = atsexec(escapeshellcmd($sh_args));	//socket call
		sleep(2); 	//wait for commmand to take effect on device //remmove when making this async
		//debug('Enable dhcp result', $sh_out); 	//DEBUG
		debug('(dhcp_controller.inc|enableDhcp()) admin client api command: \'enabledhcpd\' output $sh_out', $sh_out);	//DEBUG
			
		if(($sh_out == 'phpcmd: ok' || $sh_out == 'phpcmd:ok'))
		{
			debug('(dhcp_controller.inc|enableDhcp()): Successfully started DHCP server (enabledhcpd command completed).');	//DEBUG
			return true;
		}
		else
		{
			debug('(dhcp_controller.inc|enableDhcp()): Failed to start DHCP server (enabledhcpd command failed).');	//DEBUG
			return false;
		}
	} //END enableDhcp
	
	/**
	 * disableDhcp
	 * 
	 * Disable the DHCP server by calling the admin client function 'disabledhcpd'
	 * @return boolean - true if admin client command is successfully executed, false if failed to execute command
	 * @author Sean Toscano (sean@absolutetrac.com)
	 */
	function disableDhcp()
	{
		
		$sh_args = 'disabledhcpd';	//admin client command for disabling dhcp server
		$sh_out = atsexec(escapeshellcmd($sh_args));	//socket call
		sleep(2); //wait for commmand to take effect on device //remmove when making this async
		//debug('Disable DHCP Result', $sh_out); 	//DEBUG
		debug('(dhcp_controller.inc|enableDhcp()) admin client api command: \'disabledhcpd\' output $sh_out', $sh_out);	//DEBUG
			
		if((($sh_out == 'phpcmd: ok' || $sh_out == 'phpcmd:ok')) || 
				(($sh_out == 'phpcmd: fail' || $sh_out == 'phpcmd:fail')))
		{
			debug('(dhcp_controller.inc|disableDhcp()): Successfully stopped DHCP server (disabledhcpd command completed).');	//DEBUG
			
			return true;
		}
		else
		{
			debug('(dhcp_controller.inc|disableDhcp()): Failed to stop DHCP server (disabledhcpd command failed).');	//DEBUG
			return false;
		}	
	} //END disableDhcp
	
	/**
	 * getActiveDhcpLeases
	 * 
	 * Read the active DHCP leases from the dhcp.leases file and build an html table with the leases
	 * @return string - html table markup 
	 * @author Sean Toscano (sean@absolutetrac.com)
	 * @todo change this function to provide active leases for particular interface eth/ra (ethernet tab/ wifi tab)
	 */
	function getActiveDhcpLeases($eip)
	{
	  $html = "<div>No DHCP lease file found.</div>";		//initialize
	  debug('(dhcp_controller.inc|getActiveDhcpLeases()) Reading the dhcp lease file at',$this->lease_file);	//DEBUG

	  $fp = fopen($this->lease_file, "r");	//read file
	  if($fp)
	  {
		$parser = new ParseClass();
		$parser->parser($fp);
		$parser->filter();
		$leases = $parser->dhcptable;

		$html = '<table><thead>
		  <tr>
		  <th>Client Host Name</th>
		  <th>IP Address</th>
		  <th>MAC Address</th>
		  <th>Lease Ends</th>
		  </tr>
		  </thead><tbody>';
		if(count($leases) > 0)
		{
		  foreach($leases as $lease)
		  {
			debug('(dhcp_controller.inc|getActiveDhcpLeases()) Single DHCP lease stanza',$lease);	//DEBUG
			$html .= '<tr>';
			$html .= '<td>'.(!empty($lease[6]) ? str_replace('"','',$lease[6]) : '-').'</td>';		//host name					
			$html .= '<td>'.(!empty($lease[1]) ? $lease[0] : '-').'</td>';		//ip
			$html .= '<td>'.(!empty($lease[4]) ? $lease[4] : '-').'</td>';		//MAC
			$html .= '<td>'.(!empty($lease[3]) ? $lease[2] : '-').'</td>';		//lease end time
			$html .= '</tr>';
		  }
		}
		else	//if the file cannot be read or does not exist
		{
		  $html .= '<tr><td colspan="4">No current DHCP leases</td></tr>';
		}
		$html .= '</tbody></table>';
	  }
	  debug('(dhcp_controller.inc|getActiveDhcpLeases()) DHCP lease html markup',$html);	//DEBUG
	  return $html;

	} //END getActiveDhcpLeases

	/**
	 * getDhcpState
	 * Get the DHCP state for $interface from db-config
	 * @param string $interface - name/alias of network interface (eth0/ra0)
	 * @return string - on/off
	 */
	function getDhcpState($interface)
	{
		debug('dhcp_controller.inc|getDhcpState: $interface = ', $interface);  	//DEBUG
		$dhcpState = "on";
		$dhcpIntfState = '';
		
		switch ($interface)
		{
			case ethernet:
				$dhcpIntfState = $this->dbconfig->getDbconfigData('system', 'eth0dhcp');
				break;
			case wireless:
				$dhcpIntfState = $this->dbconfig->getDbconfigData('system', 'ra0dhcp');
				break;
		}
		
		if($dhcpIntfState == 1 || $dhcpIntfState == "1")	//if the dbconfig entry is set then dhcp is off for that interface otherwise dhcp is on
			$dhcpState = "off";
		
		return $dhcpState;
	
	} //END getDhcpState
	

	/**
	 * setDhcpState
	 * Set the DHCP state for $interface from db-config
	 * @param string $interface - name/alias of network interface (eth0/ra0)
	 * @return array DHCP information Start IP
	 */
	function setDhcpState($interface, $value)
	{
		debug('dhcp_controller.inc|setDhcpState: $interface = ', $interface);  	//DEBUG
	
		if($value == "on")
		{
			switch ($interface)
			{
				case ethernet:
					return $this->dbconfig->unsetDbconfigData('system', 'eth0dhcp');
				case wireless:
					return $this->dbconfig->unsetDbconfigData('system', 'ra0dhcp');
			}
		}
		else if($value == "off")
		{
			switch ($interface)
			{
				case ethernet:
					return $this->dbconfig->setDbconfigData('system', 'eth0dhcp', 1);
				case wireless:
					return $this->dbconfig->setDbconfigData('system', 'ra0dhcp', 1);
			}
		}
	
	} //END setDhcpState
	
	/**
	 * getDhcpRange
	 * Get the DHCP start and end ip range for $interface from db-config  
	 * @param string $interface - name/alias of network interface (eth0/ra0) 
	 * @return array DHCP information Start IP, End IP
	 */
	function getDhcpRange($interface)
	{
		debug('dhcp_controller.inc|getDhcp: $interface = ', $interface);  	//DEBUG
		
		$startip = $endip = $dhcp = null;
		
		switch ($interface)
		{
			case ethernet:
				$startip = $this->dbconfig->getDbconfigData('system', 'eth0startip');
				$endip = $this->dbconfig->getDbconfigData('system', 'eth0endip');
				break;
			case wireless:
				$startip = $this->dbconfig->getDbconfigData('system', 'ra0startip');
				$endip = $this->dbconfig->getDbconfigData('system', 'ra0endip');
				break;
			default:
				break;
		}
		
		if($startip && $endip){  $dhcp = array($startip, $endip); }
				
		return $dhcp;
		
	} //END getDhcpRange
	
	/**
	 * setDhcpRange
	 *
	 * Send the dhcp IP address range to admin client for the network interface passed in $interface
	 *
	 * @param array $interface contains network interface info (name, start ip, end ip, subnet, subnet mask)
	 * @return array $result - contains success/fail flag + error codes indicating at which step the failure occured.
	 * @author Sean Toscano (sean@absolutetrac.com)
	 */
	
	function setDhcpRange($interfaceName, $interfaceIp, $DhcpStartIp, $DhcpEndIp)
	{
		//Save DHCP range via admin client API
		if($interfaceName == ethernet)
		{
			$sh_args = 'setethdhcp ip='.$interfaceIp.' startip='.$DhcpStartIp.' endip='.$DhcpEndIp;
		}
		else if($interfaceName == wireless)
		{
			$sh_args = 'setwifidhcp ip='.$interfaceIp.' startip='.$DhcpStartIp.' endip='.$DhcpEndIp;
		}
	
		$sh_out = atsexec(escapeshellcmd($sh_args));			//socket call
	
		debug('(dhcp_controller.inc|setDhcp()) admin client api command: '.$sh_args); 	//DEBUG
		debug('(dhcp_controller.inc|setDhcp()) admin client api command output: sh_out', $sh_out); 	//DEBUG
	
		if($sh_out == 'phpcmd: ok')		//API successfully set network interface config
		{
			debug('(dhcp_controller.inc|setDhcp()): set dhcp command completed.'); 	//DEBUG
			return true;
		}
		else
		{
			debug('(dhcp_controller.inc|setDhcp()) set dhcp command failed.'); 	//DEBUG
			return false;
		}
		
	} //END setDhcpRange

	
	function setDhcp($dhcp)
	{
		$result = array("success" => "false", "codes" => array());	//array for capturing success status, error codes
	
		//First, update db-config with the dhcp state
		if(!$this->setDhcpState($dhcp['name'], $dhcp['dhcp_status']))
		{
			debug('(dhcp_controller.inc|setDhcp()): Failed to save DHCP state to db-config.');	//DEBUG
			//$result['codes'][] = 121;	//need to create an error code for this failure
			return $result;
		}
		debug('(dhcp_controller.inc|setDhcp()): Saved DHCP state to db-config.');	//DEBUG
	
		//Then, unset the dhcp.conf file in db-config
		if(!$this->dbconfig->unsetDbconfigData('system', 'dhcpd.conf'))
		{
			debug('(dhcp_controller.inc|setDhcp()): Failed to unset dhcpd.conf in db-config.');	//DEBUG
			//$result['codes'][] = 121;	//need to create an error code for this failure
			return $result;
		}
		$result['success'] = "true";
		debug('(dhcp_controller.inc|setDhcp()): Unset dhcpd.conf in db-config.');	//DEBUG
	
		//Finally, run the admin client command to update the DHCP range
		if(!empty($dhcp['dhcp_sip']) && !empty($dhcp['dhcp_eip']))
		{
			if(!$this->setDhcpRange($dhcp["name"], $dhcp['dhcp_gateway'], $dhcp['dhcp_sip'], $dhcp['dhcp_eip']))
			{
				debug('(dhcp_controller.inc|setDhcp()): Failed to save DHCP range via admin client API.');	//DEBUG
				$result['success'] = "false";
				//$result['codes'][] = 121;	//need to create an error code for this failure
				return $result;
			}
			debug('(dhcp_controller.inc|setDhcp()): Saved DHCP range via admin client API.');	//DEBUG
				
			$result['success'] = "true";
		}
	
		return $result;
	}
	
	/**
	 * getDhcp	-- DEPRECATED: Dec 2013, pulling this info from db-config now
	 * Get the DHCP configuration information for the $ip address
	 * @param string $interface ip address of the network subnet (eg: 192.168.1.1) 
	 * @return array DHCP information: Enabled/Disabled, Subnet, Start IP, End IP
	 */
	/*function getDhcp($ip)
	{
		$subnet = $dhcp_conf = '';
		$dhcp = array("status" => "Disabled");
		
		if(!empty($ip))
		{
			//build the network IP by taking the first three octets of the interface IP and adding the fourth octet as 0
			for($i=0; $i<3; $i++)
			{
				$subnet .= $ip[$i].".";		  
			}
			$subnet .= "0";
		
			debug('(dhcp_controller.inc|getDhcp()) dhcp subnet',$subnet);	//DEBUG
		
		}
		
		//read the dhcpd.conf from db-config
		$dhcp_conf = $this->getDhcpConfFromDbconfig();		
		
		if(!empty($subnet) && !empty($dhcp_conf))	//check that something was actually received
		{
			$filestring = implode(" ",$dhcp_conf);
			debug('(dhcp_controller.inc|getDhcp()) dhcp array to string: ',$filestring);	//DEBUG
			
			$filedata = array();
			$reg_str = '/(subnet\s*('.$subnet.')\s*netmask\s*([0-9\.]+)\s*{\s*range\s*dynamic-bootp\s*([0-9\.]+)\s*([0-9\.]+)\s*.*option\s*routers\s*\s*([0-9\.]+)\s*;\s*})/ims';	//regex
			preg_match_all($reg_str, $filestring, $filedata, PREG_SET_ORDER);	//search the dhcp conf file for dhcp info for the $subnet IP address
			debug('(dhcp_controller.inc|getDhcp()) dhcp preg_match_all result',$filedata);	//DEBUG
			
			if(count($filedata) > 0)	//if there is any matching info
			{
				$data = $filedata[0];
				$dhcp['network'] = (!empty($data[2]) ? $data[2] : '');	//subnet 
				$dhcp['sip'] = (!empty($data[4]) ? explode('.',$data[4]) : '');	//start ip range
				$dhcp['eip'] = (!empty($data[5]) ? explode('.',$data[5]) : '');	//end ip range
				$dhcp['status'] = 'Enabled';	//status
			}
			else 
			{
				$dhcp['status'] = 'Disabled';	//status
				
			}
			debug('(dhcp_controller.inc|getDhcp()) dhcp data',$dhcp);	//DEBUG
		}
		return $dhcp;
	
	} //END getDhcp
	*/

	
	
	/**
	 * setDhcp -- DEPRECATED: dhcp conf generation is now handled by the admin client, Dec 2013.
	 * 
	 * Set the dhcp IP address range for a particular network interface subnet.
	 * This function:
	 * 	1) Updates LCM db with DHCP settings 
	 * 	2) Invokes the DHCP conf file generator which generates a conf file based on the values in the LCM db, saves it to the file system, and updates dbconfig
	 *  3) Cycles DHCP server  
	 * @param array $interface contains network interface info (name, start ip, end ip, subnet, subnet mask)
	 * @return array $result - contains success/fail flag + error codes indicating at which step the failure occured.
	 * @author Sean Toscano (sean@absolutetrac.com)
	 */
	/*
	function setDhcp($interface)
	{
		$result = array("success" => "false", "codes" => array());	//array for capturing success status, error codes
		$dhcp = array();

		// 1) Updates LCM db with DHCP settings from page 
		$sql=  "insert or replace into DhcpfixedIpPool('LogicalIfName', 'StartIpAddr', 'EndIpAddr', 'Network', 'Subnetmask', 'Gateway', 'Status') values(:interface, :start_ip, :end_ip, :network, :mask, :gateway, :status)";
		$data = array(':interface' => $interface['name'] ,':start_ip' => $interface['dhcp_sip'], ':end_ip' => $interface['dhcp_eip'], ':network' => $interface['dhcp_network'], ':mask' => $interface['dhcp_mask'], ':gateway' => $interface['dhcp_gate'], ':status' => $interface['dhcp_status']);		
		$result_db = $this->dt->exec_sqlite($this->conn, $sql, $data);		//execute query
		
		if($result_db > 0)		//if commit successfull
		{
			debug('(dhcp_controller.inc|setDhcp()): Successfully saved DHCP settings to LCM db');	//DEBUG

			// 2) Generate dhcp conf file
			$result_conf = $this->generateDHCPdConf();	//generate dhcp conf file
			
			if($result_conf["success"] === "true")					//if conf file successfully generated
			{
				debug('(dhcp_controller.inc|setDhcp()): Successfully saved DHCP settings to conf file');	//DEBUG
				//$result['codes'][] = 154;
				
				// 3) Cycles DHCP server
				$result_d = $this->disableDhcp();		//Stop dhcp server 
				$result_e = $this->enableDhcp();		//Start dhcp server
								
				if($result_e === true)	
				{
					$result['success'] = 'true';
					//$result['codes'][] = 155;
				}
				else
					$result['codes'][] = 150;
			}
			else	//Failed to write conf file
			{
				debug('(dhcp_controller.inc|setDhcp()): Failed to generate DHCP conf file');	//DEBUG
				$result['codes'] = array_merge($result['codes'], $result_conf['codes']);
				debug('(dhcp_controller.inc|setDhcp()): Error codes: ', $result['codes']);	//DEBUG
			}
		}
		else		//Failed to commit dhcp settings to LCM db
		{
			debug('(dhcp_controller.inc|setDhcp()): Failed to save DHCP settings to LCM db.');	//DEBUG
			$result['codes'][] = 151;
		}
		return $result;
	} //END setDhcp
	*/ 
	
	/**
	 * generateDHCPdConf
	 * 
	 * 1) Generates dhcpd conf file content
	 * 2) Saves conf to device via API
	 * @return array $result - contains success/fail flag + error codes indicating at which step the failure occured.
	 * @author Sean Toscano (sean@absolutetrac.com)
	 * @todo enable the call to the generateStaticHosts function once static ip functionality has been implemented. 
	 */
	function generateDHCPdConf() 
	{
		$result = array("success" => "false", "codes" => array());	//array for capturing success status, error codes
		
		// 1) Generates dhcpd conf file content
		$dumpTXT = '';								//initialize
		$dumpTXT .= $this->staticTXT();				//generate static header content for dhcp conf file  
		$dumpTXT .= $this->generatePools();			//generate content for dhcp pools
		//$dumpTXT .= $this->generateStaticHosts(); //uncomment when implementing static ip
       
		debug('(dhcp_controller.inc|generateDHCPdConf()): DHCP Conf contents: ', $dumpTXT);	//DEBUG
        $write_conf = file_put_contents($this->path.$this->conffile, $dumpTXT);	//write conf file to temp location
        
        if($write_conf === false)	//if failed to write file.
        {
        	debug('(dhcp_controller.inc|generateDHCPdConf()): Failed to write conf file to temp location on device');	//DEBUG
        	$result['codes'][] = 152;
        	return $result;
        }
        
        debug('(dhcp_controller.inc|generateDHCPdConf()) Path to dhcp conf file', $this->path.$this->conffile);	//DEBUG
        
        $file_md5sum = exec('md5sum '.escapeshellarg($this->path.$this->conffile).' | cut -d " " -f 1');	//generate md5 checksum. admin client uses this checksum to verify file integrity.
        
        if(empty($file_md5sum))		//if cannot generate md5 checksum 
	    {
        	debug('(dhcp_controller.inc|generateDHCPdConf()): Failed to generate md5sum for conf file.');	//DEBUG
        	$result['codes'][] = 152;
        	return $result;
        }
        	
     	// 2) Save conf to device via API   
        $sh_args = 'setdhcpconf '.$this->conffile.' '.$file_md5sum;		//admin client command to write dhcpd conf to file system
        debug('(dhcp_controller.inc|generateDHCPdConf()): admin client api command: setdhcpconf '.$this->conffile.' '.$file_md5sum);	//DEBUG
        
        $sh_out = atsexec(escapeshellcmd($sh_args));	//socket call
        debug('(dhcp_controller.inc|generateDHCPdConf()): admin client api output: sh_out', $sh_out);	//DEBUG
                
        if($sh_out == 'phpcmd: ok')		//Successfully saved conf to device
        {
        	debug('(dhcp_controller.inc|generateDHCPdConf()): Successfully saved conf to device (setdhcpconf command completed).'); 		//DEBUG
        	$result["success"] = "true";
        }
        else
        {
        	debug('(dhcp_controller.inc|generateDHCPdConf()) Failed to save dhcp conf file to device (setdhcpconf command failed).'); 	//DEBUG
        	$result['codes'][] = 152;
        }
        
        return $result;
	} //END generateDHCPdConf
	
	
	/**
	 * 
	 */
	function getDhcpConfFromDbconfig()
	{
		return $this->dbconfig->getDbconfigMultilineData('system', 'dhcpd.conf');
		
	} //END getDhcpConfFromDbconfig


	
	/**
	 * staticTXT
	 * 
	 * Generate header content for dhcpd conf file
	 * @return string header content for dhcpd conf file 
	 * @author Lee Wei (lee@absolutetrac.com)
	 */
	private function staticTXT() 
	{
		$txt .= "ddns-update-style interim;\n";
		$txt .= "ignore client-updates;\n";
	    $txt .= "#DHCPDARGS=usb0;\n";
		$txt .= "option SIP code 120 = string;\n";
		$txt .= "option domain-name-servers 8.8.8.8,8.8.4.4;\n";
		$txt .= "\n";	

		return $txt;
	} //END staticTXT

	/**
	 * generatePools
	 * 
	 * Generate content for static dhcp ranges conf file from settings stored in db
	 * @return string dhcp conf file content
	 * @author Lee Wei (lee@absolutetrac.com)
	 */
	private function generatePools() 
	{		
		$sql="select * from DhcpfixedIpPool where Status = 1";	//fetch dhcp settings for network interfaces that are enabled
		$result = $this->dt->query_sqlite($this->conn, $sql);	//execute query
		
		$dumpTXT = "";		//initialize
		foreach ($result as $dhcp) //generate dhcp stanza for each enabled row in db
		{
			$dumpTXT .= "# ".$dhcp['LogicalIfName']."\n";												//interface name
			$dumpTXT .= "subnet ".$dhcp['Network']." netmask ".$dhcp['Subnetmask']." {\n";				//subnet ip
			$dumpTXT .= "   range dynamic-bootp ".$dhcp['StartIpAddr']." ".$dhcp['EndIpAddr'].";\n"; 	//static ip range
			$dumpTXT .= "   max-lease-time 84600;\n";													//defaults		
			$dumpTXT .= "   default-lease-time 84600;\n";
			$dumpTXT .= "   option routers ".$dhcp['Gateway'].";\n";									//gateway
			$dumpTXT .= "}\n\n";
	     }
		return $dumpTXT;
	} //END generatePools

	/**
	 * generateStaticHosts
	 * 
	 * Generate content for static dhcp ip addresses conf file from settings stored in db
	 * @return string dhcp conf file content
	 * @author Lee Wei (lee@absolutetrac.com)
	 * @todo: this function is not being used as this feature is not implemented yet.
	 */
	private function generateStaticHosts() 
	{
		$sql="select * from DhcpfixedIpAddress";
		$result = $this->dt->query_sqlite($this->conn, $sql);	//execute query

		$dumpTXT = "";		//initialize
		foreach ($result as $tempArray)
		{
			$dumpTXT .= "host ".$tempArray['LogicalIfName']." {\n";
			$dumpTXT .= "   hardware ethernet ".$tempArray['MacAddr'].";\n";
			$dumpTXT .= "   fixed-address ".$tempArray['IpAddr'].";\n";
			$dumpTXT .= "}\n\n";
		}
		return $dumpTXT;
	} //END generateStaticHosts
} //END dhcpcontroller
?>
