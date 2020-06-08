$(document).ready(function()
{
	$("form#cellular input, form#cellular select").on("change keyup", validateCellular);
	validateCellular();
	// updateRSSI();
});

function validateCellular()
{
	$("form#cellular").find('.errorMsg').empty();
	var enable = true;
	enable &= isNotEmpty($("input[name='apn']"));
	$("form#cellular button[type='submit']").prop('disabled', !enable);
}

function updateRSSI()
{
	setTimeout(function()
	{
		setInterval(function()		//update the rssi every 10 seconds
		{
			$.ajax
			({
			  type: 'GET',
			  dataType: "text",
			  async: true,
			  cache: false,
			  url: 'https://'+window.location.hostname+'/inc/cell_view.inc',
			  data: { op: 'RSSIupdate' },
				  
			  beforeSend:function(){},
				  
			  success:function(rssidata)
			  {
					$("#XcellRSSI").val(rssidata);
			  },
				  
			  error:function(jqXHR, textStatus, errorThrown)		// ajax request failed
			  {
					$("#XcellRSSI").val("unknown");
			  }
			});
		},10*1000); //end setInterval
	},2*1000);	//end setTimeout}
}

