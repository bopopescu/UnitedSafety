#pragma once
#include <map>

#include "socket_interface_core.h"
#include "ats-common.h"

class MyData;

class AdminCommandContext
{
public:
	AdminCommandContext(MyData& p_data, ClientSocket& p_socket);
	AdminCommandContext(MyData& p_data, ClientData& p_cd);

	int get_sockfd() const;

	MyData& my_data() const;

	MyData* m_data;
private:
	ClientSocket* m_socket;
	ClientData* m_cd;
};

typedef int (*AdminCommand)(AdminCommandContext&, int p_argc, char* p_argv[]);
typedef std::map <const ats::String, AdminCommand> AdminCommandMap;
typedef std::pair <const ats::String, AdminCommand> AdminCommandPair;
