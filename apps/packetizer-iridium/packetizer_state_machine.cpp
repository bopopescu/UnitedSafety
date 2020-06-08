//--------------------------------------------------------------------------------------------------------
//
// PacketizerSm: handles sending of messages to the Iridium.
//
// Messages are sent when they are priority 1 messages (always) or when
// the system has lost cellular connection.
//
// Messages are deleted after being sent
//
// Startup: load database reader and sender -> 0
// 0: look for priority 1 records in database -> 4 OR look for fallback -> 1
// 1: If in fallback -> 2 If not sleep(1) -> 0
// 2: Delete all records sent by other packetizers -> 3
// 3: Send first record in database -> 4 If no records sleep(1) -> 0
// 4: Send record - 1) Wait for Iridium network -> 5
// 5: Send record - 2) Send message to 9602 -> 6
// 6: Send record - 3) Wait for ack from 9602 -> 7
// 7: Delete record just sent -> 0

#include <vector>
#include <iomanip>
#include <sstream>
#include <iterator>

#include <stdlib.h>

#include "atslogger.h"
#include "db-monitor.h"
#include "ConfigDB.h"
#include "Iridium.h"

#include "packetizer.h"
#include "packetizerIridiumDB.h"

#include "packetizer_state_machine.h"
#include "midDB.h"

#define STATE_MACHINE_CLASS PacketizerSm
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(PacketizerSm)
extern ATSLogger g_log;

//--------------------------------------------------------------------------------
PacketizerSm::PacketizerSm(MyData& p_data) : StateMachine( p_data),sequence_num(0)
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	dbreader = new PacketizerIridiumDB(md, "iridium_db", "/mnt/update/database/iridium.db");
	m_sender = new PacketizerIridiumSender(md);

	dbreader->start();
	m_sender->start();
	db_monitor::ConfigDB db;
	m_imei = db.GetValue("RedStone", "IMEI");
	set_log_level(11);	// turns off state machine logging
	SET_NEXT_STATE(0);
}


//--------------------------------------------------------------------------------
PacketizerSm::~PacketizerSm()
{
	delete dbreader;
	delete m_sender;
	delete m_msg;
}


//--------------------------------------------------------------------------------
bool PacketizerSm::readfromPacketizerDB(int mid)
{
	ats::StringMap sm;

	if(!dbreader->dbquery_from_packetizerdb(mid, sm))
	{
		ats_logf(&g_log, "%s,%d: read from packetizer db fail where mid = %d", __FILE__, __LINE__, mid);
		return false;
	}
	sm.set("cell_imei", m_imei);

	m_msg = new PacketizerIridiumMessage(sm);
	m_msg->mid(mid);
	m_msg->msg_type(sm.get_int("mtid"));
	ats_logf(&g_log, "%s,%d: read message mtid = %d", __FILE__, __LINE__, m_msg->msg_type());

	return true;
}


//--------------------------------------------------------------------------------
// 0: look for priority 1 records in database -> 4 OR look for fallback -> 1
//
void PacketizerSm::state_0()
{
	int mid = dbreader->Query_SelectPriorityOneMessage();

	if(mid > 0)
	{
		ats_logf(&g_log, "%s,%d: found Priority 1 mid %d", __FILE__, __LINE__, mid);

		if(!readfromPacketizerDB(mid))
		{
			dbreader->dbrecordremove(mid);
			ats_logf(&g_log, "Remove one corrupt message from packetizer_db mid = %d in state 0", mid);
			SET_NEXT_STATE(1);
		}
		else
		{
			SET_NEXT_STATE(4);	// send the message
		}
		return;
	}
	SET_NEXT_STATE(1);	// look for other messages
}

//-----------------------------------------------------------------------------------------------
//
// State 1: watch for fallback
void PacketizerSm::state_1()
{
	if (m_RedStone.IridiumEnabled())
	{
		SET_NEXT_STATE(2);	// delete messages already sent by other packetizers
	}
	else
	{
		sleep(1);
		SET_NEXT_STATE(0);
	}
}

