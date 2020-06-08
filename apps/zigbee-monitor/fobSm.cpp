#include <math.h>

#include "J2K.h"
#include "db-monitor.h"
#include "ConfigDB.h"
#include "zigbee-monitor.h"

extern ClientSocket g_cs_powermonitor;  // used to notify power-monitor that a fob is still operating.
extern int g_KeepAliveMinutes;  // length of time to keep system alive after a zigbee communication.
extern int g_AllowTimerExtensions;
extern int g_hazardExtensionMinutes;
extern int g_shiftTimerExtensionMinutes;
extern int g_overdueAllow;
extern int g_checkinpri;
extern int g_checkoutpri;
extern int g_sospri;
extern int g_watStaterequestpri;

extern MyData* g_md;
ats::String convertHextoDec(const ats::String& s);

//J2000 is the date of January 1, 2000; 12:00:00 GMT
//struct tm J2000 = {0, 0, 12, 1, 0, 100, 6, 0, 0};
ats::String generateExtensionTime(time_t timer, int extensionMins)
{
	J2K jNow;
	jNow.SetSystemTime();
  //Since J2K construtor accept time since or before J2000 (seconds) as a parameter
  //timer represent number of seconds since Jan 1, 1970 UTC.
  J2K jTime(timer - MINUNIXTIMESTAMP );
  jTime += extensionMins*60;
	
	if (jTime < jNow)  // check that the timer is in the future - otherwise new timer is now + extension
	{
		jTime.SetSystemTime();
		jTime += extensionMins*60;
	}

  ats::String str;
  ats_sprintf(&str, "%.2d%.2d%.2d%.2d%.2d%.2d", jTime.GetYear()-2000, jTime.GetMonth(),
	  jTime.GetDay(), jTime.GetHour(), jTime.GetMinute(), jTime.GetSecond());
  return str;
}

// AWARE360 FIXME: Document the input and return values for this function.
static double DecodeDDMM(const char *ddmmBuf)
{
	double deg;
	const double minutes = modf(atof(ddmmBuf)/100.0, &deg) * 100.0;
	const double dval = (deg + (minutes / 60.0));
	return (dval > 180.0) ? (dval - 360.0) : dval;
}

void fobContent::getSystemTime()
{
	const time_t now = ::time(NULL);
	struct tm* ts = localtime(&now);
	m_gps.tm_year = ts->tm_year + 1900;
	m_gps.tm_mon = ts->tm_mon + 1;
	m_gps.tm_mday = ts->tm_mday;
	m_gps.tm_hour = ts->tm_hour;
	m_gps.tm_min = ts->tm_min;
	m_gps.tm_sec = ts->tm_sec;
}

void fobContent::updateTimeStamp()
{
	ats_sprintf(&m_gps.timeStampStr, "\"%.4d-%.2d-%.2d %.2d:%.2d:%.2d\"", m_gps.tm_year, m_gps.tm_mon, m_gps.tm_mday, m_gps.tm_hour, m_gps.tm_min, m_gps.tm_sec);
}

short fobContent::DecodeGGA (const ats::StringList& strList)
{
	char*  dec_place;

	if (strList.size() < 6)
	{
		return -1; //bad nmea
	}

	getSystemTime();

	if (!strList[0].empty() && strList[0].size() > 6)
	{
		const ats::String& str = strList[0].c_str();
		m_gps.tm_hour = atoi(str.substr(0, 2).c_str());
		m_gps.tm_min = atoi(str.substr(2, 2).c_str());
		m_gps.tm_sec = atoi(str.substr(4, 2).c_str());
	}

	updateTimeStamp();

	if (!strList[5].empty())
	{
		m_gps.gps_quality = (short)atoi(strList[5].c_str());

		if (m_gps.gps_quality == 0) // not valid position
		{
			return -1;
		}
	}

	if (!strList[1].empty() && !strList[2].empty() && !strList[3].empty() && !strList[4].empty())
	{
		if ((dec_place = strchr(strList[1].c_str(), '.')) == NULL || (short)(dec_place - strList[1].c_str()) != 4)
		{
			return -1;
		}

		m_gps.ddLat = DecodeDDMM(strList[1].c_str());

		if (*(strList[2].c_str()) == 'S' || *(strList[2].c_str()) == 's')
			m_gps.ddLat  = -m_gps.ddLat;
		else if (*(strList[2].c_str()) != 'N' && *(strList[2].c_str()) != 'n')
		{
			return -1;
		}

		if (fabs(m_gps.ddLat) > 180.0)
		{
			return -1;
		}

		if ((dec_place = strchr(strList[3].c_str(), '.')) == NULL || (short)(dec_place - strList[3].c_str()) != 5)
		{
			return -1;
		}

		m_gps.ddLon = DecodeDDMM(strList[3].c_str());

		if (*(strList[4].c_str()) == 'W' || *(strList[4].c_str()) == 'w')
			m_gps.ddLon  = -m_gps.ddLon;
		else if (*(strList[4].c_str()) != 'E' && *(strList[4].c_str()) != 'e')
		{
			return -1;
		}

		if (fabs(m_gps.ddLon) > 360.0)
		{
			return -1;
		}

		if (m_gps.ddLon > 180.0)  // make it -180 to 180
			m_gps.ddLon -= 360.0;
	}
	else
		return 0;

	if (!strList[6].empty() && !strList[7].empty())
	{
		m_gps.num_svs = (short)atoi(strList[6].c_str());
		m_gps.hdop    = atof(strList[7].c_str());
	}
	else
		return 0;

	m_gps.valid = true;
	return 1;
}

