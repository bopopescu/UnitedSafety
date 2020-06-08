<?php

require_once $_SERVER['DOCUMENT_ROOT'].'/inc/db_sqlite3.inc';	//contains functions for db interaction
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';			//contains functions for socket interaction, error message display, and logging. 

class cellcontroller
{
        var $imei_file = '/tmp/config/imei';		//IMEI number stored in this file 
        var $wandns_file = '/etc/ppp/resolv.conf';	//WAN DNS stored in this file
        var $dt = '';
        var $conn = '';

        function __construct()
        {
                $this->dt = new db_sqlite3();
                $this->conn = $this->dt->connect_sqlite(DB);
        }
        
        /**
         * setApn
         *
         * Set the APN on the device
         * This function sets the apn by calling the admin client function 'setdefaultapn'
         * @return boolean - true if admin client command is successfully executed, false if failed to execute command
         * @author Sean Toscano (sean@absolutetrac.com)
         */
        function setApn($apn)
        {
        	$sh_args = 'setdefaultapn '.$apn; //admin client command for setting apn
        	$sh_out = atsexec(escapeshellcmd($sh_args));	//socket call 	
        	debug('(cell_controller.inc|setApn()) admin client api command: setdefaultapn '.$apn); 						//DEBUG
        	debug('(cell_controller.inc|setApn()) admin client api command setdefaultapn output: sh_out', $sh_out); 	//DEBUG

        	if(($sh_out == 'phpcmd: ok') || ($sh_out == 'phpcmd:ok'))
        	{
        		debug('(cell_controller.inc|setApn()) setdefaultapn command completed.'); 		//DEBUG
        		return true;
        	}
        	else
        	{
        		debug('(cell_controller.inc|setApn()) setdefaultapn command failed.'); 			//DEBUG
        		return false;
        	}
        } //END setApn
        
        
        /** 
         * ****** DEPRECATED ***********
         * getApnList
         * 
         * Get the list of secondary apns
         * This function gets the secondary apn list by calling the admin client function 'getapnlist'
         * @return string $html - select tag html markup
         * @author - Sean Toscano (sean@absolutetrac.com)
         */
        function getApnList()
        {
        	$activeApn = $this->getApn();	//get the current apn being used by the device
        	debug('(cell_controller.inc|getApnList()) current apn being used on the device'.$activeApn); 				//DEBUG
        	
        	$sh_args = 'getapnlist';		//admin client command for getting secondary apn list
        	$sh_out = atsexec(escapeshellcmd($sh_args));	//socket call
        	debug('(cell_controller.inc|getApnList()) admin client api command getapnlist output: sh_out', $sh_out); 	//DEBUG
        	
        	if(($sh_out != 'phpcmd: fail') && ($sh_out != 'phpcmd: invalid command'))
        	{
        		$xml = new SimpleXMLElement($sh_out);	//create SimpleXML object
	        	$html =''; 									//store the html markup
	        	debug('(cell_controller.inc|getApnList()) apn list converted into SimpleXMLElement object '.$xml); 		//DEBUG
	        	
	        	foreach($xml->network as $network)			
	        	{
	        		$html .= '<optgroup label="'.$network->attributes()->name.'">';	 		//create Carrier section
	        		foreach ($network->apn as $apn)											//group apns under each Carrier
	        		{
	        			debug('(cell_controller.inc|getApnList()) $apn '.$apn); 		//DEBUG
	        			$html .= '<option '.((strcasecmp($apn,$activeApn) == 0) ? 'selected="selected"':'').' value="'.$apn.'">'.$apn.'</option>';
	        		}
	        		$html .= '</optgroup>';
	        	}

	        	return $html;
        	}
        	else
        	{
        		return '<option value="">Failed to retrieve APN list</option>';
        	}
        } //END getApnList
		
