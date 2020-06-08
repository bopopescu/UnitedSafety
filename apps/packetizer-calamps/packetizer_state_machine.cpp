#include <iomanip>
#include <sstream>
#include <iterator>

#include <stdlib.h>

#include "zlib.h"

#include "ats-common.h"
#include "atslogger.h"
#include "ConfigDB.h"
#include "timer-event.h"

#include "messagetypes.h"
#include "packetizer.h"
#include "packetizerDB.h"
#include "packetizer_state_machine.h"
#include "calampseventmessage.h"
#include "calampsusermessage.h"
#include "AFS_Timer.h"
#include "lmdirect/lmdirect.h"

#define STATE_MACHINE_CLASS PacketizerSm
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

extern const ats::String g_DBcolumnname[];
extern const ats::String g_CALAMPDBcolumnname[];
extern ATSLogger g_log;
extern int g_IsCAMS3;

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(PacketizerSm)

//---------------------------------------------------------------------------------------
PacketizerSm::PacketizerSm(MyData& p_data) : StateMachine( p_data), m_sequence_num(1), m_compressed_was_incremented(false)
{
	db_monitor::ConfigDB db;
	m_keepalive_timeout_seconds = db.GetInt("packetizer-calamps", "m_keepalive", 30);
	m_max_backlog = db.GetInt("packetizer-calamps", "m_max_backlog", 30);
	m_UseCompression = db.GetInt("packetizer-calamps", "UseCompression", 0);
	m_timeout = db.GetInt("packetizer-calamps", "timeout", 10);
	m_dbreader = new PacketizerDB(p_data);
	m_sender = new PacketizerSender(p_data);
	set_log_level(0);
	SET_NEXT_STATE(0);
}

//---------------------------------------------------------------------------------------
PacketizerSm::~PacketizerSm()
{
	delete m_dbreader;
	delete m_sender;
}

//---------------------------------------------------------------------------------------
bool PacketizerSm::readfromCantelDB(int p_mid, struct send_message& p_msg)
{
	ats::StringMap sm;

	if(!m_dbreader->dbquery_from_canteldb(p_mid, sm))
	{
		ats_logf(&g_log, "%s,%d: read from cantel db fail where mid = %d", __FILE__, __LINE__, p_mid);
		return false;
	}
	
	p_msg.data.clear();

	const int code = sm.get_int(g_DBcolumnname[14]);
	p_msg.msg_mid = sm.get_int(g_CALAMPDBcolumnname[35]);
	CalAmpsMessage* msg;
	
	if(code == TRAK_CALAMP_USER_MSG)
	{
		msg = new CalAmpsUserMessage(sm);
	}
	else
	{
		msg = new CalAmpsEventMessage(sm,m_messagetypes[code]);
		ats_logf(&g_log, "%s,%d: Read Event Message %d for seq num %d", __FILE__, __LINE__, m_messagetypes[code], get_sequence_num());
	}

	if(msg->getImei().isEmpty())
	{
		ats_logf(&g_log, "%s,%d: IMEI is empty.	Exiting with code 1.", __FILE__, __LINE__);
		exit(1);
	}

	msg->setSequenceNumber(get_sequence_num());
	msg->WriteData(p_msg.data);
	delete msg;

	p_msg.seq = get_sequence_num();
	p_msg.mid = p_mid;

	inc_sequence_num();
	return true;
}

//---------------------------------------------------------------------------------------
void PacketizerSm::get_message_backlog(std::vector<struct send_message>& p_msg, int p_limit)
{
	std::vector<int> mid;
	m_dbreader->dbselectbacklog(CALAMPDB, mid, (p_limit >= 1) ? p_limit : 1);

	if(mid.size() <= 1 || 0 == m_UseCompression)
	{

		if(mid.empty())
		{
			p_msg.clear();
		}
		else
		{
			p_msg.resize(1);

			if(!readfromCantelDB(mid[0], p_msg[0]))
			{
				m_dbreader->dbrecordremove(mid[0]);
				ats_logf(&g_log, "%s: Remove corrupt %s.mid=%d", __FUNCTION__, CALAMPDB, mid[0]);
			}
		}
		return;
	}

	p_msg.resize(mid.size());
	m_CompressedSeqNum = get_sequence_num();	// this allows for the message log to show 1 (2,3,4,5) 6 (7,8,9,10)... 
	inc_sequence_num();											 // where 1 and 6 are the compressed message, the rest are real messages
	
	for(size_t i = 0; i < p_msg.size(); ++i)
	{
		if(!readfromCantelDB(mid[i], p_msg[i]))
		{
			m_dbreader->dbrecordremove(mid[i]);
			ats_logf(&g_log, "%s: Remove corrupt %s.mid=%d", __FUNCTION__, CALAMPDB, mid[i]);
			mid[i] = -1;
			continue;
		}
	}
}

