<?php 

require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';			//contains functions for socket interaction, error message display, and logging.

debug('(reboot.php $_POST: ', $_POST);	//DEBUG

session_start();

if(!empty($_POST) && $_POST['op'] == 'reboot' && !empty($_SESSION['M2M_SESH_USERID']) && !empty($_SESSION['M2M_SESH_USERAL']) )
{
	session_destroy();
	unset($_SESSION['M2M_SESH_USERID']);
	$reboot_result = reboot();		
	debug('reboot.php: Reboot function returned >> ', $reboot_result);
}
else
{
	echo "fail";
}


?>