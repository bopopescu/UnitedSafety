#pragma once

// this takes in the output feed from the RTU and decodes
// each individual channel

#include "ats-common.h"
#include "SERDevice.h"
#include "AFS_Timer.h"
#include "BufferedFile.h"
#include "NMEA_Client.h"

class GPS_Parms
{
public:
  int myBaudRateIndex;  // index into NMEABaudRate array
  int mybIsLogging;  // 1 if logging on
  int mytLogRate;    // log every tLogRate seconds
  int myLogType;     // 0:CSV, 1:KML, 2:Both

  GPS_Parms() : myBaudRateIndex(B115200),  mybIsLogging(false), mytLogRate(5), myLogType(0){}
};

class SER_GPS : public SerDevice
{
public:
  SER_GPS();
  virtual ~SER_GPS();

  void SetPort(short RTUPort);  // setup and open the port
  void ConfigurePort(int);  //configure GPS port
  bool ProcessIncomingData();
  void ProcessEverySecond();  // look for no data coming in.

  bool VerifyParms();
  void ReopenPort();

private:
  GPS_Parms myParms;  // parameters for logging and baud rates
  AFS_Timer myTimer, m_NoDataTimer, myUpdateSysTimeTimer;
  AFS_Timer m_UpdateIMEITimer;
  AFS_Timer m_UpdateInputsTimer;
  ats::String  m_IMEIString, m_InputString;
  
  unsigned long m_nCharsRcvd;
//  BufferedFile myCSVFile;
//  KMLBufferedFile myKMLFile;
  bool m_bNoGPS;  // true if no GPS is coming in.
  bool m_bLogging;
  bool m_sendGPS;
  bool m_bReadyToSendID;  // ready to send ID out port 
  bool m_bReadyToSendInputs;  // ready to send Inputs out port 

private:

  void WriteLogFile();

  void CheckSystemTime(); // checks the GPS time and date against the system time and date

  void WriteKMLRecord(FILE *fp);

  ats::String m_Type;

  AFS_Timer m_IncomingDataTimer;  // SelfTest period
};

