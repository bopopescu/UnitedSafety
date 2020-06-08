#include <iostream>
#include <map>
#include <list>
#include <fstream>

#include <syslog.h>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/ioctl.h>
#include "ConfigDB.h"

#include "linux/i2c-dev.h"
#include "db-monitor.h"
#include "ats-common.h"
#include "ats-string.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "OnTime.h"
#include "TimerEngine.h"
#include "TimerControlBlock.h"
#include "MyData.h"
#include "RegisteredLogger_socket.h"
#include "SocketReferenceManager.h"

static ATSLogger g_log;
static int g_fd = -1;

int set_wdt( MyData &p_md, unsigned int p_seconds)
{
	p_md.lock();
	int control = i2c_smbus_read_byte_data(g_fd, 7);

	if(control >= 0)
	{
		control &= ((~0x60) & 0xffff);
		i2c_smbus_write_byte_data(g_fd, 7, control);
		i2c_smbus_write_byte_data(g_fd, 4, p_seconds & 0xff);
		i2c_smbus_write_byte_data(g_fd, 5, (p_seconds >> 8) & 0xff);
		i2c_smbus_write_byte_data(g_fd, 6, (p_seconds >> 16) & 0xff);
		control = i2c_smbus_read_byte_data(g_fd, 7);

		if(control >= 0)
		{
			control |= 0x60;
		}

	}

	p_md.unlock();
	return 0;
}

int set_rtc_control( MyData& p_md, const ats::String& p_bit, int p_val)
{
	int control = i2c_smbus_read_byte_data(g_fd, 7);

	if(control >= 0)
	{
		const ats::String& bit = p_bit;
		const int val = p_val ? 1 : 0;

		if("aie" == bit)
		{
			control &= 0xFE;
			control |= val;
		}
		else if("rs1" == bit)
		{
			control &= 0xFD;
			control |= val << 1;
		}
		else if("rs2" == bit)
		{
			control &= 0xFB;
			control |= val << 2;
		}
		else if("wdstr" == bit)
		{
			control &= 0xF7;
			control |= val << 3;
		}
		else if("bbsqw" == bit)
		{
			control &= 0xEF;
			control |= val << 4;
		}
		else if("wd/alm" == bit)
		{
			control &= 0xDF;
			control |= val << 5;
		}
		else if("wace" == bit) // wace - Watchdog/Alarm Counter Enable
		{
			control &= 0xBF;
			control |= val << 6;
		}
		else if("eosc" == bit)
		{
			control &= 0x7F;
			control |= val << 7;
		}
		else
		{
			return -EINVAL;
		}

		i2c_smbus_write_byte_data(g_fd, 7, control);
	}

	return 0;
}

static int set_rtc( MyData &p_md, unsigned int *p_seconds, bool p_force, bool p_gps);

static int set_system_time( MyData &p_md, bool p_force);

