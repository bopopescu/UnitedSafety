#pragma once
#include <sys/time.h>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include "socket_interface.h"
#include "LensMAC.h"
#include "LensRegisters.h"
#include "AFS_Timer.h"
#include <ats-common.h>
#include <atslogger.h>
// INet Interprocess Global memory
//
// This defines all of the variables needed for accessing and passing data from the
// isc-lens program and the packetizer-inet
//
#include "string.h"
#include <unistd.h>
#include <stdint.h>

typedef enum
{
	NORMAL = 0x00,
	SHUTDOWN = 0x04,
	LOW_BATTERY = 0x20
} TRULINK_RUN_STATE;



struct INET_INSTRUMENT_DATA
{
	bool m_bIsAssigned;  // true if slot is assigned to an instrument
	bool m_bNew;  // true if updated object ID from iNet
	bool m_bIsLeaving; // true if we are leaving so that iNet response doesn't set New and IsAssigned.
	bool m_bSentIdentify;  // true if instrument has sent an Identify via Iridium
	char m_ObjectID[16]; //returned from createinst - used in update url
  LensMAC m_MAC;
	
	INET_INSTRUMENT_DATA() : m_bIsAssigned(false), m_bNew(false), m_bIsLeaving(false), m_bSentIdentify(false) 
	{
		strcpy(m_ObjectID, "0");
	}
};

struct INET_IPC_DATA
{
	bool m_bLensScriptUploading;
	bool m_bScriptUploadFailure;
	bool m_bINetConnected;	// true if the system gets an auth token
	bool m_bLensConnectionStatus;  // set to true in isc-lens when the status registers are read and set. 
	bool m_bLensInConnectingState;  // set to true in isc-lens when the peer is in connecting state. // ISCP-321
	
	int  m_PeerCount; // number of instruments attached to lens
	int  m_MaxPeers;  // maximum number of peers from db-config.
	uint64_t  m_GatewayObjectID; //returned from creategateway - used in update gateway
	TRULINK_RUN_STATE	 m_RunState;  // 0:running normal 1: shutting down 2:low battery
	INET_INSTRUMENT_DATA m_InstrumentData[29];  // up to 29 instruments attached - usually 24.
	bool m_bNewRegisters;  // set to true when Settings API comes in.  Cleared on isc-lens side when new values written to Registers.
	bool m_bValidLensOS;  // true if valid lens OS. If false then all lens function is off and we flash fast red.
	LensRegisters m_LensRegisters;
	int m_LensNetworkStatus;
	bool m_bUsingIridium; // true if using iridium or faking iridium in test mode.  Causes updateinst to be generated without proper id.
	bool m_bSendingToINet	;
	bool m_bSwitchedToIridium;  // if true isc-lens will recreate the CreateInstrument messages to send via Iridium
	bool m_bLENSInitFailure;
	bool m_bLENSSPIFailure;
	bool m_bLENSUARTFailure;
	

public:
	INET_IPC_DATA() : m_bLensScriptUploading(false), m_bScriptUploadFailure(false), m_bINetConnected(false), m_bLensConnectionStatus(false), m_bLensInConnectingState(false),m_PeerCount(0), m_RunState(NORMAL),
		m_bValidLensOS(true), m_bUsingIridium(false)
	{
		m_bLENSInitFailure = false;
		m_bLENSSPIFailure = false;
		m_bLENSUARTFailure = false;


	}
};

class INET_IPC
{
	INET_IPC_DATA *ipc;
	boost::interprocess::shared_memory_object * shdmem;
	boost::interprocess::mapped_region * region;

public:
	INET_IPC()
	{
		bool init = false;
		// allocate the shared memory for INET_IPC_DATA
		try
		{
			shdmem = new boost::interprocess::shared_memory_object (boost::interprocess::open_only, "INET_DATA", boost::interprocess::read_write);
		}
		catch(boost::interprocess::interprocess_exception e)
		{
			shdmem = new boost::interprocess::shared_memory_object (boost::interprocess::create_only, "INET_DATA", boost::interprocess::read_write, boost::interprocess::permissions(0666));
			init = true;
		}
		shdmem->truncate(sizeof(INET_IPC_DATA));
		region = new boost::interprocess::mapped_region (*shdmem, boost::interprocess::read_write);
		ipc = static_cast<INET_IPC_DATA *>(region->get_address());

		if(init)
		{
			initialize();
		}
	}

	~INET_IPC()
	{
		delete region;
		delete shdmem;
	}

	void initialize()
	{
		INET_IPC_DATA data;
		memcpy(ipc, &data, sizeof(INET_IPC_DATA));
	}

