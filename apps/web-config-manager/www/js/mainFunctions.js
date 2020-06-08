$(document).ready(function()
{
	var lastChecked = 0;

	/******** TABS Functionality: Begin **********************/
	  // Activates the appropriate jquery tab and sets a cookie to return the user to same tab after a page submission/re-load

	  // Device Configuration Tab: Settings sub-tabs
	  $("#settings").tabs({
		  activate: function( event, ui ) {
			  $.cookie("tabs3_settings_selected", $("#settings").tabs("option","active"));
		  },
		  active: $("#settings").tabs({ active: $.cookie("tabs3_settings_selected") })
	  });

	  // Network Configuration Tab: WiFi sub-tabs
	  $("#wifi").tabs({
		  activate: function( event, ui ) {
			  $.cookie("tabs3_wifi_selected", $("#wifi").tabs("option","active"));
		  },
		  active: $("#wifi").tabs({ active: $.cookie("tabs3_wifi_selected") })
	  });

	// Network Configuration Tab: VPN sub-tabs
	  $("#vpn").tabs({
		  activate: function( event, ui ) {
			  $.cookie("tabs3_vpn_selected", $("#vpn").tabs("option","active"));
		  },
		  active: $("#vpn").tabs({ active: $.cookie("tabs3_vpn_selected") })
	  });

	  $( ".level3tabs" ).addClass( "ui-tabs-vertical ui-helper-clearfix" );

	  $( ".level3tabs li" ).removeClass( "ui-corner-top" ).addClass( "ui-corner-left" );

	  //remove all js tab cookies before logging out
	  $("#logout").click(function(e){
		e.preventDefault();

		$.removeCookie('tabs3_fob_selected');
		$.removeCookie('tabs3_wifi_selected');
		$.removeCookie('tabs3_vpn_selected');

		window.location = '/logout.php';
	  });
	/******** TABS Functionality: End **********************/

	/******** General Page controls: Start ****************/
	$(".page_refresh").click(function(e){
		e.preventDefault();

		window.location.replace('http://'+window.location.hostname + window.location.pathname);
	});

	$('div.expandIcon').hide();
	$('div.collapseIcon').show();
	$('div.expand > h4').click(function() {

		  var $nextDiv = $(this).next();

		  $expClass = $(this).children('div:visible');

		  if ($expClass.attr("class") == "expandIcon")
		  {
			  $expClass.addClass('collapseIcon');
			  $expClass.removeClass('expandIcon');
			  $nextDiv.slideDown('fast');
		  }
		  else if ($expClass.attr("class") == "collapseIcon")
		  {
			  $expClass.addClass('expandIcon');
			  $expClass.removeClass('collapseIcon');
			  $nextDiv.slideUp('fast');
		  }

	 });
	/******** General Page controls: End ****************/

	/******** Form controls: Begin **********************/
	//Code for success/failure/warning messages
	if($(".msgBox").html())
	{
	  if($(".msgBox").html().length > 1)
	  {
		  $(".msgBox").show();
	  }
	}

	//Code for removing the submit message box on form input change
	// This works for text fields but not for radio buttons or menus... need to figure out why before releasing --ST
	/*$("form").find("input, select").bind('input', function(){

		$(".msgBox").hide('slow').empty();

	 });*/

	//Code for cancel button - clears all the error message fields from form
	$("button[type='reset']").click(function(){
		event.preventDefault();
		$(this).parents("form")[0].reset();
		$(this).parents("form").find("input, select").each(function()
		{
			if($(this).attr('type') == 'radio')
			{
				if($(this).prop("checked"))
				{
					$(this).change();
				}
			}
			else
			{
				$(this).change();
			}
		});
		$(this).parents("form").find('.errorMsg').empty();
	});

	// Show modal message on form submission
	$("form").not("#login").submit(function(){
		displaySavingMessage();
	});

	// Code used to automatically advance the cursor to the next field
	$(".autotab").bind('input', function(){
		if(this.value.length == $(this).attr('maxlength'))
		{
			$(this).next().focus();
		 }
	});

	//disable sync button
	$(".button4-link").attr("disabled",true);

	//disabled form elements on Accumulators tab
	$('#accumulatorsdiv input').attr('disabled', true);
	$('#accumulatorsdiv button').attr('disabled', true);
	$('#accumulatorsdiv select').attr('disabled', true);
	$('#accumulatorsdiv').addClass('disabledivcolor');

	$('#modulesdiv input').attr('disabled', true);
	$('#modulesdiv button').attr('disabled', true);
	$('#modulesdiv select').attr('disabled', true);
	$('#modulesdiv').addClass('disabledivcolor');

	$('#fobdiv input').attr('disabled', true);
	$('#fobdiv button').attr('disabled', true);
	$('#fobdiv select').attr('disabled', true);
	$('#fobdiv').addClass('disabledivcolor');

	//disabled tabs for network config

	$('#pp2pdiv input').attr('disabled', true);
	$('#pp2pdiv button').attr('disabled', true);
	$('#pp2pdiv select').attr('disabled', true);
	$('#pp2pdiv').addClass('disabledivcolor');

	$('#lt2pdiv input').attr('disabled', true);
	$('#lt2pdiv button').attr('disabled', true);
	$('#lt2pdiv select').attr('disabled', true);
	$('#lt2pdiv').addClass('disabledivcolor');

	$('#openvpndiv input').attr('disabled', true);
	$('#openvpndiv button').attr('disabled', true);
	$('#openvpndiv select').attr('disabled', true);
	$('#openvpndiv').addClass('disabledivcolor');

	//disable asset info tab

	$('#asseti input').attr('disabled', true);
	$('#asseti button').attr('disabled', true);
	$('#asseti select').attr('disabled', true);
	$('#asseti').addClass('disabledivcolor');

	//disable network diagnostics tabs

	$('#networkd input').attr('disabled', true);
	$('#networkd button').attr('disabled', true);
	$('#networkd textarea').attr('disabled', true);
	$('#networkd').addClass('disabledivcolor');
	/******** Form controls: End **********************/

	/******** Bind Events for common functions: Start ****************/
	$("span.ip > input").change(function(){

		var id = $(this).parent("span").attr("id");

		if(validateIP(id))	//first validate the IP
		{
			// then check for a conflict with the IP that is currently used to acccess the system
			if(Math.round(Date.now()/1000) - lastChecked > 60)		// only do a conflict check if one hasn't been done in the last minute. checking too often =>  annoying to the user
			{
				checkConnectionConflict(id);
				lastChecked = Math.round(Date.now()/1000);
			}
		}

	});

	// script to validate empty fields
	$("span.reg > input").change(function(){

		validateEmptyFields($(this));

	});

	$("span.reg > input").keyup(function(){

		validateEmptyFields($(this));

	});

	// script to check if number is less than 0
	$("span.lzero > input").change(function(){

		lessThanZeroCheck($(this));

	});

	$("span.lzero > input").keyup(function(){

		lessThanZeroCheck($(this));

	});

	// script to check if number is less than 1
	$("span.lone > input").change(function(){

		lessThanOneCheck($(this));

	});

	$("span.lone > input").keyup(function(){

		lessThanOneCheck($(this));

	});

	// script to check if number is less than 0 or greater than 6
	$("span.velocityChange > input").change(function(){
		lZeroOrGSixCheck($(this));
	});

	$("span.velocityChange > input").keyup(function(){
		lZeroOrGSixCheck($(this));
	});

	// script to check if number is less than 0, 1, 2, or greater than 180
	$("span.degreeChange > input").change(function(){
		lZeroOneToFourOrG30Check($(this));
	});

	$("span.degreeChange > input").keyup(function(){
		lZeroOneToFourOrG30Check($(this));
	});

	// script to check if number is less than 0, 1, 2, or greater than 180
	$("span.numCheck > input").change(function(){
		numCheck($(this));
	});

	$("span.numCheck > input").keyup(function(){
		numCheck($(this));
	});

	//script to check if number is hexidecimal
	$("span.hexCheck > input").change(function(){
		hexDecNumberCheck($(this));
	});
	$("span.hexCheck > input").keyup(function(){
		hexDecNumberCheck($(this));
	});
	/******** Bind Events for common functions: End ****************/

}); // END DOC READY

