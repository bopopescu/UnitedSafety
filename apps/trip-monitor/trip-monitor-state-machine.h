#pragma once

#include "ats-common.h"
#include "state_machine.h"
#include "RedStone_IPC.h"

class AFS_Timer;
class AdminCommandContext;
class AdminCommand;
class MyData;

class TripMonitorSm : public StateMachine
{
public:
	TripMonitorSm(MyData& );
	virtual~ TripMonitorSm();

	virtual void start();
private:
	void (TripMonitorSm::*m_state_fn)();

	void state_0();
	void state_1();
	void state_2();
	void state_3();
	void state_4();
	void state_5();
	void state_6();
	void state_7();
	void state_8();
	void state_9();
	void state_10();
	void state_11();
	void state_12();
	void state_14();
	void state_15();
	void state_16();
	void state_17();
	void state_19();
	void state_20();
	void state_21();
	void state_23();
	static int ac_TripMonitorSm(AdminCommandContext&, const AdminCommand&, int p_argc, char* p_argv[]);

	void log(const ats::String& p_msg) const;
	void send_msg(const ats::String& p_msg) const;
	void set_schedule_msg(const ats::String&)const;

	void send_message();
	bool getDistance(int&);

	void load_config();

	// Description: Returns the current instantaneous speed limit.
	int get_speed_limit();

	AFS_Timer* m_debug_timer;
	AFS_Timer* m_distance_timer;
	double m_distance_meters;
	int m_distance;
	int m_prev_display_distance;
	int m_distance_init;

	int m_buzzer_delay_seconds;
	int m_message_delay_seconds;
	int m_periodic_msg_generator_run;
	int m_max_speed;  // the max speed cutoff that triggers a buzzer (usually 120kph)
	int m_ignition_on_buzz_delay;
	int m_ignition_on_buzz_on;
	int m_ignition_on_buzz_off;
	int m_ignition_on_buzz_dur;
	int m_StartupDistance;  // distance travelled before 
	int m_state_4_low_speed_kph;
	ats::String m_speed_source;

	int m_sbr_offset;
	int m_sbr_timeout;
	float m_sbr_scale;

	REDSTONE_IPC m_RedStoneData;
};
