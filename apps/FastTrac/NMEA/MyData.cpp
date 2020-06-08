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
#include "MyData.h"
#include "NMEA.h"

extern NMEA * pdev;
static const ats::String g_app_name("SER_GPS");

int g_dbg = 0;

// set_input
int ac_set_input(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	MyData& md = p_acc.my_data();

	ats::StringMap s;
	s.from_args(p_argc - 1, p_argv + 1);

	const ats::String& err = md.set_input(s);
	pdev->AddNewInput();
	
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


int ac_debug(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	if(p_argc <= 1)
	{
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n", g_dbg);
	}
	else
	{
		g_dbg = strtol(p_argv[1], 0, 0);
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL, "debug=%d\n", g_dbg);
	}
	return 0;
}

COMMON_EVENT_DEFINITION(,InputEvent, AppEvent)

InputEvent::InputEvent()
{
}

InputEvent::~InputEvent()
{
}

void InputEvent::start_monitor()
{
}

void InputEvent::stop_monitor()
{
}

MyData& MyData::getInstance()
{
	static MyData* p = new MyData();
	return *p;
}

MyData::MyData()
{
	m_gga_rmc_count = 0;

	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

	m_work_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_work_mutex, 0);

	m_command.insert(AdminCommandPair("input", AdminCommand(ac_set_input)));
	m_command.insert(AdminCommandPair("debug", AdminCommand(ac_debug)));

	m_nmea = new NMEA();
}

MyData::~MyData()
{
}

ats::String MyData::set_input(const ats::StringMap& p_args)
{
	ats::String ret;
	lock_work();
	
	unlock_work();
	return ret;
}

void MyData::open_gps_log(db_monitor::ConfigDB& p_db)
{
	const ats::String log_key(ATS_LOG_KEY);

	{
		const ats::String& emsg = p_db.open_db(log_key, ATS_LOG_DB);

		if(!emsg.empty())
		{
			ats_logf(ATSLOG_DEBUG, "%s: open_db(%s): %s", __FUNCTION__, ATS_LOG_DB, emsg.c_str());
		}

	}

	{
		const ats::String& emsg = p_db.query(log_key, "create table if not exists t_NMEA_pos (v_id integer primary key, v_gga text, v_rmc text)");

		if(!emsg.empty())
		{
			ats_logf(ATSLOG_DEBUG, "%s: query: %s", __FUNCTION__, emsg.c_str());
		}

	}

	set_gps_log_row_limit(p_db, 10);
}

bool MyData::save_gps_gga_rmc(db_monitor::ConfigDB& p_db)
{

	if((!m_gga_rmc_count) || (m_gga_rmc_count >= 60))
	{
		m_gga_rmc_count = 1;
	}
	else
	{
		++m_gga_rmc_count;
		return true;
	}

	lock();
	const ats::String& query = (m_gga.empty() || m_rmc.empty()) ? "" : "insert into t_NMEA_pos (v_gga, v_rmc) values (X'" + ats::to_hex(m_gga) + "',X'" + ats::to_hex(m_rmc) + "')";
	m_gga.clear();
	m_rmc.clear();
	unlock();

	if(query.empty())
	{
		return false;
	}

	const ats::String& emsg = p_db.query(ATS_LOG_KEY, query);

	if(!emsg.empty())
	{
		ats_logf(ATSLOG_DEBUG, "%s: %s", __FUNCTION__, emsg.c_str());
		return false;
	}

	return true;
}

bool MyData::set_gps_log_row_limit(db_monitor::ConfigDB& p_db, int p_limit)
{
	const ats::String query(
		"drop trigger if exists row_limit;"
		" create trigger row_limit after insert on t_NMEA_pos"
		" begin"
			" delete from t_NMEA_pos where v_id <= (select v_id from t_NMEA_pos order by v_id desc limit " + ats::toStr(p_limit) + ", 1);"
		" end;"
	);

	const ats::String& emsg = p_db.query(ATS_LOG_KEY, query);

	if(!emsg.empty())
	{
		ats_logf(ATSLOG_DEBUG, "%s: %s", __FUNCTION__, emsg.c_str());
		return false;
	}

	return true;
}
