<?php if(empty($_SESSION['M2M_SESH_USERNAME'])) { exit;}  ?>
<div class="tabblock2">
	<div class="tabs2">
	
		<!-- Main tabs -->
		<ul>
			<li <?php if (stripos($_SERVER["REQUEST_URI"],"/network/wifi/index.php") === 0) echo " class='active' ";?>><a href="/network/wifi/index.php">WiFi</a></li>
			<?php
				if(hasSubmitAccess())
				{
			?>
					<li <?php if (stripos($_SERVER["REQUEST_URI"],"/network/ethernet/index.php") === 0) echo " class='active' ";?>><a href="/network/ethernet/index.php">Ethernet</a></li>
			<?php 
				}
			?>
			<li <?php if (stripos($_SERVER["REQUEST_URI"],"/network/cellular/index.php") === 0) echo " class='active' ";?>><a href="/network/cellular/index.php">Cellular</a></li>
		</ul>
	</div>
</div>
