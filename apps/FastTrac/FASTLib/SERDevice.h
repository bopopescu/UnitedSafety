//	See DeviceBase.h for the various class descriptions.
/*!
 *
 *
 * \brief base class for input devices.
 *
 *
 *	This is the SERDevice base class
 *
 * \author David Huff
 * \date	 24/08/2010
 *
 * Copyright &copy; Absolute Fire Systems (2010)
 */

#pragma once

#include "CircBuf.h"
#include "WriteDebug.h"
#include "termios.h"
#include "DeviceBase.h"


enum RTU_STARTUP_TEST
{
	NO_STARTUP_TEST,
	NO_DATA_ON_PORT,
	GOOD_DATA_ON_PORT
};


/*!
 * \class RTUDevice - base class for all RTU devices
 */
class SerDevice : public DeviceBase
{
public:
	FTDATA_DRIVER_HEALTH selftest;	//status of driver


public:	// common to all subclasses
	static bool mybIsDebugging;

public:	// defined by each sub class.
	short	m_RTUPort;			//!< which port is the device on
	bool	 m_bIsNMEA;			//!< indicates if the RTU is a source of NMEA data
	int		m_fd;					 //!< port file descriptor for read/write to the port

	CircBuf <char, 2560> m_Data;	//!< incoming data from the RTU port lands here.

	char m_InBuf[4096];	// the incoming data
	short m_LenInBuf;		// the length of the incoming data

public:
	SerDevice();

	virtual ~SerDevice();
	virtual void SetPort(short RTUPort, short baudIdx);	// setup and open the port
	virtual void SetPort(const char * RTUPort, short baudIdx);	// setup and open the port

	// accessor functions for low level values
	bool IsNMEA();					//!< returns the m_bIsNMEA indicating if this device provide NMEA
	int GetFD();

	virtual bool ProcessIncomingData();	// will usually be overridden - this does the reading from the port and the raw logging
	virtual void DumpParms();	//!< dumps out the parms to debug.txt and screen.

	virtual void GetByteCount(char *buf){buf = buf;};

	virtual void LogRawData();	// logs the raw
	short GetBaudBit(short idx);

protected:	// Reading and writing only occur internally to the object
	void SetPortBaud(speed_t speed);
};




