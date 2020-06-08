/*-----------------------------------------------------------------------------
	FILE: NMEA.C - Decoding NMEA Messages
-----------------------------------------------------------------------------*/
using namespace std;

#define TRUE 1
#define FALSE 0

#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "math.h"
#include "J2K.h"
#include "WriteDebug.h"
#include "angle.h"
#include "utility.h"
#include "ats-common.h"
#include "socket_interface.h"
#include "atslogger.h"
#include "NMEA.h"
#include "RedStone_IPC.h"
#include "MyData.h"

static size_t error_CHECKSUM = 0;
static size_t error_GGA = 0;
static size_t error_RMC = 0;
static size_t error_VTG = 0;
static size_t error_PGRMZ = 0;
static size_t error_PGRME = 0;

extern bool g_bDebugging;
extern ATSLogger g_log;
extern REDSTONE_IPC m_RedStoneData;
static const int ID_INJECTION_TIME = 5;
static const int NMEA_MAX_LINE_SIZE = 256;

extern int dbg_level;

//-----------------------------------------------------------------------------------------
// constructor
//
NMEA::NMEA()
{
	m_Err = 0;
	m_Buf.SetSize(4096);
	m_newRecord = false;

	db_monitor::ConfigDB db;

	// must have valid values - get them all
	m_bLogging = false; // DRH removed from db-config Aug 2017
	m_fpLog = NULL;
	OpenLog();

	// stuff relating to GPS forwarding to external socket.
	m_sendGPS = true;
	m_sendGPSPort = 41092;

	ats::String unit_id = db.GetValue("RedStone", "IMEI");
	m_IMEIString = "$PATSID," + unit_id + ",IMEI";
	CHECKSUM cs;
	cs.add_checksum(m_IMEIString);

	m_isValid = false;
	m_gps_quality = 0;

	// output the current config to the log file for debugging purposes.
	ats_logf(ATSLOG_ERROR, "Parms: Logging %s", m_bLogging ? "On": "Off");
	ats_logf(ATSLOG_ERROR, "			  GPS redirect %s", m_sendGPS ? "On": "Off");
	ats_logf(ATSLOG_ERROR, "			  IMEI String %s", m_IMEIString.c_str() );
};

//-----------------------------------------------------------------------------------------
// Add the chars	to the buffer and then look for a
// valid NMEA message
int NMEA::Add( const char* buf, unsigned int nchars)
{
	int ret = NMEA_NOT_NMEA;

	if (nchars == 0)
	{
		ats_logf(ATSLOG_INFO, "0 chars input");
		return ret;
	}

	if (!m_bReadyToSendID && m_UpdateIMEITimer.DiffTime() > ID_INJECTION_TIME)
		m_bReadyToSendID = true;

	int ix;
	m_Buf.Add(buf, nchars);

	while (1)
	{

		if ((ix = m_Buf.Find('$')) < 0)
		{
			break;
		}
		else if (ix > 0)
		{
			m_Buf.Toss(ix);	// get rid of all chars before the '$'
		}

		char strInputLine[NMEA_MAX_LINE_SIZE];
		char* pInputLine;

		if ((ix = m_Buf.GetLine(strInputLine, NMEA_MAX_LINE_SIZE - 1)) > 0)	// we have a valid line
		{

			if(m_bLogging)
				WriteLog(strInputLine);
			// check for Telit Modem strings - prefaced by $GPSNMUN

			if (strstr(strInputLine, "$GPSNMUN") == strInputLine)	// pointing to the first char in the buffer
				pInputLine = strrchr(strInputLine, '$');
			else
				pInputLine = strInputLine;

			SendGPSToPort(strInputLine);  //send all NMEA data to port if necessary

			char strCopyOfInputLine[NMEA_MAX_LINE_SIZE];
			memcpy(strCopyOfInputLine, strInputLine, ix); // note - decode is destructive of the input buffer - therefore need a copy.
			ret = Decode(pInputLine);

			if (ret == NMEA_OK || ret == NMEA_RMC_OK || ret == NMEA_GGA_OK)
			{
				// ATS XXX: The time information in the GGA string is ignored. The RMC string
				//	shall supply the time information. The RMC string will always follow the
				//	GGA string.
				if(ret == NMEA_GGA_OK)	 // start the millisecond count when GGA is decoded.
				{
					MyData& md = MyData::getInstance();
					md.lock();
					md.m_gga = strCopyOfInputLine;
					md.unlock();
					m_LastEpochTimer.SetTime();
					ret = NMEA_OK;
				}
				else if(ret == NMEA_RMC_OK)
				{
					MyData& md = MyData::getInstance();
					md.lock();
					md.m_rmc = strCopyOfInputLine;
					md.unlock();
					ats_logf(ATSLOG_DEBUG,"GPS_Quality:%d GPS_Valid:%d\n", m_Data.gps_quality,m_Data.valid);
					if(m_bLogging)
					{
						char posBuf[NMEA_MAX_LINE_SIZE];
						GetFullPosStr(posBuf, NMEA_MAX_LINE_SIZE);
						ats_logf(ATSLOG_INFO, "%s", posBuf);
					}

					ret = NMEA_OK;
				}

			}

		}
		else
		{
			ats_logf(ATSLOG_DEBUG, "NMEA: Invalid line");
			break;
		}

	}

	return ret;
}