static void run_scheduler(MyData& p_md)
{
	MyData& md = p_md;

	Task& task = md.m_task;
	Schedule& sched = md.m_schedule;

	for(;;)
	{
		++md.m_scheduler_loop_cycles;
		sleep(1);

		md.lock_sched();
		Schedule::iterator next_trigger_tasks = sched.begin();

		if(sched.end() == next_trigger_tasks)
		{
			md.unlock_sched();
			continue;
		}

		struct timeval tv;
		gettimeofday(&tv, 0);
		const time_t current_time = tv.tv_sec;
		const time_t wakeup_time = (next_trigger_tasks->first);

		if((!md.m_next_trigger_date) || (wakeup_time && (wakeup_time < md.m_next_trigger_date)))
		{
			md.m_next_trigger_date = wakeup_time;
			md.set_wdt_for_next_trigger_date(current_time, wakeup_time);
		}

		if(current_time >= wakeup_time)
		{
			md.m_next_trigger_date = 0;
			enable_watchdog_alarm_counter(false);  // clear the RTC IO flag to prevent immediate wakeup
			ScheduleList& list = next_trigger_tasks->second;
			ats_logf(ATSLOG_ERROR, "%s,%d: Expired timer found", __FILE__, __LINE__);

			std::map <const ats::String, void*>::iterator i_task_to_trigger = list.begin();

			while(i_task_to_trigger != list.end())
			{
				const ats::String key_of_task_to_trigger(i_task_to_trigger->first);
				std::map <const ats::String, void*>::iterator j = i_task_to_trigger;
				++i_task_to_trigger;
				list.erase(j);

				Task::iterator i = task.find(key_of_task_to_trigger);

				if(i != task.end())
				{
					const ats::String& task_key = i->first;
					ScheduleTask& t = *(i->second);

					// ATS FIXME: Not triggering events yet (no custom callback is called here), because of mutex.
					//	Copy trigger tasks outside of critical section, then trigger from there.
					if(t.m_repeat_period > 0)
					{
						const time_t trigger_date = current_time + t.m_repeat_period;

						std::pair <Schedule::iterator, bool> r = sched.insert(SchedulePair(trigger_date, ScheduleList()));

						ScheduleList& list = (r.first)->second;

						list.insert(std::pair<const ats::String, void*>(task_key, 0));
					}

				}

			}

			if(list.empty())
			{
				sched.erase(next_trigger_tasks);
			}

		}

		md.unlock_sched();
	}
}

unsigned int get_rtc_control( MyData &p_md)
{
	p_md.lock();
	const int status = i2c_smbus_read_byte_data(g_fd, 7);
	p_md.unlock();
	return status;
}

unsigned int get_rtc_status( MyData &p_md)
{
	p_md.lock();
	const int status = i2c_smbus_read_byte_data(g_fd, 8);
	p_md.unlock();
	return status;
}

// Description: Returns the current RealTime Clock seconds since the standard Unix epoch.
//
//	XXX: DO NOT CALL "set_rtc" from within this function.
//
// Return:
unsigned int get_rtc( MyData &p_md)
{
	p_md.lock();
	const unsigned int lo = i2c_smbus_read_word_data(g_fd, 0);
	if(lo < 0)
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: Error reading Time-Of-Day (TOD) Counter value. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		p_md.unlock();
		return -1;
	}
	const unsigned int hi = i2c_smbus_read_word_data(g_fd, 2);
	if(hi < 0)
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: Error reading Time-Of-Day (TOD) Counter value. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		p_md.unlock();
		return -1;
	}
	p_md.unlock();
	return ((hi << 16) | lo);
}

static int set_rtc( MyData &p_md, unsigned int *p_seconds, bool p_force, bool p_gps)
{
	const unsigned int rtc_seconds = get_rtc(p_md);
	timeval tv;
	if(p_seconds)
	{
		memset(&tv, 0, sizeof(tv));
		tv.tv_sec = *p_seconds;
	}
	else
	{
		gettimeofday(&tv, 0);
	}

	ats_logf(ATSLOG_DEBUG, "%s,%d: p_seconds=%d, p_force=%d, p_gps=%d, rtc_sec=%u, sys_sec=%u",
		__FILE__, __LINE__,
		p_seconds ? *p_seconds : 0,
		p_force ? 1 : 0,
		p_gps ? 1 : 0,
		(unsigned int)tv.tv_sec,
		(unsigned int)rtc_seconds);

	if(!p_force)
	{
		if(p_gps)
		{
			if(abs(tv.tv_sec - rtc_seconds) < 2) return 0;
		}
		else
		{
			if((tv.tv_sec - rtc_seconds) < 2) return 0;
		}
	}

	p_md.lock();
	int status = i2c_smbus_read_byte_data(g_fd, 7);
	if(status >= 0)
	{
		status |= 0x0080;
		i2c_smbus_write_byte_data(g_fd, 7, status);
		i2c_smbus_write_word_data(g_fd, 0, ((unsigned int)tv.tv_sec) & 0xffff);
		i2c_smbus_write_word_data(g_fd, 2, ((unsigned int)tv.tv_sec) >> 16);
		status = i2c_smbus_read_byte_data(g_fd, 7);
		if(status >= 0)
		{
			status &= 0xff7ff;
			i2c_smbus_write_byte_data(g_fd, 7, status);
		}
	}
	p_md.unlock();
	return 0;
}

