#include "zlib.h"

#include "atslogger.h"
#include "ConfigDB.h"
#include "IridiumUtil.h"

#include "packetizer.h"
#include "packetizerCams.h"
#include "packetizerIridiumMessage.h"
#include "packetizerCamsStateMachine.h"

extern MyData g_md;
extern SList g_msg;
extern sem_t g_msg_sem;
pthread_mutex_t PacketizerCamsStateMachine::m_priorityOneMutex;
bool PacketizerCamsStateMachine::m_thread_finished = false;

#define STATE_MACHINE_CLASS PacketizerCamsStateMachine
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(PacketizerCamsStateMachine)

PacketizerCamsStateMachine::PacketizerCamsStateMachine(MyData& p_data) : StateMachine(p_data), sequence_num(0)
{
	ats_logf(ATSLOG(0), "%s,%d: CONSTRUCTOR", __FILE__, __LINE__);
	MyData& md = dynamic_cast<MyData&>(my_data());
	m_dbreader = new PacketizerCamsDB(md, "cams_db", "/mnt/update/database/cams.db");
	m_cell_sender = new PacketizerCellSender(md);
	m_sequence_num = 0;
	m_dbreader->start();

	SET_NEXT_STATE(0);
}

//---------------------------------------------------------------------------------------
bool PacketizerCamsStateMachine::readfromPacketizerDB(int p_mid, struct send_message& p_msg)
{
	ats::StringMap sm;

	if(!m_dbreader->dbquery_from_packetizerdb(p_mid, sm))
	{
		ats_logf(ATSLOG(0), "%s,%d: read from cantel db fail where mid = %d", __FILE__, __LINE__, p_mid);
		return false;
	}

	p_msg.data.clear();

	const int code = sm.get_int("event_type");
	p_msg.pri =  sm.get_int("msg_priority");
	p_msg.msg_db_id = sm.get_int("mtid");
	CalAmpsMessage* msg;

	if(code == TRAK_CALAMP_USER_MSG)
	{
		msg = new CalAmpsUserMessage(sm);
	}
	else
	{
		msg = new CalAmpsEventMessage(sm,m_messagetypes[code]);
		ats_logf(ATSLOG(0), "%s,%d: Read Event Message %d for seq num %d. Mtid: %d",
				 __FILE__, __LINE__, m_messagetypes[code], get_sequence_num(), sm.get_int("mtid"));
	}

	if(msg->getImei().empty())
	{
		ats_logf(ATSLOG(0), "%s,%d: IMEI is empty.  Exiting with code 1.", __FILE__, __LINE__);
		exit(1);
	}

	msg->setSequenceNumber(get_sequence_num());
	QByteArray data;
	msg->WriteData(data);
	p_msg.data.resize(data.size());
	p_msg.data.insert(p_msg.data.begin(), data.begin(), data.end());
	delete msg;

	p_msg.seq = get_sequence_num();
	p_msg.mid = p_mid;

	inc_sequence_num();
	return true;
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::get_message_backlog(std::vector<struct send_message>& p_msg, int p_limit)
{
	std::vector<int> mid;
	m_dbreader->dbselectbacklog(mid, (p_limit >= 1) ? p_limit : 1);

	if(mid.size() <= 1 || 0 == m_UseCompression)  // don't use compression
	{

		if(mid.empty())
		{
			p_msg.clear();
		}
		else
		{
			p_msg.resize(1);
		}
		return;
	}

	p_msg.resize(mid.size());
	inc_sequence_num();
	m_CompressedSeqNum = get_sequence_num(); // this allows for the message log to show 1 (2,3,4,5) 6 (7,8,9,10)...
	inc_sequence_num();                      // where 1 and 6 are the compressed message, the rest are real messages

	for(size_t i = 0; i < p_msg.size(); ++i)
	{
		if(!readfromPacketizerDB(mid[i], p_msg[i]))
		{
			m_dbreader->dbrecordremove(mid[i]);
			ats_logf(ATSLOG(0), "%s: Remove corrupt camsdb.mid=%d", __FUNCTION__, mid[i]);
			mid[i] = -1;
			continue;
		}
	}
}

void PacketizerCamsStateMachine::state_0()
{
	set_log_level(11);  // turns off state machine logging

	db_monitor::ConfigDB db;
	//Initialize state machine variables
	m_keepalive_timeout_seconds = db.GetInt("packetizer-cams", "m_keepalive", 30);
	m_timeout = db.GetInt("packetizer-cams", "timeout", 10);
	m_max_backlog = db.GetInt("packetizer-cams", "m_max_backlog", 30);
	m_retry_limit = db.GetInt("packetizer-cams", "retry_limit", 1);;
	m_iridium_timeout_seconds = db.GetInt("packetizer-cams", "iridium_timeout", 60);
	m_UseCompression = db.GetInt("packetizer-cams", "UseCompression", 0);
	m_IridiumPriorityLevel = db.GetInt("packetizer-cams", "IridiumPriorityLevel", 10);
	m_CellFailModeEnable = db.GetBool("packetizer-cams", "CellFailModeEnable", "Off");
	m_IridiumEnable = db.GetBool("packetizer-cams", "IridiumEnable", "Off");
	m_ForceIridium = db.GetValue("packetizer-cams", "ForceIridium", "Never");  // test iridium without checking cellular
	
	if(m_IridiumEnable)
	{
		if (!db.GetBool("feature","iridium-monitor", "Off"))
		{
			ats_logf(ATSLOG(0),"%s, %d: WARNING: Iridium feature is turned off, and IridiumEnable is turned on in packetizer. Disabling Iridium for packetizer."
					 ,__FILE__,__LINE__);
			m_IridiumEnable = false;
		}
		else
		{
			m_iridium_util = new IridiumUtil();
		}
	}
	m_PriorityIridiumDataLimit = db.GetInt("packetizer-cams", "IridiumDataLimitPriority", 1);
	m_imei = db.GetValue("RedStone", "IMEI");
	m_CellFailMode = false;
	m_keepAliveTimer.SetTime();
	m_cell_sender->start();
	m_dbreader->dbload_messagetypes(m_messagetypes);

	SET_NEXT_STATE(1);
}

void PacketizerCamsStateMachine::state_1()
{
//	int mid = m_dbreader->dbquery_SelectPriorityOneMessage();
//	if(mid > 0)

	sem_wait(&g_msg_sem);
	g_md.lock_data();
	m_msgInfo.sm = *(g_msg.begin());
	g_msg.pop_front();
	g_md.unlock_data();

	{
		inc_sequence_num();
//		m_msgInfo.mid = mid;
//		m_msgInfo.mid = int((double(random()) / double(RAND_MAX))* 120);
		m_msgInfo.pri = int((double(random()) / double(RAND_MAX))* 20);
		m_msgInfo.seq = get_sequence_num();
		ats_logf(ATSLOG(0), "%s,%d,: found Priority %d mid %d", __FILE__, __LINE__, m_msgInfo.pri, m_msgInfo.mid);
		SET_NEXT_STATE(21);
		return;
	}
/*
	if(m_keepalive_timeout_seconds && (m_keepAliveTimer.DiffTime() >= m_keepalive_timeout_seconds))
	{
		//send keep alive
		m_cell_sender->keepAlive();
		m_keepAliveTimer.SetTime();
	}
	SET_NEXT_STATE(23);
*/
}

void PacketizerCamsStateMachine::state_2()
{
#if 0
	m_msgInfo.mid = m_dbreader->dbqueryhighestprimid_from_packetizerdb(m_msgInfo.pri);
	if(m_msgInfo.mid >= 0)
	{
		if(m_dbreader->dbquery_from_packetizerdb(m_msgInfo.mid,m_msgInfo.sm))
		{
			inc_sequence_num();
			m_msgInfo.seq = get_sequence_num();
			SET_NEXT_STATE(17);
			return;
		}
	}
	clearMsgInfo(m_msgInfo);
	usleep(500000);
	SET_NEXT_STATE(1);
#endif
}

void PacketizerCamsStateMachine::state_3()
{
  if (m_ForceIridium != "On" )  // don't even try to send compressed if Forcing iridium.
{
	struct send_message zmsg;
		
	if(compress(zmsg, m_msg, m_CompressedSeqNum))
	{
		m_cell_sender->sendMessage(zmsg);
		SET_NEXT_STATE(5);
		return;
	}
	}

	SET_NEXT_STATE(9);
}

void PacketizerCamsStateMachine::state_4()
{
	ats_logf(ATSLOG(0), "%s,%d: Sending message with mtid:%d", __FILE__, __LINE__, m_msgInfo.sm.get_int("mtid"));
	m_cell_sender->sendSingleMessage(&m_msgInfo, m_messagetypes);
	SET_NEXT_STATE(6);
}

void PacketizerCamsStateMachine::state_5()
{
	const int res = m_cell_sender->waitforack();

	switch(res)
	{
		case -2://genenal error
		case -1://ack error
		case 0://timeout
			break;
		case 1:
			m_CellFailMode = false;
			SET_NEXT_STATE(12);
			return;
		default:
			ats_logf(ATSLOG(0), "%s, %d: Unknown ack error received: %d.", __FILE__, __LINE__, res);
	}
	m_cell_retries++;
	if(m_cell_retries > m_retry_limit)
	{
		m_CellFailMode = true;
		SET_NEXT_STATE(9);
		return;
	}
	SET_NEXT_STATE(3);
}

void PacketizerCamsStateMachine::state_6()
{
	const int res = m_cell_sender->waitforack();

	switch(res)
	{
		case -2://genenal error
		case -1://ack error
		case 0://timeout
			break;
		case 1:
			m_CellFailMode = false;
			SET_NEXT_STATE(11);
			return;
		default:
			ats_logf(ATSLOG(0), "%s, %d: Unknown ack error received: %d.", __FILE__, __LINE__, res);
	}
	m_cell_retries++;
	if(m_cell_retries >= m_retry_limit)
	{
		m_CellFailMode = true;
		SET_NEXT_STATE(8);
		return;
	}
	SET_NEXT_STATE(4);
}

void PacketizerCamsStateMachine::state_7()
{
	if(m_IridiumEnable)
	{
		SET_NEXT_STATE(14);
		return;
	}
	clearMsgInfo(m_msgInfo);
	SET_NEXT_STATE(1);
}

void PacketizerCamsStateMachine::state_8()
{
	ats_logf(ATSLOG(0), "State 8: %s, %d: Message Priority = %d", __FILE__, __LINE__, m_msgInfo.pri);
	if((uint)m_msgInfo.pri <= m_IridiumPriorityLevel)
	{
		SET_NEXT_STATE(7);
		return;
	}
	clearMsgInfo(m_msgInfo);
	SET_NEXT_STATE(9);
}

void PacketizerCamsStateMachine::state_9()
{
	int mid = m_dbreader->dbquery_SelectPriorityOneMessage();
	if(mid > 0)
	{
		inc_sequence_num();
		m_msgInfo.mid = mid;
		m_msgInfo.pri = 1;
		m_msgInfo.seq = get_sequence_num();
		ats_logf(ATSLOG(0), "%s,%d,: found Priority 1 mid %d", __FILE__, __LINE__, mid);
		SET_NEXT_STATE(21);
		return;
	}
	SET_NEXT_STATE(13);

}

void PacketizerCamsStateMachine::state_10()
{
	ats_logf(ATSLOG(0), "%s,%d:Checking for message backlog.", __FILE__, __LINE__);
	get_message_backlog(m_msg, m_max_backlog);
	if(m_msg.empty())
	{
		SET_NEXT_STATE(1);
		usleep(500000);
		return;
	}
	else if(m_msg.size() == 1)
	{
		m_msg.clear();
		SET_NEXT_STATE(2);
		return;
	}
	SET_NEXT_STATE(3);

}

void PacketizerCamsStateMachine::state_11()
{
	ats_logf(ATSLOG(0), "%s,%d: Acknowledgement received for mtid %d.", __FILE__,__LINE__, m_msgInfo.sm.get_int("mtid"));
	// Have the cell and activity light blink once to indicate a send.
	send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=packetizer-cams_sending led=cell,act script=\"0,100000;1,1000000\" priority=10 timeout=100000\r");
	if(m_msgInfo.pri == CAMS_CURRENT_MSG_PRI)
	{
		m_dbreader->dbrecordremove(0, "current_table");
	}
	else
	{
		m_dbreader->dbrecordremove(m_msgInfo.mid);
	}
	{
		SocketError err;
		send_msg("localhost", 41009, &err, "unset_work key=Message_%d\r",m_msgInfo.sm.get_int("mtid"));
	}
	clearMsgInfo(m_msgInfo);

	if (m_timeout == 0)  // if we are not waiting for acks we sleep 1 second between sends ATSFIXME: Needs to be added to the design document in SVN
		sleep(1);
	SET_NEXT_STATE(1);
}

