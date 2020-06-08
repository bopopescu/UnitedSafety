<?php require_once $_SERVER['DOCUMENT_ROOT'].'inc/session_controller.inc'; ?>
<!DOCTYPE HTML>

<html>
<head>
<meta charset="UTF-8">
<meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
<title>Change Password - <?php echo DEVICE_NAME; ?></title>

<?php include $_SERVER['DOCUMENT_ROOT'].'mainscriptsgroup.php'; ?>
		<script type="text/javascript" src="/js/support.js"></script>
</head>

<body>
	<div class="container">

		<?php include $_SERVER['DOCUMENT_ROOT'].'header.php'; ?>


		<div class="clear"></div>
		<div class="clear"></div>

		<?php include $_SERVER['DOCUMENT_ROOT'].'tabs.php'; ?>


		<div class="contentblock">
			<!-- Support tab -->
			<h2>Support</h2>

			<div>
				<div class="hr" style="padding-left:25px;padding-right:25px;">
					<hr />
				</div>
				<p style="font-family: Arial; font-size: 14px; color: #555555;padding-left:25px;">
					<span style="color:#002539;font-weight:bold;">Toll Free:</span>
						1-877-367-4638 &nbsp;&nbsp;
					<span style="color:#143d8d;">|</span>
					<span style="color:#002539;font-weight:bold;">E-Mail:</span>
						inetadmin@indsci.com &nbsp;&nbsp;
					&nbsp;&nbsp;
				</p>

				<?php include '../supporttabs.php'; ?>

				<div class="contentblock2">

				<div class="msgBox"></div>
				<?php require_once $_SERVER['DOCUMENT_ROOT'].'inc/password_view.inc'; ?>

				<div class="inversetab">Change Password</div>
				<!-- <a href="/TL3000_HTML5/Default_CSH.htm#GETTINGSTARTED" TARGET="_blank"><img src="/images/help.png" alt="help" border="0" ></a>--> 
				<div class="hr"><hr /></div>

				<div id="changepwd">

				<form method="post" id="chgPassword" action="/inc/password_processor.php">

					<div class="row">
						<span class="label">Enter <strong><?php echo (!empty($_SESSION['M2M_SESH_USERNAME']) ? $_SESSION['M2M_SESH_USERNAME'] : 'your');  ?></strong> password</span>
						<span class="formw" id="extpwdID">
							<input type="password" size="35" name="authPwd" /><!-- extpwdID -->
							<br />
							<span class="errorMsg" name="authPwdError"></span>
							<input type="hidden" name="csrfToken" value="<?php echo (!empty($_SESSION['csrfToken']) ? $_SESSION['csrfToken'] : 'noData'); ?>" />
						</span>
					</div>

					<div class="row">
						<span class="label">Change password for account</span>
						<span class="formw">
							<select name="loginProfile" style="width: 200px;"> <!-- loginProfile -->
								<option value="">Select an account...</option>
								<?php echo $userHtml; ?>
							</select>
						</span>
					</div>

					<div class="row">
						<span class="label">Enter new password</span>
						<span class="formw pwd" id="chgpwdID"> <!-- chgpwdID -->
							<input type="password" size="35" name="newPwd" />
							<br />
							<span class="errorMsg" name="newPwdError"></span>
						</span>
					</div>

					<div class="row">
						<span class="label">Re-enter new password</span>
						<span class="formw pwd" id="rchgpwdID">
							<input type="password" size="35" name="confirmNewPwd" /> <!-- rchgPwd -->
							<br />
							<span class="errorMsg" name="confirmNewPwdError"></span>
						</span>
					</div>
					<div class="spacer">&nbsp;</div>
					<div class="hr"><hr /></div>
					<div class="row">
					<span class="formw">
						<button type="submit" class="button2-link">Save</button>&nbsp;
						<button type="reset" class="button3-link">Cancel</button>&nbsp;
					</span>
					</div>
				</form>
				</div>
			</div>
			</div>
		</div>
	</div>
</body>
</html>
