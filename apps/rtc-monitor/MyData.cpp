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
#include "ConfigDB.h"
#include "ats-common.h"
#include "ats-string.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "OnTime.h"
#include "TimerEngine.h"
#include "MyData.h"
#include "SocketReferenceManager.h"
#include "RegisteredLogger_console.h"
#include "RegisteredLogger_socket.h"
#include "RegisteredLogger_sql.h"
#include "TimerControlBlock.h"

MyData::MyData() :
	m_logger_console(0),
	m_logger_socket(0),
	m_logger_sql(0)
{
	m_dbg = 0;

	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

	m_sched_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_sched_mutex, 0);

	m_alarm_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_alarm_mutex, 0);

	m_loggen_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_loggen_mutex, 0);

	m_logger_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_logger_mutex, 0);

	m_next_trigger_date = 0;
	m_scheduler_loop_cycles = 0;
}

void MyData::lock() const
{
	pthread_mutex_lock(m_mutex);
}

void MyData::unlock() const
{
	pthread_mutex_unlock(m_mutex);
}

void MyData::lock_alarm() const
{
	pthread_mutex_lock(m_alarm_mutex);
}

void MyData::unlock_alarm() const
{
	pthread_mutex_unlock(m_alarm_mutex);
}

void MyData::lock_sched() const
{
	pthread_mutex_lock(m_sched_mutex);
}

void MyData::unlock_sched() const
{
	pthread_mutex_unlock(m_sched_mutex);
}

void MyData::lock_logger() const
{
	pthread_mutex_lock(m_logger_mutex);
}

void MyData::unlock_logger() const
{
	pthread_mutex_unlock(m_logger_mutex);
}

void MyData::lock_loggen() const
{
	pthread_mutex_lock(m_loggen_mutex);
}

void MyData::unlock_loggen() const
{
	pthread_mutex_unlock(m_loggen_mutex);
}

ats::String MyData::register_logger(const ats::String& p_key, const ats::String& p_des, RegisteredLogger& p_rl)
{

	if(p_key.empty())
	{
		return "Logger key is an empty string";
	}

	lock_logger();

	RegisteredLoggerMap::iterator i = (m_logger.insert(RegisteredLoggerPair(p_des, RegisteredLoggerListMap()))).first;

	{
		RegisteredLoggerListMap& list = i->second;
		RegisteredLoggerListMap::iterator i = list.insert(RegisteredLoggerListPair(p_key, &p_rl)).first;

		if((&p_rl) != (i->second))
		{
			h_deregister_logger(*(i->second));
			(i->second) = &p_rl;
		}

		p_rl.m_key = &(i->first);
	}

	p_rl.m_des = &(i->first);

	unlock_logger();
	return ats::String();
}

void MyData::h_deregister_logger(RegisteredLogger& p_rl)
{
	p_rl.m_des = &ats::g_empty;
	p_rl.m_key = &ats::g_empty;
}

void MyData::deregister_logger(const ats::String& p_des)
{
	lock_logger();
	RegisteredLoggerMap::iterator i = m_logger.find(p_des);

	if(i != m_logger.end())
	{
		{
			RegisteredLoggerListMap& rlm = i->second;
			RegisteredLoggerListMap::const_iterator i = rlm.begin();

			while(i != rlm.end())
			{
				h_deregister_logger(*(i->second));
				++i;
			}

		}

		m_logger.erase(i);
	}

	unlock_logger();
}

void MyData::deregister_logger(const ats::String& p_des, const ats::String& p_key)
{
	lock_logger();
	RegisteredLoggerMap::iterator i = m_logger.find(p_des);

	if(i != m_logger.end())
	{
		RegisteredLoggerListMap& rlm = i->second;
		RegisteredLoggerListMap::iterator j = rlm.find(p_key);

		if(j != rlm.end())
		{
			h_deregister_logger(*(j->second));
			rlm.erase(j);

			if(rlm.empty())
			{
				m_logger.erase(i);
			}

		}

	}

	unlock_logger();
}

bool MyData::forward_log_message(const ats::String& p_des, const ats::String& p_log)
{
	bool result = false;
	lock_logger();
	RegisteredLoggerMap::const_iterator i = m_logger.find(p_des);

	if(i != m_logger.end())
	{
		const RegisteredLoggerListMap& list = i->second;
		RegisteredLoggerListMap::const_iterator i = list.begin();

		while(i != list.end())
		{
			RegisteredLogger& rl = *(i->second);
			++i;

			if(rl.send_log(p_log))
			{
				result = true;
			}

		}

	}

	unlock_logger();
	return result;
}

