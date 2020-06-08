<?php
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';			//contains functions for socket interaction, error message display, and logging.
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/db_sqlite3.inc';	//contains functions for db interaction

class passwordController
{
	function getAllowedUsers($userLevel)
	{
		if(!empty($userLevel))
		{
			$dt = new db_sqlite3();
			
			$conn = $dt->connect_sqlite(DB);		
			$sql = 'select * from user_profile where access_level > :userlevel';	//sql prepared statement
			$sqldata = array(':userlevel' => $userLevel);							//prepared statement data
			
			$sqlResult = $dt->query_sqlite($conn, $sql, $sqldata);	//execute query
			debug('[password_controller.inc][getAllowedUsers] db result', print_r($sqlResult, true));		//DEBUG
			
			
			if(count($sqlResult) > 0){ return $sqlResult; }
			
			$dt->close_sqlite($conn);	//close db connection
		}
		return null;
	}
	
	function changeUserPassword($userId, $newPass)
	{
		if(!empty($userId) && !empty($newPass))
		{
			$dt = new db_sqlite3();
			
			$conn = $dt->connect_sqlite(DB);
			
			$sql = "update user_profile set password = :newpass, last_update = :timestamp where u_id = :uid";	//sql prepared statement

			$sqldata = array(':uid' => $userId, ':newpass' => md5($newPass), ':timestamp' => time());							//prepared statement data			
			$sqlResult = $dt->exec_sqlite($conn, $sql, $sqldata);	//execute query
			debug('[password_controller.inc][changeUserPassword] db result', print_r($sqlResult, true));		//DEBUG
			
			if($sqlResult == 1){ return true; }
			
			$dt->close_sqlite($conn);	//close db connection
		}
		return false;
		
	}

	function verifyUserCreds($userId, $pass)
	{
		$userVerified = false;
		
		if(!empty($userId) && !empty($pass))
		{
			$dt = new db_sqlite3();
			$conn = $dt->connect_sqlite(DB);
			$sql = 'select * from user_profile where u_id = :userId and password = :pass';	//sql prepared statement
			$sqldata = array(':userId' => $userId, ':pass' => md5($pass));							//prepared statement data
			
			$sqlResult = $dt->query_sqlite($conn, $sql, $sqldata);	//execute query
			debug('[password_controller.inc][verifyUserCreds] db result', print_r($sqlResult, true));		//DEBUG
			
			
			if(count($sqlResult) == 1){ $userVerified = true; }
			
			$dt->close_sqlite($conn);	//close db connection
		}
		return $userVerified;
		
	}
}

	

	
	
