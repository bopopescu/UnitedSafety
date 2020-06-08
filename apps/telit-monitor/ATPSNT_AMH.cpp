#include "atslogger.h"
#include "ats-string.h"

#include "telit-monitor.h"
#include "ATPSNT_AMH.h"

void ATPSNT_AMH::on_message(MyData& p_md, const ats::String& p_cmd, const ats::String& p_msg)
{
	const ats::String prefix("#PSNT: ");

	if(0 != p_msg.find(prefix))
	{
		return;
	}

	ats::StringList sl;
	ats::split(sl, p_msg.substr(prefix.length()), ",");

	if(sl.size() > 0)
	{
		const int mode = atoi(sl[0].c_str());
		p_md.lock_data();

		if(g_psnt_mode != mode)
		{
			g_psnt_mode = mode;
			p_md.unlock_data();
			const char* s;

			switch(mode)
			{
			case 0: s = "PSNT unsolicited result code disabled"; break;
			case 1: s = "PSNT unsolicited result code enabled"; break;
			default: s = "invalid mode"; break;
			}

			ats_logf(ATSLOG_DEBUG, "PSNT=%d - %s", mode, s);
		}
		else
		{
			p_md.unlock_data();
		}

	}

	if(sl.size() > 1)
	{
		const int nt = atoi(sl[1].c_str());
		p_md.lock_data();

		if(g_network_type != nt)
		{
			g_network_type = nt;
			p_md.unlock_data();
			const char* s;

			switch(nt)
			{
			case 0: s = "GPRS"; break;
			case 1: s = "EGPRS"; break;
			case 2: s = "WCDMA"; break;
			case 3: s = "HSDPA"; break;
			case 4: s = "unknown or not registered"; break;
			default: s = "invalid"; break;
			}

			ats_logf(ATSLOG_DEBUG, "NetworkType=%d - %s", nt, s);
		}
		else
		{
			p_md.unlock_data();
		}

	}

}
