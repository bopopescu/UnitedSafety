<?php
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/db_sqlite3.inc';	//contains functions for db interaction
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';			//contains functions for socket interaction, error message display, and logging.

class ipseccontroller
{
	//defining the wifi authentication protocols
	var $authalgs = array(
		1 => 'des',
		2 => '3des',
		3 => 'aes128',
		4 => 'aes192',
		5 => 'aes256',
		6 => 'blowfish',
		7 => 'cast128'
	);

	//defining the hashing algorithms
	var $hashalgs = array(
		1 => 'md5',
		2 => 'sha1',
		3 => 'sha256',
		4 => 'sha384',
		5 => 'sha512'
	);

	//defining the DH encryption modes
	var $dhgroups = array(
		1  => 'modp768',
		2  => 'modp1024',
		5  => 'modp1536',
		14 => 'modp2048',
		15 => 'modp3072',
		16 => 'modp4096',
		17 => 'modp6144',
		18 => 'modp8192'
	);

    var $confpath;
    var $secretpath;
    var $x509certspath='/etc/ipsec.d/certs/';		//path to the certs file
    var $x509privatepath='/etc/ipsec.d/private/';	//path to the private file
	var $localkeyname='';

	var $dt = '';
	var $conn = ''; 
	var $path = '/tmp/webconfig/';			//this is where the (php) script stores the conf file it generates. admin client reads from this location.
	var $conffile = 'ipsec.conf';			//name of ipsec conf file
	var $secretsfile = 'ipsec.secrets';		//name of ipsec secrets file
	 
	/**
	 * Constructor
	 * @param string $confpath - path to ipsec conf file
	 * @param string $secretpath - path to ipsec secrets file
	 */
    function __construct($confpath='/etc/ipsec.conf', $secretpath='/etc/ipsec.secrets')
	{
        $this->confpath = $confpath;
        $this->secretpath = $secretpath;
        $this->dt = new db_sqlite3();
        $this->conn = $this->dt->connect_sqlite($_SERVER['DOCUMENT_ROOT'].'/../www-app/config_mang.db');
	}

  /**
	 * ipsecDBSanityCheck
	 * Check ipsec DB sanity
	 * 
	 * @return boolean true if DB is, false if failed to execute command
	 * @author Lee Wei 
	 */
  function ipsecDBSanityCheck()
  {
    $sqlc="PRAGMA table_info(ipsec)";
		$result=$this->dt->query_sqlite($this->conn, $sqlc);			//execute query
    debug('(ipsec_controller.inc|ipsecDBSanity()) : '.sizeof($result));	//DEBUG
    if(50 != sizeof($result)) 
    {
			$sqld= "drop table if exists ipsec;";
      $result=$this->dt->query_sqlite($this->conn, $sqld);			//execute query
      debug('(ipsec_controller.inc|ipsecDBSanity()) : drop ipsec table');	//DEBUG
      debug('(ipsec_controller.inc|ipsecDBSanity()) : create ipsec table');	//DEBUG
      $this->createIpsecDB();

    }
  }

	/**
	 * ipsecConnect
	 * Connects an ipsec policy
	 * 
	 * @param string $policyname - name of ipsec policy to connect
	 * @return boolean true if admin client command is successfully executed, false if failed to execute command
	 * @author Sean Toscano
	 * @todo make this asynchronous
	 */
	function ipsecConnect($policyname)
	{
		$status = $this->getIpsecStatus($policyname);	//check the status of the this policy
		debug('(ipsec_controller.inc|ipsecConnect()) ipsec policy status', $status); 	//DEBUG
		
		if($status === false)	//if disconnected
		{
			//currently disconnected, proceed to connect
			$sh_args = 'ipsecconnect '.trim($policyname);		//admin client command for connecting ipsec policy
			$sh_out = atsexec(escapeshellcmd($sh_args));		//socket call	
			
			debug('(ipsec_controller.inc|ipsecConnect()) admin client api command: ',$sh_args); 					//DEBUG
			debug('(ipsec_controller.inc|ipsecConnect()) admin client api command output: $sh_out', $sh_out); 		//DEBUG
			
			
			if($sh_out == 'phpcmd: ok' || $sh_out == 'phpcmd:ok')
			{
				debug('(ipsec_controller.inc|ipsecConnect()) Successfully connected Ipsec policy: '.$policyname); 	//DEBUG
				//when supporting multiple policies
				/* 
				//update policy in db
				$sql=  "update or rollback ipsec set Status = 1 where VPNPolicyName = :policyname";
				$data = array(':policyname' => $policyname);
				$result_db = $this->dt->exec_sqlite($this->conn, $sql, $data);
				if(count($result_db) > 0)
					echo "Ipsec policy successfully enabled in db";	//change to display in app log
				else
					echo "Failed to enable Ipsec policy in db";	//change to display in app log
				*/
				return true;
			}
			else
			{
				debug('(ipsec_controller.inc|ipsecConnect()) Failed to connect Ipsec policy: '.$policyname); 		//DEBUG
				return false;
			}
		}
		elseif($status === true)
		{
			return true;	//already connected
		}
		else
		{
			return false; 	//failed to connect
		}
	} //END ipsecConnect

	/**
	 * ipsecDisconnect
	 * Disconnects the ipsec policy
	 * 
	 * @param string $policyname - name of ipsec policy to disconnect 
	 * @return boolean true if admin client command is successfully executed, false if failed to execute command
	 * @author Sean Toscano
	 * @todo make this asynchronous
	 */
	function ipsecDisconnect($policyname)
	{
		
		$status = $this->getIpsecStatus($policyname);		//check the status of the this policy
		debug('(ipsec_controller.inc|ipsecDisconnect()) ipsec policy status', $status); 	//DEBUG
		
		if($status === true)	//if connected
		{
			//currently connected, proceed to disconnect
			$sh_args = 'ipsecdisconnect '.trim($policyname);	//admin client command for disconnecting ipsec policy
			$sh_out = atsexec(escapeshellcmd($sh_args));	//socket call
			
			debug('(ipsec_controller.inc|ipsecDisconnect()) admin client api command: ',$sh_args); 						//DEBUG
			debug('(ipsec_controller.inc|ipsecDisconnect()) admin client api command output: $sh_out', $sh_out); 		//DEBUG
			
			if($sh_out == 'phpcmd: ok' || $sh_out == 'phpcmd:ok')
			{
				debug('(ipsec_controller.inc|ipsecDisconnect()) Successfully disconnected Ipsec policy: '.$policyname); //DEBUG
				//when supporting multiple policies
				/*
				//update policy in db
				$sql=  "update or rollback ipsec set Status = 0 where VPNPolicyName = :policyname";
				$data = array(':policyname' => $policyname);
				$result_db = $this->dt->exec_sqlite($this->conn, $sql, $data);
				if(count($result_db) > 0)
					echo "Ipsec policy successfully disabled in db";	//change to display in app log
				else 	
					echo "Failed to disable Ipsec policy in db";	//change to display in app log
				*/
				return true;
			}
			else
			{
				debug('(ipsec_controller.inc|ipsecDisconnect()) Failed to disconnect Ipsec policy: '.$policyname); 		//DEBUG
				return false;
			}
		}
		elseif($status === false)
		{
			return true;	//already disconnected
		}
		else
		{
			return false; //failed to disconnect
		}
	} //END ipsecDisconnect