void PacketizerCamsStateMachine::state_12()
{
	SocketError err;
	for(size_t i = 0; i < m_msg.size(); ++i)
	{
		ats_logf(ATSLOG(0), "     remove mid %d from calamp db - table message_table", m_msg[i].mid);
		m_dbreader->dbrecordremove(m_msg[i].mid);
		{
			send_msg("localhost", 41009, &err, "unset_work key=Message_%d\r",m_msg[i].msg_db_id);
		}
	}
	m_msg.resize(1);
	SET_NEXT_STATE(1);
}

void PacketizerCamsStateMachine::state_13()
{

	m_msgInfo.mid = m_dbreader->dbqueryhighestprimid_from_packetizerdb(m_msgInfo.pri);
	if((uint)m_msgInfo.pri <= m_IridiumPriorityLevel)
	{
		if(m_dbreader->dbquery_from_packetizerdb(m_msgInfo.mid,m_msgInfo.sm))
		{
			SET_NEXT_STATE(7);
			return;
		}
	}
	clearMsgInfo(m_msgInfo);
	SET_NEXT_STATE(1);
}

void PacketizerCamsStateMachine::state_14()
{
	if(m_iridium_util->isNetworkAvailable())
	{
		SET_NEXT_STATE(15);
		return;
	}
	
	ats_logf(ATSLOG(0), "%s,%d: Iridium network is unavailable.", __FILE__, __LINE__);
	clearMsgInfo(m_msgInfo);
	SET_NEXT_STATE(1);
}

