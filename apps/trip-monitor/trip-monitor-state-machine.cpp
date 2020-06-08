// Description: Trip Monitor State Machine
//
// State Machine Version:
//    SVN 1955 - http://svn.atsplace.int/software-group/AbsoluteTrac/RedStone/Documents/Design/CAN-Telematics-Trip-State-Machine.odg
//
// XXX: This code MUST match with the above state machine diagram. Do NOT modify this code without also
//      modifying the above state machine diagram. If this code and state machine diagram differ, then the
//      diagram shall be taken as correct.

#include <fstream>
#include <vector>

#include <stdlib.h>
#include <sys/stat.h>

#include "ats-string.h"
#include "atslogger.h"
#include "AFS_Timer.h"
#include "timer-event.h"
#include "trip-monitor.h"
#include "trip-monitor-state-machine.h"
#include "buzzer-monitor.h"
#include "ConfigDB.h"
#include "SocketInterfaceResponse.h"
#include <sstream>
#define TRIP_TESTDATA_LOG_FILE "/var/log/testdata/trip-monitor/trip-monitor-testdata.log"

#define STATE_MACHINE_CLASS TripMonitorSm
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(TripMonitorSm)

extern ATSLogger g_log;

static int g_max_valid_speed;

TripMonitorSm::TripMonitorSm(MyData& p_data) : StateMachine( p_data)
{
	set_log_level(11);  // turns off state machine logging
	m_debug_timer = new AFS_Timer();
	m_distance_timer = new AFS_Timer();
	m_distance_meters = 0.0;
	m_distance = 0;
	m_prev_display_distance = -1;
	m_distance_init = -1;

	load_config();

	MyData& md = dynamic_cast<MyData&>(my_data());
	md.m_command.insert(AdminCommandPair(__FUNCTION__, AdminCommand(ac_TripMonitorSm, "", *this)));
	SET_NEXT_STATE(0);
}

TripMonitorSm::~TripMonitorSm()
{
	delete m_debug_timer;
	delete m_distance_timer;
}

int TripMonitorSm::get_speed_limit()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	return md.get_speed_limit(m_max_speed, m_sbr_offset, m_sbr_scale);
}

static void get_speed_scale(int& p_value, float& p_scale, const ats::String& p_str)
{

	if((!p_str.empty()) && ('%' == *(p_str.c_str() + p_str.length() - 1)))
	{
		p_value = 0;
		p_scale = strtof(p_str.c_str(), 0) * 0.01f;
	}
	else
	{
		p_value = strtol(p_str.c_str(), 0, 0);
		p_scale = 1.0f;
	}

}

void TripMonitorSm::load_config()
{
	db_monitor::ConfigDB db;
	const ats::String app_name("trip-monitor");
	m_state_4_low_speed_kph = 4;//db.GetInt(app_name, "m_state_4_low_speed_kph", 4);
	m_buzzer_delay_seconds = 1;//db.GetInt(app_name, "m_state_6_sound_buzzer_delay_seconds", 1);
	m_message_delay_seconds = 1;//db.GetInt(app_name, "m_state_14_sound_buzzer_delay_seconds", 1);
	m_max_speed = 120;//db.GetInt(app_name, "max_speed", 120);
	m_StartupDistance = 10;//db.GetInt(app_name, "StartupDistance", 10);
	g_max_valid_speed = 239;//db.GetInt(app_name, "max_valid_speed", 239);

	// [speed offset, timeout, poll period, stable readings]
	{
		ats::StringList sbr;
//		ats::split(sbr, db.GetValue(app_name, "SpeedByRoad", ""), ",");
		ats::split(sbr, "", ",");
		get_speed_scale(m_sbr_offset, m_sbr_scale, (sbr.size() >= 1) ? sbr[0] : "");
		m_sbr_timeout = (sbr.size() >= 2) ? strtol(sbr[1].c_str(), 0, 0) : 5;
		const int period = (sbr.size() >= 3) ? strtol(sbr[2].c_str(), 0, 0) : 3;
		const int stable_readings = (sbr.size() >= 4) ? strtol(sbr[3].c_str(), 0, 0) : 5;
		MyData& md = dynamic_cast<MyData&>(my_data());
		md.set_skybase_poll_period(period);
		md.set_skybase_stable_readings(stable_readings);
	}

}

