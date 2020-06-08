#pragma once

#include "ats-common.h"
#include "state_machine.h"
#include "RedStone_IPC.h"

class AFS_Timer;
class AdminCommandContext;
class AdminCommand;
class MyData;

struct periodicRecord
{
	float m_min;
	float m_max;
	int   m_timestamp;
};

bool compareTimeStamp( const periodicRecord& pl, const periodicRecord& pr );
typedef std::map <int, std::list<periodicRecord> > periodicRecordMap;

class CanJ1939MonitorSm : public StateMachine
{
public:
	CanJ1939MonitorSm(MyData& );
	virtual~ CanJ1939MonitorSm();

	virtual void start();
private:
	void (CanJ1939MonitorSm::*m_state_fn)();

	void state_0();
	void sendMessage(bool);

	static int ac_CanJ1939MonitorSm(AdminCommandContext&, const AdminCommand&, int p_argc, char* p_argv[]);

	void log(const ats::String& p_msg) const;

	void load_config();

	int m_periodic_msg_generator_seconds;
	int m_sourceaddress;
	periodicRecordMap m_periodicRecordMap;
};
