#include <iostream>
#include <algorithm>
#include <set>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <pwd.h>

#include "ats-common.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "SocketInterfaceResponse.h"
#include "command_line_parser.h"
#include "buzzer-monitor.h"
#include "NMEA_Client.h"
#include "RedStone_IPC.h"
#include "geoconst.h"

#include "IridiumUtil.h"
unsigned short ComputeCheckSum(std::vector<char> &msg)
{
	unsigned short val = 0;

	for (size_t i = 0; i < msg.size(); i++)
		val += short(msg[i]);

	msg.push_back((char)((val >> 8) & 0xFF));
	msg.push_back((char)(val & 0xFF));

	return val;
}

int main(int argc, char* argv[])
{
	fprintf(stderr, "Enter applicaiton\n");
	static char tmp[] = {0x83, 0x08, 0x01,0x50,0x29,0x00,0x00,0x74,0x93,0x2F,0x01,0x02,0x01,0x81,0x00,0x01,0x5b};
	std::vector<char> m_iridium_data(tmp, tmp + sizeof(tmp)/sizeof(tmp[0]));
	ComputeCheckSum(m_iridium_data);
	IridiumUtil *m_iridium_util;
	m_iridium_util = new IridiumUtil();
	while(1)
	{
		fprintf(stderr, "Will send data\n");
		bool sent = m_iridium_util->sendMessage(m_iridium_data);
		if(sent)
		{
			fprintf(stderr, "send successful\n");
		}
		else
		{
			fprintf(stderr, "send fail\n");
		}
		sleep(30);
	}
	ats::infinite_sleep();
	return 0;
}