// AWARE360 FIXME: UtilityLib or FASTLib do not contain NMEA Checksum code?
//	Why is the Makefile for this application still referring to UtilityLib and FASTLib?
static ats::String NMEAChecksum(const ats::String& str)
{

	if(str.size() < 2)
	{
		return ats::g_empty;
	}

	ats::String s;

	if(str[0] == '$')
	{
		s = str.substr(1);
	}
	else
	{
		s = str;
	}

	char checksum = 0;

	for(size_t i = 0; i < s.size(); ++i)
	{
		checksum ^= s[i];
	}

	ats::String buf;
	ats_sprintf(&buf, "*%.2X", checksum);
	return buf;
}

void fobContent::messageHandle(const ats::String& msg)
{
	if(msg[0] == 'E')
	{
		if(handleButtonEvent(msg))
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: Error: handleButtonEvent failed", __FILE__, __LINE__);
		}
	}
	else if(msg[0] == 'S')
	{
		if(handleStatusReply(msg))
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: Error: handleStatusReply failed", __FILE__, __LINE__);
		}
	}
	else if(msg[0] == 'C')
	{
		if(handleWriteConfigAck(msg))
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: Error: handleWriteConfigAck failed", __FILE__, __LINE__);
		}
	}
	else if(msg[0] == 'R')
	{
		if(handleReadConfigAck(msg))
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: Error: handleReadConfigAck failed", __FILE__, __LINE__);
		}
	}
	else if(msg[0] == 'I')
	{
		if(handleUniqueIDReply(msg))
		{
			ats_logf(ATSLOG_ERROR, "%s,%d: Error: handleUniqueIDReply failed", __FILE__, __LINE__);
		}
	}
	else
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: Error: unknown message event %s", __FILE__, __LINE__, msg.c_str() );
	}
}

int fobContent::handleUniqueIDReply(const ats::String& msg)
{
	const ats::String& pid = getEUI();
	if(msg.size() < 32)
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: (handleUniqueIDReply) Failed to parse message from FOB %s, msg: %s", __FILE__, __LINE__, pid.c_str(), msg.c_str());
		return 1;
	}
	uint8_t commID = atoi(msg.substr(1,2).c_str());
	uint8_t m_type = FROMFOB_unique_id_reply;

	//send ack to server who request FOB status.
	m_md->m_fobrequest_manager->answer(pid, m_type, commID, msg);
	return 0;
}

int fobContent::handleWriteConfigAck(const ats::String& msg)
{
	const ats::String& pid = getEUI();
	uint8_t commID = atoi(msg.substr(1,2).c_str());
	uint8_t	m_type = FROMFOB_write_config_ack;

	//send ack to server who request FOB status.
	m_md->m_fobrequest_manager->answer(pid, m_type, commID, msg);
	return 0;
}

int fobContent::handleReadConfigAck(const ats::String& msg)
{
	const ats::String& pid = getEUI();
	const uint8_t commID = atoi(msg.substr(1,2).c_str());
	const uint8_t m_type = FROMFOB_read_config_ack;

	if( g_overdueAllow )
	{
		ats_logf(ATSLOG_DEBUG, "%s,%d: read config  %s", __FILE__, __LINE__, msg.c_str());
		m_readconfigManager.signal_client(msg);
	}

	//send ack to server who request FOB status.
	m_md->m_fobrequest_manager->answer(pid, m_type, commID, msg);
	return 0;
}

int fobContent::handleStatusReply(const ats::String& msg)
{
	const ats::String& pid = getEUI();
	if(msg.size() < 8)
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: (handleStatusReply) - Failed to parse message from FOB %s, msg: %s", __FILE__, __LINE__, pid.c_str(), msg.c_str());
		return 1;
	}

	const uint8_t commID = atoi(msg.substr(1,2).c_str());
	const uint8_t m_type = FROMFOB_status_reply;

	sendCmdreply();

	//send ack to server who request FOB status.
	m_md->m_fobrequest_manager->answer(pid, m_type, commID, msg);
	return 0;
}

