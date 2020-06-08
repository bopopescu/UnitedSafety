#pragma once

enum  LSM_InstrumentState
{
	LSMIS_RUNNING = 0x01,
	LSMIS_SYSTEM_ALARM = 0x08,
	LSMIS_ERROR = 0x0A,
	LSMIS_LEAVING = 0x1B
};

enum LSM_EXCEPTIONS
{	
	LSMEX_GATEWAY_DEVICE = (1<<4) 
};
enum LSM_POWER_SOURCE
{
	LSMPS_BATTERY = 0x00,
	LSMPS_DCSOURCE = 0x01,
	LSMPS_ISSOURCE = 0x02
};
enum LSM_INET_CONNECTION
{
	LSMIC_NONE,
	LSMIC_ETHERNET,
	LSMIC_WIFI,
	LSMIC_CELLULAR,
	LSMIC_SATELLITE
};

#pragma pack(1)

struct MacVal
{
	char mac[3];
	char inetStatus;
};

class LensStatusMem
{
private:
	char m_NewData; 
	char m_Length;
	char m_Seq;
	char m_InstrumentState;  // map to LSM_InstrumentState but defined as char for packing reasons.
	char m_Exceptions;  // Exceptions/Features - mat pto LSM_EXCEPTIONS but defined as char for packing reasons.
	char m_NumVerbose;
	char m_AlarmDetail;
	char m_Battery;
	char m_PowerSource;
	char m_INetConnection;
	char m_INetConnectionStrength;
	char m_GPSSatelliteCount;
	char m_NumMAC;
	char m_CheckSum;
	MacVal m_MACs[4];

public:
	//----------------------
	LensStatusMem()
	{
		m_NewData = 0; 
		m_Length= 12;;
		m_Seq = 1;
		m_InstrumentState = (char)LSMIS_RUNNING  ;
		m_Exceptions = (char)LSMEX_GATEWAY_DEVICE;
		m_NumVerbose = 0;
		m_AlarmDetail = 0;
		m_Battery = 99;
		m_PowerSource = (char)LSMPS_BATTERY;
		m_INetConnection = LSMIC_NONE;
		m_INetConnectionStrength = 0;
		m_GPSSatelliteCount = 0;
		m_NumMAC = 0;
		m_CheckSum = 0;
	}

	//----------------------
	~LensStatusMem()
	{
	}
	void CheckSum()
	{
		m_CheckSum = (char)(m_NewData + m_Length + m_Seq + m_InstrumentState + m_Exceptions +
									m_AlarmDetail + m_Battery + m_PowerSource + m_INetConnection + m_INetConnectionStrength + 
									m_GPSSatelliteCount + m_NumMAC);
									
		for (short i = 0; i < m_NumMAC; i++)
			m_CheckSum = (char)(m_CheckSum + m_MACs[i].inetStatus + m_MACs[i].mac[0] + m_MACs[i].mac[1] + m_MACs[i].mac[2]);
	}	
};
#pragma pack()
