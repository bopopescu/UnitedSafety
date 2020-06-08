$(document).ready(function(){


	$("form#login button[type='submit']").click(function(e){

		e.preventDefault();

		if(validateLoginForm())
		{
			$("form#login").submit();
		}
	});


	$("form#login input[name = 'user']").change(function () {
    		validateUserField();    	
	});
	
	$("form#login input[name = 'pass']").change(function () {
		validatePassField();    	
	});
	
});


function validateLoginForm()
{
	
	var validateUserFieldResult = validateUserField();
	var validatePassFieldResult = validatePassField();

	if(validateUserFieldResult  && validatePassFieldResult )
	{
		return true;
	}
	else{ return false; }

	/*
	var proceed = true;

	if ($("form#login input[name = 'user']").val() == "")
    	{
    		$("form#login span[name = 'errorLoginUser']").text("Username required");
    		$("form#login span[name = 'errorLoginUser']").show();		
		proceed = false;
    	}
	else
	{
		$("form#login span[name = 'errorLoginUser']").hide();		
	}

	if ($("form#login input[name = pass]").val() == "")
    	{
    		$("form#login span[name = 'errorLoginPwd']").text("Password required");
    		$("form#login span[name = 'errorLoginPwd']").show();		
		proceed = false;
    	}
	else
	{
    		$("form#login span[name = 'errorLoginPwd']").show();	
	}

  	//enableStatusSavebtn("login");

	return proceed;
	*/
}    

   
function validateUserField()
{

	if ($("form#login input[name = 'user']").val() == "")
    	{
    		$("form#login span[name = 'errorLoginUser']").text("Username required");
    		$("form#login span[name = 'errorLoginUser']").show();		
		return false;
    	}
	else
	{
		$("form#login span[name = 'errorLoginUser']").hide();		
		return true;
	}
}  

function validatePassField()
{

	if ($("form#login input[name = 'pass']").val() == "")
    	{
    		$("form#login span[name = 'errorLoginPwd']").text("Password required");
    		$("form#login span[name = 'errorLoginPwd']").show();		
		return false;
    	}
	else
	{
		$("form#login span[name = 'errorLoginPwd']").hide();		
		return true;
	}
}  
    
    