ats::String MyData::register_log_generator(const ats::StringMap& p_arg, RegisteredLogGenerator& p_rl)
{
	const ats::String& key = p_arg.get("key");

	if(key.empty())
	{
		return "Log generator key is an empty string";
	}

	lock_loggen();

	std::pair <RegisteredLogGeneratorMap::iterator, bool> r = m_loggen.insert(RegisteredLogGeneratorPair(key, &p_rl));

	if(!r.second)
	{
		(r.first)->second = &p_rl;
		unlock_loggen();
	}

	unlock_loggen();
	return ats::String();
}

ats::String MyData::register_log_event(
	const ats::String& p_log_event_type,
	const ats::String& p_key,
	const ats::String& p_des)
{
	lock_loggen();
	RegisteredLogGeneratorMap::const_iterator i = m_loggen.find(p_key);

	if(m_loggen.end() == i)
	{
		unlock_loggen();
		return "Log \"" + p_key + "\" is not registered";
	}

	RegisteredLogGenerator& rl = *(i->second);

	const int fd = rl.m_srm.m_sockfd;
	// ATS FIXME: Remove loggen if communication fails.
	// ATS FIXME: Do not block on send (don't hold lock_loggen while performing a blocking call).
	send_cmd(fd, MSG_NOSIGNAL, ":%s %s %s\r", p_log_event_type.c_str(), p_key.c_str(), p_des.empty() ? "-" : p_des.c_str());
	unlock_loggen();
	return ats::String();
}

ats::String MyData::request_log(
	const ats::String& p_key,
	const ats::String& p_des,
	int p_sec,
	int p_msec)
{
	lock_loggen();
	RegisteredLogGeneratorMap::const_iterator i = m_loggen.find(p_key);

	if(m_loggen.end() == i)
	{
		unlock_loggen();
		return "Log \"" + p_key + "\" is not registered";
	}

	RegisteredLogGenerator& rl = *(i->second);

	const int fd = rl.m_srm.m_sockfd;
	// ATS FIXME: Remove loggen if communication fails.
	// ATS FIXME: Do not block on send (don't hold lock_loggen while performing a blocking call).
	send_cmd(fd, MSG_NOSIGNAL, "%s %s %d.%03d\r", p_key.c_str(), p_des.empty() ? "-" : p_des.c_str(), p_sec, p_msec);
	unlock_loggen();
	return ats::String();
}

bool MyData::parse_log(const ats::String& p_log, ats::StringList& p_list)
{
	ats::split(p_list, p_log.substr(), ",", 2);
	return true;
}

ats::String MyData::alarm(const ats::StringMap& p_arg, TimerControlBlock* p_tcb)
{
	const ats::String& time = p_arg.get("time");
	const ats::String& key = p_arg.get("key");

	if(!p_tcb)
	{
		return "p_tcb is null for key \"" + key + "\" for time \"" + time + "\"";
	}

	ats::String emsg;
	int period_msec = TimerEngine::time_to_period_msec(time, &emsg);

	if(period_msec < 0)
	{
		delete p_tcb;
		return emsg;
	}

	lock_alarm();
	AlarmMap::iterator i = m_alarm.find(period_msec);

	if(m_alarm.end() == i)
	{
		TimerEngine* p = new TimerEngine();
		ats::String emsg;
		period_msec = p->set_time(time, &emsg);

		if(period_msec < 0)
		{
			unlock_alarm();
			delete p;
			delete p_tcb;
			return emsg;
		}

		i = m_alarm.insert(AlarmPair(period_msec, p)).first;
	}

	{
		TimerEngine& te = *(i->second);
		ats::StringMap sm;
		sm.set("key", key);
		const ats::String& emsg = te.add(sm, p_tcb);

		if(emsg.empty())
		{
			m_rev_alarm.insert(StrIntPair(key, period_msec));
		}

		unlock_alarm();

		if(!emsg.empty())
		{
			return emsg;
		}

	}

	return ats::String();
}

ats::String MyData::remove_alarm(const ats::String& p_key)
{
	lock_alarm();
	StrIntMap::iterator j = m_rev_alarm.find(p_key);

	if(j == m_rev_alarm.end())
	{
		unlock_alarm();
		return ats::String();
	}

	AlarmMap::iterator i = m_alarm.find(j->second);

	if(m_alarm.end() == i)
	{
		unlock_alarm();
		return ats::String();
	}

	delete (i->second);
	m_rev_alarm.erase(j);
	m_alarm.erase(i);
	unlock_alarm();
	return ats::String();
}

