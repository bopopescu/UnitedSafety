#include "socket_interface.h"
#include "atslogger.h"

#include "ATCOPS_AMH.h"

void ATCOPS_AMH::on_message(MyData& p_md, const ats::String& p_cmd, const ats::String& p_msg)
{

	if("+COPS" != p_cmd)
	{
		return;
	}

	const size_t i = p_msg.find('"');

	if(i != ats::String::npos)
	{
		const ats::String& s = p_msg.substr(i+1);
		const size_t i = s.find('"');

		if(i != ats::String::npos)
		{
			const ats::String& cellnetwork = s.substr(0,i);
			send_trulink_ud_msg("admin-client-cmd", 0, "atcmd cellnetwork %s\r", cellnetwork.c_str());
			ats_logf(ATSLOG_DEBUG, "atcmd network: %s", cellnetwork.c_str());
		}
	}
}
