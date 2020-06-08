#include <iostream>
#include <list>

#include <sys/time.h>
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
#include <sys/ioctl.h>

#include "ats-common.h"
#include "socket_interface.h"
#include "command_line_parser.h"

typedef std::list <ats::String> CommentList;

class MyData
{
public:
	CommentList m_comment;
	ats::StringMap m_config;

	ats::String m_buzzer_key;
	int m_buzzer_set_time;
	int m_buzzer_time;
	int m_buzzer_priority;

	bool m_watchdog_running;
	sem_t *m_watchdog_sem;

	MyData()
	{
		m_watchdog_running = false;
		m_buzzer_set_time = 0;
		m_buzzer_time = 0;
		m_buzzer_priority = 0;

		m_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_mutex, 0);

		m_config_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_config_mutex, 0);

		m_comment_mutex = new pthread_mutex_t;
		pthread_mutex_init(m_comment_mutex, 0);

		m_watchdog_sem = new sem_t;
		sem_init(m_watchdog_sem, 0, 0);
	}

	void add_comment( const ats::String &p_comment)
	{
		const size_t max_comments = abs(get_int("max_comments"));
		lock_comment();
		if((!m_comment.empty()) && (m_comment.size() == max_comments))
			m_comment.pop_front();
		m_comment.push_back(p_comment);
		unlock_comment();
	}

	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	void lock_config() const
	{
		pthread_mutex_lock(m_config_mutex);
	}

	void unlock_config() const
	{
		pthread_mutex_unlock(m_config_mutex);
	}

	void lock_comment() const
	{
		pthread_mutex_lock(m_config_mutex);
	}

	void unlock_comment() const
	{
		pthread_mutex_unlock(m_config_mutex);
	}



	ats::String get(const ats::String &p_key) const;
	int get_int(const ats::String &p_key) const;
	void set(const ats::String &p_key, const ats::String &p_val);

	void set_from_args(int p_argc, char *p_argv[])
	{
		lock_config();
		m_config.from_args(p_argc, p_argv);
		unlock_config();
	}

private:
	pthread_mutex_t *m_mutex;
	pthread_mutex_t *m_config_mutex;
	pthread_mutex_t *m_comment_mutex;
};

ats::String MyData::get(const ats::String &p_key) const
{
	lock_config();
	const ats::String s(m_config.get(p_key));
	unlock_config();
	return s;
}

int MyData::get_int(const ats::String &p_key) const
{
	lock_config();
	const int n = m_config.get_int(p_key);
	unlock_config();
	return n;
}

void MyData::set(const ats::String &p_key, const ats::String &p_val)
{
	lock_config();
	m_config.set(p_key, p_val);
	unlock_config();
}

static int g_dbg = 0;

// ************************************************************************
// Description: Command server.
//
// Parameters:  p - pointer to ClientData
// Returns:     NULL pointer
// ************************************************************************
static void *client_command_server( void *p);

int main(int argc, char *argv[])
{
	MyData md;
	ats::StringMap &config = md.m_config;

	config.set("buzzer_file", "/dev/buzzer");
	config.set("max_comments", "30");
	config.from_args(argc - 1, argv + 1);

	g_dbg = config.get_int("debug");

	openlog("buzzer-monitor", LOG_PID, LOG_USER);

	ClientSocket cs;
	init_ClientSocket(&cs);
	ServerData server_data;
	{
		ServerData *sd = &server_data;
		init_ServerData(sd, 64);
		sd->m_port = 41008;
		sd->m_hook = &md;
		sd->m_cs = client_command_server;
		::start_server(sd);
	}

	const ats::String buzzer_off_cmd("echo '0,0' > \"" + md.get("buzzer_file") + "\"");
	bool running = false;
	for(;;)
	{
		if(!running) sem_wait(md.m_watchdog_sem);
		running = true;
		// REDSTONE FIXME: This sleep/delay method drifts.
		//	[Amour Hassan - September 17, 2012]
		usleep(50000);
		md.lock();
		int &time = md.m_buzzer_time;
		if(time > 0) time -= 50000;
		if(time <= 0)
		{
			running = md.m_watchdog_running = false;
			time = 0;
			ats::system(buzzer_off_cmd);
			md.m_buzzer_set_time = 0;
			md.m_buzzer_priority = 0;
			md.m_buzzer_key.clear();
			struct timeval t;
			gettimeofday(&t, 0);
			std::stringstream s;
			s << t.tv_sec << "." << t.tv_usec << " - buzzer watchdog disabled buzzer";
			md.add_comment(s.str());
		}
		md.unlock();
	}

	return 0;
}

