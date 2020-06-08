#pragma once
#include "ats-common.h"

class MyData;
class RegisteredLogger;

typedef std::map <const ats::String, RegisteredLogger*> RegisteredLoggerListMap;
typedef std::pair <const ats::String, RegisteredLogger*> RegisteredLoggerListPair;

typedef std::map <const ats::String, RegisteredLoggerListMap> RegisteredLoggerMap;
typedef std::pair <const ats::String, RegisteredLoggerListMap> RegisteredLoggerPair;

class RegisteredLogger
{
public:
	RegisteredLogger(MyData& p_md) : m_md(p_md)
	{
		m_des = m_key = &ats::g_empty;
	}

	virtual~ RegisteredLogger();

	virtual bool send_log(const ats::String& p_log) = 0;

	bool active() const
	{
		return (m_des != &ats::g_empty) && (m_key != &ats::g_empty);
	}

	MyData& m_md;
	const ats::String* m_des;
	const ats::String* m_key;
};
