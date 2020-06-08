#pragma once
#include <stdio.h>

#include "socket_interface.h"
#include "db-monitor.h"
#include "messagedatabase.h"

#include "packetizer.h"
#define MESSAGECENTERDB "messages_db"
#define MESSAGECENTERDBPATH "/mnt/update/database/messages.db"

class PacketizerDB
{
public:
	PacketizerDB(MyData&, const ats::String& p_packetizerdb_name, const ats::String& p_packetizerdb_path);
	PacketizerDB(MyData&);
	~PacketizerDB();

	virtual void start();
	int dbquerylastmid(const ats::String&, const char* table="message_table");
	int dbqueryoldestmid(const ats::String& dbname);
	int dbqueryoldestmid_from_packetizerdb();

	// Description: Finds the next highest priority message from the packetizer database.
	//
	// Return: True is returned if a messsage is found, and "p_mid" and "p_msg_pri" are set to the
	//	message ID (MID) and message priority respectivley. If no message is found, then false
	//	is returned, and "p_mid" and "p_msg_pri" are undefined.
	bool dbqueryhighestprimid_from_packetizerdb(size_t& p_mid, size_t& p_msg_pri);

	int dbquerynextmid_from_messagedb(int p_currmid);
	bool dbquery_from_messagesdb(db_monitor::DBMonitorContext& p_db, const int mid, ats::StringMap& sm);
	bool dbquery_from_packetizerdb(const int mid, ats::StringMap& sm, const char* table="message_table");
	bool dbquery(db_monitor::DBMonitorContext& p_db, const ats::String& p_dbname, const ats::String& p_query);
	virtual bool dbcopy(int mid);
	bool dbrecordremove(int mid, const char* table="message_table");
	void dbclose();
	void CreateMIDTable(db_monitor::DBMonitorContext& p_db);
	int GetLatestPacketizerMID();
	int GetLatestIridiumMID();
	void SetLatestPacketizerMID(int latestMID);
	void SetLatestIridiumMID(int latestMID);

protected:
	bool dbcreate(db_monitor::DBMonitorContext& p_db);
	static ats::StringMap m_DBPacketizerColumnNameType;
	ats::String m_packetizerdb_name;
	ats::String m_packetizerdb_path;
	int m_pri;

private:
	PacketizerDB(const PacketizerDB&);
	PacketizerDB& operator =(const PacketizerDB&);

	MyData* m_data;
	pthread_mutex_t* m_mutex;

	void init();
};
