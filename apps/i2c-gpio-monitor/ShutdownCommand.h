#pragma once

#include "ats-common.h"
#include "ats-string.h"
#include "socket_interface.h"
#include "command_line_parser.h"

class MyData;

class ShutdownCommand : public Command
{
public:

	virtual void fn(MyData& p_md, ClientData& p_cd, const CommandBuffer&)
	{
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=shutdown.g led=gps,cell,wifi,sat,inp6,zigbee script=\"1,1000000;0,10000000\" priority=30\r");
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=shutdown.r led=gps.r,cell.r,wifi.r,sat.r,inp6.r,zigbee.r script=\"1,1000000;0,10000000\" priority=30\r");
	}

};
