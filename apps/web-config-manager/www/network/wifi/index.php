<?php require_once $_SERVER['DOCUMENT_ROOT'].'inc/session_controller.inc'; ?>
<!DOCTYPE HTML>

<html>
	<head>
		<meta charset="UTF-8">
		<meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
		<title>WiFi - <?php echo DEVICE_NAME; ?></title>

		<?php include $_SERVER['DOCUMENT_ROOT'].'mainscriptsgroup.php'; ?>
		<script type="text/javascript" src="/js/wifi.js"></script>
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

					<!-- Wifi subtab -->
					<div class="msgBox"></div>
					<?php require_once $_SERVER['DOCUMENT_ROOT'].'inc/wifi_view.inc'; ?>

					<!-- Header -->
					<div class="inversetab">WiFi</div>
					<!---  <a href="/TL3000_HTML5/Default_CSH.htm#WIFI" TARGET="_blank"><img src="/images/help.png" alt="help" border="0" ></a> -->
					<div class="hr"><hr /></div>
					
					<div id="wifi" class="level3tabs">
						<!-- Wifi subsubtabs -->
						<ul>
							<li><a href="#wifi-accesspoint">Access Point</a></li>
						</ul>
						<div id="wifi-accesspoint">
							<!-- Access Point subsubtab -->
							<form id="wifi-ap" method="post" action="/inc/wifi_processor.php">
								<input type="hidden" name="csrfToken" value="<?php echo (!empty($_SESSION['csrfToken']) ? $_SESSION['csrfToken'] : 'noData'); ?>" />
								<input type="hidden" name="network" value="wifi" />
								<input type="hidden" name="interface" value="ra0" />
								<input type="hidden" name="interface_other" value="eth0" />
								<?php // Powersoft19 remove wifi-ap-enable fields to resolved ISCP-244 JIRA ?>
								<div class="row">
									<span class="label2">SSID</span>
									<span class="formw2 reg">
										<input type="text" size="26" name="ssid" value="<?php echo $wssid; ?>" />
										<span class="errorMsg"></span>
									</span>
								</div>
								<div class="row">
									<span class="label2">Authentication Mode</span>
									<span class="formw2">
										<select name="authtype" style="width: 156px;">
											<option <?php echo ((strcasecmp($wauth,'OPEN') == 0) ? 'selected="selected"':'');?> value="OPEN">OPEN</option>
											<option <?php echo ((strcasecmp($wauth,'SHARED') == 0) ? 'selected="selected"':'');?> value="SHARED">SHARED</option>
											<option <?php echo ((strcasecmp($wauth,'WEPAUTO') == 0) ? 'selected="selected"':'');?> value="WEPAUTO">WEPAUTO</option>
											<option <?php echo ((strcasecmp($wauth,'WPAPSK') == 0) ? 'selected="selected"':'');?> value="WPAPSK">WPAPSK</option>
											<option <?php echo (((strcasecmp($wauth,'WPA2PSK') == 0) || empty($wauth)) ? 'selected="selected"':'');?> value="WPA2PSK">WPA2PSK</option>
											<option <?php echo ((strcasecmp($wauth,'WPAPSKWPA2PSK') == 0) ? 'selected="selected"':'');?> value="WPAPSKWPA2PSK">WPAPSKWPA2PSK</option>
										</select>
									</span>
								</div>
								<div class="row">
									<span class="label2">Encryption Type</span>
									<span class="formw2">
										<select name="encryptype" style="width: 156px;">
											<option <?php echo ((strcasecmp($wencrypt,'NONE') == 0) ? 'selected="selected"':'');?> value="NONE">NONE</option>
											<option <?php echo ((strcasecmp($wencrypt,'WEP') == 0) ? 'selected="selected"':'');?> value="WEP">WEP</option>
											<option <?php echo ((strcasecmp($wencrypt,'AES') == 0) ? 'selected="selected"':'');?> value="AES">AES</option>
											<option <?php echo (((strcasecmp($wencrypt,'TKIP') == 0) || empty($wencrypt)) ? 'selected="selected"':'');?> value="TKIP">TKIP</option>
											<option <?php echo ((strcasecmp($wencrypt,'TKIPAES') == 0) ? 'selected="selected"':'');?> value="TKIPAES">TKIPAES</option>
										</select>
									</span>
								</div>
								<div class="row">
									<span class="label2">IP Address</span>
									<span class="formw2 ip" id="wip">
										<input type="text" size="2" maxlength="3" name="wipoct1" class="autotab" value="<?php echo $wip[0]; ?>"/>.
										<input type="text" size="2" maxlength="3" name="wipoct2" class="autotab" value="<?php echo $wip[1]; ?>"/>.
										<input type="text" size="2" maxlength="3" name="wipoct3" class="autotab" value="<?php echo $wip[2]; ?>"/>.
										<input type="text" size="2" maxlength="3" name="wipoct4" class="autotab" value="<?php echo $wip[3]; ?>"/>
										<span class="errorMsg" name="errorWIP"></span>
									</span>
								</div>
								<div class="row">
									<span class="label2">Subnet Mask</span>
									<span class="formw2 ip" id="wsm">
										<input type="text" size="2" maxlength="3" name="wsoct1" class="autotab" value="<?php echo $wmask[0]; ?>"/>.
										<input type="text" size="2" maxlength="3" name="wsoct2" class="autotab" value="<?php echo $wmask[1]; ?>"/>.
										<input type="text" size="2" maxlength="3" name="wsoct3" class="autotab" value="<?php echo $wmask[2]; ?>"/>.
										<input type="text" size="2" maxlength="3" name="wsoct4" class="autotab" value="<?php echo $wmask[3]; ?>"/>
										<span class="errorMsg"></span>
									</span>
								</div>
								<div class="row">
									<span class="label2">Primary DNS</span>
									<span class="formw2">
										<input type="text" size="2" maxlength="3" name="dns1oct1" value="<?php echo $dns1[0]; ?>" readonly="readonly"/>.
										<input type="text" size="2" maxlength="3" name="dns1oct2" value="<?php echo $dns1[1]; ?>" readonly="readonly"/>.
										<input type="text" size="2" maxlength="3" name="dns1oct3" value="<?php echo $dns1[2]; ?>" readonly="readonly"/>.
										<input type="text" size="2" maxlength="3" name="dns1oct4" value="<?php echo $dns1[3]; ?>" readonly="readonly"/>
									</span>
								</div>
								<div class="row">
									<span class="label2">Secondary DNS</span>
									<span class="formw2">
										<input type="text" size="2" maxlength="3" name="dns2oct1" value="<?php echo $dns2[0]; ?>" readonly="readonly"/>.
										<input type="text" size="2" maxlength="3" name="dns2oct2" value="<?php echo $dns2[1]; ?>" readonly="readonly"/>.
										<input type="text" size="2" maxlength="3" name="dns2oct3" value="<?php echo $dns2[2]; ?>" readonly="readonly"/>.
										<input type="text" size="2" maxlength="3" name="dns2oct4" value="<?php echo $dns2[3]; ?>" readonly="readonly"/>
									</span>
									</div>
								<div class="row">
									<span class="label2">SSH Enable</span> <span class="formw2">
									<input type="radio" name="wifi-ssh-enable" value="1" <?php echo !isOff($wifi_ssh_enable_status) ? 'checked="checked"' : '';?> />On&nbsp;&nbsp;
									<input type="radio" name="wifi-ssh-enable" value="0" <?php echo isOff($wifi_ssh_enable_status) ? 'checked="checked"' : '';?> /> Off
									</span>
								</div>
								<br /> <br />
								<div id="dhcp-server-section">
									<div class="inversetab">DHCP</div>
									<div class="hr"> <hr /> </div>
									<div class="row">
										<span class="label2">DHCP Server</span>
										<span class="formw2">
											<input type="radio" name="dhcpserver" value="1" <?php echo ((strcasecmp($wdhcp_status,'on') == 0) ? 'checked="checked"' : ''); ?>/> Enabled&nbsp;&nbsp;
											<input type="radio" name="dhcpserver" value="0" <?php echo ((strcasecmp($wdhcp_status,'off') == 0) ? 'checked="checked"' : ''); ?>/> Disabled
										</span>
									</div>
									<div class="row">
										<span class="label2">Starting IP Address</span>
										<span class="formw2 ip" id="sdhcpip">
											<input type="text" size="2" maxlength="3" name="sdhcpoct1" readonly="readonly" value="<?php echo $wdhcp_startip[0]; ?>"/>.
											<input type="text" size="2"	maxlength="3" name="sdhcpoct2" readonly="readonly" value="<?php echo $wdhcp_startip[1]; ?>"/>.
											<input type="text" size="2" maxlength="3" name="sdhcpoct3" readonly="readonly" value="<?php echo $wdhcp_startip[2]; ?>"/>.
											<input type="text" size="2" maxlength="3" name="sdhcpoct4" value="<?php echo $wdhcp_startip[3]; ?>"/>
											<span class="errorMsg"  name="errorDHCPSIP"></span>
										</span>
									</div>
									<div class="row">
										<span class="label2">Ending IP Address</span>
										<span class="formw2 ip" id="edhcpip">
											<input type="text" size="2" maxlength="3" name="edhcpoct1" readonly="readonly" value="<?php echo $wdhcp_endip[0]; ?>"/>.
											<input type="text" size="2" maxlength="3" name="edhcpoct2" readonly="readonly" value="<?php echo $wdhcp_endip[1]; ?>"/>.
											<input type="text" size="2" maxlength="3" name="edhcpoct3" readonly="readonly" value="<?php echo $wdhcp_endip[2]; ?>"/>.
											<input type="text" size="2" maxlength="3" name="edhcpoct4" value="<?php echo $wdhcp_endip[3]; ?>"/>
											<span class="errorMsg"  name="errorDHCPEIP"></span>
										</span>
									</div>
									<div class="row"><span class="label2">DHCP leases</span></div>
									<div class="row">
										<input type="button" class="button-link" value="Refresh"/>
									</div>
									<div id="leasestable" class="row">
										<span>
											<div style="width: 100%; overflow-x: auto;">
												<?php echo (!empty($wdhcp_leases) ? $wdhcp_leases : ''); ?>
											</div>
										</span>
									</div>
								</div>
								<div class="spacer">&nbsp;</div>
								<?php
								if(hasSubmitAccess())
								{
								?>
								<div class="hr"><hr /></div>
								<div class="row">
									<span class="formw2">
										<button class="button2-link" type="submit">Save</button>&nbsp;
										<button class="button3-link" type="reset">Cancel</button>&nbsp;
										<!-- <button class="button4-link">Sync</button> -->
									</span>
								</div>
								<?php
								}
								?>
							</form>
						</div>
					</div>
				</div>

		</div> <!-- end of content block (main tab container) -->
	</div><!-- end of entire div container -->
</body>
</html>
