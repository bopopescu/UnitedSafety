#include "zlib.h"

#include <fstream>
#include <iomanip>
#include "atslogger.h"
#include "ConfigDB.h"
#include "IridiumUtil.h"

#include "packetizer.h"
#include "packetizerCams.h"
#include "packetizerIridiumMessage.h"
#include "packetizerCamsStateMachine.h"
#include "CThreadWait.h"

pthread_mutex_t PacketizerCamsStateMachine::m_priorityOneMutex;
bool PacketizerCamsStateMachine::m_thread_finished = false;
int msgType = 1;
extern CThreadWait g_TWaitForNewRecord;

#define STATE_MACHINE_CLASS PacketizerCamsStateMachine
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

enum 
{
  SM_ATSLOG_ERROR,
  SM_ATSLOG_DEBUG,
  SM_ATSLOG_TRACE = 10
} ;

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(PacketizerCamsStateMachine)
#define ATS_TRACE ats_logf(ATSLOG_ERROR, "%s,%d: %s", __FILE__, __LINE__, __FUNCTION__);
//---------------------------------------------------------------------------------------
PacketizerCamsStateMachine::PacketizerCamsStateMachine(MyData& p_data) : StateMachine(p_data), sequence_num(0)
{
	ats_logf(ATSLOG_ERROR, "%s,%d: CONSTRUCTOR", __FILE__, __LINE__);
	pthread_mutex_init(&m_priorityOneMutex, NULL);
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
		ats_logf(ATSLOG_ERROR, "%s,%d: read from cantel db fail where mid = %d", __FILE__, __LINE__, p_mid);
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
		ats_logf(ATSLOG_INFO, "%s,%d: Read Event Message %d for seq num %d. Mtid: %d",
				 __FILE__, __LINE__, m_messagetypes[code], get_sequence_num(), sm.get_int("mtid"));
	}

	if(msg->getImei().empty())
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: IMEI is empty.  Exiting with code 1.", __FILE__, __LINE__);
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
			ats_logf(ATSLOG_ERROR, "%s: Remove corrupt camsdb.mid=%d", __FUNCTION__, mid[i]);
			mid[i] = -1;
			continue;
		}
	}
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_0()
{
	ATS_TRACE;
	set_log_level(11);  // turns off state machine logging

	db_monitor::ConfigDB db;
	//Initialize state machine variables
	m_keepalive_timeout_seconds = 30;// db.GetInt("packetizer-cams", "m_keepalive", 30);  // keeps the udp channel back to the server open 
	m_timeout = db.GetInt("packetizer-cams", "timeout", 10);
	m_max_backlog = 30; //db.GetInt("packetizer-cams", "m_max_backlog", 30);
	m_retry_limit = 1; //db.GetInt("packetizer-cams", "retry_limit", 1);;
	m_iridium_timeout_seconds = db.GetInt("packetizer-cams", "iridium_timeout", 60);
	m_UseCompression = db.GetInt("packetizer-cams", "UseCompression", 1);
	m_IridiumPriorityLevel = 10; // db.GetInt("packetizer-cams", "IridiumPriorityLevel", 10);
	m_CellFailModeEnable = false; //db.GetBool("packetizer-cams", "CellFailModeEnable", "Off");
	m_IridiumEnable = db.GetBool("packetizer-cams", "IridiumEnable", "Off");
	m_ForceIridium = db.GetValue("packetizer-cams", "ForceIridium", "Never");  // test iridium without checking cellular
	m_FakeIridium = false; //db.GetBool("packetizer-cams", "FakeIridium", "Off");  // Send Iridium email directly via cell network
	
	if(m_IridiumEnable)
	{
		if (!db.GetBool("feature","iridium-monitor", "Off"))
		{
			ats_logf(ATSLOG_ERROR,"%s, %d: WARNING: Iridium feature is turned off, and IridiumEnable is turned on in packetizer. Disabling Iridium for packetizer.", __FILE__, __LINE__);
			m_IridiumEnable = false;
		}
		else
		{
			m_iridium_util = new IridiumUtil();
		}
	}
	m_PriorityIridiumDataLimit = db.GetInt("packetizer-cams", "IridiumDataLimitPriority", 1);
	m_imei = db.GetValue("RedStone", "IMEI");
	m_IridiumIMEI = db.GetValue("Iridium", "IMEI");
	m_CellFailMode = false;
	m_keepAliveTimer.SetTime();
	m_cell_sender->start();
	m_dbreader->dbload_messagetypes(m_messagetypes);
	set_log_level(SM_ATSLOG_TRACE);
	
	SET_NEXT_STATE(1);
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_1()
{
	ATS_TRACE
	ats_logf(ATSLOG_INFO, "%s,%d,: state 1", __FILE__, __LINE__);
	int mid = m_dbreader->dbquery_SelectPriorityOneMessage();
	
	if(mid > 0)
	{
		inc_sequence_num();
		m_msgInfo.mid = mid;
		m_msgInfo.pri = 1;
		m_msgInfo.seq = get_sequence_num();
		ats_logf(ATSLOG_DEBUG, "%s,%d,: found Priority 1 mid %d", __FILE__, __LINE__, mid);
		SET_NEXT_STATE(21);
		return;
	}
	if(m_keepalive_timeout_seconds && (m_keepAliveTimer.DiffTime() >= m_keepalive_timeout_seconds))
	{
		//send keep alive
		m_cell_sender->keepAlive();
		m_keepAliveTimer.SetTime();
	}
	SET_NEXT_STATE(23);
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_2()
{
	ATS_TRACE

	if(m_dbreader->dbqueryhighestprimid_from_packetizerdb((size_t &)m_msgInfo.mid, (size_t &)m_msgInfo.pri))
	{

		if(m_dbreader->dbquery_from_packetizerdb(m_msgInfo.mid, m_msgInfo.sm))
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
}

//---------------------------------------------------------------------------------------
// State 3 - we have multiple messages to send. Compress them then send them - go to 5
//           Skip to 9 if we are forcing iridium
void PacketizerCamsStateMachine::state_3()
{
	ATS_TRACE
  if (m_ForceIridium != "Never")
  {
		db_monitor::ConfigDB db;
		m_ForceIridium = db.GetValue("packetizer-cams", "ForceIridium", "Never");  // test iridium without checking cellular
	}
	
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

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_4()
{
	ATS_TRACE
	ats_logf(ATSLOG_DEBUG, "%s,%d: Sending message with mtid:%d", __FILE__, __LINE__, m_msgInfo.sm.get_int("mtid"));
	m_cell_sender->sendSingleMessage(&m_msgInfo, m_messagetypes);
	SET_NEXT_STATE(6);
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_5()
{
	ATS_TRACE
	const int res = m_cell_sender->waitforack();

	switch(res)
	{
		case -2://general error
		case -1://ack error
		case 0://timeout
			break;
		case 1:
			m_CellFailMode = false;
			SET_NEXT_STATE(12);
			return;
		default:
			ats_logf(ATSLOG_ERROR, "%s, %d: Unknown ack error received: %d.", __FILE__, __LINE__, res);
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

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_6()
{
	ATS_TRACE
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
			ats_logf(ATSLOG_ERROR, "%s, %d: Unknown ack error received: %d.", __FILE__, __LINE__, res);
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

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_7()
{
	ATS_TRACE
	if(m_IridiumEnable)
	{
		SET_NEXT_STATE(14);
		return;
	}
	clearMsgInfo(m_msgInfo);
	SET_NEXT_STATE(1);
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_8()
{
	ATS_TRACE
	ats_logf(ATSLOG_INFO, "State 8: %s, %d: Message Priority = %d", __FILE__, __LINE__, m_msgInfo.pri);
	if((uint)m_msgInfo.pri <= m_IridiumPriorityLevel)
	{
		SET_NEXT_STATE(7);
		return;
	}
	clearMsgInfo(m_msgInfo);
	SET_NEXT_STATE(9);
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_9()
{
	ATS_TRACE
	int mid = m_dbreader->dbquery_SelectPriorityOneMessage();
	if(mid > 0)
	{
		inc_sequence_num();
		m_msgInfo.mid = mid;
		m_msgInfo.pri = 1;
		m_msgInfo.seq = get_sequence_num();
		ats_logf(ATSLOG_DEBUG, "%s,%d,: found Priority 1 mid %d", __FILE__, __LINE__, mid);
		SET_NEXT_STATE(21);
		return;
	}
	SET_NEXT_STATE(13);

}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_10()
{
	ATS_TRACE
	ats_logf(ATSLOG_INFO, "%s,%d:Checking for message backlog.", __FILE__, __LINE__);
	
	get_message_backlog(m_msg, m_max_backlog);
	if(m_msg.empty())
	{
		ats_logf(ATSLOG_INFO, "%s,%d,: no messages - sleeping", __FILE__, __LINE__);
		g_TWaitForNewRecord.Wait();
		SET_NEXT_STATE(1);
		usleep(1000000);
		return;
	}
	else if(m_msg.size() == 1)
	{
		ats_logf(ATSLOG_DEBUG, "%s,%d,: 1 message", __FILE__, __LINE__);
		m_msg.clear();
		SET_NEXT_STATE(2);
		return;
	}
	
	ats_logf(ATSLOG_DEBUG, "%s,%d,: messages", __FILE__, __LINE__);
	SET_NEXT_STATE(3);

}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_11()
{
	ATS_TRACE
	ats_logf(ATSLOG_INFO, "%s,%d: Acknowledgement received for mtid %d.", __FILE__,__LINE__, m_msgInfo.sm.get_int("mtid"));
	// Have the cell and activity light blink once to indicate a send.
	send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=packetizer-cams_sending led=cell script=\"0,100000;1,100000\" priority=5 timeout=800000\r");
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

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_12()
{
	ATS_TRACE
	SocketError err;
	for(size_t i = 0; i < m_msg.size(); ++i)
	{
		ats_logf(ATSLOG_INFO, "     remove mid %d from calamp db - table message_table", m_msg[i].mid);
		m_dbreader->dbrecordremove(m_msg[i].mid);
		{
			send_msg("localhost", 41009, &err, "unset_work key=Message_%d\r",m_msg[i].msg_db_id);
		}
	}
	m_msg.resize(1);
	SET_NEXT_STATE(1);
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_13()
{
	ATS_TRACE

	if(m_dbreader->dbqueryhighestprimid_from_packetizerdb((size_t &)m_msgInfo.mid, (size_t &)m_msgInfo.pri))
	{
		if((uint)m_msgInfo.pri <= m_IridiumPriorityLevel)
		{

			if(m_dbreader->dbquery_from_packetizerdb(m_msgInfo.mid,m_msgInfo.sm))
			{
				SET_NEXT_STATE(7);
				return;
			}
		}
	}

	clearMsgInfo(m_msgInfo);
	SET_NEXT_STATE(1);
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_14()
{
	ATS_TRACE
	if(m_iridium_util->isNetworkAvailable() || m_FakeIridium)
	{
		SET_NEXT_STATE(15);
		return;
	}
	
	ats_logf(ATSLOG_DEBUG, "%s,%d: Iridium network is unavailable.", __FILE__, __LINE__);
	clearMsgInfo(m_msgInfo);
	SET_NEXT_STATE(1);
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_15()
{
	ATS_TRACE
	m_msgInfo.sm.set("cell_imei",m_imei);
	PacketizerIridiumMessage msg(m_msgInfo.sm);
	msg.mid(m_msgInfo.mid);
	m_state15_iridium_data.clear();
	msg.packetize(m_state15_iridium_data);

	if(m_state15_iridium_data.size() == 0)
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: Failed to packetize message.", __FILE__, __LINE__);
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

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_16()
{
	ATS_TRACE
	if(m_state16_message_sent)
	{
		SET_NEXT_STATE(11);
		return;
	}
	ats_logf(ATSLOG_ERROR, "%s,%d: Message failed to send. Check iridium-monitor log for error.", __FILE__, __LINE__);
	clearMsgInfo(m_msgInfo);
	SET_NEXT_STATE(1);
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_17()
{
	ATS_TRACE
  static int logCount = 0;
  if (m_ForceIridium != "Never")
  {
		db_monitor::ConfigDB db;
		m_ForceIridium = db.GetValue("packetizer-cams", "ForceIridium", "Never");  // test iridium without checking cellular
		
		if (m_ForceIridium == "On")
		{
			if (++logCount % 100 == 0)
				ats_logf(ATSLOG_ERROR, "%s,%d: ForceIridium is ON - sending via Iridium.", __FILE__, __LINE__);

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

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_18()
{
	ATS_TRACE
	m_cell_sender->sendSingleMessage(&m_msgInfo, m_messagetypes);
	SET_NEXT_STATE(19);
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_19()
{
	ATS_TRACE
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
			ats_logf(ATSLOG_ERROR, "%s, %d: Unknown ack error received: %d.", __FILE__, __LINE__, res);
	}

	SET_NEXT_STATE(8);
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_20()
{
	ATS_TRACE
	if(m_UseCompression && (m_max_backlog > 1))
	{
		SET_NEXT_STATE(10);
		return;
	}
	SET_NEXT_STATE(2);
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_21()
{
	ATS_TRACE
	m_thread_finished = false;

	if(!m_dbreader->dbquery_from_packetizerdb(m_msgInfo.mid,m_msgInfo.sm))
	{
		ats_logf(ATSLOG_DEBUG, "%s, %d: Cannot read priority 1 message from database mid %d", __FILE__,__LINE__, m_msgInfo.mid);
		SET_NEXT_STATE(1);
		clearMsgInfo(m_msgInfo);
		return;
	}

	struct cell_thread_data cd;
	cd.cs = m_cell_sender;
	cd.mi = &m_msgInfo;
	cd.msg_types = &m_messagetypes;
	m_msgInfo.sm.set("cell_imei",m_imei);
	pthread_t cellThread, iridiumThread;
	int err = pthread_create(&cellThread, 0, priorityOneCellSend, &cd);
	if(err)
	{
		ats_logf(ATSLOG_ERROR, "%s, %d: Error %d when creating cellThread for priority 1 messages.", __FILE__, __LINE__, err);
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
			ats_logf(ATSLOG_ERROR, "%s, %d: Error %d when creating iridiumThread for priority1 messages.", __FILE__, __LINE__, err);
		}
	}
	pthread_join(cellThread, NULL);
	SET_NEXT_STATE(22);
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_22()
{
	ATS_TRACE
	m_dbreader->dbrecordremove(m_msgInfo.mid);
	{
		SocketError err;
		send_msg("localhost", 41009, &err, "unset_work key=Message_%d\r",m_msgInfo.sm.get_int("mtid"));
	}
	SET_NEXT_STATE(1);
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_23()
{
	ATS_TRACE
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

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_24()
{
	ATS_TRACE
	if (m_FakeIridium)
		SendFakeIridium(m_state15_iridium_data);
	else
		m_state16_message_sent = m_iridium_util->sendMessage(m_state15_iridium_data);
	SET_NEXT_STATE(16);
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::state_25()
{
	ATS_TRACE
	if (m_FakeIridium)
	{
		SendFakeIridium(m_state15_iridium_data);
		m_state16_message_sent = true;
	}
	else
		m_state16_message_sent = m_iridium_util->sendMessageWDataLimit(m_state15_iridium_data);
	SET_NEXT_STATE(16);
}


//---------------------------------------------------------------------------------------
// compress
bool PacketizerCamsStateMachine::compress(struct send_message& outputMsg, const std::vector<struct send_message>& inputMsgs, int p_sequence_num)
{
	ats_logf(ATSLOG_INFO, "%s,%d: Compressing %d messages.", __FILE__,__LINE__, inputMsgs.size());
	
	if(inputMsgs.empty())
	{
		return false;
	}
	
	// concatenate the inputMsgs into one long byte array
	QByteArray msg;
	{
		for(size_t i = 0; i < inputMsgs.size(); ++i)
		{
			const int len = (inputMsgs[i].data.size() & 0xffff);
			msg.append(char((len >> 8) & 0xff));
			msg.append(char(len & 0xff));
			msg.append(&(inputMsgs[i].data[0]), inputMsgs[i].data.size());
		}
	}

	{
		std::ostringstream outstr;
		outstr << "Uncompressed[" << msg.length() << "]:";

		for(uint i = 0; i < msg.length(); ++i)
		{
			outstr << std::hex <<	std::setfill('0') << std::setw(2) << std::uppercase << (msg[i] & 0xff) << ',';
		}

		ats_logf(ATSLOG_INFO, "%s,%d : %s", __FILE__, __LINE__, outstr.str().c_str());
	}
  // zlib compress the long byte array
  
	uLongf len = compressBound(msg.length());
	char* z = new char[len];
	const int ret = ::compress((Bytef*)z, &len, (Bytef*)(msg.data()), msg.length());
	ats_logf(ATSLOG_INFO, "%s, %d: Compressing %d bytes to %ld bytes.", __FILE__, __LINE__, msg.size(), len);

	if(Z_OK != ret)
	{
		delete[] z;
		ats_logf(ATSLOG_ERROR, "%s: Error compressing messages: Error %d for %zu mid compression", __FUNCTION__, ret, inputMsgs.size());
		return false;
	}

	// put the compressed data into a calamps message
	
	CalAmpsCompressedMessage m(CalAmpsMessage::readImei(),ats::String(z,len));

	delete[] z;
	m.setSequenceNumber(p_sequence_num);

	outputMsg.seq = p_sequence_num;
	outputMsg.mid = -1;
	const ats::String& s = m.getMessage();
	outputMsg.data.insert(outputMsg.data.end(), s.begin(), s.end());

	return true;
}

//---------------------------------------------------------------------------------------
void* PacketizerCamsStateMachine::priorityOneCellSend(void* p_data)
{
	struct cell_thread_data* cd = (struct cell_thread_data*) p_data;
	int res = false;
	while(!isThreadFinished())
	{
		res = cd->cs->sendSingleMessage(cd->mi, *(cd->msg_types));
		if(!res)
		{
			ats_logf(ATSLOG_ERROR, "%s, %d: Error sending priority one message", __FILE__, __LINE__);
		}
		else
		{
			int ack = cd->cs->waitforack();
			switch(ack)
			{
				case -2://general error
				case -1://ack error
				case 0://timeout
					break;
				case 1:
					pthread_mutex_lock(&m_priorityOneMutex);
					m_thread_finished = true;
					pthread_mutex_unlock(&m_priorityOneMutex);
					return (void*)0;
				default:
					ats_logf(ATSLOG_ERROR, "%s, %d: Unknown ack error received: %d.", __FILE__, __LINE__, res);
			}
		}
		sleep(1);
	}
	return (void*)1;
}

//---------------------------------------------------------------------------------------
void* PacketizerCamsStateMachine::priorityOneIridiumSend(void* p_data)
{
	struct iridium_thread_data* id = (struct iridium_thread_data*)p_data;

	PacketizerIridiumMessage msg(id->mi->sm);
	msg.mid(id->mi->mid);
	std::vector<char> data;
	msg.packetize(data);
	if(data.size() == 0)
	{
		ats_logf(ATSLOG_ERROR, "%s,%d: Failed to packetize message.", __FILE__, __LINE__);
		return (void*)1;
	}
	IridiumUtil::ComputeCheckSum(data);
	ats_logf(ATSLOG_DEBUG, "%s: Calculated checksum. connecting to iridium-monitor.", __PRETTY_FUNCTION__);
	IridiumUtil iu;
	while(!isThreadFinished())
	{
		if(iu.isNetworkAvailable())
		{
			ats_logf(ATSLOG_DEBUG, "%s,%d: Sending Iridium message.", __FILE__, __LINE__);
			if(iu.sendMessage(data))
			{
				pthread_mutex_lock(&m_priorityOneMutex);
				m_thread_finished = true;
				pthread_mutex_unlock(&m_priorityOneMutex);
				return (void*)0;
			}
			ats_logf(ATSLOG_ERROR, "%s,%d: Failed to Send message.", __FILE__, __LINE__);
		}
		sleep(1);
		ats_logf(ATSLOG_DEBUG, "%s,%d: Iridium network not available.", __FILE__, __LINE__);
	}

	return (void*)1;
}

//---------------------------------------------------------------------------------------
void PacketizerCamsStateMachine::	SendFakeIridium(std::vector<char> data)
{
	ats_logf(ATSLOG_ERROR, "%s,%d: Sending Fake Iridium", __FILE__, __LINE__);
  char buf[8];
  sprintf(buf, "%d", msgType++);
  std::string strFname = "/home/root/" + m_IridiumIMEI + ".sbd." + buf;
  
  ofstream myFile (strFname.c_str(), ios::out | ios::binary);
  myFile.write(reinterpret_cast<const char*>(&data[0]), data.size()*sizeof(char));
  myFile.close();
  char ibuf[256];
  sprintf(ibuf, "/usr/bin/send-iridium.sh %s %s", m_IridiumIMEI.c_str(), strFname.c_str());
  system(ibuf); 
}