typedef std::map <const ats::String, int> GPSMsgCountMap;
typedef std::pair <const ats::String, int> GPSMsgCountPair;

/*$-----------------------------------------------------------------------------
Name:				Decode

Purpose:		 To decode NMEA navigation messages and fill the NMEA structure

Arguments:	 NMEA (I/O)		- gps position and time

Description: returns
						 NMEA_OK					 - everything OK
						 NMEA_BAD_CHECKSUM - checksum not correct
						 NMEA_BAD_NMEA		 - incorrectly formatted message
						 NMEA_NOT_NMEA		 - a message not handled by the routine

----------------------------------------------------------------------------- */
short NMEA::Decode(char * buf)
{
	short error = NMEA_BAD_CHECKSUM;
	static size_t prev_count = 0;
	static size_t count = 0;

	static ats::String prev_msg;
	static size_t consecutive_msgs = 0;

	static GPSMsgCountMap message;

	ats_logf(ATSLOG_DEBUG,"GPS_DATA:%s\n",buf);
	if (CheckChecksum(buf + 1) != 1)
	{
		error_CHECKSUM++;
	}
	else
	{
		char *ptrs[40];
		short rc = NMEA_NOT_NMEA;
		short num = 0;

		if ((num = (short)fragment(buf, ',', ptrs, 39)) > 2)
		{
			const ats::String msg(ptrs[0], 6);

			if(msg == prev_msg) consecutive_msgs++;
			else consecutive_msgs = 1;

			GPSMsgCountMap::iterator i = message.find(msg);

			if(message.end() == i) message.insert(GPSMsgCountPair(msg, 1));
			else ++(i->second);

			// REDSTONE FIXME: BUG #28 - GOBI 2000 modem outputs only "$GPGGA,,,,,,0,,,,,,,,*66" string on /dev/ttyUSB2 port
			//
			//	When 30 GPGGA messages are read in a row, then assume that the GOBI modem GPS system has failed.
			//	Therefore turn off the GOBI modem (which will in turn cause the system to detect a modem fault
			//	and restart the modem). When the system restarts the modem, it is likely the problem will be
			//	resolved. There is no solution for the case when the GPS system does not recover after multiple
			//	modem restarts (user intervention will be required).
			//
			// [Amour Hassan - September 16, 2012]
			if((consecutive_msgs > 30) && ("$GPGGA" == msg))
			{
				//DRH May 2013	system("turn-off-gobi-modem");
				consecutive_msgs = 1;
			}

			if (strncmp(&ptrs[0][3], "GGA", 3) == 0)
			{
				rc = DecodeGGA(ptrs, num);
				if (rc != NMEA_GGA_OK)
				{
					m_Data.valid = false;
					count++;
					error_GGA++;
				}
				else
				{
					m_Data.valid = true;
				}

			}
			else if (strncmp(&ptrs[0][3], "RMC", 3) == 0)
			{
				rc = DecodeRMC(ptrs, num);

				if (rc != NMEA_RMC_OK)
				{
					m_Data.valid = false;
					count++;
					error_RMC++;
				}
				else if (m_Data.gps_quality == 0)
				{
					m_Data.valid = false;
				}
				else
				{
					m_Data.valid = true;
				}

			}
			else if (strncmp(&ptrs[0][3], "VTG", 3) == 0)
			{
				rc = DecodeVTG(ptrs, num);

				if (rc != NMEA_OK)
				{
					count++;
					error_VTG++;
				}

			}
			else if (strncmp(ptrs[0], "$PGRMZ", 6) == 0) // Garmin proprietary altitude
			{
				rc = DecodePGRMZ(ptrs, num);

				if (rc != NMEA_OK)
				{
					count++;
					error_PGRMZ++;
				}

			}
			else if (strncmp(ptrs[0], "$PGRME", 6) == 0) // Garmin proprietary error
			{
				rc = DecodePGRME(ptrs, num);

				if (rc != NMEA_OK)
				{
					count++;
					error_PGRME++;
				}

			}
			else if (strncmp(ptrs[0], "$GPGST", 6) == 0)
			{
				rc = DecodeGST(ptrs, num);

				if (rc != NMEA_OK)
				{
					count++;
				}

			}

			prev_msg = msg;
		}

		error = rc;
	}

	if(dbg_level > 0 && prev_count != count)  // only store this at DEBUG level - not ERROR level of logging
	{
		prev_count = count;
		std::ofstream out("/var/log/NMEA-Decode.log");

		{
			char buf[64];
			out << ats::getstrtimef("%a %Y-%m-%d %H:%M:%S", buf, sizeof(buf));
		}

		out	<< "\nerror_CHECKSUM: " << error_CHECKSUM
			<< "\nerror_GGA: " << error_GGA
			<< "\nerror_RMC: " << error_RMC
			<< "\nerror_VTG: " << error_VTG
			<< "\nerror_PGRMZ: " << error_PGRMZ
			<< "\nerror_PGRME: " << error_PGRME
			<< "\n";

		out	<< "\n"
			<< "Message Stats:\n";
		out << "Previous message: " << prev_msg << "\n";
		out << "Consecutive messages: " << consecutive_msgs << "\n";
		GPSMsgCountMap::const_iterator i = message.begin();

		while(i != message.end())
		{
			out << "\t" << i->first << ": " << i->second << "\n";
			++i;
		}

	}

	return error;
}

