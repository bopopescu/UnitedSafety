#include <signal.h>
#include <signal.h>
#include <signal.h>
#include <unistd.h>
#include <INET_IPC.h>
#include "colordef.h"
#include "lens.h"
#include "MyData.h"
#include "INetConfig.h"
#include "InstrumentStatus.h"

extern INetConfig *g_pLensParms;  // read the db-config parameters
extern ATSLogger g_log;
extern INET_IPC g_INetIPC;  // common data.
NMEA_Client g_nmea_client;

//-----------------------------------------------------------------------
MyData::MyData(Lens *pLens)
{
	m_pLens = pLens;
	m_ventisdata_message = new ClientData();
	m_mutex = new pthread_mutex_t;
	pthread_mutex_init(m_mutex, 0);

	ats_logf(ATSLOG_INFO, "Starting threads\n");
	m_urgent_fd = open("/dev/lens-urgent", O_RDONLY);
	pthread_create(&m_INetMonitor_thread, 0, MyData::INetMonitor_thread, this);
	pthread_create(&m_DecodeMessages_thread, 0, MyData::DecodeMessages_thread, this);
	pthread_create(&m_LostPeer_thread, 0, MyData::LostPeer_thread, this);
	pthread_create(&m_fetchmessage_thread, 0, MyData::fetchmessage_thread, this);
	pthread_create(&m_urgentInterrupt_thread, 0, MyData::urgentInterrupt_thread, this);
	pthread_create(&m_Monitor_thread, 0, MyData::Monitor_thread, this);
}
//-----------------------------------------------------------------------
void MyData::messageParserAndSend(const char *data, int length, MyData *md)
{
	char *p = (char *)data;
	std::list<std::vector<char> > sl;
	vector<int> temp;
	for(int i = 0; i < length - 1; ++i)
	{
		if (p[i] == 0x24 && p[i+1] == 0x24)
		{
			temp.push_back(i);
		}
	}
	if (temp.size() == 0)
	{
		ats_logf(ATSLOG_ERROR, RED_ON "Message is corrupt!\n" RESET_COLOR);
		return;
	}
	std::vector<int>::const_iterator it = temp.begin();
	for(; it != temp.end() - 1; ++it)
	{
		int index = *it;
		int nindex = *(it + 1);
		if (nindex < index) continue;
		std::vector<char> v;
		for(int i = index; i < nindex; ++i)
		{
			v.push_back(*(p + i));
		}
		if (v.size() < 2) continue;
		if (v[v.size() - 1] == 0x23 && v[v.size() - 2] == 0x23)
		{
			sl.push_back(v);
		}
	}
	// for the last marked index;
	int lastindex = *(temp.end() - 1);
	for(int i = lastindex ; i < length - 1 ; ++i)
	{
		if (p[i] == 0x23 && p[i + 1] == 0x23)
		{
			std::vector<char> v;
			for(int j = lastindex ; j <= i + 1; ++j)
			{
				v.push_back(*(p+j));
			}
			sl.push_back(v);
		}
	}
	{
		std::list<vector<char> >::const_iterator it = sl.begin();
		for(; it != sl.end(); ++it)
		{
#if 0
			// test result
			vector<char>::const_iterator itt = (*it).begin();
			for(; itt != (*it).end(); ++itt)
			{
				printf("%02x ", (*itt));
			}
			printf("\n");
#endif

			const ats::String str((*it).begin(), (*it).end());
			messageFrame m(0, str, ats::String());
			md->post_message(md->get_ventisdata_key(), m);
		}
	}
}

