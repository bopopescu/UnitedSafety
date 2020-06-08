#pragma once
#include "RegisteredLogger.h"


class RegisteredLogger_socket : public RegisteredLogger
{
public:
	RegisteredLogger_socket(MyData& p_md, int p_sockfd) : RegisteredLogger(p_md)
	{
		m_sockfd = p_sockfd;
	}

	virtual~ RegisteredLogger_socket(){}

	virtual bool send_log(const ats::String& p_log);

	int m_sockfd;
};
