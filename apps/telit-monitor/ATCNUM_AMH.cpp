#include "socket_interface.h"
#include "atslogger.h"

#include "ATCNUM_AMH.h"

void ATCNUM_AMH::on_message(MyData& p_md, const ats::String& p_cmd, const ats::String& p_msg)
{

	if("+CNUM" != p_cmd)
	{
		return;
	}

	const ats::String& response = p_msg.substr(p_cmd.length());

	if(response.empty())
	{
		return;
	}

	const size_t i = response.find(',');

	if(i != ats::String::npos)
	{
		const ats::String& s = response.substr(i+2);
		const size_t i = s.find("\"");

		if(i != ats::String::npos)
		{
			const ats::String& number = s.substr(0,i);
			send_trulink_ud_msg("admin-client-cmd", 0, "atcmd cellpnumber %s\r", number.c_str());
			ats_logf(ATSLOG_DEBUG, "atcmd phone number: %s", number.c_str());
		}

	}

}
