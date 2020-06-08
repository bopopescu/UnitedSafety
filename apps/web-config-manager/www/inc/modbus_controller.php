<?php
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/session_validator.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/dbconfig_controller.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';	//contains functions for socket interaction, error message display, and logging.

define('_SUCCESS_SAVE_SETTINGS_CODE', '10');
define('_FAIL_SAVE_SETTINGS_CODE', '11');
define('_FAIL_SAVE_FIELDS_CODE', '12');
define('_REBOOT_MESSAGE_CODE', '14');
define('_TEMPLATE_FILE_LARGE_CODE', '1200');

define('_MAX_TEMPLATE_FILE_SIZE', '61440'); //60KB

$dbconfig = new dbconfigController();
//Check form submission
if(!empty($_REQUEST) && !empty($_REQUEST['csrfToken']) && $_REQUEST['csrfToken'] == $_SESSION['csrfToken'] && isset($_SESSION['M2M_SESH_USERAL']) && $_SESSION['M2M_SESH_USERAL'] == 200)
{
	debug('=========_REQUEST=============', $_REQUEST);

	$result = array("success" => 'false', "module" => "Modbus", "codes" => array(), "fields" => null, "getParams" => null);	//array for capturing result status, status code, and field names.

	switch($_REQUEST['op'])
	{
		case "updateSettings":
			$result = updateSettings($dbconfig, trimRequest($_REQUEST));
			header("location:http://".$_SERVER['HTTP_HOST']."/device/modbus/index.php?success=".$result['success']."&module=add".$result['module']."Template&codes=".implode(",",$result['codes'])."&fields=".$result['fields'].$result['getParams']);
			break;
		case "addTemplate";
			$result = addTemplate($dbconfig, trimRequest($_REQUEST), $_FILES);
			header("location:http://".$_SERVER['HTTP_HOST']."/device/modbus/index.php?success=".$result['success']."&module=add".$result['module']."Template&codes=".implode(",",$result['codes'])."&fields=".$result['fields'].$result['getParams']);
			break;
		case "deleteTemplate":
			$result = deleteTemplate($dbconfig, trimRequest($_REQUEST));
			echo "http://".$_SERVER['HTTP_HOST']."/device/modbus/index.php?success=".$result['success']."&module=".$result['module']."&codes=".implode(",",$result['codes'])."&fields=".$result['fields'].$result['getParams'];
			break;
		case "addAssignment":
			$result = addAssignment($dbconfig, trimRequest($_REQUEST));
			header("location:http://".$_SERVER['HTTP_HOST']."/device/modbus/index.php?success=".$result['success']."&module=".$result['module']."&codes=".implode(",",$result['codes'])."&fields=".$result['fields'].$result['getParams']);
			break;
		case "deleteAssignment":
			$result = deleteAssignment($dbconfig, trimRequest($_REQUEST));
			echo "http://".$_SERVER['HTTP_HOST']."/device/modbus/index.php?success=".$result['success']."&module=".$result['module']."&codes=".implode(",",$result['codes'])."&fields=".$result['fields'].$result['getParams'];
			break;
		default:
			header("location:http://".$_SERVER['HTTP_HOST']."/device/modbus/index.php?success=".$result['success']."&module=".$result['module']."&codes=".implode(",",$result['codes'])."&fields=".$result['fields'].$result['getParams']);
	}

}
else
{
	header("location:http://".$_SERVER['HTTP_HOST']."/device/modbus/index.php");
}

/**
 * updateSettings
 * Saves the modbus settings into dbconfig using the dbconfig wrapper
 * @param object $p_dbconfig - wrapper class for dbconfig interaction
 * @param array $request - form REQUEST array
 * @author Tyson Pullukatt (Tyson.Pullukatt@gps1.com)
 */