	/**
	 * ipsecRestart
	 * 
	 * Restarts the ipsec service. Disconnecting and then connecting wasn't working, so we switched to restarting the ipsec service all together
	 * @author Sean Toscano
	 */
	function ipsecRestart()
	{
		$sh_args = 'ipsecrestart';	//admin client command for restarting ipsec policy
		$sh_out = atsexec(escapeshellcmd($sh_args));	//socket call

		debug('(ipsec_controller.inc|ipsecRestart()) admin client api command: ',$sh_args); 					//DEBUG
		debug('(ipsec_controller.inc|ipsecRestart()) admin client api command output: $sh_out', $sh_out); 		//DEBUG
		
			
		if($sh_out == 'phpcmd: ok' || $sh_out == 'phpcmd:ok')
		{
			debug('(ipsec_controller.inc|ipsecRestart()) Successfully restarted Ipsec service'); 				//DEBUG
			return true;
		}
		else
		{
			debug('(ipsec_controller.inc|ipsecRestart()) Failed to restart Ipsec service'); 					//DEBUG
			return false;
		}
	} //END ipsecRestart
	
	/**
	 * getActivePolicyName
	 * Get the name of the active ipsec policy 
	 * @return string|boolean return policy name or false if can't find policy
	 * @author Sean Toscano (sean@absolutetrac.com)
	 */
	function getActivePolicyName()
	{
		$sql="select VPNPolicyName from ipsec where OID IN (select max(OID) from ipsec) AND Status = 1";	//fetch the latest active policy from the db  	//needs to be changed to accomodate multiple ipsec policies
		$result = $this->dt->query_sqlite($this->conn, $sql);		//execute query
		$policyname =  $result[0]['VPNPolicyName'];									
		debug('(ipsec_controller.inc|getActivePolicyName()) db query result: $result', $result); 			//DEBUG
		
		if(count($policyname) > 0)		//if there is a policy in the db
		{
			debug('(ipsec_controller.inc|getActivePolicyName()) ipsec policy namet: $policyname', $policyname); 			//DEBUG
			return $policyname;
		}
		else							//cannot find policy in the db
		{
			return false;
		}
	} //END getActivePolicyName

	
	/**
	 * getIpsecStatus
	 * 
	 * Check the status of the ipsec policy
	 * @param string $policyname name of ipsec policy
	 * @return boolean|number returns true if the policy is currently connected, false if disconnected, -1 if undefined
	 * @author Sean Toscano (sean@absolutetrac.com)
	 */
	function getIpsecStatus($policyname)
	{
		$sh_args = 'ipsecstatus '.trim($policyname);	//admin client command for getting the status of the ipsec policy
		$sh_out = atsexec(escapeshellcmd($sh_args));	//socket call

		debug('(ipsec_controller.inc|getIpsecStatus()) admin client api command: ',$sh_args); 							//DEBUG
		debug('(ipsec_controller.inc|getIpsecStatus()) admin client api command output: $sh_out', $sh_out); 			//DEBUG
					
		if(count($sh_out) > 0)	//if command is successfull
		{
			$regex=array();
			preg_match("/^phpcmd:\s?(.*)\sstatus:\s?([dD]?i?s?[cC]onnected)"."/ims", $sh_out, $regex); //get policy name and connected/disconnected
			debug('(ipsec_controller.inc|getIpsecStatus()) preg_match result: $regex ',$regex); 						//DEBUG

			if(strcasecmp(trim($regex[1]),$policyname) == 0 && strcasecmp(trim($regex[2]),'connected') == 0)
			{
				debug('(ipsec_controller.inc|getIpsecStatus()) Ipsec policy: '.$policyname.', is currently connected');	//DEBUG
				return true;	//connected
			}
			else if(strcasecmp(trim($regex[1]),$policyname) == 0 && strcasecmp(trim($regex[2]),'disconnected') == 0)
			{
				debug('(ipsec_controller.inc|getIpsecStatus()) Ipsec policy: '.$policyname.', is currently disconnected');	//DEBUG
				return false;	//disconnected
			}
			else 
			{
				debug('(ipsec_controller.inc|getIpsecStatus()) Ipsec policy: '.$policyname.', is currently undefined');	//DEBUG
				return -1;	//undefined
			}
		}	
		else	//command has failed
		{
			return -1;
		}
	} // END getIpsecStatus
	

