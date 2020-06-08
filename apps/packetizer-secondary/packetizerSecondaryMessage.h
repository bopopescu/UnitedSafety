#pragma once

#include "ats-common.h"
#include "NMEA_DATA.h"
#include "messagetypes.h"

#include "ats-string.h"

class PacketizerSecondaryMessage
{
public:
    PacketizerSecondaryMessage(ats::StringMap& sm, std::string &strIMEI);
    void packetize(std::vector< char >& data);
    ats::String GetMsgName(int id);

private:
    ats::String m_Msg;

	// Description: Forward message name mapping (Message Name ---> Message ID).
	typedef std::map <const ats::String, int> MsgNameIDMap;
	 MsgNameIDMap m_msg_name;
};

