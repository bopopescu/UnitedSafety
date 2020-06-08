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

#include <QTimer>
#include <QHostInfo>

#include "ats-common.h"
#include "atslogger.h"
#include "ConfigDB.h"
#include "RedStone_IPC.h"
#include "socket_interface.h"
#include "messagetypes.h"
#include "utility.h"

#include "packetizerSecondarySender.h"

ats::String PacketizerSecondarySender::m_host;
int PacketizerSecondarySender::m_port;
ats::String PacketizerSecondarySender::m_defaultMobileId;
sem_t PacketizerSecondarySender::m_ack_sem;
pthread_mutex_t PacketizerSecondarySender::m_mutex;
uint PacketizerSecondarySender::m_sequence_num;
ats::String PacketizerSecondarySender::m_ack_type;
int PacketizerSecondarySender::m_ack;
bool PacketizerSecondarySender::m_bIsPrimary;
int PacketizerSecondarySender::m_BroadcastPort;
int PacketizerSecondarySender::m_MsgChecksum;
//-------------------------------------------------------------------------------------------------
PacketizerSecondarySender::PacketizerSecondarySender(MyData& pData): PacketizerSender(pData)
{
	sem_init(&m_ack_sem, 0 , 0);
	pthread_mutex_init(&m_mutex, NULL);
	m_sequence_num = 1;

	db_monitor::ConfigDB db;
	const ats::String app_name("packetizer-secondary");

	int routeIP = db.GetInt("system", "RouteIP", 253);
	std::string cur_ip = GetInterfaceAddr( "eth0" );
	m_host = ReplaceLastOctet(cur_ip, routeIP);
	m_port = db.GetInt("system", "PrimaryPort", 39002);
	m_HostPort = m_port;
	m_IMEI = db.GetValue("RedStone", "IMEI");
	m_timeout = db.GetInt(app_name, "timeout", 10);
	m_defaultMobileId = db.GetValue("RedStone", "IMEI");
	m_BroadcastPort = db.GetInt("system", "BroadcastPort", 39003);
	
	m_ack = 0;
}

//-------------------------------------------------------------------------------------------------
PacketizerSecondarySender::~PacketizerSecondarySender()
{}

//-------------------------------------------------------------------------------------------------
bool PacketizerSecondarySender::start()
{

	init_ServerData(&m_sd, 1);
	m_sd.m_port = m_HostPort;
	m_sd.m_cs = processUdpData;
	start_udp_server(&m_sd)	;

	return true;
}



//-------------------------------------------------------------------------------------------------
ssize_t PacketizerSecondarySender::sendUdpData(ServerData& p_sd, const char *p_data, uint p_data_length)
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
bool PacketizerSecondarySender::SendMessage(const std::string & strMsg)
{
	m_MsgChecksum = Checksum(strMsg.c_str());
	
	std::vector<char> write_data(strMsg.begin(), strMsg.end());;
	ats_logf(ATSLOG(0), "%s,%d - size %d  string ##%s\n##\nChecksum=%d\n", __FILE__, __LINE__, write_data.size(), &write_data[0], m_MsgChecksum);

	ssize_t nsend = sendUdpData(m_sd, &write_data[0], write_data.size());

	if(nsend < (ssize_t)(write_data.size()))
	{
		if (nsend >= 0)
		{
			ats_logf(ATSLOG(0), "%s,%d: Error sending message. Sent %d of %d bytes", __FILE__, __LINE__, nsend, write_data.size());
		}
		else if(nsend == -1)
		{
			ats_logf(ATSLOG(0), "%s,%d: Error sending message. Error code: %d - %s", __FILE__, __LINE__, errno, strerror(errno));
		}
		return false;
	}
	return true;
}



int PacketizerSecondarySender::waitforack()
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
//	 - sends data to appropriate socket if it is not for this
//		 device.
//
void PacketizerSecondarySender::processMessage(QByteArray &data,ServerData &p_sd)
{
	int data_size = data.length();
  std::string strAck(data.constData());

	if (atoi(strAck.c_str()) == m_MsgChecksum)
	{
		sem_post(&m_ack_sem);
		return;
	}
  ats_logf(ATSLOG(0), "Checksum %d does not match %d", atoi(strAck.c_str()), m_MsgChecksum);
}



//-------------------------------------------------------------------------------------------------
void* PacketizerSecondarySender::processUdpData(void* p)
{
	struct ServerData& sd = *((struct ServerData*) p);
	for(;;)
	{
		QByteArray message_buf;
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
		ats_logf(ATSLOG(0), " %s,%d: --[UDP data from %s]-- %s", __FILE__, __LINE__,inet_ntoa(src_addr.sin_addr), buf);
		message_buf.append(buf, nread);

		//process Message
		processMessage(message_buf, sd);
	}
	return 0;
}

//-------------------------------------------------------------------------------------------------
void PacketizerSecondarySender::keepAlive()
{
	char data = 0;
	sendUdpData(m_sd, &data, 1);
}





short PacketizerSecondarySender::Checksum(const char *msg)
{
	short i, blen;
	char check_sum = 0;

  blen = strlen(msg);

	for (i = 0; i < blen; i++)
 	{
		check_sum ^= msg[i];
	}
	return check_sum;
}

