<?php require_once $_SERVER['DOCUMENT_ROOT'].'/inc/session_controller.inc'; ?>
<!DOCTYPE HTML>

<html>
	<head>
		<meta charset="UTF-8">
		<meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
		<title>GPS - <?php echo DEVICE_NAME; ?></title>

		<?php include $_SERVER['DOCUMENT_ROOT'].'mainscriptsgroup.php'; ?>

		<?php
			if(!empty($_GET) && $_GET['success'])
			{
				echo "<script type='text/javascript'>var g_update_gps = false;</script>";
			}
			else echo "<script type='text/javascript'>var g_update_gps = true;</script>";
		?>
		<script type='text/javascript' src='/js/gps.js'></script>
	</head>

<body>
	<div class="container">
		<?php include $_SERVER['DOCUMENT_ROOT'].'header.php'; ?>

		<div class="clear"></div>
		<div class="clear"></div>

		<?php include $_SERVER['DOCUMENT_ROOT'].'tabs.php'; ?>


		<div class="contentblock">

			<!-- Device Configuration tab -->
			<h2>Device Configuration</h2>

			<?php include '../devicetabs.php'; ?>

				<div class="contentblock2">
						<!-- GPS subtab -->
						<div class="msgBox"></div>
						<?php require_once $_SERVER['DOCUMENT_ROOT'].'/inc/gps_view.php'; ?>
						
						<!-- Header -->
						<div class="inversetab">GPS</div>
						<!---  <a href="/TL3000_HTML5/Default_CSH.htm#gps" TARGET="_blank"><img src="/images/help.png" alt="help" border="0" ></a> -->
						<div class="hr"><hr /></div>

						<form id="gps" method="post" action="/inc/gps_processor.php">
							<input type="hidden" name="csrfToken" value="<?php echo (!empty($_SESSION['csrfToken']) ? $_SESSION['csrfToken'] : 'noData'); ?>" />
							<div class="row">
								<span class="label">GPS Time</span>
								<span class="formw">
									<input type="text" size="30" readonly="readonly" id="gpsTime" value="<?php echo $time; ?>"/>
								</span>
							</div>
							<div class="row">
								<span class="label">GPS Chip</span>
								<span class="formw">
								<input type="text" size="15" readonly="readonly" id="gpsChip" name="gpsChip" value="<?php echo $gps_type; ?>"/>
								</span>
							</div>

							<div class="row">
								<span class="label">GPS Source</span>
								<span class="formw">
								<input type="text" readonly="readonly" id="gpsOption" name="gpsOption" value="<?php echo $gps_source; ?>"/>
								</span>
							</div>

							<div class="row gpsLive">
								<span class="label">Latitude</span>
								<span class="formw">
									<input type="text" size="15" readonly="readonly" id="latitude" value="<?php echo $latitude; ?>"/>
								</span>
							</div>

							<div class="row gpsLive">
								<span class="label">Longitude</span>
								<span class="formw">
									<input type="text" size="15" readonly="readonly" id="longitude" value="<?php echo $longitude; ?>"/>
								</span>
							</div>

							<div class="row gpsLive">
								<span class="label">Elevation</span>
								<span class="formw">
									<input type="text" size="25" readonly="readonly" id="elevation" value="<?php echo $elevation; ?>"/>
								</span>
							</div>

							<div class="row">
								<span class="label">Heading</span>
								<span class="formw">
									<input type="text" size="5" readonly="readonly" id="heading" value="<?php echo $heading; ?>"/>
								</span>
							</div>

							<div class="row">
								<span class="label">GPS Speed</span>
								<span class="formw">
									<input type="text" size="16" readonly="readonly" id="velocity" value="<?php echo $velocity; ?>"/>
								</span>
							</div>


							<div class="row">
								<span class="label">Number of Satellites</span>
								<span class="formw">
									<input type="text" size="5" readonly="readonly" id="satellites" value="<?php echo $satellites; ?>"/>
									<!-- <span class="realtime"><img style="margin-top:-3px;" src="/images/loading.gif" alt="Loading..." /></span> -->
								</span>
							</div>

							<div class="row">
								<span class="label">HDOP</span>
								<span class="formw">
									<input type="text" size="6" readonly="readonly" id="hdop" value="<?php echo $hdop; ?>"/>
								</span>
							</div>

							<div class="row">
								<span class="label">Quality</span>
								<span class="formw">
									<input type="text" size="14" readonly="readonly" id="quality" value="<?php echo $quality; ?>"/>
								</span>
							</div>

							<div class="spacer">&nbsp;</div>
							<?php
							if (0) //(hasSubmitAccess()) // not needed for LENS - there is nothing to change.
							{
							?>
							<div class="hr"><hr /></div>
							<div class="row">
								<span class="formw">
									<button type="submit" class="button2-link">Save</button>&nbsp;
									<button type="reset" class="button3-link">Cancel</button>&nbsp;
									<!-- <button class="button4-link">Sync</button> -->
								</span>
							</div>
							<?php
							}
							?>
						</form>
				</div> <!--  end contentblock2 -->
			</div> <!--  end contentblock -->
		</div> <!--  end container -->
</body>
</html>

