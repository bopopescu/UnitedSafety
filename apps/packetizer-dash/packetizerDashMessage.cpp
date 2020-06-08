#include "packetizerDashMessage.h"
#include "FTData_Unhandled.h"
PacketizerDashMessage::PacketizerDashMessage(ats::StringMap& sm)
{
	strcpy(m_DevName, "PacketizerIridiumMessage");
	m_MsgCode = (TRAK_MESSAGE_TYPE)sm.get_int("event_type");
	m_nmea.SetTimeFromDateTimeString(sm.get("event_time").c_str());
	m_nmea.ddLat = sm.get_double("latitude");
	m_nmea.ddLon = sm.get_double("longitude");
	m_nmea.H = sm.get_double("altitude");
	m_nmea.ddCOG = sm.get_double("heading");
	m_nmea.sog = (sm.get_double("speed")/ MS_TO_KPH) * MS_TO_KNOTS ;
	m_nmea.hdop = sm.get_double("hdop");
	m_nmea.num_svs = sm.get_double("satellites");
	m_nmea.gps_quality = sm.get_double("fix_status");
	m_SeatbeltOn = sm.get_int("inputs");
	m_MsgUserData = sm.get("usr_msg_data");
}


void PacketizerDashMessage::packetize(std::vector<char> &data)
{
	data.clear();
	char buf[256];
	switch(m_MsgCode)
	{
	case TRAK_HEARTBEAT_MSG:
	case TRAK_SCHEDULED_MSG:
	case TRAK_PING_MSG:
	{
		FTData_Debug myEvent;
		DeviceBase::SetMyEvent((FASTTrackData *) &myEvent);
		myEvent.SetData(m_nmea, FTData_Debug::ID_PositionUpdate, FTData_Debug::PU_TIME, 0);
		break;
	}
	case TRAK_STOP_COND_MSG:
	{
		FTData_Generic myEvent("Stopped");
		DeviceBase::SetMyEvent((FASTTrackData *) &myEvent);
		myEvent.SetData(m_nmea, STS_STOPPED_MOVING);
		break;
	}
	case TRAK_START_COND_MSG:
	{
		FTData_Generic myEvent("Started");
		DeviceBase::SetMyEvent((FASTTrackData *) &myEvent);
		myEvent.SetData(m_nmea, STS_STARTED_MOVING);
		break;
	}
	case TRAK_IGNITION_ON_MSG:
	{
		FTData_Generic myEvent("IgnitionOn");
		DeviceBase::SetMyEvent((FASTTrackData *) &myEvent);
		myEvent.SetData(m_nmea, STS_IGNITION_ON);
		break;
	}
	case TRAK_IGNITION_OFF_MSG:
	{
		FTData_Generic myEvent("IgnitionOff");
		DeviceBase::SetMyEvent((FASTTrackData *) &myEvent);
		myEvent.SetData(m_nmea, STS_IGNITION_OFF);
		break;
	}
	case TRAK_SENSOR_MSG:
	{
		if(m_SeatbeltOn & 0x01)
		{
			FTData_Generic myEvent("SeatbeltOn");
			DeviceBase::SetMyEvent((FASTTrackData *) &myEvent);
			myEvent.SetData(m_nmea, STS_SEATBELT_ON);
		}
		else
		{
			FTData_Generic myEvent("SeatbeltOff");
			DeviceBase::SetMyEvent((FASTTrackData *) &myEvent);
			myEvent.SetData(m_nmea, STS_SEATBELT_OFF);
		}
		break;
	}
	case TRAK_LOW_BATTERY_MSG:
	case TRAK_CRITICAL_BATTERY_MSG:
	{
		FTData_Generic myEvent("LowBattery");
		DeviceBase::SetMyEvent((FASTTrackData *) &myEvent);
		myEvent.SetData(m_nmea, STS_LOW_BATTERY);
		break;
	}
	case TRAK_J1939_STATUS2_MSG:
	{
		FTData_J1939Status2 myEvent;
		vector<ats::String> v;

		ats::split(v, m_MsgUserData, ",");

		string strHours, strRPM, strOilPress, strCoolantTemp;
		strHours = v[0];
		strRPM = v[1] + ", " + v[2];
		strOilPress = v[3] + ", " + v[4];
		strCoolantTemp = v[5] + ", " + v[6];

		DeviceBase::SetMyEvent((FASTTrackData *)&myEvent);
		myEvent.SetData(m_nmea, strHours, strRPM, strOilPress, strCoolantTemp);
		break;
	}
	default:
	{
		FTData_Unhandled myEvent;
		DeviceBase::SetMyEvent((FASTTrackData *) &myEvent);
		myEvent.SetData(m_nmea, m_MsgCode);
	  break;
	}
	}
	short len = GetEventBuf(buf,256);
	data.resize(len + 2);
	data[0] = (unsigned char)((0xFF00 & len)>>8);
	data[1] = (unsigned char)(0xFF & len);
	for(int i=2; i< len + 2 ; i++)
	{
		data[i] = buf[i-2];
	}
	return;
}