/*$----------------------------------------------------------------------------
	Name:				atohex

	Purpose:		 converts a character to a hex digit

	Parameters:	c (I) - the input character

	Discussion:

	History:		 Dave Huff - Jan. 30, 1995 - original code.
-----------------------------------------------------------------------------*/
short NMEA::atohex
(
	char c
)
{
	if (c >= '0' && c <= '9')
		return((short)(c - '0'));

	if (c >= 'a' && c <= 'f')
		return((short)(c - 'a' + 10));

	if (c >= 'A' && c <= 'F')
		return((short)(c - 'A' + 10));

	return -1;
}						 // End of atohex

/*$--------------------------------------------------------------------------
Name:				CheckChecksum

Purpose:		 to compute NMEA checksum and compare against that in a string

Arguments:	 buffer[] (I) - message that checksum is to be computed for

Description: computes NMEA checksum for given string and compares against
						 the checksum found in the nmea message
						 see NMEA 0183 standards for details on check sum computations.

Returns:		 1 if string passes checksum test.
						 0	if no checksum was found.
						 -1 if a checksum is found, and it's bad.

----------------------------------------------------------------------------- */
short NMEA::CheckChecksum
(
	char buffer[]
)
{
	short i;
	short lower, upper;
	char check_sum = '\0';
	char *asteric;

	if ((asteric = strchr(buffer, '*')) == NULL)			// no checksum to check
		return( 0 );

	upper = atohex(*(asteric + 1));
	lower = atohex(*(asteric + 2));

	if (upper == -1 || lower == -1)
		return(-1);

	check_sum ^= (char)lower;
	check_sum ^= (char)(upper << 4);

	for (i = 0; *(buffer + i) && *(buffer + i) != '*'; ++i)
		check_sum ^= *(buffer+i);

	if (check_sum == '\0')
		return(1);
	else
		return(-1);
}

/*$--------------------------------------------------------------------------
Name:				 fragment

Purpose:			Fragment a buffer into separate items.


Arguments:		buf					(I/O) - buffer to fragment
							delimit_char (I)	 - char that separates items
							ptr_array		(O)	 - array of pointers to items
							max_array		(I)	 - size of ptr_array

Description:	Go through the buffer, identifying the start of each item
							found.	The end of each item is terminated with delimit_char.
							This function replaces this delimiter with a NUL byte on all
							but the last item.	This implies that if you want 2
							null-terminated items and the rest untouched, put max_array
							to be 3...

							RETURN:	Number of items found and placed in ptr_array.
-------------------------------------------------------------------------- */
short NMEA::fragment
(
	char *buf,									 // Buffer to fragment (GETS MANGLED)
	char delimit_char,					 // What delimits items
	char *ptr_array[],					 // Target list of items
	short	max_array						 // Num pointers in ptr_array
)
{
	char *p;
	short	num_found;

//	 -----------------------------------------------------------
//		 Loop looking for new items.
//	------------------------------------------------------------

	for (num_found = 0, p = buf; *p != '\0'; )
	{
		ptr_array[num_found++] = p;

//		-----------------------------------------------------------
//			 Look for the next delimiting character
//		------------------------------------------------------------

		while (*p != '\0' && *p != delimit_char && *p != '*')
			p++;

		if (*p == '\0')
			break;

//		-----------------------------------------------------------
//			 End the item, then check for item pointer overflow.
//		-----------------------------------------------------------

		if (num_found >= max_array)
			break;

		*p++ = '\0';
	}

	return(num_found);
}