//-----------------------------------------------------------------------
void *MyData::urgentInterrupt_thread(void* p)
{
	MyData& md = *((MyData*)(p));

	md.WaitForLensInit(md);

	ats_logf(ATSLOG_DEBUG, "%s,%d: Begin urgent Interrupt thread", __FILE__, __LINE__);

	for(;;)
	{
		ats::String des;
		int len;
		char buf[32];
		memset(buf, 0, 32);
		len = read(md.m_urgent_fd, buf, 32);  //won't work without this read.

		if (len && buf[0] == 'i')
		{
			// TODO: need handle the interrupt
			ats_logf(ATSLOG_DEBUG, RED_ON "%s, %d: Detected Interrupt" RESET_COLOR, __FILE__, __LINE__);
		}
	}
	return 0;
}
//-----------------------------------------------------------------------
void *MyData::Monitor_thread(void* p)
{
	MyData& md = *((MyData*)(p));
	ATSLogger local_log;
	local_log.set_level(5);
	local_log.open_testdata("LensMonitorThread");
	ats_logf(&local_log, GREEN_ON "================ ISC Lens Monitor Thread started ================" RESET_COLOR);

	md.WaitForLensInit(md);
	ats_logf(&local_log, GREEN_ON "LENS has initialized." RESET_COLOR);
	
	std::string lensNetName, lensNetName1;
	int dbNetName, dbNetName1;
	md.m_pLens->GetLensRegisters().NetworkID(lensNetName);
	{
		db_monitor::ConfigDB db;
		dbNetName = db.GetInt("isc-lens", "NetworkName", 1);
	}

	ats_logf(&local_log, "base values:  DB-config NetworkName = %d\n              LENS NetworkName %s", dbNetName, lensNetName.c_str());
	int count = 0;
	for(;;)
	{
		count++;
		sleep(1);
		if (count % 60 == 0)  // once a minute
		{
			db_monitor::ConfigDB db;
			dbNetName1 = db.GetInt("isc-lens", "NetworkName", 1);
			if (dbNetName != dbNetName1)
			{
				ats_logf(&local_log, RED_ON "ERROR! db-config value has changed! Was %d  Now %d" RESET_COLOR, dbNetName, dbNetName1);
				dbNetName = dbNetName1;
			}
		}
		if (count %5 == 0)
		{
			md.m_pLens->GetLensRegisters().NetworkID(lensNetName1);

			if (lensNetName != lensNetName1)
			{
				ats_logf(&local_log, RED_ON "ERROR! LENS register value has changed! Was %s  Now %s" RESET_COLOR, lensNetName.c_str(), lensNetName1.c_str());
				lensNetName = lensNetName1;
			}			
		}

		// test for switch to Iridium.  We need to resend the instruments otherwise they show as lost.
		if (g_INetIPC.SwitchedToIridium())
		{
			//;md.ResendCreateInstrument(md); //test adnan 
			g_INetIPC.SwitchedToIridium(false);
		}
	}
	return 0;
}
//-----------------------------------------------------------------------
// fetchmessage_thread - pulls bytes in from the SPI stream and stores
// them for retrieval by the incomingmanage_thread.
//
void *MyData::fetchmessage_thread(void* p)
{
	MyData& md = *((MyData*)(p));
	
	md.WaitForLensInit(md);

	ats_logf(ATSLOG_DEBUG, "Beginning fetch_message thread");
	char b[max_outgoing_buffer_size];
	int msgSize;
	int icount = 0;
	
	for(;;)
	{
		memset(b, 0, max_outgoing_buffer_size);

		if (md.m_pLens->read_from_incoming_mailbox((char *)b, msgSize) && msgSize < 1000)  // if the message is over 1000 we are probably doing the loop thing so s
		{
			md.messageParserAndSend(b, msgSize, &md);
			icount = 0;
		}
		else
		{
			usleep(500	* 1000);
			
			if (++icount %200 == 0)
				md.WakeupRadio(); // just in case it has gone to sleep
				
			if (icount%4  == 0) // every 2 seconds get network status
			md.m_pLens->GetRadioNetworkStatus();
		}
	}
	return 0;
}
//-----------------------------------------------------------------------------------
//  iNetMonitor_thread - monitors the inet status and changes the registers
//  if it changes.  Also updates the InstrumentStatusandData memory and monitors
//	it for changes.
//
void *MyData::INetMonitor_thread(void* p)
{
	MyData& md = *((MyData*)(p));
	md.WaitForLensInit(md);

	bool bLastINetConnectionStatus = (g_INetIPC.INetConnected() || g_INetIPC.UsingIridium());

	md.m_pLens->SetINetConnectionFlag(bLastINetConnectionStatus?1:0);

	for(;;)
	{
		// monitors the connection status.
		if ((g_INetIPC.INetConnected() || g_INetIPC.UsingIridium()) !=  bLastINetConnectionStatus)
		{
			bLastINetConnectionStatus = (g_INetIPC.INetConnected() || g_INetIPC.UsingIridium());
			md.m_pLens->SetINetConnectionFlag(bLastINetConnectionStatus?1:0);
			//g_INetIPC.LensConnectionStatus(bLastINetConnectionStatus?true:false);// set the LED to yellow until paired to a unit //ISCP-343

	 		ats_logf(ATSLOG_INFO,  YELLOW_ON "iNet connection status is now '%s'" RESET_COLOR, bLastINetConnectionStatus ? "connected" : "disconnected");
		}

		if (g_INetIPC.NewRegisters())
		{
			g_INetIPC.NewRegisters(false);  // we received the Update Settings API from iNet
			md.m_pLens->WriteRegisters();  // update the lens memory with the new settings.
		}

		md.UpdateInstrumentStatus(md); // update the Instrument status memory

		usleep(1500000);
	}

	return 0;
}