        /**
         * getCellNetwork
         * 
         * Get the cellular interface networking information from the ifconfig command.
         * @param string $if - name of the cellular interface (ppp0)
         * @return array $interface - contains the cellular ip address, gateway, and subnet.
         * @author Lee Wei (lee@absolutetrac.com); Sean Toscano (sean@absolutetrac.com)
         */
        function getCellNetwork($if)
        {
        	$arlines = array();		//Store result of ifconfig command
        	exec('ifconfig '.escapeshellarg($if).' '.'2>&1 ', $arlines);	//execute ifconfig
        	debug('(cell_controller.inc|getCellNetwork()) admin client api command: ifconfig '.escapeshellarg($if).' '.'2>&1 '); 	//DEBUG
        	debug('(cell_controller.inc|getCellNetwork()) admin client api command output: $arlines', $arlines); 					//DEBUG
        	
        	//convert array into single line of text
        	$str='';
        	foreach($arlines as $line)
        	{
        		$str=$str.$line;
        	}
        	
        	//run regex on string and find: IP, Gateway, Subnet
        	$regex=array();
        	preg_match("/^([A-z]*\d)\s+Link\s+encap:([A-z-]*)\s.*\sinet addr:([0-9.]+)\s*P-t-P:([0-9.]+)\s*Mask:([0-9.]+)/ims", $str, $regex);
        	
        	$interface = array();
        	if( !empty($regex) ){
        		$interface['name'] = $regex[1];
        		$interface['type'] = $regex[2];
        		$interface['ip'] = $regex[3];
        		$interface['broadcast'] = $regex[4];
        		$interface['mask'] = $regex[5];
        	}
        	debug('(cell_controller.inc|getCellNetwork()) $interface array '.$interface); 				//DEBUG
        	
        	return $interface;
        } //END getCellNetwork
        
        /**
         * getSim
         * 
         * Get the sim number by calling the admin client command: 'getcellccid'
         * @return string cell sim number
         * @author Sean Toscano (sean@absolutetrac.com)
         */
        function getSim()
        {
        	$sh_args = 'getcellccid'; 	//admin client command for getting sim number
        	$sh_out = atsexec(escapeshellcmd($sh_args));	//socket call
        	debug('(cell_controller.inc|getSim()) admin client api command "getcellccid" output: $sh_out', $sh_out); 	//DEBUG
        	
        	if($sh_out != 'phpcmd: fail' && $sh_out != 'phpcmd: invalid command')
        	{
        		debug('(cell_controller.inc|getSim()) getcellccid command completed.'); 								//DEBUG
        		return $sh_out;
        	}
        	else
        	{
        		debug('(cell_controller.inc|getSim()) getcellccid command failed.'); 									//DEBUG
        		return false;
        		//return 'Failed to detect sim number';
        	}
        } //END getSim
        
        /**
         * getPhoneNumber
         * 
         * Get the phone number off the sim card by calling the admin client command: 'getPhoneNumber'
         * @return string
         * @author Sean Toscano (sean@absolutetrac.com)
         */
        function getPhoneNumber()
        {
        	$sh_args = 'getcellpnumber'; //admin client command for getting phone number
        	$sh_out = atsexec(escapeshellcmd($sh_args));	//socket call
        	debug('(cell_controller.inc|getPhoneNumber()) admin client api command "getcellpnumber" output: $sh_out', $sh_out); 	//DEBUG
        	 
        	if($sh_out != 'phpcmd: fail' && $sh_out != 'phpcmd: invalid command')
        	{
        		debug('(cell_controller.inc|getPhoneNumber()) getcellpnumber command completed.'); 	//DEBUG
        		return $sh_out;
        	}
        	else
        	{
        		debug('(cell_controller.inc|getPhoneNumber()) getcellpnumber command failed.'); 	//DEBUG
        		return false;
        		//return 'Failed to detect phone number';
        	}
        } //END getPhoneNumber
        
        /**
         * getCarrier
         * 
         * Get the cellular carrier/network name by calling the admin client command: 'getcellnetwork'
         * @return string
         * @author Sean Toscano (sean@absolutetrac.com)
         */
        function getCarrier()
        {
        	$sh_args = 'getcellnetwork'; //admin client command for getting cellular carrier/network name
        	$sh_out = atsexec(escapeshellcmd($sh_args));	//socket call
        	debug('(cell_controller.inc|getCarrier()) admin client api command "getcellnetwork" output: $sh_out', $sh_out); 	//DEBUG
        	 
        	if($sh_out != 'phpcmd: fail' && $sh_out != 'phpcmd: invalid command')
        	{
        		debug('(cell_controller.inc|getCarrier()) getcellnetwork command completed.'); 	//DEBUG
        		return $sh_out;
        	}
        	else
        	{
        		debug('(cell_controller.inc|getCarrier()) getcellnetwork command failed.'); 	//DEBUG
        		return false;
        		//return 'Failed to detect cellular carrier';
        	}
        } //END getCarrier

