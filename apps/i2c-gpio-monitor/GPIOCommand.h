#pragma once
#include "socket_interface.h"
#include "command_line_parser.h"
#include "ats-common.h"

class GPIOCommand : public Command
{
public:

	GPIOCommand()
	{
	}

	virtual void fn(MyData& p_md, ClientData& p_cd, const CommandBuffer& p_cb)
	{

		if(p_cb.m_argc < 2)
		{
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "ok\n");
			ats::String label1;
			ats::String label2;
			ats_sprintf(&label1, "GPIO Expander 1 (0x%X)", p_md.m_exp_addr[0]);
			ats_sprintf(&label2, "GPIO Expander 2 (0x%X)", p_md.m_exp_addr[1]);

			display_gpio_expander_status(p_cd, label1, p_md.m_gpio->get_expander(p_md.m_exp_addr[0]));
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "\n");
			display_gpio_expander_status(p_cd, label2, p_md.m_gpio->get_expander(p_md.m_exp_addr[1]));
		}
		else
		{
			const ats::String& cmd = p_cb.m_argv[1];

			if("set" == cmd)
			{ // set <expander> <byte> <pin> <value>
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "ok\n");

				if(p_cb.m_argc < 6)
				{
					ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "usage: %s %s <expander> <byte> <pin> <value> [owner]\n", p_cb.m_argv[0], p_cb.m_argv[1]);
				}
				else
				{
					const int exp = strtol(p_cb.m_argv[2], 0, 0);
					const int byte = strtol(p_cb.m_argv[3], 0, 0);
					const ats::String owner((p_cb.m_argc >= 7) ? p_cb.m_argv[6] : "");
					write_expander(
						p_md,
						(p_cb.m_argc >= 7) ? &owner : 0,
						exp,
						byte,
						strtol(p_cb.m_argv[4], 0, 0),
						strtol(p_cb.m_argv[5], 0, 0));
					write_expander_hw(p_md, exp, byte);
				}

			}
			else if("get" == cmd)
			{

				if(p_cb.m_argc < 4)
				{
					ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "usage: %s %s <expander> <byte> [pin] [owner]\n", p_cb.m_argv[0], p_cb.m_argv[1]);
				}
				else
				{

					if(p_cb.m_argc >= 6)
					{
						const int exp_addr = p_md.m_exp_addr[strtol(p_cb.m_argv[2], 0, 0) ? 1 : 0];
						p_md.lock();
						int pin = strtol(p_cb.m_argv[4], 0, 0);

						if(!(((pin >= 0) && (pin <= 7))))
						{
							pin = 0;
						}

						const ats::String owner(p_cb.m_argv[5]);
						const GPIO* gv = p_md.m_gpio->get_gpio(exp_addr, strtol(p_cb.m_argv[3], 0, 0));
						const GPIOContext* gc = ((pin >= 0) && (pin < 8)) ? gv[pin].get_context_by_owner(&owner) : 0;
						const int state = gc ? gc->m_state : 0;
						p_md.unlock();

						if(gc)
						{
							ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "value(\"%s\",%p): %s\n", p_cb.m_argv[5], gc, state ? "ON" : "OFF");
						}
						else
						{
							ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "Error, cannot get requested GPIO\n");
						}

					}
					else
					{
						int n;

						if(read_expander_hw(
							p_md,
							strtol(p_cb.m_argv[2], 0, 0),
							strtol(p_cb.m_argv[3], 0, 0),
							n))
						{
							ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "Error reading from expander %s, byte %s\n", p_cb.m_argv[2], p_cb.m_argv[3]);
						}
						else
						{

							if(p_cb.m_argc >= 5)
							{
								n = (n & (1 << (strtol(p_cb.m_argv[4], 0, 0))));
								ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "value: %s\n", n ? "ON" : "OFF");
							}
							else
							{
								ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "value: 0x%02X\n", (unsigned int)n);
							}

						}

					}

				}

			}
			else
			{
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "error\nInvalid GPIO command \"%s\"\n", cmd.c_str());
			}

		}

	}

	void display_gpio_expander_status(ClientData& p_cd, const ats::String& p_label, const GPIOExpander& p_exp)
	{
		ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "%s:\n", p_label.c_str());

		size_t i = 0;
		int n = 0;

		while(i < p_exp.size())
		{
			const GPIO& g = p_exp.m_gpio[i++];

			const bool msb_pin_for_byte = !(n - ((n/8)*8));
			const int pin = 7 - (n - ((n/8)*8));

			if(msb_pin_for_byte)
			{
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "%s  [Byte %d]:\n", (n / 8) ? "\n" : "", (n / 8));
			}

			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "    |\n");
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "    +->pin %d:\n", pin);

			GPIO::GPIOVector::const_iterator j = g.m_stack.begin();

			while(j != g.m_stack.end())
			{
				GPIOContext& gi = *(*j);

				if(j == g.m_stack.begin())
				{
					ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "    %s  |\n",
						(pin && (i != p_exp.size())) ? "|" : " ");
				}

				++j;

				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "    %s  +->owner=%s, priority=%d, state=%s\n",
					(pin && (i != p_exp.size())) ? "|" : " ",
					gi.m_owner ? (gi.m_owner)->c_str() : "", gi.m_priority, gi.m_state ? "ON" : "OFF");
			}

			++n;
		}

	}

};
