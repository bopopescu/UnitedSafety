#pragma once

#include "FASTTrackData.h"
#include "fragment.h"


class FTData_J1939Fault_ESN : public FASTTrackData
{
public:
	
private:
	int m_SPN;
	int m_FMI;
	int m_OC;
	string m_ESN;
	unsigned char m_ESN_type;

public:
	FTData_J1939Fault_ESN()
	{
	m_Type = STS_J1939_FAULT_ESN;	// record id type in FASTTrackData.h
		strcpy(m_strEventType, "J1939 Fault");
		strcpy(m_CSVHeader, "Type, PGN, SPN, Value, Limit");
	}

	void DecodeUserData(string strUserData)
	{
		FRAGMENT f;
		
		f.fragment(strUserData.c_str(), ',');
		f.item(1, m_SPN);
		f.item(2, m_FMI);
		f.item(3, m_OC);
	}

	void SetData
	(
		NMEA_DATA gps, 
	string strUserData,
	const string esn,
	const unsigned char esn_type
	)
	{
		SetGPS(gps);
		DecodeUserData(strUserData);
	m_ESN = esn;
	m_ESN_type = esn_type;
	}

	short	 Encode(char *buf)	// encode to buf - return length encoded
	{
		FASTTrackData::Encode(buf);	// will set idx for us
	buf[idx++] = m_ESN.length();
	memcpy(&buf[idx], m_ESN.c_str(), m_ESN.length()); idx += m_ESN.length();
	buf[idx++] = m_ESN_type;
		memcpy(&buf[idx], &m_SPN, 4); idx += 4;
		memcpy(&buf[idx], &m_FMI, 4); idx += 4;
		memcpy(&buf[idx], &m_OC,	4); idx += 4;
		return idx;
	}
};

class FTData_J1939Status_ESN : public FASTTrackData
{
public:
	
private:
	char m_strStatus[512];
	string m_ESN;
	unsigned char m_ESN_type;

public:
	FTData_J1939Status_ESN()
	{
	m_Type = STS_J1939_STATUS_ESN;	// record id type in FASTTrackData.h
		strcpy(m_strEventType, "J1939 Status");
		strcpy(m_CSVHeader, "Type, Status");
	}

	void SetData
	(
		NMEA_DATA gps, 
	string strUserData,
	string esn,
	const unsigned char esn_type
	)
	{
		SetGPS(gps);
	m_ESN = esn;
	m_ESN_type = esn_type;
		if (strUserData.length() > 180)
		{
			strncpy(m_strStatus, strUserData.c_str(), 180);
			m_strStatus[180] = '\0';
		}
		else
			strcpy(m_strStatus, strUserData.c_str());
	}

	short	 Encode(char *buf)	// encode to buf - return length encoded
	{
		FASTTrackData::Encode(buf);	// will set idx for us
	buf[idx++] = m_ESN.length();
	memcpy(&buf[idx], m_ESN.c_str(), m_ESN.length()); idx += m_ESN.length();
	buf[idx++] = m_ESN_type;
		buf[idx++] = strlen(m_strStatus);
		memcpy(&buf[idx], &m_strStatus, strlen(m_strStatus)); idx += strlen(m_strStatus);
		return idx;
	}

};

class FTData_J1939Status2_ESN : public FASTTrackData
{
public:
	
private:
	string m_strHours;
	string m_strRPM;
	string m_strOilPress;
	string m_strCoolantTemp;
	string m_ESN;
	unsigned char m_ESN_type;

public:
	FTData_J1939Status2_ESN()
	{
	m_Type = STS_J1939_STATUS2_ESN;	// record id type in FASTTrackData.h
		strcpy(m_strEventType, "J1939 Status");
	}

	void SetData
	(
		NMEA_DATA gps, 
		string strHours,
		string strRPM,
		string strOilPress,
	string strCoolantTemp,
	const string esn,
	const unsigned char esn_type
	)
	{
		SetGPS(gps);
		m_strHours = strHours;
		m_strRPM = strRPM;
		m_strOilPress = strOilPress;
		m_strCoolantTemp = strCoolantTemp;
	m_ESN = esn;
	m_ESN_type = esn_type;
	}

	short	 Encode(char *buf)	// encode to buf - return length encoded
	{
		FASTTrackData::Encode(buf);	// will set idx for us
	EncodeString(m_ESN, buf);
	buf[idx++] = m_ESN_type;
		EncodeString(m_strHours, buf);
		EncodeString(m_strRPM, buf);
		EncodeString(m_strOilPress, buf);
		EncodeString(m_strCoolantTemp, buf);
		
		return idx;
	}

	void EncodeString(string str, char * buf)
	{
		int len = str.length();
		buf[idx++] = (unsigned char)(len);
		memcpy(&buf[idx], str.c_str(), len); idx += len;
	}

};



