#include <iostream>
#include <iomanip>
#include <list>
#include <vector>

#include <stdio.h>
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
#include "redstone-socketcan.h"
#include "linux/can.h"
#include "socket_interface.h"
#include "SocketInterfaceResponse.h"
#include "command_line_parser.h"
#include "AFS_Timer.h"

#include "ClientMessageManager.h"

#define CAN_CODE_DODGE_SEATBELT (0x10)
#define DODGE_SEATBELT_MASK 0x4
int g_dbg = 0;
bool g_has_work = false;
bool g_testdata = false;
AFS_Timer* g_debug_timer;
ATSLogger g_log;

static int g_has_work_wait_line = 0;
static int g_can_message_handler_line = 0;
static int g_can_read_line = 0;
static int g_can_write_line = 0;

typedef struct
{
	int data;
	timeval time;
} g_candata;

g_candata g_seatbelt;

class CAN_Message_Queue : public ats::ClientMessageQueue<struct can_frame>
{
public:
	int m_raw;
	int m_terminate;

	CAN_Message_Queue()
	{
		m_raw = 1;
		m_terminate = 0;
	}

	virtual~ CAN_Message_Queue()
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

	void lock_can() const;
	void unlock_can() const;

	void lock_client() const
	{
		pthread_mutex_lock(m_client_mutex);
	}

	void unlock_client() const
	{
		pthread_mutex_unlock(m_client_mutex);
	}

	void post_can_message(const struct can_frame& p_msg);
	bool get_can_message(ClientData* p_client, struct can_frame& p_msg);

	size_t can_message_count() const;

	ats::String get(const ats::String& p_key) const;
	int get_int(const ats::String& p_key) const;
	void set(const ats::String& p_key, const ats::String& p_val);

	void set_from_args(int p_argc, char* p_argv[])
	{
		m_config.from_args(p_argc, p_argv);
	}

	CANSocket* m_s;

	size_t m_seatbelt_count;

	ServerData m_command_server;
	ServerData m_listen_server;

	ats::ClientMessageManager<struct can_frame> m_msg_manager;

	pthread_t m_can_message_handler_thread;
	pthread_t m_can_polling_thread;
	pthread_t m_can_work_thread;


private:
	ats::StringMap m_config;

	sem_t* m_can_message_sem;
	pthread_mutex_t* m_mutex;
	pthread_mutex_t* m_can_message_mutex;
	pthread_mutex_t* m_client_mutex;

	size_t m_can_msg_count;
};

MyData::MyData()
{
	m_can_message_sem = new sem_t;
	sem_init(m_can_message_sem, 0, 0);
	
	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

	m_can_message_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_can_message_mutex, 0);

	m_client_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_client_mutex, 0);

	m_can_msg_count = 0;

	m_msg_manager.add_client(ats::toStr((ClientData*)0), new CAN_Message_Queue());

	m_seatbelt_count = 0;
}

void MyData::lock() const
{
	pthread_mutex_lock(m_mutex);
}

void MyData::unlock() const
{
	pthread_mutex_unlock(m_mutex);
}

void MyData::lock_can() const
{
	pthread_mutex_lock(m_can_message_mutex);
}

void MyData::unlock_can() const
{
	pthread_mutex_unlock(m_can_message_mutex);
}

void MyData::post_can_message(const struct can_frame& p_msg)
{
	m_msg_manager.post_msg(p_msg);
}

bool MyData::get_can_message(ClientData* p_client, struct can_frame& p_msg)
{
	return m_msg_manager.wait_msg(ats::toStr(p_client), p_msg);
}

