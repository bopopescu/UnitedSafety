#pragma once
#include <sqlite3.h>

#include "RegisteredLogger.h"

namespace db_monitor
{
class ConfigDB;
}

class RegisteredLogger_sql : public RegisteredLogger
{
public:
	RegisteredLogger_sql(
		MyData& p_md,
		const ats::String& p_log_key,
		const ats::String& p_dbfname,
		const ats::String& p_log_tname,
		int p_row_limit=100);

	virtual~ RegisteredLogger_sql();

	virtual bool send_log(const ats::String& p_log);

	bool set_row_limit(int p_limit);

private:
	ats::String m_log_key;
	ats::String m_log_tname;
	db_monitor::ConfigDB* m_db;
};
