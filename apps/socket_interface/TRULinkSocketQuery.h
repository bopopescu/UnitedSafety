#pragma once

#include <errno.h>

#include "ats-common.h"
#include "SocketInterfaceResponse.h"

namespace ats
{

class TRULinkSocketQuery
{
public:

	TRULinkSocketQuery(const ats::String& p_path)
	{
		m_cs = new ClientSocket;
		init_ClientSocket(m_cs);
		connect_trulink_ud_client(m_cs, p_path.c_str());
		m_response = new SocketInterfaceResponse(sockfd());
	}

	virtual~ TRULinkSocketQuery()
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

	TRULinkSocketQuery(const TRULinkSocketQuery&);
	TRULinkSocketQuery& operator =(const TRULinkSocketQuery&) const;
};

}
