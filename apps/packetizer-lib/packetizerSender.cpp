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

	return true;
}

bool PacketizerSender::sendMessage(send_message& msg)
{
	return true;
}

int PacketizerSender::waitforack(send_message& msg)
{
	return 0;
}

void PacketizerSender::connect()
{

}

void PacketizerSender::reconnect()
{
	disconnect();
	connect();
}

void PacketizerSender::disconnect()
{
}