static ats::String to_human_readable_date(time_t p_unix_date)
{
	std::stringstream o;
	ats::system("date -D '%s' " + ats::toStr(p_unix_date) + " 2>&1|grep -v date|tr -d '\\n'", &o);
	return o.str();
}

void MyData::print_stats(std::ostream& p_out) const
{
	p_out << "Debug=" << m_dbg << "\n";

	// ATS FIXME: There is a race condition with printing out m_config.
	//            m_config can be set in other threads.
	{
		const ats::StringMap& c = m_config;

		p_out << "Config: " << c.size() << "\n";

		ats::StringMap::const_iterator i = c.begin();

		while(i != c.end())
		{
			p_out << "\t" << (i->first) << "=" << (i->second) << "\n";
			++i;
		}

	}

	lock_sched();
	{
		struct timeval tv;
		gettimeofday(&tv, 0);
		p_out << "Current Date: " << tv.tv_sec << " (" << to_human_readable_date(tv.tv_sec) << ")\n";
	}
	p_out << "Next Trigger Date: " << m_next_trigger_date << " (" << to_human_readable_date(m_next_trigger_date) << ")\n";
	p_out << "Scheduler Loop Cycles: " << m_scheduler_loop_cycles << "\n";

	p_out << "Tasks: " << m_task.size() << "\n";

	{
		Task::const_iterator i = m_task.begin();

		while(i != m_task.end())
		{
			const ats::String& task_key = i->first;
			const ScheduleTask& st = *(i->second);
			++i;

			p_out	<< "\t" << task_key
				<< ": trigger_date=" << st.m_trigger_date << "(" << to_human_readable_date(st.m_trigger_date) << ")";

			if(st.m_repeat_period)
			{
				p_out << ", repeat_period=" << st.m_repeat_period;
			}

			if(!(st.m_trigger_command.empty()))
			{
				p_out << ", trigger_command=\"" << st.m_trigger_command << "\"";
			}

			p_out << "\n";
		}

	}

	p_out << "\n";
	p_out << "Schedule List: " << m_schedule.size() << "\n";

	{
		Schedule::const_iterator i = m_schedule.begin();

		while(i != m_schedule.end())
		{
			const time_t trigger_date = i->first;
			const ScheduleList& list = i->second;
			++i;

			p_out << "\t[" << trigger_date << " (" << to_human_readable_date(trigger_date) << ")]: " << list.size() << "\n";

			ScheduleList::const_iterator i = list.begin();

			while(i != list.end())
			{
				const ats::String& task_key = i->first;
				++i;

				p_out << "\t\t" << task_key << "\n";
			}

		}

	}

	unlock_sched();
}

void MyData::set_wdt_for_next_trigger_date(time_t p_current_time, time_t p_date)
{

	if(p_date > p_current_time)
	{
		const time_t diff = p_date - p_current_time;
		set_wdt(*this, diff);

		set_rtc_control(*this, "wd/alm", 0);
		set_rtc_control(*this, "wace", 1);
		set_rtc_control(*this, "aie", 1);

		ats_logf(ATSLOG_DEBUG, "%s: Next trigger is at %lu (%lu second%s from now)", __FUNCTION__, p_date, diff, (diff != 1) ? "s" : "");
	}

}

void MyData::sched_task(const ats::StringMap& p_args)
{
	lock_sched();
	h_sched_task(p_args);
	unlock_sched();

	// ATS FIXME: Race-condition between scheduling and saving.
	//	Perform the save within lock_sched/unlock_sched.
	save_sched_tasks();
}

void MyData::h_sched_task(const ats::StringMap& p_args)
{
	ScheduleTask* t = new ScheduleTask();

	std::pair <Task::iterator, bool> r = m_task.insert(Task::TaskMapPair(p_args.get("key"), t));

	if(!r.second)
	{
		delete t;
		t = (r.first)->second;
	}

	const long long trigger_date = p_args.get_long_long("trigger_date");

	struct timeval tv;
	gettimeofday(&tv, 0);

	if(p_args.get_int("from_now"))
	{
		t->m_trigger_date = tv.tv_sec + time_t(trigger_date);
	}
	else
	{
		t->m_trigger_date = time_t(trigger_date);
	}

	if(t->m_trigger_date > tv.tv_sec)
	{
		enable_watchdog_alarm_counter(true);
	}

	t->m_repeat_period = time_t(p_args.get_long_long("repeat_period"));
	t->m_trigger_command = p_args.get("trigger_command");

	const ats::String& task_key = (r.first)->first;
	{
		std::pair <Schedule::iterator, bool> r = m_schedule.insert(SchedulePair(t->m_trigger_date, ScheduleList()));

		ScheduleList& list = (r.first)->second;

		list.insert(std::pair<const ats::String, void*>(task_key, 0));
	}

	ats_logf(ATSLOG_DEBUG, "%s: Task scheduled \"%s\", trigger_date=%lu, repeat_period=%lu, trigger_command=\"%s\"",
		__FUNCTION__,
		((r.first)->first).c_str(),
		t->m_trigger_date,
		t->m_repeat_period,
		t->m_trigger_command.c_str());
}

