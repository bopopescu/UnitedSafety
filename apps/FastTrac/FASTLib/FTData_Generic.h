#pragma once

#include "FASTTrackData.h"


// this is a generic class that has no extra data attached to it.	It can be used
// to send a variety of messages (Ignition On, Ignition Off, Startup, Stopped, etc) that 
// have no specific data without create a FTData_xxx class for each type.
//
//	The SetData function takes the current GPS and the STS_xxx type as inputs.
//
class FTData_Generic : public FASTTrackData
{
public:
	FTData_Generic(const char * strTypeName)
	{
		m_Type = STS_IGNITION_ON;	// record id type in FASTTrackData.h
		strcpy(m_strEventType, strTypeName);
		strcpy(m_CSVHeader, "");
	}


	void SetData
	(
		NMEA_DATA gps, 
		const STS_TYPES type
	)
	{
		m_Type = type;
		SetGPS(gps);
	}

	short	 Encode(char *buf)	// encode to buf - return length encoded
	{
		FASTTrackData::Encode(buf);	// will set idx for us
		return idx;
	}


	void	 WriteKMLRecord(FILE *fp)
	{
		fprintf(fp,"	 Event: %s\r\n", m_strEventType);
	}
};
