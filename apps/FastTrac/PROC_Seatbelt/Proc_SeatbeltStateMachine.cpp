#include <stdlib.h>
#include <sys/stat.h>

#include "ats-string.h"
#include "atslogger.h"
#include "AFS_Timer.h"
#include "timer-event.h"
#include "buzzer-monitor.h"

#include "Proc_SeatbeltStateMachine.h"

#define STATE_MACHINE_CLASS SeatbeltStateMachine
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

static int g_max_valid_speed = 239;

extern REDSTONE_IPC* g_shmem;
extern int g_seatbeltBit;

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(SeatbeltStateMachine)
//-------------------------------------------------------------------------------------------------
SeatbeltStateMachine::SeatbeltStateMachine(MyData& p_data) : StateMachine( p_data)
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	md.m_command.insert(AdminCommandPair("speed", AdminCommand(ac_speed, "", *this)));
	md.m_command.insert(AdminCommandPair("sensor", AdminCommand(ac_sensor, "", *this)));
	md.m_command.insert(AdminCommandPair("seatbelt", AdminCommand(ac_seatbelt, "", *this)));
	md.m_command.insert(AdminCommandPair("ignition_off", AdminCommand(ac_ignition, "", *this)));
	SET_NEXT_STATE(0);
}

//-------------------------------------------------------------------------------------------------
SeatbeltStateMachine::~SeatbeltStateMachine()
{
}

//-------------------------------------------------------------------------------------------------
void SeatbeltStateMachine::state_0()
{
	ats_logf(ATSLOG(0), "enter state 0");
	SET_NEXT_STATE(1);
}

//-------------------------------------------------------------------------------------------------
void SeatbeltStateMachine::state_1()
{
	ats_logf(ATSLOG(0), "enter state 1");

	MyData& md = dynamic_cast<MyData&>(my_data());
	ats::TimerEvent* speed_exceedence_duration = new ats::TimerEvent(md.m_speed_exceedence_duration_seconds);
	SpeedEvent* speed = new SpeedEvent();

	EventListener listener(md);
	md.add_event_listener(speed_exceedence_duration, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);

	for(;;)
	{
		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			SET_TO_STOP_STATE;
			break;
		}
		else if(e.m_event == speed_exceedence_duration)
		{
			SET_NEXT_STATE(2);
			break;
		}
		else if(e.m_event == speed)
		{
			md.set("speed", e.m_event->data().get("speed"));
			const int speed_kph = md.get_int("speed");

			if(speed_kph < md.m_low_speed)
			{
				listener.destroy_event(speed_exceedence_duration);
				speed_exceedence_duration = new ats::TimerEvent(md.m_speed_exceedence_duration_seconds);
				md.add_event_listener(speed_exceedence_duration, __PRETTY_FUNCTION__, &listener);
			}

			speed = new SpeedEvent();
			md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
		}
	}
}