//-----------------------------------------------------------------------------------
void MyData::UpdateInstrumentStatus(MyData &md)
{
	static unsigned char seq = 0;
	NMEA_Client nmea_client;

	unsigned char len = 0;
	seq++;
	char buf[32];
	buf[0] = 0x01;
	buf[1] = len;
	buf[2] = seq;
	buf[3] = (g_INetIPC.RunState() == SHUTDOWN) ? 0x1B : 0x01;
	buf[4] = 1<<4; // indicates a gateway device.
	buf[5] = 0;
	buf[6] = 0;  //ALWAYS 0
	buf[7] = 99;
	buf[8] = 0;  // 0 INDICATES BATTERY (We don't know what our power supply is)
// 298 //241
	buf[9] = g_INetIPC.INetConnected() ? 0x03 : (g_INetIPC.UsingIridium() ? 0x04 : 0x00);  // cellular  or iridium connected.
	buf[10] = 0x42;  //TODO: set to actual signal strength as a %
	buf[11] = g_nmea_client.NumSVs();
	buf[12] = 0;
	short numNew = 0;
	LensMAC mac;
	short i;

	char macbuf[8];

	for (i = 0; i < g_INetIPC.MaxPeers(); i++)
	{
		if (g_INetIPC.IsNew(i))
		{
			g_INetIPC.GetMAC(i, mac);
			std::map<std::string, LensPeer *>::iterator it;
			it = md.m_Peers.find(mac.toHex());

			if (it != md.m_Peers.end())  // if it is not in m_Peers it is because it has disconnected.  Don't try and reconnect.
			{
				LensPeer *pLP =  it->second;
				if (pLP->GetInstrumentStatus().IsInetConnected()) // we have already alerted the unit so we don't need to do it
				{
					g_INetIPC.ClearNew(mac);
					break;
				}
				else
				{
					numNew++;
					buf[12] = numNew;
					ats_logf(ATSLOG_INFO,  YELLOW_ON "New connection at idx %d  MAC %s" RESET_COLOR, i, mac.toHex().c_str() );
					int idx = 13 + 4 * (numNew - 1);
					memcpy((void *)&buf[idx], (void *)mac.GetMac(macbuf), 3);
					buf[16 + 4 *(numNew -1)] = 1;  // subscription status
				
					if (numNew == 4)
						break;
				}
			}
		}
	}

	buf[5] = 7 + numNew*4;
	unsigned char cs= 0;
	len = 13+(4 * numNew);  // base message is 13 plus one for checksum!
	buf[1] = len;
	for (i = 1; i < len; i++)
	{
		cs += buf[i];
	}
	buf[len] = cs;
	md.m_pLens->WriteStatusData(buf, len + 1);
	if (len != 0x0d)
	{
		md.LogHexData("Writing Status:", buf, len + 1, GREEN_ON);  //output to log file 
	}		
}