	/**
	 * getActiveIpsecPolicies
	 * 
	 * Get all the active ipsec policies
	 * @return string html table containing the policies
	 * @author Sean Toscano (sean@absolutetrac.com)
	 * @todo add functionality to accomodate multiple ipsec policies in future release
	 */
	function getActiveIpsecPolicies()
	{
		$html = '';
		$html .= '<table id="vpnpolicies">
					<thead>
						<th>Policy</th>
						<th>Status</th>
						<th>Local Group</th>
						<th>Remote Group</th>
						<th>&nbsp;</th>
					</thead>
					<tbody>';
		
		$sql="select * from ipsec where OID IN (select max(OID) from ipsec) AND Status = 1";	//get the policy data from the db 	//needs to be changed to accomodate multiple ipsec policies
		$policies = $this->dt->query_sqlite($this->conn, $sql);		//execute query
		debug('(ipsec_controller.inc|getActiveIpsecPolicies()) db query result: $policies', $policies); 		//DEBUG
		
		if(count($policies) > 0)		//if policies exist in db
		{
			//build html table with policy data
			foreach($policies as $policy)
			{
				$html .= '<tr>';
				
				//policy name cell
				$html .= '<td>'.(!empty($policy['VPNPolicyName']) ? $policy['VPNPolicyName'] : '').'</td>';			

				$getStatus = $this->getIpsecStatus(trim($policy['VPNPolicyName']));		//check policy status on device
				//policy status cell
				$html .= '<td class="status">'.($getStatus === true ? 'Connected' : ($getStatus === false ? 'Disconnected' : ($getStatus === -1 ? 'Unknown' : ''))).'</td>';		
				
				//policy local group cell
				$html .= '<td>'.
							(!empty($policy['LocalStartAddress']) ? $policy['LocalStartAddress'].'<br />' : '').
							(!empty($policy['LocalEndAddress']) ? $policy['LocalEndAddress'].'<br />' : '').
							(!empty($policy['LocalSubnetMask']) ? $policy['LocalSubnetMask'] : '').
						'</td>';
				
				//policy remote group cell
				$html .= '<td>'.
							(!empty($policy['RemoteStartAddress']) ? $policy['RemoteStartAddress'].'<br />' : '').
							(!empty($policy['RemoteEndAddress']) ? $policy['RemoteEndAddress'].'<br />' : '').
							(!empty($policy['RemoteSubnetMask']) ? $policy['RemoteSubnetMask'] : '').
						'</td>';
				
				//edit link
				$html .= '<td><ul class="vpncontrols">';
				$html .= '<li class="noborder edit" style="margin-left:-5px;!important"><a href="#tabs2-5#vpn-ipsec" class="editIpsec" id="ipsec">Edit</a></li>';
				$html .= '<li class="connect inactivelink" '.(($getStatus === false ||  $getStatus === -1) ? 'style="display:none;"' : '').'>Connect</li>';
				$html .= '<li class="connect activelink" '.($getStatus === true ? 'style="display:none;"' : '').'><a href="'.$_SERVER['PHP_SELF'].'" id="connect_'.$policy['VPNPolicyName'].'" class="vpnConnect">Connect</a></li>';
				$html .= '<li class="disconnect inactivelink" '.(($getStatus === true || $getStatus === -1) ? 'style="display:none;"' : '').'>Disconnect</li>';
				$html .= '<li class="disconnect activelink" '.($getStatus === false ? 'style="display:none;"' : '').'><a href="'.$_SERVER['PHP_SELF'].'" id="disconnect_'.$policy['VPNPolicyName'].'" class="vpnDisconnect">Disconnect</a></li>';
				$html .= '<li class="noborder realtime" style="display: none;width:16px;height:16px"><img style="margin-top: -3px;width:16px;height:16px;" src="/images/loading.gif" alt="Loading..."></li>';
				$html .= '</ul>';
				
				$html .= '</td></tr>';
			}
		}
		else
		{
			$html .= '<tr><td colspan="4">No VPN policies to display.</td></tr>';
		}
		$html .= '</tbody></table>';
		
		return $html;
	}// END getActiveIpsecPolicies

	/**
	 * ipsecSecrets
	 * Generates content for ipsec secrets file using the values from the latest updated/inserted row in the ipsec table
	 * @return string content of secrets conf file
	 * @author Lee Wei (lee@absolutetrac.com), Sean Toscano (sean@absolutetrac.com)
	 * @todo change to accomodate multiple policies
	 * @todo generate a private key without password. For now password is 123456 for test case.
	 */
	function ipsecSecrets()
	{
		$sql = 'select * from ipsec where OID IN (select max(OID) from ipsec)';	//this will need to be changed once we move to multiple ipsec profiles
		$current = $this->dt->query_sqlite($this->conn, $sql);		//execute query
		debug('(ipsec_controller.inc|ipsecSecrets()) db query result: $current', $current); 		//DEBUG
		
		$dumpTXT="";
		foreach($current as $configrow)
		{
			if($configrow["AuthType"] == 0)		//pre-shared key
				$dumpTXT.="%any ".$configrow["RemoteEndPoint"].": PSK \"".$configrow["Presharedkey"]."\"\n";
			else if($configrow["AuthType"] == 1)//RSA
				echo 'RSA siganature not support yet';
			else
			{
				//FIXME: need generate a private key without password.now password 123456 is for test case.
				$dumpTXT.=": RSA ".$this->localkeyname." \"123456\"\n";
			}
		}
		return $dumpTXT;
	} //END ipsecSecrets

	/**
	 * commonTXT
	 * Generates conmmon content for the top of the ipsec conf file
	 * @return string
	 * @author Lee Wei (lee@absolutetrac.com)
	 */
	private function commonTXT()
	{
		$dumpTXT='';
		$dumpTXT .="# /etc/ipsec.conf - Openswan IPSec configuration file\n";
		$dumpTXT .="version 2.0    # conforms to second version of ipsec.conf specification\n";
		$dumpTXT .="# basic configuration\n";
		$dumpTXT .="config setup\n";
		$dumpTXT .="    dumpdir=/var/run/pluto\n";
		$dumpTXT .="    nat_traversal=yes\n";
		$dumpTXT .="    virtual_private=%v4:10.0.0.0/8,%v4:192.168.0.0/16,%v4:172.16.0.0/12\n";
		$dumpTXT .="    oe=off\n";
		$dumpTXT .="    protostack=netkey\n\n";

		return $dumpTXT;
	} //END commonTXT

