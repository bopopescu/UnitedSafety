#include "geoconst.h"

#include "atslogger.h"
#include "packetizer.h"
#include "db-monitor.h"
#include "calampsmessage.h"

#include "packetizerIridiumMessage.h"
#include "FTDebug.h"
#include "FTData_Unhandled.h"

extern ATSLogger g_log;

PacketizerIridiumMessage::PacketizerIridiumMessage(ats::StringMap& sm)
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
	m_esn = sm.get("mobile_id");
	m_esn_type = sm.get_int("mobile_id_type");
	m_cell_imei = sm.get("cell_imei");
}


void PacketizerIridiumMessage::packetize(std::vector<char> &data)
{
	data.clear();
	char buf[256];
	FASTTrackData * myEvent;

	switch(m_MsgCode)
	{
	case TRAK_HEARTBEAT_MSG:
	{
		myEvent = new FTData_Generic("Heartbeat");
		DeviceBase::SetMyEvent(myEvent);
		((FTData_Generic *)myEvent)->SetData(m_nmea, STS_HEARTBEAT);
		break;
	}
	case TRAK_SCHEDULED_MSG:
	case TRAK_PING_MSG:
	{
		if(!m_esn.empty() && (m_esn != m_cell_imei))
		{
			myEvent = new FTData_Debug_ESN();
			DeviceBase::SetMyEvent(myEvent);
			((FTData_Debug_ESN *)myEvent)->SetData(m_nmea, FTData_Debug_ESN::ID_PositionUpdate, FTData_Debug_ESN::PU_TIME, 0, m_esn, m_esn_type);
		}
		else
		{
			myEvent = new FTData_Debug();
			DeviceBase::SetMyEvent(myEvent);
			((FTData_Debug *)myEvent)->SetData(m_nmea, FTData_Debug::ID_PositionUpdate, FTData_Debug::PU_TIME, 0);
		}
		break;
	}
	case TRAK_STOP_COND_MSG:
	{
		if(!m_esn.empty() && (m_esn != m_cell_imei))
		{
			myEvent = new FTData_Generic_ESN("Stopped");
			DeviceBase::SetMyEvent(myEvent);
			((FTData_Generic_ESN *)myEvent)->SetData(m_nmea, STS_STOPPED_MOVING_ESN, m_esn, m_esn_type);
		}
		else
		{
			myEvent = new FTData_Generic("Stopped");
			DeviceBase::SetMyEvent(myEvent);
			((FTData_Generic *)myEvent)->SetData(m_nmea, STS_STOPPED_MOVING);
		}
		break;
	}
	case TRAK_START_COND_MSG:
	{
		if(!m_esn.empty() && (m_esn != m_cell_imei))
		{
			myEvent = new FTData_Generic_ESN("Started");
			DeviceBase::SetMyEvent(myEvent);
			((FTData_Generic_ESN *)myEvent)->SetData(m_nmea, STS_STARTED_MOVING_ESN, m_esn, m_esn_type);
		}
		else
		{
			myEvent = new FTData_Generic("Started");
			DeviceBase::SetMyEvent(myEvent);
			((FTData_Generic *)myEvent)->SetData(m_nmea, STS_STARTED_MOVING);
		}
		break;
	}
	case TRAK_IGNITION_ON_MSG:
	{
		if(!m_esn.empty() && (m_esn != m_cell_imei))
		{
			myEvent = new FTData_Generic_ESN("IgnitionOn");
			DeviceBase::SetMyEvent(myEvent);
			((FTData_Generic_ESN *)myEvent)->SetData(m_nmea, STS_IGNITION_ON_ESN, m_esn, m_esn_type);
		}
		else
		{
			myEvent = new FTData_Generic("IgnitionOn");
			DeviceBase::SetMyEvent(myEvent);
			((FTData_Generic *)myEvent)->SetData(m_nmea, STS_IGNITION_ON);
		}
		break;
	}
	case TRAK_SOS_MSG:
	{
		myEvent = new FTData_Generic("SOS");
		DeviceBase::SetMyEvent(myEvent);
		((FTData_Generic *)myEvent)->SetData(m_nmea, STS_SOS);
		break;
	}

	case TRAK_IRIDIUM_OVERLIMIT_MSG:
	{
		{
			myEvent = new FTData_Generic("IridiumOverLimit");
			DeviceBase::SetMyEvent(myEvent);
			((FTData_Generic *)myEvent)->SetData(m_nmea, STS_IRIDIUM_OVERLIMIT);
		}
		break;
	}
	case TRAK_IGNITION_OFF_MSG:
	{
		if(!m_esn.empty() && (m_esn != m_cell_imei))
		{
			myEvent = new FTData_Generic_ESN("IgnitionOff");
			DeviceBase::SetMyEvent(myEvent);
			((FTData_Generic_ESN *)myEvent)->SetData(m_nmea, STS_IGNITION_OFF_ESN, m_esn, m_esn_type);
		}
		else
		{
			myEvent = new FTData_Generic("IgnitionOff");
			DeviceBase::SetMyEvent(myEvent);
			((FTData_Generic *)myEvent)->SetData(m_nmea, STS_IGNITION_OFF);
		}
		break;
	}
	case TRAK_SENSOR_MSG:
	{
		if(m_SeatbeltOn & 0x01)
		{
			if(!m_esn.empty() && (m_esn != m_cell_imei))
			{
				myEvent = new FTData_Generic_ESN("SeatbeltOn");
				DeviceBase::SetMyEvent(myEvent);
				((FTData_Generic_ESN *)myEvent)->SetData(m_nmea,STS_SEATBELT_ON_ESN, m_esn, m_esn_type);
			}
			else
			{
				myEvent = new FTData_Generic("SeatbeltOn");
				DeviceBase::SetMyEvent(myEvent);
				((FTData_Generic *)myEvent)->SetData(m_nmea, STS_SEATBELT_ON);
			}
		}
		else
		{
			if(!m_esn.empty() && (m_esn != m_cell_imei))
			{

				myEvent = new FTData_Generic_ESN("SeatbeltOff");
				DeviceBase::SetMyEvent(myEvent);
				((FTData_Generic_ESN *)myEvent)->SetData(m_nmea, STS_SEATBELT_OFF_ESN, m_esn, m_esn_type);
			}
			else
			{
				myEvent = new FTData_Generic("SeatbeltOff");
				DeviceBase::SetMyEvent(myEvent);
				((FTData_Generic *)myEvent)->SetData(m_nmea, STS_SEATBELT_OFF);
			}
		}
		break;
	}
	case TRAK_LOW_BATTERY_MSG:
	case TRAK_CRITICAL_BATTERY_MSG:
	{
		if(!m_esn.empty() && (m_esn != m_cell_imei))
		{
			myEvent = new FTData_Generic_ESN("LowBattery");
			DeviceBase::SetMyEvent(myEvent);
			((FTData_Generic_ESN *)myEvent)->SetData(m_nmea, STS_LOW_BATTERY_ESN, m_esn, m_esn_type);
		}
		else
		{
			myEvent = new FTData_Generic("LowBattery");
			DeviceBase::SetMyEvent(myEvent);
			((FTData_Generic *)myEvent)->SetData(m_nmea, STS_LOW_BATTERY);
		}
		break;
	}
	case TRAK_J1939_STATUS2_MSG:
	{
		vector<string> v;

		split( v, m_MsgUserData, "," );

		string strHours, strRPM, strOilPress, strCoolantTemp;
		strHours = v[0];
		strRPM = v[1] + ", " + v[2];
		strOilPress = v[3] + ", " + v[4];
		strCoolantTemp = v[5] + ", " + v[6];
		if(!m_esn.empty() && (m_esn != m_cell_imei))
		{
			myEvent = new FTData_J1939Status2_ESN();
			DeviceBase::SetMyEvent(myEvent);
			((FTData_J1939Status2_ESN *)myEvent)->SetData(m_nmea, strHours, strRPM, strOilPress, strCoolantTemp, m_esn, m_esn_type);
		}
		else
		{
			myEvent = new FTData_J1939Status2();
			DeviceBase::SetMyEvent(myEvent);
			((FTData_J1939Status2 *)myEvent)->SetData(m_nmea, strHours, strRPM, strOilPress, strCoolantTemp);
		}
		break;
	}
	case TRAK_CALAMP_USER_MSG:
	{
		myEvent = new FTData_UserData_ESN();
		DeviceBase::SetMyEvent(myEvent);
		if(!m_esn.empty() && (m_esn != m_cell_imei))
		{
			((FTData_UserData_ESN *)myEvent)->SetData(m_nmea, m_MsgUserData, m_esn, m_esn_type);
		}
		else
		{
			((FTData_UserData_ESN *)myEvent)->SetData(m_nmea, m_MsgUserData, m_cell_imei, CalAmpsMessage::OptionsHeader::MOBILE_ID_IMEI);
		}
		break;
	}
	default:
		ats_logf(ATSLOG(0), "%s,%d: Failed to packetize: event = %d", __FILE__, __LINE__, m_MsgCode);
		return;
	}
	short len = GetEventBuf(buf,256);
	data.resize(len);
	for(int i =0; i< len ; i++)
	{
		data[i] = buf[i];
	}
	delete myEvent;
	return;

}
void PacketizerIridiumMessage::split( vector<string> & theStringVector,  /* Altered/returned value */
	   const  string  & theString,
	   const  string  & theDelimiter)
{
  size_t  start = 0, end = 0;

  while ( end != string::npos)
  {
	end = theString.find( theDelimiter, start);

	// If at end, use length=maxLength.  Else use length=end-start.
	theStringVector.push_back( theString.substr( start,
						   (end == string::npos) ? string::npos : end - start));

		// If at end, use start=maxSize.  Else use start=end+delimiter.
	start = (   ( end > (string::npos - theDelimiter.size()) )
				  ?  string::npos  :  end + theDelimiter.size());
  }
}

