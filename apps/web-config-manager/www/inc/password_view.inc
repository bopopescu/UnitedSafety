<?php
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';			//contains functions for socket interaction, error message display, and logging.
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/db_sqlite3.inc';	//contains functions for db interaction
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/session_controller.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/password_controller.inc';


	$passwd = new passwordController();
	$resetSession = false;
	
	if(!empty($_GET))
	{
		if(!empty($_GET['codes']))
		{
			foreach(explode(",",$_GET['codes']) as $key)
			{
				if($key == "53"){ $resetSession = true; }
					
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
	
	//get accounts that the user has access to change
	$users = $passwd->getAllowedUsers($_SESSION['M2M_SESH_USERAL']);

	$userHtml = "<option id='current' value='".$_SESSION['M2M_SESH_USERID']."'>".$_SESSION['M2M_SESH_USERNAME']." (currently logged in)</option>";

	if(!empty($users))
	{
		foreach($users as $user)
		{
			$userHtml .= "<option value='".$user['u_id']."'>".$user['username']."</option>";	
			
		}
	}
	
	if($resetSession)
	{
		unset($_SESSION['M2M_SESH_USERID']);
		unset($_SESSION['M2M_SESH_USERNAME']);
		unset($_SESSION['M2M_SESH_USERAL']);
	}

?>