void TripMonitorSm::send_msg(const ats::String& p_msg)const
{
	send_redstone_ud_msg("message-assembler", 0, "%s", p_msg.c_str());
}

int TripMonitorSm::ac_TripMonitorSm(AdminCommandContext& p_acc, const AdminCommand& p_cmd, int p_argc, char* p_argv[])
{
	// ATS FIXME: This function (and similar functions) must be thread safe. Make sure to lock all non-function-local
	//	variables to prevent race conditions.
	TripMonitorSm& sm = *((TripMonitorSm*)p_cmd.m_state_machine);

	if(p_argc < 2)
	{
		return 0;
	}

	const ats::String cmd(p_argv[1]);

	if("rpm" == cmd)
	{
		ats::StringMap* s = new ats::StringMap();

		if(p_argc >= 3)
		{
			s->set("rpm", p_argv[2]);
		}

		sm.my_data().post_event("RPMEvent", s);
	}
	else if("speed" == cmd)
	{
		ats::StringMap* s = new ats::StringMap();

		if(p_argc >= 3)
		{
			const char* speed = p_argv[2];
			const int i_speed = strtol(speed, 0, 0);

			if((i_speed >= 0) && (i_speed <= g_max_valid_speed))
			{
				s->set("speed", speed);
				sm.my_data().post_event("SpeedEvent", s);
			}

		}

	}
	else if("ignition" == cmd)
	{
		ats::StringMap* s = new ats::StringMap();
		
		if(p_argc >= 3)
		{
			ats::String status = p_argv[2];
			s->set("ignition", status);
			sm.my_data().post_event("IgnitionEvent", s);
		}
	}
	else if("stat" == cmd)
	{
		MyData& md = dynamic_cast<MyData&>(sm.my_data());
		send_cmd(p_acc.get_sockfd(), MSG_NOSIGNAL,
			"%s: ok"
			"\n\tm_state_4_low_speed_kph=%d"
			"\n\tm_state_6_sound_buzzer_delay_seconds=%d"
			"\n\tm_state_14_sound_buzzer_delay_seconds=%d"
			"\n\tm_max_speed=%d"
			"\n\tm_StartupDistance=%d"
			"\n\tg_max_valid_speed=%d"
			"\n\tm_sbr_offset=%d"
			"\n\tm_sbr_scale=%f"
			"\n\tm_sbr_timeout=%d"
			"\n\tmd.m_skybase_poll_period=%d"
			"\n\tmd.m_skybase_stable_readings=%d"
			"\r"
			, cmd.c_str()
			, sm.m_state_4_low_speed_kph
			, sm.m_buzzer_delay_seconds
			, sm.m_message_delay_seconds
			, sm.m_max_speed
			, sm.m_StartupDistance
			, g_max_valid_speed
			, sm.m_sbr_offset
			, sm.m_sbr_scale
			, sm.m_sbr_timeout
			, md.get_skybase_poll_period()
			, md.get_skybase_stable_readings()
			);
	}
	else
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: Invalid %s command \"%s\"", __FILE__, __LINE__, __FUNCTION__, cmd.c_str());
	}

	return 0;
}

void TripMonitorSm::state_0()
{
	SET_NEXT_STATE(1);
}

