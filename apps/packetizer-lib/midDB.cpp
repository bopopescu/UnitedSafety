#include <sstream>
#include <map>

#include <stdio.h>
#include <stdlib.h>

#include "atslogger.h"
#include "db-monitor.h"
#include "midDB.h"

using namespace ats;
extern ATSLogger g_log;

midDB::midDB()
{
	CreateMIDTable();
}

midDB::~midDB()
{
}

void midDB::CreateMIDTable()
{
	const ats::String strCreateTable(
		"CREATE TABLE  IF NOT EXISTS MIDs_table("
		"idx INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
		"name TEXT,"
		"lastMID INTEGER"
		");"
		);

	db_monitor::DBMonitorContext db("mid_db", "/mnt/update/database/mid.db");
	const ats::String& err = db.query(strCreateTable);

	if(!err.empty())
	{
		ats_logf(&g_log, "Error: %s", err.c_str());
	}

}

bool midDB::RunQuery(db_monitor::DBMonitorContext& p_db, const ats::String& p_query)
{
	const ats::String& err = p_db.query(p_query);

	if(!err.empty())
	{
		ats_logf(ATSLOG(0), "%s,%d: Error: %s", __FILE__, __LINE__, err.c_str());
		return false;
	}

	return true;
}

int midDB::GetLatestPacketizerMID(db_monitor::DBMonitorContext& p_db)
{
	RunQuery(p_db, "SELECT lastMID FROM MIDs_table WHERE name = 'Packetizer'");

	if(p_db.Table().size() <= 0)
	{
		ats_logf(&g_log, "%s: Database table size is zero", __PRETTY_FUNCTION__);
		RunQuery(p_db, "insert into MIDs_table (name,lastMID) VALUES('Packetizer', '1');");
		return 0;
	}

	const db_monitor::ResultRow& row = p_db.Table()[0];
	return atol(row[0].c_str());
}

int midDB::GetLatestPacketizerMID()
{
	db_monitor::DBMonitorContext db("mid_db", "/mnt/update/database/mid.db");
	return GetLatestPacketizerMID(db);
}

int midDB::GetLatestIridiumMID()
{
	db_monitor::DBMonitorContext db("mid_db", "/mnt/update/database/mid.db");
	RunQuery(db, "SELECT lastMID FROM MIDs_table WHERE name = 'Iridium'");

	if(db.Table().size() <= 0)
	{
		ats_logf(&g_log, "%s: Database table size is zero", __PRETTY_FUNCTION__);
		RunQuery(db, "insert into MIDs_table (name,lastMID) VALUES('Iridium', '1');");
		return 0;
	}

	const db_monitor::ResultRow& row = db.Table()[0];
	return atol(row[0].c_str());
}

void midDB::SetLatestPacketizerMID(int latestMID)
{
	db_monitor::DBMonitorContext db("mid_db", "/mnt/update/database/mid.db");
	const int mid = GetLatestPacketizerMID(db);

	if(latestMID <= mid)
	{
		return;
	}

	char buf[128];
	sprintf(buf, "UPDATE MIDs_table SET lastMID = %d WHERE name = 'Packetizer';", latestMID);
	RunQuery(db, buf);
}

void midDB::SetLatestIridiumMID(int latestMID)
{
	db_monitor::DBMonitorContext db("mid_db", "/mnt/update/database/mid.db");
	const int mid = GetLatestPacketizerMID(db);

	if(latestMID <= mid)
	{
		return;
	}

	char buf[128];
	sprintf(buf, "UPDATE MIDs_table SET lastMID = %d WHERE name = 'Iridium';", latestMID);
	RunQuery(db, buf);
}
