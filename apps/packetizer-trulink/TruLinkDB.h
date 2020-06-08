#pragma once

#include "packetizerDB.h"

#define CAMS_CURRENT_MSG_PRI 255

//---------------------------------------------------------------------------------------------------------------------
//
//  TruLinkDB - contains records with data required for sending TruLink Protocol packets.
//
//  Note: dbcopy will only copy records from the message table with an event_code of TRAK_TRULINK_MSG (as defined in
//        ../message-assembler/messagetypes.h.  All other messages are assumed to be CalAmp format messages without
//        any TruLink protocol encoding data.
//
//	Note: The Event Time and the Event Type are included in the database primarily for debugging purposes.  It allows
// 				the developers to see the message creation as it is happening.  The only data sent is the TruLink protocol
//				packet stored in the encoded_data field.
//
class TruLinkDB : public PacketizerDB
{
public:
	TruLinkDB(MyData& pData, const ats::String& p_packetizerdb_name, const ats::String& p_packetizerdb_path);
	void start();
	bool createCurrTable();
	bool copy(const ats::String& p_table, ats::StringMap &p_sm);
	bool dbcopy(int p_mid);
	void dbselectbacklog(std::vector<int>& p_mid, int p_limit);
	bool dbquery_curr_msg_from_packetizerdb(ats::StringMap& sm );
	bool dbload_messagetypes(std::map<int,int>& p_mt);
	int dbquery_SelectPriorityOneMessage();

	bool GetRecord(const int mid, ats::StringMap& sm, const char* table="message_table")
	{
		return dbquery_from_packetizerdb(mid, sm, table);
	}

};
