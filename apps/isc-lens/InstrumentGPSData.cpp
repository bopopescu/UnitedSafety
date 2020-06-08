#include <string.h>
#include <math.h>
#include "atslogger.h"
#include "colordef.h"
#include "InstrumentGPSData.h"
#include "AFS_Timer.h"
#include "socket_interface.h"


static bool g_bSentInetNotification = false;
//----------------------------------------------------------------------------------------------------------------------
// Decode - take a raw packet (including frame) and decode it
int InstrumentGPSData::Decode(const unsigned char *rawData)
{
	int ret;
	ret = CheckBuffer(rawData); // check that the framing and checksum are OK

	if (ret != 0)
		return ret;

	memcpy(&m_GPS, &rawData[2], sizeof(GPSDataPacket));
	
	double dval;
	char strVal[32];
	
	m_MAC.MAC(m_GPS.m_MAC);
	// get the Latitude
	memcpy(strVal, m_GPS.m_Lat, 10); strVal[10] = '\0';
	dval = atof(strVal);
	m_Lat = ((int)dval / 100) + (fmod(dval, 100) / 60.0);
	if (m_GPS.m_NS == 'S' || m_GPS.m_NS == 's')
		m_Lat = -m_Lat;
		
	// get the Longitude
	memcpy(strVal, m_GPS.m_Lon, 10); strVal[8] = '\0';
	dval = atof(strVal);
	m_Lon = ((int)dval / 100) + (fmod(dval, 100) / 60.0);
	if (m_GPS.m_EW == 'W' || m_GPS.m_EW == 'w')
		m_Lon = -m_Lon;
	
	m_HDOP = (int)m_GPS.m_HDOP;
	m_IsValid = true;
	return 0;
}

//----------------------------------------------------------------------------------------------------------------------
// CheckBuffer - check that the framing and checksum are OK
int InstrumentGPSData::CheckBuffer(const unsigned char *rawData)
{
	char len = rawData[3];

	// check for framing error
	if (rawData[0] != 0x24 || rawData[1] != 0x24 ||rawData[len+3] != 0x23 || rawData[len+4] != 0x23 )
		return 1;

	// check for checksum error
	len = rawData[7];
	char checksum = rawData[len + 7 -1];
	int _checksum = 0;
	// calculate checksum
	for(int i = 0; i < len - 1; ++i)
	{
		_checksum += rawData[i + 7];
	}
		
	if ((char)(_checksum%256) != checksum)
	{
		ats_logf(ATSLOG_ERROR, RED_ON "InstrumentGPSData - checksum failure. Checksum is %02x expected %02x" RESET_COLOR, (char)_checksum, checksum);
		if (!g_bSentInetNotification) //<ISCP-164>
		{
			ats_logf(ATSLOG_DEBUG, "LENS sending InstrumentGPSData - checksum failure\r");
			g_bSentInetNotification = true;
			AFS_Timer t;
			t.SetTime();
			std::string user_data = "1011," + t.GetTimestampWithOS() + ", LENS Checksum Failure";
			user_data = ats::to_hex(user_data);

			send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
		}			
		return 2;
	}
	else
	{
		g_bSentInetNotification = false; // reset flag to send checksum error if occures again
	}
	return 0;
}
