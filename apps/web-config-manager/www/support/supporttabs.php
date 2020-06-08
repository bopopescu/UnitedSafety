<?php if(empty($_SESSION['M2M_SESH_USERNAME'])) { exit;}  ?>
<div class="tabblock2">
	<div class="tabs2">
	
		<!-- Main tabs -->
		
		<ul>
			<!-- The What's New and Known Issues tabs were removed because their content wasn't being updated for each release -->
			<li <?php if (stripos($_SERVER["REQUEST_URI"],"/support/changepassword/index.php") === 0) echo " class='active' ";?>><a href="/support/changepassword/index.php">Change Password</a></li>
		</ul>
	</div>
</div>
