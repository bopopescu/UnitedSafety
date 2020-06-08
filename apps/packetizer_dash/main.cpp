#include <fstream>

#include "packetizer.h"
#include "ats-common.h"
#include "packetizer_state_machine.h"
#include "packetizerCopy.h"
#include "atslogger.h"

ATSLogger g_log;

int g_dbg = 0;

MyData::MyData()
{
	current_mid = 0;
	m_timer = new AFS_Timer();
}

MyData::~MyData()
{
	delete m_timer;
}

void MyData::testdata_log(const ats::String& p_msg) const
{
  ats_logf(&g_log,p_msg.c_str());
}

static MyData g_md;

//-----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	g_log.open_testdata("packetizer_dash");
  ats_logf(&g_log,"Starting packetizer_dash");
  
	MyData& md = g_md;
	md.set("debug", "1");
	md.set_from_args(argc - 1, argv + 1);

	g_dbg = md.get_int("debug");

	PacketizerSm* w = new PacketizerSm(md);
	w->run_state_machine(*w);

	PacketizerCopy *c = new PacketizerCopy(md);
	c->run_state_machine(*c);

	sem_t s;
	sem_init(&s,0,0);
	sem_wait(&s);
}

