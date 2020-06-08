#include <errno.h>
#include <stdlib.h>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <time.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "ats-common.h"
#include "atslogger.h"
#include "ConfigDB.h"
#include "RedStone_IPC.h"
#include "socket_interface.h"
//#include "messagetypes.h"
#include "utility.h"

#include "sender.h"

extern ATSLogger g_log;

ats::String PrimarySender::m_host;
int PrimarySender::m_port;
ats::String PrimarySender::m_defaultMobileId;
sem_t PrimarySender::m_ack_sem;
pthread_mutex_t PrimarySender::m_mutex;
uint PrimarySender::m_sequence_num;
ats::String PrimarySender::m_ack_type;
int PrimarySender::m_ack;
//-------------------------------------------------------------------------------------------------
PrimarySender::PrimarySender(MyData& pData): PacketizerSender(pData)
{
	sem_init(&m_ack_sem, 0 , 0);
	pthread_mutex_init(&m_mutex, NULL);
	m_sequence_num = 1;

	db_monitor::ConfigDB db;
	const ats::String app_name("packetizer-secondary");

	m_host = "0.0.0.0";
	m_port = db.GetInt("system", "PrimaryPort", 39002);
	m_HostPort = m_port;
	m_IMEI = db.GetValue("RedStone", "IMEI");
	m_timeout = db.GetInt(app_name, "timeout", 10);
	m_defaultMobileId = db.GetValue("RedStone", "IMEI");
	
	m_ack = 0;
}

//-------------------------------------------------------------------------------------------------
PrimarySender::~PrimarySender()
{}

//-------------------------------------------------------------------------------------------------
bool PrimarySender::start()
{
	init_ServerData(&m_sd, 1);
	m_sd.m_port = m_HostPort;
	m_sd.m_cs = WaitForMessage;
	start_udp_server(&m_sd)	;

	return true;
}




void*  PrimarySender::WaitForMessage(void *pData)
{
	struct ServerData& sd = *((struct ServerData*)pData);

	for(;;)
	{
		char buf[1024];
		struct sockaddr_in src_addr;
		socklen_t addrlen = sizeof(src_addr);
		ats_logf(&g_log, "%s,%d", __FILE__, __LINE__);
		const ssize_t nread = recvfrom(m_sd.m_udp->m_sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&src_addr, &addrlen);
		ats_logf(&g_log, "%s,%d", __FILE__, __LINE__);

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

		std::string cs = Checksum(buf);
		
		sendto(m_sd.m_udp->m_sockfd, cs.c_str(), cs.length(), 0, (struct sockaddr*) &src_addr, addrlen);
		send_redstone_ud_msg("message-assembler", 0, "%s\r", buf);

		ats_logf(ATSLOG(0), " %s,%d: --[UDP data from %s]--", __FILE__, __LINE__,inet_ntoa(src_addr.sin_addr));
		// output hex data to log

		std::stringstream ss;
		ss << "RECV[" << nread << "]:";

		for(ssize_t i = 0; i < nread; ++i)
		{
			ss << std::hex <<	std::setfill('0') << std::setw(2) << std::uppercase << (buf[i] & 0xff) << ',';
		}

		ats_logf(ATSLOG(0), "%s, %d: %s", __FILE__, __LINE__, ss.str().c_str());
	}
	return 0;
}


//-------------------------------------------------------------------------------------------------
void PrimarySender::connect()
{
	ats_logf(ATSLOG(0), "Connected to %s", m_host.c_str());
}

//-------------------------------------------------------------------------------------------------
void PrimarySender::reconnect()
{
}

//-------------------------------------------------------------------------------------------------
void PrimarySender::disconnect()
{
	ats_logf(ATSLOG(0), "%s,%d: Disconnect to Server", __FILE__, __LINE__);
}


//-------------------------------------------------------------------------------------------------
std::string PrimarySender::Checksum
(
	const char *msg
)
 {
	short i, blen;
	char check_sum = 0;

 	blen = strlen(msg);

	for (i = 0; i < blen; i++)
		check_sum ^= msg[i];

	char buf[8];
	sprintf(buf, "%d\r", check_sum);
	std::string strBuf(buf);
	return strBuf;
}

//-------------------------------------------------------------------------------------------------
void* PrimarySender::processUdpData(void* p)
{
/*
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
		ats_logf(ATSLOG(0), " %s,%d: --[UDP data from %s]--", __FILE__, __LINE__,inet_ntoa(src_addr.sin_addr));
		std::stringstream ss;
		ss << "RECV[" << nread << "]:";

		for(ssize_t i = 0; i < nread; ++i)
		{
			ss << std::hex <<	std::setfill('0') << std::setw(2) << std::uppercase << (buf[i] & 0xff) << ',';
		}

		ats_logf(ATSLOG(0), "%s, %d: %s", __FILE__, __LINE__, ss.str().c_str());
		message_buf.append(buf, nread);

		//process Message
		processMessage(message_buf, sd);
	}
*/
	return 0;
}

