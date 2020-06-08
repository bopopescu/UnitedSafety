<?php
require_once $_SERVER['DOCUMENT_ROOT'] . '/inc/session_controller.inc';
?>
<!DOCTYPE HTML>

<?php
function isTemplateDeletable($templateName) // can the template be deleted? (Must have submit access and not be a default template)
{
	if (!hasSubmitAccess())
	  return false;
	if ($templateName == "Cummins" || $templateName == "Murphy" || $templateName == "VFD")
		return false;
	
	return true;
}
?>

<html>
<head>
	<meta charset="UTF-8">
	<meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
	<title>TGX - <?php echo DEVICE_NAME; ?></title>

<?php
include $_SERVER['DOCUMENT_ROOT'] . 'mainscriptsgroup.php';
?>
</head>

<body>
	<div id="dialog" class="ui-dialog-content ui-widget-content"></div>
	<div class="container">
		<?php
		include $_SERVER['DOCUMENT_ROOT'] . 'header.php';
		?>

		<div class="clear"></div>
		<div class="clear"></div>

		<?php
		include $_SERVER['DOCUMENT_ROOT'] . 'tabs.php';
		?>

		<div class="contentblock">

			<!-- Device  tab -->
			<h2>Device Configuration</h2>

			<?php
			include '../devicetabs.php';
			?>

				<div class="contentblock2">
				<div class="msgBox"></div>
				<?php
					require_once $_SERVER['DOCUMENT_ROOT'] . '/inc/modbus-settings.inc';
					print "
				  <script type=\"text/javascript\" >
					var g_templateAssignments = {";
					foreach ($slave_array as $slave => $template)
					{
						print "
						$slave : '$template',";
					}
					print "
					};
					var g_templateNames = {";
					foreach ($templates_array as $key => $template)
					{
						print "
						$key : '$template',";
					}
					print "
					};
				</script>";
				?>
				<div id="modbusSettings">
					<div class="inversetab">Modbus</div>
					<!--- <a href="/TL3000_HTML5/Default_CSH.htm#MODBUS" TARGET="_blank"><img src="/images/help.png" alt="help" border="0" ></a> -->
					<div class="hr"><hr /></div>
					<br/>
					<form id="modbusSubSettings" method="post" action="/inc/modbus_controller.php">
						<input type="hidden" name="csrfToken" value="<?php echo (!empty($_SESSION['csrfToken']) ? $_SESSION['csrfToken'] : 'noData'); ?>" />
						<input type="hidden" name="op" value="updateSettings" />
						<fieldset id="LensModbusSettingsField">
							<legend>LENS Settings</legend>
							<div class="row">
								<span class="label">Enable Modbus for Lens</span>
								<span class="formw units">
									<input type="radio" size="6"  name="LensEnable" value="On" <?php print(($LensEnable) ? "checked=\"checked\"" : ""); ?> /> On &nbsp;
									<input type="radio" size="6"  name="LensEnable" value="Off" <?php print((!$LensEnable) ? "checked=\"checked\"" : ""); ?> /> Off
								</span>
								<br/>
							</div>
						</fieldset>
						<br/>
						
						<fieldset id="EthernetModbusSettingsField">
							<legend>Ethernet Settings</legend>
							<div class="row">
								<span class="label">Enable Ethernet</span>
								<span class="formw units">
									<input type="radio" size="6" name="EthernetEnable" value="On" <?php print(($EthernetEnable) ? "checked=\"checked\"" : ""); ?> /> On &nbsp;
									<input type="radio" size="6" name="EthernetEnable" value="Off" <?php print((!$EthernetEnable) ? "checked=\"checked\"" : ""); ?> /> Off
								</span>
								<br/>
							</div>
						</fieldset>
							
						<div class="spacer">&nbsp;</div>
						<?php
						if(hasSubmitAccess())
						{
						?>
						<div class="row">
							<span class="formw">
								<button type="submit" class="button2-link">Save</button>&nbsp;
								<button type="reset" class="button3-link" >Clear</button>
							</span>
						</div>
						<?php
						}
						?>
						<div class="spacer">&nbsp;</div>
					</form>
					<br />
					<div class="spacer">&nbsp;</div>
				</div>
			</div>
	</div><!-- end of entire div container -->
</body>
</html>
