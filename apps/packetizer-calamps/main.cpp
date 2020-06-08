#include <fstream>

#include <sys/stat.h>

#include "packetizer.h"
#include "ats-common.h"
#include "packetizer_state_machine.h"
#include "packetizerCopy.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"

#include "lmdirect/lmdirect.h"

#define PACKETIZER_TESTDATA_LOG_FILE "/var/log/testdata/packetizer-calamps/packetizer-calamps-testdata.log"

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

static MyData g_md;

int main(int argc, char *argv[])
{
#if 0
	printf("\n\n");

	LMDirect m;
	m.m_option.set_option(LMDirect::Option::MOBILE_ID_TYPE, true);
	m.m_option.set_option(LMDirect::Option::AUTHENTICATION_WORD, true);
	m.m_option.set_authentication_word("abcd");

	{
		const ats::String& s = m.get_gms_format();
		printf("Length: %zu\n", s.size());

		for(size_t i = 0; i < s.size(); ++i)
		{
			printf("%02X,", s[i]);
		}

		printf("\n");
	}

	return 0;
#endif

	g_log.open_testdata("packetizer-calamps");

	MyData& md = g_md;
	md.set("debug", "0");
	md.set_from_args(argc - 1, argv + 1);

	g_dbg = md.get_int("debug");

	ATSLogger::set_global_logger(&g_log);

	signal_app_unix_socket_ready("packetizer-calamps", "packetizer-calamps");

	PacketizerSm* w = new PacketizerSm(md);
	w->run_state_machine(*w);

	PacketizerCopy *c = new PacketizerCopy(md);
	c->run_state_machine(*c);

	signal_app_ready("packetizer-calamps");

	ats::infinite_sleep();
}