void TripMonitorSm::state_1()
{

	if(m_RedStoneData.IgnitionOn())
	{
		SET_NEXT_STATE(17);
		return;
	}

	MyData& md = dynamic_cast<MyData&>(my_data());
	IgnitionEvent* ignitione = new IgnitionEvent();
	EventListener listener(md);
	md.add_event_listener(ignitione, __PRETTY_FUNCTION__, &listener);
	for(;;)
	{
		AppEventHandler e(listener.wait_event());
		
		if(e.m_event->m_reason.is_cancelled())
		{
			SET_TO_STOP_STATE;
			break;
		}
		else if(e.m_event == ignitione)
		{
			if(e.m_event->data().get_bool("ignition"))
			{
				SET_NEXT_STATE(17);
				break;
			}
			ignitione = new IgnitionEvent();
			md.add_event_listener(ignitione, __PRETTY_FUNCTION__, &listener);
		}
	}
}

void TripMonitorSm::state_2()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	const int thirty_seconds = 30;
	ats::TimerEvent* timer_NoNewSpeedInput = new ats::TimerEvent(thirty_seconds);
	SpeedEvent* speed = new SpeedEvent();
	IgnitionEvent* ignitione = new IgnitionEvent();

	EventListener listener(md);
	md.add_event_listener(ignitione, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(timer_NoNewSpeedInput, __PRETTY_FUNCTION__, &listener);

	for(;;)
	{
		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			SET_TO_STOP_STATE;
			break;
		}
		else if( e.m_event ==  ignitione )
		{

			if(!e.m_event->data().get_bool("ignition"))
			{
				SET_NEXT_STATE(5);
				break;
			}

			ignitione = new IgnitionEvent();
			listener.destroy_event(timer_NoNewSpeedInput);
			timer_NoNewSpeedInput = new ats::TimerEvent(thirty_seconds);
			md.add_event_listener(ignitione, __PRETTY_FUNCTION__, &listener);
			md.add_event_listener(timer_NoNewSpeedInput, __PRETTY_FUNCTION__, &listener);
		}
		else if( e.m_event == speed )
		{
			md.set("speed", e.m_event->data().get("speed"));
			SET_NEXT_STATE(16);
			break;

		}
		else if(e.m_event == timer_NoNewSpeedInput)
		{
			SET_NEXT_STATE(5);
			break;
		}

	}

}

void TripMonitorSm::state_3()
{
	ats_logf(ATSLOG_INFO,"START CONDITION");
	send_msg("msg start_condition\r");
	m_RedStoneData.Started(true);
	SET_NEXT_STATE(4);
}
//---------------------------------------------------------------------
//
//
void TripMonitorSm::state_4()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	const int thirty_seconds = 30;
	ats::TimerEvent* timer_NoNewSpeedInput = new ats::TimerEvent(thirty_seconds);
	SpeedEvent* speed = new SpeedEvent();

	EventListener listener(md);
	md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(timer_NoNewSpeedInput, __PRETTY_FUNCTION__, &listener);

	for(;;)
	{
		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			SET_TO_STOP_STATE;
			break;
		}
		else if(e.m_event == timer_NoNewSpeedInput)
		{
			SET_NEXT_STATE(19);  // No speed readings being received
			break;
		}
		else if(e.m_event == speed )
		{
			md.set("speed", e.m_event->data().get("speed"));
			const int speed_kph = md.get_int("speed");

			if(speed_kph < m_state_4_low_speed_kph)
			{
				SET_NEXT_STATE(9);
				break;
			}
			else if(speed_kph > get_speed_limit())
			{
				SET_NEXT_STATE(6); // beginning to speed
				break;
			}

			speed = new SpeedEvent();
			listener.destroy_event(timer_NoNewSpeedInput);
			timer_NoNewSpeedInput = new ats::TimerEvent(thirty_seconds);
			md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
			md.add_event_listener(timer_NoNewSpeedInput, __PRETTY_FUNCTION__, &listener);
		}

	}

}

void TripMonitorSm::state_5()
{
	SET_NEXT_STATE(0);
}

