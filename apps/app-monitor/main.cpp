#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>

#include "ats-common.h"
#include "socket_interface.h"
#include "ClientMessageManager.h"
#include "command_line_parser.h"
#include "AdminCommandContext.h"
#include "Message.h"
#include "Messenger.h"

static int g_dbg = 0;

class MyData : public ats::CommonData
{
public:
	typedef std::map <const ats::String, Messenger> MessengerMap;
	typedef std::pair <const ats::String, Messenger> MessengerPair;

	class ReadyMutex
	{
	public:
		ReadyMutex()
		{
			m_ref_count = 0;
		}

		pthread_mutex_t m_mutex;
		size_t m_ref_count;
	};

	typedef std::map <const ats::String, ReadyMutex> ReadyMutexMap;
	typedef std::pair <const ats::String, ReadyMutex> ReadyMutexPair;

	ReadyMutexMap m_rdy_mutex;

	void wait_until_ready(const ats::String& p_app)
	{
		lock_data();
		std::pair <ReadyMutexMap::iterator, bool> r = m_rdy_mutex.insert(ReadyMutexPair(p_app, ReadyMutex()));

		ReadyMutex& m = (r.first)->second;

		if(r.second)
		{
			pthread_mutex_init(&(m.m_mutex), 0);
			pthread_mutex_lock(&(m.m_mutex));
		}

		++(m.m_ref_count);
		unlock_data();

		pthread_mutex_lock(&(m.m_mutex));
		lock_data();
		--(m.m_ref_count);
		unlock_data();
		pthread_mutex_unlock(&(m.m_mutex));
	}

	void set_ready(const ats::String& p_app)
	{
		lock_data();
		std::pair <ReadyMutexMap::iterator, bool> r = m_rdy_mutex.insert(ReadyMutexPair(p_app, ReadyMutex()));

		pthread_mutex_t& m = ((r.first)->second).m_mutex;

		const int ready_count = m_ready.get_int(p_app);
		m_ready.set(p_app, ats::toStr(ready_count + 1));

		if(r.second)
		{
			pthread_mutex_init(&m, 0);
		}
		else
		{
			pthread_mutex_unlock(&m);
		}

		unlock_data();
	}

	MyData();

	void create_messenger(const ats::String& p_socket, bool p_is_uds);

	static ats::ClientMessageQueue<Message>* create_message_queue()
	{
		return new ats::ClientMessageQueue<Message>();
	}

	// Description: Creates a new client message queue, but only if a message queue for client
	//	"p_client" does not already exist.
	void create_client_message_queue(const ats::String& p_client)
	{

		if(m_mm.add_client(p_client, create_message_queue))
		{
			m_mm.start_client(p_client);
		}

	}

	void send_message(const ats::String& p_sender_id, const ats::String& p_destination, const char* p_msg)
	{
		struct timeval t;
		gettimeofday(&t, 0);

		if(p_sender_id.empty() || p_destination.empty() || (!p_msg) || ('\0' == *p_msg))
		{
			return;
		}

		create_client_message_queue(p_destination);
		m_mm.post_msg(Message(p_sender_id, t, p_msg), p_destination);
	}

	// Description: Lists of threads that will deliver messages for each Unix domain socket
	//	or regular socket.
	MessengerMap m_messenger;

	// Description: List of applications that are "ready".
	ats::StringMap m_ready;

	// Description: Queue of messages destined for Unix domain sockets or regular sockets.
	ats::ClientMessageManager <Message> m_mm;

	ats::ClientMessageManager <void*> m_rdy;

	AdminCommandMap m_command;
};

void* messenger_service(void* p)
{
	Messenger& m = *((Messenger*)p);
	MyData& md = m.m_md;
	ats::ClientMessageManager<Message>& mm = md.m_mm;

	const ats::String& key = *(m.m_key);
	const int port = m.m_is_uds ? -1 : strtol(key.c_str(), 0, 0);

	md.create_client_message_queue(key);

	for(;;)
	{
		{
			void* dont_care;
			md.m_rdy.wait_msg(key, dont_care);
		}

		for(;;)
		{
			Message msg;

			if(!(mm.wait_msg(key, msg)))
			{
				break;
			}

			if(m.m_is_uds)
			{

				if(send_redstone_ud_msg(key.c_str(), 0, "%s\r", msg.m_msg.c_str()) < 0)
				{
					break;
				}

			}
			else
			{

				if(send_msg("localhost", port, 0, "%s\r", msg.m_msg.c_str()) < 0)
				{
					break;
				}

			}

		}

	}

	return 0;
}