/******** Common functions: Start ****************/
function validateEmptyFields($field)
{
	var $emptyFields =  $field.val().replace(/\s+/g, '');

	if ($emptyFields.length>0)
	{
		$field.siblings(".errorMsg").empty();
		enableStatusSavebtn($field.parents("form").attr("id"));
	}
	else
	{
		$field.siblings(".errorMsg").text("Field required");
		enableStatusSavebtn($field.parents("form").attr("id"));
	}
} //end validateEmptyFields

function numCheck($field)
{
	if($field.prop("disabled"))
	{
		return;
	}
	var texttoNum = $.isNumeric($field.val().replace(/\s+/g, ''));

	if (!texttoNum)
	{
		$field.siblings(".errorMsg").text("Number required");
		enableStatusSavebtn($field.parents("form").attr("id"));
	}

} //end lessThanZeroCheck

function lessThanZeroCheck($field)
{
	if($field.prop("disabled"))
	{
		return;
	}
	var texttoNum = $.isNumeric($field.val().replace(/\s+/g, ''));

	if (!texttoNum)
	{
		$field.siblings(".errorMsg").text("Number required");
		enableStatusSavebtn($field.parents("form").attr("id"));
	}

	else if (parseInt($field.val()) < 0)
	{
		$field.siblings(".errorMsg").text("Entry must be greater than or equal to 0");
		enableStatusSavebtn($field.parents("form").attr("id"));

	}

} //end lessThanZeroCheck