//----------------------------------------------------------
// State 6 - beginning to speed
//
void TripMonitorSm::state_6()
{
	MyData& md = dynamic_cast<MyData&>(my_data());

	ats::TimerEvent* timer_buzzer_delay = new ats::TimerEvent(m_buzzer_delay_seconds);
	SpeedEvent* speed = new SpeedEvent();

	EventListener listener(md);
	md.add_event_listener(timer_buzzer_delay, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);

	for(;;)
	{
		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			SET_TO_STOP_STATE;
			break;
		}
		else if(e.m_event == timer_buzzer_delay)
		{
			//Fixme, sound buzz for 2 mins, if still always stay at state-11,after 2 mins, buzz will be quiet.
//			buzz_on("trip-monitor", 12, 50000, 450000, 120000000);
			SET_NEXT_STATE(14);  // turn buzzer on
			break;
		}
		else if(e.m_event == speed)
		{
			md.set("speed", e.m_event->data().get("speed"));
			const int speed_kph = md.get_int("speed");

			if(speed_kph < get_speed_limit())
			{
				SET_NEXT_STATE(4);
				break;
			}

			speed = new SpeedEvent();
			md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
		}
	}
}

void TripMonitorSm::state_7()
{
	SET_NEXT_STATE(17);
}

void TripMonitorSm::state_8()
{
	ats_logf(ATSLOG_INFO,"STOP CONDITION");
	send_msg("msg stop_condition\r");
	m_RedStoneData.Started(false);
	SET_NEXT_STATE(7);
}

void TripMonitorSm::state_9()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	const int two_minutes = 120;
	ats::TimerEvent* timer_2min = new ats::TimerEvent(two_minutes);
	SpeedEvent* speed = new SpeedEvent();
	IgnitionEvent* ignitione = new IgnitionEvent();

	EventListener listener(md);
	md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(timer_2min, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(ignitione, __PRETTY_FUNCTION__, &listener);

	for(;;)
	{
		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			SET_TO_STOP_STATE;
			break;
		}
		else if(e.m_event == ignitione)
		{
			if(!e.m_event->data().get_bool("ignition"))
			{
				m_RedStoneData.Started(false);
				ats_logf(ATSLOG_INFO, "STOP CONDITION");
				send_msg("msg stop_condition\r");
				SET_NEXT_STATE(5);
				break;
			}

			ignitione = new IgnitionEvent();
			md.add_event_listener(ignitione, __PRETTY_FUNCTION__, &listener);
		}
		else if(e.m_event == timer_2min)
		{
			SET_NEXT_STATE(8);
			break;
		}
		else if(e.m_event == speed)
		{
			md.set("speed", e.m_event->data().get("speed"));
			const int speed_kph = md.get_int("speed");

			if(speed_kph > m_state_4_low_speed_kph)
			{
				SET_NEXT_STATE(4);
				break;
			}
			speed = new SpeedEvent();
			md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
		}
	}
}


//--------------------------------------------------------------------------
// state 10 - been speeding too long - send a message back
//
void TripMonitorSm::state_10()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	const int speed_kph = md.get_int("speed");
	ats::String data;
	std::ostringstream stringStream;
	stringStream << "msg speed_exceeded usr_msg_data=" << speed_kph << "," << md.get_skybase_speed_limit() << "\r";
	data = stringStream.str();
	ats_logf(ATSLOG_INFO,"enter state_10 - SPEED EXCEEDED  %d limit: %d", speed_kph, md.get_skybase_speed_limit());
	send_msg(data);
	SET_NEXT_STATE(11);
}

