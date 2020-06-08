#include <iostream>
#include <list>
#include <vector>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
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
#include "db-monitor.h"

#include "heartbeat-state-machine.h"
#include "heartbeat.h"

#define APPNAME "heartbeat"

int g_dbg = 0;
ATSLogger g_log;

static int ac_debug(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
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

	g_log.set_level(g_dbg);

	return 0;
}

static int ac_help(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "This is the help system\n\r");

	return 0;
}

COMMON_EVENT_DEFINITION(,IgnitionEvent, AppEvent)

IgnitionEvent::IgnitionEvent()
{
}

IgnitionEvent::~IgnitionEvent()
{
}

void IgnitionEvent::start_monitor()
{
}

void IgnitionEvent::stop_monitor()
{
}

int get_time_from_db()
{
	const ats::String db_name("heartbeat_db");
	const ats::String db_file("/mnt/update/database/heartbeat.db");
	db_monitor::DBMonitorContext db(db_name, db_file);
	{
		const ats::String query(
			"SELECT time "
			"FROM timer_table;");
		const ats::String& err = db.query(query);

		if(!err.empty())
		{
			//insert current timestamp into database
			return -1;
		}

		if(db.Table().size() > 0)
		{
			db_monitor::ResultRow& row = db.Table()[0];
			return atoi(row[0].c_str());
		}

	}
	//insert current timestamp into database
	return -1;
}

int set_time_in_db(time_t p_sec)
{
	const ats::String db_name("heartbeat_db");
	const ats::String db_file("/mnt/update/database/heartbeat.db");
	db_monitor::DBMonitorContext db(db_name, db_file);
	const ats::String query("CREATE TABLE IF NOT EXISTS timer_table("
		"tid INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
		"time INTEGER );INSERT OR REPLACE INTO timer_table VALUES(0," + ats::toStr(p_sec) + ");");
	ats_logf(ATSLOG(0), "%s, %d, %s", __FILE__, __LINE__, query.c_str());
	const ats::String& err = db.query(query);

	if(!err.empty())
	{
		//insert current timestamp into database
		return -1;
	}

	return 0;
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
			if(c != -ENODATA) ats_logf(ATSLOG(0), "%s,%d: %s", __FILE__, __LINE__, ebuf);
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
			ats_logf(ATSLOG(0), "%s,%d: command is too long", __FILE__, __LINE__);
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
			ats_logf(ATSLOG(0), "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "gen_arg_list failed: %s\n", err);
			cmd.clear();
			continue;
		}

		if(g_dbg >= 2)
		{
			ats_logf(ATSLOG(0), "%s,%d:%s: Command received: %s", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());
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
	sd.m_port = 41011;
	sd.m_hook = this;
	sd.m_cs = local_command_server;
	::start_server(&sd);
	signal_app_tcp_socket_ready(APPNAME, ats::toStr(sd.m_port).c_str());
	signal_app_ready(APPNAME);
}

static MyData g_md;

int main(int argc, char* argv[])
{
	MyData& md = g_md;
	md.set("debug", "1");
	md.set("user", "applet");
	md.set_from_args(argc - 1, argv + 1);

	g_dbg = md.get_int("debug");

	if(ats::su(md.get("user")))
	{
		ats_logf(ATSLOG(0), "Could not become user \"%s\": ERR (%d): %s", md.get("user").c_str(), errno, strerror(errno));
		return 1;
	}

	g_log.set_global_logger(&g_log);
	g_log.set_level(g_dbg);
	g_log.open_testdata(argv[0]);

	ats::String version;
	ats::get_file_line(version, "/version", 1);
	md.setversion(version);

	ats_logf(&g_log, "Heartbeat Started");

	md.start_server();
	
	wait_for_app_ready("message-assembler");
	ats_logf(&g_log, "message-assembler detected");
	
	HeartbeatSm* w = new HeartbeatSm(md);
	w->run_state_machine(*w);

	md.wait_exit_sem();

	return 0;
}
