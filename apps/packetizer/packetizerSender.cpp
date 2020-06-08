#include <fstream>
#include <iomanip>
#include <algorithm>

#include <errno.h>
#include <stdlib.h>

#include "atslogger.h"
#include "packetizerSender.h"
#include "ConfigDB.h"

PacketizerSender::PacketizerSender(MyData& pData) :
	m_data(&pData) 
{
}

PacketizerSender::~PacketizerSender()
{
}

void split(std::vector<ats::String> &p_list, const ats::String &p_s, char p_c)
{
	ats::String s;
	p_list.clear();
	for(ats::String::const_iterator i = p_s.begin(); i != p_s.end(); ++i) {
		const char c = *i;
		if(c == p_c) {
			p_list.push_back(s);
			s.clear();
		} else {
			s += c;
		}
	}
	p_list.push_back(s);
}

bool PacketizerSender::start()
{
  db_monitor::ConfigDB db;
  
  m_host = db.GetValue("packetizer", "host", "redstone.atsplace.com");
  m_port = db.GetInt("packetizer", "port", "38023");

	if(m_host.empty())
	{
		ats_logf(ATSLOG(0), "%s: Host not specified",__PRETTY_FUNCTION__);
	}

	if(!m_port)
	{
		ats_logf(ATSLOG(0), "%s: Port not specified for host \"%s\"",__PRETTY_FUNCTION__, m_host.c_str());
	}

	init_ClientSocket(&cs_sender);
	return true;
}

bool PacketizerSender::sendMessage(send_message& msg)
{
	ats_logf(ATSLOG(0), "Connected to %s", m_host.c_str());

	char buf[8192];
	for( int i = 0; i < (int)msg.data.size(); i++)
		buf[i]=msg.data[i];

	ssize_t nsend =  send(cs_sender.m_fd, buf, msg.data.size(), MSG_NOSIGNAL); 
	if(nsend > 0)
	{
		std::ostringstream outstr;
		outstr << "MSG[" << msg.data.size() << "]:";
		for(int i = 0; i < (int)msg.data.size(); ++i)
		{
			outstr << std::hex <<  std::setfill('0') << std::setw(2) << std::uppercase << (msg.data[i] & 0xff) << ',';
		}
		ats_logf(ATSLOG(0), "%s,%d : %s", __FILE__, __LINE__, outstr.str().c_str());
	}
	else 
		return false;
	return true;
}

int PacketizerSender::waitforack(send_message& msg)
{
	int ret = 0;
	char buf[8192];
	struct timeval tv;

	for(;;)
	{
		std::ostringstream outstr;
		tv.tv_sec = 30;
		tv.tv_usec = 0;

		setsockopt(cs_sender.m_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
		const ssize_t nread = recv(cs_sender.m_fd, buf, sizeof(buf), 0);

		if(nread <= 0)
		{
			if(errno == EAGAIN )
			{
				ats_logf(ATSLOG(0), "%s,%d: Read failed from Server, 30 sec timeout", __FILE__, __LINE__);
				m_data->testdata_log("Read failed from Server, 30 sec timeout");
				ret = 0;
				break;
			}

			ats_logf(ATSLOG(0), "%s,%d: Read failed from Server, closing connection err=%d", __FILE__, __LINE__, errno);

			{
				ats::String buf;
				ats_sprintf(&buf, " Read failed from Server, closing connection err=%d", errno);
				m_data->testdata_log(buf);
				ret = -2;
				break;
			}
		}

		outstr << "FROM-REDSTONE[" << int(nread) << "]:";
		for(int i = 0; i < nread; ++i)
		{
			outstr << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << (buf[i] & 0xff) << ',';
		}

		ats_logf(ATSLOG(0), "%s,%d : %s", __FILE__, __LINE__, outstr.str().c_str());

		if(*(int*)buf == msg.seq)
		{
			ret = 1;
			break;
		}
		else if((*(int*)buf) < msg.seq) //not expected value, sometimes because of timeout, we send twice, and system may send old response number back, so still wait.
		{
			continue;
		}
		else
		{
			//ack error;
			ret = -1;
			break;
		}
	}

	return ret;
}

void PacketizerSender::connect()
{
	for(;;)
	{
		if(!connect_client(&cs_sender, m_host.c_str(), m_port)) 
			break;
		
		m_RedStone.FailedToSend();
		sleep(1);
	}
}

void PacketizerSender::reconnect()
{
	close(cs_sender.m_fd);
	cs_sender.m_fd = -1;
	connect();
}

void PacketizerSender::disconnect()
{
	ats_logf(ATSLOG(0), "%s,%d: Disconnect to Server", __FILE__, __LINE__);
	close(cs_sender.m_fd);
	cs_sender.m_fd = -1;
}
