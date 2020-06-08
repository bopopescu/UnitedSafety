
#include "atslogger.h"
#include "ConfigDB.h"
#include "IridiumUtil.h"

#include "packetizer.h"
#include "TruLink.h"
#include "packetizerIridiumMessage.h"
#include "TruLinkStateMachine.h"
#include "TLPMessage.h"

pthread_mutex_t TruLinkStateMachine::m_priorityOneMutex;
bool TruLinkStateMachine::m_thread_finished = false;

#define STATE_MACHINE_CLASS TruLinkStateMachine
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(TruLinkStateMachine)

TruLinkStateMachine::TruLinkStateMachine(MyData& p_data) : StateMachine(p_data), sequence_num(0)
{
	ats_logf(ATSLOG(0), "%s,%d: CONSTRUCTOR", __FILE__, __LINE__);
	MyData& md = dynamic_cast<MyData&>(my_data());
	m_dbreader = new TruLinkDB(md, "trulink_db", "/mnt/update/database/trulink.db");
	m_cell_sender = new PacketizerCellSender(md);
	m_sequence_num = 0;
	m_dbreader->start();

	SET_NEXT_STATE(0);
}


//---------------------------------------------------------------------------------------
void TruLinkStateMachine::get_message_backlog(int p_limit)
{
	std::vector<int> mid;
	m_dbreader->dbselectbacklog(mid, (p_limit >= 1) ? p_limit : 1);

	if(mid.empty())
	{
		m_OutputMsg.clear();
		return;
	}

	m_OutputMsg.resize(mid.size());
	m_CompressedSeqNum = get_sequence_num(); // this allows for the message log to show 1 (2,3,4,5) 6 (7,8,9,10)...
	inc_sequence_num();                      // where 1 and 6 are the compressed message, the rest are real messages

	for(size_t i = 0; i < m_OutputMsg.size(); ++i)
	{
		if(!BuildTLPEventBuffer(mid[i], m_OutputMsg[i]))
		{
			m_dbreader->dbrecordremove(mid[i]);
			ats_logf(ATSLOG(0), "%s: Removing corrupt trulink db: mid=%d", __FUNCTION__, mid[i]);
			mid[i] = -1;
			continue;
		}
	}
}

void TruLinkStateMachine::state_0()
{
	set_log_level(11);  // turns off state machine logging

	db_monitor::ConfigDB db;
	//Initialize state machine variables
	m_keepalive_timeout_seconds = db.GetInt("packetizer-trulink", "m_keepalive", 30);
	m_timeout = db.GetInt("packetizer-trulink", "timeout", 10);
	m_max_backlog = db.GetInt("packetizer-trulink", "m_max_backlog", 30);
	m_retry_limit = db.GetInt("packetizer-trulink", "retry_limit", 1);
	m_iridium_timeout_seconds = db.GetInt("packetizer-trulink", "iridium_timeout", 60);
	m_UseCompression = db.GetInt("packetizer-trulink", "UseCompression", 0);
	m_IridiumPriorityLevel = db.GetInt("packetizer-trulink", "IridiumPriorityLevel", 10);
	m_CellFailModeEnable = db.GetBool("packetizer-trulink", "CellFailModeEnable", "Off");
	m_IridiumEnable = db.GetBool("packetizer-trulink", "IridiumEnable", "Off");
	m_ForceIridium = db.GetValue("packetizer-trulink", "ForceIridium", "Never");  // test iridium without checking cellular
	
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
	m_PriorityIridiumDataLimit = db.GetInt("packetizer-trulink", "IridiumDataLimitPriority", 1);
	m_imei = db.GetValue("RedStone", "IMEI");
	m_CellFailMode = false;
	m_keepAliveTimer.SetTime();
	m_cell_sender->start();
	m_dbreader->dbload_messagetypes(m_messagetypes);

	SET_NEXT_STATE(1);
}
// Look for priority 1 messages
void TruLinkStateMachine::state_1()
{
	int mid = m_dbreader->dbquery_SelectPriorityOneMessage();
	
	if(mid > 0)
	{
		inc_sequence_num();
		m_RecordData.mid = mid;
		m_RecordData.pri = 1;
		m_RecordData.seq = get_sequence_num();
		ats_logf(ATSLOG(0), "%s,%d,: found Priority 1 mid %d", __FILE__, __LINE__, mid);
		SET_NEXT_STATE(21);  // found one - send it
		return;
	}
	
	if(m_keepalive_timeout_seconds && (m_keepAliveTimer.DiffTime() >= m_keepalive_timeout_seconds))
	{
		//send keep alive
		m_cell_sender->keepAlive();
		m_keepAliveTimer.SetTime();
	}
	
	SET_NEXT_STATE(23); // look for current messages
}

