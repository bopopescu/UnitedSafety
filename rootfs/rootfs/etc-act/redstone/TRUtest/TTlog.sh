TTRESULTS="/mnt/update/TRUtest/TTresult.txt"
TTDebug="/mnt/update/TRUtest/TTresult.dbg"
 
# Use RED - errors
#			GREEN - Response required ("Hit Enter to continue")
#			CYAN - debugging information (progress)
#			BLUE - instructions/actions required ("Remove the sym",...)
#			YELLOW - regular log entries ("Test was successful"...)
RED='\033[31m'
GREEN="\033[32m"
YELLOW='\033[33m'
BLUE='\033[34m'
CYAN='\033[36m'
NORMAL="\033[39m\033[0m"
INVERT='\033[7m'

pRED=$'\033[31m'
pGREEN=$'\033[32m'
pYELLOW=$'\033[33m'
pNORMAL=$'\033[39m'
pBLUE=$'\033[34m'
pCYAN=$'\033[36m\033[0m'
pINVERT=$'\033[7m'

write_log()
{
  now=`date`
  echo -e "($now) $TESTNAME: $1" >> $TTRESULTS
  echo -e "($now) $YELLOW$TESTNAME: $1$NORMAL"
}

write_error()
{
  now=`date`
  echo "($now) $TESTNAME: ERROR: $1" >> $TTRESULTS
  echo -e "($now) $RED$INVERT$TESTNAME:ERROR: $1 $NORMAL"
} 

write_debug()
{
  now=`date`
  echo "($now) $TESTNAME: $1" >> $TTDebug
  echo -e "($now) $CYAN $TESTNAME: $1 $NORMAL"
}

write_config()
{
  db-config get -v >> /mnt/update/TRUtest/$TESTNAME_$stage.cfg
}

write_command()
{
  write_debug "Running: $1"
  `$1` >> $TTDebug
}
