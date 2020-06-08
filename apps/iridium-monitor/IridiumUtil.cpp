#include "IridiumUtil.h"

IridiumUtil::IridiumUtil()
{
	init();
}

IridiumUtil::~IridiumUtil()
{
	if(m_cs)
	{
		disconnect();
		delete m_cs;
	}
}

void IridiumUtil::connect()
{
	connect_trulink_ud_client(m_cs, "iridium-monitor");
}

void IridiumUtil::disconnect()
{
	if(m_cs)
	{
		close_ClientSocket(m_cs);
	}
}

void IridiumUtil::init()
{
	m_cs = new ClientSocket;

	init_ClientSocket(m_cs);

	wait_for_app_ready("iridium-monitor");

	connect();
}

//298
bool IridiumUtil::getResponse(std::string &data )
{
	const int ret = send_cmd(m_cs->m_fd, MSG_NOSIGNAL, "response\r");
	if(ret<0)
	{
                reconnect();
		return false;	
	}
	ats::SocketInterfaceResponse ir(m_cs->m_fd);
	data = ir.get();
	return true;
}


bool IridiumUtil::isNetworkAvailable()
{
	const int ret = send_cmd(m_cs->m_fd, MSG_NOSIGNAL, "ready\r");

	if(ret < 0)
	{
		reconnect();
		//ats_logf(ATSLOG_DEBUG, "Could not send command 'ready'. Reconnecting..");
		return false;
	}

	ats::SocketInterfaceResponse ir(m_cs->m_fd);
	ats::String resp = ir.get();

	if((resp.find("yes") != std::string::npos) && (resp.find("ready") != std::string::npos))
	{
		return true;
	}

	return false;
}

bool IridiumUtil::sendMessageWDataLimit(std::vector<unsigned char> data)
{
	ats::String hexData = ats::to_hex(ats::String(data.begin(), data.end()));
	return sendMessageWDataLimit(hexData);
}

bool IridiumUtil::sendMessageWDataLimit(std::vector<char> data)
{
	ats::String hexData = ats::to_hex(ats::String(data.begin(), data.end()));
	return sendMessageWDataLimit(hexData);
}

bool IridiumUtil::sendMessageWDataLimit(ats::String hexData)
{
	const int ret = send_cmd(m_cs->m_fd, MSG_NOSIGNAL, "sendtdata %s\r", hexData.c_str());
	if(ret < 0)
	{
		reconnect();
		ats_logf(ATSLOG_DEBUG, "Could not send command send message with data throttle. Reconnecting..");
		return false;
	}

	int code;

	if(getResponseCode(code))
	{
		return isMessageSent(code);
	}
	return false;
}

bool IridiumUtil::sendMessage(std::vector<unsigned char> data)
{
	ats::String hexData = ats::to_hex(ats::String(data.begin(), data.end()));
	return sendMessage(hexData);
}

bool IridiumUtil::sendMessage(std::vector<char> data)
{
	ats::String hexData = ats::to_hex(ats::String(data.begin(), data.end()));
	return sendMessage(hexData);
}

bool IridiumUtil::sendMessage(ats::String hexData)
{
	const int ret = send_cmd(m_cs->m_fd, MSG_NOSIGNAL, "send %s\r", hexData.c_str());

	if(ret < 0)
	{
		reconnect();
		ats_logf(ATSLOG_DEBUG, "Could not send command send message. Reconnecting..");
		return false;
	}
	int code;
	if(getResponseCode(code))
	{
		return isMessageSent(code);
	}
	return false;
}

bool IridiumUtil::getResponseCode(int& code)
{
	ats::SocketInterfaceResponse ir(m_cs->m_fd);
	ats::String resp = ir.get();

	if(resp.empty())
	{
		ats_logf(ATSLOG_DEBUG, "Error getting response for send_message. Error %d", ir.error());
		return false;
	}
	size_t pos =0;
	std::string token;
	if ((pos = resp.find(":")) == std::string::npos)
	{
		return false;
	}
	if(resp.substr(0, pos).find("send") != std::string::npos)
	{
		resp.erase(0, pos + 1);
		if ((pos = resp.find(":")) != std::string::npos)
		{
			if(resp.substr(0, pos) == "ok")
			{
				resp.erase(0 , pos +1);
				if(!resp.empty())
				{
					code = (int)strtol(ats::rtrim_newline(resp).c_str(),0, 0);
					ats_logf(ATSLOG_DEBUG, "Response for send_message: %d", code);
					return true;
				}
			}
		}
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
// ComputeCheckSum: sum up the bytes as a short and adds it to the end of msg
//
unsigned short IridiumUtil::ComputeCheckSum(char *msg, short len)
{
	unsigned short val = 0;

	for (short i = 0; i < len; i++)
		val += short(msg[i]);

	msg[len] = (char)((val >> 8) & 0xFF);
	msg[len + 1] = (char)(val & 0xFF);
	msg[len + 2] = '\0';
	return val;
}

unsigned short IridiumUtil::ComputeCheckSum(std::vector<char> &msg)
{
	unsigned short val = 0;

	for (size_t i = 0; i < msg.size(); i++)
		val += short(msg[i]);

	msg.push_back((char)((val >> 8) & 0xFF));
	msg.push_back((char)(val & 0xFF));

	return val;
}

unsigned short IridiumUtil::ComputeCheckSum(std::vector<unsigned char> &msg)
{
	unsigned short val = 0;

	for (size_t i = 0; i < msg.size(); i++)
		val += short(msg[i]);

	msg.push_back((unsigned char)((val >> 8) & 0xFF));
	msg.push_back((unsigned char)(val & 0xFF));

	return val;
}
