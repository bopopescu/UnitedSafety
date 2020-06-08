#include <iomanip>
#include <geoconst.h>
#include <colordef.h>
#include "ServerSocket.h"
#include "SocketException.h"

#include "atslogger.h"
#include "packetizer.h"
#include "db-monitor.h"
#include "INetConfig.h"

#include <INET_IPC.h>
#include "InetIridiumSender.h"
#include <IridiumUtil.h>

#include "SatData90_IDGateway.h"
#include "SatData91_IDInstrument.h"
#include "SatData92_CreateInstrument.h"
#include "SatData93_UpdateInstrument.h"
#include "SatData94_UploadError.h"
#include "SatData95_LostInstrument.h"
#include "SatData96_UpdateGateway.h"

extern INET_IPC g_INetIPC;
extern INetConfig *g_pConfig;
//298
int IridiumFails = 0;
int IridiumPass = 0;
static bool g_bSentInetNotification = false;

//-------------------------------------------------------------------------------------------------
InetIridiumSender::InetIridiumSender()
{
	m_testID = 1;
	db_monitor::ConfigDB db;
	m_IridiumIMEI = db.GetValue("Iridium", "IMEI");
	m_bPS19Test = db.GetBool("isc-lens", "PS19Test", false);
}


//-------------------------------------------------------------------------------------------------
// 298
bool InetIridiumSender::GetResponseIridium(IridiumUtil *pIridiumUtil)
{
	std::string receivedStr;
	bool retVal = false;
	retVal = pIridiumUtil->getResponse(receivedStr);
	if(retVal == true)
	{
		HandleIridiumResponse(receivedStr);
		//ats_logf(ATSLOG_DEBUG, "%s:%d - Hamza - Incoming MTMessage: %s", __FILE__, __LINE__, receivedStr.c_str());
	}
	return retVal;
}

//-------------------------------------------------------------------------------------------------
//298
void InetIridiumSender::HandleIridiumResponse(std::string receivedStr)
{
	char ReceivedCharArray[60];
	// convert string to char array
	receivedStr.copy(ReceivedCharArray,60,0);
	// extract from char array to int 
	// status_code 
	int status_code = hexcharToInt(ReceivedCharArray[27]) *16 + hexcharToInt(ReceivedCharArray[28]);
	int packetLength =  hexcharToInt(ReceivedCharArray[22]);
	// if status_code == 200 and 201
	//ats_logf(ATSLOG_DEBUG, "Response recieved: %s\r\n",&ReceivedCharArray[0]);
	if((ReceivedCharArray[0] == 'r') && (ReceivedCharArray[18] == '0'))
	{
		//ats_logf(ATSLOG_DEBUG, "%s:%d - Status_code = %d\r\n", __FILE__, __LINE__,status_code);
		if((status_code == 200) || (status_code == 201))
		{
			// update inet led 
			g_INetIPC.UsingIridium(true);
			IridiumFails =0;
			// extra work in case of peer mac received	
			// if peer mac is present i.e. length == 5
			if(packetLength == 5)
			{	
				unsigned char mac_r[3];
				mac_r[2] =   hexcharToInt(ReceivedCharArray[30]) *16 + hexcharToInt(ReceivedCharArray[31]);
				mac_r[1] =   hexcharToInt(ReceivedCharArray[33]) *16 + hexcharToInt(ReceivedCharArray[34]);
				mac_r[0] =   hexcharToInt(ReceivedCharArray[36]) *16 + hexcharToInt(ReceivedCharArray[37]);
				LensMAC macReceieved;
				macReceieved.MAC(mac_r);
				g_INetIPC.Add(macReceieved,"FFFFFFFF");	
				//ats_logf(ATSLOG_DEBUG, "mac id found: %s\r\n",&ReceivedCharArray[30]);
			}
		}
		else
		{
			// errase inet led
			g_INetIPC.UsingIridium(false);
			g_INetIPC.INetConnected(false);//ISCP-339
		}
		IridiumPass = 0; // reset count if we get any response from Iridium Translator Service.

	}
}
//-------------------------------------------------------------------------------------------------
//298
int InetIridiumSender::hexcharToInt(char hexChar)
{
	int retVal;
	std::stringstream ss;
	ss << hexChar;
	ss>> std::hex>>retVal;
	return retVal;
}


//-------------------------------------------------------------------------------------------------
void InetIridiumSender::SetData(ats::StringMap& sm)
{
	strcpy(m_DevName, "InetIridiumSender");
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
	m_MsgUserData = ats::String(ats::from_hex(sm.get("usr_msg_data").c_str()));
	m_esn = sm.get("mobile_id");
	m_esn_type = sm.get_int("mobile_id_type");
	m_cell_imei = sm.get("cell_imei");

	init_ServerData(&m_sd, 1);
	m_sd.m_port = 38050;
	m_sd.m_cs = processUdpData;
	start_udp_server(&m_sd)	;
}


