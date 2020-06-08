#pragma once

#include "packetizerDB.h"

#define CAMS_CURRENT_MSG_PRI 255

class PacketizerSecondaryDB : public PacketizerDB
{
public:
	PacketizerSecondaryDB(MyData& pData,const ats::String p_packetizerdb_name, const ats::String p_packetizerdb_path);
	void start();
	bool createCurrTable();
	bool copy(ats::StringMap &p_sm);
	bool dbcopy(int p_mid);
	void dbselectbacklog(std::vector<int>& p_mid, int p_limit);
	bool dbquery_curr_msg_from_packetizerdb(ats::StringMap& sm );
	bool dbload_messagetypes(std::map<int,int>& p_mt);
	int dbquery_SelectPriorityOneMessage();
};


