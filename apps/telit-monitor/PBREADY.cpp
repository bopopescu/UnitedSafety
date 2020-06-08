#include "PBREADY.h"

#include <RedStone_IPC.h>
#include <atslogger.h>
extern REDSTONE_IPC g_RedStone;
extern ATSLogger g_log;

void PBREADY::on_message(MyData& p_md, const ats::String& p_cmd, const ats::String& p_msg)
{
	if("+PBREADY" != p_cmd)
	{
		return;
	}
	// PBREADY indicates SIM found
	g_RedStone.ModemState(AMS_NO_CARRIER);  // 2 indicates SIM found 
	ats_logf(ATSLOG_DEBUG, "PBREADY - setting ModemState to AMS_NO_CARRIER");

}
