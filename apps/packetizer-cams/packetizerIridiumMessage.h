#pragma once

#include "ats-common.h"
#include "DeviceBase.h"
#include "FTData_Debug.h"
#include "FTData_Generic.h"
#include "FTData_J1939.h"
#include "FTData_Debug_ESN.h"
#include "FTData_Generic_ESN.h"
#include "FTData_J1939_ESN.h"
#include "FTData_UserData_ESN.h"
#include "NMEA_DATA.h"
#include "messagetypes.h"

class PacketizerIridiumMessage : public DeviceBase
{
public:
	PacketizerIridiumMessage(ats::StringMap& sm);
	void packetize(std::vector< char >& data);
	int mid() {return m_mid;}
	void mid(int mid) {m_mid = mid;}
private:
	TRAK_MESSAGE_TYPE m_MsgCode;
	NMEA_DATA m_nmea;
	ats::String m_esn;
	int m_esn_type; //based on moblie id type in CalAMPs message
	ats::String m_cell_imei;
	int m_mid;
	int m_SeatbeltOn;
	ats::String m_MsgUserData;

// ATS FIXME: Use the "split" function in ats-common/ats-string.h
  void split( vector<string> & theStringVector,  /* Altered/returned value */
	   const  string  & theString,
	   const  string  & theDelimiter);

};

