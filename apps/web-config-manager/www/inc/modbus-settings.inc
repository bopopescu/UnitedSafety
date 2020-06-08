<?php
require_once $_SERVER['DOCUMENT_ROOT'].'inc/dbconfig_controller.inc';

//get page variable values

$dbconfig = new dbconfigController();

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

/*
 * ATS-FIXME: All the checks for GET variables can be simplified to using
 * a single function.
*/
$templateName = '';
if(isset($_GET['templateName']) && $_GET['templateName'] !== false)
{
	$templateName = $_GET['templateName'];
	debug("templateName=", $templateName);
}

if(isset($_GET['LensEnable']) && $_GET['LensEnable'] !== false)
{
	$value = $_GET['LensEnable'];
	$LensEnable = ((strcmp($value, "On") == 0));
}
else
{
	$value = $dbconfig->getDbconfig('isc-lens', 'ModbusEnabled');
	$value = hex2bin(trim($value));
	debug("LensEnable=", $value);
	$LensEnable = ($value == 1);
}

if(isset($_GET['EthernetEnable']) && $_GET['EthernetEnable'] !== false)
{
	$value = $_GET['EthernetEnable'];
	$EthernetEnable = ((strcmp($value, "On") == 0));
}
else
{
	$value = $dbconfig->getDbconfig('RedStone', 'Ethernet');
	$value = hex2bin(trim($value));
	debug("EthernetEnable=", $value);
	$EthernetEnable = ($value == 1);
}

if(isset($_GET['enable']) && $_GET['enable'] !== false)
{
	$value = $_GET['enable'];
	$enable = ((strcmp($value, "On") == 0));
}
else
{
	$value = $dbconfig->getDbconfig('feature', 'modbus-monitor');
	$value = hex2bin(trim($value));
	debug("enable=", $value);
	$enable = ($value == 1);
}

if(isset($_GET['modbusMode']) && $_GET['modbusMode'] !== false)
{
	$modbusMode= $_GET['modbusMode'];
}
else
{
	$modbusMode= $dbconfig->getDbconfigData('modbus', 'protocol');
}

if(isset($_GET['baudrate']) && $_GET['baudrate'] !== false)
{
	$baudrate = $_GET['baudrate'];
}
else
{
	$baudrate = trim($dbconfig->getDbconfigData('modbus','baudrate'));
}

if(isset($_GET['data_bits']) && $_GET['data_bits'] !== false)
{
	$data_bits = $_GET['data_bits'];
}
else
{
	$data_bits = trim($dbconfig->getDbconfigData('modbus','data_bits'));
}


if(isset($_GET['parity']) && $_GET['parity'] !== false)
{
	$parity = $_GET['parity'];
}
else
{
	$parity = trim($dbconfig->getDbconfigData('modbus','parity'));
}

if(isset($_GET['stop_bits']) && $_GET['stop_bits'] !== false)
{
	$stop_bits = $_GET['stop_bits'];
}
else
{
	$stop_bits = trim($dbconfig->getDbconfigData('modbus','stop_bits'));
}

if(isset($_GET['modbustcpip']) && $_GET['modbustcpip'] !== false)
{
	$mip_raw = $_GET['modbustcpip'];
	$mip = explode(".", $mip_raw);
}
else
{
	$mip_raw = $dbconfig->getDbconfigData('modbus', 'ipovertcp');
	$mip = (isValidIP($mip_raw) ? explode(".", $mip_raw) : array(" ", " ", " ", " "));
}

if(isset($_GET['modbustcpport']) && $_GET['modbustcpport'] !== false)
{
	$mport = $_GET['modbustcpport'];
}
else
{
	$mport = $dbconfig->getDbconfigData('modbus', 'portovertcp');
}

if(isset($_GET['qDelaySeconds']) && $_GET['qDelaySeconds'] !== false)
{
	$qDelaySeconds = $_GET['qDelaySeconds'];
}
else
{
	$qDelaySeconds = trim($dbconfig->getDbconfigData('modbus','q_delay_seconds'));
}

if(isset($_GET['periodicSeconds']) && $_GET['periodicSeconds'] !== false)
{
	$periodicSeconds = $_GET['periodicSeconds'];
}
else
{
	$periodicSeconds = trim($dbconfig->getDbconfigData('modbus','periodic_seconds'));
}

if(isset($_GET['periodicOveriridiumMinutes']) && $_GET['periodicOveriridiumMinutes'] !== false)
{
	$periodicOveriridiumMinutes= $_GET['periodicOveriridiumMinutes'];
}
else
{
	$periodicOveriridiumMinutesRaw = $dbconfig->getDbconfigData('modbus', 'periodic_overiridium_seconds');
	$periodicOveriridiumMinutes = trim(isValidNumber($periodicOveriridiumMinutesRaw) ? $periodicOveriridiumMinutesRaw/60 : '60');
}

$modbus_db_data = $dbconfig->getDbconfig('modbus-db', NULL);
$modbus_db_array = split("\n", $modbus_db_data);
$templates_array = array();
$slave_array = array();
foreach ($modbus_db_array as $entry)
{
	$entry_array = split(' ', $entry);
	$key = hex2bin($entry_array[0]);

	$templateHeaderLength = strlen("template_");
	$slaveHeaderLength = strlen("slave");
	if(strncmp($key, "template_", $templateHeaderLength) == 0)
	{
		$templates_array[] = substr($key, $templateHeaderLength);
	}
	elseif(strncmp($key, "slave", $slaveHeaderLength) == 0)
	{
		$slave_num = substr($key,$slaveHeaderLength);
		$val = hex2bin($entry_array[1]);
		$slave_array[$slave_num] = $val;
	}
}
?>