short NMEA::DecodeUTC( char *utc_buf )
{
	m_Data.utc = atof(utc_buf);
	m_Data.hour = (short)floor( m_Data.utc / 10000.0);
	m_Data.minute = (short)floor((m_Data.utc - m_Data.hour * 10000.0) / 100.0);
	m_Data.seconds = m_Data.utc - m_Data.hour * 10000.0 - m_Data.minute * 100.0;

	if (m_Data.hour > 24.0 || m_Data.hour < 0.0 || m_Data.minute > 60.0 || m_Data.minute < 0.0
			|| m_Data.seconds > 60.0 || m_Data.seconds < 0.0)
	{
		m_Err = 30;
		return(NMEA_BAD_NMEA);
	}

	m_Data.utc = m_Data.hour * 3600.0 + m_Data.minute * 60.0 + m_Data.seconds;

	return NMEA_OK;
}

/*-----------------------------------------------------------------------------
*/
static double DecodeDDMM(char *ddmmBuf)
{
	double deg, minutes, dval;

	minutes = modf( atof(ddmmBuf)/100.0, &deg) * 100.0;

	dval =	(deg + (minutes / 60.0));

	if (dval > 180.0)
		dval -= 360;

	return dval;
}

/*$-----------------------------------------------------------------------------
Name:				DecodeGGA

Purpose:		 To decode NMEA gga navigation message and fill the NMEA structure

Arguments:	 NMEA (I/O)		- gps position and time
						 pt	 (I)			- pointer to each NMEA element
						 num	(I)			- number of elements

Description: only sets nmeaData.flag for data groups if the
						 quality indicator is > 0 (valid)

						 then if the position is present it sets NMEA_POSITION bit
									if the time		 is present it sets NMEA_TIME bit
									if the altitude is present it sets NMEA_ALTITUDE bit

						 and	if all the status indicators (dgps age, quality, num svs)
									are present it sets the NMEA_STATUS bit

						 returns
						 NMEA_GGA_OK					 - everything OK
						 NMEA_BAD_NMEA		 - incorrectly formatted message

----------------------------------------------------------------------------- */
short NMEA::DecodeGGA
(
	char *pt[],
	short num
)
{
	char*	dec_place;

	if (num < 15)
	{
		m_Err = 10;
		return NMEA_BAD_NMEA;
	}

	if (strlen(pt[6]))
	{
		m_gps_quality = (short)atoi(pt[6]);

		if (m_gps_quality == 0) // not valid position
		{
			m_isValid = false;
			return NMEA_BAD_NMEA;
		}
		else
			m_isValid = true;
	}

	if (strlen(pt[2]) && strlen(pt[3]) && strlen(pt[4]) && strlen(pt[5]))
	{
		if ((dec_place = strchr(pt[2], '.')) == NULL || (short)(dec_place - pt[2]) != 4)
		{
			m_Err = 13;
			return NMEA_BAD_NMEA;
		}

		m_Data.ddLat = DecodeDDMM(pt[2]);

		if (*pt[3] == 'S' || *pt[3] == 's')
			m_Data.ddLat	= -m_Data.ddLat;
		else if (*pt[3] != 'N' && *pt[3] != 'n')
		{
			m_Err = 14;
			return NMEA_BAD_NMEA;
		}

		if (fabs(m_Data.ddLat) > 180.0)
		{
			m_Err = 15;
			return(NMEA_BAD_NMEA);
		}

		if ((dec_place = strchr(pt[4], '.')) == NULL || (short)(dec_place - pt[4]) != 5)
		{
			m_Err = 16;
			return NMEA_BAD_NMEA;
		}

		m_Data.ddLon = DecodeDDMM(pt[4]);

		if (*pt[5] == 'W' || *pt[5] == 'w')
			m_Data.ddLon	= -m_Data.ddLon;
		else if (*pt[5] != 'E' && *pt[5] != 'e')
		{
			m_Err = 17;
			return NMEA_BAD_NMEA;
		}

		if (fabs(m_Data.ddLon) > 360.0)
		{
			m_Err = 18;
			return(NMEA_BAD_NMEA);
		}

		if (m_Data.ddLon > 180.0)	// make it -180 to 180
			m_Data.ddLon -= 360.0;
	}
	else
		return (NMEA_EMPTY);

	if (strlen(pt[9]))
	{
		m_Data.H = atof(pt[9]);

		if (strlen(pt[11]))
		{
			m_Data.N = atof(pt[11]);
			m_Data.h = m_Data.H + m_Data.N;
		}
	}

	if (strlen(pt[7]) && strlen(pt[8]))
	{
		m_Data.num_svs = (short)atoi(pt[7]);
		m_Data.hdop		= atof(pt[8]);
	}
	else
		return (NMEA_EMPTY);

	if (strlen(pt[13]))
		m_Data.dgps_age		= (short)atoi(pt[13]);

	if (strlen(pt[14]))
		m_Data.ref_id = (short)atoi(pt[14]);

	m_Err = 0;

	return NMEA_GGA_OK;
}

