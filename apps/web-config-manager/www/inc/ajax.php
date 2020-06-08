<?php 

require_once $_SERVER['DOCUMENT_ROOT'].'/inc/network_controller.inc';	
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';				//contains functions for socket interaction, error message display, and logging.

//OBJECT INSTANTIATION
$nt_ctrl = new networkcontroller();


//if an ajax script is calling this script, then send back (print out) the gps xml
if(!empty($_GET))
{
	switch($_GET['op'])
	{
		case 'getip':
			if(!empty($_GET['data']))
				$ip = $nt_ctrl->getIpAddress($_GET['data']);
			else
				$ip = null;
			
			echo $ip;
			break;
			
		default:
			break;
	}
}




?>