function lZeroOrGSixCheck($field)
{
	if($field.prop("disabled"))
	{
		return;
	}
	var texttoNum = $.isNumeric($field.val().replace(/\s+/g, ''));

	if (!texttoNum)
	{
		$field.siblings(".errorMsg").text("Number required");
		enableStatusSavebtn($field.parents("form").attr("id"));
	}

	else if (parseInt($field.val()) < 0 || parseInt($field.val()) > 6)
	{
		$field.siblings(".errorMsg").text("Entry must be between 0 and 6 (inclusive)");
		enableStatusSavebtn($field.parents("form").attr("id"));
	}

} //end lessThanZeroCheck

function lessThanOneCheck($field)
{
	if($field.prop("disabled"))
	{
		return;
	}

	var texttoNum = $.isNumeric($field.val().replace(/\s+/g, ''));

	if (!texttoNum)
	{
		$field.siblings(".errorMsg").text("Number required");
		enableStatusSavebtn($field.parents("form").attr("id"));
	}

	else if (parseInt($field.val()) < 1)
	{
		$field.siblings(".errorMsg").text("Entry must be greater than 1");
		enableStatusSavebtn($field.parents("form").attr("id"));

	}

} //end lessThanOneCheck

function lZeroOneToFourOrG30Check($field)
{
	if($field.prop("disabled"))
	{
		return;
	}
	var texttoNum = $.isNumeric($field.val().replace(/\s+/g, ''));

	if (!texttoNum)
	{
		$field.siblings(".errorMsg").text("Number required");
		enableStatusSavebtn($field.parents("form").attr("id"));
	}

	else if (parseInt($field.val()) < 0 || parseInt($field.val())==1 || parseInt($field.val())==2 || parseInt($field.val())==3 || parseInt($field.val())==4 || parseInt($field.val()) > 30)
	{
		$field.siblings(".errorMsg").text("Entry must be 0 or between 5 and 30");
		enableStatusSavebtn($field.parents("form").attr("id"));

	}

} //end lZeroOneTwoOrG180Check

function hexDecNumberCheck($field)
{
	if($field.prop("disabled"))
	{
		return;
	}
	var regHex = /^0x[a-fA-F0-9]+$/;
	var value = $field.val().replace(/\s+/g, '');

	if(!regHex.test(value) && !$.isNumeric(value))
	{
		$field.siblings(".errorMsg").text("Hexidecimal(0x[0-9,A-F]) or decimal([0-9,A-F]) number required. ");
		enableStatusSavebtn($field.parents("form").attr("id"));
	}
}
/* ## FIXME:
 * Need to change/remove enableStatusSavebtn from mainFunctions. Function should either
 * get information from entire form before changing state of save button or the functionality
 * should be separated to each handle each form separately.
 */
function enableStatusSavebtn($id)
{
	var noError = true;
	$("form#" + $id + " .errorMsg").each(function(){
		if($(this).text().length > 0)
		{
			noError = false;
		}
});
		if (noError == true)
		{
			$("form#" + $id + " button[type='submit']").removeAttr('disabled');
		}
		else
		{
			$("form#" + $id + " button[type='submit']").attr('disabled','disabled');
		}
}

