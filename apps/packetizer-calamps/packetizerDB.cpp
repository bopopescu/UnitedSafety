#include <stdio.h>
#include <stdlib.h>

#include "ats-common.h"
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
	"rssi",//34
	"mobile_id", //35
	"mobile_id_type" //36
};

extern const ats::String g_CALAMPDBcolumnname[]={
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
	"msg_mid", //35
	"mobile_id", //36
	"mobile_id_type" //37
};

extern ATSLogger g_log;

#define sendsql(dbname) \
const ats::String& err = \
	m_db->query(dbname, buf);\
	if(!err.empty())\
	{\
		ats_logf(ATSLOG(0), "%s,%d: Error: %s", __FILE__, __LINE__,err.c_str());\
		return false;\
	}

//----------------------------------------------------------------------------------------------------
PacketizerDB::PacketizerDB(MyData& pData):m_data(&pData)
{
  m_db = NULL;
}

//----------------------------------------------------------------------------------------------------
PacketizerDB::~PacketizerDB()
{
	dbclose();
}

//----------------------------------------------------------------------------------------------------
void PacketizerDB::start()
{
	m_db = new db_monitor::DBMonitorContext();

	{
		const ats::String err = m_db->open_db("messages_db","/mnt/update/database/messages.db");

		if(!err.empty())
		{
			// AWARE360 FIXME: How come no error message is logged here?
			exit(1);
		}
	}
	{
		const ats::String err = m_db->open_db(CALAMPDB,"/mnt/update/database/calamp.db");

		if(!err.empty())
		{
			// AWARE360 FIXME: How come no error message is logged here?
			exit(1);
		}
	}
	{
		const ats::String createMSG("CREATE TABLE IF NOT EXISTS message_table ");
		const ats::String createRT("CREATE TABLE IF NOT EXISTS realtime_table ");
		const ats::String createdb("	("
			"mid INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
			"msg_priority INTEGER,"
			"event_time TIMESTAMP,"
			"fix_time TIMESTAMP,"
			"latitude DOUBLE,"
			"longitude DOUBLE,"
			"altitude DOUBLE,"
			"speed DOUBLE,"
			"heading DOUBLE,"
			"satellites INTEGER,"
			"fix_status INTEGER,"
			"hdop FLOAT,"
			"inputs INTEGER,"
			"unit_status INTEGER,"
			"event_type INTEGER,"
			"acum0 INTEGER,"
			"acum1 INTEGER,"
			"acum2 INTEGER,"
			"acum3 INTEGER,"
			"acum4 INTEGER,"
			"acum5 INTEGER,"
			"acum6 INTEGER,"
			"acum7 INTEGER,"
			"acum8 INTEGER,"
			"acum9 INTEGER,"
			"acum10 INTEGER,"
			"acum11 INTEGER,"
			"acum12 INTEGER,"
			"acum13 INTEGER,"
			"acum14 INTEGER,"
			"acum15 INTEGER,"
			"usr_msg_route INTEGER,"
			"usr_msg_id INTEGER,"
			"usr_msg_data TEXT,"
			"rssi INTEGER,"
			"msg_mid  INTEGER,"
			"mobile_id TEXT,"
			"mobile_id_type INTEGER);");
		//if add new field into message_table, make sure message_table column count is right.
		const size_t message_table_column_count = 38;

		//database sanity check
		{
			const ats::String& err = m_db->query(CALAMPDB, "PRAGMA table_info(message_table);");

			if(!err.empty())
			{
				ats_logf(ATSLOG(0), "%s: Error:%s when query calamp table",__PRETTY_FUNCTION__, err.c_str());
				return;
			}

			if(m_db->Table().size() != message_table_column_count)
			{
				ats_logf(&g_log, " Deleting message_table!  %d columns found  %d columns expected", m_db->Table().size(), message_table_column_count);
				const ats::String& err = m_db->query(CALAMPDB, "drop table if exists message_table");

				if(!err.empty())
				{
					ats_logf(ATSLOG(0), "%s: Error:%s when drop calamp table", __PRETTY_FUNCTION__, err.c_str());
					return;
				}

			}

		}

		// create the message_table table
		{
			const ats::String& err = m_db->query(CALAMPDB, createMSG + createdb);

			if(!err.empty())
			{
				ats_logf(ATSLOG(0), "%s: Error:%s when creating message_table table", __PRETTY_FUNCTION__, err.c_str());
				return;
			}

		}
		
		// create the realtime_table table
		{
			const ats::String& err = m_db->query(CALAMPDB, createRT + createdb);

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
  if (m_db)
		delete m_db;
}

//----------------------------------------------------------------------------------------------------
bool PacketizerDB::dbload_messagetypes(std::map<int,int>& p_mt)
{
	ats_logf(&g_log, " %s,%d: Loading message types.", __FILE__, __LINE__);
	ats::String buf("SELECT mid,calamp_id FROM message_types WHERE calamp_id!=''");
	sendsql(MESSAGECENTERDB);

	if(m_db->Table().size() <= 0)
	{
		ats_logf(&g_log,"%s,%s: Database table size is zero", __FILE__, __PRETTY_FUNCTION__);
		return false;
	}
	ats_logf(&g_log, "%s, %s: Database result returned: Table size:%d", __FILE__, __PRETTY_FUNCTION__, m_db->Table().size());
	for(size_t i = 0; i < m_db->Table().size(); i++)
	{
		db_monitor::ResultRow& row = m_db->Table()[i];
		ats_logf(&g_log, "%s, %s: Database row %s, %s", __FILE__, __PRETTY_FUNCTION__,row[0].c_str(), row[1].c_str());

		p_mt[strtol(row[0].c_str(),0,0)] = strtol(row[1].c_str(),0,0);
	}

	return true;
}

//----------------------------------------------------------------------------------------------------
bool PacketizerDB::dbquery_from_messagesdb(int mid, ats::StringMap& sm )
{

	ats::String buf;
	ats_sprintf(&buf, "SELECT * FROM message_table WHERE mid=\'%d\' AND (event_type IN (SELECT mid FROM message_types WHERE calamp_id!=''))",	mid);

	sendsql(MESSAGECENTERDB);

	if(m_db->Table().size() <= 0)
	{
		ats_logf(ATSLOG(5), "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}

	for(size_t j = 0; j < m_db->Table().size(); ++j)
	{
		db_monitor::ResultRow& row = m_db->Table()[j];
		size_t j;

		for(j = 0; j < row.size(); ++j)
		{
			sm.set(g_DBcolumnname[j],row[j].c_str());
			if(j == 2 && row[j].empty()) //event_time
			{
				ats_logf(ATSLOG(0), "%s,%d : Timestamp format wrong ", __FILE__, __LINE__);
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

	sendsql(CALAMPDB);

	if(m_db->Table().size() <= 0)
	{
		ats_logf(ATSLOG(5), "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}

	for(size_t i = 0; i < m_db->Table().size(); ++i)
	{
		const db_monitor::ResultRow& row = m_db->Table()[i];
		size_t i;
		for(i = 0; i < row.size(); ++i)
		{
			sm.set(g_CALAMPDBcolumnname[i],row[i].c_str());
			if(i == 2 && row[i].empty()) //event_time
			{
				ats_logf(ATSLOG(0), "%s,%d : Timestamp format wrong ", __FILE__, __LINE__);
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
		ats_logf(ATSLOG(1), "%s: Database table size is zero ",__PRETTY_FUNCTION__);
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
		ats_logf(ATSLOG(1), "%s: Database table size is zero ",__PRETTY_FUNCTION__);
		return false;
	}

	db_monitor::ResultRow& row = m_db->Table()[m_db->Table().size() - 1];
	return strtol(row[row.size()-1].c_str(),0,0);
}

//----------------------------------------------------------------------------------------------------
void PacketizerDB::dbselectbacklog(const ats::String& p_dbname, std::vector<int>& p_mid, int p_limit)
{
	{
		ats::String buf;
		ats_sprintf(&buf, "SELECT mid FROM message_table ORDER BY mid ASC LIMIT %d", p_limit);

		const ats::String& err = m_db->query(p_dbname, buf);

		if(!err.empty())
		{
			ats_logf(ATSLOG(0), "%s,%d: Error: %s", __FILE__, __LINE__, err.c_str());
			return;
		}

	}

	if(m_db->Table().size() <= 0)
	{
		p_mid.clear();
		return;
	}

	p_mid.resize(m_db->Table().size());

	for(size_t i = 0; i < p_mid.size(); ++i)
	{
		p_mid[i] = atoi(((m_db->Table()[i])[0]).c_str());
	}

}

//----------------------------------------------------------------------------------------------------
bool PacketizerDB::dbcopy(int mid)
{
	ats::StringMap sm;
	if(PacketizerDB::dbquery_from_messagesdb(mid, sm))
	{
	  if (sm.get_int(g_DBcolumnname[1]) != 11)  // all but the realtime messages
		{
			ats::String buf;
			ats_sprintf(&buf, "insert into message_table (msg_priority,event_time,"\
			"fix_time,latitude, longitude,altitude,speed, heading,satellites,"\
			"fix_status,hdop,inputs,unit_status,event_type,acum0,acum1,acum2,acum3,"\
			"acum4,acum5,acum6,acum7,acum8,acum9,acum10,acum11,acum12,acum13,"\
			"acum14,acum15,usr_msg_route,usr_msg_id,usr_msg_data,rssi,msg_mid,mobile_id, mobile_id_type) "\
			"VALUES (\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',"\
			"\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',"\
					"\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',"\
					"\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\')",
			(sm.get(g_DBcolumnname[1])).c_str(),(sm.get(g_DBcolumnname[2])).c_str(),(sm.get(g_DBcolumnname[3])).c_str(),
			(sm.get(g_DBcolumnname[4])).c_str(),(sm.get(g_DBcolumnname[5])).c_str(),(sm.get(g_DBcolumnname[6])).c_str(),
			(sm.get(g_DBcolumnname[7])).c_str(),(sm.get(g_DBcolumnname[8])).c_str(),(sm.get(g_DBcolumnname[9])).c_str(),
			(sm.get(g_DBcolumnname[10])).c_str(),(sm.get(g_DBcolumnname[11])).c_str(),(sm.get(g_DBcolumnname[12])).c_str(),
			(sm.get(g_DBcolumnname[13])).c_str(),(sm.get(g_DBcolumnname[14])).c_str(),(sm.get(g_DBcolumnname[15])).c_str(),
			(sm.get(g_DBcolumnname[16])).c_str(),(sm.get(g_DBcolumnname[17])).c_str(),(sm.get(g_DBcolumnname[18])).c_str(),
			(sm.get(g_DBcolumnname[19])).c_str(),(sm.get(g_DBcolumnname[20])).c_str(),(sm.get(g_DBcolumnname[21])).c_str(),
			(sm.get(g_DBcolumnname[22])).c_str(),(sm.get(g_DBcolumnname[23])).c_str(),(sm.get(g_DBcolumnname[24])).c_str(),
			(sm.get(g_DBcolumnname[25])).c_str(),(sm.get(g_DBcolumnname[26])).c_str(),(sm.get(g_DBcolumnname[27])).c_str(),
			(sm.get(g_DBcolumnname[28])).c_str(),(sm.get(g_DBcolumnname[29])).c_str(),(sm.get(g_DBcolumnname[30])).c_str(),
			(sm.get(g_DBcolumnname[31])).c_str(),(sm.get(g_DBcolumnname[32])).c_str(),(sm.get(g_DBcolumnname[33])).c_str(),
			(sm.get(g_DBcolumnname[34])).c_str(),(sm.get(g_DBcolumnname[0])).c_str(), (sm.get(g_DBcolumnname[35])).c_str(),
			(sm.get(g_DBcolumnname[36])).c_str());
			sendsql(CALAMPDB);
			ats_logf(ATSLOG(3), "%s: %s", __PRETTY_FUNCTION__, buf.c_str());
		}
		else  // real time messages
		{
			ats::String buf;
			ats_sprintf(&buf, "insert into realtime_table (msg_priority,event_time,"\
			"fix_time,latitude, longitude,altitude,speed, heading,satellites,"\
			"fix_status,hdop,inputs,unit_status,event_type,acum0,acum1,acum2,acum3,"\
			"acum4,acum5,acum6,acum7,acum8,acum9,acum10,acum11,acum12,acum13,"\
			"acum14,acum15,usr_msg_route,usr_msg_id,usr_msg_data,rssi,msg_mid,mobile_id, mobile_id_type) "\
			"VALUES (\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',"\
			"\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',"\
					"\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',"\
					"\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\',\'%s\')",
			(sm.get(g_DBcolumnname[1])).c_str(),(sm.get(g_DBcolumnname[2])).c_str(),(sm.get(g_DBcolumnname[3])).c_str(),
			(sm.get(g_DBcolumnname[4])).c_str(),(sm.get(g_DBcolumnname[5])).c_str(),(sm.get(g_DBcolumnname[6])).c_str(),
			(sm.get(g_DBcolumnname[7])).c_str(),(sm.get(g_DBcolumnname[8])).c_str(),(sm.get(g_DBcolumnname[9])).c_str(),
			(sm.get(g_DBcolumnname[10])).c_str(),(sm.get(g_DBcolumnname[11])).c_str(),(sm.get(g_DBcolumnname[12])).c_str(),
			(sm.get(g_DBcolumnname[13])).c_str(),(sm.get(g_DBcolumnname[14])).c_str(),(sm.get(g_DBcolumnname[15])).c_str(),
			(sm.get(g_DBcolumnname[16])).c_str(),(sm.get(g_DBcolumnname[17])).c_str(),(sm.get(g_DBcolumnname[18])).c_str(),
			(sm.get(g_DBcolumnname[19])).c_str(),(sm.get(g_DBcolumnname[20])).c_str(),(sm.get(g_DBcolumnname[21])).c_str(),
			(sm.get(g_DBcolumnname[22])).c_str(),(sm.get(g_DBcolumnname[23])).c_str(),(sm.get(g_DBcolumnname[24])).c_str(),
			(sm.get(g_DBcolumnname[25])).c_str(),(sm.get(g_DBcolumnname[26])).c_str(),(sm.get(g_DBcolumnname[27])).c_str(),
			(sm.get(g_DBcolumnname[28])).c_str(),(sm.get(g_DBcolumnname[29])).c_str(),(sm.get(g_DBcolumnname[30])).c_str(),
			(sm.get(g_DBcolumnname[31])).c_str(),(sm.get(g_DBcolumnname[32])).c_str(),(sm.get(g_DBcolumnname[33])).c_str(),
			(sm.get(g_DBcolumnname[34])).c_str(),(sm.get(g_DBcolumnname[0])).c_str(), (sm.get(g_DBcolumnname[35])).c_str(),
			(sm.get(g_DBcolumnname[36])).c_str());
			sendsql(CALAMPDB);
			ats_logf(ATSLOG(3), "%s: %s", __PRETTY_FUNCTION__, buf.c_str());
		}
	}
	return true;
}

//----------------------------------------------------------------------------------------------------
bool PacketizerDB::dbrecordremove(int mid, const char* table )
{
	ats::String buf;
	ats_sprintf(&buf, "DELETE FROM %s WHERE mid=\'%d\'", table, mid);
	sendsql(CALAMPDB);
	return true;
}

//----------------------------------------------------------------------------------------------------
bool PacketizerDB::DeleteAllRecords(const char* table)
{
	ats::String buf;
	ats_sprintf(&buf, "DELETE FROM %s ", table);
	sendsql(CALAMPDB);
	return true;
}

