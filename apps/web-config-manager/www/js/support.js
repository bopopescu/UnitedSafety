$(document).ready(function () {
	
    /*$("form#chgPassword input[name = newPwd], form#chgPassword input[name = confirmNewPwd]").change(function () {
    	checkPasswordMatch('newPwd', 'confirmNewPwd');
       
    });*/
    
    
    
    $("form#chgPassword button[type = submit]").click(function(e){
    	
    	var proceed = true;
    	e.preventDefault();
    	
    	
    	$("form#chgPassword").find("input").each(function(){

    		 if(!$(this).val())
    		 {
    			 $(this).nextAll(".errorMsg").html("This is a required field").show();
    			 proceed = false;
    			 //enableStatusSavebtn('chgPassword');
    		 }
    		 else
			 {
    			 $(this).nextAll(".errorMsg").empty().hide();
			 }
    	});
    
    	
    	if(proceed === true && checkPasswordMatch('newPwd', 'confirmNewPwd') === true)
    	{
    		$("form#chgPassword").submit();
    	}
    	
    	
    });
});

       
        
//check if the retyped password matches the first one
function checkPasswordMatch($field1, $field2)
{	
	if($("form#chgPassword input[name = "+$field1+"]").val() != $("form#chgPassword input[name = "+$field2+"]").val())
	{
		$("form#chgPassword span[name = "+$field2+"Error]").empty().html("Re-entered password does not match new password.");
		$("form#chgPassword span[name = "+$field2+"Error]").show();
		return false;
	}
	else
	{
		$("form#chgPassword span[name = "+$field2+"Error]").empty();
		return true;
	}
	//enableStatusSavebtn('chgPassword');
	
}
/*function checkPasswordMatch($id, $id2)
{
	 $("form#chgPassword input[name = 'newPwd']").keyup(function () {

			if (($("form#chgPassword input[name = 'newPwd']").val()) != ($("form#chgPassword input[name = 'rchgPwd']").val()))
			{
				$("#" + $id2 + " > .errorMsg").show();
	        $("#" + $id2 + " > .errorMsg").text("Password re-entry does not match entered password");
	        enableStatusSavebtn($("#" + $id2).parents("form").attr("id"));
			}
		if (($("form#chgPassword input[name = 'chgPwd']").val()) == ($("form#chgPassword input[name = 'rchgPwd']").val()))
			{
			$("#" + $id2 + " > .errorMsg").hide();
	        $("#" + $id2 + " > .errorMsg").empty();
	        enableStatusSavebtn($("#" + $id2).parents("form").attr("id"));
			}
	 });
	 
	 $("form#chgPassword input[name = 'chgPwd']").blur(function () {

			if (($("form#chgPassword input[name = 'chgPwd']").val()) != ($("form#chgPassword input[name = 'rchgPwd']").val()))
			{
				$("#" + $id2 + " > .errorMsg").show();
	        $("#" + $id2 + " > .errorMsg").text("Password re-entry does not match entered password");
	        enableStatusSavebtn($("#" + $id2).parents("form").attr("id"));
			}
		if (($("form#chgPassword input[name = 'chgPwd']").val()) == ($("form#chgPassword input[name = 'rchgPwd']").val()))
			{
			$("#" + $id2 + " > .errorMsg").hide();
	        $("#" + $id2 + " > .errorMsg").empty();
	        enableStatusSavebtn($("#" + $id2).parents("form").attr("id"));
			}
	 });
	 
	 $("form#chgPassword input[name = 'rchgPwd']").keyup(function () {
			if (($("form#chgPassword input[name = 'rchgPwd']").val()) != ($("form#chgPassword input[name = 'chgPwd']").val()))
			{
		        $("#" + $id2 + " > .errorMsg").show();
	        $("#" + $id2 + " > .errorMsg").text("Password re-entry does not match entered password");
	        enableStatusSavebtn($("#" + $id2).parents("form").attr("id"));
			}
		if (($("form#chgPassword input[name = 'chgPwd']").val()) == ($("form#chgPassword input[name = 'rchgPwd']").val()))
			{
			$("#" + $id2 + " > .errorMsg").hide();
	        $("#" + $id2 + " > .errorMsg").empty();
	        enableStatusSavebtn($("#" + $id2).parents("form").attr("id"));
			}
	 });
	 
	 $("form#chgPassword input[name = 'rchgPwd']").blur(function () {
			if (($("form#chgPassword input[name = 'rchgPwd']").val()) != ($("form#chgPassword input[name = 'chgPwd']").val()))
			{
		        $("#" + $id2 + " > .errorMsg").show();
	        $("#" + $id2 + " > .errorMsg").text("Password re-entry does not match entered password");
	        enableStatusSavebtn($("#" + $id2).parents("form").attr("id"));
			}
		if (($("form#chgPassword input[name = 'chgPwd']").val()) == ($("form#chgPassword input[name = 'rchgPwd']").val()))
			{
			$("#" + $id2 + " > .errorMsg").hide();
	        $("#" + $id2 + " > .errorMsg").empty();
	        enableStatusSavebtn($("#" + $id2).parents("form").attr("id"));
			}
	 });
}*/