//-------------------------------------------------------------------------------------------------
void SeatbeltStateMachine::state_2()
{
	ats_logf(ATSLOG(0), "enter state 2");

	MyData& md = dynamic_cast<MyData&>(my_data());
	ats::TimerEvent* buzzer_delay_seconds = new ats::TimerEvent(md.m_buzzer_delay_time_limit);
	ats::TimerEvent* speed_exceedence_duration = new ats::TimerEvent(md.m_speed_exceedence_duration_seconds);
	SpeedEvent* speed = new SpeedEvent();
	SeatbeltEvent* seatbelt = new SeatbeltEvent();

	EventListener listener(md);
	md.add_event_listener(speed_exceedence_duration, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(buzzer_delay_seconds, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(seatbelt, __PRETTY_FUNCTION__, &listener);

	for(;;)
	{
		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			SET_TO_STOP_STATE;
			break;
		}
		else if(e.m_event == speed_exceedence_duration)
		{
			SET_NEXT_STATE(1);
			break;
		}
		else if(e.m_event == buzzer_delay_seconds)
		{
			bool seatbelt_buckled = !((g_shmem->GPIO())>>(g_seatbeltBit) & 0x01);
			if(!seatbelt_buckled)
			{
				SET_NEXT_STATE(6);
				break;
			}

			buzzer_delay_seconds = new ats::TimerEvent(md.m_buzzer_delay_time_limit);
			md.add_event_listener(buzzer_delay_seconds, __PRETTY_FUNCTION__, &listener);
		}
		else if(e.m_event == speed)
		{
			md.set("speed", e.m_event->data().get("speed"));
			const int speed_kph = md.get_int("speed");

			if(speed_kph > md.m_low_speed)
			{
				listener.destroy_event(speed_exceedence_duration);
				speed_exceedence_duration = new ats::TimerEvent(md.m_speed_exceedence_duration_seconds);
				md.add_event_listener(speed_exceedence_duration, __PRETTY_FUNCTION__, &listener);
			}

			speed = new SpeedEvent();
			md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
		}
		else if(e.m_event == seatbelt)
		{
			const bool seatbeltbuckled = e.m_event->data().get_bool("seatbelt");
			if( seatbeltbuckled == true )
			{
				listener.destroy_event(buzzer_delay_seconds);
				buzzer_delay_seconds = new ats::TimerEvent(md.m_buzzer_delay_time_limit);
				md.add_event_listener(buzzer_delay_seconds, __PRETTY_FUNCTION__, &listener);
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------
// state_3 - waiting for 'unbuckled' condition to cleared (state 4) or go long enough for a 
//					 message to be generated (state 5).
void SeatbeltStateMachine::state_3()
{
	ats_logf(ATSLOG(0), "enter state 3");

	MyData& md = dynamic_cast<MyData&>(my_data());
	ats::TimerEvent* unbuckle_time_limit = new ats::TimerEvent(md.m_unbuckle_time_limit);
	ats::TimerEvent* speed_exceedence_duration = new ats::TimerEvent(md.m_speed_exceedence_duration_seconds);
	SpeedEvent* speed = new SpeedEvent();
	SeatbeltEvent* seatbelt = new SeatbeltEvent();
	RestartEvent* restart = new RestartEvent();

	EventListener listener(md);
	md.add_event_listener(speed_exceedence_duration, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(unbuckle_time_limit, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(seatbelt, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(restart, __PRETTY_FUNCTION__, &listener);

	for(;;)
	{
		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			SET_TO_STOP_STATE;
			break;
		}
		else if(e.m_event == speed_exceedence_duration)
		{
			//buzz off
			buzz_off("PROC_Seatbelt", 11);
			ats_logf(ATSLOG(0), "Buzz Off");
			SET_NEXT_STATE(1);
			break;
		}
		else if(e.m_event == unbuckle_time_limit)
		{
			//FIXME: send unbuckled event
			send_redstone_ud_msg("message-assembler", 0, "msg seatbelt_off\r");
			ats_logf(ATSLOG(0), "UNBuckled_event sent");
			SET_NEXT_STATE(5);
			break;
		}
		else if(e.m_event == speed)
		{
			md.set("speed", e.m_event->data().get("speed"));
			const int speed_kph = md.get_int("speed");
	
			if(speed_kph > md.m_low_speed)
			{
				listener.destroy_event(speed_exceedence_duration);
				speed_exceedence_duration = new ats::TimerEvent(md.m_speed_exceedence_duration_seconds);
				md.add_event_listener(speed_exceedence_duration, __PRETTY_FUNCTION__, &listener);
			}

			speed = new SpeedEvent();
			md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
		}
		else if(e.m_event == seatbelt)
		{
			const bool seatbeltbuckled = e.m_event->data().get_bool("seatbelt");
			if( seatbeltbuckled == true )
			{
				SET_NEXT_STATE(4);
				break;
			}

			seatbelt = new SeatbeltEvent();
			md.add_event_listener(seatbelt, __PRETTY_FUNCTION__, &listener);
		}
		else if(e.m_event == restart)
		{
			buzz_off("PROC_Seatbelt", 11);
			ats_logf(ATSLOG(0), "Buzz Off");
			SET_NEXT_STATE(1);
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
// State 4 - turning buzzer off
void SeatbeltStateMachine::state_4()
{
	ats_logf(ATSLOG(0), "enter state 4 - Buzzer Off.");
	buzz_off("PROC_Seatbelt", 11);
	SET_NEXT_STATE(2);
}

//-------------------------------------------------------------------------------------------------
void SeatbeltStateMachine::state_5()
{
	ats_logf(ATSLOG(0), "enter state 5");
	MyData& md = dynamic_cast<MyData&>(my_data());
	ats::TimerEvent* speed_exceedence_duration = new ats::TimerEvent(md.m_speed_exceedence_duration_seconds);
	SpeedEvent* speed = new SpeedEvent();
	SeatbeltEvent* seatbelt = new SeatbeltEvent();
	RestartEvent* restart = new RestartEvent();

	EventListener listener(md);
	md.add_event_listener(speed_exceedence_duration, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(seatbelt, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(restart, __PRETTY_FUNCTION__, &listener);

	for(;;)
	{
		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			SET_TO_STOP_STATE;
			break;
		}
		else if(e.m_event == speed_exceedence_duration)
		{
			buzz_off("PROC_Seatbelt", 11);
			ats_logf(ATSLOG(0), "Buzzer is now Off");
			SET_NEXT_STATE(7);
			break;
		}
		else if(e.m_event == seatbelt)
		{
			const bool seatbeltbuckled = e.m_event->data().get_bool("seatbelt");
			if( seatbeltbuckled == true )
			{
				//FIXME send buckled event
				send_redstone_ud_msg("message-assembler", 0, "msg seatbelt_on\r");
				ats_logf(ATSLOG(0), "Buckled_event sent");
				SET_NEXT_STATE(4);
				break;
			}
		}
		else if(e.m_event == restart)
		{
			buzz_off("PROC_Seatbelt", 11);
			ats_logf(ATSLOG(0), "Buzz Off");
			SET_NEXT_STATE(1);
			break;
		}
		else if(e.m_event == speed)
		{
			md.set("speed", e.m_event->data().get("speed"));
			const int speed_kph = md.get_int("speed");

			if(speed_kph > md.m_low_speed)
			{
				listener.destroy_event(speed_exceedence_duration);
				speed_exceedence_duration = new ats::TimerEvent(md.m_speed_exceedence_duration_seconds);
				md.add_event_listener(speed_exceedence_duration, __PRETTY_FUNCTION__, &listener);
			}

			speed = new SpeedEvent();
			md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
		}
	}
}

//-------------------------------------------------------------------------------------------------
// State 6 - turning buzzer on 
void SeatbeltStateMachine::state_6()
{
	ats_logf(ATSLOG(0), "enter state 6 - Buzz On");
	MyData& md = dynamic_cast<MyData&>(my_data());
	buzz_on("PROC_Seatbelt", 11, 550000, 150000, md.m_buzzer_timeout * 1000000);
	SET_NEXT_STATE(3);
}

//-------------------------------------------------------------------------------------------------
void SeatbeltStateMachine::state_7()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	ats::TimerEvent* speed_exceedence_duration = new ats::TimerEvent(md.m_speed_exceedence_duration_seconds);
	SpeedEvent* speed = new SpeedEvent();
	SeatbeltEvent* seatbelt = new SeatbeltEvent();
	RestartEvent* restart = new RestartEvent();

	EventListener listener(md);
	md.add_event_listener(speed_exceedence_duration, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(seatbelt, __PRETTY_FUNCTION__, &listener);
	md.add_event_listener(restart, __PRETTY_FUNCTION__, &listener);

	ats_logf(ATSLOG(0), "enter state 7");
	for(;;)
	{
		AppEventHandler e(listener.wait_event());

		if(e.m_event->m_reason.is_cancelled())
		{
			SET_TO_STOP_STATE;
			break;
		}
		else if(e.m_event == speed_exceedence_duration)
		{
			SET_NEXT_STATE(8);
			break;
		}
		else if(e.m_event == restart)
		{
			SET_NEXT_STATE(1);
			break;
		}
		else if(e.m_event == seatbelt)
		{
			const bool seatbeltbuckled = e.m_event->data().get_bool("seatbelt");
			if( seatbeltbuckled == true )
			{
				//FIXME send buckled event
				send_redstone_ud_msg("message-assembler", 0, "msg seatbelt_on\r");
				ats_logf(ATSLOG(0), "Buckled_event sent");
				SET_NEXT_STATE(1);
				break;
			}
		}
		else if(e.m_event == speed)
		{
			md.set("speed", e.m_event->data().get("speed"));
			const int speed_kph = md.get_int("speed");

			if(speed_kph < md.m_low_speed)
			{
				listener.destroy_event(speed_exceedence_duration);
				speed_exceedence_duration = new ats::TimerEvent(md.m_speed_exceedence_duration_seconds);
				md.add_event_listener(speed_exceedence_duration, __PRETTY_FUNCTION__, &listener);
			}

			speed = new SpeedEvent();
			md.add_event_listener(speed, __PRETTY_FUNCTION__, &listener);
		}
	}
}

//-------------------------------------------------------------------------------------------------
void SeatbeltStateMachine::state_8()
{
	ats_logf(ATSLOG(0), "enter state 8");
	MyData& md = dynamic_cast<MyData&>(my_data());
	buzz_on("PROC_Seatbelt", 11, 550000, 150000, md.m_buzzer_timeout * 1000000);
	ats_logf(ATSLOG(0), "Buzz On");
	SET_NEXT_STATE(5);
}

//-------------------------------------------------------------------------------------------------
// Received 'speed' message from avl-monitor
int SeatbeltStateMachine::ac_speed(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	if(p_argc >= 2)
	{
		ats::StringMap* s = new ats::StringMap();
		const char* speed = p_argv[1];
		const int i_speed = strtol(speed, 0, 0);

		if((i_speed >= 0) && (i_speed <= g_max_valid_speed))
		{
			s->set("speed", speed);
			p_acc.m_data->post_event("SpeedEvent", s);
			ats_logf(ATSLOG(2), "post speed event %d", i_speed);
		}
	}
}

//-------------------------------------------------------------------------------------------------
// Received 'sensor' changed message from i2c-gpio-monitor
int SeatbeltStateMachine::ac_sensor(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	ats::StringMap* s = new ats::StringMap();
	bool seatbelt_buckled = !((g_shmem->GPIO())>>(g_seatbeltBit) & 0x01);
	s->set("seatbelt", (seatbelt_buckled)?"1":"0");
	p_acc.m_data->post_event("SeatbeltEvent", s);
	ats_logf(ATSLOG(2), "post sensor buckled event %d", seatbelt_buckled);
}

//-------------------------------------------------------------------------------------------------
// Received 'seatbelt' message from OBD based monitor can-(ford, dodge, gm)-seatbelt-monitor
int SeatbeltStateMachine::ac_seatbelt(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	if(p_argc >= 2)
	{
		ats::StringMap* s = new ats::StringMap();
		const char* seatbelt= p_argv[1];
		s->set("seatbelt", seatbelt);
		p_acc.m_data->post_event("SeatbeltEvent", s);
	}
}

//-------------------------------------------------------------------------------------------------
// Received 'speed' message from avl-monitor
int SeatbeltStateMachine::ac_ignition(AdminCommandContext &p_acc, const AdminCommand&, int p_argc, char *p_argv[])
{
	ats::StringMap* s = new ats::StringMap();
	s->set("restart", "0");
	p_acc.m_data->post_event("RestartEvent", s);
}

