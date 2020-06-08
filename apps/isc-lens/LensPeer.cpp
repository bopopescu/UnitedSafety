#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <boost/format.hpp>
#include "colordef.h"
#include "LensPeer.h"

#include <INET_IPC.h>
#include "INetConfig.h"
#include "rapidjson.h"

extern INetConfig *g_pLensParms;  // read the db-config parameters
extern INET_IPC g_INetIPC;  // common data.

#define TIMESTAMP 26

//------------------------------------------------------------------------------
//
//  returns 0: no messages to be sent
//			1: leaving group so delete the peer.


int LensPeer::HandleInstrumentStatusData(InstrumentStatus & iStatus)
{
	m_ContactTime.SetTime();  // used for LOST status
	char macbuf[8];
	int lCriticalIntervalMinutes = 1; 

	switch (m_ConnectionState)
	{
		case UNKNOWN:
			m_ConnectionState = JOINING;
			m_MsgCount = 1;
			break;
		case JOINING:
			ats_logf(ATSLOG_DEBUG, YELLOW_ON "(%s) JOINING" RESET_COLOR, iStatus.MAC().toHex().c_str());	
			m_MsgCount++;
			if (m_MsgCount == 3)
			{
				SendIdentifyRequest(iStatus.MAC().GetMac(macbuf) );
				m_ConnectionState = IDENTIFYING;
			}
			break;
		case IDENTIFYING:
			ats_logf(ATSLOG_DEBUG, YELLOW_ON "(%s) IDENTIFYING General:%s  Sensor:%s" RESET_COLOR, iStatus.MAC().toHex().c_str(), (m_bIdentifyGeneral ? "true": "false"), (m_bIdentifySensor ? "true": "false"));	
			if (m_bIdentifyGeneral && m_bIdentifySensor)
			{
				RequestVerbose(iStatus);
				m_ConnectionState = SENSING;
				g_INetIPC.Add(iStatus.MAC());
				m_MsgCount = 0;
				// Peer is detected // ISCP-321
				g_INetIPC.LENSisInConnectingState(false);
			}
			else
			{
				m_MsgCount++;
				if (m_MsgCount%6 == 0)  // keep sending identify request until unit responds.
				{
					char buf[8];
					SendIdentifyRequest(iStatus.MAC().GetMac(buf));
				}
			}
			break;
		case SENSING:  // waiting for a verbose Status message
				ats_logf(ATSLOG_DEBUG, YELLOW_ON "(%s) SENSING" RESET_COLOR, iStatus.MAC().toHex().c_str());	
				if (iStatus.IsVerbose())
				{
					m_pLens->IncrementPeerCount();
					m_MAC = m_IDGeneral.CopyMAC();

					ats_logf(ATSLOG_DEBUG, GREEN_ON "Adding instrument %s" RESET_COLOR, m_MAC.toHex().c_str());	
					m_ConnectionState = JOINED;
					m_InstrumentStatus = iStatus;
					SendINetCreateInstrumentState();
				}
				else if (++m_MsgCount % 5 == 0)  // if still getting brief messages request verbose again
				{
					m_ConnectionState = IDENTIFYING;
				}
			break;
		case JOINED:  //once here we check each incoming message against the last message and update the status based on changes.
			//ats_logf(ATSLOG_DEBUG, YELLOW_ON "Now JOINED" RESET_COLOR);

				//ISCP-311 Fix Sattelite Critical interval
			if( ( false == g_INetIPC.UsingIridium() ) && ( true == g_INetIPC.INetConnected() ) )
			{
				//ats_logf(ATSLOG_DEBUG, GREEN_ON " Critical Interval Set to Cellular Critical Interval Settings\n" RESET_COLOR);	
				lCriticalIntervalMinutes = g_pLensParms->CriticalIntervalMinutes();
			}
			else if( ( true == g_INetIPC.UsingIridium() ) && ( true == g_INetIPC.INetConnected() ) )
			{
				//ats_logf(ATSLOG_DEBUG, GREEN_ON " Critical Interval Set to Sattelite Critical Interval Settings\n" RESET_COLOR);	
				lCriticalIntervalMinutes = g_pLensParms->CriticalIntervalMinutes();
			}
			else
			{
				//ats_logf(ATSLOG_DEBUG, GREEN_ON "Critical Interval Set to largest Critical Interval Settings\n" RESET_COLOR);
				lCriticalIntervalMinutes = ( 
												( 
													( g_pLensParms->SatelliteCriticalIntervalMinutes() > g_pLensParms->CriticalIntervalMinutes() ) ? 
													g_pLensParms->SatelliteCriticalIntervalMinutes() : g_pLensParms->CriticalIntervalMinutes()
												)
								   			);
			}

			if (iStatus.GetInstrumentState() == 0x1b)  // is the instrument leaving the network.
			{
				m_ConnectionState = LEAVING;
				m_InstrumentStatus.Update(iStatus); // just save the short message part
				//;ats_logf(ATSLOG_INFO, YELLOW_ON "UpdateInstrument-0\n\r" RESET_COLOR);	//adnan
				SendINetUpdateInstrumentState();

				if (g_INetIPC.LeaveNetwork(m_MAC) != 0)
					ats_logf(ATSLOG_DEBUG, RED_ON "Unit was not able to leave the network - MAC %s not found in list" RESET_COLOR, m_MAC.toHex().c_str());
				else
					ats_logf(ATSLOG_DEBUG, YELLOW_ON "Unit %s has left the network." RESET_COLOR, m_MAC.toHex().c_str());

				m_pLens->DecrementPeerCount();
				m_bIdentifyGeneral = false;
				m_bIdentifySensor = false;
				
				return 1;
				break;
			}
	
			// check for low battery
			if (iStatus.GetExceptions() != m_InstrumentStatus.GetExceptions())
			{
				if (iStatus.IsVerbose())
					m_InstrumentStatus = iStatus;
				else
					m_InstrumentStatus.Update(iStatus); 		// just save the short message part

				//;ats_logf(ATSLOG_INFO, YELLOW_ON "UpdateInstrument-1\n\r" RESET_COLOR);	//adnan
				SendINetUpdateInstrumentState(); // just sends the brief message parts.					
			}
			else if (m_InstrumentStatus.HasChanged(iStatus) || IsSensorChanged())  // one of the sensors has changed alarm status so send out the createinst.
			{
				if (iStatus.IsVerbose() || IsSensorChanged())
				{
					m_InstrumentStatus = iStatus;
					SendINetCreateInstrumentState();  // sends the full set of sensor data
					ClearSensorChanged();
				}
				else
				{
					m_InstrumentStatus.Update(iStatus); 		// just save the short message part

					//;ats_logf(ATSLOG_INFO, YELLOW_ON "UpdateInstrument-2\n\r" RESET_COLOR);	//adnan
					SendINetUpdateInstrumentState(); // just sends the brief message parts.
				}
			}
			else if(m_InstrumentStatus.HasSensorChanged(iStatus)) //<ISCP-260>
			{
					ats_logf(ATSLOG_INFO, YELLOW_ON "JOINED State 4\n\r" RESET_COLOR);	
					m_InstrumentStatus = iStatus;
					SendINetCreateInstrumentState();  // sends the full set of sensor data
			}
			
			// ISCP-165 - 2.2.1.3.3.1 PRIN-EREQ-65 GPS Update Event
			else if (m_InstrumentGPSData.IsValid() && m_GPSMessageTime.DiffTime() > g_pLensParms->GPSUpdateMinutes() * 60 )
			{
			
				//;ats_logf(ATSLOG_INFO, YELLOW_ON "UpdateInstrument-3\n\r" RESET_COLOR);	//adnan
				SendINetUpdateInstrumentState(); // just sends the brief message parts.				
			}

			// ISCP-172: 2.2.3.2 PRIN-EREQ-19 Upload Critical Events
			else if ( iStatus.IsInstrumentInAlarm() && ( m_LastMessageTime.DiffTime() > ( lCriticalIntervalMinutes * 60 ) ) )
			{
				m_InstrumentStatus = iStatus;
				SendINetCreateInstrumentState(); // resend the whole message since we are in alarm.
				ats_logf(ATSLOG_DEBUG, GREEN_ON " Event Genrated after %d minutes Interval Settings\n" RESET_COLOR,lCriticalIntervalMinutes);			
			}
			else if ( (m_InstrumentStatus.IsInstrumentSensorInAlarm(iStatus) && (m_LastMessageTime.DiffTime() > (g_pLensParms->InstrumentKeepAliveMinutes() * 60) )  ) ) //<ISCP-260>
			{
				m_InstrumentStatus = iStatus;
				SendINetCreateInstrumentState(); // resend the whole message since we are in alarm.				
			}

			break;
		case LEAVING:
			return 1;
		case LOST:
			m_ConnectionState = JOINED;
			break;
		default:
			ats_logf(ATSLOG_DEBUG, RED_ON "Invalid peer status %d- setting to UNKNOWN" RESET_COLOR, m_ConnectionState);	
			m_ConnectionState = UNKNOWN;
			break;
	}

	if (iStatus.IsVerbose())  // only save the complete status if it is a verbose record.
		m_InstrumentStatus = iStatus;

	if ((iStatus.GetExceptions() & (1 <<7))) // inet connected
	{
		g_INetIPC.ClearNew(m_MAC);
	}
	return 0;
}