unsigned int get_wdt( MyData &p_md)
{
	p_md.lock();
	const unsigned int lo = i2c_smbus_read_word_data(g_fd, 4);
	if(lo < 0)
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: Error reading Watch Dog (WDT) Counter value. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		p_md.unlock();
		return -1;
	}
	const unsigned int hi = i2c_smbus_read_byte_data(g_fd, 6);
	if(hi < 0)
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: Error reading Watch Dog (WDT) Counter value. (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		p_md.unlock();
		return -1;
	}
	p_md.unlock();
	return ((hi << 16) | lo);
}

void enable_watchdog_alarm_counter( bool p_enable)
{
	int control = i2c_smbus_read_byte_data(g_fd, 7);
	if(control >= 0)
	{
		control &= 0xBE; // WACE and AIE are set to zero
		control |= ((p_enable ? 1 : 0) << 6) | (p_enable ? 1 : 0); // WACE and AIE are set to p_enable
		i2c_smbus_write_byte_data(g_fd, 7, control);
		if(!p_enable)
		{
			int status = i2c_smbus_read_byte_data(g_fd, 8);
			if(status >= 0)
			{
				i2c_smbus_write_byte_data(g_fd, 8, status & 0xfe);
			}
		}
	}
}

static int set_system_time( MyData &p_md, bool p_force)
{
	timeval tv;
	memset(&tv, 0, sizeof(tv));
	tv.tv_sec = get_rtc(p_md);

	if(!p_force)
	{
		timeval now;
		memset(&now, 0, sizeof(now));
		gettimeofday(&now, 0);
		if((tv.tv_sec - now.tv_sec) < 2) return 0;
	}

	if(settimeofday(&tv, 0))
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: Error setting system time from RTC, (%d) %s", __FILE__, __LINE__, errno, strerror(errno));
		return -errno;
	}
	return 0;
}

static void test_rtc(ClientData& p_cd, CommandBuffer& p_cb, MyData& p_md)
{
	send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "%s: ok\n", p_cb.m_argv[0]);
	// Maximum number of seconds to count.
	const int max_count = 5;

	// Due to RTC vs CPU clock drift, allow a few seconds extra when collecting
	// a sequential count.
	const int extra_count = 2;

	// Polling frequency when trying to detect RTC clock ticks. The frequency must
	// take into account context switch and other delays, however it is expected
	// that the Operating System will be at least 95% idle.
	const int loops_per_second = 4;

	const int iteration_delay = 250000; // 250 ms

	// This limits the time taken to detect a failing RTC.
	const int max_loops = loops_per_second * (max_count + extra_count);

	int count[max_count] = {0, 0, 0, 0, 0};
	int i;
	int j;

	// Wait for the RTC to fail due to timeout, or until it has generated the
	// required sequence (which ever comes first).
	send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "Waiting for %d ticks\n", max_count);

	for(i = 0, j = 0; (i < max_count) && (j < max_loops); ++j)
	{
		const int val = get_rtc(p_md);

		if(val != count[i ? (i-1) : i])
		{
			count[i++] = val;
			send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "\ttick %d/%d%s\n", i, max_count, (i >= max_count) ? " complete" : "");
		}

		if((i < max_count) && (j < max_loops))
		{
			usleep(iteration_delay);
		}

	}

	if(i != max_count)
	{
		send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "failed: short tick count [got %d but expected %d]\n\tSequence: ", i, max_count);
		int j;

		for(j = 0; j < i; ++j)
		{
			send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "0x%08X%s", count[j], ((j+1) < i) ? "," : "");
		}

		send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "\n\r");

		return;
	}

	for(i = 0; i < (max_count - 1); ++i)
	{

		if(count[i + 1] != ((count[i] + 1)))
		{
			send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "failed: Sequence error [got [%d]=0x%08X but expected [%d]=0x%08X]\n\tSequence: ", i + 1, count[i + 1], i, count[i] + 1);

			{

				for(i = 0; i < max_count; ++i)
				{
					send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "0x%08X%s", count[i], ((i+1) < max_count) ? "," : "");
				}

				send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "\n\r");
			}

			return;
		}

	}

	send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "pass: ");

	for(i = 0; i < max_count; ++i)
	{
		send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "0x%08X%s", count[i], ((i+1) < max_count) ? "," : "");
	}

	send_cmd(p_cd.m_sockfd, MSG_NOSIGNAL, "\n\r");
}

