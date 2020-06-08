#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <fstream>
#include <iomanip>
#include <algorithm>

#include "packetizerSender.h"

#define TRAK_CONFIG_FILE "/var/config/ct_config.data"
#define TRAK_DEFAULT_CONFIG_FILE "/home/root/ct_config.data"

PacketizerSender::PacketizerSender(MyData& pData) :
	m_data(&pData) 
{
}

PacketizerSender::~PacketizerSender()
{
}


bool PacketizerSender::start()
{
	return true;
}

bool PacketizerSender::sendMessage(send_message& msg)
{
	return true;
}

int PacketizerSender::waitforack(send_message& msg)
{
	return 1;
}

void PacketizerSender::connect()
{
}

void PacketizerSender::disconnect()
{
}
