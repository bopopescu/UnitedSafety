#pragma once
#include <stdio.h>

#include "socket_interface.h"
#include "db-monitor.h"


#include "packetizer.h"
#define MESSAGECENTERDB "messages_db"
#define MESSAGECENTERDBPATH "/mnt/update/database/messages.db"

class PacketizerDB
{
public:
	PacketizerDB(MyData&, const ats::String p_packetizerdb_name=0, const ats::String p_packetizerdb_path=0);
	~PacketizerDB();
	void start();
	int Query_MostRecentMID(const ats::String&);
  int Query_SelectPriorityOneMessage();
	int Query_OldestMID(const ats::String& dbname);
	int dbqueryoldestmid_from_packetizerdb();
	int dbquerynextmid_from_messagedb(int p_currmid);
	bool Query_SelectMIDRecordFromMessagesDB(const int mid, ats::StringMap& sm);
	bool dbquery_from_packetizerdb(const int mid, ats::StringMap& sm);
	bool dbquery(ats::String db, ats::String query);
	virtual bool dbcopy(int mid);
	bool dbrecordremove(int mid);
	void dbclose();
  void CreateMIDTable();
  int GetLatestPacketizerMID();
  int GetLatestIridiumMID();
  void SetLatestPacketizerMID(int latestMID);
  void SetLatestIridiumMID(int latestMID);
  bool DeleteSentMIDs(int mid);

protected:

	bool dbcreate();
	static ats::StringMap m_DBPacketizerColumnNameType;
	ats::String m_packetizerdb_name;
	ats::String m_packetizerdb_path;
	int m_pri;
private:
	MyData* m_data;
	db_monitor::DBMonitorContext* m_db;
	ClientSocket m_cs;
	pthread_mutex_t* m_mutex;

};
