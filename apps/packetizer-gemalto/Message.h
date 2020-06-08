#pragma once

#include "ats-common.h"
#include "NMEA_DATA.h"
#include "messagetypes.h"

#include "ats-string.h"

class Message
{
public:
	Message(ats::StringMap& sm);

	void packetize(std::vector< char >& data);
	int mid() {return m_mid;}
	void mid(int mid) {m_mid = mid;}

private:
	NMEA_DATA m_nmea;
	int m_mid;
	int m_SeatbeltOn;
	ats::String m_MsgUserData;
};

