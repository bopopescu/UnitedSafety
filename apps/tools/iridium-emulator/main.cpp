#include <iostream>
#include <algorithm>
#include <set>

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
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <pwd.h>

#include "ats-common.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "SocketInterfaceResponse.h"
#include "command_line_parser.h"
#include "buzzer-monitor.h"
#include "NMEA_Client.h"
#include "RedStone_IPC.h"
#include "geoconst.h"

#define to_base64(P_s) ats::base64_encode(P_s)
#define from_base64(P_s) ats::base64_decode(P_s)

ATSLogger g_log;

FILE* g_app_f = 0;
int g_app_fd = 0;

int g_dbg = 0;
int g_buzz_length = 0;

static const char* g_iid = 0;

class MyData;

class AdminCommandContext
{
public:
	AdminCommandContext(MyData& p_data, ClientSocket& p_socket)
	{
		m_data = &p_data;
		m_socket = &p_socket;
		m_cd = 0;
	}

	AdminCommandContext(MyData& p_data, ClientData& p_cd)
	{
		m_data = &p_data;
		m_socket = 0;
		m_cd = &p_cd;
	}

	int get_sockfd() const
	{
		return (m_cd ? m_cd->m_sockfd : m_socket->m_fd);
	}

	MyData* m_data;

private:
	ClientSocket* m_socket;
	ClientData* m_cd;
};

typedef int (*AdminCommand)(AdminCommandContext&, int p_argc, char* p_argv[]);
typedef std::map <const ats::String, AdminCommand> AdminCommandMap;
typedef std::pair <const ats::String, AdminCommand> AdminCommandPair;

class MyData
{
public:
	ats::StringMap m_config;

	// Description: General RedStone commands.
	AdminCommandMap m_command;

	// Description: RedStone commands for system administrators.
	AdminCommandMap m_admin_cmd;

	// Description: RedStone commands for developers.
	AdminCommandMap m_dev_cmd;

	MyData();

	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	sem_t* m_gsm_sem;

private:
	pthread_mutex_t* m_mutex;
	NMEA_Client	nmea_client;
};

MyData::MyData()
{
	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

	m_gsm_sem = new sem_t;
	sem_init(m_gsm_sem, 0 ,0);
}

MyData g_md;

int g_fd = -1;

// ************************************************************************
// Description: A local command server so that other applications or
//	developers can query admin-client directly (instead of having to
//	go through AdminServer and the SSH tunnel).
//
//	This function supports the same command set as "client_for_AdminServer".
//
//	One instance of this function is created for every local connection
//	made.
//
// Parameters:
// Return:	NULL pointer
// ************************************************************************
static void* local_command_server(void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData& md = *((MyData*)(cd->m_data->m_hook));

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
			if(c != -ENODATA) ats_logf(ATSLOG(0), "%s,%d:ERROR: %s", __FILE__, __LINE__, ebuf);
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
			ats_logf(ATSLOG(0), "%s,%d:ERROR: command is too long", __FILE__, __LINE__);
			cmd.clear();
			command_too_long = false;
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "xml <devResp error=\"command is too long\"></devResp>\r");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char* err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			ats_logf(ATSLOG(0), "%s,%d:ERROR: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "xml <devResp error=\"gen_arg_list failed\">%s</devResp>\r", err);
			cmd.clear();
			continue;
		}

		ats_logf(ATSLOG(2), "%s,%d:%s: Command received: %s", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());

		cmd.clear();

		if(cb.m_argc <= 0)
		{
			continue;
		}

		ats::String cmd;
		AdminCommandMap* cmd_map = &(md.m_command);
		int argc = cb.m_argc;
		char** argv = cb.m_argv;

		if(!strcmp("dev", argv[0]) && (argc >= 2))
		{
			cmd.assign(argv[1]);
			argc -= 1;
			argv += 1;
			cmd_map = &(md.m_dev_cmd);
		}
		else if(!strcmp("adm", argv[0]) && (argc >= 2))
		{
			cmd.assign(argv[1]);
			argc -= 1;
			argv += 1;
			cmd_map = &(md.m_admin_cmd);
		}
		else
		{
			cmd.assign(argv[0]);
		}

		{
			AdminCommandMap::const_iterator i = cmd_map->find(cmd);

			if(i != cmd_map->end())
			{
				(i->second)(acc, argc, argv);
			}
			else
			{
				send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "Invalid command \"%s\"\r", cmd.c_str());
			}

		}
	}
	return 0;
}

