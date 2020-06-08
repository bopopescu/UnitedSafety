function validateModbusSettings()
{
	$("form#modbusSubSettings").find('.errorMsg').empty();
	var enable = true;
	enable &= ($("input[name='qDelaySeconds']").is(':disabled'))? true: isValidNumber($("input[name='qDelaySeconds']"));
	enable &= ($("input[name='periodicSeconds']").is(':disabled'))? true: isValidNumber($("input[name='periodicSeconds']"));
	enable &= ($("input[name='periodicOveriridiumMinutes']").is(':disabled'))? true: isValidNumber($("input[name='periodicOveriridiumMinutes']"));
	enable &= ($("input[name='mportdata']").is(':disabled'))? true: isValidNumber($("input[name='mportdata']"));

	var val = $('input[name="modbusMode"]:checked').val();
	if(val == "tcp")
	{
	enable &= ($("#mip input").is(':disabled'))? true: hasValidIP("mip");
	}
	$("form#modbusSubSettings button[type='submit']").prop('disabled', !enable);
}

function validateAddTemplate()
{
	var templateName = $('#templateName').val();
	var templateFile = $('input:file[name=templateFile]').val();
	var status = (($("#addModbusTemplate button.button2-link").attr("disabled")) == "disabled");
	if((templateName != "" ) && (templateFile != "") && status)
	{
		$("#addModbusTemplate button.button2-link").attr("disabled", false);
	}
	else if((!status)&&((templateName == "") || (templateFile == "")))
	{
		$("#addModbusTemplate button.button2-link").attr("disabled", true);
	}
}

function validateAssignTemplate()
{
	var slaveId = $("select[name=slave_name] option:selected").val();
	var templateId = $("select[name=template_name] option:selected").val();
	var status = (($("#assignModbusTemplate button.button2-link").attr("disabled")) == "disabled");
	if((slaveId != "*") && (templateId != "*") && status)
	{
		$("#assignModbusTemplate button.button2-link").attr("disabled", false);
	}
	else if((!status)&&((slaveId == "*") || (templateId == "*")))
	{
		$("#assignModbusTemplate button.button2-link").attr("disabled", true);
	}
}

function checkTemplateName(e)
{
	var templateName = $('#templateName').val();
	var status = (($("#addModbusTemplate button.button2-link").attr("disabled")) == "disabled");
	if(status)
	{
		e.preventDefault();
		return;
	}
	var found = false;

	$.each(g_templateNames, function(key, value)
	{
		if(value == templateName)
		{
			found = true;
			return false;
		}
	});

	if(found)
	{
		e.preventDefault();
		$("#dialog").attr("title","Add Template...");
		$("#dialog").html("<p >A template exists with the same name. Do you want to overwrite this template?</p>");
		$("#dialog").dialog({
			resizable: false,
			height:"auto",
			modal: true,
			buttons: {
				"Yes": function() {
					$("#addModbusTemplate").off("submit");
					displaySavingMessage();
					$("#addModbusTemplate").submit();
					$(this).dialog("close");
				},
				"No": function() {
					$(this).dialog("close");
				}
			}
		});
		return;
	}
	displaySavingMessage();
	return;
}
//Array to store url codes for displaying messages. These codes match the ones defined in
//config_mang.db
var g_codes = {
	'failedToSaveSettings': '11',
};

function addTemplate(templateId)
{
	var dataStr = templateId + "&assignments="+assignments;
	// submit the data via ajax
	$.ajax({
			url: 'https://'+window.location.hostname+'/inc/modbus_controller.php',
			type: 'POST',
			data: "op=addTemplate&templateId="+dataStr,
			dataType: "text",
			beforeSend:function(){

				displaySavingMessage();

			},
			success:function(result)
			{
				window.location.href = result;
			},
			error:function(jqXHR, textStatus, errorThrown)		// ajax request failed
			{
				alert(textStatus);
				window.location.href = 'https://'+window.location.hostname+'/device/modbus/index.php?success=false&codes='+g_codes['failedtoSaveSettings'];
			},
			complete:function()
			{
				$.unblockUI;
			}
	});

}

function deleteTemplate(templateId, assignments)
{
	var dataStr = templateId + "&assignments="+assignments;
	// submit the data via ajax
	$.ajax({
			url: 'https://'+window.location.hostname+'/inc/modbus_controller.php',
			type: 'POST',
			data: "op=deleteTemplate&templateId="+dataStr,
			dataType: "text",
			beforeSend:function(){
				displaySavingMessage();
			},
			success:function(result)
			{
				window.location.href = result;
			},
			error:function(jqXHR, textStatus, errorThrown)		// ajax request failed
			{
				alert(textStatus);
				window.location.href = 'https://'+window.location.hostname+'/device/modbus/index.php?success=false&codes='+g_codes['failedtoSaveSettings'];
			},
			complete:function()
			{
				$.unblockUI;
			}
	});
}
//  switchModbusMode
//    if Modbus is disabled we want both blocks (tcp and rts) fully disabled
//    if Modbus is enabled we want the selected block enabled and the other block disabled
//
function switchModbusMode()
{
	var state = $('input[name="enable"]:checked').val();

	var val = $('input[name="modbusMode"]:checked').val();
	if(val == "rtu")
	{
		$('.rtu').each(function(){
			$(this).attr('style', 'display:block');
			$(this).find('input,select').prop('disabled', isOff(state));
		});
		$('.tcp').each(function(){
			$(this).attr('style', 'display:none');
			$(this).find('input,select').prop('disabled', true);
		});
	}
	else
	{
		if(val != 'tcp')
		{
			$('input[name="modbusMode"][value="tcp"]').prop("checked",true);
		}
		$('.tcp').each(function(){
			$(this).attr('style', 'display:block');
			$(this).find('input,select').prop('disabled', isOff(state));
		});
		$('.rtu').each(function(){
			$(this).attr('style', 'display:none');
			$(this).find('input,select').prop('disabled', true);
		});
	}
}

