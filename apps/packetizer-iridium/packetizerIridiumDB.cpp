#include "ConfigDB.h"
#include "packetizerIridiumDB.h"

extern const ats::String g_DBcolumnname[];
extern ATSLogger g_log;

PacketizerIridiumDB::PacketizerIridiumDB(MyData& pData,const ats::String p_packetizerdb_name, const ats::String p_packetizerdb_path) :
	PacketizerDB(pData,p_packetizerdb_name, p_packetizerdb_path)
{
	//define Iridium Packetizer Database.
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
	m_DBPacketizerColumnNameType["mobile_id"] = "TEXT";
	m_DBPacketizerColumnNameType["mobile_id_type"] = "INTEGER";

	//Load priority level for packetizer-iridium
	db_monitor::ConfigDB db;
	const ats::String app_name("packetizer-iridium");
	m_pri = db.GetInt(app_name, "priority_level", 10);
}


// dbcopy - only copy messages with valid priorities.
//
bool PacketizerIridiumDB::dbcopy(int mid)
{
	ats::StringMap sm;
	if(PacketizerDB::Query_SelectMIDRecordFromMessagesDB(mid, sm))
	{
    int pri = atoi(sm.get("msg_priority").c_str());
    if (pri <= m_pri)
    {
  		ats::String buf;
	  	ats_sprintf(&buf, "insert into message_table (mtid, msg_priority,event_time,latitude, longitude, altitude, speed, heading," \
					" satellites, fix_status, hdop, event_type, usr_msg_data, mobile_id, mobile_id_type) \
				VALUES (\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\', \'%s\', \'%s\')",
				(sm.get(g_DBcolumnname[0])).c_str(), (sm.get(g_DBcolumnname[1])).c_str(), (sm.get(g_DBcolumnname[2])).c_str(),
				(sm.get(g_DBcolumnname[4])).c_str(), (sm.get(g_DBcolumnname[5])).c_str(), (sm.get(g_DBcolumnname[6])).c_str(),
				(sm.get(g_DBcolumnname[7])).c_str(), (sm.get(g_DBcolumnname[8])).c_str(), (sm.get(g_DBcolumnname[9])).c_str(),
				(sm.get(g_DBcolumnname[10])).c_str(), (sm.get(g_DBcolumnname[11])).c_str(), (sm.get(g_DBcolumnname[14])).c_str(),
				(sm.get(g_DBcolumnname[33])).c_str(), (sm.get(g_DBcolumnname[35])).c_str(), (sm.get(g_DBcolumnname[36])).c_str());
      ats_logf(&g_log, " %s, %d: dbcopy() query:\n%s", __FILE__, __LINE__, buf.c_str());

      if(!dbquery(m_packetizerdb_name, buf))
      {
        ats_logf(&g_log," %s, %d:dbcopyt() Insert into packetizer_db failed", __FILE__, __LINE__ );
        return false;
      }
    }
  }
	return true;
}

void PacketizerIridiumDB::dbEmpty()  // delete all the records
{
	if(!dbquery(m_packetizerdb_name, "DELETE from message_table"))
	{
		ats_logf(&g_log," %s, %d:dbEmpty -  Delete message table failed", __FILE__, __LINE__ );
	}
}