void PacketizerCamsStateMachine::state_15()
{
	m_msgInfo.sm.set("cell_imei",m_imei);
	PacketizerIridiumMessage msg(m_msgInfo.sm);
	msg.mid(m_msgInfo.mid);
	m_state15_iridium_data.clear();
	msg.packetize(m_state15_iridium_data);

	if(m_state15_iridium_data.size() == 0)
	{
		ats_logf(ATSLOG(0), "%s,%d: Failed to packetize message.", __FILE__, __LINE__);
		SET_NEXT_STATE(11);
		return;
	}

	IridiumUtil::ComputeCheckSum(m_state15_iridium_data);

	if(m_msgInfo.pri > m_PriorityIridiumDataLimit)
	{
		SET_NEXT_STATE(25);
		return;
	}

	SET_NEXT_STATE(24);
}

void PacketizerCamsStateMachine::state_16()
{
	if(m_state16_message_sent)
	{
		SET_NEXT_STATE(11);
		return;
	}
	ats_logf(ATSLOG(0), "%s,%d: Message failed to send. Check iridium-monitor log for error.", __FILE__, __LINE__);
	clearMsgInfo(m_msgInfo);
	SET_NEXT_STATE(1);
}

void PacketizerCamsStateMachine::state_17()
{
  if (m_ForceIridium != "Never")
  {
		db_monitor::ConfigDB db;
		m_ForceIridium = db.GetValue("packetizer-cams", "ForceIridium", "Never");  // test iridium without checking cellular
		
		if (m_ForceIridium == "On")
		{
			ats_logf(ATSLOG(0), "%s,%d: ForceIridium is ON - sending via Iridium.", __FILE__, __LINE__);
		  SET_NEXT_STATE(8);  // skip to sending via iridium
		  return;
		}
	}

	m_cell_retries = 0;
	if(m_CellFailMode && m_CellFailModeEnable)
	{
		SET_NEXT_STATE(18);
		return;
	}
	SET_NEXT_STATE(4);
}

