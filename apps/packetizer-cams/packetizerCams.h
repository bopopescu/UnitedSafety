#pragma once

#include "packetizer.h"
#include "packetizerCellSender.h"

struct iridium_thread_data
{
	message_info* mi;
};

struct cell_thread_data
{
	message_info* mi;
	PacketizerCellSender* cs;
	std::map<int,int>* msg_types;
};