void TruLinkStateMachine::state_2()
{
	usleep(500000);
	SET_NEXT_STATE(1);
}
//-----------------------------------------------------------------------------------------
// compress the backlogged messages (actually combines all the messages into a single
// packet with one header for sequence and IMEI).
//
void TruLinkStateMachine::state_3()
{
  if (m_ForceIridium != "On" )  // don't even try to send compressed if Forcing iridium.
	{
		TLPMessage msg;
		
		if(compress(msg))
		{
			m_cell_sender->sendMessage(msg.GetMessage());
			SET_NEXT_STATE(5);
			return;
		}
	}

	SET_NEXT_STATE(9);
}

void TruLinkStateMachine::state_4()
{
	ats_logf(ATSLOG(0), "%s,%d: Sending message with mtid:%d", __FILE__, __LINE__, m_RecordData.sm.get_int("mtid"));
	//m_cell_sender->sendSingleMessage(&m_msgInfo, m_messagetypes);
	SET_NEXT_STATE(6);
}

void TruLinkStateMachine::state_5()
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

void TruLinkStateMachine::state_6()
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
	m_cell_retries++;
	if(m_cell_retries >= m_retry_limit)
	{
		m_CellFailMode = true;
		SET_NEXT_STATE(8);
		return;
	}
	SET_NEXT_STATE(4);
}

void TruLinkStateMachine::state_7()
{
	if(m_IridiumEnable)
	{
		SET_NEXT_STATE(14);
		return;
	}
	clearMsgInfo(m_RecordData);
	SET_NEXT_STATE(1);
}

// Check for Iridium priority message
void TruLinkStateMachine::state_8()
{
	ats_logf(ATSLOG(0), "State 8: %s, %d: Message Priority = %d", __FILE__, __LINE__, m_RecordData.pri);
	if((uint)m_RecordData.pri <= m_IridiumPriorityLevel)
	{
		SET_NEXT_STATE(7);  // send via iridium
		return;
	}
	clearMsgInfo(m_RecordData);
	SET_NEXT_STATE(9);
}
// check for Priority 1 message via iridium
void TruLinkStateMachine::state_9()
{
	int mid = m_dbreader->dbquery_SelectPriorityOneMessage();
	if(mid > 0)
	{
		inc_sequence_num();
		m_RecordData.mid = mid;
		m_RecordData.pri = 1;
		m_RecordData.seq = get_sequence_num();
		ats_logf(ATSLOG(0), "%s,%d,: found Priority 1 mid %d", __FILE__, __LINE__, mid);
		SET_NEXT_STATE(21);
		return;
	}
	SET_NEXT_STATE(13);

}

void TruLinkStateMachine::state_10()
{
	get_message_backlog(m_max_backlog);
	
	if(m_OutputMsg.empty())
	{
		SET_NEXT_STATE(1);
		usleep(500000);
		return;
	}
	SET_NEXT_STATE(3);
}

void TruLinkStateMachine::state_11()
{
	ats_logf(ATSLOG(0), "%s,%d: Acknowledgement received for mtid %d.", __FILE__,__LINE__, m_RecordData.sm.get_int("mtid"));
	// Have the cell and activity light blink once to indicate a send.
	send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=packetizer-trulink_sending led=cell,act script=\"0,100000;1,1000000\" priority=10 timeout=100000\r");
	if(m_RecordData.pri == CAMS_CURRENT_MSG_PRI)
	{
		m_dbreader->dbrecordremove(0, "current_table");
	}
	else
	{
		m_dbreader->dbrecordremove(m_RecordData.mid);
	}
	{
		SocketError err;
		send_msg("localhost", 41009, &err, "unset_work key=Message_%d\r",m_RecordData.sm.get_int("mtid"));
	}
	clearMsgInfo(m_RecordData);

	if (m_timeout == 0)  // if we are not waiting for acks we sleep 1 second between sends ATSFIXME: Needs to be added to the design document in SVN
		sleep(1);
	SET_NEXT_STATE(1);
}