void PacketizerCamsStateMachine::state_18()
{
	m_cell_sender->sendSingleMessage(&m_msgInfo, m_messagetypes);
	SET_NEXT_STATE(19);
}

void PacketizerCamsStateMachine::state_19()
{
	const int res = m_cell_sender->waitforack();

	switch(res)
	{
		case -2://general error
		case -1://ack error
		case 0://timeout
			break;
		case 1:
			m_CellFailMode = false;
			SET_NEXT_STATE(11);
			return;
		default:
			ats_logf(ATSLOG(0), "%s, %d: Unknown ack error received: %d.", __FILE__, __LINE__, res);
	}

	SET_NEXT_STATE(8);
}

void PacketizerCamsStateMachine::state_20()
{
	if(m_UseCompression && (m_max_backlog > 1))
	{
		SET_NEXT_STATE(10);
		return;
	}
	SET_NEXT_STATE(2);
}

ats::String event_time()
{
        struct timeval tv;
        gettimeofday(&tv, 0);

        struct tm t;
        gmtime_r(&(tv.tv_sec), &t);
        char buf[128];
        snprintf(buf, sizeof(buf) - 1, "%04d-%02d-%02d %02d:%02d:%02d",
                t.tm_year + 1900,
                t.tm_mon + 1,
                t.tm_mday,
                t.tm_hour,
                t.tm_min,
                t.tm_sec);
        buf[sizeof(buf) - 1] = '\0';
        return ats::String(buf);
}

// AWARE360 FIXME: This function should be in a library.
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

