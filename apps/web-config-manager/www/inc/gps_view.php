<?php

require_once $_SERVER['DOCUMENT_ROOT'].'/inc/gps_controller.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';				//contains functions for socket interaction, error message display, and logging.

//OBJECT INSTANTIATION
$gps_ctrl = new gpscontroller();
$dbconfig = new dbconfigController();

//VARIABLE INSTANTIATION
$gps = $gps_type = $gps_source = $gps_list_html = $gps_type_error = '';
$time = $satellites = $latitude = $longitude = $elevation = $heading = $hdop = $quality = $velocity = $obdspeed = '';

debug('(gps_view.php) 1'); 	//DEBUG
if(!empty($_GET))
{
	if(!empty($_GET['codes']))
	{
		foreach(explode(",",$_GET['codes']) as $key)
		{
			translateStatusCode($key, $_GET['module']);
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
debug('(gps_view.php) 2'); 	//DEBUG
if(isset($_GET['gpsReporting']) && $_GET['gpsReporting'] !== false)
{
	$gpsReport = $_GET['gpsReporting'];
}
else
{
	$gpsReport = $dbconfig->getDbconfigData('NMEA', 'Source');
}

//READ GPS
debug('(gps_view.php) 3'); 	//DEBUG
$gps = $gps_ctrl->getGpsData();
debug('(gps_view.php) 4'); 	//DEBUG
debug('(gps_view.php) $gps: ', $gps); 	//DEBUG
debug('(gps_view.php) 5'); 	//DEBUG

//Parse GPS data

if(!empty($gps))
{
	$time = $gps->time;
	$satellites =  $gps->satellites;
	$latitude   =  $gps->latitude;
	$longitude  =  $gps->longitude;
	$elevation  =  $gps->elevation;
	$heading    =  $gps->heading;
	$hdop       =  $gps->hdop;
	$quality    =  $gps->quality;
	$velocity   =  $gps->velocity;
	$obdspeed   =  $gps->obdspeed;
}

//Get internal GPS chip type
$gps_type_raw = $gps_ctrl->getGpsType();
$gps_type = !empty($gps_type_raw) ? $gps_type_raw : '';

//Get GPS source (receiver) -- to be used when the unit supports an external GPS receiver
//$gps_source_raw = $gps_ctrl->getGpsSource();
//$gps_source = !empty($gps_source_raw) ? $gps_source_raw : '';
$gps_source = 'Internal';

//if an ajax script is calling this page, then send back (print out) the gps xml
if(!empty($_GET) && $_GET['op'] == 'gpsupdate')
{
	if(!empty($gps))
	{
		$gps_xml = $gps->asXML();	//converts SimpleXmlElement object to XML
		debug('(gps_view.php) $gps_xml: ', $gps_xml); 	//DEBUG
		echo trim($gps_xml);
	}
	else
	{
		echo null;
	}
}
else
{
	$gpsGGAFix = $dbconfig->getDbconfigData('NMEA', 'FixedGGA');
	$gpsRMCFix = $dbconfig->getDbconfigData('NMEA', 'FixedRMC');
	if($gpsRMCFix != '')
	{
		$gpsRMC = DecodeRMC($gpsRMCFix);
		$latDirFix = $gpsRMC['latDir'];
		$latDegFix = $gpsRMC['latDeg'];
		$lonDirFix = $gpsGGA['lonDir'];
		$lonDegFix = $gpsRMC['lonDeg'];
	}
	if($gpsGGAFix != '')
	{
		$gpsGGA = DecodeGGA($gpsGGAFix);
		$latDirFix = $gpsGGA['latDir'];
		$latDegFix = $gpsGGA['latDeg'];
		$lonDirFix = $gpsGGA['lonDir'];
		$lonDegFix = $gpsGGA['lonDeg'];
		$elevationFix = $gpsGGA['elevation'];
	}
}

if(isset($_GET['latDir']) && $_GET['latDir'] !== false)
{
	$latDirFix = $_GET['latDir'];
}

if(isset($_GET['latDeg']) && $_GET['latDeg'] !== false)
{
	$latDegFix = $_GET['latDeg'];
}

if(isset($_GET['lonDir']) && $_GET['lonDir'] !== false)
{
	$lonDirFix = $_GET['lonDir'];
}

if(isset($_GET['lonDeg']) && $_GET['lonDeg'] !== false)
{
	$lonDegFix = $_GET['lonDeg'];
}

if(isset($_GET['elevation']) && $_GET['elevation'] !== false)
{
	$elevationFix = $_GET['elevation'];
}

function DecodeGGA($p_GGAString)
{
	$GGAArray = explode(',', $p_GGAString);
	$latDegMM = $GGAArray[2];
	$gps['latDir'] = $GGAArray[3];
	$lonDegMM = $GGAArray[4];
	$gps['lonDir'] = $GGAArray[5];
	$gps['elevation'] = $GGAArray[9];
	$gps['latDeg'] = floor($latDegMM/100) + ($latDegMM - (floor($latDegMM/100) * 100)) / 60.0 ;
	$gps['lonDeg'] = floor($lonDegMM/100) + ($lonDegMM - (floor($lonDegMM/100) * 100)) / 60.0 ;
	return $gps;
}

function DecodeRMC($p_RMCString)
{
	$RMCArray = explode(',', $p_RMCString);
	$latDegMM = $RMCArray[3];
	debug('(gps_view.php) $gps_xml: DegMM=', $latDegMM); 	//DEBUG
	$gps['latDir'] = $RMCArray[4];
	$gps['latDeg'] = floor($latDegMM/100) + ($latDegMM - (floor($latDegMM/100) * 100)) / 60.0 ;
	debug('(gps_view.php) $gps_xml: latDD', $gps['latDeg']); 	//DEBUG

	$lonDegMM = $RMCArray[5];
	$gps['lonDir'] = $RMCArray[6];
	$gps['lonDeg'] = floor($lonDegMM/100) + ($lonDegMM - (floor($lonDegMM/100) * 100)) / 60.0 ;
	debug('(gps_view.php) $gps_xml: ', $latDegMM, $gps['latDeg']); 	//DEBUG
	return $gps;
}





?>