function updateSettings($p_dbconfig, $request)
{
	$result = array("success" => 'false', "module" => "Modbus", "codes" => array(), "fields" => null, "getParams" => null);	//array for capturing result status, status code, and field names.
	$settingsSaved = false;
	$result['fields'] = '';
	$result['getParams'] = '';


	if(!empty($request['LensEnable']))
	{
		$LensEnable = $request['LensEnable'];
		$app = 'isc-lens';
		$key = 'ModbusEnabled';
		if(isOff($LensEnable))
		{
			$settingsSaved = $p_dbconfig->setDbconfig($app, $key, 0);
		}
		else
		{
			$settingsSaved = $p_dbconfig->setDbconfig($app, $key, 1);
		}
	}
	if(!empty($request['EthernetEnable']))
	{
		$EthernetEnable = $request['EthernetEnable'];
		$app = 'RedStone';
		$key = 'Ethernet';
		if(isOff($EthernetEnable))
		{
			$settingsSaved = $p_dbconfig->setDbconfig($app, $key, 0);
		}
		else
		{
			$settingsSaved = $p_dbconfig->setDbconfig($app, $key, 1);
		}
	}
	
	if(!empty($request['enable']))
	{
		$enable = $request['enable'];
		if(isOff($enable))
		{
			$app = 'feature';
			$key = 'modbus-monitor';
			$settingsSaved = $p_dbconfig->setDbconfig($app, $key, 0);
			$p_dbconfig->setDbconfig($app, 'can-odb2-monitor', 1);  // turn can back on when modbus is off
		}
		else
		{
			$modbus_results = array();
			$app = "modbus";
			$modbus_results['enable'] = $p_dbconfig->setDbconfig('feature', 'modbus-monitor', 1);
			$p_dbconfig->setDbconfig('feature', 'can-odb2-monitor', 0);  // turn can back on when modbus is off
			$modbus_results['qDelaySeconds'] = ((isValidNumber($request['qDelaySeconds'])&& ($request['qDelaySeconds'] >= 0))?
		($p_dbconfig->setDbconfig($app, 'q_delay_seconds', $request['qDelaySeconds'])):false);
			$modbus_results['periodicSeconds'] = ((isValidNumber($request['periodicSeconds'])&&($request['periodicSeconds'] > 0))?
				($p_dbconfig->setDbconfig($app, 'periodic_seconds', $request['periodicSeconds'])):false);
			$modbus_results['periodicOveriridiumMinutes'] = ((isValidNumber($request['periodicOveriridiumMinutes']))?
				($p_dbconfig->setDbconfig($app, 'periodic_overiridium_seconds', $request['periodicOveriridiumMinutes']*60)):false);

			$modbusMode = '';
			if( (isset($request['modbusMode']) && $request['modbusMode'] != ''))
			{
				$modbusMode = $request['modbusMode'];
			}

			if($modbusMode == 'tcp')
			{
				$modbus_results['modbusMode'] = $p_dbconfig->setDbconfig('modbus', 'protocol', $modbusMode);
				$modbustcpip = '';		//IP
				if( (isset($request['mipoct1']) && $request['mipoct1'] != '') &&
					(isset($request['mipoct2']) && $request['mipoct2'] != '') &&
					(isset($request['mipoct3']) && $request['mipoct3'] != '') &&
					(isset($request['mipoct4']) && $request['mipoct4'] != ''))
				{
					$modbustcpip = $request['mipoct1'].'.'.$request['mipoct2'].'.'.$request['mipoct3'].'.'.$request['mipoct4'];
				}

				// Save IP
				$modbus_results['mipoct1'] = (isValidIP($modbustcpip) ? $p_dbconfig->setDbconfig('modbus', 'ipovertcp', $modbustcpip) : false);

				$modbustcpport = '';		//port
				if( (isset($request['mportdata']) && $request['mportdata'] != ''))
				{
					$modbustcpport = $request['mportdata'];
				}

				// Save Port
				$modbus_results['mportdta'] = (isValidNumber($modbustcpport) ? $p_dbconfig->setDbconfig('modbus', 'portovertcp', $modbustcpport) : false);
			}
			elseif ($modbusMode == 'rtu')
			{
				$modbus_results['modbusMode'] = $p_dbconfig->setDbconfig('modbus', 'protocol', $modbusMode);
				$modbus_results['baudrate'] = ((isValidNumber($request['baudrate'])&& (($request['baudrate'] >= 1200 &&	$request['baudrate'] <= 115200)))?
					($p_dbconfig->setDbconfig($app, 'baudrate', $request['baudrate'])):false);
				$modbus_results['data_bits'] = ((($request['data_bits'] == '5') ||
					($request['data_bits'] == '7') ||
					($request['data_bits'] == '8') ||
					($request['data_bits'] == '9')) ? ($p_dbconfig->setDbconfig($app, 'data_bits', $request['data_bits'])): false);
				$modbus_results['parity'] = ((($request['parity'] == 'E') || ($request['parity'] == 'O') || ($request['parity'] == 'N'))?($p_dbconfig->setDbconfig($app, 'parity', $request['parity'])):false);
				$modbus_results['stop_bits'] = ((($request['stop_bits'] == '0') ||
					($request['stop_bits'] == '1') ||
					($request['stop_bits'] == '2')) ? ($p_dbconfig->setDbconfig($app, 'stop_bits', $request['stop_bits'])):false);
			}
			else
			{
				$modbus_results['modbusMode'] = false;
			}

			$failed_results = array_keys($modbus_results, false, true);
			$result['fields'] = implode(',', $failed_results);
			foreach($failed_results as $field)
			{
				$result['getParams'] .= '&'.$field.'='.urlencode($request[$field]);
			}

			if(empty($result['fields']))
			{
				$settingsSaved = true;
			}
		}
	}
	if($settingsSaved)
	{
		$result['success'] = 'true';
		$result['codes'][] = _SUCCESS_SAVE_SETTINGS_CODE;
		$result['codes'][] = _REBOOT_MESSAGE_CODE;
	}
	else
	{
		$result['success'] = 'false';
		$result['codes'][] = _FAIL_SAVE_FIELDS_CODE;
	}

	return $result;
}