//--------------------------------------------------------------------------
// state 11 - still speeding, buzzer on, message has been sent
//
void TripMonitorSm::state_11()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	const int thirty_seconds = 30;
	ats::TimerEvent* timer_NoNewSpeedInput = new ats::TimerEvent(thirty_seconds);
	SpeedEvent* speed = new SpeedEvent();

	EventListener listener(md);
	md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(timer_NoNewSpeedInput, __PRETTY_FUNCTION__, &listener);

	for(;;)
	{
		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			SET_TO_STOP_STATE;
			break;
		}
		else if(e.m_event == timer_NoNewSpeedInput)
		{
			SET_NEXT_STATE(19);  // No speed readings being received
			break;
		}
		else if(e.m_event == speed)
		{
			md.set("speed", e.m_event->data().get("speed"));
			const int speed_kph = md.get_int("speed");

			if(speed_kph < get_speed_limit())
			{
				SET_NEXT_STATE(15);  // need to go below speed for 1 second worth of speed readings
				break;
			}

			listener.destroy_event(timer_NoNewSpeedInput);
			timer_NoNewSpeedInput = new ats::TimerEvent(thirty_seconds);
			speed = new SpeedEvent();
			md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
			md.add_event_listener(timer_NoNewSpeedInput, __PRETTY_FUNCTION__, &listener);
		}

	}

}

//--------------------------------------------------------------------------
// Speed has returned to acceptable - send the message
void TripMonitorSm::state_12()
{
	ats_logf(ATSLOG_INFO,"SPEED ACCEPTABLE");
	send_msg("msg speed_acceptable\r");
	SET_NEXT_STATE(4);
}

//--------------------------------------------------------------------------
// state 14 - buzzer on - delay to send message
void TripMonitorSm::state_14()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	ats::TimerEvent* timer = new ats::TimerEvent(m_message_delay_seconds);
	SpeedEvent* speed = new SpeedEvent();

	EventListener listener(md);
	md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);

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
			SET_NEXT_STATE(10);  // time to send a message back
			break;
		}
		else if(e.m_event == speed)
		{
			md.set("speed", e.m_event->data().get("speed"));
			const int speed_kph = md.get_int("speed");

			if(speed_kph < get_speed_limit())
			{
//				buzz_off("trip-monitor", 12);
				SET_NEXT_STATE(4);
				break;
			}
			speed = new SpeedEvent();
			md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
		}

	}

}

//--------------------------------------------------------------------------
// need to go below speed for 1 second worth of speed readings
//
void TripMonitorSm::state_15()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	const int one_second = 1;
	ats::TimerEvent* timer = new ats::TimerEvent(one_second);
	SpeedEvent* speed = new SpeedEvent();

	EventListener listener(md);
	md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);

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
//			buzz_off("trip-monitor", 12);
			SET_NEXT_STATE(12);  // speed is OK - send message
			break;
		}
		else if(e.m_event == speed)
		{
			md.set("speed", e.m_event->data().get("speed"));
			const int speed_kph = md.get_int("speed");

			if(speed_kph > get_speed_limit())
			{
				SET_NEXT_STATE(11);  // still speeding - leave buzzer on
				break;
			}
			speed = new SpeedEvent();
			md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
		}
	}
}

void TripMonitorSm::state_16()
{
	MyData& md = dynamic_cast<MyData&>(my_data());

	// ATS FIXME: Distance only updates when a new speed comes in.
	//	    Distance calculations will not be accurate.

	if(m_distance_init != -1)
	{
		int dis;

		if(getDistance(dis))
			m_distance = dis - m_distance_init;
		else
			m_distance_init = -1;
	}
	else
	{
		const int speed_kph = md.get_int("speed");
		const int meters_per_kilometer = 1000;
		const int speed_meters_per_hour = speed_kph * meters_per_kilometer;


		const double diff_time_seconds = double(m_distance_timer->ElapsedTime()) / ((1000.0) * (1000.0) * (1000.0));
		m_distance_timer->SetTime();

		const double seconds = 60.0;
		m_distance_meters += (double(speed_meters_per_hour) / seconds) * diff_time_seconds;
	}

	if(m_distance_meters > m_StartupDistance || m_distance > (int)m_StartupDistance )
	{
		my_data().queue_undeliverable_events("SpeedEvent", false);
		SET_NEXT_STATE(3);
	}
	else
	{
		SET_NEXT_STATE(23);
	}

}

