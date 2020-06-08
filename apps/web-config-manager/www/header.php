<?php if(empty($_SESSION['M2M_SESH_USERNAME'])) { exit;}  ?>
<!-- header content -->

<?php require_once $_SERVER['DOCUMENT_ROOT'] . 'inc/display_controller.inc'; ?>
<div class="item-1">
	<img class="logo" alt="ISC_TGX" src="/images/TGXLogo-Approved.png" />
</div>

<div class="item-5">
	<a id="logout" href="/logout.php"><img title="Logout" alt="Logout" src="/images/logouticon.png" width="35px" height="35px" border="0" /><br /><span style="margin-left: 15px;">Logout</span></a>
</div>

<!-- active reboot icon -->
<div class="item-2">
	<a id="reboot" href="/reboot.php"><img title="Reboot" alt="Reboot" src="/images/rebooticon.png" width="35px" height="35px" border="0" /><br />
		<span>Reboot</span></a>
</div>

<!--active -->
<!--
		<div class="item-2">
			<img title="System Alerts" alt="System Alerts" src="/images/alerticon.png" width="50px" height="50px" border="0" />
			<br />
			Alerts
		</div>
		-->
<!-- disabled alert icon -->
<!--
		<div class="item-3">
			<img title="System Alerts" alt="System Alerts" src="/images/alerticon_disabled.png" width="35px" height="35px" border="0"/>
			<br />
			<span style="color:#666666;">Alerts</span>
		</div>
		-->

<!--active -->
<!-- <div class="item-3">
			<img title="System Error Log" alt="System Error Log" src="/images/systemlogicon.png" width="50px" height="50px" border="0"/>
			<br />
			System Log
		</div>-->

<!-- disabled system log icon -->
<!--
		<div class="item-4">
			<img title="System Error Log" alt="System Error Log" src="/images/systemlogicon_disabled.png" width="35px" height="35px" border="0"/>
			<br />
			<span style="color:#666666;">System Log</span>
		</div>
		-->





<div class="item-6">
	<!--
			<p><?php echo "<span id='clock'>" . $current_date_time . "</span>"; ?>
			<br />
			<span id="serial">Serial #: <strong><?php echo $serialNumber; ?></strong></span><span id="firmware"> - Firmware Version #: <strong><?php echo $firmwareVersion; ?></strong></span>
			<br />
			<span class="login">Users: <strong><?php echo (!empty($_SESSION['M2M_SESH_USERNAME']) ? $_SESSION['M2M_SESH_USERNAME'] : '');  ?></strong></span>
			</p>-->

	<?php
	$microVer = GetMicroVersion();
	$hwVer = GetHWVersion();
	?>
	<div style="float:left;">
		<span id="serial">Serial #: <strong><?php echo $serialNumber; ?></strong></span><span id="firmware"> &nbsp &nbsp &nbsp &nbsp Firmware: <strong><?php echo $firmwareVersion; ?></strong> Micro: <strong> <?php echo $microVer; ?></strong></span>
		<br />
		<span class="login">User: <strong><?php echo (!empty($_SESSION['M2M_SESH_USERNAME']) ? $_SESSION['M2M_SESH_USERNAME'] : '');  ?></strong></span> &nbsp &nbsp &nbsp Hardware: <strong><?php echo $hwVer; ?></strong>
	</div>

	<div style="float:right; margin-right: 8px;"><?php echo "<span id='clock'>" . $current_date_time . "</span>"; ?></div>



	<script type="text/javascript">
		$("#clock").clock({
			"format": "24",
			"timezone": "utc",
			"timestamp": <?php echo $timestamp * 1000; ?>
		});
	</script>
	<?php
	$batteryVoltage = GetBatteryVoltage();
	?>
	<br />
	<div style="float:right; margin-right: 8px;">Input Voltage: <strong><?php echo $batteryVoltage;  ?></strong></div>
</div>