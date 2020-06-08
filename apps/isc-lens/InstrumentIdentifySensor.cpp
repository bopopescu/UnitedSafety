#include <boost/format.hpp>
#include "InstrumentIdentifySensor.h"
#include "AFS_Timer.h"

#define SENSOR_TYPE_LEL (8)

static bool g_bSentInetNotification = false;
//---------------------------------------------------------------------------------
InstrumentIdentifySensor& InstrumentIdentifySensor::operator=(const InstrumentIdentifySensor& rhs)
{
  memcpy(&m_ISP, &rhs.m_ISP, sizeof(IdentifySensorPacket));
  
  for (short i = 0; i < 8; i++)
		memcpy(&m_Sensors[i], &rhs.m_Sensors[i], sizeof(SensorPacket));

	m_pLens = rhs.m_pLens;
	return *this;
}


//---------------------------------------------------------------------------------
// take a raw packet and decode it.   
// returns 0:OK
//		or CheckBuffer error codes.

int InstrumentIdentifySensor::Decode(const unsigned char *rawData)
{
	int ret;
	ret = CheckBuffer(rawData); // check that the framing and checksum are OK

	if (ret > 0)
		return ret;

	memcpy(&m_ISP, &rawData[2], sizeof(IdentifySensorPacket));
	
	for (short i = 0; i < m_ISP.m_SensorCount; i++)
	{
		memcpy(&m_Sensors[i], &rawData[10 + (i * sizeof(SensorPacket))], sizeof(SensorPacket));
		// ISCP-204 - have to look at the sensor type to fix 'Dual' sensors.
		if ((m_Sensors[i].m_Type & 0x0F) == 10)
		{
			ats_logf(ATSLOG_INFO, YELLOW_ON "%s,%d: Decode - Dual Sensor Detected - changing 10 to 4" RESET_COLOR, __FILE__, __LINE__);
			m_Sensors[i].m_Type = (m_Sensors[i].m_Type & 0xF0) + 4;
		}
		else if ((m_Sensors[i].m_Type & 0x0F) == 11)
		{
			ats_logf(ATSLOG_INFO, YELLOW_ON "%s,%d: Decode - Dual Sensor Detected - changing 11 to 3" RESET_COLOR, __FILE__, __LINE__);
			m_Sensors[i].m_Type = (m_Sensors[i].m_Type & 0xF0) + 3;
		}
		// end ISCP-204
	}
	m_MAC.MAC(m_ISP.m_MAC);  // set the mac for easier access.
	Log();  // output the sensor package to log file.
	return 0;
}

//---------------------------------------------------------------------------------
// check that the framing and checksum are OK
// returns 	0:OK
// 			1:Framing error
//			2:Checksum error
int InstrumentIdentifySensor::CheckBuffer(const unsigned char *rawData)
{
	char len = rawData[7];

	// check for framing error
	if (rawData[0] != 0x24 || rawData[1] != 0x24 ||rawData[len+7] != 0x23 || rawData[len+8] != 0x23 )
	{
		ats_logf(ATSLOG_ERROR, "%s, %d: CheckBuffer - Framing error", __FILE__, __LINE__);
		return 1;
	}	
	// check for checksum error
	char checksum = rawData[len + 6];
	int _checksum = 0;
	// calculate checksum
	for(int i = 0; i < len - 1; ++i)
		_checksum += rawData[i+7];
		
	if ((char)(_checksum%256) != checksum)
	{
		ats_logf(ATSLOG_ERROR, "%s, %d: InstrumentIdentifySensor - checksum failure. Checksum is %02x expected %02x", __FILE__, __LINE__, (char)_checksum, checksum);
		if (!g_bSentInetNotification) //<ISCP-164>
		{
			ats_logf(ATSLOG_DEBUG, "LENS sending InstrumentIdentifySensor - checksum failure\r");
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

//---------------------------------------------------------------------------------
std::string InstrumentIdentifySensor::SensorTypeJSON(const int idx)
{
	std::string ret;
	
	ats_sprintf(&ret, "S%04u", (m_Sensors[idx].m_Type & 0x0F));
	return ret;
}

//---------------------------------------------------------------------------------
std::string InstrumentIdentifySensor::GasTypeJSON(const int idx)
{
	std::string ret;
	
	ats_sprintf(&ret, "G%04u", m_Sensors[idx].m_GasType );
	return ret;	
}


//---------------------------------------------------------------------------------
int InstrumentIdentifySensor::UnitsOfMeasurement(const int idx)
{
	// ISCP-208 - have to fix the LEL reading.
	int type = (m_Sensors[idx].m_Type & 0x0f);
	if (type == SENSOR_TYPE_LEL) // SENSOR_TYPE_LEL
		return (int)((m_Sensors[idx].m_Units) & 0x0F);
	else
		return (int)((m_Sensors[idx].m_Units >> 4) & 0x0F);
}

int InstrumentIdentifySensor::GetMeasDecimalPlaces(int idx)
{
	// ISCP-208 - have to fix the LEL reading.
	int type = (m_Sensors[idx].m_Type & 0x0f);
	if (type == SENSOR_TYPE_LEL) // SENSOR_TYPE_LEL
		return (int)((m_Sensors[idx].m_Decimals) & 0x0F);
	else
		return (int)((m_Sensors[idx].m_Decimals >> 4) & 0x0F);
	// end ISCP-208
}
//---------------------------------------------------------------------------------
std::string InstrumentIdentifySensor::MACToHex()
{
	ats::String mac;
	ats_sprintf(&mac, "%02x%02x%02x", m_ISP.m_MAC[0], m_ISP.m_MAC[1], m_ISP.m_MAC[2]);
	return (mac);
}

void InstrumentIdentifySensor::Log()
{
	std::string log;
	log = str(boost::format("MAC:%s Sensors:%d ") % m_MAC.toHex().c_str() % (short)m_ISP.m_SensorCount);
	for (short i = 0; i < m_ISP.m_SensorCount; i++)
		log += str(boost::format("(%d: Type:%d  GasType:%d  Units:%d  Decimals:%d  High:%d Low:%d TWA:%d STEL:%d ) ") % i % 	(short)(m_Sensors[i].m_Type & 0x0f) %
			 (short)m_Sensors[i].m_GasType % (short)(m_Sensors[i].m_Units & 0xf0) %  (short)(m_Sensors[i].m_Decimals & 0xF0) % *(short *)(&m_Sensors[i].m_HighAlarm[0]) %
			 *(short *)(&m_Sensors[i].m_LowAlarm[0]) %	*(short *)(&m_Sensors[i].m_TWA[0]) % *(short *)(&m_Sensors[i].m_STEL[0]));
	ats_logf(ATSLOG_INFO, CYAN_ON "%s" RESET_COLOR, log.c_str());
}
