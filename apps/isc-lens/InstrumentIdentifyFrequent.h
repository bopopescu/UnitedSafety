#pragma once
#include <string>
#include <LensMAC.h>
#include "lens.h"

//---------------------------------------------------------------------------------
// InstrumentIdentifyFrequent - provides decoding and iNet encoding for Instrument Status and
// Data message.
//
// page 27 - 2.2.2.6 Identify General Information.
//  NOTE ISD_xxx will define the various enums for the data in the packet.
//

#pragma pack(1)

class InstrumentFrequentPacket // bytewise match to the identify general portion of the Identify Frequent msg.
{
public:
	unsigned char m_MessageType;
	unsigned char m_Length;
	unsigned char m_MAC[3];
	unsigned char m_Length2;
};
#pragma pack()

class InstrumentIdentifyFrequent
{
public:
	InstrumentFrequentPacket m_IFP;
	char m_UserName[24];
	char m_SiteName[24];
	bool m_bIsInetEnabled;

	Lens* m_pLens;
	LensMAC m_MAC;
public:
	InstrumentIdentifyFrequent() : m_pLens(NULL) 
	{
		m_UserName[0] = 0;
		m_SiteName[0] = 0;
		m_bIsInetEnabled = false;
	};
	InstrumentIdentifyFrequent(Lens* lens) : m_pLens(lens) {};
	~InstrumentIdentifyFrequent(){};
	
	InstrumentIdentifyFrequent& operator=(const InstrumentIdentifyFrequent& rhs);
	
	int Decode(const unsigned char *rawData); // take a raw packet (including frame) and decode it

	std::string GetUserName(){std::string ret = m_UserName; return ret;}
	std::string GetSiteName(){std::string ret = m_SiteName; return ret;}
	
	std::string GetMAC(){return m_MAC.toHex();}
	LensMAC CopyMAC(){return m_MAC;}
private:
	int CheckBuffer(const unsigned char *rawData); // check that the framing and checksum are OK
};

