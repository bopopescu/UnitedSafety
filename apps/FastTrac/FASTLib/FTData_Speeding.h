#pragma once

#include "FASTTrackData.h"


// this is a generic class that has no extra data attached to it.	It can be used
// to send a variety of messages (Ignition On, Ignition Off, Startup, Stopped, etc) that 
// have no specific data without create a FTData_xxx class for each type.
//
//	The SetData function takes the current GPS and the STS_xxx type as inputs.
//
class FTData_Speeding : public FASTTrackData
{
private:
	short m_Speed;	// kph
	short m_SpeedLimit;	//kph
	
public:
	FTData_Speeding()
	{
		m_Type = STS_SPEEDING;	// record id type in FASTTrackData.h
		strcpy(m_strEventType, "Speeding");
	}


	void SetData
	(
		NMEA_DATA gps,
		short speed,
		short speedLimit
	)
	{
		m_Speed = speed;
		m_SpeedLimit = speedLimit;
		SetGPS(gps);
	}
	void SetData(NMEA_DATA gps, ats::String userData)
	{
		std::string::size_type pos = userData.find_first_of(',');
		std::string token = userData.substr(0, pos);
		m_Speed = (short)atoi(userData.c_str());
		m_SpeedLimit = (short)atoi(token.c_str());
		SetGPS(gps);
	}

	short	 Encode(char *buf)	// encode to buf - return length encoded
	{
		FASTTrackData::Encode(buf);	// will set idx for us
		buf[idx++] = (unsigned char)m_Speed;
		buf[idx++] = (unsigned char)m_SpeedLimit;
		return idx;
	}
};
