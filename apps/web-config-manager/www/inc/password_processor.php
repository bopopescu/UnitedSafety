<?php
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';			//contains functions for socket interaction, error message display, and logging.
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/db_sqlite3.inc';	//contains functions for db interaction
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/session_controller.inc';
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/password_controller.inc';


	$passwd_ctrl = new passwordController();
	
	if(!empty($_REQUEST) && !empty($_REQUEST['csrfToken']) && $_REQUEST['csrfToken'] == $_SESSION['csrfToken'])
	{
		debug('[change_password.php] $_REQUEST param:', $_REQUEST); 	//DEBUG
		//print_r($_REQUEST);
		
		//check for empty fields
		if(!empty($_REQUEST['loginProfile']) && 
		($_SESSION['M2M_SESH_USERID'] == $_REQUEST['loginProfile'] || 
		$_SESSION['M2M_SESH_USERAL']  == 200)) {
			if(!empty($_REQUEST['authPwd']) && !empty($_REQUEST['newPwd'])  && !empty($_REQUEST['confirmNewPwd']))	
			{
				//check that new and re-entered passwords match
				if(strcasecmp($_REQUEST['newPwd'],$_REQUEST['confirmNewPwd']) === 0)	
				{
					//check that the authorizing account's password is correct
					if($passwd_ctrl->verifyUserCreds($_SESSION['M2M_SESH_USERID'], $_REQUEST['authPwd']))	
					{
						if($passwd_ctrl->changeUserPassword($_REQUEST['loginProfile'], $_REQUEST['newPwd']))	//change the password
						{
							//password successfully changed
							$resetSessionCode = '';
							
							//the password for the currently logged in account has been changed
							if($_REQUEST['loginProfile'] == $_SESSION['M2M_SESH_USERID'])
							{
								$resetSessionCode = ',53';
							}
							
							//the password for the user account has been changed

							if($_REQUEST['loginProfile'] == "3")
							{
								require_once $_SERVER['DOCUMENT_ROOT'].'inc/dbconfig_controller.inc';
								$dbconfig = new dbconfigController();
								$dbconfig->setDbconfigData('isc-lens', 'IsPasswordSet', 1);
							}
							else
							{
								require_once $_SERVER['DOCUMENT_ROOT'].'inc/dbconfig_controller.inc';
								$dbconfig = new dbconfigController();
							}
							header("location:https://".$_SERVER['HTTP_HOST']."/support/changepassword/index.php?success=true&module=password&codes=50".$resetSessionCode);
							
						}
						else
						{
							//failed to change password
							header("location:https://".$_SERVER['HTTP_HOST']."/support/changepassword/index.php?success=false&module=password&codes=51");
						}
					}
					else
					{
						//user's credentials are incorrect
						header("location:https://".$_SERVER['HTTP_HOST']."/support/changepassword/index.php?success=false&module=password&codes=52&fields=authPwd");
					}
				}
				else
				{
					header("location:https://".$_SERVER['HTTP_HOST']."/support/changepassword/index.php?success=false&module=password&codes=55&fields=newPwd,confirmNewPwd");
				}
			}
			else
			{
				$emptyField = '';
				
				foreach($_REQUEST as $key => $value)
				{
					if(empty($value))
					{
						
						if(!empty($emptyField))
						{
							$delim = ',';	
						}
						else
						{
							$delim = '';
						}
							
						$emptyField .= $delim.$key;
						
					}
				}
				
				header("location:https://".$_SERVER['HTTP_HOST']."/support/changepassword/index.php?success=false&module=password&codes=54&fields=".$emptyField);
			}
		} else {
			header("location:https://".$_SERVER['HTTP_HOST']."/support/changepassword/index.php?success=false&module=password&codes=51");
		}
	}
	else
	{
		header("location:https://".$_SERVER['HTTP_HOST']."/support/changepassword/index.php");
	}
?>
