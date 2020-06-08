<?php 

require_once $_SERVER['DOCUMENT_ROOT'].'/inc/db_sqlite3.inc';	//contains functions for db interaction
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';			//contains functions for socket interaction, error message display, and logging.


$timestamp = time();
$current_date_time = formatDateTime($timestamp, "UTC");	//format the time: 10:55am (UTC) - Monday, April 3, 2013

$dt = new db_sqlite3();			
$conn = $dt->connect_sqlite(DB);	//open db

$sql_device="select * from device";		//read device data from 'device' table
$result_device = $dt->query_sqlite($conn, $sql_device);

$device = $result_device[0];	
$device['serial'] = getSerial();	//look up the device serial from the conf file

$version = getFirmwareVer();		//look up the firmware version and last updated date from the conf file
$device['firmware_ver'] = $version['num'];	
$device['last_firmware_update'] = $version['date'];
debug('(display_controller.inc) $device info',$device); 		//DEBUG
$serialNumber = (!empty($device['serial']) ? $device['serial'] : '');
$firmwareVersion = (!empty($device['firmware_ver']) ? $device['firmware_ver'] : '');

$dt->close_sqlite($conn);	//close db connection



//set access for general users (USER == 300)
if(isUser())
{
	$access = array("ethernet","wifi-ap","ipsec", "cellular", "positionCheck", "hardwareCheck", "outputCheck", "inputCheck", "gps");	//form id to disable
	echo setUserAccess($access);													//call function that will return jquery code to disable above forms
}
//set access for admin users (ADMIN == 200) or installers (INSTALLER == 100) -- installer access can be split out into its own; if needed
else if(isAdmin())
{
	$access = array("ethernet","wifi-ap");	//form id to disable
	echo setAdminAccess($access);													//call function that will return jquery code to disable above forms
}


