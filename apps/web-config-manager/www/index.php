<?php 

require_once $_SERVER['DOCUMENT_ROOT'].'inc/session_controller.inc';

debug("(index.php) User authenticated as Admin or User. Redirecting to general/index");
header("location:http://".$_SERVER['HTTP_HOST']."/device/general/index.php");

exit;

?>
