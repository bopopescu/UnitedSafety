<?php
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';

	if(isAdmin())
	{
		header('Content-type: text/plain');
		header('Content-Disposition: attachment; filename="settings.txt"');
		print shell_exec('db-export generic');
	}

?>