//---------------------------------------------------------------------------------------
// state 0 - load the databases
void PacketizerSm::state_0()
{
	m_dbreader->start();
	m_sender->start();
	
	m_dbreader->dbload_messagetypes(m_messagetypes);
	SET_NEXT_STATE(5);
}

//---------------------------------------------------------------------------------------
// state 1 - get the regular messages
//
void PacketizerSm::state_1()
{
	AFS_Timer t;
	m_CurTable = "message_table";

	for(;;)
	{
		get_message_backlog(m_msg, m_max_backlog);

		if(m_msg.empty())
		{
		 	if(m_dbreader->dbquerylastmid(CALAMPDB, "realtime_table") > 0)	// check for realtime message
		 	{
				SET_NEXT_STATE(5);	// send realtime message
				return;
		 	}

			sleep(1);
		}
		else
		{
			ats_logf(&g_log, "%s: Found %zu old mid%s", __FUNCTION__, m_msg.size(), (1 == m_msg.size()) ? "" : "s");
			SET_NEXT_STATE(2);
			return;
		}

		if(m_keepalive_timeout_seconds && (t.DiffTime() >= m_keepalive_timeout_seconds))
		{
			//send keep alive
			m_sender->keepAlive();
			t.SetTime();
		}
	}
}


//---------------------------------------------------------------------------------------
// state 2 - send the message
//
void PacketizerSm::state_2()
{
	for(;;)
	{
		if(0 == m_UseCompression || 1 == m_msg.size())
		{
			if(m_sender->sendMessage(m_msg[0]))
			{
				SET_NEXT_STATE(3);
				return;
			}
		}
		else
		{
			struct send_message zmsg;

			if(compress(zmsg, m_msg, m_CompressedSeqNum))
			{
				if(!m_compressed_was_incremented)
				{
					m_compressed_was_incremented = true;
//					inc_sequence_num();
				}

				if(m_sender->sendMessage(zmsg))
				{
					SET_NEXT_STATE(3);
					return;
				}
			}
		}

		m_RedStone.FailedToSend();
		sleep(1);
	}
}

//---------------------------------------------------------------------------------------
// state 3
void PacketizerSm::state_3()
{
	const int res = m_sender->waitforack();

	switch(res)
	{
		case -2://genenal error
		case -1://ack error
		case 0://timeout
			break;
		case 1:

			SET_NEXT_STATE(4);
			return;
		default:
			ats_logf(&g_log, "%s, %d: Unknown ack error received: %d.", __FILE__, __LINE__, res);
	}

	m_RedStone.FailedToSend();
	SET_NEXT_STATE(2);
}

//---------------------------------------------------------------------------------------
// state 4
void PacketizerSm::state_4()
{
	// Have the cell and activity light blink once to indicate a send.
	send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=packetizer-calamps_sending led=cell,act script=\"0,100000;1,1000000\" priority=10 timeout=100000\r");
	m_compressed_was_incremented = false;

	if(m_msg.size())
	{

		for(size_t i = 0; i < m_msg.size(); ++i)
		{
			ats_logf(&g_log, "		 remove mid %d from calamp db - table %s", m_msg[i].mid, m_CurTable.c_str());
			m_dbreader->dbrecordremove(m_msg[i].mid, m_CurTable.c_str());

			SocketError err;
			send_msg("localhost", 41009, &err, "unset_work key=Message_%ld\r",m_msg[i].msg_mid);
		}

		if (m_CurTable != "realtime_table")
			m_mdb.SetLatestPacketizerMID(m_msg[m_msg.size() - 1].msg_mid);
		else	// if we were in the realtime table we clear it out
			m_dbreader->DeleteAllRecords(m_CurTable.c_str());

	}

	m_RedStone.LastSendFailed(false);	// notify iridium side that we are still sending OK
	// need to add updating of lastMID.
	
	if (m_timeout == 0)	// if we are not waiting for acks we sleep 1 second between sends
		sleep(1);
		
	SET_NEXT_STATE(5);
}

