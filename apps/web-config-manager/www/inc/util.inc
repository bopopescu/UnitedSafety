<?php

session_start();

require_once $_SERVER['DOCUMENT_ROOT'].'inc/config.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'inc/db_sqlite3.inc';		//contains functions for db interaction
require_once $_SERVER['DOCUMENT_ROOT'].'inc/klogger.inc'; 		//logging utility

$log = new KLogger('/tmp/webconfig/', KLogger::ERR);		//instantiating logging utility

$db = new db_sqlite3();
//$db_conn = $db->connect_sqlite($_SERVER['DOCUMENT_ROOT'].'/../www-app/config_mang.db');
$db_conn = $db->connect_sqlite(DB);

/**
 * readresult
 *
 * Reads data from the socket connection. The data stream read is terminated with a carriage return.
 * The carriage return is not part of the result returned.
 * @param object $socket
 * @return Reads a standard ATS command response (which is a string of bytes terminated by carriage return).
 * @author Lee Wei (lee@absolutetrac.com)
 * @author Amour Hassan (Amour.Hassan@gps1.com)
 */
function readresult($p_socket)
{
	$data = '';

	while($resp = socket_read($p_socket,1024))
	{

		if("\r" == substr($resp, -1))
		{
			$data .= substr($resp, 0, -1);
			break;
		}

		$data .= $resp;
	}

	return $data;
} //END readresult

/**
 * atsexec
 *
 * Establishes a socket connection to the device and reads/writes data to that socket
 * @param string $str - the command to be sent over the socket
 * @return string - output of running the admin client/shell command over the socket
 * @author Lee Wei (lee@absolutetrac.com)
 * @author Amour Hassan (Amour.Hassan@gps1.com)
 */
function atsexec($p_str)
{
	$socket = socket_create(AF_INET, SOCK_STREAM, SOL_TCP); //create a socket connection
	$result = socket_connect($socket, '127.0.0.1', 39000);	//connect to localhost at port 39000

	if($result === false)
	{
		return false;
	}

	$cmd = "phpcmd $p_str\r";				//admin client API format: phpcmd <command name> [parameters]\r
	socket_write($socket, $cmd, strlen($cmd));
	$output = readresult($socket);				//read reply from socket
	socket_close($socket);
	return $output;
} //END atsexec

/**
 * display_msg
 *
 * Displays success, error, warning messages on the UI
 * @param string $msg - the content of the message
 * @param string $type - type of message (success, fail, warning); this indicates which icon should be used besides the message
 * @return none - prints message on screen/UI
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function display_msg($msg, $type)
{
	$html = '';	//instantiation

	if($type == 'success')
	{
		$html = '<div class="successImage"></div><div class="successMsg">'.$msg.'</div><div style="clear:both;"></div>';
	}
	else if($type == 'fail')
	{
		$html = '<div class="failImage"></div><div class="failMsg">'.$msg.'</div><div style="clear:both;"></div>';
	}
	else if($type == 'warning')
	{
		$html = '<div class="warningImage"></div><div class="warningMsg">'.$msg.'</div><div style="clear:both;"></div>';
	}

	//write out jquery code to add the above html markup into the msgBox div container.
	$jscript = '<script type="text/javascript">';
	$jscript .= '$(".msgBox").append(\''.$html.'\');';
	$jscript .= '</script>';
	echo $jscript;	//display

} //END display_msg

/**
 * debug
 * Print dump of parameter to screen or log file depending on whether debug REQUEST parameter is set to true or not
 *
 * @param string $msg - the header/title message to display before dumping the content from $item to screen/log file
 * @param mixed $item - the data to dump to screen/log
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function debug($msg, $args="NO_ARGS")
{
	if(DEBUG == "ON")
	{
		global $log;		//logging utility object
		if($args !== "NO_ARGS")			//if a variable is supplied, pass it thru to the logging utility
		{
			$log->logLCM($msg, $args);	//temp fix for bug #636
		}
		else
		{
			$log->logLCM($msg);			//temp fix for bug #636
		}
	}
} //END debug

/**
 * highlightField
 *
 * Writes out jquery code that hightlights the form fields whose names are passed in the $field parameter.
 * @param string $field
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function highlightField($field)
{
	if(!empty($field))
	{
		$jscript = '<script type="text/javascript">';
		$jscript .= '$(document).ready(function(){';

		$jscript .= '$("*[name = '.$field.']").closest("div.row").children(".label, .label2, .column-label, .label-tight").addClass("highlighted-error");';

		$jscript .= '});';

		$jscript .= '</script>';
		echo $jscript;	//display
	}
} //END highlightFields

/**
 * isOn
 *
 * Validates whether an input is set and equates to "On" or "1" or 1
 * @param mixed[string | int] $val
 * @return boolean
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function isOn($val)
{
	return (isset($val) && (strcasecmp($val,'On') == 0 || strcasecmp($val,'Enabled') == 0 || $val === 1 || strcasecmp($val,'1') == 0)) ? true : false;
}

/**
 * isOff
 *
 * Validates whether an input is set, not false, and equates to "Off" or "0" or 0 or ''
 * @param mixed[string | int] $val
 * @return boolean
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function isOff($val)
{
	return (isset($val) && $val !== false && (strcasecmp($val,'Off') == 0 || strcasecmp($val,'Disabled') == 0 || $val === 0 || strcasecmp($val,'0') == 0 || $val == '')) ? true : false;
}

/**
 * isValidOnOff
 *
 * Validates whether an input equates to either "On", "1", 1, "Off", "0", 0
 * @param mixed[string | int] $val
 * @return boolean
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function isValidOnOff($val)
{
	if(isset($val) && $val !== false && (strcasecmp($val,'On') == 0 || strcasecmp($val,'Off') == 0 || strcasecmp($val,'Enabled') == 0 || strcasecmp($val,'Disabled') == 0 || strcasecmp($val,'1') == 0 || strcasecmp($val,'0') == 0 || $val === 1 || $val === 0))
	{
		return true;
	}
	else
	{
		return false;
	}
} //END isValidOnOff

/**
 * isValidNumber
 *
 * Validates whether an input is a positive number
 * @param int $val
 * @return boolean
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function isValidNumber($val)
{
	if(isset($val) && $val !== false && is_numeric($val) && $val >= 0)
	{
		return true;
	}
	else
	{
		return false;
	}
} //END isValidNumber

function isValidInteger($val)
{
	return (isset($val) && $val !== false && is_numeric($val));
}

/**
 * isValidString
 *
 * Validates whether an input is a valid string
 * @param mixed[string | int | boolean] $val
 * @return boolean
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function isValidString($val)
{
	if(!empty($val) && is_string($val))
	{
		return true;
	}
	else
	{
		return false;
	}
} //END isValidString

/**
 * isValidTemplateName
 *
 * Validates whether an input is a valid template name (A-Z,a-z, 0-9, - , _)
 * @param string $name
 * @return boolean
 * @author Tyson Pullukatt (Tyson.Pullukatt@gps1.com)
 */
