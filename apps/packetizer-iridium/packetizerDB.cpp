#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <map>

#include "atslogger.h"
#include "ConfigDB.h"

#include "packetizerDB.h"

using namespace ats;
extern ATSLogger g_log;
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
	"usr_msg_data",//33
	"rssi",//34
	"mobile_id", //35
	"mobile_id_type" //36
};


ats::StringMap PacketizerDB::m_DBPacketizerColumnNameType;


#define sendsql(dbname) \
const ats::String& err = \
	m_db->query(dbname, buf);\
	if(!err.empty())\
	{\
		ats_logf(&g_log, "%s,%d: Error: %s", __FILE__, __LINE__,err.c_str());\
		return false;\
	}

PacketizerDB::PacketizerDB(MyData& pData,const ats::String p_packetizerdb_name, const ats::String p_packetizerdb_path):m_data(&pData)
{
	m_db = 0;
	m_packetizerdb_name = p_packetizerdb_name;
	m_packetizerdb_path = p_packetizerdb_path;
	m_pri = 10;

	m_DBPacketizerColumnNameType["mid"] = "INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL";
	m_DBPacketizerColumnNameType["mtid"] = "INTEGER";
	m_DBPacketizerColumnNameType["msg_priority"] = "INTEGER";

}

PacketizerDB::~PacketizerDB()
{
	dbclose();
}


bool PacketizerDB::dbcreate()
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
	const ats::String& err = m_db->query(m_packetizerdb_name, sql.str().c_str());

	if(!err.empty())
	{
		ats_logf(&g_log, "%s,%d: Error: %s", __FILE__, __LINE__,err.c_str());
		return false;
	}

  CreateMIDTable();
	return true;
}

void PacketizerDB::CreateMIDTable()
{
	const ats::String strCreateTable(
	"CREATE TABLE  IF NOT EXISTS MIDs_table("
	"idx INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
	"name TEXT,"
	"lastMID INTEGER"
	");"
	);

	const ats::String& err = m_db->query(m_packetizerdb_name, strCreateTable.c_str());
	
	if(!err.empty())
	{
		ats_logf(&g_log, "Error: %s", err.c_str());
	}
}


void PacketizerDB::start()
{
	m_db = new db_monitor::DBMonitorContext();
	{
		const ats::String& err = m_db->open_db(MESSAGECENTERDB,MESSAGECENTERDBPATH);

		if(!err.empty())
		{
			exit(1);
		}

	}
	{
		const ats::String& err = m_db->open_db(m_packetizerdb_name, m_packetizerdb_path);

		if(!err.empty())
		{
			exit(1);
		}

	}
	dbcreate();
}

bool PacketizerDB::dbquery(ats::String p_dbname, ats::String p_query)
{
	if(!m_db)
	{
		ats_logf(&g_log, "%s,%d : Database m_db not initialized.", __FILE__, __LINE__);
		return false;
	}

	const ats::String& err = m_db->query(p_dbname, p_query.c_str());

	if(!err.empty())
	{
			ats_logf(&g_log, "%s,%d: Error: %s", __FILE__, __LINE__,err.c_str());
			return false;
	}

	return true;
}

void PacketizerDB::dbclose()
{
	delete m_db;
}

