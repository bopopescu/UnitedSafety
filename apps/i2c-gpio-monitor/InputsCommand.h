#pragma once
#include "ats-common.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "RedStone_IPC.h"
#include "GPIOPin.h"

extern REDSTONE_IPC* g_shmem;

class InputsCommand : public Command
{
public: 
	InputsCommand()
	{
	}
	
	virtual void fn(MyData& p_md, ClientData& p_cd, const CommandBuffer& p_cb)
	{
		int val;
		read_expander_hw(p_md, EXP_2, 0, val);

		if(g_shmem->GPIO() != val)
		{
			g_shmem->GPIO(val);
			send_redstone_ud_msg("message-assembler", 0, "msg sensor\r");
		}

	}
};
