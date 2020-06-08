#pragma once

#include "packetizerDB.h"

// Change these when adding a new database

#define TRULINK_DB_NAME ("gemalto_db")
#define TRULINK_DB_FILE ("/mnt/update/database/gemalto.db")

class DB : public PacketizerDB
{
public:
	DB(MyData& pData, const ats::String& p_packetizerdb_name, const ats::String& p_packetizerdb_path);
	bool copy(const ats::String& p_table, ats::StringMap &p_sm);
	bool dbcopy(int p_mid);
	void dbselectbacklog(std::vector<int>& p_mid, int p_limit);
	bool dbquery_curr_msg_from_packetizerdb(ats::StringMap& sm );
	bool dbload_messagetypes(std::map<int,int>& p_mt);
	int dbquery_SelectPriorityOneMessage();
};


