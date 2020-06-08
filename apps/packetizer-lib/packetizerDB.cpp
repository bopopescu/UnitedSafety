#include <sstream>
#include <map>

#include <stdio.h>
#include <stdlib.h>

#include "atslogger.h"
#include "db-monitor.h"
#include "messagedatabase.h"
#include "packetizerDB.h"

using namespace ats;
extern ATSLogger g_log;

ats::StringMap PacketizerDB::m_DBPacketizerColumnNameType;

PacketizerDB::PacketizerDB(MyData& pData, const ats::String& p_packetizerdb_name, const ats::String& p_packetizerdb_path) : m_data(&pData)
{
	m_packetizerdb_name = p_packetizerdb_name;
	m_packetizerdb_path = p_packetizerdb_path;
	init();
}

PacketizerDB::PacketizerDB(MyData& pData) : m_data(&pData)
{
	init();
}

void PacketizerDB::init()
{
	m_pri = 10;
	m_DBPacketizerColumnNameType["mid"] = "INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL";
	m_DBPacketizerColumnNameType["mtid"] = "INTEGER";
	m_DBPacketizerColumnNameType["msg_priority"] = "INTEGER";
}

PacketizerDB::~PacketizerDB()
{
	dbclose();
}

bool PacketizerDB::dbcreate(db_monitor::DBMonitorContext& p_db)
{
	std::stringstream sql;
	sql << "CREATE TABLE IF NOT EXISTS message_table( ";

	ats::StringMap::iterator i;

	for(i = m_DBPacketizerColumnNameType.begin(); i != m_DBPacketizerColumnNameType.end(); i++)
	{

		if(m_DBPacketizerColumnNameType.begin() != i)
		{
			sql << ", ";
		}

		sql << i->first << " " << i->second;
	}

	sql << ");";
	const ats::String& err = p_db.query(m_packetizerdb_name, sql.str());

	if(!err.empty())
	{
		ats_logf(&g_log, "%s,%d: Error: %s", __FILE__, __LINE__,err.c_str());
		return false;
	}

	CreateMIDTable(p_db);
	return true;
}

void PacketizerDB::CreateMIDTable(db_monitor::DBMonitorContext& p_db)
{
	const ats::String strCreateTable(
		"CREATE TABLE  IF NOT EXISTS MIDs_table("
		"idx INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
		"name TEXT,"
		"lastMID INTEGER"
		");"
		);

	const ats::String& err = p_db.query(m_packetizerdb_name, strCreateTable);

	if(!err.empty())
	{
		ats_logf(&g_log, "Error: %s", err.c_str());
	}

}

void PacketizerDB::start()
{
	db_monitor::DBMonitorContext db;

	{
		const ats::String err = db.open_db(MESSAGECENTERDB, MESSAGECENTERDBPATH);

		if(!err.empty())
		{
			exit(1);
		}

	}
	{
		const ats::String err = db.open_db(m_packetizerdb_name, m_packetizerdb_path);

		if(!err.empty())
		{
			exit(1);
		}

	}

	dbcreate(db);
}

bool PacketizerDB::dbquery(db_monitor::DBMonitorContext& p_db, const ats::String& p_dbname, const ats::String& p_query)
{
	const ats::String& err = p_db.query(p_dbname, p_query);

	if(!err.empty())
	{
		ats_logf(&g_log, "%s,%d: Error on query %s (%s): %s", __FILE__, __LINE__, p_dbname.c_str(), p_query.c_str(), err.c_str());
		return false;
	}

	return true;
}

void PacketizerDB::dbclose()
{
}

