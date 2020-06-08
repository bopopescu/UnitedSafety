#pragma once

#include <iostream>
#include <map>
#include <list>
#include <fstream>

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

#include "linux/i2c-dev.h"
#include "db-monitor.h"
#include "ats-common.h"
#include "ats-string.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "OnTime.h"
#include "RegisteredLogGenerator.h"

typedef ats::String ScheduleKey;
typedef std::map <const ScheduleKey, void*> ScheduleList;

class TimerEngine;
class SocketReferenceManager;
class ScheduleTask;
class RegisteredLogger_console;
class RegisteredLogger_socket;
class RegisteredLogger_sql;

class Task : public std::map <const ScheduleKey, ScheduleTask*>
{
public:
	typedef std::pair <const ScheduleKey, ScheduleTask*> TaskMapPair;
};

typedef time_t ScheduleTime;

typedef std::map <ScheduleTime, ScheduleList> Schedule;
typedef std::pair <ScheduleTime, ScheduleList> SchedulePair;

class MyData
{
public:
	typedef std::map <int, TimerEngine*> AlarmMap;
	typedef std::pair <int, TimerEngine*> AlarmPair;

	typedef std::map <const ats::String, int> StrIntMap;
	typedef std::pair <const ats::String, int> StrIntPair;

	MyData();

	void lock() const;

	void unlock() const;

	void lock_alarm() const;

	void unlock_alarm() const;

	void lock_sched() const;

	void unlock_sched() const;

	void lock_logger() const;

	void unlock_logger() const;

	void lock_loggen() const;

	void unlock_loggen() const;

	// Description:
	//
	// Parameters:
	//
	//	p_tcb - The Timer Control Block that will manage the alarm. "p_tcb" must be non-NULL.
	//		If there is an error adding the alarm, then "p_tcb" will be deleted (class
	//		MyData will take ownership of "p_tcb").
	// Options:
	//
	ats::String alarm(const ats::StringMap& p_arg, TimerControlBlock* p_tcb);

	ats::String remove_alarm(const ats::String& p_key);

	void save_sched_tasks();

	void load_sched_tasks();

	// Description: Add a new Real Time Clock (RTC) schedule for the given key. If an RTC schedule
	//	with the given key already exists, then that schedule will be overwritten with the new
	//	schedule.
	//
	// Parameters (within p_args):
	//
	//	key - The name/key of the schedule.
	//
	//	from_now - If set to 1, then the given "trigger_date" specifies the number of seconds
	//		in the future (from now/current-time).
	//
	//	trigger_date - The date to trigger in Unix seconds since the epoch. The "from_now"
	//		option (if specified) may modify the meaning of this option. See "from_now" for
	//		more information.
	//
	//	trigger_command - The shell command to run when the trigger is activated.
	//
	//	repeat_period - The RTC schedule repeat period in seconds. If specified and greater than zero,
	//		the given RTC schedule will repeat after waiting the specified number of seconds.
	//		If not specified (or is 0 or negative), then the RTC schedule will occur only once.
	void sched_task(const ats::StringMap& p_args);

	void unsched_task(const ats::StringMap& p_args);

	void unsched_all_tasks(const ats::StringMap& p_args);

	void set_wdt_for_next_trigger_date(time_t p_current_time, time_t p_date);

	void print_stats(std::ostream& p_out) const;

	RegisteredLogGeneratorMap m_loggen;

	// Description: A mapping (1 to many) of log output devices (by device name) to RegisteredLoggers
	//	that will perform the log output service for the named device.
	//
	//	+-----------+                +-----------------+
	//	|device name|1              *| RegisteredLogger|
	//	|           |--------------->|                 |
	//	+-----------+                +-----------------+
	RegisteredLoggerMap m_logger;

	// Description: Registers "p_rl" as the logger for destination/output device "p_des". "p_key" will
	//	be used to identify logger "p_rl" and must be unique for each device "p_des". "p_key" cannot
	//	be the empty string.
	//
	//	If a RegisteredLogger already exists on destination/output device "p_des" with key "p_key",
	//	then the existing RegisteredLogger will be deregistered for that device/key combination and
	//	will be replaced with the RegisteredLogger "p_rl".
	//
	// Return: The empty string is returned on success and an error message is returned otherwise.
	ats::String register_logger(const ats::String& p_key, const ats::String& p_des, RegisteredLogger& p_rl);

	// Description: Removes the destination/output device logger registered for device "p_des" with key "p_key".
	//	Either a single logger will be deregistered (match found) or no logger will be deregistered (no
	//	match found).
	void deregister_logger(const ats::String& p_des, const ats::String& p_key);

	// Description: Removes all destination/output device loggers registered for device "p_des".
	void deregister_logger(const ats::String& p_des);

	bool forward_log_message(const ats::String& p_des, const ats::String& p_log);

	ats::String register_log_generator(const ats::StringMap& p_arg, RegisteredLogGenerator& p_rl);

	ats::String request_log(
		const ats::String& p_key,
		const ats::String& p_des,
		int p_sec,
		int p_msec);

	ats::String register_log_event(
		const ats::String& p_log_event_type,
		const ats::String& p_key,
		const ats::String& p_des);

	static bool parse_log(const ats::String& p_log, ats::StringList& p_list);

	static const ats::String m_schedule_fname;

	Task m_task;
	Schedule m_schedule;
	AlarmMap m_alarm;
	StrIntMap m_rev_alarm;
	ats::StringMap m_config;

	// Description:
	//
	// XXX: Only member variables and "static void run_scheduler()" may access this variable.
	time_t m_next_trigger_date;

	size_t m_scheduler_loop_cycles;

	int m_dbg;

	pthread_t m_timer_thread;

	RegisteredLogger_console* m_logger_console;
	RegisteredLogger_socket* m_logger_socket;
	RegisteredLogger_sql* m_logger_sql;

	typedef std::map <const ats::String, RegisteredLogger*> RegLogMap;
	typedef std::pair <const ats::String, RegisteredLogger*> RegLogPair;

	RegLogMap m_reg_log;

	void init_db_logger();
	ats::String sql_logger(const ats::StringMap& p);

private:
	pthread_mutex_t* m_mutex;
	pthread_mutex_t* m_sched_mutex;
	pthread_mutex_t* m_alarm_mutex;
	pthread_mutex_t* m_logger_mutex;
	pthread_mutex_t* m_loggen_mutex;

	void h_sched_task(const ats::StringMap& p_args);
	void h_remove_task_name_from_schedule(const ats::StringMap& p_args);
	size_t h_num_schedules_for_task(const ats::String& p_key) const;
	void h_deregister_logger(RegisteredLogger& p_rl);
};

class ScheduleTask
{
public:
	ScheduleTask()
	{
		m_monotonic = false;
		m_next_trigger_date = 0;
	}

	bool m_monotonic;

	time_t m_trigger_date;
	time_t m_repeat_period;
	ats::String m_trigger_command;

	struct timespec* m_next_trigger_date;
};

int set_wdt(MyData &p_md, unsigned int p_seconds);
int set_rtc_control(MyData& p_md, const ats::String& p_bit, int p_val);
void enable_watchdog_alarm_counter(bool p_enable);