function isValidTemplateName($name)
{
	return (!preg_match('/[^-\w]/i', $name));
}

function isValidIP($ip)
{
	return preg_match('/\d{1,3}.\d{1,3}.\d{1,3}.\d{1,3}/',$ip);	//returns 1 if matches, 0 if no matches and false if error
}

function isValidDNS($dns)
{
	return preg_match('/[\w\.:\/\/]*[\w-]+\.[\w]+[\w\.-~:\/?#@!$&+,;=]*/',$dns);	//returns 1 if matches, 0 if no matches and false if error
}

/**
 * isValidMAC
 *
 * Validates whether an input is a valid mac address in the form xx:xx:xx:xx:xx:xx
 * where x is a valid hex number 
 * @param string $name
 * @return boolean
 * @author Tyson Pullukatt (Tyson.Pullukatt@gps1.com)
 */
function isValidMAC($mac)
{
	return preg_match('/[a-fA-F0-9]{2}:[a-fA-F0-9]{2}:[a-fA-F0-9]{2}:[a-fA-F0-9]{2}:[a-fA-F0-9]{2}:[a-fA-F0-9]{2}/',$mac);	//returns 1 if matches, 0 if no matches and false if error
}

function translateStatusCode($code, $mod=null)
{
	global $db, $db_conn;

	$sql="select * from status_codes where code = :code";
	$data = array(":code" => $code);
	debug('translateStatusCode() $data', $data);
	$result = $db->query_sqlite($db_conn, $sql, $data);	//execute query
	debug('translateStatusCode() $sql', $sql); //DEBUG
	debug('(util.inc|translateStatusCode()) $result: ', $result); 	//DEBUG

	if(count($result) > 0)
	{
		//echo $result[0]['message']." - ".$result[0]['type'];

		display_msg($result[0]['message'], $result[0]['type']);
	}
	else
	{
		return false;
	}
} //END translateStatusCode

function reboot()
{

	$sh_args = 'reboot';	//admin client command for rebooting device
	$sh_out = atsexec(escapeshellcmd($sh_args));	//socket call

	debug('(util.inc|reboot()) admin client api command: \'reboot result', $sh_out);	//DEBUG

	if(($sh_out == 'phpcmd: ok') || ($sh_out == 'phpcmd:ok'))
	{
		debug('(util.inc|reboot()): Rebooting device.');	//DEBUG
		return true;
	}
	else
	{
		debug('(util.inc|reboot()): Failed to reboot device.');	//DEBUG
		return false;
	}
}

/*
 * commandSuccessful
 * Interprets the response from the admin client API
 */