static void* signal_monitor(void*)
{

	for(;;)
	{
		sleep(5);
		ats::String resp;
		ats_sprintf(&resp, "\r\n+CIEV:1,1\r\n");
		g_md.lock();
		write(g_app_fd, resp.c_str(), resp.length());
		g_md.unlock();
	}

	return 0;
}

static void email_iridium_data(const ats::String& p_id, const ats::String& p_data)
{
	std::stringstream date;
	ats::system("date|tr -d '\n'", &date);

	ats::String gps_s;
	{
		std::stringstream g;
		ats::system("NMEA show=NMEA_Client|grep '^\\(Lat\\|Lon\\):'|sed 's/.*: //'", &g);

		ats::StringList gps;
		ats::split(gps, g.str(), "\n");

		if(gps.size() >= 2)
		{
			gps_s = "Lat = " + gps[0] + " Long = " + gps[1];
		}

	}

	mkdir("/tmp/iridium-emulator", 0777);

	ats::String body_fname;
	ats_sprintf(&body_fname, "/tmp/iridium-emulator/body.txt");

	{
		FILE* f = fopen(body_fname.c_str(), "w");

		if(f)
		{
			fprintf(f, "MOMSN: 2598\n");
			fprintf(f, "MTMSN: 0\n");
			fprintf(f, "Time of Session (UTC): %s\n", date.str().c_str());
			fprintf(f, "Session Status: 00 - Transfer OK\n");
			fprintf(f, "Message Size (bytes): %d\n", p_data.length());
			fprintf(f, "\n");
			fprintf(f, "Unit Location: %s\n", gps_s.c_str());
			fprintf(f, "CEPradius = 7\n");
		}

	}

	ats::String fname;
	ats_sprintf(&fname, "/tmp/iridium-emulator/%s.sbd", p_id.c_str());

	{
		FILE* f = fopen(fname.c_str(), "w");

		if(f)
		{
			fwrite(p_data.c_str(), 1, p_data.length(), f);
			fclose(f);
		}

	}

	ats::system("send-iridium-email.sh \"SBD Msg From Unit: " + p_id + "\" /tmp/iridium-emulator/body.txt /tmp/iridium-emulator/" + p_id + ".sbd");
}

