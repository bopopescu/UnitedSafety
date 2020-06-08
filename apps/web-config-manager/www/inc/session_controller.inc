<?php
require_once $_SERVER['DOCUMENT_ROOT'].'inc/util.inc';			//contains functions for socket interaction, error message display, and logging.

//Start session
session_start();

//if Session info does not exist for the user
if(empty($_SESSION['M2M_SESH_USERID']) && empty($_SESSION['M2M_SESH_USERAL']))
{
	debug("(session_controller.inc) Session info does not exist for the user. Redirecting to login page...");
	//redirect user to the login page
	//header("location:http://".$_SERVER['HTTP_HOST']."/login.php?ref="."http://".$_SERVER['HTTP_HOST'].$_SERVER['REQUEST_URI']);

	$queryString = "";

	if(!empty($_SERVER['QUERY_STRING']))
	{
		$queryString = "?".urlencode($_SERVER['QUERY_STRING']);
	}

	header("location:http://".$_SERVER['HTTP_HOST']."/login.php?ref="."http://".$_SERVER['HTTP_HOST'].$_SERVER['PHP_SELF'].$queryString);

	exit;
}
//if Session info does exist for the user
else
{
	debug("(session_controller.inc) User session info validated");
	debug("(session_controller.inc) Session info", $_SESSION['M2M_SESH_USERID']);
	debug("(session_controller.inc) Session info", $_SESSION['M2M_SESH_USERNAME']);
	debug("(session_controller.inc) Session info", $_SESSION['M2M_SESH_USERAL']);
}

?>

