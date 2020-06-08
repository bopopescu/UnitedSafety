#pragma once
// PROC Interprocess class definition
//
// A proc basically runs its own program and usually outputs something at a
// regular interval.  It can have a setup but it usually require no other interaction
// from the main task.  It may access the VFD and it may access the GPS and AFF processes.
//
#include <unistd.h>
#include "ProcID.h"

#include "boost/interprocess/shared_memory_object.hpp"
#include "boost/interprocess/mapped_region.hpp"

enum PROC_UPDATE_TYPE
{
  PROC_UPDATE_NONE, // set to this after the PROC program uses the data
  PROC_UPDATE_SETUP, // enter the proc setup
  PROC_UPDATE_EXIT  // exit the program - if the proc is changed or on exit from main task.
};

struct PROC_IPC_DATA
{
  PROC_UPDATE_TYPE type;
};


class PROC_IPC
{
  PROC_IPC_DATA *ipc;
  PROC_ID pid;
  char proc_ipc_name[16];

  boost::interprocess::shared_memory_object * shdmem;
  boost::interprocess::mapped_region * region;

public:
  PROC_IPC(PROC_ID pid)
  {
    this->pid = pid;

    if (pid == PID_PROC1)
      strcpy(proc_ipc_name, "AFS_PROC1");
    else
      strcpy(proc_ipc_name, "AFS_PROC2");

    // allocate the shared memory for AFF_Client_IPC
    shdmem = new boost::interprocess::shared_memory_object (boost::interprocess::open_or_create, proc_ipc_name, boost::interprocess::read_write, boost::interprocess::permissions(0666));
    shdmem->truncate(sizeof(PROC_IPC_DATA));
    region = new boost::interprocess::mapped_region (*shdmem, boost::interprocess::read_write);
    ipc = static_cast<PROC_IPC_DATA *>(region->get_address());
  }

  PROC_UPDATE_TYPE Type() const { return ipc->type;}

  void Type(const PROC_UPDATE_TYPE& p_type) {ipc->type = p_type; usleep(5000);}  // Changing this will trigger the AFF to respond so we sleep to give the AFF task time to respond.

  void SetType(const PROC_UPDATE_TYPE& p_type) {ipc->type = p_type;}
};