//------------------------------------------------------------------------------
void LensPeer::SendIdentifyRequest(char *mac)
{
	ats_logf(ATSLOG_INFO, "%s,%d: Requesting unit identify", __FILE__, __LINE__);
	const unsigned char identify_request[] = {0x24, 0x24, 0xA5, 0x05, 0x03, 0x00, 0x00, 0x00, 0x23, 0x23};

	char t[16];
	memcpy(t, identify_request, sizeof identify_request);
	memcpy(t + 5, mac, 3);
	m_pLens->send_to_outgoing_mailbox(t, 10);
}

//------------------------------------------------------------------------------
// 2.2.5.1 Instrument Status and Data.
//  This is the Trulink sending connection data, etc to specific attached units
//
/*
void LensPeer::SendStatusAndData(char *mac)
{
	ats_logf(ATSLOG_INFO, "%s,%d: Requesting unit identify", __FILE__, __LINE__);
	const unsigned char identify_request[] = {0x24, 0x24, 0xA5, 0x05, 0x03, 0x00, 0x00, 0x00, 0x23, 0x23};

	char t[16];
	memcpy(t, identify_request, sizeof identify_request);
	memcpy(t + 5, mac, 3);
	m_pLens->send_to_outgoing_mailbox(t, 10);
}
*/
//------------------------------------------------------------------------------
int LensPeer::HandleIdentifyGeneral(InstrumentIdentifyGeneral & iID)
{
	ats_logf(ATSLOG_DEBUG, "Handling Identify General" );	

	if (m_bIdentifyGeneral == false)
	{
		m_bIdentifyGeneral = true;  // we have received this message for this peer.
		m_IDGeneral = iID;  // just copy the first one directly.
		ats_logf(ATSLOG_DEBUG, YELLOW_ON "Adding IdentifyGeneral %s %s" RESET_COLOR,  m_IDGeneral.GetMAC().c_str(),  iID.GetMAC().c_str());	
	}

	if (m_ConnectionState == JOINED)
	{
		// check for changes that involve resending INet messages, SiteName

		if ((m_IDGeneral.GetSiteName() != iID.GetSiteName() ) || (m_IDGeneral.GetUserName() != iID.GetUserName()) )
		{
			m_IDGeneral = iID;  // just copy the first one directly.

			//;ats_logf(ATSLOG_INFO, YELLOW_ON "UpdateInstrument-4\n\r" RESET_COLOR);	//adnan
						if(!g_INetIPC.UsingIridium()) // ISCP-304
						{
							//;ats_logf(ATSLOG_INFO, YELLOW_ON "UpdateInstrument-6\n\r" RESET_COLOR);	//adnan
							SendINetUpdateInstrumentState();
						}
						else
						{
							SendINetCreateInstrumentState();
						}

			return 0;
		}
	}
	m_IDGeneral = iID;  // just copy the first one directly.
	
	return 0;
}

