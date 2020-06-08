#pragma once

#include <iostream>

#include <stdio.h>
#include <sys/time.h>

#include "ats-common.h"
#include "ats-string.h"

class MyData;

class Message
{
public:
	Message(){}

	Message(const ats::String& p_sender_id, const struct timeval& p_t, const ats::String& p_msg)
	{
		m_sender_id = p_sender_id;
		m_t = p_t;
		m_msg = p_msg;
	}

	ats::String m_sender_id;
	ats::String m_msg;
	struct timeval m_t;

	friend std::ostream& operator <<(std::ostream& p_o, const Message& p_msg)
	{
		char buf[32];
		snprintf(buf, sizeof(buf) - 1, "%ld.%06ld", p_msg.m_t.tv_sec, p_msg.m_t.tv_usec);
		buf[sizeof(buf) - 1] = '\0';
		return p_o << "[" << &p_msg << "," << p_msg.m_sender_id << "," << buf << "," << p_msg.m_msg.size() << "|" << ats::to_line_printable(p_msg.m_msg) << "]";
	}

};