bool PacketizerDB::Query_SelectMIDRecordFromMessagesDB(const int mid, ats::StringMap& sm )
{

	ats::String buf;
	ats_sprintf(&buf, "SELECT * FROM message_table WHERE mid=\'%d\'",mid);

	if (!dbquery(MESSAGECENTERDB, buf))
	{
		return false;
	}

	if(m_db->Table().size() <= 0)
	{
		ats_logf(&g_log, "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}

	for(size_t i = 0; i < m_db->Table().size(); ++i)
	{
		db_monitor::ResultRow& row = m_db->Table()[i];

		for(size_t i = 0; i < row.size(); ++i)
		{
			sm.set(g_DBcolumnname[i].c_str(),row[i].c_str());

			if(i == 2 && row[i].empty()) //event_time
			{
				ats_logf(&g_log, "%s,%d : Timestamp format wrong ", __FILE__, __LINE__);
				return false;
			}

		}

	}

	return true;
}

bool PacketizerDB::dbquery_from_packetizerdb(const int mid, ats::StringMap& sm )
{

	ats::String buf;
	ats_sprintf(&buf, "SELECT * FROM message_table WHERE mid=\'%d\'",mid);

	dbquery(m_packetizerdb_name, buf);

	if(m_db->Table().size() <= 0)
	{
		ats_logf(&g_log, "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}

	for(size_t i = 0; i < m_db->Table().size(); ++i)
	{
		db_monitor::ResultRow& row = m_db->Table()[i];
		ats::StringMap::iterator it;
		size_t i = 0;
		for(it = m_DBPacketizerColumnNameType.begin(); it != m_DBPacketizerColumnNameType.end(); it++)
		{
			sm.set(it->first,row[i].c_str());
			if(i == 2 && row[i].empty()) //event_time
			{
				ats_logf(&g_log, "%s,%d : Timestamp format wrong ", __FILE__, __LINE__);
				return false;
			}
			i++;

		}
	}

	return true;
}

int PacketizerDB::Query_MostRecentMID(const ats::String& dbname)
{
	ats::String buf;
	ats_sprintf(&buf, "SELECT MAX(mid) FROM message_table");

	dbquery(dbname.c_str(), buf);

	if(m_db->Table().size() <= 0)
	{
		ats_logf(&g_log, "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}

	db_monitor::ResultRow& row = m_db->Table()[m_db->Table().size() - 1];
	return strtol(row[0].c_str(),0,0);
}

int PacketizerDB::Query_OldestMID(const ats::String& dbname)
{
	ats::String buf;
	ats_sprintf(&buf, "SELECT MIN(mid) FROM message_table");

	dbquery(dbname.c_str(), buf);

	if(m_db->Table().size() <= 0)
	{
		ats_logf(&g_log, "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return -1;
	}

	db_monitor::ResultRow& row = m_db->Table()[m_db->Table().size() - 1];
	return strtol(row[row.size()-1].c_str(),0,0);
}

int PacketizerDB::dbquerynextmid_from_messagedb(int p_currmid)
{
	ats::String buf;
	ats_sprintf(&buf,
				"SELECT MIN(mid) " \
				"FROM message_table " \
				"WHERE mid > '%d' " \
				"AND msg_priority <= '%d'", p_currmid, m_pri);

	dbquery(MESSAGECENTERDB, buf);

	if(m_db->Table().size() <= 0)
	{
		ats_logf(&g_log, "%s: Database table size is zero ", __PRETTY_FUNCTION__);
		return -1;
	}

	db_monitor::ResultRow& row = m_db->Table()[m_db->Table().size() - 1];
	return strtol(row[row.size()-1].c_str(),0,0);
}

int PacketizerDB::dbqueryoldestmid_from_packetizerdb()
{
	ats::String buf;
	ats_sprintf(&buf, "SELECT MIN(msg_priority) FROM message_table");
	dbquery(m_packetizerdb_name.c_str() , buf);

	if(m_db->Table().size() <= 0)
	{
		ats_logf(&g_log, "%s: Database table size is zero", __PRETTY_FUNCTION__);
		return -1;
	}

	{
		int h_pri;
		db_monitor::ResultRow& row = m_db->Table()[m_db->Table().size() -1];

		h_pri = strtol(row[row.size() -1].c_str(), 0,0);
		if(h_pri > 0)
		{
			ats_sprintf(&buf, "SELECT mid FROM message_table "\
					"WHERE msg_priority=\'%d\' "\
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



	dbquery(m_packetizerdb_name.c_str(), buf);

	if(m_db->Table().size() <= 0)
	{
		ats_logf(&g_log, "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return -1;
	}

	db_monitor::ResultRow& row = m_db->Table()[m_db->Table().size() - 1];
	return strtol(row[row.size()-1].c_str(),0,0);
}


bool PacketizerDB::dbcopy(int mid)
{
	ats::StringMap sm;
	if(PacketizerDB::Query_SelectMIDRecordFromMessagesDB(mid, sm))
	{
		ats::String buf;
		ats_sprintf(&buf, "insert into message_table (msg_priority,event_time,latitude, longitude,speed, heading, inputs, event_type, usr_msg_id,usr_msg_data) \
				VALUES (\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\')",
				(sm.get(g_DBcolumnname[1])).c_str(),(sm.get(g_DBcolumnname[2])).c_str(), (sm.get(g_DBcolumnname[4])).c_str(),(sm.get(g_DBcolumnname[5])).c_str(), (sm.get(g_DBcolumnname[7])).c_str(),(sm.get(g_DBcolumnname[8])).c_str(),
				(sm.get(g_DBcolumnname[12])).c_str(), (sm.get(g_DBcolumnname[14])).c_str(), (sm.get(g_DBcolumnname[32])).c_str(), (sm.get(g_DBcolumnname[33])).c_str());
		sendsql(m_packetizerdb_name);
		ats_logf(&g_log, " %s, %d: db_copy() query:\n%s", __FILE__, __LINE__, buf.c_str());
	}

	return true;
}

bool PacketizerDB::dbrecordremove(int mid)
{
	ats::String buf;
	ats_sprintf(&buf, "DELETE FROM message_table WHERE mid=\'%d\'", mid);
	dbquery(m_packetizerdb_name, buf);
	return true;
}



int PacketizerDB::Query_SelectPriorityOneMessage()
{
	ats::String buf;
	ats_sprintf(&buf, "SELECT mid FROM message_table WHERE msg_priority=\'1\' ORDER BY mid LIMIT 1");

	dbquery(m_packetizerdb_name.c_str(), buf);

	if(m_db->Table().size() <= 0)
	{
		return -1;
	}

	db_monitor::ResultRow& row = m_db->Table()[m_db->Table().size() - 1];
	ats_logf(&g_log, "%s: Found High Priority Event - Record %ld",__PRETTY_FUNCTION__, strtol(row[row.size()-1].c_str(),0,0));
	return strtol(row[row.size()-1].c_str(),0,0);
}

//--------------------------------------------------------------------------------
// DeleteSentMIDs - delete all mids up to and including the mid passed in.
//
bool PacketizerDB::DeleteSentMIDs(int mid)
{
	ats::String buf;
	ats_sprintf(&buf, "DELETE FROM message_table WHERE mtid<=\'%d\'", mid);
	dbquery(m_packetizerdb_name, buf);
	return true;
}
