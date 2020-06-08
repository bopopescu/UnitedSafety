#!/bin/sh

#---------------------------------------------------------------------------------------
#  hardware ID test 
#
#  test that the hardware id value is correct
#
#  Procedure:
#	1) run hid application to return proper Hardware ID. 
#
# Failure modes:
#	2) Display Unknown or Wrong number. 
#
#---------------------------------------------------------------------------------------


# TTlog will log to the screen and the TTresult.txt file
source TTlog.sh

#TThelper.sh include the get_val, set_val, clear_val functions.
source TThelper.sh

# must define TESTNAME for TTlog.sh to output the script name
#---------------------------------------------------------------------------------------
TESTNAME="HardwareIDTest"
stage=`get_val stage`
write_debug "Running test $TESTNAME,current stage $stage"

# startup stage
val=$(hid)
write_prompt "$val"
GetYNResponse "Hardware ID Test" "Does the result match your HardWare?" "Test Failed"
