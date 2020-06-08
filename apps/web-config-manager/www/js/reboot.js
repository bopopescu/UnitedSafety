$(document).ready(function(){
	
$("#reboot").click(function(e){
	
	e.preventDefault();
	
	
	$.blockUI({ 
		message: '<h3>Are you sure you want to reboot the device?</h3><input type="button" id="ackModalYes" value="Yes" /> <input type="button" id="ackModalNo" value="No" /><br />',
	}); 
	
	
	$('#ackModalYes').click(function() { 
        

		$.ajax({
			  type: 'POST',
			  url: 'https://'+window.location.hostname+'/inc/reboot.php',
			  timeout: 10000,
			  data: { op: 'reboot' },
			  beforeSend:function(){ },
			  success:function(data){
				if(data == "fail"){
					$.blockUI({ message: "<h3>Failed to reboot the device</h3>" }); 
				}
				else{		
				  $.blockUI({ message: "<h3>The device is rebooting</h3> <p>This could take several minutes.</p><p>Please re-connect to it, once the reboot is complete, and re-fresh this page.</p>" }); 
				  setTimeout(function(){document.location.reload(true)}, 60000);
				}
			  },
			  error:function(jqXHR, textStatus, errorThrown){	// ajax request failed
				if(textStatus == 'error'){
				  //alert("Failed to reboot the device. Error = " + errorThrown);
				  $.blockUI({ message: "<h4>Failed to reboot the device.</h4> <p>Error = " + errorThrown + "</p>" });
				}
			  }
			});
         
    }); 

    $('#ackModalNo').click(function() { 
        $.unblockUI(); 
        return false; 
    }); 
	
	//Original implementation. Works fine. The above just makes use of the modal. 
	/*var proceed_to_reboot = confirm("Are you sure you want to reboot the device?");
	
	if(proceed_to_reboot == true)
	{
		$.ajax({
			  type: 'POST',
			  url: 'http://'+window.location.hostname+'/inc/reboot.php',
			  timeout: 10000,
			  data: { op: 'reboot' },
			  beforeSend:function(){ },
			  success:function(data){
				if(data == "fail"){
					alert("Failed to reboot the device.");
				}
				else{		
				  $("body").append('<div id="overlay" style="width: 100%; height: 100%; background: #000; opacity: 0.5; top: 0px; left: 0px; position: fixed; z-index: 5000;"></div>');	
				  alert("The device is currently being rebooted. Please re-connect to it once this operation is complete.");
				}
			  },
			  error:function(jqXHR, textStatus, errorThrown){	// ajax request failed
				if(textStatus == 'error'){
				  alert("Failed to reboot the device. Error = " + errorThrown);
				}
			  }
			});
	}*/
	
});

});