#pragma once
// InstrumentGPSData - stores data from the Peer GPS Data message (0x84)
//
#include <LensMAC.h>

#pragma pack (1)

class GPSDataPacket
{
public:
	unsigned char m_MessageType;
	unsigned char m_Length;
	unsigned char m_MAC[3];
	unsigned char m_Length2;
	unsigned char m_Time[10];
	unsigned char m_Lat[9]; // dddmm.mmmm
	unsigned char m_NS; // North south indicator (N or S)
	unsigned char m_Lon[10]; // ddmm.mmmm
	unsigned char m_EW; // East/West indicator (E or W)
	unsigned char m_HDOP;
};
#pragma pack()


class InstrumentGPSData
{
private:
	LensMAC m_MAC;
	GPSDataPacket m_GPS;
	double m_Lat, m_Lon;
	int m_HDOP;
	bool m_IsValid; // True if a GPS message has been received.  False indicates using the TruLink GPS data
public:
	InstrumentGPSData() : m_IsValid(false) {};
	int Decode(const unsigned char *rawData); // take a raw packet (including frame) and decode it
	std::string GetMAC(){return m_MAC.toHex();};
	bool IsValid() const {return m_IsValid;}  // true if a GPS data record has been received.
	double Lat(){return m_Lat;}
	double Lon(){return m_Lon;}
	int HDOP(){return m_HDOP;}
private:
	int CheckBuffer(const unsigned char *rawData); // check that the framing and checksum are OK
};

