<?php require_once $_SERVER['DOCUMENT_ROOT'].'/inc/session_controller.inc'; ?>
<!DOCTYPE HTML>

<html>
	<head>
		<meta charset="UTF-8">
		<meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
		<title><?php echo DEVICE_NAME; ?></title>

		<?php include $_SERVER['DOCUMENT_ROOT'].'mainscriptsgroup.php'; ?>
	</head>

<body>
	<div class="container">
		<?php include $_SERVER['DOCUMENT_ROOT'].'header.php'; ?>

		<div class="clear"></div>
		<div class="clear"></div>

		<?php include $_SERVER['DOCUMENT_ROOT'].'tabs.php'; ?>
		<?php require_once $_SERVER['DOCUMENT_ROOT'].'inc/status_view.php'; ?>

		<div class="contentblock">

			<!-- Device Configuration tab -->
			<h2>Device Configuration</h2>
			<div class="msgBox"></div>
			
			<?php include '../devicetabs.php'; ?>
			

				<div class="contentblock2">
				<div id="statusgeneral">
						<!-- General subtab -->
					<div class="inversetab">Status</div>
					<!--- <a href="/TL3000_HTML5/Default_CSH.htm#STATUS" TARGET="_blank"><img src="/images/help.png" alt="help" border="0" ></a> --->
					<div class="hr"><hr /></div>
						<!-- <form> -->
							<div class="row">
								<span class="label">Serial number</span>
								<span class="formw"> 
								<label for="serial"><?php echo (!empty($device['serial']) ? $device['serial'] : ''); ?></label>

								</span>
							</div>
							<?php if(isAdmin()){ ?>
							<div class="row">
								<span class="label">Model</span>
								<span class="formw"> 
								<label for="model"><?php echo (!empty($device['model']) ? $device['model'] : ''); ?></label>

								</span>
							</div>
							<?php } ?>
							<div class="row">
								<span class="label">Current Firmware Version</span>
								<span class="formw"> 
									<label for="version"><?php echo (!empty($device['firmware_ver']) ? $device['firmware_ver'] : ''); ?> &nbsp;
									<!--<button class="button-link">Update</button></label>-->

								</span>
							</div>
							<div class="row">
								<span class="label">Firmware Release Date</span>
								<span class="formw"> 
									<label class="label2" for="lastDate"><?php echo (!empty($device['last_firmware_update']) ? $device['last_firmware_update'] : ''); ?></label>
								</span>
							</div>
							<br/>
							<br/>
							</div>
							
							
							<!-- Wakeup related data section -->
							<div class="inversetab">Wakeups</div>
							<div class="hr"><hr /></div>

							<div class="row">
								<span class="label">Most Recent Wakeup</span>
								<span class="formw"> 
								<label class="label2">
									<?php
										if (substr($wakeup_reason, 0, 5) === 'power')
											echo '<b>Power</b> at ';
										else if (substr($wakeup_reason,0 ,4) === 'inp1')
											echo '<b>Ignition</b> at';
										else
											echo "<b>$wakeup_reason:</b> at";
										echo "$wakeup_time";
									?>
									</label>
								</span>
							</div>
							<br/>
							<br/>

							<div class="spacer">&nbsp;</div>
						<!-- </form> -->
					</div>
				</div>
	</div><!-- end of entire div container -->
</body>
</html>
