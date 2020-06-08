#pragma once

#include <vector>

#include "socket_interface.h"
#include "ats-common.h"

class ClientSocket;
typedef std::vector <ats::String> ResultRow;
typedef std::vector <ResultRow> ResultTable;

class CanJ1939DB
{
public:
	CanJ1939DB();
	virtual ~CanJ1939DB();

	ats::String open_db(const ats::String& p_db_key, const ats::String& p_db_loc);
	ats::String open_db_config();
	ats::String open_db_ong();
	ats::String query(const ats::String& p_db_key, const ats::String& p_query, size_t p_max_query_length=1000000);
	ats::String set_config(const ats::String &p_db, const ats::String& p_app, const ats::String& p_value);
	ats::String Set(const ats::String &p_db, const ats::String& p_app, const ats::String& p_value);
	ats::String h_get_config(const ats::String &p_db, const ats::String& p_app);
	ats::String h_get_applist(const ats::String &p_db);
	ats::String Get(const ats::String &p_db);
	ats::String Get(const ats::String &p_db, const ats::String& p_app);
	ats::String GetValue(const ats::String &p_db, const ats::String& p_app);
	ats::String GetApp(const ats::String &p_db);
	ats::String Update(const ats::String &p_db, const ats::String& p_app, const ats::String& p_value);
	ats::String Clear(const ats::String &p_db);
	ats::String ClearApp(const ats::String &p_db, const ats::String& p_app);
	ats::String Unset(const ats::String &p_db);
	ats::String Unset(const ats::String &p_db, const ats::String& p_app);
	ResultTable& Table(){return m_table;};

private:
	ClientSocket* m_cs;
	int m_sockfd;
	ResultRow m_column;
	ResultTable m_table;
};
