// Usage: SER_GPS(PROC_SER1/2/3)
// This determines what IPC memory to look at.
//
#include <iostream>

#include <syslog.h>
#include <stdio.h>

#include "ProcID.h"  // the process ids
#include "string.h"
#include "AFS_Timer.h"
#include "J2K.h"
#include "SER_GPS.h"
#include "SER_IPC.h"
#include <CSelect.h>
#include "atslogger.h"
#include "socket_interface.h"
#include "command_line_parser.h"
#include "MyData.h"

ATSLogger g_log;


int main(int argc, char **argv)
{
	g_log.open_testdata("SER_GPS");

  ats_logf(&g_log,"Starting SER_GPS");

  SER_IPC ipc(PID_SER1);

  ipc.Type(SER_UPDATE_NONE);  // with built in sleep

  CSelect mSelect;
  bool done = false;
  AFS_Timer timer, resetTimer;

  SER_GPS dev;
  dev.ConfigurePort(0);
  dev.SetPort(0);

  mSelect.SetDelay(0, 50000); // delay for 50 milliseconds on read for data to show up
  mSelect.Add(dev.GetFD());


  while (!done)
  {
    ipc.Type(SER_UPDATE_NONE);  // with built in sleep!

    // now do the work
    if (mSelect.Select())
    {
			if (mSelect.HasData(dev.GetFD()))
			{
	      dev.ProcessIncomingData();  // will log if we fill up the buffer
	      resetTimer.SetTime();
			}
    }

    if (timer.DiffTimeMS() > 1000)
    {
      timer.SetTime();
      dev.ProcessEverySecond();  // will log data every 5 seconds if it hasn't filled up the buffer enough.
    }
    if (resetTimer.DiffTimeMS() > 10000)  // if no data after 10 seconds close and reopen the port
    {
		  ats_logf(&g_log, "SER_GPS: reconnecting to ttySER1");
    	resetTimer.SetTime();
    	dev.ReopenPort();
    	mSelect.Reset();
    	mSelect.Add(dev.GetFD());
    }

    usleep(50 * 1000);
  }
}
