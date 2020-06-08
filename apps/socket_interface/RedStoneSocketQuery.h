// AWARE360 FIXME: This header file is deprecated. Use "TRULinkSocketQuery.h" instead.
#pragma once

#include <errno.h>

#include "ats-common.h"
#include "SocketInterfaceResponse.h"

namespace ats
{

class RedStoneSocketQuery
{
public:

	RedStoneSocketQuery(const ats::String& p_path)
	{
		m_cs = new ClientSocket;
		init_ClientSocket(m_cs);
		connect_redstone_ud_client(m_cs, p_path.c_str());
		m_response = new SocketInterfaceResponse(sockfd());
	}

	virtual~ RedStoneSocketQuery()
	{
		delete m_response;
		close_ClientSocket(m_cs);
		delete m_cs;
	}

	ats::String query(const ats::String& p_cmd) const
	{
		const int ret = send_cmd(sockfd(), MSG_NOSIGNAL, "%s\r", p_cmd.c_str());

		if(ret < 0)
		{
			m_response->set_error(-errno);
			return ats::String();
		}

		return m_response->get();
	}

	int error() const
	{
		return m_response->error();
	}

private:
	ClientSocket* m_cs;
	SocketInterfaceResponse* m_response;

	int sockfd() const
	{
		return m_cs->m_fd;
	}

	RedStoneSocketQuery(const RedStoneSocketQuery&);
	RedStoneSocketQuery& operator =(const RedStoneSocketQuery&) const;
};

}
