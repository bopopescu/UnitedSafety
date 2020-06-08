#include "socket_interface.h"
#include "atslogger.h"
#include "ats-string.h"

#include "ATCGDCONT_AMH.h"

void ATCGDCONT_AMH::on_message(MyData& p_md, const ats::String& p_cmd, const ats::String& p_msg)
{

	if("+CGDCONT" != p_cmd)
	{
		return;
	}

	ats::StringList sl;
	ats::split(sl, p_msg, ",");

	if(sl.size() < 3)
	{
		return;
	}

	const ats::String& cellapn = sl[2];
#if 0
	const ats::String& response = p_msg.substr(p_cmd.length());
	std::stringstream v;
	const ats::String& cmd = "echo " + response + " | awk -F\",\" '{print $3}'\n";
	// AWARE360 FIXME: "system" is a heavy-weight call. Is it really needed? Re-implement logic
	//	using standard (or TRULink) libraries instead of starting another process with "system".
	ats::system(cmd, &v);
	const ats::String& cellapn = v.str();
#endif

	if(!cellapn.empty())
	{
		const int ret = send_trulink_ud_msg("admin-client-cmd", 0, "atcmd cellapn %s\r", cellapn.c_str());
		ats_logf(ATSLOG_DEBUG, "atcmd apn: %s, ret=%d, uid=%d, gid=%d", cellapn.c_str(), ret, getuid(), getgid());
	}

}