void TripMonitorSm::state_17()
{
	my_data().queue_undeliverable_events("SpeedEvent", true);
	m_distance_meters = 0.0;
	m_distance_timer->SetTime();

	int dis;

	if(getDistance(dis))
	{
		m_distance_init = dis;
	}
	else
		m_distance_init = -1;

	SET_NEXT_STATE(23);
}

void TripMonitorSm::state_19()
{
	ats_logf(ATSLOG_INFO, "%s,%d: Message time-out", __FILE__, __LINE__);
	SET_NEXT_STATE(20);
}

void TripMonitorSm::state_20()
{
	sleep(5);
	SET_NEXT_STATE(21);
}

void TripMonitorSm::state_21()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	SET_TO_STOP_STATE;
	md.post_exit_sem();
}

void TripMonitorSm::state_23()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	const int thirty_seconds = 30;
	ats::TimerEvent* timer = new ats::TimerEvent(thirty_seconds);
	SpeedEvent* speed = new SpeedEvent();
	IgnitionEvent* ignitione = new IgnitionEvent();

	EventListener listener(md);
	md.add_event_listener(ignitione, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);

	for(;;)
	{
		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			SET_TO_STOP_STATE;
			break;
		}
		else if( e.m_event ==  ignitione )
		{

			if(!e.m_event->data().get_bool("ignition"))
			{
				SET_NEXT_STATE(5);
				break;
			}

			ignitione = new IgnitionEvent();
			listener.destroy_event(timer);
			timer = new ats::TimerEvent(thirty_seconds);
			md.add_event_listener(ignitione, __PRETTY_FUNCTION__, &listener);
			md.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);
		}
		else if( e.m_event == speed )
		{

			if(e.m_event->data().get_int("speed") > m_state_4_low_speed_kph)
			{
				md.set("speed", e.m_event->data().get("speed"));
				SET_NEXT_STATE(16);
				break;
			}

			speed = new SpeedEvent();
			listener.destroy_event(timer);
			timer = new ats::TimerEvent(thirty_seconds);
			md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
			md.add_event_listener(timer, __PRETTY_FUNCTION__, &listener);
		}
		else if(e.m_event == timer)
		{
			SET_NEXT_STATE(5);
			break;
		}

	}
	
}

bool TripMonitorSm::getDistance(int& distance)
{
	int retry = 0;
	MyData& md = dynamic_cast<MyData&>(my_data());
	ats::SocketInterfaceResponse response(md.cs_trip_stats.m_fd);

	for(;;)
	{
		if( send_cmd(md.cs_trip_stats.m_fd, MSG_NOSIGNAL, "getdistance\n") < 0)
		{
			close_ClientSocket(&md.cs_trip_stats);
			connect_unix_domain_client(&md.cs_trip_stats, "trip-stats");
		}
		else
		{
			const ats::String& cmdline = response.get();

			if(cmdline.empty())
			{

				if(response.error())
				{
					ats_logf(ATSLOG_ERROR, "%s,%d: (%d) Read failed from Server, closing connection", __FILE__, __LINE__, response.error());
				}
				else
				{
					ats_logf(ATSLOG_DEBUG, "%s,%d: Server closed connection", __FILE__, __LINE__);
				}

				return false;
			}

			const int pos = cmdline.find("distance=");
			const ats::String& dis = cmdline.substr(pos + 10);
			distance = (int)(strtol(dis.c_str(), 0, 0));

			if(m_prev_display_distance != distance)
			{
				m_prev_display_distance = distance;
				ats_logf(ATSLOG_INFO, "%s() = %d", __FUNCTION__, distance);
			}

			break;
		}

		if( retry > 5)
		{
			ats_logf(ATSLOG_ERROR, "%s,%d:%s Can't connect to trip-stats ", __FILE__, __LINE__, __FUNCTION__);
			return false;
		}

		retry++;
	}

	return true;
}
