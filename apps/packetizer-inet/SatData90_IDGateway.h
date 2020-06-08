#pragma once

#include "SatData.h"


// This defines the Identify Gateway message.
//	The SetData function takes the current GPS and the STS_xxx type as inputs.
// See ISC_Iridium_Specification Rev1_4 IIS-16 for definition.
//
/*
<data>
  <unsignedShort name="event_id"/>
  <unsignedByte name="seqnum"/>
  <unsignedShort name="vehicle_uid"/>
  <DateTime name="datetime" units="UTC"/>
  <latitude name="latitude"/>
  <longitude name="longitude"/>
  <altitude name="altitude" units="meters"/>
  <unsignedShort name="speed" units="kph"/>
  <tenthsUShort name="COG" units="degrees"/>
  <tenthsUShort name="HDOP" units="degrees"/>
  <ShortMAC name="MAC"/>
  <string name="SerialNum"/>
  <string name="SiteName"/>
  <unsignedByte name="iNetURLIdx"/>
  <unsignedByte name="GatewayStatus"/>
</data>

</event>
*/
class SatData90_IDGateway : public SatData
{
private:
		LensMAC m_MAC;
		std::string m_serialNum;
		std::string m_siteName;
		std::string m_userName;
		unsigned char m_iNetURLIdx;
		unsigned char m_status;  // this is the gateway status byte found in ...
	
public:
	SatData90_IDGateway()
	{
		m_Type = SMT_IDENTIFY_GATEWAY;
	}


	void SetData
	(
		NMEA_DATA gps,
		LensMAC MAC,
		std::string serialNum,
		std::string siteName,
		int iNetURLIdx,
		unsigned char status
	)
	{
		SetGPS(gps);
		m_MAC        = MAC;
		m_serialNum  = serialNum;
		m_siteName   = siteName;
		m_iNetURLIdx = iNetURLIdx;
		m_status     = (unsigned char)status;
	}

	short	 Encode(char *buf)	// encode to buf - return length encoded
	{
		SatData::Encode(buf);	// will set idx for us
		EncodeMAC(buf, m_MAC);
		EncodeString(buf, m_serialNum, 16);
		EncodeString(buf, m_siteName, 16);
		buf[idx++] = m_iNetURLIdx;
		buf[idx++] = m_status;
		
		return idx;
	}
};