//---------------------------------------------------------------------------------------------------
// DecodeMessages_thread
//  waits for incoming messages then process them based on message type.
//	Incoming messages are queued up from messageParserAndSend
//	TODO: make each message type decode into its own function in the Lens class.
//	TODO: add the remaining undecoded messages.
//
void *MyData::DecodeMessages_thread(void* p)
{
	MyData& md = *((MyData*)(p));
	ats_logf(ATSLOG_INFO, "%s,%d: DecodeMessages thread started...", __FILE__, __LINE__);

	md.m_msg_manager.add_client(ats::toStr(md.get_ventisdata_key()), new message_queue());
	md.m_msg_manager.start_client(ats::toStr(md.get_ventisdata_key()));

	messageFrame m1;
	
	for(;;)
	{
		if (!md.get_message(md.get_ventisdata_key(), m1))
		{
			usleep(10000);
			continue;
		}
	
		const ats::String& s1 = m1.msg1;
		const char *p = s1.c_str();
		int size = s1.size();

		//md.LogHexData("Incoming message", p, size, BCYAN_ON);  //output to log file 

		// check if buffer has extra corrupt data attached
		if (size >= 6)
		{
			for(int i = 2; i < size - 3; i++)
			{
				if (p[i] == 0x23 && p[i+1] == 0x23)
				{
					ats_logf(ATSLOG_INFO, "Resetting size from %d to %d\n", size, size-i);
					size = size - i;
					break;
				}
			}
		}

		// need to check for a clean message frame 
		if (p[0] != 0x24 || p[1] != 0x24 || p[size - 1] != 0x23 || p[size - 2] != 0x23)
		{
			ats_logf(ATSLOG_ERROR, "%s, %d: No Framing character found", __FILE__, __LINE__);
			continue;
		}

		switch ((int)p[2])
		{
			case (InstrumentStatusData): //  message 0x00 - page 20 - 2.2.2.2 Instrument Status and Data (response to TruLink).
			{
				md.LogHexData("Decoding: 0x00 InstrumentStatusData: ", s1.c_str(), s1.size(), MAGENTA_ON);  //output to log file 
				md.DecodeInstrumentStatusData(p, s1.size(), s1);
				break;
			}

			case (IdentifyRequest):  	  // message 0x05 - page 24 - 2.2.2.3 - Identify Request
			{
				md.LogHexData("Decoding: 0x05 IdentifyRequest: ", s1.c_str(), s1.size(), MAGENTA_ON);  //output to log file 
				md.DecodeIdentifyRequest(p);
				break;
			}

			case (IdentifyGeneral):	    // message 0xA2 - page 27 - 2.2.2.6 - Identify General Information
			{
				md.LogHexData("Decoding: 0xA2 IdentifyGeneral: ", s1.c_str(), s1.size(), MAGENTA_ON);  //output to log file 
				md.DecodeIdentifyGeneral(p, s1.size());
				break;
			}
			case (IdentifySensorConfig):	//message 0xA3 - page 29 -2.2.2.7 - Identify SensorConfiguration
			{
				md.LogHexData("Decoding: 0xA3 IdentifySensorConfig: ", s1.c_str(), s1.size(), MAGENTA_ON);  //output to log file 
				md.DecodeIdentifySensorConfig(p, s1.size());
				break;
			}
			case (IdentifyFrequent):	//message 0xA4 - page 39 -2.2.2.8 - Identify SensorConfiguration
			{
				md.LogHexData("Decoding: 0xA4 IdentifyFrequent: ", s1.c_str(), s1.size(), MAGENTA_ON);  //output to log file 
				ats_logf(ATSLOG_ERROR, MAGENTA_ON "Decoding: 0xA4 IdentifyFrequent" RESET_COLOR);
				md.DecodeIdentifyFrequent(p, s1.size());
				break;
			}
			case (NetworkError): // message 0x77 - Network Error.
			{
				md.LogHexData("Decoding: 0x77 NetworkError: ", s1.c_str(), s1.size(), MAGENTA_ON);  //output to log file 
				md.DecodeNetworkError(p, s1.size());
				break;
			}
			case (PeerGPSData): // message 0x84 - Peer GPS Data
			{
				md.LogHexData("Decoding: 0x84 PeerGPSData: ", s1.c_str(), s1.size(), MAGENTA_ON);  //output to log file 
				md.DecodePeerGPSData((const char *)p, s1.size());
				break;
			}
			default:
				break;
		}
	}
}
//-----------------------------------------------------------------------------------
//  LostPeer_thread - monitors the attached peers for loss of communications.
//
//
void *MyData::LostPeer_thread(void* p)
{
	MyData& md = *((MyData*)(p));
	md.WaitForLensInit(md);

	std::map<std::string, LensPeer *>::iterator it;
	while (1)
	{
		for ( it = md.m_Peers.begin(); it != md.m_Peers.end(); it++)
		{
			LensPeer *pLP =  it->second;
			if (pLP->CheckLostStatus() == 1)  // lost if 1 is returned
			{
				LensMAC mac = pLP->GetMAC();
				char buf[32];
				delete(it->second);
				md.m_Peers.erase(mac.GetMac(buf));
				ats_logf(ATSLOG_INFO, CYAN_ON "%s has been lost. Deleting from peers" RESET_COLOR, buf);
			}
		}
		sleep(15);
	}
}
//-----------------------------------------------------------------------------------
void MyData::post_message(const ClientData* p_client, const messageFrame& p_msg)
{
	string key = ats::toStr(p_client);
	m_msg_manager.post_msg(p_msg, key);
}

