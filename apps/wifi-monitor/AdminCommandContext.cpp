#include "socket_interface.h"
#include "AdminCommandContext.h"
	
AdminCommandContext::AdminCommandContext(MyData& p_data, ClientSocket& p_socket)
{
	m_data = &p_data;
	m_socket = &p_socket;
	m_cd = 0;
}

AdminCommandContext::AdminCommandContext(MyData& p_data, ClientData& p_cd)
{
	m_data = &p_data;
	m_socket = 0;
	m_cd = &p_cd;
}

int AdminCommandContext::get_sockfd() const
{
	return (m_cd ? m_cd->m_sockfd : m_socket->m_fd);
}

MyData& AdminCommandContext::my_data() const
{
	return *m_data;
}
