// Description: J1939 Periodic State Machine
//

#include <fstream>
#include <vector>

#include <syslog.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "atslogger.h"
#include "AFS_Timer.h"
#include "timer-event.h"
#include "can-j1939-monitor.h"
#include "can-j1939-monitor-state-machine.h"
#include "db-monitor.h"
#include "SocketInterfaceResponse.h"
#include "ats-common.h"

#define STATE_MACHINE_CLASS CanJ1939MonitorSm
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(CanJ1939MonitorSm)

extern ATSLogger g_log;
static const ats::String g_app_name("can-j1939-monitor");
extern bool g_has_message_assembler;
const ats::String Checksum(const ats::String& str);
static bool send_first_message = false;
#define PERIODIC_ITEMSPERGROUP 5

bool compareTimeStamp( const periodicRecord& pl, const periodicRecord& pr )
{
	return pl.m_timestamp < pr.m_timestamp;
}

CanJ1939MonitorSm::CanJ1939MonitorSm(MyData& p_data) : StateMachine( p_data)
{
	load_config();

	MyData& md = dynamic_cast<MyData&>(my_data());
	md.m_command.insert(AdminCommandPair(__FUNCTION__, AdminCommand(ac_CanJ1939MonitorSm, "", *this)));
	SET_NEXT_STATE(0);
}

CanJ1939MonitorSm::~CanJ1939MonitorSm()
{
}

void CanJ1939MonitorSm::load_config()
{
	//MyData& md = dynamic_cast<MyData&>(my_data());
	db_monitor::ConfigDB db;
	m_periodic_msg_generator_seconds = db.GetInt("CanJ1939Monitor", "periodic_seconds", 15*60);//15 mins default value.
	m_sourceaddress = strtol((db.GetValue("CanJ1939Monitor", "sourceaddress")).c_str(), 0, 0);

}

int CanJ1939MonitorSm::ac_CanJ1939MonitorSm(AdminCommandContext& p_acc, const AdminCommand& p_cmd, int p_argc, char* p_argv[])
{
	// ATS FIXME: This function (and similar functions) must be thread safe. Make sure to lock all non-function-local
	//	variables to prevent race conditions.
	//CanJ1939MonitorSm& sm = *((CanJ1939MonitorSm*)p_cmd.m_state_machine);

	if(p_argc < 2)
	{
		return 0;
	}

	const ats::String cmd(p_argv[1]);

	ats_logf(ATSLOG(0), "%s,%d: Invalid %s command \"%s\"", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());

	return 0;
}

void CanJ1939MonitorSm::state_0()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	int periodic_overiridium_seconds = md.get_int("mb_periodic_overiridium_seconds");

	ats::TimerEvent* timer = new ats::TimerEvent((send_first_message == true)?m_periodic_msg_generator_seconds:60);
	timer->set_default_event_name("PERIODICTIMER");

	ats::TimerEvent* timer_overiridium = new ats::TimerEvent(periodic_overiridium_seconds);
	timer_overiridium->set_default_event_name("PERIODICTIMERIRIDIUM");

	EventListener listener(md);
	md.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(timer_overiridium, __PRETTY_FUNCTION__, &listener);

	for(;;)
	{
		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			SET_TO_STOP_STATE;
			break;
		}
		else if(e.m_event == timer)
		{
			sendMessage(false);
			timer = new ats::TimerEvent(m_periodic_msg_generator_seconds);
			timer->set_default_event_name("PERIODICTIMER");
			md.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);
		}
		else if(e.m_event == timer_overiridium )
		{
			sendMessage(true);
			listener.destroy_event(timer);
			timer = new ats::TimerEvent(m_periodic_msg_generator_seconds);
			timer->set_default_event_name("PERIODICTIMER");

			timer_overiridium = new ats::TimerEvent(periodic_overiridium_seconds);
			timer_overiridium->set_default_event_name("PERIODICTIMERIRIDIUM");

			md.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);
			md.add_event_listener(timer_overiridium, __PRETTY_FUNCTION__, &listener);
		}
	}
}