//-------------------------------------------------------------------------------------------------
// Set up the gateway via Iridium  (this has been reduced to a single call.  Leave the start function
// to parallel the CellSender call structure
//


//-------------------------------------------------------------------------------------------------
bool InetIridiumSender::SendiNetCreateGateway(IridiumUtil *pIridiumUtil)
{
	ATS_TRACE;

	SatData90_IDGateway sd_msg;
	NMEA_Client nmea;
	NMEA_DATA GPSdata = nmea.GetData();

	LensMAC mac = g_INetIPC.GetLensRegisters().GetMAC();
	sd_msg.SetData(GPSdata, mac, g_pConfig->SerialNum(), g_pConfig->SiteName(), g_pConfig->INetIndex(), g_INetIPC.RunState());
	char sdbBuf[128];
	int len;
	len = sd_msg.Encode(sdbBuf);
	return SendViaIridium(sdbBuf, len, pIridiumUtil);
}


//-------------------------------------------------------------------------------------------------
bool InetIridiumSender::SendiNetUpdateGateway(IridiumUtil *pIridiumUtil)
{
	ATS_TRACE;

	SatData96_UpdateGateway sd_msg;
	NMEA_Client nmea;
	NMEA_DATA GPSdata = nmea.GetData();

	LensMAC mac = g_INetIPC.GetLensRegisters().GetMAC();
	sd_msg.SetData(GPSdata, mac, g_INetIPC.RunState());
	char sdbBuf[128];
	int len;
	len = sd_msg.Encode(sdbBuf);
	return SendViaIridium(sdbBuf, len, pIridiumUtil);
}


