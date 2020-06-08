#include <vector>
#include <iomanip>
#include <sstream>
#include <iterator>

#include <stdlib.h>

#include "atslogger.h"
#include "packetizer.h"
#include "packetizerDB.h"
#include "packetizer_state_machine.h"

#define STATE_MACHINE_CLASS PacketizerSm
#define SET_NEXT_STATE(P_STATE) m_state_fn = &STATE_MACHINE_CLASS::state_ ## P_STATE; m_state = P_STATE

extern char *g_DBcolumnname[];

COMMON_STATE_MACHINE_START_FUNCTION_DEFINITION(PacketizerSm)

PacketizerSm::PacketizerSm(MyData& p_data) : StateMachine( p_data),sequence_num(0)
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	dbreader = new PacketizerDB(md);
	sender = new PacketizerSender(md);

	dbreader->start();
	sender->start();

	SET_NEXT_STATE(0);
}

PacketizerSm::~PacketizerSm()
{
	delete dbreader;
	delete sender;
}

bool PacketizerSm::readfromCantelDB(int mid)
{
	ats::StringMap sm;
	std::vector<char> data;
	if(!dbreader->dbquery_from_canteldb(mid, sm))
	{
		ats_logf(ATSLOG(0), "%s,%d: read from cantel db fail where mid = %d", __FILE__, __LINE__, mid);
		return false;
	}
	PacketizerMessage msg(sm);
	msg.setSequenceNum(++sequence_num);
	msg.packetizer(data);
	m_msg.seq = sequence_num;
	m_msg.data = data;
	m_msg.mid = mid;
	m_msg.msg_mid = atoi((sm.get(g_DBcolumnname[35])).c_str());
	return true;
}

void PacketizerSm::state_0()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	md.testdata_log("PacketizerSm enter state 0");
	for(;;)
	{
		int mid = dbreader->dbqueryoldestmid(CANTELDB);
		if(mid)
		{
			ats_logf(ATSLOG(0), "%s,%d: found oldest mid %d", __FILE__, __LINE__, mid);
			if(!readfromCantelDB(mid))
			{
				dbreader->dbrecordremove(mid);
				ats::String buf;
				ats_sprintf(&buf, "Remove one corrupt message from cantel_db mid = %d in state 0", mid);
				md.testdata_log(buf);
				sleep(1);
				continue;
			}
			SET_NEXT_STATE(1);
			break;
		}
		else
		{
			sleep(1);
		}
	}
}

void PacketizerSm::state_1()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	md.testdata_log("PacketizerSm enter state 1");
	if(g_dbg)
		ats_logf(ATSLOG(0), "%s,%d: ENTER STATE 1", __FILE__, __LINE__);
	sender->connect();
	SET_NEXT_STATE(2);
}

void PacketizerSm::state_2()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	md.testdata_log("PacketizerSm enter state 2");
	if(g_dbg)
		ats_logf(ATSLOG(0), "%s,%d: ENTER STATE 2", __FILE__, __LINE__);
	SET_NEXT_STATE(3);
}

void PacketizerSm::state_3()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	md.testdata_log("PacketizerSm enter state 3");
	if(g_dbg)
		ats_logf(ATSLOG(0), "%s,%d: ENTER STATE 3", __FILE__, __LINE__);
    if(sender->sendMessage(m_msg))
	{
		std::stringstream ss;
		for( std::vector<char>::const_iterator i = m_msg.data.begin(); i !=  m_msg.data.end(); ++i )
		{
			const char c = *i;
			ss << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << (c & 0xff) << ',';
		}
		ss<<"\n";
		const ats::String& s = "PacketizerSm send msg " + ss.str();
		md.testdata_log(s);
		SET_NEXT_STATE(4);
		return;
	}
	sender->disconnect();
	m_RedStone.FailedToSend();
	SET_NEXT_STATE(1);
}

void PacketizerSm::state_4()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	md.testdata_log("PacketizerSm enter state 4");
	if(g_dbg)
		ats_logf(ATSLOG(0), "%s,%d: ENTER STATE 4", __FILE__, __LINE__);
	int res = sender->waitforack(m_msg);
	switch(res) 
	{
		case -1://ack error
		case -2://genenal error
			sender->disconnect();
			m_RedStone.FailedToSend();
			SET_NEXT_STATE(1);
			break;
		case 0://timeout
			sender->reconnect();
			m_RedStone.FailedToSend();
			SET_NEXT_STATE(3);
			break;
		case 1:
			SET_NEXT_STATE(5);
			break;
	}
}

void PacketizerSm::state_5()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	md.testdata_log("PacketizerSm enter state 5");
	if(g_dbg)
		ats_logf(ATSLOG(0), "%s,%d: ENTER STATE 5", __FILE__, __LINE__);
	dbreader->dbrecordremove(m_msg.mid);
	m_mdb.SetLatestPacketizerMID(m_msg.msg_mid);	
	m_RedStone.LastSendFailed(false);
	md.testdata_log("PacketizerSm remove one message from cantel_db");
	SET_NEXT_STATE(6);
}

void PacketizerSm::state_6()
{
	MyData& md = dynamic_cast<MyData&>(my_data());
	md.testdata_log("PacketizerSm enter state 6");
	if(g_dbg)
		ats_logf(ATSLOG(0), "%s,%d: ENTER STATE 6", __FILE__, __LINE__);

	for(;;)
	{
		int mid = dbreader->dbqueryoldestmid(CANTELDB);
		if(mid)
		{
			ats_logf(ATSLOG(0), "%s,%d: found oldest mid %d", __FILE__, __LINE__, mid);
			if(!readfromCantelDB(mid))
			{
				dbreader->dbrecordremove(mid);
				ats::String buf;
				ats_sprintf(&buf, "Remove one corrupt message from cantel_db mid = %d in state 6", mid);
				md.testdata_log(buf);
				sleep(1);
				continue;
			}
			SET_NEXT_STATE(2);
			break;
		}
		else
		{
			sleep(1);
		}
	}
}

