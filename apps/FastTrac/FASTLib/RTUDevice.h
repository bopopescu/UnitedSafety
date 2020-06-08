#pragma once

//-----------------------------------------------------------------------------
//
// RTUDevice - July 2011 - changes for multiple RTU_485Base derived devices per
//												 RTU.	So now a device derived here can have up to 4
//												 485Base class devices feeding of of one RTU.	This
//												 class sorts out which device class gets called for each pin.
//
// July 2011 - further changes: Added the m_bValidDataRcvd and m_bValidDataMissed to
//						 allow for the notification via AFF if valid data from the RTU is interrupted.
//						 m_bValidDataRcvd is set true once we have had one set of valid data.
//						 m_bValidDataMissed is set true if we miss data and a timer is set.
//						 If the timer goes past 5 seconds without resetting a message is sent.
//						 The message should only be sent once per 5 second failure.	If the data comes back
//						 and goes away again it should be sent again.	There is a limit of 5 interruptions
//						 per startup so we don't flood the system with messages if there is an issue.
//

#include "SERDevice.h"
#include "RTU_485Base.h"
#include "AFS_Timer.h"
#include "BufferedFile.h"

//							Pin		A	 B	 C	E	F	 G	H	 J	 K	 L	 M	 N	 P	 S	 T	 U
//int arrChanIdx[] = {31, 49, 46, 1, 4, 10, 7, 22, 16, 43, 34, 37, 52, 13, 19, 40}; // where each chan starts in the 'U' string



//---------------------------------------------------------------------
//
class RTUDevice : public SerDevice
{
public:
	RTU_485Base *m_RTUDevices[4];

	AFS_Timer m_Timer;
	bool m_bStartup;	// ignore first second of data at startup to eliminate noise
	AvgChan m_Chans[NUM_PINS];	 // running average of channel readings for Analog RTU.
	unsigned long m_nCharsRcvd; // watch for no data on the RTU port.
	short m_nDevices;

	// ----	data interruption detection stuff
	bool m_bValidDataRcvd;	// indicates that a full decodable line was received at some point.
	unsigned short m_TimeSinceValidData;	// set to true if data is missed after a valid line is received.
	bool m_bInterruptSent;	// set to true if data has been missing for 5 seconds and an error was sent for this interruption
	short m_nInterrupts;	// number of interruptions detected - don't send more than 5 per startup.

protected:
	void	 (RTUDevice::*myHandlers[18])(short, double);


public:
	unsigned long m_nBytesSent, m_nBytesRcvd;

	RTUDevice();
	~RTUDevice();

	void SetPort(short RTUPort){SerDevice::SetPort(RTUPort, 5); } // setup and open the port at 115200
	bool ProcessIncomingData();

	void Setup();	// setup the parameters

	int ProcessIncomingString();
	void	ProcessEverySecond(); // called when there is no data on port but we need to do things
	void ProcessEveryTime();

	bool EventUp(const double lastV, const double curV, const double cutoffV) // going up through cutoff
			 {	return (lastV > -10.0 && lastV <= cutoffV && curV > cutoffV); }	 // but not when first reading
	bool EventDown(const double lastV, const double curV, const double cutoffV) // going up through cutoff
			 {	return (lastV > -10.0 && lastV >= cutoffV && curV < cutoffV); }

	double GetChanVoltage(int chan); // return the voltage on the given channel
	void GetByteCount(char *buf){sprintf(buf, "RTU%d S:%ld R:%ld ", m_RTUPort + 1, m_nBytesSent, m_nBytesRcvd);};

	void SelfTest();
	FTDATA_DRIVER_HEALTH getStatus(){ SelfTest(); return selftest; } //get the device condition

	unsigned long ConvertHex(char *buf);	//!< used to convert the hex data to decimal numbers.

	void UpdateFileNames();

	void LogRawData(){};	// Doesn't do anything because the individual internal devices will handle logging
												// If you don't define it all incoming RTU data will be logged and the disk will fill up!

private:

	bool VerifyParms();	//return true if the parms are valid.
	void DumpParms();


};

