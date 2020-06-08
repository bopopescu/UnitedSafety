#include <iostream>

#include "ats-common.h"
#include "atslogger.h"
#include "packetizer.h"
#include "TruLinkStateMachine.h"
#include "TruLinkCopy.h"

ATSLogger g_log;
static MyData g_md;

int main(int argc, char* argv[])
{
	g_log.open_testdata(argv[0]);
	g_log.set_global_logger(&g_log);

	MyData& md = g_md;
	md.set("debug", "1");
	md.set_from_args(argc - 1, argv +1);
	int dbg;
	dbg = md.get_int("debug");
	g_log.set_level(dbg);

	TruLinkStateMachine *psm = new TruLinkStateMachine(g_md);
	psm->run_state_machine(*psm);

	TruLinkCopy *pc = new TruLinkCopy(g_md);
	pc->run_state_machine(*pc);

	ats::infinite_sleep();

	return 0;
}