	void Dump()
	{
		printf("INet Connected %s\n", ipc->m_bINetConnected ? "true" : "false");
		printf("Lens Connection status %s\n", ipc->m_bLensConnectionStatus ? "true" : "false");
		printf("Lens Valid OS %s\n", ipc->m_bValidLensOS ? "true" : "false");
		printf("Peer count %d of %d\n", ipc->m_PeerCount, ipc->m_MaxPeers);

		for (short i = 0; i < ipc->m_MaxPeers; i++)
		{
			if (ipc->m_InstrumentData[i].m_bIsAssigned || ipc->m_InstrumentData[i].m_bIsLeaving)
				printf("IPC[%d]: Assigned:%s New:%s  Leaving:%s   ID:%s  MAC:%s\n", i, ipc->m_InstrumentData[i].m_bIsAssigned ? "true" : "false",
					ipc->m_InstrumentData[i].m_bNew ? "true" : "false", 
					ipc->m_InstrumentData[i].m_bIsLeaving ? "true" : "false", 
					ipc->m_InstrumentData[i].m_ObjectID, 
					ipc->m_InstrumentData[i].m_MAC.toHex().c_str());
		}
			
	}

	bool INetConnected() const { return ipc->m_bINetConnected;}
	void INetConnected(const bool val) { if (val != ipc->m_bINetConnected){ipc->m_bINetConnected = val;  SetLensLED();}}
	
	bool UsingIridium() const { return ipc->m_bUsingIridium; }
	void UsingIridium(const bool val) {ipc->m_bUsingIridium = val; SetLensLED();}
	
	bool LensScriptUploading() const { return ipc->m_bLensScriptUploading; }
	void LensScriptUploading(const bool val) { ipc->m_bLensScriptUploading = val;  SetLensLED();}

	bool ScriptUploadFailure() const { return ipc->m_bScriptUploadFailure; }
	void ScriptUploadFailure(const bool val) { ipc->m_bScriptUploadFailure = val;  SetLensLED();}

	bool LENSInitFailure() const { return ipc->m_bLENSInitFailure; }
	void LENSInitFailure(const bool val) { ipc->m_bLENSInitFailure = val;  SetLensLED();}
	
	bool LENSSPIFailure() const { return ipc->m_bLENSSPIFailure; }
	void LENSSPIFailure(const bool val) { ipc->m_bLENSSPIFailure = val;  SetLensLED();}

	bool LENSUARTFailure() const { return ipc->m_bLENSUARTFailure; }
	void LENSUARTFailure(const bool val) { ipc->m_bLENSUARTFailure = val;  SetLensLED();}

	bool LensConnectionStatus() const { return ipc->m_bLensConnectionStatus; }
	void LensConnectionStatus(const bool val) { ipc->m_bLensConnectionStatus = val; SetLensLED();}
	
	bool LENSisInConnectingState() const { return ipc->m_bLensInConnectingState; }//ISCP-321
	void LENSisInConnectingState(const bool val) { ipc->m_bLensInConnectingState = val; SetLensLED();}//ISCP-321



	bool NewRegisters() const { return ipc->m_bNewRegisters; }
	void NewRegisters(const bool val) { ipc->m_bNewRegisters = val;}
	
	bool SwitchedToIridium() const { return ipc->m_bSwitchedToIridium; }
	void SwitchedToIridium(const bool val) { ipc->m_bSwitchedToIridium = val;}
	
	bool ValidLensOS() const { return ipc->m_bValidLensOS; }
	void ValidLensOS(const bool val) { ipc->m_bValidLensOS = val; SetLensLED();}
	
	int PeerCount()const {return ipc->m_PeerCount;}
	void PeerCount(const int count){ipc->m_PeerCount = count; SetLensLED();} 

	void SendingToINet(const bool bSending){ipc->m_bSendingToINet = bSending; SetLensLED();} 

	int MaxPeers()const {return ipc->m_MaxPeers;}
	void MaxPeers(const int count){ipc->m_MaxPeers = count;} 

	int LensNetworkStatus()const {return ipc->m_LensNetworkStatus;}
	void LensNetworkStatus(const int val){ipc->m_LensNetworkStatus = val;SetLensLED();} 

	uint64_t  GatewayObjectID() const {return ipc->m_GatewayObjectID;} //returned from creategateway - used in update gateway
	void GatewayObjectID(const uint64_t val) {ipc->m_GatewayObjectID = val;}

	int RunState()const {return ipc->m_RunState;}
	void RunState(const 	TRULINK_RUN_STATE state){ipc->m_RunState = state;} 

