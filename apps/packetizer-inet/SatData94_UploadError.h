#pragma once

#include "SatData.h"


// This defines the Identify Gateway message.
//	The SetData function takes the current GPS and the STS_xxx type as inputs.
// See ISC_Iridium_Specification Rev1_4 IIS-16 for definition.
//
/*  Here is the XML definition for Iridium decoding.
<?xml version="1.0" encoding="utf-8" ?>

<event name="UploadError" id="94">

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
  <short name="ErrorCode"/>
</data>

</event>
*/

class SatData94_UploadError : public SatData
{
private:
		LensMAC m_MAC;
		unsigned short m_err_code;

public:
	SatData94_UploadError()
	{
		m_Type = SMT_UPLOAD_ERROR ;
	}

	void SetData
	(
		LensMAC mac,
		NMEA_DATA gps,
		unsigned short err_code
	)
	{
		m_MAC = mac;
		SetGPS(gps);
		m_err_code = err_code;
	}

	short	 Encode(char *buf)	// encode to buf - return length encoded
	{
		SatData::Encode(buf);	// will set idx for us
		EncodeMAC(buf, m_MAC);
		char swap[8];
		memcpy(swap, &m_err_code, 2);		// Two byte for gas reading need to be swapped
		buf[idx++] = swap[1];
		buf[idx++] = swap[0];

		return idx;
	}
};
