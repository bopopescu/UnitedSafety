#pragma once

#include "ats-common.h"
#include "socket_interface.h"
#include "RedStone_IPC.h"
#include "packetizerSender.h"

class PacketizerDashSender : public PacketizerSender
{
public:
	PacketizerDashSender(MyData&);
	bool connect();
	void disconnect();
	bool sendMessage(send_message &msg);
	bool waitForAck(send_message &msg);
private:
	REDSTONE_IPC m_redstone;
	ats::String m_host;
	uint m_port;
	ClientSocket m_cs_sender;
	bool m_connected;
};
