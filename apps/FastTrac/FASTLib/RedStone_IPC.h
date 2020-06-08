#pragma once
#include <sys/time.h>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
// RedStone Interprocess Global memory
//
// This defines all of the variables needed for accessing data from the
// current state of the RedStone system - add what you need here.
//
#include "string.h"
#include <unistd.h>
#include "ProcID.h"
#include "AFS_Timer.h"

enum AWARE_MODEM_STATES
{
	AMS_NO_COMM,
	AMS_NO_SIM,
	AMS_NO_CARRIER,
	AMS_NO_PPP,
	AMS_PPP_ESTABLISHED,
	AMS_UNSET
};

enum AWARE_PPP_STATES
{
	APS_UNCONNECTED,
	APS_CONNECTING,
	APS_CONNECTED,
	APS_POWER_OFF
};


struct REDSTONE_IPC_DATA
{
	bool m_bIgnitionOn;	// true if the ignition is on
	//292
	bool m_bIgnitionShutdown;
	bool m_bStarted;		 // true if started or not stopped long enough
	short m_RPM ;	 // from the can bus
	short m_Speed;	// from the can bus
	bool m_SeatbeltBuckled; // true if seatbelt is buckled
	short m_rssi; // from the telit modem
	time_t m_ObdLastUpdated; // time of the last On Board Diagnostics (OBD) message
	double m_BatteryVoltage; // from the can bus
	bool m_BatteryVoltageValid; // true if value in m_BatteryVoltage is valid
	unsigned char m_GPIO;	// bitfield of current input GPIOs
	bool m_bSendingEmail;
	unsigned long m_LastMID;	// MID of last message successfully sent
	bool m_IridiumEnabled;	// true if using Iridium to send
	bool m_LastSendFailed;	// true if we failed to send data in a packetizer - false if a Send is successful
	AFS_Timer m_LastSendFailTimer;	// Time since last failed to send (ignore if m_LastSendFailed is false);
	short m_wifiLEDStatus; // 0: OFF, 1: AP ON, 2: Client ON, 3: traffic
	AWARE_MODEM_STATES m_ModemState;    // 0:NoComm, 1:No SIM found, 2:Sim, No carrier 3: carrier, no ppp 4:ppp established
	AWARE_PPP_STATES m_pppState;      // 0:no ppp established, 1:ppp established (only set in connect-ppp)
	time_t m_firmUpdatetime; //ISCP-209
	bool m_isNewFirmAvailable; //ISCP-209
	bool m_isEthernetWorking; //ISCP-295
	bool m_isInstrumentShuttingDown;//ISC-162
	bool m_isShutDownMessageSent;//ISC-162
public:
	REDSTONE_IPC_DATA() : m_bIgnitionOn(false), m_RPM(0), m_Speed(0), m_SeatbeltBuckled(true), m_rssi(0), \
				m_ObdLastUpdated(0), m_BatteryVoltageValid(false), m_GPIO(0),m_bSendingEmail(false), \
				m_ModemState(AMS_NO_COMM), m_pppState(APS_UNCONNECTED),m_firmUpdatetime(10000), \
				m_isNewFirmAvailable(false),m_isEthernetWorking(false),m_isInstrumentShuttingDown(false), m_isShutDownMessageSent(false){}
};

class REDSTONE_IPC
{
	REDSTONE_IPC_DATA *ipc;
	boost::interprocess::shared_memory_object * shdmem;
	boost::interprocess::mapped_region * region;

public:
	REDSTONE_IPC()
	{
		bool init = false;
		// allocate the shared memory for REDSTONE_IPC_DATA
		try
		{
			shdmem = new boost::interprocess::shared_memory_object (boost::interprocess::open_only, "AFS_REDSTONE", boost::interprocess::read_write);
		}
		catch(boost::interprocess::interprocess_exception e)
		{
			shdmem = new boost::interprocess::shared_memory_object (boost::interprocess::create_only, "AFS_REDSTONE", boost::interprocess::read_write, boost::interprocess::permissions(0666));
			init = true;
		}
		shdmem->truncate(sizeof(REDSTONE_IPC_DATA));
		region = new boost::interprocess::mapped_region (*shdmem, boost::interprocess::read_write);
		ipc = static_cast<REDSTONE_IPC_DATA *>(region->get_address());
		if(init)
		{
			initialize();
		}
	}

	~REDSTONE_IPC()
	{
		delete region;
		delete shdmem;
	}

	double BatteryVoltage() { return ipc->m_BatteryVoltage; }
	void	BatteryVoltage(double val) { ipc->m_BatteryVoltage =	val; }
	
	bool BatteryVoltageValid() { return ipc->m_BatteryVoltageValid; }
	void BatteryVoltageValid(bool val) { ipc->m_BatteryVoltageValid = val; }

	short Rssi() { return ipc->m_rssi; }
	void	Rssi(short val) { ipc->m_rssi =	val; }

	short RPM() { return ipc->m_RPM; }
	void	RPM(short val) { ipc->m_RPM =	val; }