function enableModbusElements()
{
	$("button[type='reset']").prop('disabled', false);
	$("a.deleteTemplateDisabled img").attr("src", "/images/DeleteIcon16.png");
	$("a.deleteTemplateDisabled").attr("class", "deleteTemplate");
	$("a.deleteAssignmentDisabled img").attr("src", "/images/DeleteIcon16.png");
	$("a.deleteAssignmentDisabled").attr("class", "deleteAssignment");
	validateAddTemplate();
	validateAssignTemplate();
}

function disableModbusElements()
{
	$("button ").not("form#modbusSettings button").prop('disabled', true);
	$("a.deleteTemplate img").attr("src", "/images/DisabledDeleteIcon16.png");
	$("a.deleteTemplate").attr("class", "deleteTemplateDisabled");
	$("a.deleteAssignment img").attr("src", "/images/DisabledDeleteIcon16.png");
	$("a.deleteAssignment").attr("class", "deleteAssignmentDisabled");
}

$(document).ready(function(){
	switchModbusMode();
	initializeFields("modbusSubSettings", "modbusSettingsField", "enable");
	//Bind form onchange event to AddTemplate Form
	$("#addModbusTemplate").off("submit");
	$("#addModbusTemplate").submit(function(event)
	{
		checkTemplateName(event);
	});
	$("#addModbusTemplate input").change(validateAddTemplate);
	$("#addModbusTemplate button.button3-link").click(validateAddTemplate);

	$("#assignModbusTemplate").change(validateAssignTemplate);

	// Delete icon operation
	$("a.deleteAssignment").click(function(e){
		e.preventDefault();

		if($(this).attr("class") == "deleteAssignmentDisabled")
		{
			return;
		}

		// find index of rule to be deleted
		templateIndexToDelete = $(this).closest("tr").index();	//indexes are zero-based
		var slave = $(this).closest("tr").attr("id");

		$.ajax({
			url: 'https://'+window.location.hostname+'/inc/modbus_controller.php',
			type: 'POST',
			data: "op=deleteAssignment&slaveId="+slave,
			dataType: "text",
				beforeSend:function(){

					displaySavingMessage();

				},
				success:function(result)
				{
					window.location.href = result;
				},
				error:function(jqXHR, textStatus, errorThrown)		// ajax request failed
				{
					alert(textStatus);
					window.location.href = 'https://'+window.location.hostname+'/device/modbus/index.php?success=false&codes='+g_codes['failedtoSaveSettings'];
				},
				complete:function()
				{
					$.unblockUI;
				}
		});

	});

	$("a.deleteAssignmentDisabled").click(function(e){
		e.preventDefault();
	});

	// Delete icon operation
	$("a.deleteTemplate").click(function(e){
		e.preventDefault();

		if($(this).attr("class") == "deleteTemplateDisabled")
		{
			return;
		}

		var done = true;
		var templateAssignments = '';

		// find index of rule to be deleted
		var templateIndexToDelete = $(this).closest("tr").index();	//indexes are zero-based
		var template = $(this).closest("tr").attr("id").substr(3);
		templateIndexToDelete++;								//rules in db-config are one-based
		$.each(g_templateAssignments, function(key, value)
		{
			if(template == value )
			{
				templateAssignments += key + ",";
			}
		});

		if(templateAssignments != '')
		{
			$("#dialog").attr("title","Delete Template...");
			$("#dialog").html("<p >Do you want to delete all assignments associated with this template?</p>");
			$("#dialog").dialog({
				resizable: false,
				height:"auto",
				modal: true,
				buttons: {
					"Delete": function() {
						deleteTemplate(template, templateAssignments);
						$(this).dialog("close");
					},
					"Cancel": function() {
						$(this).dialog("close");
					}
				}
			});
		}
		else
		{
			deleteTemplate(template, '');
		}
	});

	$("a.deleteTemplateDisabled").click(function(e){
		e.preventDefault();
	});

	$("#modbusSettings input[name='modbusMode']").change(switchModbusMode);
	$("#modbusSettings input[name='enable']").change(switchModbusMode);
	$("form#modbusSubSettings input,form#modbusSubSettings select").on("change keyup", validateModbusSettings);
	// validateModbusSettings();
});