//----------------------------------------------------------------------------------------------------
// state_5 - send the real time message if it is there.
//
void PacketizerSm::state_5()
{
	m_CurTable = "realtime_table";
	const int mid = m_dbreader->dbquerylastmid(CALAMPDB, m_CurTable.c_str());

	if(mid)	// if there is a record - send it
	{
		ats_logf(&g_log, "%s,%d: found real time mid %d", __FILE__, __LINE__, mid);
		m_msg.resize(1);

		if(!ReadFromRealTimeDB(mid, m_msg[0]))
		{
			m_dbreader->dbrecordremove(mid, m_CurTable.c_str());
			ats_logf(&g_log, "Remove one corrupt message from cantel_db mid = %d in state 1", mid);
		}

		SET_NEXT_STATE(2);	// send this message
		return;
	}

	SET_NEXT_STATE(1);	// no message - look for other messages.
}

//----------------------------------------------------------------------------------------------------
// ReadFromRealTimeDB - get the specified mid from the realtime table in the calamp deb
//
bool PacketizerSm::ReadFromRealTimeDB(int mid, struct send_message& p_msg)
{
	ats::StringMap sm;
	QByteArray data;

	if(!m_dbreader->dbquery_from_canteldb(mid, sm, "realtime_table"))
	{
		ats_logf(&g_log, "%s,%d: read from realtime db fail where mid = %d", __FILE__, __LINE__, mid);
		return false;
	}

	p_msg.data.clear();

	const int code = sm.get_int(g_DBcolumnname[14]);
	p_msg.msg_mid = sm.get_int(g_CALAMPDBcolumnname[35]);	
	CalAmpsMessage* msg;

	if(code == TRAK_CALAMP_USER_MSG)
	{
		msg = new CalAmpsUserMessage(sm);
	}
	else
	{
		msg = new CalAmpsEventMessage(sm,m_messagetypes[code]);
	}

	if(msg->getImei().isEmpty())
	{
		ats_logf(&g_log, "%s,%d: IMEI is empty.	Exiting with code 1.", __FILE__, __LINE__);
		exit(1);
	}

	msg->setSequenceNumber(get_sequence_num());
	msg->WriteData(p_msg.data);
	delete msg;

	p_msg.seq = get_sequence_num();
	p_msg.mid = mid;

	inc_sequence_num();
	return true;
}

//---------------------------------------------------------------------------------------
// compress
bool PacketizerSm::compress(struct send_message& p_des, const std::vector<struct send_message>& p_msg, int p_sequence_num)
{

	if(p_msg.empty())
	{
		return false;
	}

	QByteArray msg;

	{

		for(size_t i = 0; i < p_msg.size(); ++i)
		{
			const int len = (p_msg[i].data.length() & 0xffff);
			msg.append(char((len >> 8) & 0xff));
			msg.append(char(len & 0xff));
			msg.append(p_msg[i].data);
		}

	}

	uLongf len = compressBound(msg.length());
	char* z = new char[len];
	const int ret = ::compress((Bytef*)z, &len, (Bytef*)(msg.data()), msg.length());

	if(Z_OK != ret)
	{
		delete[] z;
		ats_logf(&g_log, "%s: Error %d for %zu mid compression", __FUNCTION__, ret, p_msg.size());
		return false;
	}

	LMDirect m;
	m.m_option.set_option(LMDIRECT::Option::MOBILE_ID, true);
	m.m_option.set_mobile_id(CalAmpsMessage::readImei());

	m.m_option.set_option(LMDIRECT::Option::MOBILE_ID_TYPE, true);
	m.m_option.set_mobile_id_type(LMDIRECT::MobileIDType::IMEI);

	m.m_message.set_service_type(LMDIRECT::Message::ServiceType::ACKNOWLEDGED_REQUEST);

	m.m_message.set_message_type(LMDIRECT::Message::MessageType::COMPRESSED);
	m.m_message.set_message(ats::String(z, len));
	delete[] z;
	m.m_message.set_sequence(p_sequence_num);

	p_des.seq = p_sequence_num;
	p_des.mid = -1;
	{
		const ats::String& s = m.get_gms_format();
		p_des.data.append(s.c_str(), s.size());
	}
	return true;
}

