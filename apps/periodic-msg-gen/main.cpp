#include <iostream>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>

#include "ats-common.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "atslogger.h"

#include "periodic-msg-gen-state-machine.h"
#include "periodic-msg-gen.h"

int g_dbg = 0;
ATSLogger g_log;

int ac_debug(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	if(p_argc <= 1)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n", g_dbg);
	}
	else
	{
		g_dbg = strtol(p_argv[1], 0, 0);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n", g_dbg);
	}
	return 0;
}

int ac_help(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "This is the help system\n\r");

	return 0;
}

COMMON_EVENT_DEFINITION(,setworkEvent, AppEvent)

setworkEvent::setworkEvent()
{
}

setworkEvent::~setworkEvent()
{
}

void setworkEvent::start_monitor()
{
}

void setworkEvent::stop_monitor()
{
}

COMMON_EVENT_DEFINITION(,unsetworkEvent, AppEvent)

unsetworkEvent::unsetworkEvent()
{
}

unsetworkEvent::~unsetworkEvent()
{
}

void unsetworkEvent::start_monitor()
{
}

void unsetworkEvent::stop_monitor()
{
}

MyData::MyData()
{
	m_exit_sem = new sem_t;
	sem_init(m_exit_sem, 0, 0);

	m_command.insert(AdminCommandPair("help", AdminCommand(ac_help, "Displays help information")));
	m_command.insert(AdminCommandPair("h", AdminCommand(ac_help, "Displays help information")));
	m_command.insert(AdminCommandPair("?", AdminCommand(ac_help, "Displays help information")));

	m_command.insert(AdminCommandPair("debug", AdminCommand(ac_debug, "Displays debug information")));
}


// ************************************************************************
// Description: A local command server so that other applications or
//      developers can query this application.
//
//      One instance of this function is created for every local connection
//      made.
//
// Parameters:
// Return: NULL pointer
// ************************************************************************
static void* local_command_server( void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData &md =* ((MyData*)(cd->m_data->m_hook));

	bool command_too_long = false;
	ats::String cmd;

	const size_t max_command_length = 1024;

	AdminCommandContext acc(md, *cd);

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cdc);

		if(c < 0)
		{
			if(c != -ENODATA) ats_logf(&g_log, "%s,%d: %s", __FILE__, __LINE__, ebuf);
			break;
		}

		if(c != '\r' && c != '\n')
		{
			if(cmd.length() >= max_command_length) command_too_long = true;
			else cmd += c;

			continue;
		}

		if(command_too_long)
		{
			ats_logf(&g_log, "%s,%d: command is too long", __FILE__, __LINE__);
			cmd.clear();
			command_too_long = false;
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "command is too long\n");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char* err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			ats_logf(&g_log, "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "gen_arg_list failed: %s\n", err);
			cmd.clear();
			continue;
		}

		if(g_dbg >= 2)
		{
			ats_logf(&g_log, "%s,%d:%s: Command received: %s", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());
		}

		cmd.clear();

		if(cb.m_argc <= 0)
		{
			continue;
		}

		{
			const ats::String cmd(cb.m_argv[0]);
			AdminCommandMap::const_iterator i = md.m_command.find( cmd);

			if(i != md.m_command.end())
			{
				(i->second).m_fn(acc, i->second, cb.m_argc, cb.m_argv);
			}
			else
			{
				send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "Invalid command \"%s\"\n", cmd.c_str());
			}
		}
	}
	return 0;
}

void MyData::start_server()
{
	ServerData& sd = m_command_server;
	init_ServerData(&sd, 64);
	sd.m_port = 41106;
	sd.m_hook = this;
	sd.m_cs = local_command_server;
	::start_server(&sd);
}

int main(int argc, char* argv[])
{
	g_log.open_testdata("periodic-msg-gen");
	MyData& md = *(new MyData());
	md.set("debug", "1");
	md.set_from_args(argc - 1, argv + 1);

	g_dbg = md.get_int("debug");

	std::stringstream v;
	ats::system("cat /version | head -n 1 | tr -d '\r' | tr -d '\n'", &v);

	const ats::String& version = v.str();
	md.setversion(version);

	const ats::String& ss = "periodic-msg-gen-" + v.str();
	openlog(ss.c_str(), LOG_PID, LOG_USER);
	ats_logf(&g_log, "Periodic Msg Gen Started");

	PeriodicmsggenSm* w = new PeriodicmsggenSm(md);
	w->run_state_machine(*w);

	md.start_server();
	md.wait_exit_sem();

	return 0;
}
