#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <syslog.h>

#include "command_line_parser.h"
#include "socket_interface.h"
#include "SocketInterfaceResponse.h"
#include "LogServer.h"

LogServer::LogServer()
{
	m_data = 0;
	m_log_buf.resize(2048);
	pthread_mutex_init(&m_mutex, 0);
	pthread_mutex_init(&m_mutex_send_log, 0);

	m_log_server_cs = 0;
}

void LogServer::lock()
{
	pthread_mutex_lock(&m_mutex);
}

void LogServer::unlock()
{
	pthread_mutex_unlock(&m_mutex);
}

void LogServer::lock_send_log()
{
	pthread_mutex_lock(&m_mutex_send_log);
}

void LogServer::unlock_send_log()
{
	pthread_mutex_unlock(&m_mutex_send_log);
}

ats::String LogServer::log_on_time(const ats::StringMap&)
{

	lock();
	unlock();

	return ats::String();
}

ats::String LogServer::log_once(const ats::StringMap&)
{
	return ats::String();
}

ats::String LogServer::log_on_changed(const ats::StringMap&)
{
	return ats::String();
}

ats::String LogServer::log_on_new(const ats::StringMap&)
{
	return ats::String();
}

ats::String LogServer::set_logger(const ats::String& p_key, LogFn p_fn)
{
//	lock();
	std::pair <LogFnMap::iterator, bool> r = m_onlog.insert(LogFnPair(p_key, p_fn));

	if(!(r.second))
	{
		(r.first)->second = p_fn;
	}

//	unlock();

	return ats::String();
}

#if 0
ats::String LogServer::unset_logger(const ats::String& p_key)
{
	lock();
	LogFnMap::iterator i = m_onlog.find(p_key);

	if(i != m_onlog.end())
	{
		m_onlog.erase(i);
	}

	unlock();

	return ats::String();
}
#endif

void LogServer::start_log_server()
{
	pthread_create(&m_log_server, 0, log_server, this);
}

void LogServer::send_log(
	const char* p_log_key,
	const char* p_log_des,
	const char* p_log_time)
{
	lock_send_log();

	if(!m_log_server_cs)
	{
		unlock_send_log();
		return;
	}

	// ATS FIXME: No locking is done since logs cannot be removed or added once the log server starts.
	//lock();
	LogFnMap::const_iterator i = m_onlog.find(p_log_key);

	if(m_onlog.end() == i)
	{
		//unlock();
		unlock_send_log();
		return;
	}

	LogFn fn = i->second;
	//unlock();

	LogRequest lr;

	if(fn(*this, lr))
	{

		if(m_log_buf[0])
		{
			send_cmd(m_log_server_cs->m_fd, MSG_NOSIGNAL, "[%s,%s,%s,%s]\r",
				p_log_key,
				p_log_des,
				p_log_time,
				m_log_buf.data());
		}
		else
		{
			send_cmd(m_log_server_cs->m_fd, MSG_NOSIGNAL, "[%s,%s,%s]\r",
				p_log_key,
				p_log_des,
				p_log_time);
		}

	}

	unlock_send_log();
}

void LogServer::update_log_request_map(LogRequestMap& p_lrm, const ats::String& p_log_name, const ats::String& p_des, bool p_remove)
{
	lock();
	LogRequestMap::iterator i = (p_lrm.insert(LogRequestPair(p_log_name, ats::StringMap()))).first;
	ats::StringMap& m = i->second;

	if(p_remove)
	{
		m.set(p_des, "");
	}
	else
	{
		m.unset(p_des);

		if(m.empty())
		{
			p_lrm.erase(i);
		}

	}

	unlock();
}

void LogServer::h_log_server(LogServer& p_log, ClientSocket*& p_cs)
{
	LogServer& log = p_log;

	ClientSocket* cs = new ClientSocket();
	init_ClientSocket(cs);

	for(;;)
	{

		if(!connect_redstone_ud_client(cs, "rtc-monitor-loggen"))
		{
			break;
		}

		sleep(1);
	}

	log.lock_send_log();
	p_cs = cs;
	log.unlock_send_log();

	log.lock();
	{
		LogFnMap::const_iterator i = log.m_onlog.begin();

		while(i != log.m_onlog.end())
		{

			if(send_cmd(cs->m_fd, MSG_NOSIGNAL, "reg key=%s\r", (i->first).c_str()) < 0)
			{
				log.unlock();
				return;
			}

			++i;
		}

	}
	log.unlock();

	ats::SocketInterfaceResponse sir(cs->m_fd);

	for(;;)
	{
		const ats::String& cmdline = sir.get();

		if(cmdline.empty())
		{
			break;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char *err;

		if((err = gen_arg_list(cmdline.c_str(), cmdline.length(), &cb)))
		{
			ats_logf(ATSLOG(0), "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			continue;
		}

		if(cb.m_argc <= 0)
		{
			continue;
		}

		if(':' == cb.m_argv[0][0])
		{

			if(cb.m_argc >= 3)
			{
				const ats::String& cmd = cb.m_argv[0] + 1;
				const bool remove = '!' == cb.m_argv[2][0];

				if("onnew" == cmd)
				{
					log.update_log_request_map(log.m_on_new, cb.m_argv[1], cb.m_argv[2] + (remove ? 1 : 0), remove);
				}
				else if("onchanged" == cmd)
				{
					log.update_log_request_map(log.m_on_changed, cb.m_argv[1], cb.m_argv[2] + (remove ? 1 : 0), remove);
				}

			}

		}
		else
		{
			log.send_log(cb.m_argv[0], cb.m_argv[1], cb.m_argv[2]);
		}

	}

}

void* LogServer::log_server(void* p)
{
	LogServer& log = *((LogServer*)p);

	for(;;)
	{
		h_log_server(log, log.m_log_server_cs);
		log.lock_send_log();
		close_ClientSocket(log.m_log_server_cs);
		delete log.m_log_server_cs;
		log.m_log_server_cs = 0;
		log.unlock_send_log();
		sleep(1);
	}

	return 0;
}

void LogServer::log_notify(const ats::String& p_log_name, LogRequestMap& p_lrm)
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	char time[32];
	snprintf(time, sizeof(time) - 1, "%u.%03u", (unsigned int)(tv.tv_sec), (unsigned int)(tv.tv_usec) / 1000);
	time[sizeof(time) - 1] = '\0';

	lock();
	LogRequestMap::const_iterator i = p_lrm.find(p_log_name);

	if(i != p_lrm.end())
	{
		const ats::StringMap& m = i->second;
		ats::StringMap::const_iterator j = m.begin();

		// Send "on new" log to all registered destinations.
		while(j != m.end())
		{
			send_log(p_log_name.c_str(), (j->first).c_str(), time);
			++j;
		}

	}

	unlock();
}

void LogServer::log_new_event(const ats::String& p_log_name)
{
	log_notify(p_log_name, m_on_new);
}

void LogServer::log_changed_event(const ats::String& p_log_name)
{
	log_notify(p_log_name, m_on_changed);
}
