<?php
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/db_sqlite3.inc';		//contains functions for db interaction
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';				//contains functions for socket interaction, error message display, and logging.
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/network_controller.inc'; //network (ethernet, wireless) controller

class wificontroller
{
    var $interface;
    var $confpath = '/etc/Wireless/RT2870AP/RT2870AP.dat';		//path to wifi conf file
    var $path = '/tmp/webconfig/';								//location for php script to store the file. admin client reads the file from this location
    var $datfile = 'RT2870AP.dat';								//name of wifi conf file
    private $nt_ctrl = null;
    private $dbconfig = null;
    
    function __construct()
    {
        $this->nt_ctrl = new networkcontroller();
        $this->dbconfig = new dbconfigController();
    }

    //wifi conf file locations. not really needed. keep as an fyi
    var $arfilelocation = array(
        'iwconfig'           => '/usr/sbin/iwconfig',
        'iwlist'             => '/usr/sbin/iwlist',
        'iwpriv'             => '/usr/sbin/iwpriv',
        '/proc/net/wireless' => '/proc/net/wireless'
    );
    
    //Regexes
    var $channelreg='/^\d{1,3}/';
    var $essidreg='/^[0-9:a-z:A-Z\s]{32}/';
    var $macreg='/^([0-9:a-f]{17})/';
    var $securityreg = '/^[0-9:A-Z]+\/[0-9:A-Z]+/';
    var $signalreg='/\d{0,2}/';

    /**
     * enableclient
     * 
     * Enables the network interface named in $this->interface
     * NOTE: This function is not being used right now.
     * @author Lee Wei (lee@absolutetrac.com)
     */
    function enableclient()
    {
        exec('ifconfig ' . escapeshellarg($this->interface) . ' up');
        exec($this->arfilelocation['iwpriv'] . ' ' . escapeshellarg($this->interface) . ' set SiteSurvey=1');
    } //END enableclient

    /**
     * disableclient
     *
     * Disables the network interface named in $this->interface
     * NOTE: This function is not being used right now.
     * @author Lee Wei (lee@absolutetrac.com)
     */
    function disableclient()
    {

        exec('ifconfig ' . escapeshellarg($this->interface) . ' down');
    } //END disableclient

    /**
     * scan
     * Scans for Wifi access points
     * NOTE: This function is not being used right now.
     * @return multitype:
     * @author Lee Wei (lee@absolutetrac.com)
     */
    function scan()
    {
        $arlines = array();
        exec(
            $this->arfilelocation['iwpriv'] . ' '
            . escapeshellarg($this->interface) . ' get_site_survey',
                $arlines
            );

        return $this->parsescan($arlines);
    } //END scan


    /**
     * parsescan
     *
     * NOTE: This function is not being used right now.
     * @author Lee Wei (lee@absolutetrac.com)
     */
    function parsescan($arLines)
    {
        #no data in first two lines.skip them.
        array_shift($arLines);
        array_shift($arLines);

        $result = array();
        foreach($arLines as $key=>$line)
        {
            if(trim($line) == '')
            {
               continue;
            }

            $arMatches = array();
            $temp = array();
            if (!preg_match($this->channelreg, $line, $arMatches))
                print "channel not found";

            $temp['channel']=$arMatches[0];

            $line = trim(substr($line,strlen($arMatches[0])));
            if (!preg_match($this->essidreg, $line, $arMatches))
                print "essid not found";

            $temp['essid']=trim($arMatches[0]);

            $line = trim(substr($line,strlen($arMatches[0])));
            if (!preg_match($this->macreg, $line, $arMatches))
                print "mac not found";

            $temp['mac']=$arMatches[0];

            $line = trim(substr($line,strlen($arMatches[0])));
            if (!preg_match($this->securityreg, $line, $arMatches))
                print "security not found";

            $temp['security']=$arMatches[0];

            $line = trim(substr($line,strlen($arMatches[0])));

            if (!preg_match($this->signalreg, $line, $arMatches))
                print "signal not found";

            $temp['signal']=$arMatches[0];

            array_push($result, $temp);

            print("\n");

        }
        return $result;
    } //END parsescan