//------------------------------------------------------------------------------
int LensPeer::HandleIdentifyFrequent(InstrumentIdentifyFrequent & iFreq)
{
	ats_logf(ATSLOG_DEBUG, "Handling Identify Frequent" );	
	if (m_ConnectionState == JOINED)
	{
		// check for changes that involve resending INet messages, SiteName

		if ((m_IDGeneral.GetSiteName() != iFreq.GetSiteName() ) || (m_IDGeneral.GetUserName() != iFreq.GetUserName()) )
		{
			m_IDFrequent = iFreq;  // just copy the first one directly.
			m_IDGeneral = iFreq;
			if(!g_INetIPC.UsingIridium()) // ISCP-304
			{
				//;ats_logf(ATSLOG_INFO, YELLOW_ON "UpdateInstrument-5\n\r" RESET_COLOR);	//adnan
				SendINetUpdateInstrumentState();
			}
			else
			{
				SendINetCreateInstrumentState();
			}
			return 0;
		}
	}
	m_IDFrequent = iFreq;  // just copy the first one directly.
	
	return 0;
}


//------------------------------------------------------------------------------
int LensPeer::HandleIdentifySensor(InstrumentIdentifySensor & iID)
{
	ats_logf(ATSLOG_DEBUG, "Handling Identify Sensor" );	
	if (m_bIdentifySensor == false)
	{
		m_bIdentifySensor = true;  // we have received this message for this peer.
		m_IDSensor = iID;  // just copy the first one directly.
	}

	if (m_ConnectionState == JOINED)
	{
		// check for changes that involve resending INet messages, SiteName
		if (iID.NumSensors() != m_IDSensor.NumSensors())
		{
			SensorChanged();  // sets the sensor changed flag.
		}
		else
		{
			for (short i = 0; i < iID.NumSensors(); i++)
			{
				if (iID.GetSensor(i)->m_GasType != m_IDSensor.GetSensor(i)->m_GasType)
					SensorChanged();  // sets the sensor changed flag.
			}
		}
	}
	m_IDSensor = iID;  // just copy the first one directly.
	
	return 0;
}