// ************************************************************************
// Description: Command server.
//
// Parameters:  p - pointer to ClientData
// Returns:     NULL pointer
// ************************************************************************
static void* client_command_server(void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData& md = *((MyData*)(cd->m_data->m_hook));

	bool command_too_long = false;
	ats::String cmd;

	const size_t max_command_length = 1024;
	ClientDataCache cache;
	init_ClientDataCache(&cache);

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cache);

		if(c < 0)
		{

			if(c != -ENODATA)
			{
				ats_logf(ATSLOG_ERROR, "%s,%d: %s", __FILE__, __LINE__, ebuf);
			}

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
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "error: command is too long\r");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char* err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "error: gen_arg_list failed: %s\r", err);
			cmd.clear();
			continue;
		}

		cmd.clear();

		if(cb.m_argc <= 0)
		{
			continue;
		}

		const ats::String cmd(cb.m_argv[0]);
		ats::StringMap args;
		args.from_args(cb.m_argc - 1, cb.m_argv + 1);

		if("sched" == cmd)
		{
			md.sched_task(args);
		}
		else if("unsched" == cmd)
		{
			md.unsched_task(args);
		}
		else if("unsched_all" == cmd)
		{
			md.unsched_all_tasks(args);
		}
		else if("set-rtc" == cmd)
		{
			ats_logf(ATSLOG_ERROR, "Setting the Real Clock Time (RTC) from system time");

			if(set_rtc(md, 0, args.get_int("force"), args.get_int("gps")))
			{
				ats_logf(ATSLOG_ERROR, "%s,%d: Failed to set RTC time from the system time", __FILE__, __LINE__);
			}

		}
		else if("set-sys" == cmd)
		{
			ats_logf(ATSLOG_ERROR, "Settting the system time from the RTC time");
			const int err = set_system_time( md, args.get_int("force"));

			if(err)
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "Failed to set system time from RTC (errno=%d)\n", err);
			}

		}
		else if("set-wdt" == cmd)
		{
			if(cb.m_argc < 2)
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "usage: set-wdt <seconds>\n");
			}
			else
			{
				ats_logf(ATSLOG_ERROR, "%s,%d: Setting WDT to %d seconds", __FILE__, __LINE__, (int)strtol(cb.m_argv[1], 0, 0));
				set_wdt(md, strtol(cb.m_argv[1], 0, 0));
			}
		}
		else if("alarm" == cmd)
		{
			ats::StringMap arg;
			arg.from_args(cb.m_argc - 1, cb.m_argv + 1);

			// ATS FIXME: This is a debug command. Memory for "tcb" will be leaked (it is not garbage
			//	collected).
			TimerControlBlock_socket_alarm* tcb = new TimerControlBlock_socket_alarm(cd->m_sockfd);

			const ats::String& emsg = md.alarm(arg, tcb);

			if(!emsg.empty())
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "alarm: error\n%s\n\r", emsg.c_str());
				continue;
			}

		}
		else if("get-status" == cmd)
		{
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "0x%08x\n", get_rtc_status(md));
		}
		else if("get-control" == cmd)
		{
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "0x%08x\n", get_rtc_control(md));
		}
		else if("get-rtc" == cmd)
		{
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%u\n", get_rtc(md));
		}
		else if("get-wdt" == cmd)
		{
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%08X\n", get_wdt(md));
		}
		else if("set-control" == cmd)
		{

			if(cb.m_argc >= 3)
			{
				const char* bit = cb.m_argv[1];
				const char* val = cb.m_argv[2];
				const int err = set_rtc_control( md, bit, strtol(val, 0, 0) ? 1 : 0);

				if(err)
				{
					send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "error %s for bit=\"%s\", val=%s\n", strerror(-err), bit, val);
				}

			}
			else
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "usage: set-control <bit> <boolean>\n");
			}

		}
		else if("stats" == cmd)
		{
			std::stringstream s;
			md.print_stats(s);
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s", s.str().c_str());
		}
		else if(("debug" == cmd) || ("dbg" == cmd))
		{

			if(cb.m_argc >= 2)
			{
				md.m_dbg = strtol(cb.m_argv[1], 0, 0);
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "debug=%d\n", md.m_dbg);
			}
			else
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "debug=%d\n", md.m_dbg);
			}

		}
		else if("start_logging" == cmd)
		{
			g_log.open_testdata(md.m_config.get("app_name"));
		}
		else if("db_dir_ready" == cmd)
		{
			md.init_db_logger();
		}
		else if("sql_logger" == cmd)
		{
			ats::StringMap m;
			m.from_args(cb.m_argc - 1, cb.m_argv + 1);
			const ats::String& emsg = md.sql_logger(m);

			if(!(emsg.empty()))
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s\n\r", emsg.c_str());
			}

		}
		else if("mfg" == cmd)
		{

			if(cb.m_argc >= 2)
			{
				const char* mfg_cmd = cb.m_argv[1];

				if(!strcmp("test-rtc", mfg_cmd))
				{
					test_rtc(*cd, cb, md);
				}
				else
				{
					send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: error\nInvalid %s command \"%s\"\n\r", cmd.c_str(), cmd.c_str(), mfg_cmd);
				}

				continue;
			}

			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: error\nusage: %s <command>\n\r", cmd.c_str(), cmd.c_str());
			continue;
		}
		else
		{
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: Invalid command \"%s\"\n", __FUNCTION__, cmd.c_str());
			continue;
		}

		// ATS FIXME: "ok" must be printed before response messages.
		send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: ok\n\r", cmd.c_str());
	}

	return 0;
}