void TruLinkStateMachine::state_12()
{
	SocketError err;
	for(size_t i = 0; i < m_OutputMsg.size(); ++i)
	{
		ats_logf(ATSLOG(0), "Removing mid %d from calamp db", m_OutputMsg[i].mid);
		m_dbreader->dbrecordremove(m_OutputMsg[i].mid);
		send_msg("localhost", 41009, &err, "unset_work key=Message_%d\r",m_OutputMsg[i].msg_db_id);  // remove message from power-monitor keep-alive
	}
	m_OutputMsg.resize(1);
	SET_NEXT_STATE(1);
}

// check for Iridium Priority message
void TruLinkStateMachine::state_13()
{
	if(m_dbreader->dbqueryhighestprimid_from_packetizerdb(m_RecordData.mid, m_RecordData.pri))
	{
		if((uint)m_RecordData.pri <= m_IridiumPriorityLevel)
		{
			if(m_dbreader->GetRecord(m_RecordData.mid, m_RecordData.sm))
			{
				SET_NEXT_STATE(7);
				return;
			}
		}
	}

	clearMsgInfo(m_RecordData);
	SET_NEXT_STATE(1);
}

// state 14 - check for iridium availability (actual network availability)
//  15 if it is
//  1 if it isn't
void TruLinkStateMachine::state_14()
{
	if(m_iridium_util->isNetworkAvailable())
	{
		SET_NEXT_STATE(15);
		return;
	}
	
	ats_logf(ATSLOG(0), "%s,%d: Iridium network is unavailable.", __FILE__, __LINE__);
	clearMsgInfo(m_RecordData);
	SET_NEXT_STATE(1);
}
// state 15 - Iridium is available
void TruLinkStateMachine::state_15()
{
	m_RecordData.sm.set("cell_imei",m_imei);
	m_OutputMsg.resize(1);
	m_CompressedSeqNum =  m_RecordData.seq;

	if(!BuildTLPEventBuffer(m_RecordData.mid, m_OutputMsg[0]))
	{
		m_dbreader->dbrecordremove(m_RecordData.mid);
		ats_logf(ATSLOG(0), "%s: Removing corrupt trulink db: mid=%d", __FUNCTION__, m_RecordData.mid);
		m_RecordData.mid = -1;
		SET_NEXT_STATE(1);
		return;
	}

  // now add the TLPMessage frame to the event data
	TLPMessage msg;
		
	compress(msg);

	struct trulink_thread_data cd;
	m_state15_iridium_data.clear();
	m_state15_iridium_data = msg.GetMessage();

	if(m_state15_iridium_data.size() == 0)
	{
		ats_logf(ATSLOG(0), "%s,%d: Failed to packetize message.", __FILE__, __LINE__);
		SET_NEXT_STATE(11);
		return;
	}

	IridiumUtil::ComputeCheckSum(m_state15_iridium_data);

	if(m_RecordData.pri > m_PriorityIridiumDataLimit)
	{
		SET_NEXT_STATE(25);
		return;
	}

	SET_NEXT_STATE(24);
}

void TruLinkStateMachine::state_16()
{
	if(m_state16_message_sent)
	{
		SET_NEXT_STATE(11);
		return;
	}
	ats_logf(ATSLOG(0), "%s,%d: Message failed to send. Check iridium-monitor log for error.", __FILE__, __LINE__);
	clearMsgInfo(m_RecordData);
	SET_NEXT_STATE(1);
}

void TruLinkStateMachine::state_17()
{
	m_cell_retries = 0;

	if(m_CellFailMode && m_CellFailModeEnable)  // are we sending via iridium?
	{
		SET_NEXT_STATE(18);
		return;
	}
	SET_NEXT_STATE(4);
}

