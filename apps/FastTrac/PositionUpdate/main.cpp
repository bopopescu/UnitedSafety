// main for PositionUpdate Black Box.
// This program takes position data from an NMEA parser
// and determines whether a new position output is required
// to meet the specifications laid out in a parameter file.
// A new position is output when any of the parameters are exceeded
//
// The parameter file is an XML file containing the following:
//   <PositionUpdateParms>
//     <Time units=seconds>60</Time>
//     <Distance units=metres>500</Distance>
//     <Heading units=degrees>15</Heading>
//     <Pinning>On</Pinning>
//     <Stop units=kph>4</Stop>
//   </PositionUpdateParms>
//
// Note: zeroes in the Time, Distance or Heading parameters means that they are not to
//       be used as a criteria for output generation.
//
//
// External events can be added as a filter reset so that if a position goes
// back with an event that position is used as the last sent record.
//
// Stopping will cause the unit to output at a slower rate which will slow down more as
// time goes on.
//
// Data from the NMEA source will be accessed through IPC and shared memory.
// Data from the Event source will be accessed through IPC and shared memory.
// Output data from this task will be accessed through IPC and shared memory.
//
// This task is not concerned with sending the data out (no compression, etc)

// basic file operations
#include <iostream>

using namespace std;
#include "Proc_PosnUpdate.h"
#include "PROC_IPC.h"
#include "atslogger.h"

ATSLogger g_log;
AFS_Timer g_ValidGPSTimer;

int main(int argc, char* argv[])
{

  if (argc != 1)
  {
    cout << "Usage: PositionUpdate" << endl;
    cout << "             Exiting." << endl;
    return 0;
  }
  
  int dbg_level;
	{  // scoping is required to force closure of db-config database
		db_monitor::ConfigDB db;
		dbg_level = db.GetInt(argv[0], "LogLevel", 0);  // 0:Error 1:Debug 2:Info
	}
	
	g_log.set_global_logger(&g_log);
	g_log.set_level(dbg_level);

  g_log.open_testdata("PositionUpdate");
  g_ValidGPSTimer.SetTime();
  ats_logf(ATSLOG_ERROR,"----------- Starting PositionUpdate ----------");

  PROC_IPC ipc(PID_PROC2);
  ipc.SetType(PROC_UPDATE_NONE);

  bool done = false;
  Proc_PosnUpdate track;

  while (!done)
  {

    switch (ipc.Type())
    {
      case PROC_UPDATE_SETUP:
        track.Setup();
        break;
      case PROC_UPDATE_EXIT:  // exit the program - if the proc is changed or on exit from main task.
        done = true;
        break;

      default:
        break;
    }

    track.ProcessEverySecond();
    ipc.SetType(PROC_UPDATE_NONE);
    const int five_hz_polling = 200 * 1000;
    usleep(five_hz_polling);
  }
}