function commandSuccessful($cmd_result)
{
	if(!empty($cmd_result) && $cmd_result[0] == 'phpcmd: ok')
	{
		return true;
	}
	else
	{
		return false;
	}
}

/*
 * isSuperAdmin
 * Checks if the account that's logged in has 'super admin' access
 */
function isSuperAdmin()
{
	//debug('[isSuperAdmin] $_SESSION[\'M2M_SESH_USERAL\'] = ' .$_SESSION['M2M_SESH_USERAL']);
	if(isset($_SESSION['M2M_SESH_USERAL']) && $_SESSION['M2M_SESH_USERAL'] == 1)
	{
		return true;
	}
	else
	{
		return false;
	}
}

/*
 * isAdmin
* Checks if the account that's logged in has 'admin' access
*/
function isAdmin()
{
	//debug('[isAdmin] $_SESSION[\'M2M_SESH_USERAL\'] = ' .$_SESSION['M2M_SESH_USERAL']);
	if(!empty($_SESSION['M2M_SESH_USERAL']) && $_SESSION['M2M_SESH_USERAL'] == 200)
	{
		return true;
	}
	else
	{
		return false;
	}
}

/*
 * isUser
* Checks if the account that's logged in has 'user' access
*/
function isUser()
{
	if(!empty($_SESSION['M2M_SESH_USERAL']) && $_SESSION['M2M_SESH_USERAL'] == 300)
	{
		return true;
	}
	else
	{
		return false;
	}
}


/*
 * hasSubmitAccess
 * Checks if the account that's logged in has higher priviledges than a 'user' account
 */
function hasSubmitAccess()
{
	//debug('[hasSubmitAccess] $_SESSION[\'M2M_SESH_USERAL\'] = ' .$_SESSION['M2M_SESH_USERAL']);

	if(!empty($_SESSION['M2M_SESH_USERAL']) && $_SESSION['M2M_SESH_USERAL'] < 300)
	{
		return true;
	}
	else
	{
		return false;
	}
}

function trimRequest($request)
{

	foreach($request as &$value)
	{
		$value = trim($value);
	}

	return $request;
}

function isReservedPort($port)
{
	if(in_array($port, $GLOBALS["trulink_reserved_ports"]) || $port < 1024 || $port > 64535)
	{
		return true;
	}
	else
	{
		return false;
	}
}

function doesRangeContainNumbers($range_start, $range_end, $reserved_nums)
{
	foreach($reserved_nums as $reserved_num)
	{
		if($reserved_num >= $range_start && $reserved_num <= $range_end)
		{
			return true;
		}
	}

	return false;
}

function isValidIPOctet($ip_octet)
{
	if(isValidNumber($ip_octet) && $ip_octet > 0 && $ip_octet < 256)
	{
		return true;
	}
	else
	{
		return false;
	}
}

function isValidAlphaNumeric($string)
{
	return ctype_xdigit($string);
}

/*
 * Displays a blocking message in a modal/pop-up box and waits for user to acknowledge
 * Setting $customFormat to true will not wrap the message in a custom HTML tag and allow custom HTML to be used
 *
 */
function displayModalMessage($message, $customFormat=false)
{
	$jscript = '<script type="text/javascript">';
	$jscript .= '$(document).ready(function(){';

	if($customFormat){
		$jscript .= 'displayModalMessage("'.$message.'<br /><input type=\"button\" id=\"ackModal\" value=\"OK\" />"); ';
	}
	else{
		$jscript .= 'displayModalMessage("<h3>'.$message.'</h3><input type=\"button\" id=\"ackModal\" value=\"OK\" />"); ';
	}

	$jscript .= '});';

	$jscript .= '</script>';

	echo $jscript;	//display
}

/**
 * getMsgPriorityFromSlider
 * returns the message priority based on the value of the slider
 * @param  int $val - the value from the slider
 * @return int
 * @author Tyson Pullukatt (Tyson.Pullukatt@gps1.com)
 */
function getMsgPriorityFromSlider($val)
{
	if($val == Iridium_Slider_Value)
	{
		return Iridium_Msg_Priority;
	}
	elseif($val == Wifi_Slider_Value)
	{
		return Wifi_Msg_Priority;
	}

	return Cell_Msg_Priority;
}
/**
 * getSliderValue
 * returns the slider value based on the message priority from db-config
 * @param  int $pri - the msg priority value from db-config
 * @return int
 * @author Tyson Pullukatt (Tyson.Pullukatt@gps1.com)
 */
function getSliderValue($pri)
{
	if($pri == '')
	{
		return Cell_Slider_Value;
	}
	$pri = hex2bin($pri);

	if($pri <= 10 )
	{
		return Iridium_Slider_Value;
	}
	elseif(($pri > 10) && ($pri <= 100))
	{
		return Cell_Slider_Value;
	}
	elseif($pri > 100)
	{
		return Wifi_Slider_Value;
	}

	return Cell_Slider_Value;
}
?>