//-----------------------------------------------------------------------------------
bool MyData::get_message(const ClientData* p_client, messageFrame& p_msg)
{
	return m_msg_manager.wait_msg(ats::toStr(p_client), p_msg);
}

//-----------------------------------------------------------------------------------
void MyData::WakeupRadio()
{
	if (m_urgent_fd)
	{
		while (m_pLens->GetRadioNetworkStatus() == 0x04)
		{
			ATS_TRACE;
			m_pLens->Wakeup(m_urgent_fd);
		}
	}
}

//-----------------------------------------------------------------------------------
void MyData::resetRadio()
{
	if (m_urgent_fd)
	{
		const int ret = ioctl(m_urgent_fd, ISC_RESETRADIO);
		if (ret)
		{
			ats_logf(ATSLOG_ERROR, "%s, %d: Fail to call ioctl ISC_RESETRADIO", __FILE__, __LINE__);
		}
	}
}

//-----------------------------------------------------------------------------------
void MyData::disconnect()
{
	if (!IsLensRadioInitialized())
	{
		ats_logf(ATSLOG_ERROR, "init flag not 0xa5, hardware issue occur");
		return;
	}

	WakeupRadio();
	ats_logf(ATSLOG_DEBUG, "Disconnecting LENS radio!");
	m_pLens->NetworkDisconnect();
}
//----------------------------------------------------------------------------------------------
// sets up the static area and sends out the connect_network signal
//
void MyData::LensSetup()
{
	int retries = 0;

	while (!IsLensRadioInitialized())
	{
		retries++;
		if (retries == 10)
			ats_logf(ATSLOG_ERROR, "init flag not 0xa5, hardware issue has occurred!");

		sleep(1);
	}

	g_INetIPC.LensConnectionStatus(true);  // notifying LEDs that the lens is talking to us.

	WakeupRadio();  // won't return until radio status is not 0x04

	if (g_pLensParms->EnableTestMode()) // ISCP-37 Enable test Mode and ISC-27 2.1.13.9 PRIN-FRS_INS-70 Command radio to test mode
	{
		m_pLens->SetTestMode(0);
		while (1)
			sleep(10);
	}
	while(m_pLens->SetNetworkConfiguration())	// wont return until radio status is 01 or 02 returns -1 if radio is asleep
		WakeupRadio();    // won't return until radio status is not 0x04

	while (m_pLens->set_sm_reg() != 0)	// wont return until radio status is 01 or 02. returns -1 if status is 4(sleep)
		WakeupRadio();    // won't return until radio status is not 0x04
	
	m_pLens->WriteIdentifyGeneral();
	WakeupRadio();  // check that it isn't asleep
	m_pLens->set_identify_sensor_configuration();
	WakeupRadio();  // check that it isn't asleep
	m_pLens->SetNetworkEncryption();
	m_pLens->NetworkConnect();
	sleep(1);
}

//-----------------------------------------------------------------------------------
ats::String MyData::trim(const ats::String& str)
{
	ats::String::size_type pos = str.find_first_not_of(' ');
	if (pos == ats::String::npos)
	{
		return str;
	}
	ats::String::size_type pos2 = str.find_last_not_of(' ');
	if (pos2 != ats::String::npos)
	{
		return str.substr(pos, pos2 - pos + 1);
	}
	return str.substr(pos);
}

//-----------------------------------------------------------------------------------
// Hard code only for Cohen project demo. our Controlcenter only accept all digital numbers as device ID.
ats::String MyData::getSN(const ats::String& str)
{
	if(str == "16022VZ-001")
	{
		return "35734545258355";
	}
	else if(str == "15072EK-003")
	{
		return "75980060883620";
	}
	return "";
}


