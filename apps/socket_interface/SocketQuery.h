#pragma once

#include <errno.h>

#include "ats-common.h"
#include "SocketInterfaceResponse.h"

namespace ats
{
// ATS FIXME: Add an implementation for Unix Domain Sockets and TRULink Unix Domain Sockets
//	specifically.

// Description: A class which connects and stays connected to the given host and port.
//	Commands are called using "query" and the "query" function does not return
//	until the receiver responds (or an error is detected). This is useful for
//	situations where hand-shaking is required.
class SocketQuery
{
public:

	SocketQuery(const ats::String& p_host, int p_port, ClientSocket* p_cs=0)
	{

		if(p_cs)
		{
			m_cs = p_cs;
		}
		else
		{
			init_ClientSocket(m_cs = new ClientSocket());
		}

		connect_client(m_cs, p_host.c_str(), p_port);

		m_response = new SocketInterfaceResponse(sockfd());
	}

	virtual~ SocketQuery()
	{
		delete m_response;
		close_ClientSocket(m_cs);
		delete m_cs;
	}

	// Description: Sends a command to the host/port server and does not return
	//	until a response is received (or an error is detected).
	ats::String query(const ats::String& p_cmd) const
	{
		send_cmd(sockfd(), MSG_NOSIGNAL, "%s\r", p_cmd.c_str());
		return m_response->get();
	}

private:
	ClientSocket* m_cs;
	SocketInterfaceResponse* m_response;

	int sockfd() const
	{
		return m_cs->m_fd;
	}

	SocketQuery(const SocketQuery&);
	SocketQuery& operator =(const SocketQuery&) const;
};

}