static void* logger_server(void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData& md = *((MyData*)(cd->m_data->m_hook));

	RegisteredLogger_socket rl(md, cd->m_sockfd);

	{
		const ats::String& emsg = md.register_logger(ats::toStr(&rl), "-", rl);

		if(!emsg.empty())
		{
			send_cmd(rl.m_sockfd, MSG_NOSIGNAL, "%s,%d: Failed to register logger. emsg=%s\n\r", __FILE__, __LINE__, emsg.c_str());
		}

	}

	ClientDataCache cache;
	init_ClientDataCache(&cache);
	bool command_too_long = false;
	ats::String cmd;
	const size_t max_command_length = 1024;

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cache);

		if(c < 0)
		{

			if(c != -ENODATA)
			{
				ats_logf(ATSLOG_ERROR, "%s,%d: %s", __FILE__, __LINE__, ebuf);
			}

			break;
		}

		if(c != '\r' && c != '\n')
		{

			if(cmd.length() >= max_command_length)
			{
				command_too_long = true;
			}
			else
			{
				cmd += c;
			}

			continue;
		}

		if(command_too_long)
		{
			cmd.clear();
			command_too_long = false;
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "command is too long:\n\r");
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char *err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "gen_arg_list failed: %s\n\r", err);
			cmd.clear();
			continue;
		}

		cmd.clear();

		if(cb.m_argc <= 0)
		{
			continue;
		}

		const ats::String cmd(cb.m_argv[0]);
		ats::StringMap args;
		args.from_args(cb.m_argc - 1, cb.m_argv + 1);

		if("log" == cmd)
		{
			const ats::String& ontime = args.get("ontime");

			if(!ontime.empty())
			{
				TimerControlBlock_log_ontime* tcb = new TimerControlBlock_log_ontime(
					md,
					args.get("key"),
					args.get("des"));

				ats::StringMap sm;
				sm.set("key", args.get("key"));
				sm.set("time", ontime);
				const ats::String& emsg = md.alarm(sm, tcb);

				if(!emsg.empty())
				{
					send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "log: error\n%s\n\r", emsg.c_str());
				}
				else
				{
					send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "log: ok\n\r");
				}

			}
			else if(args.has_key("onnew"))
			{
				md.register_log_event(
					"onnew",
					args.get("key"),
					args.get("des"));
					
			}
			else if(args.has_key("onchanged"))
			{
				md.register_log_event(
					"onchanged",
					args.get("key"),
					args.get("des"));
			}
			else
			{
				struct timeval t;
				gettimeofday(&t, 0);
				const ats::String& emsg = md.request_log(
					args.get("key"),
					args.get("des"),
					int(t.tv_sec),
					int(t.tv_usec) / 1000);

				if(!emsg.empty())
				{
					send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "log: error\n%s\n\r", emsg.c_str());
				}
				else
				{
					send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "log: ok\n\r");
				}

			}

		}
		else if("unlog" == cmd)
		{
			const ats::String& emsg = md.remove_alarm(args.get("key"));

			if(!emsg.empty())
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "unlog: error\n%s\n\r", emsg.c_str());
			}
			else
			{
				send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "unlog: ok\n\r");
			}

		}
		else
		{
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: Invalid command \"%s\"\n\r", __FUNCTION__, cmd.c_str());
		}

	}

	return 0;
}

