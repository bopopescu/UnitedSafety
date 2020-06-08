#include "packetizerSecondaryMessage.h"
#include "ats-common.h"
#include "socket_interface.h"
#include "db-monitor.h"

PacketizerSecondaryMessage::PacketizerSecondaryMessage(ats::StringMap& sm, std::string &strIMEI)
{
    m_Msg = "msg ";
    m_Msg += GetMsgName((TRAK_MESSAGE_TYPE)sm.get_int("event_type"));
    m_Msg += " msg_priority="; m_Msg += sm.get("msg_priority");
    m_Msg += " latitude="; m_Msg += sm.get("latitude");
    m_Msg += " longitude="; m_Msg += sm.get("longitude");
    m_Msg += " event_type="; m_Msg += sm.get("event_type");
    m_Msg += " event_time=\""; m_Msg += sm.get("event_time") + "\"";
    m_Msg += " altitude="; m_Msg += sm.get("altitude");
    m_Msg += " heading="; m_Msg += sm.get("heading");
    m_Msg += " speed="; m_Msg += sm.get("speed");
    m_Msg += " hdop="; m_Msg += sm.get("hdop");
    m_Msg += " satellites="; m_Msg += sm.get("satellites");
    m_Msg += " fix_status="; m_Msg += sm.get("fix_status");
    m_Msg += " inputs="; m_Msg += sm.get("inputs");
    
    if (sm.get("mobile_id").length() == 0)
    {
	    m_Msg += " mobile_id="; m_Msg += strIMEI;
	  }
	  else
	  {
	    m_Msg += " mobile_id="; m_Msg += sm.get("mobile_id");
	  }
    m_Msg += " mobile_id_type="; m_Msg += sm.get("mobile_id_type");
    
    if (strlen(sm.get("usr_msg_data").c_str()) > 0 )
	    m_Msg += " usr_msg_data="; m_Msg += sm.get("usr_msg_data");

    m_Msg += "\r";
}


void PacketizerSecondaryMessage::packetize(std::vector<char> &data)
{
    std::copy(m_Msg.begin(), m_Msg.end(), back_inserter(data));
    return;
}

ats::String PacketizerSecondaryMessage::GetMsgName(int id)
{
	ClientSocket cs;
	init_ClientSocket(&cs);
	connect_redstone_ud_client(&cs, "db-monitor");
	db_monitor::DBMonitorContext db(cs);
	const ats::String db_name = "messages_db";
	ats::String retval("scheduled_message");
	
	const ats::String db_file = "/mnt/update/database/messages.db";
	{
		{
			const ats::String err = db_monitor::open_db(db, db_name, db_file);
			if(!err.empty())
			{
				close_ClientSocket(&cs);
				return retval;
			}
		}
		
		char mid[128];
		sprintf(mid, "SELECT name FROM message_types where mid = %d\r", id);
		ats::String myQuery = mid;
		
		const ats::String err = db_monitor::query(db, db_name, myQuery.c_str());
		
		if(!err.empty())
		{
			close_ClientSocket(&cs);
			return retval;
		}

		if(db.m_table.size() > 0)
		{
			db_monitor::ResultRow& row = db.m_table[0];
			close_ClientSocket(&cs);
			retval = row[0].c_str();
			return retval;
		}

	}
	close_ClientSocket(&cs);
	return retval;
}

