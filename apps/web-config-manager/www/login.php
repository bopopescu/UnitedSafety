<!DOCTYPE HTML>

<html>
<head>
<meta charset="UTF-8">
<meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
<title>TGX Configuration Manager</title>

<?php include 'mainscriptsgroup.php'; ?>
	<script type='text/javascript' src='/js/login.js'></script>
</head>

<body>
	<?php require_once $_SERVER['DOCUMENT_ROOT'].'/inc/login_controller.inc'; ?>

	<div>
		<div class="loginbox">
			<div class="loginbox-top" >
				<img class="logo" title="ISC - TGX Configuration Manager" alt="ISC TGX" src="images/ISClogo.png" />
			</div>

			<div class="loginbox-mid">
				TGX Configuration Manager
			</div>

			<div class="loginbox-bot">
				<form method="post" id="login" action="/login.php">
					<div class="rowl">
						<span class="loginbox-label">Username</span>
					</div>

					<div class="row1">
						<span class="loginbox-input"><input type="text" size="26" name="user" autocorrect="off" autocapitalize="none">
							<br/>
							<span class="errorMsg" name="errorLoginUser" style="display:none;"></span>
						</span>
					</div>
	
					<div class="rowl">
						<span class="loginbox-label">Password</span>
					</div>
	
					<div class="row1">
						<span class="loginbox-input"><input type="password" size="26" name="pass">
							<br/>
							<span class="errorMsg" name="errorLoginPwd" style="display:none;"></span>
						</span>
					</div>

					<input type="hidden" name="ref" value="<?php echo (!empty($_REQUEST['ref']) ? htmlspecialchars($_REQUEST['ref'], ENT_QUOTES, 'UTF-8'):''); ?>" />
					<div class="spacer">&nbsp;</div>

					<div class="rowl">
						<span class="formwl">
							<button type="submit" class="button2-link">Log In</button>&nbsp;
							<button type="reset" class="button3-link">Cancel</button>&nbsp;
						</span>
					</div>
				</form>
			</div>
		</div>
	</div>
</body>
</html>
