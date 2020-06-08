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
#include "calampseventmessage.h"
#include "calampsusermessage.h"
#include "calampsackmessage.h"
#include "calampsmessage.h"
#include "messagetypes.h"
#include "utility.h"
#include "packetizerCellSender.h"

ats::String PacketizerCellSender::m_host;
int PacketizerCellSender::m_port;
ats::String PacketizerCellSender::m_defaultMobileId;
sem_t PacketizerCellSender::m_ack_sem;
pthread_mutex_t PacketizerCellSender::m_mutex;
uint PacketizerCellSender::m_sequence_num;
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
	m_sequence_num = 1;

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
bool PacketizerCellSender::packetizeMessageInfo(message_info* p_mi, std::vector<char>& p_data, std::map<int,int>& p_msgTypes)
{
	const int code = p_mi->sm.get_int("event_type");
fprintf(stderr, "%s,%d:%s: event_type=%d, TRAK_CALAMP_USER_MSG=%d\n", __FILE__, __LINE__, __FUNCTION__, code, TRAK_CALAMP_USER_MSG);
	CalAmpsMessage* msg;
	if(code == TRAK_CALAMP_USER_MSG)
	{
fprintf(stderr, "%s,%d:%s:\n", __FILE__, __LINE__, __FUNCTION__);
		msg = new CalAmpsUserMessage(p_mi->sm);
	}
	else
	{
fprintf(stderr, "%s,%d:%s:\n", __FILE__, __LINE__, __FUNCTION__);
		msg = new CalAmpsEventMessage(p_mi->sm, (quint8)p_msgTypes[code] );
		ats_logf(ATSLOG(0), "%s,%d: Read Event Message %d for seq num %d", __FILE__, __LINE__, p_msgTypes[code], p_mi->seq);
	}

	if(msg->getImei().empty())
	{
fprintf(stderr, "%s,%d:%s:\n", __FILE__, __LINE__, __FUNCTION__);
		ats_logf(ATSLOG(0), "%s,%d: IMEI is empty.	Exiting with code 1.", __FILE__, __LINE__);
		exit(1);
	}

	msg->setSequenceNumber(p_mi->seq);
	QByteArray data;
	msg->WriteData(data);
	p_data.reserve(data.size());
	p_data.insert(p_data.begin(), data.begin(),data.end());
	delete msg;

	{
		std::vector<char>::const_iterator i = p_data.begin();

		while(i != p_data.end())
		{
			printf("%02x", (unsigned int)(*i));
			++i;
		}

		printf("\n\t");
		i = p_data.begin();

		while(i != p_data.end())
		{
			printf("\\x%02x", (unsigned int)(*i));
			++i;
		}


		printf("\n");
	}

fprintf(stderr, "%s,%d:%s:\n", __FILE__, __LINE__, __FUNCTION__);
	return true;
}

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
	std::vector<char> write_data;

	if(!packetizeMessageInfo(p_mi, write_data, p_msgTypes))
	{
		return false;
	}

	ssize_t nsend = -1;

	nsend = sendUdpData(m_sd, &write_data[0], write_data.size());

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

	lock();
	m_sequence_num = p_mi->seq;
	unlock();
	return true;
}