void PacketizerCamsStateMachine::state_21()
{
	pthread_mutex_init(&m_priorityOneMutex, NULL);
	m_thread_finished = false;

	if(!m_msgInfo.sm.has_key("event_type"))
	{
		m_msgInfo.sm.set("event_type", "256"); // TRAK_CALAMP_USER_MSG
	}

	{
		const ats::String& et = event_time();

		if(!m_msgInfo.sm.has_key("event_time"))
		{
			m_msgInfo.sm.set("event_time", et);
		}

		if(!m_msgInfo.sm.has_key("fix_time"))
		{
			m_msgInfo.sm.set("fix_time", et);
		}

	}

	if(!m_msgInfo.sm.has_key("latitude"))
	{
		m_msgInfo.sm.set("latitude", "50.988623333");
	}

	if(!m_msgInfo.sm.has_key("longitude"))
	{
		m_msgInfo.sm.set("longitude", "-114.049068333");
	}

	if(!m_msgInfo.sm.has_key("altitude"))
	{
		m_msgInfo.sm.set("altitude", "1066.2");
	}

	if(!m_msgInfo.sm.has_key("fix_status"))
	{
		m_msgInfo.sm.set("fix_status", "2");
	}

	if(!m_msgInfo.sm.has_key("speed"))
	{
		m_msgInfo.sm.set("speed", "37.0");
	}

	if(!m_msgInfo.sm.has_key("heading"))
	{
		m_msgInfo.sm.set("heading", "38.1");
	}

	if(!m_msgInfo.sm.has_key("rssi"))
	{
		int rssi = int((double(random()) / double(RAND_MAX)) * 116);
		m_msgInfo.sm.set("rssi", ats::toStr(0 - rssi));
	}

	if(!m_msgInfo.sm.has_key("msg_priority"))
	{
		int mpri = int((double(random()) / double(RAND_MAX)) * 116);
		m_msgInfo.sm.set("msg_priority", ats::toStr(mpri));
	}

	if(!m_msgInfo.sm.has_key("inputs"))
	{
		const unsigned int in = ((random()&0x1) ? 0x1 : 0)
			| ((random()&0x1) ? 0x2 : 0)
			| ((random()&0x1) ? 0x4 : 0)
			| ((random()&0x1) ? 0x8 : 0)
			| ((random()&0x1) ? 0x10 : 0)
			| ((random()&0x1) ? 0x20 : 0)
			| ((random()&0x1) ? 0x40 : 0)
			| ((random()&0x1) ? 0x80 : 0)
			| ((random()&0x1) ? 0x100 : 0)
			| ((random()&0x1) ? 0x200 : 0)
			| ((random()&0x1) ? 0x400 : 0)
			| ((random()&0x1) ? 0x800 : 0)
			| ((random()&0x1) ? 0x1000 : 0)
			| ((random()&0x1) ? 0x2000 : 0)
			| ((random()&0x1) ? 0x4000 : 0)
			| ((random()&0x1) ? 0x8000 : 0)
			| ((random()&0x1) ? 0x10000 : 0)
			| ((random()&0x1) ? 0x20000 : 0)
			| ((random()&0x1) ? 0x40000 : 0)
			| ((random()&0x1) ? 0x80000 : 0)
			| ((random()&0x1) ? 0x100000 : 0)
			| ((random()&0x1) ? 0x200000 : 0)
			| ((random()&0x1) ? 0x400000 : 0)
			| ((random()&0x1) ? 0x800000 : 0)
			| ((random()&0x1) ? 0x1000000 : 0)
			| ((random()&0x1) ? 0x2000000 : 0)
			| ((random()&0x1) ? 0x4000000 : 0)
			| ((random()&0x1) ? 0x8000000 : 0)
			| ((random()&0x1) ? 0x10000000 : 0)
			| ((random()&0x1) ? 0x20000000 : 0)
			| ((random()&0x1) ? 0x40000000 : 0)
			| ((random()&0x1) ? 0x80000000 : 0)
		;
		m_msgInfo.sm.set("inputs", ats::toStr(in));
		fprintf(stderr, "inputs=%08X\n", in);
	}

	if(!m_msgInfo.sm.has_key("usr_msg_route"))
	{
		m_msgInfo.sm.set("usr_msg_route", "0");
	}

	if(!m_msgInfo.sm.has_key("usr_msg_id"))
	{
		m_msgInfo.sm.set("usr_msg_id", "17");
	}

// \x83
// \x05			# Mobile ID Length
// \x11\x11\x00\x10\x23	# Mobile ID
// \x01			# Mobile ID Type Length
// \x02			# Mobile ID Type (2 = International Mobile Equipment Identifier (IMEI))
// \x01			# Service Type (1 = Acknowledged Request)
// \x04			# Message Type (2 = Event Report message, 4 = User Data Message)
// \x00\x1c
// \x53\x32\x95\x26\x00\x00\x00\x00\x12\x2b\xcd\x86\xc5\xbe\xa4\x57\x00\x00\x00\x00\x00\x00\x11\x76\x01\x0e\x01\x02\x00\x00\xff\xc1\x00\x10\xff\x08\x00\x00\x00\x10\x24\x50\x47\x45\x4d\x45\x4d\x2c\x2c\x2c\x53\x4c\x50\x2a\x37\x34


// \x83
// \x08					# Mobile ID Length
// \x11\x22\x33\x44\x55\x66\x77\x88	# Mobile ID
// \x01					# Mobile ID Type Length
// \x02					# Mobile ID Type (2 = International Mobile Equipment Identifier (IMEI))
// \x01					# Service Type (1 = Acknowledged Request)
// \x04					# Message Type (2 = Event Report message, 4 = User Data Message)
// \x00\x1c				# Sequence Number
// \x53\x32\x95\x26			# Update Time
// \x00\x00\x00\x00			# Time of fix
// \x12\x2b\xcd\x86			# Lat
// \xc5\xbe\xa4\x57			# Long
// \x00\x00\x00\x00			# Altitude
// \x00\x00\x11\x76			# Speed
// \x01\x0e				# Heading
// \x01					# Satellites
// \x02					# Fix status
// \x00\x00				# Carrier
// \xff\xc1				# RSSI
// \x00					# Comm State
// \x10					# HDOP
// \xff					# Inputs
// \x08					# Unit Status
// \x00					# User Message Route
// \x00					# User Message ID
// \x00\x10				# User Message Length
// \x24\x50\x47\x45\x4d\x45\x4d\x2c\x2c\x2c\x53\x4c\x50\x2a\x37\x34 # User Message

// \x83
// \x05
// \x61\x11\x00\x10\x23
// \x01
// \x02
// \x00			# Service Type
// \x02			# Message Type
// \x00\x1c		# Sequence Number
// \x53\x32\x95\x26
// \x00\x00\x00\x00
// \x12\x2b\xcd\x86	# Lat
// \xc5\xbe\xa4\x57	# Long
// \x00\x00\x00\x00	# Altitude
// \x00\x00\x11\x76	# Speed
// \x01\x0e		# Heading
// \x07			# Satellites
// \x50			# Fix status
// \x00\x00		# Carrier
// \xff\xc1		# RSSI
// \x00			# Comm Status
// \x10			# HDOP
// \xff			# Inputs
// \x08			# Unit Status
// \x01			# Event Index
// \x64			# Event Code
// \x04			# accumCount
// \x00			# Spare
// \x00\x00\x00\x00	# accum
// \x00\x00\x00\x00	# fixTime
// \x00\x00\x00\x00\x00\x00\x00\x00


// \x83
// \x08					# Mobile ID Length
// \x35\x71\x64\x04\x10\x70\x58\x5f	# Mobile ID
// \x01					# Mobile ID Type Length
// \x02					# Mobile ID Type (2 = International Mobile Equipment Identifier (IMEI))
// \x01					# Service Type (1 = Acknowledged Request)
// \x04					# Message Type (2 = Event Report message, 4 = User Data Message)
// \x00\x04				# Sequence Number
// \x55\xe9\x9c\xe5			# Update Time
// \x55\xe9\x9c\xe5			# Time of fix
// \x1e\x64\x3f\x20			# Lat
// \xbc\x05\x7e\x80			# Long
// \x00\x01\xa0\x7c			# Altitude
// \x00\x00\x04\x03			# Speed
// \x00\x26				# Heading
// \x00					# Satellites
// \x02					# Fix status
// \x00\x00				# Carrier
// \xff\xc4				# RSSI
// \x00					# Comm State
// \x00					# HDOP
// \xff					# Inputs
// \x00					# Unit Status
// \x00					# User Message Route
// \x01					# User Message ID
// \x00\x02				# User Message Length
// \xee\x74				# User Message

//	const ats::String msg("$PGEMEM,,MANDOWN INITIATED EMERGENCY,SLP");
	if(!m_msgInfo.sm.has_key("usr_msg_data"))
	{
		const ats::String msg("$PGEMEM,,,SLP");
		m_msgInfo.sm.set("usr_msg_data", ats::to_hex(msg + NMEAChecksum(msg) + "\r\n"));

		{
			int i;

			for(i = 0; i < 16; ++i)
			{
				const int acc = int((double(random()) / double(RAND_MAX)) * 65535);
				m_msgInfo.sm.set("acum" + ats::toStr(i), ats::toStr(acc));
			}

		}

	}
	// User can disable auto-checksum by specifying "_no_checksum".
	else if(!m_msgInfo.sm.has_key("_no_checksum"))
	{
		// Check if user message requires checksum, and if so, add it.
		const ats::String& s = m_msgInfo.sm.get("usr_msg_data");

		if(!s.empty() && ('$' == s[0]))
		{

			if(
				(s.rfind('*') != (s.size() - 5))
				||
				(s.rfind('\r') != (s.size() - 2))
				||
				(s.rfind('\n') != (s.size() - 1))
			)
			{
				m_msgInfo.sm.set("usr_msg_data", s + NMEAChecksum(s) + "\r\n");
			}

		}

	}
/*
INSERT into message_table (
mtid,'7630',
msg_priority,
event_time,
fix_time,
latitude,
longitude,
altitude,
speed,
heading, satellites, fix_status,
hdop, inputs, unit_status, event_type,
acum0, acum1, acum2, acum3, acum4, acum5, acum6, acum7, acum8, acum9, acum10, acum11, acum12, acum13, acum14, acum15,
usr_msg_route, usr_msg_id, usr_msg_data,
rssi,
mobile_id,
mobile_id_type)

VALUES (
'20',
'2015-09-02 20:05:08',
'2015-09-02 20:05:08',
'50.988623333',
'-114.049068333',
'1066.2',
'0.0','39.52','16','2','0.689999998','1','','10','','','','','','','','','','','','','','','','','','','','-101','357164041070585','2')
*/

fprintf(stderr, "event_time=%s\n", (event_time()).c_str());
/*
	if(!m_dbreader->dbquery_from_packetizerdb(m_msgInfo.mid,m_msgInfo.sm))
	{
		ats_logf(ATSLOG(0), "%s, %d: Cannot read priority 1 message from database mid %d", __FILE__,__LINE__, m_msgInfo.mid);
		SET_NEXT_STATE(1);
		clearMsgInfo(m_msgInfo);
		return;
	}
*/

	struct cell_thread_data cd;
	cd.cs = m_cell_sender;
	cd.mi = &m_msgInfo;
	cd.msg_types = &m_messagetypes;

	if(!m_msgInfo.sm.has_key("cell_imei"))
	{
		m_msgInfo.sm.set("cell_imei",m_imei);
	}

#if 0
	{
		CalAmpsUserMessage msg(m_msgInfo.sm);
		QByteArray data;
		msg.WriteData(data);
		m_msgInfo.data.resize(data.size());
		m_msgInfo.data.insert(p_msg.data.begin(), data.begin(), data.end());
	}
#endif

	pthread_t cellThread, iridiumThread;
	int err = pthread_create(&cellThread, 0, priorityOneCellSend, &cd);

	if(err)
	{
		ats_logf(ATSLOG(0), "%s, %d: Error %d when creating cellThread for priority %d messages.", __FILE__, __LINE__, err, m_msgInfo.pri);
		sleep(1);
		SET_NEXT_STATE(1);
		return;
	}

	if(m_IridiumEnable)
	{
		struct iridium_thread_data id;
		id.mi = &m_msgInfo;
		err = pthread_create(&iridiumThread, 0, priorityOneIridiumSend, &id);
		if(!err)
		{
			pthread_join(iridiumThread, NULL);
		}
		else
		{
			ats_logf(ATSLOG(0), "%s, %d: Error %d when creating iridiumThread for priority1 messages.", __FILE__, __LINE__, err);
		}
	}
	pthread_join(cellThread, NULL);
	SET_NEXT_STATE(22);
}