//----------------------------------------------------------------------------------
//  We just hold onto this.  It will be sent out when an UpdateInstrument is sent
//	See ISCP-156
//
int LensPeer::HandlePeerGPS(InstrumentGPSData &iGPS)
{
	bool isvalid = m_InstrumentGPSData.IsValid(); // this will be false before the first GPS reading.
	m_InstrumentGPSData = iGPS;
	
	if (!isvalid && iGPS.IsValid())  // indicates that this it the first GPS - we want to send out an update
	{
		SendINetCreateInstrumentState(); 
	}
	return 0;
}

//------------------------------------------------------------------------------
// SendINetCreateInstrumentState - send the create instrument message.
//
int LensPeer::SendINetCreateInstrumentState()
{
	if ( !m_IDGeneral.IsInetEnabled() )	 // only send if iNet is enabled in General or Frequent message
	{
		ats_logf(ATSLOG_DEBUG, RED_ON "SendINetCreateInstrumentState - iNet is not enabled!" RESET_COLOR );	
		return 0;
	}
	else if( m_IDGeneral.GetInstDeviceType() == 0x15)
	{
		return 0;	
	}
	ats::String result;  // hex data for adding to message
	rapidjson::Document dom;
	rapidjson::Document::AllocatorType &allocator = dom.GetAllocator();
	dom.SetObject();
	std::string url;
	ats_sprintf(&url, "%s/iNetAPI/v1/live/%s/createinst", g_pLensParms->iNetURL().c_str(), g_pLensParms->SerialNum().c_str());
	std::string mac = m_MAC.toHex();
	dom.AddMember("url", rapidString(url), allocator);
	dom.AddMember("mac", rapidString(mac), allocator );

	// now build the data to be sent
	Value dataObj(kObjectType);
	dataObj.SetObject();

	ats::String ts = GetTimestamp();

	dataObj.AddMember("sn", rapidString(m_IDGeneral.GetSN()), allocator);
	dataObj.AddMember("t", rapidString(ts), allocator);
	dataObj.AddMember("seq", ++m_Sequence, allocator);
	dataObj.AddMember("s", GetStateFlag(), allocator);

	if (m_InstrumentGPSData.IsValid())
	{
			Value gpsObj(kObjectType);
			gpsObj.SetObject();
			gpsObj.AddMember("lat", m_InstrumentGPSData.Lat(), allocator);
			gpsObj.AddMember("lon", m_InstrumentGPSData.Lon(), allocator);
			gpsObj.AddMember("a", m_InstrumentGPSData.HDOP(), allocator);
			gpsObj.AddMember("s", "0", allocator);
			gpsObj.AddMember("b", "0", allocator);
			dataObj.AddMember("p", gpsObj, allocator);		
	}
	else
	{
		/*
		if(nmea_client.Lat() > -90 && nmea_client.Lat() < 90 && nmea_client.Lon() > -180 && nmea_client.Lon() < 180)
		{
			Value gpsObj(kObjectType);
			gpsObj.SetObject();
			gpsObj.AddMember("lat", nmea_client.Lat(), allocator);
			gpsObj.AddMember("lon", nmea_client.Lon(), allocator);
			gpsObj.AddMember("a", nmea_client.HDOP(), allocator);
			gpsObj.AddMember("s", nmea_client.SOG(), allocator);
			gpsObj.AddMember("b", nmea_client.COG(), allocator);
			dataObj.AddMember("p", gpsObj, allocator);
		}
		*/
	}

	Value sensorArray(kArrayType);
	sensorArray.SetArray();
	double sensorGasReading;
	std::string cc[8];
	std::string gc[8]; // do not change - some bug in rapidjson or something requires this.
	
	for (short i = 0; i < m_IDSensor.NumSensors(); i++)
	{
		Value sensorObj(kObjectType), vtmp(kObjectType);
		sensorObj.SetObject();
		cc[i] = m_IDSensor.SensorTypeJSON(i);
		vtmp.SetString(cc[i].c_str(),cc[i].size(), allocator);
		sensorObj.AddMember("cc", vtmp, allocator);
		gc[i] = m_IDSensor.GasTypeJSON(i);
		sensorObj.AddMember("gc", StringRef(gc[i].c_str(), gc[i].size()), allocator);
    sensorGasReading = m_InstrumentStatus.GetGasReading(i, m_IDSensor.GetMeasDecimalPlaces(i));
		sensorObj.AddMember("gr", sensorGasReading, allocator);
		sensorObj.AddMember("uom", m_IDSensor.UnitsOfMeasurement(i), allocator);
		sensorObj.AddMember("s", m_InstrumentStatus.GetSensorStatus(i), allocator);
		sensorArray.PushBack(sensorObj, allocator);
	}
	dataObj.AddMember("sensors", sensorArray, allocator);

	Value siteObj, userObj(kObjectType);

	if(!m_IDGeneral.GetSiteName().empty())
	{
		std::string site = m_IDGeneral.GetSiteName();
		siteObj.SetString(site.c_str(), allocator);
		dataObj.AddMember("site", siteObj, allocator);
	}
	if(!m_IDGeneral.GetUserName().empty())
	{
		std::string user = m_IDGeneral.GetUserName();
		userObj.SetString(user.c_str(), allocator);
		dataObj.AddMember("user", userObj, allocator);
	}

	dom.AddMember("data", dataObj, allocator);
	result = toHex(dom);
	send_redstone_ud_msg("message-assembler", 0, "msg inet_msg msg_priority=9 usr_msg_data=%s\r", result.c_str());
	m_GPSMessageTime.SetTime();  // reset the GPS timer since GPS will be sent out if this is a GPS based instrument.
	m_LastMessageTime.SetTime();  // reset when the last message to iNet was sent.
	return 0;
}

