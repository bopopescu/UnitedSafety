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
#include "atslogger.h"
#include "socket_interface.h"
#include "RedStoneSocketQuery.h"
#include "command_line_parser.h"
#include "ConfigDB.h"
#include "trip-monitor-state-machine.h"
#include "trip-monitor.h"

ATSLogger g_log;

int g_dbg = 0;

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

COMMON_EVENT_DEFINITION(, RPMEvent, AppEvent)

RPMEvent::RPMEvent()
{
}

RPMEvent::~RPMEvent()
{
}

void RPMEvent::start_monitor()
{
}

void RPMEvent::stop_monitor()
{
}

COMMON_EVENT_DEFINITION(, SpeedEvent, AppEvent)

SpeedEvent::SpeedEvent()
{
}

SpeedEvent::~SpeedEvent()
{
}

void SpeedEvent::start_monitor()
{
}

void SpeedEvent::stop_monitor()
{
}

COMMON_EVENT_DEFINITION(, IgnitionEvent, AppEvent)

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


MyData::MyData()
{
	m_exit_sem = new sem_t;
	sem_init(m_exit_sem, 0, 0);

	m_skybase_poll_period = 3;
	pthread_mutex_init(&m_skybase_mutex, 0);

	init_ClientSocket(&cs_trip_stats);

	if(connect_redstone_ud_client(&cs_trip_stats, "trip-stats"))
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: %s", __FILE__, __LINE__, cs_trip_stats.m_emsg);
		exit(1);
	}

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

	ClientDataCache cache;
	init_ClientDataCache(&cache);

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cache);

		if(c < 0)
		{
			if(c != -ENODATA) 
				ats_logf(ATSLOG_ERROR, "%s,%d: %s", __FILE__, __LINE__, ebuf);
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
			ats_logf(ATSLOG_ERROR, "%s,%d: command is too long", __FILE__, __LINE__);
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
			ats_logf(ATSLOG_ERROR, "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "gen_arg_list failed: %s\n", err);
			cmd.clear();
			continue;
		}

		ats_logf(ATSLOG_INFO, "%s,%d:%s: Command received: %s", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());

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
	init_ServerData(&sd, 32);
	sd.m_hook = this;
	sd.m_cs = local_command_server;
	::start_redstone_ud_server(&sd, "trip-monitor", 1);
	signal_app_unix_socket_ready("trip-monitor", "trip-monitor");
	signal_app_ready("trip-monitor");
}

int MyData::get_speed_limit(int p_default, int p_offset, float p_scale)
{
	int skybase_speed = 0;
	pthread_mutex_lock(&m_skybase_mutex);

	if(m_skybase_speed_valid)
	{
		skybase_speed = int(m_skybase_speed * p_scale) + p_offset;
	}

	pthread_mutex_unlock(&m_skybase_mutex);

	return (skybase_speed > 0) ? skybase_speed : p_default;
}

void MyData::set_skybase_poll_period(int p_period)
{
	pthread_mutex_lock(&m_skybase_mutex);
	m_skybase_poll_period = p_period;
	pthread_mutex_unlock(&m_skybase_mutex);
}

void MyData::set_skybase_stable_readings(int p_stable_readings)
{
	pthread_mutex_lock(&m_skybase_mutex);
	m_skybase_stable_readings = p_stable_readings;
	pthread_mutex_unlock(&m_skybase_mutex);
}

int MyData::get_skybase_poll_period()
{
	pthread_mutex_lock(&m_skybase_mutex);
	const int n = m_skybase_poll_period;
	pthread_mutex_unlock(&m_skybase_mutex);
	return n;
}

int MyData::get_skybase_stable_readings()
{
	pthread_mutex_lock(&m_skybase_mutex);
	const int n = m_skybase_stable_readings;
	pthread_mutex_unlock(&m_skybase_mutex);
	return n;
}

static int get_int_tokens(const char* p_src, int* p_des, int p_size)
{
	int count = 0;

	if((!p_src) || (p_size <= 0))
	{
		return 0;
	}

	const char* p = p_src;

	for(;;)
	{
		const char c = *(p++);

		if((!c) || (' ' == c) || ('\t' == c) || (',' == c) || ('\n' == c))
		{

			*(p_des++) = strtol(p_src, 0, 0);
			p_src = p;
			++count;

			if(!(--p_size))
			{
				return count;
			}

		}

	}

}

// Description: Thread that runs forever retrieving speed limits from SkyBase.
void* skybase_thread_fn(void* p)
{
	MyData& md = *((MyData*)p);

	const char* SkyBase = "SkyBase";

	for(;;)
	{
		wait_for_app_ready(SkyBase);
		ats::RedStoneSocketQuery rsq(SkyBase);

		for(;;)
		{
			const ats::String& resp = rsq.query("get\r");

			if(resp.empty())
			{
				break;
			}

			int n[3];
			int& valid = n[0];
			int& stable_readings = n[1];
			int& speed = n[2];
			const int num_tokens = get_int_tokens(resp.c_str(), n, 3);
			pthread_mutex_lock(&md.m_skybase_mutex);

			if((num_tokens >= 3) && (stable_readings >= md.m_skybase_stable_readings))
			{
				md.m_skybase_speed_valid = valid;
				md.m_skybase_speed = speed;
			}

			pthread_mutex_unlock(&md.m_skybase_mutex);
			sleep(md.m_skybase_poll_period);
		}

		// XXX: Sleep one second to prevent high CPU usage. Note that execution will never reach this
		//	point if everything is working normally (only enter this state if there is a
		//	communications problem between the SkyBase application and this application).
		pthread_mutex_lock(&md.m_skybase_mutex);
		md.m_skybase_speed_valid = 0;
		md.m_skybase_speed = 0;
		pthread_mutex_unlock(&md.m_skybase_mutex);
		sleep(1);
	}

	return 0;
}

int main(int argc, char* argv[])
{
  int dbg_level;
	{  // scoping is required to force closure of db-config database
		db_monitor::ConfigDB db;
		dbg_level = db.GetInt(argv[0], "LogLevel", 0);  // 0:Error 1:Debug 2:Info
	}
	
	g_log.set_global_logger(&g_log);
	g_log.set_level(dbg_level);	g_log.set_global_logger(&g_log);
	g_log.open_testdata("trip-monitor");
	ats_logf(ATSLOG_ERROR, "---------- Trip Monitor Started ----------");

	MyData& md = *(new MyData());
	md.set("user", "applet");
	md.set("debug", "1");
	md.set_from_args(argc - 1, argv + 1);

	g_dbg = md.get_int("debug");
	g_log.set_level(g_dbg);

	ats::su(md.get("user"));

	pthread_t skybase_thread;
	pthread_create(&skybase_thread, 0, skybase_thread_fn, &md);

	TripMonitorSm* w = new TripMonitorSm(md);
	w->run_state_machine(*w);

	md.start_server();
	md.wait_exit_sem();

	return 0;
}
