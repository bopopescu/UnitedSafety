#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#include "packetizerDB.h"
#include <atslogger.h>
using namespace ats;

extern int g_CurMTID; // current records MTID for sending to mid.db for iridium tracking

extern const ats::String g_DBcolumnname[]={
	"mid",//0
	"msg_priority",//1
	"event_time",//2
	"fix_time",//3
	"latitude",//4
	"longitude",//5
	"altitude",//6
	"speed",//7
	"heading",//8
	"satellites",//9
	"fix_status",//10
	"hdop",//11
	"inputs",//12
	"unit_status",//13
	"event_type",//14
	"acum0",//15
	"acum1",//16
	"acum2",//17
	"acum3",//18
	"acum4",//19
	"acum5",//20
	"acum6",//21
	"acum7",//22
	"acum8",//23
	"acum9",//24
	"acum10",//25
	"acum11",//26
	"acum12",//27
	"acum13",//28
	"acum14",//29
	"acum15",//30
	"usr_msg_route",//31
	"usr_msg_id",//32
	"usr_msg_data"//33
	"rssi",//34
	"mobile_id", //35
	"mobile_id_type" //36
};

extern const ats::String g_CANTELDBcolumnname[]={
	"mid",//0
	"mtid",
	"msg_priority",//1
	"event_time",//2
	"fix_time",//3
	"latitude",//4
	"longitude",//5
	"altitude",//6
	"speed",//7
	"heading",//8
	"hdop",//9
	"inputs",//10
	"event_type",//11
	"usr_msg_id",//13
	"usr_msg_data"//14
};

extern ATSLogger g_log;

#define sendsql(dbname) \
const ats::String& err = \
	m_db->query(dbname, buf);\
	if(!err.empty())\
	{\
		ats_logf(&g_log, "%s,%d: Error: %s", __FILE__, __LINE__,err.c_str());\
		return false;\
	}

//----------------------------------------------------------------------------------------------------
PacketizerDB::PacketizerDB(MyData& pData):m_data(&pData)
{
}

//----------------------------------------------------------------------------------------------------
PacketizerDB::~PacketizerDB()
{
	dbclose();
}

//----------------------------------------------------------------------------------------------------
void PacketizerDB::start()
{
	SanityCheck();
  
	m_db = new db_monitor::DBMonitorContext();

	{
		const ats::String& err = m_db->open_db("messages_db","/mnt/update/database/messages.db");

		if(!err.empty())
		{
			ats_logf(&g_log, "%s: Error:err.empty when opening database messages.db",__PRETTY_FUNCTION__);
			return;
		}

	}
	{
		const ats::String& err = m_db->open_db("dashboard_db","/mnt/update/database/dashboard.db");

		if(!err.empty())
		{
			ats_logf(&g_log, "%s: Error:err.empty when opening database dashboard.db",__PRETTY_FUNCTION__);
			return;
		}

	}
	{
		const ats::String createMSG("CREATE TABLE IF NOT EXISTS message_table ");
		const ats::String createRT("CREATE TABLE IF NOT EXISTS realtime_table ");
		const ats::String createdb("	("
			"mid		 INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
			"mtid   INTEGER, "
			"msg_priority INTEGER,"
			"event_time TIMESTAMP,"
			"fix_time TIMESTAMP,"
			"latitude DOUBLE,"
			"longitude DOUBLE,"
			"altitude DOUBLE,"
			"speed DOUBLE,"
			"heading DOUBLE,"
			"hdop DOUBLE,"
			"inputs INTEGER,"
			"event_type INTEGER,"
			"usr_msg_id INTEGER,"
			"usr_msg_data TEXT);");

		// create the message_table table
		{
			const ats::String& err = m_db->query(CANTELDB, createMSG + createdb);

			if(!err.empty())
			{
				ats_logf(ATSLOG(0), "%s: Error:%s when creating message_table table", __PRETTY_FUNCTION__, err.c_str());
				return;
			}

		}

		// create the realtime_table table
		{
			const ats::String& err = m_db->query(CANTELDB, createRT + createdb);

			if(!err.empty())
			{
				ats_logf(ATSLOG(0), "%s: Error:%s when creating realtime_table table", __PRETTY_FUNCTION__, err.c_str());
				return;
			}

		}

	}

}