//------------------------------------------------------------------------------
int LensPeer::SendINetUpdateInstrumentState()
{
	if (!m_IDGeneral.IsInetEnabled() || (m_IDGeneral.GetInstDeviceType() == 0x15) )	 // only send if iNet is enabled in General or Frequent message
		return 0;

	ats::String result;  // hex data for adding to message
	int pri = 20;
	
	Document dom;
	Document::AllocatorType &allocator = dom.GetAllocator();
	dom.SetObject();
	std::string url;
	std::string id = g_INetIPC.FindInstrumentObjectID(m_MAC);
	ats_logf(ATSLOG_INFO, MAGENTA_ON "%s,%d:  ID %s for mac %s" RESET_COLOR, __FILE__, __LINE__, id.c_str(), m_MAC.toHex().c_str());
	
	// occasionally the id is 0.  Test here if that is the case and send a createinstrument message instead
	// Sept 2019 - added the UsingIridium check because we want to send it to the Iridium service as updateinst which is smaller byte count.
	// TODO: find why the id is 0!
	if ( !g_INetIPC.UsingIridium() && (id.length() < 13 || id.c_str()[0] < '4'))
//	uint64_t cutoff = uint64_t(4000000 * 1000000);
//	if ( !g_INetIPC.UsingIridium() && id < cutoff ) 
	{
		ats_logf(ATSLOG_INFO, RED_ON "%s,%d: We have Lost the ID %s" RESET_COLOR, __FILE__, __LINE__, id.c_str());
		return SendINetCreateInstrumentState(); // resend the whole message since we are in alarm.				
	}

	ats_sprintf(&url, "%s/iNetAPI/v1/live/%s/updateinst", g_pLensParms->iNetURL().c_str(), id.c_str());
	dom.AddMember("url", rapidString(url), allocator);
	dom.AddMember("mac", Value(m_MAC.toHex().c_str(), allocator).Move(), allocator);

	// now build the data to be sent
	Value dataObj(kObjectType);
	dataObj.SetObject();

	ats::String ts = GetTimestamp();

	std::string strSN = m_IDGeneral.GetSN();
	Value snObj;
	snObj.SetString(strSN.c_str(), allocator);
	dataObj.AddMember("sn", snObj, allocator);
	dataObj.AddMember("t", Value(ts.c_str(), allocator).Move(), allocator);
	dataObj.AddMember("seq", ++m_Sequence, allocator);
	dataObj.AddMember("s", GetStateFlag(), allocator);

	if (m_InstrumentGPSData.IsValid())
	{
			Value gpsObj(kObjectType);
			gpsObj.SetObject();
			gpsObj.AddMember("lat", m_InstrumentGPSData.Lat(), allocator);
			gpsObj.AddMember("lon", m_InstrumentGPSData.Lon(), allocator);
			gpsObj.AddMember("a", m_InstrumentGPSData.HDOP(), allocator);
			gpsObj.AddMember("s", "0", allocator);
			gpsObj.AddMember("b", "0", allocator);
			dataObj.AddMember("p", gpsObj, allocator);		
	}
	else
	{
		/*
		if(nmea_client.Lat() > -90 && nmea_client.Lat() < 90 && nmea_client.Lon() > -180 && nmea_client.Lon() < 180)
		{
			Value gpsObj(kObjectType);
			gpsObj.SetObject();
			gpsObj.AddMember("lat", nmea_client.Lat(), allocator);
			gpsObj.AddMember("lon", nmea_client.Lon(), allocator);
			gpsObj.AddMember("a", nmea_client.HDOP(), allocator);
			gpsObj.AddMember("s", nmea_client.SOG(), allocator);
			gpsObj.AddMember("b", nmea_client.COG(), allocator);
			dataObj.AddMember("p", gpsObj, allocator);
		}
		*/
	}

	Value siteObj, userObj(kObjectType);

	if(!m_IDGeneral.GetSiteName().empty())
	{
		std::string site = m_IDGeneral.GetSiteName();
		siteObj.SetString(site.c_str(), allocator);
		dataObj.AddMember("site", siteObj, allocator);
	}
	if(!m_IDGeneral.GetUserName().empty())
	{
		std::string user = m_IDGeneral.GetUserName();
		userObj.SetString(user.c_str(), allocator);
		dataObj.AddMember("user", userObj, allocator);
	}

	dom.AddMember("data", dataObj, allocator);
	result = toHex(dom);
	
	if (m_InstrumentStatus.GetAlarmDetail() && 0x0b) // Sensor, Panic or mandown bit raises the priority
		pri = 9;
	
	ats_logf(ATSLOG_INFO, WHITE_ON_BLUE "%s,%d: msg inet_msg  usr_msg_data=%s\r" RESET_COLOR, __FILE__, __LINE__, result.c_str());
	send_redstone_ud_msg("message-assembler", 0, "msg inet_msg  msg_priority=%d usr_msg_data=%s\r", pri, result.c_str());//SOS cancel message .
	m_GPSMessageTime.SetTime();  // reset the GPS timer since GPS will be sent out if this is a GPS based instrument.
	m_LastMessageTime.SetTime();  // reset when the last message to iNet was sent.

	return 0;
}

