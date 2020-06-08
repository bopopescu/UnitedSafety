$(document).ready(function() {

	/*********** On Page Load - Form mods: Begin ******************/

	// populate the first three octets of the DHCP Start and End IP with the ethernet IP values
	$("form#ethernet input[name = sdhcpoct1]").val($("form#ethernet input[name = eipoct1]").val());
	$("form#ethernet input[name = edhcpoct1]").val($("form#ethernet input[name = eipoct1]").val());
	$("form#ethernet input[name = sdhcpoct2]").val($("form#ethernet input[name = eipoct2]").val());
	$("form#ethernet input[name = edhcpoct2]").val($("form#ethernet input[name = eipoct2]").val());
	$("form#ethernet input[name = sdhcpoct3]").val($("form#ethernet input[name = eipoct3]").val());
	$("form#ethernet input[name = edhcpoct3]").val($("form#ethernet input[name = eipoct3]").val());

	disableCIPInput("ethernet");

	// Disabled the DHCP fields if DHCP server is Disabled
	if ($("form#ethernet input[name = dhcpserver]:checked").val() == 'Disabled') {
		disableDHCPsettings("ethernet");
	} else {
		enableDHCPsettings("ethernet");
	}

	// Disable the Routing IP Address fields if UseDefaultRoute is not 'secondary'
	if ($("form#ethernet input[name = ctlRouteOverride]:checked").val() != 'secondary') {
		disableRouteIPsettings("ethernet");
	} else {
		enableRouteIPsettings("ethernet");
	}

	/*********** On Page Load - Form mods: End ******************/

	/*********** Bind Event handlers for form elements: Begin ******************/

	// When the DHCP is enabled or disabled
	$("form#ethernet input[name = dhcpserver]").change(function() {
		var selection = $(this).val();

		if (selection == 'Disabled') {
			disableDHCPsettings("ethernet");
			validateIP("eip");
		} else {
			enableDHCPsettings("ethernet");
			validateIP("eip");
			validateDHCPSettings("ethernet");
		}
	});

	// When the DefaultRoute is enabled or disabled
	$("form#ethernet input[name = ctlRouteOverride]").change(function() {
		var selection = $(this).val();

		if (selection == 'none' || selection == 'primary') {
			disableRouteIPsettings("ethernet");
		} else {
			enableRouteIPsettings("ethernet");
		}
	});

	// When the first IP octet of the Ethernet IP changes, update the first octet of the DHCP Start and End IPs
	$("form#ethernet input[name = 'eipoct1']").change(function() {

		$("form#ethernet input[name = 'sdhcpoct1']").val($("form#ethernet input[name = 'eipoct1']").val());
		$("form#ethernet input[name = 'edhcpoct1']").val($("form#ethernet input[name = 'eipoct1']").val());
		validateDHCPSettings("ethernet");

	});

	// When the second IP octet of the Ethernet IP changes, update the second octet of the DHCP Start and End IPs
	$("form#ethernet input[name = 'eipoct2']").change(function() {

		$("form#ethernet input[name = 'sdhcpoct2']").val($("form#ethernet input[name = 'eipoct2']").val());
		$("form#ethernet input[name = 'edhcpoct2']").val($("form#ethernet input[name = 'eipoct2']").val());
		validateDHCPSettings("ethernet");
	});

	// When the third IP octet of the Ethernet IP changes, update the third octet of the DHCP Start and End IPs
	$("form#ethernet input[name = 'eipoct3']").change(function() {

		$("form#ethernet input[name = 'sdhcpoct3']").val($("form#ethernet input[name = 'eipoct3']").val());
		$("form#ethernet input[name = 'edhcpoct3']").val($("form#ethernet input[name = 'eipoct3']").val());
		validateDHCPSettings("ethernet");
	});

	// When the fourth octet of the Ethernet IP changes, check that there is no conflict with the DHCP IP range
	$("form#ethernet input[name = eipoct4]").change(function() {

		validateDHCPSettings("ethernet");

	});

	// When the DHCP Start/End IP change, validate the settings
	$("form#ethernet input[name = 'sdhcpoct4'], form#ethernet input[name = 'edhcpoct4']").change(function() {

		validateIP("eip");
		validateDHCPSettings("ethernet");

	});

	$("form#ethernet input, form#ethernet select").on("change keyup", validateEthernet);
	validateEthernet();
	/*********** Bind Event handlers for form elements: End ******************/

});
// END DOC READY

function validateEthernet() {
	$("form#ethernet").find('.errorMsg').empty();
	var enable = true;
	enable &= hasValidIP("eip");
	enable &= hasValidIP("esm");

	enable &= ($("#sdhcpip input").prop('disabled')) ? true : hasValidIP("sdhcpip");
	enable &= ($("#edhcpip input").prop("disabled")) ? true : hasValidIP("edhcpip");

	enable &= ($("#routeip input").prop("disabled")) ? true : hasValidIP('routeip');

	enable &= ($("#sdhcpip input").prop('disabled')) ? true: validateDHCPSettings("ethernet");

	$("form#ethernet button[type='submit']").prop('disabled', !enable);
}

