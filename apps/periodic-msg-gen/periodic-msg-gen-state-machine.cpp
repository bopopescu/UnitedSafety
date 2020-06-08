// Description: Periodic Message Generator State Machine
//
// XXX: This code MUST match with the above state machine diagram. Do NOT modify this code without also
//      modifying the above state machine diagram. If this code and state machine diagram differ, then the
//      diagram shall be taken as correct.

#include <fstream>
#include <vector>

#include <syslog.h>
#include <stdlib.h>

#include "atslogger.h"
#include "AFS_Timer.h"
#include "timer-event.h"
#include "periodic-msg-gen.h"
#include "periodic-msg-gen-state-machine.h"
#include "ConfigDB.h"

#define STATE_MACHINE_CLASS PeriodicmsggenSm
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(PeriodicmsggenSm)

extern ATSLogger g_log;
PeriodicmsggenSm::PeriodicmsggenSm(MyData& p_data) : StateMachine( p_data)
{
	m_debug_timer = new AFS_Timer();
	load_config();

	MyData& md = dynamic_cast<MyData&>(my_data());
	md.m_command.insert(AdminCommandPair(__FUNCTION__, AdminCommand(ac_PeriodicmsggenSm, "", *this)));
	SET_NEXT_STATE(0);
}

PeriodicmsggenSm::~PeriodicmsggenSm()
{
	delete m_debug_timer;
}

void PeriodicmsggenSm::load_config()
{
	db_monitor::ConfigDB db;
	const ats::String app_name("periodic-msg-gen");
	m_schedule_msg_seconds = db.GetInt(app_name, "m_schedule_msg_seconds", 120);
}

void PeriodicmsggenSm::send_msg(const ats::String& p_msg)const
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	send_redstone_ud_msg("message-assembler", 0, "%s", p_msg.c_str());
}

int PeriodicmsggenSm::ac_PeriodicmsggenSm(AdminCommandContext& p_acc, const AdminCommand& p_cmd, int p_argc, char* p_argv[])
{
	// ATS FIXME: This function (and similar functions) must be thread safe. Make sure to lock all non-function-local
	//	variables to prevent race conditions.
	PeriodicmsggenSm& sm = *((PeriodicmsggenSm*)p_cmd.m_state_machine);

	if(p_argc < 2)
	{
		return 0;
	}

	const ats::String cmd(p_argv[1]);

	if(g_dbg)
	{
		ats_logf(&g_log, "%s: Got command \"%s\"", __PRETTY_FUNCTION__, cmd.c_str());
	}

	if("set_work" == cmd)
	{
		sm.my_data().post_event("setworkEvent");
	}
	else if("unset_work" == cmd)
	{
		sm.my_data().post_event("unsetworkEvent");
	}
	else
	{
		ats_logf(&g_log, "%s,%d: Invalid %s command \"%s\"", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());
	}

	return 0;
}

void PeriodicmsggenSm::state_0()
{
	MyData& md = dynamic_cast<MyData&>(my_data());

	ats_logf(&g_log,"enter stat_0");
	for(;;)
	{
		setworkEvent* sw = new setworkEvent();
		EventListener listener(md);
		md.add_event_listener(sw, __PRETTY_FUNCTION__, &listener);
		AppEventHandler e(listener.wait_event());

		if(e.m_event == sw )
		{
			ats_logf(&g_log,"set work");
			SET_NEXT_STATE(1);
			break;

		}
		else if(e.m_event->m_reason.is_cancelled())
		{
			SET_TO_STOP_STATE;
			break;
		}
	}
}

void PeriodicmsggenSm::state_1()
{
	ats_logf(&g_log,"enter stat_1");
	MyData& md = dynamic_cast<MyData&>(my_data());
	ats::TimerEvent* timer = new ats::TimerEvent(m_schedule_msg_seconds);
	unsetworkEvent* unsw = new unsetworkEvent();

	EventListener listener(md);
	md.add_event_listener(unsw, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);

	for(;;)
	{
		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			SET_TO_STOP_STATE;
			break;
		}
		else if( e.m_event == unsw )
		{
			ats_logf(&g_log,"unset work");
			SET_NEXT_STATE(0);
			break;

		}
		else if(e.m_event == timer)
		{
			SET_NEXT_STATE(2);
			break;
		}
	}
}

void PeriodicmsggenSm::state_2()
{
	ats_logf(&g_log,"enter stat_2");
	ats_logf(&g_log,"SCHEDULED MESSAGE");
	send_msg("msg scheduled_message\r");
	SET_NEXT_STATE(1);
}
