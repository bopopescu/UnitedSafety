#include "atslogger.h"
#include "messagedatabase.h"


#include "TruLinkDB.h"

TruLinkDB::TruLinkDB(MyData& pData, const ats::String& p_packetizerdb_name, const ats::String& p_packetizerdb_path) :
	PacketizerDB(pData, p_packetizerdb_name, p_packetizerdb_path)
{
	m_DBPacketizerColumnNameType["mid"] = "INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL";
	m_DBPacketizerColumnNameType["mtid"] = "INTEGER";
	m_DBPacketizerColumnNameType["msg_priority"] = "INTEGER";
	m_DBPacketizerColumnNameType["event_time"] = "TIMESTAMP";
	m_DBPacketizerColumnNameType["fix_time"] = "TIMESTAMP";
	m_DBPacketizerColumnNameType["latitude"] = "DOUBLE";
	m_DBPacketizerColumnNameType["longitude"] = "DOUBLE";
	m_DBPacketizerColumnNameType["altitude"] = "DOUBLE";
	m_DBPacketizerColumnNameType["speed"] = "DOUBLE";
	m_DBPacketizerColumnNameType["heading"] = "DOUBLE";
	m_DBPacketizerColumnNameType["satellites"] = "INTEGER";
	m_DBPacketizerColumnNameType["fix_status"] = "INTEGER";
	m_DBPacketizerColumnNameType["hdop"] = "FLOAT";
	m_DBPacketizerColumnNameType["inputs"] = "INTEGER";
	m_DBPacketizerColumnNameType["unit_status"] = "INTEGER";
	m_DBPacketizerColumnNameType["event_type"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum0"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum1"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum2"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum3"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum4"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum5"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum6"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum7"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum8"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum9"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum10"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum11"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum12"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum13"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum14"] = "INTEGER";
	m_DBPacketizerColumnNameType["acum15"] = "INTEGER";
	m_DBPacketizerColumnNameType["usr_msg_route"] = "INTEGER";
	m_DBPacketizerColumnNameType["usr_msg_id"] = "INTEGER";
	m_DBPacketizerColumnNameType["usr_msg_data"] = "TEXT";
	m_DBPacketizerColumnNameType["rssi"] = "INTEGER";
	m_DBPacketizerColumnNameType["mobile_id"] = "TEXT";
	m_DBPacketizerColumnNameType["mobile_id_type"] = "INTEGER";

	m_pri = 256;
}

void TruLinkDB::start()
{
	PacketizerDB::start();

	if(!createCurrTable())
	{
		ats_logf(ATSLOG(0),"%s,%d: Could not create 'current_table' in database. Current-priority messages will not be sent.", __FILE__, __LINE__ );
	}

}

bool TruLinkDB::createCurrTable()
{
	std::stringstream sql;
	sql << "CREATE TABLE IF NOT EXISTS current_table( ";
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

	db_monitor::DBMonitorContext db;
	return dbquery(db, m_packetizerdb_name, sql.str());
}

bool TruLinkDB::copy(const ats::String& p_table, ats::StringMap &p_sm)
{
	ats::String stmnt("INSERT into message_table");
	ats::String columns("(");
	ats::String values("(");

	if("current_table" == p_table)
	{
		stmnt = "REPLACE into current_table";
		columns += "mid, ";
		values += "\'0\', ";
	}

	columns += "mtid, msg_priority,event_time,fix_time,latitude, longitude, altitude, speed, heading,"
		" satellites, fix_status, hdop, inputs, unit_status, event_type, acum0, acum1, acum2, acum3, acum4, acum5,"
		" acum6, acum7, acum8, acum9, acum10, acum11, acum12, acum13, acum14, acum15, usr_msg_route, usr_msg_id,"
		" usr_msg_data, rssi, mobile_id, mobile_id_type)";

	ats::String buf;
	ats_sprintf(&buf, "%s %s "
		"VALUES %s\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',"
		"\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',"
		"\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\')",
		stmnt.c_str(), columns.c_str(), values.c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_MID])).c_str(), (p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_MSG_PRIORITY])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_EVENT_TIME])).c_str(),(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_FIX_TIME])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_LATITUDE])).c_str(), (p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_LONGITUDE])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ALTITUDE])).c_str(), (p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_SPEED])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_HEADING])).c_str(),(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_SATELLITES])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_FIX_STATUS])).c_str(), (p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_HDOP])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_INPUTS])).c_str(), (p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_UNIT_STATUS])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_EVENT_TYPE])).c_str(),(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ACCUM0])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ACCUM1])).c_str(), (p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ACCUM2])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ACCUM3])).c_str(), (p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ACCUM4])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ACCUM5])).c_str(),(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ACCUM6])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ACCUM7])).c_str(), (p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ACCUM8])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ACCUM9])).c_str(), (p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ACCUM10])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ACCUM11])).c_str(),(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ACCUM12])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ACCUM13])).c_str(), (p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ACCUM14])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_ACCUM15])).c_str(), (p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_USR_MSG_ROUTE])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_USR_MSG_ID])).c_str(), (p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_USR_MSG_DATA])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_RSSI])).c_str(), (p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_MOBILE_ID])).c_str(),
		(p_sm.get(MessageDatabase::m_db_mt_columnnames[MT_MOBILE_ID_TYPE])).c_str());

	ats_logf(ATSLOG(0), " %s, %d: dbcopy() query:\n%s", __FILE__, __LINE__, buf.c_str());
	db_monitor::DBMonitorContext db;

	if(!dbquery(db, m_packetizerdb_name, buf))
	{
		ats_logf(ATSLOG(0)," %s, %d:dbcopy() Insert into packetizer_db failed", __FILE__, __LINE__ );
		return false;
	}

	return true;
}

