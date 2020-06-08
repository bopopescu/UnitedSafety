#include <errno.h>
#include <stdlib.h>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <time.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>

//#include <QTimer>
//#include <QHostInfo>

#include "ats-common.h"
#include "atslogger.h"
#include "ConfigDB.h"
#include "RedStone_IPC.h"
#include "socket_interface.h"
#include "utility.h"
#include "packetizerCellSender.h"
#include "TLPMessage.h"

ats::String PacketizerCellSender::m_host;
int PacketizerCellSender::m_port;
ats::String PacketizerCellSender::m_defaultMobileId;
sem_t PacketizerCellSender::m_ack_sem;
pthread_mutex_t PacketizerCellSender::m_mutex;
ats::String PacketizerCellSender::m_ack_type;
int PacketizerCellSender::m_ack;
bool PacketizerCellSender::m_bIsPrimary;
int PacketizerCellSender::m_BroadcastPort;

//-------------------------------------------------------------------------------------------------
PacketizerCellSender::PacketizerCellSender(MyData& pData): PacketizerSender(pData)
{
	sem_init(&m_ack_sem, 0 , 0);
	pthread_mutex_init(&m_mutex, NULL);
	m_HostPort = 41414;

	db_monitor::ConfigDB db;
	const ats::String app_name("packetizer-cams");
	m_host = db.GetValue(app_name, "host", "www.myabsolutetrac.com");
	m_port = db.GetInt(app_name, "port", 51001);
	m_timeout = db.GetInt(app_name, "timeout", 10);
	m_ack_type = db.GetValue(app_name, "ack_type", "standard");
	m_defaultMobileId = db.GetValue("RedStone", "IMEI");
	m_BroadcastPort = db.GetInt("system", "BroadcastPort", 39002);
	
	m_bIsPrimary = false;
	
	if ( db.GetValue("system", "RouteOverride") == "primary")
		m_bIsPrimary =	true;

	m_ack = 0;

}

//-------------------------------------------------------------------------------------------------
PacketizerCellSender::~PacketizerCellSender()
{}

//-------------------------------------------------------------------------------------------------
bool PacketizerCellSender::start()
{

	init_ServerData(&m_sd, 1);
	m_sd.m_port = m_HostPort;
	m_sd.m_cs = processUdpData;
	start_udp_server(&m_sd)	;

	return true;
}

