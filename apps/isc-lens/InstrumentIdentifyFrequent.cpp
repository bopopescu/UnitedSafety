#include <string.h>
#include <INET_IPC.h>
#include <INetConfig.h>
#include "atslogger.h"
#include "InstrumentIdentifyFrequent.h"
#include "rapidjson.h"  //  rapidjson includes, namespace and functions.
#include "AFS_Timer.h"

extern INetConfig *g_pLensParms;  // read the db-config parameters

static bool g_bSentInetNotification = false;
//---------------------------------------------------------------------------------
InstrumentIdentifyFrequent& InstrumentIdentifyFrequent::operator=(const InstrumentIdentifyFrequent& rhs)
{
  memcpy(&m_IFP, &rhs.m_IFP, sizeof(InstrumentFrequentPacket));
  memcpy(m_UserName, rhs.m_UserName, 17);
	m_UserName[16] = '\0';
  memcpy(m_SiteName, rhs.m_SiteName, 17);
	m_SiteName[16] = '\0';
	m_MAC = rhs.m_MAC;
	m_bIsInetEnabled = rhs.m_bIsInetEnabled;

	m_pLens = rhs.m_pLens;
	return *this;
}

//---------------------------------------------------------------------------------
// take a raw packet and decode it.   
// returns 0:OK
//				-1: ISCP-92 - This message is from another gateway (ignore it)
//				or CheckBuffer error codes.

int InstrumentIdentifyFrequent::Decode(const unsigned char *rawData)
{
	int ret;
	ret = CheckBuffer(rawData); // check that the framing and checksum are OK

	if (ret > 0)
		return ret;

	memcpy(&m_IFP, &rawData[2], sizeof(InstrumentFrequentPacket));
	m_MAC.MAC(m_IFP.m_MAC);

	ats_logf(ATSLOG_ERROR, CYAN_ON "Decoding InstrumentIdentifyFrequent: MAC:%s" RESET_COLOR, m_MAC.toHex().c_str());

	if (m_IFP.m_Length2 > 0) // do we have any fields?
	{
		int os = 0;  // offset from byte 8 (start of fields)
		int fieldType, len;

		while (os < (m_IFP.m_Length2-2))  // there are undocumented bytes at the end of the fields!
		{
			ats_logf(ATSLOG_ERROR, YELLOW_ON "Decoding InstrumentIdentifyFrequent: field byte:%02x  offset %d of %d" RESET_COLOR, rawData[os+8], os,  m_IFP.m_Length2);

			fieldType = rawData[os + 8] >> 5;
			len = rawData[os + 8] & 0x1f;
			
			if (fieldType == 0) // User name
			{
				strncpy(m_UserName, (char *)&rawData[os + 9], len);
				m_UserName[len] = 0;
				for (short i = 15; i > 0; i++)
				{
					if (m_UserName[i] == ' ')
						m_UserName[i] = '\0';
					else
						break;
				}
			}
			else if (fieldType == 1) // Site Name
			{
				strncpy(m_SiteName, (char *)&rawData[os + 9], len);
				m_SiteName[len] = 0;
				for (short i = 15; i > 0; i++)
				{
					if (m_SiteName[i] == ' ')
						m_SiteName[i] = '\0';
					else
						break;
				}
			}
			else if (fieldType == 4) // if field is 4 this is the EnableINet flag (2 indicates enabled)
			{
				if ( (int)(rawData[os + 9]) == 2)
					m_bIsInetEnabled = true;
				else
					m_bIsInetEnabled = false;
			}
			else  // undefined
				ats_logf(ATSLOG_ERROR, RED_ON "InstrumentIdentifyFrequent decoding error -  unknown field type %d" RESET_COLOR, fieldType);

			os += (len + 1);
		}
	}
	return 0;
}


//---------------------------------------------------------------------------------
// check that the framing and checksum are OK
// returns 	0:OK
// 			1:Framing error
//			2:Checksum error
int InstrumentIdentifyFrequent::CheckBuffer(const unsigned char *rawData)
{
	char len = rawData[3];

	// check for framing error
	if (rawData[0] != 0x24 || rawData[1] != 0x24 ||rawData[len+3] != 0x23 || rawData[len+4] != 0x23 )
		return 1;

	// check for checksum error
	len = rawData[7];
	char checksum = rawData[len + 7 - 1];
	int _checksum = 0;
	// calculate checksum
	for(int i = 0; i < len - 1; ++i)
	{
		_checksum += rawData[i + 7];
	}
		
	if ((char)(_checksum%256) != checksum)
	{
		ats_logf(ATSLOG_ERROR, "%s, %d: InstrumentIdentifyFrequent - checksum failure. Checksum is %02x expected %02x", __FILE__, __LINE__, (char)_checksum, checksum);
		
		if (!g_bSentInetNotification) //<ISCP-164>
		{
			ats_logf(ATSLOG_DEBUG, "LENS sending InstrumentIdentifyFrequent - checksum failure\r");
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