void PacketizerCamsStateMachine::state_22()
{
//	m_dbreader->dbrecordremove(m_msgInfo.mid);
	{
		SocketError err;
		send_msg("localhost", 41009, &err, "unset_work key=Message_%d\r",m_msgInfo.sm.get_int("mtid"));
	}

	sleep(1);
	SET_NEXT_STATE(1);
}

void PacketizerCamsStateMachine::state_23()
{
	if(m_dbreader->dbquery_curr_msg_from_packetizerdb(m_msgInfo.sm))
	{
		inc_sequence_num();
		m_msgInfo.pri = CAMS_CURRENT_MSG_PRI;
		m_msgInfo.seq = get_sequence_num();
		SET_NEXT_STATE(17);
		return;
	}
	clearMsgInfo(m_msgInfo);
	SET_NEXT_STATE(20);

}

void PacketizerCamsStateMachine::state_24()
{
	m_state16_message_sent = m_iridium_util->sendMessage(m_state15_iridium_data);
	SET_NEXT_STATE(16);
}

void PacketizerCamsStateMachine::state_25()
{
	m_state16_message_sent = m_iridium_util->sendMessageWDataLimit(m_state15_iridium_data);
	SET_NEXT_STATE(16);
}


//---------------------------------------------------------------------------------------
// compress
bool PacketizerCamsStateMachine::compress(struct send_message& p_des, const std::vector<struct send_message>& p_msg, int p_sequence_num)
{

	if(p_msg.empty())
	{
		return false;
	}
	ats_logf(ATSLOG(0), "%s,%d: Compressing %d messages.", __FILE__,__LINE__, p_msg.size());
	QByteArray msg;
	{
		for(size_t i = 0; i < p_msg.size(); ++i)
		{
			const int len = (p_msg[i].data.size() & 0xffff);
			msg.append(char((len >> 8) & 0xff));
			msg.append(char(len & 0xff));
			msg.append(&(p_msg[i].data[0]), p_msg[i].data.size());
		}
	}

	uLongf len = compressBound(msg.length());
	char* z = new char[len];
	const int ret = ::compress((Bytef*)z, &len, (Bytef*)(msg.data()), msg.length());
	ats_logf(ATSLOG(0), "%s, %d: Compressing %d bytes to %ld bytes.", __FILE__, __LINE__, msg.size(), len);

	if(Z_OK != ret)
	{
		delete[] z;
		ats_logf(ATSLOG(0), "%s: Error %d for %zu mid compression", __FUNCTION__, ret, p_msg.size());
		return false;
	}

	CalAmpsCompressedMessage m(CalAmpsMessage::readImei(),ats::String(z,len));

	delete[] z;
	m.setSequenceNumber(p_sequence_num);

	p_des.seq = p_sequence_num;
	p_des.mid = -1;
	{
		const ats::String& s = m.getMessage();
		p_des.data.insert(p_des.data.end(), s.begin(), s.end());
	}
	return true;
}

