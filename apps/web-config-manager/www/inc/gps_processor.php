<?php

/*
 * NOTE: As of Dec 2013 (#1158), the GPS page is supposed to be a display-only page for viewing GPS information from the specified GPS receiver.
 * As such, the code below should be treated as deprecated.
 * At the time of this writing, only Internal receivers are supported. Once external receivers are supported, this page might need to be modified and revived.
 */
require_once $_SERVER['DOCUMENT_ROOT'].'inc/session_validator.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/dbconfig_controller.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';				//contains functions for socket interaction, error message display, and logging.
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/db_sqlite3.inc';		//contains functions for db interaction

//OBJECT INSTANTIATION
$dbconfig = new dbconfigController();


//Check form submission
if(!empty($_REQUEST) && !empty($_REQUEST['csrfToken']) && $_REQUEST['csrfToken'] == $_SESSION['csrfToken'] && isset($_SESSION['M2M_SESH_USERAL']) && $_SESSION['M2M_SESH_USERAL'] == 200)
{
	$result = submitGPS($dbconfig, $_REQUEST);
	header("location:http://".$_SERVER['HTTP_HOST']."/device/gps/index.php?success=".$result['success']."&module=".$result['module']."&codes=".implode(",",$result['codes'])."&fields=".$result['fields']."&".
		$result['getParams']);
}
else
{
	header("location:http://".$_SERVER['HTTP_HOST']."/device/gps/index.php");
}

/**
 * submitGPS
 * Submits the gps form content to the gps controller for processing
 * @param object $gps_ctrl
 * @param array $request
 * @author Sean Toscano (sean@absolutetrac.com)
 */
function submitGPS($dbconfig, $request)
{
	debug ('submitGPS', $request);
	$result = array("success" => 'false', "module" => "gps", "codes" => array(), "fields" => null);	//array for capturing result status, status code, and field names.
	$reporting = '';
	$latDir = $latDeg = $latMin = $latSec = '';
	$lonDir = $lonDeg = $lonMin = $lonSec = '';
	$elevation = '';
	$result_gps = array();
	// 1) find all the keys in the $Satellite_result array that have a value of false (these are the API calls that failed)
	// 2) build a string with the keys. (The key names are the same as the html element (input/radio/select) names and will be used to highlight the fields with jquery)
	$failed_results = array_keys($result_gps, false, true);
	$result['fields'] = implode(',', $failed_results);
	debug('(gps_processor.php|submitGPS()) $result[\'fields\']: ', $result['fields']); 	//DEBUG

	if(empty($result['fields']))	//if setGpsData call was successfull
	{
		$result['success'] = 'true';
		$result['codes'][] = 800;
		$result['codes'][] = 13;
	}
	else					//if setGpsData call was unsuccessfull
	{
		foreach(array_keys($result_gps) as $field)
		{
			$result['getParams'] .= '&'.$field.'='.$request[$field];
		}
		if(!$result_gps['gpsReporting'])
		{
			$result['codes'][] = 12;
		}
		$result['success'] = 'false';

	}

	debug('(gps_processor.php|submitGPS()) $result: ', $result); 	//DEBUG
	return $result;

} //END submitGPS

/**
 * calculateChecksum
 * Generates checksum for NMEA string and returns the NMEA string with checksum on end.
 * Taken from http://svn.atsplace.int/software-group/AbsoluteTrac/RedStone/Branches/trunk/apps/FastTrac/UtilityLib/checksum.cpp add_checksum()
 */
 function calculateChecksum($str)
 {
 	$chksum = 0;
	debug("length of string", strlen($str));
	for($i =0; $i < strlen($str); $i++)
	{
		if($str[$i] == '$')
		{
			$chksum = 0;
		}
		else
		{
			$chksum ^= ord($str[$i]);
		}
	}
	debug("checksum= ", $chksum);
	$chksumStr = sprintf('*%1X%1X'."\r\n", ($chksum >> 4) & 0x0f, $chksum & 0x0f);
	return $str.$chksumStr;
 }

/**
 * genGGAString
 * Generates a NMEA GGA String based on GPS and elevation parameters
 *
 */
 function getGGAString($latDir, $latDeg, $latMin, $latSec, $lonDir, $lonDeg, $lonMin, $lonSec, $elev)
 {
 	$str = '$GPGGA,';
	$decLat = floor($latDeg)*100 + ($latDeg - floor($latDeg)) *60;
	$decLon = floor($lonDeg)*100 + ($lonDeg - floor($lonDeg)) *60;
	$str = sprintf($str."000000,%09.4f,%s,%010.4f,%s,1,0,0,%.4f,M,,M,,", $decLat, $latDir, $decLon, $lonDir, $elev);
	debug("GGA str before checksum: ", $str);
 	return calculateChecksum($str);
 }
//END genGGAString

/**
 * genRMCString
 * Generates a NMEA GGA String based on GPS and elevation parameters
 *
 */
 function getRMCString($latDir, $latDeg, $latMin, $latSec, $lonDir, $lonDeg, $lonMin, $lonSec)
 {
 	$str = '$GPRMC,';
	$decLat = floor($latDeg)*100 + ($latDeg - floor($latDeg)) *60;
	$decLon = floor($lonDeg)*100 + ($lonDeg - floor($lonDeg)) *60;
	$str = sprintf($str.'000000,A,%09.4f,%s,%010.4f,%s,0,0,010100,0,', $decLat,$latDir, $decLon, $lonDir);
	debug("RMC str before checksum: ", $str);
 	return calculateChecksum($str);
 }
//END genRMCString

?>