int main(int argc, char* argv[])
{
	g_log.set_global_logger(&g_log);
	g_log.open_testdata("iridium-emulator");

	MyData& md = g_md;
	ats::StringMap &config = md.m_config;
	config.set("app-dev", "/dev/iridium-emulator-APP");
	config.from_args(argc - 1, argv + 1);

	g_iid = getenv("IID");

	if(!g_iid)
	{
		g_iid = "";
	}

	ats::system("rm -f /dev/ttySP4;ln -s /dev/iridium-emulator-AT /dev/ttySP4");

	g_dbg = config.get_int("debug");
	g_log.set_level(g_dbg);

	g_app_fd = open(config.get("app-dev").c_str(), O_RDWR);

	ats_logf(ATSLOG(0), "Iridium Emulator App started. IID='%s', RECIPIENT='%s'", g_iid, getenv("RECIPIENT"));

	ServerData sd;
	init_ServerData(&sd, 64);
	sd.m_hook = &md;
	sd.m_cs = local_command_server;
	::start_redstone_ud_server(&sd, "iridium-emulator", 1);

	ats::ReadDataCache_fd rdc(g_app_fd);

	ats::String cmdline;

	bool echo = true;

	pthread_t thread;
	bool signal_monitor_started = false;

	for(;;)
	{
		int c = rdc.getc();

		if(c < 0)
		{
			// Error may be temporary, try again.
			c = rdc.getc();

			if(c < 0)
			{
				ats_logf(ATSLOG(0), "%s,%d: (%d) read failed\n", __FILE__, __LINE__, c);
				break;
			}

		}

		if('\r' != c)
		{
			cmdline += c;
			continue;
		}

		ats::String cmd(cmdline);
		cmdline.clear();
		ats_logf(ATSLOG(0), "Command is \"%s\"", cmd.c_str());

		ats::String resp;

		if(echo)
		{
			ats_sprintf(&resp, "%s\n", cmd.c_str());
			write(g_app_fd, resp.c_str(), resp.length());
		}

		if("ATE0" == cmd)
		{
			echo = false;
			ats_sprintf(&resp, "\r\nOK\r\n");
			write(g_app_fd, resp.c_str(), resp.length());
		}
		else if("AT*R1" == cmd)
		{
			ats_sprintf(&resp, "\r\nOK\r\n");
			write(g_app_fd, resp.c_str(), resp.length());
		}
		else if("AT+SBDAREG=1" == cmd)
		{
			ats_sprintf(&resp, "\r\nOK\r\n");
			write(g_app_fd, resp.c_str(), resp.length());
		}
		else if("AT+SBDMTA=1" == cmd)
		{
			ats_sprintf(&resp, "\r\nOK\r\n");
			write(g_app_fd, resp.c_str(), resp.length());
		}
		else if("AT+CIER=1,1,1" == cmd)
		{
			ats_sprintf(&resp, "\r\nOK\r\n\r\n+CIEV:1,1\r\n");
			write(g_app_fd, resp.c_str(), resp.length());

			if(!signal_monitor_started)
			{
				ats_logf(ATSLOG(0), "%s,%d: Starting signal monitor...", __FILE__, __LINE__);
				pthread_create(&thread, 0, signal_monitor, 0);
				signal_monitor_started = true;
				ats_logf(ATSLOG(0), "%s,%d: Signal monitor started...", __FILE__, __LINE__);
			}

		}
		else if("AT+SBDIX" == cmd)
		{
			ats_sprintf(&resp, "\r\n+SBDIX: 0, 1, 0, 0, 0, 0\r\n");
			write(g_app_fd, resp.c_str(), resp.length());
		}
		else if(0 == cmd.find("AT+SBDWB="))
		{
			md.lock();

			bool fail = false;
			int length = atoi(cmd.substr(9).c_str());
			ats::String data;

			if(length > 0)
			{
				length += 2;
			}

			ats_sprintf(&resp, "\r\nREADY\r\n");
			write(g_app_fd, resp.c_str(), resp.length());

			ats_logf(ATSLOG(0), "%s,%d: Waiting for %d bytes...", __FILE__, __LINE__, length);

			for(; length > 0; length--)
			{
				int c = rdc.getc();

				if(c < 0)
				{
					// Error may be temporary, try again.
					c = rdc.getc();

					if(c < 0)
					{
						ats_logf(ATSLOG(0), "%s,%d: (%d) read failed\n", __FILE__, __LINE__, c);
						fail = true;
						break;
					}

				}

				data.push_back(c);
			}

			if(fail)
			{
				md.unlock();
				ats_logf(ATSLOG(0), "%s,%d: failed", __FILE__, __LINE__);
				break;
			}

			ats_sprintf(&resp, "0\r\n");
			write(g_app_fd, resp.c_str(), resp.length());
			md.unlock();

			ats_logf(ATSLOG(0), "%s,%d: Bytes read are: (%d) %s", __FILE__, __LINE__, data.length(), ats::to_hex(data).c_str());

			email_iridium_data(g_iid, data);
		}
		else
		{
			ats_sprintf(&resp, "\r\nOK\r\n");
			write(g_app_fd, resp.c_str(), resp.length());
		}

	}

	return 0;
}
