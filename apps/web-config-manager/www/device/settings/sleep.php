<?php if(empty($_SESSION['M2M_SESH_USERNAME'])) { exit;}  ?>
	<!-- Sleep subsubtab -->
	<form id="outputCheck" method="post" action="/inc/sleep_processor.php">
		<input type="hidden" name="csrfToken" value="<?php echo (!empty($_SESSION['csrfToken']) ? $_SESSION['csrfToken'] : 'noData'); ?>" />
		<input type="hidden" name="maxCriticalVoltage" value="12" />
		<input type="hidden" name="minCriticalVoltage" value="11" />
		<!-- Header -->
		<div class="inversetab">Sleep Conditions</div>
		<!--- <a href="/TL3000_HTML5/Default_CSH.htm#SLEEPCONDITIONS" TARGET="_blank"><img src="/images/help.png" alt="help" border="0" ></a> -->
		<div class="hr"><hr /></div>
		<div class="row">
			<span>The Sleep Conditions determine when a TruLink will shut down after the wake up conditions are cleared.</span>
		</div>
		<br/>
		<br/>
		
		<div class="inversetab">Priority 1</div>
 		<div class="hr"><hr /></div>
		<div id="Priority1Section">
			<div class="row">
				<span class="label2">Keep Awake</span>
				<span class="formw2 reg">
					<select name="SleepKeepAwake" style="width: 60px;">
						<option value="0" <?php echo (($sleep_keep_awake == 0) ? 'selected="selected"':'');?>>Off</option>
						<option value="1" <?php echo (($sleep_keep_awake == 1) ? 'selected="selected"':'');?>>1</option>
						<option value="2" <?php echo (($sleep_keep_awake == 2) ? 'selected="selected"':'');?>>2</option>
						<option value="3" <?php echo (($sleep_keep_awake == 3) ? 'selected="selected"':'');?>>3</option>
						<option value="4" <?php echo (($sleep_keep_awake == 4) ? 'selected="selected"':'');?>>4</option>
						<option value="5" <?php echo (($sleep_keep_awake == 5) ? 'selected="selected"':'');?>>5</option>
						<option value="10" <?php echo (($sleep_keep_awake == 10) ? 'selected="selected"':'');?>>10</option>
						<option value="15" <?php echo (($sleep_keep_awake == 15) ? 'selected="selected"':'');?>>15</option>
						<option value="20" <?php echo (($sleep_keep_awake == 20) ? 'selected="selected"':'');?>>20</option>
						<option value="25" <?php echo (($sleep_keep_awake == 25) ? 'selected="selected"':'');?>>25</option>
						<option value="30" <?php echo (($sleep_keep_awake == 30) ? 'selected="selected"':'');?>>30</option>
						<option value="45" <?php echo (($sleep_keep_awake == 45) ? 'selected="selected"':'');?>>45</option>
						<option value="60" <?php echo (($sleep_keep_awake == 60) ? 'selected="selected"':'');?>>60</option>
						<option value="120" <?php echo (($sleep_keep_awake == 120) ? 'selected="selected"':'');?>>120</option>
						<option value="180" <?php echo (($sleep_keep_awake == 180) ? 'selected="selected"':'');?>>180</option>
						<option value="240" <?php echo (($sleep_keep_awake == 240) ? 'selected="selected"':'');?>>240</option>
					</select>&nbsp;&nbsp;Minutes &nbsp;&nbsp;&nbsp;&nbsp;
				</span>
			</div>
			<br/>
		</div>
		<br/>
 		<div class="row">
			<span>If the input voltage falls below the Shutdown Voltage for more than a minute the system will clear any high priority messages and then shut down.</span>
		</div>
		<br/>
		<br/>

		<div class="inversetab">Priority 2</div>
		<div class="hr"><hr /></div>
		<div class="row">
			<span class="label2">Critical Low Voltage</span>
			<span class="formw2 reg">
				<input type="text" size="5" name="wakeupLowBattV" value="<?php echo $wakeup_low_battery_voltage; ?>">&nbsp;V (Min:11.0  , Max:12.0)
				<span class="errorMsg"></span> 
				<br>
			</span>
		</div>
		<div class="row">
			<span>If the system is off and the input voltage drops below the Critical Low Voltage the system will wake up once to send a 'Critical Battery' message then shutdown again and ignore all of the wakeup signals until full voltage is restored.</span>
		</div>
		<br/>
		<div class="row">
			<span>If the system is on and the input voltage drops below the Critical Low Voltage the system will send a 'Critical Battery' message then shutdown  and ignore all of the wakeup signals until full voltage is restored</span>
		</div>
		<br/>
		<br/>
		
		<div class="spacer">&nbsp;</div>
		<?php
		if(hasSubmitAccess())
		{
		?>
			<div class="hr"><hr /></div>
			<div class="row">
				<span class="formw2">
					<button type="submit" class="button2-link">Save</button>&nbsp;
					<button type="reset" class="button3-link">Cancel</button>&nbsp;
				</span>
			</div>
		<?php 
		}							
		?>
	</form>
	
