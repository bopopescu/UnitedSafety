/*-------------------------------------------------------------------------------------------
Lens.h - define the messages, registers, and item values for the Whisper Host Interface.

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
#include "../zigbee-monitor/zigbee-base.h"
//#include "messageFormatter.h"
#include "LensRegisters.h"
class MyData;

// SPI related definitions.
#define max_outgoing_buffer_size 4092
#define CMD_READ_BYTES      0x03
#define CMD_WRITE           0x02
#define CMD_WRMR            0x01

// see page 39 - 2.2.4.1 - Registers.
/*#define reg_init_flags                  0x0f00
#define reg_radio_mac_addr              0x0f01
#define reg_radio_hardware_ver          0x0f09
#define reg_radio_os_ver                0x0f0a
#define reg_radio_protocol_ver          0x0f0d
#define reg_network_id                  0x0f10
#define reg_encryption_type             0x0f12
#define reg_encryption_key              0x0f13
#define reg_primary_public_channel      0x0f23
#define reg_second_public_channel       0x0f24
#define reg_network_status              0x0f27
#define reg_max_number_hops             0x0f28
#define reg_leader_qualification_score  0x0f29
#define reg_network_interval            0x0f2a
#define reg_number_peers                0x0f2b
#define reg_max_number_peers            0x0f2c
#define reg_feature_bits                0x0f2d
#define reg_script_crc                  0x0f2f
#define reg_power_transmit              0x0f31
#define reg_current_listening_channel   0x0f32
*/
// Message Identifiers.
/*
#define type_instrument_status_data 0x00  // page 22 - 2.2.2.2 Instrument Status and Data (response to TruLink).
#define type_identify_request 0x05  	  // page 24 - 2.2.2.3 - Identify Request
#define type_identify_general_information 0xA2	// page 27 - 2.2.2.6 - Identify General Information
#define type_identify_sensor_configuration 0xA3 // page 29 - 2.2.2.7 - Identify Sensor Configuration
*/



// Page 35 - Identify General static string in shared memory - this is everything after mac address
const char static_string[] = {0x01, 0x00, 0x01, //radio protocol (9-11)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Unique Device ID (12-27)
	0x15,//device type
	0x01, //device subtype
	0x01, //hardware revision of instrument
	0x02, 0x01, 0x06, // firmware revision
	0x27, // radio HW
	0x02, 0x05, 0x03, // Firmware revision of radio
	0x00 // Field length 0x10 = 0001000 = Username/16 bytes 
//	0x41, 0x77,	0x61, 0x72, 0x65, 0x33, 0x36, 0x30, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, //User name
//	0x30, // Field length 0x30 = 0011000 = Site or Group name/ 16 bytes
//	0x20, 0x20, 0x20, 0x20,	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20  // site or group name
	};

// Page 29 - Identify Sensor Configuration - seems a little long????
const unsigned char identify_sensor_configuration[] = {0x10, 0x08, 0x01, 0x05, 0x22, 0x11, 0x00, 0x00, 0xeb, 0x00, 0xc3, 0x00, 0x00, 0x00, 0x00, 0xFF};




#define ISC_RESETRADIO  _IO('r', 0)
#define ISC_WAKEUPRADIO  _IO('r', 1)

typedef enum
{
	GENERAL_USERNAME = 0x00,
	GENERAL_SITENAME = 0X01,
}generalFieldData;


struct messageFrame
{
	int data;
	ats::String msg1;
	ats::String msg2;
	commonMessage* ptr;

public:
	messageFrame(int e, ats::String i, ats::String m, commonMessage* p = NULL):data(e), msg1(i), msg2(m), ptr(p){}
	messageFrame(){}
};

enum LensMessageID
{
	InstrumentStatusData = 0x00,// page 20 - 2.2.2.2 Instrument Status and Data (response to TruLink).
	NetworkError = 0x77,				// page 33 - 2.2.2.9 - Network Error
	IdentifyRequest = 0x05,  	  // page 24 - 2.2.2.3 - Identify Request
	IdentifyGeneral = 0xA2,	    // page 27 - 2.2.2.6 - Identify General Information
	IdentifySensorConfig = 0xA3, // page 29 - 2.2.2.7 - Identify Sensor Configuration
	IdentifyFrequent = 0xA4, 		// page 32 - 2.2.2.8 - Identify Frequent
	PeerGPSData = 0x84					// page 26 - 2.2.2.5 - Peer GPS Data
};

class Lens
{
private:
	int fd;
	int status;
	ats::String devName;
	LensRegisters m_LensRegisters;
	pthread_mutex_t m_SPIMutex;

public:
	Lens();

	~Lens()
	{
		pthread_mutex_destroy(&m_SPIMutex);
		close(fd);
	}

	void write(int add1, int add2, int length, char *buff)
	{
		pthread_mutex_lock(&m_SPIMutex);
		spi_write(add1, add2, length, buff);
		pthread_mutex_unlock(&m_SPIMutex);
	}


	LensRegisters & GetLensRegisters(){return m_LensRegisters;};
	
	void StartRadio();  // Run everything required to get the radio ready to connect.

	char ReadInitFlag();
	char InitFlag();
	void ReadRegisters();  // read from memory and then set LensRegisters
	void WriteRegisters(); // write updated values from ifconfig to the memory - then update LensRegisters
	static void reset();
	int GetRadioNetworkStatus();
	char GetINetConnection();
	void SetINetConnectionFlag(char inetconn);
	int set_sm_reg();
	void DecrementPeerCount();
	void IncrementPeerCount();
	void WriteIdentifyGeneral();
	void set_identify_sensor_configuration();
	bool read_from_incoming_mailbox(char *data, int &length);
	bool send_to_outgoing_mailbox(const char *data, int length);
	int spi_init(const ats::String& name);
	void dump_registers();
	void dump_registers(std::string fname, std::string title);  // output registers to fname with a title.
	void SetRemoteDeviceVerbose(char * mac, int timeout);
	int SetNetworkConfiguration();
	int SetNetworkEncryption();
	void NetworkConnect();
	void NetworkDisconnect();
	bool IsConnected();
	void WriteStatusData(char *buf, int len);
	bool GetStatusDataRead();
	bool LowLevelMemoryTest();  //  returns true if able to write then read the memory chip on the TruLink.
	bool SwitchToUploadMode();  // returns true when upload mode established.  False if can't change after 30 seconds.
	
	void SetTestMode(int channel); // see 2.2.1.11 Enter Test Mode
	bool IsRadioInitialized() 
	{ 
		return (0xA5 == InitFlag());
	}
	void SetUploadMode();  // puts the LENS into upload mode
	void Wakeup(int urgent_fd);
	void Reconnect();  // disconnects and reconnects to the Lens Network.
	void Shutdown();  // broadcasts shutdown message 3 times then turns the lens off.
protected:
	void spi_write(int add1, int add2, int nbytes, char *value);
	void spi_read(int add1,int add2,int nbytes, char *destbuf);
private:
	std::string toHex(char * buf, int len);// will convert all 'len' characters regardless of 0x00 value or not
	void LogHexData(const char *title, const char *data, const int len, const char*color);
};


typedef ats::ClientMessageQueue<struct messageFrame> message_queue;





