$(document).ready(function() {
	$camsStatus = initializeFields('outputCheck', 'CAMSSection', 'cams', ['CAMSCompressionSection', 'CAMSIridiumSection', 'IridiumEnable'], ['camsCompress', 'IridiumEnable', 'IridiumStatus']);
	$camsCompressionStatus = initializeFields('outputCheck', 'CAMSCompressionSection', 'camsCompress');
	$camsIridiumStatus = initializeFields('outputCheck', 'CAMSIridiumSection', 'IridiumEnable');
	$trakStatus = initializeFields('outputCheck', 'TrakopolisSection', 'Trak');
	$rdsStatus = initializeFields('outputCheck', 'RDSSection', 'RDS');
	$gpsSocketServerStatus = initializeFields('socketsCheck', 'GpsSection', 'gpsSocketServer');
	$gpiMonitor = initializeFields('inputCheck', 'GpiSection', 'gpiMonitor');
	$CPCom1Enable = initializeFields('comportsCheck', 'com1Section', 'CPCom1Enable');
	$CPCom2Enable = initializeFields('comportsCheck', 'com2Section', 'CPCom2Enable');
	$IridiumEnableCtl = initializeFields('satelliteSettings', 'satellitediv', 'IridiumEnableCtl');

	$("form#outputCheck input,form#outputCheck select").on("change keyup",function()
	{
		validateProtocolForm();
	});

	$("form#socketsCheck input").on("change keyup", function(){validateSocketsForm();});
	$("form#comportsCheck input").on("change keyup", function(){validateCOMPortsForm();});
	$("form#positionCheck input").on("change keyup", function(){validatePositionUpdateForm();});
	$("form#hardwareCheck input, form#hardwareCheck select").on("change keyup", function(){validateHardwareForm();});
	validateHardwareForm();
});

function updateDataLimitPrioritySelect(max) {
	if (max === undefined) {
		return;
	}
	var value = $('input[name="camsIridiumDataLimitPriority"]').val();
	var innerHTML = '';
	for (var i; i < max; ++i) {
		innerHTML += '<option value="' + i + '" ' + ((value == i) ? 'selected="selected"' : '') + ' >' + i + '</option>\n';
		//<option value=\"$i\" ". (($i == $camsIridiumDataLimitPriority)? 'selected="selected"': '') . " >$i </option>";
	}
}

function validatePositionUpdateForm()
{
	$("form#positionCheck").find(".errorMsg").empty();
	var enable = true;
	enable &= isValidNumber($("input[name='positionUpdateTime']"));
	enable &= isValidNumber($("input[name='positionUpdateDistance']"));
	enable &= hasValidRange($("input[name='positionHeading']"),5,30,true);
	enable &= hasValidRange($("input[name='positionStopVelocity']"),0,6,false);
	enable &= isValidNumber($("input[name='positionStopTime']"));
	enable &= isValidNumber($("input[name='IridiumUpdateIntervalCtl']"));
	$("form#positionCheck button[type='submit']").prop('disabled', !enable);
}

function validateProtocolForm()
{
	$("form#outputCheck").find(".errorMsg").empty();

	var enable = true;

	enable &= ($("input[name='camsHost']").is(':disabled'))? true: isNotEmpty($("input[name='camsHost']"));
	enable &= ($("input[name='camsPort']").is(':disabled'))? true: isValidNumber($("input[name='camsPort']"));
	enable &= ($("input[name='camsIridiumTimeout']").is(':disabled'))? true: isValidNumber($("input[name='camsIridiumTimeout']"));
	enable &= ($("input[name='camsKeepAlive']").is(':disabled'))? true: isValidNumber($("input[name='camsKeepAlive']"));
	enable &= ($("input[name='camsTimeout']").is(':disabled'))? true: isValidNumber($("input[name='camsTimeout']"));
	$("form#outputCheck button[type='submit']").prop('disabled', !enable);
}

function validateSocketsForm()
{
	$("form#socketsCheck").find(".errorMsg").empty();
	var enable = true;

	enable &= ($("input[name='gpsSocketServerPort']").is(':disabled'))? true: isValidNumber($("input[name='gpsSocketServerPort']"));
	$("form#socketsCheck button[type='submit']").prop('disabled', !enable);

}

function validateWakeupForm()
{
	$("form#wakeupCheck").find(".errorMsg").empty();
	var enable = true;
}

function validateHardwareForm()
{
	$("form#hardwareCheck").find(".errorMsg").empty();
	var enable = true;
	$("form#hardwareCheck button[type='submit']").prop('disabled', !enable);
}

function validateCOMPortsForm()
{
	$("form#comportsCheck").find(".errorMsg").empty();
	var enable = true;

	enable &= ($("input[name='CPCom1Baud']").is(':disabled'))? true: hasValidRange($("input[name='CPCom1Baud']"),1200, 115200,false);
	enable &= ($("input[name='CPCom1Port']").is(':disabled'))? true: hasValidRange($("input[name='CPCom1Port']"),1, 65000, false);
	enable &= ($("input[name='CPCom2Baud']").is(':disabled'))? true: hasValidRange($("input[name='CPCom2Baud']"),1200, 115200,false);
	enable &= ($("input[name='CPCom2Port']").is(':disabled'))? true: hasValidRange($("input[name='CPCom2Port']"),1, 65000, false);

	$("form#comportsCheck button[type='submit']").prop('disabled', !enable);
}
