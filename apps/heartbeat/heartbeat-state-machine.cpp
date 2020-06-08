// Description: Heartbeat State Machine
//
// State Machine Version:
//    SVN 1431 - http://svn.atsplace.int/software-group/AbsoluteTrac/RedStone/Documents/Design/Redstone-Heartbeat-State-Machine.odg
//
// XXX: This code MUST match with the above state machine diagram. Do NOT modify this code without also
//      modifying the above state machine diagram. If this code and state machine diagram differ, then the
//      diagram shall be taken as correct.

#include <fstream>
#include <vector>

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "RedStone_IPC.h"
#include "AFS_Timer.h"
#include "timer-event.h"
#include "heartbeat.h"
#include "heartbeat-state-machine.h"
#include "ConfigDB.h"

#define STATE_MACHINE_CLASS HeartbeatSm
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

extern ATSLogger g_log;

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(HeartbeatSm)
//------------------------------------------------------------------------------
HeartbeatSm::HeartbeatSm(MyData& p_data) : StateMachine( p_data)
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	md.m_command.insert(AdminCommandPair(__FUNCTION__, AdminCommand(ac_HeartbeatSm, "", *this)));

	m_debug_timer = new AFS_Timer();
	m_testdatadir_existing = testdatadir_existing();

	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

	m_time_from_db = 0;

	load_config();

	SET_NEXT_STATE(0);
}

//------------------------------------------------------------------------------
HeartbeatSm::~HeartbeatSm()
{
	delete m_debug_timer;
}

//------------------------------------------------------------------------------
bool HeartbeatSm::testdatadir_existing() const
{
	struct stat st;
	const ats::String& s1 = "/var/log/testdata";
	const ats::String& s2 = s1 + "/" + "heartbeat";
	const ats::String& s3 = "mkdir " + s2;

	if(stat(s1.c_str(), &st) != 0 )
	{
		return false;
	}
	else if (stat(s2.c_str(), &st) != 0 )
	{
		std::stringstream s;
		ats::system(s3.c_str(), &s);
		if(!s.str().empty())
		{
			ats_logf(&g_log, "%s: Failed to create folder \"%s\"", __PRETTY_FUNCTION__, s2.c_str());
			return false;
		}
	}

	return true;
}

//------------------------------------------------------------------------------
void HeartbeatSm::load_config()
{
	db_monitor::ConfigDB db;
	const ats::String app_name("heartbeat");
	m_state_1_timeout_seconds = db.GetInt(app_name, "Interval", 86400);  // change at 1.9.1 to force all units to a 1 day heartbeat.  This is not changeable anymore.
	db.Unset(app_name, "m_state_1_timeout_seconds");  // change at 1.9.1 to remove the old entry
}

//------------------------------------------------------------------------------
void HeartbeatSm::send_msg(const ats::String& p_msg)const
{
	send_redstone_ud_msg("message-assembler", 0, "%s", p_msg.c_str());
}

//------------------------------------------------------------------------------
void HeartbeatSm::set_wakeup_sched() const
{
	send_redstone_ud_msg("rtc-monitor", 0, "unsched key=heartbeat\rsched key=heartbeat trigger_date=%u from_now=1\r", m_state_1_timeout_seconds);
}



//------------------------------------------------------------------------------
int HeartbeatSm::ac_HeartbeatSm(AdminCommandContext& p_acc, const AdminCommand& p_cmd, int p_argc, char* p_argv[])
{
	// ATS FIXME: This function (and similar functions) must be thread safe. Make sure to lock all non-function-local
	//	variables to prevent race conditions.
	HeartbeatSm& sm = *((HeartbeatSm*)p_cmd.m_state_machine);

	if(p_argc < 2)
	{
		return 0;
	}

	const ats::String cmd(p_argv[1]);

	if(g_dbg)
	{
		ats_logf(&g_log, "%s: Got command \"%s\"", __PRETTY_FUNCTION__, cmd.c_str());
	}

	if("ignition" == cmd)
	{
		ats::StringMap* s = new ats::StringMap();

		if(p_argc >= 3)
		{
			s->set("ignition", p_argv[2]);
			const ats::String& s1(p_argv[2]);
			const ats::String& s2 = "ignition " + s1;
			ats_logf(&g_log, s2.c_str());
		}
		sm.my_data().post_event("IgnitionEvent", s);
	}
	else
	{
		ats_logf(&g_log, "%s,%d: Invalid %s command \"%s\"", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());
	}

	return 0;
}