//--------------------------------------------------------------------------------
// State 2: Delete all records sent by other packetizers -> 3
//
void PacketizerSm::state_2()
{
	midDB MidDB;
	dbreader->DeleteSentMIDs(MidDB.GetLatestPacketizerMID());	// delete any records in the DB that have been sent by other packetizers.

	SET_NEXT_STATE(3);
}

//--------------------------------------------------------------------------------
// 3: Send first record in database -> 4 If no records sleep(1) -> 0
void PacketizerSm::state_3()
{
	int mid = dbreader->dbqueryoldestmid_from_packetizerdb();
		
	if(mid > 0)
	{
		ats_logf(&g_log, "%s,%d: found oldest mid %d", __FILE__, __LINE__, mid);
			
		if(!readfromPacketizerDB(mid))
		{
			dbreader->dbrecordremove(mid);
			ats_logf(&g_log, "Remove one corrupt message from packetizer_db mid = %d in state 0", mid);
			SET_NEXT_STATE(0);	// start over again
			sleep(1);
			return;
		}
		SET_NEXT_STATE(4);	// send the message
	}
	else
	{
		sleep(1);	// no messages to send - wait a bit for something to happen
		SET_NEXT_STATE(0);	// start over again
	}
}

//--------------------------------------------------------------------------------------------------------
// 4: Send record	- 1) Wait for Iridium network -> 5
//
void PacketizerSm::state_4()
{
	ats_logf(&g_log, "%s,%d: ENTER STATE 4 - waiting for Iridium network", __FILE__, __LINE__);
	m_sender->waitForNetworkAvailable();
	SET_NEXT_STATE(5);
}

//--------------------------------------------------------------------------------------------------------
// 5: Send record	- 2) Send message to 9602 -> 6
//
void PacketizerSm::state_5()
{
	ats_logf(&g_log, "%s,%d: ENTER STATE 5", __FILE__, __LINE__);
	std::vector<char> data;
	m_msg->packetize(data);
	
	if(data.size() == 0)
	{
		ats_logf(&g_log, "%s,%d: Failed to packetize message.", __FILE__, __LINE__);
		SET_NEXT_STATE(7);	// if we cant packetize we delete the message - all we can do at this point
		return;
	}

	IRIDIUM::ComputeCheckSum(data);
	
	if(!m_sender->sendMessage(data))
	{
		ats_logf(&g_log, "%s,%d: Failed to Send message.", __FILE__, __LINE__);
		delete m_msg;
		sleep(1);
		SET_NEXT_STATE(0);
		return;
	}
	SET_NEXT_STATE(6);
}

//--------------------------------------------------------------------------------------------------------
// 6: Send record	- 3) Wait for ack from 9602 -> 7
//
void PacketizerSm::state_6()
{
	ats_logf(&g_log, "%s,%d: ENTER STATE 6", __FILE__, __LINE__);
	
	if (m_sender->waitforack())
	{
		SET_NEXT_STATE(7);
	}
	else
	{
		ats_logf(&g_log, "%s,%d: %s", __FILE__, __LINE__, m_sender->errorStr().c_str());
		SET_NEXT_STATE(0);
	}
}

//--------------------------------------------------------------------------------------------------------
// 7: Delete sent record -> 0
//
void PacketizerSm::state_7()
{
	midDB MidDB;
	ats_logf(&g_log, "%s,%d: ENTER STATE 7", __FILE__, __LINE__);
	dbreader->dbrecordremove(m_msg->mid());
	SocketError err;

	send_msg("localhost", 41009, &err, "unset_work key=Message_%d\r", m_msg->msg_type());

	ats_logf(&g_log, "%s, %d: Remove message %d from packetizer_db", __FILE__,__LINE__, m_msg->mid());
	MidDB.SetLatestIridiumMID(m_msg->mid() );
	delete m_msg;

	//blink the led 
	send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink kick name=packetizer-iridium_sent led=sat,act	script=\"0,100000;1,100000\" priority=9\r");
	usleep(500000);
	send_redstone_ud_msg("i2c-gpio-monitor", 0, "blink del packetizer-iridium_sent\r");
	SET_NEXT_STATE(0);
}