int fobContent::handleButtonEvent(const ats::String& msg)
{
	const ats::String& pid = getEUI();

	int a = 0;
	fobEvent b = noEvent;
	fobButton c = noButton;
	fobHold d = noHold;

	const uint8_t event_flag = std::strtol(msg.substr(6,2).c_str(), 0, 16);
	const uint8_t commID = atoi(msg.substr(1,2).c_str());
	ats_logf(ATSLOG_DEBUG, "%s,%d:  handleButtonEvent: Incoming %s    event_flag=%d", __FILE__, __LINE__, msg.c_str(), event_flag);

	if(msg.size() < 8)
		ats_logf(ATSLOG_ERROR, "%s,%d: %s button msg %s: format wrong", __FILE__, __LINE__, pid.c_str(), msg.c_str());

	m_gps.valid = false;
	
	if(msg.size() > 8 && msg[8] == ',')
	{
		ats::StringList spec;
		ats::split(spec, msg.substr(9), ",");

		if(DecodeGGA(spec) < 0)
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: %s button msg %s: Bad GPS signal", __FILE__, __LINE__, pid.c_str(), msg.c_str());
		}
#if 0
		else
		{
			ats_logf(ATSLOG_ERROR, "GPS, year %d, month %d, day %d, hour %d, min %d, sec %d, ddLat %f, ddLon %f, gps_quality %d, num_sys %d, hdop %f timestamp %s", m_gps.tm_year, m_gps.tm_mon, m_gps.tm_mday, m_gps.tm_hour, m_gps.tm_min, m_gps.tm_sec, m_gps.ddLat, m_gps.ddLon, m_gps.gps_quality, m_gps.num_svs, m_gps.hdop, m_gps.timeStampStr.c_str()); 
		}
#endif
	}

	ats::String gpsStr;

	if(m_gps.valid)
		ats_sprintf(&gpsStr, "fix_time=%s latitude=%f longitude=%f satellites=%d fix_status=%d hdop=%f", m_gps.timeStampStr.c_str(), m_gps.ddLat, m_gps.ddLon, m_gps.num_svs, m_gps.gps_quality, m_gps.hdop);
	else
		getLocalGPSPosition(gpsStr);

	if((a = event_flag & 0xF0))
	{
		if(a == 0x10)
		{
			ats_logf(ATSLOG_DEBUG, CYAN_ON "key %s SOS Event" RESET_COLOR, pid.c_str());
			b = sosEvent;
		}
		else if (a == 0x20)
		{
			ats_logf(ATSLOG_DEBUG, CYAN_ON "key %s RIGHT Event" RESET_COLOR, pid.c_str());
			b = rightEvent;
		}
		else if( a == 0x30)
		{
			ats_logf(ATSLOG_DEBUG, CYAN_ON "key %s POWER Event" RESET_COLOR, pid.c_str());
			b = powerEvent;
		}
		else if( a == 0x40)
		{
			ats_logf(ATSLOG_ERROR, CYAN_ON "key %s MANDOWN Event" RESET_COLOR, pid.c_str());
			b = mandownEvent;
		}
		else if( a == 0x80)
		{
			ats_logf(ATSLOG_ERROR, CYAN_ON "key %s NoMotion Event" RESET_COLOR,  pid.c_str());
			b = noMotionEvent;
		}

	}

	if((a = event_flag & 0x0C))
	{
		if(a == 0x04)
		{
			ats_logf(ATSLOG_ERROR, CYAN_ON "key %s SOS Button" RESET_COLOR, pid.c_str());
			c = sosButton;
		}
		else if (a == 0x08)
		{
			ats_logf(ATSLOG_DEBUG, CYAN_ON "key %s RIGHT Button" RESET_COLOR, pid.c_str());
			c = rightButton;
		}
		else if (a == 0x0C)
		{
			ats_logf(ATSLOG_DEBUG, CYAN_ON "key %s POWER Button" RESET_COLOR, pid.c_str());
			c = powerButton;
		}

	}

	if((a = (event_flag & 0x03)))
	{
		if(a == 0x01)
		{
			ats_logf(ATSLOG_DEBUG, CYAN_ON "key %s Short Hold" RESET_COLOR, pid.c_str());
			d = shortHold;
		}
		else if (a == 0x02)
		{
			ats_logf(ATSLOG_DEBUG, CYAN_ON "key %s Medium Hold" RESET_COLOR, pid.c_str());
			d = mediumHold;
		}
		else if (a == 0x03)
		{
			ats_logf(ATSLOG_DEBUG, CYAN_ON "key %s Long Hold" RESET_COLOR, pid.c_str());
			d = longHold;
		}

	}

	int result = 0x32;
	if( b == sosEvent && c == sosButton && d == mediumHold)  // this is an SOS button press
	{
		const ats::String id = convertHextoDec(pid);
		ats_logf(ATSLOG_ERROR, "%s,%d: " YELLOW_ON "SOS from %s" RESET_COLOR, __FILE__, __LINE__, pid.c_str());
		const ats::String& c1 = "$PGEMEM,SOS BUTTON INITIATED EMERGENCY,,SLP";
		const ats::String& cmd = c1 + NMEAChecksum(c1);
		send_redstone_ud_msg("message-assembler", 0, "msg scheduled_message msg_priority=%d mobile_id=%s mobile_id_type=4 inputs=223 %s\r",g_sospri, id.c_str(), gpsStr.c_str());//Position Update
		send_redstone_ud_msg("message-assembler", 0, "msg calamp_user_msg msg_priority=%d mobile_id=%s mobile_id_type=4 usr_msg_data=%s inputs=223 %s\r",g_sospri, id.c_str() ,ats::to_hex(cmd).c_str(), gpsStr.c_str());//SOS message, with inputs bit 4 set to 0 for emergency SOS.
		setRequest(FOB_REQUEST_SOS);
		m_md->fob_status_request(m_mac, 0x82, 0x12);
	}
	else if (c == rightButton && d == shortHold)  // this is a checkin press
	{
		const ats::String id = convertHextoDec(pid);
		ats_logf(ATSLOG_DEBUG, YELLOW_ON "CHECK IN from %s" RESET_COLOR, pid.c_str());
		ats::String c0 = "$PGEMCI,";
		ats::String c1 = ",,,,SLP";
		ats::String c2="";

		if( g_overdueAllow && getState() != FOB_STATE_UNKNOWN )
		{
			if( g_AllowTimerExtensions)
			{
				m_md->fob_status_request(m_mac, FDS_LOWER_GREEN_ON, FDS_ALL_OFF);  // we turn off buzzing and light immediately
				time_t now = time(NULL);
				time_t offt = offMonitorTime();
				time_t hazardt = hazardTime();

				int diffOffTimer = offt - now;
				int diffhazardTimer = hazardt - now;

				int remind = getTimeRemind();
				bool atleastOnce = false;
				if(getState() ==  FOB_STATE_OVERDUE_SHIFT_TIMER || getState()== FOB_STATE_OVERDUE_SAFETY_AND_SHIFT || ( offt > MINUNIXTIMESTAMP && (diffOffTimer < g_timerExpireMinutes*60)))
				{
					ats_logf(ATSLOG_DEBUG, "set offtime extension time state %d, timeremind %d", getState(), remind);
					c2=generateExtensionTime(offMonitorTime(), g_shiftTimerExtensionMinutes);
					const ats::String& c = c0 + c2 + c1;
					const ats::String& cmd = c + NMEAChecksum(c);
					sendCheckinMessage(cmd, id, gpsStr);
					atleastOnce = true;
				}

				if(getState() == FOB_STATE_OVERDUE_HAZARD || (hazardt > MINUNIXTIMESTAMP && (diffhazardTimer < g_timerExpireMinutes*60)))
				{
					ats_logf(ATSLOG_DEBUG, "set hazard extension time state %d, timeremind %d", getState(), remind);
					int t = g_hazardExtensionMinutes;
					time_t ht = hazardTime();
					time_t now = ::time(NULL);
					if( ht > now )
					{
						t += (ht-now)/60;
					}

					ats_sprintf(&c2, "%d", t);
					c0="$PGEMHA,";
					c1=",hazard extension,,SLP";
					const ats::String& c = c0 + c2 + c1;
					const ats::String& cmd = c + NMEAChecksum(c);
					sendCheckinMessage(cmd, id, gpsStr);
					atleastOnce = true;
				}

				if( remind&FOB_TIMERREMIND_OFFTIME)
				{
					fobTimeRemindStatus(FOB_TIMERREMIND_OFFTIME, FOB_TIMERREMIND_STATUS_WAITING);
					setTimeRemind(FOB_TIMERREMIND_OFFTIME,false);
				}
				if( remind&FOB_TIMERREMIND_SAFETY)
				{
					fobTimeRemindStatus(FOB_TIMERREMIND_SAFETY, FOB_TIMERREMIND_STATUS_WAITING);
					setTimeRemind(FOB_TIMERREMIND_SAFETY,false);
				}
				if( remind&FOB_TIMERREMIND_HAZARD)
				{
					fobTimeRemindStatus(FOB_TIMERREMIND_HAZARD, FOB_TIMERREMIND_STATUS_WAITING);
					setTimeRemind(FOB_TIMERREMIND_HAZARD,false);
				}

				if( atleastOnce)
					return 0;
			}
			else  // no timer extensions allowed.  We want to go to flashing green but have the next status update not cause a buzzing - just orange lights.
			{
				m_md->fob_status_request(m_mac, FDS_LOWER_GREEN_ON, FDS_ALL_OFF);  // we turn off buzzing and light immediately
				switch (getState())
				{
					case FOB_STATE_OVERDUE_SHIFT_TIMER:
						setShifttimeFlashingFlag(true);
						break;
					case FOB_STATE_OVERDUE_HAZARD:
						setHazardFlashingFlag(true);
						break;
					default:
						break;
				}
				if( getTimeRemind()&FOB_TIMERREMIND_SAFETY )
			{
				fobTimeRemindStatus(FOB_TIMERREMIND_SAFETY, FOB_TIMERREMIND_STATUS_WAITING);
				setTimeRemind(FOB_TIMERREMIND_SAFETY,false);
				}
			}
		}
		const ats::String& c = c0 + c2 + c1;
		const ats::String& cmd = c + NMEAChecksum(c);
		sendCheckinMessage(cmd, id, gpsStr);
	}
	else if (b == rightEvent && c == rightButton && d == mediumHold)  // this is a checkout press
	{
		const ats::String id = convertHextoDec(pid);
		ats_logf(ATSLOG_DEBUG, YELLOW_ON "CHECK OUT from %s" RESET_COLOR, pid.c_str());
		const ats::String& c1 = "$PGEMMO,,,SLP";
		const ats::String& cmd = c1 + NMEAChecksum(c1);
		send_redstone_ud_msg("message-assembler", 0, "msg scheduled_message msg_priority=%d mobile_id=%s mobile_id_type=4 %s\r",g_checkoutpri, id.c_str(), gpsStr.c_str());
		send_redstone_ud_msg("message-assembler", 0, "msg calamp_user_msg msg_priority=%d mobile_id=%s mobile_id_type=4 usr_msg_data=%s %s\r",g_checkoutpri, id.c_str() ,ats::to_hex(cmd).c_str(), gpsStr.c_str());//Check Out message
		setRequest(FOB_REQUEST_CHECKOUT);
		m_md->fob_status_request(pid, 0x80, 0x32);
	}
	else if( b == noEvent && c == sosButton && d == longHold)
	{
		ats_logf(ATSLOG_ERROR, YELLOW_ON "SOS CANCEL from %s" RESET_COLOR, pid.c_str());
		switch (getState())
		{
			case FOB_STATE_SOS:
			case FOB_STATE_MANDOWN:
				sendWatSosCancel();
				break;
			default:
				m_md->fob_status_request(pid, FDS_LOWER_GREEN_ON, FDS_ALL_OFF);
				break;
		}
	}
	else if( b == powerEvent && c == powerButton && d == mediumHold)
	{
		ats_logf(ATSLOG_ERROR, YELLOW_ON "POWER OFF from %s" RESET_COLOR, pid.c_str());
		m_md->fob_remove(pid);
	}
	else if( b == mandownEvent)
	{
		ats_logf(ATSLOG_ERROR, YELLOW_ON "Man Down from %s" RESET_COLOR, pid.c_str());
		const ats::String id = convertHextoDec(pid);
		const ats::String& c1 = "$PGEMEM,,MANDOWN INITIATED EMERGENCY,SLP";
		const ats::String& cmd = c1 + NMEAChecksum(c1);
		send_redstone_ud_msg("message-assembler", 0, "msg scheduled_message msg_priority=%d mobile_id=%s mobile_id_type=4 inputs=223 %s\r",g_sospri, id.c_str(), gpsStr.c_str());//Position Update
		send_redstone_ud_msg("message-assembler", 0, "msg calamp_user_msg msg_priority=%d mobile_id=%s mobile_id_type=4 usr_msg_data=%s inputs=223 %s\r",g_sospri, id.c_str() ,ats::to_hex(cmd).c_str(), gpsStr.c_str());//SOS message, with inputs bit 4 set to 0 for emergency SOS.
		setRequest(FOB_REQUEST_MANDOWN);
		m_md->fob_status_request(m_mac, 0x80, 0x12);
	}
	else if( b == noMotionEvent)
	{
		ats_logf(ATSLOG_ERROR, YELLOW_ON "NO MOTION from %s" RESET_COLOR, pid.c_str());
		const ats::String id = convertHextoDec(pid);
		const ats::String& c1 = "$PGEMEM,,NO MOTION INITIATED EMERGENCY,SLP";
		const ats::String& cmd = c1 + NMEAChecksum(c1);
		int pri = g_md->m_db.GetInt("zigbee", "SOS_Priority", 1);
		send_redstone_ud_msg("message-assembler", 0, "msg scheduled_message msg_priority=%d mobile_id=%s mobile_id_type=4 inputs=223 %s\r",pri, id.c_str(), gpsStr.c_str());//Position Update
		send_redstone_ud_msg("message-assembler", 0, "msg calamp_user_msg msg_priority=%d mobile_id=%s mobile_id_type=4 usr_msg_data=%s inputs=223 %s\r",pri, id.c_str() ,ats::to_hex(cmd).c_str(), gpsStr.c_str());//SOS message, with inputs bit 4 set to 0 for emergency SOS.
		setRequest(FOB_REQUEST_MANDOWN);
	}
	else if( b == noEvent && c == noButton && d == noHold)
	{
		setLastHbTimeStamp(time(NULL));
		ats_logf(ATSLOG_DEBUG, YELLOW_ON "Heartbeat event %llu from %s" RESET_COLOR, getLastHbTimeStamp(), pid.c_str());
		send_cmd(g_cs_powermonitor.m_fd, MSG_NOSIGNAL, "set_work key=SLP:%s expire=%d\r", pid.c_str(), (g_KeepAliveMinutes * 60));  // keep the system alive
	}

	//FIXME: send back phone/vpn ack to fob only, no server ack send back yet.

	ats::String msg1;
	ats_sprintf(&msg1, "E%.2d%c", commID, result);

	ats_logf(ATSLOG_DEBUG, "got ID %d, Button Ack %s", commID, msg1.c_str());
	messageFrame m(commID, pid, msg1) ;
	m_md->post_message(m_md->get_ucast_key(), m);

	return 0;
}

