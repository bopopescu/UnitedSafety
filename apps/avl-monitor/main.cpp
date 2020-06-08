#include <iostream>
#include <algorithm>

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

#include "feature.h"
#include "db-monitor.h"
#include "ConfigDB.h"
#include "ats-common.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "SocketInterfaceResponse.h"
#include "command_line_parser.h"
#include "LogServer.h"

ATSLogger g_log;

const ats::String g_app_name("avl-monitor");

static int g_dbg = 0;
static int g_msg_enabled;
static int g_max_valid_speed = 239;

// ATS FIXME: This is wrong. Applications should register with AVL-Monitor to receive messages.
//	AVL-Monitor should not be aware of any specific application in its source code.
//	Use the ClientMessageManager class in the socket_interface library. See can-odb2-monitor
//	for an example implementation.
static bool g_has_message_assembler = false;
static bool g_has_proc_seatbelt = false;
static bool g_has_heartbeat = false;
static bool g_has_j1939 = false;
static bool g_has_trip_stats = false;
static bool g_has_trip_monitor = false;

#define IF_PRESENT(P_name, P_EXP) if(g_has_ ## P_name) {P_EXP;}

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

class MyData : public ats::CommonData
{
public:
	ats::StringMap m_config;

	AdminCommandMap m_command;

	MyData();

	size_t m_num_ignition_msg;
	size_t m_num_velocity_msg;
	int m_max_velocity;
	int m_min_velocity;
	int m_cur_velocity;
};

int ac_ignition(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{

	if(1 == p_argc)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s:ok: %d\n\r", p_argv[0], ("on" == p_acc.m_data->get("ignition")) ? 1 : 0);
		ats_logf(ATSLOG(0), "Ignition received but we only have 1 argument");
		return 0;
	}
	else
		ats_logf(ATSLOG(0), "Ignition %s received with %d args", p_argv[1], p_argc);

	p_acc.m_data->set("ignition", p_argv[1]);
	send_cmd(p_acc.get_sockfd(),  MSG_NOSIGNAL, "%s:ok:\n\r", p_argv[0]);

	const bool ignition = p_acc.m_data->get_bool("ignition");

	// ATS FIXME: wifi-client-monitor should use ClientMessageManager, and should not be hard-coded here.
	//	In other words, AVL-Monitor should have apps register for signals at run-time, rather than having
	//	AVL-Monitor be aware of all future apps.
	if(!ignition)
	{
		db_monitor::ConfigDB db;
		
		const int timeout = db.GetInt("system", "WiFiClientAliveTimeoutMinutes", 0);
		const int maxTimeout = db.GetInt("system", "WiFiClientMaxKeepAliveMinutes", 0);

		// only run wifi-client monitor if the settings require it to be run
    
		if(db.GetBool("system", "WiFiStayAwakeEnable", "on") && timeout != 0 && maxTimeout != 0)
		{
			// ATS FIXME: This is a rather heavy-weight call for redundant ignition off signals. However redundant
			//	ignition off signals "should" not occur, and when they do, no one will really care about the
			//	few milliseconds lost (for admin-client to exec wifi-client-monitor again).
			//
			// Solution:
			//	1. Reduce the cost of the call in admin-client
			//	2. Reduce the cost of the exec in wifi-client-monitor
			//	3. Filter out the redundant ignition off here
			//	4. Do nothing because it does not matter enough <-- currently selected
			const char* wifi_client_monitor = "/usr/bin/wifi-client-monitor";
			send_trulink_ud_msg("admin-client", 0, "exec \"%s\" \"%s\"\r", wifi_client_monitor, wifi_client_monitor);
		}

	}

	const char* val = ignition ? "on" : "off";
	ats_logf(ATSLOG(0), "%s,%d: Sending ignition_%s update to apps", __FILE__, __LINE__, val);
	IF_PRESENT(message_assembler, send_app_msg(g_app_name.c_str(), "message-assembler", 0, "msg ignition_%s\r", val))
	IF_PRESENT(message_assembler, ats_logf(ATSLOG(0), "%s,%d: Sending ignition_%s update to message_assembler", __FILE__, __LINE__, val))
	IF_PRESENT(proc_seatbelt, send_app_msg(g_app_name.c_str(), "PROC_Seatbelt", 0, "ignition_%s\r", val))
	IF_PRESENT(heartbeat, send_app_msg(g_app_name.c_str(), "41011", 0, "HeartbeatSm ignition %s\r", val))
	IF_PRESENT(j1939, send_app_msg(g_app_name.c_str(), "41017", 0, "ignition %s\r", val))
	IF_PRESENT(trip_monitor, send_app_msg(g_app_name.c_str(), "trip-monitor", 0, "TripMonitorSm ignition %s\r", val))

	return 0;
}

