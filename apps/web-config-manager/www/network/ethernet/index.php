<?php require_once $_SERVER['DOCUMENT_ROOT'].'inc/session_controller.inc'; ?>
<!DOCTYPE HTML>

	<head>
		<meta charset="UTF-8">
		<meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
		<title>Ethernet - <?php echo DEVICE_NAME; ?></title>

		<?php include $_SERVER['DOCUMENT_ROOT'].'mainscriptsgroup.php'; ?>
		<script type="text/javascript" src="/js/ethernet.js"></script>
	
	</head>

<html>
<body>

	<?php
	if(!hasSubmitAccess())
	{
		header("location:https://".$_SERVER['HTTP_HOST']."//network/wifi/index.php");	
	}
	else
	{
	?>
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
				<!-- Ethernet subtab -->
				<div class="msgBox"></div>
				<?php require_once $_SERVER['DOCUMENT_ROOT'].'inc/ethernet_view.inc'; ?>
				
				<!-- Header -->
				<div class="inversetab">Ethernet</div>
				<!---  <a href="/TL3000_HTML5/Default_CSH.htm#ETHERNET" TARGET="_blank"><img src="/images/help.png" alt="help" border="0" ></a> -->
				<div class="hr"><hr /></div>
				
				<form id="ethernet" method="post" action="/inc/ethernet_processor.php">
					<input type="hidden" name="csrfToken" value="<?php echo (!empty($_SESSION['csrfToken']) ? $_SESSION['csrfToken'] : 'noData'); ?>" />
					<input type="hidden" name="network" value="ethernet" />
					<input type="hidden" name="interface" value="eth0" />
					<input type="hidden" name="interface_other" value="ra0" />
					
					<div class="row">
						<span class="label">IP Address</span>
						<span class="formw ip" id="eip">
							<input type="text" size="2" maxlength="3" name="eipoct1" class="autotab" value="<?php echo $eip[0]; ?>">.
							<input type="text" size="2" maxlength="3" name="eipoct2" class="autotab" value="<?php echo $eip[1]; ?>">.
							<input type="text" size="2" maxlength="3" name="eipoct3" class="autotab" value="<?php echo $eip[2]; ?>">.
							<input type="text" size="2" maxlength="3" name="eipoct4" class="autotab" value="<?php echo $eip[3]; ?>">
							<span class="errorMsg" name="errorEIP"></span>
						</span>
					</div>
					<div class="row">
						<span class="label">Current IP Address</span>
						<span class="formw ip" id="cip">
							<input type="text" size="2" maxlength="3" name="cipoct1" class="autotab" value="<?php echo $cip[0]; ?>" readonly="readonly">.
							<input type="text" size="2" maxlength="3" name="cipoct2" class="autotab" value="<?php echo $cip[1]; ?>" readonly="readonly">.
							<input type="text" size="2" maxlength="3" name="cipoct3" class="autotab" value="<?php echo $cip[2]; ?>" readonly="readonly">.
							<input type="text" size="2" maxlength="3" name="cipoct4" class="autotab" value="<?php echo $cip[3]; ?>" readonly="readonly">
							<span class="errorMsg" name="errorEIP"></span>
						</span>
					</div>
					<div class="row">
						<span class="label">Subnet Mask</span>
						<span class="formw ip" id="esm">
							<input type="text" size="2" maxlength="3" name="esoct1" class="autotab" value="<?php echo $emask[0]; ?>">.
							<input type="text" size="2" maxlength="3" name="esoct2" class="autotab" value="<?php echo $emask[1]; ?>">.
							<input type="text" size="2" maxlength="3" name="esoct3" class="autotab" value="<?php echo $emask[2]; ?>">.
							<input type="text" size="2" maxlength="3" name="esoct4" class="autotab" value="<?php echo $emask[3]; ?>">
							<span class="errorMsg"></span>
						</span>
					</div>
					<div class="row">
						<span class="label">Primary DNS</span>
						<span class="formw">
							<input type="text" size="2" maxlength="3" name="dns1oct1" value="<?php echo $dns1[0]; ?>" readonly="readonly">.
							<input type="text" size="2" maxlength="3" name="dns1oct2" value="<?php echo $dns1[1]; ?>" readonly="readonly">.
							<input type="text" size="2" maxlength="3" name="dns1oct3" value="<?php echo $dns1[2]; ?>" readonly="readonly">.
							<input type="text" size="2" maxlength="3" name="dns1oct4" value="<?php echo $dns1[3]; ?>" readonly="readonly">
						</span>
					</div>
					<div class="row">
						<span class="label">Secondary DNS</span>
						<span class="formw">
							<input type="text" size="2" maxlength="3" name="dns2oct1" value="<?php echo $dns2[0]; ?>" readonly="readonly">.
							<input type="text" size="2" maxlength="3" name="dns2oct2" value="<?php echo $dns2[1]; ?>" readonly="readonly">.
							<input type="text" size="2" maxlength="3" name="dns2oct3" value="<?php echo $dns2[2]; ?>" readonly="readonly">.
							<input type="text" size="2" maxlength="3" name="dns2oct4" value="<?php echo $dns2[3]; ?>" readonly="readonly">
						</span>
					</div>
					<br /> <br />
					<div class="inversetab">DHCP</div>
					<div class="hr"><hr /></div>
					<div class="row">
						<span class="label">DHCP Server</span>
						<span class="formw">
							<?php	debug('(ethernet/index.php) $edhcp_state info',$edhcp_status); ?>
							<input type=radio name="dhcpserver" value="auto" <?php echo ((strcasecmp($edhcp_status,'auto') == 0) ? 'checked="checked"' : ''); ?>/> Auto&nbsp;&nbsp;
							<input type=radio name="dhcpserver" value="Enabled"  <?php echo ((strcasecmp($edhcp_status,'on') == 0) ? 'checked="checked"' : ''); ?>/> Enabled&nbsp;&nbsp;
							<input type=radio name="dhcpserver" value="Disabled" <?php echo ((strcasecmp($edhcp_status,'off') == 0) ? 'checked="checked"' : ''); ?>/> Disabled
						</span>
					</div>
					<div class="row">
						<span class="label">Starting IP Address</span>
						<span class="formw ip" id="sdhcpip">
							<input type="text" size="2" maxlength="3" readonly="readonly" name="sdhcpoct1" value="<?php echo $edhcp_startip[0]; ?>">.
							<input type="text" size="2"	maxlength="3" readonly="readonly" name="sdhcpoct2" value="<?php echo $edhcp_startip[1]; ?>">.
							<input type="text" size="2" maxlength="3" readonly="readonly" name="sdhcpoct3" value="<?php echo $edhcp_startip[2]; ?>">.
							<input type="text" size="2" maxlength="3" name="sdhcpoct4" value="<?php echo $edhcp_startip[3]; ?>">
							<span class="errorMsg"  name="errorDHCPSIP"></span>
						</span>				
					</div>
					<div class="row">
						<span class="label">Ending IP Address</span>
						<span class="formw ip" id="edhcpip">
							<input type="text" size="2" maxlength="3" readonly="readonly" name="edhcpoct1" value="<?php echo $edhcp_endip[0]; ?>">.
							<input type="text" size="2" maxlength="3" readonly="readonly" name="edhcpoct2" value="<?php echo $edhcp_endip[1]; ?>">.
							<input type="text" size="2" maxlength="3" readonly="readonly" name="edhcpoct3" value="<?php echo $edhcp_endip[2]; ?>">.
							<input type="text" size="2" maxlength="3" name="edhcpoct4" value="<?php echo $edhcp_endip[3]; ?>">
							<span class="errorMsg"  name="errorDHCPEIP"></span>
						</span>
					</div>						
					<div class="row">
						<span class="label">DHCP leases</span>
						<span class="formw">
							<div style="width: 470px; overflow-x: auto;">
									<?php echo (!empty($edhcp_leases) ? $edhcp_leases : ''); ?>
							</div>
						</span>
					</div>
					<div class="spacer">&nbsp;</div>

					<div class="inversetab">Routing Override</div>
					<div class="hr"><hr /></div>
					
					<div class="row">
						<span class="label">Override</span>
						<span class="formw">
							<input type=radio title="No routing override" name="ctlRouteOverride" value="none" <?php echo ((strcasecmp($useDefaultRoute,'none') == 0) ? 'checked="checked"' : ''); ?>/> None&nbsp;&nbsp;
							<input type=radio title="This unit will act as a fallback gateway to internet access" name="ctlRouteOverride" value="primary"  <?php echo ((strcasecmp($useDefaultRoute,'primary') == 0) ? 'checked="checked"' : ''); ?>/> Gateway&nbsp;&nbsp;
							<input type=radio title="This unit will redirect to a Gateway unit" name="ctlRouteOverride" value="secondary" <?php echo ((strcasecmp($useDefaultRoute,'secondary') == 0) ? 'checked="checked"' : ''); ?>/> Node
						</span>
					</div>
					
					<div class="row">
						<span class="label">Route IP Address</span>
						<span class="formw ip" id="routeip">
							<input type="text" size="2" maxlength="3" readonly="readonly" name="ctlRouteIP1" value="<?php echo $cip[0]; ?>">.
							<input type="text" size="2"	maxlength="3" readonly="readonly" name="ctlRouteIP2" value="<?php echo $cip[1]; ?>">.
							<input type="text" size="2" maxlength="3" readonly="readonly" name="ctlRouteIP3" value="<?php echo $cip[2]; ?>">.
							<input type="text" title="Address of Gateway TruLink on network" size="2" maxlength="3" name="ctlRouteIP" value="<?php echo $RouteIP; ?>">
							<span class="errorMsg"  name="errorDHCPRouteIP"></span>
						</span>				
					</div>
					<div class="spacer">&nbsp;</div>
					<?php
					//if($_SESSION['M2M_SESH_USERAL'] < 300)
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
	<?php
	}
	?>

</body>
</html>
