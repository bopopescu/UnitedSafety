#include <iostream>

#include "ats-common.h"


#include "atslogger.h"
#include "packetizer.h"
#include "InetStateMachine.h"
#include "CopySM.h"
#include "ConfigDB.h"
#include "CThreadWait.h"
#include <INetConfig.h>  // db-config params.
#include <INET_IPC.h>  // data to switch between isc-lens and packetizer-inet.

#include "SatData.h"
#include "SatData90_IDGateway.h"
#include "SatData91_IDInstrument.h"
#include "SatData93_UpdateInstrument.h"
#include "SatData93_UpdateInstrument.h"
#include "SatData94_UploadError.h"
#include "SatData95_LostInstrument.h"
#include "SatData96_UpdateGateway.h"

static MyData g_md;
ATSLogger g_log;
CThreadWait g_TWaitForNewRecord;
INetConfig * g_pConfig;
INET_IPC g_INetIPC;
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
void OpenGlobalLogger(ATSLogger &log, const char * command_name)
{
	db_monitor::ConfigDB db;
	int LogLevel = db.GetInt(command_name, "LogLevel", 0);
	log.open_testdata(command_name);
	log.set_global_logger(&log);
	log.set_level(LogLevel);
//	ats_logf(ATSLOG_INFO, "%s,%d: Logging set up for %s", __FILE__, __LINE__, command_name);
}


int main(int argc, char* argv[])
{
	wait_for_app_ready("message-assembler");
	argc = argc;  // eliminate compiler warning
	OpenGlobalLogger(g_log, "packetizer-inet");
	INetConfig config;
	g_pConfig = &config;
	ats_logf(ATSLOG_ERROR, "%s,%d: ========= packetizer-inet starting ========", __FILE__, __LINE__);

	//InetStateMachine *psm = new InetStateMachine(g_md);
printf("%s:%d\n", __FILE__, __LINE__);
	InetStateMachine psm(g_md);
printf("%s:%d\n", __FILE__, __LINE__);
	psm.run_state_machine(psm);
printf("%s:%d\n", __FILE__, __LINE__);

	CopySM *pc = new CopySM(g_md);
printf("%s:%d\n", __FILE__, __LINE__);
	pc->run_state_machine(*pc);
printf("%s:%d\n", __FILE__, __LINE__);
	ats::infinite_sleep();

	return 0;
}
