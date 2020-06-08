#pragma once

#include "ats-common.h"

#include "FASTTrackData.h"
#include "fragment.h"


class FTData_UserData_ESN : public FASTTrackData
{
public:
	
private:
	ats::String m_ESN;
	unsigned char m_ESN_type;
	ats::String m_data;
public:
	FTData_UserData_ESN()
	{
	m_Type = STS_USER_MESSAGE_ESN;	// record id type in FASTTrackData.h
	strcpy(m_strEventType, "UserData");
		strcpy(m_CSVHeader, "Type, PGN, SPN, Value, Limit");
	}

	void SetData
	(
		NMEA_DATA gps, 
	const ats::String strUserData,
	const ats::String esn,
	const unsigned char esn_type
	)
	{
	SetGPS(gps);
	m_data = ats::from_hex(strUserData);
	m_ESN = esn;
	m_ESN_type = esn_type;
	}

	short	 Encode(char *buf)	// encode to buf - return length encoded
	{
		FASTTrackData::Encode(buf);	// will set idx for us
	buf[idx++] = m_ESN.length();
	memcpy(&buf[idx], m_ESN.c_str(), m_ESN.length()); idx += m_ESN.length();
	buf[idx++] = m_ESN_type;
	buf[idx++] = m_data.length();
	memcpy(&buf[idx],m_data.c_str(), m_data.length()); idx += m_data.length();
		return idx;
	}
};



