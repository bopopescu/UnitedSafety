#pragma once
//---------------------------------------------------------------------------------
// A LensPeer is the Instrument, plus all the stuff needed to connect to the 
// network, disconnect, set status to 'Lost' etc.
//
// Typical usage: A brief InstrumentStatusData message comes in with a new MAC.
//   - we create a LensPeer and give it the message.  It assigns the MAC address
//   and begins the process of JOINING (need 3 messages to join).  Once joined it
//   begins IDENTIFYING - this is the process of send 0XA5 messages until the unit
//   responds.  Once this happens we notify INet and move to JOINED.
//   Once JOINED we must get a message from the Instrument every so often or we go to 
//   Lost and send an INet message.  If  we get a "LEAVING" message we notify INet and
//   drop the LensPeer from the network.
#include <rapidjson/document.h>
#include "AFS_Timer.h"
#include "NMEA_Client.h"

#include "InstrumentStatus.h"
#include "InstrumentIdentifyGeneral.h"
#include "InstrumentIdentifyFrequent.h"
#include "InstrumentIdentifySensor.h"
#include "InstrumentGPSData.h"

typedef enum
{
	UNKNOWN,
	JOINING,
	IDENTIFYING,
	SENSING,			// Full sensor message received
	JOINED,
	LEAVING,
	LOST,
	GATEWAY
} ConnectionState;


class LensPeer
{
private:
	ConnectionState m_ConnectionState;
	AFS_Timer m_LastMessageTime; // last time a create or update message was sent.
	AFS_Timer m_ContactTime;  // last time the instrument sent data
	AFS_Timer m_GPSMessageTime;

	int m_MsgCount;	
	Lens* m_pLens;
	bool m_bIdentifyGeneral;  // set to true when the IdentifyGeneral message received
	bool m_bIdentifySensor;  // set to true when the IdentifySensor message received
	bool m_bIdentifyFrequent;  // set to true when the IdentifyFrequent message received
	bool m_bIsLost;  // true if the unit has been reported lost.
	int m_Sequence;
	bool m_bSensorsChanged; // true if the definition of the sensors have changed (not the readings)  See ISCP-147

	InstrumentIdentifyGeneral m_IDGeneral;  // last message received. Compare incoming for changes.
	InstrumentIdentifySensor m_IDSensor;  // last sensor ID message received. Compare incoming for changes.
	InstrumentIdentifyFrequent m_IDFrequent;  // last message received. Compare incoming for changes.
	InstrumentStatus m_InstrumentStatus;  // last sensor data message received.
	InstrumentGPSData m_InstrumentGPSData; // last GPS data message received.
	LensMAC m_MAC;
	NMEA_Client nmea_client;

public:
	LensPeer(Lens* lens) : m_ConnectionState(UNKNOWN), m_pLens(lens), m_bIdentifyGeneral(false), m_bIdentifySensor(false), m_Sequence(0), m_bSensorsChanged(false) {}
	~LensPeer(){}; 
	
	int HandleInstrumentStatusData(InstrumentStatus & iStatus);
	int HandleIdentifyGeneral(InstrumentIdentifyGeneral & iGen);
	int HandleIdentifySensor(InstrumentIdentifySensor & iID);
	int HandleIdentifyFrequent(InstrumentIdentifyFrequent & iFreq);
	int HandlePeerGPS(InstrumentGPSData &iGPS);
	void SendIdentifyRequest(char *mac);
	int SendINetCreateInstrumentState();
	int SendINetUpdateInstrumentState();
	
	InstrumentStatus &GetInstrumentStatus(){return m_InstrumentStatus;}
	
	ats::String GetTimestamp();
	void SetMAC(const LensMAC & mac){m_MAC = mac;}
	int CheckLostStatus();
	int SendINetLostInstrument();

	void SensorChanged(){m_bSensorsChanged = true;}
	bool IsSensorChanged() const {return m_bSensorsChanged;}
	void ClearSensorChanged(){m_bSensorsChanged = false;}
	
	bool IsGPSValid() const {return m_InstrumentGPSData.IsValid();}
	LensMAC GetMAC(){return m_MAC;};
	
	bool IsGateway(){return (m_ConnectionState == GATEWAY);}
	void UpdateLostTimer(){m_ContactTime.SetTime();}

private:
	ats::String toHex(const rapidjson::Document& dom);
	void RequestVerbose(InstrumentStatus iStatus);
	void Log(const rapidjson::Value& val);
	void Log(const rapidjson::Document& dom);
	int GetStateFlag();
};