void TruLinkStateMachine::state_18()
{
//	m_cell_sender->sendSingleMessage(&m_msgInfo, m_messagetypes);
	SET_NEXT_STATE(19);
}

void TruLinkStateMachine::state_19()
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

//---------------------------------------------------------------------------------------
// Send a priority 1 message
void TruLinkStateMachine::state_21()
{
	pthread_mutex_init(&m_priorityOneMutex, NULL);
	m_thread_finished = false;
	m_OutputMsg.resize(1);
	m_CompressedSeqNum =  m_RecordData.seq;

	if(!BuildTLPEventBuffer(m_RecordData.mid, m_OutputMsg[0]))
	{
		m_dbreader->dbrecordremove(m_RecordData.mid);
		ats_logf(ATSLOG(0), "%s: Removing corrupt trulink db: mid=%d", __FUNCTION__, m_RecordData.mid);
		m_RecordData.mid = -1;
		SET_NEXT_STATE(1);
		return;
	}

  // now add the TLPMessage frame to the event data
	TLPMessage msg;
		
	compress(msg);

	struct trulink_thread_data cd;
	cd.data = msg.GetMessage();
	m_RecordData.sm.set("cell_imei",m_imei);
	
	pthread_t cellThread, iridiumThread;
	
	int err = pthread_create(&cellThread, 0, priorityOneCellSend, &cd);
	
	if(err)
	{
		ats_logf(ATSLOG(0), "%s, %d: Error %d when creating cellThread for priority 1 messages.", __FILE__, __LINE__, err);
		sleep(1);
		SET_NEXT_STATE(1);
		return;
	}

	if(m_IridiumEnable)
	{
		err = pthread_create(&iridiumThread, 0, priorityOneIridiumSend, &cd);
		if(!err)
		{
			pthread_join(iridiumThread, NULL);  // wait for iridium thread to complete
		}
		else
		{
			ats_logf(ATSLOG(0), "%s, %d: Error %d when creating iridiumThread for priority1 messages.", __FILE__, __LINE__, err);
		}
	}
	pthread_join(cellThread, NULL);// wait for cell thread to complete (iridium thread will already be done by now)
	SET_NEXT_STATE(22);
}

//---------------------------------------------------------------------------------------
//Remove Priority 1 message from DB after it was sent
void TruLinkStateMachine::state_22()
{
	m_dbreader->dbrecordremove(m_RecordData.mid);
	{
		SocketError err;
		send_msg("localhost", 41009, &err, "unset_work key=Message_%d\r",m_RecordData.sm.get_int("mtid"));
	}
	SET_NEXT_STATE(1);
}

//---------------------------------------------------------------------------------------
// check for message in the 'current' table
void TruLinkStateMachine::state_23()
{
	if(m_dbreader->dbquery_curr_msg_from_packetizerdb(m_RecordData.sm))
	{
		m_OutputMsg[0].data.clear();

		m_OutputMsg[0].pri =  m_RecordData.sm.get_int("msg_priority");
		m_OutputMsg[0].msg_db_id = m_RecordData.sm.get_int("mtid");

		std::string tlpData = m_RecordData.sm.get("usr_msg_data");

		// update the sequence number
		short seq = get_sequence_num();
		inc_sequence_num();
	
		char buf[8];
		memcpy(buf, &seq, 2);
		tlpData[2] = buf[0];
		tlpData[3] = buf[1];
	
		// copy the tlp data to the p_msg buffer
		m_OutputMsg[0].data.resize(tlpData.size());
		m_OutputMsg[0].data.insert(m_OutputMsg[0].data.begin(), tlpData.begin(), tlpData.end());

		m_OutputMsg[0].seq = seq;
		m_RecordData.pri = CAMS_CURRENT_MSG_PRI;
		m_RecordData.seq = seq;
		SET_NEXT_STATE(17);
		return;
	}
	
	clearMsgInfo(m_RecordData);
	SET_NEXT_STATE(10);
}

//---------------------------------------------------------------------------------------
void TruLinkStateMachine::state_24()
{
	m_state16_message_sent = m_iridium_util->sendMessage(m_state15_iridium_data);
	SET_NEXT_STATE(16);
}