//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
ssize_t PacketizerCellSender::sendUdpData(ServerData& p_sd, const char *p_data, uint p_data_length)
{
	struct addrinfo hints;
	struct addrinfo* res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_family = AF_INET;
	int ret = getaddrinfo(m_host.c_str(), NULL, &hints, &res);
	if( ret != 0)
	{
		ats_logf(ATSLOG(0), "%s,%d:getaddrinfo error:%d - %s",__FILE__,__LINE__,ret, gai_strerror(ret));
		if(ret == EAI_SYSTEM)
		{
			ats_logf(ATSLOG(0), "%s,%d:EAI_SYSTEM Error %d -%s", __FILE__,__LINE__, errno, strerror(errno));
		}
		return -2;
	}

	struct sockaddr_in src_addr;
	src_addr.sin_family = AF_INET;
	src_addr.sin_addr.s_addr = ((sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
	src_addr.sin_port=htons(m_port);
	socklen_t addrlen = sizeof(src_addr);
	std::ostringstream outstr;
	outstr << "MSG[" << p_data_length << "]:";

	for(uint i = 0; i < p_data_length; ++i)
	{
		outstr << std::hex <<	std::setfill('0') << std::setw(2) << std::uppercase << (p_data[i] & 0xff) << ',';
	}

	ats_logf(ATSLOG(0), "%s,%d : %s", __FILE__, __LINE__, outstr.str().c_str());
	return sendto(p_sd.m_udp->m_sockfd, p_data, p_data_length, MSG_NOSIGNAL, (struct sockaddr*)&src_addr, addrlen);
}

//-------------------------------------------------------------------------------------------------
bool PacketizerCellSender::sendSingleMessage(message_info* p_mi, std::map<int,int>& p_msgTypes)
{
	std::vector<unsigned char> write_data;
	
	if(!packetizeMessageInfo(p_mi, write_data))
	{
		return false;
	}
	
	return sendMessage(write_data);
	return true;
}


//-------------------------------------------------------------------------------------------------
bool PacketizerCellSender::sendMessage(std::vector <unsigned char> msg)
{
	ssize_t nsend = -1;

	nsend = sendUdpData(m_sd, (char *)&msg[0], msg.size());
	
	if(nsend < msg.size())
	{
		if (nsend >= 0)
		{
			ats_logf(ATSLOG(0), "%s,%d: Error sending message. Sent %d of %d bytes", __FILE__, __LINE__, nsend, msg.size());
		}
		else if(nsend == -1)
		{
			ats_logf(ATSLOG(0), "%s,%d: Error sending message. Error code: %d - %s", __FILE__, __LINE__, errno, strerror(errno));
		}
		return false;
	}
	return true;
}

int PacketizerCellSender::waitforack()
{
	if (m_timeout == 0)	// we do not wait for acks.
		return 1;

	struct timespec ts;
	int ack = 0;

	//ATS-FIXME: Switch to use TimerEvent instead of sem_timedwait to use CLOCK_MONOTONIC instead of CLOCK_REALTIME
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += m_timeout;
	ts.tv_nsec = 0;
	lock();
	m_ack = 0;
	unlock();
	int err = sem_timedwait(&m_ack_sem, &ts);

	if(err == -1)
	{
		if (errno == ETIMEDOUT)
		{
			ats_logf(ATSLOG(0), "%s,%d : Timeout by sem_timedwait() - waited %d seconds.", __FILE__, __LINE__, m_timeout);
			return 0;
		}
		ats_logf(ATSLOG(0), "%s,%d : Error returned by sem_timedwait() - %d:%s", __FILE__, __LINE__, errno, strerror(errno));
		return -1;
	}
	lock();
	ack = m_ack;
	unlock();

	if(ack > 0)
	{
		return ack +1;
	}
	return 1;
}

//-------------------------------------------------------------------------------------------------
//	processMessage() - process an incoming CALAMP message
//	 - sends data to appropriate socket if it is not for this device.
//

//-------------------------------------------------------------------------------------------------
void* PacketizerCellSender::processUdpData(void* p)
{
	struct ServerData& sd = *((struct ServerData*) p);
	for(;;)
	{
		std::vector<unsigned char> message_buf;
		char buf[1024];
		struct sockaddr_in src_addr;
		socklen_t addrlen = sizeof(src_addr);
		const ssize_t nread = recvfrom(sd.m_udp->m_sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&src_addr, &addrlen);

		if(!nread)
		{
			//Orderly shutdown
			break;
		}

		if(nread < 0)
		{
			//Error
			break;
		}
		ats_logf(ATSLOG(0), " %s,%d: --[UDP data from %s]--", __FILE__, __LINE__,inet_ntoa(src_addr.sin_addr));
		std::stringstream ss;
		ss << "RECV[" << nread << "]:";

		for(ssize_t i = 0; i < nread; ++i)
		{
			ss << std::hex <<	std::setfill('0') << std::setw(2) << std::uppercase << (buf[i] & 0xff) << ',';
		}

		ats_logf(ATSLOG(0), "%s, %d: %s", __FILE__, __LINE__, ss.str().c_str());
//		message_buf.append(buf, nread);

		//process Message
//		processMessage(message_buf, sd);
	}
	return 0;
}

//-------------------------------------------------------------------------------------------------
void PacketizerCellSender::keepAlive()
{
	char data = 0;
	sendUdpData(m_sd, &data, 1);
}

//-------------------------------------------------------------------------------------------------
void PacketizerCellSender::connect()
{
	ats_logf(ATSLOG(0), "Connected to %s", m_host.c_str());
}

//-------------------------------------------------------------------------------------------------
void PacketizerCellSender::reconnect()
{
}

//-------------------------------------------------------------------------------------------------
void PacketizerCellSender::disconnect()
{
	ats_logf(ATSLOG(0), "%s,%d: Disconnected from %s", __FILE__, __LINE__, m_host.c_str());
}

//-------------------------------------------------------------------------------------------------
void PacketizerCellSender::BroadcastToSecondary(std::vector<unsigned char> &data)
{
	try
	{
		std::string strIP = GetInterfaceAddr("eth0");
		std::string strIPBcast = ReplaceLastOctet(strIP, 255);
		boost::system::error_code error;
		boost::asio::io_service io_service;
		boost::asio::ip::udp::socket socket(io_service);

		socket.open(boost::asio::ip::udp::v4(), error);
		
		if (!error)
		{
			socket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
			socket.set_option(boost::asio::socket_base::broadcast(true));

			boost::asio::ip::udp::endpoint senderEndpoint(boost::asio::ip::address::from_string(strIPBcast.c_str()), m_BroadcastPort);						
			socket.send_to(boost::asio::buffer(data), senderEndpoint);
			socket.close(error);
			ats_logf(ATSLOG(0), "%s,%d: Broadcast data on ip %s:%d -> sent %d bytes", __FILE__, __LINE__, strIP.c_str(), m_BroadcastPort, data.size());
		}
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
}


