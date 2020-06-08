// ---------------------------------------------------------------------------------------------------
// * This is the basic device base class - the other devices are derived from this base class
//	 It provides the device name, file names, raw logging capabilities, Setup functions,
//	 and the virtual methods that are used to read and manipulate data in the system.
//
//	There are several derived classes as described below:
//
//	SerDevice - opens a port and reads data from the port
//		 -- RTU_485Base is then derived from SerDevice for RTU specific data handling.
//		 -- RTUDevice is a container for multiple RTU_485Base derived devices so that
//				a single RTU can be monitored and the data be sent to multiple drivers, e.g.
//				a bucket and a hoist, or a Oil pressure monitor, and a hook release, etc.
//
//	AFFDevice - opens a port and reads/writes to an AFF device - has procedures for
//							encoding and decoding the data.
//
//	ProcDevice - doesn't open a port but does some task at scheduled intervals, e.g.
//							 Position will output the position to the screen, Track will send the
//							 current position as a track point at a specified interval etc.

#pragma once

#include "FASTTrackData.h"
#include "BufferedFile.h"	// used to log all data coming in a port
#include "NMEA_Client.h"

class AFF_IPC;

/*!
 * \enum Health statuses from SelfTest
 */
enum FTDATA_DRIVER_HEALTH
{
	DRVR_DIS,			 //!< no driver attached (disconnected)
	DRVR_INIT,			//!< busy or no response
	DRVR_UNKNOWN,	 //!< no self tests (or not run yet)
	DRVR_FAILED,		//!< internal tests failed
	DRVR_NO_DATA,	 //!< passed but no incoming data to check
	DRVR_RXING,		 //!< receiving data -unknown format
	DRVR_DATA_OK,	 //!< working and proud
	DRVR_NUM_STATUS
};

class DeviceBase
{
public:
	char m_ParmFileName[64];	//!< Parameter file stuff - Names defined in CTOR
	char m_CSVFileName[64];	 //!< Log file stuff - names defined in CTOR
	char m_RawFileName[64];	 //!< Log file for all incoming data (serial) or voltage changes (RTUs)
	char m_KMLFileName[64];

	char m_DevName[16];	//!< display name of the device

	FASTTrackData *m_pMyEvent;		//!< pointer to the myEvent in each derived class

	FTDATA_DRIVER_HEALTH selftest;	//status of driver

	bool m_bIsRawLogging;
	bool mybIsTesting;				//!< outputs testing data to the screen.

private:
	AFF_IPC* m_aff;
	NMEA_Client* m_nmea;
	BufferedFile*	m_RawFile;

	DeviceBase(const DeviceBase&);
	DeviceBase& operator=(const DeviceBase&);

public:
	DeviceBase();
	virtual ~DeviceBase();

	NMEA_Client& GetNMEA() const
	{
		return *m_nmea;
	}

	BufferedFile& GetRawFile() const
	{
		return *m_RawFile;
	}


	AFF_IPC& GetAFF() const
	{
		return *m_aff;
	}

	const char* GetName() const;	//!< all types set a DevNam in construction

	virtual void UpdateFileNames();
	void SetMyEvent(FASTTrackData *myEvent){m_pMyEvent = myEvent;};


	virtual void SelfTest();
	virtual FTDATA_DRIVER_HEALTH getStatus(){SelfTest(); return selftest;} //get the device condition
	void DisplayStatus();	//send the results of a self test to the RDC

	// overridable functions for derived RTUs

	virtual bool ProcessIncomingData();				//!< process the RTU input, return 1 if there was an event
	virtual void ProcessEveryTime(){}; //!< called when there is no data on port but we need to do things
	virtual void ProcessEverySecond(){}; //!< called every second for data processing

	virtual void	Setup();							//!< interactive setup of device if required.
	void Setup_DoSelfTest();						//!< asks to do the self test and runs SelfTest
	void Setup_ResetLogFile();					//!< asks if you want to reset the log file and resets if you do.
	void Setup_SetTesting();						//!< output Event data to the VFD and sets mybIsTesting

	void GetStatusString(char *buf);

	virtual bool VerifyParms(); //!< return false if invalid parms - therefore call WriteParms
	bool IsLoggingRawData() const;

protected:
	virtual void WriteKMLFile();
	virtual void WriteLogFile();
	void InsertLogHeader(const char *strHeader);
	virtual void InsertKMLHeader();

	virtual bool ReadParms(int fSize, void *pData);	//!< read fSize bytes from pData to parmfile - call from VerfyParms
	void WriteParms(int fSize, void *pData);				 //!< write fSize bytes from pData to parmfile

	virtual void SendEvent();
	virtual short GetEventBuf(char *p_buf,short p_maxLen);

};
