#pragma once

#include <map>
#include <set>

#include <semaphore.h>

#include "socket_interface.h"
#include "state-machine-data.h"
#include "redstone-socketcan-j1939.h"
#include "signal-monitor.h"
#include "can-j1939-db.h"
#include "ConfigDB.h"
#include "RedStone_IPC.h"  // global shared memory for Voltage

#define J1939_CAN_DM1_PGN   0x0FECA
#define J1939_CAN_DM5_PGN   0x0FECE
#define J1939_CAN_DM11_PGN	0x0FED3
#define J1939_CAN_REQ_PGN	0x0EA00
#define J1939_CAN_SPEED_PGN	0x0FEF1
#define J1939_CAN_RPM_PGN	0x0F004
#define J1939_CAN_IDENTIFICATION_PGN	0x0FEEB
#define J1939_CAN_ENGINEHOURS_PGN 0x0FEE5
#define J1939_CAN_ENGINEHOURS_SPN 247
#define J1939_CAN_FUELCONSUMPTION_PGN 0x0FEE9
#define J1939_CAN_FUELCONSUMPTION_SPN 250
#define J1939_CAN_ENGINE_SPEEP_SPN 190
#define J1939_CAN_WHEEL_BASED_SPEEP_SPN 84

#define J1939_CAN_IDENTIFICATION_MAKE_SPN 586
#define J1939_CAN_IDENTIFICATION_MODEL_SPN 587
#define J1939_CAN_IDENTIFICATION_SN_SPN 588
#define J1939_CAN_IDENTIFICATION_UNITNUMBER_SPN 233

#define IF_PRESENT(P_name, P_EXP) if(g_has_ ## P_name) {P_EXP;}

typedef unsigned long long ull;
class MyData;
class StateMachine;
class CanJ1939DB;

class AdminCommandContext
{
public:
	AdminCommandContext(MyData& p_data, ClientSocket& p_socket)
	{
		m_data = &p_data;
		m_socket = &p_socket;
		m_cd = 0;
	}

	AdminCommandContext(MyData& p_data, ClientData& p_cd)
	{
		m_data = &p_data;
		m_socket = 0;
		m_cd = &p_cd;
	}

	int get_sockfd() const
	{
		return (m_cd ? m_cd->m_sockfd : m_socket->m_fd);
	}

	MyData& my_data() const
	{
		return *m_data;
	}

	MyData* m_data;

private:
	ClientSocket* m_socket;
	ClientData* m_cd;
};

class AdminCommand;

typedef int (*AdminCommandFn)(AdminCommandContext&, const AdminCommand&, int p_argc, char* p_argv[]);
typedef void (*process_diagnosticsfn)(MyData& md, const uint8_t* buf, int bufsize );

class AdminCommand
{
public:
	AdminCommand(AdminCommandFn p_fn, const ats::String& p_synopsis)
	{
		m_fn = p_fn;
		m_state_machine = 0;
	}

	AdminCommand(AdminCommandFn p_fn, const ats::String& p_synopsis, StateMachine& p_state_machine)
	{
		m_fn = p_fn;
		m_state_machine = &p_state_machine;
	}

	AdminCommandFn m_fn;
	StateMachine* m_state_machine;

	ats::String m_synopsis;
};

typedef std::map <const ats::String, AdminCommand> AdminCommandMap;
typedef std::pair <const ats::String, AdminCommand> AdminCommandPair;

typedef struct
{
	int spn;
	uint8_t span;
	uint8_t startposition;

}spndata;

struct spnconfig
{
	int spn;
	float llimit;
	float hlimit;
	int duration;
	public: spnconfig():llimit(0.0), hlimit(0.0),duration(0){}
};

struct requestData
{
	int spn;
	float value;
};

typedef std::vector <ats::String > fileslist;
typedef std::vector <periodicdata > periodicdatalist;
typedef std::map <const int, requestData> requestDataMap;

typedef std::map <const int, SignalMonitor*> spnsignalMap;
typedef std::map <const int, process_diagnosticsfn> diagnosticMap;

class MyData : public StateMachineData
{
public:
	AdminCommandMap m_command;
	spnsignalMap m_spnsignalMap;
	diagnosticMap m_diagnosticMap;
	std::set <int> m_diagnosticSet;
	REDSTONE_IPC m_RedStoneData;
	std::map<int,int> m_reported_dm1_map;
	std::set<int> m_requestSet;

	MyData();

	int start_state_machines(int argc, char* argv[]);

	void start_server();

	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	void post_exit_sem(){ sem_post(m_exit_sem);}
	void wait_exit_sem(){ sem_wait(m_exit_sem);}
	void post_scan_sem(){ sem_post(m_scan_sem);}
	void wait_scan_sem(){ sem_wait(m_scan_sem);}
	void setversion(const ats::String& version){m_version = version;}
	ats::String getversion()const {return m_version;}

	CANSocket* m_s;
	pthread_t m_reader_thread;
	pthread_t m_signalmonitor_thread;
	pthread_t m_request_thread;

	bool getSignalMonitorList( int, std::vector<SignalMonitor*>& );

private:

	ServerData m_command_server;

	sem_t* m_exit_sem;
	sem_t* m_scan_sem;
	pthread_mutex_t *m_mutex;
	ats::String m_version;
};

class IgnitionEvent : public AppEvent
{
public:
        COMMON_EVENT_DECLARATION(IgnitionEvent)
};

