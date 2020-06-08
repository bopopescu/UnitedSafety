#pragma once

#include <map>
#include <list>

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

	int start_state_machines(int argc, char* argv[]);

	void start_server();

	void post_exit_sem(){ sem_post(m_exit_sem);}
	void wait_exit_sem(){ sem_wait(m_exit_sem);}
	void setversion(const ats::String& version){m_version = version;}
	ats::String getversion()const {return m_version;}

private:

	ServerData m_command_server;

	sem_t* m_exit_sem;
	ats::String m_version;
};


class IgnitionEvent : public AppEvent
{
public:
	COMMON_EVENT_DECLARATION(IgnitionEvent)
};

int set_time_in_db(time_t p_sec);

int get_time_from_db();
