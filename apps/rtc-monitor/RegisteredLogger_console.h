#pragma once
#include "ats-common.h"

class RegisteredLogger_console : public RegisteredLogger
{
public:
	RegisteredLogger_console(MyData& p_md);

	virtual~ RegisteredLogger_console();

	virtual bool send_log(const ats::String& p_log);

private:
	FILE* m_f;
};
