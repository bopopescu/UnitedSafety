/*!
 * \file AFFDevice.h
 *
 * \brief AFF Device base class
 *
 *	common to all devices is the DevName (max 16 chars)
 *	and an ability to verify initial connections and decode the data.
 *	GPS data is added by AddGPS.
 *
 *	The AFF device is common for all subclasses and is used by each AFF
 *	to send event data and to read the raw NMEA stream if necessary.
 *	The AFF device contains the latest GPS position data and time as well.
 *
 *	Some AFFs will require a 2 plug system to work.	The Latitude system for
 *	example will require one plug for the AFF messages and another acting as
 *	an NMEA input on an RTU port.
 *
 *	RTUs can be a source of NMEA data - only the first NMEA source on the
 *	ports will be used!	The main loop will access the NMEA source RTU first
 *	and buffer the raw data and process the GPS data so that all of the other
 *	RTUs can access it.	If an external
 *	NMEA source is found the AFF data will
 *	not be used for GPS.
 *
 * \author David Huff
 * \date	 24/08/2010
 *
 * Copyright &copy; Absolute Fire Systems (2010)
 */


#pragma once

#include "BufferedFile.h"
#include "tinyxml.h"

#define MAX_AFF_BUF_SIZE 512
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "CharBuf.h"
#include "CircBuf.h"
#include "WriteDebug.h"
#include "AFFMessage.h"
#include "NMEA_Client.h"
#include "DeviceBase.h"

enum
{
	AFF_STATUS_CONFIRM1,
	AFF_STATUS_CONFIRM2,
	AFF_STATUS_CONFIRM3,
	AFF_STATUS_CONFIRM_RESET, // used under SkyConnect
	AFF_STATUS_DONE
};



/*!
 * \class AFFDevice
 */
class AFFDevice
{
public:
	NMEA_Client nmea;

protected:	// can be accessed by derived classes
	char	 m_DevName[16];	//!< display name of the device
	char m_ParmFileName[64];	//!< Parameter file stuff - Names defined in CTOR
	char m_CSVFileName[64];	 //!< Log file stuff - names defined in CTOR
	char m_RawFileName[64];	 //!< Log file for all incoming data (serial) or voltage changes (RTUs)
	static char m_KMLFileName[64];
	BufferedFile	m_RawFile;
	bool m_bIsRawLogging;

	int	 m_fdAFF;					//!< file descriptor of AFF port
	bool m_isSending;			 //!< is the AFF currently sending data? (needs to be serviced even if no chars)
	bool m_bHasNMEA;				//!< indicates that this AFF type outputs NMEA to the system (most don't).
	unsigned char m_MsgID;	//!< incremental message ID

	CharBuf m_cmdBuf;			 //!< what is left after the GPS is removed from inputBuf

	// UUEncode stuff
	short myLenUUBuf;	// non zero means buffer was allocated
	char *mypUUBuf;

	CircBuf<AFFMessage, 5L> m_OutgoingMessages; // circular buffer of messages
	AFFMessage curMsg;	// the message that is currently trying to be sent.

public:
	AFFDevice();
	virtual ~AFFDevice();

	virtual bool OpenPort( ){return false;};
	virtual void Setup(){};

	char *GetName() {return m_DevName;};	// return the displayable name

	virtual bool SendData(const char *data, short lenData);	// Send bytes out as a message
	virtual bool ProcessIncomingData(){return 0;}	// process the AFF GPS input. NMEAflag of -1 - use NMEA
	virtual bool ProcessSend(){return false;}	// process the AFF GPS input
	virtual int GetFD(){return m_fdAFF; }
	virtual void ProcessEveryTime(){};
	virtual void ProcessEverySecond(){};

	bool IsSending(){return m_isSending;};
	bool HasNMEA(){return m_bHasNMEA;};
//	virtual void GetByteCount(char *buf){};

	virtual void UpdateFileNames(){};
	bool ReadParms();
	void WriteParms(const char *commentField);
	void DisplayStatus();

protected:	// Reading and writing only occur internally to the object
	virtual void xmlEncode(TiXmlElement *root){root=root;}; //content specific part for each driver
	virtual void xmlDecode(TiXmlHandle *hRoot){hRoot=hRoot;}; //content specific parts for each driver
	bool ReadParms(int fSize, void *pData);	//!< read fSize bytes from pData to parmfile - call from VerifyParms
	void WriteParms(int fSize, void *pData); //!< write fSize bytes from pData to parmfile
	virtual bool VerifyParms(); // return false if invalid parms - therefore call WriteParms
	int ResetLogFile();
	int Input_LogRate();

	virtual void SetPortBaud(speed_t speed);
	void GetStatusString(char *condition);

public:
	// UU encoding stuff - just call UUEncode(), then GetUUString() into a big enough buffer.
	//										 Deleting the allocated buffer will happen automatically.
	short UUEncode(unsigned char *buf, const short len);	 // return len of encodeBuf
	const char * GetUUString();

private:
	void	UUFree();	// free the allocated space
	short UUAlloc(const short len);	// allocate space for the UUEncoded buffer
	short encode_base64(unsigned char *src, short len);
	short decode_base64(const char *src, const short len, unsigned char *dest);

	virtual void BuildMessageFrame(const char * buf, short len, AFFMessage &msg){buf=buf;len = len;msg=msg;}; // code is there to get rid of compiler warnings
};

