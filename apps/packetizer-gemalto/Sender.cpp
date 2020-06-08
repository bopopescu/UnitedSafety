#include <iostream>
#include <iomanip>
#include <stdlib.h>

#include "atslogger.h"
#include "ConfigDB.h"
#include "socket_interface.h"
#include "Sender.h"

Sender::Sender(MyData& p_data) : PacketizerSender(p_data)
{
	db_monitor::ConfigDB db;
	m_host = db.GetValue("packetizer-gemalto", "host", "redstone.atsplace.com");
	m_port = db.GetInt("packetizer-gemalto", "port", "40556");

	if(m_host.empty())
	{
		ats_logf(ATSLOG(0), "%s: Host not specified",__PRETTY_FUNCTION__);
	}

	if(!m_port)
	{
		ats_logf(ATSLOG(0), "%s: Port not specified for host \"%s\"",__PRETTY_FUNCTION__, m_host.c_str());
	}

	init_ClientSocket(&m_cs_sender);
	m_connected = false;
}

bool Sender::connect()
{
	m_connected = (!connect_client(&m_cs_sender, m_host.c_str(), m_port)) ? true : false;
	return m_connected;
}

void Sender::disconnect()
{
	close_ClientSocket(&m_cs_sender);
	m_connected = false;
}

bool Sender::sendMessage(send_message &msg)
{
	char buf[8192];
	for( int i = 0; i < (int)msg.data.size(); i++)
		buf[i]=msg.data[i];

	ssize_t nsend =  send(m_cs_sender.m_fd, buf, msg.data.size(), MSG_NOSIGNAL);

	if(nsend > 0)
	{
		std::ostringstream outstr;
		outstr << "MSG[" << msg.data.size() << "]:";
		for(int i = 0; i < (int)msg.data.size(); ++i)
		{
			outstr << std::hex <<  std::setfill('0') << std::setw(2) << std::uppercase << (msg.data[i] & 0xff) << ',';
		}
		ats_logf(ATSLOG(0), "%s,%d : %s", __FILE__, __LINE__, outstr.str().c_str());
		return true;
	}

	return false;
}