//-------------------------------------------------------------------------------------------------------------------------------
// An incoming identify request specifies which message should be returned
// page 24 - 2.2.2.3 Identify Request
void MyData::DecodeIdentifyRequest(const char * p)
{
	
	if (int(*(p+3)) != 5)
	{
		ats_logf(ATSLOG_ERROR, "DecodeIdentifyRequest: Incoming message size (%d) is wrong (should be 5)", int(*(p+3)) );
		return;
	}

	char identify_general[] = {0x24, 0x24, 0x12, 0x04, 0x00, 0x00, 0x00, 0x23, 0x23};
	char t[16];
	t[9] = '\0';

	int requestedMsg = (int)(*(p+4));
	bool bSent = false;

	if (requestedMsg & 0x01)
	{
		memcpy(t, identify_general, sizeof (identify_general) );
		memcpy(t + 4, p + 5, 3);
		m_pLens->send_to_outgoing_mailbox(t, 9);
		bSent = true;
		LogHexData("DecodeIdentifyRequest: Requesting general", t, 9, YELLOW_ON);  //output to log file 
	}
	if (requestedMsg & 0x02) // Identify Sensor Configuration
	{
//		t[2] = 0x13;
//		m_pLens->send_to_outgoing_mailbox(t, 9);
//		bSent = true;
//		LogHexData("DecodeIdentifyRequest: Requesting sensor", t, 9, YELLOW_ON);  //output to log file 
	}
	
	if (requestedMsg & 0x04) // Identify Frequent
	{
		memcpy(t, identify_general, sizeof (identify_general) );
		memcpy(t + 4, p + 5, 3);
		t[2] = 0x14;
		m_pLens->send_to_outgoing_mailbox(t, 9);
		bSent = true;
		LogHexData("DecodeIdentifyRequest Requesting frequent", t, 9, YELLOW_ON);  //output to log file 
	}
	if (!bSent  && 	!(requestedMsg & 0x02)) // message 0x02 gets requested but we don't respond - no need to log an error.
	{
		ats_logf(ATSLOG_ERROR, "DecodeIdentifyRequest: Unknown message requested (%d)", requestedMsg);
	}
}


//-------------------------------------------------------------------------------------------------------------------------------
// An incoming identify request specifies which message should be returned
// page 27 - 2.2.2.6 Identify General Information
void MyData::DecodeIdentifyGeneral(const char * p, unsigned int msgLen)
{
	ats_logf(ATSLOG_INFO, "%s,%d: DecodeIdentifyGeneral - decoding incoming message", __FILE__, __LINE__);
	InstrumentIdentifyGeneral instID(m_pLens);
	int ret;
	if ((ret = instID.Decode((const unsigned char *)p)) == 0)
	{
		std::string mac = instID.GetMAC();  // 3 byte mac as hex
		std::map<std::string, LensPeer *>::iterator it;
		it = m_Peers.find(mac);

		if (it == m_Peers.end())  // if it isn't here yet we got the message before 3 instrument status messages showed up (shouldn't happen) - ignore ir.
		{
			ats_logf(ATSLOG_INFO, "%s,%d:" RED_ON "DecodeIdentifyGeneral - Peer %s not found - ignoring!" RESET_COLOR, __FILE__, __LINE__, mac.c_str());
			return;
		}

		LensPeer *pLensPeer = it->second;

		if(pLensPeer->HandleIdentifyGeneral(instID) )
		{
			m_Peers.erase(mac);
		}
	}
	else if (ret == -1)  // ISCP-92
		ats_logf(ATSLOG_INFO, "%s,%d:" CYAN_ON "DecodeIdentifyGeneral - Ignoring message from another gateway" RESET_COLOR, __FILE__, __LINE__);
	else
		ats_logf(ATSLOG_INFO, "%s,%d:" RED_ON "DecodeIdentifyGeneral - failed in decoding incoming message: %d" RESET_COLOR, __FILE__, __LINE__, ret);
}

//-------------------------------------------------------------------------------------------------------------------------------
// An incoming identify request specifies which message should be returned
// page 27 - 2.2.2.8 Identify Frequent Information
void MyData::DecodeIdentifyFrequent(const char * p, unsigned int msgLen)
{
	ats_logf(ATSLOG_INFO, "%s,%d: DecodeIdentifyFrequent - decoding incoming message", __FILE__, __LINE__);
	InstrumentIdentifyFrequent freqID(m_pLens);
	int ret;
	if ((ret = freqID.Decode((const unsigned char *)p)) == 0)
	{
		std::string mac = freqID.GetMAC();  // 3 byte mac as hex
		std::map<std::string, LensPeer *>::iterator it;
		it = m_Peers.find(mac);

		if (it == m_Peers.end())  // if it isn't here yet we got the message before 3 instrument status messages showed up (shouldn't happen) - ignore ir.
		{
			ats_logf(ATSLOG_INFO, "%s,%d:" RED_ON "DecodeIdentifyFrequent - Peer %s not found - ignoring!" RESET_COLOR, __FILE__, __LINE__, mac.c_str());
			return;
		}

		LensPeer *pLensPeer = it->second;

		if(pLensPeer->HandleIdentifyFrequent(freqID) )
		{
			m_Peers.erase(mac);
		}
	}
	else if (ret == -1)  // ISCP-92
		ats_logf(ATSLOG_INFO, "%s,%d:" CYAN_ON "DecodeIdentifyFrequent - Ignoring message from another gateway" RESET_COLOR, __FILE__, __LINE__);
	else
		ats_logf(ATSLOG_INFO, "%s,%d:" RED_ON "DecodeIdentifyFrequent - failed in decoding incoming message: %d" RESET_COLOR, __FILE__, __LINE__, ret);
}