//------------------------------------------------------------------------------
ats::String LensPeer::GetTimestamp()
{

	char s[TIMESTAMP + 1];
	long msec;
	struct timeval tv;
	struct tm *tm;
	gettimeofday(&tv, 0);
	tm = localtime(&tv.tv_sec);
	if (tm != NULL)
	{
		strftime(s, TIMESTAMP, "%Y-%m-%dT%H:%M:%S", tm);
		msec = (tv.tv_usec/(long)1000);  //<ISCP-353>
		sprintf(s,"%s.%03ld-0000",s,msec);
	}


	return ats::String(s);
}

//------------------------------------------------------------------------------
void LensPeer::RequestVerbose(InstrumentStatus iStatus)
{
	const unsigned char request[] = {0x24, 0x24, 0x30, 0x05, 0x00, 0x00, 0x00, 0x05, 0x23, 0x23};
	char t[16];
	memcpy(t, request, sizeof request);
	iStatus.MAC().GetMac(t + 4);  // copy the three byte mac address
	t[10] = '\0';
	m_pLens->send_to_outgoing_mailbox(t, 10);
//	std::string str = t;
	ats_logf(ATSLOG_INFO, YELLOW_ON "%s,%d: Sending Request Verbose %s\r" RESET_COLOR, __FILE__, __LINE__, (ats::to_hex(t)).c_str());
}

