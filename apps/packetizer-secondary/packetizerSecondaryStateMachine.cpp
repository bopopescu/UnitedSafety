#include <iostream>
#include <iomanip>
#include <stdlib.h>
#include "ConfigDB.h"
#include "atslogger.h"
#include "packetizer.h"
#include "packetizerSecondaryMessage.h"
#include "packetizerSecondaryStateMachine.h"
#include "UDPClient.h"

#define STATE_MACHINE_CLASS PacketizerSecondaryStateMachine
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

extern ATSLogger g_log;

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(PacketizerSecondaryStateMachine)
//---------------------------------------------------------------------------------------------------------------------
PacketizerSecondaryStateMachine::PacketizerSecondaryStateMachine(MyData& p_data) : StateMachine(p_data), sequence_num(0)
{
	MyData& md = dynamic_cast<MyData&>(my_data());

	m_pSecondarySender = NULL;
  ConnectToPrimary();
	db_monitor::ConfigDB db;
	m_RouteIP = db.GetInt("system", "RouteIP", 253);
	std::string cur_ip = GetInterfaceAddr( "eth0" );
	m_PrimaryIP = ReplaceLastOctet(cur_ip, m_RouteIP);
	m_PrimaryPort = db.GetInt("system", "PrimaryPort", 39002);
	m_IMEI = db.GetValue("RedStone", "IMEI");
	m_retry_limit = 5;
	
	m_dbreader = new PacketizerSecondaryDB(md, "secondary_db", "/mnt/update/database/secondary.db");
	m_dbreader->start();

	ats_logf(&g_log, "PacketizerSecondaryStateMachine: RouteIP %d  PrimaryPort %d, eth addr of primary %s", m_RouteIP, m_PrimaryPort, m_PrimaryIP.c_str());

	SET_NEXT_STATE(0);
}

//---------------------------------------------------------------------------------------------------------------------
PacketizerSecondaryStateMachine::~PacketizerSecondaryStateMachine()
{
	delete m_dbreader;
}


//---------------------------------------------------------------------------------------------------------------------
bool PacketizerSecondaryStateMachine::readFromSecondaryDB(int mid)
{
	ats_logf(&g_log, "%s, %d: ", __FILE__, __LINE__);
	ats::StringMap sm;
	std::vector<char> data;
	ats_logf(&g_log, "%s, %d: ", __FILE__, __LINE__);
	if(!m_dbreader->dbquery_from_packetizerdb(mid,sm))
	{
		ats_logf(&g_log, "%s, %d: Read from secondary_db failed where mid=%d.", __FILE__, __LINE__, mid);
		return false;
	}

	ats_logf(&g_log, "%s, %d: ", __FILE__, __LINE__);
	PacketizerSecondaryMessage msg(sm, m_IMEI);
	msg.packetize(data);
	m_msg.data = data;
	m_msg.mid = mid;
	m_msg.msg_db_id = sm.get_int("mtid");
	ats_logf(&g_log, "%s, %d: ", __FILE__, __LINE__);
	return true;
}

//---------------------------------------------------------------------------------------------------------------------
// state 0: wait for a message
void PacketizerSecondaryStateMachine::state_0()
{
	ats_logf(&g_log, "%s,%d:State 0", __FILE__, __LINE__);
	waitForNextMessage();
	m_cell_retries = 0;
	SET_NEXT_STATE(1);
}

//---------------------------------------------------------------------------------------------------------------------
// State 1: Send the message
void PacketizerSecondaryStateMachine::state_1()
{
	ats_logf(&g_log, "%s,%d:State 1", __FILE__, __LINE__);
	std::string str(m_msg.data.begin(), m_msg.data.end());

	m_pSecondarySender->SendMessage(str);
  SET_NEXT_STATE(2);  // wait for ack (checksum of message)

	return;
}

//---------------------------------------------------------------------------------------------------------------------
// State 2: process ACK
void PacketizerSecondaryStateMachine::state_2()
{
	const int res = m_pSecondarySender->waitforack();

	switch(res)
	{
		case -2://general error
		case -1://ack error
		case 0://timeout
			break;
		case 1:
			SET_NEXT_STATE(3);  // remove the record
			return;
		default:
			ats_logf(ATSLOG(0), "%s, %d: Unknown ack error received: %d.", __FILE__, __LINE__, res);
	}
	
	m_cell_retries++;
	
	if(m_cell_retries >= m_retry_limit)
	{
		SET_NEXT_STATE(4);  // reset the connection and try again (possible IP change???)
		return;
	}
	SET_NEXT_STATE(1);  // try to send it again.
}

//---------------------------------------------------------------------------------------------------------------------
//  Delete sent record
void PacketizerSecondaryStateMachine::state_3()
{
	m_dbreader->dbrecordremove(m_msg.mid);
	m_mDB.SetLatestPacketizerMID(m_msg.msg_db_id);
	m_redstone.LastSendFailed(false);
	ats_logf(&g_log, "remove MTID %d from secondary_db", m_msg.mid);
	SET_NEXT_STATE(0);  // wait for the next message
}

//---------------------------------------------------------------------------------------------------------------------
// reset the connection 
void PacketizerSecondaryStateMachine::state_4()
{
	ats_logf(&g_log, "%s,%d:State 4", __FILE__, __LINE__);
	ConnectToPrimary();
	SET_NEXT_STATE(0);
}


//---------------------------------------------------------------------------------------------------------------------
void PacketizerSecondaryStateMachine::waitForNextMessage()
{
	for(;;)
	{
		int mid = m_dbreader->dbqueryoldestmid_from_packetizerdb();
		
		if(mid)
		{
			ats_logf(&g_log, "%s, %d: Found oldest mid %d", __FILE__, __LINE__, mid);
			
			if(!readFromSecondaryDB(mid))
			{
				m_dbreader->dbrecordremove(mid);
				ats_logf(&g_log, "%s, %d, Remove one corrupt message from secondary_db mid=%d in state 0", __FILE__, __LINE__, mid);
				sleep(1);
				continue;
			}
			ats_logf(&g_log, "%s, %d: ", __FILE__, __LINE__);
			return;
		}
		
		sleep(1);
	}
}

void PacketizerSecondaryStateMachine::ConnectToPrimary()
{
	if (m_pSecondarySender != NULL)
		delete m_pSecondarySender;
		
	MyData& md = dynamic_cast<MyData&>(my_data());
	m_pSecondarySender = new PacketizerSecondarySender(md);
	m_pSecondarySender->start();
}