//---------------------------------------------------------------------------------------
void TruLinkStateMachine::state_25()
{
	m_state16_message_sent = m_iridium_util->sendMessageWDataLimit(m_state15_iridium_data);
	SET_NEXT_STATE(16);
}


//---------------------------------------------------------------------------------------
// compress - takes all of the messages in m_msg and adds them to the TLPMessage that is
//            passed in.
bool TruLinkStateMachine::compress(TLPMessage &tlp)
{
	ats_logf(ATSLOG(0), "%s,%d: Compressing %d messages.", __FILE__,__LINE__, m_OutputMsg.size());

	tlp.IMEI(m_imei);
	tlp.Seq(m_CompressedSeqNum);
	std::string tlpData;

	for(size_t i = 0; i < m_OutputMsg.size(); ++i)
	{
		tlp.AddEvent(m_OutputMsg[i].data);
	}
	return true;
}

//---------------------------------------------------------------------------------------
//  send a high priority message - first one of Cell or Iridium that gets done 
//	terminates the sending threads.
void* TruLinkStateMachine::priorityOneCellSend(void* p_data)
{
	struct trulink_thread_data* cd = (struct trulink_thread_data*) p_data;
	int res = false;
	
	while(!isThreadFinished())
	{
		res = cd->cs->sendMessage(cd->data);

		if(!res)
		{
			ats_logf(ATSLOG(0), "%s, %d: Error sending priority one message", __FILE__, __LINE__);
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
					ats_logf(ATSLOG(0), "%s, %d: Unknown ack error received: %d.", __FILE__, __LINE__, res);
			}
		}
		sleep(1);
	}
	return (void*)1;
}

//---------------------------------------------------------------------------------------
void* TruLinkStateMachine::priorityOneIridiumSend(void* p_data)
{
	struct trulink_thread_data* id = (struct trulink_thread_data*)p_data;

	IridiumUtil iu;
	
	while(!isThreadFinished())
	{
		if(iu.isNetworkAvailable())
		{
			ats_logf(ATSLOG(0), "%s,%d: Sending Iridium message.", __FILE__, __LINE__);
			
			if(iu.sendMessage(id->data))
			{
				pthread_mutex_lock(&m_priorityOneMutex);
				m_thread_finished = true;
				pthread_mutex_unlock(&m_priorityOneMutex);
				return (void*)0;
			}
			ats_logf(ATSLOG(0), "%s,%d: Failed to Send message.", __FILE__, __LINE__);
		}
		
		ats_logf(ATSLOG(2), "%s,%d: Iridium network not available.", __FILE__, __LINE__);
		sleep(1);
	}

	return (void*)1;
}

/*---------------------------------------------------------------------------------------
	BuildTLPEventBuffer - reads the mid record from the database and puts the contents
	of the usr_msg_data field into the data field of the p_msg.  It updates the sequence
	number of the message before copying it.
---------------------------------------------------------------------------------------*/
bool TruLinkStateMachine::BuildTLPEventBuffer(int mid, struct send_umessage& p_msg)
{
	ats::StringMap sm;

  // get the record.
	if(!m_dbreader->dbquery_from_packetizerdb(mid, sm))
	{
		ats_logf(ATSLOG(0), "%s,%d: Read from trulink db failed where mid = %d", __FILE__, __LINE__, mid);
		return false;
	}

	p_msg.data.clear();

	const int code = sm.get_int("event_type");
	p_msg.pri =  sm.get_int("msg_priority");
	p_msg.msg_db_id = sm.get_int("mtid");

	std::string tlpData = sm.get("usr_msg_data");

	// update the sequence number
	short seq = get_sequence_num();
	
	char buf[8];
	memcpy(buf, &seq, 2);
	tlpData[2] = buf[0];
	tlpData[3] = buf[1];
	
	// copy the tlp data to the p_msg buffer
	p_msg.data.resize(tlpData.size());
	p_msg.data.insert(p_msg.data.begin(), tlpData.begin(), tlpData.end());

	p_msg.seq = seq;
	p_msg.mid = mid;

	inc_sequence_num();
	return true;
}