    /**
     * switchChannel
     *
     * NOTE: This function is not being used right now.
     * @author Lee Wei (lee@absolutetrac.com)
     */
    function switchChannel($ch)
    {
        exec( $this->arfilelocation['iwpriv'] . ' '
            . escapeshellarg($this->interface) . ' set Channel='.$ch);
    } //END switchChannel

    
   /**
    * getconnectedAP
    * 
    * Returns an string with the name of access point associated.
    * NOTE: This function is not being used right now.
    * 
    * @access public
    * @return string with associated AP name as values, if string is empty, no ap connected.
    * @author Lee Wei (lee@absolutetrac.com)
    */
    function getconnectedAP()
    {
        $arMatches = array();
        
        exec( $this->arfilelocation['iwconfig'] . ' '
                . escapeshellarg($this->interface), $arMatches );

        $essidreg = '/ESSID:"([^"]+)"/';

        $match = array();
        if(preg_match($essidreg, $arMatches[0], $match))
            return $match[1];
        else
            return '';
    } //END getconnectedAP

    /**
    * connectAP
    * Try to connecte to an access point.
    * NOTE: This function is not being used right now.
    * 
    * @param string $essid....
    * @return empty string If the interface is connected, otherwise retun fail info.
    * @access public
    * @author Lee Wei (lee@absolutetrac.com)
    */
    function connectAP($essid, $passw, $authMode, $encryptType, $retry=5)
    {
        $result = $this->scan();
        $ch = -1;

        foreach($result as $line)
        {
            if($line['essid'] == $essid)
            {
                $ch = $line['channel'];
            }
        }

        if($ch == -1)
        {
            echo "No Essid: ".$essid." existing";
            return "No Essid: ".$essid." existing";
        }


        exec('ifconfig ' . escapeshellarg($this->interface) . ' down');
        $this->switchChannel($ch);
        exec( $this->arfilelocation['iwpriv'] . ' '
            . escapeshellarg($this->interface) . ' set ApCliEnable=0');
        exec( $this->arfilelocation['iwpriv'] . ' '
            . escapeshellarg($this->interface) . ' set ApCliSsid='.escapeshellarg($essid));
        exec( $this->arfilelocation['iwpriv'] . ' '
            . escapeshellarg($this->interface) . ' set ApCliAuthMode='.$authMode);
        exec( $this->arfilelocation['iwpriv'] . ' '
            . escapeshellarg($this->interface) . ' set ApCliEncrypType='.$encryptType);
        exec( $this->arfilelocation['iwpriv'] . ' '
            . escapeshellarg($this->interface) . ' set ApCliWPAPSK='.$passw);
        exec( $this->arfilelocation['iwpriv'] . ' '
            . escapeshellarg($this->interface) . ' set ApCliEnable=1');
        exec('ifconfig ' . escapeshellarg($this->interface) . ' up');

        $isConnected = 0;
        while($retry--)
        {
            if(!strcmp($this->getconnectedAP(), $essid))
            {
                $isConnected=1;
                break;
            }

            if($retry == 0 )
                break;

            # sleep 1 seconds.
            sleep(1);
        }

        if($isConnected == 1)
            return '';
        else
            return 'fail';
    }

	/**
    * APupdate
    *  
	* this function is the public accessible wrapper for the generateRT2870APCONF function
	* authmode: "OPEN", "SHARED", "WEPAUTO", "WPAPSK", "WPA", "WPA2PSK", "WPA2", "WPA1WPA2", "WPAPSKWPA2PSK", default value is WPA2PSK.
	* encryptype: "NONE", "WEP", "TKIP", "AES", "TKIPAES", default is "TKIP"
    * @access public
    */

	function APupdate($data)
	{
		return $this->generateRT2870APCONF($data);
	}

