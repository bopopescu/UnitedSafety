#include <iostream>
#include <list>
#include <vector>

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

#include "ats-common.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "ConfigDB.h"
#include "RedStone_IPC.h"
#include "MovingAverage.h"
#include "adc2volt.h"

#include "power-monitor-state-machine.h"
#include "power-monitor.h"

static const ats::String g_app_name("power-monitor");

int g_dbg = 0;
ATSLogger g_log;

// set_work <key>
int ac_set_work(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	MyData& md = p_acc.my_data();

	ats::StringMap s;
	s.from_args(p_argc - 1, p_argv + 1);

	const ats::String& err = md.set_work(s);

	if(err.empty())
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "OK\n\r");
	}
	else
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "Failed: %s\n\r", err.c_str());
	}

	return 0;
}

// unset_work <key>
int ac_unset_work(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	MyData& md = p_acc.my_data();

	ats::StringMap s;
	s.from_args(p_argc - 1, p_argv + 1);

	md.unset_work(s);
	send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "OK\n\r");
	return 0;
}

int ac_debug(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	if(p_argc <= 1)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n\r", g_dbg);
	}
	else
	{
		g_dbg = strtol(p_argv[1], 0, 0);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n\r", g_dbg);
	}
	return 0;
}

COMMON_EVENT_DEFINITION(,ShutdownEvent, AppEvent)

ShutdownEvent::ShutdownEvent()
{
}

ShutdownEvent::~ShutdownEvent()
{
}

void ShutdownEvent::start_monitor()
{
}

void ShutdownEvent::stop_monitor()
{
}

COMMON_EVENT_DEFINITION(,IgnitionMonitorStartedEvent, AppEvent)

IgnitionMonitorStartedEvent::IgnitionMonitorStartedEvent()
{
}

IgnitionMonitorStartedEvent::~IgnitionMonitorStartedEvent()
{
}

void IgnitionMonitorStartedEvent::start_monitor()
{
}

void IgnitionMonitorStartedEvent::stop_monitor()
{
}

MyData::MyData()
{
	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

	m_work_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_work_mutex, 0);

	m_command.insert(AdminCommandPair("set_work", AdminCommand(ac_set_work)));
	m_command.insert(AdminCommandPair("unset_work", AdminCommand(ac_unset_work)));

	m_command.insert(AdminCommandPair("debug", AdminCommand(ac_debug)));
}

ats::String MyData::set_work(const ats::StringMap& p_args)
{
	ats::String ret;
	lock_work();
	h_expire_work();

	if(h_is_shutting_down())
	{
		ret = "too late, shutting down";
	}
	else
	{
		std::pair<PowerMonitor::WorkMap::iterator, bool> r = m_work.insert(PowerMonitor::WorkPair(p_args.get("key"), PowerMonitor::Work()));

		if(p_args.has_key("expire"))
		{
			PowerMonitor::Work& w = (r.first->second);

			timespec t;
			clock_gettime(CLOCK_MONOTONIC, &t);
			w.m_data.set("expire", ats::toStr(t.tv_sec + strtol(p_args.get("expire").c_str(), 0, 0)));
		}

		// add a priority field that allows us to make some messages delay shutdown. (low_battery needs to go out)
		if(p_args.has_key("priority"))
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: Message has shutdown override priority", __FILE__, __LINE__);
			PowerMonitor::Work& w = (r.first->second);
			w.m_data.set("priority", "1");
		}
	}

	unlock_work();
	return ret;
}

void MyData::unset_work(const ats::StringMap& p_args)
{
	lock_work();
	h_expire_work();
	PowerMonitor::WorkMap::iterator i = m_work.find(p_args.get("key"));

	if(i == m_work.end())
	{
		unlock_work();
		return;
	}

	m_work.erase(i);
	unlock_work();
}

void MyData::h_expire_work()
{
	timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);

	PowerMonitor::WorkMap::iterator i = m_work.begin();

	while(i != m_work.end())
	{
		PowerMonitor::Work& w = i->second;
		PowerMonitor::WorkMap::iterator j = i;
		++i;

		const ats::String& expire = w.m_data.get("expire");

		if(!expire.empty())
		{

			if(strtol(expire.c_str(), 0, 0) < t.tv_sec)
			{
				m_work.erase(j);
			}

		}

	}

}

void MyData::list_jobs(std::vector <ats::String>& p_jobs, std::vector <int>& p_expire)
{
	p_jobs.clear();
	p_expire.clear();

	lock_work();
	h_expire_work();
	PowerMonitor::WorkMap::const_iterator i = m_work.begin();

	timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);

	while(i != m_work.end())
	{
		p_jobs.push_back(i->first);
		const PowerMonitor::Work& w = i->second;
		++i;

		const ats::String& expire = w.m_data.get("expire");

		if(expire.empty())
		{
			p_expire.push_back(-1);
		}
		else
		{
			int expire_remain = strtol(expire.c_str(), 0, 0);
			expire_remain = (expire_remain > t.tv_sec) ? (expire_remain - t.tv_sec) : 0;
			p_expire.push_back(expire_remain);
		}

	}

	unlock_work();
}