	LensRegisters GetLensRegisters() const {return ipc->m_LensRegisters;}
	void  SetLensRegisters(const LensRegisters lr) const {ipc->m_LensRegisters = lr;}
	
//--------------------------------------------------------------------------------------------------
	INET_INSTRUMENT_DATA * InstrumentData(int idx) // !NOTE - check for null return!
	{
		if (idx >=0 && idx < ipc->m_MaxPeers && ipc->m_InstrumentData[idx].m_bIsAssigned)
			return &ipc->m_InstrumentData[idx];
		return NULL;
	}
//--------------------------------------------------------------------------------------------------
	void InstrumentData(int idx, const INET_INSTRUMENT_DATA &data) { if (idx >= 0 && idx < ipc->m_MaxPeers)ipc->m_InstrumentData[idx] = data;}

	//--------------------------------------------------------------------------------------------------
	int  Find(LensMAC mac)
	{
		for (short i = 0; i < ipc->m_MaxPeers; i++)
		{
			if (ipc->m_InstrumentData[i].m_MAC == mac && (ipc->m_InstrumentData[i].m_bIsAssigned || ipc->m_InstrumentData[i].m_bIsLeaving))
				return i;
		}
		return -1;
	}

	//--------------------------------------------------------------------------------------------------
	// Adds or updates the mac with the id and sets the bNew flag to true.
	// RETURNS: index of the InstrumentData or -1 if the array is full
	//
	int Add(LensMAC mac, std::string id)
	{
		int idx;
		if ( (idx = Find(mac)) != -1)
		{
			if (ipc->m_InstrumentData[idx].m_bIsLeaving)
			{
				ipc->m_InstrumentData[idx].m_bIsLeaving = false;
				return -2;  // indicates the leaving the network message
			}
			else
			{
				ipc->m_InstrumentData[idx].m_bIsAssigned = true;
				strncpy(ipc->m_InstrumentData[idx].m_ObjectID, id.c_str(), (id.length() < 16 ? id.length() : 15) );
				ipc->m_InstrumentData[idx].m_ObjectID[(id.length() < 16 ? id.length() : 15)] = '\0';
				ipc->m_InstrumentData[idx].m_bNew = true;
				return idx;
			}
		}

		for (short i = 0; i < ipc->m_MaxPeers; i++)
		{
			if (!ipc->m_InstrumentData[i].m_bIsAssigned)
			{
				idx = i;
				ipc->m_InstrumentData[i].m_bIsAssigned = true;
				ipc->m_InstrumentData[i].m_MAC = mac;
				strncpy(ipc->m_InstrumentData[idx].m_ObjectID,id.c_str(), (id.length() < 16 ? id.length() : 15) );
				ipc->m_InstrumentData[i].m_bNew = true;
				ipc->m_PeerCount++;
				return idx;
			}
		}
		return -1;  // the array is fully used.
	}

	//--------------------------------------------------------------------------------------
	int Add(LensMAC mac)
	{
		int idx;
		if ( (idx = Find(mac)) != -1)
		{
			if (ipc->m_InstrumentData[idx].m_bIsLeaving)
			{
				ipc->m_InstrumentData[idx].m_bIsLeaving = false;
				return -2;  // indicates the leaving the network message
			}
			else
			{
				ipc->m_InstrumentData[idx].m_bIsAssigned = true;
				ipc->m_InstrumentData[idx].m_ObjectID[0] = '0';
				ipc->m_InstrumentData[idx].m_bNew = false;
				return idx;
			}
		}

		for (short i = 0; i < ipc->m_MaxPeers; i++)
		{
			if (!ipc->m_InstrumentData[i].m_bIsAssigned)
			{
				idx = i;
				ipc->m_InstrumentData[i].m_bIsAssigned = true;
				ipc->m_InstrumentData[i].m_MAC = mac;
				ipc->m_InstrumentData[i].m_ObjectID[0] = '0';
				ipc->m_InstrumentData[i].m_bNew = false;
				ipc->m_PeerCount++;
				return idx;
			}
		}
		return -1;  // the array is fully used.
	}
	
	
	bool IsNew(int idx){ if (idx < ipc->m_MaxPeers && ipc->m_InstrumentData[idx].m_bIsAssigned) return ipc->m_InstrumentData[idx].m_bNew; return false;}
	bool IsAssigned(int idx){ if (idx < ipc->m_MaxPeers) return ipc->m_InstrumentData[idx].m_bIsAssigned; return false;}
	int ClearNew(LensMAC mac)
	{
		int idx;
		if ( (idx = Find(mac)) != -1)
		{
			ipc->m_InstrumentData[idx].m_bNew = false;
			return idx;
		}
		return -1; // MAC not found.
	}
	int LeaveNetwork(LensMAC mac)
	{
		int idx;
		if ( (idx = Find(mac)) != -1)
		{
			ipc->m_InstrumentData[idx].m_bIsAssigned = false;
			ipc->m_InstrumentData[idx].m_bIsLeaving = true;
			ipc->m_InstrumentData[idx].m_ObjectID[0] = '0';
			ipc->m_InstrumentData[idx].m_bNew = false;
			return 0;
		}
		return -1; // MAC not found.
	}
		