//-------------------------------------------------------------------------------------------------
bool PacketizerCellSender::sendMessage(const send_message& msg)
{
	QByteArray write_data(&msg.data[0], msg.data.size());

	ssize_t nsend = -1;

	nsend = sendUdpData(m_sd, write_data.data(), write_data.size());

	if(nsend < write_data.length())
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
	lock();
	m_sequence_num = msg.seq;
	unlock();
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
void PacketizerCellSender::processMessage(QByteArray &data,ServerData &p_sd)
{
	int data_size = data.length();

	if(data_size < CALAMP_HEADER_MIN_SIZE)
	{
		return;
	}

	int option_header_offset = 0;
	const int message_header_offset = 4;

	if(data[option_header_offset] & CALAMP_OPTION_HEADER_DEFINE_BIT)
	{
		option_header_offset++;
		quint8 mask = 1;

		for(int i=0; i<6; i++)
		{
			if(data[0] & mask)
				option_header_offset += data[option_header_offset] + 1;

			if(data_size < (option_header_offset + message_header_offset + 1))
			{
				return;
			}

			mask = mask << 1;
		}

	}

	ats::String id(CalAmpsMessage::getImei(data).toUtf8().data());
	int msg_length = option_header_offset + message_header_offset;

	switch(data[option_header_offset + 1])
	{
	case CalAmpsMessage::MessageHeader::MessageType::USER_DATA:
		{
			const int user_data_length_msb_offset = option_header_offset + message_header_offset + 2;
			const int user_data_length_lsb_offset = option_header_offset + message_header_offset + 3;
			const int user_data_offset = 4;
			msg_length += user_data_offset + data[user_data_length_msb_offset] * 256 + data[user_data_length_lsb_offset];
			ats_logf(ATSLOG(0), "%s,%d: Calamp User Message Detected. Calculated size=%d. Actual Size=%d", __FILE__, __LINE__, msg_length, data.size());

			if (msg_length > data.length())
			{
				return;
			}

			sendAck(p_sd, data);
			break;
		}
		
	case CalAmpsMessage::MessageHeader::MessageType::ACK_NACK:
		{
			bool valid = false;

			lock();
			uint seq_number = m_sequence_num;
			unlock();
			const int seq_num_msb_offset = option_header_offset + 2;
			const int seq_num_lsb_offset = option_header_offset + 3;
			uint read_seq = data[seq_num_msb_offset] * 256 + data[seq_num_lsb_offset];

			if(seq_number == read_seq)
			{
				valid = true;
			}
			else
				ats_logf(ATSLOG(0), "%s,%d: Calamp User Ack is invalid seq_number=%d. read_seq=%d", __FILE__, __LINE__, seq_number, read_seq);


			if("standard" == m_ack_type)
			{
				const int ack_msg_length = 6;
				msg_length = option_header_offset + message_header_offset + ack_msg_length;

				if((msg_length) > data_size)
				{
					return;
				}

				if (valid)
				{
					lock();
					m_ack = data[option_header_offset + message_header_offset + 1];
					unlock();
				}

			}

			if(valid)
			{
				sem_post(&m_ack_sem);
				lock();
				m_sequence_num = 0;
				unlock();
			}
			break;
		}
		
		default:	// unrecognized as CALAMP
			data.clear();
			return;
	}

	// send data to other devices if it is not for this TRULink

	if( id != m_defaultMobileId)
	{
		//check db-config for a known attached device
		ats_logf(ATSLOG(0), "%s,%d: Mobile ID of message %s", __FILE__, __LINE__, id.c_str());
		db_monitor::ConfigDB db;
		ats::String socket = db.GetValue("devices", id);

		if(!socket.empty())
		{
			ats_logf(ATSLOG(0), "%s,%d: socket= %s, data:##%s##", __FILE__, __LINE__, socket.c_str(), data.toHex().data());
			send_redstone_ud_msg(socket.c_str(), 0, "calamp data=%s sender=packetizer-cams\r", data.toHex().data());
		}

		// also forward the data for broadcast across the network if needed
		
		if (m_bIsPrimary)
			BroadcastToSecondary(data);
	}

	data.remove(0, msg_length);
	//remove leading 0s
	int i;

	for(i = 0; (i < data.size()); i++)
	{

		if(data[i] != (char)0)
		{
			break;
		}

	}

	data.remove(0, i);
}

//-------------------------------------------------------------------------------------------------
void PacketizerCellSender::sendAck(ServerData& p_sd, QByteArray& msg)
{
	CalAmpsAckMessage *ack_msg = new CalAmpsAckMessage();
	int ret = CalAmpsAckMessage::fromData(ack_msg,msg);
	if(ret < 0 )
		return;
	QByteArray msgData;
	ack_msg->WriteData(msgData);
	ats_logf(ATSLOG(0), "%s,%d: Sending Ack", __FILE__, __LINE__);
	PacketizerCellSender::sendUdpData(p_sd, msgData.data(), msgData.size());
}


//-------------------------------------------------------------------------------------------------
void* PacketizerCellSender::processUdpData(void* p)
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
void PacketizerCellSender::BroadcastToSecondary(QByteArray &data)
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
			std::vector<unsigned char> c(data.constData(), data.constData() + data.size());
			socket.send_to(boost::asio::buffer(c), senderEndpoint);
			socket.close(error);
			ats_logf(ATSLOG(0), "%s,%d: Broadcast data on ip %s:%d -> sent %d bytes", __FILE__, __LINE__, strIP.c_str(), m_BroadcastPort, data.size());
		}
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
}


