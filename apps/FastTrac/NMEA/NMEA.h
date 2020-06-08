#pragma once

#include "CharBuf.h"
#include "J2K.h"
#include "geoconst.h"
#include "NMEA_DATA.h"
#include <ConfigDB.h>  // gets the logging flag
#include "ats-common.h"
#include "AFS_Timer.h"

enum
{
  NMEA_OK,
  NMEA_NOT_NMEA,
  NMEA_BAD_CHECKSUM,
  NMEA_BAD_NMEA,
  NMEA_GGA_OK, 
  NMEA_RMC_OK,
  NMEA_EMPTY  // indicates ,,,, where data was expected.  
};

class NMEA
{
private:

  CharBuf  m_Buf;
  NMEA_DATA m_Data;
  int m_Err;  // indicates where a decode failed
  bool m_newRecord; // set to true to when an RMC time update is received. Used to alert system.
  bool m_bLogging;  // if true - log all incoming data - set by db-config NMEA Logging On/Off
  FILE *m_fpLog;  // file to log to.
  char m_strLogFile[128];  // file to log to.
  
  
  // the following allow for sending the NMEA stream out a specific port for live feed to a local CAMS
  AFS_Timer m_UpdateIMEITimer;  // We send out the IMEI and INPUT state every 10 seconds
  AFS_Timer m_LastEpochTimer;   // Time elapsed since last GGA message was decoded.
  ats::String m_IMEIString;     // a fixed string containing the IMEI in $PATSID,IMEI*CS format
  ats::String m_InputString;    // a string containing the Inputs status in $PATSIO,unix time,inputs in hex*CS format
  int m_sendGPSPort;            // the port we send to for gps-socket-server to get data
  bool m_bReadyToSendID;        // ready to send ID out port (set every 10 seconds)
  bool m_bReadyToSendInputs;    // ready to send Inputs out port - received notification from i2c-gpio-monitor that an input has changed
  bool m_sendGPS;               // flag for whether the output to local cams is being used

  bool m_isValid;
  short m_gps_quality;   

public:
  NMEA();
  
  ~NMEA()
  {
    CloseLog();
  }

  int Add(const char* buf, unsigned int nchars);

  NMEA_DATA &GetData();

  double Lat() const {return m_Data.ddLat;};
  void Lat(const double newVal){m_Data.ddLat = newVal;};

  double UTC() const {return m_Data.utc;};

  double Lon() const {return m_Data.ddLon;};
  void Lon(const double newVal){m_Data.ddLon = newVal;};

  short GPS_Quality() const {return m_Data.gps_quality;};
  short NumSVs() const {return m_Data.num_svs;};
  float HDOP() const {return m_Data.hdop;};
  double H_ellip() const {return m_Data.h;};
  double H_ortho() const {return m_Data.H;};
  void H_ortho(const double newVal) {m_Data.H = newVal;};
  double N_Geoid() const {return m_Data.N;};
  double DGPSAge() const {return m_Data.dgps_age;};
  short RefID() const {return m_Data.ref_id;};

  double COG() const {return m_Data.ddCOG;};
  void COG(const double newVal){m_Data.ddCOG = newVal;};

  double SOG() const {return m_Data.sog;};  // speed in knots
  void SOG( const double newVal){m_Data.sog = newVal;};

  void  SetTimeFromSystem();  // use the system time as the time stamp
  short Day() const {return m_Data.day;}
  short Month() const {return m_Data.month;};
  short Year() const {return m_Data.year;};
  short Hour() const {return m_Data.hour;};
  short Minute() const {return m_Data.minute;};
  double Seconds() const {return m_Data.seconds;};

  double HorizErr() const {return m_Data.HorizErr;};
  double VertErr() const {return m_Data.VertErr;};
  double SphereErr() const {return m_Data.SphereErr;};

  char *GetBasicPosStr(char *buf);         //  writes H:M:S,D/M/Y,LatDD,LonDD,H
  unsigned short GetBasicPosStr(FILE *fp); //  writes H:M:S,D/M/Y,LatDD,LonDD,H
  char *GetLogStr(char *buf); //  writes H:M:S,D/M/Y,LatDD,LonDD,H, COG, SOG, HDOP
  void GetLogStr(FILE *fp);   //  writes H:M:S,D/M/Y,LatDD,LonDD,H, COG, SOG, HDOP
  char *GetFullPosStr(char *buf, int maxLen);
  unsigned short GetFullPosStr(FILE *fp);
  char *GetTimeStr(char *buf, int maxLen);
  char *GetGarminErrorStr(char *buf, int maxLen);
  void Dump(){m_Buf.Dump();};

  short GetGMTOffsetFromLon();

  bool IsNewGPS() const {return m_newRecord;};
  void ClearNewGPS(){m_newRecord = false;};
  void SetNewGPS(){m_newRecord = true;};
  
  void OpenLog();
  void WriteLog(char *buf);
  void CloseLog();
  void AddNewInput();  // builds the PATSIO string when a new input is received.
  
private:

  short Decode(char *buf);
  short atohex(char c);
  short CheckChecksum( char buffer[] );
  short fragment( char *buf, char delimit_char,  char *ptr_array[],  short max_array );
  short DecodeUTC( char *utc_buf );
  short DecodeGGA ( char *pt[], short num );
  short DecodeGST ( char *pt[], short num );
  short DecodeVTG ( char *pt[], short num );
  short DecodeRMC( char *pt[], short num );
  short DecodePGRME( char *pt[], short num );
  short DecodePGRMZ( char *pt[], short num );

  short AddChecksum( char *msg, short lenBuf );
  
  void NewInput(){m_bReadyToSendInputs = true;};
  void SendGPSToPort(char *buf);
  ats::String GetDateStr();
};