/* Builds an IP string using the latest values of the $id > child fields. */
function build($id)
{
	var valueString = "";

	$("#" + $id).find("input").each(function(index){
		valueString = valueString + $.trim($(this).val());

	    if (index < 3)
	    {
	    	valueString = valueString + ".";
	    }

	});

	return valueString;
}//end build

/* Builds an IP string using the original values of the $id > child fields. i.e. before they were changed by the user */
function buildPreChangeIP($id)
{
	var valueString = "";

	$("#" + $id).find("input").each(function(index){
		valueString = valueString + $(this).attr('value');

	    if (index < 3)
	    {
	    	valueString = valueString + ".";
	    }

	});

	return valueString;
}//end buildPreChangeIP

function validateIP($id)
{
	var buildString = build($id);
	var pattern = /^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$/g;
	var bValidIP = pattern.test(buildString);
	if (bValidIP == false)
	{
		$("#" + $id + " > .errorMsg").text("Invalid IP Address");
		enableStatusSavebtn($("#" + $id).parents("form").attr("id"));
		return false;
	}

	else if($id == 'eip' || $id == 'wip')			//only do this for etherne and wifi interfaces
	{
		//if valid ip, check for conflict with other interface
		//$("#" + $id + " > .errorMsg").empty();
		//enableStatusSavebtn($("#" + $id).parents("form").attr("id"));

		 if(checkIpConflict($("input[name=interface_other]").val(), $id))
		 { }
		 else
		 {
			 $("#" + $id + " > .errorMsg").empty();
			 enableStatusSavebtn($("#" + $id).parents("form").attr("id"));
			 return true;
		 }
	}
	else
	{
		$("#" + $id + " > .errorMsg").empty();
		enableStatusSavebtn($("#" + $id).parents("form").attr("id"));
		return true;
	}
}// end validateIP

/* Checks if the IP address in the field, identified by interfaceId, matches the IP in the browser's address bar
 * If they do match => the LCM is being used via this IP. Alert the user about this.
 */
function checkConnectionConflict(interfaceId)
{
	//alert(interfaceId);
	var connectionIP = window.location.hostname;
	var interfaceIP =  buildPreChangeIP(interfaceId);		//gets the value in the IP address field, before it was changed by the user

	//alert("connectionIP = " + connectionIP);		//DEBUG
	//alert("interfaceIP = " + interfaceIP);		//DEBUG

	var conflictCount = compareIPAddresses(connectionIP, interfaceIP, 4);	//# of matching octets between the two ip addresses
	//alert(conflictCount);				//DEBUG

	if(conflictCount > 3)
	{
		if(compareIPAddresses(connectionIP, build(interfaceId), 4) > 3){}	// this is done to ensure that changing the IP back to the original will not trigger a warning message.
		else {
			//displayModalMessage("<h3>CAUTION!</h3><p>Changing the IP address will break your connection to the device.</p><p>You will need to reboot the device and access it using the new IP address: " + build(interfaceId) + "</p><input type=\"button\" id=\"ackModal\" value=\"OK\" />");
			displayModalMessage("<h3>CAUTION!</h3><p>The device will not be accessible at " + connectionIP + " (after a reboot). Use " + build(interfaceId) + " instead.</p><input type=\"button\" id=\"ackModal\" value=\"OK\" />");
			//$("#" + interfaceId + " .warningMsg").text("The device will not be accessible at " + connectionIP + " (after a reboot). Use " + build(interfaceId) + " instead.").show();
		}
	}
}

/* Compares and returns the number of matching octets between firstIP string and secondIP string.
 * octetsToCheck specifies the number of octets to compare
 */
function compareIPAddresses(firstIP, secondIP, octetsToCheck)
{
	var conflictCount = -1;
	//alert("firstIP = " + firstIP);		//DEBUG
	//alert("secondIP = " + secondIP);		//DEBUG

	if(firstIP != null && secondIP != null)
	{
		var firstIPArray = firstIP.split(".");
		var secondIPArray = secondIP.split(".");
		conflictCount = 0;		//tracks the number of matching octets among the two sets of IP addresses

		for(var indx = 0; indx < octetsToCheck; indx++)
		{
			//alert("firstIPArray["+indx+"] = " + firstIPArray[indx]);	//DEBUG
			//alert("secondIPArray["+indx+"] = " + secondIPArray[indx]);	//DEBUG

			if(firstIPArray[indx] == secondIPArray[indx])
			{ conflictCount++; }
		}
	}

	return conflictCount;
}

