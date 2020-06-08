#pragma once
#include "socket_interface.h"
#include "db-monitor.h"

#include "packetizer.h"

#define MESSAGECENTERDB "messages_db"
#define CANTELDB "cantel_db"

class PacketizerDB
{
public:
	PacketizerDB(MyData&);
	~PacketizerDB();
	void start();
	int dbquerylastmid(const ats::String&);
	int dbqueryoldestmid(const ats::String& dbname);
	bool dbquery_from_messagesdb(const int mid, ats::StringMap& sm);
	bool dbquery_from_canteldb(const int mid, ats::StringMap& sm);
	bool dbcopy(int mid);
	bool dbrecordremove(int mid);
	void dbclose();
private:
	ClientSocket m_cs;
	pthread_t m_readthread;
	pthread_t m_postthread;
	db_monitor::DBMonitorContext* m_db;
	pthread_mutex_t* m_mutex;
	MyData* m_data;
};

