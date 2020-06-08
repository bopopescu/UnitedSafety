#pragma once

#include "ats-common.h"
#include "FASTTrackData.h"



class FTData_Debug_ESN : public FASTTrackData
{
public:
	enum DEBUG_PROCESS_IDS	// add an ID for each process - adjust the data_167.xml file with new strings.
	{
		ID_UNKNOWN,
		ID_PositionUpdate,
		ID_CANTrak
	};
	enum DEBUG_STATUS	// add an ID for each status - adjust the data_167.xml file with new strings.
	{
		UNKNOWN,
		PU_PINNED,
		PU_TIME,
		PU_DISTANCE,
		PU_HEADING,
		PU_STARTUP,
		PU_STOPPED,
		PU_STARTED,
		PU_IGNITION_ON,
		PU_IGNITION_OFF,
		PU_SEATBELT_ON,
		PU_SEATBELT_OFF

	};
private:
	unsigned char m_ProcessID;
	unsigned char m_Status;
	unsigned short m_UserData;
	ats::String m_ESN;
	unsigned char m_ESN_type;

public:
	FTData_Debug_ESN()
	{
	m_Type = STS_DEBUG_ESN;	// record id type in FASTTrackData.h
		strcpy(m_strEventType, "Debug");
		strcpy(m_CSVHeader, "ProcessID, Status, User");
		m_ProcessID = (unsigned char)ID_UNKNOWN;
		m_Status = (unsigned char)UNKNOWN;
		m_UserData = 0;
	}


	void SetData
	(
		NMEA_DATA gps, 
		const DEBUG_PROCESS_IDS processID,
		const DEBUG_STATUS status,
	const unsigned short userData,
	const ats::String esn,
	const unsigned char esn_type
	)
	{
		SetGPS(gps);
	m_ESN = esn;
	m_ESN_type = esn_type;
	m_ProcessID = (unsigned char)processID;
	m_Status = (unsigned char)status;
		m_UserData = userData;
	}

	short	 Encode(char *buf)	// encode to buf - return length encoded
	{
		FASTTrackData::Encode(buf);	// will set idx for us
	buf[idx++] = m_ESN.length();
	memcpy(&buf[idx], m_ESN.c_str(), m_ESN.length()); idx += m_ESN.length();
	buf[idx++] = m_ESN_type;
		buf[idx++] = m_ProcessID;
		buf[idx++] = m_Status;
		memcpy(&buf[idx], &m_UserData, 2); idx += 2;
		return idx;
	}

	void	 WriteLogRecord(FILE *fp)
	{
		fprintf(fp, "%d,%d,%d, ", m_ProcessID, m_Status, m_UserData);
	}

	void	 WriteKMLRecord(FILE *fp)
	{
		fprintf(fp,"	 Event: %s\r\n", m_strEventType);
		fprintf(fp, "	ProcessID:		%d\r\n", m_ProcessID);
		fprintf(fp, "	Status:		%d\r\n", m_Status);
		fprintf(fp, "	UserData:		%d\r\n", m_UserData);
	}
};