//----------------------------------------------------------------------------------------------------
void PacketizerDB::dbclose()
{
	delete m_db;
}

//----------------------------------------------------------------------------------------------------
bool PacketizerDB::dbquery_from_messagesdb(const int mid, ats::StringMap& sm )
{

	ats::String buf;
	ats_sprintf(&buf, "SELECT mid,msg_priority,event_time,fix_time,latitude,longitude,altitude,speed,heading,hdop,inputs,event_type,usr_msg_id,usr_msg_data FROM message_table WHERE mid=\'%d\'",mid);

	sendsql(MESSAGECENTERDB);

	if(m_db->Table().size() <= 0)
	{
		ats_logf(&g_log, "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}
  unsigned int numCols = sizeof(g_CANTELDBcolumnname) / sizeof(g_CANTELDBcolumnname[0]);
  
  
	for(size_t j = 0; j < m_db->Table().size(); ++j)
	{
		db_monitor::ResultRow& row = m_db->Table()[j];
		size_t i;

		for(i = 0; i < row.size() && i + 1 < numCols; ++i)
		{
			sm.set(g_CANTELDBcolumnname[i + 1], row[i].c_str());

			if(g_CANTELDBcolumnname[i + 1] == "event_time" && row[i].empty()) //event_time
			{
				ats_logf(&g_log, "%s,%d : Timestamp format wrong ", __FILE__, __LINE__);
				return false;
			}
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------------
bool PacketizerDB::dbquery_from_canteldb(int mid, ats::StringMap& sm , const char* table)
{

	ats::String buf;
	ats_sprintf(&buf, "SELECT * FROM %s WHERE mid=\'%d\'",table, mid);

	sendsql(CANTELDB);

	if(m_db->Table().size() <= 0)
	{
		ats_logf(&g_log, "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}

  unsigned int numCols = sizeof(g_CANTELDBcolumnname) / sizeof(g_CANTELDBcolumnname[0]);

	for(size_t i = 0; i < m_db->Table().size(); ++i)
	{
		db_monitor::ResultRow& row = m_db->Table()[i];
		size_t i;

		for(i = 0; i < row.size() && i < numCols; ++i)
		{
			sm.set(g_CANTELDBcolumnname[i],row[i].c_str());
			if(i == 2 && row[i].empty()) //event_time
			{
				ats_logf(&g_log, "%s,%d : Timestamp format wrong ", __FILE__, __LINE__);
				return false;
			}
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------------
int PacketizerDB::dbquerylastmid(const ats::String& dbname, const char* table)
{
	ats::String buf;
	ats_sprintf(&buf, "SELECT MAX(mid) FROM %s", table);
	sendsql(dbname.c_str());

	if(m_db->Table().size() <= 0)
	{
		ats_logf(&g_log, "%s: Database (%s) table size is zero ",__PRETTY_FUNCTION__, dbname.c_str());
		return false;
	}

	db_monitor::ResultRow& row = m_db->Table()[m_db->Table().size() - 1];
	return strtol(row[row.size()-1].c_str(),0,0);
}

//----------------------------------------------------------------------------------------------------
int PacketizerDB::dbqueryoldestmid(const ats::String& dbname)
{
	ats::String buf;
	ats_sprintf(&buf, "SELECT MIN(mid) FROM message_table");

	sendsql(dbname.c_str());

	if(m_db->Table().size() <= 0)
	{
		ats_logf(&g_log, "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}

	db_monitor::ResultRow& row = m_db->Table()[m_db->Table().size() - 1];
	return strtol(row[row.size()-1].c_str(),0,0);
}

//----------------------------------------------------------------------------------------------------
bool PacketizerDB::dbcopy(int mid)
{
	ats::StringMap sm;
	if(PacketizerDB::dbquery_from_messagesdb(mid, sm))
	{
		ats::String buf;

	  if (sm.get_int(g_DBcolumnname[1]) != 11)  // all but the realtime messages
		{
			ats_sprintf(&buf, "insert into message_table (mtid,msg_priority,event_time,fix_time,latitude, longitude,altitude, speed, heading, hdop, inputs, event_type, usr_msg_id,usr_msg_data) \
				VALUES (\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\')",
				(sm.get("mtid")).c_str(), (sm.get(g_DBcolumnname[1])).c_str(),(sm.get(g_DBcolumnname[2])).c_str(), (sm.get(g_DBcolumnname[3])).c_str(),
				(sm.get(g_DBcolumnname[4])).c_str(), (sm.get(g_DBcolumnname[5])).c_str(), (sm.get(g_DBcolumnname[6])).c_str(), 
        (sm.get(g_DBcolumnname[7])).c_str(), (sm.get(g_DBcolumnname[8])).c_str(),(sm.get(g_DBcolumnname[11])).c_str(),
				(sm.get(g_DBcolumnname[12])).c_str(), (sm.get(g_DBcolumnname[14])).c_str(), 
        (sm.get(g_DBcolumnname[32])).c_str(), (sm.get(g_DBcolumnname[33])).c_str());
        
        g_CurMTID = atoi((sm.get("mtid")).c_str());

			ats_logf(&g_log, "%s: SQL (%s) ",__PRETTY_FUNCTION__, buf.c_str());
			sendsql(CANTELDB);
		}
		else  // real time messages
		{
			ats_sprintf(&buf, "insert into realtime_table (mtid,msg_priority,event_time,fix_time,latitude, longitude,altitude, speed, heading, hdop, inputs, event_type, usr_msg_id,usr_msg_data) \
				VALUES (\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\')",
				(sm.get("mtid")).c_str(), (sm.get(g_DBcolumnname[1])).c_str(),(sm.get(g_DBcolumnname[2])).c_str(), (sm.get(g_DBcolumnname[3])).c_str(),
				(sm.get(g_DBcolumnname[4])).c_str(), (sm.get(g_DBcolumnname[5])).c_str(), (sm.get(g_DBcolumnname[6])).c_str(), 
        (sm.get(g_DBcolumnname[7])).c_str(), (sm.get(g_DBcolumnname[8])).c_str(),(sm.get(g_DBcolumnname[11])).c_str(),
				(sm.get(g_DBcolumnname[12])).c_str(), (sm.get(g_DBcolumnname[14])).c_str(), 
        (sm.get(g_DBcolumnname[32])).c_str(), (sm.get(g_DBcolumnname[33])).c_str());
        
//        g_CurMTID = atoi((sm.get("mtid")).c_str());

			ats_logf(&g_log, "%s: SQL (%s) ",__PRETTY_FUNCTION__, buf.c_str());
			sendsql(CANTELDB);
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------------
bool PacketizerDB::dbcopy(int midfirst,int midlast)
{
	if(midfirst> midlast)
		return false;

	for(int i = midfirst; i <= midlast ; i++)
	{
    dbcopy(i);
	}
	return true;
}

//----------------------------------------------------------------------------------------------------
bool PacketizerDB::dbrecordremove(int mid, const char* table )
{
	ats::String buf;
	ats_sprintf(&buf, "DELETE FROM %s WHERE mid=\'%d\'", table, mid);
	sendsql(CANTELDB);
	return true;
}

//----------------------------------------------------------------------------------------------------
// Check that the table has the proper number of columns in it.  Delete it if it doesn't
//  This needs to be called before the table is opened since that is where it is created if
//  it is deleted it here
void PacketizerDB::SanityCheck()
{
	db_monitor::DBMonitorContext db("dashboard_db", "/mnt/update/database/dashboard.db");
	const ats::String& err = m_db->query("PRAGMA table_info(message_table);");

	if(!err.empty())
	{
		ats_logf(&g_log, "SanityCheck::Error: %s", err.c_str());
	}

	const size_t message_table_column_count = sizeof(g_CANTELDBcolumnname) / sizeof(g_CANTELDBcolumnname[0]);
 
	if(db.Table().size() != message_table_column_count)
	{
		const ats::String& err = m_db->query("drop table if exists message_table");

		if(!err.empty())
		{
			ats_logf(&g_log, "Error: %s", err.c_str());
		}

	}

}

//----------------------------------------------------------------------------------------------------
bool PacketizerDB::DeleteAllRecords(const char* table)
{
	ats::String buf;
	ats_sprintf(&buf, "DELETE FROM %s ", table);
	sendsql(CANTELDB);
	return true;
}