	short Speed() { return ipc->m_Speed; }
	void	Speed(short val) { ipc->m_Speed =	val; }

	bool IgnitionOn() { return ipc->m_bIgnitionOn; }
	void IgnitionOn(bool val){ipc->m_bIgnitionOn = val;}
	// 292
	bool IgnitionShutdown() {return ipc->m_bIgnitionShutdown;}
	void IgnitionShutdown(bool val){ipc->m_bIgnitionShutdown = val;}

	bool Started() { return ipc->m_bStarted; }
	void Started(bool val){ipc->m_bStarted = val;}

	bool SeatbeltBuckled() { return ipc->m_SeatbeltBuckled; }
	void SeatbeltBuckled(bool val){ipc->m_SeatbeltBuckled = val;}

	time_t ObdLastUpdated() { return ipc->m_ObdLastUpdated; }
	void ObdLastUpdated(time_t val){ipc->m_ObdLastUpdated = val;}

	unsigned char GPIO() { return ipc->m_GPIO; }
	void GPIO(unsigned char val) { ipc->m_GPIO = val; }

	AWARE_MODEM_STATES ModemState() { return ipc->m_ModemState; }
	void ModemState(AWARE_MODEM_STATES val) { if (ipc->m_ModemState != val) ipc->m_ModemState = val; }
	
	AWARE_PPP_STATES pppState() { return ipc->m_pppState; }
	void pppState(AWARE_PPP_STATES val) { if (ipc->m_pppState != val) ipc->m_pppState = val; }

	bool SendingEmail() { return ipc->m_bSendingEmail; }
	void SendingEmail(bool val){ipc->m_bSendingEmail = val;}

	time_t GetFirmwareUpdateTime(void) { return ipc->m_firmUpdatetime; }
	void SetFirmwareUpdateTime(time_t tm) { ipc->m_firmUpdatetime = tm; }
	
	bool GetFirmwareUpdateStatus(void){ return ipc->m_isNewFirmAvailable; }
	void SetFirmwareUpdateStatus(bool val){ipc->m_isNewFirmAvailable = val; }	
    
    bool GetInstrumentShutdownStatus(void){ return ipc->m_isInstrumentShuttingDown; }//ISC-162
	void SetInstrumentShutdownStatus(bool val){ipc->m_isInstrumentShuttingDown = val; }

	bool GetInstrumentShutdownMessageStatus(void){ return ipc->m_isShutDownMessageSent; }
	void SetInstrumentShutdownMessageStatus(bool val){ipc->m_isShutDownMessageSent = val; }

	
        bool isEthernetInternetWorking(void){ return ipc->m_isEthernetWorking; } //ISCP-295
        void isEthernetInternetWorking(bool val){ipc->m_isEthernetWorking = val; }	


	void initialize()
	{
		ipc->m_bIgnitionOn = false;
		//292
		ipc->m_bIgnitionShutdown = false;
		ipc->m_RPM = 0;
		ipc->m_Speed = 0;
		ipc->m_SeatbeltBuckled = true;
		ipc->m_rssi = 0;
		ipc->m_ObdLastUpdated = 0;
		ipc->m_BatteryVoltageValid = false;
		ipc->m_GPIO=0x00;
		ipc->m_bSendingEmail=false;
		ipc->m_IridiumEnabled = false;
		ipc->m_LastSendFailed = false;
		ipc->m_wifiLEDStatus = 0;
		ipc->m_ModemState = AMS_NO_COMM;
		ipc->m_pppState = APS_UNCONNECTED;
	}



	void Dump(char *strID = NULL)
	{
		if (strID != NULL)
			fprintf(stderr, "REDSTONE_IPC %s: ", strID);
		else
			fprintf(stderr, "REDSTONE_IPC: " );
			
		fprintf(stderr, "RPM %d	Speed:%d	Ignition:%s	Started:%s	SeatBelt:%s\n", RPM(), Speed(), IgnitionOn()?"On":"Off", Started()?"On":"Off", SeatbeltBuckled()?"On":"Off");
		
	}
	
	unsigned long LastMID(){return ipc->m_LastMID;}
	void LastMID(unsigned long mid)
	{
		if (mid > ipc->m_LastMID)
			ipc->m_LastMID = mid;
	}
	
	AFS_Timer * GetLastFailTimer(){return &ipc->m_LastSendFailTimer;};

	bool IridiumEnabled() { return ipc->m_IridiumEnabled; }
	void IridiumEnabled(bool val){ipc->m_IridiumEnabled = val;}
	bool LastSendFailed() { return ipc->m_LastSendFailed; }
	void LastSendFailed(bool val){ipc->m_LastSendFailed = val;}
	short wifiLEDStatus() { return ipc->m_wifiLEDStatus; }
	void wifiLEDStatus(short val){ipc->m_wifiLEDStatus = val;}

	void FailedToSend()
	{
		if (ipc->m_LastSendFailed == false)
		{
			LastSendFailed(true);
			ipc->m_LastSendFailTimer.SetTime();
		}
	}
};