static void* loggen_server(void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData& md = *((MyData*)(cd->m_data->m_hook));

	bool command_too_long = false;
	ats::String cmd;

	const size_t max_command_length = 1024;

	ClientDataCache cache;
	init_ClientDataCache(&cache);

	SocketReferenceManager srm(cd->m_sockfd);
	RegisteredLogGenerator rl(srm);

	for(;;)
	{
		char ebuf[1024];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cache);

		if(c < 0)
		{

			if(c != -ENODATA)
			{
				ats_logf(ATSLOG_ERROR, "%s,%d: %s", __FILE__, __LINE__, ebuf);
			}

			break;
		}

		if(c != '\r' && c != '\n')
		{

			if(cmd.length() >= max_command_length)
			{
				command_too_long = true;
			}
			else
			{
				cmd += c;
			}

			continue;
		}

		if(command_too_long)
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: command is too long", __FILE__, __LINE__);
			cmd.clear();
			command_too_long = false;
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "command is too long\n\r");
			continue;
		}

		if('[' == cmd.c_str()[0])
		{
			const ats::String log(cmd.c_str() + 1, cmd.length() - 2);
			ats::StringList loga;
			MyData::parse_log(log, loga);

			const ats::String* destination = (loga.size() >= 2) ? &(loga[1]) : 0;
			bool log_sent = false;

			if(destination)
			{
				log_sent = md.forward_log_message(*destination, cmd);
			}

			if(!log_sent)
			{
				syslog(LOG_NOTICE, "%s", cmd.c_str());
			}

			cmd.clear();
			continue;
		}

		CommandBuffer cb;
		init_CommandBuffer(&cb);
		const char *err;

		if((err = gen_arg_list(cmd.c_str(), cmd.length(), &cb)))
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "gen_arg_list failed: %s\n\r", err);
			cmd.clear();
			continue;
		}

		cmd.clear();

		if(cb.m_argc <= 0)
		{
			continue;
		}

		const ats::String cmd(cb.m_argv[0]);
		ats::StringMap args;
		args.from_args(cb.m_argc - 1, cb.m_argv + 1);

		if("reg" == cmd)
		{
			const ats::String& emsg = md.register_log_generator(args, rl);

			if(!emsg.empty())
			{
				syslog(LOG_NOTICE, "%s,%d: emsg=%s\n", __FILE__, __LINE__, emsg.c_str());
			}

		}
		else
		{
			send_cmd(cd->m_sockfd, MSG_NOSIGNAL, "%s: Invalid command \"%s\"\n", __FUNCTION__, cmd.c_str());
		}

	}

	srm.shutdown();
	return 0;
}