	/**
	 * connTXT
	 * Generates the content for the ipsec conf file using the values from the latest updated/inserted row in the ipsec table
	 * @return string content for ipsec conf
	 * @author Lee Wei (lee@absolutetrac.com), Sean Toscano (sean@absolutetrac.com)
	 */
	private function connTXT()
	{
		$sqld="select * from tableDefaults where tableName='ipsec'";	//fetch the default ipsec settings from the db 
		$default = $this->dt->query_sqlite($this->conn, $sqld);			//execute query
		debug('(ipsec_controller.inc|connTXT()) db query result: $default', $default); 		//DEBUG

		$sqlc = 'select * from ipsec where OID IN (select max(OID) from ipsec)';	//fetch the current policy info from db		//this will need to be changed once we support multiple ipsec profiles
		$current = $this->dt->query_sqlite($this->conn, $sqlc);			//execute query
		debug('(ipsec_controller.inc|connTXT()) db query result: $current', $current); 		//DEBUG
		
		$dumpTXT="";
		foreach($current as $configrow)
		{
			if($configrow['Status']==1)	//if policy is enabled, create conf file stanza with its settings
			{
				//FIXME, use default value instead of blank field.

				$dumpTXT.="conn ".$configrow["VPNPolicyName"]."\n";
				$dumpTXT.="    type=".(($configrow["TunnelOrTransport"])?"tunnel":"transport")."\n";
				$dumpTXT.="    authby=".((!$configrow["AuthType"])?"secret":"rsaig")."\n";
				$dumpTXT.=($configrow["PFS"] == 1 ? "    pfs=yes\n" : "    pfs=no\n");

				//FIXME consider RemoteEndpointType and host/spoke mode.
				$dumpTXT.="    left=".$configrow["RemoteEndPoint"]."\n";
				
				require_once "cidr.inc";
				$cidr = new CIDR();

				$leftsubnet = array();
				$rightsubnet = array();

				//FIXME: not support multi subnet.
				switch($configrow["RemoteNetworkType"]){
				case 0: //any
					$leftsubnet=array("0.0.0.0/0\n");
					break;
				case 1://Ip
					$leftsubnet=array($configrow["RemoteStartAddress"]."/32\n"); 
					break;
				case 2: //range
					$lsrange = $cidr->rangeToCIDRList($configrow["RemoteStartAddress"],$configrow["RemoteEndAddress"]);
					$leftsubnet = ((is_array($lsrange) && count($lsrange) > 0) ? array_shift($lsrange) : "")."\n";
					break;
				case 3://subnet
					$leftsubnet = $cidr->alignedCIDR($configrow["RemoteStartAddress"] ,$configrow["RemoteSubnetMask"])."\n";
				}

				$dumpTXT.="    leftsubnet=".$leftsubnet;
				#$dumpTXT.="    leftnexthop=%defaultroute\n";

				//x509 certificate
				if(($configrow["AuthType"] == 2) && isset($configrow["X509Remotecert"]) && isset($configrow["X509Localcert"]) && isset($configrow["X509Localkey"]))
				{
					$remotecertname=$configrow['VPNPolicyName'].'_remote_pem';
					$localcertname=$configrow['VPNPolicyName'].'_local.pem';
					$localkeyname=$configrow['VPNPolicyName'].'_local.key';
					file_put_contents($this->x509certspath.$remotecertname, $configrow["X509Remotecert"]);
					file_put_contents($this->x509certspath.$localcertname, $configrow["X509Localcert"]);
					file_put_contents($this->x509privatepath.$localkeyname, $configrow["X509Localkey"]);
					$this->localkeyname=$localkeyname;
					$dumpTXT .= "    leftcert=".$remotecertname."\n";
					$dumpTXT .= "    rightcert=".$localcertname."\n";
					$dumpTXT .= "    leftrsasigkey=%cert\n";
					$dumpTXT .= "    rightrsasigkey=%cert\n";
				}

				$dumpTXT.="    right=%defaultroute\n";

				switch($configrow["LocalNetworkType"]){
				case 0: //any
					$rightsubnet="0.0.0.0/0\n";
					break;
				case 1://Ip
					$rightsubnet=$configrow["LocalStartAddress"]."/32\n"; 
					break;
				case 2: //range
					$rsrange = $cidr->rangeToCIDRList($configrow["LocalStartAddress"],$configrow["LocalEndAddress"]);
					$rightsubnet = ((is_array($rsrange) && count($rsrange) > 0) ? array_shift($rsrange) : "")."\n";
					break;
				case 3://subnet
					$rightsubnet = $cidr->alignedCIDR($configrow["LocalStartAddress"] ,$configrow["LocalSubnetMask"])."\n";
				}

				if($configrow['VPNMode'] == 'GateWay')
					$dumpTXT.="    rightsubnet=".$rightsubnet;

				$dumpTXT.="    rightnexthop=%defaultroute\n";
				$dumpTXT.="    rightupdown=\"ipsec _updown.my --route no\"\n";

				if(!empty($configrow["LocalIdentifier"]))
					$dumpTXT.="    rightid=".$configrow["LocalIdentifier"]."\n";

				$dumpTXT.="    ike=".$this->authalgs[$configrow["EncryptionAlgorithm"]]."-".$this->hashalgs[$configrow["AuthAlgorithm"]].";".$this->dhgroups[$configrow["DHGroup"]]."\n";

				$dumpTXT.="    keyexchange=ike\n";
				$dumpTXT.="    auth=esp\n";

				if($configrow["ExchangeMode"] == 1)
					$dumpTXT.="    aggrmode=yes\n";

				if($configrow["AutoInitiate"])
					$dumpTXT.="    auto=start\n";
				else
					$dumpTXT.="    auto=start\n";   ///change to add to support multiple policies

				$dumpTXT.="    ikelifetime=24h\n";//FIXME: need change by web.

				if($configrow["DPD"])
				{
					$dumpTXT.="    dpddelay=30\n";
					$dumpTXT.="    dpdtimeout=120\n";
					$dumpTXT.="    dpdaction=restart\n";
				}

				$dumpTXT.="    forceencaps=yes\n\n";
			} //END if
		} //END foreach
		
		debug('(ipsec_controller.inc|connTXT()) contents of ipsec conf file', $dumpTXT); 		//DEBUG
		return $dumpTXT;
	} //END connTXT

	/**
	 * removelines
	 * NOTE: This function is not implemented yet
	 *  
	 * @param unknown_type $file
	 * @param unknown_type $startline
	 * @param unknown_type $endline
	 * @return boolean
	 * @author Lee Wei (lee@absolutetrac.com)
	 */
	private function removelines($file, $startline, $endline)
	{
		if($startline > $endline)
		{
			error_log('Bad parameter '.__LINE__." ".__FUNCTION__."\n");
			return false;
		}

		$arr  = file($file);
		for($i=$startline; $i<$endline; $i++)
			unset($arr[$i]);
		$arr=array_values($arr);
		file_put_contents($file, implode($arr));
	}// END removelines

	/**
	 * ipsecUpdateCertificate
	 * NOTE: This function is not implemented yet
	 * 
	 * @param unknown_type $vpname
	 * @param unknown_type $remotecrt
	 * @param unknown_type $localcrt
	 * @param unknown_type $localkey
	 * @author Lee Wei (lee@absolutetrac.com)
	 */
	function ipsecUpdateCertificate($vpname, $remotecrt, $localcrt, $localkey)
	{
		require_once "db_sqlite3.php";
		$dt = new db_sqlite3();
		$conn = $dt->connect_sqlite('data.db');

		if(file_exists($remotecrt))
		{
			$lines=file($remotecrt);
			$str='';
			foreach($lines as $line)
			{
				$str.=$line;
			}

			$sql='update ipsec set X509Remotecert='.escapeshellarg($str).' where VPNPolicyName='.escapeshellarg($vpname);
			$dt->exec_sqlite($conn, $sql);
		}

		if(file_exists($localcrt))
		{
			$lines=file($localcrt);
			$str='';
			foreach($lines as $line)
			{
				$str.=$line;
			}

			$sql='update ipsec set X509Localcert='.escapeshellarg($str).' where VPNPolicyName='.escapeshellarg($vpname);
			$dt->exec_sqlite($conn, $sql);
		}

		if(file_exists($localkey))
		{
			$lines=file($localkey);
			$str='';
			foreach($lines as $line)
			{
				$str.=$line;
			}

			$sql='update ipsec set X509Localkey='.escapeshellarg($str).' where VPNPolicyName='.escapeshellarg($vpname);
			$dt->exec_sqlite($conn, $sql);
		}
	} //END ipsecUpdateCertificate

