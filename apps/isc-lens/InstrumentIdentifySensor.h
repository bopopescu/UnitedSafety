#pragma once
#include "lens.h"
#include <LensMAC.h>

//---------------------------------------------------------------------------------
// InstrumentIdentifySensor - provides decoding and iNet encoding for 
//		the Identify Sensor Data message.
//
//  See 2.2.2.7 on page 29.
//
#pragma pack(1)
class SensorPacket
{
public:
	unsigned char m_Type;
	unsigned char m_GasType;
	unsigned char m_Units;
	unsigned char m_Decimals;
	unsigned char m_HighAlarm[2];
	unsigned char m_LowAlarm[2];
	unsigned char m_TWA[2];
	unsigned char m_STEL[2];
public:
	int UnitsOfMeasurement()
	{
		// ISCP-208 - have to fix the LEL reading.
		if ((m_Type & 0x0f) == 8) // SENSOR_TYPE_LEL
			return (int)(m_Units & 0x0F);
		else
			return (int)((m_Units >> 4) & 0x0F);
	}

	int GetMeasDecimalPlaces()
	{
		// ISCP-208 - have to fix the LEL reading.
		if ((m_Type & 0x0f) == 8) // SENSOR_TYPE_LEL
			return (int)((m_Decimals) & 0x0F);
		else
			return (int)((m_Decimals >> 4) & 0x0F);
	}
};

class IdentifySensorPacket
{
public:
	unsigned char m_MessageType;
	unsigned char m_Length1;
	unsigned char m_MAC[3];
	unsigned char m_Length;
	unsigned char m_TWATimeBase;
	unsigned char m_SensorCount;

};
#pragma pack()

class InstrumentIdentifySensor
{
private:
	IdentifySensorPacket m_ISP;
	SensorPacket m_Sensors[8];

	Lens* m_pLens;
	LensMAC m_MAC;
public:
	InstrumentIdentifySensor() : m_pLens(NULL) {}
	InstrumentIdentifySensor(Lens* lens) : m_pLens(lens) {};
	~InstrumentIdentifySensor(){};
	InstrumentIdentifySensor& operator=(const InstrumentIdentifySensor& rhs);

	int Decode(const unsigned char *rawData); // take a raw packet (including frame) and decode it

	std::string MACToHex();
	int NumSensors() const { return (int)m_ISP.m_SensorCount;}
	SensorPacket * GetSensor(int idx){return &m_Sensors[idx];}
	std::string SensorTypeJSON(int idx);
	std::string GasTypeJSON(int idx);
	int UnitsOfMeasurement(const int idx);
	int GetMeasDecimalPlaces(int idx);
	void Log();
private:
	int CheckBuffer(const unsigned char *rawData); // check that the framing and checksum are OK

};