/*$-----------------------------------------------------------------------------
Name:			 DecodeRMC

Purpose:		 To decode NMEA rmc navigation message and ouput to file

Arguments:	 NMEA (I/O)		- gps position and time
						 pt	 (I)			- pointer to each NMEA element
						 num	(I)			- number of elements

Description: grabs track made good and ground speed and dumps to a file
						 named nmea_vel.dat

						 returns
						 NMEA_RMC_OK			 - everything OK
						 NMEA_BAD_NMEA		 - incorrectly formatted message

----------------------------------------------------------------------------- */
short NMEA::DecodeRMC
(
	char *pt[],
	short num
)
{
	short ret;
	char*	dec_place;
	m_newRecord = false;

	J2K theTime;
	theTime.SetSystemTime();	// Get the time from the system
	ats_logf(ATSLOG_DEBUG, "GPS RMC 1\n");
	if (num < 9)
	{
		m_Err = 20;
		ats_logf(ATSLOG_DEBUG, "DecodeRMC: Not enough fields");
	}
	if (*pt[2] != 'A')  // must be valid to continue processing
	{
		m_Data.gps_quality = 0; // not valid position
		m_Data.valid = false;
		return NMEA_BAD_NMEA;
	}
	if (strlen(pt[1]) && strlen(pt[3]) && strlen(pt[4]) && strlen(pt[5]))
	{
		if ((dec_place = strchr(pt[3], '.')) == NULL || (short)(dec_place - pt[3]) != 4)
		{
			m_Err = 21;
			return NMEA_BAD_NMEA;
		}
		m_Data.ddLat = DecodeDDMM(pt[3]);

		if (*pt[4] == 'S' || *pt[4] == 's')
			m_Data.ddLat	= -m_Data.ddLat;
		else if (*pt[4] != 'N' && *pt[4] != 'n')
		{
			m_Err = 22;
			return NMEA_BAD_NMEA;
		}

		if (fabs(m_Data.ddLat) > 180.0)
		{
			m_Err = 23;
			return(NMEA_BAD_NMEA);
		}

		if ((dec_place = strchr(pt[5], '.')) == NULL || (short)(dec_place - pt[5]) != 5)
		{
			m_Err = 24;
			return NMEA_BAD_NMEA;
		}

		m_Data.ddLon = DecodeDDMM(pt[5]);

		if (*pt[6] == 'W' || *pt[6] == 'w')
			m_Data.ddLon	= -m_Data.ddLon;
		else if (*pt[6] != 'E' && *pt[6] != 'e')
		{
			m_Err = 25;
			return NMEA_BAD_NMEA;
		}

		if (fabs(m_Data.ddLon) > 360.0)
		{
			m_Err = 26;
			return(NMEA_BAD_NMEA);
		}
	}
	else
		return (NMEA_EMPTY);

	if (strlen(pt[1]) && strlen(pt[7]) && strlen(pt[9]))
	{
		short ret;
		if ((ret = DecodeUTC(pt[1])) != 0)
		{
			m_Err = 27;
			return NMEA_BAD_NMEA;
		}

		long date = atoi(pt[9]);
		m_Data.day = date / 10000;
		m_Data.month = (date / 100) % 100;
		m_Data.year = 2000 + date % 100;
	}

	// ATS FIXME: Are all of the following fields mandatory?
	//	For now, all fields will be processed even if some may fail. If there
	//	is a failure then it will be returned after processing all fields
	//	(rather than throwing away all information when one field fails).
	// [Amour Hassan - July 5 2012]
	m_Err = 0;
	if (strlen(pt[1]))
	{
		if ((ret = DecodeUTC(pt[1])) != 0)
		{
			m_Err = 28;
		}
	}

	if(strlen(pt[7]))
	{
		m_Data.sog = (double)atof(pt[7]); // speed in knots
	}
	else
	{
		m_Err = 29;
	}

	if(strlen(pt[8]))
	{
		m_Data.ddCOG = (double)atof(pt[8]);
	}
	else
	{
		m_Err = 30;
	}

	if(m_Err)
	{
		ats_logf(ATSLOG_DEBUG, "RMC decode error: %d", m_Err);
	}

ats_logf(ATSLOG_DEBUG, "GPS RMC 8\n");
	 if(
		( m_Data.year < theTime.GetYear() ) 	||
		( m_Data.month < theTime.GetMonth() ) 	||
		( m_Data.day < theTime.GetDay() ) 		||
		( m_Data.hour < theTime.GetHour() )		||
		( m_Data.minute < theTime.GetMinute() ) 
		)
 		{
 			//ISCP-303 Fix discard old data from modem bassed on GPS time
 			ats_logf(ATSLOG_DEBUG, "GPS Fix Lost\n");
 			m_Data.gps_quality = 0; // not valid position
			m_Data.valid = false;
  		}
  		else
  		{
  			//ISCP-303 Fix discard old data from modem bassed on GPS time
 			ats_logf(ATSLOG_DEBUG, "GPS Fix found\n");
 			m_Data.gps_quality = m_gps_quality; // not valid position
			m_Data.valid = m_isValid;	
  		}
  		ats_logf(ATSLOG_DEBUG, "GPS_DATE_TIME:%d:%d:%d %d:%d\n",m_Data.year,m_Data.month,m_Data.day,m_Data.hour,m_Data.minute);
		ats_logf(ATSLOG_DEBUG, "CURRENT_DATE_TIME:%d:%d:%d %d:%d\n",theTime.GetYear(),theTime.GetMonth(),theTime.GetDay(),theTime.GetHour(),theTime.GetMinute());
	return NMEA_RMC_OK;
}