//------------------------------------------------------------------------------
ats::String LensPeer::toHex(const Document& dom)
{
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	dom.Accept(writer);
	ats::String rs = buffer.GetString();
	boost::replace_all(rs, "\"$$", "");
	boost::replace_all(rs, "$$\"", "");
	ats_logf(ATSLOG_DEBUG, "%s",rs.c_str());
	return ats::to_hex(rs);
}
//------------------------------------------------------------------------------
void LensPeer::Log(const Document& dom)
{
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	dom.Accept(writer);
	ats::String rs = buffer.GetString();
	ats_logf(ATSLOG_DEBUG, "%s, %d: %s", __FILE__, __LINE__, rs.c_str());
}
//------------------------------------------------------------------------------
void LensPeer::Log(const Value& val)
{
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	val.Accept(writer);
	ats::String rs = buffer.GetString();
	ats_logf(ATSLOG_DEBUG, "%s, %d: %s", __FILE__, __LINE__, rs.c_str());
}

//------------------------------------------------------------------------------
int LensPeer::GetStateFlag()
{
	int flag = 0;

	if (m_InstrumentStatus.GetInstrumentState() == 0x09)
	{
		if(m_InstrumentStatus.GetAlarmDetail() == 0x01) // sensor alarm
			flag |= 0x01;
		if(m_InstrumentStatus.GetAlarmDetail() == 0x02) // panic
			flag |= 0x02;
		if(m_InstrumentStatus.GetAlarmDetail() == 0x08) // man down
			flag |= 0x08;
		if(m_InstrumentStatus.GetAlarmDetail() == 0x10) // pump fault
			flag |= 0x10;
	}
	////////////< ISCP-260>
	else if (m_InstrumentStatus.GetInstrumentState() == 0x01) 
	{
					for (short i = 0; i < m_InstrumentStatus.InstrumentNumberOfSensors(); i++)
					{
						if (m_InstrumentStatus.GetSensorStatus(i) == 0x05 || // Calibration Fault
							m_InstrumentStatus.GetSensorStatus(i) == 0x06 || // Zero Fault
							m_InstrumentStatus.GetSensorStatus(i) == 0x09 || // Bump Fault
							m_InstrumentStatus.GetSensorStatus(i) == 0x0D )  // Data Fail
							{
								flag |= 0x01; // Send it as sensor alarm state as well
								break;
							}
					}
	}
    //////////////	< /ISCP-260>	

	if(m_InstrumentStatus.GetExceptions() & 0x04) // low battery
	{
		flag |= 0x20;
	}

	if (m_InstrumentStatus.GetInstrumentState() == 0x1b)
		flag |= 0x04;
	if (m_InstrumentStatus.GetInstrumentState() == 0x1d)
		flag |= 0x80;
       
	return flag;
}


