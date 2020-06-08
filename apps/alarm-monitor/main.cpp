//--------------------------------------------------------------------------------------------------
//  alarm-monitor -  all non-device related incoming messages from packetizer-calamps will be
//                   redirected here.  If the device ID is for the TruLink then we can generate an 
//                   alarm or terminate an alarm.
//                  
// Dave Huff - Sept 2015 - Needed for Cermaq.

#include <iostream>
#include <map>
#include <list>
#include <fstream>
#include <iomanip>

#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/ioctl.h>

#include "ats-common.h"
#include "ats-string.h"
#include "atslogger.h"
#include "timer-event.h"

#include "zigbee-monitor.h"

ATSLogger g_log;
MyData* g_md = NULL;

const int g_power_monitor_port = 41009;
ClientSocket g_cs_powermonitor;


#define CHECKPOINTER if(fm  == NULL)\
		{\
			ats_logf(ATSLOG(0), "%s,%d: Object is not FOBMessage type", __FILE__, __LINE__);\
			if(m1.ptr != NULL)\
				delete m1.ptr;\
			continue;\
		}\

static int g_dbg = 0;
static const ats::String g_app_name("alarm-monitor");



// ************************************************************************
// Description: Command server.
//
// Parameters:  p - pointer to ClientData
// Returns:     NULL pointer
// ************************************************************************
static void* client_command_server(void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData &md = *((MyData *)(cd->m_data->m_hook));

	bool command_too_long = false;
	ats::String cmd;

	const size_t max_command_length = 1024;

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cdc);

		if(c < 0)
		{
			if(c != -ENODATA) ats_logf(ATSLOG(3), "%s,%d: %s", __FILE__, __LINE__, ebuf);
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
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "xml <devResp error=\"command is too long\"></devResp>\r");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char *err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			ats_logf(ATSLOG(0), "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "xml <devResp error=\"gen_arg_list failed\">%s</devResp>\r", err);
			cmd.clear();
			continue;
		}
		cmd.clear();

		if(cb.m_argc <= 0)
		{
			continue;
		}

		const ats::String cmd(cb.m_argv[0]);
		ats::StringMap args;
		args.from_args(cb.m_argc - 1, cb.m_argv + 1);

		if(("debug" == cmd) || ("dbg" == cmd))
		{

			if(cb.m_argc >= 2)
			{
				g_dbg = strtol(cb.m_argv[1], 0, 0);
				g_log.set_level(g_dbg);
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: debug=%d\r\n", cmd.c_str(), g_dbg);
			}
			else
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: debug=%d\r\n", cmd.c_str(), g_dbg);
			}

		}
		else
		{
			CommandMap::const_iterator i = md.m_cmd.find(cmd);

			if(i != md.m_cmd.end())
			{
				Command& c = *(i->second);
				md.lock_data();
				c.fn(md, *cd, cb);
				md.unlock_data();
				
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
	sd.m_cs = client_command_server;
	::start_redstone_ud_server(&sd, "alarm-monitor", 1);
	signal_app_unix_socket_ready("alarm-monitor", "alarm-monitor");
	signal_app_ready("alarm-monitor");
}



int main(int argc, char* argv[])
{
	g_log.set_global_logger(&g_log);
	g_log.set_level(g_dbg);
	g_log.open_testdata(g_app_name);

	init_ClientSocket(&g_cs_powermonitor);

	if(!ats::testmode())
	{

		if(connect_client(&g_cs_powermonitor, "127.0.0.1", g_power_monitor_port))
		{
			ats_logf(&g_log, "%s,%d:g_cs_powermonitor %s", __FILE__, __LINE__, g_cs_powermonitor.m_emsg);
			exit(1);
		}
	}

	static MyData md;
	g_md = &md;

	ats::StringMap &config = md.m_config;

	config.set("user", "root");
	config.from_args(argc - 1, argv + 1);

	g_dbg = config.get_int("debug");

	ats_logf(ATSLOG(0), "alarm-monitor started");

	md.m_cmd.insert(CommandPair("calamp", new calampCommand()));
	md.m_cmd.insert(CommandPair("iridium", new iridiumCommand()));

	md.start_server();

	if(ats::su(config.get("user")))
	{
		ats_logf(ATSLOG(0), "Could not become user \"%s\": ERR (%d): %s", md.get("user").c_str(), errno, strerror(errno));
		return 1;
	}

	ats::infinite_sleep();

	return 0;
}

