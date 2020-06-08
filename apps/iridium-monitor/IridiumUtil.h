#pragma once

#include "ats-common.h"
#include "atslogger.h"
#include "socket_interface.h"
#include "SocketInterfaceResponse.h"

#include "IridiumSender.h"

class IridiumUtil
{
public:
	IridiumUtil();
	~IridiumUtil();
	void init();
	bool sendMessageWDataLimit(std::vector<unsigned char> data);
	bool sendMessageWDataLimit(std::vector<char> data);
	bool sendMessage(std::vector<unsigned char> data);
	bool sendMessage(std::vector<char> data);
//298
	bool getResponse(std::string &data);
	bool isNetworkAvailable();

	bool isMessageSent(int resp)
	{
		if((resp <= 4) && (resp >= 0))
		{
			return true;
		}
		return false;
	}

	static unsigned short ComputeCheckSum(char *msg, short len);
	static unsigned short ComputeCheckSum(std::vector<char> &data);
	static unsigned short ComputeCheckSum(std::vector<unsigned char> &data);
private:
	bool getResponseCode(int& code);
	void connect();
	void disconnect();
	void reconnect(){disconnect();connect();}
	int m_fd;
	bool sendMessageWDataLimit(ats::String hexData);
	bool sendMessage(ats::String hexData);

	ClientSocket* m_cs;
};
