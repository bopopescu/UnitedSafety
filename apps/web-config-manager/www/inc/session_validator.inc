<?php
require_once $_SERVER['DOCUMENT_ROOT'].'inc/util.inc';			//contains functions for socket interaction, error message display, and logging.

//Start session
session_start();

//if Session info does not exist for the user
if(empty($_SESSION['M2M_SESH_USERID']) && empty($_SESSION['M2M_SESH_USERAL']))
{
	$authenticated = false;
	debug("(session_validator.inc) Session info does not exist for the user. Prompting login screen...");
	if($_REQUEST['ref'] == 'login')
	{
		debug('_REQUEST', $_REQUEST);	//DEBUG
		if(!empty($_REQUEST['user']) && !empty($_REQUEST['pass']))
		{
			//validate the user's credentials
			$dt = new db_sqlite3();
			$conn = $dt->connect_sqlite(DB);		//connect to db
			$sql_login = 'select * from user_profile where username = :user and password = :passwd';	//sql prepared statement
		     $creds = array(':user' => $_REQUEST['user'], ':passwd' => md5($_REQUEST['pass']));				   //prepared statement data

			$result_login = $dt->query_sqlite($conn, $sql_login, $creds);	//execute query
			debug('(login_controller.inc) User-entered credentials lookup db result',$result_login);		//DEBUG

			//if valid credentials, redirect users to url passed in http referrer or to index page
			if(count($result_login) > 0)
			{
				session_regenerate_id();     //Regenerate session ID to prevent session fixation attacks
                                $_SESSION['csrfToken'] = md5(uniqid(rand(), true));
				$_SESSION['M2M_SESH_USERID'] = $result_login[0]['u_id'];			//set user id in Session
				$_SESSION['M2M_SESH_USERNAME'] = $result_login[0]['username'];		//set user name in Session
				$_SESSION['M2M_SESH_USERAL'] = $result_login[0]['access_level'];	//set user access level in Session

				//Write session to disc
				session_write_close();		//Write session
				setcookie('login', $_REQUEST['user']);
				$authenticated = true;
			}
			//if invalid credentials, display error
			else
			{
				$html = '<div class="msgBoxLogin">';
				$html .= '<div class="failImage"></div>';
				$html .= '<div class="failMsg">You have entered an incorrect username or password.</div>';
				$html .= '</div>';

				$jscript = '<script type="text/javascript">';
				$jscript .= '$(document).ready(function(){';
				$jscript .= '$(".loginbox2 form#login").before(\''.$html.'\');';
				$jscript .= '});';
				$jscript .= '</script>';
			}

			$dt->close_sqlite($conn);	//close db connection
		}
	}
	if(!$authenticated)
	{
		if(!isset($_COOKIE['login']))
		{
			$queryString = "";

			if(!empty($_SERVER['QUERY_STRING']))
			{
				$queryString = "?".urlencode($_SERVER['QUERY_STRING']);
			}

			header("location:http://".$_SERVER['HTTP_HOST']."/login.php?ref="."http://".$_SERVER['HTTP_HOST'].$_SERVER['PHP_SELF'].$queryString);
			exit;
		}
?>
<html>
<head>
	<meta charset="UTF-8">
	<meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
	<title>Session Expired - <?php echo DEVICE_NAME; ?></title>

<?php
include $_SERVER['DOCUMENT_ROOT'] . 'mainscriptsgroup.php';
?>
	<script type='text/javascript' src='/js/login.js'></script>
	<?php
	echo $jscript;
	?>
</head>
	<body>
		<div class="loginbox2" >
			<h3 style="text-align: center">Session Expired.</h3>
			<div style="text-align: center">
				<span style="font: 14px Verdana;" >
					Please login to continue submission of settings.
					<br/>
					Any other action may cause loss of any current changes.
				</span>
			</div>
			<div class="spacer">&nbsp;</div>
			<div>
				<form method="post" id="login" action="<?php print $_SERVER['PHP_SELF']; ?>">
					<div class="rowl">
						<span class="label">Username</span>
						<span class="formwl">
							<input type="text" size="26" name="user" readonly="readonly" value="<?php print $_COOKIE['login']; ?>">
							<br/>
							<span class="errorMsg" name="errorLoginUser" style="display:none;"></span>
						</span>
					</div>
					<div class="rowl">
						<span class="label">Password</span>
						<span class="formwl">
							<input type="password" size="26" name="pass">
							<br/>
							<span class="errorMsg" name="errorLoginPwd" style="display:none;"></span>
						</span>
					</div>
					 <input type="hidden" name="ref" value="login" />
					<div class="spacer">&nbsp;</div>
					<div class="rowl">
						<span class="formwl">
							<button type="submit" class="button2-link">Login</button>&nbsp;
							<button type="reset" class="button3-link">Cancel</button>&nbsp;
						</span>
					</div>
					<?php
					foreach($_REQUEST as $key => $value)
					{
						if(($key != 'ref')&&($key != 'user')&&($key != 'pass'))
						{
							$value = (!empty($value) ? htmlspecialchars($value, ENT_QUOTES, 'UTF-8'):'');
							echo "<input type='hidden' name='$key' value='$value' />";
						}
					}
					?>
				</form>
			</div>
		</div>
	</body>
</html>
<?php
		exit;
	}
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