//------------------------------------------------------------------------------
void HeartbeatSm::state_0()
{
  ats::String strWakeup;
  if (ats::get_file_line(strWakeup, "/tmp/logdir/wakeup.txt", 1, 0) == 0)
  {
    if (strWakeup == "rtc")
    {
      ats_logf(&g_log, "RTC Wakeup - will send heartbeat message");
      SET_NEXT_STATE(1);  // woke up from RTC - send a heartbeat message
      return;
    }
  }
  
  SET_NEXT_STATE(4); // woke up for some other reason - watch for ignition
}

//------------------------------------------------------------------------------
// wait for valid GPS
void HeartbeatSm::state_1()
{
	set_work();  // keep the system alive til we have gps and send the message
	
  if (!m_NMEA.IsValid())
    sleep(1);
    
  SET_NEXT_STATE(2);
}

//------------------------------------------------------------------------------
// send a heartbeat message
//
void HeartbeatSm::state_2()
{
	ats_logf(&g_log, "HEARTBEAT msg being sent");
	send_msg("msg heartbeat set_work=1\r");
	
	// put the time sent into db-config  (used for LCM display of last heartbeat
  time_t timer;
  char buffer[26];
  struct tm* tm_info;

  time(&timer);
  tm_info = localtime(&timer);

  strftime(buffer, 26, "%Y/%m/%d %H:%M:%S", tm_info);
 	db_monitor::ConfigDB db;
 	db.set_config("heartbeat", "LastHeartbeat", buffer);
 
  
	SET_NEXT_STATE(3);
}
//------------------------------------------------------------------------------
void HeartbeatSm::state_3()
{
	ResetHeartbeat();
	unset_work();
	SET_NEXT_STATE(4);
}

//------------------------------------------------------------------------------
void HeartbeatSm::state_4()
{
	m_time_from_db = get_time_from_db();
	
	while (1)
	{
		if(m_ipc.IgnitionOn())
		{
			SET_NEXT_STATE(5);
			return;
		}

		struct timeval curr_tv;
		gettimeofday(&curr_tv, 0);
		
		if(m_time_from_db < curr_tv.tv_sec)
		{
			SET_NEXT_STATE(1);
			return;
		}
		
		sleep(1);
	}
}

//------------------------------------------------------------------------------
void HeartbeatSm::state_5()
{
	ResetHeartbeat();
	SET_NEXT_STATE(6);
}

//------------------------------------------------------------------------------
void HeartbeatSm::state_6()
{
	m_time_from_db = get_time_from_db();
	
	while (1)
	{
		if(!m_ipc.IgnitionOn())
		{
			SET_NEXT_STATE(7);
			return;
		}

		struct timeval curr_tv;
		gettimeofday(&curr_tv, 0);
		
		if(m_time_from_db < curr_tv.tv_sec)
		{
			SET_NEXT_STATE(2);
			return;
		}
		
		sleep(1);
	}
}

//------------------------------------------------------------------------------
void HeartbeatSm::state_7()
{
	ResetHeartbeat();
	SET_NEXT_STATE(4);
}


void HeartbeatSm::ResetHeartbeat()
{
	set_work();
	struct timeval curr_tv;
	gettimeofday(&curr_tv, 0);
	set_time_in_db(curr_tv.tv_sec + m_state_1_timeout_seconds);
	set_wakeup_sched();
	unset_work();
}
