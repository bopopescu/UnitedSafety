#include <string.h>
#include "atslogger.h"
#include "InstrumentStatus.h"
#include "AFS_Timer.h"

static bool g_bSentInetNotification = false;
//---------------------------------------------------------------------------------
// take a raw packet and decode it.   
// returns 0:OK
//		or CheckBuffer error codes.

int InstrumentStatus::Decode(const unsigned char *rawData)
{
	int ret;
	ret = CheckBuffer(rawData); // check that the framing and checksum are OK

	if (ret > 0)
		return ret;

	memcpy(&m_ISP, &rawData[2], sizeof(InstrumentStatusPacket));
	m_MAC.MAC(m_ISP.m_MAC);
	
	if (m_ISP.m_NumVerbose)
	{
		m_AlarmDetail = rawData[13];
		m_NumSensors = ((m_ISP.m_NumVerbose - 1) / 4);
		memcpy(m_SRP, &rawData[14], m_ISP.m_NumVerbose - 1);
		
		// PRINCE-116 // TWA and STEL alarms need to be adjusted
		for (short i = 0; i < m_NumSensors; i++)
		{
			if (m_SRP[i].m_Status == 0x0F) m_SRP[i].m_Status = 0x0E;
			if (m_SRP[i].m_Status == 0x10) m_SRP[i].m_Status = 0x0F;
		}
		// End PRINCE-116
	}
	return 0;
}

//---------------------------------------------------------------------------------
// check that the framing and checksum are OK
// returns 	0:OK
// 			1:Framing error
//			2:Checksum error
int InstrumentStatus::CheckBuffer(const unsigned char *rawData)
{
	char len = rawData[3] + 5;
	
	// check for framing error
	if (rawData[0] != 0x24 || rawData[0] != 0x24 ||rawData[len-2] != 0x23 || rawData[len-1] != 0x23 )
		return 1;

	// check for checksum error
	char checksum = rawData[len - 3];
	int _checksum = 0;
	// calculate checksum
	for(int i = 8; i < len - 3; ++i)
		_checksum += rawData[i];
		
	if ((char)_checksum != checksum)
	{
		ats_logf(ATSLOG_ERROR, "%s, %d: InstrumentStatus - checksum failure. Checksum is %02x expected %02x", __FILE__, __LINE__, (char)_checksum, checksum);
		if (!g_bSentInetNotification) //<ISCP-164>
		{
			ats_logf(ATSLOG_DEBUG, "LENS sending InstrumentStatus - checksum failure\r");
			g_bSentInetNotification = true;
			AFS_Timer t;
			t.SetTime();
			std::string user_data = "1011," + t.GetTimestampWithOS() + ", LENS Checksum Failure";
			user_data = ats::to_hex(user_data);

			send_redstone_ud_msg("message-assembler", 0, "msg inet_error msg_priority=9 usr_msg_data=%s\r", user_data.c_str());
		}				
		return 2;
	}
	else
	{
		g_bSentInetNotification = false; // reset flag to send checksum error if occures again
	}
	return 0;
}

//---------------------------------------------------------------------------------
// return true if something important is different.
bool InstrumentStatus::HasChanged(InstrumentStatus & iStatus)
{
	if (iStatus.m_ISP.m_InstrumentState != m_ISP.m_InstrumentState)
	{
		ats_logf(ATSLOG_DEBUG, CYAN_ON "InstrumentStatus State has changed was:%d is %d" RESET_COLOR, m_ISP.m_InstrumentState, iStatus.m_ISP.m_InstrumentState);	
		return true;
	}
	else
		return false;

	if (iStatus.IsVerbose()) // check for change in Instrument state
	{
		if (iStatus.m_AlarmDetail != m_AlarmDetail) // Alarm detail has changed - send out updateinst
		{
			ats_logf(ATSLOG_DEBUG, CYAN_ON "InstrumentStatus Alarm detail has changed was:%d is %d" RESET_COLOR, m_AlarmDetail, iStatus.m_AlarmDetail);	
			return true;
		}
	}
	return false;
}
//---------------------------------------------------------------------------------
bool InstrumentStatus::HasSensorChanged(InstrumentStatus & iStatus)
{
	if (!iStatus.IsVerbose())
		return false;
	
	for (short i = 0; i < iStatus.m_NumSensors; i++)
	{
		if (iStatus.m_SRP[i].m_Status != m_SRP[i].m_Status)
			return true;
	}
	return false;
}

//---------------------------------------------------------------------------------
double InstrumentStatus::GetGasReading(int idx, int ndec) 
{
//	unsigned short gr;
//	gr =  ((unsigned short)(m_SRP[idx].m_GasReading[0]) << 8 ) + m_SRP[idx].m_GasReading[1];
	short gr;
	gr =  ((short)(m_SRP[idx].m_GasReading[0]) << 8 ) + m_SRP[idx].m_GasReading[1];
	
	double dgr = (double)(gr) /	pow( (double)10, (double)ndec );
	return dgr;
}

//---------------------------------------------------------------------------------
// Update the first portion of the status from a short message
void InstrumentStatus::Update(InstrumentStatus & iStatus)
{
	m_ISP.m_SignalStrength =  iStatus.m_ISP.m_SignalStrength;
	m_ISP.m_InstrumentState =	iStatus.m_ISP.m_InstrumentState;
	m_ISP.m_Exceptions =  		iStatus.m_ISP.m_Exceptions;
}
//---------------------------------------------------------------------------------
// To check if any sensor in cal/bump/zero/data fail state <ISCP-260>
bool InstrumentStatus::IsInstrumentSensorInAlarm(InstrumentStatus & iStatus)
{
	for (short i = 0; i < iStatus.m_NumSensors; i++)
	{
		if (iStatus.m_SRP[i].m_Status == 0x05 ||
			iStatus.m_SRP[i].m_Status == 0x06 ||
			iStatus.m_SRP[i].m_Status == 0x09 ||
			iStatus.m_SRP[i].m_Status == 0x0D
			)
		{
			return true;		
		}
	}
	return false;
}



