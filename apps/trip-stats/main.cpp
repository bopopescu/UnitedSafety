#include <iostream>
#include <vector>
#include <deque>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>

#include "ats-common.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "AFS_Timer.h"

int g_dbg = 0;

typedef enum
{
	speedType,
	defaultType
} messageType;

struct message_frame
{
	messageType mType;
	int val;
};

class MyData
{
public:
	MyData();

	void lock() const;
	void unlock() const;

	void lock_message() const;
	void unlock_message() const;

	typedef std::deque <struct message_frame> message_Queue;

	void post_speed_message(const struct message_frame& p_msg);
	bool get_speed_message(struct message_frame& p_msg);

	size_t can_message_count() const;

	ats::String get(const ats::String& p_key) const;
	int get_int(const ats::String& p_key) const;
	void set(const ats::String& p_key, const ats::String& p_val);

	void setdistance(const int& speed);
	int getdistance() const;

	void set_from_args(int p_argc, char* p_argv[])
	{
		m_config.from_args(p_argc, p_argv);
	}

	ServerData m_client_server;

	pthread_t m_can_message_handler_thread;
	pthread_t m_can_polling_thread;
	pthread_t m_can_work_thread;

private:
	ats::StringMap m_config;

	sem_t* m_can_message_sem;
	pthread_mutex_t* m_mutex;
	pthread_mutex_t* m_can_message_mutex;

	message_Queue m_speed_queue;
    unsigned int m_distance;//centimeter unit
	AFS_Timer m_Timer;
};

MyData::MyData()
{
	m_can_message_sem = new sem_t;
	sem_init(m_can_message_sem, 0, 0);

	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

	m_can_message_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_can_message_mutex, 0);

	init_ServerData(&m_client_server, 16);

	m_distance = 0;
	m_Timer.SetTime();
}

void MyData::lock() const
{
	pthread_mutex_lock(m_mutex);
}

void MyData::unlock() const
{
	pthread_mutex_unlock(m_mutex);
}

void MyData::lock_message() const
{
	pthread_mutex_lock(m_can_message_mutex);
}

void MyData::unlock_message() const
{
	pthread_mutex_unlock(m_can_message_mutex);
}

void MyData::post_speed_message(const struct message_frame& p_msg)
{
	lock_message();
	m_speed_queue.push_back(p_msg);
	sem_post(m_can_message_sem);
	unlock_message();
}

bool MyData::get_speed_message(struct message_frame& p_msg)
{
	bool b;
	sem_wait(m_can_message_sem);
	lock_message();
	if((b = (!m_speed_queue.empty())))
	{
		p_msg = *(m_speed_queue.begin());
		m_speed_queue.pop_front();
	}
	unlock_message();
	return b;
}

size_t MyData::can_message_count() const
{
	lock_message();
	const size_t num = m_speed_queue.size();
	unlock_message();
	return num;
}

ats::String MyData::get(const ats::String& p_key) const
{
	lock();
	const ats::String s(m_config.get(p_key));
	unlock();
	return s;
}

int MyData::get_int(const ats::String& p_key) const
{
	lock();
	const int n = m_config.get_int(p_key);
	unlock();
	return n;
}

void MyData::set(const ats::String& p_key, const ats::String& p_val)
{
	lock();
	m_config.set(p_key, p_val);
	unlock();
}

void MyData::setdistance(const int& speed)
{
	lock();
	m_distance += (m_Timer.DiffTimeMS() * ((speed) * 1.0 / 36));
	m_Timer.SetTime();
	unlock();
}

int MyData::getdistance() const
{
	int distance;
	lock();
	distance = m_distance/100.0; //return meter unit
	unlock();
	return distance;
}

void split(std::vector<ats::String> &p_list, const ats::String &p_s, char p_c)
{
	ats::String s;
	p_list.clear();
	for(ats::String::const_iterator i = p_s.begin(); i != p_s.end(); ++i) {
		const char c = *i;
		if(c == p_c) {
			p_list.push_back(s);
			s.clear();
		} else {
			s += c;
		}
	}
	p_list.push_back(s);
}

