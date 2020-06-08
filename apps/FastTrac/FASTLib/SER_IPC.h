#pragma once
// SER interprocess class definition
//
// A SER latches to a port, reading and interpreting the incoming data.
// It can have a setup but it usually require no other interaction
// from the main task.	It may access the VFD and it may access the GPS and AFF SEResses.
//
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <unistd.h>
#include "ProcID.h"

enum SER_UPDATE_TYPE
{
	SER_UPDATE_NONE, // set to this after the SER program uses the data
	SER_UPDATE_SETUP, // enter the SER setup
	SER_UPDATE_EXIT	// exit the program - if the SER is changed or on exit from main task.
};

struct SER_IPC_DATA
{
	SER_UPDATE_TYPE type;
};


class SER_IPC
{
	SER_IPC_DATA *ipc;
	PROC_ID pid;
	char SER_ipc_name[16];

	boost::interprocess::shared_memory_object * shdmem;
	boost::interprocess::mapped_region * region;

public:
	SER_IPC(PROC_ID pid)
	{
		this->pid = pid;

		if (pid == PID_SER1)
			strcpy(SER_ipc_name, "AFS_SER1");
		else if (pid == PID_SER2)
			strcpy(SER_ipc_name, "AFS_SER2");
		else
			strcpy(SER_ipc_name, "AFS_SER3");

		// allocate the shared memory for AFF_Client_IPC
		shdmem = new boost::interprocess::shared_memory_object (boost::interprocess::open_or_create, SER_ipc_name, boost::interprocess::read_write, boost::interprocess::permissions(0666));
		shdmem->truncate(sizeof(SER_IPC_DATA));
		region = new boost::interprocess::mapped_region (*shdmem, boost::interprocess::read_write);
		ipc = static_cast<SER_IPC_DATA *>(region->get_address());
	}
	
	~SER_IPC()
	{
		delete region;
		delete shdmem;
	}

	SER_UPDATE_TYPE Type() const { return ipc->type;}
	void Type(const SER_UPDATE_TYPE t) {ipc->type = t;}
};
