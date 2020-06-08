#include "ats-string.h"
#include "atslogger.h"

#include "telit-monitor.h"
#include "ATRFSTS_AMH.h"

static void decode_gsm_status(const ats::String& p_msg)
{
}

class WCDMA_Stat
{
public:
	WCDMA_Stat(const char* p_name) : m_name(p_name)
	{
	}

	ats::String m_prev;
	const char* m_name;
};

// Description: Decodes the "AT#RFSTS" response.
//
static void decode_wcdma_status(const ats::String& p_msg)
{
	ats::StringList sl;
	ats::split(sl, p_msg, ",");

	// XXX: The WCDMA stats are in the same order as they are listed in the "AT#RFSTS" response.
	static WCDMA_Stat cmd[] =
	{
		WCDMA_Stat("PLMN"),
		WCDMA_Stat("UARFCN"),
		WCDMA_Stat("PSC"),
		WCDMA_Stat("EcIo"),
		WCDMA_Stat("RSCP"),
		WCDMA_Stat("RSSI"),
		WCDMA_Stat("LAC"),
		WCDMA_Stat("RAC"),
		WCDMA_Stat("TXPWR"),
		WCDMA_Stat("DRX"),
		WCDMA_Stat("MM"),
		WCDMA_Stat("RRC"),
		WCDMA_Stat("NOM"),
		WCDMA_Stat("BLER"),
		WCDMA_Stat("CID"),
		WCDMA_Stat("IMSI"),
		WCDMA_Stat("NetNameAsc"),
		WCDMA_Stat("SD"),
		WCDMA_Stat("nAST")
	};

	size_t i;

	for(i = 0; (i < (sizeof(cmd) / sizeof(*cmd))) && (i < sl.size()); ++i)
	{
		WCDMA_Stat& stat = cmd[i];

		if(sl[i] == stat.m_prev)
		{
			continue;
		}

		stat.m_prev = sl[i];
		ats_logf(ATSLOG_DEBUG, "%s=%s", stat.m_name, sl[i].c_str());
	}

}

void ATRFSTS_AMH::on_message(MyData& p_md, const ats::String& p_cmd, const ats::String& p_msg)
{
	const ats::String prefix("#RFSTS: ");

	if(0 != p_msg.find(prefix))
	{
		return;
	}

	// AWARE360 FIXME: There is a race condition between when "g_network_type" is set (from a "#PSNT" unsolicited response)
	//	and when the "AT#RFSTS" response is recevied).
	//
	//	Find a way to get the "g_network_type" from the "AT#RFSTS" response, or synchronize "#PSNT" with "AT#RFSTS").
	//
	// XXX: Responses are processed sequentially, so as long as the modem reports the data consistently, then
	//	there should be no issue.
	p_md.lock_data();
	const int network_type = g_network_type;
	p_md.unlock_data();

	// Values are from Telit_HE910_AT_Commands_Reference_Guide_r4.pdf, page 186, "5.1.6.1.43 Packet Service Network Type - #PSNT".
	static const int GPRS = 0;
	static const int EGPRS = 1;
	static const int WCDMA = 2;
	static const int HSDPA = 3;

	switch(network_type)
	{
	case GPRS: // Assumging "GSM" compatible
	case EGPRS: // Assuming "GSM" compatible
		decode_gsm_status(p_msg.substr(prefix.length()));
		break;

	case WCDMA:
	case HSDPA: // Assuming "WCDMA" compatible
		decode_wcdma_status(p_msg.substr(prefix.length()));
		break;

	default:
		// AWARE360 FIXME: Do a "best-effort" processing of "AT#RFSTS" response even if data format (network type) is not known.
		//	Simply storing the raw string may be sufficient because the saved/logged information can be deciphered later.
		break;
	}

}
