#pragma once

#include "ats-common.h"
#include "state_machine.h"

class AdminCommandContext;
class AdminCommand;

class PowerMonitorStateMachine : public StateMachine
{
private:
	bool m_bWokeOnCritBatt;  // set to true if wakeup was critical battery.
	int64_t m_firmwareUpdateTime;
public:
	PowerMonitorStateMachine(StateMachineData& );
	virtual~ PowerMonitorStateMachine();

	virtual void start();

	bool& set_shutdown_flag(bool p_b)
	{
		return (m_immediate_shutdown_flag = p_b);
	}

	bool& set_force_shutdown_flag(bool p_b)
	{
		return (m_force_shutdown_flag = p_b);
	}
private:
	void (PowerMonitorStateMachine::*m_state_fn)();

	void state_0();
	void state_1();
	void state_2();
	void state_3();
	void state_4();
	void state_5();
	void state_6();
	void state_7();
	void state_8();  // low battery - waiting for message to clear

	bool m_immediate_shutdown_flag;
	bool m_force_shutdown_flag;
	bool m_bDisableSleep;  // from db-config RedStone DisableSleep On/Off - if On the system will not power down unless forced to
	int m_KeepAwakeMinutes; // number of minutes to stay awake until we sleep
	float m_WakeupVoltage; 
	float m_ShutdownVoltage; // if above this - keep running.  Otherwise clear messages and shutdown.
	float m_CriticalVoltage;  // if below this send a critical battery message and shutdown.
	short m_CountLowBattery;  // count of low battery messages below cutoff.
	short m_IgnitionDelay;
  
	static int ac_PowerMonitorStateMachine(AdminCommandContext&, const AdminCommand&, int p_argc, char* p_argv[]);
	bool CheckForLowBattery();
};
