#include <iostream>
#include <map>
#include <list>

#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "ats-common.h"
#include "ats-string.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "state-machine-data.h"
#include "redstone-socketcan-j1939.h"
#include "ConfigDB.h"
#include "AFS_Timer.h"
#include "expat.h"

/* message xml format:
<data>
    <pgn>0x0FEE5</pgn>
    <value>
	    <destaddress>0xFF</destaddress>
	    <cycletime>2</cycletime>
	    <rawdata>00 00 00 00 00 00 00 00</rawdata>
    </value>
    <pgn>0x0FECA</pgn>
    <value>
	    <destaddress>0xFF</destaddress>
	    <cycletime>2</cycletime>
	    <rawdata>11 22 33 44 55 66 77 88 99 aa bb cc dd ee ff</rawdata>
    </value>
</data>
*/

static const ats::String g_app_name("J1939Simulator");

static void XMLCALL xmlend(void *p, const char *name);
static void handle_data(void *p, const char *content, int length);

int main(int argc, char* argv[])
{
	g_log.set_global_logger(&g_log);
	g_log.set_level(g_dbg);
	g_log.open_testdata(g_app_name);

	MyData md;
	md.set("debug", "1");
	g_dbg = md.get_int("debug");

	ats::StringMap &config = md.m_config;
	config.from_args(argc - 1, argv + 1);

	//g_dbg = config.get_int("debug");
	//g_log.set_global_logger(&g_log);
	//g_log.open_testdata(g_app_name);
	//g_log.set_level(g_dbg);

	ats_logf(ATSLOG(0), "J1939 Simulator started");

	const ats::String& fname = config.get("config");
	if(fname.empty())
	{
		print_usage(argv[0]);
		return 1;
	}

	FILE* f = fopen(fname.c_str(), "r");
	ats::String value;

	if(f)
	{
		const int err = read_from_file(f, value);
		fclose(f);

		if(err)
		{
			ats_logf(ATSLOG(0), "ERROR(%d,%s): An error occurred while reading from \"%s\"\n", err, strerror(err), fname.c_str());
			return 1;
		}

	}
	else
	{
		ats_logf(ATSLOG(0), "ERROR: Could not open \"%s\" for reading\n", fname.c_str());
		return 1;
	}

	h_xml_parse(&md, value);

	{
		pthread_t m_transmit_thread;
		const int retval = pthread_create(
				&(m_transmit_thread),
				(pthread_attr_t *)0,
				h_transmit_thread,
				&md);
		if( retval )
		{
			ats_logf(ATSLOG(0), "%s,%d: Failed to create reader. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
			return 1;
		}
	}

	ServerData sd;
	init_ServerData(&sd, 64);
	sd.m_hook = &md;
	sd.m_cs = client_command_server;
	::start_redstone_ud_server(&sd, "j1939messagesend", 1);

	md.wait_exit_sem();
	return 0;
}
