#include <iostream>

#include "ats-common.h"
#include "atslogger.h"
#include "packetizer.h"
#include "packetizerCamsStateMachine.h"
#include "packetizerCamsCopy.h"
#include "ConfigDB.h"
#include "CThreadWait.h"


static MyData g_md;
ATSLogger g_log;
CThreadWait g_TWaitForNewRecord;

//-------------------------------------------------------------------------------------------------
// OpenGlobalLogger - Use this function to open log (defined as ATSLogger log) in 
//                    the 'main.cpp' source file (usually as g_log in global space).  
//										Call this function immediately in main as 
//				OpenGlobalLogger(log, argv[0]) 
//                    to pull the current logging level from 
//				db-config <process name> LogLevel
//										You can then use ats_logf(ATSLOG(x)... to log data.  Only values of x less
//                    than the value of LogLevel will be logged to the the log file.
//
void OpenGlobalLogger(ATSLogger &log, char * command_name)
{
	db_monitor::ConfigDB db;
	int LogLevel = db.GetInt(command_name, "LogLevel", 0);

	log.open_testdata(command_name);
	log.set_global_logger(&log);
	log.set_level(LogLevel);
}


int main(int argc, char* argv[])
{
	OpenGlobalLogger(g_log, argv[0]);

	PacketizerCamsStateMachine *psm = new PacketizerCamsStateMachine(g_md);
	psm->run_state_machine(*psm);

	PacketizerCamsCopy *pc = new PacketizerCamsCopy(g_md);
	pc->run_state_machine(*pc);

	ats::infinite_sleep();

	return 0;
}