size_t MyData::number_of_priority_jobs_remaining()
{
	lock_work();
	h_expire_work();
	PowerMonitor::WorkMap::const_iterator i = m_work.begin();

	size_t njobs = 0;
	
	while(i != m_work.end())
	{
		const PowerMonitor::Work& w = i->second;
		++i;

		const ats::String& priority = w.m_data.get("priority");

		if(!priority.empty())
		{
			njobs++;
		}
	}

	unlock_work();
	return njobs;
}


size_t MyData::number_of_jobs_remaining()
{
	lock_work();
	h_expire_work();
	const size_t num = m_work.size();
	unlock_work();
	return num;
}

void MyData::set_shutdown_flag()
{
	lock_work();
	set("shutting_down", "1");
	unlock_work();
}

bool MyData::is_shutting_down() const
{
	lock_work();
	const bool b = h_is_shutting_down();
	unlock_work();
	return b;
}

bool MyData::h_is_shutting_down() const
{
	return get_bool("shutting_down");
}

// ************************************************************************
// Description: A local command server so that other applications or
//			developers can query this application.
//
//			One instance of this function is created for every local connection
//			made.
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
			if(c != -ENODATA) ats_logf(ATSLOG_DEBUG, "%s,%d: %s", __FILE__, __LINE__, ebuf);
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
			ats_logf(ATSLOG_ERROR, "%s,%d: command is too long", __FILE__, __LINE__);
			cmd.clear();
			command_too_long = false;
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "command is too long\n\r");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char* err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "gen_arg_list failed: %s\n\r", err);
			cmd.clear();
			continue;
		}

		if(g_dbg >= 2)
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d:%s: Command received: %s", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());
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
				(i->second).m_fn(acc, i->second, cb.m_argc, cb.m_argv);
			}
			else
			{
				send_cmd(acc.get_sockfd(), MSG_NOSIGNAL, "Invalid command \"%s\"\n\r", cmd.c_str());
			}
		}
	}
	return 0;
}


static pthread_t g_reset_button_thread;


//------------------------------------------------------------------------------------------------
// reset_button_task
//
// New for tl5000:
// New behaviour - receives '0', '1', '2', or '3' instead of original letters.
//    0 - button not pressed 
//    1 - button pressed < 3 seconds (release at this point does nothing) 				- solid orange power button
//    2 - button pressed for > 3 seconds < 10) - release causes a reboot 					- quick flash orange
//    3 - button pressed >10 and <20 - release causes factory restore							- slow flash orange
//    4 - button pressed >20 - release causes factory restore and db-config wipe	- alternate red/green on quick flash

static void* reset_button_task(void* p)
{
	MyData& md = *((MyData*)p);
  unsigned short count = 0;
  
	for(;;)
	{
		FILE* f = fopen("/dev/reset-button", "r");
		char action = '\0';
		ats_logf(ATSLOG_DEBUG, "reset_button_task - opened /dev/reset-button");

		for(;f;)
		{
			char c;
			const size_t nread = fread(&c, 1, 1, f);

			if(!nread)
			{
				fclose(f);
				f = NULL;
				break;
			}

			if('1' == c)
			{
				action = c;
		 		count = 0;
//				ats::write_file("/dev/set-gpio", "TU");  // Power LED to orange.
				ats_logf(ATSLOG_DEBUG, "%s,%d: 1 received - reset-button pressed", __FILE__, __LINE__);
			}
			else if('2' == c)
			{	
				action = c;
		 		count = 0;
//				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=reset app=/usr/redstone/reboot-blink.so\r");
				ats_logf(ATSLOG_DEBUG, "%s,%d: reset-button - 2 received", __FILE__, __LINE__);
			}
			else if('3' == c)
			{
				action = c;
		 		count = 0;
//				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink del reset\r");
				usleep(250 * 1000);
//				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=reset app=/usr/redstone/factory-blink.so\r");
				ats_logf(ATSLOG_DEBUG, "%s,%d: reset-button - 3 received", __FILE__, __LINE__);
			}
			else if('4' == c)
			{
				action = c;
		 		count = 0;
//				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink del reset\r");
				usleep(250 * 1000);
//				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=reset app=/usr/redstone/reset-blink.so \r");
				ats_logf(ATSLOG_ERROR, "%s,%d: reset-button - 4 received", __FILE__, __LINE__);
			}
			else if('0' == c)  // released the button - do the action
			{
//				ats::write_file("/dev/set-gpio", "Tu");  // Power LED to orange.
				ats_logf(ATSLOG_ERROR, "%s,%d: reset-button - 0 received", __FILE__, __LINE__);

				if('2' == action)  //reboot
				{
					ats::system("echo \"`date|tr -d '\\n'`: Reset button requested reboot\" >> /tmp/logdir/reset-button.txt");
//					ats::system("reboot");
				}
				else if('3' == action)
				{
					ats::system("echo \"`date|tr -d '\\n'`: Reset button requested factory restore\" >> /tmp/logdir/reset-button.txt");
//					ats::system("set_env fail_cnt 0xff;set_env fail_start yes");
//					ats::system("reboot");
				}
				else if('4' == action)
				{
					ats::system("echo \"`date|tr -d '\\n'`: Reset button requested full restore\" >> /tmp/logdir/reset-button.txt");
//					ats::system("db-config unset");
//					ats::system("set_env fail_cnt 0xff;set_env fail_start yes");
//					ats::system("reboot");
				}
				action = '\0';
			}
		}
	}

	return 0;
}