void fobContent::fob_set_timer(const ats::String& pid, int value)
{
	m_md->fob_status_request(pid, 0x80, value);
	start_timer();
}

void fobContent::sendAck(const ats::String& sender, const ats::String& key, int seqNum, int ack)
{
	send_redstone_ud_msg(sender.c_str(), 0, "ack mobile_id=%s mobile_id_type=4 sequence_num=%d ack=%d\r", (convertHextoDec(key)).c_str(), seqNum, ack);
}

void fobContent::sendWatStateRequest(bool highPri)
{
	const ats::String& pid = getEUI();
	const ats::String id = convertHextoDec(pid);
	const ats::String& cmd = "$PGEMCC*1F";
	
	ats::String gpsStr;
	if(m_gps.valid)
		ats_sprintf(&gpsStr, "fix_time=%s latitude=%f longitude=%f satellites=%d fix_status=%d hdop=%f", m_gps.timeStampStr.c_str(), m_gps.ddLat, m_gps.ddLon, m_gps.num_svs, m_gps.gps_quality, m_gps.hdop);
	else
		getLocalGPSPosition(gpsStr);

	if( highPri )
	{
		send_redstone_ud_msg("message-assembler", 0, "msg scheduled_message msg_priority=%d mobile_id=%s mobile_id_type=4 %s\r", g_watStaterequestpri, id.c_str(), gpsStr.c_str());//Position Update
		send_redstone_ud_msg("message-assembler", 0, "msg calamp_user_msg msg_priority=%d mobile_id=%s mobile_id_type=4 usr_msg_data=%s %s\r", g_watStaterequestpri, id.c_str(), ats::to_hex(cmd).c_str(), gpsStr.c_str());
		watStatusRequestHourlyTime(time(NULL));
	}
	else
	{
		send_redstone_ud_msg("message-assembler", 0, "msg scheduled_message mobile_id=%s mobile_id_type=4 %s\r", id.c_str(), gpsStr.c_str());//Position Update
		send_redstone_ud_msg("message-assembler", 0, "msg calamp_user_msg mobile_id=%s mobile_id_type=4 usr_msg_data=%s %s\r", id.c_str(), ats::to_hex(cmd).c_str(), gpsStr.c_str());
		watStatusRequestTime(time(NULL));
	}
}

