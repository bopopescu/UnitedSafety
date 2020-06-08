#include <iostream>

#include "AFF_IPC.h"
#include "AFF_Email.h"
#include "NMEA_Client.h"
#include "CSelect.h"
#include "AFS_Timer.h"
#include "atslogger.h"

ATSLogger g_log;

extern std::string g_email;

int main(int argc, char **argv)
{
  g_log.open_testdata("AFF_Email");
  g_log.set_global_logger(&g_log);
  ats_logf(&g_log,"Starting AFF_Email");

  AFF_IPC ipc;
  AFF_Email aff;

  ats::StringMap arg;
  arg.from_args(argc - 1, argv + 1);
  arg.get_if_exists(g_email, "email");

  aff.OpenPort();
  ipc.HasNMEA(false);  // Email does not provide GPS output

  bool done = false;

//  AFS_Timer oneSecTimer;
  
  while (!done)
  {
    switch (ipc.Type())
    {
      case AFF_UPDATE_EVENT:
        aff.SendData(ipc.EventText(), ipc.EventLen());
        break;

      case AFF_UPDATE_SETUP:
        aff.Setup();
        break;

      case AFF_UPDATE_EXIT:
        done = true;
        break;

      default:
      break;
    }

    ipc.Type(AFF_UPDATE_NONE);
    ipc.IsSending(aff.IsSending()); // lets us know when we are finished
    //now look at the port

//    if (oneSecTimer.DiffTimeMS() > 1000)
//    {
//      oneSecTimer.SetTime();
      aff.Send_Queued_Messages_If_Any_Otherwise_Sleep();
//    }
  }
  return 0;
}