bool PacketizerDB::dbquery_from_messagesdb(db_monitor::DBMonitorContext& p_db, const int mid, ats::StringMap& sm)
{
	ats::String buf;
	ats_sprintf(&buf, "SELECT * FROM message_table WHERE mid=\'%d\'",mid);

	if(!dbquery(p_db, MESSAGECENTERDB, buf))
	{
		return false;
	}

	if(p_db.Table().size() <= 0)
	{
//		ats_logf(&g_log, "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}

	for(size_t i = 0; i < p_db.Table().size(); ++i)
	{
		db_monitor::ResultRow& row = p_db.Table()[i];

		for(size_t i = 0; i < row.size(); ++i)
		{
			sm.set(MessageDatabase::m_db_mt_columnnames[i].c_str(),row[i].c_str());

			if(i == 2 && row[i].empty()) //event_time
			{
				ats_logf(&g_log, "%s,%d : Timestamp format wrong ", __FILE__, __LINE__);
				return false;
			}
		}
	}

	return true;
}

bool PacketizerDB::dbquery_from_packetizerdb(const int mid, ats::StringMap& sm, const char* table)
{
	ats::String buf;
	ats_sprintf(&buf, "SELECT * FROM %s WHERE mid=\'%d\'", table, mid);

	db_monitor::DBMonitorContext db;
	dbquery(db, m_packetizerdb_name, buf);

	if(db.Table().size() <= 0)
	{
//		ats_logf(&g_log, "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}

	for(size_t i = 0; i < db.Table().size(); ++i)
	{
		db_monitor::ResultRow& row = db.Table()[i];
		ats::StringMap::iterator it;
		size_t j = 0;

		for(it = m_DBPacketizerColumnNameType.begin(); it != m_DBPacketizerColumnNameType.end(); it++)
		{
			sm.set(it->first,row[j].c_str());

			if((it->first == "event_time") && row[j].empty()) //event_time
			{
				ats_logf(&g_log, "%s,%d : Timestamp format wrong ", __FILE__, __LINE__);
				dbrecordremove(mid);
				return false;
			}

			j++;
		}

	}

	return true;
}

int PacketizerDB::dbquerylastmid(const String &dbname, const char *table)
{
	ats::String buf;
	ats_sprintf(&buf, "SELECT MAX(mid) FROM %s", table);

	db_monitor::DBMonitorContext db;
	dbquery(db, dbname, buf);

	if(db.Table().size() <= 0)
	{
//		ats_logf(&g_log, "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}

	db_monitor::ResultRow& row = db.Table()[db.Table().size() - 1];
	return strtol(row[0].c_str(),0,0);
}

int PacketizerDB::dbqueryoldestmid(const ats::String& dbname)
{
	ats::String buf;
	ats_sprintf(&buf, "SELECT MIN(mid) FROM message_table");

	db_monitor::DBMonitorContext db;
	dbquery(db, dbname, buf);

	if(db.Table().size() <= 0)
	{
//		ats_logf(&g_log, "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return -1;
	}

	db_monitor::ResultRow& row = db.Table()[db.Table().size() - 1];
	return strtol(row[row.size()-1].c_str(),0,0);
}

//-------------------------------------------------------------------------------------------------------------------
// Select the highest priority message to send first.
bool PacketizerDB::dbqueryhighestprimid_from_packetizerdb(size_t& p_mid, size_t& p_msg_pri)
{
	db_monitor::DBMonitorContext db;
	dbquery(db, m_packetizerdb_name, "SELECT mid, msg_priority FROM message_table ORDER BY msg_priority, mid ASC LIMIT 1");

	if(db.Table().empty())
	{
		return false;
	}

	const db_monitor::ResultRow& row = db.Table()[0];
	p_mid = strtoul(row[0].c_str(),0,0);
	p_msg_pri = strtoul(row[1].c_str(), 0, 0);
	return true;
}

int PacketizerDB::dbquerynextmid_from_messagedb(int p_currmid)
{
	ats::String buf;
	ats_sprintf(&buf,
		"SELECT MIN(mid) "
		"FROM message_table "
		"WHERE mid > '%d' "
		"AND msg_priority <= '%d'", p_currmid, m_pri);

	db_monitor::DBMonitorContext db;
	dbquery(db, MESSAGECENTERDB, buf);

	if(db.Table().size() <= 0)
	{
		return -1;
	}

	db_monitor::ResultRow& row = db.Table()[db.Table().size() - 1];
	return strtol(row[row.size()-1].c_str(),0,0);
}

int PacketizerDB::dbqueryoldestmid_from_packetizerdb()
{
	ats::String buf;
	ats_sprintf(&buf, "SELECT MIN(msg_priority) FROM message_table");
	db_monitor::DBMonitorContext db;
	dbquery(db, m_packetizerdb_name, buf);

	if(db.Table().size() <= 0)
	{
//		ats_logf(&g_log, "%s: Database table size is zero", __PRETTY_FUNCTION__);
		return -1;
	}

	{
		int h_pri;
		db_monitor::ResultRow& row = db.Table()[db.Table().size() -1];
		h_pri = strtol(row[row.size() -1].c_str(), 0,0);

		if(h_pri > 0)
		{
			ats_sprintf(&buf, "SELECT mid FROM message_table "
				"WHERE msg_priority=\'%d\' "
				"ORDER BY mid LIMIT 1", h_pri);
		}
#if 0
		/* This else is for debugging use only.  All messages should have priorities assigned to them.*/
		else
		{
			ats_sprintf(&buf, "SELECT MIN(mid) FROM message_table ");
		}
#endif
	}

	dbquery(db, m_packetizerdb_name, buf);

	if(db.Table().size() <= 0)
	{
//		ats_logf(&g_log, "%s: Database table size is zero ", __PRETTY_FUNCTION__);
		return -1;
	}

	db_monitor::ResultRow& row = db.Table()[db.Table().size() - 1];
	return strtol(row[row.size()-1].c_str(),0,0);
}

bool PacketizerDB::dbcopy(int mid)
{
	ats::StringMap sm;
	db_monitor::DBMonitorContext db;

	if(PacketizerDB::dbquery_from_messagesdb(db, mid, sm))
	{
		ats::String buf;
		ats_sprintf(&buf, "insert into message_table (msg_priority,event_time,latitude, longitude,speed, heading, inputs, event_type, usr_msg_id,usr_msg_data) \
			VALUES (\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\')",
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_MSG_PRIORITY])).c_str(),
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_EVENT_TIME])).c_str(),
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_LATITUDE])).c_str(),
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_LONGITUDE])).c_str(),
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_SPEED])).c_str(),
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_HEADING])).c_str(),
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_INPUTS])).c_str(),
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_EVENT_TYPE])).c_str(),
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_USR_MSG_ID])).c_str(),
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_USR_MSG_DATA])).c_str());
		ats_logf(&g_log, " %s, %d: db_copy() query:\n%s", __FILE__, __LINE__, buf.c_str());
		dbquery(db, m_packetizerdb_name, buf);
	}

	return true;
}

bool PacketizerDB::dbrecordremove(int mid, const char* table)
{
	ats::String buf;
	ats_sprintf(&buf, "DELETE FROM %s WHERE mid=\'%d\'", table, mid);
	db_monitor::DBMonitorContext db;
	dbquery(db, m_packetizerdb_name, buf);
	return true;
}
