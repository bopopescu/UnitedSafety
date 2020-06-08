#include <iostream>
#include <iomanip>
#include <stdlib.h>

#include "atslogger.h"
#include "packetizer.h"
#include "Message.h"
#include "StateMachine.h"

#define STATE_MACHINE_CLASS GemaltoStateMachine
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(GemaltoStateMachine)

GemaltoStateMachine::GemaltoStateMachine(MyData& p_data) : StateMachine(p_data), sequence_num(0)
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	m_dbreader = new DB(md, TRULINK_DB_NAME, TRULINK_DB_FILE);  
	m_sender = new Sender(md);

	m_dbreader->start();

	SET_NEXT_STATE(0);
}

GemaltoStateMachine::~GemaltoStateMachine()
{
	delete m_dbreader;
	delete m_sender;
}


bool GemaltoStateMachine::readFromDB(int mid)
{
	ats::StringMap sm;
	std::vector<char> data;
	if(!m_dbreader->dbquery_from_packetizerdb(mid,sm))
	{
		ats_logf(ATSLOG(0), "%s, %d: Read from dashboard_db failed where mid=%d.", __FILE__, __LINE__, mid);
		return false;
	}
	
	Message msg(sm);
	msg.packetize(data);
	m_msg.data = data;
	m_msg.mid = mid;
	m_msg.msg_db_id = sm.get_int("mtid");
	return true;
}

void GemaltoStateMachine::state_0()
{
	waitForNextMessage();
	SET_NEXT_STATE(1);
}

void GemaltoStateMachine::state_1()
{
	for(;;)
	{
		if(m_sender->connect())
		{
			SET_NEXT_STATE(2);
			return;
		}
		sleep(1);
	}
}

void GemaltoStateMachine::state_2()
{
	if(m_sender->sendMessage(m_msg))
	{
		/* -- Remove comments to see output stream in log files.
		std::stringstream ss;
		for( std::vector<char>::const_iterator i = m_msg.data.begin(); i !=  m_msg.data.end(); ++i )
		{
			const char c = *i;
			ss << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << (c & 0xff) << ',';
		}
		ss<<"\n";
		const ats::String& s = "GemaltoStateMachine send msg " + ss.str();
		ats_logf(ATSLOG(0), "%s,%d:%s",__FILE__,__LINE__, s.c_str());
		*/
		SET_NEXT_STATE(3);
		return;
	}
	m_sender->disconnect();
	m_redstone.FailedToSend();
	SET_NEXT_STATE(1);
}

void GemaltoStateMachine::state_3()
{
	m_dbreader->dbrecordremove(m_msg.mid);
	m_mDB.SetLatestPacketizerMID(m_msg.msg_db_id);
	m_redstone.LastSendFailed(false);
	ats_logf(ATSLOG(0), "%s,%d: GemaltoStateMachine remove message %dfrom dashboard_db", __FILE__, __LINE__, m_msg.mid);
	SET_NEXT_STATE(4);
}

void GemaltoStateMachine::state_4()
{
	waitForNextMessage();
	SET_NEXT_STATE(2);
}


void GemaltoStateMachine::waitForNextMessage()
{
	for(;;)
	{
		int mid = m_dbreader->dbqueryoldestmid_from_packetizerdb();
		if(mid)
		{
			if(!readFromDB(mid))
			{
				m_dbreader->dbrecordremove(mid);
				ats_logf(ATSLOG(0), "%s, %d, Remove one corrupt message from dashboard_db mid=%d in state 0", __FILE__, __LINE__, mid);
				sleep(1);
				continue;
			}
			return;
		}
		sleep(1);
	}
}

