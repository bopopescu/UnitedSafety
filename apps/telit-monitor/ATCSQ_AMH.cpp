#include "atslogger.h"
#include <RedStone_IPC.h>

#include "ATCSQ_AMH.h"
#include "telit-monitor.h"

extern REDSTONE_IPC g_RedStone;
extern ATSLogger g_log;

void ATCSQ_AMH::on_message(MyData& p_md, const ats::String& p_cmd, const ats::String& p_msg)
{

	if("+CSQ" != p_cmd)
	{
		return;
	}

	const size_t i = p_msg.find(' ');

	if(i != ats::String::npos)
	{
		const ats::String& s = p_msg.substr(i+1);
		ats_logf(ATSLOG_DEBUG, "CSQ:: string %s", s.c_str());
		const size_t i = s.find(',');

		if(i != ats::String::npos)
		{
			const int code = atoi(s.substr(0,i).c_str());
			const int minimum_dBm = -113;
			const int rssi = (g_signal_undetectable == code) ? code : (minimum_dBm + (code * 2));
			ats_logf(ATSLOG_DEBUG, "CSQ:: code %d rssi %d undetect %d", code, rssi, g_signal_undetectable);

			if(g_rssi != rssi)
			{
				g_rssi = rssi;

				if(g_signal_undetectable == g_rssi)
				{
					ats_logf(ATSLOG_DEBUG, "Signal undetectable");
				}
				else
				{
					ats_logf(ATSLOG_DEBUG, "RSSI=%d", g_rssi);
				}

				p_md.m_RedStoneData.Rssi(g_rssi);
			}

		}

	}

}
