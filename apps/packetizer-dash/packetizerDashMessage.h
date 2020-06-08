#pragma once

#include "ats-common.h"
#include "DeviceBase.h"
#include "FTData_Debug.h"
#include "FTData_Generic.h"
#include "FTData_J1939.h"
#include "NMEA_DATA.h"
#include "messagetypes.h"

#include "ats-string.h"

class PacketizerDashMessage : public DeviceBase
{
public:
	PacketizerDashMessage(ats::StringMap& sm);
	void packetize(std::vector< char >& data);
	int mid() {return m_mid;}
	void mid(int mid) {m_mid = mid;}
private:
	TRAK_MESSAGE_TYPE m_MsgCode;
	NMEA_DATA m_nmea;
	int m_mid;
	int m_SeatbeltOn;
	ats::String m_MsgUserData;
};

