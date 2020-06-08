#pragma once

#include "ats-common.h"
#include "FASTTrackData.h"


// this is a generic class that has no extra data attached to it.	It can be used
// to send a variety of messages (Ignition On, Ignition Off, Startup, Stopped, etc) that 
// have no specific data without create a FTData_xxx class for each type.
//
//	The SetData function takes the current GPS and the STS_xxx type as inputs.
//
class FTData_Generic_ESN : public FASTTrackData
{
private:
	ats::String m_ESN;
	unsigned char m_ESN_type;
public:
	FTData_Generic_ESN(const char * strTypeName)
	{
	m_Type = STS_IGNITION_ON_ESN;	// record id type in FASTTrackData.h
		strcpy(m_strEventType, strTypeName);
		strcpy(m_CSVHeader, "");
	}


	void SetData
	(
		NMEA_DATA gps, 
		const STS_TYPES type,
		const ats::String esn,
		const unsigned char esn_type
	)
	{
		m_Type = type;
		SetGPS(gps);
		m_ESN = esn;
		m_ESN_type = esn_type;
	}

	short	 Encode(char *buf)	// encode to buf - return length encoded
	{
		FASTTrackData::Encode(buf);	// will set idx for us

		buf[idx++] = m_ESN.length();
		memcpy(&buf[idx], m_ESN.c_str(), m_ESN.length()); idx += m_ESN.length();
		buf[idx++] = m_ESN_type;
		return idx;
	}


	void	 WriteKMLRecord(FILE *fp)
	{
		fprintf(fp,"	 Event: %s\r\n", m_strEventType);
	}
};