/*
 * Disable the Starting and Ending DHCP IP fields
 * Empty and hide the DHCP error fields
 */
function disableDHCPsettings(formId)
{
    $("form#" + formId + " input[name ^= 'sdhcpoct']").attr('disabled', true);
    $("form#" + formId + " input[name ^= 'edhcpoct']").attr('disabled', true);
    $("form#" + formId + " span[name ^= 'errorDHCPSIP']").empty();
    $("form#" + formId + " span[name ^= 'errorDHCPEIP']").empty();
    enableStatusSavebtn(formId);
}

function disableCIPInput(formId)
{
    $("form#" + formId + " input[name ^= 'cipoct1']").attr('disabled', true);
    $("form#" + formId + " input[name ^= 'cipoct2']").attr('disabled', true);
    $("form#" + formId + " input[name ^= 'cipoct3']").attr('disabled', true);
    $("form#" + formId + " input[name ^= 'cipoct4']").attr('disabled', true);
    enableStatusSavebtn(formId);
}

/*
 * Enable the Starting and Ending DHCP IP fields
 */

function enableDHCPsettings(formId)
{
    $("form#" + formId + " input[name ^= 'sdhcpoct']").removeAttr('disabled');
    $("form#" + formId + " input[name ^= 'edhcpoct']").removeAttr('disabled');
    enableStatusSavebtn(formId);
}

function validateDHCPSettings(formId)
{
	if($("form#" + formId + " input[name = sdhcpoct4]:enabled").val() && $("form#" + formId + " input[name = edhcpoct4]:enabled").val())
	{
		// Validate DHCP Starting and Ending IP
		if(validateIP("sdhcpip") && validateIP("edhcpip"))
		{
			// Check that start IP < end IP
			if (parseInt($("form#" + formId + " input[name = sdhcpoct4]").val()) > parseInt($("form#" + formId + " input[name = edhcpoct4]").val()))
			{
				$("form#" + formId + " span[name = errorDHCPSIP]").text("Starting DHCP IP must not be greater than the ending DHCP IP.").show();
				enableStatusSavebtn(formId);
				return false;
			}

			if(formId == "ethernet")
			{
				// Check that the Ethernet IP is not in the DHCP range
				if (parseInt($("form#" + formId + " input[name = eipoct4]").val()) >= parseInt($("form#" + formId + " input[name = sdhcpoct4]").val())
						&& parseInt($("form#" + formId + " input[name = eipoct4]").val()) <= parseInt($("form#" + formId + " input[name = edhcpoct4]").val()) )
				{
					$("form#" + formId + " span[name = 'errorDHCPSIP'],form#" + formId + " span[name = 'errorEIP']").text("IP address must not be in the DHCP range.").show();
					enableStatusSavebtn(formId);
					return false;
				}
			}
			else if(formId == "wifi-ap")
			{
				// Check that the Ethernet IP is not in the DHCP range
				if (parseInt($("form#" + formId + " input[name = wipoct4]").val()) >= parseInt($("form#" + formId + " input[name = sdhcpoct4]").val())
						&& parseInt($("form#" + formId + " input[name = 'wipoct4']").val()) <= parseInt($("form#" + formId + " input[name = edhcpoct4]").val()) )
				{
					$("form#" + formId + " span[name = 'errorDHCPSIP'],form#" + formId + " span[name = 'errorWIP']").text("IP address must not be in the DHCP range.").show();
					enableStatusSavebtn(formId);
					return false;
				}
			}
			return true;
		}

		return false;
	}
}

function enableRouteIPsettings(formId)
{
    $("form#" + formId + " input[name ^= 'ctlRouteIP']").removeAttr('disabled');
    enableStatusSavebtn(formId);
}
function disableRouteIPsettings(formId)
{
		$("form#" + formId + " input[name ^= 'ctlRouteIP']").attr('disabled', true);
    enableStatusSavebtn(formId);
}



function isOn($val)
{
	if($val !== undefined && ($val.toUpperCase() == "ON" || $val === 1 || $val === "1"))
	{
		return true;
	}
	else
	{
		 return false;
	}
}

function isOff($val)
{
	if($val !== undefined && ($val.toUpperCase() == "OFF" || $val === 0 || $val === "0"))
	{
		return true;
	}
	else
	{
		 return false;
	}
}