void CanJ1939MonitorSm::sendMessage(bool overIridium)
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	int periodic_overiridium_seconds = md.get_int("mb_periodic_overiridium_seconds");

	periodicdatalist list;
	spnsignalMap::iterator i = md.m_spnsignalMap.begin();
	for(; i != md.m_spnsignalMap.end(); ++i)
	{
		SignalMonitor &sm = *((*i).second);
		SignalData& set = sm.getset();
		if( set.periodic != true )
		{
			continue;
		}

		periodicdata pd;
		sm.getperiodicdata(pd);
		list.push_back(pd);
	}

	int interval;
	if( !send_first_message )
	{
		interval = 1;
	}
	else if( overIridium )
	{
		interval = periodic_overiridium_seconds/60;
	}
	else
		interval = m_periodic_msg_generator_seconds/60;


	periodicdatalist::iterator j = list.begin();
	ats::String buf = "$PATSPM,0," + ats::toStr(interval);
	ats::String s_buf_1;
	ats::String s_buf_3;
	int count = 0;
	int splitcount = (list.size())/2;
	while(j != list.end())
	{
		ats::String s_buf;

		periodicRecord r;
		r.m_min = (*j).minvalue;
		r.m_max = (*j).maxvalue;
		int timeStamp = r.m_timestamp = time(NULL);

		std::list<periodicRecord>& plist = m_periodicRecordMap[(*j).spn];
		plist.push_back(r);
		plist.sort(compareTimeStamp);

		{
			std::list<periodicRecord>::iterator it = plist.begin();
			while( it != plist.end())
			{
				if(timeStamp - (*it).m_timestamp > (2 * periodic_overiridium_seconds)) //over two times of periodic_overiridium_seconds.
				{
					plist.erase(it++);
					continue;
				}
				++it;
			}
		}

		ats::String s_buf_2;
		if(overIridium)
		{
			std::list<periodicRecord>::iterator it = plist.begin();
			while( it != plist.end())
			{
				if(timeStamp - (*it).m_timestamp > periodic_overiridium_seconds)
				{
					++it;
					continue;
				}

				float min = (*it).m_min;
				float max = (*it).m_max;
				if( (*j).minvalue > min ) (*j).minvalue = min;
				if( (*j).maxvalue < max ) (*j).maxvalue = max;

				++it;
			}
		}

		ats_sprintf(&s_buf_2, ",%d,%d,%.1f,%.1f,%.1f", (*j).pgn, (*j).spn, (*j).minvalue, (*j).maxvalue, (*j).currentvalue);
		++count;
		if( count <= splitcount )
		{
			s_buf_1.append(s_buf_2);
		}
		else
		{
			s_buf_3.append(s_buf_2);
		}

		++j;
	}

	//send splited periodic messages.
	int downcount = 2;
	while(downcount--)
	{
		ats::String buff;
		if( downcount == 1)
		{
			if( s_buf_1.empty())
				continue;
			buff = buf + "," + ats::toStr(splitcount) + "," + ats::toStr(PERIODIC_ITEMSPERGROUP) + s_buf_1;
		}
		else if( downcount == 0 )
		{
			if( s_buf_3.empty())
				continue;
			buff = buf + "," + ats::toStr(count - splitcount) + "," + ats::toStr(PERIODIC_ITEMSPERGROUP) + s_buf_3;
		}

		const ats::String& sbuf = buff + Checksum(buff);

		if( overIridium || ( !send_first_message ))
		{
			const int pri = md.get_int("mb_overiridium_priority_periodic");
			ats_logf(ATSLOG(0), "%s,%d: Periodic message %s with Priority %d", __FILE__, __LINE__, sbuf.c_str(), pri);
			IF_PRESENT(message_assembler, send_app_msg(g_app_name.c_str(), "message-assembler", 0,
						"msg calamp_user_msg msg_priority=%d usr_msg_data=\"%s\"\r", pri, ats::to_hex(sbuf).c_str()))
		}
		else
		{
			ats_logf(ATSLOG(0), "%s,%d: Periodic message %s", __FILE__, __LINE__, sbuf.c_str());
			IF_PRESENT(message_assembler, send_app_msg(g_app_name.c_str(), "message-assembler", 0,
						"msg calamp_user_msg usr_msg_data=\"%s\"\r", ats::to_hex(sbuf).c_str()))
		}
	}

	if(!send_first_message) send_first_message = true;
}

