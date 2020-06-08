#include "atslogger.h"
#include "packetizerDashDB.h"

extern ATSLogger g_log;

PacketizerDashDB::PacketizerDashDB(MyData& pData,const ats::String p_packetizerdb_name, const ats::String p_packetizerdb_path) :
	PacketizerDB(pData,p_packetizerdb_name, p_packetizerdb_path)
{
	m_DBPacketizerColumnNameType["mid"] = "INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL";
	m_DBPacketizerColumnNameType["mtid"] = "INTEGER";
	m_DBPacketizerColumnNameType["msg_priority"] = "INTEGER";
	m_DBPacketizerColumnNameType["event_time"] = "TIMESTAMP";
	m_DBPacketizerColumnNameType["latitude"] = "DOUBLE";
	m_DBPacketizerColumnNameType["longitude"] = "DOUBLE";
	m_DBPacketizerColumnNameType["altitude"] = "DOUBLE";
	m_DBPacketizerColumnNameType["speed"] = "DOUBLE";
	m_DBPacketizerColumnNameType["heading"] = "DOUBLE";
	m_DBPacketizerColumnNameType["satellites"] = "INTEGER";
	m_DBPacketizerColumnNameType["fix_status"] = "INTEGER";
	m_DBPacketizerColumnNameType["hdop"] = "FLOAT";
	m_DBPacketizerColumnNameType["event_type"] = "INTEGER";
	m_DBPacketizerColumnNameType["usr_msg_data"] = "TEXT";

	m_pri= 256;
}

bool PacketizerDashDB::dbcopy(int mid)
{
	ats::StringMap sm;
	db_monitor::DBMonitorContext db;

	if(PacketizerDB::dbquery_from_messagesdb(db, mid, sm))
	{
		ats::String buf;
		ats_sprintf(&buf, "insert into message_table (mtid, msg_priority,event_time,latitude, longitude, altitude, speed, heading,"
			" satellites, fix_status, hdop, event_type, usr_msg_data)"
			" VALUES (\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\')",
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_MID])).c_str(), (sm.get(MessageDatabase::m_db_mt_columnnames[MT_MSG_PRIORITY])).c_str(),
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_EVENT_TIME])).c_str(),(sm.get(MessageDatabase::m_db_mt_columnnames[MT_LATITUDE])).c_str(),
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_LONGITUDE])).c_str(), (sm.get(MessageDatabase::m_db_mt_columnnames[MT_ALTITUDE])).c_str(),
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_SPEED])).c_str(), (sm.get(MessageDatabase::m_db_mt_columnnames[MT_HEADING])).c_str(),
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_SATELLITES])).c_str(), (sm.get(MessageDatabase::m_db_mt_columnnames[MT_FIX_STATUS])).c_str(),
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_HDOP])).c_str(), (sm.get(MessageDatabase::m_db_mt_columnnames[MT_EVENT_TYPE])).c_str(),
			(sm.get(MessageDatabase::m_db_mt_columnnames[MT_USR_MSG_DATA])).c_str());
		ats_logf(&g_log, " %s, %d: dbcopy() query:\n%s", __FILE__, __LINE__, buf.c_str());

		if(!dbquery(db, m_packetizerdb_name, buf))
		{
			ats_logf(&g_log," %s, %d:dbcopyt() Insert into packetizer_db failed", __FILE__, __LINE__ );
			return false;
		}

	}

	return true;
}
