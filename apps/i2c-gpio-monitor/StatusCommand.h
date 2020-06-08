#pragma once

#include "ats-common.h"
#include "ats-string.h"
#include "socket_interface.h"
#include "command_line_parser.h"

class MyData;

class StatusCommand : public Command
{
public:

	virtual void fn(MyData& p_md, ClientData& p_cd, const CommandBuffer&)
	{
		ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "%s", ats::toStr(p_md).c_str());
	}

};
