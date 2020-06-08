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
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"

#include "ipsec-monitor.h"

ATSLogger g_log;

int g_dbg = 0;

int ac_debug(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
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

int ac_help(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "This is the help system\n\r");
	return 0;
}

int ac_status(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	std::stringstream output;
	ats::system("/usr/local/sbin/ipsec auto --status | grep -i \"ipsec sa established\" \
			| awk '{print $3}' | awk '{split($0, a, \":\"); print a[1]}' | tr -d '\r' | tr -d '\n' ", &output);
	if(!output.str().empty())
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s connected\n", output.str().c_str());
	else
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "No ipsec connection\n");

	return 0;
}

int ac_reload(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	ats::system("/usr/local/sbin/ipsec auto --rereadall");
	return 0;
}

int ac_restart(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	ats::system("/etc/rc.d/init.d/ipsec restart");
	return 0;
}

int ac_start(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	ats::system("/etc/rc.d/init.d/ipsec start");
	return 0;
}

int ac_stop(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	ats::system("/etc/rc.d/init.d/ipsec stop");
	return 0;
}

int ac_refresh(AdminCommandContext &p_acc, int p_argc, char *p_argv[]) //command format: refresh <policy name>
{
	if(p_argc != 2)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: Wrong commad format\n", __FUNCTION__);
		return 1;
	}

	ats::String cmd;
	const ats::String& policyname = p_argv[1];

	cmd = "/usr/local/sbin/ipsec auto --down " + policyname;
	cmd += "; /usr/local/sbin/ipsec auto --replace " + policyname;
	cmd += "; /usr/local/sbin/ipsec auto --rereadsecrets >/dev/null ";
	cmd += "; /usr/local/sbin/ipsec auto --asynchronous --up " + policyname;

	int ret = ats::system(cmd);
	if(ret)
	{
		ats_logf(ATSLOG(0), "%s,%d:ERROR: ret=%d for cmd=%s", __FILE__, __LINE__, ret, cmd.c_str());
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: fail\n", p_argv[0]);
		return 1;
	}

	return 0;
}

/*
    issue ipsec commmands to turn on connection 'name'
*/
int ac_connect(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	if(p_argc != 2)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: Wrong commad format\n", __FUNCTION__);
		return 1;
	}

	ats::String cmd;
	const ats::String& name = p_argv[1];

	ats::system("/usr/local/sbin/ipsec auto --rereadsecrets >/dev/null");
	cmd = "/usr/local/sbin/ipsec auto --replace " + name;
    ats::system(cmd);
	cmd="/usr/local/sbin/ipsec auto --asynchronous --up " + name;
	ats::system(cmd);
	return 0;
}

/*
    issue ipsec commmands to turn off connection 'name'
*/
int ac_disconnect(AdminCommandContext &p_acc, int p_argc, char *p_argv[])
{
	if(p_argc != 2)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: Wrong commad format\n", __FUNCTION__);
		return 1;
	}

	ats::String cmd;
	const ats::String& name = p_argv[1];

	cmd= "/usr/local/sbin/ipsec auto --down " + name;
    ats::system(cmd);
    ats::system("/usr/local/sbin/ipsec auto --rereadsecrets >/dev/null");
	return 0;
}


int MyData::ipsec_running()
{
    return (access("/var/run/pluto/pluto.pid", 0) != -1) ?  1: 0;
}

static bool is_pppd_running()
{
	const int ret = system("ifconfig ppp0 | grep \"inet addr.* P-t-P\" > /dev/null");
	return ((WIFEXITED(ret) && 0 == WEXITSTATUS(ret)));
}

static bool is_ipsec_unrouted()
{
	const int ret = system("/usr/local/sbin/ipsec auto --status | grep unrouted | grep invalid > /dev/null");
	return ((WIFEXITED(ret) && 0 == WEXITSTATUS(ret)));
}

void *plutoCheck(void *p_md)
{
	MyData& md = *((MyData *)p_md);

	for(;;)
	{
		if(md.getStatus() && is_pppd_running())
		{
			if(!md.ipsec_running() || is_ipsec_unrouted())
				ats::system("/etc/rc.d/init.d/ipsec restart");
		}

		sleep(60);
	}
	return 0;
}

MyData::MyData():flag_start(true)
{
	m_exit_sem = new sem_t;
	sem_init(m_exit_sem, 0, 0);

	m_command.insert(AdminCommandPair("start", ac_start));
	m_command.insert(AdminCommandPair("stop", ac_stop));
	m_command.insert(AdminCommandPair("status", ac_status));
	m_command.insert(AdminCommandPair("reload", ac_reload));
	m_command.insert(AdminCommandPair("restart", ac_restart));
	m_command.insert(AdminCommandPair("refresh", ac_refresh));
	m_command.insert(AdminCommandPair("disconnect", ac_disconnect));
	m_command.insert(AdminCommandPair("connect", ac_connect));
	m_command.insert(AdminCommandPair("debug", ac_debug));
	m_command.insert(AdminCommandPair("help", ac_help));
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
				(i->second)(acc, cb.m_argc, cb.m_argv);
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
	sd.m_port = 41013;
	sd.m_hook = this;
	sd.m_cs = local_command_server;
	::start_server(&sd);
}

int main(int argc, char* argv[])
{
	g_log.open_testdata("ipsec-monitor");

	MyData& md = *(new MyData());
	ats::StringMap &config = md.m_config;
	config.set("debug", "1");
	config.from_args(argc - 1, argv + 1);

	g_dbg = config.get_int("debug");

	std::stringstream v;
	ats::system("cat /version | head -n 1 | tr -d '\r' | tr -d '\n'", &v);

	const ats::String& version = v.str();
	md.setversion(version);

	const ats::String& ss = "ipsec-monitor-" + v.str();
	openlog(ss.c_str(), LOG_PID, LOG_USER);
	ats_logf(&g_log, "Ipsec Monitor Started");

	pthread_t plutocheck_thread;
	{
		const int retval = pthread_create(
			&plutocheck_thread,
			(pthread_attr_t*)0,
			plutoCheck,
			&md);
		if(retval) ats_logf(ATSLOG(0), "%s,%d:ERROR: (%d) Failed to start run_client thread", __FILE__, __LINE__, retval);
	}

	md.start_server();
	md.wait_exit_sem();

	return 0;
}
