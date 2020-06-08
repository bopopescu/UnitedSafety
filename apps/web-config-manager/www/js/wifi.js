var g_wifi_ap_ssid = '';
var g_wifi_ap_authtype = '';
var g_wifi_ap_encryptype = '';
$(document).ready(function () {
	/*********** On Page Load - Form mods: Begin ******************/

	// script to switch encryption mode based off of authentication mode
	if ($("form#wifi-ap select[name = 'authtype'] option:selected").val() != "") {
		setWifiEncrypt();
	}

	g_wifi_ap_ssid = $("form#wifi-ap input[name='ssid']").val();
	g_wifi_ap_authtype = $("form#wifi-ap select[name='authtype']").val();
	g_wifi_ap_encryptype = $("form#wifi-ap select[name='encryptype']").val();

	// populate the first three octets of the DHCP Start and End IP with the wifi-ap IP values
	$("form#wifi-ap input[name = sdhcpoct1]").val($("form#wifi-ap input[name = wipoct1]").val());
	$("form#wifi-ap input[name = edhcpoct1]").val($("form#wifi-ap input[name = wipoct1]").val());
	$("form#wifi-ap input[name = sdhcpoct2]").val($("form#wifi-ap input[name = wipoct2]").val());
	$("form#wifi-ap input[name = edhcpoct2]").val($("form#wifi-ap input[name = wipoct2]").val());
	$("form#wifi-ap input[name = sdhcpoct3]").val($("form#wifi-ap input[name = wipoct3]").val());
	$("form#wifi-ap input[name = edhcpoct3]").val($("form#wifi-ap input[name = wipoct3]").val());

	// Disabled the DHCP fields if DHCP server is Disabled
	if ($("form#ethernet input[name = dhcpserver]:checked").val() == 'Disabled') {
		disableDHCPsettings("wifi-ap");
	} else {
		enableDHCPsettings("wifi-ap");
	}

	/*********** On Page Load - Form mods: End *******************/

	/*********** Bind Event handlers for form elements: Begin ******************/

	$("form#wifi-ap select[name = 'authtype']").change(function () {
		setWifiEncrypt();
	});

	$("form#wifi-ap input[type='button']").click(function() {
	  $("#leasestable").load("index.php #leasestable");
	});

	// When the first IP octet of the Ethernet IP changes, update the first octet of the DHCP Start and End IPs
	$("form#wifi-ap input[name = 'wipoct1']").change(function () {
		$("form#wifi-ap input[name = 'sdhcpoct1']").val($("form#wifi-ap input[name = 'wipoct1']").val());
		$("form#wifi-ap input[name = 'edhcpoct1']").val($("form#wifi-ap input[name = 'wipoct1']").val());
		validateDHCPSettings("wifi-ap");

	});

	// When the second IP octet of the Ethernet IP changes, update the second octet of the DHCP Start and End IPs
	$("form#wifi-ap input[name = 'wipoct2']").change(function () {
		$("form#wifi-ap input[name = 'sdhcpoct2']").val($("form#wifi-ap input[name = 'wipoct2']").val());
		$("form#wifi-ap input[name = 'edhcpoct2']").val($("form#wifi-ap input[name = 'wipoct2']").val());
		validateDHCPSettings("wifi-ap");
	});

	// When the third IP octet of the Ethernet IP changes, update the third octet of the DHCP Start and End IPs
	$("form#wifi-ap input[name = 'wipoct3']").change(function () {
		$("form#wifi-ap input[name = 'sdhcpoct3']").val($("form#wifi-ap input[name = 'wipoct3']").val());
		$("form#wifi-ap input[name = 'edhcpoct3']").val($("form#wifi-ap input[name = 'wipoct3']").val());
		validateDHCPSettings("wifi-ap");
	});

	// When the fourth octet of the Ethernet IP changes, check that there is no conflict with the DHCP IP range
	$("form#wifi-ap input[name = wipoct4]").change(function(){
		validateDHCPSettings("wifi-ap");
	});

	// When the DHCP Start/End IP change, validate the settings
	$("form#wifi-ap input[name = 'sdhcpoct4'], form#wifi-ap input[name = 'edhcpoct4']").change(function(){
		validateIP("wip");
		validateDHCPSettings("wifi-ap");
	});
	
	$WiFiDHCPEnable = initializeFields('wifi-ap', 'dhcp-server-section', 'dhcpserver', null, null, on_enable_dhcp, on_disable_dhcp);

	$("form#wifi-ap input, form#wifi-ap select").on("change keyup", validateWiFiAP);

	validateWiFiAP();
	/*********** Bind Event handlers for form elements: End ******************/

}); // END DOC READY

