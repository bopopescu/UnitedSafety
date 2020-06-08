#pragma once

#include "SatData.h"


// This defines the Identify Gateway message.
//	The SetData function takes the current GPS and the STS_xxx type as inputs.
// See ISC_Iridium_Specification Rev1_4 IIS-16 for definition.
//
class SatData96_UpdateGateway : public SatData
{
private:
		LensMAC m_MAC;
		unsigned char m_status;  // this is the gateway status byte found in ...
	
public:
	SatData96_UpdateGateway()
	{
		m_Type = SMT_UPDATE_GATEWAY;
	}


	void SetData
	(
		NMEA_DATA gps,
		LensMAC MAC,
		unsigned char status
	)
	{
		SetGPS(gps);
		m_MAC        = MAC;
		m_status     = (unsigned char)status;
	}

	short	 Encode(char *buf)	// encode to buf - return length encoded
	{
		SatData::Encode(buf);	// will set idx for us
		EncodeMAC(buf, m_MAC);
		buf[idx++] = m_status;
		
		return idx;
	}
};
