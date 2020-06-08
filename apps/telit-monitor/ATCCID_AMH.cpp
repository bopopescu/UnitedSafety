#include "socket_interface.h"
#include "atslogger.h"
#include "ConfigDB.h"

#include "ATCCID_AMH.h"
extern std::string g_CCID;
extern db_monitor::ConfigDB db;

void ATCCID_AMH::on_message(MyData& p_md, const ats::String& p_cmd, const ats::String& p_msg)
{
	const ats::String& response = p_msg.substr(p_cmd.length());
	ats_logf(ATSLOG_DEBUG, "atcmd ccid:  0:%s ", response.c_str());

	if(!(response.empty()))
	{
		const ats::String& ccid = response.substr(2);  // becaus CCID: #### has a ':' and a ' ' taking the first 2 

		if(ccid.size() >= 20 )
		{
			ats_logf(ATSLOG_DEBUG, "atcmd ccid: %s - current ccid: %s", ccid.c_str(), g_CCID.c_str());
			
			if (g_CCID != ccid)
			{
				g_CCID = ccid;
				const ats::String& err = db.Set("Cellular", "CCID", g_CCID);
				
				g_CCID = db.GetValue("Cellular", "CCID");

				if(err.empty())
					ats_logf(ATSLOG_DEBUG, "settng new ccid to be %s", g_CCID.c_str());
				else
					ats_logf(ATSLOG_DEBUG, "ERROR::setting new ccid returned error: %s", err.c_str());
			}
		
			send_trulink_ud_msg("admin-client-cmd", 0, "atcmd cellccid %s\r", ccid.c_str());
			ats_logf(ATSLOG_DEBUG, "atcmd ccid: %s", ccid.c_str());
		}

	}

}
