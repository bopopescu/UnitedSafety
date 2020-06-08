#pragma once

//ProcIDs are used to identify which sub-process port or proc slot a
// sub-process is being assigned to.  Sub processes will  be called with a ProcID as
// an argument.  Procs will be assigned to PID_PROC1/2,
// RTU devices can be set to one of the two RTU ports PID_RTU1/2, etc.
//
// The ProcIDs are also used to block access to the VFD to a specific device.
// If a sub-process needs the VFD exclusively it can set the VFD IPC value to
// its ProcID until it frees the VFD.  This will prevent screen overwrites by
// other sub-processes when a sub-process is in its SETUP etc.

enum PROC_ID
{
  PID_NONE, // used when there is no block.
  PID_MAIN,
  PID_AFF,
  PID_VFD,
  PID_NMEA,
  PID_PROC1,
  PID_PROC2,
  PID_PROC_STARTUP,
  PID_RTU1,
  PID_RTU2,
  PID_SER1,
  PID_SER2,
  PID_SER3,
  PID_UNKNOWN  // used to catch improperly set values
};
