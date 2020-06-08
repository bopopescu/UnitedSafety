#pragma once

#include <vector>

#include "socket_interface.h"

#include "packetizer.h"

void split(std::vector<ats::String> &p_list, const ats::String &p_s, char p_c);

class PacketizerSender
{
public:
	PacketizerSender(MyData&);
	~PacketizerSender();

	bool start();
	void connect();
	void disconnect();
	void reconnect();
	bool sendMessage(send_message& msg);
	int waitforack(send_message& msg);

private:
	MyData *m_data;

};

