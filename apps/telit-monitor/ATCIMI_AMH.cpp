#include "atslogger.h"
#include "socket_interface.h"

#include "ATCIMI_AMH.h"

void ATCIMI_AMH::on_message(MyData& p_md, const ats::String& p_cmd, const ats::String& p_msg)
{
	const ats::String& response = p_msg.substr(p_cmd.length());

	if(!(response.empty()))
	{
		send_trulink_ud_msg("admin-client-cmd", 0, "atcmd cellimsi %s\r", response.c_str());
		ats_logf(ATSLOG_DEBUG, "atcmd imsi: %s", response.c_str());
	}

}