function validateWiFiAP()
{
	$("form#wifi-ap").find('.errorMsg').empty();
	var enable = true;
	//validate ssid always
	enable &= ($("form#wifi-ap input[name='ssid']").prop('disabled')?true: isNotEmpty($("form#wifi-ap input[name='ssid']")));

	//validate IP and subnet
	enable &= ($("#wip input").prop("disabled"))? true: hasValidIP("wip");

	enable &= ($("#sdhcpip input").prop('disabled'))? true: hasValidIP("sdhcpip");
	enable &= ($("#edhcpip input").prop("disabled"))? true: hasValidIP("edhcpip");
	//if values changed check password for blank
	$("form#wifi-ap button[type='submit']").prop('disabled', !enable);
}

function on_enable_wifi_client()
{
	$('#wifi_networks_table').find('input').removeAttr('disabled');
	return true;
}

function on_disable_wifi_client()
{
	$('#wifi_networks_table').find('input').attr('disabled','disabled');
	return true;
}

function on_enable_dhcp()
{
	validateIP("wip");
	validateDHCPSettings("wifi-ap");
}

function on_disable_dhcp()
{
	validateIP("wip");
}

/* Populate the encrypt type menu based on the auth mode menu selection */
function setWifiEncrypt() {
	$wifiap_json = {
		"OPEN": ["NONE", "WEP"],
		"SHARED": ["WEP"],
		"WEPAUTO": ["WEP"],
		"WPAPSK": ["TKIP", "AES", "TKIPAES"],
		"WPA2PSK": ["TKIP", "AES", "TKIPAES"],
		"WPAPSKWPA2PSK": ["TKIP", "AES", "TKIPAES"],
	};

	//grab the set encryp type that the php sets
	$setEncrypType = $("form#wifi-ap select[name = 'encryptype'] option:selected").val();

	$auth = $("select[name='authtype']").val();
	$("select[name = 'encryptype']").empty();

	$wifiap_json[$auth].forEach(function (val) {
		if ($setEncrypType == val) {
			$("select[name = 'encryptype']").append('<option selected="selected" value="' + val + '">' + val + '</option>');
		}
		else
		{
			$("select[name = 'encryptype']").append('<option value="' + val + '">' + val + '</option>');
		}
	});

}

function validatePassword513($id)
{
	if (($("form#wifi-ap input[name = 'wifipassword']").val().length == 5) || ($("form#wifi-ap input[name = 'wifipassword']").val().length == 13))
	{
		$("#" + $id + " > .errorMsg").empty();
		return true;
	}

	$("#" + $id + " > .errorMsg").text("Password is not 5 OR 13 characters").show();
	return false;
} // end validatePassword513

function validatePassword863($id)
{
	if (($("form#wifi-ap input[name = 'wifipassword']").val().length < 8) || ($("form#wifi-ap input[name = 'wifipassword']").val().length > 63)) {
		$("#" + $id + " > .errorMsg").text("Password is not 8 to 63 characters").show();
		return false;
	}
	$("#" + $id + " > .errorMsg").empty();
	return true;
} // end validatePassword863

function passwordCheck() {
	var selection = $("form#wifi-ap select[name = 'encryptype']").val();
	if (selection == 'NONE')
	{
		$("form#wifi-ap input[name = 'wifipassword']").attr('disabled', true);
		$("form#wifi-ap input[name = 'rwifipassword']").attr('disabled', true);

		$("form#wifi-ap span[name = 'pwdMessage']").empty();
		$("form#wifi-ap span[name = 'errorPassword']").empty();
		$("form#wifi-ap span[name = 'rerrorPassword']").empty();

		return true;
	}
	var enable = true;
	if (selection == 'WEP')
	{
		$("form#wifi-ap input[name = 'wifipassword']").removeAttr('disabled');
		$("form#wifi-ap input[name = 'rwifipassword']").removeAttr('disabled');
		$("form#wifi-ap span[name = 'errorPassword']").show();
		$("form#wifi-ap span[name = 'rerrorPassword']").show();

		$("form#wifi-ap span[name = 'wifipwdMessage']").text("");
		$("form#wifi-ap span[name = 'wifipwdMessage']").next("br").remove();
		enable &= validatePassword513("pwdID");

		enable &= checkPasswordMatch("pwdID","rpwdID");
	}

	if (selection == 'TKIP' || selection == 'AES' || selection == 'TKIPAES')
	{
		$("form#wifi-ap input[name = 'wifipassword']").removeAttr('disabled');
		$("form#wifi-ap input[name = 'rwifipassword']").removeAttr('disabled');
		$("form#wifi-ap span[name = 'wifipwdMessage']").text("");
		$("form#wifi-ap span[name = 'wifipwdMessage']").next("br").remove();
		enable &= validatePassword863("pwdID");

		enable &= checkPasswordMatch("pwdID","rpwdID");
	}
	return enable;
}

//check if the retyped password matches the first one
function checkPasswordMatch($id, $id2)
{
		if ($("form#wifi-ap input[name = 'wifipassword']").val() != $("form#wifi-ap input[name = 'rwifipassword']").val())
		{
			$("#" + $id2 + " > .errorMsg").text("Password and re-entered password do not match").show();
			return false;
		}
		else
		{
			$("#" + $id2 + " > .errorMsg").empty();
			return true;
		}
}