	/**
	 * generateconf
	 * Generate the ipsec conf and ipsec secrets files and writes them to the file system at $this->path
	 * @return boolean true if admin client command is successfully executed, false if failed to execute command
	 * Sean Toscano (sean@absolutetrac.com)
	 */
	function generateconf()
	{
		$dumpTXT = "";
		$dumpTXT.=$this->commonTXT();	//get content for common header info
		$dumpTXT.=$this->connTXT();		//get content for conf file
		$write_conf = file_put_contents($this->path.$this->conffile, $dumpTXT);	//Write conf file
		debug('(ipsec_controller.inc|generateconf()) writing ipsec conf file to:', $this->path.$this->conffile); 		//DEBUG
		debug('(ipsec_controller.inc|generateconf()) contents of ipsec conf file', $dumpTXT); 							//DEBUG
		
		
		if($write_conf === false)	//failed to write conf file
		{
			debug('(ipsec_controller.inc|generateconf()) failed to write ipsec conf file.'); 							//DEBUG
			return false;
		}
		
		$file_md5sum = exec('md5sum '.escapeshellarg($this->path.$this->conffile).' | cut -d " " -f 1');	//generate md5 checksum. admin client uses this checksum to verify file integrity.
		debug('(ipsec_controller.inc|generateconf()) generating md5sum of conf file: md5sum '.escapeshellarg($this->path.$this->conffile).' | cut -d " " -f 1'); 		//DEBUG
				
		if(empty($file_md5sum))		//failed to generate checksum
		{
			debug('(ipsec_controller.inc|generateconf()) failed to generate ipsec conf file md5sum'); 					//DEBUG
			return false;
		}
		
		debug('(ipsec_controller.inc|generateconf()) ipsec conf file md5sum:', $file_md5sum); 							//DEBUG
		
		$sh_args_dat = 'setipsecconf '.$this->conffile.' '.$file_md5sum;	//admin client command for writing ipsec conf file to device		
		$sh_out_dat = atsexec(escapeshellcmd($sh_args_dat));	//socket call
		debug('(ipsec_controller.inc|generateconf()) admin client api command: ',$sh_args_dat); 						//DEBUG
		debug('(ipsec_controller.inc|generateconf()) admin client api command output: $sh_out_dat', $sh_out_dat); 		//DEBUG
		
		
		$dumpTXTsec = $this->ipsecSecrets();												//Generate secrets file
		$write_secrets = file_put_contents($this->path.$this->secretsfile, $dumpTXTsec);	//Write secrets file
		debug('(ipsec_controller.inc|generateconf()) writing ipsec secrets file to:', $this->path.$this->secretsfile); 	//DEBUG
		debug('(ipsec_controller.inc|generateconf()) contents of ipsec secrets file', $dumpTXTsec); 					//DEBUG
		
		
		if($write_secrets === false)	//failed to write file to system
		{
			debug('(ipsec_controller.inc|generateconf()) failed to write ipsec secrets file.'); 						//DEBUG
			return false;
		}
		
		$sfile_md5sum = exec('md5sum '.escapeshellarg($this->path.$this->secretsfile).' | cut -d " " -f 1');	//generate md5 checksum. admin client uses this checksum to verify file integrity.
		debug('(ipsec_controller.inc|generateconf()) generating md5sum of secrets file: md5sum '.escapeshellarg($this->path.$this->secretsfile).' | cut -d " " -f 1'); 		//DEBUG
		debug('(ipsec_controller.inc|generateconf()) ipsec secrets file md5sum:', $sfile_md5sum); 						//DEBUG
		
		if(empty($sfile_md5sum))	//failed to generate checksum
		{
			debug('(ipsec_controller.inc|generateconf()) failed to generate ipsec conf file md5sum'); 					//DEBUG
			return false;
		}
		
		$sh_args_sec = 'setipsecsecrets '.$this->secretsfile.' '.$sfile_md5sum;		//admin client command for writing ipsec secrets file to device
		$sh_out_sec = atsexec(escapeshellcmd($sh_args_sec));
		
		debug('(ipsec_controller.inc|generateconf()) admin client api command: ',$sh_args_sec); 						//DEBUG
		debug('(ipsec_controller.inc|generateconf()) admin client api command output: $sh_out_sec', $sh_out_sec); 		//DEBUG
		
		
		if(($sh_out_dat == 'phpcmd: ok') && (!empty($sh_out_sec) && $sh_out_sec == 'phpcmd: ok'))
		{
			debug('(ipsec_controller.inc|generateconf()) successfully saved ipsec conf and secrets files'); 			//DEBUG
			return true;
		}
		else
		{
			debug('(ipsec_controller.inc|generateconf()) failed to save ipsec conf and secrets files'); 				//DEBUG
			return false;
		}
	} //END generateconf
	