static void* serv_client(void* p)
{
	const size_t max_cmd_length = 2048;
	ClientData& cd = *((ClientData*)p);
	MyData& md = *((MyData*)(cd.m_data->m_hook));

	CommandBuffer cb;
	init_CommandBuffer(&cb);
	alloc_dynamic_buffers(&cb, 256, 65536);

	bool command_too_long = false;
	ats::String cmd;

	ClientDataCache cache;
	init_ClientDataCache(&cache);

	for(;;)
	{
		char ebuf[256];
		const int c = client_getc_cached(&cd, ebuf, sizeof(ebuf), &cache);

		if(c < 0)
		{
			if(c != -ENODATA)
			{
				syslog(LOG_ERR, "Client %p: client_getc_cached failed: %s", &cd, ebuf);
			}

			break;
		}

		if((c != '\r') && (c != '\n'))
		{
			if(cmd.length() >= max_cmd_length) command_too_long = true;
			else cmd += c;
			continue;
		}

		if(command_too_long)
		{
			syslog(LOG_ERR, "Client %p: command too long (%64s...)", &cd, cmd.c_str());
			cmd.clear();
			command_too_long = false;
			continue;
		}

		const char* err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			syslog(LOG_ERR, "Client %p: gen_arg_list failed (%s)", &cd, err);
			break;
		}

		const ats::String full_command(cmd);
		cmd.clear();

		if(cb.m_argc < 1)
		{
			continue;
		}

		{
			const ats::String cmd(cb.m_argv[0]);
			if("test" == cmd) {
				send_cmd(cd.m_sockfd, MSG_NOSIGNAL, "%s,%d: Hello, World!\n", __FILE__, __LINE__);

			}
			else if ( "speed" == cmd)
			{
				struct message_frame msg;
				msg.mType = speedType;
				msg.val = strtol(cb.m_argv[1], 0, 0);
				md.post_speed_message(msg);
			}
			else if( ("debug" == cmd) || ("dbg" == cmd))
			{

				if(cb.m_argc >= 2)
				{
					g_dbg = strtol(cb.m_argv[1], 0, 0);
				}
				else
				{
					send_cmd(cd.m_sockfd, MSG_NOSIGNAL, "%s=%d\n", cmd.c_str(), g_dbg);
				}

			}
			else if ( cmd.find("get") == 0 )
			{
				std::vector <ats::String> tokens;
				ats::String key = "";
				split(tokens, cmd, ':');

				if(tokens.size() > 1)
					key = tokens[1];

				if( tokens[0] == "getdistance"   )
				{
					int distance = md.getdistance();
					send_cmd(cd.m_sockfd, MSG_NOSIGNAL,"getdistance:%s distance=\"%d\"\r",key.c_str(),distance);
				}
			}
		}

	}

	free_dynamic_buffers(&cb);
	shutdown(cd.m_sockfd, SHUT_WR);
	// FIXME: Race-condition exists because "shutdown" does not gaurantee that the receiver will receive all the data.
	close_client(&cd);
	return 0;
}

static MyData g_md;

int main(int argc, char* argv[])
{
	MyData& md = g_md;
	md.set("user", "applet");
	md.set_from_args(argc - 1, argv + 1);

	g_dbg = md.get_int("debug");

	if(ats::su(md.get("user")))
	{
		syslog(LOG_ERR, "%s, %d: Could not become user \"%s\": ERR (%d): %s", __FILE__, __LINE__, md.get("user").c_str(), errno, strerror(errno));
		return 1;
	}

	{
		ServerData &s = md.m_client_server;
		s.m_cs = serv_client;
		s.m_hook = &md;

		if(start_redstone_ud_server(&s, "trip-stats", 1))
		{
			syslog(LOG_ERR, "%s,%d: Error starting client/device server: %s", __FILE__, __LINE__, s.m_emsg);
			return 1;
		}

		signal_app_unix_socket_ready("trip-stats", "trip-stats");
		signal_app_ready("trip-stats");
	}

	for(;;)
	{
		struct message_frame msg;
		if(!md.get_speed_message(msg))
		{
			usleep(100000);
			continue;
		}

		md.setdistance(msg.val);
	}

	syslog(LOG_ERR, "%s,%d:CAN Connection is closed\n", __FILE__, __LINE__);
	return 0;
}