	int SentIdentify(LensMAC mac)  // called on the Iridium side the first time the Identify portion is sent for an instrument
	{
		int idx;
		if ( (idx = Find(mac)) != -1)
		{
			ipc->m_InstrumentData[idx].m_bSentIdentify = true;
			return 0;
		}
		return -1; // MAC not found.
	}


	bool IsIdentifySent(LensMAC mac)
	{
		int idx;
		if ( (idx = Find(mac)) != -1)
		{
			return ipc->m_InstrumentData[idx].m_bSentIdentify ;
		}

		return false; // MAC not found.  (this is bad - by the time you check this the instrument should be in the list!)		
	}

	int GetMAC(int idx, LensMAC &mac) 
	{
		if (idx < ipc->m_MaxPeers)
		{ 
			mac = ipc->m_InstrumentData[idx].m_MAC; 
			return 0;
		}
		return -1;
	}
	
	std::string  FindInstrumentObjectID( LensMAC &mac)     // returns 0 if not found
	{
		std::string ret = "0";
		int idx = 0;
		if ((idx = Find(mac)) < 0)
			return ret;
		ret.assign(ipc->m_InstrumentData[idx].m_ObjectID);
		return ret;
	}
	
//--------------------------------------------------------------------------------------------------
// Lens LED states:
//  Flashing indicates no connection to INet, Yellow is connection to lens radio but no instruments
//	Green is connection to instruments.
//  
//  Off - no lens found -  if we have no lens then the INet connection is irrelevant.
//  Flashing Yellow - lens found - no connection to INet
//  Yellow - lens found - INet connection 
//	Flashing Green - lens found, Instrument connected, no connection to INet
//	Green  - lens found, Instrument connected,  connection to INet
	void SetLensLED() const
	{
		int cur_state = 0;
		//<ISCP-163>
		static bool g_bSentInetNotification_script = false; 	
		static bool g_bSentInetNotification = false;
		static bool g_bSentInetNotification_LENS = false;
		static bool g_bSentInetNotification_SPI = false;
		static bool g_bSentInetNotification_UART = false;
		//</ISCP-163>

		// Lens LED
		if(LENSInitFailure()) //<ISCP-163>
			cur_state = 8;
		else if(LENSSPIFailure()) //<ISCP-163>
			cur_state = 9;
		else if(LENSUARTFailure()) //<ISCP-163>
			cur_state = 10;
		else if (LensScriptUploading())
			cur_state = 1;
		else if (!ValidLensOS())
			cur_state = 3;
		else if (ScriptUploadFailure() ) //ISCP-321
			cur_state = 2;
		else if (!LensConnectionStatus())
			cur_state = 4;
		else if (LENSisInConnectingState())
		{
// Peer is in connection State, Set Yellow LED
			cur_state = 7;//ISCP-321
		}
		else if (LensConnectionStatus())
		{
			if (PeerCount() == 0) // set to solid green
				cur_state = 5;
			else
				cur_state = 6;  // set to green flashing
		}

		ats_logf(ATSLOG_DEBUG, "LED SET Cur State A[%d]\r",cur_state);
		
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink del lens.g \r");
		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink del lens.r \r");
		usleep(50 * 1000);

		switch (cur_state)
		{

			case 1:// uploading - flash yelloow
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.g led=inp6   script=\"0,100000;1,1000000;\" \r");
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.r led=inp6.r script=\"0,100000;1,1000000;\" \r");							
				break;
			/*case 2:  // failed to upload -solid yellow
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.g led=inp6   script=\"0,1000000;\" \r");//ISCP-250
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.r led=inp6.r script=\"1,1000000;\" \r");							
				break;*/
			case 2: // changing the error indication for script upload failure as per TGX - Error Code rev1.05 sheet
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.g led=inp6   script=\"0,1000000;\" \r");
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.r led=inp6.r script=\"0,100000;1,100000;\" \r");							
				if (!g_bSentInetNotification_script && ScriptUploadFailure()) //<ISCP-163>
				{
					ats_logf(ATSLOG_DEBUG, "LENS sending script upload failure inet_error message\r");
					g_bSentInetNotification_script = true;
					AFS_Timer t;
					t.SetTime();
					std::string user_data = "922," + t.GetTimestampWithOS() + ", LENS Script Upload Failed";
					user_data = ats::to_hex(user_data);

					send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
				}				
			
				break;
			case 3:  // invalid LENS os - flash red
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.g led=inp6   script=\"0,1000000;\" \r");
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.r led=inp6.r script=\"0,100000;1,100000;\" \r");							
				if (!g_bSentInetNotification) //<ISCP-163>
				{
					ats_logf(ATSLOG_DEBUG, "LENS sending invalid OS inet_error message\r");
					g_bSentInetNotification = true;
					AFS_Timer t;
					t.SetTime();
					std::string user_data = "1007," + t.GetTimestampWithOS() + ", Invalid LENS OS";
					user_data = ats::to_hex(user_data);

					send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
				}				
				break;
			case 4: // LENS connection status is not good - (red)
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.g led=inp6   script=\"0,1000000;\" \r");
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.r led=inp6.r script=\"1,1000000;\" \r");							
				break;
			case 5: // LENS connection status is good - no instruments (Solid green)
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.g led=inp6 script=\"1,1000000;\" \r");
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.r led=inp6.r script=\"0,1000000;\" \r");							
				break;
			case 6: // LENS connected to units status
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.g led=inp6   script=\"0,100000;1,1000000;\" \r");
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.r led=inp6.r script=\"0,1000000;\" \r");							
				break;
			case 7: // LENS connecting to Peer
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.g led=inp6   script=\"1,1000000;\" \r");
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.r led=inp6.r script=\"1,1000000;\" \r");							
				break;
                        case 8:
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.g led=inp6   script=\"0,1000000;\" \r");
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.r led=inp6.r script=\"1,1000000;\" \r");							
				if (!g_bSentInetNotification_LENS) //<ISCP-163>
				{
					ats_logf(ATSLOG_DEBUG, "LENS sending device inet_error message\r");
					g_bSentInetNotification_LENS = true;
					AFS_Timer t;
					t.SetTime();
					std::string user_data = "971," + t.GetTimestampWithOS() + ", LENS Device Error";
					user_data = ats::to_hex(user_data);
					send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
				}	
			break;
			case 9:
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.g led=inp6   script=\"0,1000000;\" \r");
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.r led=inp6.r script=\"1,1000000;\" \r");							
				if (!g_bSentInetNotification_SPI) //<ISCP-163>
				{
					ats_logf(ATSLOG_DEBUG, "LENS sending SPI inet_error message\r");
					g_bSentInetNotification_SPI = true;
					AFS_Timer t;
					t.SetTime();
					std::string user_data = "1018," + t.GetTimestampWithOS() + ", SPI Error";
					user_data = ats::to_hex(user_data);
					send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
				}	

			break;
			case 10:
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.g led=inp6   script=\"0,1000000;\" \r");
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.r led=inp6.r script=\"0,100000;1,100000;\" \r");	
				if (!g_bSentInetNotification_UART) //<ISCP-163>
				{
					ats_logf(ATSLOG_DEBUG, "LENS sending UART inet_error message\r");
					g_bSentInetNotification_UART = true;
					AFS_Timer t;
					t.SetTime();
					std::string user_data = "1005," + t.GetTimestampWithOS() + ", LENS UART Error";
					user_data = ats::to_hex(user_data);
					send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
				}	

			break;
			default:
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.g led=inp6 script=\"0,1000000;\" \r");
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=lens.r led=inp6.r script=\"0,1000000;\" \r");							
				break;
		}
		// now set the INP5 LED for the inet connection
		
//		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink del inet.g \r");
//		send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink del inet.r \r");
		usleep(25000);

		if (INetConnected() || UsingIridium())
		{
			//usleep(25000);
			//if (ipc->m_bSendingToINet && (UsingIridium() == false))
			if (ipc->m_bSendingToINet)
			{
				//send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=inet.r led=zigbee.r script=\"1,1000000\" \r");
				//send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=inet.g led=zigbee   script=\"1,1000000\" \r");
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=inet.g led=zigbee   script=\"0,250000;1,250000\" \r");//ISCP-308
			}
			else
			{				
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=inet.r led=zigbee.r script=\"0,1000000\" \r");
				send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=inet.g led=zigbee   script=\"1,1000000\" \r");
		}
		}
		else
		{
			send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=inet.g led=zigbee   script=\"0,1000000\" \r");
			send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink add name=inet.r led=zigbee.r script=\"1,1000000\" \r");
		}
	}
};