//-------------------------------------------------------------------------------------------------------------------------------
// DecodeIdentifySensorConfig
// page 29 - 2.2.2.7 - Identify Sensor Configuration
//
void MyData::DecodeIdentifySensorConfig(const char * p, unsigned int msgLen)
{
	ats_logf(ATSLOG_INFO, "%s,%d: DecodeIdentifySensor - decoding incoming message", __FILE__, __LINE__);
	InstrumentIdentifySensor instIDSensor(m_pLens);

	if (instIDSensor.Decode((const unsigned char *)p) == 0)
	{
		std::string mac = instIDSensor.MACToHex();
		std::map<std::string, LensPeer *>::iterator it;
		it = m_Peers.find(mac);

		if (it == m_Peers.end())  // if it isn't here yet we got the message before 3 instrument status messages showed up (shouldn't happen) - ignore ir.
			return;  // too many peers - can't add any more.  TODO: check for inet notification.

		LensPeer *pLensPeer = it->second;

		if(pLensPeer->HandleIdentifySensor(instIDSensor))
		{
			m_Peers.erase(mac);
		}
	}
	else
		ats_logf(ATSLOG_INFO, YELLOW_ON "%s,%d: DecodeIdentifySensor - Failed to decode incoming message" RESET_COLOR, __FILE__, __LINE__);

}
//-------------------------------------------------------------------------------------------------------------------------------
// DecodeInstrumentStatusData
// page 20 - 2.2.2.2 - 0x00 - Instrument Status and Data.
// 
//	ISCP-92 - If the Exception bit 4 is set this message is from another gateway and will be ignored.
//
void MyData::DecodeInstrumentStatusData(const char * p, const unsigned int msgLen, const ats::String& s1)
{
	InstrumentStatus instStatus(m_pLens);

	if (instStatus.Decode((const unsigned char *)p) == 0)
	{
		std::string mac = instStatus.GetMAC();
		std::map<std::string, LensPeer *>::iterator it;
		it = m_Peers.find(mac);

		if (it == m_Peers.end())
		{
			if (instStatus.GetInstrumentState() == 0x1b)  // didn't find it so we add it if it isn't the leaving network message (we already left on the first leaving message).
			{
//				ats_logf(ATSLOG_INFO, CYAN_ON "Ignoring status message because it is a 'leaving' message for %s" RESET_COLOR, mac.c_str());
				return;				
			}

			if ((int)m_Peers.size() < (int)g_pLensParms->MaxPeers())  // check that we aren't full
			{

				g_INetIPC.LENSisInConnectingState(true);//ISCP-321
				m_Peers.insert(pair<std::string, LensPeer *>(mac, (LensPeer *)(new LensPeer(m_pLens)) ));
				ats_logf(ATSLOG_INFO, CYAN_ON "Inserting %s into peers" RESET_COLOR, mac.c_str());
				// ISCP-92 - check that this is not another gateway.  This is here becuse you want the gateway to be added to the peer list but ignored otherwise
				if (instStatus.GetExceptions() & (1<<4) ) // indicates a gateway - we do nothing with gateways.
					m_pLens->IncrementPeerCount();  // done here for gateways only
			}
			else
			{	
				return;  // too many peers - can't add any more.  TODO: check for inet notification.
			}

			it = m_Peers.find(mac);  // just added so now we find it so we can handle the message
			LensPeer *pLensPeer = it->second;
			pLensPeer->SetMAC(instStatus.MAC());
		}

		LensPeer *pLensPeer = it->second;

		// ISCP-92 - check that this is not another gateway.  This is here becuse you want the gateway to be added to the peer list but ignored otherwise
		if (instStatus.GetExceptions() & (1<<4) ) // indicates a gateway - we do nothing with gateways.
		{
			pLensPeer->UpdateLostTimer();  // used for LOST status
		  return;
		}

		if(pLensPeer->HandleInstrumentStatusData(instStatus) == 1)
		{
			if ((it = m_Peers.find(mac)) != m_Peers.end())
			{
				delete(it->second);
				m_Peers.erase(mac);
				ats_logf(ATSLOG_INFO, CYAN_ON "Deleting %s from peers" RESET_COLOR, mac.c_str());
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------------------------------------
// An incoming identify request specifies which message should be returned
// page 24 - 2.2.2.3 Identify Request
//
// What this actually means:  When you receive a Network full message it means that the network you are trying to join
// does not have any more room.  NOW the room to join is defined by your own MaxPeers value - not the other units MaxPeers value.
// When the network is full - put the radio to disconnect - which will push it to idle state.  Sleep for 1 minute.  Trigger the
// interrupt to wake the radio and let things go from there.  You might end up back here right away.
//
void MyData::DecodeNetworkError(const char * p, const unsigned int msgLen)
{
	if (int(*(p+4)) == 1)  // network is full
	{
		ats_logf(ATSLOG_ERROR, "DecodeNetworkError: The network is full!");

		m_pLens->NetworkDisconnect();
		sleep(60);  // sleep for a minute then wake up the radio.
		LensSetup();
	}
}
//-------------------------------------------------------------------------------------------------------------------------------
// page 26 - 2.2.2.5 Peer GPS Data
void MyData::DecodePeerGPSData(const char * p, const unsigned int msgLen)
{
	InstrumentGPSData iGPS;
	if (iGPS.Decode((const unsigned char *)p) == 0)
	{
		std::string mac = iGPS.GetMAC();
		std::map<std::string, LensPeer *>::iterator it;
		it = m_Peers.find(mac);

		if (it == m_Peers.end())  // if it isn't here yet we got the message before 3 instrument status messages showed up (shouldn't happen) - ignore ir.
			return;  // too many peers - can't add any more.  TODO: check for inet notification.

		LensPeer *pLensPeer = it->second;

		pLensPeer->HandlePeerGPS(iGPS);
	}
	else
	{
			ats_logf(ATSLOG_ERROR, RED_ON "Unable to decode Peer GPS data message for %s" RESET_COLOR, iGPS.GetMAC().c_str());
	}
}

//-------------------------------------------------------------------------------------------------------------------------------
void MyData::LogHexData(const char *title, const char *data, const int len, const char*color)
{
//	if (g_log.get_level() >= LOG_LEVEL_INFO) //ISCP-337
	{
		ats::String s;
		char str[16];
		for(int i = 0; i < len; ++i)
		{
			if (i%10 == 0)  // write out the 0,10,20, etc byte as white for easier parsing.
				sprintf(str, RESET_COLOR "%02x %s", data[i], color);

			else
				sprintf(str, "%02x ", data[i]);
			s += str;
		}
		ats_logf(ATSLOG_INFO, "%s: %s %s" RESET_COLOR, title, color, s.c_str());
	}
}

//-------------------------------------------------------------------------------------------------------------------------------
void MyData::WaitForLensConnection(MyData &md)
{
	while (!md.m_pLens || !md.m_pLens->IsConnected()	)
	{
		usleep(1000000);
	}
}

//-------------------------------------------------------------------------------------------------------------------------------
void MyData::WaitForLensInit(MyData &md)
{
	static int count = 0;
	while (!md.m_pLens || !md.IsLensRadioInitialized()	)
	{
		usleep(100000);
		if (++count % 50 == 0)
			ats_logf(ATSLOG_ERROR, RED_ON "Waiting for Lens Init (Init flag: %d)" RESET_COLOR, m_pLens->InitFlag() );
	}
}


//-----------------------------------------------------------------------------------
void MyData::ResendCreateInstrument(MyData &md)
{
	ats_logf(ATSLOG_DEBUG, "%s,%d: ResendCreateInstrument - running.", __FILE__, __LINE__);
	std::map<std::string, LensPeer *>::iterator it;
	LensMAC mac;

	for (int i = 0; i < g_INetIPC.MaxPeers(); i++)
	{
		if (g_INetIPC.IsAssigned(i))
		{
			g_INetIPC.GetMAC(i, mac);
			std::map<std::string, LensPeer *>::iterator it;
			it = md.m_Peers.find(mac.toHex());

			if (it != md.m_Peers.end())  // if it is not in m_Peers it is because it has disconnected.  Don't try and reconnect.
			{
				LensPeer *pLP =  it->second;
				pLP->SendINetCreateInstrumentState();
			}
		}
	}
}