	/**
	 * generateRT2870APCONF
	 * 
	 * Generate access point conf file, update database and send ioctl command to AP module.
	 * @param array $data - array containing wifi ap info: "essid", "authmode","encryptype", "password" as keys.
	 * @return boolean true if admin client command is successfully executed, false if failed to execute command
	 * @access private
	 */
	private function generateRT2870APCONF($data)
	{
		$result = array("success" => "false", "codes" => array());	//array for capturing result status, error codes
		
		debug('(wifi_controller.inc|generateRT2870APCONF()) data', $data); 	//LOG

		if(!is_array($data) || empty($data))
		{
			debug('(wifi_controller.inc|generateRT2870APCONF()) data parameter is empty.'); 	//LOG
			$result['success'] = 'false';
			return $result;
		}
		//DB object
		$dt = new db_sqlite3();
		$conn = $dt->connect_sqlite(DB);
		
		//initialize
		$essid= '';
		$password = '';
		$authmode = '';
		$encryptype = '';
		$interface = '';
		$mac = '';

		//variable assignment
		foreach($data as $key => $value)
		{
			switch($key)
			{
				case "interface":
					$interface=$value;
					break;
				case "mac":
					$mac=$value;
					break;
				case "essid":
					$essid=$value;
					break;
				case "authmode":
					$authmode=$value;
					break;
				case "encryptype":
					$encryptype=$value;
					break;
				case "password":
					$password=$value;
					break;
			}
		}
		
		debug('(wifi_controller.inc|generateRT2870APCONF()) interface', $interface); 	//LOG
		debug('(wifi_controller.inc|generateRT2870APCONF()) essid', $essid); 			//LOG
		debug('(wifi_controller.inc|generateRT2870APCONF()) authmode', $authmode); 		//LOG
		debug('(wifi_controller.inc|generateRT2870APCONF()) encryptype', $encryptype); 	//LOG
		debug('(wifi_controller.inc|generateRT2870APCONF()) password', $password); 		//LOG
		debug('(wifi_controller.inc|generateRT2870APCONF()) mac', $mac); 				//LOG

		$apinfo = $this->getAPinfo();
		debug('(wifi_controller.inc|generateRT2870APCONF()) checking for (SSID + AUTH + ENCRYP) in conf', $apinfo); 	//LOG

		//No password provided: ssid not changed, or encryptype is NONE
		if(empty($password))
		{
			debug('(wifi_controller.inc|generateRT2870APCONF()) password field empty');
			if( strcasecmp($encryptype, 'NONE') != 0 )
			{
				if((strcasecmp($apinfo['ssid'], $essid) == 0) && !empty($apinfo['pass']))
				{
					//only update autmode and encryptype
					debug('(wifi_controller.inc|generateRT2870APCONF()) update authmode and encryptype');
					if(strcasecmp($apinfo['encryp'], $encryptype) != 0)
					{
						$this->dbconfig->setDbconfigData('WiFi', 'encryptype', $encryptype);
					}
					if(strcasecmp($apinfo['auth'], $authmode) != 0)
					{
						$this->dbconfig->setDbconfigData('WiFi', 'authmode', $authmode);
					}

					$result['success'] = 'true';
					return $result;
				}
			}
			else
			{
				//encryptype: NONE
				$this->dbconfig->setDbconfigData('WiFi', 'ssid', $essid);
				$this->dbconfig->setDbconfigData('WiFi', 'authmode', $authmode);
				$this->dbconfig->setDbconfigData('WiFi', 'encryptype', $encryptype);
				$this->dbconfig->setDbconfigData('WiFi', 'password', '');
				$result['success'] = 'true';
				return $result;
			}
		}
		else
		{
			//update password
			debug('(wifi_controller.inc|generateRT2870APCONF()) User entered a password');
			$pass_len = strlen($password);

			//if password encrypt type = TKIP, AES, TKIPAES
			if(((strcasecmp($encryptype,'TKIP') == 0) || (strcasecmp($encryptype,'AES') == 0) || (strcasecmp($encryptype,'TKIPAES') == 0)))
			{
				if($pass_len < 8 || $pass_len > 63)
				{
					debug('(wifi_controller.inc|generateRT2870APCONF()) Incorrect password length. Allowed length = 8 - 63 characters. Entered length = '.$pass_len);
					$result['success'] = 'false';
					$result['codes'][] = 213;
					return $result;
				}
				else
				{
					$password = exec('wpa_passphrase ' . escapeshellarg($essid).' '. escapeshellarg($password) . ' | sed -n '.escapeshellarg('4p').' | cut -d '.escapeshellarg('=').' -f 2');
				}
			}
			//if password encrypt type = WEP
			else if((strcasecmp($encryptype,'WEP') == 0) && ($pass_len != 5 && $pass_len != 13))
			{
				debug('(wifi_controller.inc|generateRT2870APCONF()) Password length error.  Allowed length = 5 or 13 characters. Entered length = '.$pass_len); 	//LOG
				$result['success'] = 'false';
				$result['codes'][] = 214;
				return $result;
			}

			if(strcasecmp($apinfo['ssid'], $essid) != 0)
				$this->dbconfig->setDbconfigData('WiFi', 'ssid', $essid);

			if(strcasecmp($apinfo['auth'], $authmode) != 0)
				$this->dbconfig->setDbconfigData('WiFi', 'authmode', $authmode);

			if(strcasecmp($apinfo['encryp'], $encryptype) != 0)
				$this->dbconfig->setDbconfigData('WiFi', 'encryptype', $encryptype);

			$this->dbconfig->setDbconfigData('WiFi', 'password', $password);
			$result['success'] = 'true';
		}

		debug('(wifi_controller.php|generateRT2870APCONF()) Result array: ', $result);	//DEBUG
		return $result;
	} //END generateRT2870APCONF

	
	/**
	 * getAPinfo()
	 * Read the Access Point info(ssid, password, auth mode, encrypt type) by parsing the .dat file at $this->confpath
	 * 
	 * @return array - contains wifi ap config info
	 * @author Sean Toscano (sean@absolutetrac.com)
	 */
	function getAPinfo()
	{
		$wifi_dat = file($this->confpath);		//open the conf file for reading
		debug('(wifi_controller.inc|getAPinfo()) reading wifi conf file at: '.$this->confpath); 	//LOG
		
		$ap_data = array();
		
		if($wifi_dat)
		{
			foreach ($wifi_dat as $data)
			{
				if(strpos($data,"SSID=") === 0)
				{
					$ssid = explode('=', $data);
					$ap_data['ssid'] = trim($ssid[1]);				
				}
				if(strpos($data,"AuthMode=") === 0)
				{
					$auth = explode('=', $data);
					$ap_data['auth'] = trim($auth[1]);
				}
				if(strpos($data,"EncrypType=") === 0)
				{
					$encryp = explode('=', $data);
					$ap_data['encryp'] = trim($encryp[1]);
				}
				if(strpos($data,"Key1Str=") === 0 && (strcasecmp($ap_data['encryp'],'WEP') == 0))
				{
					$pass = explode('=', $data);
					$ap_data['pass'] = $pass[1];
				}
				if(strpos($data,"WPAPSK=") === 0 && ((strcasecmp($ap_data['encryp'],'TKIP') == 0) || (strcasecmp($ap_data['encryp'],'AES') == 0) || (strcasecmp($ap_data['encryp'],'TKIPAES') == 0)))
				{
					$pass = explode('=', $data);
					$ap_data['pass'] = $pass[1];
				} 
				if(strpos($data,"Channel=") === 0)
				{
					$channel = explode('=', $data);
					$ap_data['channel'] = trim($channel[1]);
				}
			}
		} //END if
		debug('(wifi_controller.inc|getAPinfo()) wifi ap data read from conf file', $ap_data); 	//LOG
		return $ap_data;
	} //END getAPinfo
	
} //END wificontroller
?>