/*$-----------------------------------------------------------------------------
Name:			 DecodeGST

Purpose:		 To decode NMEA GST navigation message and ouput to file

Arguments:	 NMEA (I/O)		- gps position and time
						 pt	 (I)			- pointer to each NMEA element
						 num	(I)			- number of elements

Description: grabs track made good and ground speed and dumps to a file
						 named nmea_vel.dat

						 returns
						 NMEA_OK					 - everything OK
						 NMEA_BAD_NMEA		 - incorrectly formatted message

----------------------------------------------------------------------------- */
short NMEA::DecodeGST
(
	char *pt[],
	short num
)
{
	short ret;
	char*	dec_place;

	if (num < 9)
	{
		m_Err = 30;
		ats_logf(ATSLOG_DEBUG, "DecodeGST: Not enough fields");
		return NMEA_BAD_NMEA;
	}

	if (strlen(pt[1]))
	{
		return(NMEA_OK);
	}
	else
		return (NMEA_EMPTY);


	return NMEA_OK;
}

//----------------------------------------------
// GetBasicPosStr
//	writes H:M:S,D/M/Y,LatDD,LonDD,H int buf
//
char *NMEA::GetBasicPosStr(char *buf)
{
	if (Year() == 0)
		SetTimeFromSystem();

	sprintf(buf, "%02d:%02d:%02d, %d/%d/%d,	%.9f, %.9f, %.3f	",
	 Hour(), Minute(), (int)Seconds(), Day(), Month(), Year(), Lat(), Lon(), H_ortho());
	return buf;
}

//----------------------------------------------
// GetBasicPosStr
//	writes H:M:S,D/M/Y,LatDD,LonDD,H int buf
//
unsigned short NMEA::GetBasicPosStr(FILE *fp)
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
//	writes H:M:S,D/M/Y,LatDD,LonDD,H, COG, SOG, HDOP
//
char *NMEA::GetLogStr(char *buf)
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
//	writes H:M:S,D/M/Y,LatDD,LonDD,H, COG, SOG, HDOP
//
void NMEA::GetLogStr(FILE *fp)
{
	if (Year() == 0)
		SetTimeFromSystem();

	fprintf(fp, "%02d:%02d:%02d,%d/%d/%d,%.9f,%.9f,%.3f, %.1f,%.1f, %.1f",
	 Hour(), Minute(), (int)(Seconds()), Day(), Month(), Year(), Lat(), Lon(), H_ortho(),
	 COG(), SOG(), HDOP());
}

