#pragma once
#include <string>
#include <LensMAC.h>
#include "lens.h"
//---------------------------------------------------------------------------------
// InstrumentStatus - provides decoding and iNet encoding for Instrument Status and
// Data message.
//
//  See 2.2.2.1 on page 20.
//  NOTE ISD_xxx will define the various enums for the data in the packet.
//

#pragma pack(1)

class SensorReadingPacket // bytewise match to the sensor reading portion of the Instrument Status msg.
{
public:
	unsigned char m_Posn;
	unsigned char m_GasReading[2];
	unsigned char m_Status;
};

class InstrumentStatusPacket // bytewise match to the sensor reading portion of the Instrument Status msg.
{
public:
	unsigned char m_MessageType;
	unsigned char m_Length;
	unsigned char m_MAC[3];
	unsigned char m_SignalStrength;
	unsigned char m_DataLength;
	unsigned char m_SeqNum;
	unsigned char m_InstrumentState;
	unsigned char m_Exceptions;
	unsigned char m_NumVerbose;
};
#pragma pack()

class InstrumentStatus
{
private:
	InstrumentStatusPacket m_ISP;
	unsigned char m_AlarmDetail;
	SensorReadingPacket	m_SRP[8];
	
	int m_NumSensors;
	Lens* m_pLens;
	LensMAC m_MAC;

public:
	InstrumentStatus(Lens* lens) : m_pLens(lens) {};
	InstrumentStatus() : m_pLens(NULL) {};
	~InstrumentStatus(){};
	
	int Decode(const unsigned char *rawData); // take a raw packet (including frame) and decode it
	bool HasChanged(InstrumentStatus & iStatus);  // return true if something important is different.
	bool HasSensorChanged(InstrumentStatus & iStatus);  // return true if something important is different.
	
	std::string GetMAC(){return m_MAC.toHex();};
	LensMAC MAC(){return m_MAC;}
	bool IsVerbose() {return (m_ISP.m_NumVerbose > 0 ? true : false );}
	
	int GetInstrumentState() const {return (int)m_ISP.m_InstrumentState;}
	int GetAlarmDetail() const { return (int)m_AlarmDetail;}
	int GetExceptions() const { return (int)m_ISP.m_Exceptions;}
	int GetSensorStatus(int idx) { return (int)m_SRP[idx].m_Status;}
	double GetGasReading(int idx, int ndec);
	
	// Feature/Exception byte flags
	bool IsInetConnected(){return (m_ISP.m_Exceptions & 0x80);}
	bool IsInstrumentInWarning(){return(m_ISP.m_Exceptions & 0x04);}
	bool IsInstrumentInAlarm(){return(m_ISP.m_InstrumentState == 0x09);}

	void Update(InstrumentStatus & iStatus);  // Update the first portion of the status from a short message
	bool IsInstrumentSensorInAlarm(InstrumentStatus & iStatus); //<ISCP-260>
	int InstrumentNumberOfSensors(){return m_NumSensors;} //<ISCP-260>
private:
	int CheckBuffer(const unsigned char *rawData); // check that the framing and checksum are OK
};

