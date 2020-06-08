/*-----------------------------------------------------------------------------
  FILE: NMEA.C - Decoding NMEA Messages
-----------------------------------------------------------------------------*/
using namespace std;

#include "math.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "J2K.h"
#include "WriteDebug.h"
#include "angle.h"
#include "utility.h"
#include "NMEA_Client.h"
#include "ats-string.h"

//----------------------------------------------
// GetBasicPosStr
//  writes H:M:S,D/M/Y,LatDD,LonDD,H int buf
//
char *NMEA_Client::GetBasicPosStr(char *buf)
{
  if (Year() == 0)
    SetTimeFromSystem();

  sprintf(buf, "%02d:%02d:%02d, %d/%d/%d,  %.9f, %.9f, %.3f  ",
   Hour(), Minute(), (int)Seconds(), Day(), Month(), Year(), Lat(), Lon(), H_ortho());
  return buf;
}

//----------------------------------------------
// GetBasicPosStr
//  writes H:M:S,D/M/Y,LatDD,LonDD,H int buf
//
unsigned short NMEA_Client::GetBasicPosStr(FILE *fp)
{
  if (fp == NULL)
    return 0;

  if (Year() == 0)
    SetTimeFromSystem();

  return fprintf(fp, "%02d:%02d:%02d,%d/%d/%d,%.9f,%.9f,%.3f",
   Hour(), Minute(), (int)Seconds(), Day(), Month(), Year(), Lat(), Lon(), H_ortho());
}

//----------------------------------------------
// GetLogStr
//  writes H:M:S,D/M/Y,LatDD,LonDD,H, COG, SOG, HDOP
//
char *NMEA_Client::GetLogStr(char *buf)
{
  if (Year() == 0)
    SetTimeFromSystem();

  sprintf(buf, "%02d:%02d:%02d,%d/%d/%d,%.9f,%.9f,%.3f, %.1f,%.1f, %.1f",
   Hour(), Minute(), (int)(Seconds()), Day(), Month(), Year(), Lat(), Lon(), H_ortho(),
   COG(), SOG(), HDOP());
  return buf;
}

//----------------------------------------------
// GetLogStr
//  writes H:M:S,D/M/Y,LatDD,LonDD,H, COG, SOG, HDOP
//
void NMEA_Client::GetLogStr(FILE *fp)
{
  if (Year() == 0)
    SetTimeFromSystem();

  fprintf(fp, "%02d:%02d:%02d,%d/%d/%d,%.9f,%.9f,%.3f, %.1f,%.1f, %.1f",
   Hour(), Minute(), (int)(Seconds()), Day(), Month(), Year(), Lat(), Lon(), H_ortho(),
   COG(), SOG(), HDOP());
}

//--------------------------------------------------------------
// GetFullPosStr
//  writes out all the data into buf in a CSV format
//  writes H:M:S,D/M/Y,LatDD,LonDD,H int buf
//
//  Up to the user to make sure they have enough space to write into!
//  should be at least 100 bytes
//
char *NMEA_Client::GetFullPosStr(char *buf, int maxLen)
{
  char rbuf[256];
  LAT lat;
  LON lon;
  char strLat[24], strLon[24];

  lat.set_dd(Lat());
  lat.get_ndms(strLat, 24, 2, ' ');
  lon.set_dd(Lon());
  lon.get_ndms(strLon, 24, 2, ' ');

  if (Year() == 0)
    SetTimeFromSystem();

  sprintf(rbuf, "%02d:%02d:%02d,%d/%d/%d,%s,%s,%.3f,%.3f, %.1f,%.1f, %d,%d,%.1f,%.0f",
   Hour(), Minute(), (int)(Seconds()), Day(), Month(), Year(), strLat, strLon, H_ortho(), N_Geoid(),
   COG(), SOG(),
   GPS_Quality(), NumSVs(), HDOP(), DGPSAge());

  if ((int)strlen(rbuf) < maxLen)
    strcpy(buf, rbuf);
  else
    return GetBasicPosStr(buf);

  return buf;
}
//--------------------------------------------------------------
// GetGarminErrorStr
//  writes out all the data into buf in a CSV format
//  writes H:M:S,D/M/Y,LatDD,LonDD,H int buf
//
//  Up to the user to make sure they have enough space to write into!
//  should be at least 100 bytes
//