void MyData::unsched_task(const ats::StringMap& p_args)
{
	const ats::String& key = p_args.get("key");

	db_monitor::ConfigDB db;
	const ats::String& err = db.open_db_config();

	if(!err.empty())
	{
		ats_logf(ATSLOG_ERROR, "%s:ERR (will not unschedule task \"%s\"): %s", __FUNCTION__, key.c_str(), err.c_str());
		return;
	}

	lock_sched();
	Task::iterator i = m_task.find(key);

	if(i != m_task.end())
	{
		h_remove_task_name_from_schedule(p_args);

		if(!h_num_schedules_for_task(key))
		{
			const ats::String& err = db.Unset(m_config.get("app_name"), "task_" + key);

			if(!err.empty())
			{
				ats_logf(ATSLOG_ERROR, "%s:ERR (will not unschedule task \"%s\"): %s", __FUNCTION__, key.c_str(), err.c_str());
				unlock_sched();
				return;
			}

			ats_logf(ATSLOG_DEBUG, "%s: Unscheduled \"%s\"", __FUNCTION__, key.c_str());

			delete i->second;
			m_task.erase(i);
		}

	}
		enable_watchdog_alarm_counter(false);

	unlock_sched();
}

void MyData::unsched_all_tasks(const ats::StringMap& p_args)
{

	for(;;)
	{
		ats::StringMap sm;
		lock_sched();

		if(m_task.empty())
		{
			db_monitor::ConfigDB db;
			const ats::String& err = db.open_db_config();

			if(!err.empty())
			{
				unlock_sched();
				ats_logf(ATSLOG_ERROR, "%s:ERR (will not unschedule tasks): %s", __FUNCTION__, err.c_str());
				return;
			}

			{
				const ats::String& err = db.Unset(m_config.get("app_name"));

				if(!err.empty())
				{
					unlock_sched();
					ats_logf(ATSLOG_ERROR, "%s:ERR (will not unschedule tasks): %s", __FUNCTION__, err.c_str());
					return;
				}

			}

			unlock_sched();
			return;
		}

		sm.set("key", (m_task.begin())->first);
		unlock_sched();

		unsched_task(sm);
	}

}

void MyData::h_remove_task_name_from_schedule(const ats::StringMap& p_args)
{
	const ats::String& key = p_args.get("key");

	const bool has_trigger_date = p_args.has_key("trigger_date");
	const long long trigger_date = has_trigger_date ? p_args.get_long_long("trigger_date") : 0;

	Schedule::iterator i = m_schedule.begin();

	while(i != m_schedule.end())
	{
		Schedule::iterator list_i = i;
		++i;

		if((!has_trigger_date) || (trigger_date == list_i->first))
		{
			ScheduleList& list = list_i->second;
			ScheduleList::iterator j = list.find(key);

			if(j != list.end())
			{
				list.erase(j);

				if(list.empty())
				{
					m_schedule.erase(list_i);
				}

			}

		}

	}

	if(m_schedule.empty())
	{
		enable_watchdog_alarm_counter(false);
	}

}

size_t MyData::h_num_schedules_for_task(const ats::String& p_key) const
{
	size_t n = 0;
	Schedule::const_iterator i = m_schedule.begin();

	while(i != m_schedule.end())
	{
		const ScheduleList& list = i->second;
		++i;

		ScheduleList::const_iterator j = list.find(p_key);

		if(j != list.end())
		{
			++n;
		}

	}

	return n;
}