bool TruLinkDB::dbcopy(int p_mid)
{
	ats::StringMap sm;
	db_monitor::DBMonitorContext db;

	if(PacketizerDB::dbquery_from_messagesdb(db, p_mid, sm))
	{

		if(sm.get_int(MessageDatabase::m_db_mt_columnnames[MT_MSG_PRIORITY].c_str()) == CAMS_CURRENT_MSG_PRI)
		{
			return copy("current_table",sm);
		}

		return copy("message_table", sm);
	}

	return true;
}

void TruLinkDB::dbselectbacklog(std::vector<int>& p_mid, int p_limit)
{
	db_monitor::DBMonitorContext db;
	{
		ats::String buf;
		ats_sprintf(&buf, "SELECT mid FROM message_table ORDER BY mid ASC LIMIT %d", p_limit);
		const ats::String& err = db.query(m_packetizerdb_name, buf);

		if(!err.empty())
		{
			ats_logf(ATSLOG(0), "%s,%d: Error: %s", __FILE__, __LINE__, err.c_str());
			return;
		}

	}

	if(db.Table().size() <= 0)
	{
		p_mid.clear();
		return;
	}

	p_mid.resize(db.Table().size());

	for(size_t i = 0; i < p_mid.size(); ++i)
	{
		p_mid[i] = atoi(((db.Table()[i])[0]).c_str());
	}

}

bool TruLinkDB::dbquery_curr_msg_from_packetizerdb(ats::StringMap& sm)
{
	ats::String buf;
	ats_sprintf(&buf, "SELECT * FROM current_table ORDER BY mid ASC LIMIT 1");
	db_monitor::DBMonitorContext db;
	dbquery(db, m_packetizerdb_name, buf);

	if(db.Table().size() <= 0)
	{
//		ats_logf(ATSLOG(1), "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}

	for(size_t i = 0; i < db.Table().size(); ++i)
	{
		const db_monitor::ResultRow& row = db.Table()[i];
		ats::StringMap::iterator it;
		size_t i = 0;

		for(it = m_DBPacketizerColumnNameType.begin(); it != m_DBPacketizerColumnNameType.end(); it++)
		{
			sm.set(it->first,row[i].c_str());

			if(i == 2 && row[i].empty()) //event_time
			{
				ats_logf(ATSLOG(0), "%s,%d : Timestamp format wrong ", __FILE__, __LINE__);
				return false;
			}

			i++;
		}

	}

	return true;
}

bool TruLinkDB::dbload_messagetypes(std::map<int,int>& p_mt)
{
	ats_logf(ATSLOG(0), " %s,%d: Loading message types.", __FILE__, __LINE__);
	ats::String buf("SELECT mid,calamp_id FROM message_types WHERE calamp_id!=''");
	db_monitor::DBMonitorContext db;
	dbquery(db, MESSAGECENTERDB, buf);

	if(db.Table().size() <= 0)
	{
//		ats_logf(ATSLOG(0),"%s,%s: Database table size is zero", __FILE__, __PRETTY_FUNCTION__);
		return false;
	}

	ats_logf(ATSLOG(0), "%s, %s: Database result returned: Table size:%d", __FILE__, __PRETTY_FUNCTION__, db.Table().size());

	for(size_t i = 0; i < db.Table().size(); i++)
	{
		const db_monitor::ResultRow& row = db.Table()[i];
		ats_logf(ATSLOG(0), "%s, %s: Database row %s, %s", __FILE__, __PRETTY_FUNCTION__, row[0].c_str(), row[1].c_str());
		p_mt[strtol(row[0].c_str(),0,0)] = strtol(row[1].c_str(),0,0);
	}

	return true;
}

int TruLinkDB::dbquery_SelectPriorityOneMessage()
{
	ats::String buf;
	ats_sprintf(&buf, "SELECT mid FROM message_table WHERE msg_priority=\'1\' ORDER BY mid LIMIT 1");
	db_monitor::DBMonitorContext db;
	dbquery(db, m_packetizerdb_name, buf);

	if(db.Table().size() <= 0)
	{
		return -1;
	}

	const db_monitor::ResultRow& row = db.Table()[db.Table().size() - 1];
	ats_logf(ATSLOG(0), "%s: Found High Priority Event - Record %ld", __PRETTY_FUNCTION__, strtol(row[row.size()-1].c_str(), 0, 0));
	return strtol(row[row.size()-1].c_str(), 0, 0);
}
