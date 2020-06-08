<?php
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';			//contains functions for socket interaction, error message display, and logging.

session_start();	//start session

//if Session info exists for the user == user is currently logged in
if(!empty($_SESSION['M2M_SESH_USERID']))
{
	//Log user out by killing their session
	unset($_SESSION['M2M_SESH_USERID']);
	unset($_SESSION['M2M_SESH_USERNAME']);
	unset($_SESSION['M2M_SESH_USERAL']);
	
	debug("(logout_controller.inc) Killed Session info. Redirecting to login page...");
	
	header("location:http://".$_SERVER['HTTP_HOST']."/login.php");	//redirect to login page
}
//if Session info does not exist for the user == user is not logged in
else
{
	debug("(logout_controller.inc) Session info does not exist for user. Redirecting to login page...");
	header("location:http://".$_SERVER['HTTP_HOST']."/login.php");	//redirect to login page
}

?>
