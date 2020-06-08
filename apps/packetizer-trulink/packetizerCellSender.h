#pragma once

#include <semaphore.h>
#include <pthread.h>
#include <vector>

#include "ats-common.h"
#include "RedStone_IPC.h"
#include "socket_interface.h"
#include "packetizer.h"

#include "packetizerSender.h"

#define CALAMPSERVER 1.1.1.1
#define CALAMP_HEADER_MIN_SIZE 5
#define CALAMPS_ACK_MESSAGE_MIN_SIZE 10
#define CALAMPS_MESSAGE_MAX_SIZE 848

typedef enum {
	CALAMP_PROCESS_MESSAGE_INCOMPLETE =0,
	CALAMP_PROCESS_MESSAGE_ACK =1,
	CALAMP_PROCESS_MESSAGE_USER = 4,
	CALAMP_PROCESS_MESSAGE_ERROR =-1
} CALAMP_PROCESS_MESSAGE_STATUS;

class PacketizerCellSender : public PacketizerSender
{
public:
	PacketizerCellSender(MyData &);
	~PacketizerCellSender();

	bool start();
	void connect();
	void disconnect();
	void reconnect();
	bool sendSingleMessage( message_info* mi,  std::map<int,int>&);
	bool sendMessage(std::vector <unsigned char> msg);
	int waitforack();
	void keepAlive();

private:
	MyData m_data;
	ServerData m_sd;
	static ats::String m_host;
	static sem_t m_ack_sem;
	static int m_ack;
	static int m_port;
	int m_HostPort;
	int m_timeout;
	static ats::String m_ack_type;
	static pthread_mutex_t m_mutex;
	REDSTONE_IPC m_RedStone;
	static ats::String m_defaultMobileId;

	// additions for NTPC like Primary/Secondary configurations
	static bool m_bIsPrimary;// is this in a network as a primary unit?
	static int m_BroadcastPort;
	
	// additions for NTPC like Primary/Secondary configurations
	void BroadcastToSecondary(std::vector<unsigned char> &data);

	bool packetizeMessageInfo(message_info* p_mi, std::vector<unsigned char>& data);
	bool packetizeMessageInfo(message_info* p_mi, std::vector<unsigned char>& p_data, std::string strIMEI);
//	static void processMessage(QByteArray &data, ServerData &p_sd);
	//*************************************************************************
	// sendUdpData()
	// Sends UDP data to the socket defined by p_sd
	//   p_sd          - is the server data that contains the socket to send the data
	//   p_data        - is the data to be sent
	//   p_data_length - is the data length
	//
	// Returns the number of bytes sent, -1 if there was a system error or -2 if
	// there was an issue resolving the host
	//***************************************************************************
	static ssize_t sendUdpData(ServerData& p_sd, const char * p_data, uint p_data_length);
	static void* processUdpData(void *p);
	static void lock() {pthread_mutex_lock(&m_mutex);}
	static void unlock() {pthread_mutex_unlock(&m_mutex);}
//	static void sendAck(ServerData& p_sd, QByteArray& msg);
};