//--------------------------------------------------------------
// GetFullPosStr
//	writes out all the data into buf in a CSV format
//	writes H:M:S,D/M/Y,LatDD,LonDD,H int buf
//
//	Up to the user to make sure they have enough space to write into!
//	should be at least 100 bytes
//
char *NMEA::GetFullPosStr(char *buf, int maxLen)
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

unsigned short NMEA::GetFullPosStr(FILE *fp)
{
	if (fp == NULL)
		return 0;

	char rbuf[256];
	GetFullPosStr(rbuf, 255);
	fprintf(fp, "%s", rbuf);
	return 1;
}

//--------------------------------------------------------------
// GetGarminErrorStr
//	writes out all the data into buf in a CSV format
//	writes H:M:S,D/M/Y,LatDD,LonDD,H int buf
//
//	Up to the user to make sure they have enough space to write into!
//	should be at least 100 bytes
//

char *NMEA::GetGarminErrorStr(char *buf, int maxLen)
{
	char rbuf[256];

	sprintf(rbuf, "%.1f,%.1f,%.1f", m_Data.HorizErr, m_Data.VertErr, m_Data.SphereErr);

	if ((int)strlen(rbuf) < maxLen)
		strcpy(buf, rbuf);
	else
		strcpy(buf, "0.0,0.0,0.0");

	return buf;
}

char *NMEA::GetTimeStr(char *buf, int maxLen)
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
void NMEA::SetTimeFromSystem()
{
	J2K theTime;

	theTime.SetSystemTime();	// set the time from the system
	m_Data.year = theTime.GetYear();
	m_Data.month = theTime.GetMonth();
	m_Data.day = theTime.GetDay();
	m_Data.hour = theTime.GetHour();
	m_Data.minute = theTime.GetMinute();
	m_Data.seconds = theTime.GetSecond();
}

//-------------------------------------------------------------------------
//	Decode PGRMZ message
//
short NMEA::DecodePGRMZ
(
	char *pt[],
	short num
)
{
	if (num < 4)
	{
		m_Err = 40;
		ats_logf(ATSLOG_DEBUG, "DecodePGRMZ: Not enough fields");
		return NMEA_BAD_NMEA;
	}

	if ( strlen(pt[1]) && strlen(pt[2]) )
	{
		m_Data.H	 = (double)atof(pt[1]);

		if (pt[2][0] == 'f' || pt[2][0] == 'F')
			m_Data.H	 = (double)atof(pt[1]) * FT_TO_M;
	}
	else
	{
		m_Err = 41;
		return NMEA_BAD_NMEA;
	}

	m_Err = 0;
	return NMEA_OK;
}

//-------------------------------------------------------------------------
//	Decode PGRME message
//
short NMEA::DecodePGRME
(
	char *pt[],
	short num
)
{
	if (num < 7)
	{
		m_Err = 50;
		ats_logf(ATSLOG_DEBUG, "DecodePGRME: Not enough fields");
		return NMEA_BAD_NMEA;
	}

	if ( strlen(pt[1]) && strlen(pt[2]) )
	{
		m_Data.HorizErr	 = (double)atof(pt[1]);
	}
	else
	{
		m_Err = 51;
		return NMEA_BAD_NMEA;
	}

	if ( strlen(pt[3]) && strlen(pt[4]) )
	{
		m_Data.VertErr	 = (double)atof(pt[3]);
	}
	else
	{
		m_Err = 51;
		return NMEA_BAD_NMEA;
	}

	if ( strlen(pt[5]) && strlen(pt[6]) )
	{
		m_Data.SphereErr	 = (double)atof(pt[5]);
	}
	else
	{
		m_Err = 51;
		return NMEA_BAD_NMEA;
	}

	m_Err = 0;
	return NMEA_OK;
}

