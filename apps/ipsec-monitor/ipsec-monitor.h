#pragma once

#include <map>
#include <list>

#include <semaphore.h>

#include "socket_interface.h"

extern int g_dbg;

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

typedef int (*AdminCommand)(AdminCommandContext&, int p_argc, char* p_argv[]);
typedef std::map <const ats::String, AdminCommand> AdminCommandMap;
typedef std::pair <const ats::String, AdminCommand> AdminCommandPair;

class MyData 
{
public:
	AdminCommandMap m_command;
	ats::StringMap m_config;

	MyData();

	int start_state_machines(int argc, char* argv[]);

	void start_server();

	void post_exit_sem(){ sem_post(m_exit_sem);}
	void wait_exit_sem(){ sem_wait(m_exit_sem);}
	void setversion(const ats::String& version){m_version = version;}
	ats::String getversion()const {return m_version;}
	bool getStatus(){return flag_start;}
	int ipsec_running();

protected:
	void turn_connection_on(const ats::String & name);
	void turn_connection_off(const ats::String & name);

private:

	ServerData m_command_server;

	sem_t* m_exit_sem;
	ats::String m_version;

	bool flag_start;
};
