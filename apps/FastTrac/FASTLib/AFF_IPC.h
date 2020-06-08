#pragma once
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
// AFF Interprocess class definition
//
// This defines all of the variables needed for sending data to the AFF
//
#include "string.h"
#include <unistd.h>
#include "ProcID.h"
#include "AFFDevice.h"

enum AFF_UPDATE_TYPE
{
	AFF_UPDATE_NONE, // set to this after the AFF program uses the data
	AFF_UPDATE_EVENT,
	AFF_UPDATE_SETUP,
	AFF_UPDATE_EXIT
};

struct AFF_IPC_DATA
{
	AFF_UPDATE_TYPE type;
	char eventText[MAX_AFF_BUF_SIZE];	// text from SendEvent
	short eventLen;
	bool bHasNMEA;	// true if the AFF device will provide GPS data
	bool bIsSending;	// true if a message is in the queue and hasn't been confirmed as sent
};

class AFF_IPC
{
	AFF_IPC_DATA *ipc;
	boost::interprocess::shared_memory_object * shdmem;
	boost::interprocess::mapped_region * region;

public:
	AFF_IPC()
	{
		bool init = false;
		// allocate the shared memory for REDSTONE_IPC_DATA
		try
		{
			shdmem = new boost::interprocess::shared_memory_object (boost::interprocess::open_only, "AFS_AFF", boost::interprocess::read_write);
		}
		catch(boost::interprocess::interprocess_exception e)
		{
			shdmem = new boost::interprocess::shared_memory_object (boost::interprocess::create_only, "AFS_AFF", boost::interprocess::read_write, boost::interprocess::permissions(0666));
			init = true;
		}
		shdmem->truncate(sizeof(AFF_IPC_DATA));
		region = new boost::interprocess::mapped_region (*shdmem, boost::interprocess::read_write);
		ipc = static_cast<AFF_IPC_DATA *>(region->get_address());
		if(init)
		{
			ipc->bIsSending = false;
		}
	}
	
	~AFF_IPC()
	{
		delete region;
		delete shdmem;
	}

	AFF_UPDATE_TYPE Type() const { return ipc->type;}
	void Type(const AFF_UPDATE_TYPE t) {ipc->type = t;usleep(10000);}	// Changing this will trigger the AFF to respond so we sleep to give the AFF task time to respond.


	void EventText(const char * text, short len) { memcpy(ipc->eventText, text, len); ipc->eventLen = len; }
	char * EventText()	{return ipc->eventText; }
	short EventLen() { return ipc->eventLen; }
	bool HasNMEA() { return ipc->bHasNMEA; }
	void HasNMEA(bool val){ipc->bHasNMEA = val;}
	bool IsSending() { return ipc->bIsSending; }
	void IsSending(bool val){ipc->bIsSending = val;}

	void SendData(const char * text, short len){EventText(text, len); Type(AFF_UPDATE_EVENT);}

};