function displaySavingMessage()
{
	  $.blockUI({
		  message: '<h3>Saving settings. Please wait...</h3>',
		  baseZ: 10000,
		  css: { padding: '15px' }
		});
}

/* Uniform way to display messages in a modal/pop-up box */
function displayModalMessage($message)
{
	  $.blockUI({
		  message: $message,
		  baseZ: 10000,
		  css: { padding: '15px' }
		});

	  $('#ackModal').click(function() {
          $.unblockUI();
      });
}

/**
 * updateFields
 *
 * Disable/enables fields if setting is OFF/ON.
 * @param string $p_form_id - ID of containing FORM tag
 * @param string $p_section_id - ID of containing section DIV
 * @param string $p_input_name - Name of INPUT containing setting
 * @param function $p_on_enable - Optional callback which is called before enabling (function must return true to continue with enable)
 * @param function $p_on_disable - Optional callback which is called before disabling (function must return true to continue with disable)
 * @param mixed $p_data - Optional callback data. The same data is passed to both p_on_enable and p_on_disable
 * @return string - The value of the INPUT setting
 * @author Amour Hassan (ahassan@gps1.com)
 */
function updateFields(p_form_id, p_section_id, p_input_name, p_on_enable, p_on_disable, p_data)
{
	var state = $('form#' + p_form_id + ' input[name = ' + p_input_name + ']:checked').val();

	if(isOff(state))
	{

		if((p_on_disable === undefined || p_on_disable(p_data)))
		{
			disableFields(p_section_id, p_input_name);
		}

	}
	else
	{

		if((p_on_enable === undefined || p_on_enable(p_data)))
		{
			enableFields(p_section_id, p_input_name);
		}

	}

	return state;
}

/**
 * initializeFields
 *
 * Disable/eanbles fields if setting is OFF/ON and attaches a field monitor. Fields will enable/disable following user input.
 * @param string $p_form_id - ID of containing FORM tag
 * @param string $p_section_id - ID of containing section DIV
 * @param string $p_input_name - Name of INPUT containing setting
 * @param string $p_child_section_id - ID of containing child section DIV (for recursive enable/disable)
 * @param string $p_child_input_name - Name of child INPUT containing setting
 * @param function $p_on_enable - Optional callback which is called before enabling (function must return true to continue with enable)
 * @param function $p_on_disable - Optional callback which is called before disabling (function must return true to continue with disable)
 * @param mixed $p_data - Optional callback data. The same data is passed to both p_on_enable and p_on_disable
 * @return string - The value of the INPUT setting
 * @author Amour Hassan (ahassan@gps1.com)
 */
function initializeFields(p_form_id, p_section_id, p_input_name, p_child_section_id, p_child_input_name, p_on_enable, p_on_disable, p_data)
{
	var $state = updateFields(p_form_id, p_section_id, p_input_name, p_on_enable, p_on_disable, p_data);
	attachFieldStateMonitor(p_section_id, p_input_name, p_form_id, p_child_section_id, p_child_input_name, p_on_enable, p_on_disable, p_data);
	return $state;
}

/**
 * attachFieldStateMonitor
 *
 * Attaches a "change" event monitor so that fields can be enabled/disabled on user input.
 * When the parent field is disabled, the child field is always disabled. When the parent field is enabled, then the child will
 * determine if child fields are enabled or disabled.
 * @param string $p_form_id - ID of containing FORM tag
 * @param string $p_section_id - ID of containing section DIV
 * @param string $p_input_name - Name of INPUT containing setting
 * @param string $p_child_section_id - Optional ID of containing child section DIV (for recursive enable/disable)
 * @param string $p_child_input_name - Optional name of child INPUT containing setting
 * @param function $p_on_enable - Optional callback which is called before enabling for child sections(function must return true to continue with enable)
 * @param function $p_on_disable - Optional callback which is called before disabling for child sections(function must return true to continue with disable)
 * @param mixed $p_data - Optional callback data. The same data is passed to both p_on_enable and p_on_disable
 * @return none
 * @author Amour Hassan (ahassan@gps1.com)
 */
