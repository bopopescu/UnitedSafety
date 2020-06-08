<?php

require_once $_SERVER['DOCUMENT_ROOT'] . '/inc/util.inc';
//contains functions for socket interaction, error message display, and logging.

class dbconfigController {
	/**
	 * getDbconfig
	 *
	 * Get dbconfig data by calling the admin client function 'dbget'
	 * @param string $p_app - name of the App in dbconfig and is optional
	 * @param string $p_key - name of the key in dbconfig and is optional. Key is ignored if "p_app" is NULL.
	 * @return string - if $p_key is specified then the value for the key is returned. If
	 * no key is specified then all the values stored for the $p_app are returned
	 * @author Amour Hassan (Amour.Hassan@gps1.com)
	 */
	function getDbconfig($p_app, $p_key) {
		$sh_args = NULL;

		if (NULL != $p_app) {

			if (NULL != $p_key) {
				$sh_args = 'dbget ' . bin2hex($p_app) . ' ' . bin2hex($p_key);
			} else {
				$sh_args = 'dbget ' . bin2hex($p_app);
			}

		} else {
			$sh_args = 'dbget';
		}

		$sh_out = atsexec(escapeshellcmd($sh_args));
		debug('dbconfig_controller.inc|getDbconfig: ' . $sh_args . ' (admin client)', $sh_out);

		if ((strcasecmp($sh_out, 'phpcmd: fail') != 0) && (strcasecmp($sh_out, 'phpcmd: invalid command') != 0)) {
			return $sh_out;
		}

		return NULL;
	}//END getDbconfig

	/**
	 * getDbconfigData
	 *
	 * Get dbconfig data by calling the admin client function 'dbconfigget'
	 * @param string $app - name of the App in dbconfig
	 * @param string $key - name of the key in dbconfig
	 * @return string
	 * @author Sean Toscano (sean@absolutetrac.com)
	 */
	function getDbconfigData($app, $key) {
		$sh_args = 'dbconfigget ' . $app . ' ' . $key;
		$sh_out = atsexec(escapeshellcmd($sh_args));
		//socket call
		debug('dbconfig_controller.inc|getDbconfigData: ' . $sh_args . ' (admin client)', $sh_out);

		if (strcasecmp($sh_out, 'phpcmd: fail') != 0 && strcasecmp($sh_out, 'phpcmd: invalid command') != 0 && !stripos($sh_out, ".cpp")) {
			return $sh_out;
		}

		return false;
	}//END getDbconfigData

	/**
	 * getDbconfigMultilineData
	 *
	 * Get dbconfig multi line data by calling the admin client function 'dbconfigget'
	 * @param string $app - name of the App in dbconfig
	 * @param string $key - name of the key in dbconfig
	 * @return string
	 * @author Sean Toscano (sean@absolutetrac.com)
	 */
	function getDbconfigMultilineData($app, $key) {
		$sh_args = 'dbconfigget ' . $app . ' ' . $key;
		$sh_out = atsexec(escapeshellcmd($sh_args));
		//socket call
		debug('dbconfig_controller.inc|getDbconfigData: ' . $sh_args . ' (admin client)', $sh_out);

		if (strcasecmp($sh_out, 'phpcmd: fail') != 0 && strcasecmp($sh_out, 'phpcmd: invalid command') != 0 && !strpos($sh_out, ".cpp")) {
			return $sh_out;
			//return condition value
		} else {
			return false;
		}
	}//END getDbconfigMultilineData

	/**
	 * DEPRACATED: This function does not account for bash commands/characters in the app, key or value.
	 * Please use setDbconfig
	 * setDbconfigData
	 *
	 * Set dbconfig data by calling the admin client function 'dbconfigset'
	 * @param string $app - name of the App in dbconfig
	 * @param string $key - name of the key in dbconfig
	 * @param string $value - value to set in dbconfig
	 * @return string
	 * @author Sean Toscano (sean@absolutetrac.com)
	 */
	function setDbconfigData($app, $key, $value) {
		$sh_args = 'dbconfigset ' . $app . ' ' . $key . ' ' . $value;
		$sh_out = atsexec(escapeshellcmd($sh_args));
		//socket call
		debug('dbconfig_controller.inc|setDbconfigData: ' . $sh_args . ' (admin client)', $sh_out);

		if (strcasecmp($sh_out, 'phpcmd: ok') == 0) {
			return true;
		} else {
			return false;
		}
	}//END setDbconfigData

	/**
	 * setDbconfig
	 *
	 * Set dbconfig data by calling the admin client function 'dbset'. Should migrate all
	 * new pages to use this command
	 * @param string $app - name of the App in dbconfig
	 * @param string $key - name of the key in dbconfig
	 * @param string $value - value to set in dbconfig
	 * @return string
	 * @author Sean Toscano (sean@absolutetrac.com)
	 * @author Tyson Pullukatt (Tyson.Pullukatt@gps1.com)
	 */
	function setDbconfig($app, $key, $value) {
		$sh_args = 'dbset ' . bin2hex($app) . ' ' . bin2hex($key) . ' ' . bin2hex($value);
		$sh_out = atsexec(escapeshellcmd($sh_args));
		//socket call
		debug('dbconfig_controller.inc|setDbconfig: ' . $sh_args . ' (admin client)', $sh_out);

		if (strcasecmp(trim($sh_out), 'dbset: ok') == 0) {
			return true;
		}

		return false;
	}//END setDbconfig

	/**
	 * setDbconfigDataFile
	 *
	 * Set dbconfig data by calling the admin client function 'dbset'. Use to set an
	 * entire file into db-config as value
	 * @param string $app - name of the App in dbconfig
	 * @return string
	 * @author Sean Toscano (sean@absolutetrac.com)
	 * @author Tyson Pullukatt (Tyson.Pullukatt@gps1.com)
	 */
	function setDbconfigDataFile($app, $key, $file) {
		$sh_args = 'dbsetfile ' . bin2hex($app) . ' ' . bin2hex($key) . ' ' . bin2hex($file);
		$sh_out = atsexec(escapeshellcmd($sh_args));
		//socket call
		debug('dbconfig_controller.inc|setDbconfigDataFile: ' . $sh_args . ' (admin client)', $sh_out);

		if (strcasecmp(trim($sh_out), 'dbsetfile: ok') == 0) {
			return true;
		}

		return false;

	}//END setDbconfigDataFile

	/**
	 * unsetDbconfigData
	 *
	 * Unset dbconfig data by calling the admin client function 'dbconfigunset'
	 * ATS FIXME: This function does not convert parameters to hex. Any key names or app names with special characters
	 * cannot be removed/unset. To fix this we need to add a function in admin client that handles hex encoded app and key
	 * parameters.
	 * @param string $app - name of the App in dbconfig
	 * @param string $key - name of the key in dbconfig
	 * @return string
	 * @author Sean Toscano (sean@absolutetrac.com)
	 */
	function unsetDbconfigData($app, $key) {
		$sh_args = 'dbconfigunset ' . $app . ' ' . $key;
		$sh_out = atsexec(escapeshellcmd($sh_args));
		//socket call
		debug('dbconfig_controller.inc|unsetDbconfigData: ' . $sh_args . ' (admin client)', $sh_out);

		if (strcasecmp($sh_out, 'phpcmd: ok') == 0) {
			return true;
		} else {
			return false;
		}
	} //END unsetDbconfigData

} //END class dbconfigController
?>
