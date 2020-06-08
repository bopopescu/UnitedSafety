/*
 * This file contains all the ajax calls
 */


/*
 * checkIpConflict()
 * Checks that the first three octets of the IP address of the interface name passed in intf does not conflict with the IP address of the current page.
 * intf - interface name of the other interface (i.e. eth0, ra0)
 * rowId - id of the row containing the IP address
 * FIXME:
 * Update checkIpConflict to be more relevant. Currently does not work with other interfaces ppp0, apcli0 and it is not utilised correctly with
 * current implementation.
 */
function checkIpConflict(intf, rowId)
{
	$.ajax({
	  type: 'GET',
	  dataType: "text",
	  async: true,
	  cache: false,
	  url: 'https://'+window.location.hostname+'/inc/ajax.php',
	  data: { op: 'getip', data: intf },
	  beforeSend:function(){
		  //alert(intf);
	  },
	  success:function(data){

		// alert("data =" + data);
		if(data != null)	//if no data is returned by the server/device => do nothing
		{
			conflictCount = compareIPAddresses(build(rowId), data, 3);

			if(conflictCount > 2)	//=> on same subnet
			{
				var intfAlias = "";
				if(intf == "eth0"){ intfAlias = "Ethernet";}

				if(intf == "ra0"){ intfAlias = "WiFi";}

				$("#" + rowId + " > .errorMsg").text("This subnet is in use by the "+ intfAlias + " interface.");
				enableStatusSavebtn($("#" + rowId).parents("form").attr("id"));

				return false;
			}
		}

		return true;
	  },
	  error:function(jqXHR, textStatus, errorThrown){	// ajax request failed
		//do something
	  }
	});
} // END checkIpConflict()
