#include "ats-string.h"
#include "atslogger.h"

#include "telit-monitor.h"
#include "ATCREG_AMH.h"


void ATCREG_AMH::on_message(MyData& p_md, const ats::String& p_cmd, const ats::String& p_msg)
{

	if(0 != p_msg.find("+CREG:"))
	{
		return;
	}

	const ats::String& response = p_msg;
	const size_t i = response.find(' ');

	if(ats::String::npos == i)
	{
		return;
	}

	const ats::String& s = response.substr(i+1);
	ats::StringList sl;
	ats::split(sl, s, ",");

	if(sl.size() < 2)
	{
		return;
	}

	static int prev_reg_status = -1;
	p_md.lock_data();
	g_creg_mode = atoi(sl[0].c_str());
	p_md.unlock_data();
	const int reg_status = atoi(sl[1].c_str());

	if(reg_status != prev_reg_status)
	{
		prev_reg_status = reg_status;
		ats_logf(ATSLOG_DEBUG, "CREG: 0x%X", reg_status);
	}

	static ats::String prev_Lac;
	const ats::String& Lac = (sl.size() > 2) ? sl[2] : ats::g_empty;

	if(prev_Lac != Lac)
	{
		prev_Lac = Lac;
		ats_logf(ATSLOG_DEBUG, "Lac=%s", Lac.c_str());
	}

	static ats::String prev_Ci;
	const ats::String& Ci = (sl.size() > 3) ? sl[3] : ats::g_empty;

	if(prev_Ci != Ci)
	{
		prev_Ci = Ci;
		ats_logf(ATSLOG_DEBUG, "Ci=%s", Ci.c_str());
	}

	if(sl.size() > 4)
	{
		const int AcT = atoi(sl[4].c_str());
		p_md.lock_data();

		if(g_AcT != AcT)
		{
			g_AcT = AcT;
			p_md.unlock_data();
			ats_logf(ATSLOG_DEBUG, "AcT=%d", AcT);
		}
		else
		{
			p_md.unlock_data();
		}

	}

}
