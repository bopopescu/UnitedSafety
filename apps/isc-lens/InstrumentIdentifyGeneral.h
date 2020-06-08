#pragma once
#include <string>
#include <LensMAC.h>
#include "lens.h"
#include "InstrumentIdentifyFrequent.h"
//---------------------------------------------------------------------------------
// InstrumentIdentifyGeneral - provides decoding and iNet encoding for Instrument Status and
// Data message.
//
// page 27 - 2.2.2.6 Identify General Information.
//  NOTE ISD_xxx will define the various enums for the data in the packet.
//

#pragma pack(1)

class IdentifyGeneralPacket // bytewise match to the sensor reading portion of the Instrument Status msg.
{
public:
	unsigned char m_Posn;
	unsigned char m_GasReading[2];
	unsigned char m_Status;
};

class InstrumentGeneralPacket // bytewise match to the identify general portion of the Instrument Status msg.
{
public:
	unsigned char m_MessageType;
	unsigned char m_Length;
	unsigned char m_MAC8[8];
	unsigned char m_RadioProtocol[3];
	unsigned char m_SN[16];
	unsigned char m_DeviceType;
	unsigned char m_DeviceSubType;
	unsigned char m_HW;
	unsigned char m_FW[3];
	unsigned char m_RadioHW;
	unsigned char m_RadioOS[3];
	unsigned char m_FieldLengthA;
};
#pragma pack()

class InstrumentIdentifyGeneral
{
private:
	InstrumentGeneralPacket m_IGP;
	char m_UserName[24];
	char m_SiteName[24];
	bool m_bIsInetEnabled;
	std::string m_SN;

	Lens* m_pLens;
	LensMAC m_MAC;
public:
	InstrumentIdentifyGeneral() : m_pLens(NULL) 
	{
		m_UserName[0] = 0;
		m_SiteName[0] = 0;
		m_bIsInetEnabled = false;
	};
	InstrumentIdentifyGeneral(Lens* lens) : m_pLens(lens) {};
	~InstrumentIdentifyGeneral(){};
	
	InstrumentIdentifyGeneral& operator=(const InstrumentIdentifyGeneral& rhs);
	InstrumentIdentifyGeneral& operator=(const InstrumentIdentifyFrequent& rhs);
	
	int Decode(const unsigned char *rawData); // take a raw packet (including frame) and decode it

	std::string GetUserName(){std::string ret = m_UserName; return ret;}
	std::string GetSiteName(){std::string ret = m_SiteName; return ret;}
	
	std::string GetSN() const {return m_SN;}
	std::string GetMAC(){return m_MAC.toHex();}
	LensMAC CopyMAC(){return m_MAC;}
	bool IsInetEnabled(){return m_bIsInetEnabled;}
	int GetInstDeviceType(){return m_IGP.m_DeviceType;} 
	int GetInstDeviceSubType(){return m_IGP.m_DeviceSubType;} 
	
private:
	int CheckBuffer(const unsigned char *rawData); // check that the framing and checksum are OK
};

