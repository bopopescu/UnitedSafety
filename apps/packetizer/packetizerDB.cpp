#include <stdio.h>
#include <stdlib.h>

#include "atslogger.h"
#include "packetizerDB.h"

using namespace ats;

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
	"rssi", // 34
	"mobile_id", // 35
	"mobile_id_type" //36
};

extern const ats::String g_CANTELDBcolumnname[]={
	"mid",//0
	"msg_priority",//1
	"event_time",//2
	"latitude",//3
	"longitude",//4
	"speed",//5
	"heading",//6
	"fix_status",//7
	"inputs",//8
	"event_type",//9
	"usr_msg_id",//10
	"usr_msg_data",//11
	"mtid" //12
};

#define sendsql(dbname) \
const ats::String& err = \
	m_db->query(dbname, buf);\
	if(!err.empty())\
	{\
		ats_logf(ATSLOG(0), "%s,%d: Error: %s", __FILE__, __LINE__,err.c_str());\
		return false;\
	}

PacketizerDB::PacketizerDB(MyData& pData):m_data(&pData)
{
}

PacketizerDB::~PacketizerDB()
{
	dbclose();
}

void PacketizerDB::start()
{
	m_db = new db_monitor::DBMonitorContext();

	{
		const ats::String& err = m_db->open_db("messages_db","/mnt/update/database/messages.db");

		if(!err.empty())
		{
			// AWARE360 FIXME: How come no error message is logged here?
			exit(1);
		}

	}

	{
		const ats::String& err = m_db->open_db("cantel_db","/mnt/update/database/cantel.db");

		if(!err.empty())
		{
			// AWARE360 FIXME: How come no error message is logged here?
			exit(1);
		}

	}

	{
		const ats::String createdb("CREATE TABLE IF NOT EXISTS message_table"
			"("
			"mid INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
			"msg_priority INTEGER,"
			"event_time TIMESTAMP,"
			"latitude DOUBLE,"
			"longitude DOUBLE,"
			"speed DOUBLE,"
			"heading DOUBLE,"
			"fix_status INTEGER,"
			"inputs INTEGER,"
			"event_type INTEGER,"
			"usr_msg_id INTEGER,"
			"usr_msg_data TEXT,"
			"mtid INTEGER);");

		const ats::String& err = m_db->query(CANTELDB, createdb);

		if(!err.empty())
		{
			ats_logf(ATSLOG(0), "%s: Error:%s when create cantel table", __PRETTY_FUNCTION__, err.c_str());
		}

	}

}

void PacketizerDB::dbclose()
{
	delete m_db;
}

bool PacketizerDB::dbquery_from_messagesdb(const int mid, ats::StringMap& sm )
{

	ats::String buf;
	ats_sprintf(&buf, "SELECT mid, msg_priority, event_time, latitude, longitude, speed, heading, fix_status, inputs, event_type, usr_msg_id, usr_msg_data \
		 FROM message_table WHERE mid=\'%d\'",mid);

	sendsql(MESSAGECENTERDB);

	if(m_db->Table().size() <= 0)
	{
		ats_logf(ATSLOG(0), "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}

	for(size_t i = 0; i < m_db->Table().size(); ++i)
	{
		db_monitor::ResultRow& row = m_db->Table()[i];
		size_t i;

		for(i = 0; i < row.size(); ++i)
		{
			sm.set(g_CANTELDBcolumnname[i],row[i].c_str());
			if(i == 2 && row[i].empty()) //event_time
			{
				ats_logf(ATSLOG(0), "%s,%d : Timestamp format wrong - record will be removed", __FILE__, __LINE__);
				ats::String buf;
				ats_sprintf(&buf, "DELETE FROM message_table WHERE mid=\'%d\'", mid);
				sendsql(MESSAGECENTERDB);
				return false;
			}
		}

	}

	return true;
}

bool PacketizerDB::dbquery_from_canteldb(const int mid, ats::StringMap& sm )
{

	ats::String buf;
	ats_sprintf(&buf, "SELECT * FROM message_table WHERE mid=\'%d\'",mid);

	sendsql(CANTELDB);

	if(m_db->Table().size() <= 0)
	{
		ats_logf(ATSLOG(0), "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}

	for(size_t i = 0; i < m_db->Table().size(); ++i)
	{
		db_monitor::ResultRow& row = m_db->Table()[i];
		size_t i;

		for(i = 0; i < row.size(); ++i)
		{
			sm.set(g_CANTELDBcolumnname[i],row[i].c_str());
			if(i == 2 && row[i].empty()) //event_time
			{
				ats_logf(ATSLOG(0), "%s,%d : Timestamp format wrong - record will be removed", __FILE__, __LINE__);
				ats::String buf;
				ats_sprintf(&buf, "DELETE FROM message_table WHERE mid=\'%d\'", mid);
				sendsql(CANTELDB);
				return false;
			}

		}
	}

	return true;
}
int PacketizerDB::dbquerylastmid(const ats::String& dbname)
{
	ats::String buf;
	ats_sprintf(&buf, "SELECT MAX(mid) FROM message_table");

	sendsql(dbname.c_str());

	if(m_db->Table().size() <= 0)
	{
		ats_logf(ATSLOG(0), "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}

	db_monitor::ResultRow& row = m_db->Table()[m_db->Table().size() - 1];
	return strtol(row[row.size()-1].c_str(),0,0);
}

int PacketizerDB::dbqueryoldestmid(const ats::String& dbname)
{
	ats::String buf;
	ats_sprintf(&buf, "SELECT MIN(mid) FROM message_table");

	sendsql(dbname.c_str());

	if(m_db->Table().size() <= 0)
	{
		ats_logf(ATSLOG(0), "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}

	db_monitor::ResultRow& row = m_db->Table()[m_db->Table().size() - 1];
	return strtol(row[row.size()-1].c_str(),0,0);
}

bool PacketizerDB::dbcopy(int mid)
{
	ats::StringMap sm;
	if(PacketizerDB::dbquery_from_messagesdb(mid, sm))
	{
		ats::String buf;
		ats_sprintf(&buf, "INSERT INTO message_table (msg_priority,event_time,latitude, longitude,speed, heading, fix_status, inputs, event_type, \
				usr_msg_id,usr_msg_data,mtid) VALUES (\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\')",
				(sm.get(g_DBcolumnname[1])).c_str(),(sm.get(g_DBcolumnname[2])).c_str(), (sm.get(g_DBcolumnname[4])).c_str(),
				(sm.get(g_DBcolumnname[5])).c_str(), (sm.get(g_DBcolumnname[7])).c_str(),(sm.get(g_DBcolumnname[8])).c_str(),
				(sm.get(g_DBcolumnname[10])).c_str(), (sm.get(g_DBcolumnname[12])).c_str(), (sm.get(g_DBcolumnname[14])).c_str(), 
				(sm.get(g_DBcolumnname[32])).c_str(), (sm.get(g_DBcolumnname[33])).c_str(), (sm.get(g_DBcolumnname[0])).c_str());
		sendsql(CANTELDB);
		m_data->testdata_log(buf);
	}

	return true;
}

bool PacketizerDB::dbrecordremove(int mid)
{
	ats::String buf;
	ats_sprintf(&buf, "DELETE FROM message_table WHERE mid=\'%d\'", mid);
	sendsql(CANTELDB);
	return true;
}