function attachFieldStateMonitor(p_section_id, p_input_name, p_form_id, p_child_id, p_child_name, p_on_enable, p_on_disable, p_data)
{
	$('input[name = ' + p_input_name + ']').change(function(){

		if(isOff($(this).val()))
		{
			disableFields(p_section_id, p_input_name);

			if(p_on_disable !== undefined)
			{
				p_on_disable(p_data);
			}
			$('#' + p_form_id).find(".button2-link").prop("enabled", true);
		}
		else
		{
			enableFields(p_section_id, p_input_name);

			if((undefined !== p_child_id) && (undefined !== p_child_name))
			{
				if(Array.isArray(p_child_id))
				{
					for(var i=0;i < p_child_id.length;++i)
					{
						updateFields(p_form_id, p_child_id[i], p_child_name[i], p_on_enable, p_on_disable, p_data);
					}
				}
				else
				{
					updateFields(p_form_id, p_child_id, p_child_name, p_on_enable, p_on_disable, p_data);
				}
			}

		}

	});
}

/**
 * disableFields
 *
 * Disables all fields for the given input section.
 * @param string $p_section_id - ID of containing section DIV
 * @param string $p_input_name - Name of INPUT containing setting
 * @return none
 * @author Amour Hassan (ahassan@gps1.com)
 */
function disableFields(p_section_id, p_input_name)
{
	$('#' + p_section_id).find('.slider').slider("disable");
	$('#' + p_section_id).find('input, select').not('input[name = ' + p_input_name + ']').attr('disabled','disabled');
}

/**
 * enableFields
 *
 * Enables all fields for the given input section.
 * @param string $p_section_id - ID of containing section DIV
 * @param string $p_input_name - Name of INPUT containing setting
 * @return none
 * @author Amour Hassan (ahassan@gps1.com)
 */
function enableFields(p_section_id, p_input_name)
{
	$('#' + p_section_id).find('.slider').slider("enable");
	$('#' + p_section_id).find('input, select').not('input[name = ' + p_input_name + ']').removeAttr('disabled');
}

function isNotEmpty(p_element)
{
	if(p_element.val().trim() != '')
	{
		return true;
	}
	p_element.siblings(".errorMsg").text("Please enter a value.");
	return false;
}

function isValidNumber(p_element)
{
	var regEx = /^\d+$/;
	if( hasValidInput(regEx, p_element.val().trim()))
	{
		return true;
	}
	p_element.siblings(".errorMsg").text("Please enter a number.");
	return false;
}

function hasValidRange(p_element, l_limit, h_limit, zeroOff)
{
	zeroOff = (typeof zeroOff !== 'undefined')? zeroOff : false;
	zeroOff = (typeof zeroOff == 'boolean')? zeroOff: false;
	if(!isValidNumber(p_element))
	{
		return false;
	}

	if(l_limit != null)
	{
		if(p_element.val() < l_limit)
		{
			if(zeroOff && (p_element.val() == 0))
			{
				return true;
			}
			else if(zeroOff && (l_limit > 0))
			{
				p_element.siblings(".errorMsg").text("Entry must be 0 or between " + l_limit + " and "+ h_limit + ".");
			}
			else
			{
				p_element.siblings(".errorMsg").text("Please enter a number greater than " + l_limit + ".");
			}
			return false;
		}
	}

	if(h_limit != null)
	{
		if(p_element.val() > h_limit)
		{
			if(zeroOff && p_element.val() == 0)
			{
				return true;
			}
			else if (zeroOff && h_limit < 0)
			{
				p_element.siblings(".errorMsg").text("Entry must be 0 or between " +l_limit + " and " + h_limit + ".");
			}
			else
			{
				p_element.siblings(".errorMsg").text("Please enter a number less than " + h_limit + ".");
			}
			return false;
		}
	}

	return true;
}

function isValidHex(p_element)
{
	var regHex = /^0x[a-fA-F0-9]+$/;
	var value = p_element.val().replace(/\s+/g, '');

	if(!hasValidInput(regHex, value) && !$.isNumeric(value))
	{
		p_element.siblings(".errorMsg").text("Hexidecimal(0x[0-9,A-F]) or decimal([0-9,A-F]) number required. ");
		return false;
	}
	return true;
}

function hasValidInput(p_regEx, p_value)
{
	return p_regEx.test(p_value);
}

function hasValidIP($id)
{
	var buildString = build($id);
	var pattern = /^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$/g;
	var bValidIP = hasValidInput(pattern, buildString);
	if (bValidIP == false)
	{
		$("#" + $id + " > .errorMsg").text("Invalid IP Address");
		return false;
	}
	return true;
}

/******** Common functions: End ****************/