void fobContent::sendWatSosCancel()
{
	const ats::String& pid = getEUI();
	const ats::String id = convertHextoDec(pid);
	const ats::String& c1 = "$PGEMEC,,,SLP";
	const ats::String& cmd = c1 + NMEAChecksum(c1);

	ats::String gpsStr;
	if(m_gps.valid)
		ats_sprintf(&gpsStr, "fix_time=%s latitude=%f longitude=%f satellites=%d fix_status=%d hdop=%f", m_gps.timeStampStr.c_str(), m_gps.ddLat, m_gps.ddLon, m_gps.num_svs, m_gps.gps_quality, m_gps.hdop);
	else
		getLocalGPSPosition(gpsStr);

	send_redstone_ud_msg("message-assembler", 0, "msg scheduled_message msg_priority=1 mobile_id=%s mobile_id_type=4 %s\r",id.c_str(), gpsStr.c_str());//Position Update
	send_redstone_ud_msg("message-assembler", 0, "msg calamp_user_msg msg_priority=1 mobile_id=%s mobile_id_type=4 usr_msg_data=%s %s\r",id.c_str() ,ats::to_hex(cmd).c_str(), gpsStr.c_str());
	setRequest(FOB_REQUEST_SOSCANCEL);
	
	ats_logf(ATSLOG_DEBUG, "%s,%d: Setting fob_status_request from sendWatSosCancel", __FILE__, __LINE__);
	m_md->fob_status_request(m_mac, 0x82, 0x12);
}

