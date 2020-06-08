/*-------------------------------------------------------------------------------------------
Lens.h - definc the messages, registers, and item values for the Whisper Host Interface.

This is working off of the 11/13/2018 version of the document.  All page references will be
based on this document.

Dave Huff - Nov 19, 2018
-------------------------------------------------------------------------------------------*/


#pragma once
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/ioctl.h>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <math.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <boost/circular_buffer.hpp>

#include "timer-event.h"
#include "socket_interface.h"
#include "event_listener.h"
#include "state_machine.h"
#include "state-machine-data.h"
#include "command_line_parser.h"
#include "ClientMessageManager.h"
#include "ConfigDB.h"
#include "NMEA_Client.h"
#include "ats-common.h"
#include "AFS_Timer.h"
#include "../zigbee-monitor/zigbee-base.h"
#include "messageFormatter.h"
#include "lens.h"
#include "LensPeer.h"

typedef enum
{
	sensorType_unknown       = 0x00,
	sensorType_IR            = 0x03,
	sensorType_Toxic         = 0x04,
	sensorType_Oxygen        = 0x05,
	sensorType_LEL           = 0x08,
	sensorType_PID           = 0x09,
	sensorType_Dual_Toxic    = 0x0a,
}sensorType;

///-----------------------------------------------------------------------------------------------
class MyData
{
public:

	MyData(Lens *m_pLens);

	~MyData()
	{
		delete m_ventisdata_message;
		std::map<std::string, boost::circular_buffer<time_t> >::iterator itr = incoming_time_map.begin();
		while (itr != incoming_time_map.end())
		{
			incoming_time_map.erase(itr++);
		}
	}

	static void* DecodeMessages_thread(void* p);
	static void* fetchmessage_thread(void* p);
	static void* urgentInterrupt_thread(void* p);
	static void* INetMonitor_thread(void* p);
	static void* LostPeer_thread(void* p);
	static void* Monitor_thread(void* p);
	
	void DecodeInstrumentStatusData(const char *p, const unsigned int msgLen, const ats::String& s1);
	void DecodeIdentifyGeneral(const char * p, unsigned int msgLen);
	void DecodeIdentifySensorConfig(const char * p, const unsigned int msgLen);
	void DecodeIdentifyFrequent(const char * p, unsigned int msgLen);
	void DecodeNetworkError(const char * p, const unsigned int msgLen);
	void DecodePeerGPSData(const char * p, const unsigned int msgLen);

	//static void* toinet_thread(void* p);

	void lock() const
	{
		pthread_mutex_lock(m_mutex);
	}

	void unlock() const
	{
		pthread_mutex_unlock(m_mutex);
	}

	Lens* m_pLens;

	ats::StringMap m_config;
	ats::ClientMessageManager<struct messageFrame > m_msg_manager;

	void LensSetup();
	void disconnect();
	int m_urgent_fd;

	const ClientData* get_ventisdata_key() const {return m_ventisdata_message;} // used to transfer incoming LENS data from fetchmessage to incomingmanage

	void post_message(const ClientData* p_client, const messageFrame& msg);
	bool get_message(const ClientData* p_client, messageFrame& p_msg);

	void messageParserAndSend(const char *data, int length, MyData *);
	void resetRadio();
	void WakeupRadio();
	
	// Hard code only for Cohen project demo. our Controlcenter only accept all digital numbers as device ID.
	static void sendSOSToCC(ats::String);
	static void sendSOSCancelToCC(ats::String);
	
	void DecodeIdentifyRequest(const char *p);
	void UpdateInstrumentStatus(MyData &md);

private:
	pthread_mutex_t* m_mutex;
	pthread_t m_DecodeMessages_thread;
	pthread_t m_updatemessage_thread;
	pthread_t m_toinet_thread;
	pthread_t m_fetchmessage_thread;
	pthread_t m_urgentInterrupt_thread;
	pthread_t m_INetMonitor_thread;
	pthread_t m_LostPeer_thread;
	pthread_t m_Monitor_thread;
	ClientData* m_ventisdata_message;

	std::map<string, boost::circular_buffer<time_t> > incoming_time_map;
	std::map<std::string, LensPeer *> m_Peers;

	static ats::String trim(const ats::String& str);
	static ats::String getSN(const ats::String& str);
	void LogHexData(const char *title, const char *data, const int len, const char*color);
	void WaitForLensConnection(MyData &md);
	void WaitForLensInit(MyData &md);
	bool IsLensRadioInitialized() 
	{ 
		return (0xA5 == m_pLens->InitFlag());
	}

	void ResendCreateInstrument(MyData &md); // need to resend instruments on cell/iridium switchover.
	//	static void GetUsrDataGPSPosition(ats::String& gpsStr);
};


