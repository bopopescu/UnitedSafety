#include <errno.h>

#include "ats-common.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "ConfigDB.h"
#include "IridiumSender.h"

ATSLogger g_log;
const ats::String g_app_name = "iridium-monitor";

static int g_dbg = 0;
static IridiumSender g_ird_sender;
static pthread_mutex_t g_sender_mutex;

// Max Iridium message length is 1960 bytes, max command length is 20 bytes
// Each Iridium message byte is two characters in command so max command length is (1960*2 + 20)
#define MAX_IRIDIUM_COMMAND_LENGTH 3940

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

typedef int (*AdminCommand) (AdminCommandContext&, int p_argc, char* p_argv[]);
typedef std::map <const ats::String, AdminCommand> AdminCommandMap;
typedef std::pair <const ats::String, AdminCommand> AdminCommandPair;

void lockSender()
{
	pthread_mutex_lock(&g_sender_mutex);
}
void unlockSender()
{
	pthread_mutex_unlock(&g_sender_mutex);
}

class MyData : public ats::CommonData
{
public:
	ats::StringMap m_config;
	AdminCommandMap m_command;

	MyData();
};

int ac_ready(AdminCommandContext& p_acc, int , char* p_argv[])
{
	if(g_ird_sender.isNetworkAvailable())
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s:ok:yes\n\r", p_argv[0]);
		return 0;
	}
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s:ok:no\n\r", p_argv[0]);
	return 0;
}

int ac_send(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	if(p_argc < 2)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s:error: %s <message>\n\r", p_argv[0], p_argv[0]);
		return 0;
	}

	ats::String buf = ats::from_hex(p_argv[1]);
	std::vector<char> vbuf;
	vbuf.insert(vbuf.begin(), buf.begin(), buf.end());

	lockSender();
	g_ird_sender.sendMessage(vbuf);
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s:ok: %d\n\r", p_argv[0], g_ird_sender.error());
	unlockSender();

	return 0;
}

int ac_sendtdata(AdminCommandContext& p_acc, int p_argc, char* p_argv[])
{
	if(p_argc < 2)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s:error: %s <message>\n\r", p_argv[0], p_argv[0]);
		return 0;
	}

	ats::String buf = ats::from_hex(p_argv[1]);
	std::vector<char> vbuf;
	vbuf.insert(vbuf.begin(), buf.begin(), buf.end());

	lockSender();
	g_ird_sender.sendMessageWithDataLimit(vbuf);
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s:ok: %d\n\r", p_argv[0], g_ird_sender.error());
	unlockSender();

	return 0;
}

int ac_status(AdminCommandContext& p_acc, int, char* p_argv[])
{
	switch(IRIDIUM::getStatus())
	{
	case IRIDIUM::PASSED:
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s:ok: passed\n\r", p_argv[0]);
		break;
	case IRIDIUM::FAILED:
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s:ok: failed\n\r", p_argv[0]);
		break;
	case IRIDIUM::NOT_READY:
	default:
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s:ok: not_ready\n\r", p_argv[0]);
	}
	return 0;
}

//298
int ac_response(AdminCommandContext& p_acc, int, char* p_argv[])
{
	char response[40];
	//ats_logf(ATSLOG_INFO, "%s,%d:ac_response", __FILE__, __LINE__);

	g_ird_sender.IridiumGetResponse((char *)response);
	//ats_logf(ATSLOG_INFO, "%s,%d:ac_response Step 2", __FILE__, __LINE__);

	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL,"%s:response:%s\n\r", p_argv[0], response);
	//ats_logf(ATSLOG_INFO, "%s,%d:ac_response Step 3", __FILE__, __LINE__);
	return 0;
}

MyData::MyData(){
	m_command.insert(AdminCommandPair("ready", ac_ready));
	m_command.insert(AdminCommandPair("send", ac_send));
	m_command.insert(AdminCommandPair("sendtdata", ac_sendtdata));
	m_command.insert(AdminCommandPair("status", ac_status));
	//298
	m_command.insert(AdminCommandPair("response", ac_response));
}

static void* local_command_server(void* p)
{
	ClientData * cd = (ClientData*)p;
	MyData& md = *((MyData*)(cd->m_data->m_hook));
	bool command_too_long = false;
	ats::String cmd;
	const size_t max_command_length = MAX_IRIDIUM_COMMAND_LENGTH;

	AdminCommandContext acc(md, *cd);

	ClientDataCache cache;
	init_ClientDataCache(&cache);

	CommandBuffer cb;
	init_CommandBuffer(&cb);
	alloc_dynamic_buffers(&cb, MAX_COMMAND_ARGS, MAX_IRIDIUM_COMMAND_LENGTH);

	for(;;)
	{
		char ebuf[MAX_IRIDIUM_COMMAND_LENGTH];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cache);
		if(c < 0)
		{
			if(c != -ENODATA) ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: %s", __FILE__, __LINE__, ebuf);
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
			ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: command is too long", __FILE__, __LINE__);
			cmd.clear();
			command_too_long = false;
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "xml <devResp error=\"command is too long\"></devResp>\r");
			continue;
		}

		const char* err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
				ats_logf(ATSLOG_ERROR, "%s,%d:ERROR: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
				send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "xml <devResp error=\"gen_arg_list failed\">%s</devResp>\r", err);
				cmd.clear();
				continue;
		}

		if (strcmp(cmd.c_str(), "ready") == 0)
			ats_logf(ATSLOG_INFO, "%s,%d:%s: Command received: %s", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());

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
		cmd.clear();

	}
	free_dynamic_buffers(&cb);
	return 0;
}

int main(int argc, char* argv[])
{
	{  // scoping is required to force closure of db-config database
		db_monitor::ConfigDB db;
		g_dbg = db.GetInt(argv[0], "LogLevel", 0);  // 0:Error 1:Debug 2:Info
	}

	g_log.set_global_logger(&g_log);
	g_log.set_level(g_dbg);
	g_log.open_testdata(g_app_name);

	MyData md;
	// Now flash the led 
	wait_for_app_ready("i2c-gpio-monitor");
  // indicate Iridium enabled by showing solid red
	send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.g led=sat script=\"0,1000000\" \r");  
 	send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=iridium-monitor.r led=sat.r script=\";1,1000000\" \r");  

	pthread_mutex_init(&g_sender_mutex,0);
	g_ird_sender.start();

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
