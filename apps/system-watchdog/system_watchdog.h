#pragma once

#include <map>

#include "socket_interface.h"
#include "ats-common.h"

class MyData;

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
	}

	AdminCommandFn m_fn;
};

typedef std::map <const ats::String, AdminCommand> AdminCommandMap;
typedef std::pair <const ats::String, AdminCommand> AdminCommandPair;

class MyData : public ats::CommonData
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

private:
	pthread_mutex_t* m_mutex;

};
