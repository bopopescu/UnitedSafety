#pragma once

#include "packetizerDB.h"

#define CAMS_CURRENT_MSG_PRI 255

class InetDB : public PacketizerDB
{
public:
	InetDB(MyData& pData, const ats::String& p_packetizerdb_name, const ats::String& p_packetizerdb_path);
	virtual ~InetDB(){}
	void start();
	bool createCurrTable();
	bool copy(const ats::String& p_table, ats::StringMap &p_sm);
	bool CopyRecordFromMessageTable(int p_mid);
	void dbselectbacklog(std::vector<int>& p_mid, int p_limit);
	bool dbquery_curr_msg_from_packetizerdb(ats::StringMap& sm );
	bool LoadMessageTypes(std::map<int,int>& p_mt);
	int  SelectSinglePriorityOneMessage();
	void CleanupDB();  // remove records over 12 hours old
	void Test();
};


