<?php require_once $_SERVER['DOCUMENT_ROOT'].'inc/session_controller.inc'; ?>
<!DOCTYPE HTML>

<html>
	<head>
		<meta charset="UTF-8">
		<meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
		<title>Cellular - <?php echo DEVICE_NAME; ?></title>

		<?php include $_SERVER['DOCUMENT_ROOT'].'mainscriptsgroup.php'; ?>
		<script type="text/javascript" src="/js/cellular.js"></script>

	</head>

<body>
	<div class="container">
		<?php include $_SERVER['DOCUMENT_ROOT'].'header.php'; ?>

		<div class="clear"></div>
		<div class="clear"></div>

		<?php include $_SERVER['DOCUMENT_ROOT'].'tabs.php'; ?>


		<div class="contentblock">

			<!-- Network Config tab -->
			<h2>Network Configuration</h2>

			<?php include '../networktabs.php'; ?>

				<div class="contentblock2">

			<!-- Cellular subtab -->
				<div class="msgBox"></div>
				<?php require_once $_SERVER['DOCUMENT_ROOT'].'inc/cell_view.inc'; ?>

					<!-- Header -->
					<div class="inversetab">Cellular</div>
					<!---  <a href="/TL3000_HTML5/Default_CSH.htm#CELLULAR" TARGET="_blank"><img src="/images/help.png" alt="help" border="0" ></a> -->
					<div class="hr"><hr /></div>

					<form class="formCheck" id="cellular" method="post" action="/inc/cell_processor.php">
						<input type="hidden" name="csrfToken" value="<?php echo (!empty($_SESSION['csrfToken']) ? $_SESSION['csrfToken'] : 'noData'); ?>" />
						<input type="hidden" name="network" value="cellular" />
						<div class="row">
							<span class="label">Cellular IMEI</span>
							<span class="formw">
								<label for="cellIMEI"><?php echo $cell_imei; ?></label>
							</span>
						</div>
						<div class="row">
							<span class="label">Cellular RSSI</span>
							<span class="formw">
								<input type="text" size="30" readonly="readonly" id="XcellRSSI" value="<?php echo $cell_rssi; ?>"/>
							</span>
						</div>
						<div class="row">
							<span class="label">SIM #</span>
							<span class="formw">
								<label for="simNum"><?php echo $cell_sim; ?></label>
							</span>
						</div>
						<div class="row">
							<span class="label">Phone #</span>
							<span class="formw">
								<label for="phoneNum"><?php echo $cell_phone; ?></label>
							</span>
						</div>
						<div class="row">
							<span class="label">Carrier</span>
							<span class="formw">
								<label for="carrier"><?php echo $cell_carrier; ?></label>
							</span>
						</div>

						<div class="row">
							<span class="label">APN</span>
							<span class="formw reg">
								<input type="text" size="26" name="apn" value="<?php echo $cell_apn; ?>" />
								<span class="errorMsg"></span>
							</span>
						</div>
						<div class="row">
							<span class="label">User Name (optional)</span>
							<span class="formw">
								<input type="text" size="26" name="sim_usr" value="<?php echo $cell_user; ?>" />
								<span class="errorMsg"></span>
							</span>
						</div>
						<div class="row">
							<span class="label">Password (optional)</span>
							<span class="formw">
								<input type="text" size="26" name="sim_pwd" value="<?php echo $cell_pwd; ?>" />
								<span class="errorMsg"></span>
							</span>
						</div>

						<div class="row">
							<span class="label">WAN IP Address</span>
							<span class="formw">
								<label for="wanIP"><?php echo $cell_network_ip; ?></label>
							</span>
						</div>
						<div class="row">
							<span class="label">WAN Subnet Mask</span>
							<span class="formw">
								<label for="wanSubnet"><?php echo $cell_network_mask; ?></label>
							</span>
						</div>
						<div class="row">
							<span class="label">WAN DNS (Primary)</span>
							<span class="formw">
								<label for="wanDns1"><?php echo $cell_dns_pri; ?></label>
							</span>
						</div>
						<div class="row">
							<span class="label">WAN DNS (Secondary)</span>
							<span class="formw">
								<label for="wanDns2"><?php echo $cell_dns_sec; ?></label>
							</span>
						</div>
						<div class="spacer">&nbsp;</div>
						<?php
						if(hasSubmitAccess())
						{
						?>
						<div class="hr"><hr /></div>
						<div class="row">
							<span class="formw">
								<button type="submit" class="button2-link">Save</button>&nbsp;
								<button type="reset" class="button3-link">Cancel</button>&nbsp;
								<!--<button class="button4-link">Sync</button>-->
							</span>
						</div>
						<?php
						}
						?>
					</form>
				</div>

		</div> <!-- end of content block (main tab container) -->
	</div><!-- end of entire div container -->
</body>
</html>
