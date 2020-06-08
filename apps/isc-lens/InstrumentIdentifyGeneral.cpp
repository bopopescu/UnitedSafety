#include <string.h>
#include <INET_IPC.h>
#include <INetConfig.h>
#include "atslogger.h"
#include "InstrumentIdentifyGeneral.h"
#include "rapidjson.h"  //  rapidjson includes, namespace and functions.
#include "AFS_Timer.h"

extern INetConfig *g_pLensParms;  // read the db-config parameters

static bool g_bSentInetNotification = false;
//---------------------------------------------------------------------------------
InstrumentIdentifyGeneral& InstrumentIdentifyGeneral::operator=(const InstrumentIdentifyGeneral& rhs)
{
  memcpy(&m_IGP, &rhs.m_IGP, sizeof(InstrumentGeneralPacket));
  memcpy(m_UserName, rhs.m_UserName, 24);
  memcpy(m_SiteName, rhs.m_SiteName, 24);
	m_SN = rhs.m_SN;
	m_MAC = rhs.m_MAC;
	m_bIsInetEnabled = rhs.m_bIsInetEnabled;

	m_pLens = rhs.m_pLens;
	return *this;
}

//---------------------------------------------------------------------------------
InstrumentIdentifyGeneral& InstrumentIdentifyGeneral::operator=(const InstrumentIdentifyFrequent& rhs)
{
  memcpy(m_UserName, rhs.m_UserName, 24);
  memcpy(m_SiteName, rhs.m_SiteName, 24);
	m_bIsInetEnabled = rhs.m_bIsInetEnabled;
	return *this;
}

//---------------------------------------------------------------------------------
// take a raw packet and decode it.   
// returns 0:OK
//				-1: ISCP-92 - This message is from another gateway (ignore it)
//				or CheckBuffer error codes.

int InstrumentIdentifyGeneral::Decode(const unsigned char *rawData)
{
	int ret;
	ret = CheckBuffer(rawData); // check that the framing and checksum are OK

	if (ret > 0)
		return ret;

	memcpy(&m_IGP, &rawData[2], sizeof(InstrumentGeneralPacket));
// 0x13 is not right - I am getting this from an instrument.	
//	if (m_IGP.m_DeviceType == 0x13) // ISCP-92 - ignore if this is another gateway
//		return -1;

	char buf[32];
	memcpy(buf, m_IGP.m_SN, 16);
	buf[16] = '\0';
	m_SN = buf;
	m_MAC.MAC8(m_IGP.m_MAC8);

	std::string space = " ";
	size_t end = m_SN.find_last_not_of(space);
	m_SN =  (end == std::string::npos) ? "" : m_SN.substr(0, end +1);
	ats_logf(ATSLOG_ERROR, CYAN_ON "InstrumentIdentifyGeneral:  Unit SN:%s  MAC:%s" RESET_COLOR, m_SN.c_str(), m_MAC.toHex().c_str());
	
	int os = 41;
	int field, len;
	while (os < rawData[3] + 2)
	{
		ats_logf(ATSLOG_ERROR, GREEN_ON "InstrumentIdentifyGeneral Decode - os %d size %d" RESET_COLOR, os, rawData[3]);
		field = rawData[os] >> 5;
		len = rawData[os] & 0x1f;
		if (field == 0)
		{
			strncpy(m_UserName, (char *)&rawData[os + 1], len);
			m_UserName[len] = 0;
			for (short i = len-1; i > 0; i++)  // remove trailing spaces
			{
				if (m_UserName[i] == ' ')
				  m_UserName[i] = '\0';
				else
					break;
			}
		}
		else if (field == 1)
		{
			strncpy(m_SiteName, (char *)&rawData[os + 1], len);
			m_SiteName[len] = 0;
			for (short i = len-1; i > 0; i++)  // remove trailing spaces
			{
				if (m_SiteName[i] == ' ')
				  m_SiteName[i] = '\0';
				else
					break;
			}
		}
		else if (field == 4) // if field is 4 this is the EnableINet flag (2 indicates enabled)
		{
			if ( (int)(rawData[os + 1]) == 2)
			{
				ats_logf(ATSLOG_ERROR, GREEN_ON "InstrumentIdentifyGeneral Decode - Inet is Enabled (%d)" RESET_COLOR, field);
				m_bIsInetEnabled = true;
			}
			else
			{
				ats_logf(ATSLOG_ERROR, RED_ON "InstrumentIdentifyGeneral Decode - Inet is Disabled (%d)" RESET_COLOR, field);
				m_bIsInetEnabled = false;
			}
		}
			
		else  // undefined
			ats_logf(ATSLOG_ERROR, RED_ON "InstrumentIdentifyGeneral decoding error -  unknown field type %d" RESET_COLOR, field);
		
		os += (len + 1);
	}
	return 0;
}


//---------------------------------------------------------------------------------
// check that the framing and checksum are OK
// returns 	0:OK
// 			1:Framing error
//			2:Checksum error
int InstrumentIdentifyGeneral::CheckBuffer(const unsigned char *rawData)
{
	char len = rawData[3];

	// check for framing error
	if (rawData[0] != 0x24 || rawData[1] != 0x24 ||rawData[len+3] != 0x23 || rawData[len+4] != 0x23 )
		return 1;

	// check for checksum error
	char checksum = rawData[len + 2];
	int _checksum = 0;
	// calculate checksum
	for(int i = 0; i < len - 1; ++i)
	{
		_checksum += rawData[i + 3];
	}
		
	if ((char)(_checksum%256) != checksum)
	{
		ats_logf(ATSLOG_ERROR, "%s, %d: InstrumentIdentifyGeneral - checksum failure. Checksum is %02x expected %02x", __FILE__, __LINE__, (char)_checksum, checksum);
		if (!g_bSentInetNotification) //<ISCP-164>
		{
			ats_logf(ATSLOG_DEBUG, "LENS sending InstrumentIdentifyGeneral - checksum failure\r");
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

