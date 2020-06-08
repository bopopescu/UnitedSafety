#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>

#include "socket_interface.h"
#include "SER_GPS.h"
#include "angle.h"
#include "CSelect.h"
#include <ConfigDB.h>  // gets the logging flag
#include "RedStone_IPC.h"
#include <sys/timeb.h>

#include "atslogger.h"
#include "ats-string.h"
#include "utility.h"

extern ATSLogger g_log;
REDSTONE_IPC m_RedStoneData;

SER_GPS::SER_GPS()
{
  strcpy(m_DevName, "GPS");

  m_fd = -1;
  m_bIsNMEA = true;  // this is a source of NMEA
  selftest = DRVR_INIT;
  m_nCharsRcvd = 0;
  m_bNoGPS = true;

  // now get the logging flag  
  db_monitor::ConfigDB db;

  const ats::String app_name("SER_GPS");

  // must have valid values - get them all
  m_sendGPS = 1;

  m_Type = db.GetValue("RedStone", "GPS", "UBLOX");  // GEMALTO or UBLOX.  If UBLOX then ttySER1 -> ttySP3
  myParms.myBaudRateIndex = B115200;
  
  if (m_Type == "UBLOX")  // set ttySER1 to point to ttySP3
  {
  	system("rm /dev/ttySER1");
  	system("ln -s /dev/ttySP3 /dev/ttySER1");
	  myParms.myBaudRateIndex = B9600;
  }
  

  ats::String unit_id = db.GetValue("RedStone", "IMEI", "123451234512345");
  m_IMEIString = "$PATSID," + unit_id + ",IMEI";
  CHECKSUM cs;
  cs.add_checksum(m_IMEIString);

  ats_logf(&g_log, "Parms: Logging %s", m_bLogging ? "On": "Off");
  ats_logf(&g_log, "       GPS redirect %s", m_sendGPS ? "On": "Off");
  ats_logf(&g_log, "       IMEI String %s", m_IMEIString.c_str() );
  ats_logf(&g_log, "       GPS Source %s", m_Type.c_str() );
}

//-------------------------------------------------------------------
SER_GPS::~SER_GPS()
{
  // close the port

  if (m_fd != -1)
    close (m_fd);
  selftest = DRVR_DIS;
}

void SER_GPS::ConfigurePort(int RTUPort)
{
	const int baudrates[][2] = {{4800, B4800},
                                {9600, B9600},
                                {19200, B19200},
                                {38400, B38400},
                                {57600, B57600}};

	for(int i = 0; i < 5; i++)
  {
    SerDevice::SetPort(RTUPort, baudrates[i][1]);

    if(m_fd != -1)
    {
      ats_logf(&g_log,"Sent baud rate config at %d baud.", baudrates[i][0]);
      close(m_fd);
    }

    usleep(50000);
  }
}


//-------------------------------------------------------------------
// setup and open the port
void SER_GPS::SetPort
(
  short RTUPort  // 0, 1 or 2
)
{
  SerDevice::SetPort(RTUPort, myParms.myBaudRateIndex);

  selftest = DRVR_NO_DATA;
}

//-------------------------------------------------------------------
// process the RTU input, return 1 if there was an event
//
// looks for chars on the port and sends them to the AFF
bool SER_GPS::ProcessIncomingData()
{
  SerDevice::ProcessIncomingData();
  m_nCharsRcvd += m_LenInBuf;
  
  GetNMEA().Add(m_InBuf, m_LenInBuf); // this will update the current GPS position
  m_InBuf[m_LenInBuf] = '\0';

  selftest = DRVR_DATA_OK;
  myUpdateSysTimeTimer.SetTime();  // reset the timer for the update from the system clock
  return true;
}