static void *client_command_server( void *p)
{
	ClientData* cd = (ClientData*)p;
	MyData& md = *((MyData *)(cd->m_data->m_hook));

	bool command_too_long = false;
	ats::String cmd;

	const size_t max_command_length = 1024;

	const ats::String buzzer_file(md.get("buzzer_file"));

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cdc);

		if(c < 0)
		{
			if(c != -ENODATA) syslog(LOG_ERR, "%s,%d: %s", __FILE__, __LINE__, ebuf);
			break;
		}

		if(c != '\r' && c != '\n') {
			if(cmd.length() >= max_command_length) command_too_long = true;
			else cmd += c;

			continue;
		}
		if(command_too_long) {
			syslog(LOG_ERR, "%s,%d: command is too long", __FILE__, __LINE__);
			cmd.clear();
			command_too_long = false;
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "xml <devResp error=\"command is too long\"></devResp>\r");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char *err;
		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb))) {
			syslog(LOG_ERR, "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "xml <devResp error=\"gen_arg_list failed\">%s</devResp>\r", err);
			cmd.clear();
			continue;
		}
		cmd.clear();

		if(cb.m_argc <= 0)
			continue;

		const ats::String cmd(cb.m_argv[0]);
		ats::StringMap args;
		args.from_args(cb.m_argc - 1, cb.m_argv + 1);

		if("touch" == cmd)
		{
			std::stringstream s_comment;

			if(cb.m_argc >= 2)
			{
				md.lock();
				struct timeval t;
				gettimeofday(&t, 0);
				const char *key = cb.m_argv[1];
				if(md.m_buzzer_time > 0)
				{
					s_comment << t.tv_sec << "." << t.tv_usec << " - [touch by " << md.m_buzzer_key << "]";
					if((md.m_buzzer_set_time > 0) && (key == md.m_buzzer_key))
					{
						md.m_buzzer_time = md.m_buzzer_set_time;
					}
				}
				else
				{
					s_comment << t.tv_sec << "." << t.tv_usec << " - [late touch by " << key << "]";
				}
				md.add_comment(s_comment.str());
				md.unlock();
			}
		}
		else if("buzz" == cmd)
		{
			if(cb.m_argc <= 1)
			{
				std::stringstream s;
				md.lock();
				ats::system("cat " + buzzer_file + "|head -n 1", &s);
				md.unlock();
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s\n", s.str().c_str());
			}
			else if(cb.m_argc >= 6)
			{
				const ats::String key(cb.m_argv[1]);
				const int priority = strtol(cb.m_argv[2], 0, 0);
				const int on = strtol(cb.m_argv[3], 0, 0);
				const int off = strtol(cb.m_argv[4], 0, 0);
				const int time = strtol(cb.m_argv[5], 0, 0);
				std::stringstream s_cmd;
				std::stringstream s_comment;
				md.lock();
				struct timeval t;
				gettimeofday(&t, 0);
				if((!md.m_buzzer_key.empty()) && (priority < md.m_buzzer_priority))
				{
					s_comment << t.tv_sec << "." << t.tv_usec
						<< " - [(" << key << ",pri=" << priority << ") cannot set since (" << md.m_buzzer_key << ",pri= " << md.m_buzzer_priority << ")]";
				}
				else
				{
					s_cmd << "echo -n '" << on << "," << off << "' > " << buzzer_file;
					ats::system(s_cmd.str());
					if(!md.m_watchdog_running)
					{
						md.m_watchdog_running = true;
						sem_post(md.m_watchdog_sem);
					}
					syslog(LOG_NOTICE, "buzz on: key=%s, priority=%d, on=%d, off=%d, time=%d", key.c_str(), priority, on, off,time);
					s_comment << t.tv_sec << "." << t.tv_usec << " - [" << key << "," << priority << "," << on << "," << off << "," << time << "]";
					if(cb.m_argc >= 7)
						s_comment << " - " << cb.m_argv[6];
					md.m_buzzer_key = key;
					md.m_buzzer_set_time = time;
					md.m_buzzer_time = time;
					md.m_buzzer_priority = priority;
				}
				md.add_comment(s_comment.str());
				md.unlock();
			}
			else
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "expected [<key> <priority> <on> <off> <time> [comment]]\n");
			}
		}
		else if("buzz-off" == cmd)
		{
			if(cb.m_argc >= 3)
			{
				const ats::String key(cb.m_argv[1]);
				const int priority = strtol(cb.m_argv[2], 0, 0);
				std::stringstream s_cmd;
				std::stringstream s_comment;
				md.lock();
				struct timeval t;
				gettimeofday(&t, 0);
				if((!md.m_buzzer_key.empty()) && (priority < md.m_buzzer_priority))
				{
					s_comment << t.tv_sec << "." << t.tv_usec
						<< " - [(" << key << ",pri=" << priority << ") cannot clear since (" << md.m_buzzer_key << ",pri= " << md.m_buzzer_priority << ")]";
				}
				else
				{
					s_cmd << "echo -n '0,0' > " << buzzer_file;
					ats::system(s_cmd.str());
					syslog(LOG_NOTICE, "buzz off: key=%s, priority=%d", key.c_str(), priority);
					s_comment << t.tv_sec << "." << t.tv_usec << " - [" << key << "," << priority << "]";
					if(cb.m_argc >= 4)
						s_comment << " - " << cb.m_argv[3];
					md.m_buzzer_key.clear();
					md.m_buzzer_set_time = 0;
					md.m_buzzer_time = 0;
					md.m_buzzer_priority = 0;
				}
				md.add_comment(s_comment.str());
				md.unlock();
			}
			else
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "expected [<key> <priority> [comment]]\n");
			}
		}

		else if("stats" == cmd)
		{
			CommentList list;
			md.lock();
			{
				md.lock_comment();
				list = md.m_comment;
				md.unlock_comment();
			}
			const int set_time = md.m_buzzer_set_time;
			const int time = md.m_buzzer_time;
			const bool running = md.m_watchdog_running;
			const ats::String key(md.m_buzzer_key);
			md.unlock();

			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "set_time=%d, time=%d, watchdog_running=%s, key=\"%s\"\n", set_time, time, running ? "yes" : "no", key.c_str());
			CommentList::const_iterator i = list.begin();
			while(i != list.end())
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s\n", (*i).c_str());
				++i;
			}
		}
		else
		{
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "Invalid command \"%s\"\n", cmd.c_str());
		}
	}
	return 0;
}

