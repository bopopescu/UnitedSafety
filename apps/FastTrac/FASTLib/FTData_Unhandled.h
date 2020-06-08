#pragma once

#include "FASTTrackData.h"


// this is an UNHANDLED MESSAGE class that has the unhandled message type attached to it.
//
//	The SetData function takes the current GPS and the STS_xxx type as inputs.
//
class FTData_Unhandled : public FASTTrackData
{
private:
	short m_MsgType;	// the type of the message that is unhandled
	
public:
	FTData_Unhandled()
	{
		m_Type = STS_UNHANDLED_MSG;	// record id type in FASTTrackData.h
		strcpy(m_strEventType, "Unhandled Message");
		strcpy(m_CSVHeader, "");
		m_MsgType = 0;
	}

	void SetData
	(
		NMEA_DATA gps, 
		const short type
	)
	{
		m_MsgType = type;
		SetGPS(gps);
	}

	short	 Encode(char *buf)	// encode to buf - return length encoded
	{
		FASTTrackData::Encode(buf);	// will set idx for us
		memcpy(&buf[idx], &m_MsgType, 2); idx += 2;
		return idx;
	}

	void	 WriteKMLRecord(FILE *fp)
	{
		fprintf(fp,"	 Event: %s\r\n", m_strEventType);
		fprintf(fp,"	 Msg: %d\r\n", m_MsgType);
	}
};