/**
 * addAssignemnt
 * Saves the assignment of template to slave into dbconfig using the dbconfig wrapper
 * @param object $p_dbconfig - wrapper class for dbconfig interaction
 * @param array $request - form REQUEST array
 * @author Tyson Pullukatt (Tyson.Pullukatt@gps1.com)
 */
function addAssignment($p_dbconfig, $request)
{
	$result = array("success" => 'false', "module" => "Modbus", "codes" => array(), "fields" => null, "getParams" => null);	//array for capturing result status, status code, and field names.
	$assignmentSaved = false;

	if(!empty($request['template_name']) && !empty($request['slave_name']))
	{
		$templateId = $request['template_name'];
		$slaveId = 'slave'.$request['slave_name'];
		$app = 'modbus-db';
		$assignmentSaved = $p_dbconfig->setDbconfig($app, $slaveId, $templateId);
	}

	if($assignmentSaved)
	{
		$result['success'] = 'true';
		$result['codes'][] = _SUCCESS_SAVE_SETTINGS_CODE;
		$result['codes'][] = _REBOOT_MESSAGE_CODE;
	}
	else
	{
		$result['success'] = 'false';
		$result['codes'][] = _FAIL_SAVE_SETTINGS_CODE;
	}
	return $result;
}

/**
 * addTemplate
 * Saves the template to dbconfig using the dbconfig wrapper
 * @param object $p_dbconfig - wrapper class for dbconfig interaction
 * @param array $request - form REQUEST array
 * @param array $file - form FILE array
 * @author Tyson Pullukatt (Tyson.Pullukatt@gps1.com)
 */
