#pragma once

#include <iostream>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "ats-common.h"
#include "ats-string.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "MyData.h"
#include "Command.h"

class LEDCommand : public Command
{
public:

	LEDCommand()
	{
	}

	virtual void fn(MyData& p_md, ClientData& p_cd, const CommandBuffer& p_cb)
	{

		if(p_cb.m_argc < 2)
		{
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "ok\n");
			ats::StringMap::const_iterator i = p_md.m_led.begin();

			while(i != p_md.m_led.end())
			{
				const ats::String& led_name = i->first;
				const ats::String& led_pin = i->second;
				++i;

				ats::StringList list;
				ats::split(list, led_pin, ",");

				if(list.size() >= 3)
				{
					const int addr = strtol(list[0].c_str(), 0, 0);
					const int byte = strtol(list[1].c_str(), 0, 0);
					const int bit = strtol(list[2].c_str(), 0, 0);
					int n;

					if(read_expander_hw(p_md, addr, byte, n))
					{
						ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "\t%s=UNKNOWN (Error reading)\n", led_name.c_str());
					}
					else
					{
						n = (n & (1 << bit));
						ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "\t%s=%s\n", led_name.c_str(), n ? "ON" : "OFF");
					}

				}

			}

			return;
		}

		ats::StringMap m;
		m.from_args(p_cb.m_argc - 1, p_cb.m_argv + 1);

		ats::StringMap::const_iterator i = m.begin();

		bool error = false;

		while(i != m.end())
		{
			const ats::String& led_name = i->first;
			const ats::String& led_val = i->second;
			++i;
			const ats::String& pin = p_md.m_led.get(led_name);

			if(pin.empty())
			{
				error = true;
				ats::String msg(ats::String("No such LED \"") + led_name + "\"");
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "%s\n", msg.c_str());
			}
			else
			{
				ats::StringList list;
				ats::split(list, pin, ",");

				if(list.size() >= 3)
				{
					const int val = ats::get_bool(ats::tolower(led_val));
					const int expander = strtol(list[0].c_str(), 0, 0);
					const int byte = strtol(list[1].c_str(), 0, 0);
					const int pin = strtol(list[2].c_str(), 0, 0);
					set_led(p_md, NULL, expander, byte, pin, val);
				}

			}

		}

		if(!error)
		{
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "ok\n");
		}

	}

};
