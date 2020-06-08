#pragma once

#include <map>
#include <list>

#include <semaphore.h>

#include "state-machine-data.h"
#include "socket_interface.h"
#include "ats-common.h"
#include "event_listener.h"

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
	AdminCommand(AdminCommandFn p_fn)
	{
		m_fn = p_fn;
		m_state_machine = 0;
	}

	AdminCommand(AdminCommandFn p_fn, StateMachine& p_state_machine)
	{
		m_fn = p_fn;
		m_state_machine = &p_state_machine;
	}

	AdminCommandFn m_fn;
	StateMachine* m_state_machine;
};

typedef std::map <const ats::String, AdminCommand> AdminCommandMap;
typedef std::pair <const ats::String, AdminCommand> AdminCommandPair;
//typedef std::map <const ats::String, Command*> CommandMap;

namespace SER_GPS_NS
{

class Work
{
public:
	ats::StringMap m_data;
};

	typedef std::map <const ats::String, Work> WorkMap;
	typedef std::pair <const ats::String, Work> WorkPair;

}; // PowerMonitor namespace

class MyData : public StateMachineData
{
public:
	AdminCommandMap m_command;

	MyData();

	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	ats::String set_input(const ats::StringMap& p_args);



private:
	SER_GPS_NS::WorkMap m_work;

	pthread_mutex_t* m_mutex;
	pthread_mutex_t* m_work_mutex;

	void lock_work() const
	{
		pthread_mutex_lock(m_work_mutex);
	}

	void unlock_work() const
	{
		pthread_mutex_unlock(m_work_mutex);
	}

	bool h_is_shutting_down() const;

};

class InputEvent : public AppEvent
{
public:
	COMMON_EVENT_DECLARATION(InputEvent)
};


