#pragma once

#include "ats-common.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "Command.h"

class MyData;

class TestCommand : public Command
{
public:

	virtual void fn(MyData& p_md, ClientData& p_cd, const CommandBuffer& p_cb);

	static void start_test_server(MyData& p_md);

	static bool m_running_server;

	static const ats::String m_testmode;

	static void get_led_info(const ats::String& p_led_spec, int& p_exp, int& p_byte, int& p_pin);
};