	/**
	 * setIpSec
	 * Commits ipsec config values to the db
	 * - if no policies are active, it just saves the settings to the db and conf file
	 * - if a policy is active, it disconnects the policy, applies the changes and then reconnects the policy
	 *  
	 * @param array $ipsec ipsec form settings
	 * @author Sean Toscano (sean@absolutetrac.com)
	 * @todo for values that the user doesn't provide, grab them from the defaults table
	 */
	function setIpSec($ipsec)
	{
		$result = array("success" => 'false', "module" => "vpn", "codes" => array(), "fields" => null);	//array for capturing result status, status code, and field names.
		
		debug('(ipsec_controller.inc|setIpSec()) $ipsec',$ipsec); 	//DEBUG
		
		//disconnect the existing policy if setting a new policy name
		$old_policy = '';
		$sqlchk = 'select VPNPolicyName from ipsec where OID IN (select max(OID) from ipsec)';		//fetch the name of the current ipsec policy
		$result_chk = $this->dt->query_sqlite($this->conn, $sqlchk);								//execute query
		debug('(ipsec_controller.inc|setIpSec()) db query result: $result_chk', $result_chk); 		//DEBUG								
		
		if(count($result_chk) > 0)	//if a policy is found
		{
			$old_policy = $result_chk[0]['VPNPolicyName'];	//grab the name of the policy
		}
		
		if(!empty($old_policy) && strcasecmp($ipsec['policy'],$old_policy) != 0)	
		{
			$discon_op = $this->ipsecDisconnect($old_policy);	//disconnect the policy
		
			if($discon_op === true)	//disconnect successfull
			{
				debug('(ipsec_controller.inc|setIpSec()) Successfully disconnected current policy: '.$old_policy); 		//DEBUG
			}
			else	//disconnect failed
			{
				debug('(ipsec_controller.inc|setIpSec()) Failed to disconnected current policy: '.$old_policy); 		//DEBUG
			}	
		}
		
		//build sql prepared statement to insert new policy into db
		$sql=  "insert or replace into
				ipsec('VPNPolicyName', 'IpsecProtocol','TunnelOrTransport',
				      'RemoteEndPointType','RemoteEndPoint',
				      'LocalNetworkType','LocalStartAddress','LocalEndAddress', 'LocalSubnetMask',
				      'RemoteNetworkType', 'RemoteStartAddress', 'RemoteEndAddress', 'RemoteSubnetMask', 
				      'EnableKeepAlive', 'KeepAliveSourceAddress', 'RemotePingIPAddress', 'KeepAlivePeriod', 'KeepAliveFailureCount', 
				      'Status', 'AutoInitiate', 'Direction', 'ExchangeMode', 'IKEVersion',
				      'LocalIdentifierType', 'LocalIdentifier', 'RemoteIdentifierType', 'RemoteIdentifier', 
				      'EncryptionAlgorithm', 'P2EncryptionAlgorithm', 'KeyLength',
				      'AuthAlgorithm', 'P2AuthAlgorithm', 'AuthType', 'Presharedkey', 'DHGroup', 
				      'SALifeTime', 'P2SALifeTime', 'SALifeTimeType', 
				      'PFS', 'DPD', 'DPDDetectionPeriod', 'DPDFailureCount', 
				      'NatTraversal', 'NatKeepAliveTime', 
				      'X509Remotecert', 'X509Localcert', 'X509Localkey', 'VPNMode')
				values(:policy, :protocol, :mode,
				       :remote_ep_type, :remote_ep_val,
				       :local_type, :local_sip, :local_eip, :local_subnet,
				       :remote_type, :remote_sip, :remote_eip, :remote_subnet, 
				       :keep_alive, :ka_source, :ka_remote, :ka_period, :ka_fail_count, 
				       :policy_status, :auto_initiate, :direction, :exchange_mode, :ike_ver,
				       :local_id_type, :local_id_val, :remote_id_type, :remote_id_val, 
				       :encryp_p1, :encryp_p2, :key_length,
				       :auth_p1, :auth_p2, :auth_type, :pskey, :dhgroup, 
				       :salife_p1, :salife_p2, :salife_type, 
				       :pfs, :dpd, :dpd_period, :dpd_failcount, 
				       :nat, :nat_ka_time, 
				       :x509_remote_cert, :x509_local_cert, :x509_local_key, :vpnmode)";
	
		$data = array(':policy' => $ipsec['policy'], ':protocol' => $ipsec['protocol'], ':mode' => $ipsec['mode'], 
				      ':remote_ep_type' => $ipsec['remote_ep_type'], ':remote_ep_val' => $ipsec['remote_ep_val'], 
				      ':local_type' => $ipsec['local_type'], ':local_sip' => $ipsec['local_sip'], ':local_eip' => $ipsec['local_eip'], ':local_subnet' => $ipsec['local_subnet'],
				      ':remote_type' => $ipsec['remote_type'], ':remote_sip' => $ipsec['remote_sip'], ':remote_eip' => $ipsec['remote_eip'], ':remote_subnet' => $ipsec['remote_subnet'], 
				      ':keep_alive' => $ipsec['keep_alive'], ':ka_source' => $ipsec['ka_source'], ':ka_remote' => $ipsec['ka_remote'], ':ka_period' => $ipsec['ka_period'], ':ka_fail_count' => $ipsec['ka_fail_count'], 
				      ':policy_status' => $ipsec['policy_status'], ':auto_initiate' => $ipsec['auto_initiate'], ':direction' => $ipsec['direction'], ':exchange_mode' => $ipsec['exchange_mode'], ':ike_ver' => $ipsec['ike_ver'],
				      ':local_id_type' => $ipsec['local_id_type'], ':local_id_val' => $ipsec['local_id_val'], ':remote_id_type' => $ipsec['remote_id_type'], ':remote_id_val' => $ipsec['remote_id_val'], 
				      ':encryp_p1' => $ipsec['encryp_p1'], ':encryp_p2' => $ipsec['encryp_p2'], ':key_length' => $ipsec['key_length'],
				      ':auth_p1' => $ipsec['auth_p1'], ':auth_p2' => $ipsec['auth_p2'], ':auth_type' => $ipsec['auth_type'], ':pskey' => $ipsec['pskey'], ':dhgroup' => $ipsec['dhgroup'], 
				      ':salife_p1' => $ipsec['salife_p1'], ':salife_p2' => $ipsec['salife_p2'], ':salife_type' => $ipsec['salife_type'], 
				      ':pfs' => $ipsec['pfs'], ':dpd' => $ipsec['dpd'], ':dpd_period' => $ipsec['dpd_period'], ':dpd_failcount' => $ipsec['dpd_failcount'], 
				      ':nat' => $ipsec['nat'], ':nat_ka_time' => $ipsec['nat_ka_time'], 
				      ':x509_remote_cert' => $ipsec['x509_remote_cert'], ':x509_local_cert' => $ipsec['x509_local_cert'], ':x509_local_key' => $ipsec['x509_local_key'], ':vpnmode' => $ipsec['vpnmode']);
		
		$result_db = $this->dt->exec_sqlite($this->conn, $sql, $data);									//execute query
		debug('(ipsec_controller.inc|setIpSec()) db query result: $result', $result); 				//DEBUG
		
		if($result_db > 0)		//db commit successfull
		{
			debug('(ipsec_controller.inc|setIpSec()) Successfully saved ipsec settings to db'); 	//DEBUG
			
			$result_conf = $this->generateconf();				//generate the ipsec conf

			if($result_conf === true)	//if conf is successfully generated
			{				
				debug('(ipsec_controller.inc|setIpSec()) IPSEC settings successfully saved.'); 		//DEBUG
				$result['success'] = 'true';
				$result['codes'][] = 410;
				
				//$this->ipsecRestart();		//Restart ipsec service
			}
			else
			{
				debug('(ipsec_controller.inc|setIpSec()) Failed to save IPSEC settings.'); 			//DEBUG
				$result['success'] = 'false';
				$result['codes'][] = 411;
				$result['codes'][] = 412;
			}
		}
		else
		{
			debug('(ipsec_controller.inc|setIpSec()) Failed to save ipsec settings to db'); 	//DEBUG
			$result['success'] = 'false';
			$result['codes'][] = 411;
			$result['codes'][] = 403;
		}
		
		return $result;
		
	} //END setIpSec
	
