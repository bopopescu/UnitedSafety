#include <map>
#include <list>

#include <semaphore.h>

#include "socket_interface.h"
#include "RedStone_IPC.h"
#include "timer-event.h"
#include "ats-common.h"
#include "ATMessageHandler.h"

#define TELIT_PORT "/dev/ttyModemAT"

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

class MyData : public ats::CommonData
{
public:
	AdminCommandMap m_command;
	ats::StringMap m_config;

	MyData();

	void start_server();

	void setversion(const ats::String& version){m_version = version;}
	ats::String getversion()const {return m_version;}
	int fd;
	REDSTONE_IPC m_RedStoneData;
//	sem_t* m_sem;

	pthread_t* m_ppp_thread;
	ats::TimerEvent m_ppp_timer;

	ATMessageHandlerMap m_mh;

	void add_message_handler(const ats::String& p_name, ATMessageHandler* p_handler);
private:
	ServerData m_command_server;

	ats::String m_version;
};

extern int g_creg_mode;
extern int g_psnt_mode;
extern int g_AcT;
extern int g_network_type;

extern const int g_signal_undetectable;
extern int g_rssi;
