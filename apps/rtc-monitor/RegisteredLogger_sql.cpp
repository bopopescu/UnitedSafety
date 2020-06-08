#include "atslogger.h"
#include "RegisteredLogger_sql.h"
#include "ConfigDB.h"

RegisteredLogger_sql::RegisteredLogger_sql(
	MyData& p_md,
	const ats::String& p_log_key,
	const ats::String& p_dbfname,
	const ats::String& p_log_tname,
	int p_row_limit) : RegisteredLogger(p_md)
{
	m_log_key = p_log_key;
	m_db = new db_monitor::ConfigDB();

	{
		const ats::String& emsg = m_db->open_db(m_log_key, p_dbfname);

		if(!emsg.empty())
		{
			ats_logf(ATSLOG_ERROR, "%s: open_db(%s): %s", __FUNCTION__, p_dbfname.c_str(), emsg.c_str());
		}

	}

	{
		m_log_tname = p_log_tname;
		const ats::String& emsg = m_db->query(m_log_key, "create table if not exists " + m_log_tname + " (v_id integer primary key, v_log text)");

		if(!emsg.empty())
		{
			ats_logf(ATSLOG_ERROR, "%s: query: %s", __FUNCTION__, emsg.c_str());
		}

	}

	set_row_limit(p_row_limit);
}

RegisteredLogger_sql::~RegisteredLogger_sql()
{

	if(m_db)
	{
		delete m_db;
	}

}

bool RegisteredLogger_sql::set_row_limit(int p_limit)
{
	const ats::String query(
		"drop trigger if exists row_limit;"
		" create trigger row_limit after insert on " + m_log_tname +
		" begin"
			" delete from " + m_log_tname + " where v_id <= (select v_id from " + m_log_tname + " order by v_id desc limit " + ats::toStr(p_limit) + ", 1);"
		" end;"
	);

	const ats::String& emsg = m_db->query(m_log_key, query);

	if(!emsg.empty())
	{
		ats_logf(ATSLOG_ERROR, "%s: %s", __FUNCTION__, emsg.c_str());
		return false;
	}

	return true;
}

bool RegisteredLogger_sql::send_log(const ats::String& p_log)
{
	// Log must be at least size 2 for the '[' and ']'. These square brackets are not included in the
	// SQL database.
	if(p_log.size() < 2)
	{
		return false;
	}

	const ats::String& log = p_log.substr(1, p_log.size() - 2);

	const ats::String& query = "insert into " + m_log_tname + " (v_log) values (X'" + ats::to_hex(log) + "')";
	const ats::String& emsg = m_db->query(m_log_key, query);

	if(!emsg.empty())
	{
		ats_logf(ATSLOG_ERROR, "%s: %s", __FUNCTION__, emsg.c_str());
		return false;
	}

	return true;
}
