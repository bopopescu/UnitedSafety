#include <signal.h>
#include <unistd.h>
#include <INET_IPC.h>
#include <INetConfig.h>
#include "lens.h"

ATSLogger g_log;

static const ats::String g_app_name("lens-identify");
INET_IPC g_INetIPC;  // common data.
INetConfig *g_pLensParms;  // read the db-config parameters



//=======================================================================================================================================
//
int main(int argc, char* argv[])
{
	g_log.set_global_logger(&g_log);
	g_log.set_level(5);
	g_log.open_testdata(g_app_name);

	ats_logf(ATSLOG_NONE, "================ lens-identify started ================");

	INetConfig parms;  // read the db-config parameters
	g_pLensParms = &parms;

	Lens *lens = new Lens();
	

	if (lens->spi_init("/dev/spidev1.0") < 0)
	{
		ats_logf(ATSLOG_ERROR, "Can't open spi device");
		exit(1);
	}
	lens->dump_registers();
	printf("\n\nINet IPC\n");
	g_INetIPC.Dump();
	return 0;
}

