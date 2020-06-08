#pragma once

#include "SatData.h"


// This defines the Lost Instrument message.
//	The SetData function takes the current GPS and the STS_xxx type as inputs.
// See ISC_Iridium_Specification Rev1_4 IIS-16 for definition.
//
/*
<event name="LostInstrument" id="95">

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
</data>

</event>
*/

class SatData95_LostInstrument : public SatData
{
private:
		LensMAC m_MAC;
	
public:
	SatData95_LostInstrument()
	{
		m_Type = SMT_LOST_INSTRUMENT;
	}


	void SetData
	(
		NMEA_DATA gps,
		LensMAC MAC
	)
	{
		SetGPS(gps);
		m_MAC        = MAC;
	}
	
	//new code
	void SetData(LensMAC mac, NMEA_DATA &nmea)
	{
		m_MAC = mac;
		SetGPS(nmea);
	}
	//end new code
	
	short	 Encode(char *buf)	// encode to buf - return length encoded
	{
		SatData::Encode(buf);	// will set idx for us
		EncodeMAC(buf, m_MAC);	
		return idx;
	}
};