void fobContent::getLocalGPSPosition(ats::String& gpsStr)
{
	ats::String fixTime;
	ats_sprintf(&fixTime, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d", nmea_client.Year(), nmea_client.Month(),nmea_client.Day(),nmea_client.Hour(),nmea_client.Minute(),(short)(nmea_client.Seconds()));

	ats_sprintf(&gpsStr, "fix_time=\"%s\" latitude=%f longitude=%f satellites=%d fix_status=%d hdop=%f", fixTime.c_str(), nmea_client.Lat(), nmea_client.Lon(), nmea_client.NumSVs(), nmea_client.GPS_Quality(), nmea_client.HDOP());
}

void fobContent::sendCheckinMessage(const ats::String& cmd, const ats::String& id, const ats::String& gpsStr)
{
	ats_logf(ATSLOG_INFO, "%s,%d: %s", __FILE__, __LINE__, cmd.c_str());
	send_redstone_ud_msg("message-assembler", 0, "msg scheduled_message msg_priority=%d mobile_id=%s mobile_id_type=4 %s\r", g_checkinpri, id.c_str(), gpsStr.c_str());
	send_redstone_ud_msg("message-assembler", 0, "msg calamp_user_msg msg_priority=%d mobile_id=%s mobile_id_type=4 usr_msg_data=%s %s\r", g_checkinpri, id.c_str() ,ats::to_hex(cmd).c_str(), gpsStr.c_str());//Check In message
	setRequest(FOB_REQUEST_CHECKIN);
	m_md->fob_status_request(m_mac, FDS_LOWER_GREEN_ON, FDS_GREEN_ON | FDS_MED);
}

void fobContent::turnOffOverdueLight()
{
	if( g_overdueAllow )
	{
		fobState state = getState();
		if(state == FOB_STATE_OVERDUE_SHIFT_TIMER || state == FOB_STATE_OVERDUE_HAZARD || state == FOB_STATE_OVERDUE_SAFETY_AND_SHIFT || state == FOB_STATE_OVERDUE_SAFETY_TIMER)
		 {
			 m_md->fob_status_request(m_mac, FDS_LOWER_GREEN_ON, 0xF0);
		 }
	}
}

bool fobContent::setupRegister()
{
	bool ret = false;
	struct ReadConfigRequest req;
	req.key = m_mac;
	req.socketfd = NULL;

	//read register 28: Brief flash Sequence, 30: Vibrator repetition"
	struct rarg r;
	r.data[0] = 0x32;
	r.data[1] = 0x38;
	req.rvt.push_back(r);

	r.data[0] = 0x33;
	r.data[1] = 0x30;
	req.rvt.push_back(r);

	m_md->m_fobrequest_manager->FOBReadConfigRequest(req, false);

	pthread_create(&m_setRegister_thread, 0, fobContent::fobsetupregister_thread, this);
	ret = true;

	return ret;
}

void* fobContent::fobsetupregister_thread(void* p)
{
	fobContent& b = *((fobContent*)p);

	response<ats::String>* res = new response<ats::String>(0);
	b.m_readconfigManager.add_client(res);
	if(!res->wait(5))
	{
		ats_logf(ATSLOG_ERROR, RED_ON "%s %d: Timed out waiting for a response " RESET_COLOR,__FILE__, __LINE__);
		b.m_readconfigManager.remove_client();
		return NULL;
	}

	const ats::String str = *(res->getresponse());

	if(str.size() < 12) return NULL;

	int reg[2];
	int data[2];
	reg[0] = atoi(str.substr(3, 2).c_str());
	reg[1] = atoi(str.substr(8, 2).c_str());
	data[0] = atoi(str.substr(5, 2).c_str());
	data[1] = atoi(str.substr(10, 2).c_str());

	ats_logf(ATSLOG_INFO, "%s %d: read register from fob: reg1 %d: %d, reg2 %d:%d",__FILE__, __LINE__,	reg[0], data[0], reg[1], data[1]);
	
	for( int i = 0; i < 2; ++i )
	{
		if((reg[i] == FOBBRIEFFLASHREG && data[i] != FOBBRIEFFLASHREGDATA ) || (reg[i] == FOBVIBRATORREPRE && data[i] != FOBVIBRATORREPREGDATA))
		{
			struct WriteConfigRequest req;
			req.key = b.m_mac;
			req.socketfd = NULL;
			struct warg r;
			r.reg[0]=0x32;
			r.reg[1]=0x38;
			r.reg[2]='\0';
			r.data[0] = 0x30;
			r.data[1] = 0x36;
			r.data[2]='\0';
			req.wvt.push_back(r);

			r.reg[0]=0x33;
			r.reg[1]=0x30;
			r.reg[2]='\0';
			r.data[0] = 0x30;
			r.data[1] = 0x30;
			r.data[2]='\0';
			req.wvt.push_back(r);
			b.m_md->m_fobrequest_manager->FOBWriteConfigRequest(req, false);

			ats_logf(ATSLOG_INFO, "%s %d: rewrite register",__FILE__, __LINE__);
			break;
		}
	}

	b.m_readconfigManager.remove_client();
	return NULL;
}

// Description: this flag decided if SLP Status LED flashing Only or with vibration in hazard, shift overdue or oderdue warning state.
//
// return true: if overdue allow and externsion not allow, and trulink received at least once hazard or shift timer overdue message.
// otherwise return false until check out action or manual clear it.
bool fobContent::isHazardFlashingOnly() const
{
	if( g_overdueAllow && !g_AllowTimerExtensions )
		return m_hazardFlashingOnly;

	return false;
}

bool fobContent::isShifttimeFlashingOnly() const
{
	if( g_overdueAllow && !g_AllowTimerExtensions )
		return m_shifttimeFlashingOnly;

	return false;
}

void fobContent::sendOverduetoSLP()
{
	m_md->fob_status_request(m_mac, 0x80, 0x74);
}

void fobContent::setShifttimeFlashingFlag(bool b)
{
	if( g_overdueAllow && !g_AllowTimerExtensions )
	{
		m_shifttimeFlashingOnly = b;
	}
}

void fobContent::setHazardFlashingFlag(bool b)
{
	if( g_overdueAllow && !g_AllowTimerExtensions )
	{
		m_hazardFlashingOnly = b;
	}
}

  //------------------------------------------------------------------------------------------------
	// setState - called when a state message is received from WA
void fobContent::setState(fobState newState)
{
		fobState currentState = getState();
		int request = getRequest();

		// if overdues are allowed and we are currently in an overdue state
		if (g_overdueAllow && (currentState == FOB_STATE_OVERDUE_SAFETY_AND_SHIFT || currentState == FOB_STATE_OVERDUE_SAFETY_TIMER || currentState == FOB_STATE_OVERDUE_SHIFT_TIMER || currentState == FOB_STATE_OVERDUE_HAZARD ))
		{
			// if a state change has occurred we clear the alert flashing and buzzing
			if( newState == FOB_STATE_CHECKIN || newState == FOB_STATE_CHECKOUT ||  newState == FOB_STATE_SOS || newState == FOB_STATE_MANDOWN || newState == FOB_STATE_ASSIST )
			{
				m_md->fob_status_request(m_mac, FDS_LOWER_GREEN_ON, FDS_ALL_OFF);
			}
		}

    // if we are in an SOS state
		if((currentState == FOB_STATE_SOS || currentState == FOB_STATE_MANDOWN) && ( newState != FOB_STATE_SOS ) && ( newState!=FOB_STATE_MANDOWN ))
		{
			if( request & FOB_REQUEST_SOSCANCEL)
			{
				fob_set_timer(m_mac, FDS_RED_ON | FDS_SOLID);//solid red for 5 seconds
				setRequest(FOB_REQUEST_SOSCANCEL, false);
			}
			else //control by manual, no sos cancel button pressed.
				m_md->fob_status_request(m_mac, FDS_LOWER_GREEN_ON, FDS_ALL_OFF);
		}

		ats_logf(ATSLOG_INFO, GREEN_ON "enter setState %d request %d" RESET_COLOR, newState, request);
		switch(newState)
		{
			case FOB_STATE_CHECKIN:
				{
					if(request&FOB_REQUEST_CHECKIN)
					{
						fob_set_timer(m_mac, FDS_GREEN_ON | FDS_SOLID);//solid green for 5 seconds
						setRequest(FOB_REQUEST_CHECKIN, false);
					}
				}
				break;
			case FOB_STATE_OVERDUE_SAFETY_TIMER:
				{
					if(g_overdueAllow)
						sendOverduetoSLP();
				}
				break;
			case FOB_STATE_ASSIST:
				break;
			case FOB_STATE_SOS:
				{
					m_md->fob_status_request(m_mac, FDS_LOWER_GREEN_ON, FDS_RED_ON | FDS_SOLID);
					setRequest(FOB_REQUEST_SOS, false);
				}
				break;
			case FOB_STATE_DISABLE:
				break;
			case FOB_STATE_CHECKOUT:
				{
					if( request&FOB_REQUEST_CHECKOUT)
					{
						fob_set_timer(m_mac, FDS_GREEN_ON | FDS_SOLID);//solid red+green for 5 seconds
						setRequest(FOB_REQUEST_CHECKOUT, false);
					}
					setHazardFlashingFlag(false);
					setShifttimeFlashingFlag(false);
				}
				break;
			case FOB_STATE_OVERDUE_HAZARD:
			{
				if(g_overdueAllow)
				{
					m_md->fob_status_request(m_mac, FDS_LOWER_GREEN_ON, (!isHazardFlashingOnly())?FDS_ORANGE_FLASH_AND_VIBE:FDS_ORANGE_FLASH);
					setHazardFlashingFlag(true);
				}
				else
				{
					if(request&FOB_REQUEST_CHECKIN)
					{
						fob_set_timer(m_mac, FDS_GREEN_ON | FDS_SOLID);//solid green for 5 seconds
						setRequest(FOB_REQUEST_CHECKIN, false);
					}
					else if (request&FOB_REQUEST_CHECKOUT )
					{
						fob_set_timer(m_mac, FDS_RED_ON | FDS_GREEN_ON | FDS_SOLID);//solid red+green for 5 seconds
						setRequest(FOB_REQUEST_CHECKOUT, false);
					}
				}
				break;
			}
			case FOB_STATE_OVERDUE_SHIFT_TIMER:
			case FOB_STATE_OVERDUE_SAFETY_AND_SHIFT:
			{
				if(g_overdueAllow)
				{
					m_md->fob_status_request(m_mac, FDS_LOWER_GREEN_ON, (!isShifttimeFlashingOnly())?FDS_ORANGE_FLASH_AND_VIBE:FDS_ORANGE_FLASH);
					setShifttimeFlashingFlag(true);
				}
				else
				{
					if(request&FOB_REQUEST_CHECKIN)
					{
						fob_set_timer(m_mac, FDS_GREEN_ON | FDS_SOLID);//solid green for 5 seconds
						setRequest(FOB_REQUEST_CHECKIN, false);
					}
					else if (request&FOB_REQUEST_CHECKOUT )
					{
						fob_set_timer(m_mac, FDS_RED_ON | FDS_GREEN_ON | FDS_SOLID);//solid red+green for 5 seconds
						setRequest(FOB_REQUEST_CHECKOUT, false);
					}
				}
				break;
			}
			default:
				break;
		}
		
		lock();
		m_state = newState;
		unlock();

		int remind = getTimeRemind();
		if( remind )
		{
			TimeRemindStatus newState = fobTimeRemindStatus(FOB_TIMERREMIND_HAZARD);
			if( newState == FOB_TIMERREMIND_STATUS_WAITING )
			{
				fobTimeRemindStatus(FOB_TIMERREMIND_HAZARD, FOB_TIMERREMIND_STATUS_INIT);
			}

			newState = fobTimeRemindStatus(FOB_TIMERREMIND_SAFETY);
			if( newState == FOB_TIMERREMIND_STATUS_WAITING )
			{
				fobTimeRemindStatus(FOB_TIMERREMIND_SAFETY, FOB_TIMERREMIND_STATUS_INIT);
			}
			newState = fobTimeRemindStatus(FOB_TIMERREMIND_OFFTIME);
			if( newState == FOB_TIMERREMIND_STATUS_WAITING )
			{
				fobTimeRemindStatus(FOB_TIMERREMIND_OFFTIME, FOB_TIMERREMIND_STATUS_INIT);
			}
			if( remind & FOB_TIMERREMIND_OFFTIME )
			{
				if(g_overdueAllow && !g_AllowTimerExtensions )
				{
					needModifyForShift = true;
					setShifttimeFlashingFlag(true);
				}
			}

			if( remind & FOB_TIMERREMIND_HAZARD)
			{
				if(g_overdueAllow && !g_AllowTimerExtensions )
				{
					needModifyForHazard = true;
					setHazardFlashingFlag(true);
				}
			}
		}
		ats_logf(ATSLOG_INFO, GREEN_ON "leave setState %d request %d" RESET_COLOR, getState(), getRequest());
}
