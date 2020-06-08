#include "DeviceBase.h"
#include "FTData_Debug.h"
#include "FTData_Generic.h"
#include "FTData_Speeding.h"
#include "FTData_J1939.h"
#include "FTData_Unhandled.h"
#include "AFF_IPC.h"
#include "ats-string.h"

class FTDebug : public DeviceBase
{
public:
	FTDebug()
	{
		strcpy(m_DevName, "FTDebug");
	};

	bool IsSending(){return GetAFF().IsSending();};

	void SendDebug(const FTData_Debug::DEBUG_PROCESS_IDS processID, const FTData_Debug::DEBUG_STATUS status, const unsigned short userData)
	{
		FTData_Debug myEvent;
		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(GetNMEA().GetData(), processID, status, userData);
		SendEvent();
	};

	void SendDebug(const NMEA_DATA &_nmea, const FTData_Debug::DEBUG_PROCESS_IDS processID, const FTData_Debug::DEBUG_STATUS status, const unsigned short userData)
	{
		FTData_Debug myEvent;
		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(_nmea, processID, status, userData);
		SendEvent();
	};

	void SendIgnitionOn()
	{
		FTData_Generic myEvent("IgnitionOn");
		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(GetNMEA().GetData(), STS_IGNITION_ON);
		SendEvent();
	}

	void SendIgnitionOn(const NMEA_DATA &_nmea)
	{
		FTData_Generic myEvent("IgnitionOn");
		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(_nmea, STS_IGNITION_ON);
		SendEvent();
	}

	void SendIgnitionOff(const NMEA_DATA &_nmea)
	{
		FTData_Generic myEvent("IgnitionOff");
		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(_nmea, STS_IGNITION_OFF);
		SendEvent();
	}
	void SendSeatbeltOn(const NMEA_DATA &_nmea)
	{
		FTData_Generic myEvent("SeatbeltOn");
		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(_nmea, STS_SEATBELT_ON);
		SendEvent();
	}

	void SendSeatbeltOff(const NMEA_DATA &_nmea)
	{
		FTData_Generic myEvent("SeatbeltOff");
		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(_nmea, STS_SEATBELT_OFF);
		SendEvent();
	}


	void SendStarted(const NMEA_DATA &_nmea)	// started moving
	{
		FTData_Generic myEvent("Started");
		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(_nmea, STS_STARTED_MOVING);
		SendEvent();
	}

	void SendStopped(const NMEA_DATA &nmea)
	{
		FTData_Generic myEvent("Stopped");
		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(nmea, STS_STOPPED_MOVING);
		SendEvent();
	}

	void SendSpeeding(const NMEA_DATA &nmea, ats::String userData)
	{
		FTData_Speeding myEvent;
		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(nmea, userData);
		SendEvent();
	}

	void SendSpeedOK(const NMEA_DATA &nmea)
	{
		FTData_Generic myEvent("Speed_OK");
		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(nmea, STS_SPEED_OK);
		SendEvent();
	}

	void SendLowBattery(const NMEA_DATA &nmea)
	{
		FTData_Generic myEvent("LowBattery");
		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(nmea, STS_LOW_BATTERY);
		SendEvent();
	}

	void SendJ1939Status(const NMEA_DATA &nmea, string userData)
	{
		FTData_J1939Status myEvent;
		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(nmea, userData);
		SendEvent();
	}
	void SendJ1939Status2(const NMEA_DATA &nmea, string userData)
	{
		FTData_J1939Status2 myEvent;
		vector<string> v;

		split( v, userData, "," );

		string strHours, strRPM, strOilPress, strCoolantTemp;
		strHours = v[0];
		strRPM = v[1] + ", " + v[2];
		strOilPress = v[3] + ", " + v[4];
		strCoolantTemp = v[5] + ", " + v[6];
		
		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(nmea, strHours, strRPM, strOilPress, strCoolantTemp);
		SendEvent();
	}
	void SendJ1939Fault(const NMEA_DATA &nmea, string userData)
	{
		FTData_J1939Fault myEvent;
		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(nmea, userData);
		SendEvent();
	}
	
	void SendUnhandledMessage(const NMEA_DATA &nmea, const short msgType)
	{
		FTData_Unhandled myEvent;
		DeviceBase::SetMyEvent((FASTTrackData *) &myEvent);
		myEvent.SetData(nmea, msgType);
		SendEvent();
	}

	void SendHeartbeat(const NMEA_DATA &nmea)
	{
		FTData_Generic myEvent("Heartbeat");
		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(nmea, STS_HEARTBEAT);
		SendEvent();
	}
// ATS FIXME: ats-common/ats-string.h contains a "split" function. Try and use that to
// reduce code volume.
	void split( vector<string> & theStringVector,	/* Altered/returned value */
			 const	string	& theString,
			 const	string	& theDelimiter)
	{
		size_t	start = 0, end = 0;

		while ( end != string::npos)
		{
				end = theString.find( theDelimiter, start);

				// If at end, use length=maxLength.	Else use length=end-start.
				theStringVector.push_back( theString.substr( start,
											 (end == string::npos) ? string::npos : end - start));

				// If at end, use start=maxSize.	Else use start=end+delimiter.
				start = (	 ( end > (string::npos - theDelimiter.size()) )
									?	string::npos	:	end + theDelimiter.size());
		}
	}
};