char *NMEA_Client::GetGarminErrorStr(char *buf, int maxLen)
{
  char rbuf[256];
  NMEA_DATA m_Data = ipc.GetData();

  sprintf(rbuf, "%.1f,%.1f,%.1f", m_Data.HorizErr, m_Data.VertErr, m_Data.SphereErr);

  if ((int)strlen(rbuf) < maxLen)
    strcpy(buf, rbuf);
  else
    strcpy(buf, "0.0,0.0,0.0");

  return buf;
}

char *NMEA_Client::GetTimeStr(char *buf, int maxLen)
{
  char rbuf[256];

  if (Year() == 0)
    SetTimeFromSystem();

  sprintf(rbuf, "%02d:%02d:%02d", Hour(), Minute(), (int)Seconds());

  if (strlen(rbuf) > 8)
    rbuf[8] = '\0';

  if ((int)strlen(rbuf) < maxLen)
    strcpy(buf, rbuf);
  else
    return GetBasicPosStr(buf);

  return buf;

}

// use the system time as the time stamp
void NMEA_Client::SetTimeFromSystem()
{
  ipc.Type(NMEA_UPDATE_TIME_FROM_SYSTEM);
}

short NMEA_Client::GetGMTOffsetFromLon() const
{
  short os = (short)(ipc.GetData().ddLon / 15.0);

  if (os > 12)
    os -= 24;

  if (os < -12 || os > 12)
    os = 0;  // don't really know what it is so GMT

  return (os);
}

std::ostream& operator <<(ostream& p_o, const NMEA_Client& p_c)
{
	p_o << "NMEA_Client: addr=" << &p_c << ", size=" << sizeof(p_c) << "\n";
	p_o << "Lat: " << p_c.Lat() << "\n";
	p_o << "Lon: " << p_c.Lon() << "\n";
	p_o << "UTC: " << p_c.UTC() << "\n";
	p_o << "GPS_Quality: " << p_c.GPS_Quality() << "\n";
	p_o << "NumSVs: " << p_c.NumSVs() << "\n";
	p_o << "HDOP: " << p_c.HDOP() << "\n";
	p_o << "H_ellip: " << p_c.H_ellip() << "\n";
	p_o << "H_ortho: " << p_c.H_ortho() << "\n";
	p_o << "N_Geoid: " << p_c.N_Geoid() << "\n";
	p_o << "DGPSAge: " << p_c.DGPSAge() << "\n";
	p_o << "RefID: " << p_c.RefID() << "\n";
	p_o << "COG: " << p_c.COG() << "\n";
	p_o << "SOG: " << p_c.SOG() << "\n";
	p_o << "Day: " << p_c.Day() << "\n";
	p_o << "Month: " << p_c.Month() << "\n";
	p_o << "Year: " << p_c.Year() << "\n";
	p_o << "Hour: " << p_c.Hour() << "\n";
	p_o << "Minute: " << p_c.Minute() << "\n";
	p_o << "Seconds: " << p_c.Seconds() << "\n";
	p_o << "HorizErr: " << p_c.HorizErr() << "\n";
	p_o << "VertErr: " << p_c.VertErr() << "\n";
	p_o << "SphereErr: " << p_c.SphereErr() << "\n";
	p_o << "IsValid: " << p_c.IsValid() << "\n";
	p_o << "GMTOffsetFromLon: " << p_c.GetGMTOffsetFromLon() << "\n";
	p_o << "IsNewGPS: " << p_c.IsNewGPS() << "\n";
	return p_o;
}
