#pragma once

#include "socket_interface.h"
#include "command_line_parser.h"

class MyData;

class Command
{
public:
	virtual void fn(MyData&, ClientData&, const CommandBuffer&) = 0;
};
