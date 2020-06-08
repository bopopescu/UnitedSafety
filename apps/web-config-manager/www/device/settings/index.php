<?php
require_once $_SERVER['DOCUMENT_ROOT'].'inc/session_controller.inc';

?>
<!DOCTYPE HTML>

<html>
<head>
<meta charset="UTF-8">
<meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
<title>Settings - <?php echo DEVICE_NAME; ?></title>

<?php include $_SERVER['DOCUMENT_ROOT'].'mainscriptsgroup.php'; ?>
<script type='text/javascript' src='/js/settings.js'></script>

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

				<div class="msgBox"></div>

				<?php require_once $_SERVER['DOCUMENT_ROOT'].'inc/settings_view.php'; ?>
				
				<!-- Settings subtab -->

				<h3>Settings</h3>

				<div id="settings" class="level3tabs" style="min-height:400px;">
					<!-- Settings subsubtabs -->
					<ul>
						<li><a href="#settings-sleep">Sleep Conditions</a></li>
					</ul>
					<div id="settings-sleep">
						<?php include_once 'sleep.php'; ?>
					</div>

				</div> <!--  end settings -->
			</div> <!--  end contentblock2 -->
		</div> <!--  end contentblock -->
	</div> <!--  end container -->
</body>
</html>