//------------------------------------------------------------------------------
// CheckLostStatus - if we transition between 'not lost' and 'lost' then
// and INet message needs to go out.
// return 1 if instrument is lost
int LensPeer::CheckLostStatus()
{
	if (m_ContactTime.DiffTime() > (g_pLensParms->InstrumentLostSeconds()))
	{
		m_bIsLost = true;
		m_ConnectionState = LOST;

		SendINetLostInstrument();  // Sends out the Lost Event Upload API message.

		//Lost instruments are no longer part of the network so drop the instrument from the peers list -  June 2019
		if (g_INetIPC.LeaveNetwork(m_MAC) != 0)
			ats_logf(ATSLOG_DEBUG, RED_ON "Lost Unit was not able to leave the network - MAC %s not found in list" RESET_COLOR, m_MAC.toHex().c_str());

		m_pLens->DecrementPeerCount();
		return 1;
	}
	return 0;
}
//------------------------------------------------------------------------------
int LensPeer::SendINetLostInstrument()
{
	if (!m_IDGeneral.IsInetEnabled() || (m_IDGeneral.GetInstDeviceType() == 0x15) )	 // only send if iNet is enabled in General or Frequent message
		return 0;
	ats::String result;  // hex data for adding to message
	Document dom;
	Document::AllocatorType &allocator = dom.GetAllocator();
	dom.SetObject();
	std::string url;
	ats_sprintf(&url, "%s/iNetAPI/v1/live/%s/lost", g_pLensParms->iNetURL().c_str(), g_pLensParms->SerialNum().c_str());
	dom.AddMember("url", rapidString(url), allocator);
	std::string mac = m_MAC.toHex();
	dom.AddMember("mac", rapidString(mac), allocator );

	// now build the data to be sent
	Value dataObj(kObjectType);
	dataObj.SetObject();

	ats::String ts = m_LastMessageTime.GetTimestamp();

	dataObj.AddMember("sn", rapidString(m_IDGeneral.GetSN()), allocator);
	dataObj.AddMember("lastContact", rapidString(ts), allocator);
	dom.AddMember("data", dataObj, allocator);
	result = toHex(dom);
	ats_logf(ATSLOG_INFO, CYAN_ON "%s,%d: msg inet_msg  \nusr_msg_data=%s\r" RESET_COLOR, __FILE__, __LINE__, result.c_str());
	send_redstone_ud_msg("message-assembler", 0, "msg inet_msg  msg_priority=9 usr_msg_data=%s\r", result.c_str());
	return 0;
}
