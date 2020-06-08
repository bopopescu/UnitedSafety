#include <iostream>
#include <iomanip>
#include <list>
#include <vector>
#include <unistd.h>

#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>


#include "ats-common.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "SocketInterfaceResponse.h"
#include "ConfigDB.h"
#include "command_line_parser.h"
#include "AFS_Timer.h"

#include "ClientMessageManager.h"

#define GPS_SERIAL_PORT "/dev/ttySER1"

int g_dbg = 0;
bool g_has_work = false;
bool g_testdata = false;
AFS_Timer* g_debug_timer;
ATSLogger g_log;

static int g_gps_read_line = 0;


typedef struct
{
	ats::String data;
	timeval time;
} g_gpsdata;

g_gpsdata g_gps_str;

class GPS_Message_Queue : public ats::ClientMessageQueue<g_gpsdata>
{
public:
	int m_raw;
	int m_terminate;

	GPS_Message_Queue()
	{
		m_raw = 1;
		m_terminate = 0;
	}

	virtual~ GPS_Message_Queue()
	{
	}

	void set_int(int& p_int, int p_val)
	{
		lock();
		p_int = p_val;
		unlock();
	}

	int get_int(int& p_int) const
	{
		lock();
		const int n = p_int;
		unlock();
		return n;
	}
};

class MyData
{
public:
	MyData();

	void lock() const;
	void unlock() const;
	void lock_gps() const;
	void unlock_gps() const;

	void lock_client() const
	{
		pthread_mutex_lock(m_client_mutex);
	}

	void unlock_client() const
	{
		pthread_mutex_unlock(m_client_mutex);
	}

	void post_gps_message(const g_gpsdata& p_msg);
	bool get_gps_message(ClientData* p_client, g_gpsdata& p_msg);

	size_t gps_message_count() const;

	ats::String get(const ats::String& p_key) const;
	int get_int(const ats::String& p_key) const;
	void set(const ats::String& p_key, const ats::String& p_val);

	void set_from_args(int p_argc, char* p_argv[])
	{
		m_config.from_args(p_argc, p_argv);
	}



	ServerData m_command_server;
	ServerData m_listen_server;

	ats::ClientMessageManager<g_gpsdata> m_msg_manager;

	pthread_t m_gps_message_handler_thread;
	pthread_t m_read_thread;

private:
	ats::StringMap m_config;

	sem_t* m_gps_message_sem;
	pthread_mutex_t* m_mutex;
	pthread_mutex_t* m_gps_mutex;
	pthread_mutex_t* m_client_mutex;

	size_t m_gps_msg_count;
};

MyData::MyData()
{
	m_gps_message_sem = new sem_t;
	sem_init(m_gps_message_sem, 0, 0);

	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

	m_gps_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_gps_mutex, 0);

	m_client_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_client_mutex, 0);

	m_gps_msg_count = 0;

	m_msg_manager.add_client(ats::toStr((ClientData*)0), new GPS_Message_Queue());

}

void MyData::lock() const
{
	pthread_mutex_lock(m_mutex);
}

void MyData::unlock() const
{
	pthread_mutex_unlock(m_mutex);
}

void MyData::lock_gps() const
{
	pthread_mutex_lock(m_gps_mutex);
}

void MyData::unlock_gps() const
{
	pthread_mutex_unlock(m_gps_mutex);
}

void MyData::post_gps_message(const g_gpsdata& p_msg)
{
	m_msg_manager.post_msg(p_msg);
}

bool MyData::get_gps_message(ClientData* p_client, g_gpsdata& p_msg)
{
	return m_msg_manager.wait_msg(ats::toStr(p_client), p_msg);
}

size_t MyData::gps_message_count() const
{
	lock_gps();
	const size_t num = m_gps_msg_count;
	unlock_gps();
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
	ClientData& cd = *((ClientData*)p);
	MyData& md = *((MyData*)(cd.m_data->m_hook));

	CommandBuffer cb;
	init_CommandBuffer(&cb);
	alloc_dynamic_buffers(&cb, 256, 65536);

	ats::String cmd;

	for(;;) {
		char buf[1024];
		memset(buf, 0, sizeof(buf));
		int nread = recv(cd.m_sockfd, buf, 1024, 0);
	
		if(nread <= 0)
		{
			if(nread)
			{
				ats_logf(ATSLOG(0), "Client %p: client_getc failed: %s", &cd, strerror(errno));
			}
			break;
		}
		g_gpsdata msg;
		ats_sprintf(&msg.data, "%s", buf);
		md.post_gps_message(msg);

	}

	free_dynamic_buffers(&cb);
	shutdown(cd.m_sockfd, SHUT_WR);
	// FIXME: Race-condition exists because "shutdown" does not gaurantee that the receiver will receive all the data.
	close_client(&cd);
	return 0;
}

static void* listen_server_command(void* p)
{
	ClientData& cd = *((ClientData*)p);
	MyData& md = *((MyData*)(cd.m_data->m_hook));

	CommandBuffer cb;
	init_CommandBuffer(&cb);

	GPS_Message_Queue* q = (GPS_Message_Queue*)(md.m_msg_manager.get_client(ats::toStr(&cd)));

	ats::SocketInterfaceResponse response(cd.m_sockfd);

	for(;;)
	{
		const ats::String& cmdline = response.get(0, "\n\r", 0);

		if(cmdline.empty())
		{
			break;
		}

		if(gen_arg_list(cmdline.c_str(), int(cmdline.size()), &cb))
		{
			break;
		}

		if(cb.m_argc >= 1)
		{
			const ats::String cmd(cb.m_argv[0]);

			if("start" == cmd)
			{
				md.m_msg_manager.start_client(ats::toStr(&cd));
			}
			else if("stop" == cmd)
			{
				md.m_msg_manager.stop_client(ats::toStr(&cd));
			}
			else
			{

				if(send_cmd(cd.m_sockfd, MSG_NOSIGNAL, "error: Invalid command \"%s\"\n\r", cmd.c_str()) <= 0)
				{
					break;
				}

			}

		}

	}

	md.m_msg_manager.put_client(q);

	return 0;
}