size_t MyData::can_message_count() const
{
	lock_can();
	const size_t num = m_can_msg_count;
	unlock_can();
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

static void* can_polling(void* p)
{
	g_can_write_line = __LINE__;
	MyData &md = *((MyData*)p);
	for(;;)
	{
		int write_error_count = 0;
		const int period = md.get_int("polling_period") >> 1;
		const int DODGE_CAN_ID = md.get_int("DODGE_CAN_ID");
		CANSocket* s = md.m_s;
		for(;;)
		{
			{
				unsigned char data[8];
				memset(data, 0, sizeof(data));
				data[0] = 2;
				data[1] = 0x21;
				data[2] = 0x10;
				g_can_write_line = __LINE__;
				CAN_WriteResult res = CAN_write(s, DODGE_CAN_ID, data, 8);

				if(res.m_err)
				{
					++write_error_count;

					if(write_error_count < 2)
					{
						ats_logf(ATSLOG(0), "%s,%d,: write: %zu Error:%d", __FILE__, __LINE__, res.m_nwrite, res.m_err);
					}

				}
				else
				{
					write_error_count = 0;
				}

			}
			g_can_write_line = __LINE__;
			usleep(period);
		}
	}

	return 0; // Never reached
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

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	for(;;)
	{
		char ebuf[256];
		const int c = client_getc_cached(&cd, ebuf, sizeof(ebuf), &cdc);

		if(c < 0)
		{
			if(c != -ENODATA)
			{
				ats_logf(ATSLOG(0), "Client %p: client_getc failed: %s", &cd, ebuf);
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
			ats_logf(ATSLOG(0), "Client %p: command too long (%64s...)", &cd, cmd.c_str());
			cmd.clear();
			command_too_long = false;
			continue;
		}

		const char* err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			ats_logf(ATSLOG(0), "Client %p: gen_arg_list failed (%s)", &cd, err);
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
			else if("stats" == cmd)
			{
				md.lock();
				const size_t count_seatbelt = md.m_seatbelt_count;
				md.unlock();
				send_cmd(cd.m_sockfd, MSG_NOSIGNAL,
					"Stats:\n"
					"\tseatbelt_count=%zu\n"
					"\tQueue_Size=%zu\n"
					"\thas_work_wait_line=%d\n"
					"\tcan_message_handler_line=%d\n"
					"\tcan_read_line=%d\n"
					"\tcan_write_line=%d\n"
					"\n",
					count_seatbelt,
					md.can_message_count(),
					g_has_work_wait_line,
					g_can_message_handler_line,
					g_can_read_line,
					g_can_write_line);
			}
			else if ( cmd.find("get") == 0 )
			{
				std::vector <ats::String> tokens;
				ats::String key = "";
				split(tokens, cmd, ':');

				if(tokens.size() > 1)
					key = tokens[1];

				if( tokens[0] == "getseatbelt"   )
				{
					md.lock();
					send_cmd(cd.m_sockfd, MSG_NOSIGNAL,"getseatbelt:%s seatbelt=\"%u\" sec=\"%lu\" usec=\"%lu\" seq=\"%zu\"\r",key.c_str(),g_seatbelt.data,g_seatbelt.time.tv_sec,g_seatbelt.time.tv_usec,md.m_seatbelt_count);
					md.unlock();
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

void get_processed_can_data(ats::String& p_result, struct can_frame p_msg)
{
	p_result.clear();

	if((p_msg.can_id & 0xFFFFFF00) != 0x514)
	{
		return;
	}

	if(p_msg.can_dlc <= 3)
	{
		return;
	}

	if(!((p_msg.data[1] == 0x61) && (p_msg.data[0] < 7) && (p_msg.data[0] >=3)))
	{
		return;
	}

	const int val = (p_msg.data[5]&DODGE_SEATBELT_MASK)? 0:1;

	switch(p_msg.data[2])
	{
	case CAN_CODE_DODGE_SEATBELT:
		ats_sprintf(&p_result, "seatbelt=%d", val);
		break;

	default: break;
	}

}

static void* listen_server_command(void* p)
{
	ClientData& cd = *((ClientData*)p);
	MyData& md = *((MyData*)(cd.m_data->m_hook));

	CommandBuffer cb;
	init_CommandBuffer(&cb);

	CAN_Message_Queue* q = (CAN_Message_Queue*)(md.m_msg_manager.get_client(ats::toStr(&cd)));

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

			if("raw" == cmd)
			{
				q->set_int(q->m_raw, 1);
			}
			else if("!raw" == cmd)
			{
				q->set_int(q->m_raw, 0);
			}
			else if("start" == cmd)
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
		md.m_msg_manager.add_client(ats::toStr(&cd), new CAN_Message_Queue());
		CAN_Message_Queue* q = (CAN_Message_Queue*)(md.m_msg_manager.get_client(ats::toStr(&cd)));

		pthread_t thread;
		const int retval = pthread_create( &thread, 0, listen_server_command, &cd);

		for(;!retval;)
		{
			struct can_frame msg;

			if(!md.get_can_message(&cd, msg))
			{

				if(q->get_int(q->m_terminate))
				{
					break;
				}

				continue;
			}

			const ats::String s((char *)&msg, sizeof(msg));

			if(q->get_int(q->m_raw))
			{

				if(send_cmd(cd.m_sockfd, MSG_NOSIGNAL, "can: data=%s\n\r", ats::to_hex(s).c_str()) <= 0)
				{
					break;
				}

			}
			else
			{
				ats::String result;
				get_processed_can_data(result, msg);

				if(!result.empty())
				{

					if(send_cmd(cd.m_sockfd, MSG_NOSIGNAL, "can: %s\n\r", result.c_str()) <= 0)
					{
						break;
					}

				}

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

static void* can_message_handler(void* p)
{
	g_can_message_handler_line = __LINE__;

	MyData& md = *((MyData*)p);
	md.m_msg_manager.start_client(ats::toStr((ClientData*)0));
	for(;;)
	{
		struct can_frame msg;

		g_can_message_handler_line = __LINE__;
		if(!md.get_can_message(0, msg))
		{
			continue;
		}

		if((msg.can_id & 0xFFFFFF00) != 0x500)
		{
			continue;
		}

		if(msg.can_dlc <= 3)
		{
			continue;
		}

		if(!((msg.data[1] == 0x61) && (msg.data[0] < 7) && (msg.data[0] >=3)))
		{
			continue;
		}

		g_can_message_handler_line = __LINE__;
		int val = (msg.data[5] & DODGE_SEATBELT_MASK)? 0 : 1;
		switch(msg.data[2])
		{
		case CAN_CODE_DODGE_SEATBELT:
			g_can_message_handler_line = __LINE__;

			send_redstone_ud_msg("message-assembler", 0, "seatbelt %d\n", val);
			send_redstone_ud_msg("PROC_Seatbelt", 0, "seatbelt %d\r", val);

			if(g_testdata)
			{
				std::stringstream s;
				size_t i;
				unsigned char *p = (unsigned char *)&msg;
				s << (g_debug_timer->ElapsedTime()) <<":seatbelt "<<val<<" msg["<<sizeof(msg)<<"]:";
				for(i = 0; i < sizeof(msg); ++i)
				{
					s << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << (p[i] & 0xff) << ',';
				}
				s<<"\n";
				ats_logf(ATSLOG(0), "%s", s.str().c_str());
			}
			md.lock();
			g_seatbelt.data = val;
			gettimeofday(&g_seatbelt.time, 0);
			md.unlock();

			break;


		default: 
			;
		}

		if(g_dbg >= 4)
		{
			ats_logf(ATSLOG(0), "J1979 PID recieved: PID:%0X Value:%d CAN_Queue_Size:%zu", msg.data[2], val, md.can_message_count());
		}

	}

	g_can_message_handler_line = __LINE__;
	return 0;
}

static void* has_work_wait(void* p)
{
	g_has_work_wait_line = __LINE__;
	size_t seconds_to_wait_for_power_monitor = 10;

	while(seconds_to_wait_for_power_monitor)
	{
		g_has_work_wait_line = __LINE__;
		const int status = system("netstat -an|grep \"41009.*power-monitor\" 2>&1 > /dev/null");

		if((status >= 0) && (0 == WEXITSTATUS(status)))
		{
			break;
		}

		g_has_work_wait_line = __LINE__;
		--seconds_to_wait_for_power_monitor;
		sleep(1);
	}

	g_has_work_wait_line = __LINE__;
	bool work_was_set = false;
	size_t seconds_without_data = 0;
	const size_t two_minutes = 120;

	for(;;)
	{

		if(g_has_work)
		{
			g_has_work = false;
			seconds_without_data = 0;

			if(!work_was_set)
			{
				g_has_work_wait_line = __LINE__;
				work_was_set = true;
				ats::system("echo \"set_work key=can-dodge-seatbelt-monitor\"|telnet localhost 41009");
				ats_logf(ATSLOG(0), "%s: There is work to do", __FUNCTION__);
			}

		}
		else
		{

			if((seconds_without_data >= two_minutes) && work_was_set)
			{
				g_has_work_wait_line = __LINE__;
				work_was_set = false;
				ats::system("echo \"unset_work key=can-dodge-seatbelt-monitor\"|telnet localhost 41009");
				ats_logf(ATSLOG(0), "%s: There is no work to do (seconds without data is %zu)", __FUNCTION__, seconds_without_data);
			}

		}

		g_has_work_wait_line = __LINE__;
		sleep(1);
		++seconds_without_data;
	}

	g_has_work_wait_line = __LINE__;
	return 0;
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
	MyData& md = p_md;

	md.set("can_dev", "can0");
	md.set("PROC_Seatbelt_pid_file", "/var/run/PROC_Seatbelt.pid");
	md.set("can_dev_loc", "/sys/devices/platform/FlexCAN.0");
	md.set("DODGE_CAN_ID", "0x6A0");
	md.set("polling_period", "500000");
	md.set("command_server_port", "41106");
	md.set("max_command_clients", "16");
	md.set("listen_server_port", "41107");
	md.set("max_listen_clients", "16");
	md.set_from_args(argc - 1, argv + 1);

	init_ServerData(&md.m_command_server, md.get_int("max_command_clients"));
	init_ServerData(&md.m_listen_server, md.get_int("max_listen_clients"));

	int &dbg = g_dbg = md.get_int("debug");

	g_debug_timer = new AFS_Timer();
	g_testdata = testdatadir_existing();

	ats::system("ifconfig " + md.get("can_dev") + " up");

	{
		const int retval = pthread_create(
			&(md.m_can_message_handler_thread),
			(pthread_attr_t*)0,
			can_message_handler,
			&md);

		if(retval)
		{
			ats_logf(ATSLOG(0), "%s,%d: (%d) Failed to start %s thread", __FILE__, __LINE__, retval, __FUNCTION__);
		}

	}

	CANSocket* s = md.m_s = create_new_CANSocket();

	if(!s)
	{
		ats_logf(ATSLOG(0), "%s,%d: Failed to create CANSocket\n", __FILE__, __LINE__);
		return 1;
	}

	CAN_connect(s, md.get("can_dev").c_str());

	{
		const int retval = pthread_create(
			&(md.m_can_polling_thread),
			(pthread_attr_t*)0,
			can_polling,
			&md);

		if(retval)
		{
			ats_logf(ATSLOG(0), "%s,%d: (%d) Failed to start %s thread", __FILE__, __LINE__, retval, __FUNCTION__);
		}

	}

	{
		const int retval = pthread_create(
			&(md.m_can_work_thread),
			(pthread_attr_t*)0,
			has_work_wait,
			0);

		if(retval)
		{
			ats_logf(ATSLOG(0), "%s,%d: (%d) Failed to start %s thread", __FILE__, __LINE__, retval, __FUNCTION__);
		}

	}

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

	}

	{

		for(;;)
		{
			g_can_read_line = __LINE__;
			struct can_frame msg;
			int bytes_read = CAN_read(s, &msg);

			if(bytes_read > 0)
			{

				if(dbg > 1)
				{
					std::stringstream s;
					size_t i;
					unsigned char *p = (unsigned char *)(&msg);
					for(i = 0; i < sizeof(msg); ++i)
					{
						if(i)
						{
							s << ",";
						}

						char buf[8];
						snprintf(buf, sizeof(buf) - 1, "%02X", (unsigned char)p[i]);
						buf[sizeof(buf) - 1] = '\0';
						s << buf;
					}

					if(dbg >= 5)
					{
						ats_logf(ATSLOG(0), "%s,%d: CAN ID:%0X read: %s", __FILE__, __LINE__, msg.can_id, s.str().c_str());
					}

				}

				if((msg.can_id & 0xFFFFFF00) == 0x500)
				{

					if(msg.can_dlc > 3)
					{
						if((msg.data[1] == 0x61) && (msg.data[0] < 7) && (msg.data[0] >=3))
						{
							g_has_work =true;
							switch(msg.data[2])
							{
							case CAN_CODE_DODGE_SEATBELT:
								g_can_read_line = __LINE__;
								md.lock();
								++(md.m_seatbelt_count);
								md.unlock();

								g_can_read_line = __LINE__;
								md.post_can_message(msg);
								break;

							default: break;
							}
						}

					}

				}

			}
			else if(bytes_read == 0)
			{
				break;
			}

		}
	}

	delete g_debug_timer;
	g_can_read_line = __LINE__;
	ats::system("echo \"unset_work key=can-dodge-seatbelt-monitor\"|telnet localhost 41009");
	ats_logf(ATSLOG(0), "%s,%d:CAN Connection is closed\n", __FILE__, __LINE__);
	return 0;
}

int main(int argc, char* argv[])
{
	g_log.open_testdata("can-dodge-seatbelt-monitor");
	ATSLogger::set_global_logger(&g_log);

	MyData* md = new MyData;

	const int ret = start_application(*md, argc, argv);

	// ATS FIXME: Not deleting "MyData* md" because threading may still be occuring. Update this
	//            code to call "join" on all threads before destroying "MyData* md".

	return ret;
}