	/**
	 * getIpSec
	 * 
	 * Fetch the ipsec settings from the db to display in the UI
	 * @author Sean Toscano (sean@absolutetrac.com)
	 * @todo add support for multiple ipsec profiles
	 */
	function getIpSec()
	{
		$ipsec = array();	//array to store the settings. this array will be passed to the front end controller to display on the UI
		
		$sql = 'select * from ipsec where OID IN (select max(OID) from ipsec)';  //fetch the current ipsec policy	//this will need to be changed once we move to multiple ipsec profiles
		$results = $this->dt->query_sqlite($this->conn, $sql);									//execute query
		
		debug('(ipsec_controller.inc|getIpSec()) db query result: $results', $results); 		//DEBUG
		
		if(count($results) > 0)	//if policy found in db
		{
			foreach($results as $result)	
			{
				$ipsec['policy'] = $result['VPNPolicyName'];
				$ipsec['mode'] = (($result['TunnelOrTransport'] == 1) ? 'Tunnel' : 'Transport');
				$ipsec['REtype'] = $result['RemoteEndPointType'];
				$ipsec['vpnmode'] = $result['VPNMode'];
				
				//REMOTE ENDPOINT TYPE
				switch($ipsec['REtype'])
				{
					case 'FQDN':
						$ipsec['RE_fqdn'] = $result['RemoteEndPoint'];
						break;
					case 'IP';
						$ipsec['RE_ip'] = explode(".", $result['RemoteEndPoint']);
						break;
				}
				
				//LOCAL NETWORK TYPE
				switch($result['LocalNetworkType'])
				{
					case 0:
						$ipsec['LN_type'] = 'Any';
						break;
					case 1:
						$ipsec['LN_type'] = 'Single';
						break;			
					case 2:
						$ipsec['LN_type'] = 'Range';
						break;
					case 3:
						$ipsec['LN_type'] = 'Subnet';
						break;
				}
				
				//LOCAL NETWORK INFO
				$ipsec['LN_sip'] = explode(".", $result['LocalStartAddress']);
				$ipsec['LN_eip'] = explode(".", $result['LocalEndAddress']);
				$ipsec['LN_subnet'] = explode(".", $result['LocalSubnetMask']);
				
				//REMOTE NETWORK TYPE
				switch($result['RemoteNetworkType'])
				{
					case 0:
						$ipsec['RN_type'] = 'Any';
						break;
					case 1:
						$ipsec['RN_type'] = 'Single';
						break;
					case 2:
						$ipsec['RN_type'] = 'Range';
						break;
					case 3:
						$ipsec['RN_type'] = 'Subnet';
						break;
				}
				
				//REMOTE NETWORK INFO
				$ipsec['RN_sip'] = explode(".", $result['RemoteStartAddress']);
				$ipsec['RN_eip'] = explode(".", $result['RemoteEndAddress']);
				$ipsec['RN_subnet'] = explode(".", $result['RemoteSubnetMask']);
				
				//KEEP ALIVE
				switch($result['EnableKeepAlive'])
				{
					case 0:
						$ipsec['keep_alive'] = 'Off';
						break;
					case 1:
						$ipsec['keep_alive'] = 'On';
						break;
				}
				
				//EXCHANGE MODE
				switch($result['ExchangeMode'])
				{
					case 0:
						$ipsec['exchange'] = 'Main';
						break;
					case 1:
						$ipsec['exchange'] = 'Aggressive';
						break;
				}
				
				//NAT
				switch($result['NatTraversal'])
				{
					case 0:
						$ipsec['nat'] = 'Off';
						break;
					case 1:
						$ipsec['nat'] = 'On';
						break;
				}
				
				$ipsec['nat_ka'] = $result['NatKeepAliveTime'];
				
				//LOCAL IDENTIFIER
				$ipsec['LID_type'] = $result['LocalIdentifierType'];
				
				//LOCAL IDENTIFIER TYPE
				switch($ipsec['LID_type'])
				{
					case 'FQDN':
						$ipsec['LID_fqdn'] = $result['LocalIdentifier'];
						break;
					case 'Email':
						$ipsec['LID_fqdn'] = $result['LocalIdentifier'];
						break;
					case 'IP';
						$ipsec['LID_ip'] = explode(".", $result['LocalIdentifier']);
						break;
				}
				
				//REMOTE IDENTIFIER TYPE
				$ipsec['RID_type'] = $result['RemoteIdentifierType'];
				
				switch($ipsec['RID_type'])
				{
					case 'FQDN':
						$ipsec['RID_fqdn'] = $result['RemoteIdentifier'];
						break;
					case 'Email':
						$ipsec['RID_fqdn'] = $result['RemoteIdentifier'];
						break;
					case 'IP';
						$ipsec['RID_ip'] = explode(".", $result['RemoteIdentifier']);
						break;
				}
				
				$ipsec['encryp_p1'] = $result['EncryptionAlgorithm'];	//PHASE 1 ENCRYPTION ALGORITHM
				$ipsec['encryp_p2'] = $result['P2EncryptionAlgorithm'];	//PHASE 2 ENCRYPTION ALGORITHM
				$ipsec['auth_p1'] = $result['AuthAlgorithm'];			//PHASE 1 AUTHENTICATION ALGORITHM
				$ipsec['auth_p2'] = $result['P2AuthAlgorithm'];			//PHASE 2 AUTHENTICATION ALGORITHM
				
				//AUTHENTICATION TYPE
				switch($result['AuthType'])
				{
					case 0:
						$ipsec['auth_type'] = 'Preshared';
						break;
					case 1:
						$ipsec['auth_type'] = 'RSA';
						break;
					case 2:
						$ipsec['auth_type'] = 'X509';
						break;
				}
				
				$ipsec['ps_key'] = $result['Presharedkey'];		//PRE-SHARED KEY
				$ipsec['dh_group'] = $result['DHGroup'];		//DH GROUP
				$ipsec['sa_life_p1'] = $result['SALifeTime'];	//PHASE 1 SA LIFETIME
				$ipsec['sa_life_p2'] = $result['P2SALifeTime'];	//PHASE 2 SA LIFETIME
				
				//DEAD PEER DETECTION
				switch($result['DPD'])
				{
					case 0:
						$ipsec['dpd'] = 'Off';
						break;
					case 1:
						$ipsec['dpd'] = 'On';
						break;
				}
				
				//PFS - PERFECT FORWARD SECURITY
				switch($result['PFS'])
				{
					case 0:
						$ipsec['pfs'] = 'Off';
						break;
					case 1:
						$ipsec['pfs'] = 'On';
						break;
				}
				
			} //END foreach
		} //END if
		else //no policy in db
    {
      //set default value
      $ipsec['protocol']='1';
      $ipsec['mode']='Tunnel';
      $ipsec['REtype']='IP';
      $ipsec['vpnmode']='Client';
      $ipsec['LN_type']='Any';
      $ipsec['RN_type']='Any';
      $ipsec['keep_alive']='On';
      $ipsec['exchange']='Main';
      $ipsec['nat']='On';
      $ipsec['nat_ka']='3600';
      $ipsec['auth_type']='Preshared';
      $ipsec['sa_life_p1']='86400';
      $ipsec['sa_life_p2']='3600';
      $ipsec['dpd']='On';
      $ipsec['pfs']='On';
    }

		debug('(ipsec_controller.inc|getIpSec()) $ipsec - parsed', $ipsec); 		//DEBUG
		
		return $ipsec;
	} // END getIpSec