void MyData::create_messenger(const ats::String& p_socket, bool p_is_uds)
{
	lock_data();
	std::pair <MessengerMap::iterator, bool> r = m_messenger.insert(MessengerPair(p_socket, Messenger(*this)));

	if(r.second)
	{
		Messenger& m = (r.first)->second;
		m.m_key = &((r.first)->first);
		m.m_is_uds = p_is_uds;
		m_rdy.add_client(p_socket, new ats::ClientMessageQueue<void*>());
		m_rdy.start_client(p_socket);
		pthread_create(&(m.m_thread), 0, messenger_service, &m);
	}
	unlock_data();

	m_rdy.post_msg((void*)0, p_socket);
}

// Description: Marks an application as "ready".
//
// Command Format:
//	ready <app name>
int ac_ready(AdminCommandContext& p_acc, const CommandBuffer* p_cb)
{

	if(p_cb->m_argc < 2)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: error\n\tusage: %s <app name>\n\r", p_cb->m_argv[0], p_cb->m_argv[0]);
		return -1;
	}

	MyData& md = *(p_acc.m_data);
	md.set_ready(p_cb->m_argv[1]);
	return 0;
}

// Description: Registers a Unix domain socket (uds) that will accpet connections.
//
// Command Format:
//	uds <app name> <socket name>
int ac_uds(AdminCommandContext& p_acc, const CommandBuffer* p_cb)
{

	if(p_cb->m_argc < 3)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: error\n\tusage: %s <app name> <socket name>\n\r", p_cb->m_argv[0], p_cb->m_argv[0]);
		return -1;
	}

	const ats::String socket_name(p_cb->m_argv[2]);
	MyData& md = *(p_acc.m_data);
	md.create_messenger(socket_name, true);
	return 0;
}

// Description: Registers a socket that will accpet connections.
//
// Command Format:
//	sck <app name> <port number>
int ac_sck(AdminCommandContext& p_acc, const CommandBuffer* p_cb)
{

	if(p_cb->m_argc < 3)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: error\n\tusage: %s <app name> <port number>\n\r", p_cb->m_argv[0], p_cb->m_argv[0]);
		return -1;
	}

	const ats::String port_number(p_cb->m_argv[2]);
	MyData& md = *(p_acc.m_data);
	md.create_messenger(port_number, false);
	return 0;
}

// Description: Displays status information for app-monitor.
//
// Command Format:
//	stat
int ac_stat(AdminCommandContext& p_acc, const CommandBuffer* p_cb)
{
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s: ok\n", p_cb->m_argv[0]);

	ats::StringMap sm;
	sm.from_args(p_cb->m_argc - 1, p_cb->m_argv + 1);

	MyData& md = *(p_acc.m_data);
	md.lock_data();
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "\tReady Applications (%zu):\n", md.m_ready.size());
		ats::StringMap::const_iterator i = md.m_ready.begin();

		while(i != md.m_ready.end())
		{
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "\t\t[%s, %s]\n", (i->first).c_str(), (i->second).c_str());
			++i;
		}

		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "\tReady Queues:\n");
		{
			std::stringstream s;
			s << md.m_rdy;
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s\n", s.str().c_str());
		}

		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "\tMessage Queues:\n");
		{
			std::stringstream s;

			if(!sm.has_key("max_msg"))
			{
				sm.set("max_msg", "10");
			}

			md.m_mm.print(s, sm);
			send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "%s\n", s.str().c_str());
		}
	}
	md.unlock_data();
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "\r");
	return 0;
}

MyData::MyData()
{
	m_command.insert(AdminCommandPair("ready", ac_ready));
	m_command.insert(AdminCommandPair("uds", ac_uds));
	m_command.insert(AdminCommandPair("sck", ac_sck));
	m_command.insert(AdminCommandPair("stat", ac_stat));
}

static void* local_command_server(void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData& md = *((MyData*)(cd->m_data->m_hook));

	bool command_too_long = false;
	ats::String cmdline;

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
			if(c != -ENODATA) syslog(LOG_ERR, "%s,%d:ERROR: %s", __FILE__, __LINE__, ebuf);
			break;
		}

		if(c != '\r' && c != '\n')
		{
			if(cmdline.length() >= max_command_length) command_too_long = true;
			else cmdline += c;

			continue;
		}

		if(command_too_long)
		{
			cmdline.clear();
			command_too_long = false;
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "error: command is too long\n\r");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char* err;

		if((err = gen_arg_list(cmdline.c_str(), cmdline.length(), &cb)))
		{
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "error: gen_arg_list failed: %s\n\r", err);

		}
		else if(cb.m_argc > 0)
		{
			AdminCommandMap* cmd_map = &(md.m_command);
			const ats::String cmd(cb.m_argv[0]);
			AdminCommandMap::const_iterator i = cmd_map->find(cmd);

			if(i != cmd_map->end())
			{
				(i->second)(acc, &cb);
			}
			else
			{
				send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "Invalid command \"%s\"\r", cmd.c_str());
			}

		}

		cmdline.clear();
	}

	return 0;
}

