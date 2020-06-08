#pragma once

#include <map>
#include <list>

#include <pthread.h>
#include <semaphore.h>

#include "socket_interface.h"
#include "event_listener.h"
#include "state-machine-data.h"

extern int g_dbg;

class MyData;
class StateMachine;

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

class MyData : public StateMachineData
{
public:
	AdminCommandMap m_command;

	MyData();

	ClientSocket cs_trip_stats;

	int start_state_machines(int argc, char* argv[]);

	void start_server();

	void post_exit_sem(){ sem_post(m_exit_sem);}
	void wait_exit_sem(){ sem_wait(m_exit_sem);}

	int get_speed_limit(int p_default, int p_offset, float p_scale);

	void set_skybase_poll_period(int);

	void set_skybase_stable_readings(int);

	int get_skybase_poll_period();

	int get_skybase_stable_readings();
	int get_skybase_speed_limit(){if (m_skybase_speed_valid) return m_skybase_speed; else return 0;};

private:
	friend void* skybase_thread_fn(void* p);

	ServerData m_command_server;

	pthread_mutex_t m_skybase_mutex;

	// Description:
	// XXX: Only function "main.cpp:skybase_thread_fn" may access these variables.
	int m_skybase_speed;
	int m_skybase_speed_valid;
	int m_skybase_poll_period;
	int m_skybase_stable_readings;

	sem_t* m_exit_sem;
};


class RPMEvent : public AppEvent
{
public:
        COMMON_EVENT_DECLARATION(RPMEvent)
};

class SpeedEvent : public AppEvent
{
public:
        COMMON_EVENT_DECLARATION(SpeedEvent)
};

class IgnitionEvent : public AppEvent
{
public:
	COMMON_EVENT_DECLARATION(IgnitionEvent)
};
