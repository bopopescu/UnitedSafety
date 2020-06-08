#include "Message.h"

Message::Message(ats::StringMap& sm)
{
	m_nmea.SetTimeFromDateTimeString(sm.get("event_time").c_str());
	m_nmea.ddLat = sm.get_double("latitude");
	m_nmea.ddLon = sm.get_double("longitude");
	m_nmea.H = sm.get_double("altitude");
	m_nmea.ddCOG = sm.get_double("heading");
	m_nmea.sog = (sm.get_double("speed")/ MS_TO_KPH) * MS_TO_KNOTS ;
	m_nmea.hdop = sm.get_double("hdop");
	m_nmea.num_svs = sm.get_double("satellites");
	m_nmea.gps_quality = sm.get_double("fix_status");
	m_SeatbeltOn = sm.get_int("inputs");
	m_MsgUserData = sm.get("usr_msg_data");
}


void Message::packetize(std::vector<char> &data)
{
	data.clear();
	char buf[256];
	return;
}