void  SER_GPS::ProcessEverySecond()
{
  if (m_bNoGPS && myUpdateSysTimeTimer.DiffTimeMS() > 1000)  // update gps time if we don't receive GPS
  {
    GetNMEA().SetTimeFromSystem();
    myUpdateSysTimeTimer.SetTime();
  }

  if (m_NoDataTimer.DiffTimeMS() > 15000)
  {
    m_NoDataTimer.SetTime();

    if (m_nCharsRcvd == 0)
    {
			send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=gps led=gps script=\"0,1000000\"\r");
			send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=gpsr led=gps.r script=\"1,1000000\"\r");
      GetNMEA().GetData().SetInvalid();
		  ats_logf(&g_log, "SER_GPS-ProcessEverySecond: restarting gps", m_Type.c_str() );
			system("/usr/bin/restart_gps");  // reissue the commands for the GPS on the Cinterion modem.

    }

    m_nCharsRcvd = 0;
  }
  
  if (m_sendGPS && m_UpdateIMEITimer.DiffTimeMS() > 10000)
  {
    m_bReadyToSendID = true;
    m_UpdateIMEITimer.SetTime();
  }
}



//-------------------------------------------------------------------------
bool SER_GPS::VerifyParms()
{
  myParms.mybIsLogging = 0;  // not logging
  myParms.mytLogRate = 5;
  myParms.myLogType = 0; // CSV and KML
  return true;
}



//-------------------------------------------------------------------------
void SER_GPS::WriteLogFile()
{
}



//-------------------------------------------------------------------------
// ReopenPort
//  Close and reopen the port (usually because a baud rate changes)
//
void SER_GPS::ReopenPort()
{
  if (m_fd != -1)
    close(m_fd);

  SetPort (m_RTUPort);
}


void SER_GPS::CheckSystemTime() // checks the GPS time and date against the system time and date
{
  J2K sysTime;
  J2K nmeaTime;
  static short lastMinute = -1;
  static time_t prevTime = 0;
  static size_t timesSet = 0;
  sysTime.SetSystemTime();

  if (!GetNMEA().GetData().isValid())
    return;
  if ( GetNMEA().Year() < 2009 ) // sanity check on GPS
    return;

  nmeaTime.set_dmy_hms(GetNMEA().Day(), GetNMEA().Month(), GetNMEA().Year(),
                       GetNMEA().Hour(), GetNMEA().Minute(), GetNMEA().Seconds() );
	
	// REDSTONE FIXME:
	// Some programs rely on the system time for timing (such as the gettimeofday
	// function). All programs using gettimeofday (or similar) must be fixed.
	// [Amour Hassan - July 5, 2012]
  if (sysTime.GetYear() != nmeaTime.GetYear() ||
      sysTime.GetMonth() != nmeaTime.GetMonth() ||
      sysTime.GetDay() != nmeaTime.GetDay() ||
      sysTime.GetHour() != nmeaTime.GetHour() ||
      fabs( sysTime.GetMinute() - nmeaTime.GetMinute()) > 1)
  {
    struct tm *atm;
    time_t rawtime;
    time (&rawtime);
    atm = localtime(&rawtime);
    atm->tm_year = nmeaTime.GetYear() - 1900;
    atm->tm_mon = nmeaTime.GetMonth() - 1;
    atm->tm_mday = nmeaTime.GetDay();
    atm->tm_hour = nmeaTime.GetHour();
    atm->tm_min = nmeaTime.GetMinute();
    atm->tm_sec = nmeaTime.GetSecond();

    const time_t x = mktime(atm);
    if(x > prevTime)
    {
      stime(&x);  // sets the system time
      ++timesSet;
      send_redstone_ud_msg("rtc-monitor", 0, "set-rtc gps=1\r");

      static std::stringstream prev_date;
      std::stringstream date;
      ats::system("date|tr -d '\\n'|tr -d '\\r'", &date);
      {
        std::stringstream s;
        s << "SYS and RTC set to " << x << " at " << date.str() << "\n"
          << "Prev set time was " << prevTime << " at " << prev_date.str() << "\n"
          << "Times Set: " << timesSet << "\n";
        std::ofstream f("/var/log/SER_GPS-status.log");
        f << s.str();
      }
      prev_date.str(date.str());
      prevTime = x;
    }
//    if (system("hwclock --systohc") == -1)
//      syslog(LOG_ERR, "%s,%d: Unable to set the system clock from GPS time.", __FILE__, __LINE__);
  }

  lastMinute = nmeaTime.GetMinute();
}