static void* listen_server(void* p)
{
	ClientData& cd = *((ClientData*)p);
	MyData& md = *((MyData*)(cd.m_data->m_hook));

	if(send_cmd(cd.m_sockfd, MSG_NOSIGNAL, "hello: client=%p\n\r", &cd) > 0)
	{
		md.m_msg_manager.add_client(ats::toStr(&cd), new GPS_Message_Queue());
		GPS_Message_Queue* q = (GPS_Message_Queue*)(md.m_msg_manager.get_client(ats::toStr(&cd)));

		pthread_t thread;
		const int retval = pthread_create( &thread, 0, listen_server_command, &cd);
		
		md.m_msg_manager.start_client(ats::toStr(&cd));

		for(;!retval;)
		{
			g_gpsdata msg;

			if(!md.get_gps_message(&cd, msg))
			{

				if(q->get_int(q->m_terminate))
				{
					break;
				}

				continue;
			}

			const ats::String s(msg.data.c_str());

			if(send_cmd(cd.m_sockfd, MSG_NOSIGNAL, "%s", s.c_str()) <= 0)
			{
				break;
			}
		}

		shutdown(cd.m_sockfd, SHUT_WR);
		// FIXME: Race-condition exists because "shutdown" does not gaurantee that the receiver will receive all the data.
		close_client(&cd);

		pthread_join(thread, 0);

		md.m_msg_manager.put_client(q);
	}
	else
	{
		shutdown(cd.m_sockfd, SHUT_WR);
		// FIXME: Race-condition exists because "shutdown" does not gaurantee that the receiver will receive all the data.
		close_client(&cd);
	}

	{
		const ats::String& err = md.m_msg_manager.remove_client(ats::toStr(&cd));

		// XXX: This error will never happen. This check is just added for completeness. The reason
		//      why this will never happen is because only the "busy" error can be returned, and it
		//      is not possible to execute this line of code and still be "busy". This is because
		//      the "busy" status is set by the "get_can_message" function.
		if(!err.empty())
		{
			ats_logf(ATSLOG(0), "%s,%d: RESOURCE LEAK: CAN_Message_Queue for ClientData(%p) could not be removed, error is: %s", __FILE__, __LINE__, &cd, err.c_str());
		}

	}

	return 0;
}

void setup_firewall(int p_port)
{
	std::stringstream s;
	s << "export LD_LIBRARY_PATH=/usr/local/lib;";
	s << "export XTABLES_LIBDIR=/lib/xtables;";
	s << "iptables -I INPUT -i eth0 -p tcp --dport "<< p_port << " -j ACCEPT;";
	s << "iptables -I INPUT -i ra0 -p tcp --dport " << p_port << " -j ACCEPT";
	ats::system(s.str());

}

bool testdatadir_existing()
{
		struct stat st;
		const ats::String& s1 = "/var/log/testdata";
		if(stat(s1.c_str(), &st) != 0 )
		{
			return false;
		}
		return true;
}

int start_application(MyData& p_md, int argc, char* argv[])
{
	db_monitor::ConfigDB db;
	MyData& md = p_md;
	
	const ats::String listen_server_port = db.GetValue("gps-socket-server", "listen_server_port", "41094");
	ats_logf(ATSLOG(0), "listen_server_port= %s",listen_server_port.c_str());
	md.set("gps_serial_port", "/dev/ttySER1");
	md.set("command_server_port", "41092");
	md.set("max_command_clients", "16");
	md.set("listen_server_port", listen_server_port);
	md.set("max_listen_clients", "16");
	md.set_from_args(argc - 1, argv + 1);

	init_ServerData(&md.m_command_server, md.get_int("max_command_clients"));
	init_ServerData(&md.m_listen_server, md.get_int("max_listen_clients"));

	g_dbg = md.get_int("debug");

	g_debug_timer = new AFS_Timer();
	g_testdata = testdatadir_existing();

	{
		ServerData &s = md.m_command_server;
		s.m_cs = serv_client;
		s.m_hook = &md;
		s.m_port = md.get_int("command_server_port");

		if(start_server(&s))
		{
			ats_logf(ATSLOG(0), "%s,%d: Error starting client/device server: %s", __FILE__, __LINE__, s.m_emsg);
		}

	}

	{
		ServerData &s = md.m_listen_server;
		s.m_cs = listen_server;
		s.m_hook = &md;
		s.m_port = md.get_int("listen_server_port");

		if(start_server(&s))
		{
			ats_logf(ATSLOG(0), "%s,%d: Error starting client/device server: %s", __FILE__, __LINE__, s.m_emsg);
		}
		
		setup_firewall(s.m_port);

	}

	{
		g_gps_read_line = __LINE__;
		
		for(;;)
		{
			usleep(100000);	
		}
	}

	delete g_debug_timer;
	g_gps_read_line = __LINE__;
	ats_logf(ATSLOG(0), "%s,%d:CAN Connection is closed\n", __FILE__, __LINE__);
	return 0;
}

int main(int argc, char* argv[])
{
	g_log.open_testdata("gps-serial-port");
	ATSLogger::set_global_logger(&g_log);

	MyData* md = new MyData;

	const int ret = start_application(*md, argc, argv);

	// ATS FIXME: Not deleting "MyData* md" because threading may still be occuring. Update this
	//            code to call "join" on all threads before destroying "MyData* md".

	return ret;
}