        /**
         * getApn
         * 
         * Get the active apn by calling the admin client command: 'getcellapn'
         * @return string
         * @author Sean Toscano (sean@absolutetrac.com)
         */
        function getApn()
        {
        	$sh_args = 'getcellapn'; //admin client command for getting cellular apn
        	$sh_out = atsexec(escapeshellcmd($sh_args));	//socket call
        	debug('cell_controller.inc|getApn(): admin client api command "getcellapn" output: $sh_out', $sh_out); 	//DEBUG
        	debug('dbconfig_controller.inc|getDbconfigData: '.$sh_args.' (admin client)', $sh_out);  	//DEBUG
        	
        	if($sh_out != 'phpcmd: fail' && $sh_out != 'phpcmd: invalid command')
        	{
        		debug('(cell_controller.inc|getApn()) getcellapn command completed.'); 	//DEBUG
        		return $sh_out;
        	}
        	else
        	{
        		debug('(cell_controller.inc|getApn()) getcellapn command failed.'); 	//DEBUG
        		//return 'Failed to detect apn';
        		return false;
        	}
        } //END getApn
        
        
        /**
         * getRssi
         * 
         * Get the rssi in dbm by calling the admin client command: 'getcellrssi'
         * @return string
         * @author Sean Toscano (sean@absolutetrac.com)
         */
        function getRssi()
        {
        	$sh_args = 'getcellrssi'; 	//admin client comand for getting cellular signal strength (rssi)
            $sh_out = atsexec(escapeshellcmd($sh_args));	//socket call
            debug('(cell_controller.inc|getRssi()) admin client api command "getcellrssi" output: $sh_out', $sh_out); 		//DEBUG

            if($sh_out != 'phpcmd: fail' && $sh_out != 'phpcmd: invalid command' && $sh_out != 99)
            {
            	debug('(cell_controller.inc|getRssi()) getcellrssi command completed.'); 	//DEBUG
            	return $sh_out;
            }
            else
            {
            	debug('(cell_controller.inc|getRssi()) getcellrssi command failed.'); 		//DEBUG
            	return false;
            	//return 'Failed to detect cellular signal';
            }
        } //END getRssi
        
        /**
         * getCellDns
         * 
         * Read the WAN DNS entries from the /etc/ppp/resolv.conf file
         * @return string | boolean - returns the dns ip or false if file can't be read or doesn't exist
         * @author Sean Toscano (sean@absolutetrac.com)
         */
        function getCellDns()
        {
        	$readfile = file($this->wandns_file,FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);	//read the file
        	debug('(cell_controller.inc|getCellDns()) reading dns file at: '.$this->wandns_file); 			//DEBUG
        	debug('(cell_controller.inc|getCellDns()) contents of dns file', $readfile); 					//DEBUG
                
            if($readfile)
            {
            	$dns_result = '';
                $dns_str = implode(',', $readfile);
                preg_match_all("/(([0-9]+\.?)+)/", $dns_str, $dns_result); //regex to match for the ip addresses
                debug('(cell_controller.inc|getCellDns()) successfully read and parsed dns file contents.'); 	//DEBUG
                return $dns_result[0];
            }
            else
            {
            	debug('(cell_controller.inc|getCellDns()) failed to read dns file contents.'); 					//DEBUG
            	return false;
            }
        } //END getCellDns
        
        /**
         * getImei
         * 
         * Read the IMEI number from the /tmp/config/imei file
         * @return string | boolean - returns the imei number or false if file can't be read or doesn't exist
         * @author Sean Toscano (sean@absolutetrac.com)
         */
        function getImei()
        {
        	$readfile = file($this->imei_file,FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
        	debug('(cell_controller.inc|getImei()) reading imei from file at: '.$this->imei_file); 		//DEBUG
        	debug('(cell_controller.inc|getImei()) contents of imei file', $readfile); 					//DEBUG
        	
        	if($readfile)
        	{
        		debug('(cell_controller.inc|getImei()) successfully read and parsed imei from file.'); 	//DEBUG
        		return $readfile[0];
        	}
        	else
        	{
        		debug('(cell_controller.inc|getImei()) failed to read imei from file.'); 				//DEBUG
        		return false;
        	}	
        } //END getImei
        
} //END class cellcontroller
?>