function addTemplate($p_dbconfig, $request, $file)
{
	debug('+++++_FILE++++++++++++++', $file);
	$result = array("success" => 'false', "module" => "Modbus", "codes" => array(), "fields" => null, "getParams" => null);	//array for capturing result status, status code, and field names.
	$templateSaved = false;
	if(!empty($file["templateFile"]))
	{
		$templateFile = $file["templateFile"];
		if($templateFile['error'] == 0)
		{
			debug("templateFile location=",$templateFile["tmp_name"]);
			debug("templateName=", $request["templateName"]);
			if($templateFile["size"] <= _MAX_TEMPLATE_FILE_SIZE)
			{
				if(!empty($templateFile["tmp_name"]) && !empty($request["templateName"]))
				{
					$templateName = $request["templateName"];
					if(isValidTemplateName($templateName))
					{
						$app = "modbus-db";
						$key = "template_".$templateName;
						$templateSaved = $p_dbconfig->setDbconfigDataFile($app, $key, $templateFile["tmp_name"]);
					}
					else
					{
						$result['fields'] = 'templateName';
					}
				}
			}
			else
			{
				$result['codes'][] = _TEMPLATE_FILE_LARGE_CODE;
				$result['fields'] = 'templateFile';
			}
		}
	}

	if($templateSaved)
	{
		$result['success'] = 'true';
		$result['codes'][] = _SUCCESS_SAVE_SETTINGS_CODE;
		$result['codes'][] = _REBOOT_MESSAGE_CODE;
	}
	else
	{
		$result['success'] = 'false';
		$result['codes'][] = _FAIL_SAVE_SETTINGS_CODE;
		$result['getParams'] = '&templateName='.urlencode($request['templateName']);
	}
	return $result;
}

/**
 * deleteTemplate
 * Delete the template from dbconfig using the dbconfig wrapper
 * @param object $p_dbconfig - wrapper class for dbconfig interaction
 * @param array $request - form REQUEST array
 * @author Tyson Pullukatt (Tyson.Pullukatt@gps1.com)
 */
function deleteTemplate($p_dbconfig, $request)
{
	$result = array("success" => 'false', "module" => "Modbus", "codes" => array(), "fields" => null, "getParams" => null);	//array for capturing result status, status code, and field names.

	$template_deleted = false;
	$assignments_deleted = true;
	$templateId = $templateKey = '';
	$templateAssignments = array();
	$app = "modbus-db";
	if(!empty($request['templateId']))
	{
		$templateId =  $request['templateId'];
		$templateKey = "template_".$templateId;
		if(!empty($request['assignments']))
		{
			$slaveArr = split(',', $request['assignments']);
			foreach ($slaveArr as $slave)
			{
				if ($slave != '')
				{
					$key = "slave". $slave;
					$value = $p_dbconfig->getDbconfig($app, $key);
					if($key != '')
					{
						$templateAssignments[$key] = $value;
						$assignments_deleted &= $p_dbconfig->unsetDbconfigData($app, $key);
					}
				}
			}
		}
		if($assignments_deleted)
		{
			$template_deleted = $p_dbconfig->unsetDbconfigData($app, $templateKey);
		}
	}

	if($assignments_deleted && $template_deleted)
	{
		$result['success'] = 'true';
		$result['codes'][] = _SUCCESS_SAVE_SETTINGS_CODE;
		$result['codes'][] = _REBOOT_MESSAGE_CODE;
	}
	else
	{
		$result['success'] = 'false';
		$result['codes'][] = _FAIL_SAVE_SETTINGS_CODE;

		//rollback
		if(!empty($templateAssignments))
		{
			foreach($templateAssignments as $slave=>$template)
			{
				$p_dbconfig->setDbconfigData($app, $slave, $template);
			}
		}
	}

	debug('(modbus_controller.php|deleteTemplate()) $result: ', $result);
	return $result;

}

function deleteAssignment($p_dbconfig, $request)
{
	$result = array("success" => 'false', "module" => "Modbus", "codes" => array(), "fields" => null, "getParams" => null);	//array for capturing result status, status code, and field names.

	$assignment_deleted = false;

	if(!empty($request['slaveId']))
	{
		$app = 'modbus-db';
		$key = $request['slaveId'];
		$assignment_deleted = $p_dbconfig->unsetDbconfigData($app, $key);
	}

	if($assignment_deleted)
	{
		$result['success'] = 'true';
		$result['codes'][] = _SUCCESS_SAVE_SETTINGS_CODE;
		$result['codes'][] = _REBOOT_MESSAGE_CODE;
	}
	else
	{
		$result['success'] = 'false';
		$result['codes'][] = _FAIL_SAVE_SETTINGS_CODE;
	}

	return $result;
}

?>
?>