int ac_velocity(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	MyData& md = *(p_acc.m_data);

	if(1 == p_argc)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s:ok: %d\n\r", p_argv[0], md.m_cur_velocity);
		return 0;
	}

	if(p_argc < 3)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s:error: usage: %s <velocity> <source>\n\r", p_argv[0], p_argv[0]);
		return 0;
	}

	const char* src = p_argv[2];

	if(src != p_acc.m_data->get("velocity_source"))
	{
		ats_logf(ATSLOG(0), "%s,%d: Invalid velocity source. Input was %s. Expectd %s", __FILE__, __LINE__, src,  p_acc.m_data->get("velocity_source").c_str());
		return 0;
	}

	const int velocity = strtol(p_argv[1], 0, 0);

	p_acc.m_data->lock_data();
	md.m_cur_velocity = velocity;

	if(!md.m_num_velocity_msg)
	{
		md.m_max_velocity = velocity;
		md.m_min_velocity = velocity;
	}
	else
	{
		md.m_max_velocity = md.m_max_velocity < velocity ? velocity : md.m_max_velocity;
		md.m_min_velocity = md.m_min_velocity > velocity ? velocity : md.m_min_velocity;
	}

	++md.m_num_velocity_msg;
	md.unlock_data();

	if((velocity >= 0) && (velocity <= g_max_valid_speed))
	{
		ats_logf(ATSLOG(2), "%s,%d: Sending 'speed' update to apps", __FILE__, __LINE__);
		IF_PRESENT(proc_seatbelt, send_app_msg(g_app_name.c_str(), "PROC_Seatbelt", 0, "speed %d\r", velocity))
		IF_PRESENT(message_assembler, send_app_msg(g_app_name.c_str(), "message-assembler", 0, "speed %d\r", velocity))
		IF_PRESENT(trip_stats, send_app_msg(g_app_name.c_str(), "trip-stats", 0, "speed %d\r", velocity))
		IF_PRESENT(trip_monitor, send_app_msg(g_app_name.c_str(), "trip-monitor", 0, "TripMonitorSm speed %d\r", velocity))
	}

	return 0;
}

MyData::MyData()
{
	m_command.insert(AdminCommandPair("ignition", ac_ignition));
	m_command.insert(AdminCommandPair("velocity", ac_velocity));

	m_num_ignition_msg = 0;
	m_num_velocity_msg = 0;
	m_max_velocity = 0;
	m_min_velocity = 0;
	m_cur_velocity = 0;
}

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

	ClientDataCache cache;
	init_ClientDataCache(&cache);

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cache);

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

		AdminCommandMap* cmd_map = &(md.m_command);
		int argc = cb.m_argc;
		char** argv = cb.m_argv;

		cmd.assign(argv[0]);
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

void load_config(MyData& p_md)
{
	db_monitor::ConfigDB db;
	const ats::String& err = db.open_db_config();

	if(!err.empty())
	{
		ats_logf(ATSLOG(0), "%s,%d: ConfigDB returned \"%s\"", __FILE__, __LINE__, err.c_str());
	}

	p_md.set("velocity_source", "GPS");
}

bool log_avl(LogServer& p_ls, LogServer::LogRequest& p_log_request)
{
	MyData& md = *((MyData*)p_ls.m_data);
	md.lock_data();	
	const size_t num_ignition = md.m_num_ignition_msg;
	const size_t num_vel = md.m_num_velocity_msg;
	const int max_vel = md.m_max_velocity;
	const int min_vel = md.m_min_velocity;
	const int cur_vel = md.m_cur_velocity;
	md.unlock_data();

	std::vector <char>& buf = p_ls.m_log_buf;
	snprintf(buf.data(), int(buf.size() - 1),
		"%s"    // ignition_source
		",%s"   // velocity_source
		",%zu"  // num_ignition
		",%zu"  // num_vel
		",%d"   // max_vel
		",%d"   // min_vel
		",%d"   // cur_vel
		",%d"   // g_msg_enabled
		",%d",  // g_max_valid_speed
		md.get("ignition_source").c_str(),
		md.get("velocity_source").c_str(),
		num_ignition,
		num_vel,
		max_vel,
		min_vel,
		cur_vel,
		g_msg_enabled ? 1 : 0,
		g_max_valid_speed);

	return true;
}

int main(int argc, char* argv[])
{
	g_log.set_global_logger(&g_log);

	MyData md;
	ats::StringMap &config = md.m_config;

	load_config(md);

	config.from_args(argc - 1, argv + 1);

	g_dbg = config.get_int("debug");
	g_log.set_level(g_dbg);

	static LogServer ls;
	ls.m_data = &md;
	ls.set_logger("avl", log_avl);
	ls.start_log_server();

	{
		const ats::String& user = config.get("user");
		ats::su(user.empty() ? "avl-monitor" : user);
	}

	g_log.open_testdata(g_app_name);

	ats_logf(ATSLOG(0), "AVL Monitor Started");

	wait_for_app_ready("feature-monitor");

	FeatureQuery fq;
	g_has_message_assembler = fq.feature_on("message-assembler");
	g_has_proc_seatbelt = fq.feature_on("PROC_Seatbelt");
	g_has_heartbeat = fq.feature_on("heartbeat");
	g_has_j1939 = fq.feature_on("j1939");
	g_has_trip_stats = fq.feature_on("trip-stats");
	g_has_trip_monitor = fq.feature_on("trip-monitor");

	ServerData server_data;
	{
		ServerData* sd = &server_data;
		init_ServerData(sd, 64);
		sd->m_hook = &md;
		sd->m_cs = local_command_server;
		start_redstone_ud_server(sd, g_app_name.c_str(), 1);
		signal_app_unix_socket_ready(g_app_name.c_str(), g_app_name.c_str());
	}

	signal_app_ready(g_app_name.c_str());

	ats::infinite_sleep();
	return 0;
}
