#pragma once
#include <ats-common.h>
#include <db-monitor.h>

class midDB
{
public:
	midDB();
	~midDB();

	int GetLatestPacketizerMID(db_monitor::DBMonitorContext& p_db);
	int GetLatestPacketizerMID();
	int GetLatestIridiumMID();
	void SetLatestPacketizerMID(int latestMID);
	void SetLatestIridiumMID(int latestMID);

private:
	bool RunQuery(db_monitor::DBMonitorContext& p_db, const ats::String& p_query);
	void CreateMIDTable();
};
