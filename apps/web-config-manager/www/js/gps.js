/**
 * This script performs the ajax call that gets gps data from the device every x seconds and updates the page.
 * @author Sean Toscano (sean@absolutetrac.com)
 */
$(document).ready(function()
{
	//Configuration parameters
	var delay = 1;		//seconds before gps reporting begins
	var interval = 2; 	//seconds between gps update requests
	if(g_update_gps)
	{
		setTimeout(function(){			//delay the gps update by 2 seconds - this is to give the user time to read the original values on the page

			//removing the updating message as per Dave and Jim's request.(ticket #740, #743)
			//$(".msgBox").html('<div class="gpsUpdate"><div class="warningImage"></div><div class="warningMsg">Updating current GPS information&nbsp;&nbsp;<img style="margin-top:-3px;" src="/images/loading.gif" alt="Loading..." /></div></div>').show();	//show the loading graphic

			setInterval(function(){		//update the gps every 2 seconds

				$.ajax({
				  type: 'GET',
				  dataType: "text",		//changed this from xml to text to avoid any issues parsing xml.
				  async: true,
				  cache: false,
				  url: 'https://'+window.location.hostname+'/inc/gps_view.php',
				  data: { op: 'gpsupdate' },
				  beforeSend:function(){
					  //$(".msgBox .gpsFailed").remove();
				  },
				  success:function(gpsdata){

					 gpsdata.trim();
					 //alert(gpsdata.length);
					 updateGpsInfo(gpsdata);

				  },
				  error:function(jqXHR, textStatus, errorThrown)		// ajax request failed
				  {
					  //alert(textStatus);
					  updateGpsInfo(null);

				  }
				});
			},interval*1000); //end setInterval
		},delay*1000);	//end setTimeout
	}

	function updateGpsInfo(gps)
	{
		var time = satellites = latitude = longitude = elevation = heading = hdop = quality = velocity = '--';

		if(gps === null || gps.length === 1)	//if no data is returned by the server/device
		{
			displayGpsFailureMsg();
		}
		else
		{
			$(".msgBox").hide();
			$("select[name=gpsType]").next("span.errorMsg").hide();

			var gpsdata = $.parseHTML(gps);	//parses the text and adds it to the DOM for querying

			time = $(gpsdata).find('time').text();
			satellites = $(gpsdata).find('satellites').text();
			latitude = $(gpsdata).find('latitude').text().replace("&deg;", "\u00B0").replace("&prime;", "\u2032").replace("&Prime;", "\u2033");
			longitude = $(gpsdata).find('longitude').text().replace("&deg;", "\u00B0").replace("&prime;", "\u2032").replace("&Prime;", "\u2033");
			elevation = $(gpsdata).find('elevation').text();
			heading = $(gpsdata).find('heading').text().replace("&deg;", "\u00B0");
			hdop = $(gpsdata).find('hdop').text();
			quality = $(gpsdata).find('quality').text();
			velocity = $(gpsdata).find('velocity').text();
		}

		$("#gpsTime").val(time);
		$("#satellites").val(satellites);
		$("#latitude").val(latitude);
		$("#longitude").val(longitude);
		$("#elevation").val(elevation);
		$("#heading").val(heading);
		$("#hdop").val(hdop);
		$("#quality").val(quality);
		$("#velocity").val(velocity);

	}
	function displayGpsFailureMsg()
	{
		if(!$(".msgBox .gpsFailed").length)
		{
			$(".msgBox").append('<div class="gpsFailed"><div class="failImage"></div><div class="failMsg">Failed to update GPS information.</div><div style="clear:both;"></div></div>');
			$(".msgBox").show();
		}

		//$("select[name=gpsOption]").next("span.errorMsg").show(); 	//this will need to be added back in when we support multiple gps types i.e. when the external gps is implemented

	}


//	$('input[name="gpsReporting"]').change(displayLatLongFields);
//	displayLatLongFields();

}); //END doc ready()
function displayLatLongFields()
	{
		var val = $('input[name="gpsReporting"]:checked').val();
		if(val == "Fixed")
		{
			$('.gpsLive').each(function(){
				$(this).attr('style', 'display:none');
			});
			$('.gpsFixed').each(function(){
				$(this).attr('style', 'display:block');
			});
		}
		else
		{
			$('.gpsLive').each(function(){
				$(this).attr('style', 'display:block');
			});
			$('.gpsFixed').each(function(){
				$(this).attr('style', 'display:none');
			});
		}
	}