//-------------------------------------------------------------------------------------------------
bool InetIridiumSender::sendSingleMessage( message_info* p_mi, IridiumUtil *pIridiumUtil)
{
	bool retVal = false;
	ATS_TRACE;
	ats::String user_msg = ats::String(ats::from_hex(p_mi->sm.get("usr_msg_data").c_str()));
	const int code = p_mi->sm.get_int("event_type");
	char mybuf[2048];
	char iridiumBuf[256];
	strncpy(mybuf, user_msg.c_str(), user_msg.size());	
	mybuf[user_msg.size()] = '\0';

	if(code == TRAK_INET_MSG && user_msg.length())
	{
		g_INetIPC.RunState(NORMAL);
		ats_logf(ATSLOG_INFO, "%s,%d: sending message ##%s##", __FUNCTION__, __LINE__, user_msg.c_str());
		rapidjson::Document doc;

		if (doc.Parse<0>(mybuf).HasParseError())
		{
			ats_logf(ATSLOG_INFO, "%s,%d: Error message JSON has errors!", __FILE__, __LINE__);
			return true;  // discard message if it can't be parsed
		}
		
		std::string url = doc["url"].GetString();
		LensMAC mac;
		mac.SetMACHex(doc["mac"].GetString());
		std::string strData; 
		NMEA_DATA nmea;
		int len;
		
		ats::StringMap sm =  p_mi->sm;
		/*
		nmea.SetTimeFromDateTimeString(sm.get("event_time").c_str());
		nmea.ddLat = sm.get_double("latitude");
		nmea.ddLon = sm.get_double("longitude");
		nmea.H = sm.get_double("altitude");
		nmea.ddCOG = sm.get_double("heading");
		nmea.sog = (sm.get_double("speed")/ MS_TO_KPH) * MS_TO_KNOTS ;
		nmea.hdop = sm.get_double("hdop");
		nmea.num_svs = sm.get_double("satellites");
		nmea.gps_quality = sm.get_double("fix_status");
		nmea.valid = true;
		*/
		if (doc["data"].IsObject() )
		{
			rapidjson::StringBuffer sb;
			rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
			doc["data"].Accept(writer);
			strData = sb.GetString();
		}
		bool bEncoded = false;

		if (std::string::npos != url.find("createinst") ) // this requires sending 2 messages
		{
			if (std::find(m_IridiumIdentifiedInstruments.begin(), m_IridiumIdentifiedInstruments.end(), mac) == m_IridiumIdentifiedInstruments.end())
			{
				ats_logf(ATSLOG_INFO, "%s,%d: data for CREATEINST: url: " GREEN_ON " %s " RESET_COLOR "\ndata: " BLUE_ON " %s " RESET_COLOR, __FILE__, __LINE__, url.c_str(), strData.c_str());
				SatData91_IDInstrument sd91;
				sd91.SetData(mac, nmea, strData);
				len = sd91.Encode(iridiumBuf);
				retVal = SendViaIridium(iridiumBuf, len, pIridiumUtil);			

				m_IridiumIdentifiedInstruments.push_back(mac);

				// Delay of 1 sec between sd91 and sd92
				sleep(1);
			}	
			//if(true == retVal)
			{
			iridiumBuf[0] = '\0';
			SatData92_CreateInstrument sd92;
			sd92.SetData(mac, nmea, strData);
			len = sd92.Encode(iridiumBuf);
			bEncoded = true;
			}
		}
		else if (std::string::npos != url.find("updateinst"))
		{
			ats_logf(ATSLOG_INFO, "%s,%d: data for UPDATEINST: url: " GREEN_ON " %s " RESET_COLOR "\ndata: " BLUE_ON " %s " RESET_COLOR, __FILE__, __LINE__, url.c_str(), strData.c_str());
			SatData93_UpdateInstrument sd93;
			sd93.SetData(mac, nmea, strData);
			len = sd93.Encode(iridiumBuf);
			bEncoded = true;
		}
		else if (std::string::npos != url.find("lost"))
		{
			ats_logf(ATSLOG_INFO, "%s,%d: data for LOST: url: " GREEN_ON " %s " RESET_COLOR "\ndata: " BLUE_ON " %s " RESET_COLOR, __FILE__, __LINE__, url.c_str(), strData.c_str());
			ats::String user_msg = ats::String(p_mi->sm.get("usr_msg_data").c_str());
			SatData95_LostInstrument sd95;
			sd95.SetData(mac, nmea);
			len = sd95.Encode(iridiumBuf);
			bEncoded = true;
		}

		if (bEncoded)
		{
			ats_logf(ATSLOG_INFO, "%s,%d: Data encoded - SendViaIridium", __FILE__, __LINE__);
			retVal = SendViaIridium(iridiumBuf, len, pIridiumUtil);
			return retVal;
		}
	}
	if(code == TRAK_INET_ERROR && user_msg.length())
	{
		g_INetIPC.RunState(NORMAL);
		int errVal = atoi (user_msg.erase(user_msg.find_first_of(",")).c_str());
		ats_logf(ATSLOG_INFO, "%s,%d: sending message ##%d##", __FUNCTION__, __LINE__, errVal);
		SatData94_UploadError sd;
		LensMAC mac = g_INetIPC.GetLensRegisters().GetMAC();

		NMEA_DATA nmea;
		int len;
		
		ats::StringMap sm =  p_mi->sm;
		nmea.SetTimeFromDateTimeString(sm.get("event_time").c_str());
		nmea.ddLat = sm.get_double("latitude");
		nmea.ddLon = sm.get_double("longitude");
		nmea.H = sm.get_double("altitude");
		nmea.ddCOG = sm.get_double("heading");
		nmea.sog = (sm.get_double("speed")/ MS_TO_KPH) * MS_TO_KNOTS ;
		nmea.hdop = sm.get_double("hdop");
		nmea.num_svs = sm.get_double("satellites");
		nmea.gps_quality = sm.get_double("fix_status");
		nmea.valid = true;
		
		sd.SetData(mac, nmea, errVal);
		if ((len = sd.Encode(iridiumBuf)) > 0)
		{
			retVal = SendViaIridium(iridiumBuf, len, pIridiumUtil);
			return retVal;
		}
	}
	return false;
}



//-------------------------------------------------------------------------------------------------
ssize_t InetIridiumSender::sendUdpData(const char *p_data, uint p_data_length)
{
	char msgBuf[256];
	msgBuf[0] =  0x02;
	short v = (short)p_data_length;
	memcpy(&msgBuf[1], &v, 2);
	memcpy(&msgBuf[3], p_data, p_data_length);

	// now add the SBD payload
	// open socket to server
  try
  {
		Socket client ;    // Create the socket
		client.create();
		if (client.connect("67.212.66.103", 38050))
		{
			ats_logf(ATSLOG_ERROR, "Encoded length %d  (%s)", p_data_length, ats::to_hex(p_data).c_str() );
			client.send(msgBuf, 3+p_data_length);	// send data
		}
		else
			printf("Failed to connect to port.\n");
	}
  catch ( SocketException& e )
	{
		ats_logf(ATSLOG_ERROR,  "%s:Exception was caught: %s  errno=%d", __FUNCTION__, e.description().c_str(), errno);
	}
	return true;
}

//-------------------------------------------------------------------------------------------------
void* InetIridiumSender::processUdpData(void* p)
{
	p = p;
	//struct ServerData& sd = *((struct ServerData*) p);
	for(;;)
	{
		sleep(10);
	}
	return NULL;
}


