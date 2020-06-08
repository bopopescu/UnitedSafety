#pragma once

#include "ats-common.h"
#include "state_machine.h"

class AFS_Timer;
class AdminCommandContext;
class AdminCommand;
class MyData;

class PeriodicmsggenSm : public StateMachine
{
public:
	PeriodicmsggenSm(MyData& );
	virtual~ PeriodicmsggenSm();

	virtual void start();
private:
	void (PeriodicmsggenSm::*m_state_fn)();

	void state_0();
	void state_1();
	void state_2();

	static int ac_PeriodicmsggenSm(AdminCommandContext&, const AdminCommand&, int p_argc, char* p_argv[]);
	void send_msg(const ats::String& p_msg) const;
	void load_config();

	AFS_Timer* m_debug_timer;
	int m_schedule_msg_seconds;

};
