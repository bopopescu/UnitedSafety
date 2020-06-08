#include "ats-common.h"
#include "atslogger.h"
#include "serialportmanager.h"
#include "plcserialmonitorstatemachine.h"
ATSLogger g_log;


int main(int , char **)
{
	g_log.open_testdata("plc-serial-monitor");
	ats_logf(&g_log, "Starting plc-serial-monitor.");
	QByteArray data;
	StateMachineData pdata;
	PlcSerialMonitorSm *plc = new PlcSerialMonitorSm(pdata);
	plc->run_state_machine(*plc);

	sem_t s;
	sem_init(&s,0,0);
	sem_wait(&s);

	return 0;
}