//-------------------------------------------------------------------------------------------------
// send the data using the Iridium.  Adds a checksum then sends the message
// 
bool InetIridiumSender::SendViaIridium(char * iridiumBuf, int len, IridiumUtil *pIridiumUtil)
{

	g_INetIPC.SendingToINet(true);

	if (m_bPS19Test)
	{
		SendTestPacket(iridiumBuf, len);
		g_INetIPC.SendingToINet(false);
		return true;  // here for testing messages
	}
	// this is the actual iridium send.

	ats_logf(ATSLOG_DEBUG, "%s,%d: Copying iridiumBuf of length %d" ,__FILE__,__LINE__, len);
	std::vector<char> vData;
	vData.insert(vData.end(), iridiumBuf, iridiumBuf+len);
	pIridiumUtil->ComputeCheckSum(vData);

	bool ret = pIridiumUtil->sendMessageWDataLimit(vData);
	// 298
	if(ret == 0)
	{		
		if(IridiumFails++ > 2) //ISCP-339
		{
			// errase inet led
			g_INetIPC.UsingIridium(false);
			g_INetIPC.INetConnected(false);//ISCP-339
			if (!g_bSentInetNotification) //<ISCP-164>
			{
				ats_logf(ATSLOG_DEBUG, "Iridium lost communication \r");
				g_bSentInetNotification = true;
				//.AFS_Timer t;
				//.t.SetTime();
				//.std::string user_data = "996," + t.GetTimestampWithOS() + ", Iridium Commuication Error";
				//.user_data = ats::to_hex(user_data);

				//.send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
			}
		}

	}
	else
	{
		g_bSentInetNotification = false; // reset flag to allow sending the same error message again
		if(IridiumPass++ > 5) // If Iridium Translator Service is not reponding //ISCP-339
		{
			// errase inet led
			g_INetIPC.UsingIridium(false);
			

		}
		g_INetIPC.INetConnected(false);//ISCP-339, This will remove CELL icon from peer device as soon Satellite communication is established between TGX and iNet
	}
	g_INetIPC.SendingToINet(false);
	return ret;
}

//-------------------------------------------------------------------------------------------------
// Send a test packet directly to the Aware Iridium Server.
void InetIridiumSender::SendTestPacket(char * sdbBuf, int len)
{
	ATS_TRACE;
	// create a header and wrap the data
	char msgBuf[256];
	msgBuf[0] =  0x01;
	msgBuf[3] =  0x01;
	short v = 28;
	msgBuf[4] =  v / 256;	msgBuf[5] =  v % 256;
	m_testID += 1;
	memcpy(&msgBuf[6], &m_testID, 4);

	memcpy(&msgBuf[10], m_IridiumIMEI.c_str(), 15);
	msgBuf[25] = 0;
	v = (short)m_testID;
	memcpy(&msgBuf[26], &v, 2);
	memcpy(&msgBuf[28], &v, 2);

	time_t curTime;
	time(&curTime);
	long t = (long)curTime;
	memcpy(&msgBuf[30], &t, 4);
	// now add the SBD payload
	msgBuf[34] = 0x02;
	msgBuf[35] =  len / 256;	msgBuf[36] =  len % 256;
	memcpy(&msgBuf[37], sdbBuf, len);

	// now reset the overall length
	v = 34 + len;
	msgBuf[1] =  v / 256;	msgBuf[2] =  v % 256;

	// open socket to server
  try
  {
		Socket client ;    // Create the socket
		client.create();

		if (client.connect("67.212.66.103", 38050))
		{
			ats_logf(ATSLOG_ERROR, "Encoded length %d  (%s)", len+37, ats::to_hex(msgBuf, 37 + len).c_str() );
			client.send(msgBuf, 37 + len);	// send data
		}
		else
			printf("Failed to connect to port.\n");
	}
  catch ( SocketException& e )
	{
		ats_logf(ATSLOG_ERROR,  "%s:Exception was caught: %s  errno=%d", __FUNCTION__, e.description().c_str(), errno);
	}	
	ATS_TRACE;
}

void InetIridiumSender::DumpIPC() // just dump the IPC instrument data to log file.
{
	INET_INSTRUMENT_DATA * pIID;
	for (short i = 0; i < 24; i++)
	{
		pIID = g_INetIPC.InstrumentData(i);
		if (pIID)
		{
			ats_logf(ATSLOG_INFO,  "%s:  %d - %s, %s, %s, %s, %s", __FUNCTION__, i, pIID->m_MAC.toHex().c_str(), 
			((pIID->m_bIsAssigned) ? "Assigned" :"Unassigned"),
			((pIID->m_bNew) ? "new unit" :"registered"),
			((pIID->m_bIsLeaving) ? "Leaving" :"normal"),
			((pIID->m_bSentIdentify) ? "Identify Sent" :"Identify NOT Sent"));
		}
		else
		{
			ats_logf(ATSLOG_INFO,  "%s:  %d not a valid instrument", __FUNCTION__, i);			
		}
	}
}
