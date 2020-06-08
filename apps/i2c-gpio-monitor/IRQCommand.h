#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ats-common.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "MyData.h"

class IRQCommand : public Command
{
public:

	virtual void fn(MyData& p_md, ClientData& p_cd, const CommandBuffer& p_cb)
	{

		if(p_cb.m_argc < 2)
		{
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "ok\n");

			p_md.lock_irq();

			ats::StringMap::const_iterator i = p_md.m_irq_task.begin();

			while(i != p_md.m_irq_task.end())
			{
				const ats::String& irq_name = i->first;
				const ats::String& irq_task = i->second;
				++i;

				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "\t%s: %s\n", irq_name.c_str(), irq_task.c_str());
			}

			p_md.unlock_irq();

			return;
		}

		const ats::String cmd(p_cb.m_argv[1]);

		if("add" == cmd)
		{

			if(p_cb.m_argc < 4)
			{
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "error\nMust specify IRQ name and task\n");
				return;
			}

			ats::String task;

			for(int i = 3; i < p_cb.m_argc; ++i)
			{
				task += (task.empty() ? "" : " ") + ats::String(p_cb.m_argv[i]);
			}

			p_md.lock_irq();
			p_md.m_irq_task.set(p_cb.m_argv[2], task);
			p_md.unlock_irq();

			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "ok\n");
		}
		else if("del" == cmd)
		{

			if(p_cb.m_argc < 3)
			{
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "error\nMust specify IRQ name to delete\n");
				return;
			}

			p_md.lock_irq();
			p_md.m_irq_task.unset(p_cb.m_argv[2]);
			p_md.unlock_irq();

			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "ok\n");
		}
		else
		{
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "error\nInvalid IRQ command \"%s\"\n", cmd.c_str());
		}

	}

};