  function createIpsecDB()
  {
    $sqlc="CREATE TABLE ipsec(
      OID                     integer   primary key,   -- OID
      IpsecProtocol           integer   NOT NULL,      -- 0 for AH and 1 for ESP, default is 1 (ESP)
      TunnelOrTransport       boolean   NOT NULL,      -- 1 for tunnel mode, 0 for transport, default:1
      VPNPolicyName text      NOT NULL UNIQUE,         -- VPN Policy Name
      VPNMode                 text      NULL,          -- Client mode or Gateway mode
      RemoteEndPointType      boolean   NOT NULL,      -- Type of Remote GateWay (IPAddress/FQDN)
      RemoteEndPoint          text      NOT NULL,      -- IP or FQDN name
      LocalNetworkType        integer   NOT NULL,      -- Type of local traffic selectors (Any/Single/Range/Subnet)   
      LocalStartAddress       text      NULL,          -- Local network Start Address
      LocalEndAddress         text      NULL,          -- Local network End Address
      LocalSubnetMask         text      NULL,          -- Local network Subnet Mask
      RemoteNetworkType       integer   NOT NULL,      -- Type of Remote traffic selectors (Any/Single/Range/Subnet)   
      RemoteStartAddress      text      NULL,          -- Remote network Start Address
      RemoteEndAddress        text      NULL,          -- Remote network End Address
      RemoteSubnetMask        text      NULL,          -- Remote network Subnet Mask
      EnableKeepAlive         boolean   NULL,          -- enable/disable KeepAlive
      KeepAliveSourceAddress  text      NULL,          -- source IP address of keepalive packet   
      RemotePingIPAddress     text      NULL,          -- IP address of th remote host to check KA   
      KeepAlivePeriod         integer   NULL,          -- KeepAlive Detection Period (seconds)
      KeepAliveFailureCount   integer   NULL,          -- Reconnect after failure count for KA
      Status                  boolean   NOT NULL,      -- status of the VPN policy (enable/disable)
      AutoInitiate            boolean   NOT NULL,      -- Auto establishes the tunnel if enabled
      Direction               integer   NOT NULL,    -- Direction (Responder/Initiator/Both)
      ExchangeMode            boolean   NOT NULL,    -- ExchangeMode (Main/Aggressive)    
      IKEVersion              integer   NOT NULL,    -- IKE Version
      LocalIdentifierType     integer   NOT NULL,    -- LocalIdentifierType (IP/FQDN/Dynamic IP + email authentication)
      LocalIdentifier         text      NULL,        -- LocalIdentifier
      RemoteIdentifierType    integer   NOT NULL,    -- RemoteIdentifierType (IP/FQDN/)
      RemoteIdentifier        text      NULL,        -- RemoteIdentifier
      EncryptionAlgorithm     integer   NOT NULL,    -- EncryptionAlgorithm
      P2EncryptionAlgorithm   integer   NOT NULL,    -- Phase2 EncryptionAlgorithm
      KeyLength               text      NULL,        -- Key length for variable key-size algorithms like blowfish and cast128  
      AuthAlgorithm           integer   NOT NULL,    -- AuthAlgorithm
      P2AuthAlgorithm         integer   NOT NULL,    -- Phase2 AuthAlgorithm
      AuthType                integer   NOT NULL,    -- Authentication Type (presharedkey/RSA signature/x509-certification) 
      Presharedkey            text      NULL,        -- Presharedkey
      DHGroup                 integer   NOT NULL,    -- DH Group 1/2/3
      SALifeTime              long integer  NOT NULL,    -- SA Life Time
      P2SALifeTime            long integer  NOT NULL,    -- Phase2 SA Life Time
      SALifeTimeType          boolean       NULL,        -- SA LifeTime Type     
      PFS                     boolean       NULL,        -- PFS (yes or no)
      DPD                     boolean       NOT NULL,    -- enable/disable DPD
      DPDDetectionPeriod      integer       NULL,        -- DPD Detection Period
      DPDFailureCount         integer       NULL,        -- DPD Failure Count   
      NatTraversal            boolean       NOT NULL,    -- 1-On, 0-off default:1
      NatKeepAliveTime        integer       NOT NULL,    -- Interval between sending keep-alive packets, in seconds 
      X509Remotecert          text      NULL,            -- X509 remote cert
      X509Localcert           text      NULL,            -- X509 local cert
      X509Localkey            text      NULL,            -- X509 local key
      last_update NOT NULL DEFAULT current_timestamp
    )";
		$result_db = $this->dt->exec_sqlite($this->conn, $sqlc);									//execute query
    debug('(ipsec_controller.inc|createIpsecDB()) : '.$result_db);	//DEBUG
  }
} //END ipseccontroller