void* PacketizerCamsStateMachine::priorityOneCellSend(void* p_data)
{
	struct cell_thread_data* cd = (struct cell_thread_data*) p_data;
	int res = false;

	while(!isThreadFinished())
	{
		res = cd->cs->sendSingleMessage(cd->mi, *(cd->msg_types));

		if(!res)
		{
			ats_logf(ATSLOG(0), "%s, %d: Error sending priority one message", __FILE__, __LINE__);
		}
		else
		{
			int ack = cd->cs->waitforack();

			switch(ack)
			{
				case -2://genenal error
				case -1://ack error
				case 0://timeout
					break;
				case 1:
					pthread_mutex_lock(&m_priorityOneMutex);
					m_thread_finished = true;
					pthread_mutex_unlock(&m_priorityOneMutex);
					return (void*)0;
				default:
					ats_logf(ATSLOG(0), "%s, %d: Unknown ack error received: %d.", __FILE__, __LINE__, res);
			}

		}

		sleep(1);
	}

	return (void*)1;
}

void* PacketizerCamsStateMachine::priorityOneIridiumSend(void* p_data)
{
	struct iridium_thread_data* id = (struct iridium_thread_data*)p_data;

	PacketizerIridiumMessage msg(id->mi->sm);
	msg.mid(id->mi->mid);
	std::vector<char> data;
	msg.packetize(data);
	if(data.size() == 0)
	{
		ats_logf(ATSLOG(0), "%s,%d: Failed to packetize message.", __FILE__, __LINE__);
		return (void*)1;
	}
	IridiumUtil::ComputeCheckSum(data);
	ats_logf(ATSLOG(0), "%s: Calculated checksum. connecting to iridium-monitor.", __PRETTY_FUNCTION__);
	IridiumUtil iu;
	while(!isThreadFinished())
	{
		if(iu.isNetworkAvailable())
		{
			ats_logf(ATSLOG(0), "%s,%d: Sending Iridium message.", __FILE__, __LINE__);
			if(iu.sendMessage(data))
			{
				pthread_mutex_lock(&m_priorityOneMutex);
				m_thread_finished = true;
				pthread_mutex_unlock(&m_priorityOneMutex);
				return (void*)0;
			}
			ats_logf(ATSLOG(0), "%s,%d: Failed to Send message.", __FILE__, __LINE__);
		}
		sleep(1);
		ats_logf(ATSLOG(2), "%s,%d: Iridium network not available.", __FILE__, __LINE__);
	}

	return (void*)1;
}