static int apply_value_bounds(int p_val, int p_min, int p_max)
{
	return (p_val < p_min) ? p_min : ((p_val > p_max) ? p_max : p_val);
}

// Description: Sleeps for less than a second using millisecond resolution, or sleeps for one second
//	or more, using one second resolution.
static void milli_sleep(int p_msec)
{

	if(p_msec < 1000)
	{
		usleep(p_msec * 1000);
	}
	else
	{
		sleep(p_msec / 1000);
	}

}

static void* battery_monitor(void*)
{
	int set_size;
	int update_period;
	{
//		db_monitor::ConfigDB db;

//		const int max_set_size = 256; // Maximum set size for averaging
//		const int min_set_size = 1;
		set_size = 5;// apply_value_bounds(db.GetInt("power-monitor", "BattMonitorSetSize", 5), min_set_size, max_set_size);

//		const int max_update_period = 864 * 1000 * 1000; // 10 days in milliseconds
//		const int min_update_period = 100; // 10 times a second is the fastest rate allowed
		update_period = 1000; //apply_value_bounds(db.GetInt("power-monitor", "BattMonitorUpdatePeriod", 1000), min_update_period, max_update_period);
	}

	REDSTONE_IPC redStoneData;
	ats::battery::init_adc2volt();

	MovingAverage avg;
	avg.SetSize(set_size);
	int count = 0;	

	for(;;)
	{
		const int fd = open("/dev/battery", O_RDONLY);
	
		for(;fd >= 0;)
		{
			char buf[27];
			const int len = read(fd, buf, sizeof(buf) - 1);

			if(len <= 0)
			{
				close(fd);
				break;
			}

			buf[len] = '\0';
			const double dval = atof(buf);
			avg.Add(dval);
			const double voltage = (double)ats::battery::ADC2Voltage((int)(avg.Average())); 
			redStoneData.BatteryVoltage(voltage);

			if(count < set_size) // set reading valid after at least 1 complete set
			{
				redStoneData.BatteryVoltageValid((++count) >= set_size);
			}

			// XXX: This timing method will drift due to Kernel context switches, etc. This is acceptable.
			milli_sleep(update_period);
		}

		milli_sleep(update_period);
	}

	return 0;
}

int main(int argc, char* argv[])
{
  int dbg_level;
	{  // scoping is required to force closure of db-config database
		db_monitor::ConfigDB db;
		dbg_level = db.GetInt(argv[0], "LogLevel", 0);  // 0:Error 1:Debug 2:Info
	}
	
	g_log.set_global_logger(&g_log);
	g_log.set_level(dbg_level);
	g_log.open_testdata(g_app_name);
	ATSLogger::set_global_logger(&g_log);

	MyData md;
	md.set("debug", "1");
	md.set_from_args(argc - 1, argv + 1);

	g_dbg = md.get_int("debug");

	static pthread_t battery_monitor_thread;
	pthread_create(&battery_monitor_thread, 0, battery_monitor, 0);

	// AWARE360 FIXME: Internal TCP socket command server is deprecated. All clients shall use the
	//	Unix domain socket version instead.
	{
		static ServerData server_data;
		ServerData* sd = &server_data;
		init_ServerData(sd, 64);
		sd->m_port = 41009;
		sd->m_hook = &md;
		sd->m_cs = local_command_server;
		::start_server(sd);
		signal_app_tcp_socket_ready(g_app_name.c_str(), ats::toStr(sd->m_port).c_str());
	}

	{
		static ServerData sd;
		init_ServerData(&sd, 64);
		sd.m_hook = &md;
		sd.m_cs = local_command_server;
		::start_trulink_ud_server(&sd, g_app_name.c_str(), 1);
		signal_app_unix_socket_ready(g_app_name.c_str(), g_app_name.c_str());
	}

	{
		PowerMonitorStateMachine* w = new PowerMonitorStateMachine(md);
		w->run_state_machine(*w);
	}

// DRH removed for now - conflict with SPI????
	pthread_create(&g_reset_button_thread, 0, reset_button_task, &md);
	signal_app_ready(g_app_name.c_str());
	ats::infinite_sleep();
	return 0;
}
