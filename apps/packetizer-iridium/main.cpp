#include <fstream>

#include <sys/stat.h>

#include "packetizer.h"
#include "ats-common.h"
#include "atslogger.h"
#include "packetizer_state_machine.h"
#include "packetizerCopy.h"
#include "IridiumMonitor.h"

#define PACKETIZER_TESTDATA_LOG_FILE "/var/log/testdata/packetizer/packetizer-testdata.log"

ATSLogger g_log;
int g_dbg = 0;

MyData::MyData()
{}

MyData::~MyData()
{
	pthread_mutex_destroy(m_mutex);
	delete m_mutex;
}

static MyData g_md;

int main(int argc, char *argv[])
{
	g_log.open_testdata(argv[0]);
	g_log.set_global_logger(&g_log);

	MyData& md = g_md;
	md.set("debug", "1");
	md.set_from_args(argc - 1, argv + 1);

	g_dbg = md.get_int("debug");
	g_log.set_level(g_dbg);
	ats_logf(&g_log, "\n\n-------------------\nStarting packetizer-iridium");

	IridiumMonitor *im = new IridiumMonitor(md);
	im->run_state_machine(*im);

	PacketizerSm* w = new PacketizerSm(md);
	w->run_state_machine(*w);

	PacketizerCopy *c = new PacketizerCopy(md);
	c->run_state_machine(*c);

	sem_t s;
	sem_init(&s,0,0);
	sem_wait(&s);
}