static void* ready_server(void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData& md = *((MyData*)(cd->m_data->m_hook));

	bool command_too_long = false;
	ats::String cmdline;

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
			if(c != -ENODATA) syslog(LOG_ERR, "%s,%d:ERROR: %s", __FILE__, __LINE__, ebuf);
			break;
		}

		if(c != '\r' && c != '\n')
		{
			if(cmdline.length() >= max_command_length) command_too_long = true;
			else cmdline += c;

			continue;
		}

		if(command_too_long)
		{
			syslog(LOG_ERR, "%s,%d:ERROR: command is too long", __FILE__, __LINE__);
			cmdline.clear();
			command_too_long = false;
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "error: command is too long\n\r");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char* err;

		if((err = gen_arg_list(cmdline.c_str(), cmdline.length(), &cb)))
		{
			syslog(LOG_ERR, "%s,%d:ERROR: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "error: gen_arg_list failed: %s\n\r", err);
		}
		else if(cb.m_argc > 0)
		{
			const ats::String app(cb.m_argv[0]);

			if(app.empty())
			{
				send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "error: No app name given.\nusage <app name>\n\r");
			}
			else
			{
				md.wait_until_ready(app);
				break;
			}

		}

		cmdline.clear();
	}

	return 0;
}

static void get_msg_part(ats::String& p_s, const char*& p_msg)
{

	for(;;)
	{

		if(*p_msg == ',')
		{
			++p_msg;
			break;
		}

		if(!(*p_msg))
		{
			break;
		}

		p_s += *p_msg;
		++p_msg;
	}

}

static void* msg_server(void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData& md = *((MyData*)(cd->m_data->m_hook));

	bool command_too_long = false;
	ats::String cmdline;

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
			if(c != -ENODATA) syslog(LOG_ERR, "%s,%d:ERROR: %s", __FILE__, __LINE__, ebuf);
			break;
		}

		if(c != '\r')
		{
			if(cmdline.length() >= max_command_length) command_too_long = true;
			else cmdline += c;

			continue;
		}

		if(command_too_long)
		{
			syslog(LOG_ERR, "%s,%d:ERROR: command is too long", __FILE__, __LINE__);
			cmdline.clear();
			command_too_long = false;
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "error: command is too long\n\r");
			continue;
		}

		// Format: <sender id>,<destination>,message
		ats::String sender_id;
		ats::String destination;

		const char* msg = cmdline.c_str();
		get_msg_part(sender_id, msg);
		get_msg_part(destination, msg);

		md.send_message(sender_id, destination, msg);

		cmdline.clear();
	}

	return 0;
}

int main(int argc, char* argv[])
{
	int pfd[2];
	{
		pipe(pfd);

		if(fork())
		{
			close(pfd[1]);
			char c;
			read(pfd[0], &c, 1);
			exit(0);
		}

		close(pfd[0]);
		daemon(0, 0);
	}

	MyData md;
	md.set("app-name", "app-monitor");
	md.set("user", "applet");
	md.set_from_args(argc - 1, argv + 1);

	g_dbg = md.get_int("debug");

	ServerData server_data;
	{
		ServerData& sd = server_data;
		init_ServerData(&sd, 256);
		sd.m_hook = &md;
		sd.m_cs = local_command_server;
		const ats::String& user = md.get("user");
		set_unix_domain_socket_user_group(&sd, user.c_str(), user.c_str());
		start_redstone_ud_server(&sd, md.get("app-name").c_str(), 1);
	}

	ServerData ready_server_data;
	{
		ServerData& sd = ready_server_data;
		init_ServerData(&sd, 256);
		sd.m_hook = &md;
		sd.m_cs = ready_server;
		const ats::String& user = md.get("user");
		set_unix_domain_socket_user_group(&sd, user.c_str(), user.c_str());
		start_redstone_ud_server(&sd, (md.get("app-name") + "-ready").c_str(), 1);
	}

	ServerData msg_server_data;
	{
		ServerData& sd = msg_server_data;
		init_ServerData(&sd, 256);
		sd.m_hook = &md;
		sd.m_cs = msg_server;
		const ats::String& user = md.get("user");
		set_unix_domain_socket_user_group(&sd, user.c_str(), user.c_str());
		start_redstone_ud_server(&sd, (md.get("app-name") + "-msg").c_str(), 1);
	}

	write(pfd[1], "\n", 1);
	close(pfd[1]);

	ats::infinite_sleep();
	return 0;
}
