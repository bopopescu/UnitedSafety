#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#include "socket_interface.h"
#include "ats-common.h"
#include "ConfigDB.h"
#include "atslogger.h"

ATSLogger g_log;

//-------------------------------------------------------------------------------------------------
// Calculates a simple 1-byte xor checksum and returns the value as a string
//		Doesn't change the input string.
//
static std::string Checksum
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
static void* my_udp_server(void* p)
{
	struct ServerData& sd = *((struct ServerData*)p);

	for(;;)
	{
		char buf[1024];
		struct sockaddr_in src_addr;
		socklen_t addrlen = sizeof(src_addr);
		ats_logf(&g_log, "%s,%d", __FILE__, __LINE__);

		const ssize_t nread = recvfrom(sd.m_udp->m_sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&src_addr, &addrlen);

		buf[nread] = '\0';  // terminate the char buffer properly.

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

    // send back the checksum as a string
		std::string cs = Checksum(buf);
		sendto(sd.m_udp->m_sockfd, cs.c_str(), cs.length(), 0, (struct sockaddr*) &src_addr, addrlen);
		
		// inject remote message into message assembler for handling by the packetizer.
		send_redstone_ud_msg("message-assembler", 0, "%s\r", buf);

		ats_logf(ATSLOG(0), " %s,%d: --[UDP data from %s]--Checksum is %s", __FILE__, __LINE__,inet_ntoa(src_addr.sin_addr), cs.c_str());
	}
	return 0;
}
//-------------------------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
	g_log.open_testdata(argv[0]);
	g_log.set_global_logger(&g_log);

	db_monitor::ConfigDB db;
	int port = db.GetInt("system", "PrimaryPort", 39002);
	ats_logf(ATSLOG(0), "%s STARTING:  PrimaryPort:%d", argv[0], port);
	
	// open the firewall
  char buf[256];
  sprintf(buf, "iptables -D INPUT -i eth0 -p udp --dport %d -j ACCEPT\niptables -I INPUT -i eth0 -p udp --dport %d -j ACCEPT\n", port, port);
  system(buf);

	ServerData sd;
	init_ServerData(&sd, 1);
	sd.m_port = port;
	sd.m_cs = my_udp_server;
	start_udp_server(&sd);

	ats::infinite_sleep();

	return 0;
}