/**
 * setUserAccess
 * 
 * Writes jQuery code that disables the $form_ids elements for USER(id = 3) level users.
 * @param array $form_ids - form ids to disable
 * @return string - jquery code
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function setUserAccess($form_ids)
{
	$js = '<script type="text/javascript">';
	$js .= '$(document).ready(function(){';
			foreach($form_ids as $formid)
			{
				$js .= '$("form#'.$formid.' input[type=\'text\']").attr(\'readonly\',true);	
						$("form#'.$formid.' input[name=\'customAPN\']").attr(\'disabled\',true); 		 	   		  
						$("form#'.$formid.' input[type=\'radio\']").attr(\'disabled\',true);
						$("form#'.$formid.' input[type=\'password\']").attr(\'disabled\',true);
						$("form#'.$formid.' select").attr(\'disabled\',true);
						$("form#'.$formid.' input[name^=\'emac\']").attr(\'readonly\',true)
						$("form#'.$formid.' input[name^=\'wmac\']").attr(\'readonly\',true);
						';		//make input fields readonly; disable radio buttons; disable password fields; disable dropdown menus
			} 
	$js .=	'});';	
	$js .= '</script>';
	
	return $js;
} //END setUserAccess

/**
 * setAdminAccess
 *
 * Writes jQuery code that disables the $form_ids elements for ADMIN (id = 2) level users.
 * @param array $form_ids - form ids to disable
 * @return string - jquery code
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function setAdminAccess($form_ids)
{
	$js = '<script type="text/javascript">';
	$js .= '$(document).ready(function(){';
			foreach($form_ids as $formid)
			{
		$js .= '$("form#'.$formid.' input[name^=\'emac\']").attr(\'readonly\',true);
				$("form#'.$formid.' input[name^=\'wmac\']").attr(\'readonly\',true);
				';		//make Wifi and Ethernet MAC Address readonly for admin
			}
	$js .=	'});';
	$js .= '</script>';

	return $js;
} //END setAdminAccess


/**
 * setSuperAdminAccess
 *
 * Writes jQuery code that disables the $form_ids elements for SUPER ADMIN (id = 1) level users.
 * @param array $form_ids - form ids to disable
 * @return string - jquery code
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function setSuperAdminAccess($form_ids)
{
	$js = '<script type="text/javascript">';
	$js .= '$(document).ready(function(){';
	foreach($form_ids as $formid)
	{
		$js .= '$("form#'.$formid.' input[name^=\'emac\']").attr(\'readonly\',false);
				$("form#'.$formid.' input[name^=\'wmac\']").attr(\'readonly\',false);
				';		//make Wifi and Ethernet MAC Address writable for super admin
	}
	$js .=	'});';
	$js .= '</script>';

	return $js;
} //END setSuperAdminAccess

/**
 * getFirmwareVer
 * 
 * Read the firmware version and date of last firmware update of the device from the 'version' file. 
 * @return array - [0] contains last firmware update date; [1] contains firmware version number
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function getFirmwareVer()
{
	$conf = '/version';										//file where firmware info is stored
	$ver = array('num' => 'unknown', 'date' => 'unknown');	//initialize

	$readfile = file($conf,FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
	
	//readfile[0] = minor ver num (eg. 3831). readfile[2] = full ver num (eg. 1.4B.3831)
	if($readfile)
	{
		$ver['date'] = $readfile[1];
		$firmwareVersion = explode(".", $readfile[2]);
		$ver['num'] = $firmwareVersion[0] . '.' . (int)($firmwareVersion[3]/10000) . '.' . $firmwareVersion[2] . '.' . (int)($firmwareVersion[3] % 10000);
	}
	
	return $ver;
} //END getFirmwareVer

function GetBatteryVoltage()
{
	$sh_args = 'BatteryVoltage '; //admin client command for setting apn
	$sh_out = atsexec(escapeshellcmd($sh_args));	//socket call 	
	return $sh_out;	
}

function GetMicroVersion()
{
	$sh_args = 'MicroVersion '; //admin client command for setting apn
	$sh_out = atsexec(escapeshellcmd($sh_args));	//socket call 	
	return $sh_out;	
}
function GetHWVersion()
{
	$sh_args = 'HWVersion '; //admin client command for setting apn
	$sh_out = atsexec(escapeshellcmd($sh_args));	//socket call 	
	return $sh_out;	
}
/**
 * getSerial
 * 
 * Read the device's serial number from the /mnt/nvram/rom/sn.txt file
 * @return string - device's serial number
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function getSerial()
{
	$serialconf = '/mnt/nvram/rom/sn.txt';	//file containing serial
	$serial = 'unknown';					//initializing
	
	$readfile = file($serialconf,FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
	
	if($readfile)
		$serial = $readfile[0];
	
	return $serial;
} //END getSerial


/* 
 * DEPRECATED: Was used to get the local time zone by using the Google API, was causing slow page loads
 * getTime
 * Computes the correct local time by using the system time and GPS co-ordinates to query the Google Maps API and get the current timezone and DST offset.
 * 
 * @return array - local timestamp and timezone
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function getTime()
{
	$timestamp = time();	//fetch system time
	$local_time = array('timestamp' => $timestamp, 'timezone' => 'UTC');
	debug('(display_controller.inc|getTime()) Unix UTC timestamp', $timestamp);  	//DEBUG
	debug('(display_controller.inc|getTime()) timezone', date_default_timezone_get());  	//DEBUG
	
	$sh_args = 'getgpsdata'; 						//admin client command for getting gps coordinates
	$sh_out = atsexec(escapeshellcmd($sh_args));	//socket call
	debug('(display_controller.inc|getTime()) admin client api command: getgpsdata output', $sh_out); 		//DEBUG
	
	if(strcasecmp($sh_out,'GPS is invalid') != 0 && strcasecmp($sh_out,'phpcmd: fail') != 0 && strcasecmp($sh_out,'phpcmd: invalid command') != 0)
	{
		$xml = new SimpleXMLElement($sh_out);	//create SimpleXML object
		debug('(display_controller.inc|getTime()) xml converted to SimpleXmlElement', $xml);  				//DEBUG
	
		if(isset($xml->latitude) && isset($xml->longitude))
		{
			//Google Maps API call
			$gmaps_arg = "https://maps.googleapis.com/maps/api/timezone/json?timestamp=".$timestamp."&sensor=true&location=".$xml->latitude.",".$xml->longitude;
			$json_object = file_get_contents($gmaps_arg);
			debug('(display_controller.inc|getTime()) Google API Args', $gmaps_arg);  			//DEBUG
			debug('(display_controller.inc|getTime()) Google API result ', $json_object);  		//DEBUG
			
			if($json_object)
			{
				$time_array = json_decode($json_object);						//convert json -> array
				debug('(display_controller.inc|getTime()) Google API result - json decode', $time_array);  		//DEBUG
				
				if($time_array->status == "OK")
				{
					$local_time['timestamp'] =  $timestamp + $time_array->dstOffset + $time_array->rawOffset;	//calculate local time = epoch time + daylight savings offset + UTC/GMT offset
					debug('(display_controller.inc|getTime()) processed timestamp', $local_time['timestamp']);  	//DEBUG
					preg_match_all('([A-Z])', $time_array->timeZoneName, $regexMatches,PREG_PATTERN_ORDER);		//abbreviate timezone. Mountain Daylight Time => MDT
					$local_time['timezone'] = implode('', $regexMatches[0]);
					return $local_time;
				}
				else	//can't calculate from Google API, then return system time.
				{	
					debug('(display_controller.inc|getTime()) Failed to get time from Google API');  	//DEBUG
					return $local_time;	
				}
			}
			else	//can't calculate from Google API, then return system time.
			{
				debug('(display_controller.inc|getTime()) Failed to get time from Google API');  	//DEBUG
				return $local_time;
			}	
		}
	}
	else
	{
		debug('(display_controller.inc|getTime()) Failed to get GPS coordinates from Admin client');  	//DEBUG
		return $local_time;	//can't reach admin client, then return system time.
	}
} //END getTime

/**
 * formatDateTime
 * Takes the epoch timestamp and formats it as, 10:55am (UTC) - Monday, April 3, 2013, for display on the UI
 * 
 * @param integer $timestamp
 * @param string $timezone
 * @return string
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function formatDateTime($timestamp, $timezone)
{	
	$current_date = getdate($timestamp);			//format date from timestamp
	$current_time = date("H:i:s",$timestamp);		//format time from timestamp: hh:mm am/pm
	
	$current_date_time = (!empty($current_date['weekday']) ? $current_date['weekday'].", ":'')
						.(!empty($current_date['month']) ? $current_date['month']." " : '')
						.(!empty($current_date['mday']) ? $current_date['mday'].", " : '')
						.(!empty($current_date['year']) ? $current_date['year'] : '')
						.(!empty($current_time) ? " - ".$current_time." (".$timezone.")" : '');
	
	return $current_date_time;	
} //END formatDateTime
debug('(display_controller.inc) $device info 2'); 		//DEBUG

?>
