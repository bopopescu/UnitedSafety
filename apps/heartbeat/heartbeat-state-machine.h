#pragma once

#include "ats-common.h"
#include "state_machine.h"
#include "NMEA_Client.h"
#include "RedStone_IPC.h"

class AFS_Timer;
class AdminCommandContext;
class AdminCommand;
class MyData;

class HeartbeatSm : public StateMachine
{
public:
	HeartbeatSm(MyData& );
	virtual~ HeartbeatSm();

	virtual void start();
	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}
	void set_work() const
	{
		system("echo \"set_work key=heartbeat\"|telnet localhost 41009");
	}
	void unset_work() const
	{
		system("echo \"unset_work key=heartbeat\"|telnet localhost 41009");
	}
private:
	void (HeartbeatSm::*m_state_fn)();

	void state_0();  // determine wakeup
	void state_1();  // wait for GPS
	void state_2();  // send heartbeat
	void state_3();  // reset heartbeat
	void state_4();  // wait for event (Ignition or heartbeat expiring)
	void state_5();  // reset heartbeat
	void state_6();  // wait for event (Igniton off or heartbeat expiring)
	void state_7();  // reset heartbeat
	
	void ResetHeartbeat();

	static int ac_HeartbeatSm(AdminCommandContext&, const AdminCommand&, int p_argc, char* p_argv[]);

	void log(const ats::String& p_msg) const;
	void testdata_log(const ats::String& p_msg) const;
	void send_msg(const ats::String& p_msg) const;
	void set_wakeup_sched() const;
	void unset_wakeup_sched() const;
	bool testdatadir_existing() const;
	void load_config();
	
	void send_message();
	bool getDistance(int&);
	bool isIgnitionOn() const;

	AFS_Timer* m_debug_timer;
	pthread_mutex_t* m_mutex;
	int m_time_from_db;
	bool m_testdatadir_existing;
	unsigned int m_state_1_timeout_seconds;
 	REDSTONE_IPC m_ipc;
  NMEA_Client m_NMEA;

};