void MyData::save_sched_tasks()
{
	db_monitor::ConfigDB db;
	const ats::String& err = db.open_db_config();

	if(!err.empty())
	{
		ats_logf(ATSLOG_ERROR, "%s:ERR (will not save scheduled tasks): %s", __FUNCTION__, err.c_str());
		return;
	}

	lock_sched();

	Task::const_iterator i_task = m_task.begin();

	while(i_task != m_task.end())
	{
		const ats::String& task_key = i_task->first;
		const ScheduleTask& task = *(i_task->second);
		++i_task;

		std::stringstream s;
		s
			<< task.m_trigger_date
			<< "," << task.m_repeat_period
			<< "," << task.m_trigger_command;

		const ats::String& err = db.set_config(m_config.get("app_name"), "task_" + task_key, s.str());

		if(!err.empty())
		{
			ats_logf(ATSLOG_ERROR, "%s:ERR: set_config(%s) returned %s", __FUNCTION__, s.str().c_str(), err.c_str());
		}

	}

	unlock_sched();

	ats::system("sync");

	ats_logf(ATSLOG_DEBUG, "%s: Schedule of tasks saved", __FUNCTION__);
}

void MyData::load_sched_tasks()
{
	db_monitor::ConfigDB db;
	{
		const ats::String& err = db.open_db_config();

		if(!err.empty())
		{
			ats_logf(ATSLOG_ERROR, "%s:ERR (will not load scheduled tasks): %s", __FUNCTION__, err.c_str());
			return;
		}

	}

	{
		const ats::String& err = db.Get("app_name");

		if(!err.empty())
		{
			ats_logf(ATSLOG_ERROR, "%s:ERR (will not load scheduled tasks): %s", __FUNCTION__, err.c_str());
			return;
		}

	}

	db_monitor::ResultTable::const_iterator i = db.Table().begin();

	std::vector<ats::StringList> task;

	while(i != db.Table().end())
	{
		const db_monitor::ResultRow& r = *i;
		++i;

		if(r.size() < 3)
		{
			ats_logf(ATSLOG_ERROR, "%s:ERR: did not get 3 columns from config query (count=%zu)", __FUNCTION__, r.size());
			continue;
		}

		const ats::String& val = r[1];

		task.push_back(ats::StringList());
		ats::StringList& list = *(--(task.end()));
		ats::split(list, val, ",", 2);

		if(list.size() < 3)
		{
			ats_logf(ATSLOG_ERROR, "%s:ERR: Task data is not in 3 parts (parts=%zu)", __FUNCTION__, list.size());
			task.erase(--(task.end()));
		}

		// date, period, command
	}

	lock_sched();
	{
		m_schedule.clear();
		m_task.clear();
		std::vector<ats::StringList>::const_iterator i = task.begin();

		while(i != task.end())
		{
			const ats::StringList& list = *i;
			++i;

			ats::StringMap sm;
			sm.set("trigger_date", list[0]);
			sm.set("repeat_period", list[1]);
			sm.set("trigger_command", list[2]);

			h_sched_task(sm);
		}

	}
	unlock_sched();
}

void MyData::init_db_logger()
{
	lock();

	if(m_logger_sql)
	{
		unlock();
		return;
	}

	RegisteredLogger* rl = m_logger_sql = new RegisteredLogger_sql(*this, "log", "/mnt/update/database/log.db", "t_Log");
	unlock();

	const ats::String& emsg = register_logger(ats::toStr(rl), "db", *rl);

	if(!emsg.empty())
	{
		ats_logf(ATSLOG_ERROR, "%s: Failed to register SQL/DB logger. emsg=%s\n\r", __FUNCTION__, emsg.c_str());
	}

}

ats::String MyData::sql_logger(const ats::StringMap& p)
{
	const ats::String& key = p.get("key");
	const ats::String& db_key = p.get("db_key");
	const ats::String& table_name = p.get("table_name");
	const int row_limit = p.get_int("row_limit");

	if(key.empty())
	{
		return "key is empty";
	}

	if(table_name.empty())
	{
		return "table name empty";
	}

	lock();
	std::pair <RegLogMap::iterator, bool> r = m_reg_log.insert(RegLogPair(key, 0));

	if(!(r.second))
	{
		unlock();
		return ats::String();
	}

	RegisteredLogger* rl = (row_limit > 0) ?
		new RegisteredLogger_sql(*this, db_key, "/mnt/update/database/log.db", table_name, row_limit)
		:
		new RegisteredLogger_sql(*this, db_key, "/mnt/update/database/log.db", table_name);
	(r.first)->second = rl;
	unlock();

	// ATS FIXME: This should be inside "lock" to maintain consistency with "m_reg_log" ("key" for
	//	"register_logger" and "m_reg_log" must be consistent).
	const ats::String& emsg = register_logger(ats::toStr(rl), key, *rl);

	if(!emsg.empty())
	{
		lock();
		delete rl;
		m_reg_log.erase(r.first);
		unlock();
		return "Failed to register SQL/DB logger: " + emsg;
	}

	return ats::String();
}