//-------------------------------------------------------------------------
//	Decode GPVTG message
//
short NMEA::DecodeVTG
(
	char *pt[],
	short num
)
{
	if (num < 9)
	{
		m_Err = 60;
		ats_logf(ATSLOG_DEBUG, "DecodeVTG: Not enough fields");
		return NMEA_BAD_NMEA;
	}

	if ( strlen(pt[1]) && strlen(pt[2]) )
	{
		if (strstr(pt[1], "nan"))
		{
			m_Data.ddCOG = 0.0;
		}
		else
		{
			m_Data.ddCOG	 = (double)atof(pt[1]);
		}
	}
	else	// no data assume 0 course (no movement yet usually)
	{
		m_Data.ddCOG = 0.0;
	}

	if ( strlen(pt[5]) && strlen(pt[6]) )
	{
		m_Data.sog = (double)atof(pt[5]); // speed in knots
	}
	else
	{
		m_Err = 61;
		return NMEA_BAD_NMEA;
	}
	m_Err = 0;
	return NMEA_OK;
}

NMEA_DATA & NMEA::GetData()
{
	if (m_Data.year == 0 && m_Data.month == 0)
		SetTimeFromSystem();

	return m_Data;
}

short NMEA::GetGMTOffsetFromLon()
{
	short os = (short)(m_Data.ddLon / 15.0);

	if (os > 12)
		os -= 24;

	if (os < -12 || os > 12)
		os = 0;	// don't really know what it is so GMT

	return (os);
}

void NMEA::OpenLog()
{
	if (!m_bLogging)
		return;

	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	sprintf(m_strLogFile, "/var/log/%d-%02d-%02d_%02d%02d%02d.nmea", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	ats_logf(ATSLOG_DEBUG, "Opening %s for logging", m_strLogFile);
}

void NMEA::CloseLog()
{
	if (!m_bLogging)
		return;

	if (m_fpLog)		
		fclose(m_fpLog);
}

void NMEA::WriteLog(char * buf)
{
	if (!m_bLogging)
		return;
	
	m_fpLog = fopen(m_strLogFile, "a");

	if (m_fpLog)
	{
		fprintf(m_fpLog, "%s", buf);
	}
	CloseLog();
}

//-----------------------------------------------------------------------------------------------------
// Send NMEA data out the port if set up that way (m_sendGPS is true)
//
void NMEA::SendGPSToPort(char *strNMEA)
{
	if(m_sendGPS)
	{
 		send_msg("localhost", m_sendGPSPort, 0, "%s", strNMEA);	// send the NMEA data out
 		
	 if (m_bReadyToSendID)
	 {
			send_msg("localhost", m_sendGPSPort, 0, "%s", m_IMEIString.c_str()); // send the IME 
			AddNewInput();
			m_bReadyToSendID = false;
			ats_logf(ATSLOG_INFO, "%s", m_IMEIString.c_str() );
			m_UpdateIMEITimer.SetTime();
 		}	
			
 		if (m_bReadyToSendInputs)	// send the input state
 		{
	 		ats_logf(ATSLOG_INFO, "%s", m_InputString.c_str());
 			send_msg("localhost", m_sendGPSPort, 0, "%s", m_InputString.c_str()); // send the IME 
 			m_bReadyToSendInputs = false;
 			m_InputString.clear();
 		}
	}
}

void NMEA::AddNewInput()
{
	ats::String strInputs;
	ats::String strDate = GetDateStr();
	char inputs[8];
	unsigned char io;
	
	bool ignition_state = m_RedStoneData.IgnitionOn();
	io = m_RedStoneData.GPIO() * 2;	// shift over to allow ignition bit in bit 0
	io |= 0x80; // set the first two bits high 
	(ignition_state)? io|=0x01: io&=0xFE;
	sprintf(inputs, "FF%1X%1X", (io >> 4) & 0x0f, io & 0x0f);	// FF padding for future 2 byte output of status (possible internal flags)
	strInputs = "$PATSIO," + strDate + "," + inputs;
	CHECKSUM cs;
	cs.add_checksum(strInputs);
	m_InputString = strInputs;
	m_bReadyToSendInputs = true;
}

ats::String NMEA::GetDateStr()
{
	char buf[128];
	struct tm timeinfo;
	
	timeinfo.tm_year = m_Data.year - 1900;
	timeinfo.tm_mon = m_Data.month - 1;
	timeinfo.tm_mday = m_Data.day;
	timeinfo.tm_hour = m_Data.hour;
	timeinfo.tm_min = m_Data.minute;
	timeinfo.tm_sec = (int)m_Data.seconds;

	snprintf(buf, (sizeof(buf) - 1), "%d.%03d", (int)mktime(&timeinfo), m_LastEpochTimer.DiffTimeMS() % 1000);
	buf[sizeof(buf) - 1] = '\0';
	return ats::String(buf);
}