static void enable_rtc_oscillator_if_not_enabled(MyData& p_md)
{
	const int control = get_rtc_control(p_md);
	const int eosc = 0x80;

	if(control & eosc)
	{
		set_rtc_control(p_md, "eosc", 0);
	}

}

// XXX: RTC-Monitor starts before USB logging has been enabled. This means that
//	RTC-Monitor cannot immediately start logging to the USB (it uses syslog instead).
//	The USB will become available after a few seconds, so delayed logging is used.
//
//	Also note that even though "/var/log" may exist when RTC-Monitor starts, that
//	directory will be destroyed (or covered) by a subsequent USB mount. This means
//	that logs will not be visible if RTC-Monitor were not to wait for the USB mount.
//
//	The current solution is to start logging on demand. An external process will signal
//	RTC-Monitor to start logging. This is done by issuing the "start_logging" command.
int main(int argc, char *argv[])
{
	int log_level;
	
	{
		db_monitor::ConfigDB db;
		log_level = db.GetInt("RedStone","LogLevel", 0);
	}

	g_log.set_global_logger(&g_log);

	static MyData md;

	ats::StringMap &config = md.m_config;

	config.set("app_name", "rtc-monitor");
	config.set("user", "applet");
	config.set("rtc-chip-address", "0x68");
	config.from_args(argc - 1, argv + 1);

	md.m_dbg = config.get_int("debug");
	g_log.set_level(log_level);

	ats_logf(ATSLOG_ERROR, "RTC monitor started");

	int &fd = g_fd = open("/dev/rtc", O_RDWR);

	if(fd < 0)
	{
		ats_logf(ATSLOG_ERROR, "ERR: Failed to open i2c-0");
		return 1;
	}

	if(ioctl(fd, I2C_SLAVE, config.get_int("rtc-chip-address")))
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: ERR (%d): %s", __FILE__, __LINE__, errno, strerror(errno));
		return 1;
	}

	enable_rtc_oscillator_if_not_enabled(md);

	set_system_time( md, false);

	enable_watchdog_alarm_counter(false);

	if(fork())
	{
		return 0;
	}

	daemon(0, 0);
	md.load_sched_tasks();

	const ats::String& user = config.get("user");

	ServerData server_data[3];

	{
		ServerData& sd = server_data[0];
		init_ServerData(&sd, 64);
		sd.m_hook = &md;
		sd.m_cs = client_command_server;
		set_unix_domain_socket_user_group(&sd, user.c_str(), user.c_str());
		::start_redstone_ud_server(&sd, "rtc-monitor", 1);
	}

	{
		ServerData& sd = server_data[1];
		init_ServerData(&sd, 256);
		sd.m_hook = &md;
		sd.m_cs = logger_server;
		set_unix_domain_socket_user_group(&sd, user.c_str(), user.c_str());
		::start_redstone_ud_server(&sd, "rtc-monitor-logger", 1);
	}

	{
		ServerData& sd = server_data[2];
		init_ServerData(&sd, 256);
		sd.m_hook = &md;
		sd.m_cs = loggen_server;
		set_unix_domain_socket_user_group(&sd, user.c_str(), user.c_str());
		::start_redstone_ud_server(&sd, "rtc-monitor-loggen", 1);
	}

	run_scheduler(md);
	return 0;
}
