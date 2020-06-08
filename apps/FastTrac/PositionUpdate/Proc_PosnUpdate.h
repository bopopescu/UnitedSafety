#pragma once

// Sends a position report and incrementing index to the AFF port
//  If a vehicle moves more than the distance parm - send a point
//  if a vehicle hasn't sent a point in more than the time parm seconds - send a point
//  if a vehicle course changes by more than the heading parm - send a point
//  If a vehicle stops - pin the position
//  if a vehicle is stopped - every time a time point is sent double the delta time
//  A vehicle is not moving until 3 points over the stop vel parm have been received
//  Dropping below the stop Vel parm before 3 updates leaves the pinned position as it was
//  Stopping and starting do not cause a position output
//
//#include <QtCore/QCoreApplication>
#include "FTData_Debug.h"
#include "AFS_Timer.h"
#include "NMEA_Client.h"
#include "DeviceBase.h"
#include "PU_Parms.h"
#include "accum.h"
#include "RedStone_IPC.h"

class Proc_PosnUpdate : public DeviceBase
{
private:
  enum PU_STATE
  {
    STARTUP,
    PINNED,  // below vel cutoff but not long enough to trigger STOPPED
    STOPPED, // below vel cutoff long enough
    STOPPED_AND_MOVING,  // above vel cutoff but not long enough to trigger MOVING from STOPPED
    MOVING,  // above vel cutoff long enough to trigger MOVING
    PINNED_AND_MOVING,  //  above vel cutoff but not long enough to trigger MOVING from PINNED
    JUST_SENT,
  };

  FTData_Debug myEvent;

//  FTDATA_DRIVER_HEALTH selftest;  //status of driver
public:
  PU_Parms myParms;
  AFS_Timer theTimer;  // reset every time an update is sent.
  AFS_Timer m_stoppedTimer;  // reset every time we stop - used to send out 2 minute stop interval.
  AFS_Timer m_JustSentTimer;
  AFS_Timer m_RealTimeTimer;
  long nextTime;
  NMEA_DATA lastSentGPS, thisGPS, pinnedGPS, lastGPS;
  NMEA_DATA  m_prevGPS;  // this is the previous seconds gps info - will be sent if epoch-to-epoch heading change is > 45
  short m_timeMultiplier; // doubles every time a position is sent out when pinned.
  bool m_bPinned;   // true if stop detected
  bool m_bStopped;  // true if stopped for myParms.StopTime()
  short m_PingReason;
  short m_UserData;
  PU_STATE m_State;
  Accum m_accHdg;
  Accum m_accDist;
  REDSTONE_IPC m_RedStoneData;

  Proc_PosnUpdate();
  ~Proc_PosnUpdate();

  void ProcessEverySecond();
  void  Setup();  // setup the device - read parms, verify parms etc
  bool VerifyParms();

protected:
  void DumpParms();
private:
  bool CheckPosition(NMEA_DATA & lastGPS, NMEA_DATA &thisGPS, AFS_Timer &t);
  void SendPosition( NMEA_DATA & lastSentGPS, NMEA_DATA &thisGPS, AFS_Timer &t);
  double QuickDistance(double lat1, double lon1, double lat2, double lon2);
  
  void State_STARTUP();
  void State_WaitForFirstPosition();
  bool SendEnabled();  // can we send data out right now?
	
	void SendMessage(std::string data);
	

};

