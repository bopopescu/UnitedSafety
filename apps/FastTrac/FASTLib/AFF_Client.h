#pragma once

// AFF_Client - provides the main program and all of the Device Drivers
// with a common interface similar to the existing AFF interface

#include "AFF_IPC.h"
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>


class AFF_Client
{
	AFF_IPC ipc;

public:
	AFF_Client(){};	// constructor

	bool SendData(const char *data, short lenData)	// Send bytes out as a message
	{
		ipc.EventText(data, lenData);
	}

	void Setup(){ipc.Type(AFF_UPDATE_SETUP);}

};
